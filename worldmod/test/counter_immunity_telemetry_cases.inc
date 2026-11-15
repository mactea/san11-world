// Real two parent bodies, usercall builder/root CALL, rounded counter store,
// terrain branch, first-provider query, sampler and zero/tag stores execute.
// Root arithmetic and water/world inputs are controlled. Direct/common and
// copied/single-aggregate native troop CALLs execute with real clamp/storage.
static uintptr_t g_counterFixtureDecision,g_counterFixtureDone;
static int g_counterFixtureWater=0,g_counterFixtureDamage=125,g_counterFixtureMode=0,g_counterFixtureMutation=0;
static bool g_counterFixtureTech=true,g_counterFixtureSpawn=true,g_counterFixtureActorRegistered=true;
static const void* g_counterFixtureActor;
static int __fastcall counterFixtureWater(void* unit,void*){assert(unit==g_criticalTarget);return g_counterFixtureWater;}
static int __fastcall counterFixtureTech(void*,void*,int kind){return kind==9 && g_counterFixtureTech;}
static int __fastcall counterFixtureSpawn(void*,void*,void*,void*){return g_counterFixtureSpawn;}
static int __fastcall counterFixturePersonId(void* self,void* unused,const void* person){
    return person==g_counterFixtureActor && !g_counterFixtureActorRegistered?-1:personFixtureId(self,unused,person);
}
static bool counterFixtureActor(const void* person,skilltelemetry::Actor& actor){
    if(!personFixtureActor(person,actor))return false;
    if(person!=g_personGuide){actor.force=3;actor.city=60;}return true;
}
static int __cdecl counterFixtureVisibility(uint32_t,uint32_t){
    if(g_counterFixtureMutation==9 && g_counterReactionScope && g_counterReactionScope->scaledPacket)
        ++*(int*)(g_counterReactionScope->scaledPacket+0xc);
    return 0;
}
static int __cdecl counterFixtureApply(uint8_t* packet){
    if(g_counterFixtureMutation==1)packet[0x7c]^=1;
    if(g_counterFixtureMutation==2)*(int*)((uint8_t*)g_counterFixtureActor+0xe8)=-1;
    if(g_counterFixtureMutation==3)g_criticalSourceRegistered=false;
    if(g_counterFixtureMutation==4)g_criticalTargetRegistered=false;
    if(g_counterFixtureMutation==5)*(uint32_t*)(g_criticalTarget+4)=0x10003;
    if(g_counterFixtureMutation==6)skillsTelemetryStream(1,0);
    if(g_counterFixtureMutation==7)g_counterFixtureActorRegistered=false;
    if(g_counterFixtureMutation==8){*(int*)(g_criticalTarget+0xc)=-1;*(int*)(g_criticalTarget+0x10)=-1;}
    return ironwallFixtureApply(packet);
}
__declspec(naked) static void runCounterFixture(void* packet,void* target,int damage){__asm {push ebx
    push ebp
    push esi
    push edi
    mov ebp,[esp+20]
    mov edi,[esp+24]
    mov eax,[esp+28]
    xor esi,esi
    jmp dword ptr [g_counterFixtureDecision]}}
