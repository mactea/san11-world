// Both complete native counter parents/builders and their real resource
// paths run; pre-tail attack arithmetic, world, visibility and UI are fixtures.
template<class Setup,class Run>
static void counterVineTelemetryTests(Setup setup,Run run,uint8_t* target){
    memcpy((void*)site(0x5b0419),g_vineLayouts[0].bytes,g_vineLayouts[0].size);
    memcpy((void*)site(0x5b041f),g_vineLayouts[3].bytes,g_vineLayouts[3].size);
    domesticStub(0x5b0451,counterFixtureCalculated);
    auto prepare=[&](int combo,int provider,int water,int troops,int damage){
        setup(water?2:1,provider,water,troops,damage);g_skills[249].effects[31]=true;
        *(int*)((uint8_t*)g_counterFixtureActor+0xe8)=combo?249:31;g_defenseSeed=combo==2?0:4;
    };
    assert(skilltelemetry::open(L"build/counter_vine_test.csv",counterFixtureActor,nullptr));int inputs=0;
    for(int combo=0;combo<3;++combo)for(int provider:{0,1})for(int water:{0,1})for(int flag:{0,1})
        for(int route:{0,1})for(int count:{1,3})for(int optional:{0,1}){
            if(route && (count!=1 || optional))continue;
            for(int troops:{100,1000})for(int damage:{1,125}){
                prepare(combo,provider,water,troops,damage);uint32_t seed=g_defenseSeed;
                int expected=combo==2?0:damage/2;
                if(!route && count>1)expected=(int)((100.-count*5.)*expected*.01f);
                int loss=troops<expected?troops:expected;int result=run(route,count,flag,optional!=0);
                assert(result==(route?-loss:1) && *(uint16_t*)(target+0x18)==troops-loss);
                uint32_t expectedSeed=combo?seed*0x6c078965u+0x3039u:seed;
                assert(g_defenseSeed==expectedSeed && g_criticalRecords.empty() && !g_counterReactionScope && !g_criticalCopyScope);
                ++inputs;
            }
        }
    skilltelemetry::close();assert(inputs==480);
    assert(skilltelemetry::open(L"build/counter_vine_edges_test.csv",counterFixtureActor,nullptr));
    for(int fault=0;fault<10;++fault){prepare(0,0,0,1000,125);g_counterFixtureMutation=fault;run(0,3,0,true);}
    skilltelemetry::close();prepare(0,0,0,1000,125);run(0,3,0,true);
    puts("PASS counter vine31:480 native parent/builder/halving/scaling/grant inputs, both modes, first/high/composed providers,13/14 coeffects and authorized optional-word/scaling transport");
}
