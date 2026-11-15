// Execute the native33 branch, real parent candidate CALL, full4b1280 capture
// routine (SEH included), native RNG/candidate lists, final disposition CALL
// and real prisoner setter, then full4bccd0 cleanup/person relocation. Parent
// frame/UI/context/adjacency inputs, disposition UI and final prisoner routine
// outer body are adapters. Native world unit removal is a registry fixture.
static uintptr_t g_captureFixtureBegin,g_captureFixtureCleanup,g_captureFixturePrisonerCall,g_captureFixtureStatusCall;
static int g_captureFixtureRange=1,g_captureFixtureDisposition=1,g_captureFixtureItemMask=0;
static int g_captureFixtureMutation=0,g_captureFixtureCandidates=0;
static bool g_captureFixtureRemove=true;
static int g_captureFixtureCleanupMutation=0;
struct CaptureFixtureNode {CaptureFixtureNode *next,*previous;void* person;};
static CaptureFixtureNode g_captureFixtureNodes[128];static int g_captureFixtureNodeCount=0;
static uint8_t g_captureFixtureForce[2][400]={};static int g_captureFixtureDifficulty=0;
static int __stdcall captureFixtureReadable(const void*,unsigned){return 0;}
static int(__stdcall* g_captureFixtureReadablePointer)(const void*,unsigned)=captureFixtureReadable;
static int __fastcall captureFixturePersonId(void* world,void* unused,const void* person){
    return g_captureFixtureMutation==8 && person==g_personTarget?-1:criticalFixturePersonId(world,unused,person);
}
static void* __fastcall captureFixtureUnit(void*,void*,int id){
    return id==4094?g_criticalTarget:id==4095 && g_criticalSourceRegistered?g_criticalSource:nullptr;
}
static void __fastcall captureFixtureNoStatus(void*,void*,int){}
static void __fastcall captureFixtureClear(uint8_t* list,void*){memset(list+4,0,0x1c);}
static void __fastcall captureFixtureAppend(uint8_t* list,void*,void* person){
    assert(g_captureFixtureNodeCount<128);auto n=&g_captureFixtureNodes[g_captureFixtureNodeCount++];n->person=person;n->next=nullptr;
    n->previous=*(CaptureFixtureNode**)(list+8);if(n->previous)n->previous->next=n;else *(CaptureFixtureNode**)(list+4)=n;
    *(CaptureFixtureNode**)(list+8)=n;++*(int*)(list+0xc);
}
static void** __fastcall captureFixtureNext(void*,void*,CaptureFixtureNode** iterator){
    auto n=*iterator;assert(n);*iterator=n->next;return &n->person;
}
static void* __fastcall captureFixtureContext(void** context,void*,void* target,void* source,void*){
    context[0]=target;context[1]=source;context[2]=(void*)1;context[3]=nullptr;return context;
}
static int __fastcall captureFixtureKind(void*,void*){return 11;}
static int __fastcall captureFixtureNotPlayer(void*,void*){return 0;}
static void* __fastcall captureFixtureForce(void*,void*,int id){return id==2?g_captureFixtureForce[0]:id==3?g_captureFixtureForce[1]:nullptr;}
static int __fastcall captureFixtureForceId(void*,void*,void* force){return force==g_captureFixtureForce[0]?2:force==g_captureFixtureForce[1]?3:-1;}
static int __fastcall captureFixtureDistance(void*,void*,uint32_t,int,uint32_t){return g_captureFixtureRange;}
static int __fastcall captureFixtureFeature(void*,void*){return 0;}
static int __fastcall captureFixtureMember(uint8_t* unit,void*,int slot){assert(slot>=0 && slot<3);return *(int*)(unit+0xc+4*slot);}
static int __fastcall captureFixtureNeighbors(void*,void*,uint32_t,int,void*,int,int){return 1;}
static uint8_t __fastcall captureFixtureAttribute(void*,void*){return 100;}
static int __fastcall captureFixtureItem(void*,void*,void* person,int){return (person==g_personTarget && (g_captureFixtureItemMask&1)) ||
    (person==g_personSpouse && (g_captureFixtureItemMask&2));}
