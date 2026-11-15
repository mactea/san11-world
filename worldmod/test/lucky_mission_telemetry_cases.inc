// Execute the complete three native executors and their actual jump-table
// dispatches. Earlier calculator scoring/world/RNG and final outcome handlers
// are controlled; the actual32 predicate/branch/fixed2 epilogues execute.
static uintptr_t g_missionTail=0;
static int g_missionFamily=0,g_missionFalseResult=0,g_missionFault=0,g_missionHandled=-1,g_missionHandlerCalls=0;
static bool g_missionEarly=false;
static void* g_missionPerson=nullptr,*g_missionFirstContext=nullptr,*g_missionSecondContext=nullptr;
__declspec(naked) static int __cdecl runMissionTail(void*){__asm {push ebp
    push ebx
    push esi
    push edi
    mov eax,[esp+20]
    push offset finished
    cmp g_missionFamily,0
    jne later
    sub esp,12
    push esi
    push edi
    push ebp
    push ebx
    mov edi,eax
    jmp dword ptr [g_missionTail]
later:
    cmp g_missionFamily,1
    jne third
    sub esp,4
    jmp common
third:
    sub esp,8
common:
    push ebx
    push esi
    push edi
    push ebp
    mov esi,eax
    jmp dword ptr [g_missionTail]
finished:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret}}
__declspec(naked) static void missionOtherResult(){__asm {mov eax,g_missionFalseResult
    cmp g_missionFamily,0
    jne later
    pop ebx
    pop ebp
    pop edi
    pop esi
    add esp,12
    ret
later:
    pop ebp
    pop edi
    pop esi
    pop ebx
    cmp g_missionFamily,1
    jne third
    add esp,4
    ret
third:
    add esp,8
    ret}}
