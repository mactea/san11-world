struct PincerScope:AfterAttackScope {
    int mode,holder=-1,actorId=-1,owned=-1,count=0;
    CriticalCopyScope* owner;
    PincerScope* previous;
    PincerScope(int m);
    ~PincerScope();
};
thread_local PincerScope* g_pincer=nullptr;
PincerScope::PincerScope(int m):AfterAttackScope(18,nullptr,nullptr),mode(m),owner(g_criticalCopyScope),previous(g_pincer){g_pincer=this;}
PincerScope::~PincerScope(){g_pincer=previous;}
int pincerHandler(void* self,const uint8_t* records,int mode){
    PincerScope scope(mode);return g_criticalHandlerTwoOriginal(self,records,mode);
}
int __fastcall pincerFind(void* unit,void*,int effect){
    typedef int(__thiscall* Find)(void*,int);auto native=*(Find*)(*(uint8_t**)unit+0x5c);auto scope=g_pincer;
    if(!scope || effect!=18)return native(unit,effect);
    ProgressionProbe probe(18);probe.scene="battle";probe.event="pincer_eligibility";QueryOrigin origin(0x586bea);
    int result=native(unit,effect);scope->source=unit;scope->holder=result;scope->actor=probe.actor;
    if(probe.actor){scope->actorId=growthPersonId((void*)probe.actor);scope->owned=*(const int*)((const uint8_t*)probe.actor+0xe8);}
    return result;
}
void __cdecl pincerSelected(void* target,int count){
    auto scope=g_pincer;if(!scope)return;scope->target=target;scope->count=count;
}
uint32_t g_pincerFindResume=0x586bf3;
__declspec(naked) void pincerFindBridge(){__asm {call pincerFind
    pushfd
    pushad
    mov eax,[esp+0xafc]
    push eax
    push ebx
    call pincerSelected
    add esp,8
    popad
    popfd
    push eax
    mov ecx,0x7201958
    jmp dword ptr [g_pincerFindResume]}}
bool pincerValid(const PincerScope& scope){
    return scope.owner && scope.owner==g_criticalCopyScope && scope.owner->destination &&
        scope.owner->generation==scope.generation && scope.holder>=0 && scope.holder==scope.actorId &&
        scope.count>1 && scope.count<=12 && *(const int*)(scope.owner->destination+0x600)==scope.count &&
        afterAttackValid(scope) && growthPersonId((void*)scope.actor)==scope.actorId &&
        *(const int*)((const uint8_t*)scope.actor+0xe8)==scope.owned;
}
StatusGrant g_pincerStatusOriginal=(StatusGrant)0x4aea70;
void __fastcall pincerStatus(void* self,void*,uint8_t* unit,void* source,int status,int turns,int display){
    auto scope=g_pincer;
    bool observe=scope && scope->source==source && scope->target==unit && status==1 && pincerValid(*scope);
    int before=observe?unit[0x28]:0,previousStatus=observe?*(int*)(unit+0x24):0;
    skilltelemetry::Target target;if(observe)target=afterAttackTarget(*scope,false);
    StatusWriteProbe write(observe?unit:nullptr);g_pincerStatusOriginal(self,unit,source,status,turns,display);
    if(observe && write.written && pincerValid(*scope) && *(int*)(unit+0x24)==status){
        char context[224];sprintf_s(context,"{\"handler_mode\":%d,\"attack_records\":%d,\"native_chance_percent\":50,\"status_before\":%d,\"status_after\":1,\"requested_turns\":%u,\"counterfactual_evaluated\":false}",
            scope->mode,scope->count,previousStatus,(unsigned)(uint8_t)turns);target.context=context;
        skilltelemetry::applied(scope->actor,scope->owned,18,"battle","pincer_status_applied","status_turns",before,unit[0x28],0x586c2b,target);
    }
}

