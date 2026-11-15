// Execute the original assist parent/constructor/footer, native27/28 checks,
// RNG/tag/zero writes, vine tail, child copy/aggregation and troop stores.
// Pre-decision damage and the between-tail energy/technology leaves are fixtures.
static uintptr_t g_assistDefenseFixtureVineTail;
__declspec(naked) static void assistDefenseFixtureContinue(){__asm {jmp dword ptr [g_assistDefenseFixtureVineTail]}}
static void __cdecl assistDefenseFixtureData(uint8_t* packet,void* source){
    assistFixturePacketData(packet,source);*(int*)(packet+0x64)=-1;
    assistVineFixtureCalculate(packet,g_criticalTarget);
}
template<class Setup>
static void assistDefenseTelemetryTests(Setup setup,AssistAttempt attempt,uint8_t* cell,uint8_t* primary,uint8_t* target,
        uint8_t* source,uint8_t* person,uint8_t* other){
    auto oldData=g_assistFixturePacketDataFn;g_assistFixturePacketDataFn=assistDefenseFixtureData;
    auto oldTail=assistVineFixtureTail;assistVineFixtureTail=site(0x5b0b3a);g_assistDefenseFixtureVineTail=site(0x5b0c05);
    uint8_t saved[2][5];memcpy(saved[0],(void*)site(0x5b0b9e),5);memcpy(saved[1],(void*)site(0x5b0c56),5);
    domesticStub(0x5b0b9e,assistDefenseFixtureContinue);domesticStub(0x5b0c56,assistVineFixtureCalculated);
    auto prepare=[&](int branch,int mask,int provider,int troops,int damage,int seed){
        setup(branch?4:0,branch==1?1:branch==2?5:0,troops,damage);
        *(int*)(source+0x10)=-1;*(int*)(target+0x10)=14162;
        *(int*)(person+0xe8)=*(int*)(other+0xe8)=-1;
        memset(person+0x98,0,16);memset(other+0x98,0,16);
        g_skills[249].effects[27]=(mask&1)!=0;g_skills[249].effects[28]=(mask&2)!=0;
        if(provider==2)*(int*)(person+0xe8)=249;
        if(provider<2){auto first=provider?other:person;auto second=provider?person:other;
            *(int*)(first+0xe8)=mask&1?27:28;if(mask==3)*(int*)(second+0xe8)=28;}
        g_defenseSeed=seed;
    };
    auto draw=[](uint32_t& state,int percent){state=state*0x6c078965u+0x3039u;return ((state>>16)%100)<(uint32_t)percent;};
    assert(skilltelemetry::open(L"build/assist_defense_test.csv",assistVineFixtureActor,nullptr));int inputs=0;
    for(int branch=0;branch<3;++branch)for(int mask:{1,2,3})for(int provider=0;provider<4;++provider)
        for(int troops:{2999,3000})for(int damage:{499,500})for(int seed:{0,5}){
            prepare(branch,mask,provider,troops,damage,seed);uint32_t expectedSeed=seed;
            assert(draw(expectedSeed,branch==0?30:branch==1?50:20));int effect=-1;
            if(provider!=3 && troops<3000 && (mask&1) && draw(expectedSeed,50))effect=27;
            if(effect<0 && provider!=3 && damage<500 && (mask&2) && draw(expectedSeed,50))effect=28;
            int loss=effect>=0?0:damage;
            assert(attempt(cell,0,primary,target)==0 && g_assistFixturePackets==1 && *(uint16_t*)(target+0x18)==troops-loss);
            assert(g_defenseSeed==expectedSeed && g_criticalRecords.empty() && !g_assist && !g_criticalCopyScope);++inputs;
        }
    assert(inputs==288);skilltelemetry::close();
    assert(skilltelemetry::open(L"build/assist_defense_edges_test.csv",assistVineFixtureActor,nullptr));
    for(int fault:{0,1,2,4,6,8,10}){
        prepare(0,1,0,2999,499,5);g_assistFixtureMutation=fault;
        assert(attempt(cell,0,primary,target)==0 && *(uint16_t*)(target+0x18)==2999 && g_criticalRecords.empty());
    }
    // Both holders have27: native first member wins, not a stacked effect.
    prepare(1,1,0,2999,499,5);*(int*)(other+0xe8)=27;
    assert(attempt(cell,0,primary,target)==0 && *(uint16_t*)(target+0x18)==2999);
    // The later31 branch still executes on the zeroed packet. Preserve both
    // actual operations without inventing additional soldiers saved.
    for(int branch:{0,1}){
        prepare(branch,1,2,2999,499,5);g_skills[249].effects[31]=true;
        assert(attempt(cell,0,primary,target)==0 && *(uint16_t*)(target+0x18)==2999);
    }
    // A direct, unowned caller retains native results without a committed event.
    prepare(0,1,0,2999,499,5);assert(((AssistAttempt)assistAttemptNative)(cell,0,primary,target)==0 && *(uint16_t*)(target+0x18)==2999);
    skilltelemetry::close();assert(skilltelemetry::open(L"build/assist_defense_failed_test.csv",assistVineFixtureActor,nullptr));
    for(int branch=0;branch<3;++branch){prepare(branch,3,2,2999,499,4);
        assert(attempt(cell,0,primary,target)==0 && !g_assistFixturePackets && *(uint16_t*)(target+0x18)==2999 && g_defenseSeed==0xb01e55cd);}
    skilltelemetry::close();prepare(0,3,2,2999,499,5);uint32_t expectedSeed=5;draw(expectedSeed,30);draw(expectedSeed,50);
    assert(attempt(cell,0,primary,target)==0 && *(uint16_t*)(target+0x18)==2999 && g_defenseSeed==expectedSeed);
    memcpy((void*)site(0x5b0b9e),saved[0],5);memcpy((void*)site(0x5b0c56),saved[1],5);
    g_assistFixturePacketDataFn=oldData;assistVineFixtureTail=oldTail;
    puts("PASS assist27/28:288 native threshold/first-provider/50-percent/tag/zero/copy/settlement inputs, three assist/relationship sources, selective stale and failed-parent controls; exact RNG preserved");
}