static int missionBackend(void* person){
    assert(person==g_missionPerson);
    if(g_missionEarly)return g_missionFalseResult;
    int result=runMissionTail(person);
    switch(g_missionFault){
    case 1:skilltelemetry::invalidateContext();break;
    case 2:*(int*)((uint8_t*)person+0xe8)=-1;break;
    case 3:g_afterActorRegistered=false;break;
    case 4:++*(int*)((uint8_t*)person+0x9c);break;
    case 5:g_skills[249].effects[32]=false;break;
    case 6:result=3;break;
    }
    return result;
}
static int __cdecl missionFirstBackend(void* person,void* context,int roll){
    assert(g_missionFamily==0 && context==g_missionSecondContext && roll==42);return missionBackend(person);
}
static int __cdecl missionSecondBackend(void* person,void* first,void* second,int roll){
    assert(g_missionFamily==1 && first==g_missionFirstContext && second==g_missionSecondContext && roll==42);return missionBackend(person);
}
static int __cdecl missionThirdBackend(void* person,void* context,int roll){
    assert(g_missionFamily==2 && context==g_missionFirstContext && roll==42);return missionBackend(person);
}
static int __fastcall missionRoll(void* person,void*,int kind){
    assert(person==g_missionPerson && kind==(g_missionFamily==2?1:2));return 42;
}
static void missionHandled(void* person,void* first,void* second,int family,int result){
    assert(person==g_missionPerson && first==g_missionFirstContext && family==g_missionFamily);
    if(family!=2)assert(second==g_missionSecondContext);
    ++g_missionHandlerCalls;g_missionHandled=result;
}
static void __cdecl missionFirst01(void* p,void* a,void* b){missionHandled(p,a,b,0,0);}
static void __cdecl missionFirst2(void* p,void* a,void* b){missionHandled(p,a,b,0,2);}
static void __cdecl missionFirst3(void* p,void* a,void* b){missionHandled(p,a,b,0,3);}
static void __cdecl missionSecond0(void* p,void* a,void* b){missionHandled(p,a,b,1,0);}
static void __cdecl missionSecond1(void* p,void* a,void* b){missionHandled(p,a,b,1,1);}
static void __cdecl missionSecond2(void* p,void* a,void* b){missionHandled(p,a,b,1,2);}
static void __cdecl missionSecond3(void* p,void* a,void* b){missionHandled(p,a,b,1,3);}
static void __cdecl missionThird0(void* p,void* a){missionHandled(p,a,nullptr,2,0);}
static void __cdecl missionThird1(void* p,void* a){missionHandled(p,a,nullptr,2,1);}
static void __cdecl missionThird2(void* p,void* a){missionHandled(p,a,nullptr,2,2);}
static void __cdecl missionThird3(void* p,void* a){missionHandled(p,a,nullptr,2,3);}
static void luckyMissionTelemetryTests(){
    uint8_t people[2][400]={},contexts[2][16]={};void* vt[24]={};vt[0x40/4]=personFixtureOwner;
    for(auto& p:people)*(void**)p=vt;
    auto oldGuide=g_personGuide,oldOther=g_personOther,oldTarget=g_personTarget,oldSpouse=g_personSpouse;
    g_personGuide=people[0];g_personOther=people[1];g_personTarget=g_personSpouse=nullptr;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;bool oldRegistered=g_afterActorRegistered,old32=g_skills[249].effects[32];
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)afterFixturePersonId;
    auto oldFirst=g_luckyMissionFirstOriginal,oldThird=g_luckyMissionThirdOriginal;auto oldSecond=g_luckyMissionSecondOriginal;
    g_luckyMissionFirstOriginal=(LuckyMissionCalc3)site(0x5b79a0);g_luckyMissionSecondOriginal=(LuckyMissionCalc4)site(0x5bf650);
    g_luckyMissionThirdOriginal=(LuckyMissionCalc3)site(0x5d05f0);
    uint32_t addresses[]={0x47a630,0x4897b0,0x5b79a0,0x5bf650,0x5d05f0,0x5b7ab1,0x5bf761,0x5d0743,
        0x5b6f50,0x5b71d0,0x5b7360,0x5bf7c0,0x5bfb40,0x5beef0,0x5bf0b0,0x5d0f30,0x5d1230,0x5cfd40,0x5cfec0};
    const void* replacements[]={domesticValid,missionRoll,missionFirstBackend,missionSecondBackend,missionThirdBackend,
        missionOtherResult,missionOtherResult,missionOtherResult,missionFirst01,missionFirst2,missionFirst3,
        missionSecond0,missionSecond1,missionSecond2,missionSecond3,missionThird0,missionThird1,missionThird2,missionThird3};
    uint8_t saved[19][5];for(int n=0;n<19;++n){memcpy(saved[n],(void*)site(addresses[n]),5);domesticStub(addresses[n],replacements[n]);}
    uint32_t tableOperands[]={0x5b7b82,0x5bffbb,0x5d1560},savedTables[3],tables[3][4];
    for(int family=0;family<3;++family){
        savedTables[family]=*(uint32_t*)site(tableOperands[family]);
        for(int n=0;n<4;++n)tables[family][n]=(uint32_t)site(*(uint32_t*)site(savedTables[family]+4*n));
        *(uint32_t*)site(tableOperands[family])=(uint32_t)tables[family];
    }
    uint32_t roots[]={0x5b7b30,0x5bff60,0x5d1520},tails[]={0x5b7aa0,0x5bf750,0x5d0732};
    uint32_t calls[]={0x5b7b72,0x5bffab,0x5d1550},calculators[]={0x5b79a0,0x5bf650,0x5d05f0};uint8_t hookedCalls[3][5];
    for(int n=0;n<3;++n)memcpy(hookedCalls[n],(void*)site(calls[n]),5);
    auto setup=[&](int family,int who,int owned){
        for(auto& p:people){*(int*)(p+0xe8)=-1;memset(p+0x98,0,16);}
        g_missionFamily=family;g_missionPerson=people[who];*(int*)(people[who]+0xe8)=owned;
        g_missionFirstContext=contexts[0];g_missionSecondContext=contexts[1];g_missionTail=site(tails[family]);
        g_missionEarly=false;g_missionFalseResult=0;g_missionFault=0;g_missionHandled=-1;g_missionHandlerCalls=0;
        g_afterActorRegistered=true;g_skills[249].effects[32]=true;
    };
    auto execute=[&](int expected){
        int result=g_missionFamily==2?((int(__cdecl*)(void*,void*))site(roots[2]))(g_missionPerson,g_missionFirstContext):
            ((int(__cdecl*)(void*,void*,void*))site(roots[g_missionFamily]))(g_missionPerson,g_missionFirstContext,g_missionSecondContext);
        assert(result==1 && g_missionHandlerCalls==1 && g_missionHandled==(g_missionFamily==0 && expected==1?0:expected) && !g_progressionProbe);
    };
    auto preview=[&]{
        int result=g_missionFamily==0?g_luckyMissionFirstOriginal(g_missionPerson,g_missionSecondContext,42):
            g_missionFamily==1?g_luckyMissionSecondOriginal(g_missionPerson,g_missionFirstContext,g_missionSecondContext,42):
            g_luckyMissionThirdOriginal(g_missionPerson,g_missionFirstContext,42);
        assert(result==2 && !g_missionHandlerCalls);
    };
    // Reproduce the old observation gap with only these three new CALL hooks
    // absent. Native predicate/classification/dispatch and metadata are identical.
    for(int n=0;n<3;++n){*(uint8_t*)site(calls[n])=0xe8;*(int32_t*)site(calls[n]+1)=(int32_t)(site(calculators[n])-site(calls[n])-5);}
    assert(skilltelemetry::open(L"build/lucky_mission_before_test.csv",personFixtureActor,nullptr));
    for(int family=0;family<3;++family)for(int who=0;who<2;++who)for(int owned:{32,249}){setup(family,who,owned);execute(2);}
    for(int family=0;family<3;++family)for(int owned:{32,249}){setup(family,0,owned);preview();}
    skilltelemetry::close();for(int n=0;n<3;++n)memcpy((void*)site(calls[n]),hookedCalls[n],5);
    assert(skilltelemetry::open(L"build/lucky_mission_test.csv",personFixtureActor,nullptr));
    for(int family=0;family<3;++family)for(int who=0;who<2;++who)for(int owned:{32,249}){setup(family,who,owned);execute(2);}
    for(int family=0;family<3;++family)for(int owned:{32,249})for(int fault=1;fault<=6;++fault){
        if(fault==5 && owned==32)continue;
        setup(family,0,owned);g_missionFault=fault;execute(fault==6?3:2);
    }
    for(int family=0;family<3;++family)for(int owned:{32,249}){setup(family,0,owned);preview();}
    for(int family=0;family<3;++family)for(int ordinary=0;ordinary<4;++ordinary){setup(family,0,-1);g_missionFalseResult=ordinary;execute(ordinary);}
    for(int family=0;family<3;++family)for(int owned:{32,249})for(int early:{1,2}){
        setup(family,0,owned);g_missionEarly=true;g_missionFalseResult=early;execute(early);
    }
    skilltelemetry::close();
    for(int family=0;family<3;++family)for(int owned:{-1,249}){setup(family,1,owned);execute(owned<0?0:2);}
    for(int n=0;n<3;++n)*(uint32_t*)site(tableOperands[n])=savedTables[n];
    for(int n=0;n<19;++n)memcpy((void*)site(addresses[n]),saved[n],5);
    g_personGuide=oldGuide;g_personOther=oldOther;g_personTarget=oldTarget;g_personSpouse=oldSpouse;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;
    g_luckyMissionFirstOriginal=oldFirst;g_luckyMissionSecondOriginal=oldSecond;g_luckyMissionThirdOriginal=oldThird;
    g_afterActorRegistered=oldRegistered;g_skills[249].effects[32]=old32;
    puts("PASS lucky mission: three complete native executors, actual32 branches/result2 dispatches, canonical raw/composed/high providers, previews/stale/ordinary/early gates and six disabled controls");
}
