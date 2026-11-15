// Called while the steering fixture's complete native M9 move/cave is alive.
// Native flags, neighbor geometry and complete ZOC unit test execute; world
// membership, hostility and controller spatial-list maintenance are adapters.
static uint8_t zocEnemy[0xf4];
static uint8_t zocBase[0x80],zocOtherBase[0x80];
static bool zocBaseRegistered=true;
static int zocBaseForce=3,zocBaseMutation=-1;
static uint32_t* zocBaseProxySlot=nullptr;
static uint32_t* zocBaseSecondarySlot=nullptr;
static uint8_t* zocBaseMapCell=nullptr;
static bool zocSourceRegistered=true,zocEnemyRegistered=true;
static int zocSourceForce=2,zocEnemyForce=3,zocMutation=0,zocRandomCalls=0;
static uint32_t* zocCommand=nullptr;
static void* __fastcall zocUnitLookup(void*,void*,int id){return id==17 && zocSourceRegistered?steeringUnit:id==18 && zocEnemyRegistered?zocEnemy:nullptr;}
static int __fastcall zocUnitId(void*,void*,void* p){return p==steeringUnit?17:p==zocEnemy?18:-1;}
static int __fastcall zocPersonId(void*,void*,void* p){return p==steeringPeople[0]?600:p==steeringPeople[1]?14162:p==steeringPeople[2]?1000:-1;}
static int __cdecl zocValid(void* p){
    assert(p!=(void*)1 && p!=zocBase+1 && p!=zocOtherBase); // no virtual validity call on an unverified proxy
    return p==steeringUnit?zocSourceRegistered:p==zocEnemy?zocEnemyRegistered:p==zocBase?zocBaseRegistered:p!=nullptr;
}
static int __fastcall zocOwner(void* p,void*){return p==steeringUnit?zocSourceForce:p==zocBase || p==zocOtherBase?zocBaseForce:zocEnemyForce;}
static const uint32_t* __fastcall zocPosition(uint8_t* p,void*){return (uint32_t*)(p+0x3c);}
static int __fastcall zocHostile(void* source,void*,void* other){assert(source==steeringUnit && (other==zocEnemy || other==zocBase));return zocSourceForce!=zocOwner(other,nullptr);}
static void* __fastcall zocBaseLookup(void*,void*,int id){return id==23 && zocBaseRegistered?zocBase:nullptr;}
static int __fastcall zocBaseId(void*,void*,void* p){return p==zocBase || p==zocBase+1?23:p==zocOtherBase?24:-1;}
static int __fastcall zocFind(uint8_t* unit,void*,int effect){
    for(int n=0;n<3;++n){int id=*(int*)(unit+0xc+4*n);auto p=steeringPerson(nullptr,nullptr,id);
        if(p && personHas(p,nullptr,effect))return id;}return -1;
}
static int __fastcall zocTechnology(void*,void*,int){return 0;}
static int __cdecl zocRandom(int){++zocRandomCalls;return 0;}
static void __fastcall zocRemove(void*,void*,int id){assert(id==17);++steeringCalls;
    if(zocMutation==1)skilltelemetry::invalidateContext();
    if(zocMutation==2)*(int*)(steeringPeople[0]+0xe8)=-1;
    if(zocMutation==3)*(int*)(steeringUnit+0xc)=-1;
    if(zocMutation==4)zocSourceForce=4;
    if(zocMutation==5)zocEnemyForce=4;
    if(zocMutation==6)++*(uint32_t*)(zocEnemy+0x3c);
    if(zocMutation==7)zocSourceRegistered=false;
    if(zocMutation==8)zocEnemyRegistered=false;
    if(zocMutation==9)zocCommand[3]^=1;
    if(zocMutation==16)*(int*)(zocEnemy+0x24)=1;
    if(zocMutation==17)++*(int*)(steeringUnit+0x48);
    if(zocMutation==18)*(int*)(steeringUnit+8)=1;
    if(zocBaseMutation==1)skilltelemetry::invalidateContext();
    if(zocBaseMutation==2)*(int*)(steeringPeople[0]+0xe8)=-1;
    if(zocBaseMutation==3)*(int*)(steeringUnit+0xc)=-1;
    if(zocBaseMutation==4)zocSourceForce=4;
    if(zocBaseMutation==5)zocBaseForce=4;
    if(zocBaseMutation==6)++*(uint32_t*)(zocBase+0x3c);
    if(zocBaseMutation==7)zocSourceRegistered=false;
    if(zocBaseMutation==8)zocBaseRegistered=false;
    if(zocBaseMutation==9)zocCommand[3]^=1;
    if(zocBaseMutation==11)*(int*)(zocBase+0x14)=0;
    if(zocBaseMutation==12)*zocBaseProxySlot=0;
    if(zocBaseMutation==13)*zocBaseProxySlot=(uint32_t)zocOtherBase;
    if(zocBaseMutation==14)*(uint32_t*)zocBaseMapCell=0;
    if(zocBaseMutation==15)*(uint16_t*)(zocBaseMapCell+8)=24;
    if(zocBaseMutation==16)++*(int*)(steeringUnit+0x48);
    if(zocBaseMutation==17)*(int*)(steeringUnit+8)=1;
    if(zocBaseMutation==18)--*(int*)(zocBase+0x14);
    if(zocBaseMutation==23)*zocBaseSecondarySlot=0;
}
static void zocTelemetryTests(uint8_t* code,uint8_t* controller){
    auto at=[&](uint32_t a){return code+a-0x400000;};
    auto stub=[&](uint32_t a,const void* fn){auto p=at(a);p[0]=0xe9;*(int32_t*)(p+1)=(int32_t)((const uint8_t*)fn-p-5);};
    auto oldUnit=g_movementUnitById;auto oldId=g_telemetryUnitId;auto oldPersonId=g_telemetryPersonId;auto oldValid=g_growthValid;
    auto oldFlags=g_zocFlagsOriginal;auto oldTest=g_zocTestOriginal;auto oldNeighbor=g_zocNeighborOriginal;auto oldRelation=g_zocRelationOriginal;auto oldHandle=g_zocHandleUnit;
    auto oldBase=g_baseById;auto oldBaseId=g_telemetryBaseId;auto oldObject=g_zocObjectFromCell;
    g_baseById=(BaseById)zocBaseLookup;g_telemetryBaseId=(BuildingId)zocBaseId;g_zocObjectFromCell=(ReachObject)at(0x483b20);
    std::vector<uint8_t> proxies(65535*0x80);uint32_t oldProxyOperand=*(uint32_t*)at(0x483b34);
    *(uint32_t*)at(0x483b34)=(uint32_t)proxies.data()+0x64;zocBaseProxySlot=(uint32_t*)(proxies.data()+500*0x80+0x64);
    g_movementUnitById=(MovementUnitById)zocUnitLookup;g_telemetryUnitId=(BuildingId)zocUnitId;
    g_telemetryPersonId=(BuildingId)zocPersonId;g_growthValid=zocValid;
    g_zocFlagsOriginal=(ZocFlags)at(0x5a3680);g_zocTestOriginal=(ZocTest)at(0x56e0e0);
    g_zocNeighborOriginal=(ZocNeighbor)at(0x483a50);g_zocRelationOriginal=(ZocRelation)zocHostile;g_zocHandleUnit=(AttackReachHandleUnit)at(0x56dba0);
    stub(0x490e70,zocUnitLookup);stub(0x56e4f0,zocRemove);stub(0x4721d0,zocRandom);stub(0x472150,zocRandom);
    *(uint32_t*)at(0x56dbab)=*(uint32_t*)at(0x56e165)=*(uint32_t*)at(0x56e192)=(uint32_t)steeringUnit;
    *(uint32_t*)at(0x5a5130)=(uint32_t)controller;
    *(uint32_t*)at(0x483a68)=(uint32_t)at(0x79c310);*(uint32_t*)at(0x483a70)=(uint32_t)at(0x79c312);
    *(uint32_t*)at(0x47ec90)=(uint32_t)at(0x79bffc);
    void* vt[25]={};vt[0x3c/4]=(void*)zocPosition;vt[0x40/4]=(void*)zocOwner;
    vt[0x58/4]=(void*)at(0x47a8a0);vt[0x5c/4]=(void*)zocFind;vt[0x60/4]=(void*)zocTechnology;
    *(void**)steeringUnit=*(void**)zocEnemy=*(void**)zocBase=*(void**)zocOtherBase=vt;
    // Execute the original bounded world-record accessor independently of the
    // world lookup adapter. Returned record addresses are compared, not read.
    auto nativeBase=(BaseById)at(0x490d00);uint32_t recordWorld=(uint32_t)zocBase-0x89730-23*0x38;
    for(int id:{-1,0,23,16383,16384,65535}){
        void* result=nativeBase((void*)(uintptr_t)recordWorld,id);
        assert(result==(id>=0 && id<=16383?(void*)(uintptr_t)(recordWorld+0x89730+id*0x38):nullptr));
    }
    auto nativeBaseId=(BuildingId)at(0x491770);
    assert(nativeBaseId((void*)(uintptr_t)recordWorld,zocBase)==23 && nativeBaseId((void*)(uintptr_t)recordWorld,nullptr)==-1 &&
        nativeBaseId((void*)(uintptr_t)recordWorld,(void*)1)==-1);
    int unaligned=nativeBaseId((void*)(uintptr_t)recordWorld,zocBase+1);
    assert(unaligned==23 && nativeBase((void*)(uintptr_t)recordWorld,unaligned)!=zocBase+1);
    uint8_t proxyCell[20]={};*(uint16_t*)(proxyCell+8)=500;*zocBaseProxySlot=(uint32_t)zocBase;
    for(int kind=0;kind<4;++kind){*(uint32_t*)proxyCell=kind;uint8_t before[20];memcpy(before,proxyCell,sizeof before);
        assert(g_zocObjectFromCell(proxyCell)==(kind==2?zocBase:nullptr) && !memcmp(before,proxyCell,sizeof before));}
    bool saved[3]={g_skills[249].effects[0],g_skills[249].effects[1],g_skills[249].effects[4]};
    uint32_t command[44];zocCommand=command;
    int width=0,height=0,x=0,y=0;std::vector<uint8_t> map;
    auto layout=[&](int w,int h,int row,int col){width=w;height=h;x=row;y=col;map.assign((size_t)w*h*20,0);
        *(int*)at(0x5a0783)=*(int*)at(0x4963a0)=height;
        *(int*)at(0x5a078f)=*(int*)at(0x5a079a)=*(int*)at(0x4963ac)=width;*(uint32_t*)at(0x5a07aa)=(uint32_t)map.data();
        for(const auto& m:g_zocImmediates)if(m.kind<3)*(uint32_t*)at(m.at)=m.kind==0?width:m.kind==1?height:(uint32_t)map.data();
    };
    auto prepare=[&](int effect,int provider,int terrain,int weapon,int blocker,int steps=3){
        steeringFault=0;zocMutation=0;zocSourceForce=2;zocEnemyForce=blocker==2?2:3;zocSourceRegistered=zocEnemyRegistered=true;
        *(int*)(zocEnemy+0x24)=blocker==3?1:0;
        *(int*)(steeringUnit+8)=0;*(int*)(steeringUnit+0x48)=weapon;*(int*)(steeringUnit+0xa8)=terrain==7 || terrain==8?9:weapon;
        *(int*)(steeringUnit+0xc)=600;*(int*)(steeringUnit+0x10)=14162;*(int*)(steeringUnit+0x14)=-1;
        *(int*)(steeringPeople[0]+0xe8)=provider==0?effect:provider==2 || provider==4?249:-1;
        *(int*)(steeringPeople[1]+0xe8)=provider==1 || provider==4?effect:provider==3?249:-1;
        *(int*)(steeringPeople[2]+0xe8)=-1;
        for(int k:{0,1,4})g_skills[249].effects[k]=k==effect;
        memset(command,0,sizeof command);command[0]=17;command[1]=x|(y<<16);command[2]=x|((y+steps)<<16);command[43]=steps;
        *(uint32_t*)(steeringUnit+0x3c)=command[1];
        for(int n=0;n<steps;++n){command[3+n]=x|((y+n+1)<<16);auto cell=map.data()+20*((size_t)x*width+y+n+1);*(uint32_t*)cell=0;*(uint32_t*)(cell+4)=terrain;}
        uint32_t enemy=(x-1)|((y+1+(x&1))<<16);*(uint32_t*)(zocEnemy+0x3c)=enemy;
        auto cell=map.data()+20*((size_t)(x-1)*width+y+1+(x&1));*(uint32_t*)cell=blocker==0?0:blocker==5?2:1;
        *(uint16_t*)(cell+8)=blocker==4?9999:18;
    };
    int inputs=0;assert(skilltelemetry::open(L"../build/zoc_route_test.csv",steeringActor,nullptr));
    for(int l=0;l<2;++l){layout(l?249:200,l?530:200,l?525:194,l?243:194);
        for(int effect:{0,1,4})for(int provider=0;provider<6;++provider)for(int terrain:{0,7,8})for(int weapon:{4,6})for(int blocker=0;blocker<6;++blocker){++inputs;
            prepare(effect,provider,terrain,weapon,blocker);
            uint8_t expected[0xf4],enemyBefore[0xf4],peopleBefore[sizeof steeringPeople];memcpy(expected,steeringUnit,sizeof expected);
            memcpy(enemyBefore,zocEnemy,sizeof zocEnemy);memcpy(peopleBefore,steeringPeople,sizeof steeringPeople);*(uint32_t*)(expected+0x3c)=command[2];
            int calls=steeringCalls;runSteering(controller,command,17,command[2],1);
            assert(steeringCalls==calls+1 && !memcmp(expected,steeringUnit,sizeof expected) && !memcmp(enemyBefore,zocEnemy,sizeof zocEnemy) &&
                !memcmp(peopleBefore,steeringPeople,sizeof steeringPeople) && !g_zocFlagProbe && !g_zocEnemyProbe && !zocRandomCalls);
            runSteering(controller,command,17,command[2],1); // already-arrived repetition must not add telemetry
        }
    }
    assert(inputs==1296);skilltelemetry::close();
    puts("PASS ZOC: 1296 native flags/neighbor/control-test and actual M9 position commits, 0/1/4 providers, terrain, natural siege immunity, enemy/ally/inactive/stale/object cases and two maps");
    layout(200,200,10,10);
    assert(skilltelemetry::open(L"../build/zoc_priority_test.csv",steeringActor,nullptr));
    for(int scenario=0;scenario<6;++scenario){prepare(1,0,0,4,1);
        *(uint32_t*)(map.data()+20*((size_t)x*width+y+2)+4)=7;
        if(scenario==0)*(int*)(steeringPeople[1]+0xe8)=0;
        if(scenario==1){*(int*)(steeringPeople[0]+0xe8)=249;for(int e:{0,1,4})g_skills[249].effects[e]=true;}
        if(scenario==2 || scenario==5)*(int*)(steeringPeople[1]+0xe8)=4;
        if(scenario==3){*(int*)(steeringPeople[0]+0xe8)=4;*(int*)(steeringPeople[1]+0xe8)=1;}
        if(scenario==4){*(int*)(steeringPeople[1]+0xe8)=0;*(int*)(steeringPeople[2]+0xe8)=4;*(int*)(steeringUnit+0x14)=1000;}
        if(scenario==5){*(int*)(steeringUnit+8)=1;*(int*)(steeringUnit+0x48)=6;*(int*)(steeringUnit+0xa8)=0;}
        runSteering(controller,command,17,command[2],1);
    }
    skilltelemetry::close();
    puts("PASS ZOC: native0-before1 priority across members, composed0/1/4, independent land/water providers, third-member4 and transport weapon-category rules");
    assert(skilltelemetry::open(L"../build/zoc_preview_test.csv",steeringActor,nullptr));
    for(int effect:{1,4}){prepare(effect,0,effect==1?0:7,4,1);auto before=map;
        uint8_t unitBefore[0xf4];memcpy(unitBefore,steeringUnit,sizeof unitBefore);uint32_t flags[4];
        {ZocFlagProbe probe;g_zocFlagsOriginal(flags,steeringUnit);}
        {ZocEnemyProbe probe(steeringUnit);assert(g_zocTestOriginal(controller+17*16,command[3])==1 && probe.enemy==zocEnemy);}
        assert(before==map && !memcmp(unitBefore,steeringUnit,sizeof unitBefore) && !zocRandomCalls);
    }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"../build/zoc_edges_test.csv",steeringActor,nullptr));
    for(int effect:{1,4})for(int mutation=0;mutation<19;++mutation){prepare(effect,0,effect==1?0:7,4,1,mutation==15?40:3);zocMutation=mutation;
        if(mutation==10)steeringFault=1;
        if(mutation==11){command[43]=1;command[2]=command[3];}
        if(mutation==12)*(uint32_t*)(map.data()+20*((size_t)x*width+y+1))=2;
        if(mutation==13)command[3]=command[5];if(mutation==14)command[43]=41;
        runSteering(controller,command,17,command[2],1);
    }
    skilltelemetry::close();zocMutation=0;prepare(1,0,0,4,1);int calls=steeringCalls;
    runSteering(controller,command,17,command[2],1);assert(steeringCalls==calls+1 && *(uint32_t*)(steeringUnit+0x3c)==command[2] && !zocRandomCalls);
    puts("PASS ZOC: preview-only queries, 34 stale/no-write/invalid-route exclusions, four controls including40 cells, disabled delegation and unchanged RNG");
    auto prepareBase=[&](int effect,int provider,int terrain,int weapon,int scenario,int steps=3){
        prepare(effect,provider,terrain,weapon,1,steps);zocBaseMutation=0;zocBaseRegistered=scenario!=6;zocBaseForce=scenario==1?2:3;
        *(int*)(zocBase+0x14)=scenario==2?0:1000;
        uint32_t position=(x-1)|((y+1+(x&1))<<16);*(uint32_t*)(zocBase+0x3c)=position;
        zocBaseMapCell=map.data()+20*((size_t)(x-1)*width+y+1+(x&1));*(uint32_t*)zocBaseMapCell=2;
        int object=scenario==8?65534:500;zocBaseProxySlot=(uint32_t*)(proxies.data()+object*0x80+0x64);
        *(uint16_t*)(zocBaseMapCell+8)=scenario==5?65535:object;
        *zocBaseProxySlot=scenario==3?0:scenario==4?(uint32_t)(zocBase+1):scenario==7?1:(uint32_t)zocBase;
        auto early=map.data()+20*((size_t)(x-1)*width+y+(x&1));
        auto late=map.data()+20*((size_t)(x+1)*width+y+1+(x&1));*(uint32_t*)early=*(uint32_t*)late=0;
        if(scenario==8 || scenario==9){auto cell=scenario==8?late:early;*(uint32_t*)cell=1;*(uint16_t*)(cell+8)=18;
            *(uint32_t*)(zocEnemy+0x3c)=(scenario==8?x+1:x-1)|((scenario==8?y+1+(x&1):y+(x&1))<<16);}
    };
    int baseInputs=0;assert(skilltelemetry::open(L"../build/zoc_base_test.csv",steeringActor,nullptr));
    for(int l=0;l<2;++l){layout(l?249:200,l?530:200,l?525:194,l?243:194);
        for(int effect:{0,1,4})for(int provider=0;provider<6;++provider)for(int terrain:{0,7,8})for(int weapon:{4,6})for(int scenario=0;scenario<10;++scenario){++baseInputs;
            prepareBase(effect,provider,terrain,weapon,scenario);
            uint8_t expected[0xf4],baseBefore[0x80],peopleBefore[sizeof steeringPeople];memcpy(expected,steeringUnit,sizeof expected);
            memcpy(baseBefore,zocBase,sizeof baseBefore);memcpy(peopleBefore,steeringPeople,sizeof peopleBefore);*(uint32_t*)(expected+0x3c)=command[2];
            int calls=steeringCalls;runSteering(controller,command,17,command[2],1);
            assert(steeringCalls==calls+1 && !memcmp(expected,steeringUnit,sizeof expected) && !memcmp(baseBefore,zocBase,sizeof baseBefore) &&
                !memcmp(peopleBefore,steeringPeople,sizeof peopleBefore) && !g_zocFlagProbe && !g_zocEnemyProbe && !zocRandomCalls);
            runSteering(controller,command,17,command[2],1);
        }
    }
    assert(baseInputs==2160);skilltelemetry::close();
    puts("PASS base ZOC:2160 native proxy/flags/ZOC/M9 move inputs, hostile/allied/inactive/null/wrong/unmapped/out-of-range/unregistered proxies and mixed unit/base priority");
    layout(200,200,10,10);assert(skilltelemetry::open(L"../build/zoc_base_preview_test.csv",steeringActor,nullptr));
    for(int effect:{1,4}){prepareBase(effect,0,effect==1?0:7,4,0);auto before=map;uint8_t unitBefore[0xf4],baseBefore[0x80];
        memcpy(unitBefore,steeringUnit,sizeof unitBefore);memcpy(baseBefore,zocBase,sizeof baseBefore);uint32_t flags[4];
        {ZocFlagProbe probe;g_zocFlagsOriginal(flags,steeringUnit);}
        {ZocEnemyProbe probe(steeringUnit);assert(g_zocTestOriginal(controller+17*16,command[3])==1 && probe.enemy==zocBase && probe.base);}
        assert(before==map && !memcmp(unitBefore,steeringUnit,sizeof unitBefore) && !memcmp(baseBefore,zocBase,sizeof baseBefore) && !zocRandomCalls);
    }
    skilltelemetry::close();assert(skilltelemetry::open(L"../build/zoc_base_edges_test.csv",steeringActor,nullptr));
    for(int effect:{1,4})for(int mutation=0;mutation<22;++mutation){prepareBase(effect,0,effect==1?0:7,4,0,mutation==21?40:3);zocBaseMutation=mutation;
        if(mutation==10)steeringFault=1;
        if(mutation==19){command[43]=1;command[2]=command[3];}if(mutation==20)command[3]=command[5];
        runSteering(controller,command,17,command[2],1);
    }
    skilltelemetry::close();prepareBase(1,0,0,4,0);int baseCalls=steeringCalls;runSteering(controller,command,17,command[2],1);
    assert(steeringCalls==baseCalls+1 && *(uint32_t*)(steeringUnit+0x3c)==command[2] && !zocRandomCalls);zocBaseMutation=-1;
    puts("PASS base ZOC:38 stale/proxy/map/no-write exclusions, six controls including nonzero state changes and40 cells, read-only previews and disabled parity");
    assert(skilltelemetry::open(L"../build/zoc_base_footprint_test.csv",steeringActor,nullptr));
    for(int effect:{1,4})for(int object:{500,65534})for(int mutation=0;mutation<4;++mutation){
        prepareBase(effect,0,effect==1?0:7,4,0);
        auto center=map.data()+20*((size_t)(x-2)*width+y+1+(x&1));*(uint32_t*)center=2;
        int other=object==500?65534:500;*(uint16_t*)(center+8)=other;
        *(uint32_t*)(zocBase+0x3c)=(x-2)|((y+1+(x&1))<<16);
        *(uint16_t*)(zocBaseMapCell+8)=object;zocBaseProxySlot=(uint32_t*)(proxies.data()+object*0x80+0x64);*zocBaseProxySlot=(uint32_t)zocBase;
        zocBaseSecondarySlot=(uint32_t*)(proxies.data()+other*0x80+0x64);*zocBaseSecondarySlot=(uint32_t)zocBase;
        zocBaseMutation=mutation==1?12:mutation==2?6:mutation==3?23:0;
        runSteering(controller,command,17,command[2],1);*(uint32_t*)center=0;
    }
    skilltelemetry::close();zocBaseMutation=-1;assert(!zocRandomCalls);
    puts("PASS base ZOC: object500/65534 ->building23, off-center footprint control, shared building proxies and selective proxy invalidation");
    for(int n=0;n<3;++n)g_skills[249].effects[n==2?4:n]=saved[n];
    g_movementUnitById=oldUnit;g_telemetryUnitId=oldId;g_telemetryPersonId=oldPersonId;g_growthValid=oldValid;
    g_zocFlagsOriginal=oldFlags;g_zocTestOriginal=oldTest;g_zocNeighborOriginal=oldNeighbor;g_zocRelationOriginal=oldRelation;g_zocHandleUnit=oldHandle;
    g_baseById=oldBase;g_telemetryBaseId=oldBaseId;g_zocObjectFromCell=oldObject;*(uint32_t*)at(0x483b34)=oldProxyOperand;
}
