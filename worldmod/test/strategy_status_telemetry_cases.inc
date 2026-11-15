static uintptr_t g_strategyStatusDurationEntry;
static int g_strategyStatusFault=0,g_strategyStatusLastCritical=0;
static void* __fastcall strategyStatusFixtureForce(void*,void*,int id){assert(id==2 || id==3);return id==2?g_strategyFixtureForce:nullptr;}
static int __fastcall strategyStatusFixtureHidden(void*,void*){return 0;}
__declspec(naked) static void __cdecl runStrategyStatusDuration(void*,void*,int){__asm {
    push ebp
    push esi
    push edi
    push ebx
    mov esi,[esp+20]
    mov edi,[esp+24]
    mov ebx,edi
    mov eax,[esp+28]
    sub esp,0x400
    lea ebp,[esp+0x3e4]
    mov [esp+0x3f4],eax
    jmp dword ptr [g_strategyStatusDurationEntry]}}
__declspec(naked) static void strategyStatusFixtureDurationDone(){__asm {
    add esp,0x400
    pop ebx
    pop edi
    pop esi
    pop ebp
    ret}}
static int __fastcall strategyStatusFixtureLeaf(void*,void*,uint32_t* command,int success,int critical,int,void*,int){
    g_strategyStatusLastCritical=critical;
    if(g_strategyStatusFault==1)skillsTelemetryStream(1,0);
    if(g_strategyStatusFault==2)*(int*)(g_personGuide+0xe8)=-1;
    if(g_strategyStatusFault==3)*(int*)(g_afterSource+0xc)=-1;
    if(g_strategyStatusFault==4)++*(uint32_t*)(g_afterTarget+4);
    if(g_strategyStatusFault==5)++*(int*)(g_personGuide+0x94);
    if(g_strategyStatusFault==6)++command[0];
    if(success)runStrategyStatusDuration(g_afterSource,g_afterTarget,critical);
    return 1;
}
static void __fastcall strategyStatusFixtureGrant(void* self,void*,void* unit,void* source,int status,int turns,int display){
    if(g_strategyStatusFault!=7)((StatusGrant)site(0x4aea70))(self,unit,source,status,turns,display);
    if(g_strategyStatusFault==8)skillsTelemetryStream(1,0);
}
template<class Setup>
static void strategyStatusTelemetryTests(Setup setup,void* world,uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,int* command){
    auto oldStatus=g_strategyStatusOriginal;auto oldTurns=g_statusTurnsOriginal;
    auto oldForceVtable=*(void**)g_strategyFixtureForce;void* forceVtable[20]={};
    forceVtable[0x48/4]=strategyStatusFixtureHidden;*(void**)g_strategyFixtureForce=forceVtable;
    g_strategyStatusOriginal=(StatusGrant)strategyStatusFixtureGrant;g_statusTurnsOriginal=(StatusTurnsSet)site(0x496350);
    memcpy((void*)site(0x59340c),"\x33\xdb\x85\xed\x74\x1c",6); // prior chance fixture returns before the critical/leaf phases
    domesticStub(0x490aa0,strategyStatusFixtureForce);
    domesticStub(0x5917d0,strategyStatusFixtureLeaf);domesticStub(0x591a20,strategyStatusFixtureLeaf);domesticStub(0x592740,strategyStatusFixtureLeaf);
    domesticStub(0x591943,strategyStatusFixtureDurationDone);domesticStub(0x591b93,strategyStatusFixtureDurationDone);domesticStub(0x592938,strategyStatusFixtureDurationDone);
    auto prepare=[&](int plan,int effect,int provider,int before,int seed){
        command[0]=plan;g_strategyStatusFault=0;setup(effect,provider,0,seed,false);
        auto helper=(provider&1)?actor:other;*(int*)(helper+0xe8)=57;helper[0xc9]=110;
        *(int*)(target+0x24)=0;target[0x28]=(uint8_t)before;
        g_strategyStatusDurationEntry=site(plan==2?0x591912:plan==3?0x591b62:0x59290b);
    };
    auto apply=[&](int mode){
        int result=((StrategyCommand)site(0x5933a0))(world,(uint32_t*)command,mode,nullptr);
        assert(result==1 && !g_strategyCommand && !g_strategyCritical && !g_statusWriteProbe);return result;
    };
    assert(skilltelemetry::open(L"build/strategy_status_test.csv",strategyMaximumActor,nullptr));int cases=0;
    for(int plan:{2,3,5})for(int effect:{63,67,58,59,16}){
        if(effect==16 && plan!=5)continue;
        for(int provider=0;provider<4;++provider)for(int mode:{0,1,2})for(int before:{0,4,5}){
            prepare(plan,effect,provider,before,4);apply(mode);
            uint32_t expected=4;for(int n=0;n<(plan==5?3:2);++n)expected=expected*0x6c078965u+0x3039u;
            int duration=plan==5?1+(expected>>16)%2:3;
            assert(g_strategyStatusLastCritical==1 && g_strategySeed==expected && *(int*)(target+0x24)==(plan==2?2:1));
            assert(target[0x28]==(before+duration>5?5:before+duration));++cases;
        }
    }
    skilltelemetry::close();assert(cases==468);
    assert(skilltelemetry::open(L"build/strategy_status_edges_test.csv",strategyMaximumActor,nullptr));
    int affiliation=*(int*)(actor+0x94);
    for(int fault=1;fault<=8;++fault){
        prepare(2,63,0,0,4);g_strategyStatusFault=fault;apply(1);
        assert(target[0x28]==(fault==7?0:3));*(int*)(actor+0x94)=affiliation;
    }
    // A native duration/store outside a command has no retained critical owner.
    prepare(2,63,0,0,4);runStrategyStatusDuration(source,target,1);assert(target[0x28]==3 && !g_strategyCommand);
    skilltelemetry::close();
    for(int plan:{2,3,5}){
        prepare(plan,63,0,0,4);apply(1);uint32_t expected=4;
        for(int n=0;n<(plan==5?3:2);++n)expected=expected*0x6c078965u+0x3039u;
        assert(g_strategySeed==expected && target[0x28]==(plan==5?1+(expected>>16)%2:3));
    }
    g_strategyStatusOriginal=oldStatus;g_statusTurnsOriginal=oldTurns;g_strategyStatusFault=0;
    *(void**)g_strategyFixtureForce=oldForceVtable;
    printf("PASS critical status: %d original command decisions/critical calls, native duration arithmetic and status stores;8 stale/no-store boundaries, unowned preview and disabled controls\n",cases);
}