__declspec(naked) static void counterFixtureCalculated(){__asm {pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
static int __cdecl counterFixtureProducer(uint8_t* packet,void* source,int tactic,uint32_t position,int mode,int b,int c){
    assert(source==g_criticalSource && tactic==-1 && b==0 && c==0 && (mode==1 || mode==2));
    g_counterFixtureMode=mode;memset(packet,0,0x80);
    for(int offset:{8,0x50,0x60,0x64,0x68,0x6c})*(int*)(packet+offset)=-1;
    *(uint32_t*)(packet+0x74)=*(uint32_t*)(packet+0x78)=*(const uint32_t*)site(0x79c1d0);*(int*)(packet+0x7c)=1;
    *(uint32_t*)(packet+0x58)=*(uint32_t*)((uint8_t*)source+4);*(uint32_t*)(packet+0x5c)=position;
    runCounterFixture(packet,g_criticalTarget,g_counterFixtureDamage);return 0x1234;
}
#include "counter_vine_telemetry_cases.inc"
static void counterImmunityTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=criticalFixtureOwner;vt[0x50/4]=battleTroops;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=(void*)site(0x495600);vt[0x60/4]=counterFixtureTech;
    *(void**)source=*(void**)target=vt;*(int*)(source+0xc)=600;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldProducer=g_criticalProducerOriginal;auto oldTroops=g_unitTroopsOriginal;auto oldApply=g_counterReactionApplyOriginal;auto oldChildApply=g_ironwallApplyOriginal;
    auto oldReaction=g_counterReactionResume,oldGroup=g_counterGroupReactionResume,oldBuilder=g_counterBuilderAddress;
    auto oldWater=g_counterWaterResume,oldLand=g_counterLandResume;auto oldHandler=g_criticalHandlerTwoOriginal;
    auto oldCopy=g_criticalCopyTwoResume,oldQueued=g_criticalQueuedTroopsResume,oldAggregate=g_ironwallAggregateResume;auto oldOffset=g_criticalCopyTwoOffset;
    auto oldCombination=g_skills[249];uint8_t oldWaterCode[5],oldVisibility[5],oldSpawn[5];
    memcpy(oldWaterCode,(void*)site(0x496410),5);memcpy(oldVisibility,(void*)site(0x5a06d0),5);memcpy(oldSpawn,(void*)site(0x5a50f0),5);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)counterFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;
    g_criticalProducerOriginal=counterFixtureProducer;g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    g_counterReactionResume=(uint32_t)site(0x584c16);g_counterGroupReactionResume=(uint32_t)site(0x584d76);g_counterBuilderAddress=(uint32_t)site(0x584b10);
    g_counterWaterResume=(uint32_t)site(0x5b0336);g_counterLandResume=(uint32_t)site(0x5b0366);
    g_counterFixtureDecision=site(0x5b031d);g_counterFixtureDone=site(0x5b041c);domesticStub(0x5b041c,counterFixtureCalculated);
    g_counterReactionApplyOriginal=counterFixtureApply;g_ironwallApplyOriginal=counterFixtureApply;
    domesticStub(0x496410,counterFixtureWater);domesticStub(0x5a06d0,counterFixtureVisibility);domesticStub(0x5a50f0,counterFixtureSpawn);
    domesticStub(0x47a630,domesticValid);domesticStub(0x490b00,personFixtureLookup);domesticStub(0x491310,personFixtureId);
    domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);
    g_criticalHandlerTwoOriginal=(CriticalHandlerTwo)ironwallFixtureHandler;
    g_criticalCopyTwoResume=(uint32_t)site(0x5860d5);g_criticalCopyTwoOffset=(const int*)site(0x5860d8);g_ironwallAggregateResume=(uint32_t)site(0x586615);
    g_ironwallFixtureCopy=site(0x5860d0);g_ironwallFixtureDecision=site(0x586554);g_ironwallFixtureApply=site(0x586bab);
    g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);
    *(uint32_t*)site(0x4721dc)=(uint32_t)&g_defenseSeed;*(uint32_t*)site(0x4721ec)=(uint32_t)&g_defenseSeed;
    float scales[]={5.0f,100.0f,0.01f};uint32_t operands[]={0x584c9b,0x584ca1,0x584cab},saved[3];
    for(int i=0;i<3;++i){saved[i]=*(uint32_t*)site(operands[i]);*(uint32_t*)site(operands[i])=(uint32_t)&scales[i];}
    auto setup=[&](int mask,int provider,int water,int troops,int damage){
        memset(g_skills[249].effects,0,sizeof g_skills[249].effects);g_skills[249].effects[13]=(mask&1)!=0;g_skills[249].effects[14]=(mask&2)!=0;
        *(int*)(actor+0xe8)=*(int*)(person+0xe8)=*(int*)(other+0xe8)=*(int*)(spouse+0xe8)=-1;
        g_counterFixtureActor=provider%2?other:person;*(int*)((uint8_t*)g_counterFixtureActor+0xe8)=provider>=2 || mask==3?249:mask==1?13:14;
        *(int*)(target+0xc)=1000;*(int*)(target+0x10)=14162;*(int*)(target+0x14)=-1;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(int*)(source+0xa8)=3;*(int*)(target+0xa8)=4;
        *(int*)(source+0x24)=0;*(uint16_t*)(target+0x18)=(uint16_t)troops;g_counterFixtureWater=water;g_counterFixtureDamage=damage;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_counterFixtureActorRegistered=g_counterFixtureTech=g_counterFixtureSpawn=true;
        g_counterFixtureMutation=0;g_counterFixtureMode=0;g_ironwallFixtureMutation=0;g_ironwallFixtureRepeats=1;
        g_ironwallFixtureSkip=g_ironwallFixtureStale=false;
    };
    auto direct=(CounterReaction)site(0x584c10);auto group=(CounterGroupReaction)site(0x584d70);int extra=0x3f400000;
    auto run=[&](int route,int count,int flag,bool withExtra){
        return route?group(nullptr,target,source,flag):direct(nullptr,target,source,count,flag,withExtra?&extra:nullptr);};
    assert(skilltelemetry::open(L"build/counter_immunity_test.csv",counterFixtureActor,nullptr));int commands=0;
    for(int mask:{1,2,3})for(int provider=0;provider<4;++provider)for(int water:{0,1})for(int flag:{0,1})
        for(int route:{0,1})for(int count:{1,3})for(int optional:{0,1}){
            if(route && (count!=1 || optional))continue;
            for(int troops:{0,100,1000})for(int damage:{0,125})for(int seed:{0,4}){
                setup(mask,provider,water,troops,damage);g_defenseSeed=seed;
                bool eligible=(mask&(water?2:1))!=0;uint32_t expectedSeed=eligible?(uint32_t)seed*0x6c078965u+0x3039u:(uint32_t)seed;
                bool immune=eligible && ((expectedSeed>>16)%100)<50;int expected=immune?0:damage;
                if(!route && count>1)expected=(int)(((double)scales[1]-(double)count*scales[0])*(double)expected*scales[2]);
                int loss=troops<expected?troops:expected;int result=run(route,count,flag,optional!=0);
                assert(result==(route?-loss:1) && *(uint16_t*)(target+0x18)==troops-loss && g_counterFixtureMode==flag+1);
                assert(g_defenseSeed==expectedSeed && g_criticalRecords.empty() && !g_counterReactionScope && !g_criticalCopyScope);++commands;
            }
        }
    assert(commands==2880);
    for(int route:{0,1})for(int mutation=1;mutation<=8;++mutation){
        setup(1,0,0,1000,125);g_defenseSeed=0;g_counterFixtureMutation=mutation;
        run(route,1,0,false);
        // Native4961f0 stores zero when the leader is removed; no stale skill
        // event may claim that unrelated invalid-unit mutation as immunity.
        assert(*(uint16_t*)(target+0x18)==(mutation==8?0:1000) && g_criticalRecords.empty());
    }
    setup(1,0,0,12000,125);g_defenseSeed=0;assert(run(0,3,0,true)==1 && *(uint16_t*)(target+0x18)==10000);
    setup(3,2,0,1000,125);*(int*)(other+0xe8)=13;g_defenseSeed=0;assert(run(1,1,0,false)==0); // first matching holder
    for(int gate=0;gate<4;++gate){setup(1,0,0,1000,125);g_defenseSeed=4;
        if(gate==0)*(int*)(source+0x24)=1;if(gate==1)*(int*)(source+0xa8)=5;
        if(gate==2)g_counterFixtureTech=false;if(gate==3)g_counterFixtureSpawn=false;
        assert(run(gate<2?0:1,1,0,false)==0 && g_counterFixtureMode==0 && g_defenseSeed==4);
    }
    setup(1,0,0,1000,125);g_defenseSeed=0;g_unitTroopsOriginal=(IncomeGrant)criticalFixtureTroopsWorldRead;
    assert(run(0,1,0,false)==1 && g_criticalRecords.empty());g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    skilltelemetry::close();setup(1,0,0,1000,125);g_defenseSeed=0;assert(run(0,1,0,false)==1 && g_defenseSeed==0x3039 && g_criticalRecords.empty());
    counterVineTelemetryTests(setup,run,target);
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;
    g_criticalProducerOriginal=oldProducer;g_unitTroopsOriginal=oldTroops;g_counterReactionApplyOriginal=oldApply;g_ironwallApplyOriginal=oldChildApply;
    g_counterReactionResume=oldReaction;g_counterGroupReactionResume=oldGroup;g_counterBuilderAddress=oldBuilder;
    g_counterWaterResume=oldWater;g_counterLandResume=oldLand;g_criticalHandlerTwoOriginal=oldHandler;
    g_criticalCopyTwoResume=oldCopy;g_criticalCopyTwoOffset=oldOffset;g_criticalQueuedTroopsResume=oldQueued;g_ironwallAggregateResume=oldAggregate;
    g_skills[249]=oldCombination;for(int i=0;i<3;++i)*(uint32_t*)site(operands[i])=saved[i];
    memcpy((void*)site(0x496410),oldWaterCode,5);memcpy((void*)site(0x5a06d0),oldVisibility,5);memcpy((void*)site(0x5a50f0),oldSpawn,5);
    puts("PASS native13/14 counter immunity: both real parent frames/builders, mode1/2 water/land gates, first holder/RNG/zero stores, optional word, source/aggregate cleanup and actual troop writes");
}
