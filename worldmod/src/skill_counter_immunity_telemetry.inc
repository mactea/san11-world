struct CounterReactionScope;
thread_local CounterReactionScope* g_counterReactionScope=nullptr;
struct CounterReactionScope {
    void* attacker;
    void* defender;
    const int* extra;
    uint8_t* scaledPacket=nullptr;
    int damageBeforeScale=0,damageAfterScale=0;
    uint64_t generation;
    CounterReactionScope* previous;
    CounterReactionScope(void* a,void* d,const int* e):attacker(a),defender(d),extra(e),
        generation(skilltelemetry::contextGeneration()),previous(g_counterReactionScope){g_counterReactionScope=this;}
    ~CounterReactionScope(){g_counterReactionScope=previous;}
};
typedef int(__thiscall* CounterReaction)(void*,void*,void*,int,int,const int*);
typedef int(__thiscall* CounterGroupReaction)(void*,void*,void*,int);
uint32_t g_counterReactionResume=0x584c16,g_counterGroupReactionResume=0x584d76,g_counterBuilderAddress=0x584b10;
__declspec(naked) void counterReactionNative(){__asm {sub esp,0x9d4
    jmp dword ptr [g_counterReactionResume]}}
__declspec(naked) void counterGroupReactionNative(){__asm {sub esp,0x9d4
    jmp dword ptr [g_counterGroupReactionResume]}}
CounterReaction g_counterReactionOriginal=(CounterReaction)counterReactionNative;
CounterGroupReaction g_counterGroupReactionOriginal=(CounterGroupReaction)counterGroupReactionNative;
int __fastcall counterReaction(void* self,void*,void* attacker,void* defender,int count,int flag,const int* extra){
    if(!skilltelemetry::enabled())return g_counterReactionOriginal(self,attacker,defender,count,flag,extra);
    CriticalCopyScope owner;CounterReactionScope scope(attacker,defender,extra);
    return g_counterReactionOriginal(self,attacker,defender,count,flag,extra);
}
int __fastcall counterGroupReaction(void* self,void*,void* attacker,void* defender,int flag){
    if(!skilltelemetry::enabled())return g_counterGroupReactionOriginal(self,attacker,defender,flag);
    CriticalCopyScope owner;CounterReactionScope scope(attacker,defender,nullptr);
    return g_counterGroupReactionOriginal(self,attacker,defender,flag);
}
__declspec(naked) void counterBuilderBridge(){__asm {pushfd
    pushad
    push esi
    call criticalOwnerZero
    add esp,4
    popad
    popfd
    jmp dword ptr [g_counterBuilderAddress]}}
int __fastcall counterImmunityHas(void* unit,void*,int effect,uint8_t* packet){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);
    auto scope=g_counterReactionScope;auto calculation=g_criticalCalculation;
    if(!scope || !calculation || !calculation->attackMode || calculation->packet!=packet || calculation->source!=scope->defender ||
       unit!=scope->attacker || !g_criticalCopyScope || g_criticalCopyScope->destination!=packet ||
       scope->generation!=calculation->generation || scope->generation!=g_criticalCopyScope->generation ||
       (effect!=13 && effect!=14))return native(unit,effect);
    ProgressionProbe probe(effect);probe.scene="battle";probe.event="counterattack_immunity_eligibility";
    int before=*(int*)(packet+0xc);int result=native(unit,effect);
    if(result && probe.actor && before>=0 && scope->generation==skilltelemetry::contextGeneration() &&
       (!calculation->target || calculation->target==unit)){
        auto id=growthPersonId((void*)probe.actor);
        if(id>=0){auto& a=calculation->defense;a.actor=probe.actor;a.actorId=id;a.owned=*(const int*)((const uint8_t*)probe.actor+0xe8);
            a.effect=effect;a.incomingDamage=before;calculation->target=unit;}
    }
    return result;
}
uint32_t g_counterWaterResume=0x5b0336,g_counterLandResume=0x5b0366;
__declspec(naked) void counterWaterBridge(){__asm {push ebp
    push [esp+4]
    call counterImmunityHas
    add esp,4
    test eax,eax
    jmp dword ptr [g_counterWaterResume]}}
