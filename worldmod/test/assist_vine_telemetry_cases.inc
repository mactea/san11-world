static uintptr_t assistVineFixtureTail;
static int assistVineFixtureMode=0;
__declspec(naked) static void __cdecl assistVineFixtureCalculate(void*,void*){__asm {push ebp
    push esi
    mov ebp,[esp+12]
    mov esi,[esp+16]
    jmp dword ptr [assistVineFixtureTail]}}
__declspec(naked) static void assistVineFixtureCalculated(){__asm {pop esi
    pop ebp
    ret}}
static void __cdecl assistVineFixtureData(uint8_t* packet,void* source){
    assistFixturePacketData(packet,source);*(int*)(packet+8)=assistVineFixtureMode;
    assistVineFixtureCalculate(packet,g_criticalTarget);
}
static bool assistVineFixtureActor(const void* p,skilltelemetry::Actor& a){
    assistFixtureActor(p,a);if(p==g_personOther){a.force=3;a.city=60;}return true;
}
template<class Setup>
static void assistVineTelemetryTests(Setup setup,AssistAttempt attempt,uint8_t* cell,uint8_t* primary,uint8_t* target,
                                     uint8_t* source,uint8_t* person,uint8_t* other){
    auto oldData=g_assistFixturePacketDataFn;g_assistFixturePacketDataFn=assistVineFixtureData;
    assistVineFixtureTail=site(0x5b0c05);uint8_t saved[5];memcpy(saved,(void*)site(0x5b0c56),5);domesticStub(0x5b0c56,assistVineFixtureCalculated);
    assert(!memcmp((void*)site(0x5b0c21),"\x8b\x45\x08",3));
    float factors[]={.85f,.75f,.65f};uint32_t operands[]={0x5b0c4a,0x5b0c3f,0x5b0c34},old[3];
    for(int n=0;n<3;++n){old[n]=*(uint32_t*)site(operands[n]);*(uint32_t*)site(operands[n])=(uint32_t)&factors[n];}
    auto prepare=[&](int branch,int provider,int troops,int damage,int mode){
        setup(branch?4:0,branch==1?1:branch==2?5:0,troops,damage);
        *(int*)(source+0x10)=-1;*(int*)(other+0xe8)=-1;g_skills[249].effects[31]=true;
        *(int*)(target+0x10)=provider==1?14162:-1;
        *(int*)(person+0xe8)=provider==0?31:provider==2?249:-1;
        if(provider==1)*(int*)(other+0xe8)=31;
        memset(person+0x98,0,16);assistVineFixtureMode=mode;g_defenseSeed=0;
    };
    assert(skilltelemetry::open(L"build/assist_vine_test.csv",assistVineFixtureActor,nullptr));int inputs=0;
    for(int branch=0;branch<3;++branch)for(int provider=0;provider<4;++provider)for(int damage:{1,317})
        for(int mode:{0,3,4,5})for(int troops:{100,1000}){
            prepare(branch,provider,troops,damage,mode);int half=provider==3?damage:damage/2;
            int expected=(int)(half*(mode?(double)factors[mode-3]:1.));int loss=troops<expected?troops:expected;
            assert(attempt(cell,0,primary,target)==0 && g_assistFixturePackets==1 && *(uint16_t*)(target+0x18)==troops-loss);
            assert(g_defenseSeed==0x3039 && g_criticalRecords.empty() && !g_assist && !g_criticalCopyScope);++inputs;
        }
    assert(inputs==192);skilltelemetry::close();
    assert(skilltelemetry::open(L"build/assist_vine_failed_test.csv",assistVineFixtureActor,nullptr));
    for(int branch=0;branch<3;++branch)for(int provider=0;provider<4;++provider){prepare(branch,provider,1000,317,0);g_defenseSeed=4;
        assert(attempt(cell,0,primary,target)==0 && !g_assistFixturePackets && *(uint16_t*)(target+0x18)==1000 && g_defenseSeed==0xb01e55cd);}
    skilltelemetry::close();assert(skilltelemetry::open(L"build/assist_vine_edges_test.csv",assistVineFixtureActor,nullptr));
    for(int fault:{0,1,2,4,6,10,12}){prepare(0,0,1000,317,0);g_assistFixtureMutation=fault;
        assert(attempt(cell,0,primary,target)==0 && g_criticalRecords.empty());}
    skilltelemetry::close();prepare(0,0,1000,317,0);assert(attempt(cell,0,primary,target)==0 && *(uint16_t*)(target+0x18)==842);
    for(int n=0;n<3;++n)*(uint32_t*)site(operands[n])=old[n];memcpy((void*)site(0x5b0c56),saved,5);g_assistFixturePacketDataFn=oldData;
    puts("PASS assist vine31:192 native assist/relationship/chance/halving/coefficient/child-copy/aggregation/resource inputs, absent/raw/deputy/composed providers,12 failed rolls and7 selective stale controls");
}
