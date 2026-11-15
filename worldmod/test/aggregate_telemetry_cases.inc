// Run the real group copy, backwards aggregation, random coefficient,
// converter and troop store. Components use native critical/immunity/halving
// decisions; world lookup and damage before those decisions are fixtures.
static uintptr_t g_aggregateFixtureVineTail;
__declspec(naked) static void aggregateFixtureVine(uint8_t*,void*){__asm {push ebp
    push edi
    mov ebp,[esp+12]
    mov edi,[esp+16]
    jmp dword ptr [g_aggregateFixtureVineTail]}}
__declspec(naked) static void aggregateFixtureVineDone(){__asm {pop edi
    pop ebp
    ret}}
static int __cdecl aggregateFixtureProducer(uint8_t* packet,void* source,int tactic,uint32_t position,int a,int b,int c){
    int result=ambushFixtureProducer(packet,source,tactic,position,a,b,c);
    *(int*)(packet+8)=0;aggregateFixtureVine(packet,g_criticalTarget);return result;
}
template<class Setup> static void aggregateTelemetryTests(Setup& setup,uint8_t* source,uint8_t* primary,uint8_t* target,
        uint8_t* actor,uint8_t* person,uint8_t* spouse,float multiplier){
    uint8_t group[0x604]={};auto handler=(CriticalHandlerTwo)site(0x5860a0);
    g_aggregateFixtureVineTail=site(0x5b0404);domesticStub(0x5b0451,aggregateFixtureVineDone);
    memcpy((void*)site(0x5b0419),g_vineLayouts[0].bytes,g_vineLayouts[0].size);
    memcpy((void*)site(0x5b041f),g_vineLayouts[3].bytes,g_vineLayouts[3].size);
    auto oldProducer=g_criticalProducerOriginal;g_criticalProducerOriginal=aggregateFixtureProducer;
    auto prepare=[&](int count,int profile,int balance){
        setup(0,0,balance,125);memset(group,0,sizeof group);g_ambushNativeCritical=true;
        *(int*)(actor+0xe8)=*(int*)(spouse+0xe8)=23;actor[0xc9]=spouse[0xc9]=100;target[0xc9]=70;
        *(int*)(primary+0x10)=*(int*)(primary+0x14)=-1;*(int*)(primary+0xa8)=4;
        *(int*)(source+8)=*(int*)(primary+8)=*(int*)(target+8)=0;
        memset(person+0x98,0,16);int sum=0;
        for(int n=0;n<count;++n){auto packet=group+n*0x80;void* from=n%2?primary:source;
            g_defenseDamage=125+10*n;g_defenseSeed=0;
            *(int*)(person+0xe8)=profile==2 && n%2?27:profile==3?31:-1;
            if(profile==1 && n%2){
                *(int*)(packet+0xc)=g_defenseDamage;*(int*)(packet+0x64)=-1;
                *(uint32_t*)(packet+0x58)=*(uint32_t*)((uint8_t*)from+4);*(uint32_t*)(packet+0x5c)=0x10002;
            }else assert(criticalProducer(packet,from,-1,0x10002,0,0,1)==0x1234);
            sum+=*(int*)(packet+0xc);
        }
        *(int*)(person+0xe8)=profile==2?27:profile==3?31:-1;
        *(int*)(group+0x600)=count;g_defenseSeed=4;return sum;
    };
    auto expected=[&](int count,int sum){uint32_t seed=4*0x6c078965u+0x3039u;
        int factor=10*count+70+(seed>>16)%(5*count);return (int)((double)factor*(double)multiplier*(double)sum);};
    assert(skilltelemetry::open(L"build/aggregate_test.csv",assistFixtureActor,nullptr));int inputs=0;
    for(int count:{2,3,6,12})for(int profile:{0,1,2,3})for(int balance:{0,100,1000}){
        {CriticalCopyScope owner;owner.destination=group;int sum=prepare(count,profile,balance),damage=expected(count,sum);
            assert(handler(nullptr,group,1)==-(balance<damage?balance:damage));
            assert(g_ironwallFixtureDamage==damage && g_defenseSeed==(uint32_t)(4*0x6c078965u+0x3039u));
        }
        assert(g_criticalRecords.empty() && !g_criticalCopyScope);++inputs;
    }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/aggregate_edges_test.csv",assistFixtureActor,nullptr));
    for(int fault=0;fault<=13;++fault){CriticalCopyScope owner;owner.destination=group;int sum=prepare(2,0,1000),damage=expected(2,sum);
        if(fault<=9)g_ironwallFixtureMutation=fault;
        if(fault==10)g_ironwallFixtureSkip=true;
        if(fault==11)g_assistFixturePrimaryRegistered=false;
        if(fault==12)*(int*)(primary+0xc)=-1;
        if(fault==13)g_ironwallFixtureMutation=10;
        assert(handler(nullptr,group,1)==(fault==10?77:-(damage+(fault==13?1:0))));
    }
    skilltelemetry::close();int sum=prepare(2,0,1000),damage=expected(2,sum);
    assert(handler(nullptr,group,1)==-damage && g_defenseSeed==(uint32_t)(4*0x6c078965u+0x3039u));
    g_criticalProducerOriginal=oldProducer;g_criticalRecords.clear();
    printf("PASS aggregate provenance: %d native multi-source/mixed/untracked/immunity/vine groups, exact coefficient/RNG and troop clamps,14 lifetime and selective-provider controls\n",inputs);
}
