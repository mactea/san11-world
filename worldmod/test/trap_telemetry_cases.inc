// Native constructors, arithmetic, float conversion, resource CALL sites and
// final clamp/setters run. Root world traversal and UI remain controlled.
static int trapFixtureKind=16,trapFixtureProfile=0,trapFixtureRoot=0,trapFixtureFault=0,trapFixtureRandomCalls=0;
static uintptr_t trapFixtureUnitEntry,trapFixtureBaseEntry;
static uintptr_t trapFixtureBuildCalls[3],trapFixtureAuxCalls[3];
static bool trapFixtureNestedPreview=false;
static uint8_t trapFixtureLate[2][0x80];
__declspec(naked) static void __cdecl trapFixturePrimary(uint8_t*,int,void*,void*,void*){__asm {
    push [esp+20]
    push [esp+20]
    push [esp+20]
    push [esp+20]
    push [esp+20]
    mov eax,trapFixtureRoot
    jmp dword ptr [trapFixtureBuildCalls+eax*4]}}
__declspec(naked) static void __cdecl trapFixtureAuxiliary(uint8_t*,void*,void*,void*){__asm {
    push [esp+16]
    push [esp+16]
    push [esp+16]
    push [esp+16]
    mov eax,trapFixtureRoot
    jmp dword ptr [trapFixtureAuxCalls+eax*4]}}
__declspec(naked) static void trapFixturePrimaryDone(){__asm {add esp,20
    ret}}
__declspec(naked) static void trapFixtureAuxiliaryDone(){__asm {add esp,16
    ret}}
static int __cdecl trapFixtureRandom(int range){assert(range==100 || range==500 || range==1000 || range==200 || range==50);++trapFixtureRandomCalls;return 17;}
static int __fastcall trapFixtureTech(void*,void*,int id){assert(id==31);return 0;}
static int __cdecl trapFixtureBaseKind(){return 5;}
static int __fastcall trapFixtureUnitId(void* w,void* x,void* p){return p==g_criticalTarget && g_criticalBuildingTarget?-1:criticalFixtureUnitId(w,x,p);}
static int __fastcall trapFixtureFind(uint8_t* object,void*,int effect){
    for(int n=0;n<3;++n){int id=g_criticalBuildingTarget && object==g_criticalTarget?(n==0?1000:n==1?601:-1):*(int*)(object+0xc+4*n);
        auto p=personFixtureLookup(nullptr,nullptr,id);if(p && personHas(p,nullptr,effect))return id;}
    return -1;
}
static bool trapFixtureActor(const void* p,skilltelemetry::Actor& a){
    personFixtureActor(p,a);bool defender=p==g_personTarget || p==g_personSpouse || (trapFixtureProfile==8 && p==g_personOther);
    a.force=defender?3:2;a.city=defender?8:9;
    if(trapFixtureNestedPreview && g_trapCalculation){trapFixtureNestedPreview=false;uint8_t preview[0x80];
        ((TrapBuild)site(0x5b15a0))(preview,trapFixtureKind,g_criticalSource,g_criticalTarget,nullptr);}
    return true;
}
__declspec(naked) static int __cdecl trapFixtureUnitCommit(void*,void*,void*){__asm {push ebp
    push esi
    mov esi,[esp+12]
    mov ebp,[esp+16]
    mov edx,[esp+20]
    mov eax,[ebp+0xc]
    add eax,[edx+0xc]
    neg eax
    push eax
    push esi
    xor ecx,ecx
    jmp dword ptr [trapFixtureUnitEntry]}}
__declspec(naked) static void trapFixtureUnitDone(){__asm {pop esi
    pop ebp
    ret}}
static void trapFixtureBuildPair(uint8_t* first,uint8_t* second){
    trapFixturePrimary(first,trapFixtureKind,g_criticalSource,g_criticalTarget,nullptr);
    if(trapFixtureFault==12){uint8_t preview[0x80];((TrapBuild)site(0x5b15a0))(preview,trapFixtureKind,g_criticalSource,g_criticalTarget,nullptr);}
    if(second)trapFixtureAuxiliary(second,g_criticalSource,g_criticalTarget,nullptr);
    if(trapFixtureFault==1)skilltelemetry::invalidateContext();
    if(trapFixtureFault==2)*(int*)(g_personTarget+0xe8)=-1;
    if(trapFixtureFault==3)*(int*)(g_personTarget+0xe8)=249;
    if(trapFixtureFault==4)*(int*)(g_criticalTarget+0xc)=-1;
    if(trapFixtureFault==5)g_criticalTargetRegistered=false;
    if(trapFixtureFault==6)*(uint32_t*)(g_criticalTarget+4)^=1;
    if(trapFixtureFault==7)*(int*)(g_criticalTarget+0xa8)=1;
    if(trapFixtureFault==8)++*(int*)(first+0xc);
    if(trapFixtureFault==9){memcpy(trapFixtureLate[0],first,0x80);if(second)memcpy(trapFixtureLate[1],second,0x80);}
    if(trapFixtureFault==10)++*(int*)(g_personTarget+0xa4);
    if(trapFixtureFault==11)++*(int*)(g_personTarget+0x98);
}
// Match the two packet offsets in the original597a60 base frame. Its actual
// HP->garrison instructions and both newly installed grant bridges execute.
__declspec(naked) static void trapFixtureBasePair(){__asm {push ebp
    push ebx
    push esi
    push edi
    sub esp,0x150
    lea eax,[esp+0xd0]
    push eax
    lea eax,[esp+0x54]
    push eax
    call trapFixtureBuildPair
    add esp,8
    mov esi,g_criticalTarget
    mov eax,[esp+0x60]
    add eax,[esp+0xe0]
    neg eax
    push eax
    push esi
    xor ecx,ecx
    jmp dword ptr [trapFixtureBaseEntry]}}
