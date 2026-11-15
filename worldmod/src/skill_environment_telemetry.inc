// Native seasonal settlement excludes protected cities before sampling/activation.
// These events describe the actual exclusion, not hypothetical prevented losses.
const int* g_environmentNewBound=(const int*)0x58f0cf;
const int* g_environmentActiveBound=(const int*)0x58f28b;
const int* g_environmentSpreadBound=(const int*)0x58f3be;
uint32_t g_environmentNewResume=0x58f0cc,g_environmentActiveResume=0x58f288,g_environmentSpreadResume=0x58f3bb;
int __fastcall environmentFind(void* base,void*,uint8_t* city,void* disaster,int phase,int bound,int effect) {
    typedef int(__thiscall* Find)(void*,int);
    auto original=(*(Find*)(*(uint8_t**)base+0x5c));
    if(!skilltelemetry::enabled())return original(base,effect);
    ProgressionProbe probe(97);probe.scene="environment";probe.event="disaster_protection_eligibility";
    int result=original(base,effect);
    // Read the same live upper bound as the immediately following native branch.
    // M10 can expand it; neither that comparison nor either branch is overwritten.
    if(result<0 || result>bound || effect!=97)return result;
    auto actor=g_telemetryPerson((void*)0x7201958,result);
    if(!actor || !contains(*(int*)(actor+0xe8),97) || !g_growthValid(city))return result;
    auto target=incomeTarget(base);
    if(target.base<0 || g_baseById((void*)0x7201958,target.base)!=base)return result;
    // Both native disaster vtables use a constant, side-effect-free kind getter.
    int kind=(*(BuildingValue*)(*(uint8_t**)disaster))(disaster);
    const char* event=phase==0?"disaster_new_candidate_excluded":phase==1?
        "disaster_activation_skipped":"disaster_spread_candidate_excluded";
    char context[192];
    sprintf_s(context,"{\"disaster_kind\":%d,\"active_flags\":%u,\"pending_flags\":%u,\"holder_bound\":%d,\"counterfactual_evaluated\":false}",
        kind,*(uint32_t*)(city+0x9c),*(uint32_t*)(city+0xa0),bound);target.context=context;
    skilltelemetry::appliedEvent(actor,*(int*)(actor+0xe8),97,"environment",event,
        phase==0?0x58f0d3:phase==1?0x58f28f:0x58f3c2,target);
    return result;
}
__declspec(naked) void environmentNewBridge() {
    __asm {mov eax,g_environmentNewBound
        push dword ptr [eax]
        push 0
        push esi
        push ebx
        call environmentFind
        test eax,eax
        jmp dword ptr [g_environmentNewResume]}
}
__declspec(naked) void environmentActiveBridge() {
    __asm {mov eax,g_environmentActiveBound
        push dword ptr [eax]
        push 1
        push esi
        push ebx
        call environmentFind
        test eax,eax
        jmp dword ptr [g_environmentActiveResume]}
}
__declspec(naked) void environmentSpreadBridge() {
    __asm {mov eax,g_environmentSpreadBound
        push dword ptr [eax]
        push 2
        push esi
        push ebx
        call environmentFind
        test eax,eax
        jmp dword ptr [g_environmentSpreadResume]}
}

