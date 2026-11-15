void strategyStatusCritical(const StrategyCriticalScope& scope,int plan,uint32_t to){
    auto owner=g_strategyCommand;
    if(!owner || (plan!=2 && plan!=3 && plan!=5) || owner->command[0]!=(uint32_t)plan || owner->command[2]!=to)return;
    StrategyStatusReceipt receipt;receipt.plan=plan;memcpy(receipt.command,owner->command,sizeof receipt.command);
    receipt.sourceId=g_telemetryUnitId((void*)0x7201958,scope.source);receipt.targetId=g_telemetryUnitId((void*)0x7201958,scope.target);
    if(receipt.sourceId<0 || receipt.targetId<0 || receipt.command[1]!=(uint32_t)receipt.sourceId ||
       g_movementUnitById((void*)0x7201958,receipt.sourceId)!=scope.source ||
       g_movementUnitById((void*)0x7201958,receipt.targetId)!=scope.target)return;
    receipt.record={scope.source,scope.target,scope.actor,scope.actorId,scope.owned,scope.effect,owner->mode,to,scope.generation};
    void* units[2]={scope.source,scope.target};
    for(int n=0;n<2;++n){
        auto unit=(uint8_t*)units[n];receipt.force[n]=unitTarget(unit).force;receipt.kind[n]=*(int*)(unit+8);
        receipt.weapon[n]=*(int*)(unit+0xa8);memcpy(receipt.ids[n],unit+0xc,sizeof receipt.ids[n]);
        typedef const uint32_t*(__thiscall* Position)(void*);receipt.position[n]=*(*(Position*)(*(uint8_t**)unit+0x3c))(unit);
    }
    memcpy(receipt.affiliation,(const uint8_t*)scope.actor+0x94,sizeof receipt.affiliation);owner->status=receipt;
}
bool strategyReceiptLive(const StrategyStatusReceipt& p){
    const auto& r=p.record;
    if(!skilltelemetry::enabled() || !r.actor || r.generation!=skilltelemetry::contextGeneration() ||
       g_movementUnitById((void*)0x7201958,p.sourceId)!=r.source ||
       g_movementUnitById((void*)0x7201958,p.targetId)!=r.target ||
       !g_growthValid(r.source) || !g_growthValid(r.target) || growthPersonId((void*)r.actor)!=r.actorId ||
       *(const int*)((const uint8_t*)r.actor+0xe8)!=r.owned || !contains(r.owned,r.effect) ||
       memcmp(p.affiliation,(const uint8_t*)r.actor+0x94,sizeof p.affiliation))return false;
    void* units[2]={r.source,r.target};
    for(int n=0;n<2;++n){
        auto unit=(uint8_t*)units[n];
        if(unitTarget(unit).force!=p.force[n] || *(int*)(unit+8)!=p.kind[n] || *(int*)(unit+0xa8)!=p.weapon[n] ||
           memcmp(p.ids[n],unit+0xc,sizeof p.ids[n]))return false;
        typedef const uint32_t*(__thiscall* Position)(void*);
        if(*(*(Position*)(*(uint8_t**)unit+0x3c))(unit)!=p.position[n])return false;
    }
    return true;
}
bool strategyStatusLive(const StrategyStatusReceipt& p,const StrategyCommandScope* owner){
    return owner && g_strategyCommand==owner && !memcmp(p.command,owner->command,sizeof p.command) && strategyReceiptLive(p);
}
StatusGrant g_strategyStatusOriginal=(StatusGrant)0x4aea70;
void __fastcall strategyStatus(void* self,void*,uint8_t* unit,void* source,int status,int turns,int display){
    uintptr_t caller=(uintptr_t)_ReturnAddress()-g_strategyCodeBase+0x400000;
    auto owner=g_strategyCommand;auto receipt=owner?owner->status:StrategyStatusReceipt{};
    bool native=(receipt.plan==2 && caller==0x591943 && status==2 && (uint8_t)turns==3) ||
        (receipt.plan==3 && caller==0x591b93 && status==1 && (uint8_t)turns==3) ||
        (receipt.plan==5 && caller==0x592938 && status==1 && ((uint8_t)turns==1 || (uint8_t)turns==2));
    bool observe=native && receipt.record.source==source && receipt.record.target==unit && strategyStatusLive(receipt,owner);
    int before=observe?unit[0x28]:0,oldStatus=observe?*(int*)(unit+0x24):0;
    StatusWriteProbe write(observe?unit:nullptr);g_strategyStatusOriginal(self,unit,source,status,turns,display);
    if(observe && write.written && strategyStatusLive(receipt,owner) && *(int*)(unit+0x24)==status){
        auto target=unitTarget(unit);target.actorUnit=receipt.sourceId;target.equipment=receipt.weapon[0];
        target.x=(int16_t)receipt.position[1];target.y=(int16_t)(receipt.position[1]>>16);char context[352];
        sprintf_s(context,"{\"strategy_id\":%d,\"critical_result\":true,\"status_before\":%d,\"status_after\":%d,\"requested_turns\":%d,\"command_mode\":%d,\"result_scope\":\"critical_strategy_status_commit_not_marginal_bonus\",\"counterfactual_evaluated\":false}",
            receipt.plan,oldStatus,status,(int)(uint8_t)turns,receipt.record.commandMode);target.context=context;
        skilltelemetry::applied(receipt.record.actor,receipt.record.owned,receipt.record.effect,"strategy",
            "strategy_critical_status_written","status_turns",before,unit[0x28],caller,target);
    }
}
struct StrategyStatusLayout {uint32_t start;int size;const char* bytes;};
const StrategyStatusLayout g_strategyStatusLayouts[]={
    {0x591912,49,"\x8b\x84\x24\xf4\x03\x00\x00\x85\xc0\x74\x07\xb8\x03\x00\x00\x00\xeb\x0e\x6a\x02\xe8\x25\x08\xee\xff\x0f\xb7\xc0\x83\xc4\x04\x40\x6a\x01\x50\x6a\x02\x56\x57\xb9\x5c\x89\x99\x07\xe8\x2d\xd1\xf1\xff"},
    {0x591b62,49,"\x8b\x84\x24\xf4\x03\x00\x00\x85\xc0\x74\x07\xb8\x03\x00\x00\x00\xeb\x0e\x6a\x02\xe8\xd5\x05\xee\xff\x0f\xb7\xc0\x83\xc4\x04\x40\x6a\x01\x50\x6a\x01\x56\x57\xb9\x5c\x89\x99\x07\xe8\xdd\xce\xf1\xff"},
    {0x59290b,45,"\x8b\x45\x10\x85\xc0\x74\x10\x6a\x02\xe8\x37\xf8\xed\xff\x0f\xb7\xc0\x83\xc4\x04\x40\xeb\x05\xb8\x01\x00\x00\x00\x6a\x01\x50\x6a\x01\x56\x53\xb9\x5c\x89\x99\x07\xe8\x38\xc1\xf1\xff"},
};
bool verifyStrategyStatusLayout(){
    for(const auto& s:g_strategyStatusLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected strategy status branch at %08x; rejected",s.start);return false;
    }
    return true;
}
