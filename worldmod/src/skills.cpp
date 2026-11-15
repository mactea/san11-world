// SIRE-style composition, implemented independently for worldmod. No fixed person array or count.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "skills.h"
#include "skill_telemetry.h"
#include <intrin.h>

namespace {
constexpr int LIMIT = 250, BASE = 100, RECORD = 0x6c;
struct Skill {
    bool defined = false;
    char name[5] = {}, description[67] = {};
    bool effects[LIMIT] = {};
};
Skill g_skills[LIMIT];
uint8_t g_records[LIMIT][RECORD];
int g_count = BASE;
TraceLogFn g_log = nullptr;
bool g_prepared = false;
thread_local uintptr_t g_queryOrigin=0;
struct RecoveryActor {
    const void* person=nullptr;
    int owned=-1;
    uint64_t generation=0;
};
struct TrapCalculation;
thread_local TrapCalculation* g_trapCalculation=nullptr;
void trapBeforeSkill(void*,int,uintptr_t);
void trapAfterSkill(void*,int,int,uintptr_t);
void trapPersonMatched(const void*,int,int,uintptr_t);
bool trapFireCalculation();
const char* nativeSkillQueryContext(int,uintptr_t,const char*&);
void precisionProvider(const void*,bool,uintptr_t);
struct MagicDecisionProbe;
thread_local MagicDecisionProbe* g_magicDecisionProbe=nullptr;
void magicDecisionMatched(void*,int,int,uintptr_t);
thread_local RecoveryActor g_elationActor;
bool criticalEffect(int effect) {return effect==0 || effect==15 || effect==17 || effect==23 || effect==25 || (effect>=34 && effect<=48);}
struct CriticalProbe;
thread_local CriticalProbe* g_criticalProbe=nullptr;
struct CriticalProbe {
    void* source;
    const void* actor=nullptr;
    int holder=-1,effect=-1;
    bool forced=false;
    CriticalProbe* previous;
    explicit CriticalProbe(void* s):source(s),previous(g_criticalProbe){g_criticalProbe=this;}
    ~CriticalProbe(){g_criticalProbe=previous;}
};
struct ProgressionProbe;
thread_local ProgressionProbe* g_progressionProbe=nullptr;
struct PersonReceipt {
    const void* person=nullptr;int id=-1,owned=-1,affiliation[4]={};uint64_t generation=0;
    PersonReceipt()=default;
    explicit PersonReceipt(const void*);
    bool valid(int effect=-1)const;
};
struct ProgressionProbe {
    int effect;
    const char* scene=nullptr;
    const char* event=nullptr;
    uintptr_t nativeCaller=0;
    const void* actor=nullptr;
    PersonReceipt* receipt=nullptr;uint64_t generation=skilltelemetry::contextGeneration();
    ProgressionProbe* previous;
    explicit ProgressionProbe(int id,PersonReceipt* r=nullptr):effect(id),receipt(r),previous(g_progressionProbe){g_progressionProbe=this;}
    void matched(const void*);
    ~ProgressionProbe(){g_progressionProbe=previous;}
};
struct OffMonthTaxProbe;
thread_local OffMonthTaxProbe* g_offMonthTaxProbe=nullptr;
struct OffMonthTaxProbe {
    RecoveryActor actor;
    OffMonthTaxProbe* previous;
    OffMonthTaxProbe():previous(g_offMonthTaxProbe){g_offMonthTaxProbe=this;}
    ~OffMonthTaxProbe(){g_offMonthTaxProbe=previous;}
};
struct TerrainProbe;
struct ZocFlagProbe;
thread_local ZocFlagProbe* g_zocFlagProbe=nullptr;
struct ZocFlagProbe {
    const void* actors[5]={};
    ZocFlagProbe* previous;
    ZocFlagProbe():previous(g_zocFlagProbe){g_zocFlagProbe=this;}
    ~ZocFlagProbe(){g_zocFlagProbe=previous;}
};
thread_local TerrainProbe* g_terrainProbe=nullptr;
struct TerrainProbe {
    const void* actor=nullptr;
    int effect=-1;
    TerrainProbe* previous;
    TerrainProbe():previous(g_terrainProbe){g_terrainProbe=this;}
    ~TerrainProbe(){g_terrainProbe=previous;}
};
struct MovementStatProbe;
thread_local MovementStatProbe* g_movementStatProbe=nullptr;
struct MovementStatProbe {
    const void* actor=nullptr;
    int effect=-1;
    MovementStatProbe* previous;
    MovementStatProbe():previous(g_movementStatProbe){g_movementStatProbe=this;}
    ~MovementStatProbe(){g_movementStatProbe=previous;}
};
struct DomesticProbe;
thread_local DomesticProbe* g_domesticProbe=nullptr;
struct DomesticProbe {
    int effect;
    const void* actor=nullptr;
    PersonReceipt provider;uint64_t generation=skilltelemetry::contextGeneration();
    bool baseline=false;
    DomesticProbe* previous;
    explicit DomesticProbe(int id):effect(id),previous(g_domesticProbe){g_domesticProbe=this;}
    ~DomesticProbe(){g_domesticProbe=previous;}
};
struct QueryOrigin {
    uintptr_t previous;
    explicit QueryOrigin(uintptr_t caller):previous(g_queryOrigin){g_queryOrigin=caller;}
    ~QueryOrigin(){g_queryOrigin=previous;}
};

bool valid(int id) { return id >= 0 && id < LIMIT && (id < BASE || g_skills[id].defined); }
bool effectId(int id) { return (id >= 0 && id < BASE) || id == 146 || id == SKILL_MERCHANT; }
bool contains(int owned, int queried) {
    return valid(owned) && (valid(queried) || effectId(queried)) &&
        (owned == queried || (effectId(queried) && g_skills[owned].effects[queried]));
}
RecoveryActor recoveryActor(const void* person) {
    if(!skilltelemetry::enabled() || !person)return {};
    uint64_t generation=skilltelemetry::contextGeneration();
    int owned=*(const int*)((const uint8_t*)person+0xe8);
    if(generation!=skilltelemetry::contextGeneration())return {};
    return {person,owned,generation};
}
bool recoveryActorValid(const RecoveryActor& actor,int effect) {
    return skilltelemetry::enabled() && actor.person && actor.generation==skilltelemetry::contextGeneration() &&
        *(const int*)((const uint8_t*)actor.person+0xe8)==actor.owned && contains(actor.owned,effect);
}

bool number(const std::string& s, int& n) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos || s.size() > 3) return false;
    n = atoi(s.c_str()); return true;
}

bool big5(const std::string& text, char* out, int capacity) {
    if (text.empty()) { out[0] = 0; return true; }
    wchar_t wide[2048];
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, wide, 2048);
    if (!n) return false;
    BOOL replaced = FALSE;
    n = WideCharToMultiByte(950, WC_NO_BEST_FIT_CHARS, wide, -1, out, capacity, nullptr, &replaced);
    return n > 0 && !replaced;
}

// id|name|description|comma-separated effects. One level; native effects, Bole (146), merchant (147).
bool parse(FILE* file, std::string& error) {
    for (auto& s : g_skills) s = Skill{};
    g_count = BASE;
    char buf[2048]; int line = 0;
    while (fgets(buf, sizeof buf, file)) {
        ++line;
        std::string text(buf);
        if (line == 1 && text.compare(0, 3, "\xef\xbb\xbf") == 0) text.erase(0, 3);
        if (!text.empty() && text.back() == '\n') text.pop_back();
        if (!text.empty() && text.back() == '\r') text.pop_back();
        if (text.empty() || text[0] == '#') continue;
        std::string fields[4]; size_t start = 0;
        bool ok = text.size() < sizeof buf - 1;
        for (int i = 0; i < 3 && ok; ++i) {
            size_t end = text.find('|', start);
            if (end == std::string::npos) { ok = false; break; }
            fields[i] = text.substr(start, end - start); start = end + 1;
        }
        fields[3] = text.substr(start);
        int id = -1;
        ok = ok && fields[3].find('|') == std::string::npos && number(fields[0], id) && id < LIMIT;
        if (ok) {
            Skill& s = g_skills[id];
            ok = !s.defined && !fields[1].empty() && big5(fields[1], s.name, sizeof s.name) &&
                big5(fields[2], s.description, sizeof s.description);
            int slots = 0;
            std::istringstream effects(fields[3]); std::string item;
            while (ok && std::getline(effects, item, ',')) {
                int effect = -1;
                ok = number(item, effect) && effectId(effect) && ++slots <= 8;
                if (ok) s.effects[effect] = true;
            }
            ok = ok && (fields[3].empty() || fields[3].back() != ',') && (id < BASE || slots > 0);
            if (ok) { s.defined = true; if (id >= g_count) g_count = id + 1; }
        }
        if (!ok) { error = "invalid definition at line " + std::to_string(line); return false; }
    }
    if (ferror(file)) { error = "could not read definitions"; return false; }
    return true;
}

