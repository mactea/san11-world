// The native modal input is a single shared slot. Copy canonical identity data
// for its live invocation; never transfer the worker's command/RAII pointers.
struct DiplomacyContestPending {
    uint64_t token=0,factoryRevision=0;PersonReceipt provider,ruler;
    void* force=nullptr;void* controller=nullptr;int forceId=-1,kind=-1;
    bool claimed=false;
    bool valid()const{
        return token && provider.valid(86) && ruler.valid() &&
            *(const int*)((const uint8_t*)provider.person+0x13c)==kind &&
            forceId>=0 && g_diplomacyForce((void*)0x7201958,forceId)==force &&
            g_diplomacyForceId((void*)0x7201958,force)==forceId && *(int*)((uint8_t*)force+4)==ruler.id;
    }
};
SRWLOCK g_diplomacyContestLock=SRWLOCK_INIT;
DiplomacyContestPending g_diplomacyContestPending;
uint64_t g_diplomacyContestToken=0;
void diplomacyContestEnd(uint64_t token){
    if(!token)return;AcquireSRWLockExclusive(&g_diplomacyContestLock);
    if(g_diplomacyContestPending.token==token)g_diplomacyContestPending=DiplomacyContestPending{};
    ReleaseSRWLockExclusive(&g_diplomacyContestLock);
}
void diplomacyContestPublish(DiplomacyInteractionScope* scope){
    auto e=scope?scope->execution:nullptr;
    if(!e || scope->contestToken || e!=g_diplomacyExecution || !e->valid() || e->target.person<0 ||
       (e->kind!=16 && e->kind!=18 && e->kind!=20) || *(int*)((uint8_t*)e->actor+0x13c)!=e->kind ||
       *(int*)g_diplomacyModalState!=2 || *(void**)(g_diplomacyModalState+0x24)!=e->actor ||
       *(int*)(g_diplomacyModalState+0x28)!=1 || *(int*)(g_diplomacyModalState+0x2c)!=1)return;
    DiplomacyContestPending pending;pending.provider=PersonReceipt(e->actor);pending.ruler=PersonReceipt(e->rulerActor);
    pending.force=e->force;pending.forceId=e->target.force;pending.kind=e->kind;
    AcquireSRWLockExclusive(&g_diplomacyContestLock);
    pending.token=++g_diplomacyContestToken;g_diplomacyContestPending=pending;scope->contestToken=pending.token;
    ReleaseSRWLockExclusive(&g_diplomacyContestLock);
}
DiplomacyContestPending diplomacyContestSnapshot(){
    AcquireSRWLockShared(&g_diplomacyContestLock);auto pending=g_diplomacyContestPending;
    ReleaseSRWLockShared(&g_diplomacyContestLock);return pending;
}
bool diplomacyContestTicketLive(const DiplomacyContestPending& pending){
    auto current=diplomacyContestSnapshot();
    return pending.token && current.token==pending.token && current.controller==pending.controller && current.claimed;
}
bool diplomacyContestController(const DiplomacyContestPending& pending,void* controller,bool executing){
    int family=pending.kind==16?0:pending.kind==18?1:pending.kind==20?2:-1;if(family<0 || !controller)return false;
    const uint32_t objects[]={0x689d90,0x689d9c,0x689da2},vtables[]={0x68a76a,0x68b43a,0x68baca};
    auto object=(uint8_t*)controller;
    return controller==*(void**)(g_strategyCodeBase+objects[family]-0x400000) &&
        *(void**)object==*(void**)(g_strategyCodeBase+vtables[family]-0x400000) &&
        *(void**)(object+0x1c90)==pending.provider.person && *(void**)(object+0x1c94)==pending.force &&
        *(void**)(object+0x1c9c)==pending.ruler.person && *(int*)(object+0x1c88)==1 &&
        (!executing || *(int*)(object+0x1c8c)==1) && *(int*)(object+4)==1 && *(int*)(object+8)==1;
}
typedef void*(__cdecl* DiplomacyContestFactory)(void*,int,int);
uint32_t g_diplomacyContestFactoryResume=0x689dda;
__declspec(naked) void diplomacyContestFactoryNative(){__asm {mov ecx,[esp+4]
    mov eax,[ecx+0x13c]
    jmp dword ptr [g_diplomacyContestFactoryResume]}}
