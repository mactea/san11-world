struct DoubleAttackScope;
thread_local DoubleAttackScope* g_doubleAttack=nullptr;
struct DoubleAttackScope {
    void* source=nullptr;
    void* target=nullptr;
    int holder=-1,mode;
    DoubleAttribution attribution;
    uint8_t* secondary=nullptr;
    bool published=false;
    uint64_t generation;
    CriticalCopyScope* owner;
    DoubleAttackScope* previous;
    explicit DoubleAttackScope(int m):mode(m),generation(skilltelemetry::contextGeneration()),owner(g_criticalCopyScope),previous(g_doubleAttack){g_doubleAttack=this;}
    ~DoubleAttackScope(){g_doubleAttack=previous;}
};
int pincerHandler(void*,const uint8_t*,int);
int doubleAttackHandler(void* self,const uint8_t* records,int mode){
    DoubleAttackScope scope(mode);return pincerHandler(self,records,mode);
}
int __fastcall doubleFind(void* unit,void*,int effect){
    typedef int(__thiscall* Find)(void*,int);auto native=*(Find*)(*(uint8_t**)unit+0x5c);
    auto scope=g_doubleAttack;
    if(!scope || effect!=12)return native(unit,effect);
    ProgressionProbe probe(12);probe.scene="battle";probe.event="double_attack_eligibility";QueryOrigin origin(0x586eeb);
    int result=native(unit,effect);scope->source=unit;scope->holder=result;return result;
}
uint32_t g_doubleFindResume=0x586ef4,g_doubleZeroResume=0x586f27,g_doubleTagResume=0x586f6f;
__declspec(naked) void doubleFindBridge(){__asm {call doubleFind
    push eax
    mov ecx,0x7201958
    jmp dword ptr [g_doubleFindResume]}}
bool doubleActorValid(const DoubleAttackScope& scope){
    const auto& a=scope.attribution;
    return a.actor && scope.owner && scope.owner==g_criticalCopyScope && scope.owner->generation==scope.generation &&
        scope.generation==skilltelemetry::contextGeneration() && growthPersonId((void*)a.actor)==a.actorId &&
        *(const int*)((const uint8_t*)a.actor+0xe8)==a.owned && contains(a.owned,12) && afterAttackMember(scope.source,a.actor);
}
typedef int(__cdecl* DoubleRoll)(int);
DoubleRoll g_doubleRollOriginal=(DoubleRoll)0x4721d0;
int __cdecl doubleRoll(void* source,void* target,void* actor,int mode,int chance){
    int result=g_doubleRollOriginal(chance);auto scope=g_doubleAttack;
    if(!scope || scope->source!=source || scope->mode!=mode || mode==2 || mode==4 || chance!=50 || result!=1)return result;
    int id=growthPersonId(actor);if(id<0 || id!=scope->holder)return result;
    scope->attribution.actor=actor;scope->attribution.actorId=id;
    scope->attribution.owned=*(const int*)((const uint8_t*)actor+0xe8);scope->attribution.parentMode=mode;
    scope->target=target;
    if(!doubleActorValid(*scope))scope->attribution=DoubleAttribution{};
    return result;
}
// The sampler's original argument remains for the native ADD ESP,4. Provider
// is the native resolved pointer in the caller's local slot, not a new lookup.
__declspec(naked) void doubleRollBridge(){__asm {mov eax,[esp+0x24]
    push [esp+4]
    push [ebp+0xc]
    push eax
    push ebx
    push esi
    call doubleRoll
    add esp,20
    ret}}
void __cdecl doubleOwnerZero(uint8_t* group){
    auto scope=g_doubleAttack;if(!scope || !doubleActorValid(*scope))return;
    if(scope->owner->secondarySource && scope->owner->secondarySource!=group)return;
    scope->secondary=group;scope->owner->secondarySource=group;
    AcquireSRWLockExclusive(&g_criticalCacheLock);criticalSyncLocked();criticalEraseRangeLocked(group,0x600);ReleaseSRWLockExclusive(&g_criticalCacheLock);
}
__declspec(naked) void doubleZeroBridge(){__asm {pushfd
    pushad
    lea eax,[esp+0xc4c]
    push eax
    call doubleOwnerZero
    add esp,4
    popad
    popfd
    mov ecx,0x181
    jmp dword ptr [g_doubleZeroResume]}}
