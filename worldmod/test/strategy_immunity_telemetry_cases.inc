// Execute actual593402 CALL/cleanup, full5af4f0/5ae980 immunity paths and
// native4721d0 with a relocated LCG seed. World/tile/member inputs are fixtures.
static bool g_strategyEnemy=true;
static uint32_t g_strategySeed=0;
static uintptr_t g_strategyAttemptEntry;
static int __fastcall strategyFixtureEnemy(void*,void*,void*){return g_strategyEnemy;}
static void* __fastcall strategyFixtureDefender(void*,void*,int id){assert(id==4094);return g_afterTarget;}
static int __fastcall strategyFixtureMaximum(void*,void*,int effect,int stat){assert(stat==2);return effect==57?101:-1;}
static bool strategyImmunityActor(const void* person,skilltelemetry::Actor& actor){
    personFixtureActor(person,actor);if(person!=g_personGuide){actor.force=3;actor.city=60;}return true;
}
__declspec(naked) static int __cdecl runStrategyAttempt(void* unit,uint32_t from,uint32_t to,int plan,int* detail){__asm {push ebp
    push [esp+24]
    push [esp+24]
    push [esp+24]
    push [esp+24]
    push [esp+24]
    jmp dword ptr [g_strategyAttemptEntry]}}
__declspec(naked) static void strategyFixtureAttemptDone(){__asm {mov eax,ebp
    pop ebp
    ret}}
