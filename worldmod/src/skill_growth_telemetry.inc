typedef int(__cdecl* GrowthValid)(void*);
GrowthValid g_growthValid=(GrowthValid)0x47a630;
int growthPersonId(void* person) {
    int id=g_telemetryPersonId((void*)0x7201958,person);
    return id>=0 && g_telemetryPerson((void*)0x7201958,id)==person?id:-1;
}
PersonReceipt::PersonReceipt(const void* p):person(p),generation(skilltelemetry::contextGeneration()){
    if(!p)return;id=growthPersonId((void*)p);if(id<0)return;
    owned=*(const int*)((const uint8_t*)p+0xe8);memcpy(affiliation,(const uint8_t*)p+0x98,sizeof affiliation);
}
bool PersonReceipt::valid(int effect)const{
    return skilltelemetry::enabled() && person && id>=0 && generation==skilltelemetry::contextGeneration() &&
        g_telemetryPerson((void*)0x7201958,id)==person && growthPersonId((void*)person)==id &&
        !memcmp(affiliation,(const uint8_t*)person+0x98,sizeof affiliation) &&
        (effect<0 || (*(const int*)((const uint8_t*)person+0xe8)==owned && contains(owned,effect)));
}
void ProgressionProbe::matched(const void* p){
    if(receipt && generation!=skilltelemetry::contextGeneration())return;
    actor=p;if(receipt)*receipt=PersonReceipt(p);
}
skilltelemetry::Target growthTarget(uint8_t* person,int id) {
    skilltelemetry::Target target;target.person=id;
    target.force=(*(BuildingValue*)(*(uint8_t**)person+0x40))(person);
    return target;
}
uint32_t g_guidanceAbilityResume=0x4a70d6,g_guidanceWeaponResume=0x4a6db6;
__declspec(naked) void guidanceAbilityNative() {
    __asm {sub esp,0x468
           jmp dword ptr [g_guidanceAbilityResume]}
}
__declspec(naked) void guidanceWeaponNative() {
    __asm {sub esp,0x4f4
           jmp dword ptr [g_guidanceWeaponResume]}
}
typedef int(__thiscall* AbilityGrowth)(void*,uint8_t*,int,int,int);
typedef int(__thiscall* WeaponGrowth)(void*,uint8_t*,int,int);
AbilityGrowth g_abilityGrowthOriginal=(AbilityGrowth)guidanceAbilityNative;
WeaponGrowth g_weaponGrowthOriginal=(WeaponGrowth)guidanceWeaponNative;
const int* g_weaponExperienceThresholds=(const int*)0x7e81fc;
int weaponExperienceTotal(int level,int progress) {
    int total=progress;for(int i=0;i<level;++i)total+=g_weaponExperienceThresholds[i];return total;
}
int __fastcall guidanceAbility(void* self,void*,uint8_t* person,int kind,int requested,int refresh) {
    if(!skilltelemetry::enabled() || kind<0 || kind>4 || !g_growthValid(person))
        return g_abilityGrowthOriginal(self,person,kind,requested,refresh);
    int id=growthPersonId(person);
    if(id<0)return g_abilityGrowthOriginal(self,person,kind,requested,refresh);
    int before=*(const uint16_t*)(person+0x12a+kind*2);
    PersonReceipt recipient(person),provider;
    ProgressionProbe probe(84,&provider);
    int result=g_abilityGrowthOriginal(self,person,kind,requested,refresh);
    if(provider.valid(84) && recipient.valid()) {
        // Native 0x4a7153 caps the signed sum, then 0x48a810 stores a capped word.
        int normal=(int32_t)((uint32_t)before+(uint32_t)requested);if(normal>3000)normal=3000;
        normal=(uint16_t)normal;if(normal>3000)normal=3000;
        auto target=growthTarget(person,id);char context[96];
        sprintf_s(context,"{\"attribute_id\":%d,\"requested_experience\":%d,\"grant_caller\":%u}",
            kind,requested,(unsigned)(uintptr_t)_ReturnAddress());target.context=context;
        skilltelemetry::appliedBonus(probe.actor,provider.owned,84,"growth",
            "ability_experience_bonus","ability_experience",before,*(const uint16_t*)(person+0x12a+kind*2),normal,0x4a7169,target);
    }
    return result;
}
int __fastcall guidanceWeapon(void* self,void*,uint8_t* person,int kind,int requested) {
    if(!skilltelemetry::enabled() || kind<0 || kind>5 || !g_growthValid(person))
        return g_weaponGrowthOriginal(self,person,kind,requested);
    int id=growthPersonId(person),level=*(const int*)(person+0xb0+kind*4);
    if(id<0 || level<0 || level>3)return g_weaponGrowthOriginal(self,person,kind,requested);
    PersonReceipt recipient(person),provider;
    int before=person[0x134+kind],normalLevel=level;
    int normal=(int32_t)((uint32_t)before+(uint32_t)requested);
    if(level<3 && normal>=g_weaponExperienceThresholds[level]) {
        normal-=g_weaponExperienceThresholds[level];++normalLevel;
    }
    if(normalLevel>=3 || normal<=0)normal=0;else if(normal>255)normal=255;
    ProgressionProbe probe(84,&provider);
    int result=g_weaponGrowthOriginal(self,person,kind,requested);
    if(provider.valid(84) && recipient.valid()) {
        int afterLevel=*(const int*)(person+0xb0+kind*4),after=person[0x134+kind];
        if(afterLevel<0 || afterLevel>3)return result;
        auto target=growthTarget(person,id);char context[256];
        sprintf_s(context,"{\"aptitude_id\":%d,\"requested_experience\":%d,\"level_before\":%d,\"level_after\":%d,\"progress_before\":%d,\"progress_after\":%d,\"grant_caller\":%u}",
            kind,requested,level,afterLevel,before,after,(unsigned)(uintptr_t)_ReturnAddress());target.context=context;
        skilltelemetry::appliedBonus(probe.actor,provider.owned,84,"growth",
            "weapon_experience_bonus","weapon_experience_total",weaponExperienceTotal(level,before),
            weaponExperienceTotal(afterLevel,after),weaponExperienceTotal(normalLevel,normal),0x4a6ea1,target);
    }
    return result;
}

