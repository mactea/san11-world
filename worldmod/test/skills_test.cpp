// Exercise mapped game instructions in a separate test process. World/I/O/
// presentation inputs remain controlled fixtures.
#include <intrin.h>
static void* precisionFixtureCaller(void*);
// The mapped native instructions determine the caller. Undo relocation only
// for these two return sites; provider selection is never supplied by a test.
#define _ReturnAddress() precisionFixtureCaller(_ReturnAddress())
#include "../src/skills.cpp"
#undef _ReturnAddress
#include <assert.h>
#include <vector>
#include <optional>

static uint8_t* g_image;
static void* precisionFixtureCaller(void* pointer) {
    uintptr_t p=(uintptr_t)pointer,base=(uintptr_t)g_image;
    if(g_image && p>=base && p-base<0x4a0000) {
        uintptr_t native=p-base+0x400000;
        if(native==0x4bf079 || native==0x4bf126)return (void*)native;
    }
    return pointer;
}
static uintptr_t site(uint32_t address) { return (uintptr_t)g_image + address - 0x400000; }
static LONG CALLBACK testException(EXCEPTION_POINTERS* exception) {
    if(exception->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION) {
        auto c=exception->ContextRecord;
        fprintf(stderr,"Native fixture access violation: EIP=%08lx mapped=%08lx EAX=%08lx ECX=%08lx EDX=%08lx ESP=%08lx address=%08lx\n",
            c->Eip,c->Eip-(uintptr_t)g_image+0x400000,c->Eax,c->Ecx,c->Edx,c->Esp,
            (unsigned long)exception->ExceptionRecord->ExceptionInformation[1]);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void mapGame(const wchar_t* path) {
    FILE* f = nullptr; assert(!_wfopen_s(&f, path, L"rb") && f);
    fseek(f, 0, SEEK_END); size_t size = ftell(f); rewind(f);
    std::vector<uint8_t> bytes(size); assert(fread(bytes.data(), 1, size, f) == size); fclose(f);
    auto dos = (IMAGE_DOS_HEADER*)bytes.data();
    auto nt = (IMAGE_NT_HEADERS32*)(bytes.data() + dos->e_lfanew);
    assert(nt->OptionalHeader.ImageBase == 0x400000);
    uint8_t* base=(uint8_t*)VirtualAlloc(nullptr,0x4a0000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(base);g_image=base;
    g_strategyCodeBase=(uintptr_t)base;
    g_strategyChanceOriginal=(StrategyAttempt)site(0x5ae980);
    g_strategyDefenderOriginal=(MovementUnitById)site(0x490e70);
    g_strategyObjectOriginal=(StrategyObject)site(0x483b40);
    g_criticalAggregateResume=(uint32_t)site(0x584516);
    g_criticalAggregateCaller=(uint32_t)site(0x586646);
    g_criticalAggregateFloatEntry=(uint32_t)site(0x707a74);
    g_vineHasResume=(uint32_t)site(0x5b040f);g_vineStoreResume=(uint32_t)site(0x5b0419);
    g_vineAssistHasResume=(uint32_t)site(0x5b0c14);g_vineAssistStoreResume=(uint32_t)site(0x5b0c1e);
    g_assistUnyieldingResume=(uint32_t)site(0x5b0b53);g_assistDiamondResume=(uint32_t)site(0x5b0b80);
    g_aiWeightResume=(uint32_t)site(0x5abfc9);
    g_constructionAmountCaller=(uint32_t)site(0x5850b7);g_constructionGrantCaller=(uint32_t)site(0x5850cf);
    g_counterFloatEntry=(uint32_t)site(0x707a74);
    auto section = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(section[i].Name, ".text", 5) && memcmp(section[i].Name, ".rdata", 6)) continue;
        memcpy(base + section[i].VirtualAddress, bytes.data() + section[i].PointerToRawData, section[i].SizeOfRawData);
    }
    for (auto& h : g_hooks) h.at = (uint32_t)site(h.at);
    g_incomeResume = (uint32_t)site(0x4cea51);
    g_tuntianSkip=(uint32_t)site(0x59c014);
    g_tuntianConsume=(uint32_t)site(0x59bfdb);
    g_personLimitOperand=(const int*)site(0x490b09);
    g_boleAllow = (uint32_t)site(0x4cf717);
    g_boleReject = (uint32_t)site(0x4cf7a7);
}

static bool parseText(const char* text) {
    FILE* f = nullptr; assert(!tmpfile_s(&f) && f); fwrite(text, 1, strlen(text), f); rewind(f);
    std::string error; bool ok = parse(f, error); fclose(f); return ok;
}

static void parserTests() {
    assert(parseText("\xef\xbb\xbf# comment\r\n100|神膽|遁走與洞察|1,61\r\n"));
    assert(contains(100, 1) && contains(100, 61) && contains(100, 100));
    assert(!contains(100, 0) && contains(0, 0));
    assert(!contains(-1, -1) && !contains(255, 1) && !contains(250, 1));
    assert(parseText("0|飛將|追加洞察|61\n"));
    assert(contains(0, 0) && contains(0, 61));
    assert(parseText("249|邊界|高位元編號|75,76,93,94\n"));
    assert(g_count == 250 && contains(249, 94));
    assert(parseText("147|商才|所在城自動商隊上限加倍|147\n249|富商|商才與富豪|147,75\n"));
    assert(contains(147,147) && contains(249,147) && contains(249,75));
    assert(!contains(147,0) && !contains(147,75));
    const char* invalid[] = {
        "250|X|Y|1\n", "-1|X|Y|1\n", "100|X|Y|120\n", "100|X|Y|100\n",
        "100|X|Y|1,2,3,4,5,6,7,8,9\n", "100|X|Y|\n", "100|X|Y|1,\n",
        "100|X|Y|1\n100|Z|Y|2\n", "100|X|Y|1||\n", "100|X|Y|x\n",
        "100||Y|1\n", "100|12345|Y|1\n", "100|\xf0\x9f\x98\x80|Y|1\n",
        "100|\xff|Y|1\n", "100|X|Y|1,,2\n",
    };
    for (auto text : invalid) assert(!parseText(text));
    puts("PASS definitions: UTF-8/Big5, original identity, effects, ranges, duplicates and malformed input");
}

static void* __fastcall fakeSerialize(uint8_t* stream, void*, int* value) {
    if (*(int*)(stream + 8) == 1) *value = (int8_t)stream[12];
    else stream[12] = (uint8_t)*value;
    return stream;
}
static int __fastcall fakeMember(void*, void*, int queried) { return queried == 61 ? 14162 : -1; }
static int g_precisionPersonId=1500,g_precisionResolutions=0,g_precisionActorForce=1;
static uint8_t* g_precisionSecondaryPerson=nullptr;
static bool precisionActor(const void* person,skilltelemetry::Actor& actor) {
    ++g_precisionResolutions;actor.person=person==g_precisionSecondaryPerson?14162:g_precisionPersonId;actor.force=g_precisionActorForce;strcpy_s(actor.name,"Precision");return true;
}
static uintptr_t g_precisionEntry;
__declspec(naked) static int __cdecl runPrecision(uint8_t* force,int requested,void* object=nullptr) {
    __asm {
        push [esp+12]
        mov eax,[esp+12]
        push eax
        mov eax,[esp+12]
        push eax
        xor ecx,ecx
        mov eax,g_precisionEntry
        jmp eax
    }
}
static int g_recruiter = 14162, g_affinity = 25;
static void* g_candidate;
static int __fastcall fakeRecruiter(void*, void*, int queried) { return queried == 146 ? g_recruiter : -1; }
static int __fastcall fakeAffinity(void* target, void*, int recruiter) {
    assert(target == g_candidate && recruiter == 14162); return g_affinity;
}
static uintptr_t g_boleEntry;
static uintptr_t g_boleFixtureCaller;
__declspec(naked) static int __cdecl runBole(void* building, void* candidate, int sameForce) {
    __asm {
        push esi
        sub esp, 0x1c
        mov esi, [esp + 0x28]
        mov eax, [esp + 0x24]
        mov [esp + 0x14], eax
        mov eax,g_boleFixtureCaller
        mov [esp + 0xc],eax
        mov eax, [esp + 0x2c]
        cmp eax, 1
        mov edx, g_boleEntry
        call edx
        add esp, 0x1c
        pop esi
        ret
    }
}
static uintptr_t g_incomeEntry;
__declspec(naked) static void __cdecl runIncome(int* flags, int owned, const void* actor=nullptr) {
    __asm {
        push ebx
        push esi
        mov ebx, [esp + 12]
        mov eax, [esp + 16]
        mov esi, [esp + 20]
        mov edx, g_incomeEntry
        call edx
        pop esi
        pop ebx
        ret
    }
}

struct UpkeepBuilding {void* vtable;int reserved,type,food,cost,force,id,gold,foodMax,goldMax;};
static uint8_t* g_upkeepActor;
static uint8_t* g_secondIncomeActor;
static int g_upkeepReads=0;
static int g_upkeepActorId=1500;
static uint8_t* __fastcall upkeepPerson(void*,void*,int holder) {
    assert(holder>=0 && holder<=1099);return g_upkeepActor;
}
static uint8_t* __fastcall expandedUpkeepPerson(void*,void*,int holder) {
    assert(holder==g_upkeepActorId && holder>=1100 && holder<=14612);return g_upkeepActor;
}
static int __fastcall upkeepFood(UpkeepBuilding* building,void*) {return building->food;}
static int __fastcall upkeepCost(void*,void*,UpkeepBuilding* building) {++g_upkeepReads;return building->cost;}
static int __fastcall upkeepBaseId(void*,void*,UpkeepBuilding* building) {return building->id;}
static int __fastcall upkeepForce(UpkeepBuilding* building,void*) {return building->force;}
static bool economyActor(const void* p,skilltelemetry::Actor& a) {
    assert(p==g_upkeepActor || p==g_secondIncomeActor);
    precisionActor(p,a);a.person=p==g_secondIncomeActor?1501:g_upkeepActorId;return true;
}
static uintptr_t g_upkeepEntry;
__declspec(naked) static int __cdecl runUpkeep(void* building,int holder) {
    __asm {
        push ebx
        push esi
        mov esi,[esp+12]
        mov eax,[esp+16]
        xor ebx,ebx // the native lower-bound comparison uses EBX=0
        mov edx,g_upkeepEntry
        call edx
        pop esi
        pop ebx
        ret
    }
}
static void economyTelemetryTests() {
    uint8_t person[400]={};g_upkeepActor=person;
    int flags[BASE]={};
    assert(skilltelemetry::open(L"build/income_telemetry_test.csv",economyActor,nullptr));
    runIncome(flags,249,person);
    memset(flags,0,sizeof flags);runIncome(flags,75,person);
    runIncome(flags,-1,person);
    // An added effect equal to the original ID must not produce a duplicate for this officer.
    bool saved=g_skills[75].effects[75];g_skills[75].effects[75]=true;
    runIncome(flags,75,person);g_skills[75].effects[75]=saved;
    skilltelemetry::close();

    auto oldPerson=g_telemetryPerson;auto oldFood=g_telemetryFood;
    auto oldCost=g_telemetryFoodCost;auto oldId=g_telemetryBaseId;
    g_telemetryPerson=(PersonById)upkeepPerson;g_telemetryFood=(BuildingValue)upkeepFood;
    g_telemetryFoodCost=(FoodCost)upkeepCost;g_telemetryBaseId=(BuildingId)upkeepBaseId;
    void* vt[24]={};vt[0x40/4]=upkeepForce;
    UpkeepBuilding building={vt,0,2,1000,300,2,52,0,5000,5000};*(int*)(person+0xe8)=78;
    // Exercise the original lower-bound branch and the installed upper-bound/skip bridge.
    g_upkeepEntry=site(0x59bfd0);
    memcpy((void*)g_tuntianSkip,"\xb8\x01\x00\x00\x00\xc3",6);
    memcpy((void*)g_tuntianConsume,"\x33\xc0\xc3",3);
    assert(runUpkeep(&building,1)==1 && !g_upkeepReads); // disabled: no extra native reads
    assert(skilltelemetry::open(L"build/upkeep_telemetry_test.csv",economyActor,nullptr));
    for(int food:{1000,100,0}) {
        building.food=food;
        assert(runUpkeep(&building,1099)==1 && building.food==food);
    }
    int calls=g_upkeepReads;
    assert(runUpkeep(&building,-1)==0 && runUpkeep(&building,1100)==0 && g_upkeepReads==calls);
    bool saved78=g_skills[249].effects[78];g_skills[249].effects[78]=true;
    *(int*)(person+0xe8)=249;building.food=1000;
    assert(runUpkeep(&building,1)==1);
    g_skills[249].effects[78]=saved78;
    int oldMax=g_tuntianMax;g_tuntianMax=14612;g_telemetryPerson=(PersonById)expandedUpkeepPerson;
    *(int*)(person+0xe8)=78;g_upkeepActorId=14162;building.food=1000;
    assert(runUpkeep(&building,14162)==1);
    g_upkeepActorId=14612;building.food=100;assert(runUpkeep(&building,14612)==1);
    calls=g_upkeepReads;assert(runUpkeep(&building,14613)==0 && calls==g_upkeepReads);
    g_tuntianMax=oldMax;g_upkeepActorId=1500;g_telemetryPerson=(PersonById)upkeepPerson;
    g_upkeepActor=nullptr;assert(runUpkeep(&building,1)==1); // stale/missing actor cannot be attributed
    skilltelemetry::close();
    g_telemetryPerson=oldPerson;g_telemetryFood=oldFood;g_telemetryFoodCost=oldCost;g_telemetryBaseId=oldId;
    puts("PASS economy telemetry: officer-list attribution, composed/deduplicated eligibility, native upkeep exemption and preserved holder bounds");
}

static int __fastcall bonusGold(UpkeepBuilding* building,void*) {return building->gold;}
static int __fastcall grantGold(void*,void*,UpkeepBuilding* building,int requested) {
    int before=building->gold;
    building->gold+=requested;
    if(building->gold>building->goldMax)building->gold=building->goldMax;
    return building->gold-before;
}
static int __fastcall grantFood(void*,void*,UpkeepBuilding* building,int requested) {
    int before=building->food;
    building->food+=requested;
    if(building->food>building->foodMax)building->food=building->foodMax;
    return building->food-before;
}
static void __fastcall collectIncome(int* flags,void*,void*,int) {
    memset(flags,0,BASE*sizeof(int));
    if(g_upkeepActor)runIncome(flags,*(int*)(g_upkeepActor+0xe8),g_upkeepActor);
    if(g_secondIncomeActor)runIncome(flags,*(int*)(g_secondIncomeActor+0xe8),g_secondIncomeActor);
}
static uintptr_t g_incomeBonusEntry;
static uintptr_t g_cityFlagsEntry;
__declspec(naked) static void __cdecl runCityFlags(int* flags,void* building,int mode) {
    __asm {
        mov eax,[esp+12]
        push eax
        mov eax,[esp+12]
        push eax
        mov ecx,[esp+12]
        mov eax,g_cityFlagsEntry
        jmp eax
    }
}
__declspec(naked) static int __cdecl runIncomeBonus(void* building,int requested) {
    __asm {
        mov eax,[esp+8]
        push eax
        mov eax,[esp+8]
        push eax
        xor ecx,ecx
        mov eax,g_incomeBonusEntry
        jmp eax
    }
}
static void assertBattleAppliedRows(const wchar_t*,int);
static void incomeNativeCollectionTests(UpkeepBuilding&,uint8_t*,uint8_t*);
static void* g_incomeNestedOuter;
static void* g_incomeNestedInner;
static int g_incomeNestedMode=0;
static void __fastcall collectNestedIncome(int* flags,void*,void* building,int mode) {
    memset(flags,0,BASE*sizeof(int));
    if(building==g_incomeNestedInner){runIncome(flags,87,g_secondIncomeActor);return;}
    assert(building==g_incomeNestedOuter);int inner[BASE]={};
    if(g_incomeNestedMode==0)runIncome(inner,87,g_secondIncomeActor);
    else cityIncomeFlags(inner,nullptr,g_incomeNestedInner,mode);
    if(g_incomeNestedMode==2)throw 7;
    runIncome(flags,*(int*)(g_upkeepActor+0xe8),g_upkeepActor);
}
static void incomeBonusTelemetryTests() {
    uint8_t person[400]={},second[400]={};g_upkeepActor=person;g_secondIncomeActor=second;
    *(int*)(person+0xe8)=249;*(int*)(second+0xe8)=87;
    auto oldFlags=g_cityIncomeFlagsOriginal;auto oldGold=g_telemetryGold;
    auto oldFood=g_telemetryFood;auto oldId=g_telemetryBaseId;
    auto oldGoldGrant=g_goldIncomeGrantOriginal;auto oldFoodGrant=g_foodIncomeGrantOriginal;
    g_cityIncomeFlagsOriginal=(CityIncomeFlags)collectIncome;g_telemetryGold=(BuildingValue)bonusGold;
    g_telemetryFood=(BuildingValue)upkeepFood;g_telemetryBaseId=(BuildingId)upkeepBaseId;
    g_goldIncomeGrantOriginal=(IncomeGrant)grantGold;g_foodIncomeGrantOriginal=(IncomeGrant)grantFood;
    void* vt[24]={};vt[0x40/4]=upkeepForce;
    UpkeepBuilding building={vt,0,0,1000,300,2,8,1000,5000,5000};int flags[BASE]={};
    g_cityFlagsEntry=site(0x590527);
    *(uint8_t*)site(0x59052c)=0xc3;
    for(uint32_t at:{0x590576u,0x5906efu,0x590635u,0x590744u})*(uint8_t*)site(at+5)=0xc3;
    runCityFlags(flags,&building,0);assert(!g_incomeProviders.city && !g_collectIncomeProviders);
    assert(skilltelemetry::open(L"build/income_bonus_telemetry_test.csv",economyActor,nullptr));
    runCityFlags(flags,&building,0);
    assert(g_incomeProviders.city==&building && !g_collectIncomeProviders);
    assert(g_incomeProviders.holders[87].person==person && g_incomeProviders.holders[88].person==person);
    // Both officers are eligible, but the flag does not stack. Log the applied grant only once.
    for(uint32_t at:{0x590576u,0x5906efu,0x590635u,0x590744u}) {
        bool gold=at==0x590576u || at==0x5906efu;
        g_incomeBonusEntry=site(at);
        for(int before:{1000,4990,5000}) {
            building.gold=building.food=before;
            assert(runIncomeBonus(&building,100)==(before==1000?100:5000-before));
            assert((gold?building.gold:building.food)==(before==1000?1100:5000));
        }
        building.type=2;building.id=52;building.gold=building.food=1000;
        assert(runIncomeBonus(&building,100)==100);
        building.type=0;building.id=8;
    }
    g_upkeepActor=g_secondIncomeActor=nullptr;runCityFlags(flags,&building,0);
    assert(!g_incomeProviders.holders[87].person && !g_incomeProviders.holders[88].person);
    building.food=1000;assert(runIncomeBonus(&building,100)==100); // no stale provider from previous city
    skilltelemetry::close();
    g_upkeepActor=person;g_secondIncomeActor=nullptr;*(int*)(person+0xe8)=249;
    assert(skilltelemetry::open(L"build/income_context_source_test.csv",economyActor,nullptr));
    runCityFlags(flags,&building,0);skilltelemetry::close();
    assert(skilltelemetry::open(L"build/income_context_after_test.csv",economyActor,nullptr));
    for(uint32_t at:{0x590576u,0x590635u}) {
        g_incomeBonusEntry=site(at);building.gold=building.food=1000;assert(runIncomeBonus(&building,100)==100);
    }
    runCityFlags(flags,&building,0);
    for(uint32_t at:{0x590576u,0x590635u}) {
        g_incomeBonusEntry=site(at);building.gold=building.food=1000;assert(runIncomeBonus(&building,100)==100);
    }
    skilltelemetry::close();assertBattleAppliedRows(L"build/income_context_after_test.csv",2);
    g_secondIncomeActor=second;*(int*)(second+0xe8)=87;
    UpkeepBuilding inner=building;inner.id=9;g_incomeNestedInner=&inner;g_incomeNestedOuter=&building;
    g_cityIncomeFlagsOriginal=(CityIncomeFlags)collectNestedIncome;
    assert(skilltelemetry::open(L"build/income_nested_test.csv",economyActor,nullptr));
    for(int mode:{0,1}) {
        g_incomeNestedMode=mode;runCityFlags(flags,&building,0);
        assert(g_incomeProviders.city==&building && g_incomeProviders.holders[87].person==person && g_incomeProviders.holders[88].person==person && !g_collectIncomeProviders);
        for(uint32_t at:{0x590576u,0x590635u}) {
            g_incomeBonusEntry=site(at);building.gold=building.food=1000;assert(runIncomeBonus(&building,100)==100);
        }
    }
    g_incomeNestedMode=2;bool thrown=false;
    try {cityIncomeFlags(flags,nullptr,&building,0);}catch(int value){assert(value==7);thrown=true;}
    assert(thrown && !g_collectIncomeProviders && !g_incomeProviders.city);
    g_incomeBonusEntry=site(0x590576);building.gold=1000;assert(runIncomeBonus(&building,100)==100);
    skilltelemetry::close();assertBattleAppliedRows(L"build/income_nested_test.csv",4);
    incomeNativeCollectionTests(building,person,second);
    g_cityIncomeFlagsOriginal=oldFlags;g_telemetryGold=oldGold;g_telemetryFood=oldFood;
    g_telemetryBaseId=oldId;g_goldIncomeGrantOriginal=oldGoldGrant;g_foodIncomeGrantOriginal=oldFoodGrant;
    puts("PASS income bonus telemetry: four native grant calls, first eligible provider, caps, port targets and stale-city rejection");
}

#include "domestic_telemetry_cases.inc"
#include "battle_telemetry_cases.inc"
#include "precision_telemetry_cases.inc"
#include "pressure_telemetry_cases.inc"
#include "movement_telemetry_cases.inc"
#include "movement_stat_telemetry_cases.inc"
#include "revenue_telemetry_cases.inc"
#include "income_native_collection_cases.inc"
#include "unrest_telemetry_cases.inc"
#include "person_telemetry_cases.inc"
#include "discovery_telemetry_cases.inc"
#include "research_telemetry_cases.inc"
#include "environment_telemetry_cases.inc"
#include "harvest_telemetry_cases.inc"
#include "diplomacy_telemetry_cases.inc"
#include "construction_telemetry_cases.inc"
#include "critical_telemetry_cases.inc"
#include "defense_telemetry_cases.inc"
#include "ambush_attack_telemetry_cases.inc"
#include "ironwall_telemetry_cases.inc"
#include "assist_telemetry_cases.inc"
#include "counter_immunity_telemetry_cases.inc"
#include "double_attack_telemetry_cases.inc"
#include "strategy_cost_telemetry_cases.inc"
#include "after_attack_telemetry_cases.inc"
#include "area_friendly_telemetry_cases.inc"
#include "raw_priority_telemetry_cases.inc"
#include "tribal_affinity_telemetry_cases.inc"
#include "ai_score_telemetry_cases.inc"
#include "lucky_unit_event_telemetry_cases.inc"
#include "lucky_duel_telemetry_cases.inc"
#include "lucky_mission_telemetry_cases.inc"
#include "query_context_telemetry_cases.inc"
#include "pincer_telemetry_cases.inc"
#include "capture_telemetry_cases.inc"
#include "strategy_immunity_telemetry_cases.inc"
#include "magic_telemetry_cases.inc"
#include "reach_telemetry_cases.inc"
#include "ai_weight_telemetry_cases.inc"
#include "attack_reach_telemetry_cases.inc"
#include "bole_telemetry_cases.inc"
#include "trap_telemetry_cases.inc"
#include "vine_telemetry_cases.inc"

int wmain(int argc, wchar_t** argv) {
    _set_error_mode(_OUT_TO_STDERR);setvbuf(stdout,nullptr,_IONBF,0);
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1,testException);
    assert(argc == 3); mapGame(argv[1]); parserTests();
#if defined(DOMESTIC_OWNER_ASAN)
    assert(skillsPrepare(argv[2],nullptr) && skillsInstall());
    g_skills[249].defined=true;for(int effect=79;effect<=83;++effect)g_skills[249].effects[effect]=true;
    domesticTelemetryTests();return 0;
#endif
    assert(verify());
    assert(!skillsInstall());
    assert(!skillsPrepare(L"nonexistent-skills-test.txt", nullptr));
    assert(verify()); // failed load must not mutate native instructions
    assert(skillsPrepare(argv[2], nullptr));
    assert(g_count == 148 && verify()); // prepare/verify_only does not write patches
    *(int*)site(0x59bfd5)=14612;assert(!verify()); // an unpaired edit remains rejected
    *(int*)site(0x490b09)=14612;assert(!verify()); // item setter must share the M10 person bound
    *(int*)site(0x484df2)=14612;assert(verify());
    *(uint8_t*)site(0x59bfda)=0x38;assert(!verify()); // branch destination cannot change
    *(uint8_t*)site(0x59bfda)=0x39;
    *(int*)site(0x59bfd5)=*(int*)site(0x490b09)=*(int*)site(0x484df2)=1099;assert(verify());
    for(uint32_t at:{0x579f10u,0x57ae45u}) {
        uint8_t original=*(uint8_t*)site(at);*(uint8_t*)site(at)=original+1;
        assert(!verify());*(uint8_t*)site(at)=original;assert(verify());
    }
    for(uint32_t at:{0x597220u,0x59729eu,0x593b78u,0x59781au}) {
        uint8_t original=*(uint8_t*)site(at);*(uint8_t*)site(at)=original^1;
        assert(!verify());*(uint8_t*)site(at)=original;assert(verify());
    }
    for(uint32_t at:{0x5aea37u,0x5aea42u,0x5aed00u,0x5aed0bu,0x5aed11u,0x5aed1cu,
                    0x5aee82u,0x5aee8du,0x5aee93u,0x5aee9eu}) {
        uint8_t original=*(uint8_t*)site(at);*(uint8_t*)site(at)=original^1;
        assert(!verify());*(uint8_t*)site(at)=original;assert(verify());
    }
    for(const auto& s:g_strategyMaximumSites)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_strategyCriticalLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_strategyFireLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_strategyFollowupLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_strategyCharmLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_defenseLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_ironwallLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_assistLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_counterImmunityLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_doubleAttackLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_pincerLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_captureLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    for(const auto& s:g_reachLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_aiWeightLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.at)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_strategyStatusLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_strategyAmbushLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_researchGoldLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_domesticLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_diplomacyEligibilityLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_strategyGroundLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_luckyUnitEventLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_luckyDuelLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_luckyDuelPostLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_luckyDuelResponseLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_luckyMissionLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_diplomacyInteractionLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_diplomacyContestLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_nativeSkillQueryLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_strategyFireImmunityLayouts)for(int offset:{3,23}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_areaFriendlyLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_rawPriorityLayouts)for(int offset:{26,204}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_tribalAffinityLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    tribalAffinityLayoutTests();
    for(const auto& s:g_trapLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.at)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_steeringLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_attackReachLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_boleLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_zocLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    {auto a=(uint32_t*)site(0x56e165),b=(uint32_t*)site(0x56e192),anchor=(uint32_t*)site(0x56dbab);uint32_t saved=*anchor;
        *a=0x12345000;assert(!verify());*anchor=*a;assert(!verify());*b=*a;assert(verify());
        *a=*b=*anchor=0;assert(!verify());*a=*b=*anchor=saved;assert(verify());}
    {auto use=(uint32_t*)site(0x5a4b66),anchor=(uint32_t*)site(0x5a5130);uint32_t a=*use,b=*anchor;
        *use=0x12345000;assert(!verify());*anchor=*use;assert(verify());
        *use=*anchor=0;assert(!verify());*use=a;*anchor=b;assert(verify());}
    for(const auto& s:g_magicLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_plunderLayouts)for(int offset:{0,s.size-1}){
        auto p=(uint8_t*)site(s.start)+offset;*p^=1;assert(!verify());*p^=1;assert(verify());
    }
    for(const auto& s:g_bloodroadHandoffLayouts)for(int offset:{0,s.size-1}){
        auto at=(uint8_t*)site(s.start)+offset;uint8_t original=*at;*at^=1;
        assert(!verify());*at=original;assert(verify());
    }
    {
        uint32_t addresses[]={0x5a0783,0x5a078f,0x5a079a,0x5a07aa,0x5ae9d0,0x5af5ad,0x592486,0x5ae087},saved[8];
        uint32_t hazardWidth=*(uint32_t*)site(0x5ae50b),hazardBase=*(uint32_t*)site(0x5ae51f);
        *(uint32_t*)site(0x5ae50b)=249;*(uint32_t*)site(0x5ae51f)=0x12345000;
        for(int n=0;n<8;++n)saved[n]=*(uint32_t*)site(addresses[n]);
        *(int*)site(0x5a0783)=530;*(int*)site(0x5a078f)=*(int*)site(0x5a079a)=249;
        for(int n=3;n<8;++n)*(uint32_t*)site(addresses[n])=0x12345000;
        uint32_t reachSaved[5];int reachIndex=0;
        uint32_t zocSaved[8];int zocIndex=0;
        for(const auto& m:g_zocImmediates){zocSaved[zocIndex++]=*(uint32_t*)site(m.at);
            if(m.kind<3)*(uint32_t*)site(m.at)=m.kind==0?249:m.kind==1?530:0x12345000;}
        uint32_t attackSaved[4];int attackIndex=0;
        for(const auto& m:g_attackReachImmediates){attackSaved[attackIndex++]=*(uint32_t*)site(m.at);
            if(m.kind!=2)*(uint32_t*)site(m.at)=m.kind==0?249:m.kind==1?0x12345004:0x12345000;}
        for(const auto& m:g_reachMapImmediates){reachSaved[reachIndex++]=*(uint32_t*)site(m.at);
            *(uint32_t*)site(m.at)=m.kind==0?249:0x12345000+(m.kind==2?4:0);}
        uint32_t magicSaved[15];int magicIndex=0;
        for(const auto& m:g_magicMapImmediates){magicSaved[magicIndex++]=*(uint32_t*)site(m.at);
            *(uint32_t*)site(m.at)=m.kind==0?249:0x12345000+(m.kind==2?8:0);}
        assert(verify());
        *(uint32_t*)site(g_reachMapImmediates[0].at)^=4;assert(!verify());*(uint32_t*)site(g_reachMapImmediates[0].at)^=4;
        *(uint32_t*)site(g_magicMapImmediates[0].at)^=4;assert(!verify());*(uint32_t*)site(g_magicMapImmediates[0].at)^=4;
        *(int*)site(0x5a079a)=250;assert(!verify());*(int*)site(0x5a079a)=249;
        *(int*)site(0x5a0783)=0;assert(!verify());*(int*)site(0x5a0783)=530;
        *(uint32_t*)site(0x592486)=0x12346000;assert(!verify());
        *(uint32_t*)site(0x592486)=0x12345000;
        *(uint32_t*)site(0x5ae087)=0x12346000;assert(!verify());
        reachIndex=0;for(const auto& m:g_reachMapImmediates)*(uint32_t*)site(m.at)=reachSaved[reachIndex++];
        zocIndex=0;for(const auto& m:g_zocImmediates)*(uint32_t*)site(m.at)=zocSaved[zocIndex++];
        attackIndex=0;for(const auto& m:g_attackReachImmediates)*(uint32_t*)site(m.at)=attackSaved[attackIndex++];
        magicIndex=0;for(const auto& m:g_magicMapImmediates)*(uint32_t*)site(m.at)=magicSaved[magicIndex++];
        *(uint32_t*)site(0x5ae50b)=hazardWidth;*(uint32_t*)site(0x5ae51f)=hazardBase;
        for(int n=0;n<8;++n)*(uint32_t*)site(addresses[n])=saved[n];assert(verify());
    }
    assert(contains(108, 146) && contains(108, 1) && contains(108, 85) && contains(108, 79));
    // Exercise IDs >=128 and the four income effects in addition to the shipped sample.
    g_skills[249] = g_skills[100];
    for (int i : {87, 88, 89, 90}) g_skills[249].effects[i] = true;
    g_skills[249].effects[SKILL_MERCHANT] = true;
    g_count = 250;
    uint8_t old = *(uint8_t*)g_hooks[0].at;
    *(uint8_t*)g_hooks[0].at = 0xcc;
    assert(!skillsInstall() && !memcmp((void*)g_hooks[1].at, g_hooks[1].expected, g_hooks[1].size));
    *(uint8_t*)g_hooks[0].at = old;
    assert(skillsInstall());
    assert(!skillsInstall());

    typedef int(__thiscall* Check)(void*, int);
    typedef void(__thiscall* Set)(void*, int);
    uint8_t person[0x190] = {};
    auto has = (Check)site(0x4890f0); auto set = (Set)site(0x4899d0);
    set(person, 101);
    for (int id : {1, 37, 2, 3, 61, 101}) assert(has(person, id));
    assert(!has(person, 57) && !has(person, -1));
    set(person, 249); assert(has(person, 57) && has(person, 249));
    assert(has(person,SKILL_MERCHANT));
    set(person, 250); assert(has(person, 249));
    set(person, 128); assert(has(person, 249)); // undefined combinations cannot be assigned
    set(person,SKILL_MERCHANT);assert(has(person,SKILL_MERCHANT) && !has(person,0) && !has(person,75));
    set(person, -1); assert(!has(person, 1));
    set(person, 0); assert(has(person, 0) && !has(person, 61));
    void* vt[24] = {}; vt[0x5c / 4] = fakeMember; void* unit = vt;
    assert(((Check)site(0x47a8a0))(&unit, 61) && !((Check)site(0x47a8a0))(&unit, 1));
    puts("PASS native hooks: officer matching/setter and units with member IDs above 1099");

    std::vector<uint8_t> worldStorage(0x80ef8 + BASE * RECORD);
    auto world = worldStorage.data();
    for (int i = 0; i < BASE; ++i) {
        uint8_t* r = world + 0x80ef8 + i * RECORD;
        *(uint32_t*)r = 0x79c90c; *(int*)(r + 0x60) = i % 8; *(int*)(r + 0x64) = 3;
    }
    typedef uint8_t*(__thiscall* Get)(void*, int);
    typedef int(__thiscall* Id)(void*, const void*);
    auto get = (Get)site(0x490c50); auto idOf = (Id)site(0x4915e0);
    auto r = get(world, 249);
    auto merchant=get(world,SKILL_MERCHANT);
    assert(merchant && idOf(world,merchant)==SKILL_MERCHANT &&
           !strcmp((char*)merchant+4,g_skills[SKILL_MERCHANT].name) &&
           !strcmp((char*)merchant+0x1c,g_skills[SKILL_MERCHANT].description));
    assert(r && idOf(world, r) == 249 && !strcmp((char*)r + 9, g_skills[100].name));
    assert(!strcmp((char*)r + 4, g_skills[100].name)); // native short-name display path
    assert(idOf(world, get(world, 99)) == 99 && idOf(world, nullptr) == -1);
    assert(idOf(world, r + 1) == -1 && !get(world, 250));
    assert(idOf(world, get(world, 128)) == 128); // gaps are safe native list entries
    typedef int(__thiscall* NoArg)(void*);
    assert(((NoArg)site(0x4945d0))(r) == 249);
    assert(!memcmp((void*)site(0x4c93e2), "\x6a\x05", 2));
    assert(!memcmp((void*)site(0x490dac), "\xb8\x64\x00\x00\x00", 5));
    puts("PASS native hooks: records, names, inverse IDs, sorting, gaps; native list count unchanged");

    g_serializeOriginal = (Serialize)fakeSerialize;
    auto serializer = (Serialize)(site(0x48b981) + *(int32_t*)site(0x48b97d));
    uint8_t stream[16] = {};
    for (int original : {0, 99, 100, 127, 128, 147, 249, -1}) {
        int value = original; *(int*)(stream + 8) = 0;
        assert(serializer(stream, &value) == stream && stream[12] == (uint8_t)original);
        value = 0; *(int*)(stream + 8) = 1;
        assert(serializer(stream, &value) == stream && value == original);
    }
    int value = 0; stream[12] = 254; serializer(stream, &value); assert(value == -1);
    puts("PASS skill-field serialization: save/load 0,99,100,127,128,249 and none; bad IDs rejected");

    g_incomeEntry = site(0x4cea41);
    *(uint8_t*)g_incomeResume = 0xc3; // stop after the income hook; no game list manager required
    int flags[BASE + 2] = {}; flags[BASE] = 123; flags[BASE + 1] = 456;
    runIncome(flags, 249);
    for (int i : {57, 79, 82, 87, 88, 89, 90}) assert(flags[i]);
    assert(!flags[0] && flags[BASE] == 123 && flags[BASE + 1] == 456);
    memset(flags, 0, sizeof flags); runIncome(flags, -1); assert(!flags[0]);
    runIncome(flags, 75); assert(flags[75]);
    puts("PASS income native bridge: composed effects, no-skill, base effects and array bounds");
    memcpy((void*)g_boleAllow, "\xb8\x01\x00\x00\x00\xc3", 6);
    memcpy((void*)g_boleReject, "\x33\xc0\xc3", 3);
    g_boleEntry = site(0x4cf711); g_affinityOriginal = (Affinity)fakeAffinity;
    vt[0x5c / 4] = fakeRecruiter; g_candidate = person;
    assert(runBole(&unit, person, 0) == 1);
    g_affinity = 26; assert(runBole(&unit, person, 0) == 0);
    g_recruiter = -1; assert(runBole(&unit, person, 0) == 0);
    assert(runBole(&unit, person, 1) == 1); // native same-force branch unchanged
    puts("PASS Bole native bridge: remote recruitment, affinity 25/26, missing recruiter and same-force path");
    precisionTelemetryTests();
    economyTelemetryTests();
    incomeBonusTelemetryTests();
    domesticTelemetryTests();
    battleTelemetryTests();
    pressureTelemetryTests();
    movementTelemetryTests();
    movementStatTelemetryTests();
    revenueTelemetryTests();
    unrestTelemetryTests();
    personTelemetryTests();
    discoveryTelemetryTests();
    researchTelemetryTests();
    environmentTelemetryTests();
    harvestTelemetryTests();
    diplomacyTelemetryTests();
    luckyUnitEventTelemetryTests();
    luckyDuelTelemetryTests();
    luckyMissionTelemetryTests();
    queryContextTelemetryTests();
    rawPriorityTelemetryTests();
    tribalAffinityTelemetryTests();
    aiScoreTelemetryTests();
    areaFriendlyTelemetryTests();
    constructionTelemetryTests();
    criticalTelemetryTests();
    defenseTelemetryTests();
    ambushAttackTelemetryTests();
    ironwallTelemetryTests();
    assistTelemetryTests();
    counterImmunityTelemetryTests();
    doubleAttackTelemetryTests();
    strategyCostTelemetryTests();
    afterAttackTelemetryTests();
    pincerTelemetryTests();
    captureTelemetryTests();
    strategyImmunityTelemetryTests();
    magicTelemetryTests();
    reachTelemetryTests();
    aiWeightTelemetryTests();
    attackReachTelemetryTests();
    boleTelemetryTests();
    vineTelemetryTests();
    trapTelemetryTests();
    puts("ALL SKILL TESTS PASSED");
    return 0;
}
