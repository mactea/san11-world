// Real command cost/commit sequences and the native two-worker order loop.
// World lookups, order cleanup/notifications and post-calculation faults are adapters.
static int domesticOwnerPath=0,domesticOwnerFault=0,domesticOwnerRequested=0;
static bool domesticOwnerRegistered=true;
static void** domesticOwnerMembers=nullptr;
static void* domesticOwnerCommand[4]={};
static uintptr_t domesticOwnerEntries[3],domesticOwnerQuantityContinue,domesticOwnerRecruitContinue,domesticOwnerTimeContinue;
static ProductionAmount domesticOwnerAmountNative;
static RecruitAmount domesticOwnerRecruitNative;
static ProductionTime domesticOwnerTimeNative;
static WeaponGrant domesticOwnerWeaponNative;
static IncomeGrant domesticOwnerTroopsNative;
static AssignTime domesticOwnerAssignNative;
static uint8_t* __fastcall domesticOwnerLookup(void*,void*,int id){
    return id==1500?(domesticOwnerRegistered?g_upkeepActor:nullptr):id==1501?g_secondIncomeActor:nullptr;
}
static int __cdecl domesticOwnerAmount(void* city,void** members,int weapon){
    domesticOwnerMembers=members;return domesticOwnerAmountNative(city,members,weapon);
}
static int __cdecl domesticOwnerRecruit(void* base,void** members){
    domesticOwnerMembers=members;return domesticOwnerRecruitNative(base,members);
}
static int __cdecl domesticOwnerTime(void** members,int weapon){
    domesticOwnerMembers=members;return domesticOwnerTimeNative(members,weapon);
}
static void __cdecl domesticOwnerMutate(){
    if(domesticOwnerFault==1)skilltelemetry::invalidateContext();
    if(domesticOwnerFault==2)*(int*)(g_upkeepActor+0xe8)=-1;
    if(domesticOwnerFault==3)domesticOwnerRegistered=false;
    if(domesticOwnerFault==4)domesticOwnerMembers[1]=nullptr;
    if(domesticOwnerFault==5)++g_domesticBase->id;
    if(domesticOwnerFault==6)++g_domesticBase->force;
    if(domesticOwnerFault==9)++*(int*)(g_upkeepActor+0x9c);
}
static int __fastcall domesticOwnerWeapon(void* self,void*,void* city,int weapon,int n){
    if(domesticOwnerFault==7)return 0;int result=domesticOwnerWeaponNative(self,city,weapon,n);
    if(domesticOwnerFault==8)skilltelemetry::invalidateContext();return result;
}
static int __fastcall domesticOwnerTroops(void* self,void*,void* base,int n){
    if(domesticOwnerFault==7)return 0;int result=domesticOwnerTroopsNative(self,base,n);
    if(domesticOwnerFault==8)skilltelemetry::invalidateContext();return result;
}
static void __fastcall domesticOwnerAssign(void* self,void*,void* person,int n){
    if(domesticOwnerFault==7)return;domesticOwnerAssignNative(self,person,n);
    if(domesticOwnerFault==8)skilltelemetry::invalidateContext();
}
static void __fastcall domesticOwnerNotify(void*,void*){}
static void __fastcall domesticOwnerPersonNotify(void*,void*,void*){}
static void __cdecl domesticOwnerBit(void*,int,int){}
static int __fastcall domesticOwnerBaseId(void*,void*,void*){return 8;}
__declspec(naked) static int __cdecl domesticOwnerRun(void*,void**,int){__asm {push ebx
    push esi
    push edi
    sub esp,0x60
    mov ebx,[esp+0x70]
    mov edx,[esp+0x74]
    mov eax,[esp+0x78]
    cmp domesticOwnerPath,2
    je timed
    cmp domesticOwnerPath,1
    je recruited
    mov [esp+0x18],eax
    mov [esp+0x1c],edx
    jmp dword ptr [domesticOwnerEntries]
recruited:
    push edx
    push g_domesticBase
    jmp dword ptr [domesticOwnerEntries+4]
timed:
    lea edi,domesticOwnerCommand
    mov ebx,eax
    mov eax,[edx]
    mov [esp+0x0c],eax
    jmp dword ptr [domesticOwnerEntries+8]}}
__declspec(naked) static void domesticOwnerDone(){__asm {add esp,0x60
    pop edi
    pop esi
    pop ebx
    ret}}
