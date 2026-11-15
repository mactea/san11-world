// Native509bb0 roll arithmetic/32 branch and the two actual50aa.. CALL paths,
// including real50c170 stores and the action-record result-flag branch.
static uintptr_t g_luckyDuelParentEntry=0;
static uint32_t g_luckyDuelSeed=0;
static int g_luckyDuelFault=0;
static int __fastcall luckyDuelNativeRoll(uint8_t* duel,void*,int* option){
    int result=((LuckyDuelRoll)site(0x509bb0))(duel,option);
    int side=*(int*)(duel+0x578);int slot=*(int*)(duel+0xe8+0xec*(1-side));
    auto person=*(uint8_t**)(duel+0x24+0xec*(1-side)+0x40*slot);
    switch(g_luckyDuelFault){
    case 1:skilltelemetry::invalidateContext();break;
    case 2:*(int*)(person+0xe8)=-1;break;
    case 3:g_afterActorRegistered=false;break;
    case 4:++*(int*)(duel+0xe8+0xec*side);break;
    case 5:++*(int*)(duel+0x580);break;
    case 6:*(int*)(duel+0x578)=1-side;break;
    case 7:++*(int*)(duel+0x4c4);break;
    case 8:++*(int*)(person+0x9c);break;
    case 9:*(void**)(duel+0x24+0xec*(1-side)+0x40*slot)=g_personTarget;break;
    case 10:g_skills[249].effects[32]=false;break;
    }
    return result;
}
static int __cdecl luckyDuelMemory(void* p,int size,int count){assert(size==0x40 && count==1);return p!=nullptr;}
static int __fastcall luckyDuelNotPlayer(void*,void*,int){return 0;}
static int __fastcall luckyDuelNoSpecialPair(void*,void*,int,int){return 0;}
static int __fastcall luckyDuelPersonForce(void* p,void*){return p==g_personTarget?2:3;}
static bool luckyDuelActor(const void* p,skilltelemetry::Actor& actor){personFixtureActor(p,actor);actor.force=p==g_personTarget?2:3;return true;}
__declspec(naked) static int __cdecl runLuckyDuelParent(void*){__asm {push ebp
    push ebx
    push esi
    push edi
    mov edi,[esp+20]
    mov ebx,[edi+0x578]
    xor ebx,1
    sub esp,0x40
    jmp dword ptr [g_luckyDuelParentEntry]}}
