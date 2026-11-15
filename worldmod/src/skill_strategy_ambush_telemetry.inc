typedef int(__thiscall* StrategyAmbushCommand)(void*,const uint32_t*,int,int,int,void*,int);
uint32_t g_strategyAmbushCommandResume=0x592746;
__declspec(naked) void strategyAmbushNative(){__asm {push ebp
    mov ebp,esp
    and esp,-8
    jmp dword ptr [g_strategyAmbushCommandResume]}}
StrategyAmbushCommand g_strategyAmbushOriginal=(StrategyAmbushCommand)strategyAmbushNative;
int __fastcall strategyAmbush(void* self,void*,const uint32_t* command,int success,int critical,int mode,void* actor,int flag){
    CriticalCopyScope owner;return g_strategyAmbushOriginal(self,command,success,critical,mode,actor,flag);
}
void strategyAmbushPrepared(uint8_t* packet,void* source,int tactic,uint32_t position,int mode,int critical,int last,uintptr_t caller){
    if(g_ambushConstructionCaller-g_strategyCodeBase+0x400000!=0x59289d ||
       caller-g_strategyCodeBase+0x400000!=0x5b0eb8 || tactic!=-1 || mode!=0 || critical!=1 || last!=1)return;
    auto owner=g_strategyCommand;if(!owner || owner->status.plan!=5)return;
    const auto& receipt=owner->status;const auto& r=receipt.record;
    if(source!=r.source || position!=r.position || !strategyStatusLive(receipt,owner) ||
       *(int*)packet!=1 || *(int*)(packet+0x70)!=0 || *(int*)(packet+0xc)<0 ||
       *(uint32_t*)(packet+0x58)!=receipt.position[0] || *(uint32_t*)(packet+0x5c)!=receipt.position[1])return;
    auto record=criticalLookup(packet);
    if(record.packet && (record.source!=source || record.target && record.target!=r.target))return;
    record.packet=packet;record.source=source;record.target=r.target;record.tactic=-1;record.weapon=receipt.weapon[0];
    record.strategy=receipt;memcpy(record.snapshot,packet,sizeof record.snapshot);bool failed=false;
    AcquireSRWLockExclusive(&g_criticalCacheLock);criticalSyncLocked();
    try{if(r.generation==g_criticalCacheGeneration)g_criticalRecords.insert_or_assign((uintptr_t)packet,record);}
    catch(...){failed=true;}
    ReleaseSRWLockExclusive(&g_criticalCacheLock);if(failed)criticalAllocationFailed();
}
bool strategyAmbushGrantMatches(const CriticalRecord& record,void* source,const uint8_t* packet,void* affected,int requested){
    const auto& p=record.strategy;
    return p.plan==5 && p.record.source==source && p.record.target==affected && record.source==source &&
        record.target==affected && record.ambush && strategyReceiptLive(p) &&
        !memcmp(record.snapshot,packet,sizeof record.snapshot) && *(const int*)packet==1 &&
        *(const int*)(packet+0x70)==1 && *(const int*)(packet+0x14)==20 && *(const int*)(packet+0xc)>=0 &&
        requested==-*(const int*)(packet+0xc);
}
void strategyAmbushGrantCommitted(const CriticalRecord& record,int requested,int before,int after,uintptr_t caller,skilltelemetry::Target target){
    const auto& p=record.strategy;target.actorUnit=p.sourceId;target.equipment=p.weapon[0];
    target.x=(int16_t)p.position[1];target.y=(int16_t)(p.position[1]>>16);char context[288];
    sprintf_s(context,"{\"strategy_id\":5,\"critical_result\":true,\"record_kind\":1,\"requested_troop_loss\":%d,\"command_mode\":%d,\"damage_attribution\":\"whole_ambush_effect_not_marginal_skill_bonus\",\"counterfactual_evaluated\":false}",
        -requested,p.record.commandMode);target.context=context;
    skilltelemetry::applied(p.record.actor,p.record.owned,p.record.effect,"strategy",
        "strategy_critical_ambush_troops_written","troops",before,after,caller,target);
}
struct StrategyAmbushLayout {uint32_t start;int size;const char* bytes;};
const StrategyAmbushLayout g_strategyAmbushLayouts[]={
    {0x59287a,70,"\x8b\x55\x10\x52\x33\xc0\xb9\x81\x01\x00\x00\x8d\xbc\x24\xf8\x03\x00\x00\x53\xf3\xab\x8d\x84\x24\xfc\x03\x00\x00\x56\x50\xe8\xf3\xe5\x01\x00\x8b\x7c\x24\x2c\x83\xc4\x1c\x6a\x03\x8d\x8c\x24\xec\x03\x00\x00\x51\x8b\xcf\xc7\x84\x24\xf0\x09\x00\x00\x01\x00\x00\x00\xe8\xe0\x37\xff\xff"},
};
bool verifyStrategyAmbushLayout(){
    for(const auto& s:g_strategyAmbushLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected strategy ambush construction at %08x; rejected",s.start);return false;
    }
    return true;
}
