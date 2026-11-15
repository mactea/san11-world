// Observe only the actual unrest routine's roll and successful camp creation.
// The shared helpers can also be queried without committing a world event.
thread_local IncomeProviders g_unrestProviders;
thread_local const int* g_unrestFlags=nullptr;
thread_local int g_unrestTribe=-1;
struct UnrestCollectionScope {
    IncomeProviders* previous;
    bool completed=false;
    explicit UnrestCollectionScope(IncomeProviders* current):previous(g_collectIncomeProviders){g_collectIncomeProviders=current;}
    ~UnrestCollectionScope(){
        g_collectIncomeProviders=previous;
        if(!completed){g_unrestProviders={};g_unrestFlags=nullptr;g_unrestTribe=-1;}
    }
};
void __fastcall unrestCityFlags(int* flags,void*,void* building,int mode) {
    g_unrestProviders=IncomeProviders{};g_unrestFlags=nullptr;g_unrestTribe=-1;
    IncomeProviders collecting;
    if(skilltelemetry::enabled()) {
        collecting.city=building;collecting.flags=flags;
        collecting.generation=skilltelemetry::contextGeneration();collecting.scene="unrest";
        collecting.event="city_unrest_eligibility";
    }
    UnrestCollectionScope scope(collecting.city?&collecting:nullptr);
    g_cityIncomeFlagsOriginal(flags,building,mode);
    g_unrestProviders=skilltelemetry::enabled() && collecting.generation==skilltelemetry::contextGeneration()?collecting:IncomeProviders{};
    g_unrestFlags=g_unrestProviders.flags;
    scope.completed=true;
}
struct UnrestGateProbe;
thread_local UnrestGateProbe* g_unrestGateProbe=nullptr;
struct UnrestGateProbe {
    void* record;
    void* city;
    RecoveryActor actor;
    UnrestGateProbe* previous;
    UnrestGateProbe(void* r,void* c,RecoveryActor a):record(r),city(c),actor(a),previous(g_unrestGateProbe){g_unrestGateProbe=this;}
    ~UnrestGateProbe(){g_unrestGateProbe=previous;}
};
typedef int(__cdecl* UnrestRoll)(const int*);
UnrestRoll g_unrestRollOriginal=(UnrestRoll)0x58cf60;
__declspec(naked) int __cdecl nativeUnrestRoll(const int* flags,void* record) {
    __asm {
        push esi
        mov esi,[esp+12]
        push dword ptr [esp+8]
        call dword ptr [g_unrestRollOriginal]
        add esp,4
        pop esi
        ret
    }
}
int __cdecl unrestRoll(const int* flags,void* record) {
    if(!skilltelemetry::enabled())return nativeUnrestRoll(flags,record);
    auto actor=flags==g_unrestFlags?g_unrestProviders.holders[95]:RecoveryActor{};
    UnrestGateProbe probe(record,g_unrestProviders.city,actor);
    return nativeUnrestRoll(flags,record);
}
__declspec(naked) void unrestRollBridge() {
    __asm {
        mov eax,[esp+4]
        push esi
        push eax
        call unrestRoll
        add esp,8
        ret
    }
}
void __cdecl recordUnrestThreshold(void* record,int threshold,int order) {
    auto probe=g_unrestGateProbe;
    if(!probe || !recoveryActorValid(probe->actor,95) || record!=probe->record || threshold!=60 || order<60 || order>=80 ||
       g_cityBuilding(record)!=probe->city)return;
    auto target=incomeTarget(probe->city);
    char context[160];sprintf_s(context,"{\"public_order\":%d,\"native_threshold\":60,\"ordinary_threshold\":80,\"rng_skipped\":true}",order);
    target.context=context;
    skilltelemetry::appliedEvent(probe->actor.person,probe->actor.owned,95,
        "unrest","bandit_roll_suppressed",0x58ea0a,target);
}
uint32_t g_unrestThresholdResume=0x58cfa9;
__declspec(naked) void unrestThresholdBridge() {
    __asm {
        // Keep the common early-return target at 0x58cfad untouched.
        neg eax
        sbb eax,eax
        and eax,-20
        add eax,80
        pushad
        push edx
        push eax
        push esi
        call recordUnrestThreshold
        add esp,12
        popad
        jmp dword ptr [g_unrestThresholdResume]
    }
}
// This operand is changed by M8 from 42 to R. Read it instead of fixing tribal IDs.
const int* g_unrestForceBase=(const int*)0x58bbb0;
uintptr_t g_unrestTribeOriginal=0x58bb90;
void __cdecl rememberUnrestTribe(int force) {g_unrestTribe=skilltelemetry::enabled()?force:-1;}
__declspec(naked) void unrestTribeBridge() {
    __asm {
        call dword ptr [g_unrestTribeOriginal] // native helper takes city ID in EAX
        pushfd
        pushad
        push eax
        call rememberUnrestTribe
        add esp,4
        popad
        popfd
        ret
    }
}
typedef void*(__cdecl* UnrestCamp)(uint32_t,int);
UnrestCamp g_unrestCampOriginal=(UnrestCamp)0x58bc60;
typedef int(__cdecl* UnrestValid)(void*);
UnrestValid g_unrestValid=(UnrestValid)0x47a630;
__declspec(naked) void* __cdecl nativeUnrestCamp(uint32_t hex,int force,void* city) {
    __asm {
        push esi
        mov esi,[esp+16]
        push dword ptr [esp+12]
        push dword ptr [esp+12]
        call dword ptr [g_unrestCampOriginal]
        add esp,8
        pop esi
        ret
    }
}
void* __cdecl unrestCamp(uint32_t hex,int force,void* city) {
    RecoveryActor actor;
    int original=g_unrestTribe;g_unrestTribe=-1;
    int effect=-1;
    if(skilltelemetry::enabled() && city==g_unrestProviders.city) {
        int relative=original-*g_unrestForceBase;
        if(relative>=0 && relative<4 && force==*g_unrestForceBase+4) {
            const int effects[]={92,93,91,94};effect=effects[relative];
            actor=g_unrestProviders.holders[effect];
        }
    }
    void* camp=nativeUnrestCamp(hex,force,city);
    if(recoveryActorValid(actor,effect) && g_unrestValid(camp)) {
        auto target=incomeTarget(camp);
        // A returned draft or failed world insertion is not a committed camp.
        if(target.base>=0 && target.force==force && g_baseById((void*)0x7201958,target.base)==camp) {
            char context[160];sprintf_s(context,"{\"protected_city\":%d,\"original_force\":%d,\"actual_force\":%d}",
                g_telemetryBaseId((void*)0x7201958,city),original,force);
            target.x=(int16_t)hex;target.y=(int16_t)(hex>>16);target.context=context;
            skilltelemetry::appliedEvent(actor.person,actor.owned,effect,
                "unrest","tribal_camp_replaced_by_bandit_camp",0x58eaaa,target);
        }
    }
    return camp;
}
__declspec(naked) void unrestCampBridge() {
    __asm {
        push esi
        push dword ptr [esp+12]
        push dword ptr [esp+12]
        call unrestCamp
        add esp,12
        ret
    }
}
