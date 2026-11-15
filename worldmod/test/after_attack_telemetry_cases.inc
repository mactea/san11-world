// Native49/51 routines,52/32 victim-filter branches and actual status/health
// stores run. World/UI primitives, small candidate list storage and shuffle
// are isolated fixtures; no native victim-shuffle/RNG claim is made.
static uint8_t *g_afterSource,*g_afterTarget;
static bool g_afterSourceRegistered=true,g_afterTargetRegistered=true,g_afterActorRegistered=true;
static bool g_afterChance=true;
static int g_afterChanceCalls=0,g_afterDurationSample=0,g_afterDurationCalls=0,g_afterPick=0;
static uintptr_t g_afterNodes[3][3]={};
static int __cdecl afterFixtureValid(void* p){return p!=nullptr;}
static int __fastcall afterFixtureId(void*,void*,void* unit){return unit==g_afterSource?4095:unit==g_afterTarget?4094:-1;}
static void* __fastcall afterFixtureUnit(void*,void*,int id){
    return id==4095 && g_afterSourceRegistered?g_afterSource:id==4094 && g_afterTargetRegistered?g_afterTarget:nullptr;
}
static int __fastcall afterFixturePersonId(void* w,void* unused,const void* person){
    return g_afterActorRegistered?personFixtureId(w,unused,person):-1;
}
static int __fastcall afterFixtureOwner(void* unit,void*){return unit==g_afterSource?2:3;}
static const uint32_t* __fastcall afterFixturePosition(uint8_t* unit,void*){return (const uint32_t*)(unit+4);}
static int __fastcall afterFixtureFind(uint8_t* unit,void*,int effect){
    for(int n=0;n<3;++n){int id=*(int*)(unit+0xc+4*n);auto p=personFixtureLookup(nullptr,nullptr,id);
        if(p && personHas(p,nullptr,effect))return id;}
    return -1;
}
static bool afterFixtureActor(const void* p,skilltelemetry::Actor& actor){
    personFixtureActor(p,actor);
    if(p==g_personTarget || p==g_personSpouse){actor.force=3;actor.city=60;actor.ruler=700;}
    return true;
}
static int __cdecl afterFixtureChance(int percent){assert(percent==50);++g_afterChanceCalls;return g_afterChance;}
static uint32_t __cdecl afterFixtureDuration(int bound){assert(bound==2);++g_afterDurationCalls;return 0x230000|g_afterDurationSample;}
static void* __fastcall afterFixtureLeader(void*,void*){return g_personTarget;}
static int __fastcall afterFixtureRelative(void*,void*){return -1;}
static void* __fastcall afterFixtureNoForce(void*,void*,int){return nullptr;}
static int __cdecl afterFixtureHidden(void*){return 1;}
static const char* __fastcall afterFixtureFormat2(void*,void*,int,void*){return "fixture";}
static const char* __fastcall afterFixtureFormat3(void*,void*,int,void*,void*){return "fixture";}
static const char* __cdecl afterFixtureText(const char* p){return p;}
static void __cdecl afterFixtureMessage(const char*,void*,int){}
static void __cdecl afterFixtureMessage4(const char*,void*,void*,int){}
static void __fastcall afterFixtureUnitMessage(void*,void*,void*,const char*,int){}
static void __fastcall afterFixtureAnimation(void*,void*,int,void*){}
static void __fastcall afterFixtureUiEvent(void*,void*,int){}
static void __fastcall afterFixtureRecalc(void*,void*){}
static int __fastcall afterFixtureMemberCount(void*,void*){return 2;}
static int __fastcall afterFixtureMember(uint8_t* unit,void*,int n){assert(n>=0 && n<2);return *(int*)(unit+0xc+4*n);}
static void __fastcall afterFixtureAppend(uint8_t* list,void*,void* person){
    int n=*(int*)(list+0xc);assert(n>=0 && n<3);
    g_afterNodes[n][2]=(uintptr_t)person;*(int*)(list+0xc)=n+1;
    if(!n)*(uintptr_t*)(list+4)=(uintptr_t)g_afterNodes[0];
    *(uintptr_t*)(list+8)=(uintptr_t)g_afterNodes[n];
}
static void __fastcall afterFixtureListDestroy(void*,void*){}
static void __cdecl afterFixturePick(uint8_t* list){
    assert(g_afterPick>=0 && g_afterPick<*(int*)(list+0xc));*(uintptr_t*)(list+4)=(uintptr_t)g_afterNodes[g_afterPick];
}
static void __stdcall afterFixtureLock(void*){}
static void(__stdcall* g_afterFixtureLockPointer)(void*)=afterFixtureLock;
static uintptr_t g_afterSelector;
__declspec(naked) static void* __cdecl afterFixtureSelect(void* unit){__asm {push ebx
    mov ebx,[esp+8]
    call dword ptr [g_afterSelector]
    pop ebx
    ret}}
