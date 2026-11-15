// Real primary apply result/mode frame, local copy/ironwall count reduction,
// aggregation RNG,18 first-holder/50% sampler/duration sampler/status CALL and
// native status/turn clamps/setter execute. Frame/SEH/world/UI and the apply
// outer body are adapters, not whole live-combat coverage.
static int g_pincerFixturePrimary=0,g_pincerFixtureMutation=0,g_pincerFixtureStatusCalls=0;
static bool g_pincerFixtureSourceValid=true,g_pincerFixtureTargetValid=true;
static int __cdecl pincerFixtureValid(void* object){return object && (object!=g_criticalSource || g_pincerFixtureSourceValid) &&
    (object!=g_criticalTarget || g_pincerFixtureTargetValid);}
static int __cdecl pincerFixtureApply(uint8_t* packet){runCriticalQueued(g_criticalTarget,g_criticalSource,packet);return g_pincerFixturePrimary;}
static void __fastcall pincerFixtureStatus(void* self,void*,uint8_t* target,void* source,int status,int turns,int display){
    ++g_pincerFixtureStatusCalls;
    if(g_pincerFixtureMutation==1)*(int*)(g_personGuide+0xe8)=-1;
    if(g_pincerFixtureMutation==2)*(int*)(g_criticalSource+0xc)=-1;
    if(g_pincerFixtureMutation==3)g_criticalSourceRegistered=false;
    if(g_pincerFixtureMutation==4)g_criticalTargetRegistered=false;
    if(g_pincerFixtureMutation==5)g_criticalActorRegistered=false;
    if(g_pincerFixtureMutation==6)skillsTelemetryStream(1,0);
    if(g_pincerFixtureMutation==7)return;
    ((StatusGrant)site(0x4aea70))(self,target,source,status,turns,display);
    if(g_pincerFixtureMutation==8)skillsTelemetryStream(1,0);
}
static void pincerTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},group[0x604]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=criticalFixtureOwner;vt[0x50/4]=battleTroops;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=(void*)site(0x495600);*(void**)source=*(void**)target=vt;
    *(int*)(target+0xc)=1000;*(int*)(target+0x10)=*(int*)(target+0x14)=-1;
    *(int*)(source+0xa8)=4;*(int*)(target+0xa8)=2;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldValid=g_growthValid;auto oldHandler=g_criticalHandlerTwoOriginal;auto oldApply=g_ironwallApplyOriginal;auto oldTroops=g_unitTroopsOriginal;
    auto oldStatus=g_pincerStatusOriginal;auto oldTurns=g_statusTurnsOriginal;bool old18=g_skills[249].effects[18];
    auto oldCopy=g_criticalCopyTwoResume,oldQueued=g_criticalQueuedTroopsResume,oldAggregate=g_ironwallAggregateResume;
    auto oldOffset=g_criticalCopyTwoOffset;auto oldFind=g_pincerFindResume,old29Find=g_ironwallFindResume,old29Limit=g_ironwallLimitResume;
    uint8_t oldGate[8],oldExit[5],oldFinish[5];memcpy(oldGate,(void*)site(0x586bb8),8);
    memcpy(oldExit,(void*)site(0x586c30),5);memcpy(oldFinish,(void*)site(0x58706f),5);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;g_growthValid=pincerFixtureValid;
    g_criticalHandlerTwoOriginal=(CriticalHandlerTwo)ironwallFixtureHandler;g_ironwallApplyOriginal=pincerFixtureApply;
    g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);g_pincerStatusOriginal=(StatusGrant)pincerFixtureStatus;g_statusTurnsOriginal=(StatusTurnsSet)site(0x496350);
    g_criticalCopyTwoResume=(uint32_t)site(0x5860d5);g_criticalCopyTwoOffset=(const int*)site(0x5860d8);
    g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);
    g_ironwallAggregateResume=(uint32_t)site(0x586615);g_ironwallFindResume=(uint32_t)site(0x586571);g_ironwallLimitResume=(uint32_t)site(0x586594);
    g_ironwallFixtureCopy=site(0x5860d0);g_ironwallFixtureDecision=site(0x586554);g_ironwallFixtureApply=site(0x586bab);
    g_ironwallFixtureStale=false;g_pincerFindResume=(uint32_t)site(0x586bf3);
    domesticStub(0x5860de,ironwallFixtureCopied);domesticStub(0x5866b7,ironwallFixtureAggregated);
    memcpy((void*)site(0x586bb8),g_doubleAttackLayouts[0].bytes,8);domesticStub(0x586c30,ironwallFixtureDone);domesticStub(0x58706f,ironwallFixtureDone);
    domesticStub(0x47a630,pincerFixtureValid);domesticStub(0x490b00,personFixtureLookup);domesticStub(0x491310,personFixtureId);
    domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);
    // Reinstall native random helpers after the earlier after-attack fixture's
    // controlled samplers, relocating only their absolute seed operands.
    const uint8_t percent[]={0x8b,0x4c,0x24,0x04,0x85,0xc9,0x7f,0x03,0x33,0xc0,0xc3,0xa1,0x44,0x5d,0x8a,0x00,0x69,0xc0,0x65,0x89,0x07,0x6c,0x05,0x39,0x30,0x00,0x00,0xa3,0x44,0x5d,0x8a,0x00,0xc1,0xe8,0x10,0x99,0x56,0xbe,0x64,0x00,0x00,0x00,0xf7,0xfe,0x33,0xc0,0x5e,0x0f,0xb7,0xd2,0x3b,0xd1,0x0f,0x9c,0xc0,0xc3};
    const uint8_t bounded[]={0x8b,0x4c,0x24,0x04,0x83,0xf9,0x02,0x7c,0x27,0x66,0x85,0xc9,0x74,0x22,0xa1,0x44,0x5d,0x8a,0x00,0x69,0xc0,0x65,0x89,0x07,0x6c,0x05,0x39,0x30,0x00,0x00,0xa3,0x44,0x5d,0x8a,0x00,0xc1,0xe8,0x10,0x0f,0xb7,0xc9,0x99,0xf7,0xf9,0x66,0x8b,0xc2,0xc3,0x66,0x33,0xc0,0xc3};
    memcpy((void*)site(0x4721d0),percent,sizeof percent);memcpy((void*)site(0x472150),bounded,sizeof bounded);
    for(int at:{0x4721dc,0x4721ec,0x47215f,0x47216f})*(uint32_t*)site(at)=(uint32_t)&g_defenseSeed;
    memcpy((void*)site(0x707a74),"\x55\x8b\xec\x83\xec",5);
    auto multiplierOperand=(uint32_t*)site(0x58669b);auto oldMultiplier=*multiplierOperand;float multiplier=0.008f;*multiplierOperand=(uint32_t)&multiplier;
    auto setup=[&](int count,int provider,int primary,int status,int before,int seed){
        *(int*)(actor+0xe8)=*(int*)(other+0xe8)=*(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        if(provider==4)*(int*)(person+0xe8)=18;
        else *(int*)((provider%2?other:actor)+0xe8)=provider>=2?249:18;
        if(provider==5){*(int*)(actor+0xe8)=249;*(int*)(other+0xe8)=18;}
        g_skills[249].effects[18]=true;*(uint16_t*)(target+0x18)=1000;*(int*)(target+0x24)=status;target[0x28]=(uint8_t)before;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
        memset(group,0,sizeof group);*(int*)(group+0x600)=count;
        for(int i=0;i<count;++i){auto packet=group+i*0x80;*(int*)(packet+0x50)=-1;
            *(uint32_t*)(packet+0x58)=0x10001;*(uint32_t*)(packet+0x5c)=0x10002;}
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        g_pincerFixturePrimary=primary;g_pincerFixtureMutation=g_pincerFixtureStatusCalls=0;g_defenseSeed=seed;
        g_pincerFixtureSourceValid=g_pincerFixtureTargetValid=true;
    };
    auto handler=(CriticalHandlerTwo)site(0x5860a0);int commands=0;
    assert(skilltelemetry::open(L"build/pincer_test.csv",assistFixtureActor,nullptr));
    for(int count:{0,1,2,6,12})for(int provider=0;provider<6;++provider)for(int mode:{0,1,2,3,4})for(int primary:{0,1})
        for(int status:{0,1,2})for(int before:{0,4,5})for(int seed:{0,3,4,9}){
            setup(count,provider,primary,status,before,seed);uint32_t state=seed;
            if(count>1)state=state*0x6c078965u+0x3039u; // native extra-record scaling
            bool eligible=count>1 && !primary && provider!=4;bool applies=false;int turns=0;
            if(eligible){state=state*0x6c078965u+0x3039u;applies=(state>>16)%100<50;
                if(applies){state=state*0x6c078965u+0x3039u;turns=1+(state>>16)%2;}}
            handler(nullptr,group,mode);assert(g_defenseSeed==state && g_pincerFixtureStatusCalls==(applies?1:0));
            assert(*(int*)(target+0x24)==(applies?1:status) && target[0x28]==(applies?(before+turns>5?5:before+turns):before));
            assert(g_criticalRecords.empty() && !g_pincer && !g_afterAttack && !g_statusWriteProbe && !g_criticalCopyScope);++commands;
        }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/pincer_edges_test.csv",assistFixtureActor,nullptr));
    for(int mutation=1;mutation<=8;++mutation){setup(2,0,0,1,5,3);g_pincerFixtureMutation=mutation;handler(nullptr,group,0);
        assert(target[0x28]==5 && g_pincerFixtureStatusCalls==1 && g_criticalRecords.empty());}
    for(int gate:{0,1}){setup(2,0,0,0,0,3);if(gate)g_pincerFixtureTargetValid=false;else g_pincerFixtureSourceValid=false;
        handler(nullptr,group,0);assert(!g_pincerFixtureStatusCalls);}
    setup(2,0,0,0,0,3);*(int*)(person+0xe8)=29;handler(nullptr,group,0);assert(!g_pincerFixtureStatusCalls && g_defenseSeed==3);
    setup(2,0,0,0,0,3);ironwallFixtureHandler(nullptr,nullptr,group,0);assert(g_pincerFixtureStatusCalls==1);
    skilltelemetry::close();
    // Execute the supported M9 cave together with the real status change. A
    // previously false-reported target becomes confused, so the five-turn
    // confusion cap must apply even with the thirty-turn setter installed.
    uint8_t setter[6];memcpy(setter,(void*)site(0x496350),6);
    auto cave=(uint8_t*)VirtualAlloc(nullptr,33,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(cave);
    const uint8_t patched[]={0x8a,0x44,0x24,0x04,0x83,0x79,0x24,0x02,0x75,0x0c,0x3c,0x1e,0x76,0x02,0xb0,0x1e,0x88,0x41,0x28,0xc2,0x04,0x00,0x8a,0x44,0x24,0x04,0x84,0xc0,0xe9};
    memcpy(cave,patched,sizeof patched);*(int32_t*)(cave+29)=(int32_t)(site(0x496356)-(uintptr_t)cave-33);
    *(uint8_t*)site(0x496350)=0xe9;*(int32_t*)site(0x496351)=(int32_t)((uintptr_t)cave-site(0x496355));*(uint8_t*)site(0x496355)=0x90;
    assert(pincerStatusSetterLayout());cave[7]^=1;assert(!pincerStatusSetterLayout());cave[7]^=1;
    cave[29]^=1;assert(!pincerStatusSetterLayout());cave[29]^=1;assert(pincerStatusSetterLayout());
    assert(skilltelemetry::open(L"build/pincer_long_false_report_test.csv",assistFixtureActor,nullptr));
    for(int status:{0,1,2})for(int before:{0,4,5})for(int seed:{3,9}){
        setup(2,0,0,status,before,seed);handler(nullptr,group,0);int turns=seed==3?1:2;
        assert(g_pincerFixtureStatusCalls==1 && *(int*)(target+0x24)==1 && target[0x28]==(before+turns>5?5:before+turns));
    }
    skilltelemetry::close();memcpy((void*)site(0x496350),setter,6);VirtualFree(cave,0,MEM_RELEASE);assert(pincerStatusSetterLayout());
    setup(2,0,0,0,0,3);handler(nullptr,group,0);assert(g_pincerFixtureStatusCalls==1 && *(int*)(target+0x24)==1);
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;g_growthValid=oldValid;
    g_criticalHandlerTwoOriginal=oldHandler;g_ironwallApplyOriginal=oldApply;g_unitTroopsOriginal=oldTroops;g_pincerStatusOriginal=oldStatus;g_statusTurnsOriginal=oldTurns;
    g_criticalCopyTwoResume=oldCopy;g_criticalCopyTwoOffset=oldOffset;g_criticalQueuedTroopsResume=oldQueued;g_ironwallAggregateResume=oldAggregate;
    g_pincerFindResume=oldFind;g_ironwallFindResume=old29Find;g_ironwallLimitResume=old29Limit;g_skills[249].effects[18]=old18;*multiplierOperand=oldMultiplier;
    memcpy((void*)site(0x586bb8),oldGate,8);memcpy((void*)site(0x586c30),oldExit,5);memcpy((void*)site(0x58706f),oldFinish,5);domesticStub(0x47a630,domesticValid);
    printf("PASS native pincer18: %d combinations, primary/count/first-holder gates, native aggregation/chance/duration RNG, actual status/turn clamp/store, modes and ironwall/stale/no-write/preview/disabled cases\n",commands);
}
