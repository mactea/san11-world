// Full native candidate routine and actual prisoner stores, with a controlled
// base personnel list and force-filter adapter. No live world traversal claim.
static int g_captureBaseId=23;
// A pointer-derived unit index is not a unit identity without its round trip.
static int __fastcall captureBaseFixtureUnitId(void*,void*,void* p){return p==g_criticalTarget?4094:p==g_criticalSource?4095:-1;}
static void* __fastcall captureBaseFixtureUnit(void*,void*,int id){return id==4094?g_criticalTarget:nullptr;}
static int __fastcall captureBaseFixtureId(void*,void*,void* p){return p==g_criticalSource?g_captureBaseId:-1;}
static void* __fastcall captureBaseFixtureLookup(void*,void*,int id){return id==g_captureBaseId && g_criticalSourceRegistered?g_criticalSource:nullptr;}
static int __fastcall captureBaseFixtureKind(void* p,void*){return p==g_criticalSource?5:11;}
static int __fastcall captureBaseFixtureOwner(void* p,void*){return p==g_criticalSource || p==g_personGuide || p==g_personOther?2:3;}
static void __fastcall captureBaseFixtureRegion(void* self,void* unused,uint8_t* list,int place,int mask){
    if((uintptr_t)_ReturnAddress()!=site(0x4b152c)){captureFixtureRegion(self,unused,list,place,mask);return;}
    assert(place==g_captureBaseId && mask==15);
    for(auto person:{g_personGuide,g_personOther}) {
        int status=*(int*)(person+0xa0);
        if(*(int*)(person+0x9c)==place && status>=0 && status<4)captureFixtureAppend(list,nullptr,person);
    }
}
static int __fastcall captureBaseFixtureFilter(uint8_t* list,void*,int field,int force,int exclude){
    assert(field==75 && force==2 && exclude==0);return *(int*)(list+0xc);
}
#include "capture_base_collection_cases.inc"
template<class Setup>
static void captureBaseTelemetryTests(Defeat run,Setup setup,uint8_t* source,uint8_t* target,void** vt,const uint8_t* prefix){
    auto oldUnitId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;auto oldBaseId=g_telemetryBaseId;auto oldBase=g_baseById;
    auto oldKind=vt[0x24/4],oldOwner=vt[0x40/4];
    g_telemetryUnitId=(BuildingId)captureBaseFixtureUnitId;g_telemetryBaseId=(BuildingId)captureBaseFixtureId;
    g_movementUnitById=(MovementUnitById)captureBaseFixtureUnit;
    g_baseById=(BaseById)captureBaseFixtureLookup;vt[0x24/4]=captureBaseFixtureKind;vt[0x40/4]=captureBaseFixtureOwner;
    uint32_t addresses[]={0x491770,0x4cf2f0,0x4af400};uint8_t saved[3][5];
    for(int n=0;n<3;++n)memcpy(saved[n],(void*)site(addresses[n]),5);
    domesticStub(0x491770,captureBaseFixtureId);domesticStub(0x4cf2f0,captureBaseFixtureRegion);domesticStub(0x4af400,captureBaseFixtureFilter);
    auto prepare=[&](int provider,int count,int disposition,int range,int seed){
        setup(provider,0,count,0,0,disposition,range,seed);
        for(auto person:{g_personGuide,g_personOther}){
            *(void**)person=vt;*(int*)(person+0x9c)=g_captureBaseId;*(int*)(person+0xa0)=1;
        }
    };
    int inputs=0;
    assert(skilltelemetry::open(L"build/capture_base_test.csv",afterFixtureActor,nullptr));
    for(int base:{23,42,52})for(int provider=0;provider<6;++provider)for(int count:{1,2})
        for(int disposition:{0,1})for(int range:{0,1})for(int seed:{0,4}){
            g_captureBaseId=base;prepare(provider,count,disposition,range,seed);
            uint32_t state=seed;int captured=0;
            for(int slot=0;slot<count;++slot){state=state*0x6c078965u+0x3039u;
                int chance=provider!=4 && range?100:5;if((state>>16)%100<(unsigned)chance)++captured;
            }
            run(nullptr,target,source,0,0,0);
            assert(g_defenseSeed==state && g_captureFixtureCandidates==captured);
            assert(!g_defeat && !g_captureAttempt && defeatUnitCleared(target));++inputs;
        }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/capture_base_edges_test.csv",afterFixtureActor,nullptr));
    g_captureBaseId=23;
    for(int mutation=1;mutation<=9;++mutation){
        prepare(0,1,1,1,4);g_captureFixtureMutation=mutation;
        run(nullptr,target,source,0,0,0);
        assert(*(int*)(g_personTarget+0xa0)==(mutation==6?3:5) && !g_defeat && !g_captureAttempt);
    }
    // Native list selection skips unaffiliated/inactive first holders and can
    // choose the second eligible holder, while duplicate holders do not stack.
    for(int profile=0;profile<3;++profile){
        prepare(5,1,1,1,4);
        if(profile==0)*(int*)(g_personGuide+0x9c)=99;
        if(profile==1)*(int*)(g_personGuide+0xa0)=5;
        run(nullptr,target,source,0,0,0);assert(*(int*)(g_personTarget+0xa0)==5);
    }
    skilltelemetry::close();
    prepare(0,1,1,1,4);run(nullptr,target,source,0,0,0);
    assert(*(int*)(g_personTarget+0xa0)==5 && !g_defeat && !g_captureAttempt);
    memcpy((void*)site(0x4af400),saved[2],5); // execute the original force filter
    captureBaseNativeCollectionTests(run,prepare,source,target,vt,prefix);
    g_telemetryUnitId=oldUnitId;g_movementUnitById=oldUnit;g_telemetryBaseId=oldBaseId;g_baseById=oldBase;
    vt[0x24/4]=oldKind;vt[0x40/4]=oldOwner;
    for(int n=0;n<3;++n)memcpy((void*)site(addresses[n]),saved[n],5);
    printf("PASS base capture19: %d native candidate inputs, city/gate/port sources, first/second/composed/duplicate providers, RNG/disposition stores and12 boundary cases\n",inputs);
}
