// The seasonal runner/UI/world traversal are fixtures. Its scope encloses the
// native preference branches, real linked-list append and real pending-flag setter.
static uintptr_t g_harvestCandidateEntry,g_harvestFlagEntry;
static void* g_harvestListVt;
static uint8_t g_harvestNodes[4][12];
static int g_harvestNodeCount=0,g_harvestSample=0,g_harvestSampleCalls=0;
static bool g_harvestAllocationFails=false,g_harvestSelect=true,g_harvestTwoCities=false;
static uint8_t* g_harvestFixtureCity;
static uint8_t* g_harvestSecondCity;
static UpkeepBuilding* g_harvestFixtureBase;
static UpkeepBuilding* g_harvestSecondBase;
static int g_harvestFixtureMode=0;
static int __fastcall harvestFixtureFind(void* base,void*,int effect) {
    if(base==g_harvestSecondBase && personHas(g_personOther,nullptr,effect))return 14162;
    return environmentFixtureFind(base,nullptr,effect);
}
static void* __fastcall harvestFixtureBaseById(void*,void*,int id) {
    if(!g_environmentRegistered)return nullptr;
    return id==g_harvestFixtureBase->id?g_harvestFixtureBase:id==g_harvestSecondBase->id?g_harvestSecondBase:nullptr;
}
static uint8_t* __fastcall harvestFixtureAllocate(uint8_t* list,void*,void* previous,int zero) {
    assert(zero==0);if(g_harvestAllocationFails)return nullptr;
    assert(g_harvestNodeCount<4);auto node=g_harvestNodes[g_harvestNodeCount++];memset(node,0,12);
    *(void**)(node+4)=previous;++*(int*)(list+0xc);return node;
}
static void __stdcall harvestFixtureLock(void*) {}
static int __cdecl harvestFixtureRoll(int percent) {assert(percent==50);++g_harvestSampleCalls;return g_harvestSample;}
__declspec(naked) static int __cdecl runHarvestCandidate(void* city) {
    __asm {push ebx
        push ebp
        push esi
        push edi
        sub esp,0x100
        mov esi,[esp+0x114]
        xor ebx,ebx
        xor ebp,ebp
        mov eax,g_harvestListVt
        mov [esp+0x1c],eax
        mov [esp+0x3c],eax
        mov [esp+0x20],ebp
        mov [esp+0x24],ebp
        mov [esp+0x28],ebp
        mov [esp+0x40],ebp
        mov [esp+0x44],ebp
        mov [esp+0x48],ebp
        mov eax,g_harvestCandidateEntry
        jmp eax}
}
__declspec(naked) static void harvestFixtureCandidateDone() {
    __asm {mov eax,[esp+0x28]
        add esp,0x100
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret}
}
__declspec(naked) static void __cdecl runHarvestFlag(void* city) {
    __asm {push esi
        mov esi,[esp+8]
        mov eax,g_harvestFlagEntry
        jmp eax}
}
__declspec(naked) static void harvestFixtureFlagDone() {__asm {pop esi
    ret}}
__declspec(naked) static void harvestFixturePrologDone() {__asm {mov esp,ebp
    pop ebp
    mov eax,29
    ret}}
