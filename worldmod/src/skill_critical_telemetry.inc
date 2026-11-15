struct CriticalMaximumProbe;
thread_local CriticalMaximumProbe* g_criticalMaximumProbe=nullptr;
struct CriticalMaximumProbe {
    int kind;
    const void* actor=nullptr;
    CriticalMaximumProbe* previous;
    explicit CriticalMaximumProbe(int k):kind(k),previous(g_criticalMaximumProbe){g_criticalMaximumProbe=this;}
    ~CriticalMaximumProbe(){g_criticalMaximumProbe=previous;}
};
uint32_t g_criticalMaximumResume=0x495687;
__declspec(naked) void criticalMaximumNative(){__asm {push ecx
    push ebx
    push ebp
    mov ebp,[esp+0x14]
    jmp dword ptr [g_criticalMaximumResume]}}
typedef int(__thiscall* CriticalMaximum)(void*,int,int);
typedef uint8_t(__thiscall* CriticalAttribute)(void*,int);
CriticalMaximum g_criticalMaximumOriginal=(CriticalMaximum)criticalMaximumNative;
CriticalAttribute g_criticalAttributeOriginal=(CriticalAttribute)0x489030;
uint8_t __fastcall criticalAttribute(void* person,void*,int kind) {
    uint8_t result=g_criticalAttributeOriginal(person,kind);
    // Native4956fb is the second getter, executed only when this member replaces
    // the maximum. It chooses the last member on a tie; do not substitute first.
    if(g_criticalMaximumProbe && kind==g_criticalMaximumProbe->kind)g_criticalMaximumProbe->actor=person;
    return result;
}
int __fastcall criticalMaximum(void* unit,void*,int effect,int kind) {
    uintptr_t caller=(uintptr_t)_ReturnAddress();
    bool strategy=strategyMaximumActive(caller);
    if(strategy){
        CriticalMaximumProbe probe(kind);ProgressionProbe eligibility(effect);
        eligibility.scene="strategy";eligibility.event="stat_effect_eligibility";
        int result=g_criticalMaximumOriginal(unit,effect,kind);
        strategyMaximumObserved(unit,effect,kind,result,probe.actor,caller);return result;
    }
    if(!g_criticalProbe || g_criticalProbe->source!=unit || !criticalEffect(effect))return g_criticalMaximumOriginal(unit,effect,kind);
    CriticalMaximumProbe probe(kind);
    int result=g_criticalMaximumOriginal(unit,effect,kind);
    g_criticalProbe->actor=probe.actor;g_criticalProbe->holder=-1;g_criticalProbe->effect=probe.actor?effect:-1;
    return result;
}
void __cdecl criticalForced(){if(g_criticalProbe)g_criticalProbe->forced=true;}
uint32_t g_criticalForcedResume=0x5ae767,g_criticalSiegeForcedResume=0x5ae77e;
__declspec(naked) void criticalForcedBridge(){__asm {pushfd
    pushad
    call criticalForced
    popad
    popfd
    mov eax,1
    jmp dword ptr [g_criticalForcedResume]}}
__declspec(naked) void criticalSiegeForcedBridge(){__asm {pushfd
    pushad
    call criticalForced
    popad
    popfd
    mov eax,1
    jmp dword ptr [g_criticalSiegeForcedResume]}}

struct CriticalCalculation;
thread_local CriticalCalculation* g_criticalCalculation=nullptr;
struct DefenseAttribution {
    const void* actor=nullptr;
    int actorId=-1,owned=-1,effect=-1,incomingDamage=-1;
};
struct VineAttribution {
    RecoveryActor actor;void* unit=nullptr;int actorId=-1,unitId=-1,force=-1,weapon=-1,type=-1;
    int ids[3]={},affiliation[4]={},before=0,after=0;uint32_t position=0;bool stored=false;
};
struct CriticalCalculation {
    uint8_t* packet;
    void* source;
    const void* actor=nullptr;
    const void* target=nullptr;
    int effect=-1;
    DefenseAttribution defense;
    VineAttribution vine;
    int attackMode=0;
    uint64_t generation;
    CriticalCalculation* previous;
    CriticalCalculation(uint8_t* p,void* s,int mode=0):packet(p),source(s),attackMode(mode),generation(skilltelemetry::contextGeneration()),previous(g_criticalCalculation){g_criticalCalculation=this;}
    ~CriticalCalculation(){g_criticalCalculation=previous;}
};
uint32_t g_criticalNativeDecision=0x5ae610;
__declspec(naked) int __cdecl criticalInvoke(void*,int,void*,int) {
    __asm {push esi
        mov esi,[esp+8]
        mov eax,[esp+12]
        push [esp+20]
        push [esp+20]
        call dword ptr [g_criticalNativeDecision]
        add esp,8
        pop esi
        ret}
}
int __cdecl criticalDecision(void* source,int tactic,uint8_t* packet,void* target,int ranged) {
    auto calculation=g_criticalCalculation;
    if(!calculation || calculation->packet!=packet || calculation->source!=source)return criticalInvoke(source,tactic,target,ranged);
    CriticalProbe probe(source);
    int result=criticalInvoke(source,tactic,target,ranged);
    if(result && probe.forced && criticalEffect(probe.effect)) {
        auto actor=probe.actor?probe.actor:g_telemetryPerson((void*)0x7201958,probe.holder);
        if(actor && contains(*(const int*)((const uint8_t*)actor+0xe8),probe.effect)) {
            calculation->actor=actor;calculation->target=target;calculation->effect=probe.effect;
        }
    }
    return result;
}
uint32_t g_criticalDecisionResume=0x5aff72;
__declspec(naked) void criticalDecisionBridge(){__asm {push ebp
    push eax
    push esi
    call criticalDecision
    add esp,12
    jmp dword ptr [g_criticalDecisionResume]}}

