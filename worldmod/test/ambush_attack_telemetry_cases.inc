// Full native5b0e90 construction, including its real5afc70 CALL, energy/type
// writes and return. Enclosing battle arithmetic remains a controlled fixture;
// native defense, selected critical decisions, RNG and troop writes execute.
static bool g_ambushNativeCritical=false;
static int __cdecl ambushFixtureProducer(uint8_t* packet,void* source,int tactic,uint32_t position,int a,int flag,int last){
    assert(tactic==-1 && a==0 && last==1);
    if(!g_ambushNativeCritical)return defenseFixtureProducer(packet,source,tactic,position,a,flag,last);
    memset(packet,0,0x80);*(int*)(packet+0xc)=g_defenseDamage;*(int*)(packet+0x64)=-1;
    *(uint32_t*)(packet+0x58)=*(uint32_t*)((uint8_t*)source+4);*(uint32_t*)(packet+0x5c)=position;
    if(flag)*(int*)packet=1;
    else runCriticalCall(packet,source,tactic,g_criticalTarget,0);
    runDefenseCall(packet,source,g_criticalTarget,tactic);return 0x1234;
}
static int __cdecl ambushFixtureCorrupt(uint8_t* packet,void* source,void* target,int flag){
    int result=((AmbushProducer)ambushProducerNative)(packet,source,target,flag);packet[0x7c]^=1;return result;
}
static void ambushAttackTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},packet[0x80]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    g_criticalBuildingTarget=false;g_defenseSharedCritical=false;
    void* vt[25]={};vt[0x2c/4]=criticalFixtureObject;vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=criticalFixtureOwner;
    vt[0x50/4]=battleTroops;vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=(void*)site(0x495600);vt[0x60/4]=defenseFixtureAptitude;
    *(void**)source=*(void**)target=vt;*(int*)(source+0xc)=1000;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
    *(int*)(target+0xc)=600;*(int*)(target+0x10)=14162;*(int*)(target+0x14)=-1;
    *(int*)(source+0xa8)=1;*(int*)(target+0xa8)=4;target[0xc9]=70;person[0xc9]=100;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldProducer=g_criticalProducerOriginal;auto oldTroops=g_unitTroopsOriginal;auto oldCombination=g_skills[249];
    auto oldNative=g_defenseNativeDecision,oldResume=g_defenseDecisionResume,oldUnyielding=g_defenseUnyieldingResume,oldDiamond=g_defenseDiamondResume;
    auto oldCommit=g_criticalTroopsResume,oldQueued=g_criticalQueuedTroopsResume,oldAmbush=g_ambushProducerResume;
    auto oldDecision=g_criticalNativeDecision,oldCall=g_criticalDecisionResume,oldMaximum=g_criticalMaximumResume,oldForced=g_criticalForcedResume;
    auto oldAttribute=g_criticalAttributeOriginal;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;
    g_criticalProducerOriginal=ambushFixtureProducer;g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);g_ambushProducerResume=(uint32_t)site(0x5b0e96);
    g_defenseNativeDecision=(uint32_t)site(0x5ae000);g_defenseDecisionResume=(uint32_t)site(0x5b03bf);
    g_defenseUnyieldingResume=(uint32_t)site(0x5ae0d8);g_defenseDiamondResume=(uint32_t)site(0x5ae114);
    g_defenseFixtureCall=site(0x5b03b5);g_criticalCommitEntry=site(0x584fdf);g_criticalTroopsResume=(uint32_t)site(0x584fe4);
    g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);
    g_criticalNativeDecision=(uint32_t)site(0x5ae610);g_criticalDecisionResume=(uint32_t)site(0x5aff72);
    g_criticalMaximumResume=(uint32_t)site(0x495687);g_criticalForcedResume=(uint32_t)site(0x5ae767);
    g_criticalAttributeOriginal=(CriticalAttribute)site(0x489030);g_criticalCallEntry=site(0x5aff6d);
    domesticStub(0x47a630,domesticValid);domesticStub(0x490b00,personFixtureLookup);domesticStub(0x491310,personFixtureId);
    domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x5b03cd,defenseFixtureCalculated);
    domesticStub(0x584fe4,criticalFixtureCommitted);domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);
    domesticStub(0x495720,criticalFixtureOpponent);domesticStub(0x489030,criticalFixtureAttribute);domesticStub(0x5aff78,criticalFixtureCalculated);
    auto terrain=(uint32_t*)site(0x5ae667);auto oldTerrain=*terrain;*terrain=(uint32_t)g_criticalCells;
    *(uint32_t*)site(0x4721dc)=(uint32_t)&g_defenseSeed;*(uint32_t*)site(0x4721ec)=(uint32_t)&g_defenseSeed;
    for(int e=0;e<LIMIT;++e)g_skills[249].effects[e]=false;
    auto setup=[&](int mask,int provider,int troops,int damage){
        *(int*)(actor+0xe8)=*(int*)(other+0xe8)=*(int*)(person+0xe8)=-1;
        g_skills[249].effects[27]=(mask&1)!=0;g_skills[249].effects[28]=(mask&2)!=0;
        g_skills[249].effects[23]=g_skills[249].effects[40]=false;
        if(mask){auto p=provider%2?other:actor;*(int*)(p+0xe8)=provider>=2 || mask==3?249:mask==1?27:28;}
        *(uint16_t*)(target+0x18)=(uint16_t)troops;g_defenseDamage=damage;g_ambushNativeCritical=false;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(int*)(target+0xc)=600;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
    };
    auto build=(AmbushProducer)site(0x5b0e90);
    auto produce=[&](int flag,int seed){g_defenseSeed=seed;g_defenseCritical=flag!=0;
        assert(build(packet,source,target,flag)==0x1234 && *(int*)(packet+0x70)==1 && *(int*)(packet+0x14)==(flag?20:10));};
    auto commit=[&](int route){return route?runCriticalQueued(target,source,packet):runCriticalCommit(target,source,packet);};
    assert(skilltelemetry::open(L"build/ambush_attack_test.csv",defenseFixtureActor,nullptr));
    int commands=0;
    for(int mask:{1,2,3})for(int provider=0;provider<4;++provider)for(int troops:{2999,3000,10000})
        for(int damage:{499,500,750})for(int flag:{0,1})for(int seed:{0,4}){
            setup(mask,provider,troops,damage);uint32_t expectedSeed=seed;int effect=-1;
            auto roll=[&](){expectedSeed=expectedSeed*0x6c078965u+0x3039u;return ((expectedSeed>>16)%100)<50;};
            if(troops<3000 && (mask&1) && roll())effect=27;
            if(effect<0 && damage<500 && (mask&2) && roll())effect=28;
            produce(flag,seed);assert(g_defenseSeed==expectedSeed && *(int*)(packet+0xc)==(effect>=0?0:damage));
            auto record=criticalLookup(packet);assert((record.defense.actor!=nullptr)==(effect>=0));
            if(effect>=0){assert(record.ambush && record.defense.actor==(provider%2?other:actor));
                assert(commit(0)==0 && commit(1)==0);}
            else assert(commit(commands%2)==-damage);
            assert(g_defenseSeed==expectedSeed);++commands;
        }
    assert(commands==432);
    setup(3,2,2999,499);produce(-7,1);assert(criticalLookup(packet).defense.effect==28 && commit(1)==0);
    // Full native23/40 critical maximum/provider decision, including simultaneous
    // defender27. Flag1 bypasses this decision as in the actual producer.
    for(int effect:{23,40})for(int owned:{effect,249})for(int mask:{0,1}){
        setup(mask,0,2999,499);*(int*)(person+0xe8)=owned;g_skills[249].effects[effect]=true;g_ambushNativeCritical=true;
        produce(0,0);auto record=criticalLookup(packet);assert(record.ambush && record.actor==person && record.effect==effect);
        assert((record.defense.actor!=nullptr)==(mask!=0));assert(commit(0)==(mask?0:-499));
        assert(g_defenseSeed==(mask?0x3039u:0u));
    }
    setup(1,0,2999,499);*(int*)(person+0xe8)=23;g_ambushNativeCritical=true;produce(1,0);
    assert(!criticalLookup(packet).actor && criticalLookup(packet).defense.actor==actor);assert(commit(1)==0);
    // Merely retagging a normal packet cannot authorize an ambush event.
    setup(1,0,2999,499);g_defenseSeed=0;g_criticalProducerOriginal=defenseFixtureProducer;
    criticalProducer(packet,source,-1,*(uint32_t*)(target+4),0,0,0);g_criticalProducerOriginal=ambushFixtureProducer;
    *(int*)(packet+0x14)=10;*(int*)(packet+0x70)=1;assert(commit(0)==0 && !criticalLookup(packet).defense.actor);
    for(int mutation=0;mutation<8;++mutation){
        setup(1,0,2999,499);produce(0,0);
        if(mutation==0)packet[0x7c]^=1;
        if(mutation==1)*(int*)(packet+0x14)=20;
        if(mutation==2)*(int*)(packet+0x70)=0;
        if(mutation==3)g_criticalTargetRegistered=false;
        if(mutation==4)g_criticalSourceRegistered=false;
        if(mutation==5)*(int*)(actor+0xe8)=-1;
        if(mutation==6)*(int*)(target+0xc)=-1;
        if(mutation==7)skillsTelemetryStream(1,0);
        assert(commit(0)==0);
    }
    setup(1,0,2999,499);g_ambushProducerOriginal=ambushFixtureCorrupt;produce(0,0);
    g_ambushProducerOriginal=(AmbushProducer)ambushProducerNative;assert(!criticalLookup(packet).defense.actor && commit(0)==0);
    setup(1,0,2999,499);g_criticalWorldReadDuringCalculation=true;produce(0,0);g_criticalWorldReadDuringCalculation=false;
    assert(!criticalLookup(packet).defense.actor && commit(0)==0);
    setup(1,0,2999,499);produce(0,0);uint8_t group[0x604]={},copy[0x604]={};
    g_defenseSeed=0;assert(build(group,source,target,1)==0x1234);*(int*)(group+0x600)=1;
    {CriticalCopyScope scope;criticalCopy(copy,group);memcpy(copy,group,sizeof group);assert(criticalLookup(copy).ambush);
        assert(runCriticalQueued(target,source,copy)==0);}
    assert(!criticalLookup(copy).defense.actor);
    setup(1,0,2999,499);produce(0,0);skilltelemetry::close();
    assert(skilltelemetry::open(L"build/ambush_attack_session_test.csv",defenseFixtureActor,nullptr));assert(commit(0)==0);
    produce(0,0);assert(commit(1)==0);skilltelemetry::close();
    setup(1,0,2999,499);produce(0,0);assert(g_defenseSeed==0x3039 && commit(0)==0);
    g_criticalRecords.clear();g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;
    g_criticalProducerOriginal=oldProducer;g_unitTroopsOriginal=oldTroops;g_skills[249]=oldCombination;
    g_defenseNativeDecision=oldNative;g_defenseDecisionResume=oldResume;g_defenseUnyieldingResume=oldUnyielding;g_defenseDiamondResume=oldDiamond;
    g_criticalTroopsResume=oldCommit;g_criticalQueuedTroopsResume=oldQueued;g_ambushProducerResume=oldAmbush;
    g_criticalNativeDecision=oldDecision;g_criticalDecisionResume=oldCall;g_criticalMaximumResume=oldMaximum;g_criticalForcedResume=oldForced;
    g_criticalAttributeOriginal=oldAttribute;*terrain=oldTerrain;
    puts("PASS native ambush packet1: full constructor, native27/28 and23/40 decisions, authorized retagging, exact RNG, both troop writes and stale exclusions");
}
