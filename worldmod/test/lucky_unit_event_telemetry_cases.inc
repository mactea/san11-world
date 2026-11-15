// Native583065..5830aa member loop and580ab0's complete positive32 branch.
// Other selector outcomes and candidate-list transport are controlled adapters.
static uintptr_t g_luckyUnitLoopEntry=0;
static int g_luckyUnitAppends=0,g_luckyUnitFalseResult=1,g_luckyUnitForce=2;
static int g_luckyUnitFault=0;
static int __cdecl luckyUnitNativeSelector(void* person){
    int result=((LuckyUnitSelector)site(0x580ab0))(person);
    if(person==g_personGuide)switch(g_luckyUnitFault){
    case 1:skilltelemetry::invalidateContext();break;
    case 2:*(int*)(g_personGuide+0xe8)=-1;break;
    case 3:g_afterActorRegistered=false;break;
    case 4:g_afterSourceRegistered=false;break;
    case 5:++g_luckyUnitForce;break;
    case 6:++*(uint32_t*)(g_afterSource+4);break;
    case 7:++*(int*)(g_afterSource+0xa8);break;
    case 8:*(int*)(g_afterSource+0xc)=-1;break;
    case 9:++*(int*)(g_personGuide+0x9c);break;
    }
    return result;
}
static int __fastcall luckyUnitCount(void*,void*){return 3;}
static int __fastcall luckyUnitMember(uint8_t* unit,void*,int n){assert(n>=0 && n<3);return *(int*)(unit+0xc+4*n);}
static int __fastcall luckyUnitOwner(void*,void*){return g_luckyUnitForce;}
static void __fastcall luckyUnitAppend(void*,void*,void*){++g_luckyUnitAppends;}
__declspec(naked) static void luckyUnitOtherOutcome(){__asm {mov eax,g_luckyUnitFalseResult
    pop ebp
    ret}}
__declspec(naked) static void __cdecl runLuckyUnitLoop(void*){__asm {push ebp
    push ebx
    push esi
    push edi
    mov esi,[esp+20]
    xor ebx,ebx
    sub esp,0x40
    jmp dword ptr [g_luckyUnitLoopEntry]}}
__declspec(naked) static void luckyUnitLoopDone(){__asm {add esp,0x40
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret}}
static void luckyUnitEventTelemetryTests(){
    uint8_t unit[0x110]={},people[3][400]={};void* vt[24]={};
    vt[0x3c/4]=afterFixturePosition;vt[0x40/4]=luckyUnitOwner;*(void**)unit=vt;
    auto oldGuide=g_personGuide,oldOther=g_personOther,oldSpouse=g_personSpouse,oldTarget=g_personTarget;
    auto oldSource=g_afterSource;g_afterSource=unit;g_personGuide=people[0];g_personOther=people[1];g_personSpouse=people[2];g_personTarget=nullptr;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldUnitId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)afterFixturePersonId;
    g_telemetryUnitId=(BuildingId)afterFixtureId;g_movementUnitById=(MovementUnitById)afterFixtureUnit;
    auto oldSelector=g_luckyUnitSelectorOriginal;g_luckyUnitSelectorOriginal=luckyUnitNativeSelector;
    bool oldSkill=g_skills[249].effects[32];g_skills[249].effects[32]=true;
    uint32_t sites[]={0x5830ac,0x4953d0,0x495290,0x490b00,0x47c1b0,0x580ac6};uint8_t saved[6][5];
    for(int n=0;n<6;++n)memcpy(saved[n],(void*)site(sites[n]),5);
    domesticStub(sites[0],luckyUnitLoopDone);domesticStub(sites[1],luckyUnitCount);domesticStub(sites[2],luckyUnitMember);
    domesticStub(sites[3],personFixtureLookup);domesticStub(sites[4],luckyUnitAppend);domesticStub(sites[5],luckyUnitOtherOutcome);
    g_luckyUnitLoopEntry=site(0x583065);
    auto reset=[&]{
        *(int*)(unit+0xc)=600;*(int*)(unit+0x10)=14162;*(int*)(unit+0x14)=601;*(int*)(unit+0xa8)=4;*(uint32_t*)(unit+4)=0x10002;
        for(auto& p:people){*(int*)(p+0xe8)=-1;memset(p+0x98,0,16);}
        g_afterActorRegistered=g_afterSourceRegistered=true;g_luckyUnitForce=2;g_luckyUnitAppends=0;g_luckyUnitFalseResult=1;g_luckyUnitFault=0;
    };
    assert(skilltelemetry::open(L"build/lucky_unit_event_test.csv",personFixtureActor,nullptr));
    for(int owned:{32,249})for(int slot=0;slot<3;++slot){reset();*(int*)(people[slot]+0xe8)=owned;
        runLuckyUnitLoop(unit);assert(g_luckyUnitAppends==2 && !g_progressionProbe);}
    reset();for(int n=0;n<3;++n)*(int*)(people[n]+0xe8)=n==1?249:32;
    runLuckyUnitLoop(unit);assert(g_luckyUnitAppends==0);
    for(int fault=1;fault<=9;++fault){reset();g_luckyUnitFault=fault;*(int*)(people[0]+0xe8)=32;
        runLuckyUnitLoop(unit);assert(g_luckyUnitAppends==2);}
    reset();for(int n=0;n<3;++n)*(int*)(people[n]+0xe8)=n==1?249:32;
    for(int n=0;n<3;++n)assert(((int(__cdecl*)(void*))site(0x580ab0))(people[n])==0); // standalone selectors are previews
    for(int result:{0,1}){reset();g_luckyUnitFalseResult=result;runLuckyUnitLoop(unit);assert(g_luckyUnitAppends==3*result);}
    skilltelemetry::close();
    for(int mode=0;mode<3;++mode){reset();g_luckyUnitFalseResult=mode==2;
        if(!mode)for(auto& p:people)*(int*)(p+0xe8)=249;
        runLuckyUnitLoop(unit);assert(g_luckyUnitAppends==(mode==2?3:0));}
    for(int n=0;n<6;++n)memcpy((void*)site(sites[n]),saved[n],5);
    g_skills[249].effects[32]=oldSkill;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;
    g_telemetryUnitId=oldUnitId;g_movementUnitById=oldUnit;g_afterSource=oldSource;
    g_personGuide=oldGuide;g_personOther=oldOther;g_personSpouse=oldSpouse;g_personTarget=oldTarget;
    g_luckyUnitSelectorOriginal=oldSelector;
    puts("PASS lucky unit event: native three-member selector/skip/append loop, raw/composed and high providers, standalone previews and three disabled controls");
}
