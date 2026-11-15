struct AssistScope;
thread_local AssistScope* g_assist=nullptr;
struct AssistScope {
    void* source=nullptr;
    void* primary;
    void* target;
    AssistAttribution attribution;
    int chance=-1,decision=-1;
    bool selected=false;
    uint64_t generation;
    AssistScope* previous;
    AssistScope(void* p,void* t):primary(p),target(t),generation(skilltelemetry::contextGeneration()),previous(g_assist){g_assist=this;}
    ~AssistScope(){g_assist=previous;}
};
typedef int(__cdecl* AssistAttempt)(void*,int,void*,void*);
uint32_t g_assistAttemptResume=0x587176,g_assistOwnerZeroResume=0x58724b;
__declspec(naked) void assistAttemptNative(){__asm {push ebp
    mov ebp,esp
    and esp,-8
    jmp dword ptr [g_assistAttemptResume]}}
AssistAttempt g_assistAttemptOriginal=(AssistAttempt)assistAttemptNative;
int __cdecl assistAttempt(void* cell,int mode,void* primary,void* target){
    if(!skilltelemetry::enabled())return g_assistAttemptOriginal(cell,mode,primary,target);
    CriticalCopyScope owner;AssistScope scope(primary,target);return g_assistAttemptOriginal(cell,mode,primary,target);
}
__declspec(naked) void assistOwnerZeroBridge(){__asm {pushfd
    pushad
    lea eax,[esp+0x34]
    push eax
    call criticalOwnerZero
    add esp,4
    popad
    popfd
    mov ecx,0x181
    jmp dword ptr [g_assistOwnerZeroResume]}}
typedef int(__cdecl* AssistChance)(void*,void*);
typedef int(__thiscall* AssistPersonCheck)(void*,int);
typedef int(__cdecl* AssistRoll)(int);
typedef int(__cdecl* AssistPacket)(uint8_t*,void*,uint32_t);
uint32_t g_assistChanceAddress=0x585510;
__declspec(naked) int __cdecl assistChanceNative(void*,void*){__asm {mov ecx,[esp+4]
    push [esp+8]
    call dword ptr [g_assistChanceAddress]
    add esp,4
    ret}}
AssistChance g_assistChanceOriginal=assistChanceNative;
AssistPersonCheck g_assistHasOriginal=(AssistPersonCheck)0x4890f0,g_assistDislikeOriginal=(AssistPersonCheck)0x4889e0;
AssistRoll g_assistRollOriginal=(AssistRoll)0x4721d0;
AssistPacket g_assistPacketOriginal=(AssistPacket)0x5b0680;
int __cdecl assistChance(void* unit,void* primary){
    auto scope=g_assist;
    if(!scope || scope->primary!=primary)return g_assistChanceOriginal(unit,primary);
    scope->source=unit;scope->attribution=AssistAttribution{};scope->selected=false;scope->decision=-1;
    int result=g_assistChanceOriginal(unit,primary);scope->chance=result;return result;
}
// Native585510 receives ECX plus one stack argument, but RET leaves that
// argument for587225 ADD ESP,8 together with the random sampler argument.
__declspec(naked) void assistChanceBridge(){__asm {push [esp+4]
    push ecx
    call assistChance
    add esp,8
    ret}}
