static uint8_t* g_areaCells=nullptr;
static bool g_areaBase=false;
static int g_areaNeighbors=1,g_areaBuilds=0,g_areaSettles=0,g_areaInvalid=-1;
static uintptr_t g_areaPreviewEntry=0;
static int __fastcall areaFixtureUnitId(void*,void*,void* p){return p==g_afterSource?4095:p==g_afterTarget && !g_areaBase?4094:-1;}
static void* __fastcall areaFixtureUnit(void*,void*,int id){
    return id==4095 && g_afterSourceRegistered?g_afterSource:id==4094 && !g_areaBase && g_afterTargetRegistered?g_afterTarget:nullptr;
}
static int __fastcall areaFixtureBaseId(void*,void*,void* p){return p==g_afterTarget && g_areaBase?529:-1;}
static void* __fastcall areaFixtureBase(void*,void*,int id){return id==529 && g_areaBase && g_afterTargetRegistered?g_afterTarget:nullptr;}
static void* __fastcall areaFixtureNativeUnit(void*,void*,int id){assert(id==4095);return g_afterSource;}
static int __fastcall areaFixtureType(void* p,void*){return p==g_afterTarget && g_areaBase?5:11;}
static void* __fastcall areaFixtureCast(void* p,void*,int type){return type==areaFixtureType(p,nullptr)?p:nullptr;}
static void* __fastcall areaFixtureObject(const uint8_t* cell,void*){assert(cell==g_areaCells+20*401);return g_afterTarget;}
static int __fastcall areaFixtureNeighbors(void*,void*,uint32_t* cells,int radius,uint32_t center){
    assert(radius==1 && center==0x10002);cells[0]=center;return g_areaNeighbors;
}
static int __cdecl areaFixtureBuildingType(){return 5;}
static int __cdecl areaFixtureUnitType(){return 11;}
static int __cdecl areaFixtureIgnore(void*){return 0;}
static int __cdecl areaFixtureVisible(uint32_t,uint32_t){return 1;}
static void* __fastcall areaFixtureLeader(void*,void*,int,int,int){return g_personGuide;}
static void __fastcall areaFixtureCasualty(void*,void*,void*,void*){}
static int __fastcall areaFixtureCounter(void*,void*,void*,void*,int){return 0;}
static void __cdecl areaFixtureBuild(uint8_t* packet,void* source,int tactic,uint32_t to,int a,int b,int c){
    assert(source==g_afterSource && tactic==7 && !a && !b && !c);++g_areaBuilds;
    memset(packet,0,0x80);*(uint32_t*)(packet+0x58)=0x10001;*(uint32_t*)(packet+0x5c)=to;
    *(int*)(packet+0x50)=7;*(int*)(packet+0xc)=50;
}
static void __cdecl areaFixtureSettle(void*){++g_areaSettles;}
static int __fastcall areaFixtureOwner(uint8_t* object,void*){
    int result=*(int*)(object+0x30);uintptr_t caller=(uintptr_t)_ReturnAddress();
    if(object==g_afterSource && g_areaInvalid>=0 && (caller==site(0x496c0b) || caller==site(0x496c3b))){
        int invalid=g_areaInvalid;g_areaInvalid=-1;
        if(invalid==0)*(int*)(g_personGuide+0xe8)=-1;
        if(invalid==1)g_afterActorRegistered=false;
        if(invalid==2)++*(int*)(g_personGuide+0x98);
        if(invalid==3)g_afterSourceRegistered=false;
        if(invalid==4)*(int*)(g_afterSource+0xc)=-1;
        if(invalid==5)++*(uint32_t*)(g_afterSource+4);
        if(invalid==6)g_afterTargetRegistered=false;
        if(invalid==7)++*(int*)(g_afterTarget+0x30);
        if(invalid==8)++*(uint32_t*)(g_afterTarget+4);
        if(invalid==9)skillsTelemetryStream(1,0);
        if(invalid==10)++*(int*)(g_afterSource+0x30);
        if(invalid==11)++*(int*)(g_afterTarget+8);
    }
    return result;
}
__declspec(naked) static int __cdecl areaFixturePreview(void* source,void* target,uint8_t* records,uint8_t* cell){__asm {push ebx
    push ebp
    push esi
    push edi
    mov ebp,[esp+20]
    mov edi,[esp+24]
    mov esi,[esp+28]
    mov ebx,[esp+32]
    jmp dword ptr [g_areaPreviewEntry]}}