struct HarvestProvider {const void* actor=nullptr;uint8_t* city=nullptr;void* base=nullptr;};
struct HarvestScope;
thread_local HarvestScope* g_harvestScope=nullptr;
struct HarvestScope {
    HarvestProvider pending;
    std::vector<HarvestProvider> priority;
    HarvestScope* previous;
    HarvestScope():previous(g_harvestScope){g_harvestScope=this;}
    ~HarvestScope(){g_harvestScope=previous;}
};
uint32_t g_environmentRunResume=0x58f4e6,g_harvestFindResume=0x58f68a;
const int* g_harvestHolderBound=(const int*)0x58f68d;
__declspec(naked) void environmentRunNative() {
    __asm {push ebp
        mov ebp,esp
        and esp,0xfffffff8
        jmp dword ptr [g_environmentRunResume]}
}
typedef int(__thiscall* EnvironmentRun)(void*);
typedef int(__cdecl* HarvestRoll)(int);
typedef uint8_t*(__thiscall* HarvestAppend)(void*,uint8_t*);
typedef void(__thiscall* HarvestFlag)(void*,int,int);
EnvironmentRun g_environmentRunOriginal=(EnvironmentRun)environmentRunNative;
HarvestRoll g_harvestRollOriginal=(HarvestRoll)0x4721d0;
HarvestAppend g_harvestAppendOriginal=(HarvestAppend)0x4ba130;
HarvestFlag g_harvestFlagOriginal=(HarvestFlag)0x47b580;
int __fastcall environmentRun(void* self,void*) {
    if(!skilltelemetry::enabled())return g_environmentRunOriginal(self);
    HarvestScope scope;
    return g_environmentRunOriginal(self);
}
int __fastcall harvestFind(void* base,void*,uint8_t* city,int effect) {
    typedef int(__thiscall* Find)(void*,int);
    auto original=(*(Find*)(*(uint8_t**)base+0x5c));
    auto scope=g_harvestScope;
    if(scope)scope->pending=HarvestProvider{};
    if(!skilltelemetry::enabled() || !scope)return original(base,effect);
    ProgressionProbe probe(98);probe.scene="environment";probe.event="harvest_preference_eligibility";
    int result=original(base,effect);
    if(effect==98 && result>=0 && result<=*g_harvestHolderBound) {
        auto actor=g_telemetryPerson((void*)0x7201958,result);
        if(actor && contains(*(int*)(actor+0xe8),98))scope->pending={actor,city,base};
    }
    return result;
}
__declspec(naked) void harvestFindBridge() {
    __asm {push esi
        call harvestFind
        cmp eax,ebp
        jmp dword ptr [g_harvestFindResume]}
}
int __cdecl harvestRoll(int percent) {
    int result=g_harvestRollOriginal(percent); // the native50-percent sample, exactly once
    if(g_harvestScope && (!result || percent!=50))g_harvestScope->pending=HarvestProvider{};
    return result;
}
uint8_t* __fastcall harvestAppend(uint8_t* list,void*,uint8_t* city) {
    auto scope=g_harvestScope;HarvestProvider pending;
    if(scope){pending=scope->pending;scope->pending=HarvestProvider{};}
    bool observe=skilltelemetry::enabled() && pending.actor && pending.city==city;
    uint8_t* result=g_harvestAppendOriginal(list,city);
    if(observe && result && *(uint8_t**)(list+8)==result && *(uint8_t**)(result+8)==city) {
        // Logging allocation failure must never repeat or cancel native gameplay.
        try {scope->priority.push_back(pending);}catch(...) {}
    }
    return result;
}
void __fastcall harvestFlag(uint8_t* city,void*,int kind,int value) {
    auto scope=g_harvestScope;
    bool observe=skilltelemetry::enabled() && scope && kind==2 && value==1 && g_growthValid(city);
    uint32_t before=observe?*(uint32_t*)(city+0xa0):0;
    g_harvestFlagOriginal(city,kind,value);
    if(!observe || (before&4) || !(*(uint32_t*)(city+0xa0)&4))return;
    for(const auto& provider:scope->priority)if(provider.city==city && contains(*(const int*)((const uint8_t*)provider.actor+0xe8),98)) {
        auto target=incomeTarget(provider.base);
        if(target.base<0 || g_baseById((void*)0x7201958,target.base)!=provider.base)return;
        char context[192];
        sprintf_s(context,"{\"harvest_kind\":2,\"pending_flags_before\":%u,\"pending_flags_after\":%u,\"priority_pool\":true,\"preference_roll_percent\":50,\"counterfactual_evaluated\":false}",
            before,*(uint32_t*)(city+0xa0));target.context=context;
        skilltelemetry::appliedEvent(provider.actor,*(const int*)((const uint8_t*)provider.actor+0xe8),98,
            "environment","harvest_scheduled_from_priority_pool",0x58f750,target);
        return;
    }
}
