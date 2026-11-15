// Execute the entire native4bf1f0 transfer body, including original currency
// clamp routines, person affiliation/current-location stores and unit reset.
// World/policy/stats/UI/list allocation and force/job bookkeeping are adapters.
static int entryNativeAccept=1,entryNativeJobs=0,entryNativeRemoved=0,entryNativeMessages=0;
static int entryNativeMutation=0;
static bool entryNativeReadyResult=true,entryNativeBaseValid=true,entryNativeInvalidateResolver=false;
static bool entryNativeActor(const void* person,skilltelemetry::Actor& actor){
    bool result=steeringActor(person,actor);
    if(entryNativeInvalidateResolver){entryNativeInvalidateResolver=false;skilltelemetry::invalidateContext();}
    return result;
}
static int __cdecl entryNativeValid(void* person){return person==steeringEntryBase?entryNativeBaseValid:person!=nullptr;}
static uint32_t entryNativeCookieValue=0x12345678;
struct EntryNativeNode {EntryNativeNode *next,*previous;void* person;};
static EntryNativeNode entryNativeNodes[32];static int entryNativeNodeCount=0;
static int __fastcall entryNativeZero(void*,void*){return 0;}
static int __fastcall entryNativeReady(void*,void*){return entryNativeReadyResult;}
static void __fastcall entryNativeVoid(void*,void*){}
static void __fastcall entryNativeVoid1(void*,void*,void*){}
static void __fastcall entryNativeVoid2(void*,void*,void*,int){}
static void __fastcall entryNativeVoid3(void*,void*,void*,int,int){}
static void* __fastcall entryNativeNoCorps(void*,void*,int){return nullptr;}
static int __fastcall entryNativePolicy(void*,void*){return entryNativeAccept;}
static int __fastcall entryNativeTroops(uint8_t* object,void*){return *(int*)(object+(object==steeringUnit?0xe0:0x60));}
static int __fastcall entryNativeGauge(uint8_t* object,void*){return *(int*)(object+(object==steeringUnit?0xe4:0x64));}
static int __fastcall entryNativeMix(void*,void*,void*,int,int,int,int){return 40;}
static void __fastcall entryNativeSetGauge(uint8_t* object,void*,int value){*(int*)(object+0x64)=value;}
static int __fastcall entryNativeGold(uint8_t* object,void*){return *(int*)(object+0x30);}
static int __fastcall entryNativeFood(uint8_t* object,void*){return *(int*)(object+0x34);}
static int __fastcall entryNativeCapacity(void*,void*){return 5000;}
static void __fastcall entryNativeSetGold(uint8_t* object,void*,int value){
    *(int*)(object+0x30)=value;
    if(entryNativeMutation==4)skilltelemetry::invalidateContext();
    if(entryNativeMutation==5)*(int*)(steeringPeople[0]+0xe8)=-1;
    if(entryNativeMutation==6)steeringForce=3;
    if(entryNativeMutation==7){uint8_t foreign[0xf4]={};steeringTransferCommitted(foreign,object);}
}
static void __fastcall entryNativeSetFood(uint8_t* object,void*,int value){*(int*)(object+0x34)=value;}
static int __fastcall entryNativeTroopGrant(void*,void*,uint8_t* object,int amount){*(int*)(object+0x60)+=amount;return amount;}
static int __fastcall entryNativeWeaponGrant(void*,void*,uint8_t* object,int weapon,int amount){assert(weapon==4 || weapon==10);*(int*)(object+0x70)+=amount;return amount;}
static int __fastcall entryNativeCity(void*,void*){return steeringEntryBaseId;}
// Native4955e0 has no stack arguments; the earlier47 belongs to4cf2f0.
static int __fastcall entryNativeHome(void*,void*){return 5;}
static int __fastcall entryNativeLeaderId(void*,void*){return 600;}
static void __fastcall entryNativeClear(uint8_t* list,void*){memset(list+4,0,0x1c);}
static void* __fastcall entryNativeAppend(uint8_t* list,void*,void* person){
    assert(entryNativeNodeCount<32);auto node=&entryNativeNodes[entryNativeNodeCount++];node->person=person;node->next=nullptr;
    node->previous=*(EntryNativeNode**)(list+8);if(node->previous)node->previous->next=node;else *(EntryNativeNode**)(list+4)=node;
    *(EntryNativeNode**)(list+8)=node;++*(int*)(list+12);return node;
}
static void** __fastcall entryNativeNext(void*,void*,EntryNativeNode** cursor){auto node=*cursor;assert(node);*cursor=node->next;return &node->person;}
static int __fastcall entryNativePeople(void*,void*,uint8_t* list,int place,int mask){
    assert(place==5 && mask==47);entryNativeClear(list,nullptr);
    for(auto& person:steeringPeople)entryNativeAppend(list,nullptr,person);return 3;
}
static void __fastcall entryNativeReturnJob(void*,void*,void* person,void* base,int flag,int zero){
    assert(!base && flag==-1 && zero==0 && *(int*)((uint8_t*)person+0x98)==5000);++entryNativeJobs;
}
static void __fastcall entryNativeJobEvent(void*,void*,void*,int,int,int,int,int,int){}
static void __fastcall entryNativeRemoveUnit(void*,void*,int id){assert(id==17);++entryNativeRemoved;}
static void __fastcall entryNativeVisual(void*,void*,void* unit){
    if(entryNativeMutation==8)steeringTransferCommitted(unit,steeringEntryBase);
    assert(unit==steeringUnit);((DefeatUnitReset)(steeringEntryCode+0x4960a0-0x400000))(unit,0);
}
static void* __fastcall entryNativeText(void* self,void*,int){return self;}
static void* __fastcall entryNativeTextArg(void* self,void*,int,void*){return self;}
static const char* __cdecl entryNativeString(void*){return "fixture";}
static int __cdecl entryNativeMessage(const char*,void*,int){++entryNativeMessages;return 1;}
static int __stdcall entryNativeReadable(const void*,unsigned){return 0;}
static int(__stdcall* entryNativeReadablePointer)(const void*,unsigned)=entryNativeReadable;
__declspec(naked) static void entryNativeCookie(){__asm {ret}}
template<class Prepare>
static void entryNativeTransferTests(uint8_t* code,void* controller,Prepare prepare){
    auto at=[&](uint32_t a){return code+a-0x400000;};
    auto stub=[&](uint32_t a,const void* fn){auto p=at(a);p[0]=0xe9;*(int32_t*)(p+1)=(int32_t)((const uint8_t*)fn-p-5);};
    auto oldTransfer=g_steeringTransferOriginal;g_steeringTransferOriginal=(SteeringTransfer)at(0x4bf1f0);
    auto oldReset=g_defeatUnitResetResume;g_defeatUnitResetResume=(uint32_t)at(0x4960a7);
    auto oldCommitted=g_steeringTransferCommittedResume;g_steeringTransferCommittedResume=(uint32_t)at(0x4bf63c);
    uint8_t oldPersonTail=*at(0x4bf61f);*at(0x4bf61f)=0x6a;
    void* unitVt[25]={};void* baseVt[25]={};auto oldUnitVt=*(void**)steeringUnit,oldBaseVt=*(void**)steeringEntryBase;
    unitVt[0x40/4]=steeringOwner;baseVt[0x40/4]=steeringEntryOwner;
    unitVt[0x44/4]=baseVt[0x44/4]=entryNativeZero;
    unitVt[0x50/4]=baseVt[0x50/4]=entryNativeTroops;unitVt[0x54/4]=baseVt[0x54/4]=entryNativeGauge;
    *(void**)steeringUnit=unitVt;*(void**)steeringEntryBase=baseVt;
    uint32_t addresses[]={0x487e10,0x490ad0,0x47a6d0,0x495300,0x4b9840,0x4875c0,0x486c80,0x486df0,0x486d30,0x486ea0,0x487310,0x4873f0,0x4ae3c0,0x4ae430,0x4ad3d0,0x486680,0x4955e0,0x4cf2f0,0x47be50,0x47c100,0x47c1b0,0x47bed0,0x4b9480,0x4a32f0,0x4a7990,0x4a7410,0x4b0560,0x588cb0,0x4be2a0,0x4a0ec0,0x486890,0x490b00,0x423230,0x49cb60,0x49cb80,0x49b390,0x4f6690,0x707b1a,0x4a06a0};
    uint8_t saved[39][5];static_assert(sizeof addresses/sizeof *addresses==39);
    for(int n=0;n<39;++n)memcpy(saved[n],at(addresses[n]),5);
    uint8_t oldNativeUnitId[5];memcpy(oldNativeUnitId,at(0x4917c0),5);stub(0x4917c0,steeringUnitId);
    uint8_t oldNativeBaseId[5];memcpy(oldNativeBaseId,at(0x491770),5);stub(0x491770,steeringEntryBaseIndex);
    uint8_t oldNativeValid[5];memcpy(oldNativeValid,at(0x47a630),5);stub(0x47a630,entryNativeValid);
    stub(0x487e10,entryNativeReady);stub(0x490ad0,entryNativeNoCorps);stub(0x47a6d0,entryNativeZero);stub(0x495300,entryNativePolicy);
    stub(0x4b9840,entryNativeMix);stub(0x4875c0,entryNativeSetGauge);
    stub(0x486c80,entryNativeGold);stub(0x486df0,entryNativeFood);stub(0x486d30,entryNativeCapacity);stub(0x486ea0,entryNativeCapacity);
    stub(0x487310,entryNativeSetGold);stub(0x4873f0,entryNativeSetFood);stub(0x4ae3c0,entryNativeTroopGrant);stub(0x4ae430,entryNativeWeaponGrant);
    stub(0x4ad3d0,entryNativeVoid3);stub(0x486680,entryNativeCity);stub(0x4955e0,entryNativeHome);stub(0x4cf2f0,entryNativePeople);
    stub(0x47be50,entryNativeClear);stub(0x47c100,entryNativeClear);stub(0x47c1b0,entryNativeAppend);stub(0x47bed0,entryNativeNext);
    stub(0x4b9480,entryNativeVoid);stub(0x4a32f0,entryNativeVoid2);stub(0x4a7990,entryNativeReturnJob);stub(0x4a7410,entryNativeJobEvent);
    stub(0x4b0560,entryNativeRemoveUnit);stub(0x588cb0,entryNativeVisual);stub(0x4be2a0,entryNativeVoid2);stub(0x4a0ec0,entryNativeVoid);
    stub(0x486890,entryNativeLeaderId);stub(0x490b00,steeringPerson);stub(0x423230,entryNativeVoid1);
    stub(0x49cb60,entryNativeText);stub(0x49cb80,entryNativeTextArg);stub(0x49b390,entryNativeString);stub(0x4f6690,entryNativeMessage);
    stub(0x707b1a,entryNativeCookie);stub(0x4a06a0,entryNativeVoid);
    uint32_t operands[]={0x4bf20c,0x4960d9,0x4960e2,0x472544,0x472551};uint32_t oldOperands[5];
    for(int n=0;n<5;++n)oldOperands[n]=*(uint32_t*)at(operands[n]);
    *(uint32_t*)at(operands[0])=(uint32_t)&entryNativeCookieValue;
    *(uint32_t*)at(operands[1])=*(uint32_t*)at(operands[2])=(uint32_t)at(0x79c1d0);
    *(uint32_t*)at(operands[3])=*(uint32_t*)at(operands[4])=(uint32_t)&entryNativeReadablePointer;
    bool oldEffects[3]={g_skills[249].effects[0],g_skills[249].effects[1],g_skills[249].effects[4]};for(int e:{0,1,4})g_skills[249].effects[e]=false;
    std::vector<uint8_t> map(200*200*20);*(int*)at(0x5a0783)=*(int*)at(0x5a078f)=*(int*)at(0x5a079a)=*(int*)at(0x59f61d)=200;
    *(uint32_t*)at(0x5a07aa)=*(uint32_t*)at(0x59f629)=(uint32_t)map.data();
    uint32_t command[44]={};steeringCommand=command;command[0]=17;command[1]=0xa000a;command[2]=0xd000a;command[43]=3;
    for(int n=0;n<3;++n){command[3+n]=10|((11+n)<<16);*(uint32_t*)(map.data()+20*(2011+n)+4)=n==0?7:n==1?0:8;}
    *(uint32_t*)(map.data()+20*2013)=2;*(uint16_t*)(map.data()+20*2013+8)=500;
    auto setup=[&](int base,int mode,int cap,int provider){
        prepare(provider,mode!=0);*(uint32_t*)(steeringUnit+0x3c)=command[1];steeringEntryBaseId=base;*(int*)(steeringEntryBase+8)=0;
        for(auto& person:steeringPeople){*(int*)(person+0x9c)=5;*(int*)(person+0xa0)=3;}
        for(int slot=0;slot<12;++slot){*(int*)(steeringUnit+0x48+slot*8)=-1;*(int*)(steeringUnit+0x4c+slot*8)=0;}
        *(int*)(steeringUnit+0x48)=4;*(int*)(steeringUnit+0x4c)=10;*(int*)(steeringUnit+0x50)=10;*(int*)(steeringUnit+0x54)=5;
        *(int*)(steeringUnit+0x1c)=100;*(int*)(steeringUnit+0x20)=200;*(int*)(steeringUnit+0xe0)=100;*(int*)(steeringUnit+0xe4)=60;
        *(int*)(steeringEntryBase+0x30)=*(int*)(steeringEntryBase+0x34)=cap;*(int*)(steeringEntryBase+0x60)=1000;*(int*)(steeringEntryBase+0x64)=30;*(int*)(steeringEntryBase+0x70)=0;
        memset(steeringEntryMeta,0,sizeof steeringEntryMeta);entryNativeNodeCount=entryNativeJobs=entryNativeRemoved=entryNativeMessages=0;entryNativeAccept=mode==2?0:1;
        entryNativeMutation=0;entryNativeReadyResult=entryNativeBaseValid=true;entryNativeInvalidateResolver=false;
    };
    int inputs=0;assert(skilltelemetry::open(L"../build/entry_native_transfer_test.csv",entryNativeActor,nullptr));
    for(int base:{23,529})for(int mode=0;mode<3;++mode)for(int cap:{1000,4990,5000})for(int provider=0;provider<6;++provider){
        setup(base,mode,cap,provider);
        runSteeringEntry(command,17,command[2]);
        assert(*(int*)(steeringEntryBase+0x30)==(cap+100>5000?5000:cap+100));
        assert(*(int*)(steeringEntryBase+0x34)==(cap+200>5000?5000:cap+200));
        assert(*(int*)(steeringEntryBase+0x60)==1100 && *(int*)(steeringEntryBase+0x70)==15 && entryNativeRemoved==1);
        assert(entryNativeJobs==(mode==2?3:0) && entryNativeMessages==(mode==2?0:1));
        for(auto& person:steeringPeople){
            if(*(int*)(person+0x9c)!=base || *(int*)(person+0x98)!=(mode==2?5000:base))fprintf(stderr,"transfer state: base=%d mode=%d cap=%d provider=%d home=%d location=%d stores=%d jobs=%d\n",base,mode,cap,provider,*(int*)(person+0x9c),*(int*)(person+0x98),steeringEntryStores,entryNativeJobs);
            assert(*(int*)(person+0x9c)==base && *(int*)(person+0x98)==(mode==2?5000:base));
        }
        assert(defeatUnitCleared(steeringUnit) && !g_steeringEntry && !g_steeringTransfer);++inputs;
    }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"../build/entry_native_transfer_edges_test.csv",entryNativeActor,nullptr));
    for(int mutation=0;mutation<10;++mutation){
        setup(23,0,1000,0);entryNativeMutation=mutation;
        if(mutation==1)steeringEntryFault=1;if(mutation==2)entryNativeReadyResult=false;
        if(mutation==3)entryNativeBaseValid=false;if(mutation==9)entryNativeInvalidateResolver=true;
        runSteeringEntry(command,17,command[2]);
        bool completed=mutation!=2 && mutation!=3;
        assert(entryNativeRemoved==(completed?1:0));
        assert(*(int*)(steeringEntryBase+0x30)==(completed?1100:1000));
        assert(!g_steeringEntry && !g_steeringTransfer);
    }
    skilltelemetry::close();zocEntryAppliedRows("../build/entry_native_transfer_edges_test.csv",8);
    // Join the real flags/ZOC decision to the complete transfer body, rather
    // than reaching the completion bridge through a controlled transfer.
    auto oldUnit=g_movementUnitById;auto oldId=g_telemetryUnitId;auto oldPersonId=g_telemetryPersonId;
    auto oldBase=g_baseById;auto oldBaseId=g_telemetryBaseId;auto oldObject=g_zocObjectFromCell;
    auto oldFlags=g_zocFlagsOriginal;auto oldTest=g_zocTestOriginal;auto oldNeighbor=g_zocNeighborOriginal;
    auto oldRelation=g_zocRelationOriginal;auto oldHandle=g_zocHandleUnit;
    auto oldEnemyVt=*(void**)zocEnemy,oldBlockerVt=*(void**)zocBase;
    uint32_t zocAddresses[]={0x483b20,0x490e70,0x490d00};uint8_t zocSaved[3][5];
    for(int n=0;n<3;++n)memcpy(zocSaved[n],at(zocAddresses[n]),5);
    stub(0x483b20,zocEntryObject);stub(0x490e70,zocUnitLookup);stub(0x490d00,zocEntryBaseLookup);
    stub(0x4917c0,zocUnitId);stub(0x491770,zocEntryBaseIndex);
    g_movementUnitById=(MovementUnitById)zocUnitLookup;g_telemetryUnitId=(BuildingId)zocUnitId;g_telemetryPersonId=(BuildingId)zocPersonId;
    g_baseById=(BaseById)zocEntryBaseLookup;g_telemetryBaseId=(BuildingId)zocEntryBaseIndex;g_zocObjectFromCell=(ReachObject)zocEntryObject;
    g_zocFlagsOriginal=(ZocFlags)at(0x5a3680);g_zocTestOriginal=(ZocTest)at(0x56e0e0);g_zocNeighborOriginal=(ZocNeighbor)at(0x483a50);
    g_zocRelationOriginal=(ZocRelation)zocEntryHostile;g_zocHandleUnit=(AttackReachHandleUnit)at(0x56dba0);
    *(uint32_t*)at(0x56dbab)=*(uint32_t*)at(0x56e165)=*(uint32_t*)at(0x56e192)=(uint32_t)steeringUnit;
    *(uint32_t*)at(0x5a5130)=(uint32_t)controller;
    for(auto m:g_zocImmediates)if(m.kind<3)*(uint32_t*)at(m.at)=m.kind==2?(uint32_t)map.data():200;
    void* enemyVt[25]={};enemyVt[0x3c/4]=zocPosition;enemyVt[0x40/4]=zocEntryOwner;
    *(void**)zocEnemy=*(void**)zocBase=enemyVt;
    unitVt[0x3c/4]=zocPosition;unitVt[0x58/4]=at(0x47a8a0);unitVt[0x5c/4]=zocFind;unitVt[0x60/4]=zocTechnology;
    int zocInputs=0;assert(skilltelemetry::open(L"../build/entry_native_transfer_zoc_test.csv",entryNativeActor,nullptr));
    for(int base:{23,529})for(int effect:{0,1,4})for(int mode=0;mode<3;++mode)for(int provider=0;provider<4;++provider)for(int blocker=0;blocker<2;++blocker){
        setup(base,mode,1000,5);memset(map.data(),0,map.size());
        for(int n=0;n<3;++n)*(uint32_t*)(map.data()+20*(2011+n)+4)=effect==4?7:0;
        *(uint32_t*)(map.data()+20*2013)=2;*(uint16_t*)(map.data()+20*2013+8)=500;
        *(uint32_t*)(map.data()+20*1811)=blocker?2:1;*(uint16_t*)(map.data()+20*1811+8)=blocker?501:18;
        zocSourceForce=2;zocEnemyForce=zocBaseForce=3;zocSourceRegistered=zocEnemyRegistered=zocBaseRegistered=true;
        *(uint32_t*)(zocEnemy+0x3c)=*(uint32_t*)(zocBase+0x3c)=9|(11<<16);
        *(int*)(zocEnemy+0x24)=0;*(int*)(zocBase+0x14)=1;
        *(int*)(steeringUnit+0xa8)=effect==4?9:4;
        *(int*)(steeringPeople[provider==1?1:0]+0xe8)=provider==2?249:effect;
        if(provider==3)*(int*)(steeringPeople[1]+0xe8)=effect;
        for(int e:{0,1,4})g_skills[249].effects[e]=e==effect;
        runSteeringEntry(command,17,command[2]);
        assert(entryNativeRemoved==1 && defeatUnitCleared(steeringUnit) && entryNativeJobs==(mode==2?3:0));
        assert(*(int*)(steeringEntryBase+0x30)==1100 && *(int*)(steeringEntryBase+0x34)==1200);
        for(auto& person:steeringPeople)assert(*(int*)(person+0x9c)==base && *(int*)(person+0x98)==(mode==2?5000:base));
        assert(!g_steeringEntry && !g_steeringTransfer && !g_zocFlagProbe && !g_zocEnemyProbe && !zocRandomCalls);++zocInputs;
    }
    skilltelemetry::close();zocEntryAppliedRows("../build/entry_native_transfer_zoc_test.csv",288);
    for(int n=0;n<3;++n)memcpy(at(zocAddresses[n]),zocSaved[n],5);
    g_movementUnitById=oldUnit;g_telemetryUnitId=oldId;g_telemetryPersonId=oldPersonId;
    g_baseById=oldBase;g_telemetryBaseId=oldBaseId;g_zocObjectFromCell=oldObject;
    g_zocFlagsOriginal=oldFlags;g_zocTestOriginal=oldTest;g_zocNeighborOriginal=oldNeighbor;g_zocRelationOriginal=oldRelation;g_zocHandleUnit=oldHandle;
    *(void**)zocEnemy=oldEnemyVt;*(void**)zocBase=oldBlockerVt;
    for(int n=0;n<39;++n)memcpy(at(addresses[n]),saved[n],5);
    memcpy(at(0x4917c0),oldNativeUnitId,5);
    memcpy(at(0x491770),oldNativeBaseId,5);
    memcpy(at(0x47a630),oldNativeValid,5);
    for(int n=0;n<5;++n)*(uint32_t*)at(operands[n])=oldOperands[n];
    for(int n=0;n<3;++n)g_skills[249].effects[n==2?4:n]=oldEffects[n];
    *at(0x4bf61f)=oldPersonTail;g_defeatUnitResetResume=oldReset;g_steeringTransferOriginal=oldTransfer;
    g_steeringTransferCommittedResume=oldCommitted;
    *(void**)steeringUnit=oldUnitVt;*(void**)steeringEntryBase=oldBaseVt;
    printf("PASS native transfer body: %d combat/transport accept/return inputs, original gold/food clamps, native person/home/acted stores and native unit reset\n",inputs);
    zocEntryAppliedRows("../build/entry_native_transfer_test.csv",144);
    printf("PASS native ZOC transfer: %d complete native flags/unit-base control/transfer/reset inputs, three policies, raw/high/composed/duplicate holders and city529\n",zocInputs);
}