__declspec(naked) static int luckyDuelParentDone(){__asm {mov eax,ebp
    add esp,0x40
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret}}
#include "lucky_duel_casualty_telemetry_cases.inc"
#include "lucky_duel_response_telemetry_cases.inc"
static void luckyDuelTelemetryTests(){
    uint8_t duel[0x600]={},people[4][400]={};void* vt[24]={};vt[0x40/4]=luckyDuelPersonForce;
    for(auto& p:people)*(void**)p=vt;
    auto oldGuide=g_personGuide,oldOther=g_personOther,oldSpouse=g_personSpouse,oldTarget=g_personTarget;
    g_personGuide=people[0];g_personOther=people[1];g_personSpouse=people[2];g_personTarget=people[3];
    auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryPersonId;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)afterFixturePersonId;
    auto oldSkill=g_skills[249];g_skills[249].effects[32]=true;
    auto oldRoll=g_luckyDuelRollOriginal;auto oldRead=g_luckyDuelPersonOriginal;
    g_luckyDuelRollOriginal=(LuckyDuelRoll)luckyDuelNativeRoll;g_luckyDuelPersonOriginal=(LuckyDuelPerson)site(0x505f70);
    uint32_t sites[]={0x50ab1c,0x505ef0,0x507ae0,0x472070,0x47a600};uint8_t saved[5][5];
    for(int n=0;n<5;++n)memcpy(saved[n],(void*)site(sites[n]),5);
    domesticStub(sites[0],luckyDuelParentDone);domesticStub(sites[1],luckyDuelNotPlayer);
    domesticStub(sites[2],luckyDuelNoSpecialPair);domesticStub(sites[3],luckyDuelMemory);domesticStub(sites[4],domesticValid);
    const uint8_t sampler[]={0x8b,0x4c,0x24,0x04,0x85,0xc9,0x7f,0x03,0x33,0xc0,0xc3,0xa1,0x44,0x5d,0x8a,0x00,0x69,0xc0,0x65,0x89,0x07,0x6c,0x05,0x39,0x30,0x00,0x00,0xa3,0x44,0x5d,0x8a,0x00,0xc1,0xe8,0x10,0x99,0x56,0xbe,0x64,0x00,0x00,0x00,0xf7,0xfe,0x33,0xc0,0x5e,0x0f,0xb7,0xd2,0x3b,0xd1,0x0f,0x9c,0xc0,0xc3};
    uint8_t savedSampler[sizeof sampler];memcpy(savedSampler,(void*)site(0x4721d0),sizeof sampler);
    memcpy((void*)site(0x4721d0),sampler,sizeof sampler);*(uint32_t*)site(0x4721dc)=*(uint32_t*)site(0x4721ec)=(uint32_t)&g_luckyDuelSeed;
    auto setup=[&](int mode,int side,int slot,int owned){
        memset(duel,0,sizeof duel);for(auto& p:people){*(int*)(p+0xe8)=-1;memset(p+0x98,0,16);}
        *(int*)(people[slot]+0xe8)=owned;int other=(slot+1)%3;
        *(int*)(duel+0x578)=side;*(int*)(duel+0x580)=mode==0?5:mode==1?4:7;*(int*)(duel+0x4c4)=2;
        *(int*)(duel+0xe8+0xec*side)=other;*(int*)(duel+0xe8+0xec*(1-side))=slot;
        *(void**)(duel+0x24+0xec*side+0x40*other)=people[3];
        *(void**)(duel+0x24+0xec*(1-side)+0x40*slot)=people[slot];
        for(int n=0;n<12;++n)*(int*)(duel+0x1fc+4*n)=side?0:100;
        *(int*)(duel+0x2b0+24*2)=slot&1;g_luckyDuelSeed=0;g_afterActorRegistered=true;
        g_luckyDuelFault=0;g_skills[249].effects[32]=true;
        g_luckyDuelParentEntry=site(mode==0?0x50aad1:0x50ab53);
    };
    assert(skilltelemetry::open(L"build/lucky_duel_test.csv",luckyDuelActor,nullptr));
    for(int mode=0;mode<3;++mode)for(int side:{0,1})for(int slot=0;slot<3;++slot)for(int owned:{32,249}){
        setup(mode,side,slot,owned);assert(runLuckyDuelParent(duel)==0 && g_luckyDuelSeed==0);
        assert(*(int*)(duel+0x2b0+24*2)==(slot&1) && !g_progressionProbe);
    }
    for(int owned:{32,249})for(int fault=1;fault<=(owned==249?10:9);++fault){
        setup(1,1,1,owned);g_luckyDuelFault=fault;
        assert(runLuckyDuelParent(duel)==0 && g_luckyDuelSeed==0 && *(int*)(duel+0x2b0+24*2)==1);
    }
    for(int owned:{32,249}){setup(0,0,0,owned);
        assert(((int(__thiscall*)(void*,int*))site(0x509bb0))(duel,(int*)(duel+0x578))==0 && g_luckyDuelSeed==0);}
    for(int mode=0;mode<3;++mode){setup(mode,0,0,-1);*(int*)(people[3]+0xe8)=32;
        assert(runLuckyDuelParent(duel)==1 && g_luckyDuelSeed==0x3039 && *(int*)(duel+0x2b0+24*2)==1);}
    skilltelemetry::close();
    for(int mode=0;mode<3;++mode){setup(mode,1,1,249);
        assert(runLuckyDuelParent(duel)==0 && g_luckyDuelSeed==0 && *(int*)(duel+0x2b0+24*2)==1);}
    luckyDuelCasualtyTelemetryTests(duel,people);
    luckyDuelResponseTelemetryTests(duel,people);
    for(int n=0;n<5;++n)memcpy((void*)site(sites[n]),saved[n],5);memcpy((void*)site(0x4721d0),savedSampler,sizeof sampler);
    g_skills[249]=oldSkill;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldId;
    g_personGuide=oldGuide;g_personOther=oldOther;g_personSpouse=oldSpouse;g_personTarget=oldTarget;
    g_luckyDuelRollOriginal=oldRoll;g_luckyDuelPersonOriginal=oldRead;
    puts("PASS lucky duel: two native action paths/three actions, both sides/three slots, raw/composed/high providers, native32 roll and action-flag/RNG controls");
}
