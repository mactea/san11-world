struct DefenseScope;
thread_local DefenseScope* g_defenseScope=nullptr;
struct DefenseScope {
    void* target;
    DefenseAttribution attribution;
    DefenseScope* previous;
    explicit DefenseScope(void* t):target(t),previous(g_defenseScope){g_defenseScope=this;}
    ~DefenseScope(){g_defenseScope=previous;}
};
uint32_t g_defenseNativeDecision=0x5ae000,g_defenseDecisionResume=0x5b03bf;
uint32_t g_defenseUnyieldingResume=0x5ae0d8,g_defenseDiamondResume=0x5ae114;
__declspec(naked) int __cdecl defenseInvoke(void*,void*,uint8_t*,int){__asm {push esi
    mov ecx,[esp+8]
    mov esi,[esp+12]
    mov eax,[esp+20]
    push [esp+16]
    call dword ptr [g_defenseNativeDecision]
    add esp,4
    pop esi
    ret}}
int __cdecl defenseDecision(void* source,void* target,uint8_t* packet,int tactic){
    auto calculation=g_criticalCalculation;
    if(!skilltelemetry::enabled() || !calculation || calculation->packet!=packet || calculation->source!=source)
        return defenseInvoke(source,target,packet,tactic);
    DefenseScope scope(target);int before=*(int*)(packet+0xc);
    int result=defenseInvoke(source,target,packet,tactic);
    auto& attribution=scope.attribution;
    if(result && attribution.actor && attribution.actorId>=0 && before>=0 &&
       (!calculation->target || calculation->target==target) &&
       *(int*)(packet+0x64)==attribution.effect && calculation->generation==skilltelemetry::contextGeneration()){
        attribution.incomingDamage=before;calculation->defense=attribution;calculation->target=target;
    }
    return result;
}
__declspec(naked) void defenseDecisionBridge(){__asm {push eax
    push [esp+4]
    push esi
    push ecx
    call defenseDecision
    add esp,16
    jmp dword ptr [g_defenseDecisionResume]}}
int __fastcall defenseHas(void* unit,void*,int effect){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);
    auto scope=g_defenseScope;
    if(!scope || scope->target!=unit || (effect!=27 && effect!=28))return native(unit,effect);
    ProgressionProbe probe(effect);probe.scene="battle";probe.event="damage_immunity_eligibility";
    int result=native(unit,effect);
    // A later eligible skill replaces a failed earlier roll, never adds to it.
    scope->attribution=DefenseAttribution{};
    if(result && probe.actor){
        auto& a=scope->attribution;a.actor=probe.actor;a.actorId=growthPersonId((void*)probe.actor);
        a.owned=*(const int*)((const uint8_t*)probe.actor+0xe8);a.effect=effect;
    }
    return result;
}
__declspec(naked) void defenseUnyieldingBridge(){__asm {call defenseHas
    test eax,eax
    jmp dword ptr [g_defenseUnyieldingResume]}}
__declspec(naked) void defenseDiamondBridge(){__asm {call defenseHas
    test eax,eax
    jmp dword ptr [g_defenseDiamondResume]}}
