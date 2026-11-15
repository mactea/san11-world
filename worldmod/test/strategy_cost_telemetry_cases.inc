// Execute the native cost CALL/NEG/debit CALL pair, entire495170,
//4964f0 and496490. Unit membership/world/technology inputs are fixtures.
static uint8_t* g_strategyFixtureUnit;
static uint8_t g_strategyFixtureForce[0x100]={};
static bool g_strategyFixtureAdvanced=false,g_strategyFixtureUnitRegistered=true,g_strategyFixtureActorRegistered=true;
static uintptr_t g_strategyFixtureEntry;
static int __fastcall strategyFixtureFind(uint8_t* unit,void*,int effect) {
    for(int n=0;n<3;++n){int id=*(int*)(unit+0xc+4*n);auto actor=personFixtureLookup(nullptr,nullptr,id);
        if(actor && personHas(actor,nullptr,effect))return id;}
    return -1;
}
static int __fastcall strategyFixtureUnitId(void*,void*,void* unit){return unit==g_strategyFixtureUnit?4095:-1;}
static void* __fastcall strategyFixtureUnit(void*,void*,int id){return id==4095 && g_strategyFixtureUnitRegistered?g_strategyFixtureUnit:nullptr;}
static int __fastcall strategyFixturePersonId(void* w,void* u,const void* person) {
    return g_strategyFixtureActorRegistered?personFixtureId(w,u,person):-1;
}
static void* __fastcall strategyFixtureForce(void*,void*,int id){assert(id==2);return g_strategyFixtureForce;}
static int __fastcall strategyFixtureTech(void*,void*,int id){assert(id==16);return g_strategyFixtureAdvanced;}
static const uint32_t* __fastcall strategyFixturePosition(uint8_t* unit,void*){return (const uint32_t*)(unit+4);}
static int __fastcall strategyFixtureCostWorld(void* unit,void*,int plan) {
    int result=((StrategyCost)site(0x495170))(unit,plan);skillsTelemetryStream(1,0);return result;
}
static int __fastcall strategyFixtureGrantWorld(void* unit,void*,int requested) {
    int result=((UnitEnergy)site(0x4964f0))(unit,requested);skillsTelemetryStream(1,0);return result;
}
__declspec(naked) static int __cdecl runStrategyCost(void* unit,int plan){__asm {push edi
    mov edi,[esp+8]
    push [esp+12]
    mov ecx,edi
    jmp dword ptr [g_strategyFixtureEntry]}}
__declspec(naked) static void strategyFixtureCommitted(){__asm {pop edi
    ret}}
