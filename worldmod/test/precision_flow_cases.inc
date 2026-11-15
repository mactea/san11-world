// Actual4bf02d..4bf175 selects the provider and100/50 reward. No test supplies
// QueryOrigin or a precision provider. Only relocated return addresses are
// canonicalized by precisionFixtureCaller in the test translation unit.
static uintptr_t g_precisionFlowEntry;
static bool g_precisionFlowAppendFailed=false;
struct PrecisionFlowNode {PrecisionFlowNode* next;void* person;};
static PrecisionFlowNode g_precisionFlowNodes[2];
static int g_precisionFlowNodeCount=0;
static void __fastcall precisionFlowClear(uint8_t* list,void*) {
    *(void**)(list+4)=*(void**)(list+8)=nullptr;*(int*)(list+12)=0;g_precisionFlowNodeCount=0;
}
static void* __fastcall precisionFlowAppend(uint8_t* list,void*,void* person) {
    if(g_precisionFlowAppendFailed)return nullptr;
    assert(g_precisionFlowNodeCount<2);auto node=&g_precisionFlowNodes[g_precisionFlowNodeCount++];
    node->next=nullptr;node->person=person;
    auto tail=*(PrecisionFlowNode**)(list+8);if(tail)tail->next=node;else *(void**)(list+4)=node;
    *(void**)(list+8)=node;++*(int*)(list+12);return node;
}
static void** __fastcall precisionFlowNext(void*,void*,PrecisionFlowNode** cursor) {
    auto node=*cursor;assert(node);*cursor=node->next;return &node->person;
}
static int __cdecl precisionFlowPersonValid(void* p){return p && (p==g_precisionNativePerson || p==g_precisionSecondaryPerson);}
__declspec(naked) static int __cdecl runPrecisionFlow(void*,void*,void*,int) {__asm {
    push ebp
    mov ebp,esp
    mov edx,ebp
    sub esp,0x4ac
    push ebx
    push esi
    push edi
    mov [esp+0x4a4],ebp
    xor ebx,ebx
    mov [esp+0x1c],ebx
    mov eax,[edx+16]
    mov [esp+0x18],eax
    mov eax,[edx+20]
    mov [esp+0x24],eax
    mov edi,[edx+12]
    mov ebp,[edx+8]
    mov esi,g_precisionNativeVictim
    jmp dword ptr [g_precisionFlowEntry]}}
__declspec(naked) static void precisionFlowDone() {__asm {
    mov ebp,[esp+0x4a4]
    pop edi
    pop esi
    pop ebx
    mov esp,ebp
    pop ebp
    ret}}