struct AssistAttribution {
    const void* actor=nullptr;
    int actorId=-1,owned=-1,primaryUnit=-1,primaryForce=-1;
};
struct DoubleAttribution {
    const void* actor=nullptr;
    int actorId=-1,owned=-1,parentMode=-1;
};
struct AttackReachAttribution {
    const void* actor=nullptr;
    void* target=nullptr;
    int actorId=-1,owned=-1,effect=-1,sourceId=-1,targetId=-1,force=-1,targetForce=-1;
    int weapon=-1,distance=0,normalMax=0,actualMax=0,terrain=-1,permission=0,tactic=-1;
    uint32_t from=0,to=0;
    uint64_t generation=0;
};
AttackReachAttribution attackReachPrepare(void*,int,uint32_t,int,int=-1);
struct StrategyFireRecord {
    void* source=nullptr;void* target=nullptr;const void* actor=nullptr;
    int actorId=-1,owned=-1,effect=-1,commandMode=-1;uint32_t position=0;uint64_t generation=0;
};
struct StrategyStatusReceipt {
    StrategyFireRecord record;int plan=-1,sourceId=-1,targetId=-1,force[2]={},kind[2]={},weapon[2]={},ids[2][3]={},affiliation[5]={};
    uint32_t command[3]={},position[2]={};
};
thread_local uintptr_t g_ambushConstructionCaller=0;
struct CriticalRecord {
    const uint8_t* packet=nullptr;
    uint8_t snapshot[0x80]={};
    void* source=nullptr;
    const void* actor=nullptr;
    const void* target=nullptr;
    int actorId=-1,owned=-1,effect=-1,tactic=-1,weapon=-1;
    DefenseAttribution defense;
    VineAttribution vine;
    bool ambush=false;
    bool assistAttack=false;
    AssistAttribution assist;
    int attackMode=0;
    DoubleAttribution doubleAttack;
    AttackReachAttribution attackReach;
    AttackReachAttribution forestReach;
    StrategyStatusReceipt strategy;
};
void strategyAmbushPrepared(uint8_t*,void*,int,uint32_t,int,int,int,uintptr_t);
bool strategyAmbushGrantMatches(const CriticalRecord&,void*,const uint8_t*,void*,int);
void strategyAmbushGrantCommitted(const CriticalRecord&,int,int,int,uintptr_t,skilltelemetry::Target);
int criticalKind(const CriticalRecord& record){return record.doubleAttack.actor?4:record.ambush?1:record.assistAttack?3:0;}
bool vineGrantMatches(const CriticalRecord&,void*,const uint8_t*,void*,int);
void vineGrantCommitted(const CriticalRecord&,void*,int,int,uintptr_t,skilltelemetry::Target);
bool defenseGrantMatches(const CriticalRecord&,void*,const uint8_t*,void*,int);
void defenseGrantCommitted(const CriticalRecord&,void*,int,int,uintptr_t,skilltelemetry::Target);
int ironwallHandler(void*,const uint8_t*,int);
bool ironwallGrantMatches(void*,const uint8_t*,void*,int);
void ironwallGrantCommitted(void*,int,int,int,uintptr_t,skilltelemetry::Target);
bool assistGrantMatches(const CriticalRecord&,void*,const uint8_t*,void*,int);
void assistGrantCommitted(const CriticalRecord&,int,int,int,uintptr_t,skilltelemetry::Target);
int doubleAttackHandler(void*,const uint8_t*,int);
bool doubleGrantMatches(const CriticalRecord&,void*,const uint8_t*,void*,int);
void doubleGrantCommitted(const CriticalRecord&,int,int,int,uintptr_t,skilltelemetry::Target);
bool attackReachGrantMatches(const CriticalRecord&,void*,const uint8_t*,void*,int);
bool attackReachAttributionMatches(const CriticalRecord&,const AttackReachAttribution&,void*,const uint8_t*,void*,int);
bool afterAttackMember(void*,const void*);
void attackReachGrantCommitted(const CriticalRecord&,void*,const uint8_t*,void*,int,int,int,uintptr_t,skilltelemetry::Target);
SRWLOCK g_criticalCacheLock=SRWLOCK_INIT;
std::map<uintptr_t,CriticalRecord> g_criticalRecords;
uint64_t g_criticalCacheGeneration=0;
void criticalSyncLocked() {
    uint64_t generation=skilltelemetry::contextGeneration();
    if(g_criticalCacheGeneration!=generation){g_criticalRecords.clear();g_criticalCacheGeneration=generation;}
}
void criticalEraseRangeLocked(const uint8_t* first,size_t bytes) {
    g_criticalRecords.erase(g_criticalRecords.lower_bound((uintptr_t)first),
        g_criticalRecords.lower_bound((uintptr_t)first+bytes));
}
void criticalAllocationFailed() {
    // Preserve gameplay, but make a missing provenance record an explicit log
    // failure instead of silently evicting an unrelated pending attack.
    skilltelemetry::close();
    if(g_log)g_log("skill telemetry: critical provenance allocation failed; logging disabled");
}
// Both native settlement handlers copy 0x181 dwords: twelve 0x80-byte
// records and the count at +0x600. Their stack copies die on handler return.
struct CriticalCopyScope;
thread_local CriticalCopyScope* g_criticalCopyScope=nullptr;
struct CriticalCopyScope {
    const uint8_t* destination=nullptr;
    const uint8_t* aggregate=nullptr;
    const uint8_t* secondarySource=nullptr;
    // The native group loop runs backwards and settles a single shared write.
    // Keep component operations separate from that shared resource balance.
    struct Component {int index;CriticalRecord record;};
    std::vector<Component> components;
    uint8_t aggregateSnapshot[0x80]={};
    int aggregateNext=-1,aggregateCount=0;
    bool aggregateScaled=false;
    CriticalCopyScope* previous;
    uint64_t generation;
    CriticalCopyScope():previous(g_criticalCopyScope),generation(skilltelemetry::contextGeneration()){g_criticalCopyScope=this;}
    ~CriticalCopyScope(){
        if(destination) {
            AcquireSRWLockExclusive(&g_criticalCacheLock);
            criticalSyncLocked();
            if(generation==g_criticalCacheGeneration){
                criticalEraseRangeLocked(destination,0x600);
                if(aggregate)g_criticalRecords.erase((uintptr_t)aggregate);
                if(secondarySource)criticalEraseRangeLocked(secondarySource,0x600);
            }
            ReleaseSRWLockExclusive(&g_criticalCacheLock);
        }
        g_criticalCopyScope=previous;
    }
};
void __cdecl criticalCopy(uint8_t* destination,const uint8_t* source) {
    if(!skilltelemetry::enabled() || !g_criticalCopyScope)return;
    g_criticalCopyScope->destination=destination;
    CriticalRecord copies[12];int count=*(const int*)(source+0x600);bool failed=false;
    AcquireSRWLockExclusive(&g_criticalCacheLock);
    criticalSyncLocked();
    // Snapshot before invalidating destinations, including an overlapping copy.
    if(g_criticalCopyScope->generation==g_criticalCacheGeneration && count>=0 && count<=12)for(int n=0;n<count;++n) {
        auto i=g_criticalRecords.find((uintptr_t)(source+n*0x80));
        if(i!=g_criticalRecords.end()) {
            const auto& record=i->second;
            if(!memcmp(record.snapshot,record.packet,sizeof record.snapshot))copies[n]=record;
        }
    }
    criticalEraseRangeLocked(destination,0x600);
    try {for(int n=0;n<12;++n)if(copies[n].actor || copies[n].defense.actor || copies[n].vine.actor.person || copies[n].assist.actor || copies[n].doubleAttack.actor || copies[n].attackReach.actor || copies[n].forestReach.actor || copies[n].strategy.record.actor) {
        copies[n].packet=destination+n*0x80;
        g_criticalRecords.insert_or_assign((uintptr_t)copies[n].packet,copies[n]);
    }}catch(...) {failed=true;}
    ReleaseSRWLockExclusive(&g_criticalCacheLock);
    if(failed)criticalAllocationFailed();
}
uint32_t g_criticalCopyOneResume=0x5857bd,g_criticalCopyTwoResume=0x5860d5;
const int* g_criticalCopyOneOffset=(const int*)0x5857c0;
const int* g_criticalCopyTwoOffset=(const int*)0x5860d8;
__declspec(naked) void criticalCopyOneBridge(){__asm {pushfd
    pushad
    lea eax,[esp+36]
    mov ecx,dword ptr [g_criticalCopyOneOffset]
    add eax,[ecx]
    push esi
    push eax
    call criticalCopy
    add esp,8
    popad
    popfd
    mov ecx,0x181
    jmp dword ptr [g_criticalCopyOneResume]}}