static void __fastcall afterFixtureNoStatus(void*,void*,void*,void*,int,int,int){}
static void __fastcall afterFixtureNoInjury(void*,void*,void*,int){}
static void __fastcall afterFixtureStatusWorld(void* self,void*,void* unit,void* source,int status,int turns,int display){
    ((StatusGrant)site(0x4aea70))(self,unit,source,status,turns,display);skillsTelemetryStream(1,0);
}
static void __fastcall afterFixtureInjuryWorld(void* self,void*,void* person,int requested){
    ((InjuryGrant)site(0x4a5690))(self,person,requested);skillsTelemetryStream(1,0);
}
#include "casualty_telemetry_cases.inc"
#include "fire_aftermath_telemetry_cases.inc"
static void afterAttackTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_afterSource=source;g_afterTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[24]={};vt[0x3c/4]=afterFixturePosition;vt[0x40/4]=afterFixtureOwner;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=afterFixtureFind;*(void**)source=*(void**)target=vt;
    *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
    *(int*)(target+0xc)=1000;*(int*)(target+0x10)=601;*(int*)(target+0x14)=-1;
    *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(int*)(source+0xa8)=4;*(int*)(target+0xa8)=2;
    auto oldDash=g_dashingResume,oldMighty=g_mightyResume,oldGuard=g_injuryGuardResume;
    auto oldStatus=g_dashingStatusOriginal;auto oldInjury=g_mightyInjuryOriginal;auto oldTurns=g_statusTurnsOriginal;auto oldSkill=g_injurySkillOriginal;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldUnitId=g_telemetryUnitId;
    auto oldUnit=g_movementUnitById;auto oldValid=g_growthValid;
    g_dashingResume=(uint32_t)site(0x593b46);g_mightyResume=(uint32_t)site(0x5977f5);g_injuryGuardResume=(uint32_t)site(0x597220);
    g_dashingStatusOriginal=(StatusGrant)site(0x4aea70);g_mightyInjuryOriginal=(InjuryGrant)site(0x4a5690);
    g_statusTurnsOriginal=(StatusTurnsSet)site(0x496350);g_injurySkillOriginal=(PersonSkill)site(0x4890f0);
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)afterFixturePersonId;
    g_telemetryUnitId=(BuildingId)afterFixtureId;g_movementUnitById=(MovementUnitById)afterFixtureUnit;g_growthValid=afterFixtureValid;
    domesticStub(0x47a630,afterFixtureValid);domesticStub(0x490b00,personFixtureLookup);domesticStub(0x490aa0,afterFixtureNoForce);
    domesticStub(0x495a40,afterFixtureLeader);domesticStub(0x489d40,afterFixtureRelative);domesticStub(0x5a0700,afterFixtureHidden);
    domesticStub(0x598350,afterFixtureFormat3);domesticStub(0x4b9450,afterFixtureFormat3);
    domesticStub(0x5c1a40,afterFixtureFormat3);domesticStub(0x4b9320,afterFixtureFormat2);
    domesticStub(0x49b390,afterFixtureText);domesticStub(0x4f6710,afterFixtureMessage);domesticStub(0x4f6690,afterFixtureMessage);
    domesticStub(0x63b180,afterFixtureUnitMessage);domesticStub(0x589980,afterFixtureAnimation);
    domesticStub(0x4f5590,afterFixtureMessage4);domesticStub(0x588f20,afterFixtureUiEvent);
    domesticStub(0x48a2d0,afterFixtureRecalc);domesticStub(0x4721d0,afterFixtureChance);domesticStub(0x472150,afterFixtureDuration);
    domesticStub(0x4953d0,afterFixtureMemberCount);domesticStub(0x495290,afterFixtureMember);
    domesticStub(0x47c1b0,afterFixtureAppend);domesticStub(0x47c100,afterFixtureListDestroy);domesticStub(0x5970e0,afterFixturePick);
    g_afterSelector=site(0x5971f0);
    uint32_t imports[]={0x5972f5,0x59730b,0x597315},savedImports[3];
    for(int n=0;n<3;++n){savedImports[n]=*(uint32_t*)site(imports[n]);*(uint32_t*)site(imports[n])=(uint32_t)&g_afterFixtureLockPointer;}
    uint32_t names[4]={};auto nameOperand=(uint32_t*)site(0x59776c);uint32_t oldNames=*nameOperand;*nameOperand=(uint32_t)names;
    int effects[]={32,49,51,52};bool saved[4];for(int n=0;n<4;++n)saved[n]=g_skills[249].effects[effects[n]];
    auto setup=[&](int effect,int provider){
        for(int e:effects)g_skills[249].effects[e]=false;g_skills[249].effects[effect]=true;
        *(int*)(actor+0xe8)=provider==0?effect:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==1?effect:provider==2?effect:provider==3?249:-1;
        *(int*)(person+0xe8)=*(int*)(spouse+0xe8)=-1;*(int*)(person+0x15c)=*(int*)(spouse+0x15c)=0;
        source[0xc9]=100;target[0xc9]=70;*(int*)(target+0x24)=0;target[0x28]=0;
        g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;
        g_afterChance=true;g_afterPick=0;g_afterDurationSample=0;
    };
    auto dash=(DashingAttempt)site(0x593b40);auto mighty=(MightyAttempt)site(0x5977f0);
    assert(!skilltelemetry::enabled());setup(49,0);dash(source,target);
    assert(*(int*)(target+0x24)==1 && target[0x28]==1 && !g_afterAttack && !g_statusWriteProbe);
    setup(51,0);mighty(nullptr,target,source);assert(*(int*)(person+0x15c)==1 && !g_afterAttack);
    assert(skilltelemetry::open(L"build/after_attack_test.csv",afterFixtureActor,nullptr));
    for(int provider=0;provider<4;++provider)for(int status:{0,1,2})for(int before:{0,4,5})for(int sample:{0,1}){
        setup(49,provider);*(int*)(target+0x24)=status;target[0x28]=(uint8_t)before;g_afterDurationSample=sample;
        int calls=g_afterDurationCalls;dash(source,target);
        assert(*(int*)(target+0x24)==1 && target[0x28]==(before+sample+1>5?5:before+sample+1) && g_afterDurationCalls==calls+1);
    }
    for(int provider=0;provider<4;++provider)for(bool chance:{false,true})for(int level:{0,1,2,3})for(int pick:{0,1}){
        setup(51,provider);g_afterChance=chance;g_afterPick=pick;*(int*)(person+0x15c)=*(int*)(spouse+0x15c)=level;
        int calls=g_afterChanceCalls;mighty(nullptr,target,source);
        assert(g_afterChanceCalls==calls+1);
        assert(*(int*)((pick?spouse:person)+0x15c)==(chance && level<3?level+1:level));
    }
    for(int sourceOwned:{51,249})for(int guardOwned:{52,249})for(bool chance:{false,true}){
        setup(51,sourceOwned==51?0:2);g_skills[249].effects[52]=true;*(int*)(person+0xe8)=guardOwned;g_afterChance=chance;
        mighty(nullptr,target,source);assert(!*(int*)(person+0x15c) && !*(int*)(spouse+0x15c));
    }
    for(int sourceOwned:{51,249})for(int protectedOwned:{32,249})for(int mask:{1,2,3}){
        setup(51,sourceOwned==51?0:2);g_skills[249].effects[32]=true;
        if(mask&1)*(int*)(person+0xe8)=protectedOwned;if(mask&2)*(int*)(spouse+0xe8)=protectedOwned;
        mighty(nullptr,target,source);
        assert(*(int*)(person+0x15c)==(mask==2?1:0) && *(int*)(spouse+0x15c)==(mask==1?1:0));
    }
    for(int guardOwned:{52,249}){
        setup(51,0);g_skills[249].effects[52]=g_skills[249].effects[32]=true;
        *(int*)(person+0xe8)=guardOwned;*(int*)(spouse+0xe8)=32;mighty(nullptr,target,source);
    }
    setup(51,0);*(int*)(person+0xe8)=32;afterFixtureSelect(target); // preview exclusion is query-only
    setup(49,0);*(int*)(actor+0xe8)=-1;int durationCalls=g_afterDurationCalls;dash(source,target);
    assert(!*(int*)(target+0x24) && !target[0x28] && g_afterDurationCalls==durationCalls);
    setup(51,0);*(int*)(actor+0xe8)=-1;int chanceCalls=g_afterChanceCalls;mighty(nullptr,target,source);
    assert(!*(int*)(person+0x15c) && g_afterChanceCalls==chanceCalls);
    for(int power:{100,101}){setup(49,0);target[0xc9]=(uint8_t)power;int calls=g_afterDurationCalls;dash(source,target);assert(g_afterDurationCalls==calls);}
    for(int invalid=0;invalid<3;++invalid){
        setup(49,0);if(invalid==0)g_afterSourceRegistered=false;if(invalid==1)g_afterTargetRegistered=false;
        if(invalid==2)g_afterActorRegistered=false;dash(source,target);
    }
    setup(49,0);*(int*)(target+0x24)=1;target[0x28]=5;g_dashingStatusOriginal=(StatusGrant)afterFixtureNoStatus;
    dash(source,target);g_dashingStatusOriginal=(StatusGrant)site(0x4aea70); // unchanged confused state without a native write is not an event
    setup(51,0);g_mightyInjuryOriginal=(InjuryGrant)afterFixtureNoInjury;mighty(nullptr,target,source);
    g_mightyInjuryOriginal=(InjuryGrant)site(0x4a5690);
    setup(49,0);g_dashingStatusOriginal=(StatusGrant)afterFixtureStatusWorld;dash(source,target);
    g_dashingStatusOriginal=(StatusGrant)site(0x4aea70);
    setup(51,0);g_mightyInjuryOriginal=(InjuryGrant)afterFixtureInjuryWorld;mighty(nullptr,target,source);
    g_mightyInjuryOriginal=(InjuryGrant)site(0x4a5690);
    assert(!g_afterAttack && !g_statusWriteProbe && !g_progressionProbe);skilltelemetry::close();
    casualtyTelemetryTests(setup,source,target,person,spouse);
    fireAftermathTelemetryTests(setup,target,person,spouse);
    g_dashingResume=oldDash;g_mightyResume=oldMighty;g_injuryGuardResume=oldGuard;
    g_dashingStatusOriginal=oldStatus;g_mightyInjuryOriginal=oldInjury;g_statusTurnsOriginal=oldTurns;g_injurySkillOriginal=oldSkill;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldUnitId;g_movementUnitById=oldUnit;g_growthValid=oldValid;
    for(int n=0;n<3;++n)*(uint32_t*)site(imports[n])=savedImports[n];*nameOperand=oldNames;
    for(int n=0;n<4;++n)g_skills[249].effects[effects[n]]=saved[n];
    puts("PASS after attack49/51/52/32: native speed/chance gates, actual status/health stores, guarded selection/candidate exclusion, first/high/composed providers and preview/no-write/stale exclusions");
}
