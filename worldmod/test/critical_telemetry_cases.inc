// Run the complete native critical decision and maximum-member scan. Packet
// arithmetic/world lookup are fixtures; the actual critical CALL/store and
// committed troop CALL execute, including original troop clamps/word setters.
static uintptr_t g_criticalCallEntry,g_criticalCommitEntry;
static uint8_t* g_criticalSource;
static uint8_t* g_criticalTarget;
static uint8_t g_criticalCells[10000]={};
static int g_criticalRandomResult=0,g_criticalRandomCalls=0;
static bool g_criticalWorldReadDuringCalculation=false;
static bool g_criticalBuildingTarget=false;
static int g_criticalFixtureTroopLoss=250,g_criticalFixtureHpLoss=125;
static bool g_criticalSourceRegistered=true,g_criticalTargetRegistered=true,g_criticalActorRegistered=true;
static const uint32_t* __fastcall criticalFixturePosition(uint8_t* unit,void*) {return (const uint32_t*)(unit+4);}
static int __fastcall criticalFixtureOwner(void* unit,void*) {return unit==g_criticalSource?2:3;}
static int __fastcall criticalFixtureObject(void* object,void*,int kind) {return kind==(object==g_criticalTarget && g_criticalBuildingTarget?5:11);}
static int __fastcall criticalFixtureFind(uint8_t* unit,void*,int effect) {
    for(int i=0;i<3;++i){int id=*(int*)(unit+0xc+4*i);auto p=personFixtureLookup(nullptr,nullptr,id);
        if(p && personHas(p,nullptr,effect))return id;}
    return -1;
}
static int __fastcall criticalFixtureUnitId(void*,void*,void* unit) {return unit==g_criticalSource?4095:unit==g_criticalTarget?4094:-1;}
static void* __fastcall criticalFixtureUnit(void*,void*,int id) {
    return id==4095 && g_criticalSourceRegistered?g_criticalSource:id==4094 && g_criticalTargetRegistered?g_criticalTarget:nullptr;
}
static int __fastcall criticalFixturePersonId(void* w,void* unused,const void* p) {
    return p==g_personGuide && !g_criticalActorRegistered?-1:personFixtureId(w,unused,p);
}
static uint8_t __fastcall criticalFixtureAttribute(uint8_t* person,void*,int kind) {assert(kind==1);return person[0xc9];}
static int __fastcall criticalFixtureOpponent(void* unit,void*,int kind) {assert(kind==1);return ((uint8_t*)unit)[0xc9];}
static int __fastcall criticalFixtureArmy(void*,void*) {return 0;}
static void* __fastcall criticalFixtureLeader(void*,void*) {return g_personGuide;}
static int __fastcall criticalFixtureMember(void*,void*,int) {return -1;}
static int __cdecl criticalFixtureRandom(int percent) {assert(percent>=0 && percent<=100);++g_criticalRandomCalls;return g_criticalRandomResult;}
static int __fastcall criticalFixtureCapacity(void*,void*) {return 10000;}
static int __fastcall criticalFixtureTroopsWorldRead(void* self,void*,void* unit,int requested) {
    int result=((IncomeGrant)site(0x4ae4a0))(self,unit,requested);skillsTelemetryStream(1,0);return result;
}
__declspec(naked) static int __cdecl runCriticalCall(void* packet,void* source,int tactic,void* target,int ranged) {
    __asm {push esi
        push ebp
        mov ebp,[esp+12]
        mov esi,[esp+16]
        mov eax,[esp+20]
        push [esp+28]
        push [esp+28]
        mov edx,g_criticalCallEntry
        jmp edx}
}
__declspec(naked) static void criticalFixtureCalculated(){__asm {pop ebp
    pop esi
    ret}}
