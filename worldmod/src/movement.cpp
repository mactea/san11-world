// movement: per-weapon movement (兵器 移動力, record +0x57) and terrain costs (record +0x58, 32 bytes, terrain 0..21
// used), M12_MOVE.md.
//
// The 12 weapon records (0 剣 1 槍 2 戟 3 弩 4 軍馬 5 衝車 6 井闌 7 投石 8 木獣 9 走舸 10 楼船 11 闘艦; 0xa0 bytes each,
// accessor 0x490bc0 = world + 0x7d054 + w * 0xa0) come from the master data Scenario.s11 (kind 0x18), not from
// saves. On every map entry 0x57e910 copies their costs into the table 0x9549830 (12 x 32 bytes, loop 0x57ecb7..
// 0x57eceb, 22 terrains); the step cost 0x56dd00 reads that table (weapon of the unit on land 0x495480 / on water
// 0x495490, column = terrain, x4 when burning). A unit's movement (unit +0xcc = stats +0x20) is rebuilt by 0x496dd0
// (-> 0x496570: record +0x57 + tech / skill bonuses, transport +5) whenever the game recomputes the unit.
//
// Hook: 0x57ecb7 (mov edi, 0x9549830: the start of the fill loop) -> write the configured absolute values into
// the records, rebuild the stats of every live unit (old saves keep their stored movement otherwise), then let the
// game fill the table from the records. Absolute values: applying them again on the next map entry changes nothing.
// Everything that reads a record (the weapon property getter 0x4c37a0 behind the info screens, 0x496570) or the
// table (0x56dd00: range search 0x5a4540, A* 0x567520, AI estimates) sees the same numbers.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "movement.h"

static TraceLogFn g_log = nullptr;
static int g_move[12];
static int g_cost[12][32];
static uint32_t g_world = 0, g_table = 0;
static bool g_loggedStock = false;

typedef uint8_t* (__thiscall* WeaponFn)(uint32_t world, int w);     // 0x490bc0
typedef uint8_t* (__thiscall* UnitFn)(uint32_t world, int id);      // 0x490e70 (M9: U units, relocated array)
typedef int (__cdecl* ValidFn)(void* obj);                          // 0x47a630
typedef void (__thiscall* RecalcFn)(void* unit);                    // 0x496dd0
static const WeaponFn weaponRec = (WeaponFn)0x490bc0;
static const UnitFn unitRec = (UnitFn)0x490e70;
static const ValidFn objValid = (ValidFn)0x47a630;
static const RecalcFn unitRecalc = (RecalcFn)0x496dd0;

// Defaults (M12_MOVE.md): roads (10) and bridges (12) faster, ships faster; every other terrain keeps its number
// of hexes per turn at base movement (floor(movement / cost)).
static const char* const DEFAULTS[12] = {
    "20;10:3,12:3",                                     // 剣
    "20;10:3,12:3", "20;10:3,12:3", "20;10:3,12:3",     // 槍 戟 弩: 22 -> 20
    "30;0:6,2:6,5:11,10:3,12:3,13:6,19:11",             // 軍馬: 28 -> 30
    "20;1:6,13:6,16:5,17:5,18:5,19:11",                 // 衝車 井闌 投石 木獣: 18 -> 20 (大道 / 橋 stay 4)
    "20;1:6,13:6,16:5,17:5,18:5,19:11", "20;1:6,13:6,16:5,17:5,18:5,19:11", "20;1:6,13:6,16:5,17:5,18:5,19:11",
    "26;7:5,8:4",                                       // 走舸: 16 -> 26, 河 5, 海 4
    "26;7:5,8:3", "26;7:5,8:3",                         // 楼船 闘艦: 20 -> 26, 河 5, 海 3
};
const char* movementDefaultSpec(int w) { return w >= 0 && w < 12 ? DEFAULTS[w] : ""; }

// Movement limit: the range search 0x5a4540 keeps 52 budget buckets and the largest bonus is +17 (transport:
// tech 0x20 +3, +5, skill 7 +5, skill 5 +4), so the record value must stay <= 51 - 17.
static const int MOVE_MAX = 34;

bool movementParseSpec(const char* spec, int* move, int cost[32]) {
    *move = -1;
    for (int t = 0; t < 32; t++) cost[t] = -1;
    while (*spec == ' ') spec++;
    if (*spec == '-') spec++;
    else {
        char* end; long m = strtol(spec, &end, 10);
        if (end == spec || m < 1 || m > MOVE_MAX) return false;
        *move = (int)m; spec = end;
    }
    while (*spec == ' ') spec++;
    if (!*spec) return true;
    if (*spec++ != ';') return false;
    for (;;) {
        while (*spec == ' ') spec++;
        if (!*spec) return true;
        char* end; long t = strtol(spec, &end, 10);
        if (end == spec || *end != ':' || t < 0 || t > 21) return false;
        spec = end + 1;
        long c = strtol(spec, &end, 10);
        if (end == spec || c < 1 || c > 255) return false;
        cost[t] = (int)c; spec = end;
        while (*spec == ' ') spec++;
        if (*spec == ',') spec++;
        else if (*spec) return false;
    }
}

int movementApplyRecords(uint8_t* records, const int move[12], const int cost[12][32]) {
    int changed = 0;
    for (int w = 0; w < 12; w++) {
        uint8_t* r = records + w * 0xa0;
        if (move[w] >= 0 && r[0x57] != move[w]) { r[0x57] = (uint8_t)move[w]; changed++; }
        for (int t = 0; t < 32; t++)
            if (cost[w][t] >= 0 && r[0x58 + t] != cost[w][t]) { r[0x58 + t] = (uint8_t)cost[w][t]; changed++; }
    }
    return changed;
}