#define DOMESTIC_OWNER_MUTATION \
    __asm {mov domesticOwnerRequested,eax} __asm {pushfd} __asm {pushad} \
    __asm {call domesticOwnerMutate} __asm {popad} __asm {popfd}
#define DOMESTIC_OWNER_REQUEST \
    __asm {cmp domesticOwnerFault,10} __asm {jne sameRequest} __asm {dec eax} __asm {sameRequest:}
__declspec(naked) static void domesticOwnerQuantityReady(){__asm {add esp,12}
    DOMESTIC_OWNER_MUTATION
    DOMESTIC_OWNER_REQUEST
    __asm {push eax
    push esi
    jmp dword ptr [domesticOwnerQuantityContinue]}}
__declspec(naked) static void domesticOwnerRecruitReady(){__asm {add esp,8}
    DOMESTIC_OWNER_MUTATION
    DOMESTIC_OWNER_REQUEST
    __asm {push eax
    mov ecx,ebx
    jmp dword ptr [domesticOwnerRecruitContinue]}}
__declspec(naked) static void domesticOwnerTimeReady(){__asm {mov ecx,[esp+0x14]
    add esp,8}
    DOMESTIC_OWNER_MUTATION
    DOMESTIC_OWNER_REQUEST
    __asm {jmp dword ptr [domesticOwnerTimeContinue]}}