static int __fastcall harvestFixtureRun(void*,void*) {
    g_environmentFixtureBase=g_harvestFixtureBase;g_harvestNodeCount=0;
    int admitted=runHarvestCandidate(g_harvestFixtureCity);
    if(g_harvestTwoCities) {
        *(int*)(g_personOther+0xe8)=98;
        g_environmentFixtureBase=g_harvestSecondBase;
        assert(runHarvestCandidate(g_harvestSecondCity)==1);
        runHarvestFlag(g_harvestSecondCity);runHarvestFlag(g_harvestFixtureCity);runHarvestFlag(g_harvestFixtureCity);
    } else if(g_harvestSelect) {
        if(g_harvestFixtureMode==1)g_environmentRegistered=false;
        if(g_harvestFixtureMode==2)*(int*)(g_personGuide+0xe8)=*(int*)(g_personOther+0xe8)=-1;
        if(g_harvestFixtureMode==3)runHarvestFlag(g_harvestSecondCity);
        else if(admitted)runHarvestFlag(g_harvestFixtureCity);
        else g_harvestFlagOriginal(g_harvestFixtureCity,2,1); // ordinary-pool selection has no priority claim
    }
    return 31;
}
static void harvestTelemetryTests() {
    uint8_t city[0x110]={},secondCity[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    g_harvestFixtureCity=city;g_harvestSecondCity=secondCity;
    void* vt[24]={};vt[0x40/4]=upkeepForce;vt[0x5c/4]=harvestFixtureFind;
    UpkeepBuilding base={vt,0,0,0,0,2,8,0,0,0},secondBase=base;secondBase.id=9;
    g_harvestFixtureBase=&base;g_harvestSecondBase=&secondBase;
    void* listVt[]={harvestFixtureAllocate};g_harvestListVt=listVt;
    auto oldPerson=g_telemetryPerson;auto oldBaseId=g_telemetryBaseId;auto oldBaseById=g_baseById;auto oldValid=g_growthValid;
    auto oldRun=g_environmentRunOriginal;auto oldRoll=g_harvestRollOriginal;auto oldAppend=g_harvestAppendOriginal;auto oldFlag=g_harvestFlagOriginal;
    auto oldRunResume=g_environmentRunResume,oldFindResume=g_harvestFindResume;auto oldBound=g_harvestHolderBound;
    auto bound=(int*)site(0x58f68d);int savedBound=*bound;g_harvestHolderBound=bound;
    g_environmentRunResume=(uint32_t)harvestFixturePrologDone;
    assert(((EnvironmentRun)environmentRunNative)(nullptr)==29); // overwritten native prologue balances its frame
    g_environmentRunResume=oldRunResume;
    g_environmentRunOriginal=(EnvironmentRun)harvestFixtureRun;g_harvestRollOriginal=harvestFixtureRoll;
    g_harvestAppendOriginal=(HarvestAppend)site(0x4ba130);g_harvestFlagOriginal=(HarvestFlag)site(0x47b580);
    g_harvestFindResume=(uint32_t)site(0x58f68a);g_harvestCandidateEntry=site(0x58f62d);g_harvestFlagEntry=site(0x58f745);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryBaseId=(BuildingId)upkeepBaseId;
    g_baseById=(BaseById)harvestFixtureBaseById;g_growthValid=domesticValid;
    domesticStub(0x47a630,domesticValid);domesticStub(0x47c4c0,environmentFixtureBase);
    domesticStub(0x58f6af,harvestFixtureCandidateDone);domesticStub(0x58f7dd,harvestFixtureCandidateDone);
    domesticStub(0x58f750,harvestFixtureFlagDone);
    memcpy((void*)site(0x4ba130),"\x6a\xff\x68\xd8\xfb",5); // previous environment fixture's append input
    uintptr_t pointerCheck=(uintptr_t)environmentFixturePointer,lock=(uintptr_t)harvestFixtureLock;
    uint32_t operands[]={0x472544,0x472551,0x4725b7,0x4ba151,0x4ba176,0x4ba1ae},saved[6];
    for(int i=0;i<6;++i){auto p=(uint32_t*)site(operands[i]);saved[i]=*p;*p=(uint32_t)(i<3?&pointerCheck:&lock);}
    bool old98=g_skills[249].effects[98];g_skills[249].effects[98]=true;
    auto reset=[&] {
        *(uint32_t*)(city+0x9c)=*(uint32_t*)(city+0xa0)=0;*(uint32_t*)(secondCity+0xa0)=0;
        *(int*)(actor+0xe8)=98;*(int*)(other+0xe8)=-1;
        g_environmentForcedHolder=-2;g_environmentRegistered=true;g_harvestAllocationFails=false;
        g_harvestSelect=true;g_harvestTwoCities=false;g_harvestFixtureMode=0;g_harvestSample=1;
    };
    auto run=(EnvironmentRun)site(0x58f4e0);reset();assert(run(nullptr)==31 && !g_harvestScope);
    assert(skilltelemetry::open(L"build/harvest_telemetry_test.csv",personFixtureActor,nullptr));
    for(int layout=0;layout<2;++layout) {
        *bound=layout?14612:1099;base.id=layout?529:8;secondBase.id=layout?528:9;
        for(int mode=0;mode<4;++mode)for(int sample:{0,1})for(bool select:{false,true}) {
            reset();*(int*)(actor+0xe8)=mode==2?-1:mode==1?249:98;*(int*)(other+0xe8)=mode>=2?98:-1;
            g_harvestSample=sample;g_harvestSelect=select;int calls=g_harvestSampleCalls;
            assert(run(nullptr)==31 && !g_harvestScope && !g_progressionProbe);
            assert(g_harvestSampleCalls==calls+(layout || mode!=2));
            assert(*(uint32_t*)(city+0xa0)==(select?4:0));
        }
        for(int holder:{-1,0,1099,1100,14162,14612,14613}) {
            reset();*(int*)(actor+0xe8)=*(int*)(other+0xe8)=-1;g_environmentForcedHolder=holder;
            int calls=g_harvestSampleCalls;
            assert(run(nullptr)==31 && g_harvestSampleCalls==calls+(holder>=0 && holder<=*bound));
            assert(*(uint32_t*)(city+0xa0)==4 && !g_harvestScope);
        }
    }
    for(int mode=1;mode<=3;++mode){reset();g_harvestFixtureMode=mode;assert(run(nullptr)==31 && !g_harvestScope);}
    reset();g_harvestAllocationFails=true;assert(run(nullptr)==31); // failed append cannot authorize an effect
    reset();*(uint32_t*)(city+0x9c)=4;int calls=g_harvestSampleCalls;
    assert(run(nullptr)==31 && g_harvestSampleCalls==calls); // native harvest precondition before query
    reset();g_harvestTwoCities=true;assert(run(nullptr)==31 && !g_harvestScope); // reverse selection + duplicate setter
    reset();g_environmentFixtureBase=&base;g_harvestNodeCount=0;
    assert(runHarvestCandidate(city)==1 && !g_harvestScope);runHarvestFlag(city); // outside seasonal scope
    reset();runHarvestFlag(city); // next command cannot inherit previous priority membership
    skilltelemetry::close();assert(!g_harvestScope && !g_progressionProbe);
    for(int i=0;i<6;++i)*(uint32_t*)site(operands[i])=saved[i];*bound=savedBound;
    g_skills[249].effects[98]=old98;g_telemetryPerson=oldPerson;g_telemetryBaseId=oldBaseId;g_baseById=oldBaseById;g_growthValid=oldValid;
    g_environmentRunOriginal=oldRun;g_harvestRollOriginal=oldRoll;g_harvestAppendOriginal=oldAppend;g_harvestFlagOriginal=oldFlag;
    g_environmentRunResume=oldRunResume;g_harvestFindResume=oldFindResume;g_harvestHolderBound=oldBound;
    puts("PASS native harvest:98 preference branch and single sample, native list append/failure, actual pending flag, final selection correlation, scope lifetime, stock/expanded bounds");
}
