// Native chance/threshold, region and flag-selection code, plus the native camp
// constructor call/return sequence. World allocation and scenario inputs are fixtures.
static uintptr_t g_unrestRollEntry,g_unrestCampEntry,g_unrestCampOnlyEntry,g_unrestFactoryTail;
static uint32_t g_unrestFrame=0x310,g_unrestFlagOffset=0x180;
static uint8_t* g_unrestRecord;
static UpkeepBuilding* g_unrestCity;
static UpkeepBuilding* g_unrestCamp;
static void* g_unrestCityForRecord;
static void* g_unrestHiddenCity;
static void* g_unrestExpectedCity;
static int g_unrestChanceCalls=0,g_unrestWorldCreates=0,g_unrestExpectedForce=46;
static bool g_unrestScenarioPresent=false,g_unrestScenarioBlock=false,g_unrestRecordValid=true;
static bool g_unrestCampFails=false,g_unrestCampRegistered=true,g_unrestCampWrongForce=false;
static uint8_t g_unrestFacility[8]={};
static int __fastcall unrestFixtureScenario(void*,void*) {return g_unrestScenarioPresent;}
static int __fastcall unrestFixtureRule(void*,void*,int id) {assert(id==7);return g_unrestScenarioBlock;}
static int __cdecl unrestFixtureChance(int n) {assert(n>=0 && n<=100);++g_unrestChanceCalls;return 1;}
static int __cdecl unrestFixtureValid(void* p) {return p && (p!=g_unrestRecord || g_unrestRecordValid);}
static void* __fastcall unrestFixtureCity(void* record,void*) {assert(record==g_unrestRecord);return g_unrestCityForRecord;}
static void* __fastcall unrestFixtureBase(void*,void*,int id) {
    return id==1000 && g_unrestCampRegistered?g_unrestCamp:nullptr;
}
static void* __fastcall unrestFixtureCreate(void*,void*,void* facility,const uint32_t* hex,int force) {
    assert(facility==g_unrestFacility && *hex==0x00220011 && force==g_unrestExpectedForce);
    assert(g_unrestHiddenCity==g_unrestExpectedCity);++g_unrestWorldCreates;
    if(g_unrestCampFails)return nullptr;
    g_unrestCamp->force=g_unrestCampWrongForce?2:force;return g_unrestCamp;
}
static bool unrestFixtureActor(const void* p,skilltelemetry::Actor& actor) {
    economyActor(p,actor);actor.person=p==g_upkeepActor?600:14162;actor.force=2;actor.city=g_unrestCity->id;
    actor.year=194;actor.month=1;actor.day=1;return true;
}
__declspec(naked) static int __cdecl runUnrestRoll(const int* flags,void* record) {
    __asm {
        push esi
        mov esi,[esp+12]
        push dword ptr [esp+8]
        mov eax,g_unrestRollEntry
        jmp eax
    }
}
__declspec(naked) static void unrestFixtureRolled() {
    __asm {add esp,4
           pop esi
           ret}
}
__declspec(naked) static void* __cdecl unrestFixtureFactory(uint32_t,int) {
    __asm {
        mov g_unrestHiddenCity,esi
        sub esp,0x3d4
        push ebx
        push ebp
        push edi
        mov ebp,[esp+0x3e8]
        mov eax,offset g_unrestFacility
        mov edx,g_unrestFactoryTail
        jmp edx
    }
}
__declspec(naked) static void unrestFixtureCreated() {
    __asm {mov eax,edi
           pop edi
           pop ebp
           pop ebx
           add esp,0x3d4
           ret}
}
__declspec(naked) static void* __cdecl runUnrestCamp(void* city,const int* flags,int cityId,int select=1) {
    __asm {
        push ebx
        push ebp
        push esi
        push edi
        sub esp,dword ptr [g_unrestFrame]
        mov edx,g_unrestFrame
        mov eax,[esp+edx+20]
        mov [esp+0x28],eax
        mov eax,[esp+edx+28]
        mov [esp+0x18],eax
        mov eax,[esp+edx+32]
        mov [esp+0x14],eax
        mov dword ptr [esp+0x1c],0x00220011
        mov esi,[esp+edx+24]
        mov edi,esp
        add edi,dword ptr [g_unrestFlagOffset]
        mov ecx,100
        rep movsd
        cmp dword ptr [esp+0x14],0
        je noSelection
        mov eax,g_unrestCampEntry
        jmp eax
    noSelection:
        mov eax,g_unrestExpectedForce
        mov edx,g_unrestCampOnlyEntry
        jmp edx
    }
}
__declspec(naked) static void unrestFixtureCommitted() {
    __asm {add esp,8
           add esp,dword ptr [g_unrestFrame]
           pop edi
           pop esi
           pop ebp
           pop ebx
           ret}
}
#include "unrest_context_cases.inc"
static void unrestTelemetryTests() {
    uint8_t first[400]={},second[400]={},record[0x248]={};
    g_upkeepActor=first;g_secondIncomeActor=second;g_unrestRecord=record;
    void* vt[24]={};vt[0x40/4]=upkeepForce;
    UpkeepBuilding city={vt,0,0,0,0,2,0,0,0,0},otherCity={vt,0,0,0,0,2,1,0,0,0};
    UpkeepBuilding camp={vt,0,15,0,0,46,1000,0,0,0};
    g_unrestCity=&city;g_unrestCamp=&camp;g_unrestCityForRecord=&city;g_unrestExpectedCity=&city;
    auto oldFlags=g_cityIncomeFlagsOriginal;auto oldCity=g_cityBuilding;auto oldId=g_telemetryBaseId;auto oldBase=g_baseById;
    auto oldRoll=g_unrestRollOriginal;auto oldCamp=g_unrestCampOriginal;auto oldValid=g_unrestValid;
    auto oldForceBase=g_unrestForceBase;auto oldTribe=g_unrestTribeOriginal;
    auto oldThresholdResume=g_unrestThresholdResume;
    g_cityIncomeFlagsOriginal=(CityIncomeFlags)collectIncome;g_cityBuilding=(CityBuilding)unrestFixtureCity;
    g_telemetryBaseId=(BuildingId)upkeepBaseId;g_baseById=(BaseById)unrestFixtureBase;
    g_unrestRollOriginal=(UnrestRoll)site(0x58cf60);g_unrestCampOriginal=unrestFixtureFactory;g_unrestValid=unrestFixtureValid;
    g_unrestForceBase=(const int*)site(0x58bbb0);g_unrestTribeOriginal=site(0x58bb90);
    g_unrestThresholdResume=(uint32_t)site(0x58cfa9);
    domesticStub(0x67f750,unrestFixtureScenario);domesticStub(0x4721d0,unrestFixtureChance);
    domesticStub(0x47a630,unrestFixtureValid);domesticStub(0x4ad500,unrestFixtureCreate);
    domesticStub(0x58ea0a,unrestFixtureRolled);domesticStub(0x58eaaa,unrestFixtureCommitted);
    domesticStub(0x58be17,unrestFixtureCreated);g_unrestFactoryTail=site(0x58bde8);
    g_unrestRollEntry=site(0x58ea05);g_unrestCampEntry=site(0x58ea56);g_unrestCampOnlyEntry=site(0x58ea99);
    g_cityFlagsEntry=site(0x58e9f8);*(uint8_t*)site(0x58e9fd)=0xc3;
    void* scenarioVt[2]={nullptr,unrestFixtureRule};void* scenario=scenarioVt;
    auto scenarioOperand=(uint32_t*)site(0x58cf6f);uint32_t oldScenario=*scenarioOperand;*scenarioOperand=(uint32_t)&scenario;
    uint32_t regionJumps[5],flagJumps[4];
    for(int i=0;i<5;++i)regionJumps[i]=(uint32_t)site(*(uint32_t*)site(0x58bbc0+i*4));
    for(int i=0;i<4;++i)flagJumps[i]=(uint32_t)site(*(uint32_t*)site(0x58eb50+i*4));
    auto regionByteOperand=(uint32_t*)site(0x58bb98);uint32_t oldRegionByte=*regionByteOperand;
    auto regionJumpOperand=(uint32_t*)site(0x58bb9f);uint32_t oldRegionJump=*regionJumpOperand;
    auto flagJumpOperand=(uint32_t*)site(0x58ea6a);uint32_t oldFlagJump=*flagJumpOperand;
    *regionByteOperand=(uint32_t)site(0x58bbd4);*regionJumpOperand=(uint32_t)regionJumps;*flagJumpOperand=(uint32_t)flagJumps;
    bool saved[250];memcpy(saved,g_skills[249].effects,sizeof saved);memset(g_skills[249].effects,0,sizeof saved);
    g_skills[249].effects[95]=true;int flags[100]={};
    *(int*)(first+0xe8)=95;*(int*)(second+0xe8)=-1;record[0x85]=70;
    runCityFlags(flags,&city,0);int calls=g_unrestChanceCalls;
    assert(runUnrestRoll(flags,record)==0 && calls==g_unrestChanceCalls && !g_unrestGateProbe);
    assert(skilltelemetry::open(L"build/unrest_telemetry_test.csv",unrestFixtureActor,nullptr));
    for(int mode=0;mode<4;++mode)for(int order:{59,60,79,80}) {
        *(int*)(first+0xe8)=mode==2?-1:mode==1?249:95;*(int*)(second+0xe8)=mode>=2?95:-1;
        auto incomeCity=g_incomeProviders.city;runCityFlags(flags,&city,0);assert(g_incomeProviders.city==incomeCity);
        record[0x85]=(uint8_t)order;calls=g_unrestChanceCalls;
        assert(runUnrestRoll(flags,record)==(order<60));assert(g_unrestChanceCalls==calls+(order<60));
    }
    *(int*)(first+0xe8)=95;*(int*)(second+0xe8)=-1;record[0x85]=70;runCityFlags(flags,&city,0);
    calls=g_unrestChanceCalls;assert(nativeUnrestRoll(flags,record)==0 && calls==g_unrestChanceCalls);
    g_unrestScenarioPresent=g_unrestScenarioBlock=true;assert(runUnrestRoll(flags,record)==0);
    g_unrestScenarioBlock=false;assert(runUnrestRoll(flags,record)==0);g_unrestScenarioPresent=false;
    g_unrestRecordValid=false;assert(runUnrestRoll(flags,record)==0);g_unrestRecordValid=true;
    int copied[100];memcpy(copied,flags,sizeof copied);assert(runUnrestRoll(copied,record)==0);
    g_unrestCityForRecord=&otherCity;assert(runUnrestRoll(flags,record)==0);g_unrestCityForRecord=&city;
    flags[95]=0;calls=g_unrestChanceCalls;assert(runUnrestRoll(flags,record)==1 && g_unrestChanceCalls==calls+1);
    runCityFlags(flags,&city,0);assert(runUnrestRoll(flags,record)==0 && runUnrestRoll(flags,record)==0);
    *(int*)(first+0xe8)=-1;runCityFlags(flags,&city,0);assert(runUnrestRoll(flags,record)==1);

    memset(g_skills[249].effects,0,sizeof saved);for(int i=91;i<=94;++i)g_skills[249].effects[i]=true;
    uint32_t forceOperands[]={0x58bba4,0x58bbaa,0x58bbb0,0x58bbb6,0x58ea95};
    int oldForces[5];for(int i=0;i<5;++i)oldForces[i]=*(int*)site(forceOperands[i]);
    uint8_t oldRange[6];memcpy(oldRange,(void*)site(0x58ea5f),6);
    uint32_t flagOperands[]={0x58ea71,0x58ea7a,0x58ea83,0x58ea8c};int oldOffsets[4];
    for(int i=0;i<4;++i)oldOffsets[i]=*(int*)site(flagOperands[i]);
    struct Region {int city,effect,relative;};Region regions[]={{0,91,2},{19,92,0},{23,93,1},{38,94,3}};
    for(int base:{42,249}) {
        int delta=base==42?0:0xf40;g_unrestFrame=0x310+delta;g_unrestFlagOffset=0x180+delta;
        int forces[]={base+2,base+1,base,base+3,base+4};
        for(int i=0;i<5;++i)*(int*)site(forceOperands[i])=forces[i];
        for(int i=0;i<4;++i)*(int*)site(flagOperands[i])=oldOffsets[i]+delta;
        if(base==249) {
            // Same widened LEA/CMP/JMP sequence as the installed M8 cave at 0x58ea5f.
            auto cave=(uint8_t*)site(0x88f000);memcpy(cave,"\x8d\x88\0\0\0\0\x83\xf9\x03\xe9\0\0\0\0",14);
            *(int32_t*)(cave+2)=-base;*(int32_t*)(cave+10)=(int32_t)(site(0x58ea65)-(uintptr_t)cave-14);
            domesticStub(0x58ea5f,cave);*(uint8_t*)site(0x58ea64)=0x90;
        }
        for(auto region:regions)for(int mode=0;mode<4;++mode) {
            city.id=region.city;*(int*)(first+0xe8)=mode==2?-1:mode==1?249:region.effect;
            *(int*)(second+0xe8)=mode>=2?region.effect:-1;runCityFlags(flags,&city,0);
            g_unrestExpectedForce=base+4;int created=g_unrestWorldCreates;
            assert(runUnrestCamp(&city,flags,region.city)==&camp && camp.force==base+4);
            assert(g_unrestWorldCreates==created+1 && g_unrestTribe==-1);
        }
        city.id=0;*(int*)(first+0xe8)=92;*(int*)(second+0xe8)=-1;runCityFlags(flags,&city,0);
        g_unrestExpectedForce=base+2;assert(runUnrestCamp(&city,flags,0)==&camp); // unrelated affinity
        *(int*)(first+0xe8)=91;city.id=3;runCityFlags(flags,&city,0);
        g_unrestExpectedForce=base+4;assert(runUnrestCamp(&city,flags,3)==&camp); // ordinary bandits, no tribe
        city.id=0;runCityFlags(flags,&city,0);g_unrestCampFails=true;
        assert(!runUnrestCamp(&city,flags,0));g_unrestCampFails=false;
        assert(runUnrestCamp(&city,flags,0,0)==&camp); // no stale kind from failed creation
        runCityFlags(flags,&city,0);g_unrestCampRegistered=false;
        assert(runUnrestCamp(&city,flags,0)==&camp);g_unrestCampRegistered=true;
        runCityFlags(flags,&city,0);g_unrestCampWrongForce=true;
        assert(runUnrestCamp(&city,flags,0)==&camp);g_unrestCampWrongForce=false;
        runCityFlags(flags,&city,0);flags[91]=0;g_unrestExpectedForce=base+2;
        assert(runUnrestCamp(&city,flags,0)==&camp);
        runCityFlags(flags,&city,0);g_unrestExpectedForce=base+4;g_unrestExpectedCity=&otherCity;
        assert(runUnrestCamp(&otherCity,flags,0)==&camp);g_unrestExpectedCity=&city;
        runCityFlags(flags,&city,0);*(int*)(first+0xe8)=-1;
        assert(runUnrestCamp(&city,flags,0)==&camp);
        *(int*)(first+0xe8)=91;runCityFlags(flags,&city,0);
        assert(runUnrestCamp(&city,flags,0)==&camp && runUnrestCamp(&city,flags,0)==&camp);
    }
    skilltelemetry::close();
    unrestCollectionContextTests(city,otherCity,first,second,record);
    assert(!g_unrestGateProbe);memcpy(g_skills[249].effects,saved,sizeof saved);
    for(int i=0;i<5;++i)*(int*)site(forceOperands[i])=oldForces[i];
    for(int i=0;i<4;++i)*(int*)site(flagOperands[i])=oldOffsets[i];memcpy((void*)site(0x58ea5f),oldRange,6);
    *scenarioOperand=oldScenario;*regionByteOperand=oldRegionByte;*regionJumpOperand=oldRegionJump;*flagJumpOperand=oldFlagJump;
    g_cityIncomeFlagsOriginal=oldFlags;g_cityBuilding=oldCity;g_telemetryBaseId=oldId;g_baseById=oldBase;
    g_unrestRollOriginal=oldRoll;g_unrestCampOriginal=oldCamp;g_unrestValid=oldValid;
    g_unrestForceBase=oldForceBase;g_unrestTribeOriginal=oldTribe;g_unrestThresholdResume=oldThresholdResume;
    g_unrestProviders=IncomeProviders{};g_unrestFlags=nullptr;g_unrestTribe=-1;
    puts("PASS native unrest: threshold/RNG gates, four affinities, real constructor call/validation, stock/expanded forces and frames, failure/preview/stale exclusions");
}