static int __cdecl strategyFixtureAttemptWorld(void* unit,uint32_t from,uint32_t to,int plan,int* detail){
    int result=((StrategyAttempt)site(0x5af4f0))(unit,from,to,plan,detail);skillsTelemetryStream(1,0);return result;
}
static int __cdecl strategyFixtureAttemptChanged(void* unit,uint32_t from,uint32_t to,int plan,int* detail){
    int result=((StrategyAttempt)site(0x5af4f0))(unit,from,to,plan,detail);*(int*)(g_personTarget+0xe8)=-1;return result;
}
#include "strategy_maximum_telemetry_cases.inc"
#include "strategy_ground_telemetry_cases.inc"
#include "strategy_critical_telemetry_cases.inc"
#include "strategy_fire_telemetry_cases.inc"
#include "strategy_fire_immunity_telemetry_cases.inc"
static void strategyImmunityTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    uint8_t tiles[20*402]={};*(int*)(tiles+20*401)=1;*(uint16_t*)(tiles+20*401+8)=4094;
    g_afterSource=source;g_afterTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[24]={};vt[0x3c/4]=afterFixturePosition;vt[0x40/4]=afterFixtureOwner;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=afterFixtureFind;*(void**)source=*(void**)target=vt;
    *(int*)(source+0xc)=600;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
    *(int*)(target+0xc)=1000;*(int*)(target+0x10)=14162;*(int*)(target+0x14)=-1;
    *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
    source[0xc6]=100;target[0xc6]=70;*(int*)(source+0xa8)=4;*(int*)(target+0xa8)=2;
    auto oldAttempt=g_strategyAttemptOriginal;auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;
    auto oldUnitId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;auto oldValid=g_growthValid;
    auto oldInsight=g_strategyInsightResume,oldDiscipline=g_strategyDisciplineResume,oldMirrorD=g_strategyMirrorDisciplineResume,
        oldCalm=g_strategyCalmResume,oldMirrorC=g_strategyMirrorCalmResume,oldFire=g_strategyFireImmunityResume;
    bool oldEffects[4];int effects[]={61,72,73,74};for(int n=0;n<4;++n){oldEffects[n]=g_skills[249].effects[effects[n]];g_skills[249].effects[effects[n]]=true;}
    g_strategyAttemptOriginal=(StrategyAttempt)site(0x5af4f0);g_strategyAttemptEntry=site(0x593402);
    g_strategyInsightResume=(uint32_t)site(0x5aea40);g_strategyDisciplineResume=(uint32_t)site(0x5aed09);
    g_strategyMirrorDisciplineResume=(uint32_t)site(0x5aed1a);g_strategyCalmResume=(uint32_t)site(0x5aee8b);
    g_strategyMirrorCalmResume=(uint32_t)site(0x5aee9c);
    g_strategyFireImmunityResume=(uint32_t)site(0x5aebb7);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)afterFixturePersonId;
    g_telemetryUnitId=(BuildingId)afterFixtureId;g_movementUnitById=(MovementUnitById)afterFixtureUnit;g_growthValid=afterFixtureValid;
    domesticStub(0x47a630,afterFixtureValid);domesticStub(0x490e70,strategyFixtureDefender);
    domesticStub(0x495c50,strategyFixtureEnemy);domesticStub(0x495680,strategyFixtureMaximum);
    domesticStub(0x59340c,strategyFixtureAttemptDone);
    *(uint32_t*)site(0x5ae9c9)=(uint32_t)tiles;*(uint32_t*)site(0x5ae9d0)=(uint32_t)tiles;
    uint32_t jumps[9];for(int n=0;n<9;++n)jumps[n]=(uint32_t)site(*(uint32_t*)site(0x5af4c4+4*n));
    *(uint32_t*)site(0x5aea5a)=(uint32_t)jumps;
    const uint8_t sampler[]={0x8b,0x4c,0x24,0x04,0x85,0xc9,0x7f,0x03,0x33,0xc0,0xc3,0xa1,0x44,0x5d,0x8a,0x00,0x69,0xc0,0x65,0x89,0x07,0x6c,0x05,0x39,0x30,0x00,0x00,0xa3,0x44,0x5d,0x8a,0x00,0xc1,0xe8,0x10,0x99,0x56,0xbe,0x64,0x00,0x00,0x00,0xf7,0xfe,0x33,0xc0,0x5e,0x0f,0xb7,0xd2,0x3b,0xd1,0x0f,0x9c,0xc0,0xc3};
    memcpy((void*)site(0x4721d0),sampler,sizeof sampler);*(uint32_t*)site(0x4721dc)=(uint32_t)&g_strategySeed;
    *(uint32_t*)site(0x4721ec)=(uint32_t)&g_strategySeed;
    auto setup=[&](int effect,int provider){
        for(int e:effects)g_skills[249].effects[e]=(e==effect || effect==249);
        *(int*)(actor+0xe8)=*(int*)(spouse+0xe8)=-1;
        *(int*)(person+0xe8)=provider==0?effect:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==1?effect:provider==2?effect:provider==3?249:-1;
        g_strategyEnemy=true;g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;g_strategySeed=123;
    };
    auto attempt=[&](int plan){int detail[2]={-1,-1};int result=runStrategyAttempt(source,0x10001,0x10002,plan,detail);
        assert(result==0 && detail[0]==0 && detail[1]==1 && g_strategySeed==123 && !g_strategyImmunity && !g_progressionProbe);};
    assert(skilltelemetry::open(L"build/strategy_immunity_test.csv",strategyImmunityActor,nullptr));
    for(int plan:{2,3})for(int effect:{61,plan==2?72:73,74})for(int provider=0;provider<4;++provider){setup(effect,provider);attempt(plan);}
    for(int plan=0;plan<8;++plan)for(int provider=0;provider<4;++provider){setup(61,provider);attempt(plan);}
    for(int plan:{2,3}){
        setup(plan==2?72:73,0);*(int*)(other+0xe8)=74;attempt(plan); // specialized immunity precedes mirror
        setup(249,0);attempt(plan); // insight precedes both specialized/mirror effects
        setup(61,0);g_strategyEnemy=false;*(int*)(other+0xe8)=74;attempt(plan); // native enemy gate fails; mirror still applies
    }
    int detail[2];
    setup(72,0);assert(g_strategyAttemptOriginal(source,0x10001,0x10002,2,detail)==0); // probability/UI call, no command scope
    setup(72,0);g_afterSourceRegistered=false;attempt(2);
    setup(72,0);g_afterTargetRegistered=false; // native lookup must still resolve target; change only telemetry registration
    attempt(2);
    setup(72,0);g_afterActorRegistered=false;attempt(2);
    setup(72,0);*(uint32_t*)(target+4)=0x10003;attempt(2);*(uint32_t*)(target+4)=0x10002;
    setup(72,0);g_strategyAttemptOriginal=strategyFixtureAttemptWorld;attempt(2);
    g_strategyAttemptOriginal=strategyFixtureAttemptChanged;setup(72,0);attempt(2);
    g_strategyAttemptOriginal=(StrategyAttempt)site(0x5af4f0);
    setup(-1,0);assert(runStrategyAttempt(source,0x10001,0x10002,2,detail)==1 && detail[1]==0 && g_strategySeed==123u*0x6c078965u+0x3039u);
    setup(61,0);g_strategyEnemy=false;assert(runStrategyAttempt(source,0x10001,0x10002,2,detail)==1 && detail[1]==0);
    setup(61,0);g_strategySeed=4;assert(runStrategyAttempt(source,0x10001,0x10002,8,detail)==0 && detail[1]==0 &&
        g_strategySeed==4u*0x6c078965u+0x3039u); // strategy8 bypasses61: natural failure is not immunity
    skilltelemetry::close();setup(72,0);attempt(2); // disabled logger preserves native result/seed
    strategyFireImmunityTelemetryTests(source,target,actor,other,person,spouse);
    strategyMaximumTelemetryTests(source,target,actor,other,person,spouse);
    strategyGroundTelemetryTests(source,actor,other,tiles);
    strategyCriticalTelemetryTests(source,target,actor,other,person,spouse,tiles);
    strategyFireTelemetryTests(source,target,actor,other,person,spouse,tiles);
    g_strategyAttemptOriginal=oldAttempt;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;
    g_telemetryUnitId=oldUnitId;g_movementUnitById=oldUnit;g_growthValid=oldValid;
    g_strategyInsightResume=oldInsight;g_strategyDisciplineResume=oldDiscipline;g_strategyMirrorDisciplineResume=oldMirrorD;
    g_strategyCalmResume=oldCalm;g_strategyMirrorCalmResume=oldMirrorC;
    g_strategyFireImmunityResume=oldFire;
    for(int n=0;n<4;++n)g_skills[249].effects[effects[n]]=oldEffects[n];
    puts("PASS native strategy immunity61/72/73/74: real probability branches, command CALL, provider precedence, zero-chance no RNG, previews/context/identity exclusions");
}
