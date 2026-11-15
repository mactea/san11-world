// Execute native preconditions, all three protection queries/range branches,
// pending->active flag setters and candidate insertion. World traversal is isolated.
static uintptr_t g_environmentEntry;
static void* g_environmentFixtureBase;
static bool g_environmentRegistered=true;
static int g_environmentForcedHolder=-2,g_environmentCandidateAdds=0,g_environmentClimate=1;
static void* __fastcall environmentFixtureBase(void*,void*) {return g_environmentFixtureBase;}
static void* __fastcall environmentFixtureBaseById(void*,void*,int id) {
    return g_environmentRegistered && id==((UpkeepBuilding*)g_environmentFixtureBase)->id?g_environmentFixtureBase:nullptr;
}
static int __fastcall environmentFixtureFind(void*,void*,int effect) {
    if(g_environmentForcedHolder!=-2)return g_environmentForcedHolder;
    if(personHas(g_personGuide,nullptr,effect))return 600;
    if(personHas(g_personOther,nullptr,effect))return 14162;
    return -1;
}
static int __cdecl environmentFixtureClimate() {return g_environmentClimate;}
static int __fastcall environmentFixtureEligible(void*,void*,void*) {return 1;}
static void __fastcall environmentFixtureAppend(void*,void*,void*) {++g_environmentCandidateAdds;}
static int __stdcall environmentFixturePointer(void*,unsigned) {return 0;}
__declspec(naked) static int __cdecl runEnvironment(void* city,void* disaster) {
    __asm {push ebx
        push ebp
        push esi
        push edi
        sub esp,0x1000
        mov ebx,[esp+0x1014]
        mov esi,[esp+0x1018]
        xor ebp,ebp
        mov eax,g_environmentEntry
        jmp eax}
}
__declspec(naked) static void environmentFixtureNewDone() {
    __asm {mov eax,ebp
        add esp,0x1000
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret}
}
__declspec(naked) static void environmentFixtureSkipped() {
    __asm {xor eax,eax
        add esp,0x1000
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret}
}
__declspec(naked) static void environmentFixtureActivated() {
    __asm {mov eax,1
        add esp,0x1000
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret}
}
__declspec(naked) static void environmentFixtureSpreadDone() {
    __asm {mov eax,g_environmentCandidateAdds
        add esp,0x1000
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret}
}
static void environmentTelemetryTests() {
    uint8_t city[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[24]={};vt[0x40/4]=upkeepForce;vt[0x5c/4]=environmentFixtureFind;
    UpkeepBuilding base={vt,0,0,0,0,2,8,0,0,0};g_environmentFixtureBase=&base;
    void* disasterVt[10]={};disasterVt[2]=environmentFixtureEligible;void** disaster=disasterVt;
    auto oldPerson=g_telemetryPerson;auto oldBaseId=g_telemetryBaseId;auto oldBaseById=g_baseById;auto oldValid=g_growthValid;
    auto oldNewBound=g_environmentNewBound,oldActiveBound=g_environmentActiveBound,oldSpreadBound=g_environmentSpreadBound;
    auto oldNewResume=g_environmentNewResume,oldActiveResume=g_environmentActiveResume,oldSpreadResume=g_environmentSpreadResume;
    auto newBound=(int*)site(0x58f0cf),activeBound=(int*)site(0x58f28b),spreadBound=(int*)site(0x58f3be);
    int savedBounds[]={*newBound,*activeBound,*spreadBound};
    auto candidateOffset=(int*)site(0x58f0e4);int savedOffset=*candidateOffset;
    uintptr_t pointerCheck=(uintptr_t)environmentFixturePointer;
    uint32_t pointerOperands[]={0x472544,0x472551,0x4725b7},savedPointerOperands[3];
    for(int i=0;i<3;++i){auto p=(uint32_t*)site(pointerOperands[i]);savedPointerOperands[i]=*p;*p=(uint32_t)&pointerCheck;}
    g_environmentNewBound=newBound;g_environmentActiveBound=activeBound;g_environmentSpreadBound=spreadBound;
    g_environmentNewResume=(uint32_t)site(0x58f0cc);g_environmentActiveResume=(uint32_t)site(0x58f288);g_environmentSpreadResume=(uint32_t)site(0x58f3bb);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryBaseId=(BuildingId)upkeepBaseId;
    g_baseById=(BaseById)environmentFixtureBaseById;g_growthValid=domesticValid;
    domesticStub(0x47a630,domesticValid);domesticStub(0x47c4c0,environmentFixtureBase);
    // The earlier revenue fixture replaces this flag getter with a facility input.
    memcpy((void*)site(0x47b3c0),"\x8b\x44\x24\x04\x85",5);
    domesticStub(0x58c570,environmentFixtureClimate);domesticStub(0x4ba130,environmentFixtureAppend);
    domesticStub(0x58f0e9,environmentFixtureNewDone);domesticStub(0x58f30a,environmentFixtureSkipped);
    domesticStub(0x58f2c3,environmentFixtureActivated);domesticStub(0x58f3ce,environmentFixtureSpreadDone);
    bool old97=g_skills[249].effects[97];g_skills[249].effects[97]=true;
    auto setup=[&](int phase,int kind) {
        g_environmentEntry=site(phase==0?0x58f080:phase==1?0x58f24d:0x58f38d);
        disasterVt[0]=(void*)site(kind==0?0x44fc50:0x5584e0);
        *(uint32_t*)(city+0x9c)=0;*(uint32_t*)(city+0xa0)=phase==1?1u<<kind:0;
        g_environmentCandidateAdds=0;g_environmentForcedHolder=-2;g_environmentRegistered=true;g_environmentClimate=1;
    };
    *(int*)(actor+0xe8)=97;*(int*)(other+0xe8)=-1;setup(0,0);
    assert(runEnvironment(city,&disaster)==0);
    assert(skilltelemetry::open(L"build/environment_telemetry_test.csv",personFixtureActor,nullptr));
    for(int layout=0;layout<2;++layout) {
        *newBound=*activeBound=*spreadBound=layout?14612:1099;base.id=layout?529:8;
        *candidateOffset=0xe8+(layout?1952:0); // installed M7 candidate-array displacement for C530
        for(int phase=0;phase<3;++phase)for(int kind=0;kind<2;++kind)for(int mode=0;mode<4;++mode) {
            setup(phase,kind);*(int*)(actor+0xe8)=mode==2?-1:mode==1?249:97;*(int*)(other+0xe8)=mode>=2?97:-1;
            bool protectedCity=layout || mode!=2;
            assert(runEnvironment(city,&disaster)==(protectedCity?0:1));
            if(phase==1)assert(*(uint32_t*)(city+0x9c)==(protectedCity?0:1u<<kind) &&
                *(uint32_t*)(city+0xa0)==(protectedCity?1u<<kind:0));
            else assert(!*(uint32_t*)(city+0x9c) && !*(uint32_t*)(city+0xa0));
        }
        *(int*)(actor+0xe8)=97;*(int*)(other+0xe8)=-1;
        for(int phase=0;phase<3;++phase)for(int kind=0;kind<2;++kind) {
            setup(phase,kind);
            if(phase==1)*(uint32_t*)(city+0xa0)=0;else *(uint32_t*)(city+0x9c)=1u<<kind;
            assert(runEnvironment(city,&disaster)==0); // native prerequisite excludes before any skill query
        }
        for(int holder:{-1,0,1099,1100,14162,14612,14613})for(int phase=0;phase<3;++phase) {
            setup(phase,0);g_environmentForcedHolder=holder;*(int*)(actor+0xe8)=*(int*)(other+0xe8)=-1;
            assert(runEnvironment(city,&disaster)==(holder>=0 && holder<=*newBound?0:1));
        }
    }
    setup(0,0);*(int*)(actor+0xe8)=97;g_environmentClimate=0;assert(runEnvironment(city,&disaster)==0);
    setup(0,0);g_environmentRegistered=false;assert(runEnvironment(city,&disaster)==0);g_environmentRegistered=true;
    personHas(actor,nullptr,97); // generic preview query cannot create an exclusion
    skilltelemetry::close();assert(!g_progressionProbe);
    g_skills[249].effects[97]=old97;g_telemetryPerson=oldPerson;g_telemetryBaseId=oldBaseId;g_baseById=oldBaseById;g_growthValid=oldValid;
    *newBound=savedBounds[0];*activeBound=savedBounds[1];*spreadBound=savedBounds[2];*candidateOffset=savedOffset;
    for(int i=0;i<3;++i)*(uint32_t*)site(pointerOperands[i])=savedPointerOperands[i];
    g_environmentNewBound=oldNewBound;g_environmentActiveBound=oldActiveBound;g_environmentSpreadBound=oldSpreadBound;
    g_environmentNewResume=oldNewResume;g_environmentActiveResume=oldActiveResume;g_environmentSpreadResume=oldSpreadResume;
    puts("PASS native environment: three wind97 exclusion branches, native flag prerequisites/setters, stock/expanded bounds and candidate frame, composed/high-ID source, no fabricated disaster loss");
}
