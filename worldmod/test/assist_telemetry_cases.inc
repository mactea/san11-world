// Execute native587170 gates/chance/roll/source-frame/constructor CALL and
// genuine child copy/single-record aggregation/resource CALL. Relationship and
// world inputs, type3 arithmetic, UI and the outer apply body are fixtures.
static uint8_t* g_assistFixturePrimary;
static uint8_t* g_assistFixtureLeader;
static bool g_assistFixturePrimaryRegistered=true,g_assistFixtureSpawn=true,g_assistFixtureSame=true,g_assistFixtureEnemy=true,g_assistFixtureBlocked=false;
static int g_assistFixtureRelations[5]={},g_assistFixtureDamage=125,g_assistFixturePackets=0,g_assistFixtureMutation=0,g_assistFixturePrimaryForce=2;
static uintptr_t g_assistFixtureTail,g_assistFixtureExit;
static int __fastcall assistFixtureId(void*,void*,void* unit){return unit==g_criticalSource?4095:unit==g_criticalTarget?4094:unit==g_assistFixturePrimary?4093:-1;}
static void* __fastcall assistFixtureUnit(void*,void*,int id){
    return id==4095 && g_criticalSourceRegistered?g_criticalSource:id==4094 && g_criticalTargetRegistered?g_criticalTarget:
        id==4093 && g_assistFixturePrimaryRegistered?g_assistFixturePrimary:nullptr;
}
static int __fastcall assistFixtureOwner(void* unit,void*){return unit==g_criticalTarget?3:unit==g_assistFixturePrimary?g_assistFixturePrimaryForce:2;}
static void* __fastcall assistFixtureLeader(void* unit,void*){return unit==g_criticalSource?g_assistFixtureLeader:unit==g_assistFixturePrimary?g_personSpouse:g_personTarget;}
static int __fastcall assistFixtureMarriage(void*,void*,int other){assert(other==601);return g_assistFixtureRelations[0];}
static int __fastcall assistFixtureSworn(void*,void*,int other){assert(other==601);return g_assistFixtureRelations[1];}
static int __fastcall assistFixtureDislike(void*,void*,int other){assert(other==601);return g_assistFixtureRelations[2];}
static int __fastcall assistFixtureFriend(void*,void*,int other){assert(other==601);return g_assistFixtureRelations[3];}
static int __fastcall assistFixtureLike(void*,void*,int other){assert(other==601);return g_assistFixtureRelations[4];}
static int __cdecl assistFixtureWeapon(int){return g_assistFixtureBlocked;}
static int __fastcall assistFixtureSame(void*,void*,void*){return g_assistFixtureSame;}
static int __fastcall assistFixtureEnemy(void*,void*,void*){return g_assistFixtureEnemy;}
static int __fastcall assistFixtureSpawn(void*,void*,void*,void*){return g_assistFixtureSpawn;}
static int __cdecl assistFixtureVisibility(uint32_t,uint32_t){
    auto packet=g_criticalCopyScope?const_cast<uint8_t*>(g_criticalCopyScope->destination):nullptr;
    if(g_assistFixtureMutation==1 && packet)packet[0x7c]^=1;
    if(g_assistFixtureMutation==2)*(int*)(g_assistFixtureLeader+0xe8)=-1;
    if(g_assistFixtureMutation==3)g_criticalSourceRegistered=false;
    if(g_assistFixtureMutation==4)g_criticalTargetRegistered=false;
    if(g_assistFixtureMutation==5)*(uint32_t*)(g_criticalTarget+4)=0x10003;
    if(g_assistFixtureMutation==6)skillsTelemetryStream(1,0);
    if(g_assistFixtureMutation==7)g_criticalActorRegistered=false;
    if(g_assistFixtureMutation==8)*(int*)(g_criticalSource+0xc)=-1;
    if(g_assistFixtureMutation==9)g_assistFixturePrimaryForce=4;
    if(g_assistFixtureMutation==10)*(int*)(g_personTarget+0xe8)=-1;
    if(g_assistFixtureMutation==12)++*(int*)(g_personTarget+0xa4);
    return 1;
}
static bool assistFixtureActor(const void* person,skilltelemetry::Actor& actor){
    if(!personFixtureActor(person,actor))return false;
    if(person==g_personTarget){actor.force=3;actor.city=60;}return true;
}
static void __cdecl assistFixturePacketData(uint8_t* packet,void*){
    ++g_assistFixturePackets;memset(packet,0,0x80);*(int*)(packet+8)=*(int*)(packet+0x50)=-1;
    *(int*)(packet+0xc)=g_assistFixtureDamage;*(int*)(packet+0x7c)=1;
}
static void(__cdecl* g_assistFixturePacketDataFn)(uint8_t*,void*)=assistFixturePacketData;
__declspec(naked) static void assistFixturePacketBody(){__asm {push ebp
    push esi
    push edi
    mov ebp,[esp+0x3c]
    push ebx
    push ebp
    call dword ptr [g_assistFixturePacketDataFn]
    add esp,8
    mov ebx,[esp+0x40]
    jmp dword ptr [g_assistFixtureTail]}}