void __cdecl doubleRecord(uint8_t* packet){
    auto scope=g_doubleAttack;
    if(!scope || scope->secondary!=packet || !doubleActorValid(*scope) || *(const int*)(packet+0x600)!=1 ||
       *(const int*)(packet+0x70)!=4 || *(const int*)(packet+0xc)<0)return;
    auto source=unitTarget(scope->source),target=unitTarget(scope->target);
    typedef const uint32_t*(__thiscall* Position)(void*);
    if(source.unit<0 || target.unit<0 || g_movementUnitById((void*)0x7201958,source.unit)!=scope->source ||
       g_movementUnitById((void*)0x7201958,target.unit)!=scope->target ||
       *(*(Position*)(*(uint8_t**)scope->source+0x3c))(scope->source)!=*(const uint32_t*)(packet+0x58) ||
       *(*(Position*)(*(uint8_t**)scope->target+0x3c))(scope->target)!=*(const uint32_t*)(packet+0x5c))return;
    bool failed=false;AcquireSRWLockExclusive(&g_criticalCacheLock);criticalSyncLocked();
    try {
        auto it=g_criticalRecords.find((uintptr_t)packet);CriticalRecord record;bool valid=true;
        if(it!=g_criticalRecords.end()){
            record=it->second;*(int*)(record.snapshot+0x70)=4;
            valid=record.source==scope->source && record.target==scope->target && !memcmp(record.snapshot,packet,sizeof record.snapshot);
            if(!valid)g_criticalRecords.erase(it);
        }
        if(valid && scope->generation==g_criticalCacheGeneration){
            record.packet=packet;record.source=scope->source;record.target=scope->target;record.doubleAttack=scope->attribution;
            record.weapon=*(const int*)((const uint8_t*)scope->source+0xa8);record.tactic=-1;
            memcpy(record.snapshot,packet,sizeof record.snapshot);g_criticalRecords.insert_or_assign((uintptr_t)packet,record);scope->published=true;
        }
    }catch(...){failed=true;}
    ReleaseSRWLockExclusive(&g_criticalCacheLock);if(failed)criticalAllocationFailed();
}
__declspec(naked) void doubleTagBridge(){__asm {mov dword ptr [esp+0xc98],4
    pushfd
    pushad
    lea eax,[esp+0xc4c]
    push eax
    call doubleRecord
    add esp,4
    popad
    popfd
    jmp dword ptr [g_doubleTagResume]}}
