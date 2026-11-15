// Native584ab2 CALL, complete occupied-base59f5f0 path, actual59f771
// transfer CALL and4bf61a receipt, and complete4a31e0 current-location body.
// Transfer outer body, world/spatial lists and person-list primitives are adapters.
static uint8_t steeringEntryBase[0x80],steeringEntryMeta[0x180];
static int steeringEntryBaseId=23,steeringEntryForce=2,steeringEntryFault=0;
static int steeringEntryStores=0,steeringEntryInserts=0,steeringEntryTransfers=0;
static uintptr_t steeringEntryCall,steeringEntryReceipt;
static uintptr_t steeringEntryMarkerCall;
static int steeringEntryCompletedMarkers=0;
static bool steeringEntryRegistered=true,steeringEntryPersonPresent=true;
static void(*steeringEntryAfterStore)()=nullptr;
static uint8_t* steeringEntryCode;
static LONG CALLBACK steeringEntryException(EXCEPTION_POINTERS* e){
    if(e->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION)fprintf(stderr,"entry AV native=%08x ecx=%08lx edx=%08lx eax=%08lx address=%08lx\n",(unsigned)(e->ContextRecord->Eip-(uintptr_t)steeringEntryCode+0x400000),e->ContextRecord->Ecx,e->ContextRecord->Edx,e->ContextRecord->Eax,(unsigned long)e->ExceptionRecord->ExceptionInformation[1]);
    return EXCEPTION_CONTINUE_SEARCH;
}
static int __cdecl steeringEntryValid(void* p){return p!=nullptr;}
static int __fastcall steeringEntryOwner(void*,void*){return steeringEntryForce;}
static int __fastcall steeringEntryBaseIndex(void*,void*,void* p){return p==steeringEntryBase?steeringEntryBaseId:-1;}
static void* __fastcall steeringEntryBaseLookup(void*,void*,int id){return id==steeringEntryBaseId && steeringEntryRegistered?steeringEntryBase:nullptr;}
static void* __fastcall steeringEntryObject(void*,void*){
    bool native=(uintptr_t)_ReturnAddress()==(uintptr_t)steeringEntryCode+0x59f632-0x400000;
    return steeringEntryFault==10 && !native?nullptr:steeringEntryBase;
}
static uint8_t* __fastcall steeringEntryPersonLookup(void*,void*,int id){return steeringEntryPersonPresent?steeringPerson(nullptr,nullptr,id):nullptr;}
static int __fastcall steeringEntryUnitIndex(void* self,void* unused,void* p){return steeringEntryFault==15 && steeringEntryStores?18:steeringUnitId(self,unused,p);}
static void* __fastcall steeringEntryLeader(void*,void*){return steeringPeople[0];}
static void* __fastcall steeringEntryMetadata(void*,void*){return steeringEntryMeta;}
static void __fastcall steeringEntryRemovePerson(void*,void*,void*){}
static void* __fastcall steeringEntryInsertPerson(void*,void*,void*){++steeringEntryInserts;return nullptr;}
static void __fastcall steeringEntrySort(void*,void*,int,int,int,int){}
static void __cdecl steeringEntryNoTransfer(int,uint32_t){}
__declspec(naked) static void __cdecl runSteeringEntryPerson(void*,void*,int){__asm {
    mov ecx,[esp+4]
    mov eax,[esp+8]
    push [esp+12]
    push eax
    jmp dword ptr [steeringEntryReceipt]}}
__declspec(naked) static void __cdecl runSteeringEntry(uint32_t*,int,uint32_t){__asm {
    push esi
    mov esi,[esp+8]
    push [esp+16]
    push [esp+16]
    jmp dword ptr [steeringEntryCall]}}
__declspec(naked) static void steeringEntryReturned(){__asm {
    add esp,8
    pop esi
    ret}}
__declspec(naked) static void steeringEntryMarkerReturned(){__asm {add esp,4
    ret}}
__declspec(naked) static void __cdecl runSteeringEntryMarker(void*,void*){__asm {
    push esi
    push edi
    mov edi,[esp+12]
    mov esi,[esp+16]
    call dword ptr [steeringEntryMarkerCall]
    pop edi
    pop esi
    ret}}
