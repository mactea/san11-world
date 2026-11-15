struct IronwallScope;
thread_local IronwallScope* g_ironwall=nullptr;
struct IronwallScope {
    void* source=nullptr;
    void* target=nullptr;
    void* queried=nullptr;
    const void* actor=nullptr;
    uint8_t* group=nullptr;
    const uint8_t* aggregate=nullptr;
    int holder=-1,actorId=-1,owned=-1,beforeCount=0,mode;
    bool countEmitted=false;
    uint64_t generation;
    IronwallScope* previous;
    explicit IronwallScope(int m):mode(m),generation(skilltelemetry::contextGeneration()),previous(g_ironwall){g_ironwall=this;}
    ~IronwallScope(){g_ironwall=previous;}
};
int ironwallHandler(void* self,const uint8_t* records,int mode){
    if(!skilltelemetry::enabled())return g_criticalHandlerTwoOriginal(self,records,mode);
    IronwallScope scope(mode);return doubleAttackHandler(self,records,mode);
}
int __fastcall ironwallFind(void* unit,void*,int effect){
    typedef int(__thiscall* Find)(void*,int);auto native=*(Find*)(*(uint8_t**)unit+0x5c);
    auto scope=g_ironwall;if(!scope || effect!=29)return native(unit,effect);
    ProgressionProbe probe(29);probe.scene="battle";probe.event="ironwall_eligibility";QueryOrigin origin(0x586568);
    int result=native(unit,effect);scope->queried=unit;scope->holder=result;return result;
}
uint32_t g_ironwallFindResume=0x586571,g_ironwallLimitResume=0x586594;
uint32_t g_ironwallAggregateResume=0x586615;
__declspec(naked) void ironwallAggregateResetBridge(){__asm {pushfd
    pushad
    lea eax,[esp+0x74]
    push eax
    call criticalReset
    add esp,4
    popad
    popfd
    xor eax,eax
    mov ecx,0x20
    jmp dword ptr [g_ironwallAggregateResume]}}
__declspec(naked) void ironwallFindBridge(){__asm {call ironwallFind
    push eax
    mov ecx,0x7201958
    jmp dword ptr [g_ironwallFindResume]}}
void __cdecl ironwallLimit(void* source,void* target,const void* actor,uint8_t* group,int count){
    auto scope=g_ironwall;
    if(!scope || scope->generation!=skilltelemetry::contextGeneration() || scope->queried!=target || count<=1 || count>12 ||
       !g_criticalCopyScope || g_criticalCopyScope->destination!=group)return;
    int id=growthPersonId((void*)actor);int owned=id>=0?*(const int*)((const uint8_t*)actor+0xe8):-1;
    if(id<0 || id!=scope->holder || !contains(owned,29) || !afterAttackMember(target,actor))return;
    scope->source=source;scope->target=target;scope->actor=actor;scope->actorId=id;scope->owned=owned;scope->group=group;scope->beforeCount=count;
}
__declspec(naked) void ironwallLimitBridge(){__asm {pushfd
    pushad
    lea eax,[esp+0x4fc]
    mov edx,[esp+0xafc]
    push edx
    push eax
    push edi
    push ebx
    push esi
    call ironwallLimit
    add esp,20
    popad
    popfd
    mov dword ptr [esp+0xad8],1
    jmp dword ptr [g_ironwallLimitResume]}}
typedef int(__cdecl* IronwallApply)(uint8_t*);
IronwallApply g_ironwallApplyOriginal=(IronwallApply)0x5b1870;
int __cdecl ironwallApply(uint8_t* aggregate){
    auto scope=g_ironwall;
    if(!scope || !scope->group || aggregate!=scope->group-0x488 || scope->generation!=skilltelemetry::contextGeneration())
        return g_ironwallApplyOriginal(aggregate);
    auto previous=scope->aggregate;scope->aggregate=aggregate;
    int result=g_ironwallApplyOriginal(aggregate);scope->aggregate=previous;return result;
}
bool ironwallGrantMatches(void* source,const uint8_t* packet,void* target,int requested){
    auto scope=g_ironwall;
    return scope && scope->actor && scope->source==source && scope->target==target && scope->aggregate==packet &&
        scope->generation==skilltelemetry::contextGeneration() && *(const int*)(scope->group+0x600)==1 &&
        *(const int*)(packet+0x70)==0 && requested==-*(const int*)(packet+0xc) && requested<=0 &&
        growthPersonId((void*)scope->actor)==scope->actorId && *(const int*)((const uint8_t*)scope->actor+0xe8)==scope->owned &&
        contains(scope->owned,29) && afterAttackMember(target,scope->actor);
}
void ironwallGrantCommitted(void* unit,int requested,int before,int after,uintptr_t caller,skilltelemetry::Target target){
    auto scope=g_ironwall;
    if(!scope || growthPersonId((void*)scope->actor)!=scope->actorId ||
       *(const int*)((const uint8_t*)scope->actor+0xe8)!=scope->owned || !afterAttackMember(unit,scope->actor))return;
    int attacker=target.actorUnit;target.actorUnit=target.unit;target.equipment=*(const int*)((const uint8_t*)unit+0xa8);
    uint32_t position=*(const uint32_t*)(scope->aggregate+0x5c);target.x=(int16_t)position;target.y=(int16_t)(position>>16);
    char context[320];
    sprintf_s(context,"{\"attacking_unit\":%d,\"handler_mode\":%d,\"attack_records_before\":%d,\"attack_records_after\":1,\"requested_troop_loss\":%d,\"damage_attribution\":\"whole_remaining_attack_not_prevented_loss\",\"counterfactual_evaluated\":false}",
        attacker,scope->mode,scope->beforeCount,-requested);target.context=context;
    if(!scope->countEmitted){
        scope->countEmitted=true;
        skilltelemetry::applied(scope->actor,scope->owned,29,"battle","ironwall_attack_group_reduced",
            "attack_records",scope->beforeCount,1,0x586589,target);
    }
    skilltelemetry::applied(scope->actor,scope->owned,29,"battle","ironwall_attack_settled",
        "troops",before,after,caller,target);
}

