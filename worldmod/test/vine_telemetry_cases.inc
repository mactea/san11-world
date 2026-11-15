// The full native predicate/halving/coefficient tail and real troop stores run.
// Damage before this tail and the surrounding native producer remain fixtures.
static uintptr_t vineFixtureTail;
static int vineFixtureDamage=317,vineFixtureMode=0,vineFixtureForce=3,vineFixtureGrantFault=0;
static bool vineFixturePersonValid=true;
static uint8_t* vineFixturePacket;
__declspec(naked) static void __cdecl vineFixtureCalculate(void*,void*){__asm {push ebp
    push edi
    mov ebp,[esp+12]
    mov edi,[esp+16]
    jmp dword ptr [vineFixtureTail]}}
__declspec(naked) static void vineFixtureCalculated(){__asm {pop edi
    pop ebp
    ret}}
static int __fastcall vineFixtureOwner(void* p,void*){return p==g_criticalSource?2:vineFixtureForce;}
static int __fastcall vineFixturePersonId(void* w,void* x,const void* p){return !vineFixturePersonValid && p!=g_personGuide?-1:personFixtureId(w,x,p);}
static bool vineFixtureActor(const void* p,skilltelemetry::Actor& a){personFixtureActor(p,a);a.force=p==g_personGuide?2:3;return true;}
static int __cdecl vineFixtureProducer(uint8_t* packet,void*,int,uint32_t,int,int,int){
    criticalReset(packet);memset(packet,0,0x80);*(int*)(packet+0xc)=vineFixtureDamage;*(int*)(packet+8)=vineFixtureMode;
    *(uint32_t*)(packet+0x58)=*(uint32_t*)(g_criticalSource+4);*(uint32_t*)(packet+0x5c)=*(uint32_t*)(g_criticalTarget+4);
    vineFixtureCalculate(packet,g_criticalTarget);return 91;
}
static int __fastcall vineFixtureGrant(void* self,void*,void* unit,int request){
    int result=((IncomeGrant)site(0x4ae4a0))(self,unit,request);
    if(vineFixtureGrantFault==1)skilltelemetry::invalidateContext();
    if(vineFixtureGrantFault==2)++*(int*)(vineFixturePacket+0xc);
    return result;
}
static void vineTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},packet[0x80];
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=vineFixtureOwner;vt[0x50/4]=battleTroops;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=criticalFixtureFind;*(void**)source=*(void**)target=vt;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)vineFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;
    auto oldProducer=g_criticalProducerOriginal;g_criticalProducerOriginal=vineFixtureProducer;g_unitTroopsOriginal=(IncomeGrant)vineFixtureGrant;
    g_criticalTroopsResume=(uint32_t)site(0x584fe4);g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);
    g_criticalCommitEntry=site(0x584fdf);g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;
    // Earlier counter fixtures stop at041c. Restore both branch continuations
    // without hiding the no-vine jump under a fixture return adapter.
    memcpy((void*)site(0x5b0419),g_vineLayouts[0].bytes,g_vineLayouts[0].size);
    memcpy((void*)site(0x5b041f),g_vineLayouts[3].bytes,g_vineLayouts[3].size);
    vineFixtureTail=site(0x5b0404);domesticStub(0x5b0451,vineFixtureCalculated);
    domesticStub(0x584fe4,criticalFixtureCommitted);domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);
    domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x48a4f0,battleCapacity);domesticStub(0x47a630,domesticValid);
    float factors[]={.85f,.75f,.65f};uint32_t operands[]={0x5b0445,0x5b043a,0x5b042f},oldOperands[3];
    for(int n=0;n<3;++n){oldOperands[n]=*(uint32_t*)site(operands[n]);*(uint32_t*)site(operands[n])=(uint32_t)&factors[n];}
    g_skills[249].effects[31]=true;g_criticalBuildingTarget=false;
    auto setup=[&](int provider,int damage,int mode,int balance){
        g_criticalSourceRegistered=g_criticalTargetRegistered=true;vineFixturePersonValid=true;vineFixtureForce=3;vineFixtureGrantFault=0;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(int*)(source+8)=*(int*)(target+8)=0;
        *(int*)(source+0xa8)=1;*(int*)(target+0xa8)=2;*(int*)(source+0xc)=600;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
        *(int*)(target+0xc)=provider==3?14162:1000;*(int*)(target+0x10)=601;*(int*)(target+0x14)=-1;
        for(auto p:{actor,other,person,spouse}){*(int*)(p+0xe8)=-1;memset(p+0x98,0,16);}
        *(int*)((provider==3?other:provider==1?spouse:person)+0xe8)=provider==2?249:31;
        vineFixtureDamage=damage;vineFixtureMode=mode;*(uint16_t*)(target+0x18)=(uint16_t)balance;vineFixturePacket=packet;
    };
    auto produce=[&](uint8_t* p,int tactic){assert(((CriticalProducer)site(0x5afc70))(p,source,tactic,0x10002,0,0,0)==91 && !g_criticalCalculation);};
    assert(skilltelemetry::open(L"build/vine_test.csv",vineFixtureActor,nullptr));int inputs=0;
    for(int tactic:{-1,12})for(int provider=0;provider<4;++provider)for(int damage:{1,317})for(int mode:{0,3,4,5})for(int queued:{0,1})for(int balance:{100,1000}){
        setup(provider,damage,mode,balance);int random=g_criticalRandomCalls;produce(packet,tactic);
        if(queued)runCriticalQueued(target,source,packet);else runCriticalCommit(target,source,packet);
        assert(random==g_criticalRandomCalls);++inputs;
    }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/vine_absent_test.csv",vineFixtureActor,nullptr));
    assert(!memcmp((void*)site(0x5b041c),"\x8b\x45\x08",3));int absent=0;
    for(int owned:{-1,13,249})for(int damage:{0,1,317})for(int mode:{0,3,4,5})for(int queued:{0,1})for(int balance:{100,1000}){
        setup(0,damage,mode,balance);*(int*)(person+0xe8)=owned;g_skills[249].effects[31]=false;produce(packet,-1);
        assert(!criticalLookup(packet).vine.actor.person);
        double factor=mode?factors[mode-3]:1.;int expected=(int)(damage*factor);
        assert(*(int*)(packet+0xc)==expected);
        if(queued)runCriticalQueued(target,source,packet);else runCriticalCommit(target,source,packet);
        assert(*(uint16_t*)(target+0x18)==(balance<expected?0:balance-expected));++absent;
    }
    assert(absent==144);g_skills[249].effects[31]=true;skilltelemetry::close();
    assert(skilltelemetry::open(L"build/vine_edges_test.csv",vineFixtureActor,nullptr));
    for(int fault=0;fault<15;++fault){setup(0,317,0,1000);produce(packet,-1);
        if(fault==1)skilltelemetry::invalidateContext();if(fault==2)*(int*)(person+0xe8)=-1;if(fault==3)*(int*)(person+0xe8)=249;
        if(fault==4)*(int*)(target+0xc)=-1;if(fault==5)vineFixturePersonValid=false;if(fault==6)g_criticalTargetRegistered=false;
        if(fault==7)*(uint32_t*)(target+4)^=1;if(fault==8)vineFixtureForce=4;if(fault==9)*(int*)(target+0xa8)=4;
        if(fault==10)++*(int*)(person+0xa4);if(fault==11)++*(int*)(packet+0xc);if(fault==12)*(int*)(packet+0x70)=1;
        if(fault==13 || fault==14)vineFixtureGrantFault=fault-12;runCriticalCommit(target,source,packet);
    }
    setup(0,317,0,1000);g_criticalProducerOriginal(packet,source,-1,0x10002,0,0,0);runCriticalCommit(target,source,packet);
    skilltelemetry::close();assert(skilltelemetry::open(L"build/vine_copy_test.csv",vineFixtureActor,nullptr));
    uint8_t group[0x604]={},copy[0x604]={};
    for(int provider=0;provider<4;++provider){setup(provider,317,0,1000);produce(group,-1);*(int*)(group+0x600)=1;
        {CriticalCopyScope frame;criticalCopy(copy,group);memcpy(copy,group,sizeof group);assert(criticalLookup(copy).vine.actor.person);
            runCriticalQueued(target,source,copy);}
        assert(!criticalLookup(copy).vine.actor.person);runCriticalQueued(target,source,copy);
    }
    skilltelemetry::close();setup(0,317,0,1000);produce(packet,-1);runCriticalCommit(target,source,packet);
    for(int n=0;n<3;++n)*(uint32_t*)site(operands[n])=oldOperands[n];g_criticalProducerOriginal=oldProducer;
    printf("PASS vine31: %d native halving/coefficient/grant inputs, ordinary/tactic IDs, first/deputy/composed/high providers, direct/queued clamps, stale ownership and copied-packet lifetime\n",inputs);
}