#include "strategy_owner_telemetry_cases.inc"
static void strategyCostTelemetryTests() {
    uint8_t unit[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},temporary[0x110]={};
    g_strategyFixtureUnit=unit;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[24]={};vt[0x3c/4]=strategyFixturePosition;vt[0x40/4]=personFixtureOwner;vt[0x54/4]=battleEnergy;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=strategyFixtureFind;*(void**)unit=vt;
    *(int*)(unit+0xc)=600;*(int*)(unit+0x10)=14162;*(int*)(unit+0x14)=-1;
    *(uint32_t*)(unit+4)=0x10001;*(int*)(unit+0xa8)=4;memcpy(temporary,unit,sizeof unit);
    auto oldCost=g_strategyCostOriginal;auto oldCosts=g_strategyNormalCosts;auto oldForced=g_strategyCostForcedResume;
    auto oldEnergy=g_unitEnergyOriginal;auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;
    auto oldUnitId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;bool old64=g_skills[249].effects[64];
    uint8_t costs[9]={10,10,15,15,10,10,20,50,50};
    auto operand=(uint32_t*)site(0x49518a);uint32_t oldOperand=*operand;*operand=(uint32_t)costs;
    g_strategyCostOriginal=(StrategyCost)site(0x495170);g_strategyNormalCosts=costs;
    g_strategyCostForcedResume=(uint32_t)site(0x495180);g_unitEnergyOriginal=(UnitEnergy)site(0x4964f0);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)strategyFixturePersonId;
    g_telemetryUnitId=(BuildingId)strategyFixtureUnitId;g_movementUnitById=(MovementUnitById)strategyFixtureUnit;
    domesticStub(0x47a630,domesticValid);domesticStub(0x490aa0,strategyFixtureForce);domesticStub(0x4811e0,strategyFixtureTech);
    domesticStub(0x59344a,strategyFixtureCommitted);g_strategyFixtureEntry=site(0x59343b);
    g_skills[249].effects[64]=true;
    g_strategyFixtureActorRegistered=g_strategyFixtureUnitRegistered=true;
    assert(skilltelemetry::open(L"build/strategy_cost_test.csv",personFixtureActor,nullptr));
    for(bool advanced:{false,true})for(int plan=0;plan<9;++plan)for(int before:{0,1,5,10,100,120,255}) {
        g_strategyFixtureAdvanced=advanced;int cap=advanced?120:100;
        *(int*)(actor+0xe8)=*(int*)(other+0xe8)=-1;unit[0x1a]=(uint8_t)before;
        int ordinary=g_strategyCostOriginal(unit,plan);assert(ordinary==costs[plan]);
        g_unitEnergyOriginal(unit,-ordinary);int without=unit[0x1a]; // independent actual native no64 grant
        int expected=before-ordinary;if(expected<0)expected=0;if(expected>cap)expected=cap;assert(without==expected);
        for(int provider=0;provider<4;++provider) {
            *(int*)(actor+0xe8)=provider==0?64:provider==2?249:-1;
            *(int*)(other+0xe8)=provider==1?64:provider==2?64:provider==3?249:-1;
            unit[0x1a]=(uint8_t)before;int after=before?before-1:0;if(after>cap)after=cap;
            assert(runStrategyCost(unit,plan)==after-before && unit[0x1a]==after && !g_strategyCostPending.actor);
            int baseline=before-costs[plan];if(baseline<0)baseline=0;if(baseline>after)baseline=after;
            assert(baseline==without && !g_strategyCostProbe && !g_progressionProbe);
        }
    }
    auto setup=[&](){*(int*)(actor+0xe8)=64;*(int*)(other+0xe8)=-1;unit[0x1a]=10;
        g_strategyFixtureAdvanced=false;g_strategyFixtureActorRegistered=g_strategyFixtureUnitRegistered=true;};
    setup();costs[0]=0;assert(runStrategyCost(unit,0)==-1 && unit[0x1a]==9); // no invented baseline when normal cost is below1
    costs[0]=1;*(int*)(actor+0xe8)=-1;assert(runStrategyCost(unit,0)==-1); // result1 alone cannot authorize64
    costs[0]=10;
    setup();assert(g_strategyCostOriginal(unit,0)==1);strategyCostGrant(unit,nullptr,-1); // unscoped UI preview
    setup();strategyCost(unit,nullptr,0);strategyCostGrant(unit,nullptr,-2); // mismatched debit
    setup();strategyCost(unit,nullptr,0);strategyCostGrant(temporary,nullptr,-1); // wrong unit
    setup();g_strategyFixtureActorRegistered=false;runStrategyCost(unit,0);
    setup();g_strategyFixtureUnitRegistered=false;runStrategyCost(unit,0);
    setup();strategyCost(unit,nullptr,0);*(int*)(actor+0xe8)=-1;strategyCostGrant(unit,nullptr,-1);
    setup();runStrategyCost(unit,-1); // native64 bypasses table; telemetry must not read outside its known9 entries
    setup();strategyCost(unit,nullptr,0);skillsTelemetryStream(1,0);strategyCostGrant(unit,nullptr,-1);
    setup();g_strategyCostOriginal=(StrategyCost)strategyFixtureCostWorld;runStrategyCost(unit,0);
    g_strategyCostOriginal=(StrategyCost)site(0x495170);
    setup();g_unitEnergyOriginal=(UnitEnergy)strategyFixtureGrantWorld;assert(runStrategyCost(unit,0)==-1 && unit[0x1a]==9);
    g_unitEnergyOriginal=(UnitEnergy)site(0x4964f0);
    assert(!g_strategyCostPending.actor && !g_strategyCostProbe && !g_progressionProbe);skilltelemetry::close();
    strategyOwnerTelemetryTests(unit,actor,other,costs);
    g_strategyCostOriginal=oldCost;g_strategyNormalCosts=oldCosts;g_strategyCostForcedResume=oldForced;
    g_unitEnergyOriginal=oldEnergy;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;
    g_telemetryUnitId=oldUnitId;g_movementUnitById=oldUnit;g_skills[249].effects[64]=old64;*operand=oldOperand;
    puts("PASS native strategy64: all9 cost/debit paths, first/high/composed provider,100/120 caps and byte255,126 independent native baselines, preview/stale/unforced1/context exclusions");
}