static int __fastcall captureFixtureHome(void*,void*,void*){return 5;}
static int __fastcall captureFixtureLeaderBase(void*,void*){return 5;}
static void* __fastcall captureFixtureLeader(uint8_t* unit,void*){return personFixtureLookup(nullptr,nullptr,*(int*)(unit+0xc));}
static void __fastcall captureFixtureVoid(void*,void*){}
static void __fastcall captureFixtureVoid1(void*,void*,void*){}
static void __fastcall captureFixtureVoid2(void*,void*,void*,int){}
static void __cdecl captureFixtureClock(){}
static void __fastcall captureFixtureRegion(void*,void*,uint8_t* list,int,int){captureFixtureAppend(list,nullptr,g_personGuide);}
static void __fastcall captureFixtureRemove(void*,void*,int id){
    assert(id==4094);
}
static void __fastcall captureFixtureVisualRemove(void*,void*,void* unit){
    assert(unit==g_criticalTarget);
    if(g_captureFixtureRemove){((DefeatUnitReset)site(0x4960a0))(unit,0);g_criticalTargetRegistered=false;}
}
static int __fastcall captureFixtureCleanup(void* self,void*,void* unit,int flag){
    if(g_captureFixtureCleanupMutation==1)*(int*)(g_personTarget+0xe8)=-1;
    if(g_captureFixtureCleanupMutation==2)skillsTelemetryStream(1,0);
    if(g_captureFixtureCleanupMutation==4)g_captureFixtureRemove=false;
    if(g_captureFixtureCleanupMutation==5)*(int*)(g_personTarget+0xa0)=5;
    int result=((DefeatCleanup)site(0x4bccd0))(self,unit,flag);
    if(g_captureFixtureCleanupMutation==3)skillsTelemetryStream(1,0);
    if(g_captureFixtureCleanupMutation==6)g_captureFixtureMutation=8;
    if(g_captureFixtureCleanupMutation==7)*(int*)(g_personTarget+0xa0)=6;
    return result;
}
__declspec(naked) static int __fastcall captureFixtureDefeat(void*,void*,void*,void*,int,int,int){__asm {
    push ebp
    mov ebp,esp
    sub esp,0x4ac
    push ebx
    push esi
    push edi
    xor ebx,ebx
    mov [esp+0x1c],ecx
    mov [esp+0x28],ebx
    mov [esp+0x4a0],ebx
    mov [esp+0x4a4],ebp
    mov esi,g_criticalTarget
    mov ebp,g_criticalSource
    jmp dword ptr [g_captureFixtureBegin]}}
// The native branch's EBP holds the attacker, not the frame pointer. Save the
// adapter frame pointer in a native-unused local before loading that register.
__declspec(naked) static void captureFixtureAfterDecision(){__asm {
    mov esi,g_criticalTarget
    mov ecx,[esp+0x1c]
    push 0
    push esi
    jmp dword ptr [g_captureFixtureCleanup]}}
__declspec(naked) static void captureFixtureDone(){__asm {
    mov ebp,[esp+0x4a4]
    pop edi
    pop esi
    pop ebx
    mov esp,ebp
    pop ebp
    ret 20}}
__declspec(naked) static void captureFixturePrisonerCall(void*,void*,void*){__asm {
    mov ecx,[esp+4]
    mov eax,[esp+8]
    push [esp+12]
    push eax
    jmp dword ptr [g_captureFixturePrisonerCall]}}
__declspec(naked) static void __fastcall captureFixturePrisoner(void*,void*,void*,void*){__asm {
    push esi
    mov esi,[esp+8]
    jmp dword ptr [g_captureFixtureStatusCall]}}
__declspec(naked) static void captureFixtureStatusDone(){__asm {pop esi
    ret 8}}
