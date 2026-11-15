static void strategyCharmTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,uint8_t* person,uint8_t* spouse,uint8_t* cells,int width){
    auto oldSkill=g_skills[249];auto oldHas=g_strategyCharmHasResume,oldDouble=g_strategyCharmDoubleResume;
    auto oldAttempt=g_strategyAttemptOriginal;
    auto oldFind=(*(void***)source)[0x5c/4];(*(void***)source)[0x5c/4]=(void*)site(0x495600);
    domesticStub(0x491310,personFixtureId);
    g_strategyCharmHasResume=(uint32_t)site(0x5af480);g_strategyCharmDoubleResume=(uint32_t)site(0x5af4a0);
    g_strategyAttemptOriginal=(StrategyAttempt)site(0x5af4f0);g_strategyAttemptEntry=site(0x593402);
    domesticStub(0x59340c,strategyFixtureAttemptDone);
    // Use the complete native member/gender guard and native x87 conversion.
    float multiplier=11.0f;for(uint32_t at:{0x5aeca8u,0x5aecd7u,0x5af069u,0x5af09cu})*(uint32_t*)site(at)=(uint32_t)&multiplier;
    int nation[4]={};for(uint32_t at:{0x5aed99u,0x5aef28u,0x5af05du,0x5af152u,0x5af294u,0x5af3ceu})*(uint32_t*)site(at)=(uint32_t)nation;
    // Relocate only verified data operands (other source tables are unchanged).
    auto setup=[&](int provider,int src,int dst,int gender){
        memset(g_skills[249].effects,0,sizeof g_skills[249].effects);g_skills[249].effects[69]=true;
        *(int*)(actor+0xe8)=provider==0?69:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==1?69:provider==3?249:-1;
        *(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;
        *(int*)(actor+0x40)=*(int*)(other+0x40)=*(int*)(person+0x40)=*(int*)(spouse+0x40)=0;
        if(gender==1)*(int*)(spouse+0x40)=1;
        if(gender==2)*(int*)(person+0x40)=*(int*)(spouse+0x40)=1;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        *(int*)(target+0xc)=1000;*(int*)(target+0x10)=601;*(int*)(target+0x14)=-1;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
        source[0xc6]=(uint8_t)src;target[0xc6]=(uint8_t)dst;
        *(int*)(source+0x24)=*(int*)(target+0x24)=0;
        auto cell=(uint32_t*)(cells+20*(2*width+1));cell[0]=1;cell[1]=0;cell[2]=4094;
        g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;g_strategyEnemy=true;
    };
    auto baseline=[&](int plan){int first=*(int*)(actor+0xe8),second=*(int*)(other+0xe8);
        *(int*)(actor+0xe8)=*(int*)(other+0xe8)=-1;int detail[2];
        int chance=g_strategyChanceOriginal(source,0x10001,0x10002,plan,detail);
        *(int*)(actor+0xe8)=first;*(int*)(other+0xe8)=second;return chance;};
    auto run=[&](int plan,int base,int seed,bool boosted){
        int chance=boosted?(base>=50?100:base*2):base;uint32_t next=(uint32_t)seed;
        if(chance>0)next=next*0x6c078965u+0x3039u;
        int expected=chance>0 && (next>>16)%100<(uint32_t)chance;
        g_strategySeed=seed;int detail[2]={-1,-1};
        assert(runStrategyAttempt(source,0x10001,0x10002,plan,detail)==expected && detail[1]==0 && g_strategySeed==next);
        assert(!g_strategyImmunity && !g_progressionProbe);return chance;
    };
    int inputs[][2]={{10,100},{50,100},{100,70},{150,1}};
    int expectedBases[7][4]={{1,15,56,99},{34,73,99,99},{1,1,45,99},{1,1,55,99},{34,73,99,99},{1,1,49,99},{1,1,45,99}};
    assert(skilltelemetry::open(L"build/strategy_charm_test.csv",strategyMaximumActor,nullptr));
    for(int plan=0;plan<7;++plan)for(int inputId=0;inputId<4;++inputId){auto& input=inputs[inputId];
        setup(0,input[0],input[1],0);int base=baseline(plan);assert(base>=0 && base<100);
        assert(base==expectedBases[plan][inputId]);
        for(int provider=0;provider<4;++provider)for(int gender=0;gender<3;++gender)for(int seed:{0,4}){
            setup(provider,input[0],input[1],gender);run(plan,base,seed,gender==0);
        }
    }
    int boundarySource=-1;
    for(int src=100;src<=140;++src){setup(0,src,70,0);if(baseline(5)==50){boundarySource=src;break;}}
    assert(boundarySource>=100);
    for(int provider=0;provider<4;++provider)for(int seed:{0,4}){setup(provider,boundarySource,70,0);run(5,50,seed,true);}
    setup(0,100,70,0);*(int*)(actor+0x40)=1;run(0,56,0,true); // source gender is not an added prerequisite
    uint32_t boundAddresses[]={0x495e7e,0x495e9e,0x495ead},savedBounds[3];
    for(int n=0;n<3;++n)savedBounds[n]=*(uint32_t*)site(boundAddresses[n]);
    for(int bound:{1099,14612}){
        setup(0,100,70,0);*(int*)(source+0x10)=-1;*(int*)(target+0x10)=14162;*(int*)(other+0x40)=1;
        for(uint32_t at:boundAddresses)*(int*)site(at)=bound;
        run(0,56,4,bound==1099); // honor the actual stock/M10 member-count bounds
    }
    for(int n=0;n<3;++n)*(uint32_t*)site(boundAddresses[n])=savedBounds[n];
    setup(0,100,70,0);*(int*)(target+0x10)=-1;*(int*)(target+0x14)=601;*(int*)(spouse+0x40)=1;
    run(0,56,0,true); // preserve native sparse-slot behavior; do not infer that every member is male
    setup(2,50,100,0);*(int*)(other+0xe8)=69;run(0,15,4,true); // native first holder, one non-stacking doubling
    for(int plan:{7,8}){setup(0,100,70,0);int base=baseline(plan);for(int seed:{0,4})run(plan,base,seed,false);}
    setup(0,100,70,0);*(int*)(actor+0xe8)=-1;run(0,56,4,false);
    setup(0,100,70,0);*(int*)(other+0xe8)=57;other[0xc9]=110;
    g_strategySeed=0;int detail[2];assert(runStrategyAttempt(source,0x10001,0x10002,0,detail)==1); // earlier forced return bypasses69
    setup(0,100,70,0);assert(g_strategyAttemptOriginal(source,0x10001,0x10002,0,detail)==1); // unscoped preview, query only
    for(int reason=0;reason<6;++reason){
        setup(0,100,70,0);
        if(reason==0)g_afterActorRegistered=false;
        if(reason==1)g_afterSourceRegistered=false;
        if(reason==2)g_afterTargetRegistered=false;
        if(reason==3)*(uint32_t*)(target+4)=0x10003;
        if(reason==4)g_strategyAttemptOriginal=strategyFixtureAttemptWorld;
        if(reason==5)g_strategyAttemptOriginal=strategyFixtureSourceChanged;
        run(0,56,0,true);g_strategyAttemptOriginal=(StrategyAttempt)site(0x5af4f0);
    }
    skilltelemetry::close();
    setup(0,100,70,0);run(0,56,4,true);
    g_skills[249]=oldSkill;g_strategyCharmHasResume=oldHas;g_strategyCharmDoubleResume=oldDouble;g_strategyAttemptOriginal=oldAttempt;
    (*(void***)source)[0x5c/4]=oldFind;
    puts("PASS native69: real whole chance calculator, member/gender guard, native doubling/cap/sample and provider/target attribution");
}
