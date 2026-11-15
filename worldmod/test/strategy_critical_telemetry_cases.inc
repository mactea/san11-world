static uintptr_t g_strategyCriticalEntry;
__declspec(naked) static int __cdecl runStrategyCritical(void* unit,void* command,int success){__asm {push ebx
    push esi
    push edi
    push ebp
    mov edi,[esp+20]
    mov esi,[esp+24]
    mov ebp,[esp+28]
    xor ebx,ebx
    jmp dword ptr [g_strategyCriticalEntry]}}
__declspec(naked) static void strategyFixtureCriticalDone(){__asm {mov eax,ebx
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret}}
static int __fastcall strategyFixtureRange(void*,void*,uint32_t){return 0;}
static int __cdecl strategyFixtureCriticalWorld(void* unit,uint32_t from,uint32_t to,int plan){
    int result=((StrategyCritical)site(0x5af820))(unit,from,to,plan);skillsTelemetryStream(1,0);return result;
}
static int __cdecl strategyFixtureCriticalChanged(void* unit,uint32_t from,uint32_t to,int plan){
    int result=((StrategyCritical)site(0x5af820))(unit,from,to,plan);*(int*)(g_personGuide+0xe8)=-1;return result;
}
static void strategyCriticalTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,uint8_t* person,uint8_t* spouse,uint8_t* tiles){
    auto oldSkill=g_skills[249];auto oldAttempt=g_strategyCriticalOriginal,oldChance=g_strategyCriticalChanceOriginal;
    auto oldMaximum=g_strategyCriticalMaximumOriginal;auto oldMaxResume=g_criticalMaximumResume;auto oldAttribute=g_criticalAttributeOriginal;
    auto oldGod=g_strategyCriticalGodResume,oldDeep=g_strategyDeepResume,oldAmbush=g_strategyAmbushResume;
    g_strategyCriticalOriginal=(StrategyCritical)site(0x5af820);g_strategyCriticalChanceOriginal=(StrategyCritical)site(0x5af520);
    g_strategyCriticalMaximumOriginal=(CriticalMaximum)site(0x495680);g_criticalMaximumResume=(uint32_t)site(0x495687);
    g_criticalAttributeOriginal=(CriticalAttribute)site(0x489030);g_strategyCriticalEntry=site(0x59340e);
    g_strategyCriticalGodResume=(uint32_t)site(0x5af5f8);g_strategyDeepResume=(uint32_t)site(0x5af605);
    g_strategyAmbushResume=(uint32_t)site(0x5af71f);
    domesticStub(0x495680,criticalMaximum);domesticStub(0x489030,strategyFixtureIntelligence);
    domesticStub(0x490b00,personFixtureLookup);domesticStub(0x495a40,afterFixtureLeader);
    domesticStub(0x496160,strategyFixtureRange);domesticStub(0x59342e,strategyFixtureCriticalDone);
    memcpy((void*)site(0x59340e),"\x85\xed\x74\x1c",4); // prior decision-only fixture ended at59340c
    *(uint32_t*)site(0x5af5a6)=(uint32_t)tiles;*(uint32_t*)site(0x5af5ad)=(uint32_t)tiles;
    uint32_t jumps[9],ranges[4];for(int n=0;n<9;++n)jumps[n]=(uint32_t)site(*(uint32_t*)site(0x5af7e8+4*n));
    for(int n=0;n<4;++n)ranges[n]=(uint32_t)site(*(uint32_t*)site(0x5af80c+4*n));
    *(uint32_t*)site(0x5af63f)=(uint32_t)jumps;*(uint32_t*)site(0x5af73b)=(uint32_t)ranges;
    int nationBonus[4]={};for(uint32_t at:{0x5af67eu,0x5af6b5u,0x5af6d5u,0x5af70bu,0x5af774u,0x5af7afu})*(uint32_t*)site(at)=(uint32_t)nationBonus;
    *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
    auto setup=[&](int effect,int provider){
        memset(g_skills[249].effects,0,sizeof g_skills[249].effects);g_skills[249].effects[effect]=true;
        for(auto p:{actor,other,person,spouse}){*(int*)(p+0xe8)=-1;p[0xc9]=effect==59?40:110;}
        *(int*)(actor+0xe8)=provider==0 || provider==4 || provider==5?effect:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==1 || provider==2 || provider==4?effect:provider==3 || provider==5?249:-1;
        if(provider==2)other[0xc9]=effect==59?30:80;if(provider==5)actor[0xc9]=effect==59?60:120;
        source[0xc6]=100;target[0xc6]=70;g_strategySeed=123;
        g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;
    };
    int command[3]={0,0,0x10002};
    auto attempt=[&](int plan){command[0]=plan;g_strategySeed=123;assert(runStrategyCritical(source,command,1)==1);
        assert(g_strategySeed==123u*0x6c078965u+0x3039u && !g_strategyCritical && !g_criticalMaximumProbe && !g_progressionProbe);};
    assert(skilltelemetry::open(L"build/strategy_critical_test.csv",strategyMaximumActor,nullptr));
    for(int plan:{0,2,3,4,5,6})for(int effect:{63,67,58,59})for(int provider=0;provider<6;++provider){setup(effect,provider);attempt(plan);}
    for(int provider=0;provider<6;++provider){setup(16,provider);attempt(5);}
    setup(58,0);actor[0xc9]=71;attempt(0);
    setup(59,0);actor[0xc9]=0;attempt(0);
    for(int plan:{0,2,3,4,5,6}){
        setup(63,2);*(int*)(other+0xe8)=-1;
        for(int effect:{63,67,58,59,16})g_skills[249].effects[effect]=true;
        attempt(plan);g_skills[249].effects[63]=false;
        attempt(plan);g_skills[249].effects[67]=false;
        attempt(plan);actor[0xc9]=69;
        attempt(plan);if(plan==5){actor[0xc9]=70;attempt(plan);}
    }
    // Natural criticals after equal/wrong-way thresholds are not skill events.
    for(int effect:{58,59})for(int value:{69,70,71}){
        if((effect==58 && value==71) || (effect==59 && value==69))continue;
        setup(effect,0);actor[0xc9]=(uint8_t)value;command[0]=0;g_strategySeed=0;
        assert(runStrategyCritical(source,command,1)==1 && g_strategySeed==0x3039);
    }
    setup(59,0);actor[0xc9]=10;*(int*)(other+0xe8)=59;other[0xc9]=70;command[0]=0;g_strategySeed=0;
    assert(runStrategyCritical(source,command,1)==1); // maximum59 is70, not the weaker10 provider
    for(int plan:{1,7,8}){
        setup(63,2);for(int effect:{63,67,58,59,16})g_skills[249].effects[effect]=true;
        command[0]=plan;g_strategySeed=0;assert(runStrategyCritical(source,command,1)==(plan==1?0:1));
        assert(g_strategySeed==(plan==1?0u:0x3039u));
    }
    setup(58,0);assert(g_strategyCriticalOriginal(source,0x10001,0x10002,0)==1); // preview
    setup(58,0);command[0]=0;assert(runStrategyCritical(source,command,0)==0 && g_strategySeed==123); // failed command skips actual critical CALL
    setup(58,0);g_afterSourceRegistered=false;attempt(0);
    setup(58,0);g_afterTargetRegistered=false;attempt(0);
    setup(58,0);g_afterActorRegistered=false;attempt(0);
    setup(58,0);*(uint32_t*)(target+4)=0x10003;attempt(0);*(uint32_t*)(target+4)=0x10002;
    setup(58,0);g_strategyCriticalOriginal=strategyFixtureCriticalWorld;attempt(0);
    setup(58,0);g_strategyCriticalOriginal=strategyFixtureCriticalChanged;attempt(0);
    g_strategyCriticalOriginal=(StrategyCritical)site(0x5af820);
    skilltelemetry::close();setup(58,0);attempt(0);
    g_skills[249]=oldSkill;g_strategyCriticalOriginal=oldAttempt;g_strategyCriticalChanceOriginal=oldChance;
    g_strategyCriticalMaximumOriginal=oldMaximum;g_criticalMaximumResume=oldMaxResume;g_criticalAttributeOriginal=oldAttribute;
    g_strategyCriticalGodResume=oldGod;g_strategyDeepResume=oldDeep;g_strategyAmbushResume=oldAmbush;
    puts("PASS native strategy critical16/58/59/63/67: actual success gate/CALL,0..8 eligibility, first versus last-maximum providers, thresholds and natural/preview/stale exclusions");
}