__declspec(naked) void counterLandBridge(){__asm {push ebp
    push [esp+4]
    call counterImmunityHas
    add esp,4
    test eax,eax
    jmp dword ptr [g_counterLandResume]}}
IronwallApply g_counterReactionApplyOriginal=(IronwallApply)0x5b1870;
void __cdecl counterScaleStored(uint8_t* packet,int damage){
    auto scope=g_counterReactionScope;
    if(!scope || !g_criticalCopyScope || g_criticalCopyScope->destination!=packet ||
       scope->generation!=g_criticalCopyScope->generation || scope->generation!=skilltelemetry::contextGeneration())return;
    scope->scaledPacket=packet;scope->damageBeforeScale=*(const int*)(packet+0xc);scope->damageAfterScale=damage;
}
uint32_t g_counterFloatEntry=0x707a74;
__declspec(naked) void counterScaleBridge(){__asm {call dword ptr [g_counterFloatEntry]
    pushfd
    pushad
    lea ecx,[esp+0x38]
    push eax
    push ecx
    call counterScaleStored
    add esp,8
    popad
    popfd
    ret}}
int __cdecl counterReactionApply(uint8_t* packet){
    auto scope=g_counterReactionScope;
    if(skilltelemetry::enabled() && scope && g_criticalCopyScope && g_criticalCopyScope->destination==packet &&
       scope->generation==g_criticalCopyScope->generation){
        AcquireSRWLockExclusive(&g_criticalCacheLock);criticalSyncLocked();auto found=g_criticalRecords.find((uintptr_t)packet);
        if(scope->generation==g_criticalCacheGeneration && found!=g_criticalRecords.end()){
            auto& r=found->second;uint8_t expected[0x80];memcpy(expected,r.snapshot,sizeof expected);
            // Native584d30 may overwrite just+78 from the caller's optional
            // word after counter scaling. Zeroed damage remains zero.
            if(scope->extra)*(int*)(expected+0x78)=*scope->extra;
            if(scope->scaledPacket==packet && scope->damageBeforeScale==*(const int*)(expected+0xc))
                *(int*)(expected+0xc)=scope->damageAfterScale;
            if(r.source==scope->defender && r.target==scope->attacker && r.attackMode!=0 &&
               (r.defense.effect==13 || r.defense.effect==14 || r.vine.actor.person) && !memcmp(expected,packet,sizeof expected))
                memcpy(r.snapshot,expected,sizeof expected);
            else g_criticalRecords.erase(found);
        }
        ReleaseSRWLockExclusive(&g_criticalCacheLock);
    }
    return g_counterReactionApplyOriginal(packet);
}