DiplomacyContestFactory g_diplomacyContestFactoryOriginal=(DiplomacyContestFactory)diplomacyContestFactoryNative;
void* __cdecl diplomacyContestFactory(void* person,int flag,int initial){
    if(!skilltelemetry::enabled())return g_diplomacyContestFactoryOriginal(person,flag,initial);
    auto pending=diplomacyContestSnapshot();bool actual=(uintptr_t)_ReturnAddress()==g_strategyCodeBase+0x290561;
    bool eligible=actual && person==pending.provider.person && flag==1 && initial==1 && pending.valid(),owner=false;
    uint64_t revision=0;
    // Every initialization revokes an earlier binding, including a direct UI
    // preview that reconstructs the same static object with identical bytes.
    AcquireSRWLockExclusive(&g_diplomacyContestLock);
    if(pending.token && g_diplomacyContestPending.token==pending.token){
        owner=eligible && !g_diplomacyContestPending.claimed;
        revision=++g_diplomacyContestPending.factoryRevision;
        g_diplomacyContestPending.controller=nullptr;
        if(actual)g_diplomacyContestPending.claimed=true;
    }
    ReleaseSRWLockExclusive(&g_diplomacyContestLock);
    void* controller=g_diplomacyContestFactoryOriginal(person,flag,initial);
    if(owner && pending.valid() && diplomacyContestController(pending,controller,false)){
        AcquireSRWLockExclusive(&g_diplomacyContestLock);
        if(g_diplomacyContestPending.token==pending.token && g_diplomacyContestPending.factoryRevision==revision)
            g_diplomacyContestPending.controller=controller;
        ReleaseSRWLockExclusive(&g_diplomacyContestLock);
    }
    return controller;
}
typedef int(__cdecl* DiplomacyContest)(void*,void*);
DiplomacyContest g_diplomacyContestOriginal=(DiplomacyContest)0x689d00;
int __cdecl diplomacyContest(void* person,void* opponent,void* controller,uintptr_t caller){
    if(!skilltelemetry::enabled())return g_diplomacyContestOriginal(person,opponent);
    auto pending=diplomacyContestSnapshot();
    uintptr_t expected=g_strategyCodeBase+(pending.kind==16?0x28ac15:pending.kind==18?0x28b962:0x28c070);
    bool owner=skilltelemetry::enabled() && caller==expected && person==pending.provider.person &&
        controller==pending.controller && pending.valid() && diplomacyContestTicketLive(pending) && diplomacyContestController(pending,controller,true);
    PersonReceipt participant(owner?opponent:nullptr);
    int result=g_diplomacyContestOriginal(person,opponent);
    auto live=[&]{return owner && result>=0 && result<=2 && participant.valid() && pending.valid() &&
        diplomacyContestTicketLive(pending) && diplomacyContestController(pending,controller,true);};
    if(live()){
        auto target=growthTarget((uint8_t*)opponent,participant.id);char context[320];
        sprintf_s(context,"{\"command_type\":%d,\"command_target_force\":%d,\"command_target_ruler\":%d,\"native_result_code\":%d,\"native_resolution\":\"%s\",\"packet_type\":25,\"result_scope\":\"native_contest_wrapper_return\",\"counterfactual_evaluated\":false}",
            pending.kind,pending.forceId,pending.ruler.id,result,result==1?"scheduled":"automatic");target.context=context;
        if(live())skilltelemetry::appliedEvent(person,pending.provider.owned,86,"diplomacy","negotiation_contest_dispatched",
            pending.kind==16?0x68ac15:pending.kind==18?0x68b962:0x68c070,target);
    }
    return result;
}
__declspec(naked) void diplomacyContestBridge(){__asm {mov eax,esp
    push [eax]
    push esi
    push [eax+8]
    push [eax+4]
    call diplomacyContest
    add esp,16
    ret}}
