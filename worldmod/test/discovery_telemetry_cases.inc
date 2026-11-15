// Run the native execution dispatch, chance/selection helper, random comparison,
// discovery state setter and location store. World lists and narrative/recruitment
// are fixtures; no native random test or state write is replayed by telemetry.
static uintptr_t g_discoveryEntry,g_discoveryStoreEntry;
static int g_discoveryCandidates=1,g_discoveryRoll=0,g_discoveryRandomCalls=0;
static int g_discoveryListCalls=0,g_discoveryHandlerCalls=0,g_discoveryStoreCalls=0;
static int g_discoveryCity=8,g_discoverySortCalls=0;
static bool g_discoveryRecruit=false,g_discoveryWrite=true;
static void* __fastcall discoveryFixtureBase(void* city,void*) {return city;}
static void __fastcall discoveryFixtureCandidates(void*,void*,uint8_t* list,void*,void*,int) {
    ++g_discoveryListCalls;*(int*)(list+0xc)=g_discoveryCandidates;*(void**)(list+0x1c)=g_personTarget;
}
static void* __fastcall discoveryFixtureFirst(uint8_t* list,void*) {return list+0x1c;}
static int __fastcall discoveryFixtureStat(void*,void*) {return 50;}
static int __fastcall discoveryFixtureAffinity(void*,void*,int) {return 25;}
static int __cdecl discoveryFixtureSeed() {++g_discoveryRandomCalls;return 1234;}
static int __cdecl discoveryFixtureRandom(int seed,int,int,int,int,int,int) {assert(seed==1234);return g_discoveryRoll;}
static void* __fastcall discoveryFixtureCity(void*,void*,int) {return nullptr;}
static void __fastcall discoveryFixtureSort(void*,void*,int,void*,void*,void*) {++g_discoverySortCalls;}
__declspec(naked) static void __cdecl runDiscoveryStore(void* person,int city) {
    __asm {
        mov eax,[esp+8]
        push eax
        mov eax,[esp+8]
        push eax
        xor ecx,ecx
        mov eax,g_discoveryStoreEntry
        jmp eax
    }
}
static int __cdecl discoveryFixtureHandler(uint8_t* person,uint8_t*,void*) {
    ++g_discoveryHandlerCalls;
    if(g_discoveryWrite){++g_discoveryStoreCalls;runDiscoveryStore(person,g_discoveryCity);}
    if(g_discoveryRecruit)*(int*)(person+0xa0)=3; // separate later recruitment, not a discovery row
    return 19;
}
__declspec(naked) static int __cdecl runDiscovery(void* actor,void* city,void* base) {
    __asm {
        push ebp
        push ebx
        push esi
        push edi
        sub esp,0x80
        mov esi,[esp+0x94]
        mov edi,[esp+0x98]
        mov ebp,[esp+0x9c]
        xor ebx,ebx
        mov [esp+0x10],ebx
        mov eax,g_discoveryEntry
        jmp eax
    }
}
__declspec(naked) static void discoveryFixtureDone() {
    __asm {mov eax,1
        add esp,0x80
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret}
}
__declspec(naked) static void discoveryFixtureEmpty() {
    __asm {xor eax,eax
        add esp,0x80
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret}
}
__declspec(naked) static void discoveryFixtureStoreDone() {__asm {ret}}
static void discoveryTelemetryTests() {
    uint8_t person[400]={},actor[400]={},other[400]={},spouse[400]={};
    g_personTarget=person;g_personGuide=actor;g_personOther=other;g_personSpouse=spouse;
    void* vt[24]={};vt[0x40/4]=personFixtureForeignOwner;*(void**)person=vt;
    void* baseVt[24]={};baseVt[0x40/4]=upkeepForce;
    UpkeepBuilding base={baseVt,0,0,0,0,2,8,0,0,0};
    uint8_t city[64]={};*(int*)(city+0x18)=8;*(int*)(actor+0xe4)=9;
    auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryPersonId;auto oldValid=g_growthValid;
    auto oldBase=g_telemetryBaseId;auto oldOutcome=g_explorationOutcomeOriginal;
    auto oldHandler=g_discoveryHandlerOriginal;auto oldStore=g_discoveryStoreOriginal;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)personFixtureId;
    g_growthValid=domesticValid;g_telemetryBaseId=(BuildingId)upkeepBaseId;
    g_explorationOutcomeOriginal=(ExplorationOutcome)site(0x5d1ea0);
    g_discoveryHandlerOriginal=discoveryFixtureHandler;g_discoveryStoreOriginal=(DiscoveryStore)site(0x4a5b20);
    g_discoveryEntry=site(0x5d5a49);g_discoveryStoreEntry=site(0x5d5703);
    domesticStub(0x47a630,domesticValid);domesticStub(0x47a600,domesticValid);
    domesticStub(0x47c4c0,discoveryFixtureBase);domesticStub(0x4cf590,discoveryFixtureCandidates);
    domesticStub(0x59cc10,discoveryFixtureFirst);domesticStub(0x491310,personFixtureId);
    domesticStub(0x489f80,discoveryFixtureAffinity);domesticStub(0x4890a0,discoveryFixtureStat);
    domesticStub(0x5b9c00,discoveryFixtureSeed);domesticStub(0x5ba4c0,discoveryFixtureRandom);
    domesticStub(0x490d00,discoveryFixtureCity);domesticStub(0x5d5450,discoveryFixtureSort);
    domesticStub(0x5d5c17,discoveryFixtureDone);domesticStub(0x5d5ac5,discoveryFixtureEmpty);
    domesticStub(0x5d5708,discoveryFixtureStoreDone);
    bool old85=g_skills[249].effects[85];g_skills[249].effects[85]=true;
    auto reset=[&] {
        *(int*)(person+0xa0)=7;*(int*)(person+0x98)=9;*(int*)(actor+0xe8)=85;
        g_discoveryCandidates=1;g_discoveryRoll=99;g_discoveryCity=8;
        g_discoveryRecruit=false;g_discoveryWrite=true;g_personRegistered=true;
    };
    reset();assert(runDiscovery(actor,city,&base)==1 && *(int*)(person+0xa0)==4);
    assert(skilltelemetry::open(L"build/discovery_telemetry_test.csv",personFixtureActor,nullptr));
    for(auto searcher:{actor,other})for(int owned:{85,249})for(int count:{1,3})for(int requestedCity:{8,530,-1})for(bool recruit:{false,true}) {
        reset();*(int*)(searcher+0xe8)=owned;g_discoveryCandidates=count;
        g_discoveryCity=requestedCity;g_discoveryRecruit=recruit;
        int rolls=g_discoveryRandomCalls,lists=g_discoveryListCalls,stores=g_discoveryStoreCalls,sorts=g_discoverySortCalls;
        assert(runDiscovery(searcher,city,&base)==1);
        assert(g_discoveryRandomCalls==rolls+1 && g_discoveryListCalls==lists+1 && g_discoveryStoreCalls==stores+1);
        assert(g_discoverySortCalls==sorts+(count>1));
        assert(*(int*)(person+0xa0)==(recruit?3:4) && *(int*)(person+0x98)==(requestedCity<0?9:requestedCity));
        assert(!g_discoveryCommit && !g_discoveryPending.actor && !g_progressionProbe);
    }
    reset();g_discoveryCandidates=0;int rolls=g_discoveryRandomCalls,handlers=g_discoveryHandlerCalls;
    assert(runDiscovery(actor,city,&base)==0 && g_discoveryRandomCalls==rolls && g_discoveryHandlerCalls==handlers);
    reset();g_discoveryRoll=100;assert(runDiscovery(actor,city,&base)==0 && !g_discoveryPending.actor);
    reset();*(int*)(actor+0xe8)=-1;g_discoveryRoll=0;
    assert(runDiscovery(actor,city,&base)==1 && *(int*)(person+0xa0)==4); // ordinary success is not skill85
    reset();*(int*)(person+0xa0)=4;assert(runDiscovery(actor,city,&base)==1); // no new discovery
    reset();g_personRegistered=false;assert(runDiscovery(actor,city,&base)==1);g_personRegistered=true;
    reset();g_discoveryWrite=false;assert(runDiscovery(actor,city,&base)==1 && *(int*)(person+0xa0)==7);
    reset();uint8_t candidates[32]={};rolls=g_discoveryRandomCalls;
    assert(g_explorationOutcomeOriginal(actor,city,candidates)==1 && g_discoveryRandomCalls==rolls+1);
    assert(!g_discoveryPending.actor); // prediction-only query cannot authorize an applied row
    discoveryHandler(person,actor,&base);assert(*(int*)(person+0xa0)==4); // no actual-command predicate
    *(int*)(person+0xa0)=7;runDiscoveryStore(person,8);assert(*(int*)(person+0xa0)==4); // setter without handler scope
    skilltelemetry::close();assert(!g_discoveryCommit && !g_discoveryPending.actor);
    g_skills[249].effects[85]=old85;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldId;g_growthValid=oldValid;
    g_telemetryBaseId=oldBase;g_explorationOutcomeOriginal=oldOutcome;g_discoveryHandlerOriginal=oldHandler;g_discoveryStoreOriginal=oldStore;
    puts("PASS native discovery: execution dispatch, chance/random once, hidden-to-free write, composed85, preview/no-op exclusion");
}