struct IronwallLayout {uint32_t start;int size;const char* bytes;};
const IronwallLayout g_ironwallLayouts[]={
    {0x5860d0,14,"\xb9\x81\x01\x00\x00\x8d\xbc\x24\xd8\x04\x00\x00\xf3\xa5"},
    {0x586554,64,"\x83\xbc\x24\xd8\x0a\x00\x00\x01\x0f\x86\xac\x00\x00\x00\x8b\x13\x6a\x1d\x8b\xcb\xff\x52\x5c\x50\xb9\x58\x19\x20\x07\xe8\x8a\xa5\xf0\xff\x8b\xf8\x57\xe8\xb2\x40\xef\xff\x83\xc4\x04\x85\xc0\x0f\x84\x85\x00\x00\x00\xc7\x84\x24\xd8\x0a\x00\x00\x01\x00\x00\x00"},
    {0x584510,131,"\x8b\x44\x24\x04\x8b\x10\x09\x11\x8b\x50\x04\x09\x51\x04\x8b\x50\x0c\x01\x51\x0c\x8b\x50\x10\x01\x51\x10\x8b\x50\x18\x01\x51\x18\x8b\x50\x14\x01\x51\x14\x8b\x50\x20\x01\x51\x20\x8b\x50\x28\x01\x51\x28\x8b\x50\x24\x01\x51\x24\x8b\x50\x2c\x01\x51\x2c\x8b\x50\x50\x89\x51\x50\x8b\x50\x58\x89\x51\x58\x8b\x50\x5c\x89\x51\x5c\x8b\x50\x60\x89\x51\x60\x8b\x50\x64\x89\x51\x64\x8b\x50\x68\x89\x51\x68\x8b\x50\x6c\x89\x51\x6c\x8b\x50\x70\x89\x51\x70\x8b\x50\x74\x89\x51\x74\x8b\x50\x78\x89\x51\x78\x8b\x40\x7c\x89\x41\x7c\xc2\x04\x00"},
    {0x58660e,169,"\x33\xc0\xb9\x20\x00\x00\x00\x8d\x7c\x24\x50\xf3\xab\x8b\xbc\x24\xd8\x0a\x00\x00\x4f\x78\x3c\x8b\xcf\xc1\xe1\x07\x8d\x94\x0c\x30\x05\x00\x00\x89\x54\x24\x14\x8b\x44\x24\x14\x83\xc0\xa8\x50\x8d\x4c\x24\x54\xe8\xca\xde\xff\xff\x8b\x44\x24\x14\x8b\x08\x89\x8c\xbc\xd4\x00\x00\x00\x4f\x2d\x80\x00\x00\x00\x85\xff\x89\x44\x24\x14\x7d\xd4\x8b\x8c\x24\xd8\x0a\x00\x00\x83\xf9\x01\x76\x4a\x83\xf9\x02\x8b\x44\x24\x5c\x89\x44\x24\x1c\x7c\x39\x8d\x14\x89\x8d\x7c\x89\x23\x52\xd1\xe7\xe8\xc7\xba\xee\xff\x0f\xb7\xc0\x03\xc7\x89\x44\x24\x28\x83\xc4\x04\xdb\x44\x24\x24\xd8\x0d\x3c\x4d\x84\x00\xdb\x44\x24\x1c\xd8\xc9\xe8\xca\x13\x18\x00\xdd\xd8\x8b\x8c\x24\xd8\x0a\x00\x00\x89\x44\x24\x5c"},
    {0x586bab,13,"\x8d\x44\x24\x50\x50\xe8\xbb\xac\x02\x00\x83\xc4\x04"},
};
bool verifyIronwallLayout(){
    for(const auto& s:g_ironwallLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected ironwall branch at %08x; rejected",s.start);return false;
    }
    return true;
}
