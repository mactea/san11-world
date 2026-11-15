// Bole146 unlocks a foreign-city free officer in the candidate list. Only a
// selected, actually appended candidate and a committed native command count.
struct BoleExecution;
thread_local BoleExecution* g_boleExecution=nullptr;
thread_local BoleExecution* g_boleOrder=nullptr;
thread_local BoleExecution* g_boleGate=nullptr;
uintptr_t g_boleGateCaller=0x5c595a,g_boleBuilderCaller=0x5c580f;
struct BoleGateScope {
    BoleExecution* previous;
    explicit BoleGateScope(BoleExecution* execution):previous(g_boleGate){g_boleGate=execution;}
    ~BoleGateScope(){g_boleGate=previous;}
};
struct BoleOrderScope {
    BoleExecution* previous;
    explicit BoleOrderScope(BoleExecution* execution):previous(g_boleOrder){g_boleOrder=execution;}
    ~BoleOrderScope(){g_boleOrder=previous;}
};
struct BoleExecution {
    void* source;uint8_t* target;uint8_t* executor;
    const void* provider=nullptr;
    int providerId=-1,owned=-1,providerForce=-1,sourceId=-1,sourceForce=-1,targetId=-1,executorId=-1,executorForce=-1;
    int location=-1,locationCity=-1,locationForce=-1,status=-1,force=-1,affinity=-1,outcome=-1,parameter2=-1;
    bool appended=false,consumed=false,handled=false,assigned=false;
    uint64_t generation;
    BoleExecution* previous;
    explicit BoleExecution(void** command):source(command[0]),target((uint8_t*)command[1]),executor((uint8_t*)command[2]),
        generation(skilltelemetry::contextGeneration()),previous(g_boleExecution){g_boleExecution=this;}
    ~BoleExecution(){g_boleExecution=previous;}
};
bool boleLive(const BoleExecution& s){
    return skilltelemetry::enabled() && s.generation==skilltelemetry::contextGeneration() && s.provider &&
        g_baseById((void*)0x7201958,s.sourceId)==s.source &&
        g_telemetryPerson((void*)0x7201958,s.targetId)==s.target && g_telemetryPerson((void*)0x7201958,s.executorId)==s.executor &&
        g_telemetryPerson((void*)0x7201958,s.providerId)==s.provider &&
        g_growthValid(s.source) && g_growthValid(s.target) && g_growthValid(s.executor) && g_growthValid((void*)s.provider) &&
        incomeTarget(s.source).force==s.sourceForce && growthTarget((uint8_t*)s.provider,s.providerId).force==s.providerForce &&
        growthTarget(s.executor,s.executorId).force==s.executorForce &&
        growthPersonId(s.target)==s.targetId && growthPersonId(s.executor)==s.executorId &&
        growthPersonId((void*)s.provider)==s.providerId && *(const int*)((const uint8_t*)s.provider+0xe8)==s.owned && contains(s.owned,146);
}
void boleCandidateBegin(void* source,void* target){
    auto s=g_boleExecution;
    if(s && s==g_boleGate && !s->consumed && s->source==source && s->target==target){s->provider=nullptr;s->appended=false;}
}
void boleCandidateAllowed(void* source,uint8_t* target,int holder,int affinity){
    auto s=g_boleExecution;
    if(!skilltelemetry::enabled() || !s || s!=g_boleGate || s->consumed || s->source!=source || s->target!=target ||
       s->generation!=skilltelemetry::contextGeneration() || !g_growthValid(source) || !g_growthValid(target) ||
       !g_growthValid(s->executor) || *(int*)(target+0xa0)!=4)return;
    auto actor=g_telemetryPerson((void*)0x7201958,holder);int id=actor?growthPersonId(actor):-1;
    if(id!=holder || id<0 || !contains(*(int*)(actor+0xe8),146))return;
    auto origin=incomeTarget(source);int person=growthPersonId(target),executor=growthPersonId(s->executor),location=*(int*)(target+0x98);
    auto where=g_baseById((void*)0x7201958,location);
    if(origin.base<0 || g_baseById((void*)0x7201958,origin.base)!=source || person<0 || executor<0 || !where || !g_growthValid(where))return;
    int foreign=incomeTarget(where).force;if(foreign==origin.force)return;
    s->provider=actor;s->providerId=id;s->owned=*(int*)(actor+0xe8);s->sourceId=origin.base;s->sourceForce=origin.force;
    s->targetId=person;s->executorId=executor;s->location=location;s->locationCity=incomeTarget(where).city;s->locationForce=foreign;
    s->status=*(int*)(target+0xa0);s->force=growthTarget(target,person).force;s->affinity=affinity;
    s->providerForce=growthTarget(actor,id).force;s->executorForce=growthTarget(s->executor,executor).force;
}
typedef void*(__thiscall* BoleAppend)(void*,void*);
BoleAppend g_boleAppendOriginal=(BoleAppend)0x47c1b0;
void* __fastcall boleAppend(void* list,void*,void* person,uintptr_t builderCaller){
    auto s=g_boleExecution;bool observe=s && s==g_boleGate && builderCaller==g_boleBuilderCaller && !s->consumed && s->target==person && boleLive(*s);
    int before=observe?*(int*)((uint8_t*)list+0xc):0;
    void* node=g_boleAppendOriginal(list,person);
    if(observe && boleLive(*s) && node && *(void**)((uint8_t*)node+8)==person &&
       *(void**)((uint8_t*)list+8)==node && *(int*)((uint8_t*)list+0xc)==before+1)s->appended=true;
    return node;
}
__declspec(naked) void boleAppendBridge(){__asm {push [esp+0x18]
    push [esp+8]
    call boleAppend
    ret 4}}