__declspec(naked) void criticalCopyTwoBridge(){__asm {pushfd
    pushad
    lea eax,[esp+36]
    mov ecx,dword ptr [g_criticalCopyTwoOffset]
    add eax,[ecx]
    push esi
    push eax
    call criticalCopy
    add esp,8
    popad
    popfd
    mov ecx,0x181
    jmp dword ptr [g_criticalCopyTwoResume]}}
uint32_t g_criticalHandlerOneResume=0x585786,g_criticalHandlerTwoResume=0x5860a6;
__declspec(naked) void criticalHandlerOneNative(){__asm {push ebp
    mov ebp,esp
    and esp,-8
    jmp dword ptr [g_criticalHandlerOneResume]}}
__declspec(naked) void criticalHandlerTwoNative(){__asm {push ebp
    mov ebp,esp
    and esp,-8
    jmp dword ptr [g_criticalHandlerTwoResume]}}
typedef int(__thiscall* CriticalHandlerOne)(void*,const uint8_t*);
typedef int(__thiscall* CriticalHandlerTwo)(void*,const uint8_t*,int);
CriticalHandlerOne g_criticalHandlerOneOriginal=(CriticalHandlerOne)criticalHandlerOneNative;
CriticalHandlerTwo g_criticalHandlerTwoOriginal=(CriticalHandlerTwo)criticalHandlerTwoNative;
int __fastcall criticalHandlerOne(void* self,void*,const uint8_t* records) {
    CriticalCopyScope scope;return g_criticalHandlerOneOriginal(self,records);
}
int __fastcall criticalHandlerTwo(void* self,void*,const uint8_t* records,int mode) {
    CriticalCopyScope scope;return ironwallHandler(self,records,mode);
}
//592a10 owns a single0x604-byte group on its own stack. Its source metadata
// must expire on return, independently of any child's copied settlement buffer.
typedef int(__thiscall* CriticalOwner)(void*,void*,uint32_t);
uint32_t g_criticalOwnerResume=0x592a16,g_criticalOwnerZeroResume=0x592a2a;
__declspec(naked) void criticalOwnerNative(){__asm {push ebp
    mov ebp,esp
    and esp,-8
    jmp dword ptr [g_criticalOwnerResume]}}
