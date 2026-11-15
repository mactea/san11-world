// Reuse the real base-entry/member-store path with native ZOC flags, neighbor
// geometry, unit/base control-test and relation hooks. World tables are fixtures.
static int zocEntryMutation=0;
static uint8_t* zocEntryBlockerCell=nullptr;
static bool zocEntryBaseBlocker=false;
static void zocEntryAppliedRows(const char* path,int expected){
    FILE* file=nullptr;assert(!fopen_s(&file,path,"rb") && file);int applied=0;char line[4096];
    while(fgets(line,sizeof line,file)){assert(strchr(line,'\n'));if(strstr(line,",\"applied\","))++applied;}
    fclose(file);assert(applied==expected);
}
static void* __fastcall zocEntryBaseLookup(void*,void*,int id){
    return id==steeringEntryBaseId && steeringEntryRegistered?steeringEntryBase:id==24 && zocBaseRegistered?zocBase:nullptr;
}
static int __fastcall zocEntryBaseIndex(void*,void*,void* p){return p==steeringEntryBase?steeringEntryBaseId:p==zocBase?24:-1;}
static void* __fastcall zocEntryObject(uint8_t* cell,void*){
    if((*(uint32_t*)cell&3)!=2)return nullptr;
    int id=*(uint16_t*)(cell+8);return id==500?steeringEntryBase:id==501?zocBase:nullptr;
}
static int __fastcall zocEntryOwner(void* p,void*){return p==steeringUnit?zocSourceForce:p==steeringEntryBase?steeringEntryForce:p==zocBase?zocBaseForce:zocEnemyForce;}
static int __fastcall zocEntryHostile(void* source,void*,void* other){assert(source==steeringUnit);return zocSourceForce!=zocEntryOwner(other,nullptr);}
static void zocEntryChanged(){
    if(zocEntryMutation==1)skilltelemetry::invalidateContext();
    if(zocEntryMutation==2)*(int*)(steeringPeople[0]+0xe8)=-1;
    if(zocEntryMutation==3)zocSourceForce=4;
    if(zocEntryMutation==4)zocEnemyForce=zocBaseForce=4;
    if(zocEntryMutation==5)zocSourceRegistered=false;
    if(zocEntryMutation==6)zocEnemyRegistered=zocBaseRegistered=false;
    if(zocEntryMutation==7){*(uint32_t*)(zocEnemy+0x3c)^=1;*(uint32_t*)(zocBase+0x3c)^=1;}
    if(zocEntryMutation==8){*(int*)(zocEnemy+0x24)=1;*(int*)(zocBase+0x14)=0;}
    if(zocEntryMutation==9){if(zocEntryBaseBlocker)*(uint16_t*)(zocEntryBlockerCell+8)=502;else zocEnemyRegistered=false;}
    if(zocEntryMutation==10)*(int*)(steeringUnit+0x48)^=1;
    if(zocEntryMutation==11)*(int*)(steeringUnit+8)=1;
    if(zocEntryMutation==13)steeringCommand[3]^=1;
    if(zocEntryMutation==14)*(uint32_t*)(steeringUnit+0x3c)^=1;
}
template<class Prepare>
static void zocEntryTelemetryTests(uint8_t* code,void* controller,Prepare commonPrepare){
    auto at=[&](uint32_t a){return code+a-0x400000;};
    auto stub=[&](uint32_t a,const void* fn){auto p=at(a);p[0]=0xe9;*(int32_t*)(p+1)=(int32_t)((const uint8_t*)fn-p-5);};
    auto oldUnit=g_movementUnitById;auto oldId=g_telemetryUnitId;auto oldPersonId=g_telemetryPersonId;
    auto oldBase=g_baseById;auto oldBaseId=g_telemetryBaseId;auto oldObject=g_zocObjectFromCell;
    auto oldFlags=g_zocFlagsOriginal;auto oldTest=g_zocTestOriginal;auto oldNeighbor=g_zocNeighborOriginal;auto oldRelation=g_zocRelationOriginal;auto oldHandle=g_zocHandleUnit;
    auto oldVt=*(void**)steeringUnit;auto oldAfter=steeringEntryAfterStore;
    g_movementUnitById=(MovementUnitById)zocUnitLookup;g_telemetryUnitId=(BuildingId)zocUnitId;g_telemetryPersonId=(BuildingId)zocPersonId;
    g_baseById=(BaseById)zocEntryBaseLookup;g_telemetryBaseId=(BuildingId)zocEntryBaseIndex;g_zocObjectFromCell=(ReachObject)zocEntryObject;
    g_zocFlagsOriginal=(ZocFlags)at(0x5a3680);g_zocTestOriginal=(ZocTest)at(0x56e0e0);g_zocNeighborOriginal=(ZocNeighbor)at(0x483a50);
    g_zocRelationOriginal=(ZocRelation)zocEntryHostile;g_zocHandleUnit=(AttackReachHandleUnit)at(0x56dba0);steeringEntryAfterStore=zocEntryChanged;
    uint32_t addresses[]={0x483b20,0x490e70,0x490d00};uint8_t saved[3][5];for(int n=0;n<3;++n)memcpy(saved[n],at(addresses[n]),5);
    stub(0x483b20,zocEntryObject);stub(0x490e70,zocUnitLookup);stub(0x490d00,zocEntryBaseLookup);
    *(uint32_t*)at(0x56dbab)=*(uint32_t*)at(0x56e165)=*(uint32_t*)at(0x56e192)=(uint32_t)steeringUnit;*(uint32_t*)at(0x5a5130)=(uint32_t)controller;
    void* vt[25]={};vt[0x3c/4]=zocPosition;vt[0x40/4]=zocEntryOwner;vt[0x58/4]=at(0x47a8a0);vt[0x5c/4]=zocFind;vt[0x60/4]=zocTechnology;
    *(void**)steeringUnit=*(void**)zocEnemy=*(void**)zocBase=vt;
    bool oldEffects[3]={g_skills[249].effects[0],g_skills[249].effects[1],g_skills[249].effects[4]};
    std::vector<uint8_t> map;int width=200,height=200,x=10,y=10;uint32_t command[44];steeringCommand=command;
    auto layout=[&](int w,int h,int row,int col){width=w;height=h;x=row;y=col;map.assign((size_t)w*h*20,0);
        *(int*)at(0x5a0783)=height;*(int*)at(0x5a078f)=*(int*)at(0x5a079a)=*(int*)at(0x59f61d)=width;
        *(uint32_t*)at(0x5a07aa)=*(uint32_t*)at(0x59f629)=(uint32_t)map.data();
        for(auto m:g_zocImmediates)if(m.kind<3)*(uint32_t*)at(m.at)=m.kind==0?width:m.kind==1?height:(uint32_t)map.data();
    };
    auto prepare=[&](int effect,int provider,int terrain,bool base){
        memset(map.data(),0,map.size());memset(command,0,sizeof command);command[0]=17;command[1]=x|(y<<16);command[2]=x|((y+3)<<16);command[43]=3;
        for(int n=0;n<3;++n){command[3+n]=x|((y+n+1)<<16);*(uint32_t*)(map.data()+20*((size_t)x*width+y+n+1)+4)=terrain;}
        commonPrepare(5,0); // no raw5: these events require only0/1/4
        *(uint32_t*)(steeringUnit+0x3c)=command[1];
        steeringEntryBaseId=23;*(int*)(steeringEntryBase+8)=0;zocEntryMutation=0;zocSourceForce=2;zocEnemyForce=zocBaseForce=3;
        zocSourceRegistered=zocEnemyRegistered=zocBaseRegistered=true;zocEntryBaseBlocker=base;
        *(int*)(steeringUnit+8)=0;*(int*)(steeringUnit+0x48)=4;*(int*)(steeringUnit+0xa8)=terrain?9:4;
        *(int*)(steeringPeople[provider==1?1:0]+0xe8)=provider==2?249:effect;
        if(provider==3)*(int*)(steeringPeople[1]+0xe8)=effect;
        for(int e:{0,1,4})g_skills[249].effects[e]=e==effect;
        uint32_t enemy=(x-1)|((y+1+(x&1))<<16);*(uint32_t*)(zocEnemy+0x3c)=*(uint32_t*)(zocBase+0x3c)=enemy;
        *(int*)(zocEnemy+0x24)=0;*(int*)(zocBase+0x14)=1;
        zocEntryBlockerCell=map.data()+20*((size_t)(x-1)*width+y+1+(x&1));*(uint32_t*)zocEntryBlockerCell=base?2:1;*(uint16_t*)(zocEntryBlockerCell+8)=base?501:18;
        auto dest=map.data()+20*((size_t)x*width+y+3);*(uint32_t*)dest=2;*(uint16_t*)(dest+8)=500;
    };
    int inputs=0;assert(skilltelemetry::open(L"../build/zoc_entry_test.csv",steeringActor,nullptr));
    for(int l=0;l<2;++l){layout(l?249:200,l?530:200,l?525:10,l?243:10);
        for(int effect:{0,1,4})for(int provider=0;provider<4;++provider)for(int blocker=0;blocker<2;++blocker){
            prepare(effect,provider,effect==4?7:0,blocker!=0);runSteeringEntry(command,17,command[2]);
            for(auto& person:steeringPeople)assert(*(int*)(person+0x98)==23);
            assert(*(uint32_t*)(steeringUnit+0x3c)==command[1] && !zocRandomCalls);
            assert(!g_steeringEntry && !g_steeringTransfer && !g_zocFlagProbe && !g_zocEnemyProbe);++inputs;
        }
    }
    skilltelemetry::close();zocEntryAppliedRows("../build/zoc_entry_test.csv",96);layout(200,200,10,10);
    assert(skilltelemetry::open(L"../build/zoc_entry_edges_test.csv",steeringActor,nullptr));
    for(int effect:{1,4})for(int base=0;base<2;++base)for(int mutation=0;mutation<=18;++mutation){
        prepare(effect,0,effect==4?7:0,base!=0);zocEntryMutation=mutation;
        if(mutation==12)steeringEntryFault=1;if(mutation==15)g_steeringEntryOriginal=steeringEntryNoTransfer;
        if(mutation==17)steeringEntryFault=13;if(mutation==18)steeringEntryFault=20;
        if(mutation==16)runSteeringEntryPerson(nullptr,steeringPeople[0],23);else runSteeringEntry(command,17,command[2]);
        g_steeringEntryOriginal=(SteeringEntry)at(0x59f5f0);assert(!g_steeringEntry && !g_steeringTransfer);
    }
    skilltelemetry::close();zocEntryAppliedRows("../build/zoc_entry_edges_test.csv",24);
    assert(skilltelemetry::open(L"../build/zoc_entry_coeffects_test.csv",steeringActor,nullptr));
    for(int profile=0;profile<4;++profile){
        prepare(1,0,0,false);*(uint32_t*)(map.data()+20*((size_t)x*width+y+2)+4)=7;
        if(profile==0 || profile==2){*(int*)(steeringPeople[1]+0xe8)=4;*(int*)(steeringPeople[2]+0xe8)=5;}
        else {
            *(int*)(steeringPeople[0]+0xe8)=249;for(int e:{0,1,4})g_skills[249].effects[e]=true;
            *(int*)(steeringPeople[profile==3?2:1]+0xe8)=5;
            if(profile==3){*(int*)(steeringPeople[1]+0xe8)=249;steeringEntryFault=13;}
        }
        if(profile==2)zocEntryMutation=2;
        runSteeringEntry(command,17,command[2]);assert(!g_steeringEntry && !g_steeringTransfer);
    }
    skilltelemetry::close();zocEntryAppliedRows("../build/zoc_entry_coeffects_test.csv",11);
    auto oldCommitted=g_steeringTransferCommittedResume;g_steeringTransferCommittedResume=(uint32_t)(uintptr_t)steeringEntryMarkerReturned;
    assert(skilltelemetry::open(L"../build/entry_transfer_zoc_test.csv",steeringActor,nullptr));
    for(int effect:{1,4})for(int base=0;base<2;++base){
        prepare(effect,0,effect==4?7:0,base!=0);steeringEntryFault=1;steeringEntryCompletedMarkers=1;
        runSteeringEntry(command,17,command[2]);
    }
    for(int profile=0;profile<3;++profile){
        prepare(1,0,0,false);*(uint32_t*)(map.data()+20*((size_t)x*width+y+2)+4)=7;
        *(int*)(steeringPeople[1]+0xe8)=4;*(int*)(steeringPeople[2]+0xe8)=5;
        steeringEntryFault=1;steeringEntryCompletedMarkers=2;
        if(profile==1)zocEntryMutation=2;if(profile==2)zocEntryMutation=1;
        runSteeringEntry(command,17,command[2]);
    }
    skilltelemetry::close();zocEntryAppliedRows("../build/entry_transfer_zoc_test.csv",13);
    g_steeringTransferCommittedResume=oldCommitted;steeringEntryCompletedMarkers=0;
    prepare(1,0,0,false);runSteeringEntry(command,17,command[2]);assert(*(int*)(steeringPeople[0]+0x98)==23);
    for(int n=0;n<3;++n)memcpy(at(addresses[n]),saved[n],5);
    for(int n=0;n<3;++n)g_skills[249].effects[n==2?4:n]=oldEffects[n];
    g_movementUnitById=oldUnit;g_telemetryUnitId=oldId;g_telemetryPersonId=oldPersonId;
    g_baseById=oldBase;g_telemetryBaseId=oldBaseId;g_zocObjectFromCell=oldObject;
    g_zocFlagsOriginal=oldFlags;g_zocTestOriginal=oldTest;g_zocNeighborOriginal=oldNeighbor;g_zocRelationOriginal=oldRelation;g_zocHandleUnit=oldHandle;
    *(void**)steeringUnit=oldVt;steeringEntryAfterStore=oldAfter;
    assert(!zocRandomCalls);
    printf("PASS ZOC base entry: %d native flags/unit-base stop/entry/member-store inputs, raw/composed/high/duplicate providers and76 context controls\n",inputs);
}
