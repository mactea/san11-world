// Native classifier tails, difficulty/player/RNG gates, three actual classifier
// CALL sites and modal state transport execute; command preconditions, arithmetic
// before these tails, UI contents and treaty/prisoner mutations are fixture controlled.
static uintptr_t g_diplomacyTailEntry,g_diplomacyDecisionEntry,g_diplomacyInteractionEntry;
static int g_diplomacyFixtureKind=16,g_diplomacyDifficulty=0,g_diplomacyPlayer=0;
static int g_diplomacySample=1,g_diplomacySampleCalls=0,g_diplomacyScore=0,g_diplomacyBaseScore=10;
static int g_diplomacyUIResult=0,g_diplomacyUICalls=0,g_diplomacyConsumed=-1,g_diplomacyCommandReturn=1;
static bool g_diplomacyInteractive=false,g_diplomacyActorRegistered=true,g_diplomacyForceRegistered=true;
static uint8_t* g_diplomacyFixtureForce;
static int g_diplomacyFixtureForceId=3;
static uint8_t g_diplomacyUIState[0x40]={};
static int g_diplomacyFixtureFault=0;
static int g_diplomacyPreModalFault=0;
static void (*g_diplomacyContestUIFixture)()=nullptr;
static void** g_diplomacyFixtureCommand;
static int __fastcall diplomacyFixturePlayer(void*,void*) {return g_diplomacyPlayer;}
static int __cdecl diplomacyFixtureRoll(int percent) {assert(percent==20);++g_diplomacySampleCalls;
    if(g_diplomacyFixtureFault==13)skilltelemetry::invalidateContext();return g_diplomacySample;}
static int __fastcall diplomacyFixturePersonId(void* w,void* unused,const void* person) {
    return person==g_personGuide && !g_diplomacyActorRegistered?-1:personFixtureId(w,unused,person);
}
static int __fastcall diplomacyFixtureForceId(void*,void*,void* force) {
    return g_diplomacyForceRegistered && force==g_diplomacyFixtureForce?g_diplomacyFixtureForceId:-1;
}
static uint8_t* __fastcall diplomacyFixtureForce(void*,void*,int id) {
    return g_diplomacyForceRegistered && id==g_diplomacyFixtureForceId?g_diplomacyFixtureForce:nullptr;
}
static void __fastcall diplomacyFixtureClear(void*,void*) {}
static void __fastcall diplomacyFixtureScreen(void*,void*,int a,int b,int c,int d) {assert(a==25 && b==29 && c==1 && d==1);}
static void __fastcall diplomacyFixtureUI(void*,void*) {
    ++g_diplomacyUICalls;if(g_diplomacyContestUIFixture)g_diplomacyContestUIFixture();
    *(int*)(g_diplomacyUIState+0x2c)=g_diplomacyUIResult;
    if(g_diplomacyFixtureFault==1)skilltelemetry::invalidateContext();
    if(g_diplomacyFixtureFault==2)*(int*)(g_personGuide+0xe8)=-1;
    if(g_diplomacyFixtureFault==3)g_diplomacyActorRegistered=false;
    if(g_diplomacyFixtureFault==4)g_diplomacyForceRegistered=false;
    if(g_diplomacyFixtureFault==5)g_diplomacyFixtureCommand[1]=g_personOther;
    if(g_diplomacyFixtureFault==6)g_diplomacyFixtureCommand[2]=nullptr;
    if(g_diplomacyFixtureFault==7)*(int*)(g_diplomacyFixtureForce+4)=1000;
    if(g_diplomacyFixtureFault==8)++*(int*)(g_personGuide+0x9c);
    if(g_diplomacyFixtureFault==9)++*(int*)(g_personGuide+0x98);
    if(g_diplomacyFixtureFault==10)g_personOther=nullptr;
    if(g_diplomacyFixtureFault==11)g_skills[249].effects[86]=false;
    if(g_diplomacyFixtureFault==12)++g_diplomacyFixtureForceId;
}
__declspec(naked) static int __fastcall diplomacyFixtureClassifier(void*,void*,int*) {
    __asm {cmp g_diplomacyFixtureKind,16
        je type16
        cmp g_diplomacyFixtureKind,18
        je type18
        sub esp,0x20
        jmp prepared
    type16:
        sub esp,8
        jmp prepared
    type18:
        sub esp,0x10
    prepared:
        push ebx
        push esi
        push edi
        mov esi,ecx
        mov ebx,g_diplomacyBaseScore
        mov edi,g_diplomacyScore
        mov eax,g_diplomacyTailEntry
        jmp eax}
}
__declspec(naked) static int __cdecl runDiplomacyDecision(void* command) {
    __asm {push ebp
        push esi
        mov esi,[esp+12]
        mov ecx,esi
        mov edx,[ecx]
        push 0
        mov eax,g_diplomacyDecisionEntry
        jmp eax}
}
__declspec(naked) static void diplomacyFixtureDecisionDone() {__asm {pop esi
    pop ebp
    ret}}