__declspec(naked) static void areaFixturePreviewSkipped(){__asm {mov eax,1
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
__declspec(naked) static void areaFixturePreviewContinue(){__asm {xor eax,eax
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
static void areaFriendlyTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},first[400]={},second[400]={},records[0x604]={},cells[20*402]={};
    g_afterSource=source;g_afterTarget=target;g_personGuide=first;g_personOther=second;g_personTarget=g_personSpouse=nullptr;g_areaCells=cells;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldUnitId=g_telemetryUnitId;
    auto oldUnit=g_movementUnitById;auto oldBaseId=g_telemetryBaseId;auto oldBase=g_baseById;
    auto oldRoot=g_areaFriendlyRootResume,oldHas=g_areaFriendlyHasResume,oldSkip=g_areaFriendlySkip,oldContinue=g_areaFriendlyContinue;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)afterFixturePersonId;
    g_telemetryUnitId=(BuildingId)areaFixtureUnitId;g_movementUnitById=(MovementUnitById)areaFixtureUnit;
    g_telemetryBaseId=(BuildingId)areaFixtureBaseId;g_baseById=(decltype(g_baseById))areaFixtureBase;
    g_areaFriendlyRootResume=(uint32_t)site(0x595256);g_areaFriendlyHasResume=(uint32_t)site(0x59537c);
    g_areaFriendlySkip=(uint32_t)site(0x59548e);g_areaFriendlyContinue=(uint32_t)site(0x59538e);
    void* vt[24]={};vt[0x24/4]=areaFixtureType;vt[0x2c/4]=areaFixtureCast;vt[0x3c/4]=afterFixturePosition;
    vt[0x40/4]=areaFixtureOwner;vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=afterFixtureFind;
    *(void**)source=*(void**)target=vt;
    uint32_t stubs[]={0x490e70,0x483b40,0x483c80,0x47a630,0x5afbe0,0x5afc70,0x5a06d0,0x5b1870,
        0x584d70,0x573470,0x468e60,0x495a40,0x5974c0};uint8_t saved[13][5];
    const void* replacements[]={areaFixtureNativeUnit,areaFixtureObject,areaFixtureNeighbors,afterFixtureValid,
        areaFixtureIgnore,areaFixtureBuild,areaFixtureVisible,areaFixtureSettle,areaFixtureCounter,
        areaFixtureBuildingType,areaFixtureUnitType,areaFixtureLeader,areaFixtureCasualty};
    for(int n=0;n<13;++n){memcpy(saved[n],(void*)site(stubs[n]),5);domesticStub(stubs[n],replacements[n]);}
    uint32_t operands[]={0x595284,0x5952b5,0x5952cc,0x595352,0x595528,0x5955ce,0x5955d5};uint32_t oldOperands[7];
    for(int n=0;n<7;++n){auto operand=(uint32_t*)site(operands[n]);oldOperands[n]=*operand;
        assert(*operand==(n==0?0x6fb0e70:0x6fb0e68));*operand=(uint32_t)cells+(n==0?8:0);}
    *(uint16_t*)(cells+20*201+8)=4095;*(int*)(cells+20*401+4)=7;
    bool old44=g_skills[249].effects[44];g_skills[249].effects[44]=true;
    auto setup=[&](int provider,int kind){
        memset(records,0,sizeof records);*(uint32_t*)(records+0x58)=0x10001;*(uint32_t*)(records+0x5c)=0x10002;*(int*)(records+0x50)=7;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(int*)(source+8)=1;*(int*)(target+8)=kind?kind-1:1;
        *(int*)(source+0x30)=*(int*)(target+0x30)=2;*(int*)(source+0xa8)=3;*(int*)(target+0xa8)=2;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        *(int*)(first+0xe8)=provider==0?44:provider==2?249:-1;
        *(int*)(second+0xe8)=provider==1?44:provider==2?44:provider==3?249:-1;
        *(int*)(first+0x98)=0;g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;
        g_areaBase=kind!=0;g_areaNeighbors=1;g_areaBuilds=g_areaSettles=0;g_areaInvalid=-1;
    };
    auto run=[&](int expected){
        assert(((AreaFriendlyHandler)site(0x595250))((void*)1,records)==1);
        assert(*(int*)(records+0x600)==expected && g_areaBuilds==expected && g_areaSettles==expected);
        assert(!g_areaFriendly && !g_progressionProbe);
    };
    assert(skilltelemetry::open(L"build/area_friendly_test.csv",personFixtureActor,nullptr));
    for(int kind=0;kind<5;++kind)for(int provider=0;provider<4;++provider){setup(provider,kind);run(0);}
    for(int kind=0;kind<5;++kind)for(int owned:{44,249}){setup(0,kind);*(int*)(first+0xe8)=*(int*)(second+0xe8)=owned;run(0);}
    for(int provider=0;provider<4;++provider){setup(provider,0);*(int*)(target+0x30)=3;run(1);}
    setup(-1,0);run(1);setup(0,0);g_areaNeighbors=0;run(0);
    for(int invalid=0;invalid<12;++invalid){setup(0,invalid&1);g_areaInvalid=invalid;run(0);assert(g_areaInvalid==-1);}
    uint8_t previewSkip[5],previewContinue[5];memcpy(previewSkip,(void*)site(0x59548e),5);memcpy(previewContinue,(void*)site(0x59538e),5);
    domesticStub(0x59548e,areaFixturePreviewSkipped);domesticStub(0x59538e,areaFixturePreviewContinue);g_areaPreviewEntry=site(0x595370);
    for(int provider=0;provider<4;++provider){setup(provider,0);assert(areaFixturePreview(source,target,records,cells+20*401)==1 && !g_areaFriendly);}
    memcpy((void*)site(0x59548e),previewSkip,5);memcpy((void*)site(0x59538e),previewContinue,5);
    skilltelemetry::close();for(int kind=0;kind<5;++kind)for(int provider=0;provider<4;++provider){setup(provider,kind);run(0);}
    for(int n=0;n<13;++n)memcpy((void*)site(stubs[n]),saved[n],5);
    for(int n=0;n<7;++n)*(uint32_t*)site(operands[n])=oldOperands[n];
    g_skills[249].effects[44]=old44;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldUnitId;
    g_movementUnitById=oldUnit;g_telemetryBaseId=oldBaseId;g_baseById=oldBase;
    g_areaFriendlyRootResume=oldRoot;g_areaFriendlyHasResume=oldHas;g_areaFriendlySkip=oldSkip;g_areaFriendlyContinue=oldContinue;
    puts("PASS native area44:52 inputs,30 same-force unit/city/gate/port/facility exclusions, real relation/loop/count, four foreign targets,12 stale identities, previews and20 disabled controls");
}
