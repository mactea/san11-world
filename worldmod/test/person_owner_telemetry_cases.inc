// Native grants/getters/cache stores and loyalty branches execute. Controlled
// leaves mutate identity after the native effect, before telemetry consumes it.
static int personOwnerFault=0;
static uint8_t* personOwnerProvider=nullptr;
static bool personOwnerProviderRegistered=true;
static AbilityGrowth personOwnerAbilityOriginal;
static WeaponGrowth personOwnerWeaponOriginal;
typedef uint8_t(__thiscall* PersonOwnerGetter)(void*,int,int);
static PersonOwnerGetter personOwnerGetterOriginal;
static uintptr_t personOwnerCacheEntry;
static uint8_t* __fastcall personOwnerLookup(void* w,void* x,int id){
    auto p=personFixtureLookup(w,x,id);return p==personOwnerProvider && !personOwnerProviderRegistered?nullptr:p;
}
static void personOwnerMutate(){
    if(personOwnerFault==1)skilltelemetry::invalidateContext();
    if(personOwnerFault==2)*(int*)(personOwnerProvider+0xe8)=-1;
    if(personOwnerFault==3)++*(int*)(personOwnerProvider+0x98);
    if(personOwnerFault==4)++*(int*)(personOwnerProvider+0x9c);
    if(personOwnerFault==5)personOwnerProviderRegistered=false;
    if(personOwnerFault==6)g_personRegistered=false;
    if(personOwnerFault==7)++*(int*)(g_personTarget+0x98);
    if(personOwnerFault==8)++*(int*)(g_personTarget+0x9c);
    if(personOwnerFault==9)g_skills[249].effects[84]=g_skills[249].effects[96]=false;
    if(personOwnerFault==10)*(int*)(g_personTarget+0x60)=-1;
}
static int __fastcall personOwnerAbility(void* self,void*,uint8_t* p,int k,int n,int refresh){
    int result=personOwnerAbilityOriginal(self,p,k,n,refresh);personOwnerMutate();return result;
}
static int __fastcall personOwnerWeapon(void* self,void*,uint8_t* p,int k,int n){
    int result=personOwnerWeaponOriginal(self,p,k,n);personOwnerMutate();return result;
}
static uint8_t __fastcall personOwnerGetter(void* self,void*,int k,int option){
    uint8_t result=personOwnerGetterOriginal(self,k,option);personOwnerMutate();return result;
}
__declspec(naked) static void __cdecl personOwnerCache(void*,int){__asm {push esi
    push edi
    mov edi,[esp+12]
    mov eax,[esp+16]
    neg eax
    xor esi,esi
    mov ecx,edi
    push eax
    push 0
    jmp dword ptr [personOwnerCacheEntry]}}
__declspec(naked) static void personOwnerCurrentDone(){__asm {add esp,4
    pop edi
    pop esi
    ret}}
__declspec(naked) static void personOwnerBaseDone(){__asm {pop edi
    pop esi
    ret}}
