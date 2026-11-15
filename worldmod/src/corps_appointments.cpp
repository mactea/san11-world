// Native PK corps dialog extension. Built separately to preserve the installed M10 runtime.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

using Byte = unsigned char;
static void* const World = reinterpret_cast<void*>(0x7201958);
template<class T> static T Fn(uintptr_t address) { return reinterpret_cast<T>(address); }
static int& Int(void* p, unsigned offset) { return *reinterpret_cast<int*>(static_cast<Byte*>(p) + offset); }
static void*& Ptr(void* p, unsigned offset) { return *reinterpret_cast<void**>(static_cast<Byte*>(p) + offset); }
using Method = void (__thiscall*)(void*);
using Get = void* (__thiscall*)(void*, int);
static void* Person(int id) { return Fn<Get>(0x490b00)(World, id); }
static bool Valid(void* p) { return p && Fn<int (__cdecl*)(void*)>(0x47a630)(p); }
static int PersonId(void* p) { return Fn<int (__thiscall*)(void*, void*)>(0x491310)(World, p); }
static void Log(const char* event, int corps = -1, int person = -1) {
    FILE* file = nullptr;
    if (!fopen_s(&file, "worldmod_corps.log", "a")) {
        fprintf(file, "%s corps=%d person=%d\n", event, corps, person);
        fclose(file);
    }
}

struct NativeList {
    uintptr_t words[8] = {0x79bf44, 0, 0, 0, 0, 0, 10, 0};
    ~NativeList() { Fn<Method>(0x47c100)(this); }
    void Add(void* person) { Fn<void (__thiscall*)(void*, void*)>(0x47c1b0)(this, person); }
    void* First() const { return words[3] ? *reinterpret_cast<void**>(words[1] + 8) : nullptr; }
};

static void* dialog;
static void* selected;
static int selectedCorps = -1;
static int pendingCorps = -1;
static int pendingPerson = -1;
static char title[64];
static char abilityTab[16];
static char abilityHint[160];
static char compositeCaption[16];
static constexpr int CompositeColumn = 0x10e;
static uintptr_t governorListVtable[0x200 / sizeof(uintptr_t)];
static bool buildingGovernorPicker;
static constexpr uintptr_t GovernorResource = 0x7f1ad0 + 0x13d7 * 24;
static bool Replace(uintptr_t address, const void* expected, const void* replacement, size_t size);

static int CompositeTenths(void* person) {
    auto p = static_cast<Byte*>(person);
    return p[0x170] * 6 + p[0x173] * 4 + p[0x174] * 4;
}

struct ColumnInfo {
    const char* caption;
    short width, flags;
    int alignment, sortType;
};
static ColumnInfo* __cdecl GovernorColumn(ColumnInfo* out, int column) {
    Fn<ColumnInfo* (__cdecl*)(ColumnInfo*, int)>(0x4c7fa0)(out, column == CompositeColumn ? 68 : column);
    if (column == CompositeColumn) {
        out->caption = compositeCaption;
        out->width = 4;
    }
    return out;
}

static void* __fastcall GovernorCell(void* self, void*, void* out, void* person, int high, int column) {
    if (column != CompositeColumn || !person)
        return Fn<void* (__thiscall*)(void*, void*, void*, int, int)>(0x4dcec0)(self, out, person, high, column);
    char value[24];
    int tenths = CompositeTenths(person);
    if (tenths % 10) sprintf_s(value, "%d.%d", tenths / 10, tenths % 10);
    else sprintf_s(value, "%d", tenths / 10);
    Fn<void (__thiscall*)(void*, const char*)>(0x4719f0)(out, value);
    return out;
}

static int __fastcall GovernorSortKey(void* self, void*, void* person, int high, int column, int direction) {
    if (column == CompositeColumn && person) return CompositeTenths(person);
    return Fn<int (__thiscall*)(void*, void*, int, int, int)>(0x4dcf10)(self, person, high, column, direction);
}

static void* __fastcall CreateGovernorList(void* self, void*, int type, void* columns, int count) {
    if (buildingGovernorPicker) {
        columns = reinterpret_cast<void*>(&GovernorColumn);
        count = CompositeColumn + 1;
    }
    void* result = Fn<void* (__thiscall*)(void*, int, void*, int)>(0x4dcc30)(self, type, columns, count);
    if (buildingGovernorPicker) Ptr(result, 0) = governorListVtable;
    return result;
}

// City list nodes contain city pointers. Compare the native building identity,
// allowing the expanded map's IDs without relying on original array bounds.
static bool CityIncluded(void* list, int home) {
    auto node = Ptr(list, 4);
    for (int remaining = Int(list, 0xc); remaining > 0 && node; --remaining) {
        void* city = Ptr(node, 8);
        int id = Fn<int (__thiscall*)(void*, void*)>(0x491180)(World, city);
        if (id == home) return true;
        node = Ptr(node, 0);
    }
    return false;
}
static bool Eligible(void* person, int corps, void* cities) {
    if (!Valid(person) || Int(person, 0x94) != corps) return false;
    int status = Int(person, 0xa0);
    if (status < 1 || status > 3) return false;
    int home = Int(person, 0x98);
    return home >= 0 && Int(person, 0x9c) == home && CityIncluded(cities, home);
}