static int __cdecl criticalFixtureProducer(uint8_t* packet,void* source,int tactic,uint32_t target,int,int ranged,int) {
    memset(packet,0,0x80);*(int*)(packet+0xc)=g_criticalFixtureTroopLoss;*(int*)(packet+0x54)=1;
    if(g_criticalBuildingTarget)*(int*)(packet+0x10)=g_criticalFixtureHpLoss;
    *(uint32_t*)(packet+0x58)=*(uint32_t*)((uint8_t*)source+4);*(uint32_t*)(packet+0x5c)=target;
    runCriticalCall(packet,source,tactic,g_criticalTarget,ranged);
    if(g_criticalWorldReadDuringCalculation)skillsTelemetryStream(1,0);
    return 0x1234;
}
__declspec(naked) static int __cdecl runCriticalCommit(void* target,void* source,void* packet) {
    __asm {push ebx
        push edi
        push esi
        mov esi,[esp+16]
        mov ebx,[esp+20]
        mov edi,[esp+24]
        mov eax,[edi+0xc]
        neg eax
        push eax
        push esi
        xor ecx,ecx
        mov eax,g_criticalCommitEntry
        jmp eax}
}
__declspec(naked) static void criticalFixtureCommitted(){__asm {pop esi
    pop edi
    pop ebx
    ret}}
__declspec(naked) static void criticalFixtureProducerProlog(){__asm {pop esi
    pop ebp
    pop ebx
    add esp,0x24
    mov eax,0x1234
    ret}}
static uintptr_t g_criticalCopyOneEntry,g_criticalCopyTwoEntry;
static uintptr_t g_criticalCounterInitEntry;
__declspec(naked) static void criticalFixtureCounterInit(){__asm {push ebp
    push esi
    push edi
    jmp dword ptr [g_criticalCounterInitEntry]}}
__declspec(naked) static void criticalFixtureCounterInitialized(){__asm {pop edi
    pop esi
    pop ebp
    pop ebx
    add esp,0x28
    mov eax,0x4321
    ret}}
__declspec(naked) static void criticalFixtureExtraInitialized(){__asm {pop edi
    pop esi
    pop ebp
    pop ebx
    add esp,8
    mov eax,0x4321
    ret}}
static const uint8_t* g_criticalCopyDeadOne;
static const uint8_t* g_criticalCopyDeadTwo;
static int __cdecl criticalFixtureCopied(uint8_t* destination,const uint8_t* source,int mode) {
    assert(g_criticalCopyScope && g_criticalCopyScope->destination==destination);
    assert(!memcmp(destination,source,0x604)); // actual native REP MOVSD, count included
    for(int slot:{0,5,11})assert(criticalLookup(destination+slot*0x80).actor);
    if(mode==1) {
        assert(((CriticalHandlerTwo)site(0x5860a0))((void*)0x1234,source,7)==0x5678);
        assert(!criticalLookup(g_criticalCopyDeadTwo).actor);
        for(int slot:{0,5,11})assert(criticalLookup(destination+slot*0x80).actor);
        g_criticalCopyDeadOne=destination;
    } else {assert(mode==2);g_criticalCopyDeadTwo=destination;}
    for(int slot:{0,5,11}) {
        *(uint16_t*)(g_criticalTarget+0x18)=1000;
        assert(runCriticalCommit(g_criticalTarget,g_criticalSource,destination+slot*0x80)==-250);
    }
    assert(runCriticalCommit(g_criticalTarget,g_criticalSource,destination+11*0x80)==-250);
    return 0x5678;
}
__declspec(naked) static void criticalFixtureHandlerOne(){__asm {sub esp,0x800
    push esi
    push edi
    mov esi,[ebp+8]
    jmp dword ptr [g_criticalCopyOneEntry]}}
__declspec(naked) static void criticalFixtureHandlerTwo(){__asm {sub esp,0xc00
    push esi
    push edi
    cmp dword ptr [ebp+12],7
    jne failed
    jmp next
failed: int 3
next: mov esi,[ebp+8]
    jmp dword ptr [g_criticalCopyTwoEntry]}}