const struct DiplomacyContestLayout {uint32_t start;int size;const char* bytes;} g_diplomacyContestLayouts[]={
    {0x689d00,107,"\x83\xec\x20\x8b\x4c\x24\x24\x8b\x54\x24\x28\xb8\xe8\x03\x00\x00\x89\x44\x24\x04\x89\x44\x24\x10\x33\xc0\x89\x44\x24\x18\x89\x44\x24\x14\x8d\x04\x24\x50\xc7\x44\x24\x20\x19\x00\x00\x00\x89\x4c\x24\x04\xc7\x44\x24\x0c\x01\x00\x00\x00\x89\x54\x24\x10\xe8\x3d\x69\xe9\xff\x83\xc4\x04\x85\xc0\x75\x18\xb9\x14\x52\x8b\x00\xe8\xdc\x3e\xe9\xff\xf7\xd8\x1b\xc0\x83\xe0\xfe\x83\xc0\x02\x83\xc4\x20\xc3\xb8\x01\x00\x00\x00\x83\xc4\x20\xc3"},
    {0x689dda,50,"\x56\x50\xe8\x8f\xff\xff\xff\x8b\xf0\x83\xc4\x04\x85\xf6\x74\x1e\x8b\x54\x24\x0c\x8b\x44\x24\x10\x89\x56\x04\x8b\x16\x51\x8b\xce\x89\x46\x08\xff\x52\x08\x85\xc0\x74\x04\x8b\xc6\x5e\xc3\x33\xc0\x5e\xc3"},
    {0x689d70,92,"\x8b\x44\x24\x04\x83\xc0\xf1\x83\xf8\x06\x77\x31\xff\x24\x85\xb0\x9d\x68\x00\xb8\xc8\xa6\xc4\x09\xc3\xb8\x70\xc3\xc4\x09\xc3\xb8\x18\xe0\xc4\x09\xc3\xb8\xc0\xfc\xc4\x09\xc3\xb8\x68\x19\xc5\x09\xc3\xb8\x10\x36\xc5\x09\xc3\xb8\xb8\x52\xc5\x09\xc3\x33\xc0\xc3\x83\x9d\x68\x00\x8f\x9d\x68\x00\x89\x9d\x68\x00\x9b\x9d\x68\x00\x95\x9d\x68\x00\xa1\x9d\x68\x00\xa7\x9d\x68\x00"},
    {0x690549,41,"\xa1\xa0\x70\xc5\x09\x8b\x0d\x98\x70\xc5\x09\x50\xa1\x9c\x70\xc5\x09\x50\x51\xe8\x6f\x98\xff\xff\x83\xc4\x0c\x3b\xc3\x89\x86\xe4\x01\x02\x00\x0f\x85\xab\x00\x00\x00"},
    {0x68ac0a,31,"\x8b\x44\x24\x1c\x53\x50\xe8\xeb\xf0\xff\xff\x83\xc4\x08\x83\xf8\x01\x89\x84\x24\x74\x09\x00\x00\x75\x05\xe9\xea\x01\x00\x00"},
    {0x68b957,31,"\x8b\x54\x24\x1c\x57\x52\xe8\x9e\xe3\xff\xff\x83\xc4\x08\x83\xf8\x01\x89\x84\x24\x74\x09\x00\x00\x75\x05\xe9\x9f\xfe\xff\xff"},
    {0x68c065,31,"\x8b\x54\x24\x1c\x57\x52\xe8\x90\xdc\xff\xff\x83\xc4\x08\x83\xf8\x01\x89\x84\x24\x7c\x09\x00\x00\x75\x05\xe9\x9f\xfe\xff\xff"},
    {0x68a768,6,"\xc7\x06\x64\x04\x86\x00"},
    {0x68b438,6,"\xc7\x06\x84\x04\x86\x00"},
    {0x68bac8,6,"\xc7\x06\x94\x04\x86\x00"},
    {0x689ece,30,"\x8b\x86\x88\x1c\x00\x00\x85\xc0\x0f\x84\x9c\x00\x00\x00\x8d\x4e\x0c\xe8\xec\x0b\xff\xff\x85\xc0\x0f\x84\x8c\x00\x00\x00"},
    {0x689f25,42,"\x8b\x4e\x08\x8b\x56\x04\x8b\x06\x57\x51\x52\x8b\xce\xc7\x86\x8c\x1c\x00\x00\x01\x00\x00\x00\xff\x50\x0c\x8d\x4c\x24\x08\x8b\xf8\xc7\x86\x8c\x1c\x00\x00\x00\x00\x00\x00"},
    {0x51dc30,12,"\x8b\x41\x1c\x2b\x41\x18\xf7\xd8\x1b\xc0\x40\xc3"},
    {0x520783,53,"\xe8\x58\xd9\xff\xff\x85\xdb\x74\x2c\x6a\x01\x6a\x01\x6a\x1d\x6a\x18\xb9\xa0\x9f\x1b\x09\xe8\xf2\x4a\xfb\xff\xb8\x01\x00\x00\x00\x8b\x8c\x24\x84\x09\x00\x00\x64\x89\x0d\x00\x00\x00\x00\x5f\x5e\x5b\x8b\xe5\x5d\xc3"},
};
bool verifyDiplomacyContestLayout(){
    for(const auto& s:g_diplomacyContestLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