static void* __cdecl ChooseLeader(void* force, int corps, void* cities) {
    if (dialog && corps == selectedCorps) {
        if (Eligible(selected, corps, cities)) return selected;
        void* currentCorps = Fn<Get>(0x490ad0)(World, corps);
        if (Valid(currentCorps)) {
            void* currentLeader = Person(Int(currentCorps, 0xc));
            if (Eligible(currentLeader, corps, cities)) return currentLeader;
        }
    }
    return Fn<void* (__cdecl*)(void*, int, void*)>(0x5c0b10)(force, corps, cities);
}

static int __fastcall InitDialog(void* self, void*, void* force, int number, int flag) {
    dialog = self;
    selected = nullptr;
    selectedCorps = -1;
    pendingCorps = pendingPerson = -1;
    // The native initializer creates controls from these descriptors. Make
    // the governor label a button only while creating a delegated corps UI.
    short before[12], after[12];
    memcpy(before, reinterpret_cast<void*>(GovernorResource), sizeof before);
    memcpy(after, before, sizeof after);
    after[3] = 8;
    after[5] = 190;
    after[6] = 24;
    after[7] = after[8] = 0;
    after[9] = 74;
    after[10] = 22;
    after[11] = 0;
    bool changed = number > 1 && Replace(GovernorResource, before, after, sizeof before);
    int result = Fn<int (__thiscall*)(void*, void*, int, int)>(0x6038d0)(self, force, number, flag);
    if (changed) Replace(GovernorResource, after, before, sizeof before);
    selectedCorps = Int(self, 0x198);
    Log("dialog initialized", selectedCorps, number);
    if (result && number > 1) Fn<Method>(0x603030)(self);
    return result;
}

static void PickLeader(void* self) {
    if (self != dialog || Int(self, 0x194) <= 1 || selectedCorps < 0) return;
    NativeList candidates;
    // M10 patches this immediate to the actual expanded person count.
    int count = *reinterpret_cast<int*>(0x490b09) + 1;
    for (int id = 0; id < count; ++id) {
        void* person = Person(id);
        if (Eligible(person, selectedCorps, static_cast<Byte*>(self) + 0x19c)) candidates.Add(person);
    }
    if (!candidates.words[3]) return;
    alignas(4) Byte params[0x154] = {};
    alignas(4) Byte picker[0x2d0] = {};
    Fn<Method>(0x4ae240)(params);
    Int(params, 0x14) = 3; // Native list flags: select a row and notify its parent.
    Int(params, 0x1c) = 1; // Maximum selection: one officer.
    Ptr(params, 0xc) = title;
    int abilityColumns[] = {1, CompositeColumn, 68, 69, 70, 71, 72};
    struct { const char* caption; int* columns; int count; } abilityLayout = {abilityTab, abilityColumns, 7};
    Ptr(params, 0x10) = &abilityLayout;
    Fn<void (__thiscall*)(void*, const char*)>(0x4719f0)(params + 0x24, abilityHint);
    Ptr(params, 0x144) = &candidates;
    Fn<Method>(0x654020)(picker);
    buildingGovernorPicker = true;
    int prepared = Fn<int (__thiscall*)(void*, void*)>(0x654d40)(picker, params);
    buildingGovernorPicker = false;
    if (prepared) {
        Fn<void (__thiscall*)(void*, void*)>(0x654060)(picker, Ptr(self, 0x1bc));
        if (Fn<int (__thiscall*)(void*)>(0x64cb20)(picker) == 0x2710) {
            NativeList result;
            if (Fn<int (__thiscall*)(void*, void*)>(0x654fd0)(picker, &result) && result.First()) {
                selected = result.First();
                Log("selected", selectedCorps, PersonId(selected));
            }
        }
    }
    Fn<Method>(0x654370)(picker);
    Fn<Method>(0x603030)(self);
}

static void __fastcall Click(void* self, void*, void* control) {
    int id = Fn<int (__thiscall*)(void*, void*)>(0x4db7e0)(self, control);
    if (id == 0x13d8 || id == 0x13d7) {
        PickLeader(self);
        return;
    }
    if (self == dialog && id == 0x13e3 &&
        Eligible(selected, selectedCorps, static_cast<Byte*>(self) + 0x19c)) {
        pendingCorps = selectedCorps;
        pendingPerson = PersonId(selected);
        Log("confirmed", pendingCorps, pendingPerson);
    }
    if (self == dialog && (id == 0x13e3 || id == 0x13e4)) {
        dialog = nullptr;
        selected = nullptr;
    }
    Fn<void (__thiscall*)(void*, void*)>(0x604670)(self, control);
}

