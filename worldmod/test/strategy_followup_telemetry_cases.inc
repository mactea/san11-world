static uintptr_t g_followupFixtureEntry,g_followupFixtureChainTail;
static int g_followupCalls=0,g_followupRelation=0;
static bool g_followupInvalidate=false,g_followupChangeSkill=false,g_followupFakeResult=false,g_followupChangeTarget=false;
static int g_followupTargetOwner=-1;
static int __fastcall followupFixtureOwner(void* unit,void*){
    return unit==g_afterTarget && g_followupTargetOwner>=0?g_followupTargetOwner:afterFixtureOwner(unit,nullptr);
}
__declspec(naked) static int __cdecl runStrategyFollowup(void* world,void* source,void* command,int primaryResult,uint32_t destination){__asm {push ebx
    push esi
    push edi
    push ebp
    mov ecx,[esp+20]
    mov esi,[esp+24]
    mov ebx,[esp+28]
    mov eax,[esp+32]
    mov edi,[esp+36]
    xor ebp,ebp
    sub esp,0x460
    mov [esp+0x1c],ecx
    mov [esp+0x438],ebx
    jmp dword ptr [g_followupFixtureEntry]}}
__declspec(naked) static void followupFixtureCandidate(){__asm {jmp dword ptr [g_followupFixtureChainTail]}}
__declspec(naked) static void followupFixtureDone(){__asm {add esp,0x460
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret}}
static int __fastcall followupFixtureCommand(void* self,void*,const uint32_t* command,int mode,void* actor){
    ++g_followupCalls;
    if(g_followupFakeResult)return 1;
    int result=((StrategyCommand)site(0x5933a0))(self,command,mode,actor);
    if(g_followupInvalidate)skillsTelemetryStream(1,0);
    if(g_followupChangeSkill)*(int*)((uint8_t*)actor+0xe8)=-1;
    if(g_followupChangeTarget)g_followupTargetOwner=4;
    return result;
}
static void* __fastcall followupFixtureObject(uint8_t* cell,void*){
    return (*(uint32_t*)cell&3)==1?strategyFireFixtureUnit(nullptr,nullptr,*(uint16_t*)(cell+8)):nullptr;
}
static int __fastcall followupFixtureKind(void*,void*,int kind){return kind==11;}
static int __cdecl followupFixtureUnitKind(){return 11;}
static void* __cdecl followupFixtureCast(void* object){return object;}
static int __fastcall followupFixtureRelation(void*,void*,void*){return g_followupRelation;}
static int __cdecl followupFixtureVisible(uint32_t){return 1;}
static int __cdecl followupFixtureHidden(void*){return 1;}
static void __fastcall followupFixtureUi3(void*,void*,int,void*,int){}
static void __fastcall followupFixtureUi2(void*,void*,int,void*){}
static int __fastcall followupFixtureBaseId(void*,void*,void*){return -1;}
static int __fastcall followupFixtureLeaf(void* self,void*,void* command,int success,int critical,int mode,void* actor,int detail){
    if(*(int*)command==0)return strategyFireFixtureLeaf(self,nullptr,command,success,critical,mode,actor,detail);
    return 1; // leaf status/damage effects for other strategies remain isolated
}
static void strategyFollowupTelemetryTests(uint8_t* world,uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,uint8_t* person,uint8_t* spouse,uint8_t* cells,int width){
    auto oldOriginal=g_strategyFollowupOriginal;auto oldBaseId=g_telemetryBaseId;
    auto oldOwner=(*(void***)source)[0x40/4];(*(void***)source)[0x40/4]=followupFixtureOwner;
    auto oldChain=g_strategyChainFindResume,oldReflect=g_strategyReflectFindResume;
    auto oldSkill=g_skills[249];g_strategyFollowupOriginal=(StrategyCommand)followupFixtureCommand;
    g_strategyChainFindResume=(uint32_t)site(0x5936a9);g_strategyReflectFindResume=(uint32_t)site(0x5939eb);
    g_telemetryBaseId=(BuildingId)followupFixtureBaseId;
    g_followupFixtureEntry=site(0x593681);g_followupFixtureChainTail=site(0x593945);
    domesticStub(0x5936ba,followupFixtureCandidate);domesticStub(0x593a7a,followupFixtureDone);
    domesticStub(0x4917c0,afterFixtureId);domesticStub(0x483b40,followupFixtureObject);
    domesticStub(0x496bc0,followupFixtureRelation);domesticStub(0x620f30,followupFixtureCast);
    domesticStub(0x468e60,followupFixtureUnitKind);domesticStub(0x5a06c0,followupFixtureVisible);
    domesticStub(0x5a0700,followupFixtureHidden);domesticStub(0x5899c0,followupFixtureUi3);domesticStub(0x589980,followupFixtureUi2);
    for(uint32_t leaf:{0x591270u,0x5915e0u,0x5917d0u,0x591a20u,0x591c70u,0x592740u,0x592a80u,0x592050u,0x592ed0u})domesticStub(leaf,followupFixtureLeaf);
    (*(void***)source)[0x2c/4]=followupFixtureKind;
    *(uint32_t*)site(0x593989)=(uint32_t)cells;*(int*)site(0x59397a)=width;
    *(uint32_t*)site(0x5933de)=(uint32_t)cells;
    int nation[4]={};
    for(uint32_t at:{0x5af67eu,0x5af6b5u,0x5af6d5u,0x5af70bu,0x5af774u,0x5af7afu,0x5af3ceu})*(uint32_t*)site(at)=(uint32_t)nation;
    uint32_t ranges[4];for(int n=0;n<4;++n)ranges[n]=(uint32_t)site(*(uint32_t*)site(0x5af80c+4*n));*(uint32_t*)site(0x5af73b)=(uint32_t)ranges;
    int command[3]={0,4095,0x10002};
    auto setup=[&](int mode,int provider,int plan,bool succeeds){
        memset(g_skills[249].effects,0,sizeof g_skills[249].effects);g_skills[249].effects[mode==2?66:68]=true;
        for(auto p:{actor,other,person,spouse}){*(int*)(p+0xe8)=-1;p[0xc9]=110;}
        auto first=mode==2?actor:person,second=mode==2?other:spouse;
        auto selected=(provider&1)?second:first,helper=(provider&1)?first:second;
        *(int*)(selected+0xe8)=provider<2?(mode==2?66:68):249;*(int*)(helper+0xe8)=57;
        if(!succeeds && plan!=8)*(int*)((mode==2?person:actor)+0xe8)=61;
        source[0xc6]=100;target[0xc6]=70;*(int*)(source+0x24)=*(int*)(target+0x24)=0;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        *(int*)(target+0xc)=1000;*(int*)(target+0x10)=601;*(int*)(target+0x14)=-1;
        for(int row:{1,2}){auto cell=(uint32_t*)(cells+20*(row*width+1));cell[0]=1;cell[1]=0;cell[2]=row==1?4095:4094;}
        command[0]=plan;command[2]=mode==2?0x10003:0x10002;
        g_strategySeed=!succeeds && plan==8?4:0;g_strategyEnemy=true;
        g_fireInvalidate=g_fireMutate=g_fireCancel=g_fireChangeSkill=g_fireMoveTarget=false;g_fireRepeats=1;
        *(DWORD*)(world+0x5e0)=GetCurrentThreadId()+1;
        g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;
        g_followupRelation=0;g_followupInvalidate=g_followupChangeSkill=g_followupFakeResult=g_followupChangeTarget=false;
        g_followupTargetOwner=-1;g_followupCalls=0;
    };
    auto run=[&](int mode){int result=runStrategyFollowup(world,source,command,mode==2?1:0,0x10002);
        assert(!g_strategyFollowup && !g_strategyCommand && g_strategyFirePending.empty());return result;};
    assert(skilltelemetry::open(L"build/strategy_followup_test.csv",strategyMaximumActor,nullptr));
    for(int mode:{2,1})for(int provider=0;provider<4;++provider)for(int plan=0;plan<(mode==2?5:9);++plan)for(bool succeeds:{true,false}){
        setup(mode,provider,plan,succeeds);assert(run(mode)==(succeeds?1:0) && g_followupCalls==1);
    }
    // True native provider/status/relation gates prevent followup execution.
    setup(2,0,0,true);*(int*)(actor+0xe8)=-1;run(2);assert(g_followupCalls==0 && g_strategySeed==0);
    setup(1,0,0,true);*(int*)(person+0xe8)=-1;run(1);assert(g_followupCalls==0 && g_strategySeed==0);
    setup(1,0,0,true);*(int*)(target+0x24)=1;run(1);assert(g_followupCalls==0 && g_strategySeed==0);
    setup(1,0,0,true);g_followupRelation=1;run(1);assert(g_followupCalls==0 && g_strategySeed==0);
    for(int reason=0;reason<5;++reason){
        setup(2,0,0,true);
        if(reason==0)g_afterActorRegistered=false;
        if(reason==1)g_afterSourceRegistered=false;
        if(reason==2)g_followupInvalidate=true;
        if(reason==3)g_followupChangeSkill=true;
        if(reason==4)g_followupFakeResult=true;
        assert(run(2)==1 && g_followupCalls==1);
    }
    setup(2,0,0,true);uint32_t direct[3]={0,4095,0x10002};
    assert(((StrategyCommand)site(0x5933a0))(world,direct,2,actor)==1); // same mode/trait without native593a75 origin
    setup(2,0,0,true);g_followupChangeTarget=true;assert(run(2)==1 && followupFixtureOwner(target,nullptr)==4);
    skilltelemetry::close();setup(2,0,0,true);assert(run(2)==1 && g_followupCalls==1);
    g_skills[249]=oldSkill;g_strategyFollowupOriginal=oldOriginal;g_telemetryBaseId=oldBaseId;
    g_strategyChainFindResume=oldChain;g_strategyReflectFindResume=oldReflect;
    (*(void***)source)[0x40/4]=oldOwner;
    puts("PASS native66/68 followup: real success/failure/provider/status/relation gates, actual argument construction and whole subcommand, all5/9 strategies, result/snapshot and preview/stale exclusions");
}