__declspec(naked) static void criticalFixtureCopiedOne(){__asm {lea eax,[esp+0x178]
    push 1
    push [ebp+8]
    push eax
    call criticalFixtureCopied
    add esp,12
    pop edi
    pop esi
    mov esp,ebp
    pop ebp
    ret 4}}
__declspec(naked) static void criticalFixtureCopiedTwo(){__asm {lea eax,[esp+0x4d8]
    push 2
    push [ebp+8]
    push eax
    call criticalFixtureCopied
    add esp,12
    pop edi
    pop esi
    mov esp,ebp
    pop ebp
    ret 8}}
#include "critical_target_telemetry_cases.inc"
#include "critical_position_owner_cases.inc"
static void criticalTelemetryTests() {
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},packet[0x80]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[24]={};vt[0x2c/4]=criticalFixtureObject;vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=criticalFixtureOwner;
    vt[0x50/4]=battleTroops;vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=criticalFixtureFind;
    *(void**)source=*(void**)target=vt;*(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
    *(int*)(target+0xc)=601;*(uint32_t*)(source+4)=0x00010001;*(uint32_t*)(target+4)=0x00010002;target[0xc9]=70;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldTroops=g_unitTroopsOriginal;auto oldProducer=g_criticalProducerOriginal;auto oldAttribute=g_criticalAttributeOriginal;
    auto oldProducerResume=g_criticalProducerResume,oldDecision=g_criticalNativeDecision,oldCallResume=g_criticalDecisionResume;
    auto oldMaximum=g_criticalMaximumResume,oldForced=g_criticalForcedResume,oldSiege=g_criticalSiegeForcedResume,oldCommit=g_criticalTroopsResume;
    g_criticalProducerResume=(uint32_t)criticalFixtureProducerProlog;
    auto producer=(CriticalProducer)site(0x5afc70);assert(producer(packet,source,0,0,0,0,0)==0x1234);
    g_criticalProducerResume=oldProducerResume;g_criticalProducerOriginal=criticalFixtureProducer;
    g_criticalNativeDecision=(uint32_t)site(0x5ae610);g_criticalDecisionResume=(uint32_t)site(0x5aff72);
    g_criticalMaximumResume=(uint32_t)site(0x495687);g_criticalAttributeOriginal=(CriticalAttribute)site(0x489030);
    g_criticalForcedResume=(uint32_t)site(0x5ae767);g_criticalSiegeForcedResume=(uint32_t)site(0x5ae77e);
    g_criticalTroopsResume=(uint32_t)site(0x584fe4);g_criticalCallEntry=site(0x5aff6d);g_criticalCommitEntry=site(0x584fdf);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;
    g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    domesticStub(0x47a630,domesticValid);domesticStub(0x490b00,personFixtureLookup);domesticStub(0x489030,criticalFixtureAttribute);
    domesticStub(0x495720,criticalFixtureOpponent);domesticStub(0x496d70,criticalFixtureArmy);domesticStub(0x495a40,criticalFixtureLeader);
    domesticStub(0x495270,criticalFixtureMember);domesticStub(0x4721d0,criticalFixtureRandom);domesticStub(0x4957b0,criticalFixtureCapacity);
    domesticStub(0x48a4f0,battleCapacity);domesticStub(0x5aff78,criticalFixtureCalculated);domesticStub(0x584fe4,criticalFixtureCommitted);
    auto terrain=(uint32_t*)site(0x5ae667);uint32_t oldTerrain=*terrain;*terrain=(uint32_t)g_criticalCells;
    auto table=(uint32_t*)site(0x5ae6cb);uint32_t oldTable=*table;uint32_t jumps[11];
    for(int i=0;i<11;++i)jumps[i]=(uint32_t)site(*(uint32_t*)site(0x5ae8d0+4*i));*table=(uint32_t)jumps;
    int effects[]={0,15,23,25,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48};bool saved[20];
    for(int i=0;i<19;++i){saved[i]=g_skills[249].effects[effects[i]];g_skills[249].effects[effects[i]]=false;}
    saved[19]=g_skills[249].effects[17];g_skills[249].effects[17]=false;
    g_criticalRecords.clear();
    auto setup=[&](int effect,int owned) {
        for(int e:effects)g_skills[249].effects[e]=false;
        if(owned==249)g_skills[249].effects[effect]=true;
        *(int*)(actor+0xe8)=owned;*(int*)(other+0xe8)=-1;actor[0xc9]=other[0xc9]=100;
        source[0xc5]=0;*(int*)(source+0xa8)=effect==35 || effect==43?2:effect==36 || effect==44?3:
            effect==25 || effect==37 || effect==45?4:effect==46?8:effect==38 || effect==47?10:1;
        *(uint32_t*)(g_criticalCells+20*201+4)=effect==15?5:0;
        *(uint32_t*)(source+4)=0x00010001;*(uint32_t*)(target+4)=0x00010002;
        g_criticalRandomResult=0;g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
    };
    auto tactic=[](int effect){return effect==15 || effect==23 || effect==25 || effect==40?-1:effect==46?14:effect==38 || effect==47?16:0;};
    assert(skilltelemetry::open(L"build/critical_telemetry_test.csv",personFixtureActor,nullptr));
    for(int effect:effects)for(int owned:{effect,249})for(int before:{0,100,1000}) {
        setup(effect,owned);*(uint16_t*)(target+0x18)=(uint16_t)before;int calls=g_criticalRandomCalls;
        assert(producer(packet,source,tactic(effect),*(uint32_t*)(target+4),0,1,0)==0x1234 && *(int*)packet==1);
        assert(g_criticalRandomCalls==calls && g_criticalRecords.size()==1);
        int loss=before<250?before:250;
        assert(runCriticalCommit(target,source,packet)==-loss && *(uint16_t*)(target+0x18)==before-loss);
    }
    auto produce=[&](int effect){setup(effect,effect);*(uint16_t*)(target+0x18)=1000;
        assert(producer(packet,source,tactic(effect),*(uint32_t*)(target+4),0,1,0)==0x1234);};
    produce(34);*(int*)(other+0xe8)=34;actor[0xc9]=90;
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);runCriticalCommit(target,source,packet); // larger second member
    produce(34);*(int*)(other+0xe8)=34;
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);runCriticalCommit(target,source,packet); // last member wins equal stat
    produce(42);*(int*)(other+0xe8)=41;
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);runCriticalCommit(target,source,packet); //41 before42
    produce(39);*(int*)(other+0xe8)=48;
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);runCriticalCommit(target,source,packet); //48 before39
    produce(34);*(int*)(actor+0xe8)=249;g_skills[249].effects[34]=g_skills[249].effects[42]=true;
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);runCriticalCommit(target,source,packet); // unconditional42 before stat34
    produce(39);actor[0xc9]=60;*(int*)(other+0xe8)=34;
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);runCriticalCommit(target,source,packet); //failed39 does not steal34 attribution
    produce(48);*(int*)(other+0xe8)=48;
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);runCriticalCommit(target,source,packet); // direct query first provider
    produce(42);runCriticalCommit(target,source,packet);runCriticalCommit(target,source,packet); // repeated actual writes both count
    produce(34);actor[0xc9]=70;source[0xc5]=80;g_criticalRandomResult=1;int calls=g_criticalRandomCalls;
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);assert(*(int*)packet==1 && g_criticalRandomCalls==calls+1);
    runCriticalCommit(target,source,packet); // natural critical after failed strict comparison, no skill attribution
    produce(25);g_criticalRandomResult=1;source[0xc5]=80;
    producer(packet,source,-1,*(uint32_t*)(target+4),0,0,0);runCriticalCommit(target,source,packet); // unranged horse attack
    produce(42);packet[0xc]++;runCriticalCommit(target,source,packet); // altered snapshot invalidates metadata
    produce(42);++*(uint32_t*)(source+4);runCriticalCommit(target,source,packet);
    produce(42);++*(uint32_t*)(target+4);runCriticalCommit(target,source,packet);
    produce(42);g_criticalSourceRegistered=false;runCriticalCommit(target,source,packet);
    produce(42);g_criticalTargetRegistered=false;runCriticalCommit(target,source,packet);
    produce(42);*(int*)(actor+0xe8)=-1;runCriticalCommit(target,source,packet);
    produce(42);g_criticalActorRegistered=false;producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);
    runCriticalCommit(target,source,packet);g_criticalActorRegistered=true;
    produce(42);uint8_t copy[0x80];memcpy(copy,packet,sizeof copy);runCriticalCommit(target,source,copy); // copy tracking explicitly pending
    produce(42);criticalTake(packet);g_criticalProducerOriginal(packet,source,0,*(uint32_t*)(target+4),0,1,0);
    runCriticalCommit(target,source,packet); // unscoped preview decision cannot authorize settlement
    // The two handler prologues and real MOV/LEA/REP sequences run. The rest
    // of each world/animation handler is replaced by a nested settlement fixture.
    uint8_t group[0x604]={},destination[0x604]={},empty[0x604]={};g_criticalRecords.clear();
    setup(42,42);*(int*)(group+0x600)=12;
    for(int slot:{0,5,11})producer(group+slot*0x80,source,0,*(uint32_t*)(target+4),0,1,0);
    auto oneResume=g_criticalHandlerOneResume,twoResume=g_criticalHandlerTwoResume;
    auto copyOneResume=g_criticalCopyOneResume,copyTwoResume=g_criticalCopyTwoResume;
    auto oneOffset=g_criticalCopyOneOffset,twoOffset=g_criticalCopyTwoOffset;
    g_criticalHandlerOneResume=(uint32_t)site(0x585786);g_criticalHandlerTwoResume=(uint32_t)site(0x5860a6);
    g_criticalCopyOneResume=(uint32_t)site(0x5857bd);g_criticalCopyTwoResume=(uint32_t)site(0x5860d5);
    g_criticalCopyOneOffset=(const int*)site(0x5857c0);g_criticalCopyTwoOffset=(const int*)site(0x5860d8);
    g_criticalCopyOneEntry=site(0x5857b8);g_criticalCopyTwoEntry=site(0x5860d0);
    domesticStub(0x585786,criticalFixtureHandlerOne);domesticStub(0x5860a6,criticalFixtureHandlerTwo);
    domesticStub(0x5857c6,criticalFixtureCopiedOne);domesticStub(0x5860de,criticalFixtureCopiedTwo);
    assert(((CriticalHandlerOne)site(0x585780))((void*)0x1234,group)==0x5678);
    assert(!g_criticalCopyScope && !criticalLookup(g_criticalCopyDeadOne).actor && !criticalLookup(g_criticalCopyDeadTwo).actor);
    assert(g_criticalRecords.size()==3); // source retained, both frames cleaned
    for(int slot:{0,5,11})assert(criticalLookup(group+slot*0x80).actor);
    criticalCopy(destination,group);assert(!criticalLookup(destination).actor); // no owner scope
    for(int count:{0,-1,13}) {
        CriticalCopyScope scope;criticalCopy(destination,group);memcpy(destination,group,sizeof group);
        assert(criticalLookup(destination).actor);
        *(int*)(empty+0x600)=count;criticalCopy(destination,empty);memcpy(destination,empty,sizeof empty);
        assert(!criticalLookup(destination).actor && !criticalLookup(destination+11*0x80).actor);
    }
    {CriticalCopyScope scope;group[5*0x80+0xc]++;criticalCopy(destination,group);memcpy(destination,group,sizeof group);
        assert(criticalLookup(destination).actor && !criticalLookup(destination+5*0x80).actor && criticalLookup(destination+11*0x80).actor);
        group[5*0x80+0xc]--;}
    assert(g_criticalRecords.size()==3 && !g_criticalCopyScope);
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/critical_lifecycle_test.csv",personFixtureActor,nullptr));
    assert(!criticalLookup(group).actor && g_criticalRecords.empty()); // prior log is invalid
    auto resetResume=g_criticalResetResume,counterResume=g_criticalCounterResetResume,extraResume=g_criticalExtraResetResume;
    g_criticalResetResume=(uint32_t)site(0x5afc35);g_criticalCounterResetResume=(uint32_t)site(0x5b0688);
    g_criticalExtraResetResume=(uint32_t)site(0x5b0ee5);g_criticalCounterInitEntry=site(0x5b06c2);
    domesticStub(0x5b0688,criticalFixtureCounterInit);domesticStub(0x5b0700,criticalFixtureCounterInitialized);
    domesticStub(0x5b0f2d,criticalFixtureExtraInitialized);
    uint32_t constantAddresses[]={0x5afc56,0x5afc5f,0x5b06e9,0x5b06f2,0x5b0f16,0x5b0f1f},constantValues[6];
    for(int i=0;i<6;++i){constantValues[i]=*(uint32_t*)site(constantAddresses[i]);
        *(uint32_t*)site(constantAddresses[i])=(uint32_t)site(0x79c1d0);}
    uint8_t expectedReset[0x80]={};
    for(int offset:{8,0x50,0x60,0x64,0x68,0x6c})*(int*)(expectedReset+offset)=-1;
    *(uint32_t*)(expectedReset+0x74)=*(uint32_t*)(expectedReset+0x78)=*(uint32_t*)site(0x79c1d0);
    *(int*)(expectedReset+0x7c)=1;
    for(uint32_t address:{0x5afc30u,0x5b0680u,0x5b0ee0u}) {
        produce(42);memcpy(copy,packet,sizeof copy);
        if(address==0x5afc30)((void(__cdecl*)(uint8_t*))site(address))(packet);
        else assert(((CriticalProducer)site(address))(packet,source,0,0,0,0,0)==0x4321);
        assert(!memcmp(packet,expectedReset,sizeof expectedReset) && !criticalLookup(packet).actor);
        memcpy(packet,copy,sizeof copy);runCriticalCommit(target,source,packet); // identical bytes cannot resurrect reset provenance
    }
    // Keep more than twice the old512-entry limit alive, then settle the oldest,
    // middle and newest records, plus both copied/nested native handler paths.
    constexpr int groups=86,totalRecords=groups*12;
    std::vector<uint8_t> pending(groups*0x604);
    setup(42,42);
    for(int n=0;n<groups;++n) {
        auto base=pending.data()+n*0x604;*(int*)(base+0x600)=12;
        for(int slot=0;slot<12;++slot)producer(base+slot*0x80,source,0,*(uint32_t*)(target+4),0,1,0);
    }
    assert(g_criticalRecords.size()==totalRecords);
    for(int n=0;n<totalRecords;++n)assert(criticalLookup(pending.data()+(n/12)*0x604+(n%12)*0x80).actor);
    for(int n:{0,512,totalRecords-1}) {
        *(uint16_t*)(target+0x18)=1000;
        assert(runCriticalCommit(target,source,pending.data()+(n/12)*0x604+(n%12)*0x80)==-250);
    }
    assert(((CriticalHandlerOne)site(0x585780))((void*)0x1234,pending.data()+(groups-1)*0x604)==0x5678);
    assert(g_criticalRecords.size()==totalRecords);
    for(int n=0;n<totalRecords;++n)((void(__cdecl*)(uint8_t*))site(0x5afc30))(pending.data()+(n/12)*0x604+(n%12)*0x80);
    assert(g_criticalRecords.empty());
    auto seed=pending.data();producer(seed,source,0,*(uint32_t*)(target+4),0,1,0);
    {
        CriticalCopyScope oldScope;criticalCopy(destination,seed);memcpy(destination,seed,0x604);
        assert(criticalLookup(destination).actor);uint64_t session=skilltelemetry::contextGeneration();
        skilltelemetry::close();assert(skilltelemetry::contextGeneration()!=session);
        assert(skilltelemetry::open(L"build/critical_session_test.csv",personFixtureActor,nullptr));
        assert(!criticalLookup(destination).actor && !criticalLookup(seed).actor && g_criticalRecords.empty());
        criticalCopy(destination,seed);assert(!criticalLookup(destination).actor); // expired scope cannot transfer
        producer(destination,source,0,*(uint32_t*)(target+4),0,1,0);
    }
    assert(criticalLookup(destination).actor); // expired old scope cannot erase new-log data
    *(uint16_t*)(target+0x18)=1000;assert(runCriticalCommit(target,source,destination)==-250);
    for(int kind:{0,1,2,3,0x16,0x17,0xc,0x1b}) {
        producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);
        uint64_t generation=skilltelemetry::contextGeneration();skillsTelemetryStream(1,kind);
        assert(skilltelemetry::enabled() && skilltelemetry::contextGeneration()!=generation && g_criticalRecords.empty());
        assert(!criticalLookup(packet).actor);runCriticalCommit(target,source,packet);
    }
    g_criticalWorldReadDuringCalculation=true;producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);
    g_criticalWorldReadDuringCalculation=false;assert(!criticalLookup(packet).actor && g_criticalRecords.empty());
    runCriticalCommit(target,source,packet); // calculation begun before world replacement cannot reinsert metadata
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);*(uint16_t*)(target+0x18)=1000;
    g_unitTroopsOriginal=(IncomeGrant)criticalFixtureTroopsWorldRead;
    assert(runCriticalCommit(target,source,packet)==-250 && *(uint16_t*)(target+0x18)==750 && g_criticalRecords.empty());
    g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0); // world replacement during grant cannot emit stale metadata
    producer(packet,source,0,*(uint32_t*)(target+4),0,1,0);
    uint64_t generation=skilltelemetry::contextGeneration();
    for(int mode:{0,2})for(int kind:{0,1,4,0x18})skillsTelemetryStream(mode,kind);
    for(int kind:{4,0x18})skillsTelemetryStream(1,kind);
    assert(skilltelemetry::contextGeneration()==generation && criticalLookup(packet).actor); // saves/master data preserve context
    *(uint16_t*)(target+0x18)=1000;assert(runCriticalCommit(target,source,packet)==-250);
    g_criticalResetResume=resetResume;g_criticalCounterResetResume=counterResume;g_criticalExtraResetResume=extraResume;
    for(int i=0;i<6;++i)*(uint32_t*)site(constantAddresses[i])=constantValues[i];
    g_criticalHandlerOneResume=oneResume;g_criticalHandlerTwoResume=twoResume;
    g_criticalCopyOneResume=copyOneResume;g_criticalCopyTwoResume=copyTwoResume;
    g_criticalCopyOneOffset=oneOffset;g_criticalCopyTwoOffset=twoOffset;
    criticalTargetTelemetryTests(source,target,actor,other,producer,effects);
    criticalOwnerTelemetryTests(source,target,actor,other,effects);
    criticalPositionOwnerTelemetryTests(source,target,actor,other,effects);
    skilltelemetry::close();assert(!g_criticalCalculation && !g_criticalProbe && !g_criticalMaximumProbe);g_criticalRecords.clear();
    for(int i=0;i<19;++i)g_skills[249].effects[effects[i]]=saved[i];g_skills[249].effects[17]=saved[19];
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;g_unitTroopsOriginal=oldTroops;
    g_criticalProducerOriginal=oldProducer;g_criticalAttributeOriginal=oldAttribute;g_criticalProducerResume=oldProducerResume;
    g_criticalNativeDecision=oldDecision;g_criticalDecisionResume=oldCallResume;g_criticalMaximumResume=oldMaximum;
    g_criticalForcedResume=oldForced;g_criticalSiegeForcedResume=oldSiege;g_criticalTroopsResume=oldCommit;*terrain=oldTerrain;*table=oldTable;
    puts("PASS native critical:19 skills, native copies/stores,1032 pending records without eviction, three native resets, identical-byte ABA exclusion and new-log/world-read invalidation");
}
