// Execute native experience grants, teammate scan, word/byte setters, spouse
// comparison/caps/cache loop, and loyalty provider bounds and recipient filters.
static uint8_t* g_personTarget;
static uint8_t* g_personGuide;
static uint8_t* g_personOther;
static uint8_t* g_personSpouse;
static uint8_t* g_personUnit;
static bool g_personRegistered=true;
static uintptr_t g_personGetterTail,g_loyaltyFindEntry,g_loyaltyCheckEntry;
static uint32_t g_personCookie=0;
static uint8_t* __fastcall personFixtureLookup(void*,void*,int id) {
    return id==1000?(g_personRegistered?g_personTarget:nullptr):id==600?g_personGuide:
        id==14162?g_personOther:id==601?g_personSpouse:nullptr;
}
static int __fastcall personFixtureId(void*,void*,const void* p) {
    return p==g_personTarget?1000:p==g_personGuide?600:p==g_personOther?14162:p==g_personSpouse?601:-1;
}
static void* __fastcall personFixtureUnit(void*,void*,int id) {return id==0?g_personUnit:nullptr;}
static int __fastcall personFixtureOwner(void*,void*) {return 2;}
static int __fastcall personFixtureForeignOwner(void*,void*) {return 3;}
static int __fastcall personFixtureHidden(void*,void*) {return 0;}
static void __fastcall personFixtureRecalc(void*,void*) {}
static bool personFixtureActor(const void* p,skilltelemetry::Actor& actor) {
    actor.person=personFixtureId(nullptr,nullptr,p);assert(actor.person>=0);
    actor.force=2;actor.city=8;actor.ruler=635;actor.year=194;actor.month=1;actor.day=1;
    strcpy_s(actor.name,p==g_personTarget?"Target":p==g_personSpouse?"Spouse":p==g_personGuide?"Guide":"Other");return true;
}
__declspec(naked) static void personFixtureCookieCheck() {__asm {ret}}
__declspec(naked) static void personFixtureCacheDone() {__asm {pop edi
    pop esi
    ret}}
