static uintptr_t g_duelResponseEntry=0;
static int g_duelResponseOwner=0,g_duelResponseFalseGate=0;
static int g_duelResponseFault=0;
static int __fastcall duelResponseNative(uint8_t* duel,void*){
    int result=((LuckyDuelResponseCalc)luckyDuelResponseNative)(duel);
    int side=*(int*)(duel+0x578),slot=*(int*)(duel+0x57c);
    auto person=*(uint8_t**)(duel+0x24+0xec*side+0x40*slot);
    switch(g_duelResponseFault){
    case 1:skilltelemetry::invalidateContext();break;
    case 2:*(int*)(person+0xe8)=-1;break;
    case 3:g_afterActorRegistered=false;break;
    case 4:++*(int*)(duel+0x57c);break;
    case 5:*(int*)(duel+0x578)=1-side;break;
    case 6:++*(int*)(duel+0x580);break;
    case 7:++*(int*)(duel+0xc);break;
    case 8:++*(int*)(person+0x9c);break;
    case 9:*(void**)(duel+0x24+0xec*side+0x40*slot)=g_personTarget;break;
    case 10:g_skills[249].effects[32]=false;break;
    }
    return result;
}
__declspec(naked) static int __cdecl runDuelResponse(void*){__asm {push ebp
    push ebx
    push esi
    push edi
    mov esi,[esp+20]
    mov ecx,esi
    sub esp,0x40
    cmp g_duelResponseOwner,0
    je ready
    push edi
ready:
    jmp dword ptr [g_duelResponseEntry]}}
__declspec(naked) static int duelResponseDone(){__asm {mov eax,[esi+0x584]
    add esp,0x40
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret}}
__declspec(naked) static void duelResponseOtherGate(){__asm {mov eax,g_duelResponseFalseGate
    pop esi
    pop edi
    pop ebp
    pop ebx
    add esp,8
    ret 8}}
static void luckyDuelResponseTelemetryTests(uint8_t* duel,uint8_t people[][400]){
    auto oldOriginal=g_luckyDuelResponseOriginal;auto oldResume=g_luckyDuelResponseResume;
    g_luckyDuelResponseOriginal=(LuckyDuelResponseCalc)duelResponseNative;g_luckyDuelResponseResume=(uint32_t)site(0x509a99);
    uint32_t sites[]={0x50a270,0x50ec0c,0x506ab8};uint8_t saved[3][5];
    for(int n=0;n<3;++n)memcpy(saved[n],(void*)site(sites[n]),5);
    domesticStub(sites[0],duelResponseDone);domesticStub(sites[1],duelResponseDone);domesticStub(sites[2],duelResponseOtherGate);
    auto setup=[&](int owner,int side,int slot,int owned){
        memset(duel,0,0x600);for(int n=0;n<4;++n){*(int*)(people[n]+0xe8)=-1;memset(people[n]+0x98,0,16);}
        *(int*)(people[slot]+0xe8)=owned;*(int*)(duel+0x578)=side;*(int*)(duel+0x57c)=slot;
        *(int*)(duel+0x580)=3;*(int*)(duel+0x584)=slot-1;*(int*)(duel+0xc)=*(int*)(duel+0x59c)=3;
        *(int*)(duel+0xe8+0xec*side)=slot;*(int*)(duel+0xe8+0xec*(1-side))=(slot+1)%3;
        *(void**)(duel+0x24+0xec*side+0x40*slot)=people[slot];
        *(void**)(duel+0x24+0xec*(1-side)+0x40*((slot+1)%3))=people[3];
        g_duelResponseOwner=owner;g_duelResponseEntry=site(owner?0x50ebfc:0x50a25b);g_duelResponseFalseGate=0;
        g_skills[249].effects[32]=true;g_afterActorRegistered=true;g_luckyDuelSeed=0;
        g_duelResponseFault=0;
    };
    assert(skilltelemetry::open(L"build/lucky_duel_response_test.csv",luckyDuelActor,nullptr));
    for(int owner:{0,1})for(int side:{0,1})for(int slot=0;slot<3;++slot)for(int owned:{32,249}){
        setup(owner,side,slot,owned);assert(runDuelResponse(duel)==0 && *(int*)(duel+(owner?0x59c:0xc))==4 && g_luckyDuelSeed==0);
    }
    for(int owner:{0,1})for(int owned:{32,249})for(int fault=1;fault<=(owned==249?10:9);++fault){
        setup(owner,1,1,owned);g_duelResponseFault=fault;
        assert(runDuelResponse(duel)==0 && *(int*)(duel+0x584)==0 && *(int*)(duel+(owner?0x59c:0xc))==4 && g_luckyDuelSeed==0);
    }
    for(int owned:{32,249}){
        setup(0,0,0,owned);assert(((int(__thiscall*)(void*))site(0x509a90))(duel)==0 && *(int*)(duel+0x584)==0);
        assert(((int(__thiscall*)(void*,int,int))site(0x506a20))(duel,0,0)==1);
    }
    for(int owner:{0,1})for(int gate:{0,1}){setup(owner,0,0,-1);g_duelResponseFalseGate=gate;
        assert(runDuelResponse(duel)==!gate && *(int*)(duel+0x584)==!gate && *(int*)(duel+(owner?0x59c:0xc))==4);}
    for(int owner:{0,1}){setup(owner,0,0,32);*(int*)(duel+0x580)=5;
        assert(runDuelResponse(duel)==0 && *(int*)(duel+0x584)==0);}
    skilltelemetry::close();
    for(int owner:{0,1})for(int protectedPerson:{0,1}){setup(owner,1,1,protectedPerson?249:-1);
        assert(runDuelResponse(duel)==!protectedPerson && *(int*)(duel+(owner?0x59c:0xc))==4 && g_luckyDuelSeed==0);}
    for(int n=0;n<3;++n)memcpy((void*)site(sites[n]),saved[n],5);
    g_luckyDuelResponseOriginal=oldOriginal;g_luckyDuelResponseResume=oldResume;
    puts("PASS lucky duel response: actual native cache/phase stores, two controller calls, both sides/three slots, raw/composed/high providers and unowned/ordinary/action controls");
}