int __fastcall personHas(uint8_t* self, void*, int queried) {
    int owned=*(int*)(self+0xe8);
    bool found=contains(owned,queried);
    uintptr_t caller=g_queryOrigin?g_queryOrigin:(uintptr_t)_ReturnAddress();
    if(found && g_trapCalculation)trapPersonMatched(self,owned,queried,caller);
    if(found && g_progressionProbe && queried==g_progressionProbe->effect && !g_progressionProbe->actor &&
       (!g_progressionProbe->nativeCaller || caller==g_progressionProbe->nativeCaller))
        g_progressionProbe->matched(self);
    if(found && g_zocFlagProbe && (queried==0 || queried==1 || queried==4) && !g_zocFlagProbe->actors[queried])
        g_zocFlagProbe->actors[queried]=self;
    if(found && queried==89 && g_offMonthTaxProbe && !g_offMonthTaxProbe->actor.person)g_offMonthTaxProbe->actor=recoveryActor(self);
    if(found && g_terrainProbe && !g_terrainProbe->actor && (queried==6 || queried==8 || queried==31 || queried==62)) {
        g_terrainProbe->actor=self;g_terrainProbe->effect=queried;
    }
    if(found && g_movementStatProbe && !g_movementStatProbe->actor && (queried==2 || queried==3 || queried==7)) {
        g_movementStatProbe->actor=self;g_movementStatProbe->effect=queried;
    }
    if(g_domesticProbe && queried==g_domesticProbe->effect) {
        if(g_domesticProbe->baseline)return 0;
        if(found && !g_domesticProbe->actor && g_domesticProbe->generation==skilltelemetry::contextGeneration()){
            g_domesticProbe->actor=self;g_domesticProbe->provider=PersonReceipt(self);
        }
    }
    if(queried==20 && (caller==0x4bf126 || caller==0x4bf079))precisionProvider(self,found,caller);
    if(queried==11 && caller==0x4bebfc)g_elationActor=recoveryActor(found?self:nullptr);
    if(found && !(g_domesticProbe && g_domesticProbe->baseline)) {
        const char* queryScene=nullptr;const char* queryEvent=nativeSkillQueryContext(queried,caller,queryScene);
        if(queryEvent)skilltelemetry::matched(self,owned,queried,caller,queryScene,queryEvent);
        else if(g_zocFlagProbe)skilltelemetry::matched(self,owned,queried,caller,"movement","zoc_flags_revalidation");
        else if(g_progressionProbe)skilltelemetry::matched(self,owned,queried,caller,
            g_progressionProbe->scene?g_progressionProbe->scene:g_progressionProbe->effect==84?"growth":g_progressionProbe->effect==85?"exploration":"loyalty",
            g_progressionProbe->event?g_progressionProbe->event:g_progressionProbe->effect==84?"guidance_eligibility":g_progressionProbe->effect==85?"discovery_eligibility":"loyalty_aura_eligibility");
        else if(g_offMonthTaxProbe)skilltelemetry::matched(self,owned,queried,0x59964c,
            "economy","between_month_tax_eligibility");
        else if(g_terrainProbe)skilltelemetry::matched(self,owned,queried,caller,
            "movement","movement_hazard_eligibility");
        else if(g_movementStatProbe)skilltelemetry::matched(self,owned,queried,caller,
            "unit_stats","movement_capacity_eligibility");
        else if(g_domesticProbe)skilltelemetry::matched(self,owned,queried,caller,
            g_domesticProbe->effect==79?"recruitment":"production","command_effect_eligibility");
        else if(caller==0x59a2b7 || caller==0x59a2c4)
            skilltelemetry::matched(self,owned,queried,caller,"turn_update","unit_recovery_eligibility");
        else if(caller==0x4bebfc)
            skilltelemetry::matched(self,owned,queried,caller,"battle","defeat_eligibility");
        else if(queried==20 && (caller==0x4bf126 || caller==0x4bf079))
            skilltelemetry::matched(self,owned,queried,caller,"battle","precision_reward_eligibility");
        else if(caller==0x584ff7)
            skilltelemetry::matched(self,owned,queried,caller,"battle","pressure_settlement_revalidation");
        else if(g_trapCalculation)
            skilltelemetry::matched(self,owned,queried,caller,"battle",trapFireCalculation()?"fire_modifier_eligibility":"trap_modifier_eligibility");
        else if(g_criticalProbe && criticalEffect(queried))
            skilltelemetry::matched(self,owned,queried,caller,"battle","critical_eligibility");
        else skilltelemetry::matched(self,owned,queried,caller);
    }
    return found;
}
void __fastcall personSet(uint8_t* self, void*, int id) {
    if (id == -1 || valid(id)) *(int*)(self + 0xe8) = id;
}
int __fastcall unitHas(void* self, void*, int queried) {
    // The native vtable+5c already resolves a matching member via personHas. Its -1 sentinel is enough;
    // do not retain the original 1099 upper bound, which breaks expanded person arrays.
    typedef int(__thiscall* Find)(void*, int);
    QueryOrigin origin((uintptr_t)_ReturnAddress());
    if(queried==20 && g_queryOrigin==0x4bf079)precisionProvider(nullptr,false,g_queryOrigin);
    if(queried==11 && g_queryOrigin==0x4bebfc)g_elationActor={};
    if(g_trapCalculation)trapBeforeSkill(self,queried,g_queryOrigin);
    int holder=(*(Find*)(*(uint8_t**)self + 0x5c))(self, queried);
    if(g_magicDecisionProbe && holder>=0)magicDecisionMatched(self,queried,holder,g_queryOrigin);
    if(g_trapCalculation)trapAfterSkill(self,queried,holder,g_queryOrigin);
    if(g_criticalProbe && g_criticalProbe->source==self && criticalEffect(queried)) {
        g_criticalProbe->actor=nullptr;g_criticalProbe->holder=holder;g_criticalProbe->effect=holder>=0?queried:-1;
    }
    return holder>=0;
}

uint8_t* __fastcall skillRecord(uint8_t* world, void*, int id) {
    if (id < 0 || id >= g_count) return nullptr;
    if (id < BASE && !g_skills[id].defined) return world + 0x80ef8 + id * RECORD;
    int templateId = id < BASE ? id : 0;
    if (id >= BASE)
        for (int i = 0; i < BASE; ++i) if (g_skills[id].effects[i]) { templateId = i; break; }
    uint8_t* r = g_records[id];
    // Preserve the game's native vtable, category/level and derived fields from an original effect.
    memcpy(r, world + 0x80ef8 + templateId * RECORD, RECORD);
    memset(r + 4, 0, 5); memset(r + 9, 0, 19); memset(r + 0x1c, 0, 67);
    if (g_skills[id].defined) {
        memcpy(r + 4, g_skills[id].name, sizeof g_skills[id].name);
        memcpy(r + 9, g_skills[id].name, sizeof g_skills[id].name);
        memcpy(r + 0x1c, g_skills[id].description, sizeof g_skills[id].description);
    } else {
        memcpy(r + 4, "----", 5);
        memcpy(r + 9, "Reserved", 9); // holes stay valid objects when native lists enumerate 0..count-1
    }
    return r;
}
int __fastcall skillId(uint8_t* world, void*, const void* record) {
    uintptr_t p = (uintptr_t)record, first = (uintptr_t)g_records;
    if (p >= first && p < first + sizeof g_records && (p - first) % RECORD == 0)
        return (int)((p - first) / RECORD);
    first = (uintptr_t)world + 0x80ef8;
    if (p >= first && p < first + BASE * RECORD && (p - first) % RECORD == 0)
        return (int)((p - first) / RECORD);
    return -1;
}
int __fastcall skillOrder(uint8_t* record, void*) {
    int id = skillId((uint8_t*)0x7201958, nullptr, record);
    return id >= 0 && id < BASE ? ((int*)0x79c948)[id] : id;
}

typedef void*(__thiscall* Serialize)(void*, int*);
Serialize g_serializeOriginal = (Serialize)0x48a970;
void* __fastcall serializeSkill(uint8_t* stream, void*, int* value) {
    void* result = g_serializeOriginal(stream, value);
    // Only the skill field's call site is replaced. Other signed-byte fields retain native semantics.
    if (*(int*)(stream + 8) == 1) {
        unsigned id = (uint8_t)*value;
        *value = id == 255 ? -1 : id < LIMIT ? (int)id : -1;
    }
    return result;
}
struct IncomeProviders {
    void* city=nullptr;
    int* flags=nullptr;
    uint64_t generation=0;
    RecoveryActor holders[BASE]; // native flags OR together; choose the first eligible officer once
    const char* scene="economy";
    const char* event="city_income_eligibility";
};
thread_local IncomeProviders g_incomeProviders;
thread_local IncomeProviders* g_collectIncomeProviders=nullptr;
void __cdecl incomeFlags(int* flags, int owned, const void* actor) {
    if (owned >= 0 && owned < BASE) flags[owned] = 1;
    if (valid(owned))
        for (int i = 0; i < BASE; ++i) if (g_skills[owned].effects[i]) flags[i] = 1;
    auto collecting=g_collectIncomeProviders && g_collectIncomeProviders->flags==flags?g_collectIncomeProviders:nullptr;
    if(collecting && collecting->generation!=skilltelemetry::contextGeneration())return;
    if(actor && skilltelemetry::enabled())
        for(int i=0;i<BASE;++i)if(contains(owned,i)) {
            if(collecting && !collecting->holders[i].person) {
                auto snapshot=recoveryActor(actor);
                if(snapshot.owned==owned)collecting->holders[i]=snapshot;
            }
            skilltelemetry::matched(actor,owned,i,0x4cea41,
                collecting?collecting->scene:"economy",collecting?collecting->event:"city_income_eligibility");
        }
}
uint32_t g_incomeResume = 0x4cea51;
__declspec(naked) void incomeBridge() {
    __asm {
        pushad
        push esi // native officer-list iterator's current person
        push eax
        push ebx
        call incomeFlags
        add esp, 12
        popad
        jmp dword ptr [g_incomeResume]
    }
}