__declspec(naked) static void assistFixtureFinished(){__asm {jmp dword ptr [g_assistFixtureExit]}}
#include "assist_vine_telemetry_cases.inc"
#include "assist_defense_telemetry_cases.inc"
#include "aggregate_telemetry_cases.inc"
static void assistTelemetryTests(){
    uint8_t source[0x110]={},primary[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},cell[20]={};
    g_criticalSource=source;g_criticalTarget=target;g_assistFixturePrimary=primary;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x2c/4]=criticalFixtureObject;vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=assistFixtureOwner;vt[0x50/4]=battleTroops;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=(void*)site(0x495600);vt[0x60/4]=defenseFixtureAptitude;
    *(void**)source=*(void**)primary=*(void**)target=vt;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldCombination=g_skills[249];auto oldHandler=g_criticalHandlerTwoOriginal;auto oldTroops=g_unitTroopsOriginal;auto oldApply=g_ironwallApplyOriginal;
    auto oldAttempt=g_assistAttemptResume,oldZero=g_assistOwnerZeroResume,oldCounter=g_criticalCounterResetResume;
    auto oldChance=g_assistChanceOriginal;auto oldHas=g_assistHasOriginal;auto oldDislike=g_assistDislikeOriginal;auto oldRoll=g_assistRollOriginal;auto oldPacket=g_assistPacketOriginal;
    auto oldChanceAddress=g_assistChanceAddress;
    auto oldCopyResume=g_criticalCopyTwoResume,oldQueued=g_criticalQueuedTroopsResume,oldAggregate=g_ironwallAggregateResume;
    auto oldIronwallFind=g_ironwallFindResume,oldIronwallLimit=g_ironwallLimitResume;
    auto oldOffset=g_criticalCopyTwoOffset;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_telemetryUnitId=(BuildingId)assistFixtureId;g_movementUnitById=(MovementUnitById)assistFixtureUnit;
    g_assistAttemptResume=(uint32_t)site(0x587176);g_assistOwnerZeroResume=(uint32_t)site(0x58724b);
    g_assistChanceOriginal=assistChanceNative;g_assistChanceAddress=(uint32_t)site(0x585510);g_assistHasOriginal=(AssistPersonCheck)site(0x4890f0);
    g_assistDislikeOriginal=(AssistPersonCheck)site(0x4889e0);g_assistRollOriginal=(AssistRoll)site(0x4721d0);g_assistPacketOriginal=(AssistPacket)site(0x5b0680);
    g_criticalCounterResetResume=(uint32_t)site(0x5b0688);g_assistFixtureTail=site(0x5b0e33);g_assistFixtureExit=site(0x587395);
    domesticStub(0x5b0688,assistFixturePacketBody);domesticStub(0x587337,assistFixtureFinished);
    domesticStub(0x47a630,domesticValid);domesticStub(0x490e70,assistFixtureUnit);domesticStub(0x490b00,personFixtureLookup);
    domesticStub(0x491310,personFixtureId);domesticStub(0x495a40,assistFixtureLeader);
    domesticStub(0x488790,assistFixtureMarriage);domesticStub(0x4887d0,assistFixtureSworn);domesticStub(0x4889e0,assistFixtureDislike);
    domesticStub(0x488910,assistFixtureFriend);domesticStub(0x48bb70,assistFixtureLike);
    domesticStub(0x5766d0,assistFixtureWeapon);domesticStub(0x495c20,assistFixtureSame);domesticStub(0x495c50,assistFixtureEnemy);
    domesticStub(0x5a50f0,assistFixtureSpawn);domesticStub(0x5a06d0,assistFixtureVisibility);
    g_criticalHandlerTwoOriginal=(CriticalHandlerTwo)ironwallFixtureHandler;g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);g_ironwallApplyOriginal=ironwallFixtureApply;
    g_criticalCopyTwoResume=(uint32_t)site(0x5860d5);g_criticalCopyTwoOffset=(const int*)site(0x5860d8);g_ironwallAggregateResume=(uint32_t)site(0x586615);
    g_ironwallFindResume=(uint32_t)site(0x586571);g_ironwallLimitResume=(uint32_t)site(0x586594);
    g_ironwallFixtureCopy=site(0x5860d0);g_ironwallFixtureDecision=site(0x586554);g_ironwallFixtureApply=site(0x586bab);
    g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);
    domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);
    *(uint32_t*)site(0x4721dc)=(uint32_t)&g_defenseSeed;*(uint32_t*)site(0x4721ec)=(uint32_t)&g_defenseSeed;
    const int relations[7][5]={{0,0,0,0,0},{1,0,0,0,0},{0,1,0,0,0},{0,0,1,1,0},{0,0,0,1,0},{0,0,0,0,1},{0,0,1,0,1}};
    auto setup=[&](int provider,int relationship,int troops,int damage){
        memset(g_skills[249].effects,0,sizeof g_skills[249].effects);g_skills[249].effects[26]=true;
        *(int*)(actor+0xe8)=*(int*)(other+0xe8)=*(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;
        g_assistFixtureLeader=provider%2 && provider<4?other:actor;
        if(provider<4)*(int*)(g_assistFixtureLeader+0xe8)=provider>=2?249:26;else *(int*)(other+0xe8)=26;
        *(int*)(source+0xc)=g_assistFixtureLeader==actor?600:14162;*(int*)(source+0x10)=g_assistFixtureLeader==actor?14162:600;*(int*)(source+0x14)=-1;
        *(int*)(target+0xc)=1000;*(int*)(target+0x10)=*(int*)(target+0x14)=-1;*(int*)(primary+0xc)=601;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(uint32_t*)(primary+4)=0x10003;
        *(int*)(source+0xa8)=1;*(int*)(target+0xa8)=4;*(int*)(source+0x24)=0;*(uint16_t*)(target+0x18)=(uint16_t)troops;
        memcpy(g_assistFixtureRelations,relations[relationship],sizeof g_assistFixtureRelations);
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=g_assistFixturePrimaryRegistered=true;
        g_assistFixtureSpawn=g_assistFixtureSame=g_assistFixtureEnemy=true;g_assistFixtureBlocked=false;g_assistFixturePrimaryForce=2;
        g_assistFixtureDamage=damage;g_assistFixtureMutation=0;g_assistFixturePackets=0;
        g_ironwallFixtureMutation=0;g_ironwallFixtureRepeats=1;g_ironwallFixtureSkip=g_ironwallFixtureStale=false;
        memset(cell,0,sizeof cell);cell[0]=1;*(uint16_t*)(cell+8)=4095;
    };
    auto attempt=(AssistAttempt)site(0x587170);
    assert(skilltelemetry::open(L"build/assist_test.csv",assistFixtureActor,nullptr));
    int commands=0;
    for(int provider=0;provider<5;++provider)for(int relationship=0;relationship<7;++relationship)
        for(int troops:{0,100,1000})for(int damage:{125,600})for(int seed:{0,4})for(int spawn:{0,1}){
            setup(provider,relationship,troops,damage);g_assistFixtureSpawn=spawn!=0;g_defenseSeed=seed;
            auto flags=relations[relationship];int chance=flags[0] || flags[1]?50:provider<4 && !flags[2]?30:flags[3]?30:flags[4] && !flags[2]?20:0;
            uint32_t expectedSeed=chance?(uint32_t)seed*0x6c078965u+0x3039u:(uint32_t)seed;
            bool selected=chance && ((expectedSeed>>16)%100)<(uint32_t)chance;bool attack=selected && spawn;
            assert(attempt(cell,0,primary,target)==0 && g_defenseSeed==expectedSeed && g_assistFixturePackets==(attack?1:0));
            int loss=attack?(troops<damage?troops:damage):0;assert(*(uint16_t*)(target+0x18)==troops-loss);
            assert(g_criticalRecords.empty() && !g_assist && !g_criticalCopyScope);++commands;
        }
    assert(commands==840);
    for(int mutation=1;mutation<=9;++mutation){setup(0,0,1000,125);g_assistFixtureMutation=mutation;g_defenseSeed=0;
        assert(attempt(cell,0,primary,target)==0 && *(uint16_t*)(target+0x18)==875 && g_criticalRecords.empty());}
    // Native gates before the chance helper do not query26 or draw a sample.
    for(int gate=0;gate<6;++gate){setup(0,0,1000,125);g_defenseSeed=4;
        if(gate==0)cell[0]=0;if(gate==1)*(uint16_t*)(cell+8)=4093;if(gate==2)*(int*)(source+0x24)=1;
        if(gate==3)g_assistFixtureBlocked=true;if(gate==4)g_assistFixtureSame=false;if(gate==5)g_assistFixtureEnemy=false;
        assert(attempt(cell,0,primary,target)==0 && g_defenseSeed==4 && !g_assistFixturePackets && *(uint16_t*)(target+0x18)==1000);}
    setup(0,0,1000,125);g_defenseSeed=0;assert(((AssistAttempt)assistAttemptNative)(cell,0,primary,target)==0);
    assert(*(uint16_t*)(target+0x18)==875 && g_criticalRecords.empty()); // unscoped preview cannot claim a skill event
    setup(0,0,1000,125);g_defenseSeed=0;g_unitTroopsOriginal=(IncomeGrant)criticalFixtureTroopsWorldRead;
    assert(attempt(cell,0,primary,target)==0 && g_criticalRecords.empty());g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    skilltelemetry::close();setup(0,0,1000,125);g_defenseSeed=0;
    assert(attempt(cell,0,primary,target)==0 && g_defenseSeed==0x3039 && *(uint16_t*)(target+0x18)==875);
    assistVineTelemetryTests(setup,attempt,cell,primary,target,source,person,other);
    assistDefenseTelemetryTests(setup,attempt,cell,primary,target,source,person,other);
    // Other existing provenances must also survive the exact one-record native
    // aggregation, and both source/copy/aggregate owners must clean up.
    auto oldProducer=g_criticalProducerOriginal;auto oldAmbush=g_ambushProducerResume;
    auto oldDefense=g_defenseNativeDecision,oldDefenseCall=g_defenseDecisionResume,old27=g_defenseUnyieldingResume,old28=g_defenseDiamondResume;
    auto oldCritical=g_criticalNativeDecision,oldCriticalCall=g_criticalDecisionResume,oldMaximum=g_criticalMaximumResume,oldForced=g_criticalForcedResume;
    auto oldAttribute=g_criticalAttributeOriginal;auto terrain=(uint32_t*)site(0x5ae667);auto oldTerrain=*terrain;
    auto multiplierOperand=(uint32_t*)site(0x58669b);auto oldMultiplier=*multiplierOperand;float multiplier=0.008f;*multiplierOperand=(uint32_t)&multiplier;
    g_defenseNativeDecision=(uint32_t)site(0x5ae000);g_defenseDecisionResume=(uint32_t)site(0x5b03bf);
    g_defenseUnyieldingResume=(uint32_t)site(0x5ae0d8);g_defenseDiamondResume=(uint32_t)site(0x5ae114);g_defenseFixtureCall=site(0x5b03b5);
    g_ambushProducerResume=(uint32_t)site(0x5b0e96);g_criticalNativeDecision=(uint32_t)site(0x5ae610);g_criticalDecisionResume=(uint32_t)site(0x5aff72);
    g_criticalMaximumResume=(uint32_t)site(0x495687);g_criticalForcedResume=(uint32_t)site(0x5ae767);g_criticalAttributeOriginal=(CriticalAttribute)site(0x489030);
    g_criticalCallEntry=site(0x5aff6d);*terrain=(uint32_t)g_criticalCells;
    domesticStub(0x5b03cd,defenseFixtureCalculated);domesticStub(0x5aff78,criticalFixtureCalculated);
    domesticStub(0x489030,criticalFixtureAttribute);domesticStub(0x495720,criticalFixtureOpponent);
    auto handler=(CriticalHandlerTwo)site(0x5860a0);auto ambush=(AmbushProducer)site(0x5b0e90);
    uint8_t group[0x604]={};assert(skilltelemetry::open(L"build/single_aggregate_test.csv",assistFixtureActor,nullptr));
    for(int critical:{0,1})for(int kind:{0,1}){
        setup(0,0,1000,125);g_defenseSeed=0;memset(group,0,sizeof group);g_defenseDamage=125;g_defenseCritical=0;g_defenseSharedCritical=false;
        *(int*)(actor+0xe8)=critical?23:-1;*(int*)(person+0xe8)=critical?-1:27;actor[0xc9]=100;target[0xc9]=70;
        g_ambushNativeCritical=critical!=0;g_criticalProducerOriginal=critical?ambushFixtureProducer:defenseFixtureProducer;
        {CriticalCopyScope owner;owner.destination=group;
            if(kind)assert(ambush(group,source,target,0)==0x1234);
            else assert(criticalProducer(group,source,-1,*(uint32_t*)(target+4),0,0,1)==0x1234);
            *(int*)(group+0x600)=1;assert(handler(nullptr,group,1)==(critical?-125:0));
            assert(g_criticalRecords.size()==1 && (critical?criticalLookup(group).actor!=nullptr:criticalLookup(group).defense.actor!=nullptr));
        }
        assert(g_criticalRecords.empty());
    }
    setup(0,0,1000,125);*(int*)(actor+0xe8)=23;actor[0xc9]=100;target[0xc9]=70;g_defenseDamage=125;g_defenseSeed=0;
    g_ambushNativeCritical=true;g_criticalProducerOriginal=ambushFixtureProducer;memset(group,0,sizeof group);
    {CriticalCopyScope owner;owner.destination=group;
        criticalProducer(group,source,-1,*(uint32_t*)(target+4),0,0,1);*(int*)(group+0x600)=2;
        *(int*)(group+0x80+0xc)=50;*(uint32_t*)(group+0x80+0x58)=0x10004;*(uint32_t*)(group+0x80+0x5c)=0x10002;
        assert(handler(nullptr,group,1)==-126); // retain the known component without inventing the unknown source
    }
    assert(g_criticalRecords.empty());skilltelemetry::close();
    aggregateTelemetryTests(setup,source,primary,target,actor,person,spouse,multiplier);
    g_criticalProducerOriginal=oldProducer;g_ambushProducerResume=oldAmbush;
    g_defenseNativeDecision=oldDefense;g_defenseDecisionResume=oldDefenseCall;g_defenseUnyieldingResume=old27;g_defenseDiamondResume=old28;
    g_criticalNativeDecision=oldCritical;g_criticalDecisionResume=oldCriticalCall;g_criticalMaximumResume=oldMaximum;g_criticalForcedResume=oldForced;
    g_criticalAttributeOriginal=oldAttribute;*terrain=oldTerrain;*multiplierOperand=oldMultiplier;g_ambushNativeCritical=false;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;
    g_skills[249]=oldCombination;g_criticalHandlerTwoOriginal=oldHandler;g_unitTroopsOriginal=oldTroops;g_ironwallApplyOriginal=oldApply;
    g_assistAttemptResume=oldAttempt;g_assistOwnerZeroResume=oldZero;g_criticalCounterResetResume=oldCounter;
    g_assistChanceOriginal=oldChance;g_assistHasOriginal=oldHas;g_assistDislikeOriginal=oldDislike;g_assistRollOriginal=oldRoll;g_assistPacketOriginal=oldPacket;
    g_assistChanceAddress=oldChanceAddress;
    g_criticalCopyTwoResume=oldCopyResume;g_criticalCopyTwoOffset=oldOffset;g_criticalQueuedTroopsResume=oldQueued;g_ironwallAggregateResume=oldAggregate;
    g_ironwallFindResume=oldIronwallFind;g_ironwallLimitResume=oldIronwallLimit;
    puts("PASS native assist26: leader/relationship/roll/validator gates, owned kind3 source, native child copy/aggregation, troop settlement and all source cleanup");
}
