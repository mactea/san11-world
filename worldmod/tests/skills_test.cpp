// Verify the game's exact instructions, then exercise the installed x86 branches in a relocated
// code buffer. Native stream I/O and the surrounding income-list traversal are stubbed explicitly.
#include "../src/skills.cpp"
#include <assert.h>
#include <vector>

static uint8_t* g_image;
static uintptr_t site(uint32_t address) { return (uintptr_t)g_image + address - 0x400000; }
static void mapGame(const wchar_t* path) {
    FILE* f = nullptr; assert(!_wfopen_s(&f, path, L"rb") && f);
    fseek(f, 0, SEEK_END); size_t size = ftell(f); rewind(f);
    std::vector<uint8_t> bytes(size); assert(fread(bytes.data(), 1, size, f) == size); fclose(f);
    auto dos = (IMAGE_DOS_HEADER*)bytes.data();
    auto nt = (IMAGE_NT_HEADERS32*)(bytes.data() + dos->e_lfanew);
    assert(nt->OptionalHeader.ImageBase == 0x400000);
    uint8_t* base = (uint8_t*)VirtualAlloc(nullptr, 0x4a0000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    assert(base); g_image = base;
    auto section = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(section[i].Name, ".text", 5) && memcmp(section[i].Name, ".rdata", 6)) continue;
        memcpy(base + section[i].VirtualAddress, bytes.data() + section[i].PointerToRawData, section[i].SizeOfRawData);
    }
    for (auto& h : g_hooks) h.at = (uint32_t)site(h.at);
    g_incomeResume = (uint32_t)site(0x4cea51);
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
static int g_recruiter = 14162, g_affinity = 25;
static void* g_candidate;
static int __fastcall fakeRecruiter(void*, void*, int queried) { return queried == 146 ? g_recruiter : -1; }
static int __fastcall fakeAffinity(void* target, void*, int recruiter) {
    assert(target == g_candidate && recruiter == 14162); return g_affinity;
}
static uintptr_t g_boleEntry;
__declspec(naked) static int __cdecl runBole(void* building, void* candidate, int sameForce) {
    __asm {
        push esi
        sub esp, 0x1c
        mov esi, [esp + 0x28]
        mov eax, [esp + 0x24]
        mov [esp + 0x14], eax
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
__declspec(naked) static void __cdecl runIncome(int* flags, int owned) {
    __asm {
        push ebx
        mov ebx, [esp + 8]
        mov eax, [esp + 12]
        mov edx, g_incomeEntry
        call edx
        pop ebx
        ret
    }
}

int wmain(int argc, wchar_t** argv) {
    assert(argc == 3); mapGame(argv[1]); parserTests();
    assert(verify());
    assert(!skillsInstall());
    assert(!skillsPrepare(L"nonexistent-skills-test.txt", nullptr));
    assert(verify()); // failed load must not mutate native instructions
    assert(skillsPrepare(argv[2], nullptr));
    assert(g_count == 109 && verify()); // prepare/verify_only does not write patches
    assert(contains(108, 146) && contains(108, 1) && contains(108, 85) && contains(108, 79));
    // Exercise IDs >=128 and the four income effects in addition to the shipped sample.
    g_skills[249] = g_skills[100];
    for (int i : {75, 76, 93, 94}) g_skills[249].effects[i] = true;
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
    set(person, 250); assert(has(person, 249));
    set(person, 128); assert(has(person, 249)); // undefined combinations cannot be assigned
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
    for (int original : {0, 99, 100, 127, 128, 249, -1}) {
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
    for (int i : {57, 79, 82, 75, 76, 93, 94}) assert(flags[i]);
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
    puts("ALL SKILL TESTS PASSED");
    return 0;
}
