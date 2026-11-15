// Native local copy/count branch/record aggregation/extra-count scaling and
// actual apply CALL execute. World/UI dispatch and the apply routine's outer
// body are controlled; its real common troop CALL/clamp/store execute.
static uintptr_t g_ironwallFixtureCopy,g_ironwallFixtureDecision,g_ironwallFixtureApply;
static int g_ironwallFixtureMutation=0,g_ironwallFixtureRepeats=1,g_ironwallFixtureDamage=0;
static bool g_ironwallFixtureSkip=false;
static bool g_ironwallFixtureStale=false;
static void __cdecl ironwallFixtureOldAggregate(uint8_t* aggregate){
    if(!g_ironwallFixtureStale)return;
    CriticalRecord record;record.packet=aggregate;record.source=g_criticalSource;record.target=g_criticalTarget;
    record.actor=g_personTarget;record.actorId=1000;record.owned=record.effect=23;
    memcpy(record.snapshot,aggregate+0x488,sizeof record.snapshot);g_criticalRecords[(uintptr_t)aggregate]=record;
}
__declspec(naked) static int __fastcall ironwallFixtureHandler(void*,void*,const uint8_t*,int){__asm {push ebp
    mov ebp,esp
    sub esp,0x1224
    push ebx
    push esi
    push edi
    mov esi,[ebp+8]
    jmp dword ptr [g_ironwallFixtureCopy]}}
__declspec(naked) static void ironwallFixtureCopied(){__asm {pushfd
    pushad
    lea eax,[esp+0x74]
    push eax
    call ironwallFixtureOldAggregate
    add esp,4
    popad
    popfd
    mov esi,g_criticalSource
    mov ebx,g_criticalTarget
    jmp dword ptr [g_ironwallFixtureDecision]}}
__declspec(naked) static void ironwallFixtureAggregated(){__asm {jmp dword ptr [g_ironwallFixtureApply]}}
__declspec(naked) static void ironwallFixtureDone(){__asm {pop edi
    pop esi
    pop ebx
    mov esp,ebp
    pop ebp
    ret 8}}
