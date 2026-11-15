// The command body/UI and world indexes are adapters. The installed root
// prologue, Bole bridge, exact candidate append/resolve/assign/store CALLs,
// linked-list insertion and complete native4a7410/489bd0 execute.
static uint8_t bolePeople[4][400],boleBases[2][64];
static int boleHolderMode,boleAffinity,boleMode,boleOutcome,boleSame,boleParent,boleFailAppend;
static int boleRootCalls,boleAffinityCalls,boleResolveCalls,boleAssignCalls;
static int boleMutation=0;
static bool boleTargetRegistered=true;
static uintptr_t boleAppendCall,boleAssignCall,boleResolveCall;
static uintptr_t boleGateCall;
static bool boleUiGate=false;
struct BoleList {void* vtable;void* head;void* tail;int count;};
static void* boleNodes[8][3];static int boleNodeCount;
static uint8_t* __fastcall bolePerson(void*,void*,int id){return id==600?bolePeople[0]:id==14162?bolePeople[1]:id==1000?bolePeople[2]:id==1001 && boleTargetRegistered?bolePeople[3]:nullptr;}
static int __fastcall bolePersonId(void*,void*,void* person){for(int id:{600,14162,1000,1001})if(bolePerson(nullptr,nullptr,id)==person)return id;return -1;}
static void* __fastcall boleBase(void*,void*,int id){return id==8?boleBases[0]:id==60?boleBases[1]:nullptr;}
static int __fastcall boleBaseId(void*,void*,void* base){return base==boleBases[0]?8:base==boleBases[1]?60:-1;}
static int __fastcall boleOwner(uint8_t* person,void*){return *(int*)(person+0x104);}
static int __fastcall boleBaseOwner(uint8_t* base,void*){return *(int*)(base+0x20);}
static int __fastcall boleFind(void*,void*,int effect){assert(effect==146);
    for(int id:{600,14162,1000}){auto p=bolePerson(nullptr,nullptr,id);if(personHas(p,nullptr,effect))return id;}return -1;
}
static int __fastcall boleAffinityValue(void* candidate,void*,int holder){assert(candidate==bolePeople[3] && (holder==600 || holder==14162 || holder==1000));++boleAffinityCalls;return boleAffinity;}
static bool boleActor(const void* p,skilltelemetry::Actor& a){a.person=bolePersonId(nullptr,nullptr,(void*)p);if(a.person<0)return false;
    a.force=*(const int*)((const uint8_t*)p+0x104);a.ruler=635;a.city=8;a.year=194;a.month=6;a.day=11;strcpy_s(a.name,a.person==14162?"High Bole":a.person==1000?"Executor Bole":"Bole");return true;
}
static void* __fastcall boleNode(void* list,void*,void* previous,int zero){assert(zero==0);if(boleFailAppend)return nullptr;
    assert(boleNodeCount<8);auto node=boleNodes[boleNodeCount++];node[0]=nullptr;node[1]=previous;node[2]=nullptr;if(boleMutation!=8)++((BoleList*)list)->count;return node;
}
__declspec(naked) static int boleRejectFixture(){__asm {xor eax,eax
    ret}}
__declspec(naked) static void* __cdecl boleAppendFixture(void*,void*){__asm {
    sub esp,20
    mov ecx,[esp+24]
    mov eax,[esp+28]
    mov edx,g_boleFixtureCaller
    mov [esp+16],edx
    push eax
    jmp dword ptr [boleAppendCall]}}
__declspec(naked) static void boleAppendReturned(){__asm {add esp,20
    ret}}
__declspec(naked) static int __cdecl boleAssignFixture(void*,int,int,int){__asm {
    mov edx,esp
    push 0
    push 0
    push [edx+16]
    push [edx+12]
    push [edx+8]
    push 12
    push [edx+4]
    xor ecx,ecx
    jmp dword ptr [boleAssignCall]}}
__declspec(naked) static int __cdecl boleResolveFixture(void*,void*,int,int){__asm {
    mov edx,esp
    push 0
    push [edx+16]
    push [edx+12]
    push [edx+8]
    push [edx+4]
    jmp dword ptr [boleResolveCall]}}