int __fastcall assistHas(void* person,void*,int effect){
    auto scope=g_assist;
    if(!scope || !scope->source || effect!=26)return g_assistHasOriginal(person,effect);
    ProgressionProbe probe(26);probe.scene="battle";probe.event="assist_eligibility";QueryOrigin origin(0x585558);
    int result=g_assistHasOriginal(person,effect);
    if(result && scope->generation==skilltelemetry::contextGeneration()){
        scope->attribution.actor=person;scope->attribution.actorId=growthPersonId(person);
        scope->attribution.owned=*(const int*)((const uint8_t*)person+0xe8);
    }
    return result;
}
int __fastcall assistDislike(void* person,void*,int other){
    int result=g_assistDislikeOriginal(person,other);auto scope=g_assist;
    if(scope && scope->attribution.actor==person && !result)scope->selected=true;
    return result;
}
bool assistActorValid(const AssistScope& scope){
    const auto& a=scope.attribution;
    return scope.selected && scope.chance==30 && a.actor && a.actorId>=0 &&
        g_criticalCopyScope && g_criticalCopyScope->generation==scope.generation &&
        scope.generation==skilltelemetry::contextGeneration() && growthPersonId((void*)a.actor)==a.actorId &&
        *(const int*)((const uint8_t*)a.actor+0xe8)==a.owned && contains(a.owned,26) && afterAttackMember(scope.source,a.actor);
}
int __cdecl assistRoll(int chance){
    int result=g_assistRollOriginal(chance);auto scope=g_assist;
    if(!scope || chance!=30 || !assistActorValid(*scope))return result;
    scope->decision=result;auto target=unitTarget(scope->target),source=unitTarget(scope->source),primary=unitTarget(scope->primary);
    if(source.unit<0 || target.unit<0 || primary.unit<0 || g_movementUnitById((void*)0x7201958,source.unit)!=scope->source ||
       g_movementUnitById((void*)0x7201958,target.unit)!=scope->target || g_movementUnitById((void*)0x7201958,primary.unit)!=scope->primary)return result;
    scope->attribution.primaryUnit=primary.unit;scope->attribution.primaryForce=primary.force;
    target.actorUnit=source.unit;target.equipment=*(const int*)((const uint8_t*)scope->source+0xa8);
    typedef const uint32_t*(__thiscall* Position)(void*);
    auto position=*(*(Position*)(*(uint8_t**)scope->target+0x3c))(scope->target);
    target.x=(int16_t)position;target.y=(int16_t)(position>>16);char context[224];
    sprintf_s(context,"{\"primary_unit\":%d,\"primary_force_at_decision\":%d,\"native_chance_percent\":30,\"native_result\":%d,\"counterfactual_evaluated\":false}",
        primary.unit,primary.force,result);target.context=context;
    skilltelemetry::appliedEvent(scope->attribution.actor,scope->attribution.owned,26,"battle","assist_skill_chance_used",0x587220,target);
    return result;
}
int __fastcall assistDefenseHas(void* unit,void*,uint8_t* packet,int effect){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);
    auto scope=g_assist;auto c=g_criticalCalculation;
    if(!skilltelemetry::enabled() || !scope || !c || c->packet!=packet || c->source!=scope->source || unit!=scope->target ||
       scope->generation!=c->generation || c->generation!=skilltelemetry::contextGeneration() || (effect!=27 && effect!=28))
        return native(unit,effect);
    c->defense=DefenseAttribution{};int before=*(const int*)(packet+0xc);
    ProgressionProbe probe(effect);probe.scene="battle";probe.event="damage_immunity_eligibility";
    int result=native(unit,effect);
    if(result && probe.actor && before>=0 && c->generation==skilltelemetry::contextGeneration() && (!c->target || c->target==unit)){
        auto& a=c->defense;a.actor=probe.actor;a.actorId=growthPersonId((void*)probe.actor);
        a.owned=*(const int*)((const uint8_t*)probe.actor+0xe8);a.effect=effect;a.incomingDamage=before;c->target=unit;
    }
    return result;
}
uint32_t g_assistUnyieldingResume=0x5b0b53,g_assistDiamondResume=0x5b0b80;
__declspec(naked) void assistUnyieldingBridge(){__asm {push ebp
    call assistDefenseHas
    test eax,eax
    jmp dword ptr [g_assistUnyieldingResume]}}
__declspec(naked) void assistDiamondBridge(){__asm {push ebp
    call assistDefenseHas
    test eax,eax
    jmp dword ptr [g_assistDiamondResume]}}
