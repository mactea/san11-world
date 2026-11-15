// Execute the actual research gold settlement tail and native calculator/setter.
static uintptr_t g_researchGoldEntry;
static void* g_researchFixtureBase;
static void* __fastcall researchFixtureBase(void*,void*) {return g_researchFixtureBase;}
__declspec(naked) static int __cdecl runResearchGold(void* command,void* city,void* research) {
    __asm {
        push ebx
        push esi
        sub esp,0x40
        mov ebx,[esp+0x4c]
        mov eax,[esp+0x50]
        mov [esp+0x2c],eax
        mov eax,[esp+0x54]
        mov [esp+0x20],eax
        mov eax,g_researchGoldEntry
        jmp eax
    }
}
__declspec(naked) static void researchFixtureGoldDone() {
    __asm {add esp,0x40
        pop esi
        pop ebx
        ret}
}
#include "research_owner_telemetry_cases.inc"
static void researchTelemetryTests() {
    uint8_t actor[400]={},other[400]={},person[400]={},spouse[400]={},city[0x100]={},research[0x70]={};
    g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* baseVt[24]={};baseVt[0x40/4]=upkeepForce;
    UpkeepBuilding base={baseVt,0,0,0,0,2,8,0,0,0};g_researchFixtureBase=&base;
    void* command[]={&base,actor,other,nullptr};auto members=command+1;
    auto oldCost=g_researchGoldCostOriginal;auto oldGrant=g_researchGoldGrantOriginal;
    auto oldCityBase=g_cityBuilding;auto oldBaseId=g_telemetryBaseId;auto oldValid=g_growthValid;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldBaseById=g_baseById;
    bool old84=g_skills[249].effects[84];g_skills[249].effects[84]=true;
    g_researchGoldCostOriginal=(ResearchGoldCost)site(0x5d7820);
    g_researchGoldGrantOriginal=(IncomeGrant)site(0x4b3ba0);
    g_cityBuilding=(CityBuilding)researchFixtureBase;g_telemetryBaseId=(BuildingId)upkeepBaseId;g_growthValid=domesticValid;
    g_telemetryPerson=(PersonById)researchOwnerPerson;g_telemetryPersonId=(BuildingId)personFixtureId;g_baseById=(BaseById)researchOwnerBuilding;
    domesticStub(0x47a630,domesticValid);domesticStub(0x5d8f47,researchFixtureGoldDone);
    g_researchGoldEntry=site(0x5d8f24);
    *(int*)(actor+0xe8)=*(int*)(other+0xe8)=-1;
    for(int cost:{0,101,65535})for(int before:{0,100,1000,100000}) {
        *(uint16_t*)(research+0x5e)=(uint16_t)cost;*(int*)(city+0x44)=before;
        int after=before-cost;if(after<0)after=0;
        assert(runResearchGold(command,city,research)==after-before && *(int*)(city+0x44)==after);
    }
    *(int*)(actor+0xe8)=84;*(int*)(other+0xe8)=-1;*(uint16_t*)(research+0x5e)=101;*(int*)(city+0x44)=1000;
    assert(runResearchGold(command,city,research)==-50 && *(int*)(city+0x44)==950);
    assert(skilltelemetry::open(L"build/research_telemetry_test.csv",personFixtureActor,nullptr));
    for(int mode=0;mode<4;++mode)for(int cost:{0,101,65535})for(int before:{0,100,1000,100000}) {
        *(int*)(actor+0xe8)=mode==2?-1:mode==1?249:84;*(int*)(other+0xe8)=mode>=2?84:-1;
        *(uint16_t*)(research+0x5e)=(uint16_t)cost;*(int*)(city+0x44)=before;
        int after=before-cost/2;if(after<0)after=0;
        assert(runResearchGold(command,city,research)==after-before && *(int*)(city+0x44)==after);
        assert(!g_researchGoldPending.actor && !g_progressionProbe);
    }
    *(int*)(actor+0xe8)=*(int*)(other+0xe8)=-1;
    *(uint16_t*)(research+0x5e)=101;*(int*)(city+0x44)=1000;
    assert(runResearchGold(command,city,research)==-101 && *(int*)(city+0x44)==899); // ordinary cost
    *(int*)(actor+0xe8)=84;
    assert(g_researchGoldCostOriginal(members,research)==50); // preview is not an applied debit
    *(int*)(city+0x44)=1000;assert(researchGoldGrant(nullptr,nullptr,city,-50)==-50);
    assert(researchGoldCost(members,research)==50);
    assert(researchGoldGrant(nullptr,nullptr,city,-49)==-49 && !g_researchGoldPending.actor); // mismatched request
    assert(g_researchGoldCostOriginal(nullptr,research)==101);
    assert(researchGoldCost(members,nullptr)==0 && !g_researchGoldPending.actor);
    skilltelemetry::close();
    researchOwnerTelemetryTests(actor,other,city,research,command,base);
    g_skills[249].effects[84]=old84;g_researchGoldCostOriginal=oldCost;g_researchGoldGrantOriginal=oldGrant;
    g_cityBuilding=oldCityBase;g_telemetryBaseId=oldBaseId;g_growthValid=oldValid;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_baseById=oldBaseById;
    puts("PASS native research: guidance gold halving, odd/zero/max cost, real treasury clamp/store, first/composed/high-ID provider, previews and stale debit exclusions");
}