static void logRecords(const char* what, uint8_t* const* rec) {
    for (int w = 0; w < 12; w++) {
        char buf[160]; int k = sprintf_s(buf, "%s w%-2d move %2d costs", what, w, rec[w][0x57]);
        for (int t = 0; t < 22; t++) k += sprintf_s(buf + k, sizeof buf - k, " %d", rec[w][0x58 + t]);
        g_log("%s", buf);
    }
}

static int recalcUnits(int* faults) {
    int U = *(const int*)(uintptr_t)0x490e79 + 1;     // cmp eax, U-1 (M9 patch table; 1000 in Koei's exe)
    int n = 0;
    for (int i = 0; i < U; i++) {
        __try {
            uint8_t* u = unitRec(g_world, i);
            if (u && objValid(u)) { unitRecalc(u); n++; }
        } __except (EXCEPTION_EXECUTE_HANDLER) { (*faults)++; }
    }
    return n;
}

static void __stdcall onMapEntry() {
    uint8_t* rec[12];
    for (int w = 0; w < 12; w++) rec[w] = weaponRec(g_world, w);
    for (int w = 1; w < 12; w++)
        if (rec[w] != rec[0] + w * 0xa0) { g_log("movement: weapon records not contiguous, not applied"); return; }
    if (!g_loggedStock) { g_loggedStock = true; logRecords("movement: before", rec); }
    int changed = movementApplyRecords(rec[0], g_move, g_cost);
    int faults = 0;
    int units = recalcUnits(&faults);
    g_log("movement: map entry: %d record bytes changed, stats rebuilt for %d units%s", changed, units,
          faults ? " (some faulted, see below)" : "");
    if (faults) g_log("movement: ERROR: %d units faulted in 0x496dd0", faults);
    if (changed) logRecords("movement: after ", rec);
}

static uint32_t g_resume = 0x57ecbc;
__declspec(naked) static void entryThunk() {
    __asm {
        pushad
        pushfd
        call onMapEntry
        popfd
        popad
        mov edi, g_table                  // the replaced instruction: mov edi, 0x9549830
        jmp g_resume
    }
}

bool movementInstall(const wchar_t* dir, const wchar_t* section, bool write, TraceLogFn log) {
    g_log = log;
    wchar_t ini[MAX_PATH]; swprintf_s(ini, L"%s\\worldmod.ini", dir);
    if (!GetPrivateProfileIntW(section, L"move", 0, ini)) { log("movement: move=0, Koei's values"); return false; }
    int bad = 0;
    for (int w = 0; w < 12; w++) {
        wchar_t key[16], v[256]; char s[256];
        swprintf_s(key, L"move_w%d", w);
        GetPrivateProfileStringW(section, key, L"", v, 256, ini);
        if (v[0]) WideCharToMultiByte(CP_ACP, 0, v, -1, s, sizeof s, nullptr, nullptr);
        else strcpy_s(s, DEFAULTS[w]);
        if (!movementParseSpec(s, &g_move[w], g_cost[w])) {
            log("movement: ERROR: move_w%d=\"%s\" is not \"<movement 1..%d or ->[;<terrain 0..21>:<cost 1..255>,...]\"", w, s, MOVE_MAX);
            bad++;
        } else if (v[0]) log("movement: move_w%d=%s (ini)", w, s);
    }
    if (bad) { log("movement: not installed"); return false; }
    // code the hook relies on
    static const uint8_t fill[] = { 0xBF, 0x30, 0x98, 0x54, 0x09, 0x8D, 0x64, 0x24, 0x00, 0x53, 0xB9 };   // 0x57ecb7
    static const uint8_t wacc[] = { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7C, 0x15, 0x83, 0xF8, 0x0B };   // 0x490bc0
    static const uint8_t uacc[] = { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7C, 0x17, 0x3D };              // 0x490e70
    static const uint8_t recalc[] = { 0x83, 0xEC, 0x14, 0x56, 0x8B, 0xF1, 0x33, 0xC9 };                  // 0x496dd0
    static const uint8_t cost[] = { 0x8A, 0x84, 0x30 };                                                  // 0x56dd4d
    if (memcmp((void*)0x57ecb7, fill, sizeof fill) || memcmp((void*)0x490bc0, wacc, sizeof wacc) ||
        memcmp((void*)0x490e70, uacc, sizeof uacc) || memcmp((void*)0x496dd0, recalc, sizeof recalc) ||
        memcmp((void*)0x56dd4d, cost, sizeof cost) || *(uint32_t*)(uintptr_t)0x56dd50 != 0x9549830) {
        log("movement: ERROR: unexpected code at 0x57ecb7 / 0x490bc0 / 0x490e70 / 0x496dd0 / 0x56dd4d, not installed");
        return false;
    }
    g_table = *(uint32_t*)(uintptr_t)0x57ecb8;
    g_world = *(uint32_t*)(uintptr_t)0x57ecc2;        // mov ecx, world (0x7201958)
    int U = *(const int*)(uintptr_t)0x490e79 + 1;
    if (!write) { log("movement: verify_only: code checked (world %08x, table %08x, %d units), nothing written", g_world, g_table, U); return false; }
    DWORD old;
    uint8_t* site = (uint8_t*)(uintptr_t)0x57ecb7;
    VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    site[0] = 0xE9; *(int32_t*)(site + 1) = (int32_t)((uintptr_t)entryThunk - (uintptr_t)(site + 5));
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    char list[200]; int k = 0;
    for (int w = 0; w < 12; w++) k += sprintf_s(list + k, sizeof list - k, " %d", g_move[w]);
    log("movement: map entry hook at 0x57ecb7 installed (world %08x, table %08x, %d units); movement%s", g_world, g_table, U, list);
    return true;
}
