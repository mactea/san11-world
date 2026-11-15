// Execute native5ae000, the real5b03ba CALL/conditional packet zeroing, native
// first-member finder/RNG, and both committed unit troop CALLs. The enclosing
// attack's damage arithmetic and world inputs are controlled fixtures.
static uintptr_t g_defenseFixtureCall;
static uint32_t g_defenseSeed;
static int g_defenseDamage=499,g_defenseCritical=0;
static bool g_defenseSharedCritical=false;
static int g_defenseAptitude=0;
static int __fastcall defenseFixtureAptitude(void*,void*,int kind){return kind==g_defenseAptitude;}
static int __cdecl defenseFixtureTerrain(uint32_t,uint32_t){return 1;}
static int __fastcall defenseFixtureMap(void*,void*,uint32_t,uint32_t){return 1;}
__declspec(naked) static void runDefenseCall(void* packet,void* source,void* target,int tactic){__asm {push ebx
    push esi
    push edi
    push ebp
    mov ebp,[esp+20]
    mov ebx,[esp+24]
    mov edi,[esp+28]
    mov eax,[esp+32]
    jmp dword ptr [g_defenseFixtureCall]}}
__declspec(naked) static void defenseFixtureCalculated(){__asm {pop ebp
    pop edi
    pop esi
    pop ebx
    ret}}