__declspec(naked) static int __cdecl runDiplomacyInteraction(void* command,int initial) {
    __asm {mov eax,[esp+8]
        push eax
        mov eax,[esp+8]
        push eax
        mov eax,g_diplomacyInteractionEntry
        jmp eax}
}
__declspec(naked) static void diplomacyFixtureInteractionDone() {__asm {add esp,8
    ret}}
static int __cdecl diplomacyFixtureExecute(uint8_t* command) {
    int result=runDiplomacyDecision(command);
    switch(g_diplomacyPreModalFault){
    case 1:skilltelemetry::invalidateContext();break;
    case 2:*(int*)(g_personGuide+0xe8)=-1;break;
    case 3:g_diplomacyActorRegistered=false;break;
    case 4:g_diplomacyForceRegistered=false;break;
    case 5:*(int*)(g_diplomacyFixtureForce+4)=1000;break;
    case 6:g_diplomacyFixtureCommand[1]=g_personOther;break;
    case 7:g_diplomacyFixtureCommand[2]=nullptr;break;
    case 8:++*(int*)(g_personGuide+0x9c);break;
    case 9:g_skills[249].effects[86]=false;break;
    }
    if(g_diplomacyInteractive)result=runDiplomacyInteraction(command,result);
    g_diplomacyConsumed=result;return g_diplomacyCommandReturn;
}
__declspec(naked) static void diplomacyFixture16Prolog() {__asm {add esp,0x3d4
    mov eax,31
    ret}}
__declspec(naked) static void diplomacyFixture18Prolog() {__asm {add esp,0x3d8
    mov eax,31
    ret}}
__declspec(naked) static void diplomacyFixture20Prolog() {__asm {add esp,0x3d0
    mov eax,31
    ret}}
