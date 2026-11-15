// Whole4b6580/4b6460 and481350/481a80 execute. World validity, object/force
// getters and presentation-only helpers are controlled fixture inputs.
static uint8_t* g_precisionNativeForce;
static uint8_t* g_precisionNativeObject;
static bool g_precisionNativeValidForce=true,g_precisionNativePlayer=false;
static int g_precisionNativeForceId=1;
static uint32_t g_precisionNativePosition=0x20001;
static bool g_precisionNativeInvalidate=false;
static int g_precisionNativeStoreChange=0;
static uint8_t* g_precisionNativePerson;
static uint8_t* g_precisionNativeSource;
static uint8_t* g_precisionNativeVictim;
static int g_precisionNativeSourceKind=2;
static bool g_precisionNativeRegistered=true;
static bool g_precisionPrimaryRegistered=true;
static int g_precisionNativeBaseId=23,g_precisionScanLookups=0;
static int __fastcall precisionRootUnitId(void*,void*,void* p){return p==g_precisionNativeVictim?998:p==g_precisionNativeSource && g_precisionNativeSourceKind==1?999:-1;}
static void* __fastcall precisionRootUnit(void*,void*,int id){return id==998?g_precisionNativeVictim:id==999 && g_precisionNativeRegistered && g_precisionNativeSourceKind==1?g_precisionNativeSource:nullptr;}
static int __fastcall precisionRootBaseId(void*,void*,void* p){return p==g_precisionNativeSource && g_precisionNativeSourceKind==2?g_precisionNativeBaseId:-1;}
static void* __fastcall precisionRootBase(void*,void*,int id){return id==g_precisionNativeBaseId && g_precisionNativeRegistered && g_precisionNativeSourceKind==2?g_precisionNativeSource:nullptr;}
static int __fastcall precisionRootPersonId(void*,void*,void* p){return p==g_precisionNativePerson?g_precisionPersonId:p==g_precisionSecondaryPerson?14162:-1;}
static uint8_t* __fastcall precisionRootPerson(void*,void*,int id){
    if((uintptr_t)_ReturnAddress()==site(0x4cf31c)){assert(id==g_precisionScanLookups);++g_precisionScanLookups;}
    return id==g_precisionPersonId?(g_precisionPrimaryRegistered?g_precisionNativePerson:nullptr):id==14162?g_precisionSecondaryPerson:nullptr;
}
static int __fastcall precisionRootOwner(void* p,void*){return p==g_precisionNativeVictim?3:g_precisionNativeForceId;}
static void* __fastcall precisionRootForce(void*,void*,int id){return id==g_precisionNativeForceId?g_precisionNativeForce:nullptr;}
static int __cdecl precisionNativeValid(void* p) {
    if(g_precisionNativeInvalidate){g_precisionNativeInvalidate=false;skillsTelemetryStream(1,0);}
    return (p==g_precisionNativeForce && g_precisionNativeValidForce) || p && (p==g_precisionNativeObject || p==g_precisionNativeSource ||
        p==g_precisionNativeVictim || p==g_precisionNativePerson || p==g_precisionSecondaryPerson);
}
static int __fastcall precisionNativeForceId(void*,void*) {return g_precisionNativeForceId;}
static int __fastcall precisionNativePlayer(void*,void*) {return g_precisionNativePlayer;}
static const uint32_t* __fastcall precisionNativePosition(void*,void*) {return &g_precisionNativePosition;}
static int __cdecl precisionNativeNoMessage(void*) {return 1;}
static int __fastcall precisionNativeNoAnimation(void*,void*,void*) {return 0;}
static void* __cdecl precisionNativeNoGraphics() {return nullptr;}
static int __fastcall precisionNativeNoReward(void*,void*,void*,int,void*);
static int __fastcall precisionNativeStore(void* force,void*,int points) {
    int result=((PrecisionPoints)site(0x481a80))(force,points);
    if(g_precisionNativeStoreChange==1)skillsTelemetryStream(1,0);
    if(g_precisionNativeStoreChange==2)*(int*)(g_precisionNativePerson+0xe8)=249;
    if(g_precisionNativeStoreChange==3 || g_precisionNativeStoreChange==4) {
        auto savedReward=g_precisionRewardOriginal;g_precisionRewardOriginal=(PrecisionReward)precisionNativeNoReward;
        if(g_precisionNativeStoreChange==3)precisionReward(nullptr,nullptr,(uint8_t*)force,100,nullptr);
        else {DefeatScope child(g_precisionNativeVictim,g_precisionNativeSource);precisionReward(nullptr,nullptr,(uint8_t*)force,100,nullptr);}
        g_precisionRewardOriginal=savedReward;
    }
    return result;
}
static int __fastcall precisionNativeNoReward(void*,void*,void*,int,void*) {return 0;}
#include "precision_flow_cases.inc"
static void precisionTelemetryTests() {
    uint8_t force[300]={},object[32]={},person[400]={},source[0x110]={},victim[0x110]={};
    g_precisionNativeForce=force;g_precisionNativeObject=object;
    g_precisionNativePerson=person;
    g_precisionNativeSource=source;g_precisionNativeVictim=victim;
    void* rootVtable[24]={};rootVtable[0x3c/4]=(void*)precisionNativePosition;rootVtable[0x40/4]=(void*)precisionRootOwner;
    *(void**)source=*(void**)victim=*(void**)person=rootVtable;
    for(int n=0;n<3;++n)*(int*)(victim+0xc+4*n)=-1;
    auto oldUnitId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;auto oldBaseId=g_telemetryBaseId;auto oldBase=g_baseById;
    auto oldPersonId=g_telemetryPersonId;auto oldPerson=g_telemetryPerson;auto oldForce=g_precisionForceById;
    g_telemetryUnitId=(BuildingId)precisionRootUnitId;g_movementUnitById=(MovementUnitById)precisionRootUnit;
    g_telemetryBaseId=(BuildingId)precisionRootBaseId;g_baseById=(BaseById)precisionRootBase;
    g_telemetryPersonId=(BuildingId)precisionRootPersonId;g_telemetryPerson=(PersonById)precisionRootPerson;
    g_precisionForceById=(BaseById)precisionRootForce;
    std::optional<DefeatScope> root;
    bool old20=g_skills[249].effects[20];g_skills[249].effects[20]=true;
    void* forceVtable[16]={};forceVtable[0x28/4]=(void*)precisionNativeForceId;*(void**)force=forceVtable;
    void* objectVtable[16]={};objectVtable[0x3c/4]=(void*)precisionNativePosition;*(void**)object=objectVtable;
    auto original=g_precisionRewardOriginal;g_precisionRewardOriginal=(PrecisionReward)site(0x4b6580);
    auto oldPoints=g_precisionPointsOriginal;g_precisionPointsOriginal=(PrecisionPoints)site(0x481a80);
    uint32_t addresses[]={0x47a630,0x47a6d0,0x5a06c0,0x4b63a0,0x673240};uint8_t saved[5][5];
    for(int n=0;n<5;++n)memcpy(saved[n],(void*)site(addresses[n]),5);
    domesticStub(0x47a630,precisionNativeValid);domesticStub(0x47a6d0,precisionNativePlayer);
    domesticStub(0x5a06c0,precisionNativeNoMessage);domesticStub(0x4b63a0,precisionNativeNoAnimation);
    domesticStub(0x673240,precisionNativeNoGraphics);
    uint32_t defaultPosition=0x40003;auto operand=(uint32_t*)site(0x4b6598);uint32_t oldOperand=*operand;*operand=(uint32_t)&defaultPosition;
    *(uint8_t*)site(0x4bf16f)=0xc3;g_precisionEntry=site(0x4bf16a);
    *(int*)(person+0xe8)=20;
    {
        QueryOrigin origin(0x4bf079);assert(personHas(person,nullptr,20));
        {QueryOrigin nested(0x1234);assert(g_queryOrigin==0x1234);}
        assert(g_queryOrigin==0x4bf079);
    }
    assert(!g_queryOrigin);
    auto begin=[&](){
        root.reset();g_precisionNativeRegistered=true;g_precisionActorForce=g_precisionNativeForceId;
        *(int*)(source+0xc)=g_precisionPersonId;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
        *(int*)(person+0x9c)=g_precisionNativeBaseId;*(int*)(person+0xa0)=1;
        root.emplace(victim,source);
    };
    auto prepare=[&](){
        begin();
        QueryOrigin origin(g_precisionNativeSourceKind==1?0x4bf079:0x4bf126);assert(personHas(person,nullptr,20));
    };
    assert(skilltelemetry::open(L"build/precision_telemetry_test.csv",precisionActor,nullptr));
    for(int before:{1000,9980,10000}) {
        *(uint16_t*)(force+0xa2)=(uint16_t)before;prepare();
        int after=before+50;if(after>10000)after=10000;
        assert(runPrecision(force,100)==after-before && *(uint16_t*)(force+0xa2)==after);
    }
    skilltelemetry::close();
    // A rejected native force at the cap has the same final points, but no store.
    assert(skilltelemetry::open(L"build/precision_rejected_test.csv",precisionActor,nullptr));
    g_precisionNativeForceId=42;*(uint16_t*)(force+0xa2)=10000;prepare();
    assert(!runPrecision(force,100) && *(uint16_t*)(force+0xa2)==10000);
    skilltelemetry::close();assertBattleAppliedRows(L"build/precision_rejected_test.csv",0);
    // Execute the installed M8 imm32 bound cave for249 regular forces.
    auto forceBound=(uint8_t*)site(0x481359);uint8_t oldBound[5];memcpy(oldBound,forceBound,5);
    auto cave=(uint8_t*)VirtualAlloc(nullptr,16,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(cave);
    const uint8_t boundCode[]={0x3d,0,0,0,0,0x0f,0x8f,0,0,0,0,0xe9,0,0,0,0};
    memcpy(cave,boundCode,16);*(int*)(cave+1)=248;
    *(int32_t*)(cave+7)=(int32_t)(site(0x481364)-(uintptr_t)cave-11);
    *(int32_t*)(cave+12)=(int32_t)(site(0x48135e)-(uintptr_t)cave-16);
    assert(skilltelemetry::open(L"build/precision_native_test.csv",precisionActor,nullptr));
    for(bool expanded:{false,true}) {
    if(expanded){forceBound[0]=0xe9;*(int32_t*)(forceBound+1)=(int32_t)((uintptr_t)cave-(uintptr_t)forceBound-5);}
    for(int kind:{1,2})for(int owned:{20,249})for(bool player:{false,true})for(bool hasObject:{false,true})for(int forceId:{0,expanded?248:41})for(int before:{0,1,1000,9975,9980,9999,10000,65535}) {
        g_precisionNativeSourceKind=kind;
        *(int*)(person+0xe8)=owned;g_precisionPersonId=owned==20?1500:14162;
        g_precisionNativePlayer=player;g_precisionNativeForceId=forceId;
        *(uint16_t*)(force+0xa2)=(uint16_t)before;
        int without=before+25;if(without>10000)without=10000;
        assert(g_precisionRewardOriginal(nullptr,force,50,hasObject?object:nullptr)==without-before);
        assert(*(uint16_t*)(force+0xa2)==without);
        *(uint16_t*)(force+0xa2)=(uint16_t)before;prepare();
        int after=before+50;if(after>10000)after=10000;
        assert(runPrecision(force,100,hasObject?object:nullptr)==after-before && *(uint16_t*)(force+0xa2)==after);
    }
    }
    skilltelemetry::close();
    *(int*)(person+0xe8)=20;g_precisionPersonId=1500;g_precisionNativeForceId=1;g_precisionNativePlayer=false;
    assert(skilltelemetry::open(L"build/precision_session_source_test.csv",precisionActor,nullptr));prepare();skilltelemetry::close();
    assert(skilltelemetry::open(L"build/precision_session_after_test.csv",precisionActor,nullptr));
    *(uint16_t*)(force+0xa2)=1000;int resolved=g_precisionResolutions;
    assert(runPrecision(force,100)==50 && g_precisionResolutions==resolved);
    prepare();assert(runPrecision(force,100)==50);skilltelemetry::close();
    assertBattleAppliedRows(L"build/precision_session_after_test.csv",1);
    assert(skilltelemetry::open(L"build/precision_context_test.csv",precisionActor,nullptr));
    for(int mutation=0;mutation<11;++mutation) {
        *(int*)(person+0xe8)=20;g_precisionNativeValidForce=true;g_precisionNativeForceId=1;
        *(uint16_t*)(force+0xa2)=mutation>=7 && mutation!=9?10000:1000;prepare();
        if(mutation==1)skillsTelemetryStream(1,0);
        if(mutation==2)*(int*)(person+0xe8)=-1;
        if(mutation==3)*(int*)(person+0xe8)=249;
        if(mutation==4 || mutation==5){g_precisionNativeStoreChange=mutation==4?1:2;g_precisionPointsOriginal=(PrecisionPoints)precisionNativeStore;}
        if(mutation==6)g_precisionNativeInvalidate=true;
        if(mutation==7)g_precisionNativeValidForce=false;
        if(mutation==8)g_precisionNativeForceId=249;
        if(mutation==10)g_precisionRewardOriginal=(PrecisionReward)precisionNativeNoReward;
        int before=*(uint16_t*)(force+0xa2),amount=mutation==9?50:100;
        int after=before+(mutation>=7 && mutation!=9?0:amount/2);
        resolved=g_precisionResolutions;
        assert(runPrecision(force,amount)==after-before && *(uint16_t*)(force+0xa2)==after);
        assert(g_precisionResolutions==resolved+(mutation==0?1:0) && !g_precisionStore);
        g_precisionPointsOriginal=(PrecisionPoints)site(0x481a80);g_precisionRewardOriginal=(PrecisionReward)site(0x4b6580);
        g_precisionNativeStoreChange=0;
    }
    skilltelemetry::close();assertBattleAppliedRows(L"build/precision_context_test.csv",1);
    assert(skilltelemetry::open(L"build/precision_root_test.csv",precisionActor,nullptr));
    int rootApplied=0;
    for(int kind:{1,2})for(int mutation=0;mutation<10;++mutation) {
        g_precisionNativeSourceKind=kind;*(int*)(person+0xe8)=20;g_precisionNativeValidForce=true;g_precisionNativeForceId=1;
        *(uint16_t*)(force+0xa2)=1000;prepare();
        if(mutation==1)root.reset();
        if(mutation==2){root.reset();root.emplace(victim,source);}
        if(mutation==3)g_precisionNativeRegistered=false;
        if(mutation==4){if(kind==1)*(int*)(source+0xc)=-1;else *(int*)(person+0x9c)=24;}
        if(mutation==5)*(int*)(person+0xa0)=5;
        if(mutation==6)*(int*)(person+0x98)=9999; // away from home remains eligible natively
        if(mutation==7 || mutation==8){g_precisionNativeStoreChange=mutation==7?3:4;g_precisionPointsOriginal=(PrecisionPoints)precisionNativeStore;}
        bool expected=mutation==0 || mutation==6 || mutation==7 || mutation==8 || mutation==9 || kind==1 && mutation==5;
        resolved=g_precisionResolutions;assert(runPrecision(force,100)==50 && *(uint16_t*)(force+0xa2)==1050);
        assert(g_precisionResolutions==resolved+(expected?1:0));rootApplied+=expected;
        if(mutation==9){resolved=g_precisionResolutions;assert(runPrecision(force,100)==50 && *(uint16_t*)(force+0xa2)==1100);assert(g_precisionResolutions==resolved);}
        g_precisionNativeStoreChange=0;g_precisionPointsOriginal=(PrecisionPoints)site(0x481a80);
    }
    skilltelemetry::close();assertBattleAppliedRows(L"build/precision_root_test.csv",rootApplied);
    precisionFlowTests(begin,force,source,person);
    *(int*)(person+0xe8)=20;g_precisionNativeValidForce=true;g_precisionNativeForceId=1;*(uint16_t*)(force+0xa2)=1000;
    resolved=g_precisionResolutions;prepare();assert(runPrecision(force,100)==50);
    assert(g_precisionResolutions==resolved && !g_precisionStore);
    root.reset();assert(!g_defeat);
    memcpy(forceBound,oldBound,5);assert(VirtualFree(cave,0,MEM_RELEASE));
    for(int n=0;n<5;++n)memcpy((void*)site(addresses[n]),saved[n],5);
    *operand=oldOperand;g_precisionRewardOriginal=original;g_precisionPointsOriginal=oldPoints;
    g_skills[249].effects[20]=old20;
    g_telemetryUnitId=oldUnitId;g_movementUnitById=oldUnit;g_telemetryBaseId=oldBaseId;g_baseById=oldBase;
    g_telemetryPersonId=oldPersonId;g_telemetryPerson=oldPerson;g_precisionForceById=oldForce;
    puts("PASS precision:512 native rewards/independent no20 baselines, unit/base scoped providers, raw/composed/high provider, stock/249-force validation, native halving/cap/word stores");
    puts("PASS precision: rejected capped force, reopened session and ten stale/changed/invalid/no-setter boundaries preserve native outcomes without applied rows");
}
