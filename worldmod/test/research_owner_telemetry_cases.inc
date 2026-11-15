static int researchOwnerFault=0;
static bool researchOwnerProviderRegistered=true,researchOwnerBaseRegistered=true;
static void** researchOwnerCommand;
static uint8_t* researchOwnerProject;
static uint8_t* researchOwnerOtherProject;
static uint8_t* researchOwnerOtherCity;
static UpkeepBuilding* researchOwnerBase;
static UpkeepBuilding* researchOwnerOtherBase;
static uintptr_t researchOwnerContinue;
static uintptr_t researchOwnerGrantEntry;
static IncomeGrant researchOwnerGrantOriginal;
__declspec(naked) static int __cdecl researchOwnerUnownedGrant(void*,void*,void*){__asm {push ebx
    push esi
    sub esp,0x40
    mov ebx,[esp+0x4c]
    mov eax,[esp+0x50]
    mov [esp+0x2c],eax
    mov esi,[esp+0x54]
    mov eax,-50
    push eax
    jmp dword ptr [researchOwnerGrantEntry]}}
static uint8_t* __fastcall researchOwnerPerson(void* w,void* x,int id){
    auto p=personFixtureLookup(w,x,id);return p==g_personGuide && !researchOwnerProviderRegistered?nullptr:p;
}
static void* __fastcall researchOwnerBuilding(void*,void*,int id){
    auto p=(UpkeepBuilding*)g_researchFixtureBase;return researchOwnerBaseRegistered && p && p->id==id?p:nullptr;
}
static void __cdecl researchOwnerMutate(uint8_t** city){
    if(researchOwnerFault==1)skilltelemetry::invalidateContext();
    if(researchOwnerFault==2)*(int*)(g_personGuide+0xe8)=-1;
    if(researchOwnerFault==3)*(int*)(g_personGuide+0xe8)=*(int*)(g_personGuide+0xe8)==84?249:84;
    if(researchOwnerFault==4)++*(int*)(g_personGuide+0x98);
    if(researchOwnerFault==5)++*(int*)(g_personGuide+0x9c);
    if(researchOwnerFault==6)researchOwnerProviderRegistered=false;
    if(researchOwnerFault==7)researchOwnerCommand[1]=g_personOther;
    if(researchOwnerFault==8)*(uint16_t*)(researchOwnerProject+0x5e)+=2;
    if(researchOwnerFault==12)g_researchFixtureBase=researchOwnerOtherBase;
    if(researchOwnerFault==13)researchOwnerBaseRegistered=false;
    if(researchOwnerFault==14)++researchOwnerBase->id;
    if(researchOwnerFault==15)++researchOwnerBase->force;
    if(researchOwnerFault==16)*city=researchOwnerOtherCity;
    if(researchOwnerFault==17)assert(researchOwnerUnownedGrant(researchOwnerCommand,*city,researchOwnerProject)==-50);
}
__declspec(naked) static void researchOwnerAfterCost(){__asm {add esp,8
    pushfd
    pushad
    lea eax,[esp+0x50]
    push eax
    call researchOwnerMutate
    add esp,4
    popad
    popfd
    cmp researchOwnerFault,9
    jne sameProject
    mov esi,researchOwnerOtherProject
sameProject:
    neg eax
    push eax
    jmp dword ptr [researchOwnerContinue]}}
static int __fastcall researchOwnerGrant(void* self,void*,void* city,int requested){
    if(researchOwnerFault==10)return 0;
    int result=researchOwnerGrantOriginal(self,city,requested);
    if(researchOwnerFault==11)skilltelemetry::invalidateContext();return result;
}
static void researchOwnerTelemetryTests(uint8_t* actor,uint8_t* other,uint8_t* city,uint8_t* project,void** command,UpkeepBuilding& base){
    uint8_t otherCity[0x100]={},otherProject[0x70]={};UpkeepBuilding otherBase=base;otherBase.id=9;
    researchOwnerBase=&base;researchOwnerOtherBase=&otherBase;researchOwnerOtherCity=otherCity;
    researchOwnerProject=project;researchOwnerOtherProject=otherProject;researchOwnerCommand=command;
    researchOwnerContinue=site(0x5d8f38);researchOwnerGrantEntry=site(0x5d8f38);
    uint8_t original[6];memcpy(original,(void*)site(0x5d8f32),6);
    domesticStub(0x5d8f32,researchOwnerAfterCost);
    researchOwnerGrantOriginal=g_researchGoldGrantOriginal;g_researchGoldGrantOriginal=(IncomeGrant)researchOwnerGrant;
    auto setup=[&](int owned,int fault){
        researchOwnerFault=fault;researchOwnerProviderRegistered=researchOwnerBaseRegistered=true;
        g_researchFixtureBase=&base;base.id=8;base.force=2;
        command[1]=actor;*(int*)(actor+0xe8)=owned;*(int*)(other+0xe8)=-1;
        memset(actor+0x98,0,16);g_skills[249].effects[84]=true;
        *(uint16_t*)(project+0x5e)=*(uint16_t*)(otherProject+0x5e)=101;
        *(int*)(city+0x44)=*(int*)(otherCity+0x44)=1000;
    };
    assert(skilltelemetry::open(L"build/research_owner_edges_test.csv",personFixtureActor,nullptr));
    for(int owned:{84,249})for(int fault=0;fault<18;++fault){
        setup(owned,fault);int result=runResearchGold(command,city,project);
        assert(result==(fault==10?0:-50));
        assert(*(int*)(city+0x44)==(fault==10 || fault==16?1000:fault==17?900:950));
        assert(*(int*)(otherCity+0x44)==(fault==16?950:1000));
        assert(!g_researchGoldPending.actor && !g_progressionProbe);
    }
    for(int owned:{84,249}){
        setup(owned,0);assert(researchGoldCost(command+1,project)==50);
        assert(researchGoldGrant(nullptr,nullptr,city,-50)==-50 && *(int*)(city+0x44)==950);
    }
    skilltelemetry::close();setup(249,0);assert(runResearchGold(command,city,project)==-50 && *(int*)(city+0x44)==950);
    memcpy((void*)site(0x5d8f32),original,6);g_researchGoldGrantOriginal=researchOwnerGrantOriginal;
    researchOwnerFault=0;researchOwnerProviderRegistered=researchOwnerBaseRegistered=true;
    puts("PASS research ownership: native cost/debit, raw/composed provider, frame/member/project/city binding, stale/no-store exclusions and disabled parity");
}
