typedef int(__cdecl* StrategyCritical)(void*,uint32_t,uint32_t,int);
StrategyCritical g_strategyCriticalOriginal=(StrategyCritical)0x5af820;
StrategyCritical g_strategyCriticalChanceOriginal=(StrategyCritical)0x5af520;
struct StrategyCriticalScope;
thread_local StrategyCriticalScope* g_strategyCritical=nullptr;
struct StrategyCriticalScope {
    void* source;
    void* target=nullptr;
    const void* actor=nullptr;
    int effect=-1,actorId=-1,owned=-1,value=-1,opponent=-1,chance=-1;
    uint64_t generation;
    StrategyCriticalScope* previous;
    explicit StrategyCriticalScope(void* s):source(s),generation(skilltelemetry::contextGeneration()),previous(g_strategyCritical){g_strategyCritical=this;}
    ~StrategyCriticalScope(){g_strategyCritical=previous;}
};
void strategyCriticalProvider(StrategyCriticalScope& scope,const void* actor,int effect,int value=-1,int opponent=-1){
    if(!actor || scope.generation!=skilltelemetry::contextGeneration())return;
    scope.actor=actor;scope.actorId=growthPersonId((void*)actor);
    scope.owned=*(const int*)((const uint8_t*)actor+0xe8);scope.effect=effect;scope.value=value;scope.opponent=opponent;
}
int __fastcall strategyCriticalHas(void* unit,void*,int effect){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);auto scope=g_strategyCritical;
    if(!scope || scope->source!=unit)return native(unit,effect);
    ProgressionProbe probe(effect);probe.scene="strategy";probe.event="critical_effect_eligibility";
    int result=native(unit,effect);
    if(result)strategyCriticalProvider(*scope,probe.actor,effect);
    return result;
}
uint32_t g_strategyCriticalGodResume=0x5af5f8,g_strategyDeepResume=0x5af605,g_strategyAmbushResume=0x5af71f;
__declspec(naked) void strategyCriticalGodBridge(){__asm {call strategyCriticalHas
    test eax,eax
    jmp dword ptr [g_strategyCriticalGodResume]}}
__declspec(naked) void strategyDeepBridge(){__asm {call strategyCriticalHas
    test eax,eax
    jmp dword ptr [g_strategyDeepResume]}}
__declspec(naked) void strategyAmbushBridge(){__asm {call strategyCriticalHas
    test eax,eax
    jmp dword ptr [g_strategyAmbushResume]}}
CriticalMaximum g_strategyCriticalMaximumOriginal=(CriticalMaximum)0x495680;
int __fastcall strategyCriticalMaximum(void* unit,void*,int effect,int kind){
    auto scope=g_strategyCritical;
    if(!scope || scope->source!=unit)return g_strategyCriticalMaximumOriginal(unit,effect,kind);
    CriticalMaximumProbe maximum(kind);ProgressionProbe eligibility(effect);
    eligibility.scene="strategy";eligibility.event="critical_effect_eligibility";
    int result=g_strategyCriticalMaximumOriginal(unit,effect,kind);
    if(scope->target && kind==2){
        int opponent=((const uint8_t*)scope->target)[0xc6];
        if((effect==58 && result>opponent) || (effect==59 && result>=0 && result<opponent))
            strategyCriticalProvider(*scope,maximum.actor,effect,result,opponent);
    }
    return result;
}
void* __fastcall strategyCriticalTarget(void* world,void*,int id){
    void* unit=g_strategyDefenderOriginal(world,id);if(g_strategyCritical)g_strategyCritical->target=unit;return unit;
}
int __cdecl strategyCriticalChance(void* unit,uint32_t from,uint32_t to,int plan){
    int chance=g_strategyCriticalChanceOriginal(unit,from,to,plan);
    if(g_strategyCritical)g_strategyCritical->chance=chance;
    return chance;
}
int __cdecl strategyCriticalAttempt(void* source,uint32_t from,uint32_t to,int plan){
    if(!skilltelemetry::enabled())return g_strategyCriticalOriginal(source,from,to,plan);
    StrategyCriticalScope scope(source);
    int result=g_strategyCriticalOriginal(source,from,to,plan);
    if(!result || scope.chance!=100 || !scope.actor || scope.actorId<0 ||
       scope.generation!=skilltelemetry::contextGeneration() || !g_growthValid(source) || !g_growthValid(scope.target) ||
       growthPersonId((void*)scope.actor)!=scope.actorId || *(const int*)((const uint8_t*)scope.actor+0xe8)!=scope.owned ||
       !contains(scope.owned,scope.effect) || !afterAttackMember(source,scope.actor))return result;
    int sourceId=g_telemetryUnitId((void*)0x7201958,source);auto target=unitTarget(scope.target);
    if(sourceId<0 || target.unit<0 || g_movementUnitById((void*)0x7201958,sourceId)!=source ||
       g_movementUnitById((void*)0x7201958,target.unit)!=scope.target)return result;
    typedef const uint32_t*(__thiscall* Position)(void*);
    if(*(*(Position*)(*(uint8_t**)scope.target+0x3c))(scope.target)!=to)return result;
    strategyFireCritical(scope,plan,to);
    strategyStatusCritical(scope,plan,to);
    target.actorUnit=sourceId;target.equipment=*(const int*)((const uint8_t*)source+0xa8);
    target.x=(int16_t)to;target.y=(int16_t)(to>>16);char context[256];
    sprintf_s(context,"{\"strategy_id\":%d,\"critical_chance\":100,\"critical_result\":true,\"stat_id\":%d,\"provider_value\":%d,\"opponent_intelligence\":%d,\"counterfactual_evaluated\":false}",
        plan,scope.value>=0?2:-1,scope.value,scope.opponent);target.context=context;
    skilltelemetry::appliedEvent(scope.actor,scope.owned,scope.effect,"strategy","strategy_forced_critical_decision",0x593424,target);
    return result;
}

struct StrategyCriticalLayout {uint32_t start;int size;const char* bytes;};
const StrategyCriticalLayout g_strategyCriticalLayouts[]={
    {0x5af616,10,"\xb8\x64\x00\x00\x00\x5d\x5e\x5b\x5f\xc3"},
    {0x59340e,4,"\x85\xed\x74\x1c"},
    {0x5af5df,14,"\x85\xed\x74\x0a\x83\xfd\x01\x7e\x4b\x83\xfd\x06\x7f\x46"},
    {0x5af5ef,11,"\x6a\x3f\x8b\xcf\xff\x52\x58\x85\xc0\x75\x1c"},
    {0x5af5fc,11,"\x6a\x43\x8b\xcf\xff\x50\x58\x85\xc0\x75\x0f"},
    {0x5af607,15,"\x6a\x02\x6a\x3a\x8b\xcf\xe8\x6e\x60\xee\xff\x3b\xc3\x7e\x0a"},
    {0x5af620,19,"\x6a\x02\x6a\x3b\x8b\xcf\xe8\x55\x60\xee\xff\x85\xc0\x7c\x04\x3b\xc3\x7c\xe3"},
    {0x5af716,15,"\x6a\x10\x8b\xcf\xff\x52\x58\x85\xc0\x0f\x85\xf1\xfe\xff\xff"},
};
bool verifyStrategyCriticalLayout(){
    for(const auto& s:g_strategyCriticalLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected strategy critical branch at %08x; rejected",s.start);return false;
    }
    return true;
}
