// Native primary apply result gate, modes, first-holder lookup, sampler, zero,
// root CALL, count/type writes, child CALL and child's copy/aggregate/resource
// store execute. Frame/SEH/UI/world dispatch and root damage arithmetic are
// adapters; no claim is made about their full native bodies here.
static uintptr_t g_doubleFixtureBranch;
static int g_doubleFixtureDamage=125,g_doubleFixtureRoots=0,g_doubleFixturePrimary=0,g_doubleFixtureMutation=0;
static int g_doubleFixtureReturn=0;
static int g_doubleFixtureCoeffects=0;
static bool g_doubleFixtureSourceValid=true,g_doubleFixtureEpoch=false,g_doubleFixtureSeedCritical=false;
static int __cdecl doubleFixtureValid(void* object){return object && (object!=g_criticalSource || g_doubleFixtureSourceValid);}
static void __cdecl doubleFixturePreparation(void*,void*){}
static const uint32_t* __fastcall doubleFixturePosition(void* unit,void*){
    if(g_doubleFixtureEpoch && unit==g_criticalTarget){g_doubleFixtureEpoch=false;skillsTelemetryStream(1,0);}
    return (const uint32_t*)((const uint8_t*)unit+4);
}
__declspec(naked) static void doubleFixtureCopied(){__asm {
    mov dword ptr [esp+0x24],1
    mov eax,[ebp-4]
    mov [esp+0x10],eax
    mov esi,g_criticalSource
    mov ebx,g_criticalTarget
    jmp dword ptr [g_ironwallFixtureDecision]}}
__declspec(naked) static int __fastcall doubleFixtureHandler(void*,void*,const uint8_t*,int){__asm {
    push ebp
    mov ebp,esp
    sub esp,0x1224
    push ebx
    push esi
    push edi
    mov [ebp-4],ecx
    mov esi,[ebp+8]
    jmp dword ptr [g_ironwallFixtureCopy]}}
__declspec(naked) static void doubleFixtureContinue(){__asm {jmp dword ptr [g_doubleFixtureBranch]}}
__declspec(naked) static void doubleFixtureDone(){__asm {
    mov eax,g_doubleFixtureReturn
    pop edi
    pop esi
    pop ebx
    mov esp,ebp
    pop ebp
    ret 8}}
