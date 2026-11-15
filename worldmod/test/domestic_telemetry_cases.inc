// Execute the patched command call sites and the native production/time calculators.
// World traversal, validity checks, attributes and facility coefficients are isolated stubs.
static uintptr_t g_domesticEntry;
__declspec(naked) static int __cdecl runDomestic3(void* a,uintptr_t b,int c) {
    __asm {
        mov edx,g_domesticEntry
        mov eax,[esp+12]
        push eax
        mov eax,[esp+12]
        push eax
        mov eax,[esp+12]
        push eax
        xor ecx,ecx
        jmp edx
    }
}
__declspec(naked) static int __cdecl runDomestic2(void* a,uintptr_t b) {
    __asm {
        mov edx,g_domesticEntry
        mov eax,[esp+8]
        push eax
        mov eax,[esp+8]
        push eax
        xor ecx,ecx
        jmp edx
    }
}
static void domesticStub(uint32_t address,const void* fn) {
    auto at=site(address);*(uint8_t*)at=0xe9;
    *(int32_t*)(at+1)=(int32_t)((uintptr_t)fn-at-5);
}
static int __cdecl domesticValid(void* p) {return p!=nullptr;}
static uint8_t __fastcall domesticStat(uint8_t* p,void*) {return p[200];}
static float __cdecl domesticCoefficient(void*,int) {return 0.375f;}
static float __cdecl domesticRecruitCoefficient(void*) {return 0.375f;}
static int __fastcall domesticAI(void*,void*) {return 0;}
__declspec(naked) static int __cdecl domesticTruncate() {
    __asm {
        sub esp,8
        fnstcw [esp]
        mov ax,[esp]
        or ax,0xc00
        mov [esp+2],ax
        fldcw [esp+2]
        fistp dword ptr [esp+4]
        fldcw [esp]
        mov eax,[esp+4]
        add esp,8
        ret
    }
}
static uint8_t g_domesticCity[0x248];
static UpkeepBuilding* g_domesticBase;
static int g_domesticTroops=0;
static void* __fastcall domesticBuilding(void*,void*) {return g_domesticBase;}
static void* __fastcall domesticBaseById(void*,void*,int id) {
    assert(id==8);return g_domesticBase;
}
static void* __fastcall domesticCity(void*,void*) {return g_domesticCity;}
static int __fastcall domesticNoTech(void*,void*,void*,int) {return 0;}
static int __fastcall domesticPersonId(void*,void*,const void* p) {
    assert(p==g_upkeepActor || p==g_secondIncomeActor);
    return p==g_upkeepActor?1500:1501;
}
static int __fastcall domesticTroops(void*,void*) {return g_domesticTroops;}
static int __fastcall domesticTroopMax(void*,void*) {return 100000;}
static int __fastcall domesticNoEnemy(void*,void*,void*) {return 0;}
static void __fastcall domesticSetTroops(void*,void*,int n) {g_domesticTroops=n;}
#include "domestic_owner_telemetry_cases.inc"
static void domesticTelemetryTests() {
    uint8_t person[400]={},second[400]={};void* people[3]={person,second,nullptr};
    g_upkeepActor=person;g_secondIncomeActor=second;person[200]=60;second[200]=50;
    *(int*)(person+0x98)=*(int*)(second+0x98)=8;
    void* vt[24]={};vt[0x40/4]=upkeepForce;vt[0x48/4]=domesticAI;vt[0x50/4]=domesticTroops;
    UpkeepBuilding building={vt,0,0,1000,300,2,8,1000,5000,5000};g_domesticBase=&building;
    *(void**)g_domesticCity=vt;
    g_domesticCity[0x85]=100;

    auto oldAmount=g_productionAmountOriginal;auto oldTime=g_productionTimeOriginal;
    auto oldRecruit=g_recruitAmountOriginal;auto oldGrant=g_weaponGrantOriginal;
    auto oldRecruitGrant=g_recruitGrantOriginal;auto oldValue=g_weaponValue;
    auto oldBuilding=g_cityBuilding;auto oldBaseId=g_telemetryBaseId;
    auto oldBaseById=g_baseById;auto oldPersonId=g_telemetryPersonId;auto oldAssign=g_assignTimeOriginal;
    auto oldPerson=g_telemetryPerson;g_telemetryPerson=(PersonById)domesticOwnerLookup;
    auto oldCapacity=g_domesticTroopCapacity;g_domesticTroopCapacity=(BuildingValue)domesticTroopMax;
    g_productionAmountOriginal=(ProductionAmount)site(0x5c63d0);
    g_productionTimeOriginal=(ProductionTime)site(0x5c6ca0);
    g_recruitAmountOriginal=(RecruitAmount)site(0x5c3610);
    g_weaponGrantOriginal=(WeaponGrant)site(0x4b4040);g_weaponValue=(WeaponValue)site(0x47b350);
    g_recruitGrantOriginal=(IncomeGrant)site(0x4ae3c0);
    g_cityBuilding=(CityBuilding)domesticBuilding;g_telemetryBaseId=(BuildingId)upkeepBaseId;
    g_baseById=(BaseById)domesticBaseById;g_telemetryPersonId=(BuildingId)domesticPersonId;
    g_assignTimeOriginal=(AssignTime)site(0x4a5660);

    domesticStub(0x47a630,domesticValid);domesticStub(0x489090,domesticStat);
    domesticStub(0x4890b0,domesticStat);domesticStub(0x5c3550,domesticRecruitCoefficient);
    domesticStub(0x486bc0,domesticTroopMax);domesticStub(0x4b99b0,domesticNoEnemy);
    domesticStub(0x487230,domesticSetTroops);
    domesticStub(0x5c5f90,domesticCoefficient);domesticStub(0x707a74,domesticTruncate);
    domesticStub(0x490d00,domesticBaseById);domesticStub(0x4866f0,domesticCity);
    domesticStub(0x49dab0,domesticNoTech);
    // The stock difficulty flag is at an absolute world address. Force the native AI-doubling branch.
    memcpy((void*)site(0x5c64ae),"\x33\xc0\x83\xf8\x00\x90\x90",7);
    memcpy((void*)site(0x5c3717),"\x33\xc0\x83\xf8\x00\x90\x90",7);
    // Relocate the native production-time switch tables (absolute addresses inside otherwise relative code).
    *(uint32_t*)site(0x5c6cd1)=(uint32_t)site(0x5c6e40);
    *(uint32_t*)site(0x5c6cd8)=(uint32_t)site(0x5c6e34);
    for(int i=0;i<3;++i)*(uint32_t*)site(0x5c6e34+i*4)=(uint32_t)site(*(uint32_t*)site(0x5c6e34+i*4));

    memcpy((void*)site(0x5c6715),"\x83\xc4\x0c\xc3",4);
    memcpy((void*)site(0x5c3b93),"\x83\xc4\x08\xc3",4);
    memcpy((void*)site(0x5c7054),"\x83\xc4\x08\xc3",4);
    for(uint32_t at:{0x5c6725u,0x5c3ba9u,0x5c70c8u})*(uint8_t*)site(at)=0xc3;
    bool saved[4];int effects[]={80,81,82,83};
    for(int i=0;i<4;++i){saved[i]=g_skills[249].effects[effects[i]];g_skills[249].effects[effects[i]]=true;}
    *(int*)(person+0xe8)=249;*(int*)(second+0xe8)=249;
    g_domesticEntry=site(0x5c6710);
    assert(runDomestic3(g_domesticCity,(uintptr_t)people,1)==2774);
    assert(!g_domesticQuantity.actor && !g_domesticProbe); // disabled never calculates a baseline
    assert(skilltelemetry::open(L"build/domestic_telemetry_test.csv",economyActor,nullptr));

    for(int weapon:{1,4})for(int before:{1000,98500,99500,100000}) {
        *(int*)(g_domesticCity+0x4c+weapon*4)=before;
        g_domesticEntry=site(0x5c6710);
        int requested=runDomestic3(g_domesticCity,(uintptr_t)people,weapon);
        assert(requested==2774 && g_domesticQuantity.normal==1386 && g_domesticQuantity.actor==person);
        assert(!g_domesticProbe);
        g_domesticEntry=site(0x5c6720);
        int granted=runDomestic3(g_domesticCity,weapon,requested);
        assert(granted==(before==1000?2774:100000-before));
        assert(!g_domesticQuantity.actor);
    }
    for(int before:{1000,98500,99500,100000}) {
        g_domesticTroops=before;g_domesticEntry=site(0x5c3b8e);
        int requested=runDomestic2(&building,(uintptr_t)people);
        assert(requested==(before==1000?1866:100000-before));
        assert(g_domesticQuantity.actor==person && !g_domesticProbe);
        g_domesticEntry=site(0x5c3ba4);
        assert(runDomestic2(&building,requested)==requested && !g_domesticQuantity.actor);
    }
    for(int weapon:{5,6,7,8,10,11}) {
        g_domesticEntry=site(0x5c704f);
        int duration=runDomestic2(people,weapon);
        assert(duration==4 && g_domesticTime.normal==9 && g_domesticTime.actor==person);
        assert(!g_domesticProbe);
        for(auto member:{person,second}) {
            *(int*)(member+0x13c)=2;*(int*)(member+0x140)=8;*(int*)(member+0x144)=weapon;
            member[0x158]=0;
            g_domesticEntry=site(0x5c70c3);runDomestic2(member,duration);
            assert(member[0x158]==4); // native 0x4a5660 -> 0x48a8b0 actually stored the byte
        }
        runDomestic2(person,duration); // repeated setter cannot duplicate an already recorded worker
    }
    g_domesticEntry=site(0x5c704f);runDomestic2(people,9);assert(!g_domesticTime.actor);
    g_domesticEntry=site(0x5c70c3);runDomestic2(person,4); // boat type 9 has no shipwright effect
    *(int*)(person+0xe8)=*(int*)(second+0xe8)=-1;
    g_domesticEntry=site(0x5c6710);assert(runDomestic3(g_domesticCity,(uintptr_t)people,1)==1386);
    assert(!g_domesticQuantity.actor && !g_domesticProbe);
    g_domesticEntry=site(0x5c6720);runDomestic3(g_domesticCity,1,1386); // no provider: no applied row
    // Mismatched requests are exercised in the complete native parent below.
    skilltelemetry::close();
    domesticOwnerTelemetryTests(person,second,building);

    for(int i=0;i<4;++i)g_skills[249].effects[effects[i]]=saved[i];
    g_productionAmountOriginal=oldAmount;g_productionTimeOriginal=oldTime;g_recruitAmountOriginal=oldRecruit;
    g_weaponGrantOriginal=oldGrant;g_recruitGrantOriginal=oldRecruitGrant;g_weaponValue=oldValue;
    g_cityBuilding=oldBuilding;g_telemetryBaseId=oldBaseId;g_baseById=oldBaseById;
    g_telemetryPersonId=oldPersonId;g_assignTimeOriginal=oldAssign;
    g_telemetryPerson=oldPerson;
    g_domesticTroopCapacity=oldCapacity;
    puts("PASS domestic telemetry: native amount/time arithmetic, native inventory caps and timer stores, exact counterfactual rounding, five effects and stale-request rejection");
}
