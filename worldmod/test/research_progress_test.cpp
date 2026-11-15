// Exercises the production code and x86 call wrappers against native-format
// records. No game process or player save is modified.
#include "../src/research_progress.cpp"
#include <stdio.h>
#include <vector>
static int failures = 0;
static void check(bool ok, const char* label) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", label); failures += !ok;
}
static int __cdecl validFixture(void* p) { return p && *(int*)((uint8_t*)p + 4) >= 0; }
static bool exchangeAccept = true, teachingSucceeds = true, humanFixture = true;
static int confirmations = 0, teachingStarts = 0, remainingCount = -1, remainingPrice = -1;
bool researchExchangeConfirm(int, int, const uint8_t*) { ++confirmations; return exchangeAccept; }
void researchExchangeRemaining(void*, const char*, int count, int price) { remainingCount=count; remainingPrice=price; }
static int __fastcall completedFixture(uint8_t* force, void*, int id) {
    if (id >= 0 && id < 48) return (force[0xb0+id/8]>>(id%8))&1;
    for (int g=0; g<10; ++g) if (((int*)(force+0xe8))[g]==id)
        return (force[0x110+g/8]>>(g%8))&1;
    return 0;
}
static uint8_t fixtureCities[575][16] = {};
static void* cityVtable[17] = {};
static int __fastcall cityOwner(void* p, void*) { return *(int*)((uint8_t*)p + 8); }
static uint8_t* __fastcall baseFixture(uintptr_t, void*, int id) {
    return id >= 0 && id < 575 ? fixtureCities[id] : nullptr;
}
static void setCities(int fid, int n) {
    for (int id = 0; id < 530; ++id)
        if (*(int*)(fixtureCities[id] + 8) == fid) *(int*)(fixtureCities[id] + 8) = -1;
    for (int id = 0; id < 530 && n; ++id)
        if (*(int*)(fixtureCities[id] + 8) < 0) { *(int*)(fixtureCities[id] + 8) = fid; --n; }
    if (n) exit(2);
}
static void quiet(const char*, ...) {}
static void setProgress(uint8_t* f, int n);
static bool __cdecl manualFixture(uint8_t*) { return humanFixture; }
static uint8_t* __fastcall forceFixture(uintptr_t, void*, int id) {
    return id >= 0 && id < g_forces->R ? g_forces->FORCEARR + id*0x12c : nullptr;
}
static void __fastcall pointsFixture(uint8_t* force, void*, int points) {
    check(points >= 0 && points <= 10000, "exchange points never go negative or exceed the native cap");
    *(uint16_t*)(force+0xa2) = (uint16_t)points;
}
static int __cdecl teachingFixture(uint8_t* command) {
    ++teachingStarts;
    if (!teachingSucceeds) return 0;
    auto person = *(uint8_t**)(command+4);
    *(int*)(person+0x13c) = 42;
    *(int*)(person+0x140) = *(int*)(command+8);
    *(int*)(person+0x150) = 0;
    return 1;
}
static uint8_t* finishPerson;
static uint8_t* finishRecord;
static int finishFrame[4], finishValue;
static uint8_t finishSkipped;
static uint8_t* remainingForce;
static uint8_t* remainingRecord;
__declspec(noinline) static void invokeRemainingUi() { __asm {
    push ebx
    sub esp,30h
    mov eax,remainingForce
    mov [esp+10h],eax
    mov ebx,remainingRecord
    push 1
    push 0
    push 0
    call remainingUi
    add esp,12
    add esp,30h
    pop ebx
} }
__declspec(noinline) static void invokeFinish() { __asm {
    push ebx
    push esi
    push ebp
    mov ebx,finishPerson
    mov esi,finishRecord
    lea ebp,finishFrame
    call finishTeaching
    setz finishSkipped
    mov finishValue,eax
    pop ebp
    pop esi
    pop ebx
} }
static void exchangeTests(uint8_t* force) {
    auto originalStart = startTeachingOriginal;
    auto originalForce = getForce;
    auto originalPoints = setPoints;
    auto originalManual = manualForce;
    startTeachingOriginal = teachingFixture;
    getForce = (ForceFn)forceFixture;
    setPoints = (PointsFn)pointsFixture;
    manualForce = manualFixture;
    int fid = forceId(force);
    setCities(fid,100); setProgress(force,48);
    uint8_t person[400] = {};
    struct Command { uint8_t* base; uint8_t* person; int id; }
        cmd = {fixtureCities[0], person, 29};
    *(int*)(fixtureCities[0]+8) = fid;
    for (auto entry : {std::pair<int,int>{95,3000}, {29,1500}, {28,800}, {62,400}}) {
        int id=entry.first, price=entry.second;
        if (id >=48) {
            int g=*(int*)(record(id)+0x64);
            ((int*)(force+0xe8))[g]=id; force[0x110+g/8] |= 1<<(g%8);
        }
        int total=quota(record(id),4);
        if (id<48) force[0xb8+id]=(uint8_t)total;
        else force[0x114+*(int*)(record(id)+0x64)]=(uint8_t)total;
        cmd.id=id;
        check(exchangePrice(force,record(id))==price,"S/A/B/C exchange prices use the existing grade table");
        *(uint16_t*)(force+0xa2)=(uint16_t)(price-1);
        auto before=std::vector<uint8_t>(force,force+0x12c);
        check(limit(record(id),force)==total,"unaffordable exhausted skill remains unavailable");
        check(!startTeaching((uint8_t*)&cmd) && !memcmp(before.data(),force,0x12c),
              "insufficient points reject the order without spending points or free uses");
        *(uint16_t*)(force+0xa2)=(uint16_t)price;
        before.assign(force,force+0x12c);
        for(int n=0;n<10;++n) check(limit(record(id),force)==total+1,"affordable exhausted skill exposes one paid training");
        check(!memcmp(before.data(),force,0x12c),"list previews never debit points or alter free uses");
        exchangeAccept=false;
        check(!startTeaching((uint8_t*)&cmd) && !memcmp(before.data(),force,0x12c),"canceling exchange is free");
        exchangeAccept=true; teachingSucceeds=false;
        check(!startTeaching((uint8_t*)&cmd) && !memcmp(before.data(),force,0x12c),"native start failure does not charge");
        teachingSucceeds=true;
        check(startTeaching((uint8_t*)&cmd)==1 && *(uint16_t*)(force+0xa2)==0 && usedCount(force,id)==total,
              "successful order consumes exactly one exchange price and preserves free-use history");
        check(paidTeaching(person,id) && !paidTeaching(person,(id+1)%98),"paid marker belongs to the exact training project");
        auto saved=std::vector<uint8_t>(person,person+400);
        memset(person,0,400); memcpy(person,saved.data(),400);
        finishPerson=person; finishRecord=record(id); finishFrame[3]=id;
        invokeFinish();
        check(finishSkipped && finishValue==0,"saved paid training skips the native free-use increment on completion");
        setCities(fid,19); invokeFinish();
        check(finishSkipped && usedCount(force,id)==total,"territory loss during paid training cannot erase free-use history");
        setCities(fid,100);
        *(int*)(person+0x150)=0; invokeFinish();
        check(!finishSkipped && finishValue==*(int*)(record(id)+0x44),"ordinary training retains the native free-use increment branch");
    }
    cmd.id=29; force[0xb8+29]=0; *(uint16_t*)(force+0xa2)=10000;
    int oldConfirm=confirmations;
    check(startTeaching((uint8_t*)&cmd)==1 && *(uint16_t*)(force+0xa2)==10000 && !paidTeaching(person,29) && confirmations==oldConfirm,
          "remaining free uses take priority without payment or an exchange dialog");
    force[0xb8+29]=10; setProgress(force,0);
    check(exchangePrice(force,record(29))==0 && limit(record(29),force)==10,"unresearched skills cannot be purchased");
    setProgress(force,48); humanFixture=false;
    check(startTeaching((uint8_t*)&cmd)==1 && *(uint16_t*)(force+0xa2)==8500 && confirmations==oldConfirm,
          "AI pays the same price through the same order path without a player modal");
    remainingForce=force; remainingRecord=record(29); invokeRemainingUi();
    check(remainingCount==1 && remainingPrice==1500,"skill UI wrapper reads the correct force and displays the exchange price");
    for(int n=0;n<32;++n) {
        *(uint16_t*)(force+0xa2)=1500;
        startTeaching((uint8_t*)&cmd);
        finishPerson=person; finishRecord=record(29); finishFrame[3]=29; invokeFinish();
        check(finishSkipped && usedCount(force,29)==10 && *(uint16_t*)(force+0xa2)==0,
              "repeated exchanges never overflow the byte counter or grant additional free training");
    }
    humanFixture=true;
    check(exchangePrice(force,record(0))==0 && exchangePrice(force,record(16))==0,
          "base abilities and aptitudes do not enter the skill exchange");
    auto other=g_forces->FORCEARR+192*0x12c;
    *(uint16_t*)(other+0xa2)=0;
    check(limit(record(29),other)==std::max(quota(record(29),cityStage(ownedCities(192))),usedCount(other,29)),
          "another force cannot spend this force's points");
    startTeachingOriginal=originalStart; getForce=originalForce; setPoints=originalPoints; manualForce=originalManual;
    *(uint16_t*)(force+0xa2)=0;
}
static void setProgress(uint8_t* f, int n) {
    memset(f + 0xb0, 0, 6);
    for (int i = 0; i < n; ++i) f[0xb0 + i / 8] |= 1 << (i % 8);
}
static int slots(uint8_t* f) {
    int n = 0; for (int i = 0; i < 10; ++i) n += ((int*)(f + 0xe8))[i] >= 48; return n;
}
static void initForce(uint8_t* f, int ruler) {
    memset(f, 0, 0x12c); *(int*)(f + 4) = ruler;
    for (int i = 0; i < 10; ++i) ((int*)(f + 0xe8))[i] = i % 2 ? -1 : 48 + 5 * i;
}
static void readMaster(const char* path) {
    FILE* fp = nullptr; fopen_s(&fp, path, "rb"); if (!fp) exit(2);
    fseek(fp, -98 * 74, SEEK_END);
    for (int i = 0; i < 98; ++i) {
        uint8_t b[74]; if (fread(b, 1, 74, fp) != 74) exit(2);
        auto r = record(i); memset(r, 0, RECORD);
        memcpy(r + 4, b, 9); memcpy(r + 0xd, b + 9, 55);
        *(int*)(r + 0x44) = b[64]; *(int*)(r + 0x48) = b[65];
        for (int p = 0; p < 3; ++p) *(int*)(r + 0x4c + 4 * p) = (int8_t)b[66 + p];
        *(int*)(r + 0x58) = (int8_t)b[69]; *(int*)(r + 0x5c) = b[70];
        r[0x61] = b[71]; r[0x60] = b[72]; *(int*)(r + 0x64) = (int8_t)b[73];
    }
    fclose(fp);
}
static uint8_t* hookForce;
static uint8_t* hookRecord;
static int hookResult;
// Recreate the caller's force location for each naked wrapper; a wrong stack
// offset, callee cleanup or register selection fails these executions.
__declspec(noinline) static void invokeWrappers() { __asm {
    push edi
    push ebp
    sub esp,20h
    mov edi,hookForce
    mov ebp,hookForce
    mov ecx,hookRecord
    call limitCheck
    movzx eax,al
    mov hookResult,eax
    add esp,20h
    pop ebp
    pop edi
} }
__declspec(noinline) static void invokeAiFirst() { __asm {
    sub esp,20h
    mov eax,hookForce
    mov [esp+1ch],eax
    mov ecx,hookRecord
    call limitAiFirst
    movzx eax,al
    mov hookResult,eax
    add esp,20h
} }
__declspec(noinline) static void invokeAiSecond() { __asm {
    push ebp
    mov ebp,hookForce
    mov ecx,hookRecord
    call limitAiSecond
    movzx eax,al
    mov hookResult,eax
    pop ebp
} }
__declspec(noinline) static void invokeUi() { __asm {
    sub esp,20h
    mov eax,hookForce
    mov [esp+10h],eax
    mov ecx,hookRecord
    call limitUi
    movzx eax,al
    mov hookResult,eax
    add esp,20h
} }
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    if (VirtualAlloc((void*)0x7280000, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) != (void*)0x7280000) {
        printf("world fixture allocation failed: %lu\n", GetLastError()); return 2;
    }
    // Move only fixture instruction pages away from the CRT's low-address heap.
    // Production addresses/signatures are checked separately against the game EXE.
    for (auto& s : sites) s.at += 0x20000000;
    for (const auto& s : sites) {
        auto page = (void*)(s.at & ~uintptr_t(0xffff));
        MEMORY_BASIC_INFORMATION info; VirtualQuery(page, &info, sizeof info);
        if (info.State == MEM_FREE && VirtualAlloc(page, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE) != page) {
            printf("native fixture allocation failed at %p: %lu\n", page, GetLastError()); return 2;
        }
        VirtualQuery(page, &info, sizeof info);
        if (info.State != MEM_COMMIT || info.AllocationBase != page) {
            printf("native fixture page occupied at %p\n", page); return 2;
        }
    }
    readMaster(argv[1]);
    Forces forces; forces.R = 249; forces.F = 254;
    std::vector<uint8_t> storage(forces.F * 0x12c); forces.FORCEARR = storage.data();
    g_forces = &forces; isValid = validFixture; getBase = (BaseFn)baseFixture;
    isCompleted = (CompletedFn)completedFixture; g_cityCount = 530; g_on = true;
    cityVtable[0x40 / 4] = (void*)cityOwner;
    for (auto& city : fixtureCities) { *(void***)city = cityVtable; *(int*)(city + 8) = -1; }
    auto f = forces.FORCEARR + 192 * 0x12c;
    initForce(f, 9227);
    uint8_t original[0x12c]; memcpy(original, f, sizeof original);
    for (int n : {0, 11, 12, 23, 24, 35, 36, 47, 48}) {
        memcpy(f, original, sizeof original); setProgress(f, n);
        auto before = std::vector<uint8_t>(f, f + 0x12c);
        unlock(f);
        int expected = n == 48 ? 10 : 5 + n / 12;
        check(slots(f) == expected, "hidden slots change only at 12/24/36/48 completed fixed projects");
        for (int i = 0; i < 10; ++i) {
            int id = ((int*)(f + 0xe8))[i];
            if (((int*)(original + 0xe8))[i] >= 0) check(id == ((int*)(original + 0xe8))[i], "existing hidden choice retained");
            if (id >= 0) check(*(int*)(record(id) + 0x64) == i, "one valid native variant per hidden group");
        }
        memcpy(before.data() + 0xe8, f + 0xe8, 40);
        check(!memcmp(f, before.data(), 0x12c), "unlock changes only selected hidden IDs, not flags or used counts");
        auto snapshot = std::vector<uint8_t>(f, f + 0x12c);
        unlock(f); check(!memcmp(f, snapshot.data(), 0x12c), "repeated checks grant no extra slots or uses");
    }
    initForce(f, 9227); setProgress(f, 48); f[0xb8 + 29] = 2; f[0x114] = 2; f[0x110] = 1;
    unlock(f);
    check(f[0xb8 + 29] == 2 && f[0x114] == 2 && f[0x110] == 1, "spent counts and already completed hidden research survive");
    auto saved = std::vector<uint8_t>(f, f + 0x12c);
    memset(f, 0, 0x12c); memcpy(f, saved.data(), saved.size()); unlock(f);
    check(!memcmp(f, saved.data(), saved.size()), "native force save/load needs no additional quota metadata");
    for (int i = 0; i < 98; ++i) {
        auto r = record(i); int previous = nativeLimit(r);
        for (int s = 0; s <= 4; ++s) {
            int q = quota(r, s);
            check(q >= previous && q <= 255, "quota grows monotonically without reducing original allocation");
            if (*(int*)(r + 0x48) != 2) check(q == nativeLimit(r), "base abilities and aptitudes are unchanged");
            previous = q;
        }
    }
    check(quota(record(95), 4) == 3 && quota(record(71), 4) == 5, "S maximum three preserves an original five-use S skill");
    check(quota(record(29), 4) == 10 && quota(record(28), 4) == 20 && quota(record(62), 4) == 30,
          "final A/B/C teaching totals match 10/20/30");
    check(quota(record(29), 0) == 3 && quota(record(29), 1) == 4 && quota(record(29), 2) == 6 && quota(record(29), 3) == 8,
          "A three-use ability grows 3/4/6/8/10 rather than granting all uses at the first milestone");
    hookForce = f; hookRecord = record(29);
    setCities(192, 100);
    invokeWrappers(); check(hookResult == 10, "validation wrapper takes force from EDI");
    invokeAiFirst(); check(hookResult == 10, "first AI ranking wrapper takes force from the native stack");
    invokeAiSecond(); check(hookResult == 10, "second AI ranking wrapper takes force from EBP");
    invokeUi(); check(hookResult == 10, "all UI quota wrappers take force from the native stack");
    auto other = forces.FORCEARR + 248 * 0x12c; initForce(other, 5); setProgress(other, 11);
    setCities(248, 19);
    check(limit(record(29), other) == 3 && limit(record(29), f) == 10, "force 248 and force 192 have independent quotas");
    other[0x110] = 0xff; other[0x111] = 0xff;
    check(stage(other) == 0 && limit(record(29), other) == 3, "hidden research completion cannot accelerate fixed-project milestones");
    setProgress(other, 48);
    check(limit(record(29), other) == 3, "completing all fixed research cannot grant teaching uses without sufficient cities");
    for (int cities : {0, 19, 20, 39, 40, 79, 80, 99, 100, 200}) {
        setCities(248, cities);
        int expected = cities >= 100 ? 10 : cities >= 80 ? 8 : cities >= 40 ? 6 : cities >= 20 ? 4 : 3;
        check(ownedCities(248) == cities && limit(record(29), other) == expected,
              "owned-city thresholds 20/40/80/100 control teaching quotas");
    }
    for (int id = 530; id < 575; ++id) *(int*)(fixtureCities[id] + 8) = 248;
    check(ownedCities(248) == 200, "ports and gates do not increase teaching quotas");
    setProgress(other, 0);
    check(limit(record(29), other) == 10, "one-hundred-plus cities grant the full quota independently of research progress");
    initForce(other, 5); unlock(other);
    check(slots(other) == 5, "large territory does not open hidden slots before the research milestones");
    other[0xb8 + 29] = 8; setCities(248, 19);
    check(limit(record(29), other) == 8 && other[0xb8 + 29] == 8,
          "territory loss shows zero remaining without negative values or erasing spent uses");
    setCities(248, 100);
    check(limit(record(29), other) - other[0xb8 + 29] == 2,
          "recapturing cities restores only unspent entitlement, never a fresh allocation");
    ((int*)(other + 0xe8))[1] = 57; other[0x114 + 1] = 9; setCities(248, 19);
    check(limit(record(57), other) == 9 && other[0x114 + 1] == 9,
          "territory losses also preserve hidden-skill use counts");
    auto exhausted = record(40); // capture: native one use, S final three
    f[0xb8 + 40] = 1;
    check(limit(exhausted, f) - f[0xb8 + 40] == 2, "an exhausted native capture allocation gains two uses, not a fresh allocation");
    f[0xb8 + 40] = 3;
    check(limit(exhausted, f) == f[0xb8 + 40], "spending the expanded allocation cannot refill it");
    auto barbarian = forces.FORCEARR + 249 * 0x12c; initForce(barbarian, 6); setProgress(barbarian, 48);
    unlock(barbarian); check(slots(barbarian) == 5 && limit(record(29), barbarian) == 3, "barbarians and bandits do not gain research");
    auto invalid = std::vector<uint8_t>(f, f + 0x12c); *(int*)(invalid.data() + 4) = -1;
    check(limit(record(29), invalid.data()) == 3, "foreign force pointers cannot inherit another force's progress");
    exchangeTests(other);

    wchar_t tmp[MAX_PATH], folder[MAX_PATH]; GetTempPathW(MAX_PATH, tmp); GetTempFileNameW(tmp, L"rpg", 0, folder);
    DeleteFileW(folder); CreateDirectoryW(folder, nullptr);
    wchar_t ini[MAX_PATH]; swprintf_s(ini, L"%s\\worldmod.ini", folder);
    WritePrivateProfileStringW(L"test", L"research_progress", L"1", ini);
    for (const auto& s : sites) memcpy((void*)s.at, s.expected, s.size);
    g_on = false;
    *(uint8_t*)sites[7].at = 0;
    check(!researchProgressInstall(folder, L"test", &forces, true, quiet) && !g_on, "one bad native signature rejects every hook");
    check(!memcmp((void*)sites[0].at, sites[0].expected, sites[0].size), "rejection leaves earlier verified sites untouched");
    memcpy((void*)sites[7].at, sites[7].expected, sites[7].size);
    check(researchProgressInstall(folder, L"test", &forces, false, quiet) && !g_on, "verify-only writes no game code");
    check(researchProgressInstall(folder, L"test", &forces, true, quiet) && g_on, "all eleven native hooks install together");
    using SelectedFn = int(__thiscall*)(uint8_t*, int);
    using HasFn = int(__thiscall*)(uint8_t*, int);
    initForce(other, 5); setProgress(other, 48);
    int choice = ((SelectedFn)sites[0].at)(other, 1);
    check(choice >= 48 && slots(other) == 10 && ((HasFn)sites[1].at)(other, choice), "native thiscall entry points unlock and expose all eligible groups");
    DeleteFileW(ini); RemoveDirectoryW(folder);
    printf("research runtime fixture: %d failures\n", failures);
    return failures ? 1 : 0;
}