// Only the taken native upkeep exemption is an applied effect. The generic income/stat
// getters above are also used by UI and AI estimates and cannot prove a resource change.
typedef uint8_t*(__thiscall* PersonById)(void*,int);
typedef int(__thiscall* BuildingValue)(void*);
typedef int(__thiscall* FoodCost)(void*,void*);
typedef int(__thiscall* BuildingId)(void*,void*);
PersonById g_telemetryPerson=(PersonById)0x490b00;
BuildingValue g_telemetryFood=(BuildingValue)0x486df0;
FoodCost g_telemetryFoodCost=(FoodCost)0x49d200;
BuildingId g_telemetryBaseId=(BuildingId)0x491770;
BuildingValue g_telemetryGold=(BuildingValue)0x486c80;
typedef void(__thiscall* CityIncomeFlags)(int*,void*,int);
CityIncomeFlags g_cityIncomeFlagsOriginal=(CityIncomeFlags)0x4ceae0;
struct IncomeCollectionScope {
    IncomeProviders* previous;
    bool completed=false;
    explicit IncomeCollectionScope(IncomeProviders* current):previous(g_collectIncomeProviders){g_collectIncomeProviders=current;}
    ~IncomeCollectionScope(){g_collectIncomeProviders=previous;if(!completed)g_incomeProviders={};}
};
void __fastcall cityIncomeFlags(int* flags,void*,void* building,int mode) {
    g_incomeProviders=IncomeProviders{};
    IncomeProviders collecting;
    if(skilltelemetry::enabled()) {
        collecting.city=building;collecting.flags=flags;collecting.generation=skilltelemetry::contextGeneration();
    }
    IncomeCollectionScope scope(collecting.city?&collecting:nullptr);
    g_cityIncomeFlagsOriginal(flags,building,mode);
    g_incomeProviders=skilltelemetry::enabled() && collecting.generation==skilltelemetry::contextGeneration()?collecting:IncomeProviders{};
    scope.completed=true;
}
skilltelemetry::Target incomeTarget(void* building) {
    skilltelemetry::Target target;
    target.base=g_telemetryBaseId((void*)0x7201958,building);
    target.force=(*(BuildingValue*)(*(uint8_t**)building+0x40))(building);
    if(*(int*)((uint8_t*)building+8)==0)target.city=target.base;
    return target;
}
typedef int(__thiscall* IncomeGrant)(void*,void*,int);
IncomeGrant g_goldIncomeGrantOriginal=(IncomeGrant)0x4ae2a0;
IncomeGrant g_foodIncomeGrantOriginal=(IncomeGrant)0x4ae300;
#include "skill_revenue_telemetry.inc"
int incomeBonus(void* proc,void* building,int requested,int effect,uintptr_t caller) {
    auto tax=monthlyRevenueObservation(building,requested,effect==87,building!=g_incomeProviders.city,true);
    auto actor=g_incomeProviders.city?g_incomeProviders.holders[effect]:RecoveryActor{};
    bool observe=recoveryActorValid(actor,effect);
    auto value=effect==87?g_telemetryGold:g_telemetryFood;
    int before=observe?value(building):0;
    int result=(effect==87?g_goldIncomeGrantOriginal:g_foodIncomeGrantOriginal)(proc,building,requested);
    if(observe && recoveryActorValid(actor,effect))skilltelemetry::appliedBonus(actor.person,actor.owned,effect,"economy",
        effect==87?"gold_income_bonus":"food_income_bonus",effect==87?"gold":"food",
        before,value(building),before,caller,incomeTarget(building));
    recordRevenueGrant(tax,building,caller,true,false);
    return result;
}
int __fastcall goldIncomeBonus(void* proc,void*,void* building,int requested) {
    return incomeBonus(proc,building,requested,87,(uintptr_t)_ReturnAddress());
}
int __fastcall foodIncomeBonus(void* proc,void*,void* building,int requested) {
    return incomeBonus(proc,building,requested,88,(uintptr_t)_ReturnAddress());
}

struct DomesticPending {
    const void* actor=nullptr;
    void* target=nullptr;
    void* members[3]={};
    int effect=-1,kind=-1,requested=0,normal=0;
    PersonReceipt provider,recipients[3];void** memberList=nullptr;bool used[3]={};
    void* base=nullptr;int baseId=-1,force=-1;uintptr_t frame=0;uint64_t generation=0;
    bool valid()const;
};
thread_local DomesticPending g_domesticQuantity,g_domesticTime;
typedef int(__cdecl* ProductionAmount)(void*,void**,int);
typedef int(__cdecl* RecruitAmount)(void*,void**);
typedef int(__cdecl* ProductionTime)(void**,int);
typedef int(__thiscall* WeaponValue)(void*,int);
typedef void*(__thiscall* CityBuilding)(void*);
typedef void*(__thiscall* BaseById)(void*,int);
typedef void(__thiscall* AssignTime)(void*,void*,int);
ProductionAmount g_productionAmountOriginal=(ProductionAmount)0x5c63d0;
RecruitAmount g_recruitAmountOriginal=(RecruitAmount)0x5c3610;
ProductionTime g_productionTimeOriginal=(ProductionTime)0x5c6ca0;
IncomeGrant g_recruitGrantOriginal=(IncomeGrant)0x4ae3c0;
typedef int(__thiscall* WeaponGrant)(void*,void*,int,int);
WeaponGrant g_weaponGrantOriginal=(WeaponGrant)0x4b4040;
WeaponValue g_weaponValue=(WeaponValue)0x47b350; // city record, not the generic building wrapper
CityBuilding g_cityBuilding=(CityBuilding)0x47c4c0;
BaseById g_baseById=(BaseById)0x490d00;
BuildingId g_telemetryPersonId=(BuildingId)0x491310;
AssignTime g_assignTimeOriginal=(AssignTime)0x4a5660;
BuildingValue g_domesticTroopCapacity=(BuildingValue)0x486bc0;
extern uintptr_t g_strategyCodeBase;
bool DomesticPending::valid()const{
    if(!actor || generation!=skilltelemetry::contextGeneration() || !provider.valid(effect) || !memberList ||
       memcmp(members,memberList,sizeof members) || !base || baseId<0 || g_baseById((void*)0x7201958,baseId)!=base ||
       g_telemetryBaseId((void*)0x7201958,base)!=baseId || (*(BuildingValue*)(*(uint8_t**)base+0x40))(base)!=force ||
       (target && (effect==79?target!=base:g_cityBuilding(target)!=base)))return false;
    bool found=false;for(int n=0;n<3;++n)if(members[n]){
        if(!recipients[n].valid())return false;if(members[n]==actor)found=true;
    }
    return found;
}