static void __fastcall steeringEntryStore(void* self,void*,uint8_t* person,int base){
    ++steeringEntryStores;
    if(steeringEntryFault!=1)((SteeringEnterPerson)(steeringEntryReceipt-0x4bf61a+0x4a31e0))(self,person,base);
    if(steeringEntryFault==3)skilltelemetry::invalidateContext();
    if(steeringEntryFault==4)steeringCommand[3]^=1;
    if(steeringEntryFault==5)*(int*)(steeringPeople[0]+0xe8)=-1;
    if(steeringEntryFault==6)*(int*)(steeringUnit+0xc)=-1;
    if(steeringEntryFault==7)++steeringForce;
    if(steeringEntryFault==8)++steeringEntryForce;
    if(steeringEntryFault==14)steeringEntryPersonPresent=false;
    if(steeringEntryFault==21)*(uint32_t*)(steeringUnit+0x3c)^=1;
    if(steeringEntryAfterStore)steeringEntryAfterStore();
}
static int __fastcall steeringEntryTransferBody(void* self,void*,void* unit,void* base){
    assert(base==steeringEntryBase);++steeringEntryTransfers;
    if(steeringEntryFault==2)skilltelemetry::invalidateContext();
    if(steeringEntryFault==9)steeringEntryRegistered=false;
    if(steeringEntryFault==20 && unit==steeringUnit){
        uint8_t foreign[0xf4]={};steeringBaseTransfer(self,nullptr,foreign,base);
        for(auto& person:steeringPeople)*(int*)(person+0x98)=5000;
    }
    for(int n=0;n<3;++n){
        int id=*(int*)(steeringUnit+0xc+4*n);auto p=steeringPerson(nullptr,nullptr,id);if(!p)continue;
        runSteeringEntryPerson(self,p,steeringEntryBaseId+(steeringEntryFault==11));
        if(steeringEntryFault==13 && n==0){*(int*)(p+0x98)=5000;runSteeringEntryPerson(self,p,steeringEntryBaseId);}
    }
    for(int n=0;n<steeringEntryCompletedMarkers;++n)runSteeringEntryMarker(unit,base);
    // The native transfer's zero-return path still permits committed member
    // relocation. This event makes no claim about its later message/removal.
    return 0;
}
#include "zoc_entry_telemetry_cases.inc"
#include "entry_native_transfer_cases.inc"
static void steeringEntryTelemetryTests(uint8_t* code,void* controller){
    steeringEntryCode=code;auto exception=AddVectoredExceptionHandler(1,steeringEntryException);
    auto at=[&](uint32_t a){return code+a-0x400000;};
    auto stub=[&](uint32_t a,const void* fn){auto p=at(a);p[0]=0xe9;*(int32_t*)(p+1)=(int32_t)((const uint8_t*)fn-p-5);};
    auto oldEntry=g_steeringEntryOriginal;auto oldTransfer=g_steeringTransferOriginal;auto oldPersonMove=g_steeringEnterPersonOriginal;
    auto oldBase=g_baseById;auto oldBaseId=g_telemetryBaseId;auto oldObject=g_zocObjectFromCell;auto oldValid=g_growthValid;
    auto oldPerson=g_telemetryPerson;auto oldUnitId=g_telemetryUnitId;
    g_steeringEntryOriginal=(SteeringEntry)at(0x59f5f0);g_steeringTransferOriginal=(SteeringTransfer)steeringEntryTransferBody;
    g_steeringEnterPersonOriginal=(SteeringEnterPerson)steeringEntryStore;
    g_baseById=(BaseById)steeringEntryBaseLookup;g_telemetryBaseId=(BuildingId)steeringEntryBaseIndex;
    g_zocObjectFromCell=(ReachObject)steeringEntryObject;g_growthValid=steeringEntryValid;
    g_telemetryPerson=(PersonById)steeringEntryPersonLookup;g_telemetryUnitId=(BuildingId)steeringEntryUnitIndex;
    uint32_t addresses[]={0x483b20,0x490e70,0x495a40,0x490d00,0x47a600,0x47a630,0x4866f0,0x486720,0x486760,0x4a2cb0,0x47c1b0,0x47cd50,0x56e4f0};
    uint8_t saved[13][5];for(int n=0;n<13;++n)memcpy(saved[n],at(addresses[n]),5);
    stub(0x483b20,steeringEntryObject);stub(0x490e70,steeringUnitLookup);stub(0x495a40,steeringEntryLeader);
    stub(0x490d00,steeringEntryBaseLookup);stub(0x47a600,steeringEntryValid);stub(0x47a630,steeringEntryValid);
    stub(0x4866f0,steeringEntryMetadata);stub(0x486720,steeringEntryMetadata);stub(0x486760,steeringEntryMetadata);
    stub(0x4a2cb0,steeringEntryRemovePerson);stub(0x47c1b0,steeringEntryInsertPerson);stub(0x47cd50,steeringEntrySort);
    stub(0x56e4f0,steeringRemove);
    uint8_t oldRootTail[5],oldPersonTail=*at(0x4bf61f);memcpy(oldRootTail,at(0x584ab7),5);stub(0x584ab7,steeringEntryReturned);*at(0x4bf61f)=0xc3;
    steeringEntryCall=(uintptr_t)at(0x584ab2);steeringEntryReceipt=(uintptr_t)at(0x4bf61a);
    steeringEntryMarkerCall=(uintptr_t)at(0x4bf636);
    uint32_t oldWidth=*(uint32_t*)at(0x59f61d),oldMap=*(uint32_t*)at(0x59f629);
    void* baseVt[17]={};baseVt[16]=steeringEntryOwner;*(void**)steeringEntryBase=baseVt;
    void* unitVt[17]={};unitVt[16]=steeringOwner;auto oldUnitVt=*(void**)steeringUnit;*(void**)steeringUnit=unitVt;
    uint8_t ordinary[384];memset(ordinary,8,sizeof ordinary);g_table=(uint32_t)ordinary;g_steerReady=true;
    for(int w:{9,10})for(int t:{7,8}){ordinary[w*32+t]=8+2*(w-9)+(t==8);g_steer[w*32+t]=ordinary[w*32+t]-1;}
    uint32_t command[44]={};steeringCommand=command;
    auto prepare=[&](int provider,int transport){
        steeringFault=0;steeringForce=steeringEntryForce=2;steeringEntryFault=0;
        steeringEntryRegistered=steeringEntryPersonPresent=true;steeringEntryStores=steeringEntryInserts=steeringEntryTransfers=0;
        *(int*)(steeringUnit+8)=transport;*(int*)(steeringUnit+0x50)=10;*(uint32_t*)(steeringUnit+0x3c)=command[1];
        for(int n=0;n<3;++n){*(int*)(steeringUnit+0xc+4*n)=n==0?600:n==1?14162:1000;
            *(int*)(steeringPeople[n]+0xe8)=-1;*(int*)(steeringPeople[n]+0x98)=5000;}
        if(provider<4)*(int*)(steeringPeople[provider==3?0:provider]+0xe8)=5;
        if(provider==3)*(int*)(steeringPeople[1]+0xe8)=5;
        if(provider==4)*(int*)(steeringPeople[0]+0xe8)=249;
    };
    int inputs=0;
    assert(skilltelemetry::open(L"../build/steering_entry_test.csv",steeringActor,nullptr));
    for(int layout=0;layout<3;++layout){int width=layout==0?200:layout==1?249:530,height=layout==0?200:layout==1?530:249;
        std::vector<uint8_t> map((size_t)width*height*20);
        *(int*)at(0x5a0783)=*(int*)at(0x4963a0)=height;
        *(int*)at(0x5a078f)=*(int*)at(0x5a079a)=*(int*)at(0x4963ac)=width;
        *(uint32_t*)at(0x5a07aa)=*(uint32_t*)at(0x59f629)=(uint32_t)map.data();*(int*)at(0x59f61d)=width;
        int x=height-3,y=width-5;
        for(int kind=0;kind<4;++kind)for(int provider=0;provider<6;++provider)for(int transport:{0,1}){
            steeringEntryBaseId=kind==0?23:kind==1?42:kind==2?52:529;*(int*)(steeringEntryBase+8)=kind==3?0:kind;
            memset(command,0,sizeof command);command[0]=17;command[1]=x|(y<<16);command[2]=x|((y+3)<<16);command[43]=3;
            for(int n=0;n<3;++n){command[3+n]=x|((y+n+1)<<16);auto cell=map.data()+20*((size_t)x*width+y+n+1);*(uint32_t*)(cell+4)=n==0?7:n==1?0:8;}
            auto destination=map.data()+20*((size_t)x*width+y+3);*(uint32_t*)destination=2;*(uint16_t*)(destination+8)=500;
            prepare(provider,transport);runSteeringEntry(command,17,command[2]);
            assert(steeringEntryTransfers==1 && steeringEntryStores==3 && steeringEntryInserts==3);
            for(auto& person:steeringPeople)assert(*(int*)(person+0x98)==steeringEntryBaseId);
            assert(*(uint32_t*)(steeringUnit+0x3c)==command[1] && !g_steeringEntry && !g_steeringTransfer);++inputs;
        }
    }
    skilltelemetry::close();
    std::vector<uint8_t> map(200*200*20);*(int*)at(0x5a0783)=*(int*)at(0x5a078f)=*(int*)at(0x5a079a)=*(int*)at(0x59f61d)=200;
    *(uint32_t*)at(0x5a07aa)=*(uint32_t*)at(0x59f629)=(uint32_t)map.data();
    assert(skilltelemetry::open(L"../build/steering_entry_edges_test.csv",steeringActor,nullptr));
    for(int fault=0;fault<=21;++fault){
        memset(map.data(),0,map.size());memset(command,0,sizeof command);command[0]=17;command[1]=0x10002;command[2]=0x20002;command[3]=command[2];command[43]=1;
        auto destination=map.data()+20*402;*(uint32_t*)destination=2;*(uint32_t*)(destination+4)=7;*(uint16_t*)(destination+8)=500;
        steeringEntryBaseId=23;*(int*)(steeringEntryBase+8)=0;prepare(fault==17?4:0,1);steeringEntryFault=fault;
        if(fault==16)command[43]=41;
        if(fault==19){command[43]=40;command[2]=0x290002;
            for(int n=0;n<40;++n){command[3+n]=2|((n+2)<<16);*(uint32_t*)(map.data()+20*(402+n)+4)=7;}
            *(uint32_t*)destination=0;destination=map.data()+20*441;*(uint32_t*)destination=2;*(uint16_t*)(destination+8)=500;}
        if(fault==18)g_steeringEntryOriginal=steeringEntryNoTransfer;
        if(fault==12)runSteeringEntryPerson(nullptr,steeringPeople[0],23);
        else runSteeringEntry(command,17,command[2]);
        assert(!g_steeringEntry && !g_steeringTransfer);g_steeringEntryOriginal=(SteeringEntry)at(0x59f5f0);
    }
    skilltelemetry::close();
    prepare(0,1);runSteeringEntry(command,17,command[2]);assert(*(int*)(steeringPeople[0]+0x98)==23 && !g_steeringEntry && !g_steeringTransfer);
    zocEntryTelemetryTests(code,controller,prepare);
    entryNativeTransferTests(code,controller,prepare);
    for(int n=0;n<13;++n)memcpy(at(addresses[n]),saved[n],5);
    memcpy(at(0x584ab7),oldRootTail,5);*at(0x4bf61f)=oldPersonTail;*(uint32_t*)at(0x59f61d)=oldWidth;*(uint32_t*)at(0x59f629)=oldMap;
    g_steeringEntryOriginal=oldEntry;g_steeringTransferOriginal=oldTransfer;g_steeringEnterPersonOriginal=oldPersonMove;
    g_baseById=oldBase;g_telemetryBaseId=oldBaseId;g_zocObjectFromCell=oldObject;g_growthValid=oldValid;
    g_telemetryPerson=oldPerson;g_telemetryUnitId=oldUnitId;
    *(void**)steeringUnit=oldUnitVt;
    printf("PASS steering base entry: %d native occupied-base inputs, real member-location stores, three maps/four base kinds and22 receipt/context controls\n",inputs);
    RemoveVectoredExceptionHandler(exception);
}