CriticalOwner g_criticalOwnerOriginal=(CriticalOwner)criticalOwnerNative;
int __fastcall criticalOwner(void* self,void*,void* source,uint32_t position) {
    CriticalCopyScope scope;return g_criticalOwnerOriginal(self,source,position);
}
void __cdecl criticalOwnerZero(const uint8_t* destination) {
    if(!skilltelemetry::enabled() || !g_criticalCopyScope)return;
    g_criticalCopyScope->destination=destination;
    AcquireSRWLockExclusive(&g_criticalCacheLock);criticalSyncLocked();
    criticalEraseRangeLocked(destination,0x600);ReleaseSRWLockExclusive(&g_criticalCacheLock);
}
__declspec(naked) void criticalOwnerZeroBridge(){__asm {pushfd
    pushad
    lea eax,[esp+56]
    push eax
    call criticalOwnerZero
    add esp,4
    popad
    popfd
    mov ecx,0x181
    jmp dword ptr [g_criticalOwnerZeroResume]}}
// Both position-action functions own the final0x604 bytes below their entry
// stack pointer. Register that group after replaying their native allocation.
typedef int(__thiscall* CriticalPositionOwner)(void*,uint32_t);
uint32_t g_criticalPositionOneResume=0x579e46,g_criticalPositionTwoResume=0x57ad76;
__declspec(naked) void criticalPositionOneNative(){__asm {sub esp,0x640
    pushfd
    pushad
    lea eax,[esp+0x60]
    push eax
    call criticalOwnerZero
    add esp,4
    popad
    popfd
    jmp dword ptr [g_criticalPositionOneResume]}}
__declspec(naked) void criticalPositionTwoNative(){__asm {sub esp,0x604
    pushfd
    pushad
    lea eax,[esp+36]
    push eax
    call criticalOwnerZero
    add esp,4
    popad
    popfd
    jmp dword ptr [g_criticalPositionTwoResume]}}
CriticalPositionOwner g_criticalPositionOneOriginal=(CriticalPositionOwner)criticalPositionOneNative;
CriticalPositionOwner g_criticalPositionTwoOriginal=(CriticalPositionOwner)criticalPositionTwoNative;
int __fastcall criticalPositionOne(void* self,void*,uint32_t position) {
    CriticalCopyScope scope;return g_criticalPositionOneOriginal(self,position);
}
int __fastcall criticalPositionTwo(void* self,void*,uint32_t position) {
    CriticalCopyScope scope;return g_criticalPositionTwoOriginal(self,position);
}
CriticalRecord criticalTake(const uint8_t* packet) {
    CriticalRecord record;
    AcquireSRWLockExclusive(&g_criticalCacheLock);
    criticalSyncLocked();
    auto i=g_criticalRecords.find((uintptr_t)packet);
    if(i!=g_criticalRecords.end()){record=i->second;g_criticalRecords.erase(i);}
    ReleaseSRWLockExclusive(&g_criticalCacheLock);return record;
}
CriticalRecord criticalLookup(const uint8_t* packet) {
    CriticalRecord record;
    AcquireSRWLockExclusive(&g_criticalCacheLock);
    criticalSyncLocked();
    auto i=g_criticalRecords.find((uintptr_t)packet);
    if(i!=g_criticalRecords.end()) {
        if(!memcmp(i->second.snapshot,packet,sizeof i->second.snapshot))record=i->second;
        else g_criticalRecords.erase(i);
    }
    ReleaseSRWLockExclusive(&g_criticalCacheLock);return record;
}

uint32_t g_criticalAggregateResume=0x584516,g_criticalAggregateCaller=0x586646;
__declspec(naked) void criticalAggregateNative(){__asm {mov eax,[esp+4]
    mov edx,[eax]
    jmp dword ptr [g_criticalAggregateResume]}}