__declspec(naked) static void trapFixtureBaseDone(){__asm {add esp,0x150
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret}}
static int trapFixtureWorkflow(){
    if(trapFixtureRoot && g_criticalBuildingTarget){trapFixtureBasePair();return 73;}
    uint8_t first[0x80],second[0x80];trapFixtureBuildPair(first,trapFixtureRoot?second:nullptr);
    if(trapFixtureFault==9)return 73;
    if(trapFixtureRoot)trapFixtureUnitCommit(g_criticalTarget,first,second);
    else {
        *(uint32_t*)(first+0x58)=*(uint32_t*)(g_criticalSource+4);*(uint32_t*)(first+0x5c)=*(uint32_t*)(g_criticalTarget+4);
        for(int hp=0;hp<(g_criticalBuildingTarget?2:1);++hp){
            g_criticalQueuedHp=hp!=0;g_criticalQueuedEntry=site(hp?0x5b1aaf:g_criticalBuildingTarget?0x5b1a74:0x5b1c6c);
            runCriticalQueued(g_criticalTarget,g_criticalSource,first);
        }
    }
    return 73;
}
static int __fastcall trapFixtureFour(void*,void*,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){assert(a==11 && b==22 && c==33 && d==44);return trapFixtureWorkflow();}
static int __fastcall trapFixtureFive(void*,void*,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e){assert(a==11 && b==22 && c==33 && d==44 && e==55);return trapFixtureWorkflow();}
#include "fire_consumer_telemetry_cases.inc"
static void trapTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x2c/4]=criticalFixtureObject;vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=criticalFixtureOwner;
    vt[0x50/4]=criticalFixtureTargetTroops;vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=trapFixtureFind;vt[0x60/4]=trapFixtureTech;
    *(void**)source=*(void**)target=*(void**)g_criticalBaseResource=vt;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)personFixtureId;
    g_telemetryUnitId=(BuildingId)trapFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;
    g_telemetryBaseId=(BuildingId)criticalFixtureBaseId;g_baseById=(BaseById)criticalFixtureBase;
    g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);g_criticalBaseTroopsOriginal=(IncomeGrant)site(0x4ae3c0);g_criticalBaseHpOriginal=(IncomeGrant)site(0x4ae360);
    g_trapBuildResume=(uint32_t)site(0x5b15a7);g_trapAuxResume=(uint32_t)site(0x5b1197);g_trapStepResume=(uint32_t)site(0x5b17e2);
    g_trapLineOriginal=(TrapRootFour)trapFixtureFour;g_trapDirectOriginal=g_trapCellOriginal=(TrapRootFive)trapFixtureFive;
    g_trapUnitGrantResume=(uint32_t)site(0x59787f);g_trapHpGrantResume=(uint32_t)site(0x597dea);g_trapBaseGrantResume=(uint32_t)site(0x597e07);
    g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);g_criticalBaseTroopsResume=(uint32_t)site(0x5b1a79);g_criticalBaseHpResume=(uint32_t)site(0x5b1ab4);
    trapFixtureUnitEntry=site(0x59787a);trapFixtureBaseEntry=site(0x597de5);
    uint32_t primaryCalls[]={0x596272,0x597a16,0x597b12};
    for(int n=0;n<3;++n){trapFixtureBuildCalls[n]=site(primaryCalls[n]);domesticStub(primaryCalls[n]+5,trapFixturePrimaryDone);}
    trapFixtureAuxCalls[1]=site(0x597a23);trapFixtureAuxCalls[2]=site(0x597b29);
    domesticStub(0x597a28,trapFixtureAuxiliaryDone);domesticStub(0x597b2e,trapFixtureAuxiliaryDone);
    domesticStub(0x47a630,domesticValid);domesticStub(0x472150,trapFixtureRandom);domesticStub(0x573470,trapFixtureBaseKind);
    domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x48a4f0,battleCapacity);domesticStub(0x486bc0,criticalFixtureBaseMax);
    domesticStub(0x487230,criticalFixtureSetBaseTroops);domesticStub(0x487150,criticalFixtureHpMax);domesticStub(0x4c0c30,criticalFixtureCityTech);
    domesticStub(0x490aa0,criticalFixtureNoForce);domesticStub(0x4875c0,criticalFixtureEmptyGarrison);
    domesticStub(0x59787f,trapFixtureUnitDone);domesticStub(0x597e07,trapFixtureBaseDone);
    domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);domesticStub(0x5b1a79,criticalFixtureQueuedCommitted);domesticStub(0x5b1ab4,criticalFixtureQueuedCommitted);
    uint32_t clock=0x10001;for(uint32_t at:{0x5b15c9,0x5b15d2,0x5b11bd,0x5b11c6})*(uint32_t*)site(at)=(uint32_t)&clock;
    float factor=.1f;*(uint32_t*)site(0x5b17cf)=(uint32_t)&factor;
    uint32_t jumps[2][9];for(int table=0;table<2;++table){int count=table?9:8;
        for(int n=0;n<count;++n)jumps[table][n]=(uint32_t)site(*(uint32_t*)site((table?0x5b1848:0x5b1828)+4*n));
        *(uint32_t*)site(table?0x5b16fa:0x5b1611)=(uint32_t)jumps[table];}
    auto setup=[&](int root,int kind,int profile,bool base,int before){
        trapFixtureRoot=root;trapFixtureKind=kind;trapFixtureProfile=profile;trapFixtureFault=0;trapFixtureNestedPreview=false;g_criticalBuildingTarget=base;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalBaseRegistered=true;g_personRegistered=true;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(int*)(source+8)=*(int*)(target+8)=0;
        *(int*)(source+0xa8)=4;*(int*)(target+0xa8)=2;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=-1;*(int*)(source+0x14)=-1;
        *(int*)(target+0xc)=1000;*(int*)(target+0x10)=601;*(int*)(target+0x14)=-1;
        for(auto p:{actor,other,person,spouse})*(int*)(p+0xe8)=-1;
        g_skills[249].effects[6]=g_skills[249].effects[31]=g_skills[249].effects[62]=true;
        if(profile==1 || profile==5 || profile==6)*(int*)(actor+0xe8)=62;
        if(profile==2 || profile==5)*(int*)(person+0xe8)=31;
        if(profile==3 || profile==6)*(int*)(person+0xe8)=62;
        if(profile==4)*(int*)(person+0xe8)=6;
        if(profile==5 || profile==6)*(int*)(spouse+0xe8)=6;
        if(profile==7){*(int*)(source+0xc)=14162;*(int*)(other+0xe8)=62;*(int*)(person+0xe8)=249;}
        if(profile==8){*(int*)(target+0xc)=14162;*(int*)(other+0xe8)=6;}
        *(uint16_t*)(target+0x18)=(uint16_t)before;*(int16_t*)(target+0x10)=base?1000:*(int16_t*)(target+0x10);
        *(int*)(g_criticalBaseResource+0x40)=before;
    };
    auto run=[&](){int result=trapFixtureRoot?((TrapRootFive)site(trapFixtureRoot==1?0x5979d0:0x597a60))(nullptr,11,22,33,44,55):
        ((TrapRootFour)site(0x5960d0))(nullptr,11,22,33,44);assert(result==73 && !g_trapOwner && !g_trapCalculation);};
    assert(skilltelemetry::open(L"build/trap_test.csv",trapFixtureActor,nullptr));int inputs=0;
    for(int root=0;root<3;++root)for(bool base:{false,true})for(int kind:{16,23})for(int profile=0;profile<9;++profile)for(int before:{100,10000}){
        if(base && profile==8)continue;setup(root,kind,profile,base,before);int calls=trapFixtureRandomCalls;run();assert(trapFixtureRandomCalls==calls+(root?2:1));++inputs;
    }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/trap_edges_test.csv",trapFixtureActor,nullptr));
    for(int fault=0;fault<14;++fault){setup(1,16,4,false,1000);trapFixtureFault=fault;trapFixtureNestedPreview=fault==13;
        int calls=trapFixtureRandomCalls;run();assert(trapFixtureRandomCalls==calls+(fault>=12?3:2) && !trapFixtureNestedPreview);}
    trapFixtureUnitCommit(target,trapFixtureLate[0],trapFixtureLate[1]); // expired command scope
    skilltelemetry::close();assert(skilltelemetry::open(L"build/trap_preview_test.csv",trapFixtureActor,nullptr));
    setup(1,16,5,false,1000);uint8_t preview[0x80];((TrapBuild)site(0x5b15a0))(preview,16,source,target,nullptr);
    ((TrapAuxBuild)site(0x5b1190))(preview,source,target,nullptr);skilltelemetry::close();
    setup(1,16,5,false,1000);run();assert(!g_trapOwner && !g_trapCalculation);
    fireConsumerTelemetryTests(setup,source,target);
    printf("PASS traps: %d native constructor/grant inputs across3 owner lifetimes, unit/base, fire/stone, raw/composed/high/coeffects, native RNG and caps; previews/expired/stale controls\n",inputs);
}