int __cdecl assistPacket(uint8_t* packet,void* source,uint32_t position){
    auto scope=g_assist;uintptr_t caller=(uintptr_t)_ReturnAddress();
    if(!skilltelemetry::enabled() || !scope || source!=scope->source || !g_criticalCopyScope ||
       g_criticalCopyScope->destination!=packet || scope->generation!=skilltelemetry::contextGeneration() ||
       (caller!=0x587271 && caller!=g_strategyCodeBase+0x187271))return g_assistPacketOriginal(packet,source,position);
    CriticalCalculation calculation(packet,source,-1);
    int result=g_assistPacketOriginal(packet,source,position);
    if(scope->generation!=skilltelemetry::contextGeneration() || *(const int*)(packet+0x70)!=3 || *(const int*)(packet+0xc)<0)return result;
    bool selected=scope->decision==1 && scope->attribution.primaryUnit>=0 && assistActorValid(*scope);
    bool vine=calculation.vine.actor.person && calculation.vine.stored && calculation.target==scope->target;
    bool defense=calculation.defense.actor && calculation.target==scope->target && *(const int*)(packet+0xc)==0 &&
        *(const int*)(packet+0x64)==calculation.defense.effect;
    if(!selected && !vine && !defense)return result;
    typedef const uint32_t*(__thiscall* Position)(void*);
    if(position!=*(*(Position*)(*(uint8_t**)scope->target+0x3c))(scope->target) || *(const uint32_t*)(packet+0x5c)!=position)return result;
    CriticalRecord record;record.packet=packet;record.source=source;record.target=scope->target;record.assistAttack=true;record.attackMode=-1;
    if(selected)record.assist=scope->attribution;
    if(vine)record.vine=calculation.vine;
    if(defense)record.defense=calculation.defense;
    record.weapon=*(const int*)((const uint8_t*)source+0xa8);record.tactic=-1;memcpy(record.snapshot,packet,sizeof record.snapshot);bool failed=false;
    AcquireSRWLockExclusive(&g_criticalCacheLock);criticalSyncLocked();
    try {if(scope->generation==g_criticalCacheGeneration)g_criticalRecords.insert_or_assign((uintptr_t)packet,record);}catch(...){failed=true;}
    ReleaseSRWLockExclusive(&g_criticalCacheLock);if(failed)criticalAllocationFailed();return result;
}
bool assistGrantMatches(const CriticalRecord& record,void* source,const uint8_t* packet,void* target,int requested){
    const auto& a=record.assist;
    return a.actor && record.source==source && record.target==target && *(const int*)(packet+0x70)==3 &&
        requested==-*(const int*)(packet+0xc) && requested<=0 && growthPersonId((void*)a.actor)==a.actorId &&
        *(const int*)((const uint8_t*)a.actor+0xe8)==a.owned && contains(a.owned,26) && afterAttackMember(source,a.actor);
}
void assistGrantCommitted(const CriticalRecord& record,int requested,int before,int after,uintptr_t caller,skilltelemetry::Target target){
    const auto& a=record.assist;
    if(growthPersonId((void*)a.actor)!=a.actorId || *(const int*)((const uint8_t*)a.actor+0xe8)!=a.owned || !afterAttackMember(record.source,a.actor))return;
    auto position=*(const uint32_t*)(record.packet+0x5c);target.x=(int16_t)position;target.y=(int16_t)(position>>16);char context[288];
    sprintf_s(context,"{\"primary_unit\":%d,\"primary_force_at_decision\":%d,\"record_kind\":3,\"native_chance_percent\":30,\"requested_troop_loss\":%d,\"damage_attribution\":\"whole_assist_attack_not_marginal_skill_bonus\",\"counterfactual_evaluated\":false}",
        a.primaryUnit,a.primaryForce,-requested);target.context=context;
    skilltelemetry::applied(a.actor,a.owned,26,"battle","assist_attack_settled","troops",before,after,caller,target);
}

