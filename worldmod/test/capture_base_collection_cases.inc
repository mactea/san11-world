// Native affiliation/status scan, force filter/comparator and first19 loop.
// Pointer storage, list allocation/copy and virtual force-field access are adapters.
typedef int(__thiscall* CaptureNativeRegion)(void*,void*,int,int);
static CaptureNativeRegion g_captureNativeRegion;
static int g_captureNativeHighId=14162,g_captureNativeLookups=0;
static bool g_captureNativeFirstPresent=true;
static uint8_t* __fastcall captureNativePerson(void*,void*,int id){
    if((uintptr_t)_ReturnAddress()==site(0x4cf31c)){assert(id==g_captureNativeLookups);++g_captureNativeLookups;}
    if(id==600)return g_captureNativeFirstPresent?g_personGuide:nullptr;
    if(id==g_captureNativeHighId)return g_personOther;
    return id==1000?g_personTarget:id==601?g_personSpouse:nullptr;
}
static int __fastcall captureNativePersonId(void* world,void* unused,const void* person){
    return person==g_personOther?g_captureNativeHighId:captureFixturePersonId(world,unused,person);
}
static bool captureNativeActor(const void* person,skilltelemetry::Actor& actor){
    afterFixtureActor(person,actor);if(person==g_personOther)actor.person=g_captureNativeHighId;return true;
}
static int __fastcall captureNativeRegion(void* self,void* unused,uint8_t* list,int place,int mask){
    if((uintptr_t)_ReturnAddress()==site(0x4b152c))return g_captureNativeRegion(self,list,place,mask);
    captureFixtureRegion(self,unused,list,place,mask);return *(int*)(list+0xc);
}
static int __fastcall captureNativeOwner(void* person,void*){
    return person==g_personGuide || person==g_personOther?*(int*)((uint8_t*)person+0x30):captureBaseFixtureOwner(person,nullptr);
}
static int __fastcall captureNativeForceField(void* person,void*,int field){
    // The force comparison operand is already on the native stack; only the
    // field ID belongs to this virtual getter (RET4, then comparator RET12).
    assert(field==75);return captureNativeOwner(person,nullptr);
}
static void* __fastcall captureNativeCopy(uint8_t* list,void*,uint8_t* source){
    for(auto node=*(CaptureFixtureNode**)(source+4);node;node=node->next)captureFixtureAppend(list,nullptr,node->person);
    return list;
}
template<class Prepare>
static void captureBaseNativeCollectionTests(Defeat run,Prepare prepare,uint8_t* source,uint8_t* target,void** vt,const uint8_t* prefix){
    assert(!memcmp(prefix,"\x53\x8b\x5c\x24\x08",5));
    auto code=(uint8_t*)VirtualAlloc(nullptr,10,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(code);
    memcpy(code,prefix,5);code[5]=0xe9;*(int32_t*)(code+6)=(int32_t)(site(0x4cf2f5)-(uintptr_t)code-10);
    g_captureNativeRegion=(CaptureNativeRegion)code;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;
    g_telemetryPerson=(PersonById)captureNativePerson;g_telemetryPersonId=(BuildingId)captureNativePersonId;
    auto oldField=vt[0x10/4],oldOwner=vt[0x40/4];vt[0x10/4]=captureNativeForceField;vt[0x40/4]=captureNativeOwner;
    uint32_t addresses[]={0x490b00,0x4cf2f0,0x49f820};uint8_t saved[3][5];
    for(int n=0;n<3;++n)memcpy(saved[n],(void*)site(addresses[n]),5);
    domesticStub(0x490b00,captureNativePerson);domesticStub(0x4cf2f0,captureNativeRegion);domesticStub(0x49f820,captureNativeCopy);
    auto statusTable=(uint32_t*)site(0x488b3d);uint32_t oldStatusTable=*statusTable,statusJumps[10];
    for(int n=0;n<10;++n)statusJumps[n]=(uint32_t)site(*(uint32_t*)site(0x488b9c+4*n));*statusTable=(uint32_t)statusJumps;
    auto compareTable=(uint32_t*)site(0x658fbc);uint32_t oldCompareTable=*compareTable,compareJumps[6];
    for(int n=0;n<6;++n)compareJumps[n]=(uint32_t)site(*(uint32_t*)site(0x659034+4*n));*compareTable=(uint32_t)compareJumps;
    auto count=(int*)site(0x4cf34e);int oldCount=*count;assert(oldCount==1100);*count=14613;
    auto cityBound=(uint8_t*)site(0x488358);uint8_t oldCityBound[5];memcpy(oldCityBound,cityBound,5);
    assert(!memcmp(cityBound,"\x83\xf8\x29\x7e\x33",5));
    auto cityCode=(uint8_t*)VirtualAlloc(nullptr,16,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(cityCode);
    const uint8_t cityTemplate[]={0x3d,0,0,0,0,0x0f,0x8e,0,0,0,0,0xe9,0,0,0,0};
    memcpy(cityCode,cityTemplate,16);*(int*)(cityCode+1)=529;
    *(int32_t*)(cityCode+7)=(int32_t)(site(0x488390)-(uintptr_t)cityCode-11);
    *(int32_t*)(cityCode+12)=(int32_t)(site(0x48835d)-(uintptr_t)cityCode-16);
    assert(skilltelemetry::open(L"build/capture_base_collection_test.csv",captureNativeActor,nullptr));
    int inputs=0;
    for(int variant=0;variant<5;++variant)for(int high:{14162,14612})for(int profile=0;profile<14;++profile){
        g_captureBaseId=variant<3?(variant==0?23:variant==1?42:52):529;
        bool expanded=variant==4;memcpy(cityBound,oldCityBound,5);
        if(expanded){cityBound[0]=0xe9;*(int32_t*)(cityBound+1)=(int32_t)((uintptr_t)cityCode-(uintptr_t)cityBound-5);}
        g_captureNativeHighId=high;g_captureNativeFirstPresent=profile!=8;g_captureNativeLookups=0;
        prepare(0,1,1,1,4);
        *(int*)(g_personGuide+0xe8)=profile==0 || profile==2?-1:profile==3?249:19;
        *(int*)(g_personOther+0xe8)=profile==0 || profile==1 || profile==9?-1:19;
        *(int*)(g_personGuide+0xa0)=profile==4?5:profile==9?-1:profile==10?4:1;
        *(int*)(g_personOther+0xa0)=3;
        if(profile==5)*(int*)(g_personGuide+0x9c)=99;
        if(profile==6)*(int*)(g_personGuide+0x98)=9999;
        if(profile==12)*(int*)(g_personGuide+0x9c)=*(int*)(g_personOther+0x9c)=-1;
        *(int*)(g_personGuide+0x30)=profile==7 || profile==11?3:2;
        *(int*)(g_personOther+0x30)=profile==11?3:2;
        bool inRange=variant!=3,empty=!inRange || profile==11 || profile==12;
        bool eligible=inRange && profile!=0 && profile!=9 && profile!=11 && profile!=12;
        uint32_t state=empty?4u:4u*0x6c078965u+0x3039u;
        run(nullptr,target,source,0,0,0);
        assert(g_captureNativeLookups==(inRange?14613:0));
        assert(g_defenseSeed==state && g_captureFixtureCandidates==(eligible?1:0));
        assert(*(int*)(g_personTarget+0xa0)==(eligible?5:3) && !g_defeat && !g_captureAttempt && defeatUnitCleared(target));++inputs;
    }
    skilltelemetry::close();assertBattleAppliedRows(L"build/capture_base_collection_test.csv",240);
    memcpy(cityBound,oldCityBound,5);*count=oldCount;*statusTable=oldStatusTable;*compareTable=oldCompareTable;
    for(int n=0;n<3;++n)memcpy((void*)site(addresses[n]),saved[n],5);
    vt[0x10/4]=oldField;vt[0x40/4]=oldOwner;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;
    VirtualFree(code,0,MEM_RELEASE);VirtualFree(cityCode,0,MEM_RELEASE);g_captureNativeRegion=nullptr;
    printf("PASS base capture native collection: %d inputs, full14613-slot scan, native status/force filtering/first holder, IDs14162/14612 and installed M7 city529 conversion\n",inputs);
}