static void SetStatus(void* person, int status) {
    Fn<void (__thiscall*)(void*, int)>(0x4898f0)(person, status);
}
static int __cdecl Execute(void* command) {
    int corpsId = pendingCorps, personId = pendingPerson;
    pendingCorps = pendingPerson = -1;
    int result = Fn<int (__cdecl*)(void*)>(0x5c1150)(command);
    if (!result || corpsId < 0 || personId < 0) return result;
    void* corps = Ptr(command, 4);
    void* person = Person(personId);
    if (!Valid(corps) || Int(corps, 8) <= 1 ||
        Fn<int (__thiscall*)(void*, void*)>(0x4912c0)(World, corps) != corpsId ||
        !Eligible(person, corpsId, static_cast<Byte*>(command) + 8)) {
        Log("appointment rejected after regroup", corpsId, personId);
        return result;
    }
    void* old = Person(Int(corps, 0xc));
    if (Valid(old) && old != person && Int(old, 0xa0) == 1) {
        int status = 3;
        void* home = Fn<Get>(0x490d00)(World, Int(old, 0x98));
        if (Valid(home) && Fn<int (__thiscall*)(void*)>(0x486890)(home) == PersonId(old)) status = 2;
        SetStatus(old, status);
    }
    Fn<void (__thiscall*)(void*, int)>(0x47e110)(corps, personId);
    SetStatus(person, 1);
    Fn<Method>(0x47eb70)(corps);
    Log("appointed", corpsId, personId);
    return result;
}

static bool Replace(uintptr_t address, const void* expected, const void* replacement, size_t size) {
    void* target = reinterpret_cast<void*>(address);
    if (memcmp(target, expected, size)) return false;
    DWORD protection;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &protection)) return false;
    memcpy(target, replacement, size);
    VirtualProtect(target, size, protection, &protection);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    return true;
}
static bool ReplaceCall(uintptr_t site, uintptr_t original, void* replacement) {
    Byte before[5] = {0xe8}, after[5] = {0xe8};
    *reinterpret_cast<int*>(before + 1) = static_cast<int>(original - site - 5);
    *reinterpret_cast<int*>(after + 1) = static_cast<int>(reinterpret_cast<uintptr_t>(replacement) - site - 5);
    return Replace(site, before, after, sizeof before);
}
static bool MatchesCall(uintptr_t site, uintptr_t original) {
    return *reinterpret_cast<Byte*>(site) == 0xe8 &&
        *reinterpret_cast<int*>(site + 1) == static_cast<int>(original - site - 5);
}

extern "C" __declspec(dllexport) void __stdcall WorldmodCorpsInit(HWND) {
    static bool initialized;
    if (initialized) return;
    initialized = true;
    WideCharToMultiByte(950, 0, L"都督：統政魅", -1, title, sizeof title, nullptr, nullptr);
    WideCharToMultiByte(950, 0, L"五維", -1, abilityTab, sizeof abilityTab, nullptr, nullptr);
    WideCharToMultiByte(950, 0, L"綜合值", -1, compositeCaption, sizeof compositeCaption, nullptr, nullptr);
    WideCharToMultiByte(950, 0, L"行動力加成：統率×60%＋政治×40%＋魅力×40%，再除以5取整，最低6。", -1,
        abilityHint, sizeof abilityHint, nullptr, nullptr);
    // Validate every site before making any change, including the expanded getter.
    uintptr_t click = 0x604670;
    if (*reinterpret_cast<uintptr_t*>(0x84ea30 + 0x11c) != click ||
        *reinterpret_cast<Byte*>(0x490b08) != 0x3d ||
        *reinterpret_cast<short*>(GovernorResource + 6) != 6 ||
        !MatchesCall(0x5c18d8, 0x6038d0) ||
        !MatchesCall(0x6030c0, 0x5c0b10) ||
        !MatchesCall(0x5b98b6, 0x5c1150) ||
        !MatchesCall(0x6544b3, 0x4dcc30)) {
        Log("unsupported executable");
        return;
    }
    uintptr_t replacement = reinterpret_cast<uintptr_t>(&Click);
    memcpy(governorListVtable, reinterpret_cast<void*>(0x833c30), sizeof governorListVtable);
    governorListVtable[0x1e0 / 4] = reinterpret_cast<uintptr_t>(&GovernorCell);
    governorListVtable[0x1fc / 4] = reinterpret_cast<uintptr_t>(&GovernorSortKey);
    bool ok = ReplaceCall(0x5c18d8, 0x6038d0, reinterpret_cast<void*>(&InitDialog));
    ok = ReplaceCall(0x6030c0, 0x5c0b10, reinterpret_cast<void*>(&ChooseLeader)) && ok;
    ok = ReplaceCall(0x5b98b6, 0x5c1150, reinterpret_cast<void*>(&Execute)) && ok;
    ok = ReplaceCall(0x6544b3, 0x4dcc30, reinterpret_cast<void*>(&CreateGovernorList)) && ok;
    ok = Replace(0x84ea30 + 0x11c, &click, &replacement, sizeof click) && ok;
    Log(ok ? "native corps menu ready" : "native hook mismatch");
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    return TRUE;
}