bool defenseGrantMatches(const CriticalRecord& record,void* source,const uint8_t* packet,void* target,int requested){
    const auto& a=record.defense;
    return a.actor && a.actorId>=0 && record.source==source && record.target==target && requested==0 &&
        *(const int*)(packet+0xc)==0 && *(const int*)(packet+0x70)==criticalKind(record) &&
        *(const int*)(packet+0x64)==a.effect && !memcmp(record.snapshot,packet,sizeof record.snapshot) &&
        (a.effect==27 || a.effect==28 || ((a.effect==13 || a.effect==14) && record.attackMode!=0 && !record.ambush && !record.doubleAttack.actor)) && contains(a.owned,a.effect) &&
        growthPersonId((void*)a.actor)==a.actorId && *(const int*)((const uint8_t*)a.actor+0xe8)==a.owned &&
        afterAttackMember(target,a.actor);
}
void defenseGrantCommitted(const CriticalRecord& record,void* unit,int before,int after,uintptr_t caller,skilltelemetry::Target target){
    const auto& a=record.defense;
    if(growthPersonId((void*)a.actor)!=a.actorId || *(const int*)((const uint8_t*)a.actor+0xe8)!=a.owned ||
       !afterAttackMember(unit,a.actor))return;
    int attacker=target.actorUnit;target.actorUnit=target.unit;target.equipment=*(const int*)((const uint8_t*)unit+0xa8);
    uint32_t position=*(const uint32_t*)(record.packet+0x5c);
    target.x=(int16_t)position;target.y=(int16_t)(position>>16);char context[320];
    bool counter=a.effect==13 || a.effect==14;
    if(counter)sprintf_s(context,"{\"responding_unit\":%d,\"native_attack_mode\":%d,\"native_water_branch\":%s,\"calculated_counter_troop_damage\":%d,\"calculation_stage\":\"before_parent_scaling\",\"settled_packet_troop_damage\":0,\"native_chance_percent\":50,\"counterfactual_evaluated\":false}",
        attacker,record.attackMode,a.effect==14?"true":"false",a.incomingDamage);
    else if(record.assistAttack)sprintf_s(context,"{\"attacking_unit\":%d,\"tactic_id\":%d,\"record_kind\":3,\"native_attack_mode\":-1,\"calculated_troop_damage_before_immunity\":%d,\"settled_packet_troop_damage\":0,\"native_chance_percent\":50,\"counterfactual_evaluated\":false}",
        attacker,record.tactic,a.incomingDamage);
    else sprintf_s(context,"{\"attacking_unit\":%d,\"tactic_id\":%d,\"calculated_troop_damage_before_immunity\":%d,\"settled_packet_troop_damage\":0,\"native_chance_percent\":50,\"counterfactual_evaluated\":false}",
        attacker,record.tactic,a.incomingDamage);target.context=context;
    // The native packet was zeroed. Observe the actual resource call without
    // claiming a replayed no-skill hit or a counterfactual troop balance.
    skilltelemetry::applied(a.actor,a.owned,a.effect,"battle",counter?"counterattack_immunity_attack_settled":record.doubleAttack.actor?"damage_immunity_double_attack_settled":record.ambush?"damage_immunity_ambush_settled":record.assistAttack?"damage_immunity_assist_attack_settled":"damage_immunity_attack_settled",
        "troops",before,after,caller,target);
}

// Native5b0e90 owns the conversion of a newly calculated normal record into an
// ambush. Preserve provenance only across its two exact field writes.
typedef int(__cdecl* AmbushProducer)(uint8_t*,void*,void*,int);
uint32_t g_ambushProducerResume=0x5b0e96;
__declspec(naked) void ambushProducerNative(){__asm {mov ecx,[esp+0xc]
    mov eax,[ecx]
    jmp dword ptr [g_ambushProducerResume]}}
AmbushProducer g_ambushProducerOriginal=(AmbushProducer)ambushProducerNative;
struct AmbushConstructionScope {
    uintptr_t previous;
    explicit AmbushConstructionScope(uintptr_t caller):previous(g_ambushConstructionCaller){g_ambushConstructionCaller=caller;}
    ~AmbushConstructionScope(){g_ambushConstructionCaller=previous;}
};
int __cdecl ambushProducer(uint8_t* packet,void* source,void* target,int flag){
    if(!skilltelemetry::enabled())return g_ambushProducerOriginal(packet,source,target,flag);
    uint64_t generation=skilltelemetry::contextGeneration();criticalTake(packet);
    AmbushConstructionScope owner((uintptr_t)_ReturnAddress());
    if(g_ambushConstructionCaller-g_strategyCodeBase+0x400000==0x59289d)criticalOwnerZero(packet);
    int result=g_ambushProducerOriginal(packet,source,target,flag);
    AcquireSRWLockExclusive(&g_criticalCacheLock);criticalSyncLocked();
    auto found=g_criticalRecords.find((uintptr_t)packet);
    if(generation==g_criticalCacheGeneration && found!=g_criticalRecords.end()){
        auto& record=found->second;uint8_t expected[0x80];memcpy(expected,record.snapshot,sizeof expected);
        bool valid=record.source==source && record.target==target && !record.ambush && *(int*)(expected+0x70)==0;
        *(int*)(expected+0x14)=flag?20:10;*(int*)(expected+0x70)=1;
        if(valid && !memcmp(expected,packet,sizeof expected)){
            memcpy(record.snapshot,expected,sizeof expected);record.ambush=true;
        }else g_criticalRecords.erase(found);
    }
    ReleaseSRWLockExclusive(&g_criticalCacheLock);return result;
}

