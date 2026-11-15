static int g_reachOwnerBaseId=529;
static int __fastcall reachOwnerBaseId(void*,void*,void* base){return base==g_reachFixtureTarget?g_reachOwnerBaseId:-1;}
template<typename Prepare> static void reachOwnerTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,Prepare prepare){
    auto oldBaseId=g_telemetryBaseId;g_telemetryBaseId=(BuildingId)reachOwnerBaseId;
    uint32_t oldTerrainTable=*(uint32_t*)site(0x4839e7);*(uint32_t*)site(0x4839e7)=(uint32_t)site(0x79c1d8);
    int flammable=-1;for(int terrain=0;terrain<32;++terrain)if(*(int*)site(0x79c1d8+4*terrain)){flammable=terrain;break;}
    assert(flammable>=0);
    auto setup=[&](int shape,int provider,int tech,int success){
        memset(actor+0x98,0,16);memset(other+0x98,0,16);g_skills[249].effects[65]=true;
        int distance=tech?4:2;prepare(provider,0,tech,distance,100,success);
        g_reachFixtureProvider=(provider==1 || provider==3)?other:actor;
        g_reachFixtureBaseTarget=shape>=1 && shape<=4;
        auto cell=g_criticalCells+20*(201+distance);*(uint32_t*)(cell+4)=flammable;
        *(uint32_t*)cell=shape==0?0:g_reachFixtureBaseTarget?2:1;
        *(int*)(target+8)=g_reachFixtureBaseTarget?shape-1:0;
        g_reachOwnerBaseId=shape==1?529:shape==2?530:shape==3?531:575;
    };
    auto run=[&](int tech,int success,int energy=100,int mode=0){
        uint32_t command[3]={0,4095,((uint32_t)(tech?5:3)<<16)|1};source[0x1a]=(uint8_t)energy;
        assert(((StrategyCommand)site(0x5933a0))(nullptr,command,mode,nullptr)==success);
        assert(source[0x1a]==(mode?energy:energy<10?0:energy-10) && g_reachFixtureLeafCalls==1 && g_defenseSeed==0x12345678);
        assert(!g_reachCommand && !g_reachPermissionProbe && !g_strategyCommand);
    };
    assert(skilltelemetry::open(L"build/reach_owner_test.csv",afterFixtureActor,nullptr));
    for(int shape=0;shape<6;++shape)for(int provider=0;provider<4;++provider)for(int tech:{0,1})for(int success:{0,1}){
        setup(shape,provider,tech,success);run(tech,success);
    }
    for(int provider=0;provider<4;++provider)for(int fault=12;fault<=15;++fault){
        setup(5,provider,0,1);g_reachFixtureMutation=fault;run(0,1);
    }
    for(int provider:{2,3}){setup(5,provider,0,1);g_reachFixtureMutation=16;run(0,1);}
    for(int provider=0;provider<4;++provider)for(int outcome:{17,18}){
        setup(5,provider,0,1);g_reachFixtureMutation=outcome;run(0,1);
    }
    for(int shape=0;shape<6;++shape)for(int tech:{0,1})for(int mode:{0,1}){
        setup(shape,0,tech,1);run(tech,1,mode?100:0,mode);
    }
    skilltelemetry::close();
    for(int shape=0;shape<6;++shape){setup(shape,3,0,1);run(0,1);}
    g_reachFixtureBaseTarget=false;g_reachFixtureProvider=nullptr;g_telemetryBaseId=oldBaseId;
    *(uint32_t*)site(0x4839e7)=oldTerrainTable;
    printf("PASS reach ownership/ground:146 native context inputs; empty/city/gate/port/facility/unit targets, actual terrain%d permission, all four provider modes, affiliation expiry and committed-target outcome controls\n",flammable);
}
