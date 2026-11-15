// Execute the native592440 consumer,5b1190 builder and resource setters.
// World lookup, visibility and UI/destruction leaves are fixture adapters.
static int fireConsumerFault=0;
static void* __fastcall fireConsumerObject(void*,void*){return g_criticalTarget;}
static int __cdecl fireConsumerUnitKind(){return 11;}
static int __cdecl fireConsumerNotExplosive(void*){return 1;}
static int __fastcall fireConsumerFalse(void*,void*){return 0;}
static void __fastcall fireConsumerRefresh(void*,void*){}
static void __cdecl fireConsumerBaseRefresh(void*){}
static void __cdecl fireConsumerDestroyBase(void*,void*,int){}
static void __fastcall fireConsumerDestroyUnit(void*,void*,void*,void*,int,int){}
static int __fastcall fireConsumerNoStore(void*,void*,void*,int){return 0;}
static int __fastcall fireConsumerStaleStore(void* self,void*,void* target,int amount){
    int result=((IncomeGrant)site(0x4ae4a0))(self,target,amount);
    skilltelemetry::invalidateContext();return result;
}
static int __cdecl fireConsumerVisible(uint32_t){
    if(fireConsumerFault==1)skilltelemetry::invalidateContext();
    if(fireConsumerFault==2)*(int*)(g_personTarget+0xe8)=-1;
    if(fireConsumerFault==3)g_criticalTargetRegistered=false;
    if(fireConsumerFault==4)++*(uint32_t*)(g_criticalTarget+4);
    if(fireConsumerFault==5 && g_trapOwner && g_trapOwner->count)++*(int*)(g_trapOwner->packets[0].packet+0xc);
    if(fireConsumerFault==6)++*(int*)(g_personTarget+0x98);
    if(fireConsumerFault==9){uint8_t preview[0x80];
        ((TrapBuild)site(0x5b15a0))(preview,16,g_criticalSource,g_criticalTarget,nullptr);}
    return 1;
}
template<class Setup> static void fireConsumerTelemetryTests(Setup setup,uint8_t* source,uint8_t* target){
    std::vector<uint8_t> tiles(20*200*3);uint32_t clock=0x10001;
    *(uint32_t*)site(0x592486)=(uint32_t)tiles.data();*(int*)site(0x59246e)=200;
    for(uint32_t at:{0x5b11bdu,0x5b11c6u})*(uint32_t*)site(at)=(uint32_t)&clock;
    g_trapFireResume=(uint32_t)site(0x592447);g_trapFireUnitResume=(uint32_t)site(0x59253d);g_trapFireHpResume=(uint32_t)site(0x59263b);
    domesticStub(0x483b40,fireConsumerObject);domesticStub(0x5afbe0,fireConsumerNotExplosive);
    domesticStub(0x5a06c0,fireConsumerVisible);domesticStub(0x468e60,fireConsumerUnitKind);
    domesticStub(0x4954c0,fireConsumerRefresh);domesticStub(0x487a30,fireConsumerFalse);domesticStub(0x487ab0,fireConsumerFalse);
    domesticStub(0x487e10,fireConsumerFalse);domesticStub(0x591150,fireConsumerDestroyBase);
    domesticStub(0x4bfa30,fireConsumerDestroyUnit);domesticStub(0x59fdd0,fireConsumerBaseRefresh);
    auto run=[&](bool neutral){uint32_t packet[]={0x10002,neutral?0u:(uint32_t)source,2,0};
        ((TrapFire)site(0x592440))(nullptr,packet);assert(!g_trapOwner && !g_trapCalculation);};
    assert(skilltelemetry::open(L"build/fire_consumer_test.csv",trapFixtureActor,nullptr));int inputs=0;
    for(bool base:{false,true})for(int profile=0;profile<8;++profile)for(int before:{100,10000}){
        setup(0,16,profile,base,before);if(base)*(int16_t*)(target+0x10)=before==100?50:1000;
        fireConsumerFault=0;int calls=trapFixtureRandomCalls;run(false);assert(trapFixtureRandomCalls==calls+1);
        int damage=317;if(profile==1 || profile==5 || profile==6 || profile==7)damage*=2;
        if(profile==2 || profile==5 || profile==7)damage*=2;else if(profile==3 || profile==6)damage=0;
        assert(base?*(int16_t*)(target+0x10)==(before==100?0:883):*(uint16_t*)(target+0x18)==(std::max)(0,before-damage));++inputs;
    }
    // A missing source must still allow genuine defender-only fire effects.
    for(int profile:{2,3}){setup(0,16,profile,false,1000);fireConsumerFault=0;run(true);++inputs;}
    skilltelemetry::close();assert(skilltelemetry::open(L"build/fire_consumer_edges_test.csv",trapFixtureActor,nullptr));
    for(int fault=0;fault<10;++fault){setup(0,16,2,false,1000);fireConsumerFault=fault;
        g_unitTroopsOriginal=fault==7?(IncomeGrant)fireConsumerNoStore:fault==8?(IncomeGrant)fireConsumerStaleStore:(IncomeGrant)site(0x4ae4a0);
        int calls=trapFixtureRandomCalls;run(false);assert(trapFixtureRandomCalls==calls+(fault==9?2:1));
        assert(*(uint16_t*)(target+0x18)==(fault==7?1000:fault==5?365:366));
    }
    skilltelemetry::close();g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    setup(0,16,5,false,1000);fireConsumerFault=0;run(false);assert(*(uint16_t*)(target+0x18)==0);
    printf("PASS fire consumer: %d native root/builder/setter inputs; unit/base caps, raw/composed/high/coeffects, neutral fire, stale/no-store/preview and disabled controls\n",inputs);
}
