struct StrategyFollowupScope;
thread_local StrategyFollowupScope* g_strategyFollowup=nullptr;
struct StrategyFollowupScope {
    const uint32_t* command;
    bool targetSeen=false;
    skilltelemetry::Target target;
    StrategyFollowupScope* previous;
    explicit StrategyFollowupScope(const uint32_t* c):command(c),previous(g_strategyFollowup){g_strategyFollowup=this;}
    ~StrategyFollowupScope(){g_strategyFollowup=previous;}
};
int __fastcall strategyFollowupFind(void* unit,void*,int effect){
    typedef int(__thiscall* Find)(void*,int);auto native=*(Find*)(*(uint8_t**)unit+0x5c);
    if(!skilltelemetry::enabled())return native(unit,effect);
    QueryOrigin origin(effect==66?0x5936a0:0x5939e2);
    ProgressionProbe probe(effect);probe.scene="strategy";probe.event="followup_eligibility";
    return native(unit,effect);
}
uint32_t g_strategyChainFindResume=0x5936a9,g_strategyReflectFindResume=0x5939eb;
__declspec(naked) void strategyChainFindBridge(){__asm {call strategyFollowupFind
    push eax
    mov ecx,0x7201958
    jmp dword ptr [g_strategyChainFindResume]}}
__declspec(naked) void strategyReflectFindBridge(){__asm {call strategyFollowupFind
    push eax
    mov ecx,0x7201958
    jmp dword ptr [g_strategyReflectFindResume]}}
typedef void*(__thiscall* StrategyObject)(void*);
StrategyObject g_strategyObjectOriginal=(StrategyObject)0x483b40;
void* __fastcall strategyFollowupTarget(void* cell,void*){
    void* object=g_strategyObjectOriginal(cell);auto scope=g_strategyFollowup;
    if(!scope || !g_strategyCommand || g_strategyCommand->command!=scope->command)return object;
    scope->targetSeen=true;
    // Snapshot before the action: a real command can destroy or move its target.
    // A null/other object keeps explicit unknown IDs instead of a guessed owner.
    if(object && g_growthValid(object)){
        int unit=g_telemetryUnitId((void*)0x7201958,object);
        if(unit>=0 && g_movementUnitById((void*)0x7201958,unit)==object)scope->target=unitTarget(object);
        else {
            int base=g_telemetryBaseId((void*)0x7201958,object);
            if(base>=0 && g_baseById((void*)0x7201958,base)==object)scope->target=incomeTarget(object);
        }
    }
    return object;
}
StrategyCommand g_strategyFollowupOriginal=(StrategyCommand)0x5933a0;
int __fastcall strategyFollowup(void* self,void*,const uint32_t* command,int mode,void* actor){
    int effect=mode==2?66:mode==1?68:-1;
    if(!skilltelemetry::enabled() || effect<0 || !actor || command[0]>8 || (mode==2 && command[0]>4))
        return g_strategyFollowupOriginal(self,command,mode,actor);
    uint32_t snapshot[3];memcpy(snapshot,command,12);
    uint64_t generation=skilltelemetry::contextGeneration();
    int actorId=growthPersonId(actor),owned=*(const int*)((const uint8_t*)actor+0xe8);
    void* source=g_movementUnitById((void*)0x7201958,(int)snapshot[1]);
    bool observe=actorId>=0 && contains(owned,effect) && g_growthValid(source) && afterAttackMember(source,actor) &&
        g_telemetryUnitId((void*)0x7201958,source)==(int)snapshot[1];
    int sourceForce=observe?unitTarget(source).force:-1;
    int sourceEquipment=observe?*(const int*)((const uint8_t*)source+0xa8):-1;
    StrategyFollowupScope scope(command);
    int result=g_strategyFollowupOriginal(self,command,mode,actor);
    if(observe && scope.targetSeen && generation==skilltelemetry::contextGeneration() && !memcmp(snapshot,command,12) &&
       growthPersonId(actor)==actorId && *(const int*)((const uint8_t*)actor+0xe8)==owned &&
       g_movementUnitById((void*)0x7201958,(int)snapshot[1])==source){
        auto target=scope.target;target.actorUnit=(int)snapshot[1];target.equipment=sourceEquipment;
        target.x=(int16_t)snapshot[2];target.y=(int16_t)(snapshot[2]>>16);char context[240];
        sprintf_s(context,"{\"strategy_id\":%u,\"command_mode\":%d,\"native_result\":%d,\"source_force_at_start\":%d,\"target_snapshot\":\"before_execution\",\"counterfactual_evaluated\":false}",
            snapshot[0],mode,result,sourceForce);target.context=context;
        skilltelemetry::appliedEvent(actor,owned,effect,"strategy",effect==66?"strategy_chain_subcommand_resolved":"strategy_reflected_subcommand_resolved",0x593a75,target);
    }
    return result;
}

struct StrategyFollowupLayout {uint32_t start;int size;const char* bytes;};
const StrategyFollowupLayout g_strategyFollowupLayouts[]={
    {0x593681,57,"\x85\xc0\x0f\x84\xe1\x02\x00\x00\x56\xe8\xa1\x6f\xee\xff\x83\xc4\x04\x85\xc0\x0f\x84\xe0\x03\x00\x00\x8b\x06\x6a\x42\x8b\xce\xff\x50\x5c\x50\xb9\x58\x19\x20\x07\xe8\x52\xd4\xef\xff\x3b\xc5\x89\x44\x24\x20\x0f\x84\xc0\x03\x00\x00"},
    {0x5936d1,19,"\x8b\x03\x83\xf8\x04\x89\x4c\x24\x18\x89\x6c\x24\x14\x0f\x87\x96\x03\x00\x00"},
    {0x593945,37,"\x8b\x6c\x24\x20\x8b\x0b\x89\x4c\x24\x24\x56\xb9\x58\x19\x20\x07\xe8\x66\xde\xef\xff\x55\x89\x44\x24\x2c\x89\x7c\x24\x30\x6a\x02\xe9\x02\x01\x00\x00"},
    {0x5939ce,44,"\x8b\x47\x24\x83\xc4\x04\x3b\xc5\x0f\x85\x9e\x00\x00\x00\x8b\x07\x6a\x44\x8b\xcf\xff\x50\x5c\x50\xb9\x58\x19\x20\x07\xe8\x10\xd1\xef\xff\x8b\xd8\x85\xdb\x0f\x84\x80\x00\x00\x00"},
    {0x593a46,52,"\x8b\x55\x00\x57\xb9\x58\x19\x20\x07\x89\x54\x24\x28\xe8\x68\xdd\xef\xff\x89\x44\x24\x28\x8b\x06\x8b\xce\xff\x50\x3c\x8b\x08\x53\x89\x4c\x24\x30\x6a\x01\x8b\x4c\x24\x24\x8d\x54\x24\x2c\x52\xe8\x26\xf9\xff\xff"},
};
bool verifyStrategyFollowupLayout(){
    for(const auto& s:g_strategyFollowupLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected strategy followup branch at %08x; rejected",s.start);return false;
    }
    return true;
}
