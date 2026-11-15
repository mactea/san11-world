// Native4b1280 excludes32 before its real RNG and captured/escaped list writes.
static int luckyCaptureMutation=0;
static bool luckyCaptureFixtureActor(const void* person,skilltelemetry::Actor& actor){
    ironwallCaptureFixtureActor(person,actor);
    if(person==g_personTarget){
        int fault=luckyCaptureMutation;luckyCaptureMutation=0;
        if(fault==1)skilltelemetry::invalidateContext();
        if(fault==2)*(int*)(g_personTarget+0xe8)=29;
        if(fault==3)*(int*)(g_criticalTarget+0xc)=-1;
        if(fault==4)*(int*)(g_criticalTarget+8)=-1;
        if(fault==5)g_personRegistered=false;
        if(fault==6)ironwallCaptureForce=4;
        if(fault==7)g_criticalSourceRegistered=false;
    }
    return true;
}
template<class Setup>
static void luckyCaptureTelemetryTests(Setup setup,void** vt){
    auto oldCombination=g_skills[249];auto oldOwner=vt[0x40/4];
    for(int e=0;e<LIMIT;++e)g_skills[249].effects[e]=false;g_skills[249].effects[32]=true;
    vt[0x40/4]=ironwallCaptureFixtureOwner;
    auto prepare=[&](int profile,int count,int item,int guarantee,int seed){
        setup(guarantee?0:4,0,count,item,0,0,1,seed);luckyCaptureMutation=0;ironwallCaptureForce=3;g_personRegistered=true;
        if(profile==0 || profile==3)*(int*)(g_personTarget+0xe8)=32;
        if(profile==1 || profile==3)*(int*)(g_personSpouse+0xe8)=32;
        if(profile==2)*(int*)(g_personTarget+0xe8)=249;
        if(profile==4 && count==2){*(int*)(g_criticalTarget+0x10)=14162;*(int*)(g_criticalSource+0x10)=-1;*(int*)(g_personOther+0xe8)=32;*(int*)(g_personOther+0xa0)=3;}
    };
    auto execute=[&](int count,bool scoped){
        uint8_t captured[0x20]={},escaped[0x20]={};int result;
        if(scoped){DefeatScope root(g_criticalTarget,g_criticalSource);result=captureAttempt(captured,escaped,g_criticalTarget,g_criticalSource,0,6,0,0);}
        else result=captureAttempt(captured,escaped,g_criticalTarget,g_criticalSource,0,6,0,0);
        assert(result==count && *(int*)(captured+0xc)+*(int*)(escaped+0xc)==count && !g_defeat && !g_captureAttempt);
        return *(int*)(captured+0xc);
    };
    int inputs=0;assert(skilltelemetry::open(L"build/lucky_capture_test.csv",luckyCaptureFixtureActor,nullptr));
    for(int profile=0;profile<6;++profile)for(int count:{1,2})for(int item:{0,1})for(int guarantee:{0,1})for(int seed:{0,4}){
        prepare(profile,count,item,guarantee,seed);uint32_t state=seed;int expected=0;
        for(int slot=0;slot<count;++slot){
            if((slot==0 && item) || profile==3 || (slot==0 && (profile==0 || profile==2)) || (slot==1 && (profile==1 || profile==4)))continue;
            state=state*0x6c078965u+0x3039u;if((state>>16)%100<(unsigned)(guarantee?100:35))++expected;
        }
        assert(execute(count,true)==expected && g_defenseSeed==state);++inputs;
    }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/lucky_capture_edges_test.csv",luckyCaptureFixtureActor,nullptr));
    for(int fault=0;fault<8;++fault){prepare(0,1,0,0,4);luckyCaptureMutation=fault;assert(execute(1,true)==0 && g_defenseSeed==4);}
    prepare(0,1,0,0,4);assert(execute(1,false)==0 && g_defenseSeed==4);
    prepare(0,1,0,0,4);{DefeatScope root(g_criticalTarget,g_criticalSource);CaptureAttemptScope attempt(&root);assert(luckyCaptureHas(g_personTarget,nullptr,32));} // query without the taken branch
    skilltelemetry::close();prepare(0,1,0,0,4);assert(execute(1,true)==0 && g_defenseSeed==4);
    g_skills[249]=oldCombination;vt[0x40/4]=oldOwner;g_personRegistered=true;ironwallCaptureForce=3;
    printf("PASS lucky capture: %d native32 exclusions/RNG/candidate lists, item/capture19 precedence, composed/high/duplicate holders, seven stale controls, unscoped/query-only/disabled parity\n",inputs);
}