struct CounterImmunityLayout {uint32_t start;int size;const char* bytes;};
const CounterImmunityLayout g_counterImmunityLayouts[]={
    {0x584c8d,5,"\x83\xfd\x01\x7e\x26"},
    {0x584cb4,6,"\x89\x44\x24\x1c\x8b\x17"},
    {0x584d99,73,"\x56\x8b\xb4\x24\xe8\x09\x00\x00\x56\xe8\x89\x58\xef\xff\x83\xc4\x04\x85\xc0\x75\x0e\x5e\x5f\x33\xc0\x5d\x81\xc4\xd4\x09\x00\x00\xc2\x0c\x00\x83\xbe\xa8\x00\x00\x00\x03\x75\xe9\x8b\x06\x6a\x09\x8b\xce\xff\x50\x60\x85\xc0\x74\xdc\x57\x56\xb9\xf8\x65\x6a\x09\xe8\x12\x03\x02\x00\x85\xc0\x74\xcc"},
    {0x5b031d,109,"\x8b\xcf\x89\x45\x0c\xe8\xe9\x60\xee\xff\x85\xc0\x8b\xcf\x74\x30\x8b\x17\x6a\x0e\xff\x52\x58\x85\xc0\x0f\x84\xc8\x00\x00\x00\x6a\x32\xe8\x8d\x1e\xec\xff\x83\xc4\x04\x85\xc0\x0f\x84\xb6\x00\x00\x00\x89\x75\x0c\xc7\x45\x64\x0e\x00\x00\x00\xe9\xa7\x00\x00\x00\x8b\x07\x6a\x0d\xff\x50\x58\x85\xc0\x0f\x84\x98\x00\x00\x00\x6a\x32\xe8\x5d\x1e\xec\xff\x83\xc4\x04\x85\xc0\x0f\x84\x86\x00\x00\x00\x89\x75\x0c\xc7\x45\x64\x0d\x00\x00\x00\xeb\x7a"},
    {0x584b10,59,"\x8b\x01\xff\x50\x3c\x8b\x54\x24\x08\x33\xc9\x85\xd2\x8b\x10\x8b\x44\x24\x04\x0f\x95\xc1\x6a\x00\x6a\x00\x41\x51\x52\x6a\xff\x50\x56\xe8\x3a\xb1\x02\x00\x83\xc4\x1c\xc7\x86\x00\x06\x00\x00\x01\x00\x00\x00\xc7\x46\x18\x00\x00\x00\x00\xc3"},
    {0x584c10,125,"\x81\xec\xd4\x09\x00\x00\x53\x8b\x9c\x24\xdc\x09\x00\x00\x53\xe8\x0c\x5a\xef\xff\x83\xc4\x04\x85\xc0\x75\x0a\x5b\x81\xc4\xd4\x09\x00\x00\xc2\x14\x00\x57\x8b\xbc\x24\xe4\x09\x00\x00\x57\xe8\xed\x59\xef\xff\x83\xc4\x04\x85\xc0\x0f\x84\x08\x01\x00\x00\x8b\x47\x24\x85\xc0\x0f\x85\xfd\x00\x00\x00\x8b\x87\xa8\x00\x00\x00\x83\xf8\x05\x7c\x09\x83\xf8\x08\x0f\x8e\xe9\x00\x00\x00\x8b\x84\x24\xec\x09\x00\x00\x55\x56\x50\x57\x8b\xcb\x8d\x74\x24\x18\xe8\x8d\xfe\xff\xff\x8b\xac\x24\xf8\x09\x00\x00\x83\xc4\x08"},
    {0x584c83,53,"\x8b\xac\x24\xf8\x09\x00\x00\x83\xc4\x08\x83\xfd\x01\x7e\x26\xdb\x84\x24\xf0\x09\x00\x00\xd8\x0d\xc4\x9b\x77\x00\xd8\x2d\xb8\x15\x7f\x00\xda\x4c\x24\x1c\xd8\x0d\x88\xaa\x77\x00\xe8\xc0\x2d\x18\x00\x89\x44\x24\x1c"},
    {0x584d23,64,"\x8b\x84\x24\xf8\x09\x00\x00\x85\xc0\x5e\x5d\x74\x09\x8b\x00\x89\x84\x24\x80\x00\x00\x00\x8d\x4c\x24\x08\x51\xe8\x2d\xcb\x02\x00\x83\xc4\x04\x5f\xb8\x01\x00\x00\x00\x5b\x81\xc4\xd4\x09\x00\x00\xc2\x14\x00\x5f\x33\xc0\x5b\x81\xc4\xd4\x09\x00\x00\xc2\x14\x00"},
    {0x584d70,32,"\x81\xec\xd4\x09\x00\x00\x55\x57\x8b\xbc\x24\xe0\x09\x00\x00\x57\x8b\xe9\xe8\xa9\x58\xef\xff\x83\xc4\x04\x85\xc0\x75\x0b\x5f\x5d"},
    {0x584e40,48,"\x8b\x84\x24\xec\x09\x00\x00\x50\x56\x8b\xcf\x8d\xb4\x24\xe4\x03\x00\x00\xe8\xb9\xfc\xff\xff\x83\xc4\x08\x6a\x01\x8b\xce\x51\x8b\xcd\xe8\x3a\x12\x00\x00\x5e\x5f\x5d\x81\xc4\xd4\x09\x00\x00\xc2"},
};
bool verifyCounterImmunityLayout(){
    for(const auto& s:g_counterImmunityLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected counter immunity branch at %08x; rejected",s.start);return false;
    }
    return true;
}
