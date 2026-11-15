// Execute the real5b1f80 classifier prefix, its skill/gate branches and native
// reject epilogue. Later score arithmetic is replaced by a controlled return2.
static int g_diplomacyGateFault=0,g_diplomacyGateCalls=0,g_diplomacyGateResult=1;
static int g_diplomacyGateObservedRuler=-1;static void* g_diplomacyGateObservedForce=nullptr;
static uint32_t g_diplomacyGateObservedFlags=0;
static int __fastcall diplomacyGateValid(void*,void*) {
    auto actor=(uint8_t*)g_diplomacyFixtureCommand[1];
    switch(g_diplomacyGateFault){
    case 1:skilltelemetry::invalidateContext();break;
    case 2:*(int*)(actor+0xe8)=-1;break;
    case 3:g_diplomacyActorRegistered=false;break;
    case 4:g_diplomacyFixtureCommand[1]=g_personOther;break;
    case 5:g_diplomacyForceRegistered=false;break;
    case 6:++g_diplomacyFixtureForceId;break;
    case 7:++*(int*)(g_diplomacyFixtureForce+4);break;
    case 8:++*(int*)(actor+0x9c);break;
    }
    return g_diplomacyGateFault!=12;
}
static int __fastcall diplomacyGatePrecondition1(void*,void*){return g_diplomacyGateFault==13;}
static int __fastcall diplomacyGatePrecondition2(void*,void*){return g_diplomacyGateFault==14;}
static int __fastcall diplomacyGateCheck(void*,void*){++g_diplomacyGateCalls;return g_diplomacyGateResult;}
__declspec(naked) static void diplomacyGateContinue(){__asm {mov g_diplomacyGateObservedRuler,eax
    mov g_diplomacyGateObservedForce,ecx
    pushfd
    pop g_diplomacyGateObservedFlags
    mov eax,2
    pop edi
    pop esi
    pop ebx
    add esp,8
    ret 4}}
static void diplomacyEligibilityTelemetryTests(void** command,void** commandVt,uint8_t* actor,uint8_t* other,uint8_t* force){
    auto oldResume=g_diplomacyEligibilityResume;g_diplomacyEligibilityResume=(uint32_t)site(0x5b1fe5);
    uint32_t sites[]={0x5bd190,0x5bd1d0,0x5bc650,0x5b1fe5};uint8_t saved[4][5];
    for(int i=0;i<4;++i)memcpy(saved[i],(void*)site(sites[i]),5);
    domesticStub(sites[0],diplomacyGatePrecondition1);domesticStub(sites[1],diplomacyGatePrecondition2);
    domesticStub(sites[2],diplomacyGateCheck);domesticStub(sites[3],diplomacyGateContinue);
    auto oldClassifier=commandVt[0x14/4],oldValid=commandVt[0];
    commandVt[0x14/4]=(void*)site(0x5b1f80);commandVt[0]=diplomacyGateValid;
    auto reset=[&](int owned,int fault){
        command[1]=actor;command[2]=force;*(int*)(force+4)=14162;memset(actor+0x98,0,16);
        *(int*)(actor+0xe8)=owned;*(int*)(other+0xe8)=86;
        g_diplomacyGateFault=fault;g_diplomacyGateCalls=0;g_diplomacyGateResult=1;
        g_diplomacyGateObservedRuler=-1;g_diplomacyGateObservedForce=nullptr;
        g_diplomacyActorRegistered=g_diplomacyForceRegistered=true;g_diplomacyFixtureForceId=249;
        g_diplomacyFixtureKind=16;g_diplomacyDecisionEntry=site(0x5b22a7);
        g_diplomacyInteractive=false;g_diplomacyCommandReturn=1;
    };
    assert(skilltelemetry::open(L"build/diplomacy_eligibility_test.csv",personFixtureActor,nullptr));
    for(int owned:{86,249})for(int fault=0;fault<15;++fault){
        reset(owned,fault);
        if(fault==9)g_diplomacyFixtureKind=18; // classifier from the wrong command kind
        if(fault==10){assert(((int(__thiscall*)(void*,int*))site(0x5b1f80))(command,nullptr)==2);}
        else if(fault==11){DiplomacyExecution execution((uint8_t*)command,16);
            assert(((int(__thiscall*)(void*,int*))site(0x5b1f80))(command,nullptr)==2);}
        else assert(diplomacyExecute((uint8_t*)command,g_diplomacyFixtureKind,diplomacyFixtureExecute)==1 && g_diplomacyConsumed==2);
        assert(g_diplomacyGateCalls==(fault==2));
        if(fault!=2 && fault<12)assert(g_diplomacyGateObservedRuler==*(int*)(force+4) &&
            g_diplomacyGateObservedForce==force && !(g_diplomacyGateObservedFlags&0x40));
        assert(!g_diplomacyExecution && !g_diplomacyDecision && !g_progressionProbe);
    }
    for(int owned:{86,249}){reset(owned,0);g_diplomacyGateResult=0;
        assert(diplomacy16((uint8_t*)command)==1 && g_diplomacyConsumed==2 && !g_diplomacyGateCalls);}
    for(int gate:{0,1}){reset(-1,0);g_diplomacyGateResult=gate;
        assert(diplomacy16((uint8_t*)command)==1 && g_diplomacyConsumed==2 && g_diplomacyGateCalls==1);
        assert(gate?g_diplomacyGateObservedRuler==-1:g_diplomacyGateObservedRuler==14162 &&
            g_diplomacyGateObservedForce==force && (g_diplomacyGateObservedFlags&0x40));}
    skilltelemetry::close();
    for(int owned:{86,249}){reset(owned,0);
        assert(diplomacy16((uint8_t*)command)==1 && g_diplomacyConsumed==2 && !g_diplomacyGateCalls);}
    for(int i=0;i<4;++i)memcpy((void*)site(sites[i]),saved[i],5);
    commandVt[0x14/4]=oldClassifier;commandVt[0]=oldValid;g_diplomacyGateFault=0;
    g_diplomacyEligibilityResume=oldResume;
    puts("PASS diplomacy eligibility:34 native prefix inputs, two disabled controls, actual bypass versus ordinary gate/preview/preconditions and ownership exclusions");
}