template<typename Calculate>
int domesticCalculation(DomesticPending& pending,int effect,void* target,int kind,void** members,uintptr_t caller,uintptr_t frame,Calculate calculate) {
    uintptr_t expected=g_strategyCodeBase+(effect==79?0x1c3b93:target?0x1c6715:0x1c7054);
    if(caller!=expected)return calculate();
    pending=DomesticPending{};
    if(effect<0 || !skilltelemetry::enabled())return calculate();
    DomesticPending candidate;candidate.effect=effect;candidate.target=target;candidate.kind=kind;candidate.frame=frame;
    candidate.generation=skilltelemetry::contextGeneration();candidate.memberList=members;
    if(members){memcpy(candidate.members,members,sizeof candidate.members);
        for(int n=0;n<3;++n)candidate.recipients[n]=PersonReceipt(members[n]);}
    candidate.base=target?(effect==79?target:g_cityBuilding(target)):nullptr;
    if(!target)for(int n=0;n<3;++n)if(candidate.members[n] && candidate.recipients[n].valid()){
        candidate.base=g_baseById((void*)0x7201958,candidate.recipients[n].affiliation[0]);break;}
    if(candidate.base){auto base=incomeTarget(candidate.base);candidate.baseId=base.base;candidate.force=base.force;}
    DomesticProbe probe(effect);
    int result=calculate();
    candidate.actor=probe.actor;candidate.provider=probe.provider;
    if(candidate.valid()) {
        // These three native calculators are read-only. Re-evaluate with only this effect
        // suppressed, retaining native rounding, difficulty, facility and capacity modifiers.
        probe.baseline=true;
        int normal=calculate();
        candidate.requested=result;candidate.normal=normal;
        if(candidate.valid())pending=candidate;
    }
    return result;
}
int __cdecl productionAmount(void* city,void** members,int weapon) {
    int effect=weapon>=1 && weapon<=3?80:weapon==4?81:-1;
    return domesticCalculation(g_domesticQuantity,effect,city,weapon,members,(uintptr_t)_ReturnAddress(),(uintptr_t)_AddressOfReturnAddress(),
        [&]{return g_productionAmountOriginal(city,members,weapon);});
}
int __cdecl recruitAmount(void* building,void** members) {
    return domesticCalculation(g_domesticQuantity,79,building,-1,members,(uintptr_t)_ReturnAddress(),(uintptr_t)_AddressOfReturnAddress(),
        [&]{return g_recruitAmountOriginal(building,members);});
}
int __cdecl productionTime(void** members,int weapon) {
    int effect=weapon>=5 && weapon<=8?82:weapon==10 || weapon==11?83:-1;
    return domesticCalculation(g_domesticTime,effect,nullptr,weapon,members,(uintptr_t)_ReturnAddress(),(uintptr_t)_AddressOfReturnAddress(),
        [&]{return g_productionTimeOriginal(members,weapon);});
}
void recordDomesticGrant(const DomesticPending& pending,void* building,int before,int after,const char* metric,uintptr_t caller) {
    if(!pending.valid())return;
    auto target=incomeTarget(building);
    target.equipment=pending.kind;
    if(!pending.valid())return;
    const char* scene=pending.effect==79?"recruitment":"production";
    const char* event=pending.effect==79?"recruitment_bonus":"equipment_production_bonus";
    if(after<before || pending.normal<0) {
        skilltelemetry::applied(pending.actor,pending.provider.owned,pending.effect,
            scene,event,target.person,target.force,target.city,metric,before,after,caller,target.base,target.unit,target.equipment);
        return;
    }
    int actual=after-before;
    int normal=pending.normal<actual?pending.normal:actual; // same capacity limit for both requests
    skilltelemetry::appliedBonus(pending.actor,pending.provider.owned,pending.effect,
        scene,event,metric,before,after,before+normal,caller,target);
}
int __fastcall productionGrant(void* proc,void*,void* city,int weapon,int requested) {
    auto pending=g_domesticQuantity;uintptr_t caller=(uintptr_t)_ReturnAddress();
    bool owner=caller==g_strategyCodeBase+0x1c6725 && pending.frame==(uintptr_t)_AddressOfReturnAddress();
    if(owner)g_domesticQuantity=DomesticPending{};
    bool observe=owner && pending.valid() && pending.target==city && pending.kind==weapon && pending.requested==requested;
    int before=observe?g_weaponValue(city,weapon):0;
    int result=g_weaponGrantOriginal(proc,city,weapon,requested);
    if(observe && pending.valid() && g_weaponValue(city,weapon)==std::min<int64_t>(100000,std::max<int64_t>(0,(int64_t)before+requested))) {
        char metric[24];sprintf_s(metric,"weapon_%d",weapon);
        recordDomesticGrant(pending,pending.base,before,g_weaponValue(city,weapon),metric,caller);
    }
    return result;
}
int __fastcall recruitGrant(void* proc,void*,void* building,int requested) {
    auto pending=g_domesticQuantity;uintptr_t caller=(uintptr_t)_ReturnAddress();
    bool owner=caller==g_strategyCodeBase+0x1c3ba9 && pending.frame==(uintptr_t)_AddressOfReturnAddress();
    if(owner)g_domesticQuantity=DomesticPending{};
    bool observe=owner && pending.valid() && pending.target==building && pending.kind==-1 && pending.requested==requested;
    auto troops=observe?(*(BuildingValue*)(*(uint8_t**)building+0x50)):nullptr;
    int before=observe?troops(building):0;
    int capacity=observe?g_domesticTroopCapacity(building):0;
    int result=g_recruitGrantOriginal(proc,building,requested);
    if(observe && pending.valid() && troops(building)==std::min<int64_t>(capacity,std::max<int64_t>(0,(int64_t)before+requested)))
        recordDomesticGrant(pending,building,before,troops(building),"troops",caller);
    return result;
}
void __fastcall productionAssignTime(void* proc,void*,uint8_t* person,int requested) {
    auto pending=g_domesticTime;uintptr_t caller=(uintptr_t)_ReturnAddress();
    bool owner=caller==g_strategyCodeBase+0x1c70c8 && pending.frame==(uintptr_t)_AddressOfReturnAddress();
    int member=-1;
    bool valid=owner && pending.valid() && pending.requested==requested;if(owner && !valid)g_domesticTime=DomesticPending{};
    if(valid && pending.requested==requested)
        for(int i=0;i<3;++i)if(!pending.used[i] && pending.members[i]==person){member=i;break;}
    bool observe=member>=0 && *(int*)(person+0x13c)==2 && *(int*)(person+0x140)==pending.baseId && *(int*)(person+0x144)==pending.kind;
    if(member>=0){g_domesticTime.used[member]=true;bool complete=true;
        for(int n=0;n<3;++n)if(pending.members[n] && !g_domesticTime.used[n])complete=false;
        if(complete)g_domesticTime=DomesticPending{};}
    int before=observe?person[0x158]:0;
    g_assignTimeOriginal(proc,person,requested);
    if(observe && pending.valid() && *(int*)(person+0x13c)==2 && *(int*)(person+0x140)==pending.baseId &&
       *(int*)(person+0x144)==pending.kind && person[0x158]==(uint8_t)requested) {
        // The order setter at 0x5c70b3 has already committed order=2, base and equipment IDs.
        int baseId=*(int*)(person+0x140);
        void* building=g_baseById((void*)0x7201958,baseId);
        skilltelemetry::Target target;
        if(building)target=incomeTarget(building);else target.base=baseId;
        target.person=g_telemetryPersonId((void*)0x7201958,person);
        target.equipment=pending.kind;
        if(!pending.valid())return;
        skilltelemetry::appliedBonus(pending.actor,pending.provider.owned,pending.effect,
            "production","equipment_production_time_reduced","remaining_turns",before,person[0x158],
            (uint8_t)pending.normal,caller,target);
    }
}
const struct DomesticLayout {uint32_t start;int size;const char* bytes;} g_domesticLayouts[]={
    {0x5c6710,21,"\xe8\xbb\xfc\xff\xff\x83\xc4\x0c\x50\x56\x53\xb9\x5c\x89\x99\x07\xe8\x1b\xd9\xee\xff"},
    {0x5c3b8e,27,"\xe8\x7d\xfa\xff\xff\x83\xc4\x08\x50\x8b\xcb\xe8\x22\x89\xeb\xff\x50\xb9\x5c\x89\x99\x07\xe8\x17\xa8\xee\xff"},
    {0x5c704f,12,"\xe8\x4c\xfc\xff\xff\x8b\x4c\x24\x14\x83\xc4\x08"},
    {0x5c70b3,21,"\xe8\x58\x03\xee\xff\x8b\x54\x24\x1c\x52\x56\xb9\x5c\x89\x99\x07\xe8\x98\xe5\xed\xff"},
    {0x4a7473,3,"\xc2\x1c\x00"},
};
bool verifyDomesticLayout(){
    for(const auto& s:g_domesticLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
void __cdecl recordTuntian(void* building,int holder) {
    if(!skilltelemetry::enabled())return;
    auto actor=g_telemetryPerson((void*)0x7201958,holder);
    if(!actor || !contains(*(int*)(actor+0xe8),78))return;
    int before=g_telemetryFood(building);
    // The skipped native calculation is read-only (0x49d200): food, troops and commander.
    int cost=g_telemetryFoodCost((void*)0x780b3cc,building);
    if(cost<0)return;
    int baseline=cost<before?before-cost:0;
    skilltelemetry::appliedBonus(actor,*(int*)(actor+0xe8),78,"economy",
        "garrison_food_consumption_prevented","food",before,before,baseline,0x59bfd9,incomeTarget(building));
}
uint32_t g_tuntianSkip=0x59c014,g_tuntianConsume=0x59bfdb;
const int* g_personLimitOperand=(const int*)0x490b09;
int g_tuntianMax=1099;
__declspec(naked) void tuntianBridge() {
    __asm {
        // Preserve both native holder bounds. The earlier negative-ID branch at 0x59bfd2
        // and its destination stay intact. M10 can raise the installed upper bound.
        cmp eax,dword ptr [g_tuntianMax]
        jg consume
        pushfd
        pushad
        push eax
        push esi
        call recordTuntian
        add esp,8
        popad
        popfd
        jmp dword ptr [g_tuntianSkip]
    consume:
        jmp dword ptr [g_tuntianConsume]
    }
}

#include "skill_battle_telemetry.inc"
#include "skill_movement_telemetry.inc"
#include "skill_unrest_telemetry.inc"
#include "skill_growth_telemetry.inc"
#include "skill_loyalty_telemetry.inc"
#include "skill_discovery_telemetry.inc"
#include "skill_environment_telemetry.inc"
#include "skill_diplomacy_telemetry.inc"
#include "skill_construction_telemetry.inc"
bool strategyMaximumActive(uintptr_t caller);
void strategyMaximumObserved(void* unit,int effect,int kind,int value,const void* actor,uintptr_t caller);
bool magicGrantMatches(void* source,const uint8_t* packet,void* affected,int requested,uintptr_t caller);
void magicGrantCommitted(const uint8_t* packet,int requested,int before,int after,uintptr_t caller,skilltelemetry::Target target);
void reachCostOwner(void* unit,int plan,uintptr_t frame);
bool reachObservesCost(void* unit,uintptr_t caller,uintptr_t frame);
void reachCostPaid(void* unit,int before,int after,int requested,int result,uintptr_t frame);
int trapGrantObserved(void*,void*,int,uintptr_t,const uint8_t*,const uint8_t*,IncomeGrant);
#include "skill_critical_telemetry.inc"
#include "skill_strategy_cost_telemetry.inc"
#include "skill_after_attack_telemetry.inc"
#include "skill_strategy_immunity_telemetry.inc"
#include "skill_strategy_maximum_telemetry.inc"
#include "skill_fire_aftermath_telemetry.inc"
#include "skill_trap_telemetry.inc"
#include "skill_vine_telemetry.inc"
struct StrategyCriticalScope;
void strategyFireCritical(const StrategyCriticalScope& scope,int plan,uint32_t to);
void strategyStatusCritical(const StrategyCriticalScope& scope,int plan,uint32_t to);
#include "skill_strategy_critical_telemetry.inc"
#include "skill_reach_telemetry.inc"
#include "skill_ai_weight_telemetry.inc"
#include "skill_attack_reach_telemetry.inc"
#include "skill_zoc_telemetry.inc"
#include "skill_steering_telemetry.inc"
#include "skill_strategy_fire_telemetry.inc"
#include "skill_strategy_status_telemetry.inc"
#include "skill_strategy_ambush_telemetry.inc"
#include "skill_strategy_followup_telemetry.inc"
#include "skill_strategy_charm_telemetry.inc"
#include "skill_defense_telemetry.inc"
#include "skill_ironwall_telemetry.inc"
#include "skill_assist_telemetry.inc"
#include "skill_counter_immunity_telemetry.inc"
#include "skill_double_attack_telemetry.inc"
#include "skill_pincer_telemetry.inc"
#include "skill_capture_telemetry.inc"
#include "skill_plunder_telemetry.inc"
#include "skill_magic_telemetry.inc"
#include "skill_bloodroad_handoff_telemetry.inc"

typedef int(__thiscall* Affinity)(void*, int);
Affinity g_affinityOriginal = (Affinity)0x489f80;
#include "skill_bole_telemetry.inc"
#include "skill_precision_telemetry.inc"
#include "skill_lucky_unit_event_telemetry.inc"
#include "skill_lucky_duel_telemetry.inc"
#include "skill_lucky_duel_post_telemetry.inc"
#include "skill_lucky_duel_response_telemetry.inc"
#include "skill_lucky_mission_telemetry.inc"
#include "skill_diplomacy_interaction_telemetry.inc"
#include "skill_diplomacy_contest_telemetry.inc"
#include "skill_query_context_telemetry.inc"
#include "skill_area_friendly_telemetry.inc"
#include "skill_raw_priority_telemetry.inc"
#include "skill_tribal_affinity_telemetry.inc"
#include "skill_ai_score_telemetry.inc"
int __cdecl boleRecruit(void* building, void* target,uintptr_t builderCaller) {
    typedef int(__thiscall* Find)(void*, int);
    if(builderCaller==g_boleBuilderCaller)boleCandidateBegin(building,target);
    ProgressionProbe probe(146);probe.scene="officer_recruitment";probe.event="remote_candidate_eligibility";
    int recruiter = (*(Find*)(*(uint8_t**)building + 0x5c))(building, 146);
    if(recruiter<0)return 0;
    int affinity=g_affinityOriginal(target,recruiter);
    if(affinity<=25 && builderCaller==g_boleBuilderCaller)boleCandidateAllowed(building,(uint8_t*)target,recruiter,affinity);
    return affinity<=25;
}
uint32_t g_boleAllow = 0x4cf717, g_boleReject = 0x4cf7a7;
__declspec(naked) void boleBridge() {
    __asm {
        jz allowed
        pushad
        mov eax, [esp + 0x30] // caller of the candidate builder
        push eax
        mov eax, [esp + 0x3c] // original stack+0x18: recruiting building
        push esi             // candidate free officer
        push eax
        call boleRecruit
        add esp, 12
        test eax, eax
        popad
        jnz allowed
        jmp dword ptr [g_boleReject]
    allowed:
        jmp dword ptr [g_boleAllow]
    }
}

struct Hook { uint32_t at; const char* expected; size_t size; const void* target; bool call; DWORD protection; const char* replacement; };
Hook g_hooks[] = {
    {0x5c5940, "\x55\x8b\xec\x83\xe4\xf8", 6, boleExecute},
    {0x4cf7a2, "\xe8\x09\xca\xfa\xff", 5, boleAppendBridge, true},
    {0x5c56d0, "\x6a\xff\x68\x36\x46\x73\x00", 7, boleCanExecute},
    {0x5c59e3, "\xe8\x58\xee\xff\xff", 5, boleResolve, true},
    {0x5c5ae6, "\xe8\x25\x19\xee\xff", 5, boleAssign, true},
    {0x4a7457, "\xe8\x74\x27\xfe\xff", 5, boleStore, true},
    {0x56e169, "\xe8\xe2\x7a\xf2\xff", 5, zocBaseRelation, true},
    {0x56e196, "\xe8\xb5\x7a\xf2\xff", 5, zocUnitRelation, true},
    {0x584acc, "\xe8\x4f\xa6\xfe\xff", 5, steeringMoveBridge, true},
    {0x584ab2, "\xe8\x39\xab\x01\x00", 5, steeringBaseEntryBridge, true},
    {0x59f771, "\xe8\x7a\xfa\xf1\xff", 5, steeringBaseTransfer, true},
    {0x4bf61a, "\xe8\xc1\x3b\xfe\xff", 5, steeringEnterPerson, true},
    {0x4bf636, "\x57\xb9\x58\x19\x20\x07", 6, steeringTransferCommittedBridge},
    {0x5a3d35, "\xe8\xa6\xd4\xed\xff", 5, reachTechnology, true},
    {0x592050, "\x81\xec\x0c\x04\x00\x00\x53", 7, magicConfuse},
    {0x592740, "\x55\x8b\xec\x83\xe4\xf8", 6, strategyAmbush},
    {0x592ed0, "\x55\x8b\xec\x83\xe4\xf8", 6, magicThunder},
    {0x5920c1, "\xff\x50\x5c\x50\xb9\x58\x19\x20\x07", 9, magicConfuseFindBridge},
    {0x592f1b, "\xff\x50\x5c\x50\xb9\x58\x19\x20\x07", 9, magicThunderFindBridge},
    {0x592246, "\xe8\x85\xfd\xff\xff", 5, magicDecisionBridge, true},
    {0x5922b4, "\xe8\xb7\xc7\xf1\xff", 5, magicStatus, true},
    {0x59193e, "\xe8\x2d\xd1\xf1\xff", 5, strategyStatus, true},
    {0x591b8e, "\xe8\xdd\xce\xf1\xff", 5, strategyStatus, true},
    {0x592933, "\xe8\x38\xc1\xf1\xff", 5, strategyStatus, true},
    {0x5930d7, "\xe8\x54\xe2\x01\x00", 5, magicThunderBuild, true},
    {0x5931af, "\xe8\x7c\xe1\x01\x00", 5, magicThunderBuild, true},
    {0x5931d1, "\xe8\x5a\xe1\x01\x00", 5, magicThunderBuild, true},
    {0x593234, "\xe8\xf7\xe0\x01\x00", 5, magicThunderBuild, true},
    {0x5931fc, "\x89\x0f\x89\x44\x24\x18", 6, magicThunderAdjustedBridge},
    {0x4be9b0, "\xe8\x3b\xa7\xfc\xff", 5, plunderHas, true},
    {0x4be9f6, "\xe8\xd5\x37\xfb\xff", 5, plunderRollBridge, true},
    {0x4bebbb, "\xe8\x80\x1e\xfe\xff", 5, plunderTransfer, true},
    {0x4bce85, "\xe8\x26\xbe\x0c\x00", 5, bloodroadVisual, true},
    {0x588cd3, "\xe8\xf8\x4e\xff\xff", 5, bloodroadUnlock, true},
    {0x589380, "\xe8\xdb\xf0\xff\xff", 5, bloodroadConsume, true},
    {0x4960a0, "\x53\x56\x33\xdb\x53\x8b\xf1", 7, defeatUnitReset},
    {0x4be790, "\x6a\xff\x68\x42\xfd\x72\x00", 7, defeat},
    {0x4bef94, "\xe8\xe7\x22\xff\xff", 5, captureAttempt, true},
    {0x4b1572, "\xe8\x79\x7b\xfd\xff", 5, captureBaseHas, true},
    {0x4b15bc, "\xff\x52\x58\x89\x44\x24\x20", 7, captureHasBridge},
    {0x4b184a, "\x89\x44\x24\x18\xb8\x56\x55\x55\x55", 9, captureGuaranteeBridge},
    {0x4b18ac, "\xe8\x1f\x09\xfc\xff", 5, captureRollBridge, true},
    {0x4b17f1, "\xe8\xfa\x78\xfd\xff", 5, ironwallCaptureHasBridge, true},
    {0x4b17b4, "\xe8\x37\x79\xfd\xff", 5, luckyCaptureHas, true},
    {0x4b17bb, "\x0f\x85\x0a\x01\x00\x00", 6, luckyCaptureTakenBridge},
    {0x4a9724, "\xe8\xc7\x01\xfe\xff", 5, captureStatus, true},
    {0x4beeaf, "\xff\x50\x58\x85\xc0", 5, bloodroadHasBridge},
    {0x4beeb4, "\x0f\x85\x73\x01\x00\x00", 6, bloodroadTakenBridge},
    {0x4bf186, "\xe8\x45\xdb\xff\xff", 5, defeatCleanup, true},
    {0x586bea, "\xff\x52\x5c\x50\xb9\x58\x19\x20\x07", 9, pincerFindBridge},
    {0x586c2b, "\xe8\x40\x7e\xf2\xff", 5, pincerStatus, true},
    {0x586eeb, "\xff\x52\x5c\x50\xb9\x58\x19\x20\x07", 9, doubleFindBridge},
    {0x586f10, "\xe8\xbb\xb2\xee\xff", 5, doubleRollBridge, true},
    {0x586f22, "\xb9\x81\x01\x00\x00", 5, doubleZeroBridge},
    {0x586f64, "\xc7\x84\x24\x98\x0c\x00\x00\x04\x00\x00\x00", 11, doubleTagBridge},
    {0x586ffc, "\xe8\x9f\xf0\xff\xff", 5, doubleChild, true},
    {0x5b0331, "\xff\x52\x58\x85\xc0", 5, counterWaterBridge},
    {0x5b0361, "\xff\x50\x58\x85\xc0", 5, counterLandBridge},
    {0x584c10, "\x81\xec\xd4\x09\x00\x00", 6, counterReaction},
    {0x584d70, "\x81\xec\xd4\x09\x00\x00", 6, counterGroupReaction},
    {0x584c7e, "\xe8\x8d\xfe\xff\xff", 5, counterBuilderBridge, true},
    {0x584e52, "\xe8\xb9\xfc\xff\xff", 5, counterBuilderBridge, true},
    {0x584d3e, "\xe8\x2d\xcb\x02\x00", 5, counterReactionApply, true},
    {0x584caf, "\xe8\xc0\x2d\x18\x00", 5, counterScaleBridge, true},
    {0x584510, "\x8b\x44\x24\x04\x8b\x10", 6, criticalAggregate},
    {0x5866a5, "\xe8\xca\x13\x18\x00", 5, criticalAggregateScaleBridge, true},
    {0x587170, "\x55\x8b\xec\x83\xe4\xf8", 6, assistAttempt},
    {0x58721a, "\xe8\xf1\xe2\xff\xff", 5, assistChanceBridge, true},
    {0x585558, "\xe8\x93\x3b\xf0\xff", 5, assistHas, true},
    {0x585564, "\xe8\x77\x34\xf0\xff", 5, assistDislike, true},
    {0x587220, "\xe8\xab\xaf\xee\xff", 5, assistRoll, true},
    {0x587246, "\xb9\x81\x01\x00\x00", 5, assistOwnerZeroBridge},
    {0x58726c, "\xe8\x0f\x94\x02\x00", 5, assistPacket, true},
    {0x58660e, "\x33\xc0\xb9\x20\x00\x00\x00", 7, ironwallAggregateResetBridge},
    {0x586568, "\xff\x52\x5c\x50\xb9\x58\x19\x20\x07", 9, ironwallFindBridge},
    {0x586589, "\xc7\x84\x24\xd8\x0a\x00\x00\x01\x00\x00\x00", 11, ironwallLimitBridge},
    {0x586bb0, "\xe8\xbb\xac\x02\x00", 5, ironwallApply, true},
    {0x5b0e90, "\x8b\x4c\x24\x0c\x8b\x01", 6, ambushProducer},
    {0x5b03ba, "\xe8\x41\xdc\xff\xff", 5, defenseDecisionBridge},
    {0x5b0b4e, "\xff\x50\x58\x85\xc0", 5, assistUnyieldingBridge},
    {0x5b0b7b, "\xff\x52\x58\x85\xc0", 5, assistDiamondBridge},
    {0x5ae0d3, "\xff\x50\x58\x85\xc0", 5, defenseUnyieldingBridge},
    {0x5ae10f, "\xff\x52\x58\x85\xc0", 5, defenseDiamondBridge},
    {0x5af47b, "\xff\x52\x58\x85\xc0", 5, strategyCharmHasBridge},
    {0x5af49a, "\x8b\x44\x24\x10\x03\xc0", 6, strategyCharmDoubleBridge},
    {0x593a75, "\xe8\x26\xf9\xff\xff", 5, strategyFollowup, true},
    {0x5933e2, "\xe8\x59\x07\xef\xff", 5, strategyFollowupTarget, true},
    {0x5936a0, "\xff\x50\x5c\x50\xb9\x58\x19\x20\x07", 9, strategyChainFindBridge},
    {0x5939e2, "\xff\x50\x5c\x50\xb9\x58\x19\x20\x07", 9, strategyReflectFindBridge},
    {0x5933a0, "\x83\xec\x0c\x53\x55\x56", 6, strategyCommand},
    {0x5abf62, "\xff\x52\x58\x85\xc0", 5, aiWeightHasBridge, true},
    {0x5abf6f, "\xff\x50\x58\x85\xc0", 5, aiWeightHasBridge, true},
    {0x5abf7c, "\xff\x52\x58\x85\xc0", 5, aiWeightHasBridge, true},
    {0x5abf89, "\xff\x50\x58\x85\xc0", 5, aiWeightHasBridge, true},
    {0x5abf96, "\xff\x52\x58\x85\xc0", 5, aiWeightHasBridge, true},
    {0x5abfa3, "\xff\x50\x58\x85\xc0", 5, aiWeightHasBridge, true},
    {0x5abfb0, "\xff\x52\x58\x85\xc0", 5, aiWeightHasBridge, true},
    {0x5abfbd, "\xff\x50\x58\x85\xc0", 5, aiWeightHasBridge, true},
    {0x5abfc4, "\xbd\x00\x04\x00\x00", 5, aiWeightSelectedBridge},
    {0x591433, "\xe8\xa8\x73\xff\xff", 5, strategyFireApply, true},
    {0x588808, "\xe8\x73\x55\xff\xff", 5, strategyFirePacketBridge},
    {0x5924cb, "\xe8\xa0\xe2\x00\x00", 5, strategyFireTurnsBridge, true},
    {0x593424, "\xe8\xf7\xc3\x01\x00", 5, strategyCriticalAttempt, true},
    {0x5af834, "\xe8\xe7\xfc\xff\xff", 5, strategyCriticalChance, true},
    {0x5af5ce, "\xe8\x9d\x18\xee\xff", 5, strategyCriticalTarget, true},
    {0x5af5f3, "\xff\x52\x58\x85\xc0", 5, strategyCriticalGodBridge},
    {0x5af600, "\xff\x50\x58\x85\xc0", 5, strategyDeepBridge},
    {0x5af71a, "\xff\x52\x58\x85\xc0", 5, strategyAmbushBridge},
    {0x5af60d, "\xe8\x6e\x60\xee\xff", 5, strategyCriticalMaximum, true},
    {0x5af626, "\xe8\x55\x60\xee\xff", 5, strategyCriticalMaximum, true},
    {0x5af509, "\xe8\x72\xf4\xff\xff", 5, strategyChance, true},
    {0x5ae9f5, "\xe8\x76\x24\xee\xff", 5, strategyDefender, true},
    {0x593402, "\xe8\xe9\xc0\x01\x00", 5, strategyAttempt, true},
    {0x5aea3b, "\xff\x50\x58\x85\xc0", 5, strategyInsightBridge},
    {0x5aebb2, "\xff\x52\x58\x85\xc0", 5, strategyFireImmunityBridge},
    {0x5aed04, "\xff\x52\x58\x85\xc0", 5, strategyDisciplineBridge},
    {0x5aed15, "\xff\x50\x58\x85\xc0", 5, strategyMirrorDisciplineBridge},
    {0x5aee86, "\xff\x52\x58\x85\xc0", 5, strategyCalmBridge},
    {0x5aee97, "\xff\x50\x58\x85\xc0", 5, strategyMirrorCalmBridge},
    {0x4ae612, "\xe8\x39\x7d\xfe\xff", 5, statusTurnsSet, true},
    {0x593b40, "\x81\xec\xd0\x03\x00\x00", 6, dashingAttempt},
    {0x5977f0, "\x56\x8b\x74\x24\x0c", 5, mightyAttempt},
    {0x593b84, "\xe8\x77\xcf\xef\xff", 5, afterAttackProvider, true},
    {0x597826, "\xe8\xd5\x92\xef\xff", 5, afterAttackProvider, true},
    {0x593be1, "\xe8\x8a\xae\xf1\xff", 5, dashingStatus, true},
    {0x595250, "\x81\xec\xac\x00\x00\x00", 6, areaFriendlyHandler},
    {0x595377, "\xff\x50\x58\x85\xc0", 5, areaFriendlyHasBridge},
    {0x595388, "\x0f\x85\x00\x01\x00\x00", 6, areaFriendlyTakenBridge},
    {0x5dea30, "\x8b\x44\x24\x08\x55", 5, rawPriorityCompare},
    {0x5e9cb9, "\xe8\x52\xc9\x00\x00", 5, tribalAffinityBridge, true},
    {0x5ed9cd, "\xe8\x3e\x8c\x00\x00", 5, tribalAffinityBridge, true},
    {0x59775e, "\xe8\x2d\xdf\xf0\xff", 5, mightyInjury, true},
    {0x59721b, "\xff\x50\x58\x85\xc0", 5, injuryGuardBridge},
    {0x597297, "\xe8\x54\x1e\xef\xff", 5, injuryExclude, true},
    {0x59736d, "\xe8\x7e\xfe\xff\xff", 5, casualtyOrdinaryDeathBridge, true},
    {0x59742c, "\xe8\xbf\xfd\xff\xff", 5, casualtyOrdinaryInjuryBridge, true},
    {0x59750d, "\xe8\xde\xfc\xff\xff", 5, casualtyTacticInjuryBridge, true},
    {0x59760d, "\xe8\xde\xfb\xff\xff", 5, casualtyTacticDeathBridge, true},
    {0x5831e4, "\xe8\xc7\xfd\xff\xff", 5, fireAftermath, true},
    {0x582fe6, "\xff\x50\x58\x85\xc0", 5, fireAftermathGuardBridge},
    {0x58300b, "\xff\x52\x58\x85\xc0", 5, fireAftermathGodBridge},
    {0x5960d0, "\x81\xec\xa0\x00\x00\x00", 6, trapLine},
    {0x5979d0, "\x81\xec\x04\x01\x00\x00", 6, trapDirect},
    {0x597a60, "\x6a\xff\x68\x5b\x3a\x73\x00", 7, trapCell},
    {0x592440, "\x6a\xff\x68\xbb\x39\x73\x00", 7, trapFire},
    {0x592538, "\xe8\x63\xbf\xf1\xff", 5, trapFireUnitGrantBridge},
    {0x592636, "\xe8\x25\xbd\xf1\xff", 5, trapFireHpGrantBridge},
    {0x5b15a0, "\x53\x55\x56\x8b\x74\x24\x10", 7, trapBuild},
    {0x5b1190, "\x53\x55\x56\x8b\x74\x24\x10", 7, trapAuxBuild},
    {0x5b17d8, "\x89\x45\x00\xc7\x46\x64\x06\x00\x00\x00", 10, trapStepBridge},
    {0x59787a, "\xe8\x21\x6c\xf1\xff", 5, trapUnitGrantBridge},
    {0x597de5, "\xe8\x76\x65\xf1\xff", 5, trapHpGrantBridge},
    {0x597e02, "\xe8\xb9\x65\xf1\xff", 5, trapBaseGrantBridge},
    {0x5b040a, "\xff\x52\x58\x85\xc0", 5, vineHasBridge},
    {0x5b0414, "\x99\x2b\xc2\xd1\xf8", 5, vineStoreBridge},
    {0x5b0c0f, "\xff\x52\x58\x85\xc0", 5, vineAssistHasBridge},
    {0x5b0c19, "\x99\x2b\xc2\xd1\xf8", 5, vineAssistStoreBridge},
    {0x49517b, "\xb8\x01\x00\x00\x00", 5, strategyCostForcedBridge},
    {0x59343b, "\xe8\x30\x1d\xf0\xff", 5, strategyCost, true},
    {0x593445, "\xe8\xa6\x30\xf0\xff", 5, strategyCostGrant, true},
    {0x579e40, "\x81\xec\x40\x06\x00\x00", 6, criticalPositionOne},
    {0x57ad70, "\x81\xec\x04\x06\x00\x00", 6, criticalPositionTwo},
    {0x592a10, "\x55\x8b\xec\x83\xe4\xf8", 6, criticalOwner},
    {0x592a25, "\xb9\x81\x01\x00\x00", 5, criticalOwnerZeroBridge},
    {0x5b1c6c, "\xe8\x2f\xc8\xef\xff", 5, criticalQueuedTroopsBridge},
    {0x5b1a74, "\xe8\x47\xc9\xef\xff", 5, criticalBaseTroopsBridge},
    {0x5b1aaf, "\xe8\xac\xc8\xef\xff", 5, criticalBaseHpBridge},
    {0x5afc30, "\x8b\x54\x24\x04\x57", 5, criticalResetBridge},
    {0x5b0680, "\x83\xec\x28\x53\x8b\x5c\x24\x34", 8, criticalCounterResetBridge},
    {0x5b0ee0, "\x83\xec\x08\x53\x55", 5, criticalExtraResetBridge},
    {0x585780, "\x55\x8b\xec\x83\xe4\xf8", 6, criticalHandlerOne},
    {0x5860a0, "\x55\x8b\xec\x83\xe4\xf8", 6, criticalHandlerTwo},
    {0x5857b8, "\xb9\x81\x01\x00\x00", 5, criticalCopyOneBridge},
    {0x5860d0, "\xb9\x81\x01\x00\x00", 5, criticalCopyTwoBridge},
    {0x5afc70, "\x83\xec\x24\x53\x55\x56", 6, criticalProducer},
    {0x5aff6d, "\xe8\x9e\xe6\xff\xff", 5, criticalDecisionBridge},
    {0x495680, "\x51\x53\x55\x8b\x6c\x24\x14", 7, criticalMaximum},
    {0x4956fb, "\xe8\x30\x39\xff\xff", 5, criticalAttribute, true},
    {0x5ae762, "\xb8\x01\x00\x00\x00", 5, criticalForcedBridge},
    {0x5ae779, "\xb8\x01\x00\x00\x00", 5, criticalSiegeForcedBridge},
    {0x584fdf, "\xe8\xbc\x94\xf2\xff", 5, criticalTroopsBridge},
    {0x5a35d7, "\xff\x52\x58\x85\xc0", 5, constructionHasBridge},
    {0x5850b2, "\xe8\x69\xe4\x01\x00", 5, constructionAmount, true},
    {0x5850ca, "\xe8\x91\x92\xf2\xff", 5, constructionGrant, true},
    {0x5b2250, "\x81\xec\xd4\x03\x00\x00", 6, diplomacy16},
    {0x58308a, "\xe8\x21\xda\xff\xff", 5, luckyUnitEventBridge, true},
    {0x50aada, "\xe8\xd1\xf0\xff\xff", 5, luckyDuelRoll, true},
    {0x50ab5c, "\xe8\x4f\xf0\xff\xff", 5, luckyDuelRoll, true},
    {0x50a529, "\xe8\x02\xec\xff\xff", 5, luckyDuelPostFirst, true},
    {0x50a5a6, "\xe8\x95\xec\xff\xff", 5, luckyDuelPostSecond, true},
    {0x509a90, "\x56\x8b\xf1\x8d\x86\x78\x05\x00\x00", 9, luckyDuelResponse},
    {0x5b7b72, "\xe8\x29\xfe\xff\xff", 5, luckyMissionFirst, true},
    {0x5bffab, "\xe8\xa0\xf6\xff\xff", 5, luckyMissionSecond, true},
    {0x5d1550, "\xe8\x9b\xf0\xff\xff", 5, luckyMissionThird, true},
    {0x5b1fdf, "\x8b\x4e\x08\x8b\x41\x04", 6, diplomacyEligibilityBridge},
    {0x5b6510, "\x81\xec\xd8\x03\x00\x00", 6, diplomacy18},
    {0x5cc650, "\x81\xec\xd0\x03\x00\x00", 6, diplomacy20},
    {0x5b22a7, "\xff\x52\x14\x8b\xe8", 5, diplomacy16DecisionBridge},
    {0x5b6539, "\xff\x52\x14\x8b\xe8", 5, diplomacy18DecisionBridge},
    {0x5cc6bc, "\xff\x52\x14\x8b\xce", 5, diplomacy20DecisionBridge},
    {0x5b2128, "\xb8\x01\x00\x00\x00", 5, diplomacy16ForcedBridge},
    {0x5b63ae, "\xb8\x01\x00\x00\x00", 5, diplomacy18ForcedBridge},
    {0x5cc511, "\xb8\x01\x00\x00\x00", 5, diplomacy20ForcedBridge},
    {0x5b22c8, "\xe8\xc3\xb4\x00\x00", 5, diplomacyInteraction, true},
    {0x5bd7bf, "\xe8\x8c\xe6\xeb\xff", 5, diplomacyInteractionState, true},
    {0x689dd0, "\x8b\x4c\x24\x04\x8b\x81\x3c\x01\x00\x00", 10, diplomacyContestFactory},
    {0x68ac10, "\xe8\xeb\xf0\xff\xff", 5, diplomacyContestBridge, true},
    {0x68b95d, "\xe8\x9e\xe3\xff\xff", 5, diplomacyContestBridge, true},
    {0x68c06b, "\xe8\x90\xdc\xff\xff", 5, diplomacyContestBridge, true},
    {0x5b655a, "\xe8\x31\x72\x00\x00", 5, diplomacyInteraction, true},
    {0x5cc6d9, "\xe8\xb2\x10\xff\xff", 5, diplomacyInteraction, true},
    {0x58f4e0, "\x55\x8b\xec\x83\xe4\xf8", 6, environmentRun},
    {0x58f685, "\xff\x50\x5c\x3b\xc5", 5, harvestFindBridge},
    {0x58f695, "\xe8\x36\x2b\xee\xff", 5, harvestRoll, true},
    {0x58f6aa, "\xe8\x81\xaa\xf2\xff", 5, harvestAppend, true},
    {0x58f74b, "\xe8\x30\xbe\xee\xff", 5, harvestFlag, true},
    {0x58f0c7, "\xff\x52\x5c\x85\xc0", 5, environmentNewBridge},
    {0x58f283, "\xff\x52\x5c\x85\xc0", 5, environmentActiveBridge},
    {0x58f3b6, "\xff\x50\x5c\x85\xc0", 5, environmentSpreadBridge},
    {0x5d8f2d, "\xe8\xee\xe8\xff\xff", 5, researchGoldCostBridge, true},
    {0x5d8f42, "\xe8\x59\xac\xed\xff", 5, researchGoldGrantBridge, true},
    {0x5d5a54, "\xe8\x47\xc4\xff\xff", 5, explorationOutcome, true},
    {0x5d5ab8, "\xe8\xe3\xfb\xff\xff", 5, discoveryHandler, true},
    {0x5d5703, "\xe8\x18\x04\xed\xff", 5, discoveryStore, true},
    {0x4a70d0, "\x81\xec\x68\x04\x00\x00", 6, guidanceAbility},
    {0x4a6db0, "\x81\xec\xf4\x04\x00\x00", 6, guidanceWeapon},
    {0x48a273, "\x8b\xaf\xe8\x00\x00\x00", 6, spouseNativeBridge},
    {0x48a2e2, "\xff\x52\x4c\x6a\xff\x88\x84\x3e\x70\x01\x00\x00", 12, spouseCurrentBridge},
    {0x48a2f3, "\xff\x50\x4c\x88\x84\x3e\x75\x01\x00\x00", 10, spouseBaseBridge},
    {0x58e5cc, "\xff\x52\x5c\x3b\xc7", 5, loyaltyFindBridge},
    {0x58e61c, "\x39\x7c\x24\x10\x0f\x85\x95\x01\x00\x00", 10, loyaltySkipBridge},
    {0x58e9f8, "\xe8\xe3\x00\xf4\xff", 5, unrestCityFlags, true},
    {0x58ea05, "\xe8\x56\xe5\xff\xff", 5, unrestRollBridge, true},
    {0x58cf9f, "\xf7\xd8\x1b\xc0\x83\xe0\xec\x83\xc0\x50", 10, unrestThresholdBridge},
    {0x58ea5a, "\xe8\x31\xd1\xff\xff", 5, unrestTribeBridge, true},
    {0x58eaa5, "\xe8\xb6\xd1\xff\xff", 5, unrestCampBridge, true},
    {0x59053d, "\x8b\x84\x24\xb4\x01\x00\x00", 7, goldRevenueSeedBridge},
    {0x5905ee, "\x8b\x84\x24\xb8\x01\x00\x00", 7, foodRevenueSeedBridge},
    {0x590558, "\xe8\x43\xdd\xf1\xff", 5, cityGoldRevenue, true},
    {0x590609, "\xe8\xf2\xdc\xf1\xff", 5, cityFoodRevenue, true},
    {0x5906d1, "\xe8\xca\xdb\xf1\xff", 5, portGoldRevenue, true},
    {0x590726, "\xe8\xd5\xdb\xf1\xff", 5, portFoodRevenue, true},
    {0x599649, "\xff\x50\x58\x85\xc0", 5, offMonthTaxBridge},
    {0x59968b, "\xe8\x10\x4c\xf1\xff", 5, offMonthCityTax, true},
    {0x5996e6, "\xe8\xb5\x4b\xf1\xff", 5, offMonthPortTax, true},
    {0x4bf16a, "\xe8\x11\x74\xff\xff", 5, precisionReward, true},
    {0x4b64c6, "\xe8\xb5\xb5\xfc\xff", 5, precisionPoints, true},
    {0x59bfd4, "\x3d\x4b\x04\x00\x00\x7e\x39", 7, tuntianBridge},
    {0x590527, "\xe8\xb4\xe5\xf3\xff", 5, cityIncomeFlags, true},
    {0x590576, "\xe8\x25\xdd\xf1\xff", 5, goldIncomeBonus, true},
    {0x5906ef, "\xe8\xac\xdb\xf1\xff", 5, goldIncomeBonus, true},
    {0x590635, "\xe8\xc6\xdc\xf1\xff", 5, foodIncomeBonus, true},
    {0x590744, "\xe8\xb7\xdb\xf1\xff", 5, foodIncomeBonus, true},
    {0x5c6710, "\xe8\xbb\xfc\xff\xff", 5, productionAmount, true},
    {0x5c6720, "\xe8\x1b\xd9\xee\xff", 5, productionGrant, true},
    {0x5c3b8e, "\xe8\x7d\xfa\xff\xff", 5, recruitAmount, true},
    {0x5c3ba4, "\xe8\x17\xa8\xee\xff", 5, recruitGrant, true},
    {0x5c704f, "\xe8\x4c\xfc\xff\xff", 5, productionTime, true},
    {0x5c70c3, "\xe8\x98\xe5\xed\xff", 5, productionAssignTime, true},
    {0x584631, "\xe8\xca\xc4\xf0\xff", 5, heartActorLookup, true},
    {0x58465a, "\xe8\x41\x9e\xf2\xff", 5, heartTroops, true},
    {0x584721, "\xe8\xda\xc3\xf0\xff", 5, rageActorLookup, true},
    {0x5847ce, "\xe8\x2d\xc3\xf0\xff", 5, rageActorLookup, true},
    {0x584737, "\xe8\xb4\x1d\xf1\xff", 5, rageEnergy, true},
    {0x5847e4, "\xe8\x07\x1d\xf1\xff", 5, rageEnergy, true},
    {0x4bec08, "\xe8\xe3\x78\xfd\xff", 5, elationEnergy, true},
    {0x59a2b4, "\xff\x52\x5c\x89\x44\x24\x18", 7, musicFindBridge},
    {0x59a2c1, "\xff\x50\x5c\x8b\x16\x8b\xce", 7, poetryFindBridge},
    {0x59a3c8, "\xe8\x23\xc1\xef\xff", 5, poetryEnergy, true},
    {0x59a410, "\xe8\xdb\xc0\xef\xff", 5, musicEnergy, true},
    {0x584ff7, "\xe8\xf4\x14\xf1\xff", 5, pressureEnergyBridge},
    {0x5848a4, "\xe8\x57\x9c\x02\x00", 5, movementHazard, true},
    {0x496efa, "\xe8\x71\xf6\xff\xff", 5, movementStatsBridge, true},
    {0x4890f0, "\x8b\x44\x24\x04\x85\xc0", 6, personHas},
    {0x4899d0, "\x8b\x44\x24\x04\x83\xf8\xff", 7, personSet},
    {0x47a8a0, "\x8b\x54\x24\x04\x8b\x01", 6, unitHas},
    {0x490c50, "\x8b\x44\x24\x04\x85\xc0", 6, skillRecord},
    {0x4915e0, "\x56\x8b\x74\x24\x08", 5, skillId},
    {0x4945d0, "\x8b\x01\xff\x50\x28", 5, skillOrder},
    // Native officer-list sorting must use the skill ID rather than the original 100-entry order table.
    {0x4c93e2, "\x6a\x01", 2, nullptr, false, 0, "\x6a\x05"},
    {0x48b97c, "\xe8\xef\xef\xff\xff", 5, serializeSkill, true},
    {0x4cea41, "\x85\xc0\x7c\x0c\x83\xf8\x63\x7f\x07\xc7\x04\x83\x01\x00\x00\x00", 16, incomeBridge},
    {0x4cf711, "\x0f\x85\x90\x00\x00\x00", 6, boleBridge},
};
bool verify() {
    if(!verifyStrategyStatusLayout())return false;
    if(!verifyStrategyAmbushLayout())return false;
    if(!verifyResearchGoldLayout())return false;
    if(!verifyDomesticLayout())return false;
    if(!verifyDiplomacyEligibilityLayout())return false;
    if(!verifyStrategyGroundLayout())return false;
    if(!verifyLuckyUnitEventLayout())return false;
    if(!verifyLuckyDuelLayout())return false;
    if(!verifyLuckyDuelPostLayout())return false;
    if(!verifyLuckyDuelResponseLayout())return false;
    if(!verifyLuckyMissionLayout())return false;
    if(!verifyDiplomacyInteractionLayout())return false;
    if(!verifyDiplomacyContestLayout())return false;
    if(!verifyNativeSkillQueryLayout())return false;
    if(!verifyStrategyFireImmunityLayout())return false;
    if(!verifyAreaFriendlyLayout())return false;
    if(!verifyRawPriorityLayout())return false;
    if(!verifyTribalAffinityLayout())return false;
    for (auto& h : g_hooks) {
        int immunityEffect=h.target==strategyInsightBridge?61:h.target==strategyDisciplineBridge?72:
            h.target==strategyCalmBridge?73:(h.target==strategyMirrorDisciplineBridge || h.target==strategyMirrorCalmBridge)?74:-1;
        if(immunityEffect>=0) {
            const uint8_t* p=(const uint8_t*)h.at;
            uint32_t nativeSite=h.target==strategyInsightBridge?0x5aea3b:h.target==strategyDisciplineBridge?0x5aed04:
                h.target==strategyMirrorDisciplineBridge?0x5aed15:h.target==strategyCalmBridge?0x5aee86:0x5aee97;
            int32_t offset=*(const int32_t*)(p+7);
            if(p[-4]!=0x6a || p[-3]!=immunityEffect || p[5]!=0x0f || p[6]!=0x85 ||
               offset!=(int32_t)(0x5af26f-nativeSite-11)) {
                if(g_log)g_log("skills: unexpected strategy immunity branch at %08x; rejected",h.at);
                return false;
            }
        }
        if((h.target==dashingAttempt && memcmp((const void*)(h.at+0x37),"\x6a\x31",2)) ||
           (h.target==mightyAttempt && memcmp((const void*)(h.at+0x29),"\x6a\x33",2)) ||
           (h.target==injuryGuardBridge && memcmp((const void*)(h.at+5),"\x74\x11",2)) ||
           (h.target==injuryExclude && memcmp((const void*)(h.at+5),"\x85\xc0\x75\x0a",4))) {
            if(g_log)g_log("skills: unexpected after-attack skill/branch at %08x; rejected",h.at);
            return false;
        }
        bool positionLayout=(h.target!=criticalPositionOne ||
            (!memcmp((const void*)(h.at+0xcd),"\x8d\x54\x24\x5c",4) &&
             !memcmp((const void*)(h.at+0xf1),"\x8d\x4c\x24\x44",4) &&
             !memcmp((const void*)(h.at+0x18c),"\x8d\x6c\x24\x4c",4))) &&
            (h.target!=criticalPositionTwo ||
            (!memcmp((const void*)(h.at+0xd2),"\x8d\x54\x24\x24",4) &&
             !memcmp((const void*)(h.at+0xdf),"\x8d\x44\x24\x0c",4) &&
             !memcmp((const void*)(h.at+0x127),"\x8d\x44\x24\x24",4) &&
             !memcmp((const void*)(h.at+0x134),"\x8d\x4c\x24\x0c",4)));
        if(!positionLayout) {
            if(g_log)g_log("skills: unexpected critical source stack layout at %08x; rejected",h.at);
            return false;
        }
        if(h.target==criticalOwnerZeroBridge && memcmp((const void*)(h.at+5),"\x8d\x7c\x24\x14\xf3\xab",6)) {
            if(g_log)g_log("skills: unexpected critical owner initialization at %08x; rejected",h.at);
            return false;
        }
        if((h.target==criticalCopyOneBridge || h.target==criticalCopyTwoBridge) &&
           (memcmp((const void*)(h.at+5),"\x8d\xbc\x24",3) || memcmp((const void*)(h.at+12),"\xf3\xa5",2))) {
            if(g_log)g_log("skills: unexpected critical copy continuation at %08x; rejected",h.at);
            return false;
        }
        if (memcmp((void*)h.at, h.expected, h.size)) {
            // M10 changes this immediate before hooks install. Accept only its paired
            // person-getter bound; keep the comparison opcode and branch bytes exact.
            const uint8_t* p=(const uint8_t*)h.at;
            if(h.target==tuntianBridge && p[0]==0x3d && !memcmp(p+5,h.expected+5,2)) {
                int bound=*(const int*)(p+1);
                if(bound>=1099 && bound<65535 && bound==*g_personLimitOperand)continue;
            }
            if (g_log) g_log("skills: unexpected instructions at %08x; rejected (another mod?)", h.at);
            return false;
        }
    }
    return verifyStrategyMaximumSites() && verifyStrategyCriticalLayout() && verifyStrategyFireLayout() && verifyStrategyFollowupLayout() && verifyStrategyCharmLayout() && verifyDefenseLayout() && verifyIronwallLayout() && verifyAssistLayout() && verifyCounterImmunityLayout() && verifyDoubleAttackLayout() && verifyPincerLayout() && verifyCaptureLayout() && verifyBloodroadHandoffLayout() && verifyPlunderLayout() && verifyMagicLayout() && verifyReachLayout() && verifySteeringLayout() && verifyAttackReachLayout() && verifyZocLayout() && verifyBoleLayout() && verifyPrecisionLayout() && verifyFireAftermathLayout() && verifyTrapLayout() && verifyVineLayout() && verifyAIWeightLayout();
}
} // namespace

void skillsSetSteeringDescribe(SkillSteerDescribe describe) {g_steeringDescribe=describe;}

void skillsTelemetryStream(int mode,int kind) {
    if(mode!=1 || kind==4 || kind==0x18)return;
    skilltelemetry::invalidateContext();
    AcquireSRWLockExclusive(&g_criticalCacheLock);criticalSyncLocked();ReleaseSRWLockExclusive(&g_criticalCacheLock);
    AcquireSRWLockExclusive(&g_strategyFireLock);strategyFireSyncLocked();ReleaseSRWLockExclusive(&g_strategyFireLock);
    AcquireSRWLockExclusive(&g_bloodroadLock);bloodroadSyncLocked();ReleaseSRWLockExclusive(&g_bloodroadLock);
}

bool skillsPrepare(const wchar_t* path, TraceLogFn log) {
    g_log = log; g_prepared = false;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") || !f) {
        if (log) log("skills: cannot open %ls", path);
        return false;
    }
    std::string error;
    bool ok = parse(f, error); fclose(f);
    if (!ok) { if (log) log("skills: %s; rejected", error.c_str()); return false; }
    if (!verify()) return false;
    g_prepared = true;
    if (log) log("skills: definitions verified, count=%d (original effects + Bole 146 + merchant 147)", g_count);
    return true;
}

