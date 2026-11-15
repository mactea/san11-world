static uint8_t* g_strategyGroundFixtureBase=nullptr;
static uint8_t* g_strategyGroundFixtureCell=nullptr;
static bool g_strategyGroundBaseRegistered=true;
static int g_strategyGroundFault=0,g_strategyGroundForce=3;
static int __fastcall strategyGroundBaseId(void*,void*,void* p){return p==g_strategyGroundFixtureBase?529:-1;}
static void* __fastcall strategyGroundBaseLookup(void*,void*,int id){return g_strategyGroundBaseRegistered && id==529?g_strategyGroundFixtureBase:nullptr;}
static void* __fastcall strategyGroundCellBase(void* cell,void*){return cell==g_strategyGroundFixtureCell?g_strategyGroundFixtureBase:nullptr;}
static int __fastcall strategyGroundBaseForce(void*,void*){return g_strategyGroundForce;}
static int __cdecl strategyGroundAttemptChanged(void* source,uint32_t from,uint32_t to,int plan,int* detail){
    int result=((StrategyAttempt)site(0x5af4f0))(source,from,to,plan,detail);
    switch(g_strategyGroundFault){
    case 1:skilltelemetry::invalidateContext();break;
    case 2:*(int*)(g_personGuide+0xe8)=-1;break;
    case 3:g_afterActorRegistered=false;break;
    case 4:g_afterSourceRegistered=false;break;
    case 5:++*(uint32_t*)(g_strategyGroundFixtureCell+4);break;
    case 6:++g_strategyGroundForce;break;
    case 7:g_strategyGroundBaseRegistered=false;break;
    case 8:*(int*)((uint8_t*)source+0xc)=-1;break;
    case 9:++*(int*)(g_personGuide+0x9c);break;
    }
    return result;
}
static void strategyGroundTelemetryTests(uint8_t* source,uint8_t* actor,uint8_t* other,uint8_t* tiles){
    uint8_t base[0x120]={};void* vt[24]={};vt[0x40/4]=strategyGroundBaseForce;*(void**)base=vt;
    g_strategyGroundFixtureBase=base;g_strategyGroundFixtureCell=tiles+20*401;
    uint8_t savedCell[20];memcpy(savedCell,g_strategyGroundFixtureCell,20);
    auto oldSkill=g_skills[249];auto oldBase=g_baseById;auto oldId=g_telemetryBaseId;
    auto oldCellBase=g_strategyGroundBase;g_strategyGroundBase=(CityBuilding)site(0x483b20);
    auto oldMaximum=g_criticalMaximumResume;auto oldAttribute=g_criticalAttributeOriginal;auto oldAttempt=g_strategyAttemptOriginal;
    auto oldCharm=g_strategyCharmHasResume;g_strategyCharmHasResume=(uint32_t)site(0x5af480);
    uint8_t savedMap[5];memcpy(savedMap,(void*)site(0x483b20),5);domesticStub(0x483b20,strategyGroundCellBase);
    g_baseById=(BaseById)strategyGroundBaseLookup;g_telemetryBaseId=(BuildingId)strategyGroundBaseId;
    g_criticalMaximumResume=(uint32_t)site(0x495687);g_criticalAttributeOriginal=(CriticalAttribute)site(0x489030);
    domesticStub(0x495680,criticalMaximum);domesticStub(0x489030,strategyFixtureIntelligence);
    domesticStub(0x490b00,personFixtureLookup);
    g_strategyAttemptOriginal=strategyGroundAttemptChanged;
    auto floatOperand1=(uint32_t*)site(0x5aeca8),floatOperand2=(uint32_t*)site(0x5aecd7);
    uint32_t savedFloat1=*floatOperand1,savedFloat2=*floatOperand2;
    *floatOperand1=*floatOperand2=(uint32_t)site(0x838d34);
    auto setup=[&](int effect,int provider,int kind,int fault){
        memset(g_skills[249].effects,0,sizeof g_skills[249].effects);g_skills[249].effects[effect]=true;
        *(int*)(actor+0xe8)=provider==0 || provider==4 || provider==5?effect:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==1 || provider==2 || provider==4?effect:provider==3 || provider==5?249:-1;
        actor[0xc9]=provider==5?120:110;other[0xc9]=provider==2?80:110;memset(actor+0x98,0,16);
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;source[0xc6]=100;
        memset(g_strategyGroundFixtureCell,0,20);*(int*)g_strategyGroundFixtureCell=kind?2:0;
        *(int*)(g_strategyGroundFixtureCell+4)=5;*(int*)(base+8)=kind==1?0:kind==2?1:kind==3?2:23;
        g_strategyGroundForce=3;g_strategyGroundBaseRegistered=true;g_strategyGroundFault=fault;
        g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;g_strategySeed=123;
    };
    auto attempt=[&]{int detail[2]={-1,-1};int result=runStrategyAttempt(source,0x10001,0x10002,1,detail);
        assert(result==1 && detail[0]==0 && detail[1]==0 && g_strategySeed==123u*0x6c078965u+0x3039u);
        assert(!g_strategyImmunity && !g_criticalMaximumProbe && !g_progressionProbe);
    };
    assert(skilltelemetry::open(L"build/strategy_ground_test.csv",strategyMaximumActor,nullptr));
    for(int effect:{53,57,62,63})for(int provider=0;provider<6;++provider)for(int kind=0;kind<5;++kind){setup(effect,provider,kind,0);attempt();}
    for(int effect:{53,57,62,63})for(int fault=1;fault<=9;++fault){setup(effect,0,4,fault);attempt();}
    for(int effect:{53,57,62,63}){
        setup(effect,0,0,0);int detail[2];
        assert(((StrategyAttempt)site(0x5af4f0))(source,0x10001,0x10002,1,detail)==1); // preview
        setup(effect,0,0,0);actor[0xc9]=0;
        runStrategyAttempt(source,0x10001,0x10002,1,detail); // strict0>0 comparison loses
        assert(g_strategySeed==123u*0x6c078965u+0x3039u);
    }
    skilltelemetry::close();
    for(int effect:{53,57,62,63})for(int kind=0;kind<5;++kind){setup(effect,2,kind,0);attempt();}
    memcpy(g_strategyGroundFixtureCell,savedCell,20);memcpy((void*)site(0x483b20),savedMap,5);
    g_skills[249]=oldSkill;g_baseById=oldBase;g_telemetryBaseId=oldId;
    g_criticalMaximumResume=oldMaximum;g_criticalAttributeOriginal=oldAttribute;g_strategyAttemptOriginal=oldAttempt;
    *floatOperand1=savedFloat1;*floatOperand2=savedFloat2;
    g_strategyCharmHasResume=oldCharm;
    g_strategyGroundBase=oldCellBase;
    puts("PASS strategy ground:164 native chance/attempt inputs and20 disabled controls; empty/city/gate/port/facility, max providers, strict0 threshold and stale/preview exclusions");
}