struct PincerLayout {uint32_t start;int size;const char* bytes;};
const PincerLayout g_pincerLayouts[]={
    {0x4aec42,20,"\x8b\x94\x24\xc8\x07\x00\x00\x52\x8b\xce\xe8\x2f\x03\xfc\xff\x8b\x7c\x24\x18\x5d"},
    {0x46ef80,10,"\x8b\x44\x24\x04\x89\x41\x24\xc2\x04\x00"},
    {0x586bc0,112,"\x56\xe8\x6a\x3a\xef\xff\x83\xc4\x04\x85\xc0\x74\x63\x53\xe8\x5d\x3a\xef\xff\x83\xc4\x04\x85\xc0\x74\x56\x83\xbc\x24\xd8\x0a\x00\x00\x01\x76\x4c\x8b\x16\x6a\x12\x8b\xce\xff\x52\x5c\x50\xb9\x58\x19\x20\x07\xe8\x08\x9f\xf0\xff\x50\xe8\x32\x3a\xef\xff\x83\xc4\x04\x85\xc0\x74\x2b\x6a\x32\xe8\xc4\xb5\xee\xff\x83\xc4\x04\x85\xc0\x74\x1d\x6a\x01\x6a\x02\xe8\x34\xb5\xee\xff\x83\xc4\x04\xfe\xc0\xb9\x5c\x89\x99\x07\x50\x6a\x01\x56\x53\xe8\x40\x7e\xf2\xff"},
    {0x4aed1f,31,"\x3c\x05\x72\x02\xb0\x05\x0f\xb6\xc0\x50\x56\x8b\xcf\xe8\x9f\xf8\xff\xff\x5b\x5f\x5e\x81\xc4\xac\x07\x00\x00\xc2\x14\x00\xcc"},
    {0x4ae5d0,87,"\x53\x8b\x5c\x24\x08\x53\xe8\x55\xc0\xfc\xff\x83\xc4\x04\x85\xc0\x74\x3f\x8b\x44\x24\x0c\x56\x57\x0f\xb6\x7b\x28\x03\xc7\x85\xc0\x7f\x04\x33\xf6\xeb\x0c\x83\xf8\x05\xbe\x05\x00\x00\x00\x7d\x02\x8b\xf0\x53\xe8\x28\xc0\xfc\xff\x83\xc4\x04\x85\xc0\x74\x08\x56\x8b\xcb\xe8\x39\x7d\xfe\xff\x8b\xc6\x2b\xc7\x5f\x5e\x5b\xc2\x08\x00\x33\xc0\x5b\xc2\x08\x00"},
    {0x496350,28,"\x8a\x44\x24\x04\x84\xc0\x77\x08\x32\xc0\x88\x41\x28\xc2\x04\x00\x3c\x05\x72\x02\xb0\x05\x88\x41\x28\xc2\x04\x00"},
};
bool pincerStatusSetterLayout(){
    const auto& s=g_pincerLayouts[5];auto at=(const uint8_t*)(g_strategyCodeBase+s.start-0x400000);
    if(!memcmp(at,s.bytes,s.size))return true;
    // M9's shipped long-false-report cave raises only status2's setter limit.
    // Validate its complete executable payload and jump back, as well as every
    // original setter byte after the six stolen bytes. Unknown detours fail.
    if(at[0]!=0xe9 || at[5]!=0x90 || memcmp(at+6,s.bytes+6,s.size-6))return false;
    auto cave=(const uint8_t*)((uintptr_t)at+5+*(const int32_t*)(at+1));MEMORY_BASIC_INFORMATION memory={};
    if(!VirtualQuery(cave,&memory,sizeof memory) || memory.State!=MEM_COMMIT ||
       (memory.Protect&(PAGE_NOACCESS|PAGE_GUARD)) || memory.RegionSize<33 ||
       (uintptr_t)cave-(uintptr_t)memory.BaseAddress>memory.RegionSize-33)return false;
    const uint8_t prefix[]={0x8a,0x44,0x24,0x04,0x83,0x79,0x24,0x02,0x75,0x0c,0x3c,0x1e,0x76,0x02,0xb0,0x1e,
        0x88,0x41,0x28,0xc2,0x04,0x00,0x8a,0x44,0x24,0x04,0x84,0xc0,0xe9};
    return !memcmp(cave,prefix,sizeof prefix) && (uintptr_t)cave+33+*(const int32_t*)(cave+29)==(uintptr_t)at+6;
}
bool verifyPincerLayout(){
    for(const auto& s:g_pincerLayouts)if(s.start==0x496350?!pincerStatusSetterLayout():
        memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)!=0){
        if(g_log)g_log("skills: unexpected pincer branch at %08x; rejected",s.start);return false;
    }
    return true;
}
