// Run the original eight-query OR, branches and selected-weight instruction.
// The surrounding AI eligibility/ranking routine is outside this fixture.
static uintptr_t g_aiWeightFixtureBlock;
static int g_aiWeightFixtureFault=0,g_aiWeightFixtureForce=2;
static int __fastcall aiWeightFixtureOwner(void*,void*){return g_aiWeightFixtureForce;}
static int __fastcall aiWeightFixtureHas(void* unit,void*,int effect){
    typedef int(__thiscall* Has)(void*,int);int result=((Has)site(0x47a8a0))(unit,effect);
    if(result && g_aiWeightFixtureFault){int fault=g_aiWeightFixtureFault;g_aiWeightFixtureFault=0;
        if(fault==1)*(int*)(g_personGuide+0xe8)=-1;
        if(fault==2)*(int*)(g_criticalSource+0xc)=-1;
        if(fault==3)g_criticalSourceRegistered=false;
        if(fault==4)skilltelemetry::invalidateContext();
        if(fault==5)g_aiWeightFixtureForce=3;
        if(fault==6)++*(int*)(g_personGuide+0xa4);
        if(fault==7)++*(int*)(g_criticalSource+0xa8);
        if(fault==8)++*(int*)(g_criticalSource+8);
        if(fault==9)++*(uint32_t*)(g_criticalSource+4);
    }
    return result;
}
__declspec(naked) static int __cdecl aiWeightFixtureRun(void*){__asm {push ebx
    push esi
    push edi
    push ebp
    mov edi,[esp+20]
    mov ebp,0x20c
    jmp dword ptr [g_aiWeightFixtureBlock]}}
__declspec(naked) static void aiWeightFixtureDone(){__asm {mov eax,ebp
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret}}
static void aiWeightTelemetryTests(){
    uint8_t unit[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_criticalSource=unit;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x2c/4]=criticalFixtureObject;vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=aiWeightFixtureOwner;
    vt[0x58/4]=aiWeightFixtureHas;vt[0x5c/4]=(void*)site(0x495600);*(void**)unit=vt;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;
    auto oldSkill=g_skills[249];
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;g_movementUnitById=(MovementUnitById)criticalFixtureUnit;
    domesticStub(0x490b00,personFixtureLookup);
    uint8_t oldEnd[5];memcpy(oldEnd,(void*)site(0x5abfc9),5);domesticStub(0x5abfc9,aiWeightFixtureDone);
    auto prepare=[&](int effect,int provider){
        memset(g_skills[249].effects,0,sizeof g_skills[249].effects);
        for(auto p:{actor,other,person,spouse}){*(int*)(p+0xe8)=-1;memset(p+0x98,0,16);}
        if(effect>=0){g_skills[249].effects[effect]=true;
            *(int*)((provider==1?other:actor)+0xe8)=provider==2?249:effect;
            if(provider==3)*(int*)(other+0xe8)=effect;}
        *(int*)(unit+0xc)=600;*(int*)(unit+0x10)=14162;*(int*)(unit+0x14)=-1;
        *(int*)(unit+0xa8)=3;*(int*)(unit+8)=0;*(uint32_t*)(unit+4)=0x10001;
        g_aiWeightFixtureForce=2;g_aiWeightFixtureFault=0;g_aiWeightFixtureBlock=site(0x5abf57);
        g_criticalSourceRegistered=g_criticalActorRegistered=true;g_defenseSeed=0x12345678;
    };
    assert(skilltelemetry::open(L"build/ai_weight_test.csv",personFixtureActor,nullptr));int inputs=0;
    for(const auto& siteInfo:g_aiWeightSites)for(int provider=0;provider<4;++provider){
        prepare(siteInfo.effect,provider);assert(aiWeightFixtureRun(unit)==1024 && !g_aiWeightPending.actor.person);
        assert(g_defenseSeed==0x12345678);++inputs;
    }
    for(auto pair:{std::pair<int,int>{64,65},{63,67},{62,70},{65,66}}){
        prepare(pair.second,0);*(int*)(other+0xe8)=pair.first;
        assert(aiWeightFixtureRun(unit)==1024 && !g_aiWeightPending.actor.person && g_defenseSeed==0x12345678);++inputs;
    }
    prepare(64,2);g_skills[249].effects[67]=true;
    assert(aiWeightFixtureRun(unit)==1024 && !g_aiWeightPending.actor.person);++inputs;
    assert(inputs==37);skilltelemetry::close();assert(skilltelemetry::open(L"build/ai_weight_edges_test.csv",personFixtureActor,nullptr));
    prepare(65,0);assert(aiWeightFixtureRun(unit)==1024);
    g_aiWeightFixtureBlock=site(0x5abfc4);assert(aiWeightFixtureRun(unit)==1024); // no new predicate receipt
    for(int fault=1;fault<=9;++fault){prepare(65,0);g_aiWeightFixtureFault=fault;
        assert(aiWeightFixtureRun(unit)==1024 && !g_aiWeightPending.actor.person && g_defenseSeed==0x12345678);}
    prepare(-1,0);assert(aiWeightFixtureRun(unit)==524 && !g_aiWeightPending.actor.person);
    skilltelemetry::close();prepare(65,2);assert(aiWeightFixtureRun(unit)==1024 && !g_aiWeightPending.actor.person && g_defenseSeed==0x12345678);
    memcpy((void*)site(0x5abfc9),oldEnd,5);g_skills[249]=oldSkill;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_telemetryUnitId=oldId;g_movementUnitById=oldUnit;
    puts("PASS AI role weight:37 native OR/first-provider/priority/composed inputs,9 stale controls, no-trait524, consumed receipt and disabled parity; evaluation is not command execution");
}