static int __cdecl defenseFixtureProducer(uint8_t* packet,void* source,int tactic,uint32_t position,int,int,int){
    memset(packet,0,0x80);*(int*)packet=g_defenseCritical;*(int*)(packet+0xc)=g_defenseDamage;
    *(int*)(packet+0x64)=-1;*(uint32_t*)(packet+0x58)=*(uint32_t*)((uint8_t*)source+4);
    *(uint32_t*)(packet+0x5c)=position;
    runDefenseCall(packet,source,g_criticalTarget,tactic);
    // Only the overlap/transport case seeds prior critical provenance. The
    // separate critical fixture exercises that native decision, not this one.
    if(g_defenseSharedCritical){g_criticalCalculation->actor=g_personTarget;g_criticalCalculation->effect=34;g_criticalCalculation->target=g_criticalTarget;}
    if(g_criticalWorldReadDuringCalculation)skillsTelemetryStream(1,0);
    return 0x1234;
}
static bool defenseFixtureActor(const void* p,skilltelemetry::Actor& actor){
    if(!personFixtureActor(p,actor))return false;
    if(p!=g_personTarget){actor.force=3;actor.city=60;}return true;
}
static void defenseTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},packet[0x80]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=criticalFixtureOwner;vt[0x50/4]=battleTroops;
    vt[0x60/4]=defenseFixtureAptitude;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=(void*)site(0x495600);
    *(void**)source=*(void**)target=vt;*(int*)(source+0xc)=1000;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
    *(int*)(target+0xc)=600;*(int*)(target+0x10)=14162;*(int*)(target+0x14)=-1;
    *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(int*)(source+0xa8)=*(int*)(target+0xa8)=4;
    *(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldProducer=g_criticalProducerOriginal;auto oldTroops=g_unitTroopsOriginal;
    auto oldNative=g_defenseNativeDecision,oldResume=g_defenseDecisionResume,oldUnyielding=g_defenseUnyieldingResume,oldDiamond=g_defenseDiamondResume;
    auto oldCommit=g_criticalTroopsResume,oldQueued=g_criticalQueuedTroopsResume;
    bool saved[2]={g_skills[249].effects[27],g_skills[249].effects[28]};
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;
    g_criticalProducerOriginal=defenseFixtureProducer;g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    g_defenseNativeDecision=(uint32_t)site(0x5ae000);g_defenseDecisionResume=(uint32_t)site(0x5b03bf);
    g_defenseUnyieldingResume=(uint32_t)site(0x5ae0d8);g_defenseDiamondResume=(uint32_t)site(0x5ae114);
    g_defenseFixtureCall=site(0x5b03b5);g_criticalCommitEntry=site(0x584fdf);g_criticalTroopsResume=(uint32_t)site(0x584fe4);
    g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);
    domesticStub(0x47a630,domesticValid);domesticStub(0x490b00,personFixtureLookup);domesticStub(0x491310,personFixtureId);
    domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x5b03cd,defenseFixtureCalculated);
    domesticStub(0x5adfc0,defenseFixtureTerrain);domesticStub(0x483db0,defenseFixtureMap);
    domesticStub(0x584fe4,criticalFixtureCommitted);domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);
    const uint8_t sampler[]={0x8b,0x4c,0x24,0x04,0x85,0xc9,0x7f,0x03,0x33,0xc0,0xc3,0xa1,0x44,0x5d,0x8a,0x00,0x69,0xc0,0x65,0x89,0x07,0x6c,0x05,0x39,0x30,0x00,0x00,0xa3,0x44,0x5d,0x8a,0x00,0xc1,0xe8,0x10,0x99,0x56,0xbe,0x64,0x00,0x00,0x00,0xf7,0xfe,0x33,0xc0,0x5e,0x0f,0xb7,0xd2,0x3b,0xd1,0x0f,0x9c,0xc0,0xc3};
    memcpy((void*)site(0x4721d0),sampler,sizeof sampler);*(uint32_t*)site(0x4721dc)=(uint32_t)&g_defenseSeed;
    *(uint32_t*)site(0x4721ec)=(uint32_t)&g_defenseSeed;
    auto setup=[&](int mask,int provider,int troops,int damage){
        *(int*)(actor+0xe8)=*(int*)(other+0xe8)=-1;g_skills[249].effects[27]=(mask&1)!=0;g_skills[249].effects[28]=(mask&2)!=0;
        auto p=provider%2?other:actor;*(int*)(p+0xe8)=provider>=2 || mask==3?249:mask==1?27:28;
        *(uint16_t*)(target+0x18)=(uint16_t)troops;g_defenseDamage=damage;g_defenseCritical=0;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
    };
    auto produce=[&](int tactic,int seed){g_defenseSeed=seed;
        assert(criticalProducer(packet,source,tactic,*(uint32_t*)(target+4),0,0,0)==0x1234);};
    auto commit=[&](int route){return route?runCriticalQueued(target,source,packet):runCriticalCommit(target,source,packet);};
    assert(skilltelemetry::open(L"build/defense_telemetry_test.csv",defenseFixtureActor,nullptr));
    int commands=0,successful=0;
    for(int mask:{1,2,3})for(int provider=0;provider<4;++provider)for(int troops:{2999,3000,10000})
        for(int damage:{499,500,750})for(int tactic:{-1,0,31,32})for(int seed:{0,4}){
            setup(mask,provider,troops,damage);g_defenseCritical=commands%2;
            uint32_t expectedSeed=seed;int effect=-1;
            auto roll=[&](){expectedSeed=expectedSeed*0x6c078965u+0x3039u;return ((expectedSeed>>16)%100)<50;};
            if(tactic<0 || tactic>31){
                if(troops<3000 && (mask&1) && roll())effect=27;
                if(effect<0 && damage<500 && (mask&2) && roll())effect=28;
            }
            produce(tactic,seed);assert(g_defenseSeed==expectedSeed);
            assert(*(int*)(packet+0xc)==(effect>=0?0:damage));assert(*(int*)(packet+0x64)==effect);
            auto record=criticalLookup(packet);assert((record.defense.actor!=nullptr)==(effect>=0));
            if(effect>=0){++successful;assert(record.defense.actor==(provider%2?other:actor));
                assert(record.defense.effect==effect && record.defense.incomingDamage==damage);}
            int loss=effect>=0?0:damage;assert(commit(commands%2)==-loss && *(uint16_t*)(target+0x18)==troops-loss);
            if(effect>=0)assert(commit(1)==0); // both native resource CALLs for every holder/threshold case
            assert(g_defenseSeed==expectedSeed);++commands;
        }
    assert(commands==864 && successful>0);
    for(int aptitude:{5,6}){
        setup(3,2,2999,499);g_defenseAptitude=aptitude;*(int*)(target+0xa8)=2;produce(-1,0);
        assert(*(int*)(packet+0xc)==0 && *(int*)(packet+0x6c)==aptitude && !criticalLookup(packet).defense.actor);
        assert(g_defenseSeed==0x3039 && commit(0)==0);
    }
    g_defenseAptitude=0;*(int*)(target+0xa8)=4;
    // Native first matching holder, not strongest/last/stacked. Include zero
    // damage/troops, a failed27 roll followed by28 success, and repeat settlement.
    setup(3,2,0,0);*(int*)(other+0xe8)=27;produce(-1,0);assert(criticalLookup(packet).defense.actor==actor);assert(commit(0)==0);
    setup(3,2,2999,499);produce(-1,1);assert(*(int*)(packet+0xc)==0 && criticalLookup(packet).defense.effect==28);
    assert(commit(1)==0);assert(commit(0)==0);
    setup(2,3,12000,499);produce(-1,0);assert(commit(1)==-2000 && *(uint16_t*)(target+0x18)==10000); // native cap still applies to a zero debit
    // Direct query/preview outside a producer may match, never gets provenance.
    setup(1,0,2999,499);g_defenseSeed=0;memset(packet,0,sizeof packet);*(int*)(packet+0xc)=499;
    runDefenseCall(packet,source,target,-1);assert(*(int*)(packet+0xc)==0 && !criticalLookup(packet).defense.actor);assert(commit(0)==0);
    // Revalidate source, target, member, owned skill, position and full packet.
    for(int mutation=0;mutation<8;++mutation){
        setup(1,0,2999,499);produce(-1,0);
        if(mutation==0)g_criticalSourceRegistered=false;
        if(mutation==1)g_criticalTargetRegistered=false;
        if(mutation==2)g_criticalActorRegistered=false;
        if(mutation==3)*(int*)(actor+0xe8)=-1;
        if(mutation==4)*(int*)(target+0xc)=-1;
        if(mutation==5)*(uint32_t*)(target+4)=0x10003;
        if(mutation==6)packet[0x7c]^=1;
        if(mutation==7)skillsTelemetryStream(1,0);
        assert(commit(0)==0);*(int*)(target+0xc)=600;
    }
    setup(1,0,2999,499);produce(-1,0);auto generation=skilltelemetry::contextGeneration();
    g_unitTroopsOriginal=(IncomeGrant)criticalFixtureTroopsWorldRead;assert(commit(0)==0);
    assert(generation!=skilltelemetry::contextGeneration());g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    setup(1,0,2999,499);g_criticalWorldReadDuringCalculation=true;produce(-1,0);g_criticalWorldReadDuringCalculation=false;
    assert(!criticalLookup(packet).defense.actor && commit(0)==0);
    setup(2,3,10000,499);produce(-1,0);
    uint8_t group[0x604]={},copy[0x604]={};
    // Produce directly in a proper owner buffer, then transfer a defense-only row.
    g_defenseSeed=0;criticalProducer(group,source,-1,*(uint32_t*)(target+4),0,0,0);*(int*)(group+0x600)=1;
    {CriticalCopyScope scope;criticalCopy(copy,group);memcpy(copy,group,sizeof group);
        assert(criticalLookup(copy).defense.actor==other);assert(runCriticalQueued(target,source,copy)==0);}
    assert(!criticalLookup(copy).defense.actor);
    setup(1,0,2999,499);*(int*)(person+0xe8)=34;g_defenseSharedCritical=true;g_defenseCritical=1;produce(-1,0);
    assert(criticalLookup(packet).actor==person && criticalLookup(packet).defense.actor==actor);assert(commit(1)==0);
    g_defenseSharedCritical=false;*(int*)(person+0xe8)=-1;
    // Packet replacement and read/reopen cannot revive the old immunity.
    setup(1,0,2999,499);produce(-1,0);g_defenseSeed=4;criticalProducer(packet,source,-1,*(uint32_t*)(target+4),0,0,0);
    assert(!criticalLookup(packet).defense.actor && commit(0)==-499);
    setup(1,0,2999,499);produce(-1,0);skilltelemetry::close();
    assert(skilltelemetry::open(L"build/defense_session_test.csv",defenseFixtureActor,nullptr));assert(commit(0)==0);
    produce(-1,0);assert(commit(1)==0);skilltelemetry::close();
    setup(1,0,2999,499);produce(-1,0);assert(g_defenseSeed==0x3039 && commit(0)==0);
    assert(!g_defenseScope && !g_criticalCalculation);
    g_criticalRecords.clear();g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;
    g_criticalProducerOriginal=oldProducer;g_unitTroopsOriginal=oldTroops;g_defenseNativeDecision=oldNative;g_defenseDecisionResume=oldResume;
    g_defenseUnyieldingResume=oldUnyielding;g_defenseDiamondResume=oldDiamond;g_criticalTroopsResume=oldCommit;g_criticalQueuedTroopsResume=oldQueued;
    g_skills[249].effects[27]=saved[0];g_skills[249].effects[28]=saved[1];
    puts("PASS defense telemetry: native27/28 gates, first holder, exact RNG, packet zeroing, both troop settlements, lifetime exclusions");
}
