static uintptr_t fireFixtureCallSite;
static int fireFixtureVisibility=1,fireFixtureHits=0,fireFixtureMessages=0,fireFixtureMutation=0;
__declspec(naked) static int __cdecl fireFixtureCall(void*,void*){__asm {push ebp
    mov ebp,[esp+8]
    push [esp+12]
    jmp dword ptr [fireFixtureCallSite]}}
__declspec(naked) static void fireFixtureReturned(){__asm {pop ebp
    ret}}
static int __cdecl fireFixtureHidden(void*){return fireFixtureVisibility;}
static int __fastcall fireFixtureCount(void*,void*){return 0;}
static int __fastcall fireFixtureTroops(uint8_t* u,void*){return *(int*)(u+0xd0);}
static void __cdecl fireFixtureCasualties(void*,int){}
__declspec(naked) static int fireFixtureDamage(){__asm {mov eax,100
    ret}}
static int __fastcall fireFixtureGrant(void*,void*,uint8_t* u,int delta){++fireFixtureHits;*(int*)(u+0xd0)+=delta;return delta;}
static void __fastcall fireFixtureMessage(void*,void*,int,void*,int){++fireFixtureMessages;}
static void __fastcall fireFixtureAnimation(void*,void*,int,void*){++fireFixtureMessages;}
static bool fireFixtureActor(const void* p,skilltelemetry::Actor& a){
    afterFixtureActor(p,a);
    if(g_progressionProbe && fireFixtureMutation){int fault=fireFixtureMutation;fireFixtureMutation=0;
        if(fault==1)skilltelemetry::invalidateContext();
        if(fault==2)*(int*)((uint8_t*)p+0xe8)=-1;
        if(fault==3)*(int*)((uint8_t*)p+0xe8)=249;
        if(fault==4)*(int*)(g_afterTarget+0xc)=-1;
        if(fault==5)g_afterTargetRegistered=false;
        if(fault==6)g_afterActorRegistered=false;
        if(fault==7)*(uint32_t*)(g_afterTarget+4)^=1;
        if(fault==8)*(int*)(g_afterTarget+0xa8)=4;
        if(fault==9)*(int*)(g_afterTarget+8)=-1;
    }
    return true;
}
template<class Setup>
static void fireAftermathTelemetryTests(Setup setup,uint8_t* target,uint8_t* person,uint8_t* spouse){
    assert(verifyFireAftermathLayout());
    for(uint32_t at:{0x582feb,0x583000,0x583010,0x583035}){auto p=(uint8_t*)site(at);*p^=1;assert(!verifyFireAftermathLayout());*p^=1;}
    auto oldOriginal=g_fireAftermathOriginal;g_fireAftermathOriginal=(FireAftermath)site(0x582fb0);
    auto oldGuard=g_fireAftermathGuardResume,oldGod=g_fireAftermathGodResume;
    g_fireAftermathGuardResume=(uint32_t)site(0x582feb);g_fireAftermathGodResume=(uint32_t)site(0x583010);
    fireFixtureCallSite=site(0x5831e4);
    uint32_t sites[]={0x5831e9,0x5a0700,0x4953d0,0x582990,0x580c70,0x4ae4a0,0x4954c0,0x47be50,0x5899c0,0x589980};
    void* stubs[]={fireFixtureReturned,fireFixtureHidden,fireFixtureCount,fireFixtureCasualties,fireFixtureDamage,
        fireFixtureGrant,afterFixtureRecalc,afterFixtureListDestroy,fireFixtureMessage,fireFixtureAnimation};
    uint8_t saved[10][5];for(int n=0;n<10;++n){memcpy(saved[n],(void*)site(sites[n]),5);domesticStub(sites[n],stubs[n]);}
    auto vt=*(void***)target;auto oldTroops=vt[0x50/4];vt[0x50/4]=fireFixtureTroops;
    bool old52=g_skills[249].effects[52],old62=g_skills[249].effects[62];g_skills[249].effects[52]=g_skills[249].effects[62]=true;
    auto prepare=[&](int profile){setup(51,0);*(int*)(target+0xd0)=500;*(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;
        *(int*)(target+0xc)=1000;*(int*)(target+0x10)=601;*(int*)(target+0x14)=-1;
        *(uint32_t*)(target+4)=0x10002;*(int*)(target+8)=0;*(int*)(target+0xa8)=2;
        g_skills[249].effects[52]=g_skills[249].effects[62]=true;
        if(profile==0)*(int*)(person+0xe8)=52;
        if(profile==1)*(int*)(person+0xe8)=62;
        if(profile==2)*(int*)(spouse+0xe8)=52;
        if(profile==3)*(int*)(spouse+0xe8)=62;
        if(profile==4)*(int*)(person+0xe8)=249;
        if(profile==5){*(int*)(person+0xe8)=62;*(int*)(spouse+0xe8)=52;}
        if(profile==6){*(int*)(target+0xc)=14162;*(int*)(g_personOther+0xe8)=62;}
        fireFixtureMutation=0;fireFixtureHits=fireFixtureMessages=0;};
    assert(skilltelemetry::open(L"build/fire_aftermath_test.csv",fireFixtureActor,nullptr));
    for(int profile=0;profile<8;++profile)for(int visibility:{0,1}){
        if(profile==7 && visibility==0)continue;
        prepare(profile);fireFixtureVisibility=visibility;
        assert(fireFixtureCall(nullptr,target)==0 && !g_fireAftermath && !g_progressionProbe);
        assert(*(int*)(target+0xd0)==(profile==7?400:500) && fireFixtureHits==(profile==7));
        assert(fireFixtureMessages==(visibility?0:(profile==1 || profile==3 || profile==6?2:1)));
    }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/fire_aftermath_preview_test.csv",fireFixtureActor,nullptr));
    for(int profile:{0,1}){prepare(profile);fireFixtureVisibility=1;assert(g_fireAftermathOriginal(nullptr,target)==0 && !fireFixtureHits);}
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/fire_aftermath_edges_test.csv",fireFixtureActor,nullptr));
    for(int fault=0;fault<10;++fault){prepare(1);fireFixtureVisibility=1;fireFixtureMutation=fault;assert(fireFixtureCall(nullptr,target)==0 && !fireFixtureHits);}
    prepare(1);g_afterTargetRegistered=false;assert(fireFixtureCall(nullptr,target)==0 && !fireFixtureHits);
    skilltelemetry::close();prepare(1);assert(fireFixtureCall(nullptr,target)==0 && !fireFixtureHits);
    for(int n=0;n<10;++n)memcpy((void*)site(sites[n]),saved[n],5);
    vt[0x50/4]=oldTroops;g_fireAftermathOriginal=oldOriginal;g_fireAftermathGuardResume=oldGuard;g_fireAftermathGodResume=oldGod;
    g_skills[249].effects[52]=old52;g_skills[249].effects[62]=old62;
    puts("PASS turn-update fire casualties: native parent/branches, guard-before-fire-god, raw/deputy/composed/high providers, visibility modes, previews and stale exclusions");
}