static int __fastcall boleResetJob(void*,void*,uint8_t* p){++boleAssignCalls;*(int*)(p+0x13c)=-1;return 0;}
static int __fastcall boleSetBusy(void*,void*,int){return 0;}
static int __fastcall boleRefresh(void*,void*,void*){return 71;}
static int __fastcall boleNoAssignment(void*,void*,void*,int,int,int,int,int,int){return 71;}
static int __fastcall boleThrowAssignment(void*,void*,void*,int,int,int,int,int,int){throw 7;}
static int __cdecl boleResolveBody(void* actor,void* target,int location,int outcome,int mode){
    assert(actor==bolePeople[2] && target==bolePeople[3] && location==60 && mode==0);++boleResolveCalls;
    if(outcome){*(int*)(bolePeople[3]+0xa0)=3;*(int*)(bolePeople[3]+0x104)=2;*(int*)(bolePeople[3]+0x98)=8;}
    return 17;
}
__declspec(naked) static int __cdecl boleGateFixture(void*){__asm {push [esp+4]
    jmp dword ptr [boleGateCall]}}
static int __cdecl boleGateBody(void* source){
    if(boleMutation==14 && !boleUiGate){boleUiGate=true;int result=boleCanExecute(source);boleUiGate=false;return result;}
    BoleList list={};void* allocator[1]={(void*)boleNode};list.vtable=allocator;boleNodeCount=0;
    int eligible=runBole(source,bolePeople[3],boleSame);
    if(eligible){auto node=boleAppendFixture(&list,bolePeople[3]);assert((node!=nullptr)==!boleFailAppend);}
    return 1;
}
__declspec(naked) static void boleGateFrame(){__asm {push [esp+0xc]
    call boleGateBody
    add esp,12
    ret}}
static int __cdecl boleRootBody(void** command){
    ++boleRootCalls;assert(command[0]==boleBases[0] && command[1]==bolePeople[3] && command[2]==bolePeople[2]);
    auto caller=g_boleFixtureCaller;if(boleMutation==13)g_boleFixtureCaller=site(0x60634d);
    assert((boleMutation==12?boleGateBody(command[0]):boleGateFixture(command[0]))==1);g_boleFixtureCaller=caller;
    if(boleMode){auto original=g_boleAssignOriginal;
        if(boleMutation==10)g_boleAssignOriginal=(BoleAssign)boleNoAssignment;
        if(boleMutation==9)boleStore((uint8_t*)command[2],nullptr,12,1001,60,3,0,0);
        else assert(boleAssignFixture(command[2],boleMutation==11?1000:1001,60,3)==71);
        g_boleAssignOriginal=original;
    }
    else assert(boleResolveFixture(command[2],command[1],60,boleOutcome)==17);
    if(boleMutation==1)*(int*)(bolePeople[0]+0x104)=3;
    if(boleMutation==2)*(int*)(bolePeople[0]+0xe8)=-1;
    if(boleMutation==3)*(int*)(bolePeople[2]+0x104)=3;
    if(boleMutation==4)*(int*)(boleBases[0]+0x20)=3;
    if(boleMutation==5)boleTargetRegistered=false;
    if(boleMutation==6)skilltelemetry::invalidateContext();
    return boleParent;
}
__declspec(naked) static void boleRootFrame(){__asm {push [ebp+8]
    call boleRootBody
    add esp,4
    mov esp,ebp
    pop ebp
    ret}}
