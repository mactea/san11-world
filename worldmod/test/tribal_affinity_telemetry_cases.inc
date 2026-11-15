#include "tribal_affinity_mapping.inc"
static uint8_t* tribalFixtureExpandedCave(int bound){
    auto cave=(uint8_t*)VirtualAlloc(nullptr,17,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(cave);
    memcpy(cave,"\x81\xfe\x00\x00\x00\x00\x0f\x8f\x00\x00\x00\x00\xe9\x00\x00\x00\x00",17);
    *(int*)(cave+2)=bound;*(int32_t*)(cave+8)=(int32_t)(site(0x5f6640)-(uintptr_t)cave-12);
    *(int32_t*)(cave+13)=(int32_t)(site(0x5f663c)-(uintptr_t)cave-17);return cave;
}
static void tribalAffinityLayoutTests(){
    uint8_t saved[5];memcpy(saved,(void*)site(0x5f6637),5);auto cave=tribalFixtureExpandedCave(529);
    domesticStub(0x5f6637,cave);assert(verify());
    for(int offset:{0,1,6,7,8,9,10,11,12,13,14,15,16}){cave[offset]^=1;assert(!verify());cave[offset]^=1;assert(verify());}
    for(int bound:{0,65535}){*(int*)(cave+2)=bound;assert(!verify());}
    *(int*)(cave+2)=41;assert(verify());*(int*)(cave+2)=529;assert(verify());
    memcpy((void*)site(0x5f6637),saved,5);assert(verify());VirtualFree(cave,0,MEM_RELEASE);
}
static uint8_t* g_tribalBase=nullptr,*g_tribalAlternateBase=nullptr;
static int g_tribalBaseId=-1,g_tribalErases=0,g_tribalFault=-1;
static bool g_tribalPersonRegistered=true,g_tribalPersonIdValid=true,g_tribalBaseRegistered=true,g_tribalBaseIdValid=true,g_tribalPresent=true;
static uintptr_t g_tribalCandidateEntry=0,g_tribalMemberEntry=0;
static TribalAffinityMatch g_tribalFixtureNative=nullptr;
static uint8_t* __fastcall tribalFixturePerson(void*,void*,int id){return g_tribalPersonRegistered?personFixtureLookup(nullptr,nullptr,id):nullptr;}
static int __fastcall tribalFixturePersonId(void*,void*,const void* p){return g_tribalPersonIdValid?personFixtureId(nullptr,nullptr,p):-1;}
static void* __fastcall tribalFixtureBase(void*,void*,int id){return g_tribalBaseRegistered && id==g_tribalBaseId?g_tribalBase:nullptr;}
static int __fastcall tribalFixtureBaseId(void*,void*,void* p){return g_tribalBaseIdValid && p==g_tribalBase?g_tribalBaseId:-1;}
static int __fastcall tribalFixtureNoUnit(void*,void*,void*){return -1;}
static const uint32_t* __fastcall tribalFixturePosition(uint8_t* p,void*){return (uint32_t*)(p+4);}
static int __fastcall tribalFixtureForce(uint8_t* p,void*){return *(int*)(p+0x30);}
static void* __fastcall tribalFixtureFind(void* self,void*,void* person,int start){
    assert(self==(void*)0x99c7888 && person && !start);return g_tribalPresent?(void*)1:nullptr;
}
static void __fastcall tribalFixtureErase(void* self,void*,void* node){
    assert(self==(void*)0x99c7888 && node==(void*)1 && g_tribalPresent);g_tribalPresent=false;++g_tribalErases;
}
static int __fastcall tribalFixtureNative(void* self,void*,int baseId,int raw){
    int result=g_tribalFixtureNative(self,baseId,raw);
    if(g_tribalFault>=0){
        int fault=g_tribalFault;g_tribalFault=-1;
        if(fault==0)*(int*)(g_personGuide+0xe8)=249;
        if(fault==1)g_tribalPersonRegistered=false;
        if(fault==2)g_tribalPersonIdValid=false;
        if(fault==3)++*(int*)(g_personGuide+0x98);
        if(fault==4)++*(int*)(g_personGuide+0x9c);
        if(fault==5)skillsTelemetryStream(1,0);
        if(fault==6)g_tribalBaseRegistered=false;
        if(fault==7)g_tribalBaseIdValid=false;
        if(fault==8)++*(int*)(g_tribalBase+0x30);
        if(fault==9)++*(uint32_t*)(g_tribalBase+4);
        if(fault==10)++*(int*)(g_tribalBase+8);
        if(fault==11)g_tribalBase=g_tribalAlternateBase;
    }
    return result;
}
__declspec(naked) static int __cdecl runTribalCandidate(void*,int){__asm {push ebx
    push ebp
    push esi
    push edi
    mov esi,[esp+20]
    mov ebp,[esp+24]
    jmp dword ptr [g_tribalCandidateEntry]}}
__declspec(naked) static void tribalCandidateMatch(){__asm {mov eax,1
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
__declspec(naked) static void tribalCandidateMiss(){__asm {xor eax,eax
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
__declspec(naked) static int __cdecl runTribalMember(void*,int){__asm {push ebx
    push ebp
    push esi
    push edi
    sub esp,0x30
    mov esi,[esp+0x44]
    mov eax,[esp+0x48]
    mov [esp+0x14],eax
    push 0
    push 0
    xor ebx,ebx
    xor edx,edx
    jmp dword ptr [g_tribalMemberEntry]}}
__declspec(naked) static void tribalMemberMatch(){__asm {mov eax,1
    add esp,0x30
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
__declspec(naked) static void tribalMemberMiss(){__asm {xor eax,eax
    add esp,0x30
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
static void tribalAffinityTelemetryTests(){
    uint8_t first[400]={},second[400]={},base[0x100]={},alternate[0x100]={};
    auto oldGuide=g_personGuide,oldOther=g_personOther,oldTarget=g_personTarget,oldSpouse=g_personSpouse;
    g_personGuide=first;g_personOther=second;g_personTarget=g_personSpouse=nullptr;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldBase=g_baseById;
    auto oldBaseId=g_telemetryBaseId,oldUnitId=g_telemetryUnitId;auto oldMatch=g_tribalAffinityOriginal;
    g_telemetryPerson=(PersonById)tribalFixturePerson;g_telemetryPersonId=(BuildingId)tribalFixturePersonId;
    g_baseById=(decltype(g_baseById))tribalFixtureBase;g_telemetryBaseId=(BuildingId)tribalFixtureBaseId;
    g_telemetryUnitId=(BuildingId)tribalFixtureNoUnit;
    g_tribalFixtureNative=(TribalAffinityMatch)site(0x5f6610);g_tribalAffinityOriginal=(TribalAffinityMatch)tribalFixtureNative;
    g_tribalCandidateEntry=site(0x5e9cac);g_tribalMemberEntry=site(0x5ed9b7);
    uint32_t sites[]={0x482af0,0x482fc0,0x5e9ce7,0x5e9da9,0x5ed9e3,0x5eda45};uint8_t saved[6][5];
    const void* replacements[]={tribalFixtureFind,tribalFixtureErase,tribalCandidateMiss,tribalCandidateMatch,tribalMemberMiss,tribalMemberMatch};
    for(int n=0;n<6;++n){memcpy(saved[n],(void*)site(sites[n]),5);domesticStub(sites[n],replacements[n]);}
    auto nativeMapping=(uint32_t*)site(0x5f662f);uint32_t oldMapping=*nativeMapping;
    assert(oldMapping==0x84cf44);*nativeMapping=(uint32_t)g_tribalAffinityMapping;
    void* vt[24]={};vt[0x3c/4]=tribalFixturePosition;vt[0x40/4]=tribalFixtureForce;
    bool oldEffects[5];for(int n=0;n<5;++n){oldEffects[n]=g_skills[249].effects[91+n];g_skills[249].effects[91+n]=n<4;}
    auto setup=[&](int personId,int raw,int baseId,int kind,bool present=true){
        memset(first,0,sizeof first);memset(second,0,sizeof second);memset(base,0,sizeof base);
        auto person=personId==600?first:second;*(int*)(person+0xe8)=raw;*(int*)(person+0x98)=baseId;
        *(void**)base=vt;*(int*)(base+8)=kind;*(uint32_t*)(base+4)=0x10002;*(int*)(base+0x30)=3;
        memcpy(alternate,base,sizeof base);g_tribalBase=base;g_tribalAlternateBase=alternate;g_tribalBaseId=baseId;
        g_tribalPersonRegistered=g_tribalPersonIdValid=g_tribalBaseRegistered=g_tribalBaseIdValid=true;
        g_tribalPresent=present;g_tribalErases=0;g_tribalFault=-1;return person;
    };
    auto run=[&](int owner,uint8_t* person,int baseId,const int* mapping=g_tribalAffinityMapping,int bound=41){
        int raw=*(int*)(person+0xe8);bool matched=false;
        if(raw>=91 && raw<=94)for(int n=0;n<3;++n)if(baseId>=0 && baseId<=bound && baseId==mapping[(raw-91)*3+n])matched=true;
        bool fallback=owner==1 && contains(raw,95);bool wasPresent=g_tribalPresent;
        int result=owner?runTribalMember(person,baseId):runTribalCandidate(person,baseId);
        assert(result==int(matched || fallback));
        assert(g_tribalErases==int(!owner && matched && wasPresent));
        assert(g_tribalPresent==(!owner && matched?false:wasPresent));
    };
    assert(skilltelemetry::open(L"build/tribal_affinity_test.csv",personFixtureActor,nullptr));
    for(int owner=0;owner<2;++owner)for(int personId:{600,14162})for(int raw:{-1,90,91,92,93,94,99,249})
        for(int baseId=-1;baseId<=43;++baseId)for(int kind=0;kind<3;++kind){int id=baseId==43?529:baseId;run(owner,setup(personId,raw,id,kind),id);}
    for(int personId:{600,14162})for(int effect=91;effect<=94;++effect)for(int n=0;n<3;++n)for(int present=0;present<2;++present){
        int id=g_tribalAffinityMapping[(effect-91)*3+n];run(0,setup(personId,effect,id,n,present!=0),id);
    }
    for(int personId:{600,14162})for(int id:{0,41})run(1,setup(personId,95,id,0),id);
    g_skills[249].effects[95]=true;
    for(int personId:{600,14162})for(int id:{0,41})run(1,setup(personId,249,id,0),id);
    g_skills[249].effects[95]=false;
    for(int owner=0;owner<2;++owner)for(int fault=0;fault<12;++fault){
        auto person=setup(600,91,0,owner);g_tribalFault=fault;run(owner,person,0);assert(g_tribalFault==-1);
    }
    for(int personId:{600,14162})for(int raw:{91,249}){
        auto person=setup(personId,raw,0,0);
        assert(tribalAffinityMatch((void*)0x9a916c4,nullptr,0,raw,person,site(0x5f0000))==int(raw==91));
    }
    for(int personId:{600,14162}){
        auto person=setup(personId,249,0,0);
        assert(tribalAffinityMatch((void*)0x9a916c4,nullptr,0,91,person,site(0x5e9cbe))==1);
    }
    uint8_t savedBound[5];memcpy(savedBound,(void*)site(0x5f6637),5);auto cave=tribalFixtureExpandedCave(529);
    domesticStub(0x5f6637,cave);int expandedMapping[12];*nativeMapping=(uint32_t)expandedMapping;
    for(int owner=0;owner<2;++owner)for(int personId:{600,14162})for(int effect=91;effect<=94;++effect)for(int id:{529,530}){
        memcpy(expandedMapping,g_tribalAffinityMapping,sizeof expandedMapping);expandedMapping[(effect-91)*3]=id;
        run(owner,setup(personId,effect,id,0),id,expandedMapping,529);
    }
    memcpy((void*)site(0x5f6637),savedBound,5);VirtualFree(cave,0,MEM_RELEASE);*nativeMapping=(uint32_t)g_tribalAffinityMapping;
    skilltelemetry::close();
    for(int owner=0;owner<2;++owner)for(int personId:{600,14162})for(int raw:{91,249})run(owner,setup(personId,raw,0,0),0);
    for(int n=0;n<6;++n)memcpy((void*)site(sites[n]),saved[n],5);*nativeMapping=oldMapping;
    for(int n=0;n<5;++n)g_skills[249].effects[91+n]=oldEffects[n];
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_baseById=oldBase;g_telemetryBaseId=oldBaseId;g_telemetryUnitId=oldUnitId;
    g_tribalAffinityOriginal=oldMatch;g_personGuide=oldGuide;g_personOther=oldOther;g_personTarget=oldTarget;g_personSpouse=oldSpouse;
    puts("PASS tribal AI affinity:4320 native owner-fragment inputs,48 list-presence inputs,eight95 fallback controls,24 stale identities,six caller/raw boundaries,32 expanded-city cave cases,eight disabled controls; native mapping and branches preserved");
}