static int __cdecl doubleFixtureProducer(uint8_t* packet,void* source,int tactic,uint32_t position,int a,int b,int c){
    assert(source==g_criticalSource && tactic==-1 && !a && !b && !c);++g_doubleFixtureRoots;
    memset(packet,0,0x80);*(int*)(packet+0xc)=g_doubleFixtureDamage;
    for(int at:{8,0x50,0x60,0x64,0x68,0x6c})*(int*)(packet+at)=-1;
    *(uint32_t*)(packet+0x58)=*(uint32_t*)(g_criticalSource+4);*(uint32_t*)(packet+0x5c)=position;
    *(int*)(packet+0x74)=*(int*)(packet+0x78)=*(int*)site(0x79c1d0);*(int*)(packet+0x7c)=1;
    if(g_doubleFixtureCoeffects&1)runCriticalCall(packet,source,tactic,g_criticalTarget,0);
    if(g_doubleFixtureCoeffects&2)runDefenseCall(packet,source,g_criticalTarget,tactic);
    // Seeded critical provenance tests the new-root/expired-owner epoch guard;
    // the native forced-critical decision has separate executable fixtures.
    if(g_doubleFixtureSeedCritical){*(int*)packet=1;g_criticalCalculation->actor=g_personOther;
        g_criticalCalculation->effect=23;g_criticalCalculation->target=g_criticalTarget;}
    return 0x1234;
}
static int __cdecl doubleFixtureApply(uint8_t* packet){
    bool extra=*(int*)(packet+0x70)==4;
    if(extra){
        if(g_doubleFixtureMutation==1)packet[0x7c]^=1;
        if(g_doubleFixtureMutation==2)*(int*)(g_personGuide+0xe8)=-1;
        if(g_doubleFixtureMutation==3)g_criticalSourceRegistered=false;
        if(g_doubleFixtureMutation==4)g_criticalTargetRegistered=false;
        if(g_doubleFixtureMutation==5)*(uint32_t*)(g_criticalTarget+4)=0x10003;
        if(g_doubleFixtureMutation==6)skillsTelemetryStream(1,0);
        if(g_doubleFixtureMutation==7)g_criticalActorRegistered=false;
        if(g_doubleFixtureMutation==8)*(int*)(g_criticalSource+0xc)=-1;
    }
    runCriticalQueued(g_criticalTarget,g_criticalSource,packet);
    if(extra && g_doubleFixtureMutation==9)g_criticalTargetRegistered=false; // dead/removed target after real grant
    return extra?0:g_doubleFixturePrimary;
}
static void doubleAttackTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},group[0x604]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x2c/4]=criticalFixtureObject;vt[0x3c/4]=doubleFixturePosition;vt[0x40/4]=criticalFixtureOwner;
    vt[0x50/4]=battleTroops;vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=(void*)site(0x495600);vt[0x60/4]=defenseFixtureAptitude;
    *(void**)source=*(void**)target=vt;*(int*)(target+0xc)=1000;*(int*)(target+0x10)=*(int*)(target+0x14)=-1;
    *(int*)(source+0xa8)=*(int*)(target+0xa8)=4;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldHandler=g_criticalHandlerTwoOriginal;auto oldProducer=g_criticalProducerOriginal;auto oldTroops=g_unitTroopsOriginal;
    auto oldApply=g_ironwallApplyOriginal;auto oldRoll=g_doubleRollOriginal;
    auto oldCopy=g_criticalCopyTwoResume,oldQueued=g_criticalQueuedTroopsResume,oldAggregate=g_ironwallAggregateResume;
    auto oldOffset=g_criticalCopyTwoOffset;bool old12=g_skills[249].effects[12];
    auto oldFind=g_doubleFindResume,oldZero=g_doubleZeroResume,oldTag=g_doubleTagResume;
    uint8_t oldGate[8],oldContinue[5],oldExit[5],oldFinish[5],oldChildFinish[5];
    memcpy(oldGate,(void*)site(0x586bb8),8);memcpy(oldContinue,(void*)site(0x586bc0),5);memcpy(oldExit,(void*)site(0x587030),5);
    memcpy(oldFinish,(void*)site(0x58706f),5);memcpy(oldChildFinish,(void*)site(0x587001),5);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;
    g_criticalHandlerTwoOriginal=(CriticalHandlerTwo)doubleFixtureHandler;g_criticalProducerOriginal=doubleFixtureProducer;
    g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);g_ironwallApplyOriginal=doubleFixtureApply;g_doubleRollOriginal=(DoubleRoll)site(0x4721d0);
    g_criticalCopyTwoResume=(uint32_t)site(0x5860d5);g_criticalCopyTwoOffset=(const int*)site(0x5860d8);
    g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);
    g_ironwallAggregateResume=(uint32_t)site(0x586615);g_ironwallFindResume=(uint32_t)site(0x586571);g_ironwallLimitResume=(uint32_t)site(0x586594);
    g_ironwallFixtureCopy=site(0x5860d0);g_ironwallFixtureDecision=site(0x586554);g_ironwallFixtureApply=site(0x586bab);
    g_doubleFindResume=(uint32_t)site(0x586ef4);g_doubleZeroResume=(uint32_t)site(0x586f27);g_doubleTagResume=(uint32_t)site(0x586f6f);
    g_doubleFixtureBranch=site(0x586ebf);
    domesticStub(0x5860de,doubleFixtureCopied);domesticStub(0x5866b7,ironwallFixtureAggregated);
    memcpy((void*)site(0x586bb8),g_doubleAttackLayouts[0].bytes,8);domesticStub(0x586bc0,doubleFixtureContinue);
    domesticStub(0x587030,doubleFixtureDone);domesticStub(0x58706f,doubleFixtureDone);domesticStub(0x587001,doubleFixtureDone);
    domesticStub(0x584680,doubleFixturePreparation);domesticStub(0x47a630,doubleFixtureValid);domesticStub(0x490b00,personFixtureLookup);
    domesticStub(0x491310,personFixtureId);domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);
    *(uint32_t*)site(0x4721dc)=(uint32_t)&g_defenseSeed;*(uint32_t*)site(0x4721ec)=(uint32_t)&g_defenseSeed;
    auto setup=[&](int provider,int troops,int damage,int primary,int seed){
        *(int*)(actor+0xe8)=*(int*)(other+0xe8)=*(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        if(provider==4)*(int*)(person+0xe8)=12;
        else *(int*)((provider%2?other:actor)+0xe8)=provider>=2?249:12;
        if(provider==5){*(int*)(actor+0xe8)=249;*(int*)(other+0xe8)=12;}
        g_skills[249].effects[12]=true;*(uint16_t*)(target+0x18)=(uint16_t)troops;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
        memset(group,0,sizeof group);*(int*)(group+0x600)=1;*(int*)(group+0xc)=25;*(int*)(group+0x50)=-1;
        *(uint32_t*)(group+0x58)=0x10001;*(uint32_t*)(group+0x5c)=0x10002;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        g_doubleFixtureDamage=damage;g_doubleFixtureRoots=0;g_doubleFixturePrimary=primary;g_defenseSeed=seed;
        g_doubleFixtureMutation=0;g_doubleFixtureSourceValid=true;g_doubleFixtureEpoch=g_doubleFixtureSeedCritical=false;g_doubleFixtureReturn=0;
        g_doubleFixtureCoeffects=0;
    };
    auto handler=(CriticalHandlerTwo)site(0x5860a0);
    assert(skilltelemetry::open(L"build/double_attack_test.csv",assistFixtureActor,nullptr));
    int commands=0;
    for(int provider=0;provider<6;++provider)for(int mode:{0,1,2,3,4})for(int primary:{0,1})
        for(int troops:{0,100,1000,12000})for(int damage:{0,125,600})for(int seed:{0,4}){
            setup(provider,troops,damage,primary,seed);uint32_t expectedSeed=seed;
            bool queried=!primary && mode!=2 && mode!=4,rolled=queried && provider!=4;
            if(rolled)expectedSeed=expectedSeed*0x6c078965u+0x3039u;
            bool extra=rolled && (expectedSeed>>16)%100<50;int expected=troops<25?0:troops-25;if(expected>10000)expected=10000;
            if(extra)expected=expected<damage?0:expected-damage;
            assert(handler(nullptr,group,mode)==0 && *(uint16_t*)(target+0x18)==expected);
            assert(g_defenseSeed==expectedSeed && g_doubleFixtureRoots==(extra?1:0));
            assert(g_criticalRecords.empty() && !g_criticalCopyScope && !g_doubleAttack && !g_ironwall);++commands;
        }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/double_attack_edges_test.csv",assistFixtureActor,nullptr));
    for(int mutation=1;mutation<=9;++mutation){setup(0,1000,125,0,0);g_doubleFixtureMutation=mutation;handler(nullptr,group,0);
        assert(*(uint16_t*)(target+0x18)==850 && g_criticalRecords.empty());}
    setup(0,1000,125,0,0);g_doubleFixtureSourceValid=false;handler(nullptr,group,0);assert(g_doubleFixtureRoots==0 && g_defenseSeed==0);
    setup(0,1000,125,0,0);g_criticalActorRegistered=false;handler(nullptr,group,0);assert(g_doubleFixtureRoots==1 && g_criticalRecords.empty());
    setup(0,1000,125,0,0);g_doubleFixtureEpoch=g_doubleFixtureSeedCritical=true;*(int*)(other+0xe8)=23;
    handler(nullptr,group,0);assert(g_doubleFixtureRoots==1 && g_criticalRecords.empty());
    setup(0,1000,125,0,0);g_doubleFixtureReturn=7;handler(nullptr,group,0);assert(g_criticalRecords.empty());
    setup(0,1000,125,0,0);doubleFixtureHandler(nullptr,nullptr,group,0);assert(g_defenseSeed==0x3039 && g_criticalRecords.empty());
    skilltelemetry::close();
    // Native forced-critical23 and defender27 must survive only the known
    // type0->4 store, local copy and single-record partial-field aggregation.
    auto oldDefense=g_defenseNativeDecision,oldDefenseCall=g_defenseDecisionResume,old27=g_defenseUnyieldingResume,old28=g_defenseDiamondResume;
    auto oldCritical=g_criticalNativeDecision,oldCriticalCall=g_criticalDecisionResume,oldMaximum=g_criticalMaximumResume,oldForced=g_criticalForcedResume;
    auto oldAttribute=g_criticalAttributeOriginal;auto terrain=(uint32_t*)site(0x5ae667);auto oldTerrain=*terrain;
    g_defenseNativeDecision=(uint32_t)site(0x5ae000);g_defenseDecisionResume=(uint32_t)site(0x5b03bf);
    g_defenseUnyieldingResume=(uint32_t)site(0x5ae0d8);g_defenseDiamondResume=(uint32_t)site(0x5ae114);g_defenseFixtureCall=site(0x5b03b5);
    g_criticalNativeDecision=(uint32_t)site(0x5ae610);g_criticalDecisionResume=(uint32_t)site(0x5aff72);
    g_criticalMaximumResume=(uint32_t)site(0x495687);g_criticalForcedResume=(uint32_t)site(0x5ae767);
    g_criticalAttributeOriginal=(CriticalAttribute)site(0x489030);g_criticalCallEntry=site(0x5aff6d);*terrain=(uint32_t)g_criticalCells;
    domesticStub(0x5b03cd,defenseFixtureCalculated);domesticStub(0x5aff78,criticalFixtureCalculated);
    domesticStub(0x489030,criticalFixtureAttribute);domesticStub(0x495720,criticalFixtureOpponent);
    assert(skilltelemetry::open(L"build/double_attack_coeffects_test.csv",assistFixtureActor,nullptr));
    for(int coeffects:{1,2,3})for(int mode:{0,1,3})for(int initialSeed:{0,3}){
        setup(0,1000,125,0,initialSeed);g_doubleFixtureCoeffects=coeffects;*(int*)(source+0xa8)=1;
        if(coeffects&1){*(int*)(other+0xe8)=23;other[0xc9]=100;target[0xc9]=70;}
        if(coeffects&2)*(int*)(person+0xe8)=27;
        handler(nullptr,group,mode);
        assert(g_doubleFixtureRoots==1 && g_criticalRecords.empty());
        uint32_t seed=initialSeed*0x6c078965u+0x3039u;if(coeffects&2)seed=seed*0x6c078965u+0x3039u;
        bool immune=(coeffects&2) && (seed>>16)%100<50;
        assert(g_defenseSeed==seed && *(uint16_t*)(target+0x18)==(immune?975:850));
    }
    g_defenseNativeDecision=oldDefense;g_defenseDecisionResume=oldDefenseCall;g_defenseUnyieldingResume=old27;g_defenseDiamondResume=old28;
    g_criticalNativeDecision=oldCritical;g_criticalDecisionResume=oldCriticalCall;g_criticalMaximumResume=oldMaximum;g_criticalForcedResume=oldForced;
    g_criticalAttributeOriginal=oldAttribute;*terrain=oldTerrain;*(int*)(source+0xa8)=4;
    skilltelemetry::close();setup(0,1000,125,0,0);handler(nullptr,group,0);
    assert(*(uint16_t*)(target+0x18)==850 && g_defenseSeed==0x3039 && g_criticalRecords.empty());
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;
    g_criticalHandlerTwoOriginal=oldHandler;g_criticalProducerOriginal=oldProducer;g_unitTroopsOriginal=oldTroops;
    g_ironwallApplyOriginal=oldApply;g_doubleRollOriginal=oldRoll;g_criticalCopyTwoResume=oldCopy;g_criticalCopyTwoOffset=oldOffset;
    g_criticalQueuedTroopsResume=oldQueued;g_ironwallAggregateResume=oldAggregate;g_skills[249].effects[12]=old12;
    g_doubleFindResume=oldFind;g_doubleZeroResume=oldZero;g_doubleTagResume=oldTag;
    memcpy((void*)site(0x586bb8),oldGate,8);memcpy((void*)site(0x586bc0),oldContinue,5);memcpy((void*)site(0x587030),oldExit,5);
    memcpy((void*)site(0x58706f),oldFinish,5);memcpy((void*)site(0x587001),oldChildFinish,5);domesticStub(0x47a630,domesticValid);
    printf("PASS native double attack12: %d combinations, primary/mode gates, first provider, unchanged LCG, real kind4 child aggregation/troop store and both owner cleanups\n",commands);
}