static void personOwnerTelemetryTests(uint8_t* target,uint8_t* guide,uint8_t* other,uint8_t* spouse,void** vt,void* city){
    auto oldPerson=g_telemetryPerson;auto oldAbility=g_abilityGrowthOriginal;auto oldWeapon=g_weaponGrowthOriginal;
    auto oldCurrent=g_spouseCurrentResume,oldBase=g_spouseBaseResume;
    personOwnerAbilityOriginal=oldAbility;personOwnerWeaponOriginal=oldWeapon;
    personOwnerGetterOriginal=(PersonOwnerGetter)vt[0x4c/4];
    g_telemetryPerson=(PersonById)personOwnerLookup;
    auto setup=[&](uint8_t* provider,int owned,int fault){
        personOwnerProvider=provider;personOwnerFault=fault;personOwnerProviderRegistered=g_personRegistered=true;
        g_skills[249].effects[84]=g_skills[249].effects[96]=true;
        for(auto p:{target,guide,other,spouse}){memset(p+0x98,0,16);*(int*)(p+0x9c)=87;*(int*)(p+0xa0)=3;*(int*)(p+0xe8)=-1;}
        *(int*)(provider+0xe8)=owned;*(int*)(target+0x60)=601;*(int*)(spouse+0x60)=1000;
        *(void**)target=vt;
    };
    g_abilityGrowthOriginal=(AbilityGrowth)personOwnerAbility;g_weaponGrowthOriginal=(WeaponGrowth)personOwnerWeapon;
    assert(skilltelemetry::open(L"build/growth_owner_edges_test.csv",personFixtureActor,nullptr));
    for(int path=0;path<2;++path)for(int owned:{84,249})for(int fault=0;fault<10;++fault){
        if(fault==9 && owned==84)continue;setup(guide,owned,fault);
        if(path==0){*(uint16_t*)(target+0x12a)=0;
            assert(((AbilityGrowth)site(0x4a70d0))(nullptr,target,0,20,0)==40 && *(uint16_t*)(target+0x12a)==40);}
        else {*(int*)(target+0xb0)=0;target[0x134]=10;
            ((WeaponGrowth)site(0x4a6db0))(nullptr,target,0,20);assert(*(int*)(target+0xb0)==0 && target[0x134]==50);}
        assert(!g_progressionProbe);
    }
    skilltelemetry::close();g_abilityGrowthOriginal=oldAbility;g_weaponGrowthOriginal=oldWeapon;
    g_spouseCurrentResume=(uint32_t)personOwnerCurrentDone;g_spouseBaseResume=(uint32_t)personOwnerBaseDone;
    vt[0x4c/4]=(void*)personOwnerGetter;
    assert(skilltelemetry::open(L"build/spouse_owner_edges_test.csv",personFixtureActor,nullptr));
    for(int cache=0;cache<2;++cache)for(bool self:{false,true})for(int fault=0;fault<10;++fault){
        setup(self?target:spouse,99,fault==9?10:fault);memset(target+0xc8,50,5);memset(target+0x170,42,10);
        personOwnerCacheEntry=site(cache?0x48a2f3:0x48a2e2);personOwnerCache(target,cache);
        assert(target[0x170+cache*5]==(cache?51:54) && !g_spouseProbe && !g_spousePending.actor);
    }
    skilltelemetry::close();setup(spouse,99,0);memset(target+0xc8,50,5);memset(target+0x170,42,10);
    personOwnerCacheEntry=site(0x48a2e2);personOwnerCache(target,0);assert(target[0x170]==54 && !g_spousePending.actor);
    vt[0x4c/4]=(void*)personOwnerGetterOriginal;
    g_spouseCurrentResume=oldCurrent;g_spouseBaseResume=oldBase;
    assert(skilltelemetry::open(L"build/loyalty_owner_edges_test.csv",personFixtureActor,nullptr));
    for(int owned:{96,249})for(int fault=0;fault<12;++fault){
        if(fault==9 && owned==96)continue;setup(guide,owned,fault);target[0xac]=90;
        auto building=(UpkeepBuilding*)city;building->id=8;building->force=2;
        assert(runLoyaltyFind(city)==1);
        if(fault==10)++building->id;else if(fault==11)++building->force;else personOwnerMutate();
        assert(runLoyaltyCheck(city,target,1)==1 && target[0xac]==90);
    }
    ((UpkeepBuilding*)city)->id=8;((UpkeepBuilding*)city)->force=2;
    skilltelemetry::close();setup(guide,84,0);
    *(uint16_t*)(target+0x12a)=0;
    assert(((AbilityGrowth)site(0x4a70d0))(nullptr,target,0,20,0)==40 && *(uint16_t*)(target+0x12a)==40);
    *(int*)(target+0xb0)=0;target[0x134]=10;
    ((WeaponGrowth)site(0x4a6db0))(nullptr,target,0,20);assert(target[0x134]==50);
    setup(guide,96,0);target[0xac]=90;
    assert(runLoyaltyFind(city)==1 && runLoyaltyCheck(city,target,1)==1 && target[0xac]==90);
    g_telemetryPerson=oldPerson;personOwnerProvider=nullptr;personOwnerFault=0;g_loyaltyProvider=LoyaltyProvider{};
    puts("PASS person ownership: native guidance grants, spouse getter/cache stores and loyalty skips; canonical actors/recipients, context/skill/affiliation changes and disabled parity");
}