int __fastcall doubleChild(void* self,void*,const uint8_t* records,int mode){
    auto scope=g_doubleAttack;bool observe=scope && scope->published && records==scope->secondary && mode==2 && doubleActorValid(*scope);
    skilltelemetry::Target target;
    if(observe){
        target=unitTarget(scope->target);target.actorUnit=g_telemetryUnitId((void*)0x7201958,scope->source);
        target.equipment=*(const int*)((const uint8_t*)scope->source+0xa8);
        auto position=*(const uint32_t*)(records+0x5c);target.x=(int16_t)position;target.y=(int16_t)(position>>16);
        observe=target.unit>=0 && target.actorUnit>=0 && g_movementUnitById((void*)0x7201958,target.unit)==scope->target &&
            g_movementUnitById((void*)0x7201958,target.actorUnit)==scope->source;
    }
    // Use the already-installed handler so local copy, aggregation and owner
    // cleanup run identically for this native nested attack.
    int result=criticalHandlerTwo(self,nullptr,records,mode);
    if(observe && doubleActorValid(*scope)){
        char context[192];sprintf_s(context,"{\"parent_mode\":%d,\"child_mode\":2,\"native_chance_percent\":50,\"native_result\":%d,\"counterfactual_evaluated\":false}",scope->mode,result);
        target.context=context;skilltelemetry::appliedEvent(scope->attribution.actor,scope->attribution.owned,12,"battle","double_attack_resolved",0x586ffc,target);
    }
    return result;
}
bool doubleGrantMatches(const CriticalRecord& record,void* source,const uint8_t* packet,void* target,int requested){
    const auto& a=record.doubleAttack;
    return a.actor && record.source==source && record.target==target && *(const int*)(packet+0x70)==4 &&
        requested==-*(const int*)(packet+0xc) && requested<=0 && growthPersonId((void*)a.actor)==a.actorId &&
        *(const int*)((const uint8_t*)a.actor+0xe8)==a.owned && contains(a.owned,12) && afterAttackMember(source,a.actor);
}
void doubleGrantCommitted(const CriticalRecord& record,int requested,int before,int after,uintptr_t caller,skilltelemetry::Target target){
    const auto& a=record.doubleAttack;
    if(growthPersonId((void*)a.actor)!=a.actorId || *(const int*)((const uint8_t*)a.actor+0xe8)!=a.owned || !afterAttackMember(record.source,a.actor))return;
    auto position=*(const uint32_t*)(record.packet+0x5c);target.x=(int16_t)position;target.y=(int16_t)(position>>16);char context[256];
    sprintf_s(context,"{\"parent_mode\":%d,\"record_kind\":4,\"native_chance_percent\":50,\"requested_troop_loss\":%d,\"damage_attribution\":\"whole_extra_attack_not_marginal_skill_bonus\",\"counterfactual_evaluated\":false}",a.parentMode,-requested);
    target.context=context;skilltelemetry::applied(a.actor,a.owned,12,"battle","double_attack_settled","troops",before,after,caller,target);
}

struct DoubleAttackLayout {uint32_t start;int size;const char* bytes;};
const DoubleAttackLayout g_doubleAttackLayouts[]={
    {0x586bb8,8,"\x85\xc0\x0f\x85\xaf\x04\x00\x00"},
    {0x586ebf,97,"\x56\xe8\x6b\x37\xef\xff\x8b\x7d\x0c\x83\xc4\x04\x85\xc0\x0f\x84\x5d\x01\x00\x00\x83\xff\x04\x0f\x84\x54\x01\x00\x00\x83\xff\x02\x0f\x84\x4b\x01\x00\x00\x8b\x16\x6a\x0c\x8b\xce\xff\x52\x5c\x50\xb9\x58\x19\x20\x07\xe8\x07\x9c\xf0\xff\x50\x89\x44\x24\x20\xe8\x2d\x37\xef\xff\x83\xc4\x04\x85\xc0\x0f\x84\x22\x01\x00\x00\x6a\x32\xe8\xbb\xb2\xee\xff\x83\xc4\x04\x85\xc0\x0f\x84\x10\x01\x00\x00"},
    {0x586f20,79,"\x33\xc0\xb9\x81\x01\x00\x00\x8d\xbc\x24\x28\x0c\x00\x00\xf3\xab\x8b\x03\x8b\xcb\xff\x50\x3c\x8b\x08\x6a\x00\x6a\x00\x6a\x00\x51\x6a\xff\x8d\x94\x24\x3c\x0c\x00\x00\x56\x52\xe8\x20\x8d\x02\x00\x8b\x44\x24\x40\x83\xc4\x1c\x85\xc0\xc7\x84\x24\x28\x12\x00\x00\x01\x00\x00\x00\xc7\x84\x24\x98\x0c\x00\x00\x04\x00\x00\x00"},
    {0x586fc9,56,"\x56\xe8\x61\x36\xef\xff\x83\xc4\x04\x85\xc0\x74\x18\x8b\x45\x0c\x85\xc0\x75\x11\x8d\x8c\x24\xd8\x04\x00\x00\x51\x56\xe8\x95\xd6\xff\xff\x83\xc4\x08\x8b\x4c\x24\x10\x6a\x02\x8d\x94\x24\x2c\x0c\x00\x00\x52\xe8\x9f\xf0\xff\xff"},
};
bool verifyDoubleAttackLayout(){
    for(const auto& s:g_doubleAttackLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected double attack branch at %08x; rejected",s.start);return false;
    }
    return true;
}
