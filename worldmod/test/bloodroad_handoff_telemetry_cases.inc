// Real588cb0 thread-ID gate/publication,57daf0/439020 blocking wait,
//5892e0 case3/acknowledgement,588460 consumer,4a30d0 and4960a0 reset
// execute on two actual Windows threads. Rendering/list primitives are adapters;
// critical sections, events, atomic stop flag, sleep and thread IDs are real.
static HANDLE g_handoffPublished,g_handoffWaiting;
static uint8_t* g_handoffController;
static uintptr_t g_handoffPump;
static int g_handoffMutation=0,g_handoffPhase=0;
static DWORD g_handoffConsumerThread=0;
static DWORD (WINAPI* g_handoffThreadId)()=GetCurrentThreadId;
static BOOL WINAPI handoffResetEvent(HANDLE event){
    BOOL result=ResetEvent(event);
    if(event==*(HANDLE*)(g_handoffController+8))SetEvent(g_handoffWaiting);
    return result;
}
static BOOL (WINAPI* g_handoffResetEvent)(HANDLE)=handoffResetEvent;
static BOOL (WINAPI* g_handoffSetEvent)(HANDLE)=SetEvent;
static DWORD (WINAPI* g_handoffWait)(HANDLE,DWORD)=WaitForSingleObject;
static VOID (WINAPI* g_handoffSleep)(DWORD)=Sleep;
static VOID (WINAPI* g_handoffEnter)(LPCRITICAL_SECTION)=EnterCriticalSection;
static VOID (WINAPI* g_handoffLeave)(LPCRITICAL_SECTION)=LeaveCriticalSection;
static LONG WINAPI handoffExchangeAdd(LONG volatile* value,LONG amount){return InterlockedExchangeAdd(value,amount);}
static LONG (WINAPI* g_handoffExchange)(LONG volatile*,LONG)=handoffExchangeAdd;
static void __fastcall handoffUnlock(void* self,void*){
    ((BloodroadUnlock)site(0x57dbd0))(self);
    assert(*(int*)((uint8_t*)self+0x4c)==3 && *(void**)((uint8_t*)self+0x48));
    SetEvent(g_handoffPublished);
}
static int __cdecl handoffKind(void* unit,int kind){return (unit==g_criticalSource || unit==g_criticalTarget) && kind==11;}
static void* __fastcall handoffUnlink(void*,void*,void*,int){return nullptr;}
static void __cdecl handoffVisualId(int id){assert(id==4094 || id==4095);}
static int __fastcall handoffConsume(void* self,void*,void* unit){
    int result=((BloodroadVisual)site(0x588460))(self,unit);
    if(g_handoffMutation==6)((DefeatUnitReset)site(0x4960a0))(unit,0); // second unrelated reset is not a second matched outcome
    return result;
}
static DWORD WINAPI handoffConsumer(void*){
    assert(WaitForSingleObject(g_handoffPublished,10000)==WAIT_OBJECT_0);
    // Let the real producer reset its acknowledgement event before the UI
    // acknowledges, avoiding a test-scheduler-dependent lost wakeup.
    assert(WaitForSingleObject(g_handoffWaiting,10000)==WAIT_OBJECT_0);
    g_handoffConsumerThread=GetCurrentThreadId();assert(g_handoffConsumerThread!=*(DWORD*)(g_handoffController+0x5e0));
    if(g_handoffMutation==1)*(int*)(g_criticalTarget+0x2c)^=1;
    if(g_handoffMutation==2)skillsTelemetryStream(1,0);
    if(g_handoffMutation==3)*(int*)(g_personTarget+0xe8)=-1;
    if(g_handoffMutation==4)g_captureFixtureMutation=8;
    if(g_handoffMutation==5){uint8_t original[0xf4];memcpy(original,g_criticalTarget,sizeof original);
        ((DefeatUnitReset)site(0x4960a0))(g_criticalTarget,0);memcpy(g_criticalTarget,original,sizeof original);}
    if(g_handoffMutation==7)*(void**)(g_handoffController+0x48)=g_criticalSource;
    typedef void(__thiscall* Pump)(void*);((Pump)g_handoffPump)(g_handoffController);
    return 0;
}
template<class Setup>
static void bloodroadHandoffTests(Defeat run,Setup setup,uint8_t* source,uint8_t* target,uint8_t* person,uint8_t* spouse){
    uint8_t controller[0x600]={};g_handoffController=controller;
    void* vt[7]={};vt[0xc/4]=(void*)site(0x57dae0);vt[0x10/4]=(void*)site(0x57daf0);vt[0x14/4]=(void*)site(0x439060);
    *(void**)controller=vt;auto event=CreateEventW(nullptr,TRUE,FALSE,nullptr);assert(event);*(HANDLE*)(controller+8)=event;
    InitializeCriticalSection((LPCRITICAL_SECTION)(controller+0x5c8));g_handoffPublished=CreateEventW(nullptr,FALSE,FALSE,nullptr);assert(g_handoffPublished);
    g_handoffWaiting=CreateEventW(nullptr,FALSE,FALSE,nullptr);assert(g_handoffWaiting);
    uint8_t visual[5];memcpy(visual,(void*)site(0x588cb0),sizeof visual);
    memcpy((void*)site(0x588cb0),g_bloodroadHandoffLayouts[0].bytes,sizeof visual); // undo earlier world adapter
    auto oldVisual=g_bloodroadVisualOriginal;auto oldUnlock=g_bloodroadUnlockOriginal;auto oldConsume=g_bloodroadConsumeOriginal;
    g_bloodroadVisualOriginal=(BloodroadVisual)site(0x588cb0);g_bloodroadUnlockOriginal=(BloodroadUnlock)handoffUnlock;g_bloodroadConsumeOriginal=(BloodroadVisual)handoffConsume;
    g_handoffPump=site(0x5892e0);
    auto oldController=*(uint32_t*)site(0x4bce81);*(uint32_t*)site(0x4bce81)=(uint32_t)controller;
    uint32_t imports[]={0x57dd85,0x57e5d5,0x57dbc9,0x57dbd9,0x4390b8,0x439029,0x439068,0x439012,0x43903b,0x57e621};uint32_t saved[10];
    const void* values[]={&g_handoffThreadId,&g_handoffThreadId,&g_handoffEnter,&g_handoffLeave,&g_handoffExchange,
        &g_handoffResetEvent,&g_handoffWait,&g_handoffSetEvent,&g_handoffSleep,&g_handoffPhase};
    for(int n=0;n<10;++n){saved[n]=*(uint32_t*)site(imports[n]);*(uint32_t*)site(imports[n])=(uint32_t)values[n];}
    uint32_t jumpTable[29];for(int n=0;n<29;++n)jumpTable[n]=(uint32_t)site(*(uint32_t*)site(0x589598+4*n));
    auto oldJump=*(uint32_t*)site(0x58931d);*(uint32_t*)site(0x58931d)=(uint32_t)jumpTable;
    auto unitVt=*(void***)target;auto oldReset=unitVt[0x20/4];unitVt[0x20/4]=(void*)site(0x4960a0);
    domesticStub(0x5ab830,captureFixtureVoid1);domesticStub(0x5a00a0,handoffVisualId);
    domesticStub(0x47a660,handoffKind);domesticStub(0x4a1e40,handoffUnlink);
    assert(skilltelemetry::open(L"build/bloodroad_handoff_test.csv",afterFixtureActor,nullptr));
    int commands=0;
    auto prepare=[&](int bloodroad,int count,int mode,int mutation){
        setup(0,bloodroad,count,0,0,1,1,4);*(int*)(source+8)=0;*(int*)(source+0xa8)=4;
        g_handoffMutation=mutation;g_handoffConsumerThread=0;ResetEvent(event);ResetEvent(g_handoffPublished);ResetEvent(g_handoffWaiting);
        *(int*)(controller+0xc)=mode==2?1:0;*(int*)(controller+0x4c)=29;*(void**)(controller+0x48)=nullptr;
        *(DWORD*)(controller+0x5e0)=mode?GetCurrentThreadId():0;
    };
    for(int bloodroad:{1,2,3})for(int count:{1,2}){
        if(bloodroad==2 && count==1)continue;
        for(int mode:{0,1,2})for(int repeat=0;repeat<16;++repeat){
            prepare(bloodroad,count,mode,0);HANDLE thread=mode==1?CreateThread(nullptr,0,handoffConsumer,nullptr,0,nullptr):nullptr;
            if(mode==1)assert(thread);run(controller,target,source,0,0,0);
            if(thread){assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);assert(g_handoffConsumerThread);}
            if(mode==2){assert(!defeatUnitCleared(target) && g_bloodroadPending.empty());*(int*)(controller+0xc)=0;
                ((void(__thiscall*)(void*))g_handoffPump)(controller); // producer already returned: no retained provenance
            }
            assert(defeatUnitCleared(target) && g_movementUnitById(nullptr,4094)==target && g_defenseSeed==4);
            assert(*(int*)(person+0xa0)==3 && *(int*)(person+0x9c)==5);
            if(count==2)assert(*(int*)(spouse+0xa0)==3 && *(int*)(spouse+0x9c)==5);
            assert(g_bloodroadPending.empty() && !g_bloodroadTransfer && !g_bloodroadConsumer && !g_defeat);++commands;
        }
    }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/bloodroad_handoff_edges_test.csv",afterFixtureActor,nullptr));
    for(int mutation=1;mutation<=7;++mutation){
        prepare(1,2,1,mutation);auto thread=CreateThread(nullptr,0,handoffConsumer,nullptr,0,nullptr);assert(thread);
        run(controller,target,source,0,0,0);assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);
        assert(defeatUnitCleared(target)==(mutation!=7) && g_bloodroadPending.empty() && g_defenseSeed==4);
    }
    skilltelemetry::close();prepare(1,2,1,0);auto thread=CreateThread(nullptr,0,handoffConsumer,nullptr,0,nullptr);assert(thread);
    run(controller,target,source,0,0,0);assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);
    assert(defeatUnitCleared(target) && g_bloodroadPending.empty() && g_defenseSeed==4);
    unitVt[0x20/4]=oldReset;*(uint32_t*)site(0x58931d)=oldJump;
    *(uint32_t*)site(0x4bce81)=oldController;
    for(int n=0;n<10;++n)*(uint32_t*)site(imports[n])=saved[n];
    memcpy((void*)site(0x588cb0),visual,sizeof visual);g_bloodroadVisualOriginal=oldVisual;g_bloodroadUnlockOriginal=oldUnlock;g_bloodroadConsumeOriginal=oldConsume;
    DeleteCriticalSection((LPCRITICAL_SECTION)(controller+0x5c8));CloseHandle(event);CloseHandle(g_handoffPublished);CloseHandle(g_handoffWaiting);
    printf("PASS bloodroad handoff: %d direct/cross-thread/stop-return commands, real native publication/wait/case3/reset/ack, canonical fixed slots, byte-identical reuse/context/provider/wrong-consumer/retired exclusions\n",commands);
}
