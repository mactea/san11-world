// Complete native4b1280 candidate selection, native29 multiplier and native
// chance/RNG/list decisions. Defeat parent and world/list primitives remain fixtures.
static int ironwallCaptureFault=0,ironwallCaptureForce=3;
static bool __cdecl ironwallCaptureFixtureActor(const void* p,skilltelemetry::Actor& actor){
    personFixtureActor(p,actor);
    if(afterAttackMember(g_criticalTarget,p)){actor.force=3;actor.ruler=700;actor.city=60;}
    return true;
}
static int __fastcall ironwallCaptureFixtureOwner(void* unit,void*){return unit==g_criticalSource?2:ironwallCaptureForce;}
static int __cdecl ironwallCaptureFixtureRoll(int chance){
    int result=((CaptureRoll)site(0x4721d0))(chance);
    if(ironwallCaptureFault==1)skilltelemetry::invalidateContext();
    if(ironwallCaptureFault==2)*(int*)(g_personTarget+0xe8)=-1;
    if(ironwallCaptureFault==3)*(int*)(g_criticalTarget+0xc)=-1;
    if(ironwallCaptureFault==4)*(int*)(g_criticalTarget+8)=-1;
    if(ironwallCaptureFault==5)g_personRegistered=false;
    if(ironwallCaptureFault==6)ironwallCaptureForce=4;
    if(ironwallCaptureFault==7)g_criticalSourceRegistered=false;
    return result;
}
template<class Setup>
static void ironwallCaptureTelemetryTests(Setup setup,void** vt){
    auto oldCombination=g_skills[249];auto oldOwner=vt[0x40/4];auto oldRoll=g_captureRollOriginal;
    auto oldValid=g_growthValid;g_growthValid=(GrowthValid)site(0x47a630);
    for(int e=0;e<LIMIT;++e)g_skills[249].effects[e]=false;g_skills[249].effects[29]=true;
    vt[0x40/4]=ironwallCaptureFixtureOwner;
    auto prepare=[&](int profile,int terrain,int seed){
        setup(4,0,2,0,0,0,1,seed);ironwallCaptureFault=0;ironwallCaptureForce=3;g_personRegistered=true;
        if(profile==0 || profile==3)*(int*)(g_personTarget+0xe8)=29;
        if(profile==1 || profile==3)*(int*)(g_personSpouse+0xe8)=29;
        if(profile==2)*(int*)(g_personTarget+0xe8)=249;
        if(profile==4){
            *(int*)(g_criticalTarget+0x10)=14162;*(int*)(g_criticalSource+0x10)=-1;
            *(int*)(g_personOther+0xe8)=29;*(int*)(g_personOther+0xa0)=3;
        }
        *(uint32_t*)(g_criticalCells+20*401+4)=terrain;
    };
    auto execute=[&](int count,bool scoped){
        uint8_t captured[0x20]={},escaped[0x20]={};int result;
        if(scoped){DefeatScope root(g_criticalTarget,g_criticalSource);
            result=captureAttempt(captured,escaped,g_criticalTarget,g_criticalSource,0,count,-1,0);}
        else result=captureAttempt(captured,escaped,g_criticalTarget,g_criticalSource,0,count,-1,0);
        assert(result==2 && *(int*)(captured+0xc)+*(int*)(escaped+0xc)==2);
        assert(!g_defeat && !g_captureAttempt);return *(int*)(captured+0xc);
    };
    assert(skilltelemetry::open(L"build/ironwall_capture_test.csv",ironwallCaptureFixtureActor,nullptr));int inputs=0;
    for(int profile=0;profile<6;++profile)for(int count:{0,1,2,6})for(int terrain:{0,3})for(int seed:{0,4,54}){
        prepare(profile,terrain,seed);uint32_t state=seed;int expected=0;
        for(int slot=0;slot<2;++slot){
            bool protectedPerson=profile==3 || (slot==0 && (profile==0 || profile==2)) || (slot==1 && (profile==1 || profile==4));
            int multiplier=count<=0?1:100*(protectedPerson?1:count);
            int chance=(int)(6*multiplier*(terrain?1.5:1.)*(double)0.01f);
            if(chance>0){state=state*0x6c078965u+0x3039u;if((state>>16)%100<(unsigned)chance)++expected;}
        }
        assert(execute(count,true)==expected && g_defenseSeed==state);++inputs;
    }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/ironwall_capture_coeffects_test.csv",ironwallCaptureFixtureActor,nullptr));
    for(int owned:{29,249}){
        prepare(0,3,54);*(int*)(g_personGuide+0xe8)=19;*(int*)(g_personTarget+0xe8)=owned;
        assert(execute(6,true)==2); // capture19 still caps the actual probability at100
    }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/ironwall_capture_edges_test.csv",ironwallCaptureFixtureActor,nullptr));
    g_captureRollOriginal=ironwallCaptureFixtureRoll;
    for(int fault=0;fault<8;++fault){prepare(0,0,4);ironwallCaptureFault=fault;execute(6,true);}
    prepare(0,0,4);execute(6,false); // a real candidate query without an owned defeat cannot authorize an effect
    skilltelemetry::close();
    prepare(0,0,4);auto state=g_defenseSeed;execute(6,true); // disabled telemetry delegates unchanged
    uint32_t expected=state;for(int n=0;n<2;++n)expected=expected*0x6c078965u+0x3039u;assert(g_defenseSeed==expected);
    g_skills[249]=oldCombination;vt[0x40/4]=oldOwner;g_captureRollOriginal=oldRoll;g_growthValid=oldValid;
    g_personRegistered=true;ironwallCaptureForce=3;*(uint32_t*)(g_criticalCells+20*401+4)=0;
    printf("PASS ironwall capture: %d complete native candidate/multiplier/RNG inputs, raw/composed/high/duplicate holders, seven stale controls, unscoped and disabled parity\n",inputs);
}