static void boleTelemetryTests(){
    auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryPersonId;auto oldBase=g_baseById;auto oldBaseId=g_telemetryBaseId;auto oldValid=g_growthValid;
    auto oldAffinity=g_affinityOriginal;auto oldAppend=g_boleAppendOriginal;auto oldAssign=g_boleAssignOriginal;auto oldStore=g_boleStoreOriginal;auto oldResolve=g_boleResolveOriginal;
    auto oldExecute=g_boleExecuteOriginal;auto oldResume=g_boleExecuteResume;auto oldReject=g_boleReject;
    auto oldGate=g_boleGateOriginal;auto oldGateResume=g_boleGateResume;auto oldGateCaller=g_boleGateCaller,oldBuilderCaller=g_boleBuilderCaller;
    void* personVt[25]={};void* baseVt[25]={};personVt[0x40/4]=boleOwner;baseVt[0x40/4]=boleBaseOwner;baseVt[0x5c/4]=boleFind;
    for(auto& p:bolePeople)*(void**)p=personVt;for(auto& b:boleBases)*(void**)b=baseVt;
    g_telemetryPerson=(PersonById)bolePerson;g_telemetryPersonId=(BuildingId)bolePersonId;g_baseById=(BaseById)boleBase;g_telemetryBaseId=(BuildingId)boleBaseId;g_growthValid=domesticValid;
    g_affinityOriginal=(Affinity)boleAffinityValue;g_boleAppendOriginal=(BoleAppend)site(0x47c1b0);g_boleAssignOriginal=(BoleAssign)site(0x4a7410);
    g_boleStoreOriginal=(BoleStore)site(0x489bd0);g_boleResolveOriginal=boleResolveBody;g_boleExecuteOriginal=(BoleExecute)boleExecuteNative;g_boleExecuteResume=(uint32_t)site(0x5c5946);
    g_boleReject=(uint32_t)boleRejectFixture;boleAppendCall=site(0x4cf7a2);boleAssignCall=site(0x5c5ae6);boleResolveCall=site(0x5c59e3);
    g_boleGateOriginal=(BoleGate)boleGateNative;g_boleGateResume=(uint32_t)site(0x5c56d7);g_boleGateCaller=site(0x5c595a);g_boleBuilderCaller=g_boleFixtureCaller=site(0x5c580f);
    boleGateCall=site(0x5c5955);memcpy((void*)site(0x5c595a),"\x83\xc4\x04\xc3",4);domesticStub(0x5c56d7,boleGateFrame);
    uint8_t appendBefore[151];memcpy(appendBefore,(void*)site(0x47c1b0),sizeof appendBefore);
    for(const auto& s:g_boleLayouts)if(s.start==0x47c1b0){assert(s.size==sizeof appendBefore);memcpy((void*)site(s.start),s.bytes,s.size);}
    CRITICAL_SECTION section;InitializeCriticalSection(&section);
    static auto enter=&EnterCriticalSection,leave=&LeaveCriticalSection;
    *(uint32_t*)site(0x47c1d1)=(uint32_t)&enter;*(uint32_t*)site(0x47c1f6)=*(uint32_t*)site(0x47c22e)=(uint32_t)&leave;
    *(uint32_t*)site(0x47c1c9)=*(uint32_t*)site(0x47c1f0)=*(uint32_t*)site(0x47c225)=(uint32_t)&section;
    domesticStub(0x4cf7a7,boleAppendReturned);*(uint8_t*)site(0x5c5aeb)=0xc3;memcpy((void*)site(0x5c59e8),"\x83\xc4\x14\xc3",4);
    domesticStub(0x5c5946,boleRootFrame);domesticStub(0x47a630,domesticValid);domesticStub(0x4a57b0,boleResetJob);domesticStub(0x489b40,boleSetBusy);domesticStub(0x482f80,boleRefresh);
    auto execute=(BoleExecute)site(0x5c5940);void* command[]={boleBases[0],bolePeople[3],bolePeople[2]};
    int inputs=0;boleRootCalls=boleAffinityCalls=boleResolveCalls=boleAssignCalls=0;
    assert(skilltelemetry::open(L"build/bole_test.csv",boleActor,nullptr));
    for(boleHolderMode=0;boleHolderMode<5;++boleHolderMode)for(int affinity:{0,25,26})for(boleMode=0;boleMode<2;++boleMode)
    for(boleOutcome=0;boleOutcome<2;++boleOutcome)for(boleFailAppend=0;boleFailAppend<2;++boleFailAppend)
    for(boleSame=0;boleSame<2;++boleSame)for(boleParent=0;boleParent<2;++boleParent){++inputs;boleAffinity=affinity;
        *(int*)(boleBases[0]+0x20)=2;*(int*)(boleBases[1]+0x20)=boleSame?2:3;
        *(int*)(bolePeople[0]+0xe8)=boleHolderMode==0 || boleHolderMode==2?108:-1;
        *(int*)(bolePeople[1]+0xe8)=boleHolderMode==1 || boleHolderMode==2?108:-1;
        *(int*)(bolePeople[2]+0xe8)=boleHolderMode==4?108:-1;
        for(auto& p:bolePeople){*(int*)(p+0x98)=8;*(int*)(p+0xa0)=3;*(int*)(p+0x104)=2;*(int*)(p+0x13c)=-1;}
        *(int*)(bolePeople[3]+0x98)=60;*(int*)(bolePeople[3]+0xa0)=4;*(int*)(bolePeople[3]+0x104)=-1;
        int calls=boleRootCalls;assert(execute(command)==boleParent && boleRootCalls==calls+1);
        if(boleMode)assert(*(int*)(bolePeople[2]+0x13c)==12 && *(int*)(bolePeople[2]+0x140)==1001 && *(int*)(bolePeople[2]+0x144)==60);
        else assert(*(int*)(bolePeople[3]+0xa0)==(boleOutcome?3:4));
        assert(!g_boleExecution && !g_boleOrder && !g_boleGate);
    }
    skilltelemetry::close();assert(inputs==480 && boleAffinityCalls==192 && boleResolveCalls==240 && boleAssignCalls==240);
    assert(skilltelemetry::open(L"build/bole_edges_test.csv",boleActor,nullptr));
    int edges=0;
    for(boleMutation=0;boleMutation<15;++boleMutation)for(boleMode=0;boleMode<2;++boleMode){
        if(boleMutation>=9 && !boleMode)continue;++edges;
        boleAffinity=25;boleSame=0;boleOutcome=boleParent=1;boleFailAppend=0;boleTargetRegistered=true;
        *(int*)(boleBases[0]+0x20)=2;*(int*)(boleBases[1]+0x20)=3;
        *(int*)(bolePeople[0]+0xe8)=108;*(int*)(bolePeople[1]+0xe8)=*(int*)(bolePeople[2]+0xe8)=-1;
        for(auto& p:bolePeople){*(int*)(p+0x98)=8;*(int*)(p+0xa0)=3;*(int*)(p+0x104)=2;*(int*)(p+0x13c)=-1;}
        *(int*)(bolePeople[3]+0x98)=60;*(int*)(bolePeople[3]+0xa0)=4;*(int*)(bolePeople[3]+0x104)=-1;
        if(boleMutation==10){*(int*)(bolePeople[2]+0x13c)=12;*(int*)(bolePeople[2]+0x140)=1001;*(int*)(bolePeople[2]+0x144)=60;}
        assert((boleMutation==7?g_boleExecuteOriginal(command):execute(command))==1);
        assert(!g_boleExecution && !g_boleOrder);
    }
    skilltelemetry::close();assert(edges==24);boleMutation=0;boleTargetRegistered=true;
    for(boleMode=0;boleMode<2;++boleMode){
        *(int*)(bolePeople[3]+0x98)=60;*(int*)(bolePeople[3]+0xa0)=4;*(int*)(bolePeople[3]+0x104)=-1;
        assert(execute(command)==1 && !g_boleExecution && !g_boleOrder);
        assert(boleMode?*(int*)(bolePeople[2]+0x140)==1001:*(int*)(bolePeople[3]+0xa0)==3);
    }
    assert(skilltelemetry::open(L"build/bole_unwind_test.csv",boleActor,nullptr));
    {BoleExecution execution(command);
        assert(boleGateFixture(command[0])==1 && execution.appended);
        auto original=g_boleAssignOriginal;g_boleAssignOriginal=(BoleAssign)boleThrowAssignment;
        try{boleAssign(nullptr,nullptr,command[2],12,1001,60,3,0,0);assert(false);}catch(int value){assert(value==7);}
        g_boleAssignOriginal=original;assert(g_boleOrder==nullptr && g_boleExecution==&execution);
        boleStore((uint8_t*)command[2],nullptr,12,1001,60,3,0,0);assert(!execution.assigned);
    }
    skilltelemetry::close();assert(!g_boleExecution && !g_boleOrder);
    DeleteCriticalSection(&section);
    memcpy((void*)site(0x47c1b0),appendBefore,sizeof appendBefore);
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldId;g_baseById=oldBase;g_telemetryBaseId=oldBaseId;g_growthValid=oldValid;g_affinityOriginal=oldAffinity;
    g_boleAppendOriginal=oldAppend;g_boleAssignOriginal=oldAssign;g_boleStoreOriginal=oldStore;g_boleResolveOriginal=oldResolve;
    g_boleExecuteOriginal=oldExecute;g_boleExecuteResume=oldResume;g_boleReject=oldReject;
    g_boleGateOriginal=oldGate;g_boleGateResume=oldGateResume;g_boleGateCaller=oldGateCaller;g_boleBuilderCaller=oldBuilderCaller;
    puts("PASS Bole146:480 native bridge/append/order/store inputs, first-high-composed/self-executing provider gates, native return values and separate resolved/assigned outcomes");
    puts("PASS Bole146:22 stale/preview/incomplete-list/no-store/wrong-target/wrong-caller/nested-UI exclusions and two accepted-command controls");
    puts("PASS Bole146: disabled parity and assignment exception cleanup; a later unrelated setter cannot inherit the failed command scope");
}