#include "diplomacy_eligibility_telemetry_cases.inc"
#include "diplomacy_interaction_telemetry_cases.inc"
#include "diplomacy_contest_telemetry_cases.inc"
static void diplomacyTelemetryTests() {
    uint8_t actor[400]={},other[400]={},person[400]={},spouse[400]={},force[0x130]={};
    g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* actorVt[24]={};actorVt[0x48/4]=diplomacyFixturePlayer;*(void**)actor=actorVt;
    void* commandVt[8]={};commandVt[0x14/4]=diplomacyFixtureClassifier;
    void* command[]={commandVt,actor,force,nullptr,nullptr,nullptr};
    g_diplomacyFixtureCommand=command;
    g_diplomacyFixtureForce=force;*(int*)(force+4)=14162;
    auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryPersonId;auto oldForce=g_diplomacyForce;auto oldForceId=g_diplomacyForceId;
    auto old16=g_diplomacy16Original,old18=g_diplomacy18Original,old20=g_diplomacy20Original;
    auto oldModal=g_diplomacyInteractionOriginal;
    auto oldModalClear=g_diplomacyModalClearOriginal;auto oldModalState=g_diplomacyModalState;
    g_diplomacyModalClearOriginal=(DiplomacyModalClear)site(0x47be50);g_diplomacyModalState=g_diplomacyUIState;
    auto old16Resume=g_diplomacy16Resume,old18Resume=g_diplomacy18Resume,old20Resume=g_diplomacy20Resume;
    auto old16Decision=g_diplomacy16DecisionResume,old18Decision=g_diplomacy18DecisionResume,old20Decision=g_diplomacy20DecisionResume;
    auto old16Forced=g_diplomacy16ForcedResume,old18Forced=g_diplomacy18ForcedResume,old20Forced=g_diplomacy20ForcedResume;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)diplomacyFixturePersonId;
    g_diplomacyForce=(PersonById)diplomacyFixtureForce;g_diplomacyForceId=(BuildingId)diplomacyFixtureForceId;
    g_diplomacy16Resume=(uint32_t)diplomacyFixture16Prolog;g_diplomacy18Resume=(uint32_t)diplomacyFixture18Prolog;g_diplomacy20Resume=(uint32_t)diplomacyFixture20Prolog;
    for(auto entry:{0x5b2250,0x5b6510,0x5cc650})assert(((DiplomacyExecute)site(entry))((uint8_t*)command)==31);
    g_diplomacy16Resume=old16Resume;g_diplomacy18Resume=old18Resume;g_diplomacy20Resume=old20Resume;
    g_diplomacy16Original=g_diplomacy18Original=g_diplomacy20Original=diplomacyFixtureExecute;
    g_diplomacyInteractionOriginal=(DiplomacyInteraction)site(0x5bd790);
    g_diplomacy16DecisionResume=(uint32_t)site(0x5b22ac);g_diplomacy18DecisionResume=(uint32_t)site(0x5b653e);g_diplomacy20DecisionResume=(uint32_t)site(0x5cc6c1);
    g_diplomacy16ForcedResume=(uint32_t)site(0x5b212d);g_diplomacy18ForcedResume=(uint32_t)site(0x5b63b3);g_diplomacy20ForcedResume=(uint32_t)site(0x5cc516);
    domesticStub(0x5b22ac,diplomacyFixtureDecisionDone);domesticStub(0x5b653e,diplomacyFixtureDecisionDone);
    domesticStub(0x5cc6c3,diplomacyFixtureDecisionDone);
    for(auto a:{0x5b22cd,0x5b655f,0x5cc6de})domesticStub(a,diplomacyFixtureInteractionDone);
    domesticStub(0x4721d0,diplomacyFixtureRoll);domesticStub(0x47be50,diplomacyFixtureClear);
    domesticStub(0x4d5290,diplomacyFixtureScreen);domesticStub(0x57dda0,diplomacyFixtureUI);
    uint32_t difficultyOperands[]={0x5b2103,0x5b6389,0x5cc4ec},savedDifficulty[3];
    for(int i=0;i<3;++i){auto p=(uint32_t*)site(difficultyOperands[i]);savedDifficulty[i]=*p;*p=(uint32_t)&g_diplomacyDifficulty;}
    uint32_t modalOperands[]={0x5bd79d,0x5bd7a2,0x5bd7a8,0x5bd7b1,0x5bd7b7,0x5bd7e1},savedModal[6];
    int modalOffsets[]={0x2c,4,0,0x24,0x28,0x2c};
    for(int i=0;i<6;++i){auto p=(uint32_t*)site(modalOperands[i]);savedModal[i]=*p;*p=(uint32_t)(g_diplomacyUIState+modalOffsets[i]);}
    bool old86=g_skills[249].effects[86];g_skills[249].effects[86]=true;
    auto select=[&](int kind) {
        g_diplomacyFixtureKind=kind;g_diplomacyTailEntry=site(kind==16?0x5b20f3:kind==18?0x5b6379:0x5cc4dc);
        g_diplomacyDecisionEntry=site(kind==16?0x5b22a7:kind==18?0x5b6539:0x5cc6bc);
        g_diplomacyInteractionEntry=site(kind==16?0x5b22c8:kind==18?0x5b655a:0x5cc6d9);
        return (DiplomacyExecute)site(kind==16?0x5b2250:kind==18?0x5b6510:0x5cc650);
    };
    assert(skilltelemetry::open(L"build/diplomacy_telemetry_test.csv",personFixtureActor,nullptr));
    for(int kind:{16,18,20}) {
        auto run=select(kind);
        for(int owned:{86,249})for(int difficulty:{0,2})for(int player:{0,1})for(int sample:{0,1})for(bool interactive:{false,true})for(int final:{0,1,2}) {
            *(int*)(actor+0xe8)=owned;g_diplomacyDifficulty=difficulty;g_diplomacyPlayer=player;g_diplomacySample=sample;
            g_diplomacyInteractive=interactive;g_diplomacyUIResult=final;g_diplomacyFixtureForceId=difficulty?249:3;
            int calls=g_diplomacySampleCalls,ui=g_diplomacyUICalls;
            assert(run((uint8_t*)command)==1 && !g_diplomacyExecution && !g_diplomacyDecision && !g_progressionProbe);
            bool rolled=difficulty==2 && player==1;
            assert(g_diplomacySampleCalls==calls+rolled && g_diplomacyUICalls==ui+interactive);
            assert(g_diplomacyConsumed==(interactive?final:rolled && !sample?2:1));
            if(interactive)assert(*(int*)g_diplomacyUIState==2 && *(void**)(g_diplomacyUIState+0x24)==actor && *(int*)(g_diplomacyUIState+0x28)==1);
        }
        *(int*)(actor+0xe8)=-1;g_diplomacyInteractive=false;assert(run((uint8_t*)command)==1 && g_diplomacyConsumed==2);
        *(int*)(actor+0xe8)=86;g_diplomacyDifficulty=2;g_diplomacyPlayer=1;g_diplomacySample=0;g_diplomacyScore=1000;
        assert(run((uint8_t*)command)==1 && g_diplomacyConsumed==1);g_diplomacyScore=0; // ordinary result1, no forced branch
        g_diplomacyDifficulty=0;g_diplomacyActorRegistered=false;assert(run((uint8_t*)command)==1);g_diplomacyActorRegistered=true;
        g_diplomacyCommandReturn=0;assert(run((uint8_t*)command)==0);g_diplomacyCommandReturn=1;
        g_diplomacyForceRegistered=false;assert(run((uint8_t*)command)==1);g_diplomacyForceRegistered=true;
        ((int(__thiscall*)(void*,int*))diplomacyFixtureClassifier)(command,nullptr); // generic/AI preview has no actual-command scope
        *(void**)other=actorVt;*(int*)(other+0xe8)=86;command[1]=other;*(int*)(force+4)=1000;
        g_diplomacyInteractive=true;g_diplomacyUIResult=2;g_diplomacyDifficulty=2;g_diplomacyPlayer=1;g_diplomacySample=1;
        int calls=g_diplomacySampleCalls;
        assert(run((uint8_t*)command)==1 && g_diplomacyConsumed==2 && g_diplomacySampleCalls==calls+1);
        command[1]=actor;*(int*)(force+4)=14162;
    }
    skilltelemetry::close();assert(!g_diplomacyExecution && !g_diplomacyDecision && !g_progressionProbe);
    assert(skilltelemetry::open(L"build/diplomacy_owner_edges_test.csv",personFixtureActor,nullptr));
    for(int kind:{16,18,20})for(int owned:{86,249})for(int fault=0;fault<14;++fault){
        if(fault==11 && owned==86)continue;
        g_personOther=other;command[1]=actor;command[2]=force;*(int*)(force+4)=14162;
        memset(actor+0x98,0,16);*(int*)(actor+0xe8)=owned;g_skills[249].effects[86]=true;
        g_diplomacyActorRegistered=g_diplomacyForceRegistered=true;g_diplomacyFixtureForceId=249;
        g_diplomacyFixtureFault=fault;g_diplomacyDifficulty=2;g_diplomacyPlayer=1;g_diplomacySample=1;
        g_diplomacyInteractive=true;g_diplomacyUIResult=2;int calls=g_diplomacySampleCalls,ui=g_diplomacyUICalls;
        assert(select(kind)((uint8_t*)command)==1 && g_diplomacyConsumed==2);
        assert(g_diplomacySampleCalls==calls+1 && g_diplomacyUICalls==ui+1);
        assert(!g_diplomacyExecution && !g_diplomacyDecision && !g_progressionProbe);
    }
    skilltelemetry::close();g_diplomacyFixtureFault=0;g_personOther=other;
    command[1]=actor;command[2]=force;*(int*)(force+4)=14162;
    g_diplomacyActorRegistered=g_diplomacyForceRegistered=true;
    *(int*)(actor+0xe8)=249;g_skills[249].effects[86]=true;
    int disabledCalls=g_diplomacySampleCalls,disabledUI=g_diplomacyUICalls;
    assert(select(20)((uint8_t*)command)==1 && g_diplomacyConsumed==2);
    assert(g_diplomacySampleCalls==disabledCalls+1 && g_diplomacyUICalls==disabledUI+1);
    diplomacyInteractionTelemetryTests(command,actor,other,force,select);
    diplomacyContestTelemetryTests(command,actor,other,person,spouse,force,select);
    diplomacyEligibilityTelemetryTests(command,commandVt,actor,other,force);
    g_skills[249].effects[86]=old86;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldId;g_diplomacyForce=oldForce;g_diplomacyForceId=oldForceId;
    g_diplomacy16Original=old16;g_diplomacy18Original=old18;g_diplomacy20Original=old20;g_diplomacyInteractionOriginal=oldModal;
    g_diplomacyModalClearOriginal=oldModalClear;g_diplomacyModalState=oldModalState;
    g_diplomacy16DecisionResume=old16Decision;g_diplomacy18DecisionResume=old18Decision;g_diplomacy20DecisionResume=old20Decision;
    g_diplomacy16ForcedResume=old16Forced;g_diplomacy18ForcedResume=old18Forced;g_diplomacy20ForcedResume=old20Forced;
    for(int i=0;i<3;++i)*(uint32_t*)site(difficultyOperands[i])=savedDifficulty[i];
    for(int i=0;i<6;++i)*(uint32_t*)site(modalOperands[i])=savedModal[i];
    puts("PASS native diplomacy:86 forced branches, actual-call scopes, difficulty/player/single RNG gates, native modal state, categorical command outcomes and preview exclusions");
}
