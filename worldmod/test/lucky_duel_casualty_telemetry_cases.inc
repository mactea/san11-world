typedef int(__thiscall* DuelPostCheck)(void*,int);
static uintptr_t g_duelPostEntry=0;
static int g_duelPostKind=0,g_duelPostQueued=0,g_duelPostRelation=0,g_duelPostModal=0,g_duelPostPolicy=2;
static void* g_duelPostUI[1];
static int g_duelPostFault=0;
static int duelPostNative(uint8_t* duel,int side,int kind){
    int result=((DuelPostCheck)site(kind?0x509240:0x509130))(duel,side);
    int slot=*(int*)(duel+0xe8+0xec*side);auto person=*(uint8_t**)(duel+0x24+0xec*side+0x40*slot);
    switch(g_duelPostFault){
    case 1:skilltelemetry::invalidateContext();break;
    case 2:*(int*)(person+0xe8)=-1;break;
    case 3:g_afterActorRegistered=false;break;
    case 4:++*(int*)(duel+0xe8+0xec*side);break;
    case 5:*(int*)(duel+0x58c)=1-side;break;
    case 6:*(int*)(duel+0x1c)^=1;break;
    case 7:++*(int*)(duel+0xc);break;
    case 8:++*(int*)(person+0x9c);break;
    case 9:*(void**)(duel+0x24+0xec*side+0x40*slot)=g_personTarget;break;
    case 10:g_skills[249].effects[32]=false;break;
    }
    return result;
}
static int __fastcall duelPostNativeFirst(uint8_t* duel,void*,int side){return duelPostNative(duel,side,0);}
static int __fastcall duelPostNativeSecond(uint8_t* duel,void*,int side){return duelPostNative(duel,side,1);}
static int __fastcall duelPostUIReady(void*,void*){return g_duelPostModal;}
static int __fastcall duelPostUIFlag(void*,void*,int flag){assert(flag==24);return g_duelPostModal;}
static int __fastcall duelPostRelated(void*,void*,int,int,int,int){return g_duelPostRelation;}
static int __cdecl duelPostValidSide(int side){assert(side==0 || side==1);return 1;}
static void __fastcall duelPostQueue(void*,void*,int kind,int side,uintptr_t){
    assert(kind==(g_duelPostKind?1:2) && (side==0 || side==1));++g_duelPostQueued;
}
__declspec(naked) static int __cdecl runDuelPost(void*){__asm {push ebx
    push ebp
    push esi
    push edi
    mov esi,[esp+20]
    sub esp,0x20
    push 0
    push offset duelPostDone
    push esi
    push edi
    jmp dword ptr [g_duelPostEntry]
duelPostDone:
    mov eax,[esi+0xc]
    add esp,0x20
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
static void luckyDuelCasualtyTelemetryTests(uint8_t* duel,uint8_t people[][400]){
    auto oldFirst=g_luckyDuelPostOriginal[0],oldSecond=g_luckyDuelPostOriginal[1];
    g_luckyDuelPostOriginal[0]=(LuckyDuelPostCheck)duelPostNativeFirst;g_luckyDuelPostOriginal[1]=(LuckyDuelPostCheck)duelPostNativeSecond;
    uint32_t sites[]={0x67f750,0x506e00,0x4cfd10,0x50d0d0};uint8_t saved[4][5];
    for(int n=0;n<4;++n)memcpy(saved[n],(void*)site(sites[n]),5);
    domesticStub(sites[0],duelPostUIReady);domesticStub(sites[1],duelPostRelated);
    domesticStub(sites[2],duelPostValidSide);domesticStub(sites[3],duelPostQueue);
    uint32_t addresses[]={0x509200,0x509252,0x509259},oldAddresses[3];
    for(int n=0;n<3;++n)oldAddresses[n]=*(uint32_t*)site(addresses[n]);
    void* uiVt[2]={nullptr,duelPostUIFlag};g_duelPostUI[0]=uiVt;
    *(uint32_t*)site(addresses[0])=(uint32_t)&g_duelPostPolicy;
    *(uint32_t*)site(addresses[1])=*(uint32_t*)site(addresses[2])=(uint32_t)g_duelPostUI;
    auto setup=[&](int kind,int side,int slot,int owned){
        memset(duel,0,0x600);for(int n=0;n<4;++n){*(int*)(people[n]+0xe8)=-1;memset(people[n]+0x98,0,16);}
        *(int*)(people[slot]+0xe8)=owned;*(int*)(duel+0x58c)=side;*(int*)(duel+0x580)=5;
        *(int*)(duel+0xc)=kind?3:2;*(int*)(duel+0xe8+0xec*side)=slot;
        *(int*)(duel+0xe8+0xec*(1-side))=(slot+1)%3;*(int*)(duel+0x1c)=slot&1;
        *(void**)(duel+0x24+0xec*side+0x40*slot)=people[slot];
        *(void**)(duel+0x24+0xec*(1-side)+0x40*((slot+1)%3))=people[3];
        g_duelPostKind=kind;g_duelPostQueued=g_duelPostRelation=g_duelPostModal=0;g_luckyDuelSeed=0;g_afterActorRegistered=true;
        g_skills[249].effects[32]=true;g_duelPostEntry=site(kind?0x50a59d:0x50a51d);
        g_duelPostFault=0;
    };
    assert(skilltelemetry::open(L"build/lucky_duel_casualty_test.csv",luckyDuelActor,nullptr));
    for(int kind:{0,1})for(int side:{0,1})for(int slot=0;slot<3;++slot)for(int owned:{32,249}){
        setup(kind,side,slot,owned);assert(runDuelPost(duel)==(kind?4:3) && !g_duelPostQueued && g_luckyDuelSeed==0);
    }
    for(int kind:{0,1})for(int owned:{32,249})for(int fault=1;fault<=(owned==249?10:9);++fault){
        setup(kind,1,1,owned);g_duelPostFault=fault;
        assert(runDuelPost(duel)==(kind?4:3) && !g_duelPostQueued && g_luckyDuelSeed==0);
    }
    for(int kind:{0,1})for(int owned:{32,249}){
        setup(kind,0,0,owned);assert(((DuelPostCheck)site(kind?0x509240:0x509130))(duel,0)==0 && g_luckyDuelSeed==0);
    }
    setup(1,0,0,32);g_duelPostModal=1;assert(runDuelPost(duel)==4 && !g_duelPostQueued && g_luckyDuelSeed==0);
    for(int kind:{0,1})for(int gate=0;gate<3;++gate){
        setup(kind,0,0,-1);*(int*)(people[3]+0xe8)=32;
        if(gate==1)*(int*)(duel+0x580)=3;if(gate==2)g_duelPostRelation=1;
        assert(runDuelPost(duel)==(kind?4:gate?3:4) && g_duelPostQueued==!gate && g_luckyDuelSeed==(gate?0:0x3039));
    }
    skilltelemetry::close();
    for(int kind:{0,1})for(int protectedPerson:{0,1}){
        setup(kind,1,1,protectedPerson?249:-1);
        assert(runDuelPost(duel)==(kind?4:protectedPerson?3:4) && g_duelPostQueued==!protectedPerson &&
            g_luckyDuelSeed==(protectedPerson?0:0x3039));
    }
    for(int n=0;n<4;++n)memcpy((void*)site(sites[n]),saved[n],5);
    for(int n=0;n<3;++n)*(uint32_t*)site(addresses[n])=oldAddresses[n];
    g_luckyDuelPostOriginal[0]=oldFirst;g_luckyDuelPostOriginal[1]=oldSecond;
    puts("PASS lucky duel post-checks: two native controllers/32 branches, both sides/three slots, original RNG/queue/phase consumption, modal/ordinary gates and disabled parity");
}