#undef DOMESTIC_OWNER_MUTATION
#undef DOMESTIC_OWNER_REQUEST
static void domesticOwnerTelemetryTests(uint8_t* person,uint8_t* second,UpkeepBuilding& base){
    auto oldOrderStore=g_boleStoreOriginal;g_boleStoreOriginal=(BoleStore)site(0x489bd0);
    uint32_t leaves[]={0x47c4c0,0x491310,0x491770,0x4a57b0,0x482f80,0x4a06a0,0x4b9480,0x472520};
    uint8_t savedLeaves[8][5];for(int n=0;n<8;++n)memcpy(savedLeaves[n],(void*)site(leaves[n]),5);
    uint32_t sites[]={0x5c6715,0x5c3b93,0x5c7054,0x5c6725,0x5c3ba9,0x5c70eb,0x5c70c8};int lengths[]={5,6,7,5,5,5,1};
    uint8_t saved[7][8];for(int n=0;n<7;++n)memcpy(saved[n],(void*)site(sites[n]),lengths[n]);
    domesticOwnerEntries[0]=site(0x5c6705);domesticOwnerEntries[1]=site(0x5c3b8e);domesticOwnerEntries[2]=site(0x5c7030);
    domesticOwnerQuantityContinue=site(0x5c671a);domesticOwnerRecruitContinue=site(0x5c3b99);domesticOwnerTimeContinue=site(0x5c705b);
    domesticStub(0x5c6715,domesticOwnerQuantityReady);domesticStub(0x5c3b93,domesticOwnerRecruitReady);domesticStub(0x5c7054,domesticOwnerTimeReady);
    for(uint32_t at:{0x5c6725u,0x5c3ba9u,0x5c70ebu})domesticStub(at,domesticOwnerDone);
    *(uint8_t*)site(0x5c70c8)=0x6a;
    domesticStub(0x47c4c0,domesticBuilding);domesticStub(0x491310,domesticPersonId);domesticStub(0x491770,domesticOwnerBaseId);
    domesticStub(0x4a57b0,domesticOwnerPersonNotify);domesticStub(0x482f80,domesticOwnerPersonNotify);
    domesticStub(0x4a06a0,domesticOwnerNotify);domesticStub(0x4b9480,domesticOwnerNotify);domesticStub(0x472520,domesticOwnerBit);
    domesticOwnerAmountNative=g_productionAmountOriginal;domesticOwnerRecruitNative=g_recruitAmountOriginal;domesticOwnerTimeNative=g_productionTimeOriginal;
    domesticOwnerWeaponNative=g_weaponGrantOriginal;domesticOwnerTroopsNative=g_recruitGrantOriginal;domesticOwnerAssignNative=g_assignTimeOriginal;
    g_productionAmountOriginal=domesticOwnerAmount;g_recruitAmountOriginal=domesticOwnerRecruit;g_productionTimeOriginal=domesticOwnerTime;
    g_weaponGrantOriginal=(WeaponGrant)domesticOwnerWeapon;g_recruitGrantOriginal=(IncomeGrant)domesticOwnerTroops;g_assignTimeOriginal=(AssignTime)domesticOwnerAssign;
    void* members[3]={};auto setup=[&](int effect,int owned,int fault){
        domesticOwnerPath=effect==79?1:effect>=82?2:0;domesticOwnerFault=fault;domesticOwnerRegistered=true;
        base.id=8;base.force=2;members[0]=person;members[1]=second;members[2]=nullptr;
        domesticOwnerCommand[0]=&base;domesticOwnerCommand[1]=person;domesticOwnerCommand[2]=second;domesticOwnerCommand[3]=nullptr;
        for(auto p:{person,second}){memset(p+0x98,0,16);*(int*)(p+0x98)=8;memset(p+0x13c,0,0x20);}
        *(int*)(person+0xe8)=owned;*(int*)(second+0xe8)=-1;g_domesticTroops=1000;
        for(int weapon:{1,4})*(int*)(g_domesticCity+0x4c+weapon*4)=1000;
        for(int skill=79;skill<=83;++skill)g_skills[249].effects[skill]=true;
    };
    auto weapon=[](int effect){return effect==79?-1:effect==80?1:effect==81?4:effect==82?5:10;};
    assert(skilltelemetry::open(L"build/domestic_owner_edges_test.csv",economyActor,nullptr));
    for(int effect=79;effect<=83;++effect)for(int owned:{effect,249})for(int fault=0;fault<11;++fault){
        setup(effect,owned,fault);domesticOwnerRun(g_domesticCity,members,weapon(effect));
        assert(domesticOwnerRequested==(effect==79?1866:effect<=81?2774:4));
        if(effect==79)assert(g_domesticTroops==(fault==7?1000:fault==10?2865:2866));
        else if(effect<=81)assert(*(int*)(g_domesticCity+0x4c+weapon(effect)*4)==(fault==7?1000:fault==10?3773:3774));
        else assert(person[0x158]==(fault==7?0:fault==10?3:4) && second[0x158]==(fault==7?0:fault==10?3:4));
    }
    for(int effect=79;effect<=83;++effect)for(int owned:{effect,249}){
        setup(effect,owned,0);int kind=weapon(effect);
        if(effect==79){int n=recruitAmount(&base,members);recruitGrant(nullptr,nullptr,&base,n);}
        else if(effect<=81){int n=productionAmount(g_domesticCity,members,kind);productionGrant(nullptr,nullptr,g_domesticCity,kind,n);}
        else {int n=productionTime(members,kind);*(int*)(person+0x13c)=2;*(int*)(person+0x140)=8;*(int*)(person+0x144)=kind;
            productionAssignTime(nullptr,nullptr,person,n);}
    }
    skilltelemetry::close();for(int effect=79;effect<=83;++effect){setup(effect,249,0);domesticOwnerRun(g_domesticCity,members,weapon(effect));
        assert(domesticOwnerRequested==(effect==79?1866:effect<=81?2774:4));
        if(effect==79)assert(g_domesticTroops==2866);
        else if(effect<=81)assert(*(int*)(g_domesticCity+0x4c+weapon(effect)*4)==3774);
        else assert(person[0x158]==4 && second[0x158]==4);
    }
    for(int n=0;n<7;++n)memcpy((void*)site(sites[n]),saved[n],lengths[n]);
    for(int n=0;n<8;++n)memcpy((void*)site(leaves[n]),savedLeaves[n],5);
    g_productionAmountOriginal=domesticOwnerAmountNative;g_recruitAmountOriginal=domesticOwnerRecruitNative;g_productionTimeOriginal=domesticOwnerTimeNative;
    g_weaponGrantOriginal=domesticOwnerWeaponNative;g_recruitGrantOriginal=domesticOwnerTroopsNative;g_assignTimeOriginal=domesticOwnerAssignNative;
    domesticOwnerRegistered=true;domesticOwnerFault=0;g_domesticQuantity=DomesticPending{};g_domesticTime=DomesticPending{};
    g_boleStoreOriginal=oldOrderStore;
    puts("PASS domestic ownership:120 native parent/control inputs across79..83; real order/timer stores and worker loop; identity/member/world/city and failed-store controls");
}
