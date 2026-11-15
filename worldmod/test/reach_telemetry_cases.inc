// Execute whole5a3cd0 permission, its native unit/city checks and the actual
//5933a0 cost/dispatch/return frame. Chance/leaf/world relations are adapters;
// original495170/4964f0 energy calculation and storage execute.
static uint8_t *g_reachFixtureExtra,*g_reachFixtureTarget;
static uint8_t g_reachFixtureForce[0x100]={};
static int g_reachFixturePlan=0,g_reachFixtureSuccess=1,g_reachFixtureLeafCalls=0,g_reachFixtureMutation=0;
static bool g_reachFixtureTechnology=false,g_reachFixtureForceChanged=false;
static bool g_reachFixtureBaseTarget=false;
static uint8_t* g_reachFixtureProvider=nullptr;
static StrategyCommand g_reachFixtureNativeCommand;
static int g_reachFixtureRingCounts[5];
static int __cdecl reachFixtureRing(const void*,uint32_t position,uint32_t center,int){
    int distance=reachDistance(center,position);assert(distance>=1 && distance<=4);
    assert(distance==reachDistance(position,center));++g_reachFixtureRingCounts[distance];return 0;
}
static void* __fastcall reachFixtureUnit(void*,void*,int id){return id==4093?g_reachFixtureExtra:criticalFixtureUnit(nullptr,nullptr,id);}
static int __fastcall reachFixtureOwner(void* unit,void*){
    if(unit==g_criticalSource)return g_reachFixtureForceChanged?3:2;
    return unit==g_reachFixtureTarget && (g_reachFixturePlan==1 || g_reachFixturePlan==4)?2:3;
}
static void* __fastcall reachFixtureObject(uint8_t* cell,void*){
    if((*(uint32_t*)cell&3)==0)return nullptr;
    int id=*(uint16_t*)(cell+8);return reachFixtureUnit(nullptr,nullptr,id);
}
static int __fastcall reachFixtureKind(void* object,void*){return g_reachFixtureBaseTarget && object==g_reachFixtureTarget?5:11;}
static int __fastcall reachFixtureCast(void* object,void*,int kind){return g_reachFixtureBaseTarget && object==g_reachFixtureTarget?kind==5:kind==11;}
static int __fastcall reachFixtureEnemy(void* unit,void*,void* other){return reachFixtureOwner(unit,nullptr)!=reachFixtureOwner(other,nullptr);}
static int __fastcall reachFixtureSame(void* unit,void*,void* other){return !reachFixtureEnemy(unit,nullptr,other);}
static void* __fastcall reachFixtureForce(void*,void*,int){return g_reachFixtureForce;}
static int __fastcall reachFixtureTech(void*,void*,int id){
    int result=id==29 && g_reachFixtureTechnology;
    if(id==29 && g_reachFixtureMutation==10)skillsTelemetryStream(1,0);return result;
}
static int __cdecl reachFixtureAttempt(void*,uint32_t,uint32_t,int,int* detail){
    detail[0]=detail[1]=0;
    if(g_reachFixtureMutation==11)*(int*)(g_personGuide+0xe8)=-1;
    return g_reachFixtureSuccess;
}
static int __cdecl reachFixtureCritical(void*,uint32_t,uint32_t,int){return 0;}
static int __fastcall reachFixtureLeaf(void* self,void*,const uint32_t* command,int,int,int,void*,int){
    ++g_reachFixtureLeafCalls;
    if(g_reachFixtureMutation==1)*(int*)(g_personGuide+0xe8)=-1;
    if(g_reachFixtureMutation==2)g_criticalSourceRegistered=false;
    if(g_reachFixtureMutation==3)g_criticalActorRegistered=false;
    if(g_reachFixtureMutation==4)skillsTelemetryStream(1,0);
    if(g_reachFixtureMutation==5)*(int*)(g_criticalSource+0xc)=-1;
    if(g_reachFixtureMutation==6)g_reachFixtureForceChanged=true;
    if(g_reachFixtureMutation==7)((uint32_t*)command)[2]^=1;
    if(g_reachFixtureMutation==8){g_reachFixtureMutation=0;((StrategyCommand)site(0x5933a0))(self,command,1,nullptr);g_reachFixtureMutation=8;}
    if(g_reachFixtureMutation>=12 && g_reachFixtureMutation<=15)++*(int*)(g_reachFixtureProvider+0x98+4*(g_reachFixtureMutation-12));
    if(g_reachFixtureMutation==16)g_skills[249].effects[65]=false;
    if(g_reachFixtureMutation==17)g_criticalTargetRegistered=false;
    if(g_reachFixtureMutation==18)++*(uint32_t*)(g_reachFixtureTarget+4);
    return 1;
}
static int __fastcall reachFixtureBadReturn(void* self,void*,const uint32_t* command,int mode,void* person){
    g_reachFixtureNativeCommand(self,command,mode,person);return 2;
}
#include "reach_owner_telemetry_cases.inc"
#include "strategy_reach_owner_cases.inc"
static void reachTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},extra[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_criticalSource=source;g_criticalTarget=target;g_reachFixtureTarget=target;g_reachFixtureExtra=extra;
    g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x24/4]=reachFixtureKind;vt[0x2c/4]=reachFixtureCast;vt[0x3c/4]=criticalFixturePosition;
    vt[0x40/4]=reachFixtureOwner;vt[0x50/4]=battleTroops;vt[0x54/4]=battleEnergy;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=criticalFixtureFind;
    for(auto unit:{source,target,extra}){*(void**)unit=vt;*(int*)(unit+0xa8)=3;*(int*)(unit+0x10)=*(int*)(unit+0x14)=-1;}
    *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(target+0xc)=1000;*(int*)(extra+0xc)=601;
    *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(extra+4)=0x10002;
    auto oldSkill=g_skills[249];memset(g_skills[249].effects,0,sizeof g_skills[249].effects);g_skills[249].effects[65]=true;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;auto oldValid=g_growthValid;
    auto oldObject=g_reachObjectOriginal;auto oldTechnology=g_reachTechnologyOriginal;auto oldPermission=g_reachPermissionEntry;
    auto oldRoot=g_strategyCommandOriginal;auto oldRootResume=g_strategyCommandResume;auto oldAttempt=g_strategyAttemptOriginal;
    auto oldCritical=g_strategyCriticalOriginal;auto oldCost=g_strategyCostOriginal;auto oldCosts=g_strategyNormalCosts;auto oldEnergy=g_unitEnergyOriginal;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)reachFixtureUnit;g_growthValid=domesticValid;
    g_reachObjectOriginal=(ReachObject)reachFixtureObject;g_reachTechnologyOriginal=(ReachTechnology)site(0x4811e0);g_reachPermissionEntry=site(0x5a3cd0);
    g_strategyCommandResume=(uint32_t)site(0x5933a6);g_reachFixtureNativeCommand=g_strategyCommandOriginal;
    memcpy((void*)site(0x59340c),"\x33\xdb\x85\xed\x74\x1c",6); // undo charm's short-frame exit
    g_strategyAttemptOriginal=reachFixtureAttempt;g_strategyCriticalOriginal=reachFixtureCritical;
    g_strategyCostOriginal=(StrategyCost)site(0x495170);g_unitEnergyOriginal=(UnitEnergy)site(0x4964f0);
    uint8_t costs[9]={10,10,15,15,10,10,20,50,50};g_strategyNormalCosts=costs;
    domesticStub(0x47a630,domesticValid);domesticStub(0x490b00,personFixtureLookup);domesticStub(0x490e70,reachFixtureUnit);
    domesticStub(0x483b40,reachFixtureObject);domesticStub(0x490aa0,reachFixtureForce);domesticStub(0x4811e0,reachFixtureTech);
    domesticStub(0x495c50,reachFixtureEnemy);domesticStub(0x495c20,reachFixtureSame);domesticStub(0x4917c0,criticalFixtureUnitId);
    domesticStub(0x5afbe0,magicFixtureNotExplosive);domesticStub(0x57e5d0,magicFixtureVoid);domesticStub(0x4954c0,afterFixtureRecalc);
    memcpy((void*)site(0x620f30),"\x56\x8b\x74\x24\x08",5); // restore native read-only unit cast
    for(uint32_t leaf:{0x591270u,0x5915e0u,0x5917d0u,0x591a20u,0x591c70u,0x592740u,0x592a80u,0x592050u,0x592ed0u})domesticStub(leaf,reachFixtureLeaf);
    uint32_t addresses[16],saved[16];int nAddresses=0;
    auto patch=[&](uint32_t at,uint32_t value){addresses[nAddresses]=at;saved[nAddresses++]=*(uint32_t*)site(at);*(uint32_t*)site(at)=value;};
    for(const auto& m:g_reachMapImmediates)if(m.kind)patch(m.at,(uint32_t)g_criticalCells+(m.kind==2?4:0));
    patch(0x5a07aa,(uint32_t)g_criticalCells);patch(0x5933de,(uint32_t)g_criticalCells);patch(0x49518a,(uint32_t)costs);
    patch(0x5a078f,200);patch(0x5a0783,200);patch(0x5933cf,200); // undo earlier expanded-map fixture
    patch(0x5a3ecb,(uint32_t)site(0x5a31f0));
    patch(0x4843e3,(uint32_t)site(0x79c310));patch(0x4843eb,(uint32_t)site(0x79c312));
    patch(0x48440f,(uint32_t)site(0x79c4cc));patch(0x484448,(uint32_t)site(0x79c310));patch(0x484453,(uint32_t)site(0x79c312));
    uint32_t table[9];for(int n=0;n<9;++n)table[n]=(uint32_t)site(*(uint32_t*)site(0x5935fc+4*n));patch(0x59345c,(uint32_t)table);
    typedef int(__thiscall* NativeRings)(void*,uint32_t,int,int(__cdecl*)(const void*,uint32_t,uint32_t,int),uint32_t,int);
    for(uint32_t center:{0x000a000au,0x000a000bu}){
        memset(g_reachFixtureRingCounts,0,sizeof g_reachFixtureRingCounts);
        assert(((NativeRings)site(0x4843a0))(g_criticalCells,center,4,reachFixtureRing,center,0)==0);
        for(int radius=1;radius<=4;++radius)assert(g_reachFixtureRingCounts[radius]==6*radius);
    }
    auto prepare=[&](int provider,int plan,int technology,int distance,int energy,int success){
        *(int*)(actor+0xe8)=provider==0?65:provider==2 || provider==4?249:-1;
        *(int*)(other+0xe8)=provider==1 || provider==4?65:provider==3?249:-1;
        *(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;*(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;
        g_reachFixturePlan=plan;g_reachFixtureTechnology=technology!=0;g_reachFixtureSuccess=success;
        g_reachFixtureMutation=g_reachFixtureLeafCalls=0;g_reachFixtureForceChanged=false;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;g_criticalBuildingTarget=false;
        g_strategyCommandOriginal=oldRoot;source[0x1a]=(uint8_t)energy;
        *(uint32_t*)(target+4)=((1u+distance)<<16)|1;*(int*)(target+0x24)=plan==4?1:0;
        for(int n=0;n<500;++n){*(uint32_t*)(g_criticalCells+20*n)=1;*(uint16_t*)(g_criticalCells+20*n+8)=4093;*(uint32_t*)(g_criticalCells+20*n+4)=0;}
        auto cell=g_criticalCells+20*(201+distance);*(uint16_t*)(cell+8)=4094;if(plan==1)*(uint32_t*)cell|=1<<23;
        *(uint16_t*)(g_criticalCells+20*201+8)=4095;*(uint32_t*)(g_criticalCells+20*201+4)=plan==5?5:0;
        g_defenseSeed=0x12345678;
    };
    // Permission checks are read-only, including native inner-strife neighbor traversal.
    for(int provider:{0,5})for(int plan=0;plan<9;++plan)for(int technology:{0,1})for(int distance=1;distance<=4;++distance){
        prepare(provider,plan,technology,distance,100,1);int normal=plan==0?(technology?3:1):plan==5 || plan==6?1:2;
        uint8_t units[3][0x110],people[4][400],cells[sizeof g_criticalCells],force[sizeof g_reachFixtureForce];
        uint8_t* unitList[]={source,target,extra};uint8_t* personList[]={actor,other,person,spouse};
        for(int n=0;n<3;++n)memcpy(units[n],unitList[n],sizeof units[n]);
        for(int n=0;n<4;++n)memcpy(people[n],personList[n],sizeof people[n]);
        memcpy(cells,g_criticalCells,sizeof cells);memcpy(force,g_reachFixtureForce,sizeof force);
        int mask=reachPermission(source,0x10001,target,*(uint32_t*)(target+4),distance,1u<<plan);
        assert(mask==(distance<=normal+(provider==0?1:0)?1<<plan:0) && g_defenseSeed==0x12345678);
        for(int n=0;n<3;++n)assert(!memcmp(units[n],unitList[n],sizeof units[n]));
        for(int n=0;n<4;++n)assert(!memcmp(people[n],personList[n],sizeof people[n]));
        assert(!memcmp(cells,g_criticalCells,sizeof cells) && !memcmp(force,g_reachFixtureForce,sizeof force));
    }
    assert(skilltelemetry::open(L"build/reach_strategy_test.csv",afterFixtureActor,nullptr));int commands=0;
    auto run=(StrategyCommand)site(0x5933a0);
    for(int provider=0;provider<6;++provider)for(int plan=0;plan<9;++plan)for(int technology:{0,1})for(int distance=1;distance<=4;++distance)
        for(int energy:{0,1,100,255})for(int mode:{0,1})for(int success:{0,1}){
            prepare(provider,plan,technology,distance,energy,success);uint32_t command[3]={(uint32_t)plan,4095,*(uint32_t*)(target+4)};
            assert(run(nullptr,command,mode,nullptr)==success && !g_reachCommand && !g_reachPermissionProbe && !g_strategyCommand);
            int expected=energy;if(!mode){expected-=costs[plan];if(expected<0)expected=0;if(expected>100)expected=100;}
            assert(source[0x1a]==expected && g_reachFixtureLeafCalls==1 && g_defenseSeed==0x12345678);++commands;
        }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/reach_strategy_edges_test.csv",afterFixtureActor,nullptr));
    for(int mutation=1;mutation<=11;++mutation){
        int plan=mutation==10?0:2;prepare(0,plan,0,mutation==10?2:3,100,1);g_reachFixtureMutation=mutation;
        uint32_t command[3]={(uint32_t)plan,4095,*(uint32_t*)(target+4)};
        if(mutation==9)g_strategyCommandOriginal=(StrategyCommand)reachFixtureBadReturn;
        assert(run(nullptr,command,0,nullptr)==(mutation==9?2:1) && !g_reachCommand && g_defenseSeed==0x12345678);
        assert(source[0x1a]==100-costs[plan] && g_reachFixtureLeafCalls==(mutation==8?2:1));
    }
    skilltelemetry::close();for(int plan=0;plan<9;++plan){prepare(0,plan,0,2,100,1);uint32_t command[3]={(uint32_t)plan,4095,*(uint32_t*)(target+4)};
        assert(run(nullptr,command,0,nullptr)==1 && source[0x1a]==100-costs[plan] && g_defenseSeed==0x12345678 && !g_reachCommand);}
    reachOwnerTelemetryTests(source,target,actor,other,prepare);
    strategyReachOwnerTests(source,target,actor,other,prepare);
    for(int n=0;n<nAddresses;++n)*(uint32_t*)site(addresses[n])=saved[n];g_skills[249]=oldSkill;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;g_growthValid=oldValid;
    g_reachObjectOriginal=oldObject;g_reachTechnologyOriginal=oldTechnology;g_reachPermissionEntry=oldPermission;
    g_strategyCommandOriginal=oldRoot;g_strategyCommandResume=oldRootResume;g_strategyAttemptOriginal=oldAttempt;g_strategyCriticalOriginal=oldCritical;
    g_strategyCostOriginal=oldCost;g_strategyNormalCosts=oldCosts;g_unitEnergyOriginal=oldEnergy;
    printf("PASS reach65: %d native permission/technology/paid-command/cost/return inputs, all9 plans, exact state/RNG, nested/stale/no-cost exclusions\n",commands);
}