bool skillsInstall() {
    if (!g_prepared || !verify()) return false;
    // Acquire write access for all sites before modifying any of them.
    size_t acquired = 0;
    for (auto& h : g_hooks) {
        if (!VirtualProtect((void*)h.at, h.size, PAGE_EXECUTE_READWRITE, &h.protection)) {
            for (size_t i = 0; i < acquired; ++i) {
                DWORD old; VirtualProtect((void*)g_hooks[i].at, g_hooks[i].size, g_hooks[i].protection, &old);
            }
            if (g_log) g_log("skills: could not make patch sites writable; nothing installed");
            return false;
        }
        ++acquired;
    }
    for (auto& h : g_hooks) {
        uint8_t* p = (uint8_t*)h.at;
        if(h.target==tuntianBridge)g_tuntianMax=*(const int*)(p+1);
        if (h.replacement) memcpy(p, h.replacement, h.size);
        else {
            p[0] = h.call ? 0xe8 : 0xe9;
            *(int32_t*)(p + 1) = (int32_t)((uintptr_t)h.target - h.at - 5);
            memset(p + 5, 0x90, h.size - 5);
        }
    }
    for (auto& h : g_hooks) {
        DWORD old; VirtualProtect((void*)h.at, h.size, h.protection, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)h.at, h.size);
    }
    g_prepared = false;
    if (g_log) g_log("skills: installed %u hooks, count=%d", (unsigned)(sizeof g_hooks / sizeof *g_hooks), g_count);
    return true;
}