struct AssistLayout {uint32_t start;int size;const char* bytes;};
const AssistLayout g_assistLayouts[]={
    {0x585510,158,"\x56\x57\xe8\x29\x05\xf1\xff\x8b\x4c\x24\x0c\x8b\xf0\xe8\x1e\x05\xf1\xff\x56\xb9\x58\x19\x20\x07\x8b\xf8\xe8\xe1\xbd\xf0\xff\x57\xb9\x58\x19\x20\x07\xe8\xd6\xbd\xf0\xff\x8b\xf8\x57\x8b\xce\xe8\x4c\x32\xf0\xff\x85\xc0\x75\x5e\x57\x8b\xce\xe8\x80\x32\xf0\xff\x85\xc0\x75\x52\x6a\x1a\x8b\xce\xe8\x93\x3b\xf0\xff\x85\xc0\x74\x0c\x57\x8b\xce\xe8\x77\x34\xf0\xff\x85\xc0\x74\x0c\x57\x8b\xce\xe8\x9b\x33\xf0\xff\x85\xc0\x74\x08\x5f\xb8\x1e\x00\x00\x00\x5e\xc3\x57\x8b\xce\xe8\xe7\x65\xf0\xff\x85\xc0\x74\x14\x57\x8b\xce\xe8\x4b\x34\xf0\xff\x85\xc0\x75\x08\x5f\xb8\x14\x00\x00\x00\x5e\xc3\x5f\x33\xc0\x5e\xc3\x5f\xb8\x32\x00\x00\x00\x5e\xc3"},
    {0x587170,15,"\x55\x8b\xec\x83\xe4\xf8\x81\xec\xdc\x09\x00\x00\x53\x56\x57"},
    {0x58717f,197,"\x8b\x7d\x10\x57\xe8\xa8\x34\xef\xff\x83\xc4\x04\x85\xc0\x74\x10\x8b\x5d\x14\x53\xe8\x98\x34\xef\xff\x83\xc4\x04\x85\xc0\x75\x0c\xb8\x01\x00\x00\x00\x5f\x5e\x5b\x8b\xe5\x5d\xc3\x8b\x45\x08\x8b\x08\x80\xe1\x03\x80\xf9\x01\x0f\x85\xd9\x01\x00\x00\x0f\xb7\x40\x08\x50\xb9\x58\x19\x20\x07\xe8\xa5\x9c\xf0\xff\x8b\xf0\x3b\xf7\x0f\x84\xc0\x01\x00\x00\x8b\x46\x24\x85\xc0\x0f\x85\xb5\x01\x00\x00\x8b\x96\xa8\x00\x00\x00\x52\xe8\xe4\xf4\xfe\xff\x83\xc4\x04\x85\xc0\x0f\x85\x9e\x01\x00\x00\x57\x8b\xce\xe8\x21\xea\xf0\xff\x85\xc0\x0f\x84\x8e\x01\x00\x00\x53\x8b\xce\xe8\x41\xea\xf0\xff\x85\xc0\x0f\x84\x7e\x01\x00\x00\x57\x8b\xce\xe8\xf1\xe2\xff\xff\x50\xe8\xab\xaf\xee\xff\x83\xc4\x08\x85\xc0\x0f\x84\x65\x01\x00\x00\x53\x56\xb9\xf8\x65\x6a\x09\xe8\xb4\xde\x01\x00\x85\xc0\x0f\x84\x51\x01\x00\x00"},
    {0x587244,45,"\x33\xc0\xb9\x81\x01\x00\x00\x8d\x7c\x24\x10\xf3\xab\x8b\x03\x8b\xcb\xc7\x84\x24\x10\x06\x00\x00\x01\x00\x00\x00\xff\x50\x3c\x8b\x08\x51\x8d\x54\x24\x14\x56\x52\xe8\x0f\x94\x02\x00"},
    {0x5872e3,20,"\x8b\x5d\x10\x6a\x04\x8d\x44\x24\x14\x50\xb9\xd8\x59\x6a\x09\xe8\xa9\xed\xff\xff"},
    {0x587395,9,"\x5f\x5e\x33\xc0\x5b\x8b\xe5\x5d\xc3"},
};
bool verifyAssistLayout(){
    for(const auto& s:g_assistLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected assist branch at %08x; rejected",s.start);return false;
    }
    return true;
}
