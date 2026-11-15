// Reuse the relocated native593402/5af4f0/5ae980 fixture and its real LCG.
static void strategyFireImmunityTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,
    uint8_t* other,uint8_t* person,uint8_t* spouse){
    bool old62=g_skills[249].effects[62];g_skills[249].effects[62]=true;
    auto oldResume=g_strategyFireImmunityResume;g_strategyFireImmunityResume=(uint32_t)site(0x5aebb7);
    int terrainChances[32];for(auto& chance:terrainChances)chance=80;
    auto oldTerrain=*(uint32_t*)site(0x5aea88);*(uint32_t*)site(0x5aea88)=(uint32_t)terrainChances;
    auto setup=[&](int provider){
        for(int effect:{61,72,73,74})g_skills[249].effects[effect]=false;
        *(int*)(actor+0xe8)=*(int*)(spouse+0xe8)=-1;
        *(int*)(person+0xe8)=provider==0?62:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==1?62:provider==2?62:provider==3?249:-1;
        *(uint32_t*)(target+4)=0x10002;g_strategyEnemy=true;g_strategySeed=123;
        g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;
        g_strategyAttemptOriginal=(StrategyAttempt)site(0x5af4f0);
    };
    auto immune=[&](){int detail[2]={-1,-1};
        assert(runStrategyAttempt(source,0x10001,0x10002,0,detail)==0 && detail[0]==0 && detail[1]==1);
        assert(g_strategySeed==123 && !g_strategyImmunity && !g_progressionProbe);
    };
    assert(skilltelemetry::open(L"build/strategy_fire_immunity_test.csv",strategyImmunityActor,nullptr));
    for(int provider=0;provider<4;++provider)for(bool enemy:{false,true}){setup(provider);g_strategyEnemy=enemy;immune();}
    for(int owned:{62,249}){setup(0);*(int*)(person+0xe8)=*(int*)(other+0xe8)=owned;immune();}
    for(int provider:{0,3})for(bool chanceOnly:{false,true}){
        setup(provider);int detail[2];int result=(chanceOnly?(StrategyAttempt)site(0x5ae980):g_strategyAttemptOriginal)(source,0x10001,0x10002,0,detail);
        assert(result==0 && detail[1]==1 && g_strategySeed==123); // shared previews remain query-only
    }
    for(int invalid=0;invalid<6;++invalid){
        setup(0);
        if(invalid==0)g_afterSourceRegistered=false;
        if(invalid==1)g_afterTargetRegistered=false;
        if(invalid==2)g_afterActorRegistered=false;
        if(invalid==3)*(uint32_t*)(target+4)=0x10003;
        if(invalid==4)g_strategyAttemptOriginal=strategyFixtureAttemptWorld;
        if(invalid==5)g_strategyAttemptOriginal=strategyFixtureAttemptChanged;
        immune();
    }
    setup(-1);*(int*)(actor+0xe8)=62;int detail[2];
    assert(runStrategyAttempt(source,0x10001,0x10002,0,detail)==1 && detail[1]==0 &&
        g_strategySeed==123u*0x6c078965u+0x3039u); // source62 is not defender immunity
    setup(0);assert(runStrategyAttempt(source,0x10001,0x10002,1,detail)==1 && detail[1]==0 &&
        g_strategySeed==123u*0x6c078965u+0x3039u); // target62 does not block plan1
    setup(0);*(int*)(other+0xe8)=61;immune(); // earlier61 wins before fire62 is tested
    skilltelemetry::close();
    for(int provider=0;provider<4;++provider){setup(provider);immune();}
    g_strategyFireImmunityResume=oldResume;g_skills[249].effects[62]=old62;
    *(uint32_t*)site(0x5aea88)=oldTerrain;
    puts("PASS native fire immunity62:23 inputs,10 raw/composed provider effects, previews/stale owners/source-only/other-plan/61 precedence and4 disabled controls; unchanged RNG");
}
