static uintptr_t casualtyFixtureCalls[4];
static int casualtyMutation=0,casualtyForce=3;
static int __fastcall casualtyFixtureOwner(void* unit,void*){return unit==g_afterSource?2:casualtyForce;}
__declspec(naked) static void* __cdecl casualtyFixtureCall(void*,int){__asm {
    push ebx
    mov ebx,[esp+8]
    mov eax,[esp+12]
    jmp dword ptr [casualtyFixtureCalls+eax*4]}}
__declspec(naked) static void casualtyFixtureReturned(){__asm {pop ebx
    ret}}
static bool casualtyFixtureActor(const void* p,skilltelemetry::Actor& actor){
    afterFixtureActor(p,actor);if(afterAttackMember(g_afterTarget,p)){actor.force=casualtyForce;actor.ruler=700;actor.city=60;}
    if(p==g_personTarget && g_progressionProbe){
        int fault=casualtyMutation;casualtyMutation=0;
        if(fault==1)skilltelemetry::invalidateContext();
        if(fault==2)*(int*)(g_personTarget+0xe8)=-1;
        if(fault==3)*(int*)(g_afterTarget+0xc)=-1;
        if(fault==4)g_afterTargetRegistered=false;
        if(fault==5)g_afterActorRegistered=false;
        if(fault==6)casualtyForce=4;
        if(fault==7)*(uint32_t*)(g_afterTarget+4)^=1;
        if(fault==8){*(int*)(g_personTarget+0xe8)=249;g_skills[249].effects[32]=true;}
        if(fault==9)*(int*)(g_afterTarget+8)=-1;
    }
    return true;
}
template<class Setup>
static void casualtyTelemetryTests(Setup setup,uint8_t* source,uint8_t* target,uint8_t* person,uint8_t* spouse){
    auto oldEntry=g_casualtySelectorEntry;g_casualtySelectorEntry=(uint32_t)site(0x5971f0);
    auto vt=*(void***)target;auto oldOwner=vt[0x40/4];vt[0x40/4]=casualtyFixtureOwner;
    uint8_t continuations[4][5];
    for(int k=0;k<4;++k){casualtyFixtureCalls[k]=site(g_casualtyCallers[k]);memcpy(continuations[k],(void*)(casualtyFixtureCalls[k]+5),5);
        domesticStub(g_casualtyCallers[k]+5,casualtyFixtureReturned);}
    auto prepare=[&](int profile,int pick){
        setup(51,0);g_afterPick=profile==10?pick:0;casualtyForce=3;casualtyMutation=0;*(int*)(target+8)=0;
        *(int*)(target+0xc)=1000;*(int*)(source+0xc)=600;
        *(uint32_t*)(target+4)=0x10002;*(uint32_t*)(source+4)=0x10001;
        *(int*)(target+0x10)=601;*(int*)(source+0x10)=14162;
        if(profile==0 || profile==7)*(int*)(person+0xe8)=52;
        if(profile==1)*(int*)(spouse+0xe8)=52;
        if(profile==2){g_skills[249].effects[52]=true;*(int*)(person+0xe8)=249;}
        if(profile==3 || profile==6)*(int*)(person+0xe8)=32;
        if(profile==4 || profile==6 || profile==7)*(int*)(spouse+0xe8)=32;
        if(profile==5){g_skills[249].effects[32]=true;*(int*)(person+0xe8)=249;}
        if(profile==8 || profile==9){*(int*)(source+0x10)=-1;*(int*)(target+0x10)=14162;*(int*)(g_personOther+0xe8)=profile==8?52:32;}
    };
    int inputs=0;assert(skilltelemetry::open(L"build/casualty_test.csv",casualtyFixtureActor,nullptr));
    for(int kind=0;kind<4;++kind)for(int profile=0;profile<11;++profile)for(int pick:{0,1}){
        prepare(profile,pick);uint8_t before[0x110];memcpy(before,target,sizeof before);
        auto selected=casualtyFixtureCall(target,kind);
        bool guard=profile<=2 || profile==7 || profile==8;
        void* expected=guard || profile==6?nullptr:profile==3 || profile==5?spouse:profile==10 && pick?spouse:person;
        assert(selected==expected && !memcmp(before,target,sizeof before) && !g_casualty);++inputs;
    }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/casualty_preview_test.csv",casualtyFixtureActor,nullptr));
    prepare(0,0);assert(!afterFixtureSelect(target));prepare(6,0);assert(!afterFixtureSelect(target)); // selector previews remain query-only
    skilltelemetry::close();assert(skilltelemetry::open(L"build/casualty_edges_test.csv",casualtyFixtureActor,nullptr));
    for(int fault=0;fault<10;++fault){prepare(6,0);casualtyMutation=fault;assert(!casualtyFixtureCall(target,0) && !g_casualty);}
    prepare(6,0);g_afterTargetRegistered=false;assert(!casualtyFixtureCall(target,0) && !g_casualty);
    assert(!casualtyFixtureCall(nullptr,0) && !g_casualty);
    skilltelemetry::close();prepare(0,0);assert(!casualtyFixtureCall(target,0) && !g_casualty); // disabled parity
    for(int k=0;k<4;++k)memcpy((void*)(casualtyFixtureCalls[k]+5),continuations[k],5);
    g_casualtySelectorEntry=oldEntry;vt[0x40/4]=oldOwner;
    printf("PASS casualty protection: %d native caller/selector receipts across four death/injury contexts, guard precedence, raw/composed/high/duplicate candidates and query-only previews\n",inputs);
}