typedef int(__thiscall* CriticalAggregate)(uint8_t*,const uint8_t*);
CriticalAggregate g_criticalAggregateOriginal=(CriticalAggregate)criticalAggregateNative;
int __fastcall criticalAggregate(uint8_t* aggregate,void*,const uint8_t* source){
    auto scope=g_criticalCopyScope;uint64_t generation=skilltelemetry::contextGeneration();CriticalRecord record;
    bool component=false;int index=-1,count=0;
    if(skilltelemetry::enabled() && scope && scope->generation==generation && (uintptr_t)_ReturnAddress()==g_criticalAggregateCaller &&
       scope->destination && aggregate==scope->destination-0x488 && (!scope->aggregate || scope->aggregate==aggregate)){
        count=*(const int*)(scope->destination+0x600);
        uintptr_t offset=(uintptr_t)source-(uintptr_t)scope->destination;
        if(count>=1 && count<=12 && offset<(uintptr_t)count*0x80 && !(offset%0x80)){
            index=(int)(offset/0x80);
            if(count==1)record=criticalLookup(source);
            else if((!scope->aggregate && index==count-1) ||
                    (scope->aggregate==aggregate && scope->aggregateNext==index &&
                     scope->aggregateCount==count && !memcmp(scope->aggregateSnapshot,aggregate,0x80))){
                component=true;record=criticalLookup(source);
            }
        }
    }
    if(skilltelemetry::enabled())criticalTake(aggregate);
    int result=g_criticalAggregateOriginal(aggregate,source);
    if(component && generation==skilltelemetry::contextGeneration()){
        scope->aggregate=aggregate;scope->aggregateCount=count;scope->aggregateNext=index-1;
        memcpy(scope->aggregateSnapshot,aggregate,0x80);
        try {if(record.packet)scope->components.push_back({index,record});}
        catch(...){scope->components.clear();scope->aggregateCount=0;criticalAllocationFailed();}
        return result;
    }
    if(scope && scope->aggregate==aggregate){scope->components.clear();scope->aggregateCount=0;scope->aggregateScaled=false;}
    if(record.actor || record.defense.actor || record.vine.actor.person || record.assist.actor || record.doubleAttack.actor || record.attackReach.actor || record.forestReach.actor || record.strategy.record.actor){
        record.packet=aggregate;memcpy(record.snapshot,aggregate,sizeof record.snapshot);bool failed=false;
        AcquireSRWLockExclusive(&g_criticalCacheLock);criticalSyncLocked();
        try {if(generation==g_criticalCacheGeneration && scope->generation==generation){
            g_criticalRecords.insert_or_assign((uintptr_t)aggregate,record);scope->aggregate=aggregate;
        }}catch(...){failed=true;}
        ReleaseSRWLockExclusive(&g_criticalCacheLock);if(failed)criticalAllocationFailed();
    }
    return result;
}
// Observe the original converter CALL, leaving the count<=1 jump target and
// original damage store intact. The grant must later match this exact result.
uint32_t g_criticalAggregateFloatEntry=0x707a74;
void __cdecl criticalAggregateScaled(uint8_t* aggregate,int damage){
    auto scope=g_criticalCopyScope;
    if(!scope || scope->aggregate!=aggregate || scope->aggregateCount<=1 || scope->aggregateNext!=-1 ||
       scope->generation!=skilltelemetry::contextGeneration() || memcmp(scope->aggregateSnapshot,aggregate,0x80))return;
    *(int*)(scope->aggregateSnapshot+0xc)=damage;scope->aggregateScaled=true;
}
__declspec(naked) void criticalAggregateScaleBridge(){__asm {call dword ptr [g_criticalAggregateFloatEntry]
    pushfd
    pushad
    lea ecx,[esp+0x78]
    push eax
    push ecx
    call criticalAggregateScaled
    add esp,8
    popad
    popfd
    ret}}
typedef int(__cdecl* CriticalProducer)(uint8_t*,void*,int,uint32_t,int,int,int);
void __cdecl criticalReset(const uint8_t* packet) {
    if(skilltelemetry::enabled())criticalTake(packet);
}
uint32_t g_criticalResetResume=0x5afc35,g_criticalCounterResetResume=0x5b0688,g_criticalExtraResetResume=0x5b0ee5;
__declspec(naked) void criticalResetBridge(){__asm {pushfd
    pushad
    push [esp+40]
    call criticalReset
    add esp,4
    popad
    popfd
    mov edx,[esp+4]
    push edi
    jmp dword ptr [g_criticalResetResume]}}
__declspec(naked) void criticalCounterResetBridge(){__asm {pushfd
    pushad
    push [esp+40]
    call criticalReset
    add esp,4
    popad
    popfd
    sub esp,0x28
    push ebx
    mov ebx,[esp+0x34]
    jmp dword ptr [g_criticalCounterResetResume]}}
__declspec(naked) void criticalExtraResetBridge(){__asm {pushfd
    pushad
    push [esp+40]
    call criticalReset
    add esp,4
    popad
    popfd
    sub esp,8
    push ebx
    push ebp
    jmp dword ptr [g_criticalExtraResetResume]}}
uint32_t g_criticalProducerResume=0x5afc76;
__declspec(naked) void criticalProducerNative(){__asm {sub esp,0x24
    push ebx
    push ebp
    push esi
    jmp dword ptr [g_criticalProducerResume]}}