struct DefenseLayout {uint32_t start;int size;const char* bytes;};
const DefenseLayout g_defenseLayouts[]={
    {0x5b0b3a,100,"\x8b\x16\x8b\xce\xff\x52\x50\x3d\xb8\x0b\x00\x00\x7d\x24\x8b\x06\x6a\x1b\x8b\xce\xff\x50\x58\x85\xc0\x74\x17\x6a\x32\xe8\x74\x16\xec\xff\x83\xc4\x04\x85\xc0\x74\x09\xc7\x45\x64\x1b\x00\x00\x00\xeb\x2b\x81\x7d\x0c\xf4\x01\x00\x00\x7d\x29\x8b\x16\x6a\x1c\x8b\xce\xff\x52\x58\x85\xc0\x74\x1c\x6a\x32\xe8\x47\x16\xec\xff\x83\xc4\x04\x85\xc0\x74\x0e\xc7\x45\x64\x1c\x00\x00\x00\xc7\x45\x0c\x00\x00\x00\x00"},
    {0x5b0e90,66,"\x8b\x4c\x24\x0c\x8b\x01\x56\x57\xff\x50\x3c\x8b\x74\x24\x18\x8b\x08\x8b\x54\x24\x10\x8b\x7c\x24\x0c\x6a\x01\x56\x6a\x00\x51\x6a\xff\x52\x57\xe8\xb8\xed\xff\xff\x83\xc4\x1c\xf7\xde\x1b\xf6\x83\xe6\x0a\x83\xc6\x0a\x89\x77\x14\xc7\x47\x70\x01\x00\x00\x00\x5f\x5e\xc3"},
    {0x5ae000,314,"\x53\x55\x57\x8b\xf8\x8b\x01\xff\x50\x3c\x8b\x16\x8b\x18\x8b\xce\xff\x52\x3c\x85\xff\x8b\x28\x7c\x09\x83\xff\x1f\x0f\x8e\x12\x01\x00\x00\x83\xbe\xa8\x00\x00\x00\x02\x0f\x85\x87\x00\x00\x00\x8b\x06\x6a\x05\x8b\xce\xff\x50\x60\x85\xc0\x74\x32\x55\x53\x8b\xc7\xe8\x7b\xff\xff\xff\x83\xc4\x08\x85\xc0\x74\x22\x6a\x1e\xe8\x7d\x41\xec\xff\x83\xc4\x04\x85\xc0\x74\x14\x8b\x4c\x24\x10\x5f\x5d\xc7\x41\x6c\x05\x00\x00\x00\xb8\x01\x00\x00\x00\x5b\xc3\x8b\x16\x6a\x06\x8b\xce\xff\x52\x60\x85\xc0\x74\x3b\x85\xff\x7c\x05\x83\xff\x1f\x7e\x32\x55\x53\xb9\x68\x0e\xfb\x06\xe8\x20\x5d\xed\xff\x85\xc0\x74\x22\x6a\x1e\xe8\x35\x41\xec\xff\x83\xc4\x04\x85\xc0\x74\x14\x8b\x44\x24\x10\x5f\x5d\xc7\x40\x6c\x06\x00\x00\x00\xb8\x01\x00\x00\x00\x5b\xc3\x85\xff\x7c\x05\x83\xff\x1f\x7e\x75\x8b\x16\x8b\xce\xff\x52\x50\x3d\xb8\x0b\x00\x00\x7d\x2f\x8b\x06\x6a\x1b\x8b\xce\xff\x50\x58\x85\xc0\x74\x22\x6a\x32\xe8\xef\x40\xec\xff\x83\xc4\x04\x85\xc0\x74\x14\x8b\x4c\x24\x10\x5f\x5d\xc7\x41\x64\x1b\x00\x00\x00\xb8\x01\x00\x00\x00\x5b\xc3\x8b\x7c\x24\x10\x81\x7f\x0c\xf4\x01\x00\x00\x7d\x2b\x8b\x16\x6a\x1c\x8b\xce\xff\x52\x58\x85\xc0\x74\x1e\x6a\x32\xe8\xb3\x40\xec\xff\x83\xc4\x04\x85\xc0\x74\x10\xc7\x47\x64\x1c\x00\x00\x00\x5f\x5d\xb8\x01\x00\x00\x00\x5b\xc3\x5f\x5d\x33\xc0\x5b\xc3"},
    {0x5b03b1,28,"\x8b\x44\x24\x40\x55\x8b\xf7\x8b\xcb\xe8\x41\xdc\xff\xff\x83\xc4\x04\x85\xc0\x74\x07\xc7\x45\x0c\x00\x00\x00\x00"},
};
bool verifyDefenseLayout(){
    for(const auto& s:g_defenseLayouts){
        const auto code=(const uint8_t*)(g_strategyCodeBase+s.start-0x400000);uint8_t expected[314];
        memcpy(expected,s.bytes,s.size);
        if(s.start==0x5ae000){
            // M7 relocates this existing map pointer. Require the same B20 as
            // the guarded native map setter, and preserve every opcode byte.
            auto map=*(const uint32_t*)(g_strategyCodeBase+0x5a07aa-0x400000);
            if(!map || *(const uint32_t*)(code+0x87)!=map)return false;
            memcpy(expected+0x87,&map,sizeof map);
        }
        if(memcmp(code,expected,s.size)){
            if(g_log)g_log("skills: unexpected defense branch at %08x; rejected",s.start);return false;
        }
    }
    return true;
}