template<class Begin>
static void precisionFlowTests(Begin begin,uint8_t* force,uint8_t* source,uint8_t* person) {
    uint8_t secondary[400]={};*(void**)secondary=*(void**)person;g_precisionSecondaryPerson=secondary;
    uint32_t addresses[]={0x490b00,0x490aa0,0x491770,0x47a600,0x47be50,0x47c100,0x47c1b0,0x47bed0,0x491310};
    uint8_t saved[9][5];for(int n=0;n<9;++n)memcpy(saved[n],(void*)site(addresses[n]),5);
    domesticStub(0x490b00,precisionRootPerson);domesticStub(0x490aa0,precisionRootForce);domesticStub(0x491770,precisionRootBaseId);
    domesticStub(0x47a600,precisionFlowPersonValid);domesticStub(0x47be50,precisionFlowClear);domesticStub(0x47c100,precisionFlowClear);
    domesticStub(0x47c1b0,precisionFlowAppend);domesticStub(0x47bed0,precisionFlowNext);
    domesticStub(0x491310,precisionRootPersonId);
    auto vt=*(void***)source;auto oldHas=vt[0x58/4],oldFind=vt[0x5c/4];
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=(void*)site(0x495600);
    uint8_t oldTail[11];memcpy(oldTail,(void*)site(0x4bf16f),11);
    memcpy((void*)site(0x4bf16f),"\xeb\x04\x8b\x7c\x24\x1c",6);domesticStub(0x4bf175,precisionFlowDone);
    auto tableOperand=(uint32_t*)site(0x488b3d);uint32_t oldTable=*tableOperand,jumps[10];
    for(int n=0;n<10;++n)jumps[n]=(uint32_t)site(*(uint32_t*)site(0x488b9c+4*n));*tableOperand=(uint32_t)jumps;
    auto countOperand=(int*)site(0x4cf34e);int oldCount=*countOperand;assert(oldCount==1100);*countOperand=14163;
    auto cityBound=(uint8_t*)site(0x4866e3);uint8_t oldCityBound[5];memcpy(oldCityBound,cityBound,5);
    assert(!memcmp(cityBound,"\x83\xf8\x29\x7e\x03",5));
    auto cityCave=(uint8_t*)VirtualAlloc(nullptr,16,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(cityCave);
    const uint8_t cityCode[]={0x3d,0,0,0,0,0x0f,0x8e,0,0,0,0,0xe9,0,0,0,0};
    memcpy(cityCave,cityCode,16);*(int*)(cityCave+1)=529;
    *(int32_t*)(cityCave+7)=(int32_t)(site(0x4866eb)-(uintptr_t)cityCave-11);
    *(int32_t*)(cityCave+12)=(int32_t)(site(0x4866e8)-(uintptr_t)cityCave-16);
    cityBound[0]=0xe9;*(int32_t*)(cityBound+1)=(int32_t)((uintptr_t)cityCave-(uintptr_t)cityBound-5);
    g_precisionFlowEntry=site(0x4bf02d);g_precisionPersonId=1500;g_precisionNativeForceId=1;g_precisionNativeValidForce=true;
    assert(skilltelemetry::open(L"build/precision_flow_test.csv",precisionActor,nullptr));
    int applied=0;
    for(int variant=0;variant<3;++variant)for(int profile=0;profile<10;++profile)for(int flag:{0,1}) {
        int kind=variant==0?1:2;g_precisionNativeBaseId=variant==2?529:23;
        g_precisionNativeSourceKind=kind;g_precisionPrimaryRegistered=true;begin();
        *(int*)(source+0x10)=14162;
        *(int*)(person+0xe8)=profile==0 || profile==2?-1:profile==3?249:20;
        *(int*)(secondary+0xe8)=profile==0 || profile==1 || profile==6?-1:20;
        *(int*)(secondary+0x9c)=g_precisionNativeBaseId;*(int*)(secondary+0xa0)=3;
        if(profile==4)*(int*)(person+0xa0)=5;
        if(profile==5)*(int*)(person+0x9c)=g_precisionNativeBaseId+1;
        if(profile==6)*(int*)(person+0x98)=9999;
        if(profile==8)g_precisionPrimaryRegistered=false;
        g_precisionFlowAppendFailed=profile==7;*(uint16_t*)(force+0xa2)=1000;
        bool eligible=profile!=0 && !(kind==2 && profile==7);
        int gain=flag?(eligible?50:25):0;int resolved=g_precisionResolutions;
        assert(!g_queryOrigin);g_precisionScanLookups=0;
        runPrecisionFlow(source,kind==1?source:nullptr,kind==2?source:nullptr,flag);
        if(*(uint16_t*)(force+0xa2)!=1000+gain)fprintf(stderr,"precision phase kind=%d profile=%d flag=%d points=%u expected=%d\n",kind,profile,flag,*(uint16_t*)(force+0xa2),1000+gain);
        assert(*(uint16_t*)(force+0xa2)==1000+gain && !g_queryOrigin && !g_precisionStore);
        assert(g_precisionResolutions==resolved+(flag && eligible?2:0));applied+=flag && eligible;
        assert(g_precisionScanLookups==(kind==2 && flag?14163:0));
    }
    skilltelemetry::close();assert(applied==25);assertBattleAppliedRows(L"build/precision_flow_test.csv",25);
    memcpy(cityBound,oldCityBound,5);assert(VirtualFree(cityCave,0,MEM_RELEASE));g_precisionNativeBaseId=23;
    *countOperand=oldCount;*tableOperand=oldTable;memcpy((void*)site(0x4bf16f),oldTail,11);
    for(int n=0;n<9;++n)memcpy((void*)site(addresses[n]),saved[n],5);
    vt[0x58/4]=oldHas;vt[0x5c/4]=oldFind;g_precisionSecondaryPerson=nullptr;g_precisionPrimaryRegistered=true;
    puts("PASS precision native phase:60 unit/base23/base529/gate/provider inputs, real provider selection and request arithmetic, native full14163-person affiliation/status traversal and member scan;25 native store attributions");
}
