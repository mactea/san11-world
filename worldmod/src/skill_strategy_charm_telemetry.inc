int __fastcall strategyCharmHas(void* unit,void*,int effect){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);auto scope=g_strategyImmunity;
    if(!scope || unit!=scope->source)return native(unit,effect);
    ProgressionProbe probe(69);probe.scene="strategy";probe.event="charm_eligibility";
    int result=native(unit,effect);
    if(result && effect==69 && probe.actor && scope->generation==skilltelemetry::contextGeneration()){
        scope->charmActor=probe.actor;scope->charmId=growthPersonId((void*)probe.actor);
        scope->charmOwned=*(const int*)((const uint8_t*)probe.actor+0xe8);
    }
    return result;
}
uint32_t g_strategyCharmHasResume=0x5af480,g_strategyCharmDoubleResume=0x5af4a0;
__declspec(naked) void strategyCharmHasBridge(){__asm {call strategyCharmHas
    test eax,eax
    jmp dword ptr [g_strategyCharmHasResume]}}
void __cdecl strategyCharmBefore(int before){
    auto scope=g_strategyImmunity;
    if(scope && scope->charmActor && scope->generation==skilltelemetry::contextGeneration())scope->charmBefore=before;
}
__declspec(naked) void strategyCharmDoubleBridge(){__asm {mov eax,[esp+0x10]
    pushfd
    pushad
    push eax
    call strategyCharmBefore
    add esp,4
    popad
    popfd
    add eax,eax
    jmp dword ptr [g_strategyCharmDoubleResume]}}
void strategyCharmOutcome(const StrategyImmunityScope& scope,uint32_t to,int plan,int result,const int* detail){
    if(!scope.charmActor || scope.charmId<0 || scope.charmBefore<0 || scope.charmBefore>100 || detail[1]!=0 ||
       scope.chance!=(scope.charmBefore>=50?100:scope.charmBefore*2) ||
       scope.generation!=skilltelemetry::contextGeneration() || !g_growthValid(scope.source) || !g_growthValid(scope.defender) ||
       growthPersonId((void*)scope.charmActor)!=scope.charmId ||
       *(const int*)((const uint8_t*)scope.charmActor+0xe8)!=scope.charmOwned || !contains(scope.charmOwned,69) ||
       !afterAttackMember(scope.source,scope.charmActor))return;
    int source=g_telemetryUnitId((void*)0x7201958,scope.source);auto target=unitTarget(scope.defender);
    if(source<0 || target.unit<0 || g_movementUnitById((void*)0x7201958,source)!=scope.source ||
       g_movementUnitById((void*)0x7201958,target.unit)!=scope.defender)return;
    typedef const uint32_t*(__thiscall* Position)(void*);
    if(*(*(Position*)(*(uint8_t**)scope.defender+0x3c))(scope.defender)!=to)return;
    target.actorUnit=source;target.equipment=*(const int*)((const uint8_t*)scope.source+0xa8);
    target.x=(int16_t)to;target.y=(int16_t)(to>>16);char context[256];
    sprintf_s(context,"{\"strategy_id\":%d,\"native_result\":%d,\"target_gender_guard_result\":0,\"counterfactual_scope\":\"final_probability_step_without69\",\"counterfactual_evaluated\":true}",plan,result);
    target.context=context;
    // Before is the same native calculation immediately before ADD EAX,EAX.
    // Omitting only that step returns before (0..100), with no second RNG draw.
    skilltelemetry::appliedBonus(scope.charmActor,scope.charmOwned,69,"strategy","strategy_charm_probability_doubled",
        "success_chance_percent",scope.charmBefore,scope.chance,scope.charmBefore,0x5af49e,target);
}

struct StrategyCharmLayout {uint32_t start;int size;const char* bytes;};
const StrategyCharmLayout g_strategyCharmLayouts[]={
    {0x495600,128,"\x53\x55\x8b\x6c\x24\x0c\x56\x57\x8b\xd9\x33\xff\x8d\x64\x24\x00\x85\xff\x7c\x20\x83\xff\x03\x7d\x1b\x85\xff\x75\x05\x8b\x43\x0c\xeb\x15\x8d\x47\xff\x85\xc0\x7c\x0b\x83\xf8\x02\x7d\x06\x8b\x44\x83\x10\xeb\x03\x83\xc8\xff\x50\xb9\x58\x19\x20\x07\xe8\xbe\xb4\xff\xff\x8b\xf0\x56\xe8\xe6\x4f\xfe\xff\x83\xc4\x04\x85\xc0\x74\x0c\x55\x8b\xce\xe8\x97\x3a\xff\xff\x85\xc0\x75\x10\x47\x83\xff\x03\x7c\xad\x5f\x5e\x5d\x83\xc8\xff\x5b\xc2\x04\x00\x56\xb9\x58\x19\x20\x07\xe8\x98\xbc\xff\xff\x5f\x5e\x5d\x5b\xc2\x04\x00\xcc"},
    {0x5af473,81,"\x8b\x4c\x24\x20\x8b\x11\x6a\x45\xff\x52\x58\x85\xc0\x74\x2b\x53\xe8\xa8\xb1\xec\xff\x83\xc4\x04\x85\xc0\x74\x1e\x8b\xcb\xe8\xda\x69\xee\xff\x85\xc0\x75\x13\x8b\x44\x24\x10\x03\xc0\x83\xf8\x64\x0f\x8d\x8f\xf7\xff\xff\x89\x44\x24\x10\x83\x7c\x24\x10\x64\x0f\x8d\x80\xf7\xff\xff\x8b\x44\x24\x10\x5f\x5e\x5d\x5b\x83\xc4\x0c\xc3"},
};
bool verifyStrategyCharmLayout(){
    for(const auto& s:g_strategyCharmLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected strategy charm branch at %08x; rejected",s.start);return false;
    }
    return true;
}