typedef int(__cdecl* ResearchGoldCost)(void**,uint8_t*);
ResearchGoldCost g_researchGoldCostOriginal=(ResearchGoldCost)0x5d7820;
IncomeGrant g_researchGoldGrantOriginal=(IncomeGrant)0x4b3ba0;
extern uintptr_t g_strategyCodeBase;
struct ResearchGoldPending {
    const void* actor=nullptr;int requested=0,normal=0;PersonReceipt provider;
    void** members=nullptr;void* memberSnapshot[3]={};uint8_t* research=nullptr;uint8_t* city=nullptr;
    void* base=nullptr;int baseId=-1,force=-1;uintptr_t frame=0;uint64_t generation=0;
    bool valid()const{
        return actor && generation==skilltelemetry::contextGeneration() && provider.valid(84) &&
            members && !memcmp(members,memberSnapshot,sizeof memberSnapshot) &&
            g_growthValid(research) && *(uint16_t*)(research+0x5e)==normal && g_growthValid(city) &&
            g_cityBuilding(city)==base && baseId>=0 && g_baseById((void*)0x7201958,baseId)==base &&
            g_telemetryBaseId((void*)0x7201958,base)==baseId &&
            (*(BuildingValue*)(*(uint8_t**)base+0x40))(base)==force;
    }
};
thread_local ResearchGoldPending g_researchGoldPending;
int __cdecl researchGoldCost(void** members,uint8_t* research,uint8_t* city=nullptr,uintptr_t caller=0,uintptr_t frame=0) {
    g_researchGoldPending=ResearchGoldPending{};
    if(!skilltelemetry::enabled())return g_researchGoldCostOriginal(members,research);
    ResearchGoldPending pending;pending.generation=skilltelemetry::contextGeneration();
    bool owned=caller==g_strategyCodeBase+0x1d8f32 && frame && members && g_growthValid(research) && g_growthValid(city);
    if(owned){
        pending.members=members;memcpy(pending.memberSnapshot,members,sizeof pending.memberSnapshot);
        pending.research=research;pending.city=city;pending.frame=frame;pending.normal=*(uint16_t*)(research+0x5e);
        pending.base=g_cityBuilding(city);
        if(g_growthValid(pending.base)){auto target=incomeTarget(pending.base);pending.baseId=target.base;pending.force=target.force;}
    }
    ProgressionProbe probe(84,owned?&pending.provider:nullptr);probe.scene="research";probe.event="research_gold_cost_eligibility";
    probe.nativeCaller=g_strategyCodeBase+0x1d786a;
    int result=g_researchGoldCostOriginal(members,research);
    if(owned && probe.actor){pending.actor=probe.actor;pending.requested=result;
        if(result==pending.normal/2 && pending.valid())g_researchGoldPending=pending;}
    return result;
}
int __fastcall researchGoldGrant(void* self,void*,uint8_t* city,int requested,void** members=nullptr,
    uint8_t* research=nullptr,uintptr_t caller=0,uintptr_t frame=0) {
    auto pending=g_researchGoldPending;
    bool owner=skilltelemetry::enabled() && pending.actor && caller==g_strategyCodeBase+0x1d8f47 && frame==pending.frame;
    if(owner)g_researchGoldPending=ResearchGoldPending{};
    bool observe=owner &&
        members==pending.members && research==pending.research && city==pending.city &&
        pending.normal/2==pending.requested && requested==-pending.requested && pending.valid();
    int before=observe?*(int*)(city+0x44):0;
    int result=g_researchGoldGrantOriginal(self,city,requested);
    if(observe && pending.valid()) {
        int after=*(int*)(city+0x44);
        if(after!=std::min<int64_t>(100000,std::max<int64_t>(0,(int64_t)before+requested)))return result;
        int normal=before-pending.normal;if(normal<0)normal=0;else if(normal>100000)normal=100000;
        auto target=incomeTarget(pending.base);char context[112];
        sprintf_s(context,"{\"base_gold_cost\":%d,\"requested_gold_cost\":%d}",pending.normal,pending.requested);target.context=context;
        skilltelemetry::appliedBonus(pending.actor,pending.provider.owned,84,
            "research","research_gold_cost_reduced","gold",before,after,normal,0x5d8f47,target);
    }
    return result;
}
__declspec(naked) void researchGoldCostBridge(){__asm {mov eax,esp
    push eax
    push [eax]
    push [eax+0x38]
    push [eax+8]
    push [eax+4]
    call researchGoldCost
    add esp,20
    ret}}
