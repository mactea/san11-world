static uint8_t __fastcall strategyFixtureIntelligence(uint8_t* person,void*,int kind){assert(kind==2);return person[0xc9];}
static bool strategyMaximumActor(const void* person,skilltelemetry::Actor& actor){
    personFixtureActor(person,actor);if(person==g_personTarget || person==g_personSpouse){actor.force=3;actor.city=60;}return true;
}
static int __cdecl strategyFixtureSourceChanged(void* unit,uint32_t from,uint32_t to,int plan,int* detail){
    int result=((StrategyAttempt)site(0x5af4f0))(unit,from,to,plan,detail);*(int*)(g_personGuide+0xe8)=-1;return result;
}
static void strategyMaximumTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,uint8_t* person,uint8_t* spouse){
    auto oldSkill=g_skills[249];auto oldMaximum=g_criticalMaximumResume;auto oldAttribute=g_criticalAttributeOriginal;
    auto oldAttempt=g_strategyAttemptOriginal;
    g_criticalMaximumResume=(uint32_t)site(0x495687);g_criticalAttributeOriginal=(CriticalAttribute)site(0x489030);
    domesticStub(0x495680,criticalMaximum);domesticStub(0x489030,strategyFixtureIntelligence);
    domesticStub(0x490b00,personFixtureLookup);domesticStub(0x495a40,afterFixtureLeader);
    int terrainChances[32];for(auto& v:terrainChances)v=80;
    auto oldTerrain=*(uint32_t*)site(0x5aea88);*(uint32_t*)site(0x5aea88)=(uint32_t)terrainChances;
    *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
    *(int*)(target+0xc)=1000;*(int*)(target+0x10)=601;*(int*)(target+0x14)=-1;
    auto setup=[&](int effect,bool defensive,int provider){
        memset(g_skills[249].effects,0,sizeof g_skills[249].effects);g_skills[249].effects[effect]=true;
        for(auto p:{actor,other,person,spouse}){*(int*)(p+0xe8)=-1;p[0xc9]=110;}
        auto first=defensive?person:actor,second=defensive?spouse:other;
        *(int*)(first+0xe8)=provider==0 || provider==4 || provider==5?effect:provider==2?249:-1;
        *(int*)(second+0xe8)=provider==1 || provider==2 || provider==4?effect:provider==3 || provider==5?249:-1;
        if(provider==2)second[0xc9]=80;if(provider==5)first[0xc9]=120;
        source[0xc6]=100;target[0xc6]=70;
        g_strategyEnemy=true;g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;g_strategySeed=123;
    };
    auto attempt=[&](int plan,bool defensive){int detail[2]={-1,-1};int result=runStrategyAttempt(source,0x10001,0x10002,plan,detail);
        assert(result==(defensive?0:1) && detail[1]==(defensive?1:0));
        assert(g_strategySeed==(defensive?123u:123u*0x6c078965u+0x3039u));
        assert(!g_strategyImmunity && !g_criticalMaximumProbe && !g_progressionProbe);
    };
    assert(skilltelemetry::open(L"build/strategy_maximum_test.csv",strategyMaximumActor,nullptr));
    for(const auto& s:g_strategyMaximumSites){
        int plan=s.call<=0x5aec2b?0:s.call<=0x5aec7c?1:s.call<=0x5aed7c?2:s.call<=0x5aeefa?3:
            s.call<=0x5af025?4:s.call<=0x5af10a?5:6;
        for(int provider=0;provider<6;++provider){setup(s.effect,s.defensive,provider);attempt(plan,s.defensive);}
    }
    // Equal and lower values lose the native strict comparison; a later real
    // provider wins instead. Both failed and winning eligibility queries stay.
    for(int effect:{53,54,55,56,57,62})for(int value:{69,70}){
        int plan=effect==53 || effect==62?0:effect==54?2:effect==55?3:effect==56?6:4;
        setup(effect,false,0);actor[0xc9]=(uint8_t)value;*(int*)(other+0xe8)=63;other[0xc9]=110;attempt(plan,false);
    }
    for(int effect:{60,63})for(int value:{99,100}){
        setup(effect,true,0);person[0xc9]=(uint8_t)value;*(int*)(actor+0xe8)=57;actor[0xc9]=110;attempt(0,false);
    }
    // Native ordering changes across strategies: fire defense wins before
    // offense; confusion evaluates offensive overrides before stat defense.
    setup(60,true,0);*(int*)(actor+0xe8)=57;attempt(0,true);
    setup(60,true,0);*(int*)(actor+0xe8)=54;attempt(2,false);
    int detail[2];
    setup(54,false,0);assert(g_strategyAttemptOriginal(source,0x10001,0x10002,2,detail)==1); // UI/AI probability only
    setup(54,false,0);g_afterSourceRegistered=false;attempt(2,false);
    setup(54,false,0);g_afterTargetRegistered=false;attempt(2,false);
    setup(54,false,0);g_afterActorRegistered=false;attempt(2,false);
    setup(54,false,0);*(uint32_t*)(target+4)=0x10003;attempt(2,false);*(uint32_t*)(target+4)=0x10002;
    setup(54,false,0);g_strategyAttemptOriginal=strategyFixtureAttemptWorld;attempt(2,false);
    setup(54,false,0);g_strategyAttemptOriginal=strategyFixtureSourceChanged;attempt(2,false);
    g_strategyAttemptOriginal=oldAttempt;
    setup(54,false,0);g_skills[249].effects[61]=true;*(int*)(person+0xe8)=61;
    int result=runStrategyAttempt(source,0x10001,0x10002,2,detail);assert(result==0 && detail[1]==1 && g_strategySeed==123); // direct immunity precedence
    skilltelemetry::close();setup(54,false,0);attempt(2,false);
    g_skills[249]=oldSkill;g_criticalMaximumResume=oldMaximum;g_criticalAttributeOriginal=oldAttribute;
    *(uint32_t*)site(0x5aea88)=oldTerrain;
    puts("PASS native strategy max-stat: all31 comparison sites, last-equal/max provider, raw/composed and high IDs,0/100 outcomes and RNG, strict thresholds and preview/stale exclusions");
}
