// Exercise the native movement tail (base, technology, skill branches and byte stores)
// through the persistent unit-stat CALL. Other stat formulas are outside this fixture.
static uintptr_t g_statCommitEntry,g_nativeMovementTail;
static uint8_t g_statWeapon[0xa0]={};
static bool g_statUnitRegistered=true;
static void* __fastcall movementStatUnit(void*,void*,int id) {
    assert(id==17);return g_statUnitRegistered?g_battleUnit:nullptr;
}
__declspec(naked) static void __cdecl runNativeMovementTail(uint8_t*,void**,int,int,int,int,int,int) {
    __asm {
        sub esp,0x40
        push ebx
        push ebp
        push edi
        push esi
        mov ebp,[esp+0x54]
        mov edi,[esp+0x5c]
        mov ebx,offset g_battleForce
        mov esi,offset g_statWeapon
        mov eax,g_nativeMovementTail
        jmp eax
    }
}
__declspec(naked) static void __cdecl runStatCommit(uint8_t* unit,void** members,int weapon,int transport,int water) {
    __asm {
        push esi
        mov esi,[esp+8]
        push 1
        push dword ptr [esp+28]
        push 0
        push dword ptr [esp+32]
        push 1000
        push dword ptr [esp+36]
        push dword ptr [esp+36]
        lea eax,[esi+0xac]
        push eax
        mov eax,g_statCommitEntry
        jmp eax
    }
}
__declspec(naked) static void statCommitted() {
    __asm {add esp,32
           pop esi
           ret}
}
static void movementStatTelemetryTests() {
    uint8_t first[400]={},second[400]={},unit[0xf4]={};
    void* members[]={first,second,nullptr};
    auto oldFirst=g_upkeepActor,oldSecond=g_secondIncomeActor,oldUnit=g_battleUnit;
    int oldFirstId=g_battleFirstId,oldSecondId=g_battleSecondId;
    auto oldId=g_telemetryUnitId;auto oldStats=g_movementStatsOriginal;auto oldValid=g_movementPersonValid;
    auto oldLookup=g_movementUnitById;g_movementUnitById=(MovementUnitById)movementStatUnit;
    g_upkeepActor=first;g_secondIncomeActor=second;g_battleUnit=unit;
    g_battleFirstId=600;g_battleSecondId=14162;
    g_telemetryUnitId=(BuildingId)battleUnitId;g_movementStatsOriginal=runNativeMovementTail;
    g_movementPersonValid=domesticValid;
    void* vt[24]={};vt[0x40/4]=battleOwner;*(void**)unit=vt;
    domesticStub(0x47a630,domesticValid);domesticStub(0x4811e0,battleTech);
    domesticStub(0x496eff,statCommitted);
    // Relocate the native weapon switch table, preserving its actual destinations.
    uint32_t branches[8];
    for(int i=0;i<8;++i)branches[i]=(uint32_t)site(*(uint32_t*)site(0x496b9c+i*4));
    auto branchOperand=(uint32_t*)site(0x496a91);uint32_t oldOperand=*branchOperand;
    *branchOperand=(uint32_t)branches;
    g_statCommitEntry=site(0x496efa);g_nativeMovementTail=site(0x496a76);
    int effects[]={2,3,7};bool saved[3];
    for(int i=0;i<3;++i){saved[i]=g_skills[249].effects[effects[i]];g_skills[249].effects[effects[i]]=true;}
    *(int*)(first+0xe8)=2;*(int*)(second+0xe8)=-1;g_statWeapon[0x57]=20;
    runStatCommit(unit,members,1,0,0);
    assert(unit[0xcc]==25 && !g_movementStatProbe);
    assert(skilltelemetry::open(L"build/movement_stat_telemetry_test.csv",battleActor,nullptr));
    struct Case {int weapon,transport,first,second,base,result;};
    Case cases[]={
        {1,0,2,-1,20,25},{4,0,3,-1,20,23},{1,1,7,-1,20,30},
        {1,0,249,-1,20,25},{4,0,249,-1,20,25},{4,0,3,2,20,25},
        {1,1,249,-1,20,30},{1,0,-1,2,20,25},{4,0,-1,3,20,23},
        {1,1,-1,7,20,30},{1,0,2,2,20,25},{1,0,2,-1,254,3},
        {1,0,2,-1,20,25},{1,0,2,-1,20,25}
    };
    for(auto c:cases) {
        *(int*)(first+0xe8)=c.first;*(int*)(second+0xe8)=c.second;
        unit[0xcc]=0;g_statWeapon[0x57]=(uint8_t)c.base;
        runStatCommit(unit,members,c.weapon,c.transport,0);
        assert(unit[0xcc]==c.result && !g_movementStatProbe);
    }
    // A direct scratch calculation with a matching skill produces only a predicate row.
    uint8_t scratch[0x24]={};
    runNativeMovementTail(scratch,members,1,1000,0,0,0,1);
    assert(scratch[0x20]==25 && !g_movementStatProbe);
    // Correct ownership does not activate an unrelated branch/weapon.
    Case inactive[]={{1,0,3,-1,20,20},{1,0,7,-1,20,20},{1,1,2,-1,20,25},{9,0,3,-1,20,20}};
    for(auto c:inactive) {
        *(int*)(first+0xe8)=c.first;*(int*)(second+0xe8)=c.second;unit[0xcc]=0;
        runStatCommit(unit,members,c.weapon,c.transport,0);
        assert(unit[0xcc]==c.result && !g_movementStatProbe);
    }
    // A temporary unit with a failed native ID round trip is not a world stat write.
    *(int*)(first+0xe8)=2;g_statUnitRegistered=false;
    runStatCommit(unit,members,1,0,0);
    assert(unit[0xcc]==25 && !g_movementStatProbe);g_statUnitRegistered=true;
    skilltelemetry::close();
    // Independently execute the no-strong native fallback, not just observer arithmetic.
    *(int*)(first+0xe8)=249;*(int*)(second+0xe8)=-1;g_skills[249].effects[2]=false;
    runNativeMovementTail(scratch,members,4,1000,0,0,0,1);
    assert(scratch[0x20]==23);
    g_skills[249].effects[2]=true;*(int*)(first+0xe8)=3;
    runNativeMovementTail(scratch,members,4,1000,0,0,0,1);
    assert(scratch[0x20]==23);
    for(int i=0;i<3;++i)g_skills[249].effects[effects[i]]=saved[i];
    *branchOperand=oldOperand;g_movementStatsOriginal=oldStats;g_telemetryUnitId=oldId;g_movementPersonValid=oldValid;
    g_movementUnitById=oldLookup;
    g_upkeepActor=oldFirst;g_secondIncomeActor=oldSecond;g_battleUnit=oldUnit;
    g_battleFirstId=oldFirstId;g_battleSecondId=oldSecondId;
    puts("PASS native movement stats: strong/long/transport branches, priority, byte wrap, persistent/scratch separation and repeated refreshes");
}