uint32_t g_boleGateResume=0x5c56d7;
__declspec(naked) void boleGateNative(){__asm {push -1
    push 0x734636
    jmp dword ptr [g_boleGateResume]}}
typedef int(__cdecl* BoleGate)(void*);
BoleGate g_boleGateOriginal=(BoleGate)boleGateNative;
int __cdecl boleCanExecute(void* source){
    auto s=g_boleExecution;
    BoleGateScope gate(skilltelemetry::enabled() && s && !s->consumed && s->source==source && (uintptr_t)_ReturnAddress()==g_boleGateCaller?s:nullptr);
    return g_boleGateOriginal(source);
}
typedef int(__cdecl* BoleResolve)(void*,void*,int,int,int);
BoleResolve g_boleResolveOriginal=(BoleResolve)0x5c4840;
int __cdecl boleResolve(void* executor,void* target,int location,int outcome,int mode){
    auto s=g_boleExecution;
    bool observe=s && !s->consumed && s->appended && boleLive(*s) && s->executor==executor && s->target==target && s->location==location && mode==0;
    if(observe){s->consumed=true;s->outcome=outcome;}
    int result=g_boleResolveOriginal(executor,target,location,outcome,mode);
    if(observe && boleLive(*s))s->handled=true;
    return result;
}
typedef int(__thiscall* BoleAssign)(void*,void*,int,int,int,int,int,int);
typedef int(__thiscall* BoleStore)(void*,int,int,int,int,int,int);
BoleAssign g_boleAssignOriginal=(BoleAssign)0x4a7410;
BoleStore g_boleStoreOriginal=(BoleStore)0x489bd0;
int __fastcall boleAssign(void* self,void*,void* executor,int job,int person,int base,int parameter2,int a,int b){
    auto s=g_boleExecution;
    bool observe=s && !s->consumed && s->appended && boleLive(*s) && s->executor==executor && job==12 && person==s->targetId && base==s->location;
    BoleOrderScope order(observe?s:nullptr);if(observe)s->consumed=true;
    return g_boleAssignOriginal(self,executor,job,person,base,parameter2,a,b);
}
int __fastcall boleStore(uint8_t* executor,void*,int job,int person,int base,int parameter2,int a,int b){
    auto s=g_boleOrder;
    bool observe=s && s==g_boleExecution && s->appended && boleLive(*s) && s->executor==executor && job==12 && person==s->targetId && base==s->location;
    int result=g_boleStoreOriginal(executor,job,person,base,parameter2,a,b);
    int values[]={job,person,base,parameter2,a,b};
    if(observe && boleLive(*s) && !memcmp(executor+0x13c,values,sizeof values)){s->assigned=true;s->parameter2=parameter2;}
    return result;
}
uint32_t g_boleExecuteResume=0x5c5946;
__declspec(naked) void boleExecuteNative(){__asm {push ebp
    mov ebp,esp
    and esp,0xfffffff8
    jmp dword ptr [g_boleExecuteResume]}}