__declspec(naked) void researchGoldGrantBridge(){__asm {mov eax,esp
    push eax
    push [eax]
    push esi
    lea edx,[ebx+4]
    push edx
    push [eax+8]
    push [eax+4]
    call researchGoldGrant
    ret 8}}
const struct ResearchGoldLayout {uint32_t start;int size;const char* bytes;} g_researchGoldLayouts[]={
    {0x5d8f24,35,"\x8b\x74\x24\x20\x8d\x43\x04\x56\x50\xe8\xee\xe8\xff\xff\x83\xc4\x08\xf7\xd8\x50\x8b\x44\x24\x30\x50\xb9\x5c\x89\x99\x07\xe8\x59\xac\xed\xff"},
    {0x5d7850,41,"\x8b\x74\xbd\x00\x56\xe8\xd6\x2d\xea\xff\x83\xc4\x04\x85\xc0\x74\x12\x6a\x54\x8b\xce\xe8\x86\x18\xeb\xff\x85\xc0\x74\x05\xbb\x01\x00\x00\x00\x47\x83\xff\x03\x7c\xd7"},
};
bool verifyResearchGoldLayout(){
    for(const auto& s:g_researchGoldLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}

struct SpouseProbe;
thread_local SpouseProbe* g_spouseProbe=nullptr;
struct SpouseProbe {
    uint8_t* person;
    int kind;
    const void* actor=nullptr;
    PersonReceipt recipient,provider;uint8_t* partner=nullptr;int partnerId=-1,spouseId=-1,partnerSpouseId=-1;
    int normal=0;
    SpouseProbe* previous;
    SpouseProbe(uint8_t* p,int k):person(p),kind(k),recipient(p),spouseId(*(int*)(p+0x60)),previous(g_spouseProbe){g_spouseProbe=this;}
    ~SpouseProbe(){g_spouseProbe=previous;}
};
struct SpousePending {
    uint8_t* person=nullptr;const void* actor=nullptr;int kind=0,cache=0,before=0,normal=0;
    PersonReceipt recipient,provider;uint8_t* partner=nullptr;int partnerId=-1,spouseId=-1,partnerSpouseId=-1;
    bool valid()const{
        return recipient.valid() && provider.valid(99) && partner && partnerId>=0 &&
            *(int*)(person+0x60)==spouseId && g_telemetryPerson((void*)0x7201958,partnerId)==partner &&
            growthPersonId(partner)==partnerId && *(int*)(partner+0x60)==partnerSpouseId;
    }
};
thread_local SpousePending g_spousePending;
void __cdecl spouseNative(uint8_t* person,uint8_t* spouse,int kind,int value) {
    auto probe=g_spouseProbe;
    if(!probe || probe->person!=person || probe->kind!=kind)return;
    // Observe the exact native raw-ID comparisons, including their self-first OR.
    // A combination containing 99 does not receive this native bonus unless raw ID is 99.
    const void* actor=*(int*)(person+0xe8)==99?person:*(int*)(spouse+0xe8)==99?spouse:nullptr;
    if(!actor)return;
    probe->actor=actor;probe->normal=value<1?1:value>100?100:value;
    probe->provider=PersonReceipt(actor);probe->partner=spouse;probe->partnerId=growthPersonId(spouse);
    probe->partnerSpouseId=*(int*)(spouse+0x60);
    skilltelemetry::matched(actor,99,99,0x48a273,"person_stats","spousal_bonus_eligibility");
}
uint32_t g_spouseNativeResume=0x48a279,g_spouseCurrentResume=0x48a2ee,g_spouseBaseResume=0x48a2fd;
__declspec(naked) void spouseNativeBridge() {
    __asm {
        pushfd
        pushad
        push ebx
        push ebp
        push esi
        push edi
        call spouseNative
        add esp,16
        popad
        popfd
        mov ebp,[edi+0xe8]
        jmp dword ptr [g_spouseNativeResume]
    }
}
uint8_t spouseGetter(uint8_t* person,int kind,int option,int cache) {
    typedef uint8_t(__thiscall* Getter)(void*,int,int);
    auto original=(*(Getter*)(*(uint8_t**)person+0x4c));
    g_spousePending=SpousePending{};
    if(!skilltelemetry::enabled())return original(person,kind,option);
    SpouseProbe probe(person,kind);
    int before=person[0x170+cache*5+kind];
    uint8_t result=original(person,kind,option);
    if(probe.actor){g_spousePending={person,probe.actor,kind,cache,before,probe.normal};
        g_spousePending.recipient=probe.recipient;g_spousePending.provider=probe.provider;
        g_spousePending.partner=probe.partner;g_spousePending.partnerId=probe.partnerId;
        g_spousePending.spouseId=probe.spouseId;g_spousePending.partnerSpouseId=probe.partnerSpouseId;}
    return result;
}
uint8_t __fastcall spouseCurrentGetter(uint8_t* p,void*,int k,int o){return spouseGetter(p,k,o,0);}
uint8_t __fastcall spouseBaseGetter(uint8_t* p,void*,int k,int o){return spouseGetter(p,k,o,1);}
void __cdecl recordSpouseStore() {
    auto pending=g_spousePending;g_spousePending=SpousePending{};
    if(!pending.actor || !pending.valid())return;
    int id=growthPersonId(pending.person);if(id<0)return;
    auto target=growthTarget(pending.person,id);char context[128];
    sprintf_s(context,"{\"attribute_id\":%d,\"cache\":\"%s\",\"native_bonus\":1}",pending.kind,pending.cache?"base":"current");
    target.context=context;
    skilltelemetry::appliedBonus(pending.actor,99,99,"person_stats","spousal_attribute_refreshed","ability",
        pending.before,pending.person[0x170+pending.cache*5+pending.kind],pending.normal,
        pending.cache?0x48a2fd:0x48a2ee,target);
}
__declspec(naked) void spouseCurrentBridge() {
    __asm {
        call spouseCurrentGetter
        push -1
        mov [esi+edi+0x170],al
        pushfd
        pushad
        call recordSpouseStore
        popad
        popfd
        jmp dword ptr [g_spouseCurrentResume]
    }
}
__declspec(naked) void spouseBaseBridge() {
    __asm {
        call spouseBaseGetter
        mov [esi+edi+0x175],al
        pushfd
        pushad
        call recordSpouseStore
        popad
        popfd
        jmp dword ptr [g_spouseBaseResume]
    }
}