__declspec(naked) static uint8_t __cdecl personFixtureGetter() {
    __asm {
        push ecx
        push ebx
        push ebp
        push edi
        push esi
        mov edi,ecx
        mov ebp,[esp+24]
        movzx ebx,byte ptr [edi+ebp+0xc8]
        cmp dword ptr [esp+28],-1
        je baseValue
        add ebx,3
    baseValue:
        mov eax,g_personGetterTail
        jmp eax
    }
}
static int __fastcall personFixtureFind(void*,void*,int effect) {
    if(personHas(g_personGuide,nullptr,effect))return 600;
    if(personHas(g_personOther,nullptr,effect))return 14162;
    return -1;
}
__declspec(naked) static int __cdecl runLoyaltyFind(void* city) {
    __asm {
        push edi
        sub esp,0x50
        xor edi,edi
        mov ecx,[esp+0x58]
        mov edx,[ecx]
        push 96
        mov eax,g_loyaltyFindEntry
        jmp eax
    }
}
__declspec(naked) static void personFixtureLoyaltyFound() {
    __asm {mov eax,[esp+0x10]
        add esp,0x50
        pop edi
        ret}
}
__declspec(naked) static int __cdecl runLoyaltyCheck(void* city,void* person,int flag) {
    __asm {
        push ebx
        push edi
        push esi
        sub esp,0x20
        mov ebx,[esp+0x30]
        mov esi,[esp+0x34]
        mov eax,[esp+0x38]
        mov [esp+0x10],eax
        xor edi,edi
        mov eax,g_loyaltyCheckEntry
        jmp eax
    }
}
__declspec(naked) static void personFixtureLoyaltySkipped() {
    __asm {mov eax,1
        add esp,0x20
        pop esi
        pop edi
        pop ebx
        ret}
}
__declspec(naked) static void personFixtureLoyaltyDecay() {
    __asm {xor eax,eax
        add esp,0x20
        pop esi
        pop edi
        pop ebx
        ret}
}
#include "person_owner_telemetry_cases.inc"
static void personTelemetryTests() {
    uint8_t target[400]={},guide[400]={},other[400]={},spouse[400]={},unit[0xf4]={};
    g_personTarget=target;g_personGuide=guide;g_personOther=other;g_personSpouse=spouse;g_personUnit=unit;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldValid=g_growthValid;
    auto oldAbilityResume=g_guidanceAbilityResume,oldWeaponResume=g_guidanceWeaponResume;
    auto oldSpouseNative=g_spouseNativeResume,oldSpouseCurrent=g_spouseCurrentResume,oldSpouseBase=g_spouseBaseResume;
    auto oldLoyaltyFind=g_loyaltyFindResume,oldLoyaltySkip=g_loyaltySkip,oldLoyaltyDecay=g_loyaltyDecay;
    auto oldBaseId=g_telemetryBaseId;auto oldThresholds=g_weaponExperienceThresholds;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)personFixtureId;g_growthValid=domesticValid;
    g_guidanceAbilityResume=(uint32_t)site(0x4a70d6);g_guidanceWeaponResume=(uint32_t)site(0x4a6db6);
    g_spouseNativeResume=(uint32_t)site(0x48a279);g_spouseCurrentResume=(uint32_t)site(0x48a2ee);g_spouseBaseResume=(uint32_t)site(0x48a2fd);
    g_loyaltyFindResume=(uint32_t)site(0x58e5d1);g_loyaltySkip=(uint32_t)site(0x58e7bb);g_loyaltyDecay=(uint32_t)site(0x58e626);
    int thresholds[]={150,200,250};g_weaponExperienceThresholds=thresholds;
    auto nativeThresholds=(uint32_t*)site(0x4a6e40);uint32_t oldNativeThresholds=*nativeThresholds;*nativeThresholds=(uint32_t)thresholds;
    auto abilityCookie=(uint32_t*)site(0x4a70d7);auto weaponCookie=(uint32_t*)site(0x4a6db7);
    uint32_t oldAbilityCookie=*abilityCookie,oldWeaponCookie=*weaponCookie;
    *abilityCookie=*weaponCookie=(uint32_t)&g_personCookie;
    domesticStub(0x707b1a,personFixtureCookieCheck);domesticStub(0x48a303,personFixtureCacheDone);
    domesticStub(0x47a630,domesticValid);domesticStub(0x490b00,personFixtureLookup);
    domesticStub(0x490e70,personFixtureUnit);domesticStub(0x496dd0,personFixtureRecalc);
    g_personGetterTail=site(0x48a256);
    void* vt[24]={};vt[0x40/4]=personFixtureOwner;vt[0x48/4]=personFixtureHidden;vt[0x4c/4]=personFixtureGetter;
    for(auto p:{target,guide,other,spouse}) {
        *(void**)p=vt;*(int*)(p+0x60)=-1;*(int*)(p+0x9c)=87;*(int*)(p+0xa0)=3;*(int*)(p+0xe8)=-1;
        memset(p+0xc8,50,5);
    }
    *(int*)(unit+0xc)=1000;*(int*)(unit+0x10)=600;*(int*)(unit+0x14)=14162;
    bool old84=g_skills[249].effects[84],old99=g_skills[249].effects[99],old96=g_skills[249].effects[96];
    g_skills[249].effects[84]=g_skills[249].effects[99]=g_skills[249].effects[96]=true;
    auto ability=(AbilityGrowth)site(0x4a70d0);auto weapon=(WeaponGrowth)site(0x4a6db0);
    *(int*)(guide+0xe8)=84;assert(ability(nullptr,target,0,20,0)==40 && *(uint16_t*)(target+0x12a)==40);
    assert(skilltelemetry::open(L"build/growth_telemetry_test.csv",personFixtureActor,nullptr));
    for(int mode=0;mode<4;++mode) {
        *(int*)(guide+0xe8)=mode==2?-1:mode==1?249:84;*(int*)(other+0xe8)=mode>=2?84:-1;
        for(int kind=0;kind<5;++kind)for(int before:{0,2990,3000}) {
            *(uint16_t*)(target+0x12a+kind*2)=(uint16_t)before;
            int gain=ability(nullptr,target,kind,20,0);
            assert(gain==(before==0?40:3000-before) && *(uint16_t*)(target+0x12a+kind*2)==before+gain);
        }
        struct WeaponCase {int level,progress,request,afterLevel,afterProgress;};
        WeaponCase cases[]={{0,10,20,0,50},{0,140,10,1,10},{1,190,10,2,10},
                            {2,200,30,3,0},{2,249,10,3,0},{0,10,400,1,255}};
        for(int kind=0;kind<6;++kind)for(auto c:cases) {
            *(int*)(target+0xb0+kind*4)=c.level;target[0x134+kind]=(uint8_t)c.progress;
            weapon(nullptr,target,kind,c.request);
            assert(*(int*)(target+0xb0+kind*4)==c.afterLevel && target[0x134+kind]==c.afterProgress);
        }
    }
    ((int(__cdecl*)(void*))site(0x4a54a0))(target); // predicate-only preview, never an experience grant
    *(int*)(guide+0xe8)=*(int*)(other+0xe8)=-1;*(int*)(target+0xe8)=84;
    *(uint16_t*)(target+0x12a)=0;assert(ability(nullptr,target,0,20,0)==20); // self cannot teach itself
    *(int*)(guide+0xe8)=84;*(int*)(target+0xb0)=3;target[0x134]=0;
    assert(weapon(nullptr,target,0,20)==0 && target[0x134]==0);
    assert(ability(nullptr,target,5,20,0)==0 && weapon(nullptr,target,6,20)==0);
    assert(ability(nullptr,nullptr,0,20,0)==0);
    g_personRegistered=false;*(uint16_t*)(target+0x12a)=0;
    assert(ability(nullptr,target,0,20,0)==40);g_personRegistered=true; // temporary recipient has no applied row
    skilltelemetry::close();assert(!g_progressionProbe);
    // Execute native no-guidance outcomes independently of observer arithmetic.
    *(int*)(guide+0xe8)=*(int*)(other+0xe8)=-1;
    struct Baseline {int level,progress,request,afterLevel,afterProgress;};
    Baseline baselines[]={{0,10,20,0,30},{0,140,10,1,0},{1,190,10,2,0},
                          {2,200,30,2,230},{2,249,10,3,0},{0,10,400,1,255}};
    for(auto c:baselines) {
        *(int*)(target+0xb0)=c.level;target[0x134]=(uint8_t)c.progress;
        weapon(nullptr,target,0,c.request);
        assert(*(int*)(target+0xb0)==c.afterLevel && target[0x134]==c.afterProgress);
    }

    assert(skilltelemetry::open(L"build/spouse_telemetry_test.csv",personFixtureActor,nullptr));
    auto recalc=(void(__thiscall*)(void*))site(0x48a2d0);
    *(int*)(target+0x60)=601;*(int*)(spouse+0x60)=1000;
    for(int mode=0;mode<5;++mode)for(int base:{0,50,100}) {
        *(int*)(target+0xe8)=mode==1 || mode==2?99:mode==3?249:-1;
        *(int*)(spouse+0xe8)=mode==0 || mode==2?99:mode==4?249:-1;
        memset(target+0xc8,base,5);memset(target+0x170,42,10);recalc(target);
        for(int kind=0;kind<5;++kind) {
            int bonus=mode<3?1:0;int current=base+3+bonus,ordinary=base+bonus;
            assert(target[0x170+kind]==(current>100?100:current));
            assert(target[0x175+kind]==(ordinary<1?1:ordinary>100?100:ordinary));
        }
    }
    *(int*)(spouse+0xe8)=99;*(int*)(target+0xe8)=-1;memset(target+0xc8,50,5);
    ((uint8_t(__thiscall*)(void*,int,int))personFixtureGetter)(target,0,0); // shared getter preview
    *(int*)(target+0x60)=-1;recalc(target);assert(target[0x170]==53 && target[0x175]==50);
    *(int*)(target+0x60)=601;g_personRegistered=false;recalc(target);g_personRegistered=true;
    skilltelemetry::close();assert(!g_spouseProbe && !g_spousePending.actor);

    *(int*)(target+0xe8)=-1;*(int*)(target+0xa0)=3;
    void* cityVt[24]={};cityVt[0x40/4]=upkeepForce;cityVt[0x5c/4]=personFixtureFind;
    UpkeepBuilding city={cityVt,0,0,0,0,2,8,0,0,0},otherCity=city;otherCity.id=9;
    g_telemetryBaseId=(BuildingId)upkeepBaseId;
    g_loyaltyFindEntry=site(0x58e5cc);g_loyaltyCheckEntry=site(0x58e602);
    domesticStub(0x58e5e6,personFixtureLoyaltyFound);domesticStub(0x58e7bb,personFixtureLoyaltySkipped);
    domesticStub(0x58e626,personFixtureLoyaltyDecay);
    assert(skilltelemetry::open(L"build/loyalty_telemetry_test.csv",personFixtureActor,nullptr));
    for(int mode=0;mode<4;++mode) {
        *(int*)(guide+0xe8)=mode==2?-1:mode==1?249:96;*(int*)(other+0xe8)=mode>=2?96:-1;
        int flag=runLoyaltyFind(&city);assert(flag==(mode!=2));
        for(int loyalty:{100,90,0}) {target[0xac]=(uint8_t)loyalty;assert(runLoyaltyCheck(&city,target,flag)==flag);}
    }
    auto nativeBound=(int*)site(0x58e5d4);int oldBound=*nativeBound;*nativeBound=14612;
    *(int*)(guide+0xe8)=-1;*(int*)(other+0xe8)=96;assert(runLoyaltyFind(&city)==1);
    for(int loyalty:{100,90,0}) {target[0xac]=(uint8_t)loyalty;assert(runLoyaltyCheck(&city,target,1)==1);}
    *(int*)(target+0xa0)=0;assert(runLoyaltyCheck(&city,target,1)==1); // ruler bypasses earlier
    *(int*)(target+0xa0)=5;assert(runLoyaltyCheck(&city,target,1)==0); // prisoners bypass aura
    *(int*)(target+0xa0)=3;*(int*)(other+0xe8)=-1;assert(runLoyaltyCheck(&city,target,1)==1);
    assert(runLoyaltyFind(&city)==0 && runLoyaltyCheck(&city,target,0)==0);
    *(int*)(guide+0xe8)=96;assert(runLoyaltyFind(&city)==1);
    assert(runLoyaltyCheck(&otherCity,target,1)==1);g_personRegistered=false;
    assert(runLoyaltyCheck(&city,target,1)==1);g_personRegistered=true;
    void* foreignVt[24];memcpy(foreignVt,vt,sizeof vt);foreignVt[0x40/4]=personFixtureForeignOwner;
    *(void**)target=foreignVt;assert(runLoyaltyCheck(&city,target,1)==1);*(void**)target=vt;
    skilltelemetry::close();assert(!g_progressionProbe);*nativeBound=oldBound;
    personOwnerTelemetryTests(target,guide,other,spouse,vt,&city);
    g_skills[249].effects[84]=old84;g_skills[249].effects[99]=old99;g_skills[249].effects[96]=old96;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_growthValid=oldValid;g_telemetryBaseId=oldBaseId;
    g_guidanceAbilityResume=oldAbilityResume;g_guidanceWeaponResume=oldWeaponResume;
    g_spouseNativeResume=oldSpouseNative;g_spouseCurrentResume=oldSpouseCurrent;g_spouseBaseResume=oldSpouseBase;
    g_loyaltyFindResume=oldLoyaltyFind;g_loyaltySkip=oldLoyaltySkip;g_loyaltyDecay=oldLoyaltyDecay;
    g_weaponExperienceThresholds=oldThresholds;*nativeThresholds=oldNativeThresholds;
    *abilityCookie=oldAbilityCookie;*weaponCookie=oldWeaponCookie;g_loyaltyProvider=LoyaltyProvider{};
    puts("PASS native person effects: guidance XP/caps/promotions, spouse raw-ID bonus/cache stores, loyalty filters and expanded provider bounds");
}