typedef int(__cdecl* BoleExecute)(void**);
BoleExecute g_boleExecuteOriginal=(BoleExecute)boleExecuteNative;
int __cdecl boleExecute(void** command){
    if(!skilltelemetry::enabled())return g_boleExecuteOriginal(command);
    BoleExecution execution(command);int result=g_boleExecuteOriginal(command);auto& s=execution;
    bool live=boleLive(s);
    bool queued=live && s.assigned && *(int*)(s.executor+0x13c)==12 && *(int*)(s.executor+0x140)==s.targetId && *(int*)(s.executor+0x144)==s.location;
    if(result && s.appended && (s.handled || queued) && live){
        auto target=growthTarget(s.target,s.targetId);target.base=s.location;target.city=s.locationCity;
        char context[640];sprintf_s(context,"{\"source_base\":%d,\"command_force\":%d,\"executor_person\":%d,\"candidate_location\":%d,\"candidate_location_force\":%d,\"affinity_distance\":%d,\"native_outcome\":%d,\"order_assigned\":%s,\"order_parameter_2\":%d,\"status_before\":%d,\"status_after\":%d,\"force_before\":%d,\"force_after\":%d,\"location_after\":%d,\"counterfactual_evaluated\":false}",
            s.sourceId,s.sourceForce,s.executorId,s.location,s.locationForce,s.affinity,s.outcome,queued?"true":"false",s.parameter2,
            s.status,*(int*)(s.target+0xa0),s.force,target.force,*(int*)(s.target+0x98));target.context=context;
        skilltelemetry::appliedEvent(s.provider,s.owned,146,"officer_recruitment",queued?"remote_recruitment_order_assigned":"remote_recruitment_attempt_resolved",0x5c5940,target);
    }
    return result;
}
struct BoleLayout {uint32_t start;int size;const char* bytes;};
const BoleLayout g_boleLayouts[]={
    {0x4cf600,43,"\x8b\x4c\x24\x04\x56\xe8\x46\xc8\xfa\xff\x8b\x74\x24\x0c\x56\xe8\x1c\xb0\xfa\xff\x83\xc4\x04\x85\xc0\x75\x04\x5e\xc2\x08\x00\x53\x56\xb9\x58\x19\x20\x07\xe8\x45\x21\xfc\xff"},
    {0x4cf644,12,"\x55\x57\x33\xed\xeb\x06\x8d\x9b\x00\x00\x00\x00"},
    {0x5c5946,55,"\x81\xec\xbc\x07\x00\x00\x53\x8b\x5d\x08\x8b\x03\x56\x57\x50\xe8\x76\xfd\xff\xff\x83\xc4\x04\x85\xc0\x74\x0b\x8b\xcb\xe8\xe8\xec\xff\xff\x85\xc0\x75\x09\x33\xc0\x5f\x5e\x5b\x8b\xe5\x5d\xc3\x8b\x3b\x8b\x73\x04\x8b\x5b\x08"},
    {0x5c5ab7,97,"\x56\xb9\x58\x19\x20\x07\x89\x44\x24\x18\xe8\x4a\xb8\xec\xff\x8b\x54\x24\x1c\x6a\x00\x6a\x00\x52\x57\xb9\x58\x19\x20\x07\x8b\xf0\xe8\x94\xbc\xec\xff\x50\x56\x6a\x0c\x53\xb9\x5c\x89\x99\x07\xe8\x25\x19\xee\xff\x8b\x44\x24\x14\x50\x53\xb9\x5c\x89\x99\x07\xe8\x65\xfb\xed\xff\x6a\x00\x6a\xff\x57\x53\xb9\x5c\x89\x99\x07\xe8\x85\x1e\xee\xff\x6a\x01\x53\xb9\x5c\x89\x99\x07\xe8\xe8\xfa\xed\xff"},
    {0x489bd0,63,"\x8b\x44\x24\x04\x8b\x54\x24\x08\x89\x81\x3c\x01\x00\x00\x8b\x44\x24\x0c\x89\x91\x40\x01\x00\x00\x8b\x54\x24\x10\x89\x81\x44\x01\x00\x00\x8b\x44\x24\x14\x89\x91\x48\x01\x00\x00\x8b\x54\x24\x18\x89\x81\x4c\x01\x00\x00\x89\x91\x50\x01\x00\x00\xc2\x18\x00"},
    {0x47c1b0,151,"\x6a\xff\x68\x38\xeb\x72\x00\x64\xa1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x51\x56\x57\x68\x68\x78\xee\x06\x8b\xf1\xff\x15\x60\xe3\x74\x00\x8b\x4e\x08\x8b\x06\x6a\x00\x51\x8b\xce\xc7\x44\x24\x1c\x00\x00\x00\x00\xff\x10\x8b\xf8\x85\xff\x75\x20\x68\x68\x78\xee\x06\xff\x15\x5c\xe3\x74\x00\x5f\x33\xc0\x5e\x8b\x4c\x24\x04\x64\x89\x0d\x00\x00\x00\x00\x83\xc4\x10\xc2\x04\x00\x8b\x54\x24\x1c\x89\x57\x08\x8b\x46\x08\x85\xc0\x74\x04\x89\x38\xeb\x03\x89\x7e\x04\x68\x68\x78\xee\x06\x89\x7e\x08\xff\x15\x5c\xe3\x74\x00\x8b\x4c\x24\x0c\x8b\xc7\x5f\x5e\x64\x89\x0d\x00\x00\x00\x00\x83\xc4\x10\xc2\x04\x00"},
    {0x4cf6bf,88,"\x6a\x10\x8b\xce\xe8\x18\xa9\xfb\xff\x85\xc0\x74\x50\x8b\x86\x98\x00\x00\x00\x50\xb9\x58\x19\x20\x07\xe8\x23\x16\xfc\xff\x8b\xf8\x57\xe8\x4b\xaf\xfa\xff\x83\xc4\x04\x85\xc0\x0f\x84\xb7\x00\x00\x00\x8b\x17\x8b\xcf\xff\x52\x40\x50\xe8\xb3\x2c\xf4\xff\x83\xc4\x04\x85\xc0\x0f\x84\x95\x00\x00\x00\x8b\x07\x8b\xcf\xff\x50\x40\x3b\xc3\x0f\x85\x90\x00\x00\x00"},
};
bool verifyBoleLayout(){
    for(const auto& layout:g_boleLayouts)if(memcmp((const void*)(g_strategyCodeBase+layout.start-0x400000),layout.bytes,layout.size))return false;
    return true;
}
