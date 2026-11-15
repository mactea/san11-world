static int g_strategyAmbushFault=0,g_strategyAmbushDamage=125;
static bool g_strategyAmbushMixed=false;
static int __fastcall strategyAmbushFixtureHandler(void* self,void*,const uint8_t* records,int mode){
    if(g_strategyAmbushMixed){auto group=(uint8_t*)records;memcpy(group+0x80,group,0x80);*(int*)(group+0x600)=2;}
    return ironwallFixtureHandler(self,nullptr,records,mode);
}
static int __cdecl strategyAmbushFixtureProducer(uint8_t* packet,void* source,int,uint32_t position,int,int critical,int){
    memset(packet,0,0x80);*(int*)packet=critical;*(int*)(packet+0xc)=g_strategyAmbushDamage;
    for(int offset:{0x50,0x60,0x64,0x68,0x6c})*(int*)(packet+offset)=-1;
    *(uint32_t*)(packet+0x58)=*(uint32_t*)((uint8_t*)source+4);*(uint32_t*)(packet+0x5c)=position;return 0;
}
static int __fastcall strategyAmbushFixtureGrant(void* self,void*,void* target,int amount){
    int result=((IncomeGrant)site(0x4ae4a0))(self,target,amount);
    if(g_strategyAmbushFault==8)skillsTelemetryStream(1,0);return result;
}
static int __cdecl strategyAmbushFixtureApply(uint8_t* packet){
    if(g_strategyAmbushFault==1)skillsTelemetryStream(1,0);
    if(g_strategyAmbushFault==2)*(int*)(g_personGuide+0xe8)=-1;
    if(g_strategyAmbushFault==3)*(int*)(g_criticalSource+0xc)=-1;
    if(g_strategyAmbushFault==4)++*(uint32_t*)(g_criticalTarget+4);
    if(g_strategyAmbushFault==5)++*(int*)(g_personGuide+0x94);
    uint8_t copy[0x80];if(g_strategyAmbushFault==6){memcpy(copy,packet,sizeof copy);packet=copy;}
    if(g_strategyAmbushFault==7)return 77;
    if(g_strategyAmbushFault==9)++*(int*)(packet+0xc);
    return runCriticalQueued(g_criticalTarget,g_criticalSource,packet);
}
static void strategyAmbushTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,uint8_t* person,uint8_t* spouse){
    auto oldSkill=g_skills[249];auto oldCritical=g_strategyCriticalOriginal,oldChance=g_strategyCriticalChanceOriginal;
    auto oldMax=g_strategyCriticalMaximumOriginal;auto oldMaxResume=g_criticalMaximumResume;auto oldAttribute=g_criticalAttributeOriginal;
    auto oldGod=g_strategyCriticalGodResume,oldDeep=g_strategyDeepResume,oldForcedAmbush=g_strategyAmbushResume;
    auto oldAmbush=g_ambushProducerResume,oldLeaf=g_strategyAmbushCommandResume;
    auto oldProducer=g_criticalProducerOriginal;auto oldHandler=g_criticalHandlerTwoOriginal;auto oldApply=g_ironwallApplyOriginal;
    auto oldCopy=g_criticalCopyTwoResume;auto oldOffset=g_criticalCopyTwoOffset;auto oldQueued=g_criticalQueuedTroopsResume;
    auto oldFind=g_ironwallFindResume,oldLimit=g_ironwallLimitResume,oldAggregate=g_ironwallAggregateResume;
    auto oldTroops=g_unitTroopsOriginal;auto oldStatus=g_strategyStatusOriginal;
    g_criticalSource=source;g_criticalTarget=target;
    g_strategyCriticalOriginal=(StrategyCritical)site(0x5af820);g_strategyCriticalChanceOriginal=(StrategyCritical)site(0x5af520);
    g_strategyCriticalMaximumOriginal=(CriticalMaximum)site(0x495680);g_criticalMaximumResume=(uint32_t)site(0x495687);
    g_criticalAttributeOriginal=(CriticalAttribute)site(0x489030);
    g_strategyCriticalGodResume=(uint32_t)site(0x5af5f8);g_strategyDeepResume=(uint32_t)site(0x5af605);g_strategyAmbushResume=(uint32_t)site(0x5af71f);
    g_ambushProducerResume=(uint32_t)site(0x5b0e96);g_strategyAmbushCommandResume=(uint32_t)site(0x592746);
    g_criticalProducerOriginal=strategyAmbushFixtureProducer;g_criticalHandlerTwoOriginal=(CriticalHandlerTwo)strategyAmbushFixtureHandler;
    g_ironwallApplyOriginal=strategyAmbushFixtureApply;g_unitTroopsOriginal=(IncomeGrant)strategyAmbushFixtureGrant;
    g_strategyStatusOriginal=(StatusGrant)site(0x4aea70);
    g_criticalCopyTwoResume=(uint32_t)site(0x5860d5);g_criticalCopyTwoOffset=(const int*)site(0x5860d8);
    g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;
    g_ironwallFindResume=(uint32_t)site(0x586571);g_ironwallLimitResume=(uint32_t)site(0x586594);g_ironwallAggregateResume=(uint32_t)site(0x586615);
    g_ironwallFixtureCopy=site(0x5860d0);g_ironwallFixtureDecision=site(0x586554);g_ironwallFixtureApply=site(0x586bab);
    domesticStub(0x5860de,ironwallFixtureCopied);domesticStub(0x5866b7,ironwallFixtureAggregated);domesticStub(0x586bb8,ironwallFixtureDone);
    domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);domesticStub(0x4957b0,criticalFixtureCapacity);
    domesticStub(0x5a06d0,defenseFixtureTerrain);domesticStub(0x489030,strategyFixtureIntelligence);domesticStub(0x495680,criticalMaximum);
    uint32_t criticalJumps[9],criticalRanges[4];int nationBonus[4]={};
    for(int n=0;n<9;++n)criticalJumps[n]=(uint32_t)site(*(uint32_t*)site(0x5af7e8+4*n));
    for(int n=0;n<4;++n)criticalRanges[n]=(uint32_t)site(*(uint32_t*)site(0x5af80c+4*n));
    *(uint32_t*)site(0x5af63f)=(uint32_t)criticalJumps;*(uint32_t*)site(0x5af73b)=(uint32_t)criticalRanges;
    for(uint32_t at:{0x5af67eu,0x5af6b5u,0x5af6d5u,0x5af70bu,0x5af774u,0x5af7afu})*(uint32_t*)site(at)=(uint32_t)nationBonus;
    float multiplier=0.008f;auto multiplierOperand=(uint32_t*)site(0x58669b);auto oldMultiplier=*multiplierOperand;
    *multiplierOperand=(uint32_t)&multiplier;
    // Undo the earlier status-only leaf adapter and its duration return seam.
    memcpy((void*)site(0x592740),"\x55\x8b\xec\x83\xe4\xf8",6);domesticStub(0x592740,strategyAmbush);
    memcpy((void*)site(0x592938),"\xe9\x9b\x00\x00\x00",5);
    for(uint32_t at:{0x5af5a6u,0x5af5adu})*(uint32_t*)site(at)=(uint32_t)g_criticalCells;
    for(uint32_t at:{0x5af55du,0x5af58eu,0x5af59au})*(uint32_t*)site(at)=200;
    *(uint32_t*)site(0x59279a)=(uint32_t)g_criticalCells+8;*(int*)site(0x59278d)=200;
    auto prepare=[&](int effect,int provider,int balance){
        g_skills[249]=Skill{};g_skills[249].defined=true;g_skills[249].effects[effect]=true;
        *(int*)(actor+0xe8)=provider==0?effect:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==1?effect:provider==3?249:-1;
        *(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;
        actor[0xc9]=other[0xc9]=effect==59?40:110;source[0xc6]=100;target[0xc6]=70;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(target+0xc)=1000;*(int*)(target+0x10)=601;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
        *(int*)(target+0x24)=0;target[0x28]=0;*(uint16_t*)(target+0x18)=(uint16_t)balance;
        g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        memset(g_criticalCells,0,sizeof g_criticalCells);*(uint32_t*)(g_criticalCells+20*401)=1;
        *(uint16_t*)(g_criticalCells+20*401+8)=4094;
        g_strategyAmbushFault=0;g_strategyAmbushMixed=false;g_strategyAmbushDamage=125;g_defenseSeed=0;g_criticalRecords.clear();
    };
    uint32_t command[3]={5,4095,0x10002};auto leaf=(StrategyAmbushCommand)site(0x592740);
    auto run=[&](int mode,bool decision){
        StrategyCommandScope owner(command,mode);
        int critical=decision?strategyCriticalAttempt(source,0x10001,0x10002,5):1;assert(critical==1);
        assert(leaf(nullptr,command,1,critical,mode,nullptr,1)==1);
        assert(g_criticalRecords.empty() && !g_criticalCopyScope && !g_ambushConstructionCaller && !g_strategyCritical);
    };
    assert(skilltelemetry::open(L"build/strategy_ambush_test.csv",afterFixtureActor,nullptr));int cases=0;
    for(int effect:{16,58,59,63,67})for(int provider=0;provider<4;++provider)for(int mode:{0,1,2})for(int balance:{0,100,1000}){
        prepare(effect,provider,balance);run(mode,true);
        uint32_t seed=0;for(int n=0;n<2;++n)seed=seed*0x6c078965u+0x3039u;
        assert(g_defenseSeed==seed && *(uint16_t*)(target+0x18)==(balance<125?0:balance-125));++cases;
    }
    skilltelemetry::close();assert(cases==180);
    assert(skilltelemetry::open(L"build/strategy_ambush_mixed_test.csv",afterFixtureActor,nullptr));
    for(int balance:{0,100,1000}){
        prepare(63,0,balance);g_strategyAmbushMixed=true;run(1,true);
        uint32_t seed=0;for(int n=0;n<2;++n)seed=seed*0x6c078965u+0x3039u;
        int damage=(int)((90+(seed>>16)%10)*(double)multiplier*250);
        seed=seed*0x6c078965u+0x3039u;if((seed>>16)%100<30)seed=seed*0x6c078965u+0x3039u;
        assert(g_defenseSeed==seed && *(uint16_t*)(target+0x18)==(balance<damage?0:balance-damage));
    }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/strategy_ambush_edges_test.csv",afterFixtureActor,nullptr));
    int affiliation=*(int*)(actor+0x94);
    for(int fault=1;fault<=9;++fault){prepare(63,0,1000);g_strategyAmbushFault=fault;run(1,true);
        assert(*(uint16_t*)(target+0x18)==(fault==7?1000:fault==9?874:875));*(int*)(actor+0x94)=affiliation;}
    prepare(63,0,1000);run(1,false);assert(*(uint16_t*)(target+0x18)==875);
    skilltelemetry::close();prepare(63,0,1000);run(1,true);assert(*(uint16_t*)(target+0x18)==875);
    g_skills[249]=oldSkill;g_strategyCriticalOriginal=oldCritical;g_strategyCriticalChanceOriginal=oldChance;
    g_strategyCriticalMaximumOriginal=oldMax;g_criticalMaximumResume=oldMaxResume;g_criticalAttributeOriginal=oldAttribute;
    g_strategyCriticalGodResume=oldGod;g_strategyDeepResume=oldDeep;g_strategyAmbushResume=oldForcedAmbush;
    g_ambushProducerResume=oldAmbush;g_strategyAmbushCommandResume=oldLeaf;g_criticalProducerOriginal=oldProducer;
    g_criticalHandlerTwoOriginal=oldHandler;g_ironwallApplyOriginal=oldApply;g_unitTroopsOriginal=oldTroops;g_strategyStatusOriginal=oldStatus;
    g_criticalCopyTwoResume=oldCopy;g_criticalCopyTwoOffset=oldOffset;g_criticalQueuedTroopsResume=oldQueued;
    g_ironwallFindResume=oldFind;g_ironwallLimitResume=oldLimit;g_ironwallAggregateResume=oldAggregate;
    *multiplierOperand=oldMultiplier;
    printf("PASS strategy ambush: %d original critical calculators/full native leaf/constructor/copy/single-aggregate/troop-store inputs;9 stale/no-store boundaries, unowned critical and source-frame cleanup\n",cases);
}