__declspec(naked) static void captureFixtureReturn(){__asm {ret}}
static void __fastcall captureFixtureDisposition(void* self,void*,void*,uint8_t* captured,void*,void* source){
    for(auto node=*(CaptureFixtureNode**)(captured+4);node;node=node->next){
        ++g_captureFixtureCandidates;
        if(g_captureFixtureMutation==1)*(int*)(g_personGuide+0xe8)=-1;
        if(g_captureFixtureMutation==2)g_criticalSourceRegistered=false;
        if(g_captureFixtureMutation==3)skillsTelemetryStream(1,0);
        if(g_captureFixtureMutation==4)*(int*)(g_criticalSource+0xc)=-1;
        if(g_captureFixtureMutation==5)g_criticalActorRegistered=false;
        if(g_captureFixtureMutation==6)g_captureStatusOriginal=(CaptureStatus)captureFixtureNoStatus;
        if(g_captureFixtureMutation==7)*(int*)(g_personGuide+0xe8)=249;
        if(g_captureFixtureMutation==9)*(int*)((uint8_t*)node->person+0xa0)=5;
        if(g_captureFixtureDisposition)captureFixturePrisonerCall(self,node->person,source);
    }
}
#include "bloodroad_handoff_telemetry_cases.inc"
#include "plunder_telemetry_cases.inc"
#include "capture_base_telemetry_cases.inc"
#include "ironwall_capture_telemetry_cases.inc"
#include "lucky_capture_telemetry_cases.inc"
static void captureTelemetryTests(){
    uint8_t nativeRegionPrefix[5];memcpy(nativeRegionPrefix,(void*)site(0x4cf2f0),5);
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x24/4]=captureFixtureKind;vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=criticalFixtureOwner;
    vt[0x48/4]=captureFixtureNotPlayer;vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=(void*)site(0x495600);
    *(void**)source=*(void**)target=vt;*(int*)(source+0xa8)=4;*(int*)(target+0xa8)=2;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldDefeat=g_defeatOriginal;auto oldAttempt=g_captureAttemptOriginal;auto oldCleanup=g_defeatCleanupOriginal;
    auto oldRoll=g_captureRollOriginal;auto oldStatus=g_captureStatusOriginal;auto oldCombination=g_skills[249];
    auto oldVisual=g_bloodroadVisualOriginal;g_bloodroadVisualOriginal=(BloodroadVisual)site(0x588cb0);
    auto oldValid=g_growthValid;g_growthValid=(GrowthValid)site(0x47a630);
    auto oldLuckySkip=g_luckyCaptureSkip,oldLuckyContinue=g_luckyCaptureContinue;
    g_luckyCaptureSkip=(uint32_t)site(0x4b18cb);g_luckyCaptureContinue=(uint32_t)site(0x4b17c1);
    auto oldHas=g_captureHasResume,oldGuarantee=g_captureGuaranteeResume,old33=g_bloodroadHasResume,oldSkip=g_bloodroadSkip,oldContinue=g_bloodroadContinue;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)captureFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)captureFixtureUnit;
    auto oldReset=g_defeatUnitResetResume;g_defeatUnitResetResume=(uint32_t)site(0x4960a7);
    g_defeatOriginal=(Defeat)captureFixtureDefeat;g_captureAttemptOriginal=(CaptureAttempt)site(0x4b1280);
    g_defeatCleanupOriginal=(DefeatCleanup)captureFixtureCleanup;g_captureRollOriginal=(CaptureRoll)site(0x4721d0);g_captureStatusOriginal=(CaptureStatus)site(0x4898f0);
    g_captureHasResume=(uint32_t)site(0x4b15c3);g_captureGuaranteeResume=(uint32_t)site(0x4b1853);
    g_bloodroadHasResume=(uint32_t)site(0x4beeb4);g_bloodroadSkip=(uint32_t)site(0x4bf02d);g_bloodroadContinue=(uint32_t)site(0x4beeba);
    g_captureFixtureBegin=site(0x4beea9);g_captureFixtureCleanup=site(0x4bf186);
    g_captureFixturePrisonerCall=site(0x4b2b5a);g_captureFixtureStatusCall=site(0x4a9720);
    domesticStub(0x4bf02d,captureFixtureAfterDecision);domesticStub(0x4bf18b,captureFixtureDone);
    domesticStub(0x4b2820,captureFixtureDisposition);domesticStub(0x4a93b0,captureFixturePrisoner);
    domesticStub(0x4b2b5f,captureFixtureReturn);domesticStub(0x4a9729,captureFixtureStatusDone);
    domesticStub(0x4b0e20,captureFixtureContext);domesticStub(0x47a630,domesticValid);domesticStub(0x47a600,domesticValid);
    domesticStub(0x490b00,personFixtureLookup);domesticStub(0x491310,personFixtureId);
    domesticStub(0x4917c0,criticalFixtureUnitId);
    domesticStub(0x490aa0,captureFixtureForce);domesticStub(0x491270,captureFixtureForceId);
    domesticStub(0x49d640,captureFixtureDistance);domesticStub(0x67f750,captureFixtureFeature);
    domesticStub(0x495290,captureFixtureMember);domesticStub(0x484510,captureFixtureNeighbors);
    domesticStub(0x489090,captureFixtureAttribute);domesticStub(0x489080,captureFixtureAttribute);domesticStub(0x4a0590,captureFixtureItem);
    domesticStub(0x47be50,captureFixtureClear);domesticStub(0x47c100,captureFixtureClear);domesticStub(0x47c1b0,captureFixtureAppend);domesticStub(0x47bed0,captureFixtureNext);
    domesticStub(0x4955e0,captureFixtureLeaderBase);domesticStub(0x4cf2f0,captureFixtureRegion);
    domesticStub(0x4b9610,captureFixtureClock);domesticStub(0x47f220,afterFixtureFormat3);domesticStub(0x57f020,captureFixtureVoid1);
    domesticStub(0x4a8c00,captureFixtureVoid1);domesticStub(0x495a40,captureFixtureLeader);domesticStub(0x4a6340,captureFixtureHome);
    domesticStub(0x489d10,captureFixtureFeature);domesticStub(0x4b9480,captureFixtureVoid);domesticStub(0x4bca30,captureFixtureVoid2);
    domesticStub(0x4b0560,captureFixtureRemove);domesticStub(0x588cb0,captureFixtureVisualRemove);domesticStub(0x423230,captureFixtureVoid1);
    uint32_t resetConstants[]={0x4960d9,0x4960e2},oldResetConstants[2];
    for(int n=0;n<2;++n){oldResetConstants[n]=*(uint32_t*)site(resetConstants[n]);*(uint32_t*)site(resetConstants[n])=(uint32_t)site(0x79c1d0);}
    domesticStub(0x4a06a0,captureFixtureVoid);
    auto oldRead=*(uint32_t*)site(0x472544),oldWrite=*(uint32_t*)site(0x472551);
    *(uint32_t*)site(0x472544)=*(uint32_t*)site(0x472551)=(uint32_t)&g_captureFixtureReadablePointer;
    auto difficulty=(uint32_t*)site(0x4b13f4);auto oldDifficulty=*difficulty;*difficulty=(uint32_t)&g_captureFixtureDifficulty;
    auto cells=(uint32_t*)site(0x4b1749);auto oldCells=*cells;*cells=(uint32_t)g_criticalCells;
    float neutral=1.f,rough=1.5f,factor=0.01f;uint32_t addresses[]={0x4b1828,0x4b1830,0x4b1878};uint32_t saved[3];
    for(int n=0;n<3;++n)saved[n]=*(uint32_t*)site(addresses[n]);
    *(uint32_t*)site(addresses[0])=(uint32_t)&neutral;*(uint32_t*)site(addresses[1])=(uint32_t)&rough;*(uint32_t*)site(addresses[2])=(uint32_t)&factor;
    for(int e=0;e<LIMIT;++e)g_skills[249].effects[e]=false;g_skills[249].effects[19]=g_skills[249].effects[33]=g_skills[249].effects[32]=true;
    // The preceding pincer fixture restored both native RNG helpers.
    *(uint32_t*)site(0x4721dc)=(uint32_t)&g_defenseSeed;*(uint32_t*)site(0x4721ec)=(uint32_t)&g_defenseSeed;
    auto setup=[&](int provider,int bloodroad,int count,int item,int luck,int disposition,int range,int seed){
        *(int*)(actor+0xe8)=*(int*)(other+0xe8)=*(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        if(provider!=4)*(int*)((provider%2?other:actor)+0xe8)=provider>=2?249:19;
        if(provider==5){*(int*)(actor+0xe8)=249;*(int*)(other+0xe8)=19;}
        *(int*)(target+0xc)=1000;*(int*)(target+0x10)=count==2?601:-1;*(int*)(target+0x14)=-1;
        if(luck&1)*(int*)(person+0xe8)=32;if(luck&2)*(int*)(spouse+0xe8)=32;
        if(bloodroad)*(int*)((bloodroad==2?spouse:person)+0xe8)=bloodroad==3?249:33;
        for(auto who:{person,spouse}){*(void**)who=vt;*(int*)(who+0xa0)=3;*(int*)(who+0x98)=5;*(int*)(who+0x9c)=4181;}
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
        *(int*)(target+8)=0;*(int*)(target+0xa8)=2;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;g_defenseSeed=seed;
        g_captureFixtureRange=range;g_captureFixtureDisposition=disposition;g_captureFixtureItemMask=item;
        g_captureFixtureMutation=g_captureFixtureCandidates=g_captureFixtureNodeCount=0;g_captureFixtureRemove=true;
        g_captureFixtureCleanupMutation=0;g_captureStatusOriginal=(CaptureStatus)site(0x4898f0);
    };
    auto run=(Defeat)site(0x4be790);int commands=0;assert(skilltelemetry::open(L"build/capture_test.csv",afterFixtureActor,nullptr));
    for(int provider=0;provider<6;++provider)for(int bloodroad:{0,1,2,3})for(int count:{1,2})for(int item:{0,1})
        for(int luck:{0,1})for(int disposition:{0,1})for(int range:{0,1})for(int seed:{0,4}){
            setup(provider,bloodroad,count,item,luck,disposition,range,seed);uint32_t state=seed;int captured=0;
            bool blocked=bloodroad==1 || bloodroad==3 || (bloodroad==2 && count==2);
            int statuses[2]={3,3};
            if(!blocked)for(int slot=0;slot<count;++slot){
                if(!slot && (item || luck))continue;
                state=state*0x6c078965u+0x3039u;int chance=provider!=4 && range?100:5;
                if((state>>16)%100<(unsigned)chance){++captured;if(disposition)statuses[slot]=5;}
            }
            run(nullptr,target,source,0,0,0);
            assert(g_defenseSeed==state && g_captureFixtureCandidates==captured);
            for(int slot=0;slot<count;++slot){auto who=slot?spouse:person;
                assert(*(int*)(who+0xa0)==statuses[slot] && *(int*)(who+0x9c)==(statuses[slot]==5?4181:5));}
            assert(!g_defeat && !g_captureAttempt && !g_criticalTargetRegistered);
            assert(g_movementUnitById(nullptr,4094)==target && defeatUnitCleared(target)); // real fixed slot survives destruction
            ++commands;
        }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/capture_edges_test.csv",afterFixtureActor,nullptr));
    for(int mutation=1;mutation<=9;++mutation){setup(0,0,1,0,0,1,1,4);g_captureFixtureMutation=mutation;run(nullptr,target,source,0,0,0);
        assert(*(int*)(person+0xa0)==(mutation==6?3:5) && !g_defeat && !g_captureAttempt);}
    for(int mutation=1;mutation<=7;++mutation){setup(0,1,2,0,0,1,1,4);g_captureFixtureCleanupMutation=mutation;run(nullptr,target,source,0,0,0);
        assert(g_defenseSeed==4 && !g_defeat && !g_captureAttempt);
        assert(defeatUnitCleared(target)==(mutation!=4) && g_criticalTargetRegistered==(mutation==4));}
    setup(0,1,2,0,0,1,1,4);*(int*)(target+0x10)=1000;run(nullptr,target,source,0,0,0); // same person in two slots: one outcome
    setup(0,0,1,0,0,1,1,4);captureFixtureDefeat(nullptr,nullptr,target,source,0,0,0);assert(*(int*)(person+0xa0)==5); // unscoped
    skilltelemetry::close();setup(0,0,1,0,0,1,1,4);run(nullptr,target,source,0,0,0);assert(*(int*)(person+0xa0)==5 && g_defenseSeed==(4u*0x6c078965u+0x3039u));
    setup(0,1,2,0,0,1,1,4);run(nullptr,target,source,0,0,0);
    assert(!g_criticalTargetRegistered && g_defenseSeed==4 && *(int*)(person+0xa0)==3 && *(int*)(spouse+0xa0)==3 &&
        *(int*)(person+0x9c)==5 && *(int*)(spouse+0x9c)==5);
    captureBaseTelemetryTests(run,setup,source,target,vt,nativeRegionPrefix);
    plunderTelemetryTests(run,setup,source,target);
    bloodroadHandoffTests(run,setup,source,target,person,spouse);
    ironwallCaptureTelemetryTests(setup,vt);
    luckyCaptureTelemetryTests(setup,vt);
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;
    g_defeatOriginal=oldDefeat;g_captureAttemptOriginal=oldAttempt;g_defeatCleanupOriginal=oldCleanup;g_captureRollOriginal=oldRoll;g_captureStatusOriginal=oldStatus;
    g_captureHasResume=oldHas;g_captureGuaranteeResume=oldGuarantee;g_bloodroadHasResume=old33;g_bloodroadSkip=oldSkip;g_bloodroadContinue=oldContinue;
    g_skills[249]=oldCombination;*difficulty=oldDifficulty;*cells=oldCells;for(int n=0;n<3;++n)*(uint32_t*)site(addresses[n])=saved[n];
    *(uint32_t*)site(0x472544)=oldRead;*(uint32_t*)site(0x472551)=oldWrite;
    g_defeatUnitResetResume=oldReset;for(int n=0;n<2;++n)*(uint32_t*)site(resetConstants[n])=oldResetConstants[n];
    g_bloodroadVisualOriginal=oldVisual;
    g_growthValid=oldValid;g_luckyCaptureSkip=oldLuckySkip;g_luckyCaptureContinue=oldLuckyContinue;
    printf("PASS capture19/bloodroad33: %d combinations, whole native candidate/cleanup routines, exact RNG, real prisoner/location/acted stores, release/item/32 guards, identity/context/membership/dedup/disabled exclusions\n",commands);
}