CriticalProducer g_criticalProducerOriginal=(CriticalProducer)criticalProducerNative;
int __cdecl criticalProducer(uint8_t* packet,void* source,int tactic,uint32_t position,int a,int b,int c) {
    if(!skilltelemetry::enabled())return g_criticalProducerOriginal(packet,source,tactic,position,a,b,c);
    criticalTake(packet); // every new generation invalidates previous data at this address
    CriticalCalculation calculation(packet,source,a);
    auto reach=attackReachPrepare(source,tactic,position,a);
    AttackReachAttribution forest;
    // Custom crossbow tactics12/15 can require both permissions. Keep50
    // independent of24, including targets inside the ordinary tactic range.
    if(source && *(const int*)((const uint8_t*)source+0xa8)==3 && (tactic==12 || tactic==15))
        forest=attackReachPrepare(source,tactic,position,a,50);
    int result=g_criticalProducerOriginal(packet,source,tactic,position,a,b,c);
    bool critical=calculation.actor && *(int*)packet==1;
    bool defense=calculation.defense.actor && *(int*)(packet+0xc)==0 &&
        *(int*)(packet+0x64)==calculation.defense.effect;
    bool vine=calculation.vine.actor.person && calculation.vine.stored;
    if((critical || defense || vine || reach.actor || forest.actor) && *(int*)(packet+0x70)==0) {
        int id=critical?growthPersonId((void*)calculation.actor):-1;
        if(id>=0 || defense || vine || reach.actor || forest.actor) {
            CriticalRecord record;record.packet=packet;record.source=source;record.actor=calculation.actor;record.target=calculation.target;record.actorId=id;
            if(id>=0){record.owned=*(const int*)((const uint8_t*)record.actor+0xe8);record.effect=calculation.effect;}
            else record.actor=nullptr;
            if(defense)record.defense=calculation.defense;record.tactic=tactic;
            if(vine)record.vine=calculation.vine;
            record.attackReach=reach;
            record.forestReach=forest;
            record.attackMode=calculation.attackMode;
            record.weapon=*(const int*)((const uint8_t*)source+0xa8);memcpy(record.snapshot,packet,sizeof record.snapshot);
            bool failed=false;AcquireSRWLockExclusive(&g_criticalCacheLock);
            criticalSyncLocked();
            try {if(calculation.generation==g_criticalCacheGeneration &&
                    (!g_criticalCopyScope || !g_criticalCopyScope->secondarySource ||
                     packet<g_criticalCopyScope->secondarySource || packet>=g_criticalCopyScope->secondarySource+0x600 ||
                     g_criticalCopyScope->generation==calculation.generation))
                g_criticalRecords.insert_or_assign((uintptr_t)packet,record);}catch(...) {failed=true;}
            ReleaseSRWLockExclusive(&g_criticalCacheLock);
            if(failed)criticalAllocationFailed();
        }
    }
    if(g_ambushConstructionCaller)strategyAmbushPrepared(packet,source,tactic,position,a,b,c,(uintptr_t)_ReturnAddress());
    return result;
}
IncomeGrant g_criticalBaseTroopsOriginal=(IncomeGrant)0x4ae3c0;
IncomeGrant g_criticalBaseHpOriginal=(IncomeGrant)0x4ae360;
bool criticalBatchMatches(CriticalCopyScope* scope,const uint8_t* packet,int requested){
    return scope && scope->aggregate==packet && scope->aggregateCount>1 && scope->aggregateNext==-1 && scope->aggregateScaled &&
        scope->generation==skilltelemetry::contextGeneration() && !scope->components.empty() &&
        requested<=0 && requested==-*(const int*)(packet+0xc) && !memcmp(scope->aggregateSnapshot,packet,0x80);
}
unsigned criticalComponentEffects(const CriticalRecord& r,void* affected){
    if(!r.source)return 0;
    int id=g_telemetryUnitId((void*)0x7201958,r.source),targetId=g_telemetryUnitId((void*)0x7201958,affected);
    if(id<0 || targetId<0 || g_movementUnitById((void*)0x7201958,id)!=r.source ||
       g_movementUnitById((void*)0x7201958,targetId)!=affected)return 0;
    typedef const uint32_t*(__thiscall* Position)(void*);
    const uint8_t* p=r.snapshot;int damage=*(const int*)(p+0xc);
    if(damage<0 || *(*(Position*)(*(uint8_t**)r.source+0x3c))(r.source)!=*(const uint32_t*)(p+0x58) ||
       *(*(Position*)(*(uint8_t**)affected+0x3c))(affected)!=*(const uint32_t*)(p+0x5c))return 0;
    unsigned flags=0;
    if(r.actor && r.target==affected && *(const int*)p==1 && *(const int*)(p+0x70)==criticalKind(r) &&
       growthPersonId((void*)r.actor)==r.actorId && *(const int*)((const uint8_t*)r.actor+0xe8)==r.owned &&
       contains(r.owned,r.effect) && afterAttackMember(r.source,r.actor))flags|=1;
    if(defenseGrantMatches(r,r.source,p,affected,-damage))flags|=2;
    if(vineGrantMatches(r,r.source,p,affected,-damage))flags|=4;
    if(assistGrantMatches(r,r.source,p,affected,-damage))flags|=8;
    if(doubleGrantMatches(r,r.source,p,affected,-damage))flags|=16;
    if(attackReachAttributionMatches(r,r.attackReach,r.source,p,affected,-damage))flags|=32;
    if(attackReachAttributionMatches(r,r.forestReach,r.source,p,affected,-damage))flags|=64;
    if(strategyAmbushGrantMatches(r,r.source,p,affected,-damage))flags|=128;
    return flags;
}
__declspec(align(8)) volatile LONG64 g_criticalAggregateWrite=0;
void criticalComponentCommitted(const CriticalCopyScope::Component& c,unsigned flags,int count,uint64_t write,
        int requested,int before,int after,uintptr_t caller,skilltelemetry::Target target){
    const auto& r=c.record;
    for(unsigned bit=1;bit<=128;bit<<=1)if(flags&bit){
        const void* actor=nullptr;int owned=-1,effect=-1,stepBefore=0,stepAfter=1;
        const char* role="",*metric="settled_component";void* actorUnit=r.source;
        if(bit==1){actor=r.actor;owned=r.owned;effect=r.effect;role="forced_critical";}
        if(bit==2){actor=r.defense.actor;owned=r.defense.owned;effect=r.defense.effect;role="damage_immunity";
            actorUnit=(void*)r.target;stepBefore=r.defense.incomingDamage;stepAfter=0;metric="calculated_physical_troop_damage";}
        if(bit==4){actor=r.vine.actor.person;owned=r.vine.actor.owned;effect=31;role="physical_halving";
            actorUnit=r.vine.unit;stepBefore=r.vine.before;stepAfter=r.vine.after;metric="calculated_physical_troop_damage";}
        if(bit==8){actor=r.assist.actor;owned=r.assist.owned;effect=26;role="assist";}
        if(bit==16){actor=r.doubleAttack.actor;owned=r.doubleAttack.owned;effect=12;role="double_attack";}
        if(bit==32 || bit==64){const auto& a=bit==32?r.attackReach:r.forestReach;actor=a.actor;owned=a.owned;effect=a.effect;role=bit==32?"extended_range":"forest_range";}
        if(bit==128){actor=r.strategy.record.actor;owned=r.strategy.record.owned;effect=r.strategy.record.effect;role="strategy_critical_ambush";}
        target.actorUnit=g_telemetryUnitId((void*)0x7201958,actorUnit);target.equipment=*(const int*)((const uint8_t*)actorUnit+0xa8);
        uint32_t position=*(const uint32_t*)(r.snapshot+0x5c);target.x=(int16_t)position;target.y=(int16_t)(position>>16);
        char context[768];sprintf_s(context,"{\"aggregate_settlement_id\":%llu,\"component_index\":%d,\"component_count\":%d,\"component_role\":\"%s\",\"component_source_unit\":%d,\"tactic_id\":%d,\"record_kind\":%d,\"native_attack_mode\":%d,\"calculated_component_damage\":%d,\"shared_requested_troop_loss\":%d,\"shared_resource_before\":%d,\"shared_resource_after\":%d,\"damage_attribution\":\"component_operation_with_shared_commit_context\",\"counterfactual_evaluated\":false}",
            (unsigned long long)write,c.index,count,role,g_telemetryUnitId((void*)0x7201958,r.source),r.tactic,criticalKind(r),r.attackMode,
            *(const int*)(r.snapshot+0xc),-requested,before,after);target.context=context;
        skilltelemetry::applied(actor,owned,effect,"battle","aggregated_attack_component_settled",metric,stepBefore,stepAfter,caller,target);
    }
}
int criticalGrant(void* self,void* source,const uint8_t* packet,void* affected,int requested,uintptr_t caller) {
    bool base=caller==0x5b1a74 || caller==0x5b1aaf,hp=caller==0x5b1aaf;
    auto grant=hp?g_criticalBaseHpOriginal:base?g_criticalBaseTroopsOriginal:g_unitTroopsOriginal;
    uint64_t generation=skilltelemetry::contextGeneration();
    auto record=skilltelemetry::enabled()?criticalLookup(packet):CriticalRecord{};
    bool observe=record.actor && record.source==source && record.target==affected && !memcmp(record.snapshot,packet,sizeof record.snapshot) &&
        *(const int*)packet==1 && *(const int*)(packet+0x70)==criticalKind(record) && requested==-*(const int*)(packet+(hp?0x10:0xc)) &&
        growthPersonId((void*)record.actor)==record.actorId && *(const int*)((const uint8_t*)record.actor+0xe8)==record.owned;
    bool defensive=!base && defenseGrantMatches(record,source,packet,affected,requested);
    bool ironwall=!base && caller==0x5b1c6c && ironwallGrantMatches(source,packet,affected,requested);
    bool assist=!base && assistGrantMatches(record,source,packet,affected,requested);
    bool doubleAttack=!base && doubleGrantMatches(record,source,packet,affected,requested);
    bool magic=magicGrantMatches(source,packet,affected,requested,caller);
    bool strategy=!base && strategyAmbushGrantMatches(record,source,packet,affected,requested);
    bool reach=!base && attackReachGrantMatches(record,source,packet,affected,requested);
    bool vine=!base && vineGrantMatches(record,source,packet,affected,requested);
    auto batch=g_criticalCopyScope;unsigned componentFlags[12]={};
    bool aggregate=!base && caller==0x5b1c6c && criticalBatchMatches(batch,packet,requested);
    if(aggregate)for(size_t n=0;n<batch->components.size();++n)componentFlags[n]=criticalComponentEffects(batch->components[n].record,affected);
    skilltelemetry::Target target;
    if(observe || defensive || ironwall || assist || doubleAttack || magic || strategy || reach || vine || aggregate) {
        target=base?incomeTarget(affected):unitTarget(affected);
        target.actorUnit=g_telemetryUnitId((void*)0x7201958,source);target.equipment=magic?*(const int*)((const uint8_t*)source+0xa8):record.weapon;
        typedef const uint32_t*(__thiscall* Position)(void*);
        auto from=(*(Position*)(*(uint8_t**)source+0x3c))(source),to=(*(Position*)(*(uint8_t**)affected+0x3c))(affected);
        bool registered=base?target.base>=0 && g_baseById((void*)0x7201958,target.base)==affected:
            target.unit>=0 && g_movementUnitById((void*)0x7201958,target.unit)==affected;
        bool valid=registered && target.actorUnit>=0 &&
            g_movementUnitById((void*)0x7201958,target.actorUnit)==source && *from==*(const uint32_t*)(packet+0x58) &&
            *to==*(const uint32_t*)(packet+0x5c);
        observe=observe && valid;defensive=defensive && valid;ironwall=ironwall && valid;assist=assist && valid;doubleAttack=doubleAttack && valid;
        magic=magic && valid;
        strategy=strategy && valid;
        reach=reach && valid;
        vine=vine && valid;
        aggregate=aggregate && valid;
    }
    auto value=(observe || defensive || ironwall || assist || doubleAttack || magic || strategy || reach || vine || aggregate) && !hp?(*(BuildingValue*)(*(uint8_t**)affected+0x50)):nullptr;
    int before=(observe || defensive || ironwall || assist || doubleAttack || magic || strategy || reach || vine || aggregate)?(hp?*(const int16_t*)((const uint8_t*)affected+0x10):value(affected)):0;
    int result=trapGrantObserved(self,affected,requested,caller,packet,nullptr,grant);
    if(observe && generation==skilltelemetry::contextGeneration()) {
        uint32_t position=*(const uint32_t*)(packet+0x5c);target.x=(int16_t)position;target.y=(int16_t)(position>>16);
        char context[224];
        sprintf_s(context,"{\"tactic_id\":%d,\"record_kind\":%d,\"critical\":true,\"%s\":%d,\"damage_attribution\":\"whole_attack_not_marginal_skill_bonus\",\"counterfactual_evaluated\":false}",
            record.tactic,criticalKind(record),hp?"requested_hp_loss":"requested_troop_loss",-requested);target.context=context;
        // This measures the whole critical hit's actual resource loss. It is not an
        // invented no-critical counterfactual or marginal skill damage bonus.
        skilltelemetry::applied(record.actor,record.owned,record.effect,"battle",record.doubleAttack.actor?"forced_critical_double_attack_settled":record.ambush?"forced_critical_ambush_settled":"forced_critical_attack_settled",
            hp?"facility_hp":"troops",before,hp?*(const int16_t*)((const uint8_t*)affected+0x10):value(affected),caller,target);
    }
    if(defensive && generation==skilltelemetry::contextGeneration())
        defenseGrantCommitted(record,affected,before,value(affected),caller,target);
    if(ironwall && generation==skilltelemetry::contextGeneration())
        ironwallGrantCommitted(affected,requested,before,value(affected),caller,target);
    if(assist && generation==skilltelemetry::contextGeneration())
        assistGrantCommitted(record,requested,before,value(affected),caller,target);
    if(doubleAttack && generation==skilltelemetry::contextGeneration())
        doubleGrantCommitted(record,requested,before,value(affected),caller,target);
    if(magic && generation==skilltelemetry::contextGeneration() && magicGrantMatches(source,packet,affected,requested,caller))
        magicGrantCommitted(packet,requested,before,hp?*(const int16_t*)((const uint8_t*)affected+0x10):value(affected),caller,target);
    if(strategy && generation==skilltelemetry::contextGeneration() && strategyAmbushGrantMatches(record,source,packet,affected,requested))
        strategyAmbushGrantCommitted(record,requested,before,value(affected),caller,target);
    if(reach && generation==skilltelemetry::contextGeneration() && attackReachGrantMatches(record,source,packet,affected,requested))
        attackReachGrantCommitted(record,source,packet,affected,requested,before,value(affected),caller,target);
    if(vine && generation==skilltelemetry::contextGeneration() && vineGrantMatches(record,source,packet,affected,requested))
        vineGrantCommitted(record,affected,before,value(affected),caller,target);
    if(aggregate && generation==skilltelemetry::contextGeneration() && criticalBatchMatches(batch,packet,requested)){
        uint64_t write=(uint64_t)InterlockedIncrement64(&g_criticalAggregateWrite);
        for(size_t n=0;n<batch->components.size();++n)
            criticalComponentCommitted(batch->components[n],componentFlags[n]&criticalComponentEffects(batch->components[n].record,affected),
                batch->aggregateCount,write,requested,before,value(affected),caller,target);
    }
    return result;
}
int __fastcall criticalTroops(void* self,void*,void* source,const uint8_t* packet,void* unit,int requested) {
    return criticalGrant(self,source,packet,unit,requested,0x584fdf);
}
int __fastcall criticalQueuedGrant(void* self,void*,void* source,const uint8_t* packet,uintptr_t caller,void* affected,int requested) {
    return criticalGrant(self,source,packet,affected,requested,caller);
}
uint32_t g_criticalQueuedTroopsResume=0x5b1c71,g_criticalBaseTroopsResume=0x5b1a79,g_criticalBaseHpResume=0x5b1ab4;
__declspec(naked) void criticalQueuedTroopsBridge(){__asm {push 0x5b1c6c
    push edi
    push ebp
    call criticalQueuedGrant
    jmp dword ptr [g_criticalQueuedTroopsResume]}}
__declspec(naked) void criticalBaseTroopsBridge(){__asm {push 0x5b1a74
    push edi
    push ebp
    call criticalQueuedGrant
    jmp dword ptr [g_criticalBaseTroopsResume]}}
__declspec(naked) void criticalBaseHpBridge(){__asm {push 0x5b1aaf
    push edi
    push ebp
    call criticalQueuedGrant
    jmp dword ptr [g_criticalBaseHpResume]}}
uint32_t g_criticalTroopsResume=0x584fe4;
__declspec(naked) void criticalTroopsBridge(){__asm {push edi
    push ebx
    call criticalTroops
    jmp dword ptr [g_criticalTroopsResume]}}