static int __cdecl ironwallFixtureApply(uint8_t* packet){
    if(g_ironwallFixtureStale)assert(!criticalLookup(packet).actor);
    g_ironwallFixtureDamage=*(int*)(packet+0xc);
    if(g_ironwallFixtureMutation==1)g_criticalSourceRegistered=false;
    if(g_ironwallFixtureMutation==2)g_criticalTargetRegistered=false;
    if(g_ironwallFixtureMutation==3)*(int*)(g_personGuide+0xe8)=-1;
    if(g_ironwallFixtureMutation==4)*(int*)(g_criticalTarget+0xc)=-1;
    if(g_ironwallFixtureMutation==5)*(uint32_t*)(g_criticalTarget+4)=0x10003;
    if(g_ironwallFixtureMutation==6)*(int*)(packet+0x70)=1;
    if(g_ironwallFixtureMutation==7)skillsTelemetryStream(1,0);
    if(g_ironwallFixtureMutation==9)*(uint32_t*)(g_criticalSource+4)=0x10004;
    if(g_ironwallFixtureMutation==10)++*(int*)(packet+0xc);
    if(g_ironwallFixtureSkip)return 77;
    uint8_t copy[0x80];if(g_ironwallFixtureMutation==8){memcpy(copy,packet,sizeof copy);packet=copy;}
    int result=0;for(int i=0;i<g_ironwallFixtureRepeats;++i)result+=runCriticalQueued(g_criticalTarget,g_criticalSource,packet);
    return result;
}
static void ironwallTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},group[0x604]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[24]={};vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=criticalFixtureOwner;vt[0x50/4]=battleTroops;
    vt[0x5c/4]=(void*)site(0x495600);*(void**)source=*(void**)target=vt;
    *(int*)(source+0xc)=1000;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
    *(int*)(target+0xc)=600;*(int*)(target+0x10)=14162;*(int*)(target+0x14)=-1;*(int*)(target+0xa8)=4;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldHandler=g_criticalHandlerTwoOriginal;auto oldTroops=g_unitTroopsOriginal;auto oldApply=g_ironwallApplyOriginal;
    auto oldCopyResume=g_criticalCopyTwoResume,oldQueued=g_criticalQueuedTroopsResume,oldFind=g_ironwallFindResume,oldLimit=g_ironwallLimitResume;
    auto oldOffset=g_criticalCopyTwoOffset;bool old29=g_skills[249].effects[29];
    auto oldAggregate=g_ironwallAggregateResume;g_ironwallAggregateResume=(uint32_t)site(0x586615);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;
    g_criticalHandlerTwoOriginal=(CriticalHandlerTwo)ironwallFixtureHandler;g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    g_ironwallApplyOriginal=ironwallFixtureApply;g_criticalCopyTwoResume=(uint32_t)site(0x5860d5);g_criticalCopyTwoOffset=(const int*)site(0x5860d8);
    g_ironwallFindResume=(uint32_t)site(0x586571);g_ironwallLimitResume=(uint32_t)site(0x586594);
    g_ironwallFixtureCopy=site(0x5860d0);g_ironwallFixtureDecision=site(0x586554);g_ironwallFixtureApply=site(0x586bab);
    g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);
    domesticStub(0x5860de,ironwallFixtureCopied);domesticStub(0x5866b7,ironwallFixtureAggregated);domesticStub(0x586bb8,ironwallFixtureDone);
    domesticStub(0x5a06d0,defenseFixtureTerrain);domesticStub(0x47a630,domesticValid);domesticStub(0x490b00,personFixtureLookup);
    domesticStub(0x491310,personFixtureId);domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);
    memcpy((void*)site(0x707a74),"\x55\x8b\xec\x83\xec",5); // undo earlier primitive stub; execute native x87 conversion
    const uint8_t randomCode[]={0x8b,0x4c,0x24,0x04,0x83,0xf9,0x02,0x7c,0x27,0x66,0x85,0xc9,0x74,0x22,0xa1,0x44,0x5d,0x8a,0x00,0x69,0xc0,0x65,0x89,0x07,0x6c,0x05,0x39,0x30,0x00,0x00,0xa3,0x44,0x5d,0x8a,0x00,0xc1,0xe8,0x10,0x0f,0xb7,0xc9,0x99,0xf7,0xf9,0x66,0x8b,0xc2,0xc3,0x66,0x33,0xc0,0xc3};
    memcpy((void*)site(0x472150),randomCode,sizeof randomCode);*(uint32_t*)site(0x47215f)=(uint32_t)&g_defenseSeed;
    *(uint32_t*)site(0x47216f)=(uint32_t)&g_defenseSeed;
    float multiplier=0.008f;auto operand=(uint32_t*)site(0x58669b);auto oldMultiplier=*operand;*operand=(uint32_t)&multiplier;
    auto setup=[&](int count,int provider,int troops,int damage){
        *(int*)(actor+0xe8)=*(int*)(other+0xe8)=*(int*)(person+0xe8)=-1;
        if(provider>=0)*(int*)((provider%2?other:actor)+0xe8)=provider>=2?249:29;
        g_skills[249].effects[29]=true;*(uint16_t*)(target+0x18)=(uint16_t)troops;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(int*)(target+0xc)=600;
        memset(group,0,sizeof group);*(int*)(group+0x600)=count;
        for(int i=0;i<count;++i){auto packet=group+i*0x80;*(int*)(packet+0xc)=damage+i*10;
            *(int*)(packet+0x50)=-1;*(uint32_t*)(packet+0x58)=i?0x10010+i:0x10001;*(uint32_t*)(packet+0x5c)=0x10002;}
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        g_ironwallFixtureMutation=0;g_ironwallFixtureRepeats=1;g_ironwallFixtureSkip=g_ironwallFixtureStale=false;g_defenseSeed=4;
    };
    auto handler=(CriticalHandlerTwo)site(0x5860a0);
    assert(skilltelemetry::open(L"build/ironwall_test.csv",defenseFixtureActor,nullptr));
    for(int count:{2,3,6,12})for(int provider=0;provider<4;++provider)for(int troops:{0,100,1000,10000})
        for(int damage:{0,125,700})for(int mode:{0,1,2,3,4}){
            setup(count,provider,troops,damage);int loss=troops<damage?troops:damage;
            assert(handler(nullptr,group,mode)==-loss && *(uint16_t*)(target+0x18)==troops-loss);
            assert(g_ironwallFixtureDamage==damage && g_defenseSeed==4 && !g_ironwall && !g_criticalCopyScope);
            assert(*(int*)(group+0x600)==count); // only the native local copy changed
        }
    // Original non-limited aggregation/extra-count scaling and RNG still execute.
    for(int count:{0,1,2,6,12}){
        setup(count,-1,10000,125);uint32_t seed=4;int sum=125*count+5*count*(count-1),expected=sum;
        if(count>1){seed=seed*0x6c078965u+0x3039u;int factor=10*count+70+(seed>>16)%(5*count);
            expected=(int)((double)factor*(double)multiplier*(double)sum);}
        assert(handler(nullptr,group,0)==-(expected<10000?expected:10000));assert(g_ironwallFixtureDamage==expected && g_defenseSeed==seed);
    }
    setup(1,0,1000,125);assert(handler(nullptr,group,1)==-125); // single attack: no29 query/limit
    setup(6,0,1000,125);g_ironwallFixtureRepeats=2;assert(handler(nullptr,group,0)==-250); // two writes, one count event
    setup(3,0,1000,125);g_ironwallFixtureStale=true;*(int*)group=1;*(int*)(person+0xe8)=23;
    assert(handler(nullptr,group,0)==-125); // identical old stack bytes cannot resurrect unrelated critical provenance
    setup(3,2,12000,125);assert(handler(nullptr,group,0)==-2000 && *(uint16_t*)(target+0x18)==10000);
    setup(3,2,1000,125);*(int*)(other+0xe8)=29;assert(handler(nullptr,group,0)==-125); // first of two holders, not stacked
    for(int mutation=1;mutation<=9;++mutation){setup(3,0,1000,125);g_ironwallFixtureMutation=mutation;assert(handler(nullptr,group,0)==-125);}
    setup(3,0,1000,125);g_criticalActorRegistered=false;assert(handler(nullptr,group,0)==-125);
    setup(3,0,1000,125);g_ironwallFixtureSkip=true;assert(handler(nullptr,group,0)==77); // no actual resource call
    setup(3,0,1000,125);assert(ironwallFixtureHandler(nullptr,nullptr,group,0)==-125); // unscoped preview
    setup(3,0,1000,125);g_unitTroopsOriginal=(IncomeGrant)criticalFixtureTroopsWorldRead;
    assert(handler(nullptr,group,0)==-125);g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    skilltelemetry::close();setup(3,0,1000,125);assert(handler(nullptr,group,0)==-125 && g_defenseSeed==4);
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;
    g_criticalHandlerTwoOriginal=oldHandler;g_unitTroopsOriginal=oldTroops;g_ironwallApplyOriginal=oldApply;
    g_criticalCopyTwoResume=oldCopyResume;g_criticalCopyTwoOffset=oldOffset;g_criticalQueuedTroopsResume=oldQueued;
    g_ironwallFindResume=oldFind;g_ironwallLimitResume=oldLimit;g_skills[249].effects[29]=old29;*operand=oldMultiplier;
    g_ironwallAggregateResume=oldAggregate;
    puts("PASS native ironwall29: real holder/count1/local copy/aggregation/LCG, committed troop clamps, group event once, scope and stale exclusions");
}
