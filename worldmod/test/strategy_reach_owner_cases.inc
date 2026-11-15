static uintptr_t g_strategyReachResume=0;
__declspec(naked) static void strategyReachGrantDone(){__asm {cmp g_strategyOwnerForeign,0
    je original
    pop edi
    add esp,0x40
    ret
original:
    mov eax,[esp+0x24]
    mov ecx,[esi]
    jmp dword ptr [g_strategyReachResume]}}
template<typename Prepare> static void strategyReachOwnerTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,Prepare prepare){
    uint8_t savedBetween[5],savedGrant[6];memcpy(savedBetween,(void*)site(0x593440),5);memcpy(savedGrant,(void*)site(0x59344a),6);
    domesticStub(0x593440,strategyOwnerBetweenBridge);domesticStub(0x59344a,strategyReachGrantDone);*(uint8_t*)site(0x59344f)=0x90;
    auto oldUnit=g_strategyFixtureUnit;auto oldEntry=g_strategyOwnerGrantEntry;auto oldForced=g_strategyCostForcedResume;
    g_strategyCostForcedResume=(uint32_t)site(0x495180);
    g_strategyFixtureUnit=source;g_strategyOwnerGrantEntry=site(0x593445);g_strategyReachResume=site(0x593450);
    bool old64=g_skills[249].effects[64];g_skills[249].effects[64]=true;
    auto setup=[&](int provider,int tech,int success){
        memset(actor+0x98,0,16);memset(other+0x98,0,16);prepare(2,0,tech,tech?4:2,10,success);
        *(int*)(actor+0xe8)=provider==0?64:provider==1?65:249;
        *(int*)(other+0xe8)=provider==0?65:provider==1 || provider==3?64:-1;
        g_strategyOwnerForeign=false;g_strategyOwnerInterpose=true;
    };
    auto run=[&](int tech,int success){uint32_t command[3]={0,4095,((uint32_t)(tech?5:3)<<16)|1};
        int result=((StrategyCommand)site(0x5933a0))(nullptr,command,0,nullptr);
        assert(result==success && source[0x1a]==8 &&
            g_reachFixtureLeafCalls==1 && g_defenseSeed==0x12345678 && !g_reachCommand && !g_strategyCostPending.actor);};
    assert(skilltelemetry::open(L"build/strategy_reach_owner_test.csv",afterFixtureActor,nullptr));
    for(int provider=0;provider<4;++provider)for(int tech:{0,1})for(int success:{0,1}){setup(provider,tech,success);run(tech,success);}
    skilltelemetry::close();for(int provider=0;provider<4;++provider){setup(provider,0,1);run(0,1);}
    memcpy((void*)site(0x593440),savedBetween,5);memcpy((void*)site(0x59344a),savedGrant,6);
    g_strategyFixtureUnit=oldUnit;g_strategyOwnerGrantEntry=oldEntry;g_strategyCostForcedResume=oldForced;g_skills[249].effects[64]=old64;
    g_strategyOwnerForeign=g_strategyOwnerInterpose=false;
    puts("PASS strategy64+65 ownership:16 complete native paid-range commands; raw first/second/composed/duplicate providers and foreign-frame debit preserve actual energy8, return and RNG");
}
