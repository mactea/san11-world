typedef int(__cdecl* StrategyAttempt)(void*,uint32_t,uint32_t,int,int*);
StrategyAttempt g_strategyAttemptOriginal=(StrategyAttempt)0x5af4f0;
struct StrategyImmunityScope;
thread_local StrategyImmunityScope* g_strategyImmunity=nullptr;
struct StrategyImmunityScope {
    void* source;
    void* defender=nullptr;
    const void* actor=nullptr;
    int effect=-1,actorId=-1,owned=-1;
    void* maximumUnit=nullptr;
    const void* maximumActor=nullptr;
    int maximumEffect=-1,maximumId=-1,maximumOwned=-1,maximumValue=-1,threshold=-1,chance=-1;
    uintptr_t maximumSite=0;
    bool maximumDefensive=false;
    bool maximumGround=false;
    uint32_t groundPosition=0;const uint8_t* groundCell=nullptr;uint8_t groundSnapshot[20]={};
    void* groundBase=nullptr;int groundKind=-1,groundBaseId=-1,groundForce=-1;
    PersonReceipt groundProvider;
    const void* charmActor=nullptr;
    int charmId=-1,charmOwned=-1,charmBefore=-1;
    uint64_t generation;
    StrategyImmunityScope* previous;
    explicit StrategyImmunityScope(void* s):source(s),generation(skilltelemetry::contextGeneration()),previous(g_strategyImmunity){g_strategyImmunity=this;}
    ~StrategyImmunityScope(){g_strategyImmunity=previous;}
};
int __fastcall strategyImmunity(void* unit,void*,int effect) {
    typedef int(__thiscall* Has)(void*,int);
    auto native=*(Has*)(*(uint8_t**)unit+0x58);auto scope=g_strategyImmunity;
    if(!scope)return native(unit,effect);
    ProgressionProbe probe(effect);probe.scene="strategy";probe.event="immunity_eligibility";
    int found=native(unit,effect);
    // A matched immunity writes detail[1]=1 and returns chance0, either
    // through5af26f or the fire-only early return at5aebb9. No RNG is sampled.
    if(found && probe.actor && scope->generation==skilltelemetry::contextGeneration()) {
        scope->defender=unit;scope->actor=probe.actor;scope->effect=effect;
        scope->actorId=growthPersonId((void*)probe.actor);
        scope->owned=*(const int*)((const uint8_t*)probe.actor+0xe8);
    }
    return found;
}
uint32_t g_strategyInsightResume=0x5aea40,g_strategyDisciplineResume=0x5aed09,
    g_strategyMirrorDisciplineResume=0x5aed1a,g_strategyCalmResume=0x5aee8b,g_strategyMirrorCalmResume=0x5aee9c,
    g_strategyFireImmunityResume=0x5aebb7;
__declspec(naked) void strategyFireImmunityBridge(){__asm {call strategyImmunity
    test eax,eax
    jmp dword ptr [g_strategyFireImmunityResume]}}
const struct StrategyFireImmunityLayout {uint32_t start;int size;const char* bytes;} g_strategyFireImmunityLayouts[]={
    {0x5aebac,34,"\x8b\x13\x6a\x3e\x8b\xcb\xff\x52\x58\x85\xc0\x74\x15\x8b\x44\x24\x30\x5f\x5e\x5d\xc7\x40\x04\x01\x00\x00\x00\x33\xc0\x5b\x83\xc4\x0c\xc3"},
};
bool verifyStrategyFireImmunityLayout(){
    for(const auto& s:g_strategyFireImmunityLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
__declspec(naked) void strategyInsightBridge(){__asm {call strategyImmunity
    test eax,eax
    jmp dword ptr [g_strategyInsightResume]}}
__declspec(naked) void strategyDisciplineBridge(){__asm {call strategyImmunity
    test eax,eax
    jmp dword ptr [g_strategyDisciplineResume]}}
__declspec(naked) void strategyMirrorDisciplineBridge(){__asm {call strategyImmunity
    test eax,eax
    jmp dword ptr [g_strategyMirrorDisciplineResume]}}
__declspec(naked) void strategyCalmBridge(){__asm {call strategyImmunity
    test eax,eax
    jmp dword ptr [g_strategyCalmResume]}}
__declspec(naked) void strategyMirrorCalmBridge(){__asm {call strategyImmunity
    test eax,eax
    jmp dword ptr [g_strategyMirrorCalmResume]}}
void strategyMaximumOutcome(const StrategyImmunityScope& scope,uint32_t to,int plan,int result,const int* detail);
void strategyGroundPrepare(StrategyImmunityScope& scope,uint32_t to,int plan);
void strategyCharmOutcome(const StrategyImmunityScope& scope,uint32_t to,int plan,int result,const int* detail);
int __cdecl strategyAttempt(void* source,uint32_t from,uint32_t to,int plan,int* detail) {
    if(!skilltelemetry::enabled())return g_strategyAttemptOriginal(source,from,to,plan,detail);
    StrategyImmunityScope scope(source);
    int result=g_strategyAttemptOriginal(source,from,to,plan,detail);
    strategyMaximumOutcome(scope,to,plan,result,detail);
    strategyCharmOutcome(scope,to,plan,result,detail);
    if(result || !scope.actor || scope.actorId<0 || detail[1]!=1 ||
       scope.generation!=skilltelemetry::contextGeneration() ||
       !g_growthValid(source) || !g_growthValid(scope.defender) ||
       growthPersonId((void*)scope.actor)!=scope.actorId ||
       *(const int*)((const uint8_t*)scope.actor+0xe8)!=scope.owned ||
       !contains(scope.owned,scope.effect) || !afterAttackMember(scope.defender,scope.actor))return result;
    int sourceId=g_telemetryUnitId((void*)0x7201958,source);
    auto target=unitTarget(scope.defender);
    if(sourceId<0 || target.unit<0 || g_movementUnitById((void*)0x7201958,sourceId)!=source ||
       g_movementUnitById((void*)0x7201958,target.unit)!=scope.defender)return result;
    typedef const uint32_t*(__thiscall* Position)(void*);
    auto position=*(Position*)(*(uint8_t**)scope.defender+0x3c);
    if(*position(scope.defender)!=to)return result;
    target.equipment=*(const int*)((const uint8_t*)scope.defender+0xa8);
    target.x=(int16_t)to;target.y=(int16_t)(to>>16);
    auto attacking=unitTarget(source);char context[224];
    sprintf_s(context,"{\"strategy_id\":%d,\"attacking_unit\":%d,\"attacking_force\":%d,\"success_chance\":0,\"native_immunity_flag\":1,\"counterfactual_evaluated\":false}",
        plan,sourceId,attacking.force);target.context=context;
    skilltelemetry::appliedEvent(scope.actor,scope.owned,scope.effect,"strategy","strategy_immunity_forced_failure",0x593402,target);
    return result;
}
