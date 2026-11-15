// Execute the original two pressure predicate/write sequences, the special ambush
// overwrite, and the actual queued-record argument load before the installed commit hook.
static uintptr_t g_pressureCalcEntry,g_pressureCommitEntry,g_counterPressureEntry;
static uint8_t* g_pressureSource;
static uint8_t* g_pressureTarget;
static int g_pressureFindCalls=0;
__declspec(naked) static void __cdecl runPressureCalculation(void* source,void* damage) {
    __asm {
        push ebx
        push ebp
        push esi
        mov ebx,[esp+16]
        mov esi,ebx
        mov ebp,[esp+20]
        mov eax,g_pressureCalcEntry
        jmp eax
    }
}
__declspec(naked) static void pressureCalculated() {
    __asm {
        pop esi
        pop ebp
        pop ebx
        ret
    }
}
__declspec(naked) static void __cdecl runCounterPressure(void* damage,int extra) {
    __asm {
        push esi
        push edi
        mov edi,[esp+12]
        mov esi,[esp+16]
        mov eax,g_counterPressureEntry
        jmp eax
    }
}
__declspec(naked) static int __cdecl runPressureCommit(void* target,void* source,void* damage) {
    __asm {
        push ebx
        push edi
        push esi
        mov esi,[esp+16]
        mov ebx,[esp+20]
        mov edi,[esp+24]
        mov eax,g_pressureCommitEntry
        jmp eax
    }
}
__declspec(naked) static void pressureCommitted() {
    __asm {
        pop esi
        pop edi
        pop ebx
        ret
    }
}
static int __fastcall pressureFind(void* unit,void*,int effect) {
    assert(unit==g_pressureSource);++g_pressureFindCalls;
    return battleFind(unit,nullptr,effect);
}
static const uint32_t* __fastcall pressurePosition(uint8_t* unit,void*) {return (const uint32_t*)(unit+4);}
static int __fastcall pressureOwner(void* unit,void*) {return unit==g_pressureSource?2:3;}
static int __fastcall pressureUnitId(void*,void*,void* unit) {
    assert(unit==g_pressureSource || unit==g_pressureTarget);return unit==g_pressureSource?16:17;
}
static void* __fastcall pressureForce(void*,void*,int id) {assert(id==2 || id==3);return g_battleForce;}
static uint8_t* __fastcall pressureMissingPerson(void*,void*,int) {return nullptr;}
static void pressureTelemetryTests() {
    uint8_t person[400]={},second[400]={},source[0xf4]={},target[0xf4]={},damage[0x80]={};
    g_upkeepActor=person;g_secondIncomeActor=second;g_battleFirstId=600;
    g_pressureSource=source;g_pressureTarget=target;g_advancedEnergy=false;
    *(uint32_t*)(source+4)=0x00200020;*(uint32_t*)(target+4)=0x00220022;
    void* vt[24]={};vt[0x3c/4]=pressurePosition;vt[0x40/4]=pressureOwner;
    vt[0x50/4]=battleTroops;vt[0x54/4]=battleEnergy;
    vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=pressureFind;
    *(void**)source=*(void**)target=vt;
    auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryUnitId;auto oldEnergy=g_unitEnergyOriginal;
    auto oldResume=g_pressureEnergyResume;
    g_telemetryPerson=(PersonById)battlePerson;g_telemetryUnitId=(BuildingId)pressureUnitId;
    g_unitEnergyOriginal=(UnitEnergy)site(0x4964f0);g_pressureEnergyResume=(uint32_t)pressureCommitted;
    domesticStub(0x47a630,domesticValid);domesticStub(0x490aa0,pressureForce);
    domesticStub(0x4811e0,battleTech);
    domesticStub(0x5b03f5,pressureCalculated);domesticStub(0x5b0bca,pressureCalculated);
    g_pressureCommitEntry=site(0x584fef);g_counterPressureEntry=site(0x5b0ebb);
    auto prepare=[&](int kind) {
        memset(damage,0,sizeof damage);
        *(int*)(damage+0x70)=kind;
        *(uint32_t*)(damage+0x58)=*(uint32_t*)(source+4);
        *(uint32_t*)(damage+0x5c)=*(uint32_t*)(target+4);
    };
    bool old9=g_skills[249].effects[9],old10=g_skills[249].effects[10];
    g_skills[249].effects[9]=g_skills[249].effects[10]=true;
    *(int*)(person+0xe8)=9;*(int*)(second+0xe8)=-1;
    prepare(0);g_pressureCalcEntry=site(0x5b03cd);runPressureCalculation(source,damage);
    assert(*(int*)(damage+0x14)==5);
    target[0x1a]=30;int calls=g_pressureFindCalls;
    assert(runPressureCommit(target,source,damage)==-5 && target[0x1a]==25 && calls==g_pressureFindCalls);
    assert(skilltelemetry::open(L"build/pressure_telemetry_test.csv",battleActor,nullptr));
    for(int path=0;path<2;++path)for(int mode=0;mode<4;++mode) {
        *(int*)(person+0xe8)=mode==0 || mode==3?9:mode==1?10:249;
        *(int*)(second+0xe8)=mode==3?10:-1;
        int beforeValues[4]={30,mode==0?3:10,3,0};
        int count=mode<2?3:4;
        if(mode<2)beforeValues[2]=0;
        for(int i=0;i<count;++i) {
            prepare(path==0?0:3);g_pressureCalcEntry=site(path==0?0x5b03cd:0x5b0ba2);
            runPressureCalculation(source,damage);
            assert(*(int*)(damage+0x14)==(mode==0?5:20));
            uint8_t snapshot[0x80];memcpy(snapshot,damage,sizeof snapshot);
            int before=beforeValues[i],cost=mode==0?5:20;
            target[0x1a]=(uint8_t)before;
            int actual=runPressureCommit(target,source,damage);
            assert(actual==-(before<cost?before:cost) && target[0x1a]==before+actual);
            assert(!memcmp(snapshot,damage,sizeof snapshot));
        }
    }
    // A native ambush can also write 20, but its +0x70 tag disqualifies pressure attribution.
    *(int*)(person+0xe8)=249;*(int*)(second+0xe8)=-1;
    prepare(0);g_pressureCalcEntry=site(0x5b03cd);runPressureCalculation(source,damage);
    runCounterPressure(damage,1);assert(*(int*)(damage+0x14)==20 && *(int*)(damage+0x70)==1);
    target[0x1a]=30;calls=g_pressureFindCalls;
    assert(runPressureCommit(target,source,damage)==-20 && calls==g_pressureFindCalls);
    runCounterPressure(damage,0);assert(*(int*)(damage+0x14)==10);
    target[0x1a]=30;assert(runPressureCommit(target,source,damage)==-10 && calls==g_pressureFindCalls);
    // Reusing an unchanged record for two real commits must produce two real effects.
    *(int*)(person+0xe8)=10;prepare(0);
    runPressureCalculation(source,damage);target[0x1a]=50;
    assert(runPressureCommit(target,source,damage)==-20 && target[0x1a]==30);
    assert(runPressureCommit(target,source,damage)==-20 && target[0x1a]==10);
    // A preview calculation produces predicate rows, never an applied row by itself.
    *(int*)(person+0xe8)=249;prepare(0);runPressureCalculation(source,damage);
    // Wrong source/target positions and unknown record kinds cannot borrow a numeric match.
    *(uint32_t*)(damage+0x58)^=1;target[0x1a]=30;calls=g_pressureFindCalls;
    assert(runPressureCommit(target,source,damage)==-20 && calls==g_pressureFindCalls);
    prepare(0);*(int*)(damage+0x14)=20;*(uint32_t*)(damage+0x5c)^=1;target[0x1a]=30;
    assert(runPressureCommit(target,source,damage)==-20 && calls==g_pressureFindCalls);
    prepare(4);*(int*)(damage+0x14)=20;target[0x1a]=30;
    assert(runPressureCommit(target,source,damage)==-20 && calls==g_pressureFindCalls);
    prepare(0);*(int*)(damage+0x14)=10;target[0x1a]=30;
    assert(runPressureCommit(target,source,damage)==-10 && calls==g_pressureFindCalls);
    *(int*)(person+0xe8)=*(int*)(second+0xe8)=-1;
    *(int*)(damage+0x14)=20;target[0x1a]=30;assert(runPressureCommit(target,source,damage)==-20);
    *(int*)(person+0xe8)=10;g_telemetryPerson=(PersonById)pressureMissingPerson;
    prepare(0);runPressureCalculation(source,damage);target[0x1a]=30;
    assert(runPressureCommit(target,source,damage)==-20);
    skilltelemetry::close();
    g_skills[249].effects[9]=old9;g_skills[249].effects[10]=old10;
    g_telemetryPerson=oldPerson;g_telemetryUnitId=oldId;g_unitEnergyOriginal=oldEnergy;
    g_pressureEnergyResume=oldResume;
    puts("PASS pressure telemetry: two native calculators, actual queued-record commit, source/target positions, override baselines, ambush exclusion and repeated commits");
}
