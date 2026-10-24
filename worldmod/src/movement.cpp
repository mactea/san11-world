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
//
// Ships (M12_MOVE.md "船和操舵"):
// - 操舵 (skill 5 of any of the unit's three members) no longer adds +4 movement on water (0x496b81 add 4 -> add 0);
//   instead the step cost on 河 / 海 (0x56dd42) comes from a second table (g_steer: the records' costs with the
//   move_steer_w9..w11 values on top) when the unit has 操舵. A* (0x567520 / pathfind.cpp) and every AI estimate go
//   through 0x56dd00 too.
// - A transport (unit +8 == 1) on water uses the 走舸 record; its base movement there is move_transport_water instead of
//   走舸's (0x496a76), so 走舸's own movement can be large without pushing transports past the 52 buckets.
// - Landing / boarding (move_shore_scale): a step onto land for a unit afloat, or onto water for a unit ashore, costs
//   ceil(cost * its movement / the other weapon's movement) (0x56dd42; the other movement from 0x496dd0's entry hook).
// - The range search 0x5a47e0 -> 0x5a4540 stops at depth 10; ships (current weapon unit +0xa8 >= 9) get
//   move_cap_ship. Its node pool (obj +0xd0, 396 nodes) and visited list (obj +0x2720, 397 hexes) are sized for
//   radius 10 / 11, so both move into DLL buffers (RANGE_CAP hexes).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "movement.h"

static TraceLogFn g_log = nullptr;
static int g_move[12];
static int g_cost[12][32];
static int g_steerCost[12][32];                    // move_steer_w*: -1 = same as the normal cost
static uint8_t g_steer[12 * 32];                   // 操舵 step costs, laid out like 0x9549830
static bool g_steerReady = false;
static uint32_t g_world = 0, g_table = 0;
static bool g_loggedStock = false;

typedef uint8_t* (__thiscall* WeaponFn)(uint32_t world, int w);     // 0x490bc0
typedef uint8_t* (__thiscall* UnitFn)(uint32_t world, int id);      // 0x490e70 (M9: U units, relocated array)
typedef uint8_t* (__thiscall* PersonFn)(uint32_t world, int id);    // 0x490b00 (M10: relocated, 0 when out of range)
typedef int (__cdecl* ValidFn)(void* obj);                          // 0x47a630
typedef void (__thiscall* RecalcFn)(void* unit);                    // 0x496dd0
typedef uint8_t* (__thiscall* UnitOfFn)(void* handle);              // 0x56dba0 (handle +8 = unit id)
static const WeaponFn weaponRec = (WeaponFn)0x490bc0;
static const UnitFn unitRec = (UnitFn)0x490e70;
static const PersonFn personRec = (PersonFn)0x490b00;
static const ValidFn objValid = (ValidFn)0x47a630;
static const RecalcFn unitRecalc = (RecalcFn)0x496dd0;
static const UnitOfFn unitOf = (UnitOfFn)0x56dba0;

// Defaults (M12_MOVE.md): roads (10) and bridges (12) faster, ships faster; every other terrain keeps its number
// of hexes per turn at base movement (floor(movement / cost)).
static const char* const DEFAULTS[12] = {
    "20;10:3,12:3",                                     // 剣
    "20;10:3,12:3", "20;10:3,12:3", "20;10:3,12:3",     // 槍 戟 弩: 22 -> 20
    "30;0:6,2:6,5:11,10:3,12:3,13:6,19:11",             // 軍馬: 28 -> 30
    "20;1:6,13:6,16:5,17:5,18:5,19:11",                 // 衝車 井闌 投石 木獣: 18 -> 20 (大道 / 橋 stay 4)
    "20;1:6,13:6,16:5,17:5,18:5,19:11", "20;1:6,13:6,16:5,17:5,18:5,19:11", "20;1:6,13:6,16:5,17:5,18:5,19:11",
    "42;7:6,8:5",                                       // 走舸: 16 -> 42, 河 7 海 8 hexes
    "50;7:8,8:4", "50;7:8,8:4",                         // 楼船 闘艦: 20 -> 50, 河 6 海 12 hexes
};
// 操舵 costs of the ships (w9..w11): 走舸 河 8 海 10, 楼船 / 闘艦 河 7 海 16 hexes
static const char* const STEER_DEFAULTS[3] = { "-;7:5,8:4", "-;7:7,8:3", "-;7:7,8:3" };
const char* movementDefaultSpec(int w) { return w >= 0 && w < 12 ? DEFAULTS[w] : ""; }

// Movement limits: the range search 0x5a4540 keeps 52 budget buckets (index = movement left), so movement plus every
// bonus must stay <= 51. Land weapons get up to +17 (transport: tech 0x20 +3, +5, skill 7 +5, skill 5 +4), ships get
// nothing with 操舵 off the movement, a transport on water up to +13 on move_transport_water.
static const int MOVE_MAX = 34, SHIP_MOVE_MAX = 51, TRANSPORT_MAX = 38;
static int g_transportWater = 31;                   // M12: 走舸 26 + 5 = 31 total base (river 6, sea 7 hexes)

// Range search: depth cap (Koei: 10) and the relocated node pool / visited list.
static const int LAND_CAP = 10, CAP_MAX = 25;      // radius 25: 1 + 3 * 25 * 26 = 1951 hexes
static const int RANGE_CAP = 2048;
static int g_capShip = 16;
static int g_depthCap = LAND_CAP;
static uint8_t g_pool[RANGE_CAP * 0x14];            // nodes: +0 coord, +0x10 next in the bucket
static int g_poolCount = 0;
static uint32_t g_visited[RANGE_CAP];               // hexes whose flag 0x40000 the search set
static int g_visitedCount = 0;
static uint8_t* g_rec9 = nullptr;                   // 走舸's record

bool movementParseSpec(const char* spec, int* move, int cost[32], int moveMax) {
    *move = -1;
    for (int t = 0; t < 32; t++) cost[t] = -1;
    while (*spec == ' ') spec++;
    if (*spec == '-') spec++;
    else {
        char* end; long m = strtol(spec, &end, 10);
        if (end == spec || m < 1 || m > moveMax) return false;
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

const uint8_t* movementSteerCosts() { return g_steerReady ? g_steer : nullptr; }

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
    // the 操舵 table: what the game is about to copy into 0x9549830, with the 操舵 costs on top
    for (int w = 0; w < 12; w++)
        for (int t = 0; t < 32; t++)
            g_steer[w * 32 + t] = g_steerCost[w][t] >= 0 ? (uint8_t)g_steerCost[w][t] : (t < 22 ? rec[w][0x58 + t] : 0);
    g_steerReady = true;
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

// ------------------------------------------------------------------ 操舵: step cost (0x56dd00)
// 0x495bd0 (the bonus check in 0x496570) asks 0x4890f0: person +0xe8 == skill, for the members 0x496dd0 collects
// (unit +0xc, +0x10, +0x14; person ids, 0x490b00 returns 0 out of range).
static const int SKILL_STEER = 5;
static int __cdecl unitHasSteer(const uint8_t* unit) {
    static const int member[3] = { 0xc, 0x10, 0x14 };
    for (int k = 0; k < 3; k++) {
        int id = *(const int*)(unit + member[k]);
        if (id < 0) continue;
        const uint8_t* p = personRec(g_world, id);
        if (p && *(const int*)(p + 0xe8) == SKILL_STEER) return 1;
    }
    return 0;
}

// ------------------------------------------------------------------ shore crossing: movement of the other weapon
// A unit's movement (unit +0xcc, 0x56dce0) is the budget of its whole move this turn (nothing deducts from it; the
// range search 0x5a47e0 and the AI get it as the start value), computed by 0x496dd0 for its current weapon
// (unit +0xa8: the ship on water, else the land weapon). With move_shore_scale, a step onto the other kind of hex
// (land for a unit afloat, water for a unit ashore) costs ceil(cost * movement / movement of the other weapon), so
// the hexes left after landing / boarding are the matching fraction of what the other weapon gets in a whole turn.
// The other weapon's movement (with every bonus) is computed by the game's own 0x496570 right after each 0x496dd0
// (hook at its entry) and kept per unit with the kind (afloat or not) the unit's movement was computed for.
typedef void (__cdecl* StatsFn)(uint8_t* stats, uint8_t** persons, int weapon, int a4, int transport, int a6, int water, int full);
static const StatsFn unitStats = (StatsFn)0x496570;
static int g_shoreScale = 1;
static uint8_t* g_unit0 = nullptr;                 // unit 0 (0x490e70; M9 may relocate the array)
static int g_units = 0;
static uint8_t* g_alt = nullptr;                    // per unit: movement of the other weapon, 0 = unknown
static uint8_t* g_afloat = nullptr;                 // per unit: 1 afloat, 0 ashore, 0xff unknown

static inline int unitIndex(const uint8_t* unit) {
    if (unit < g_unit0) return -1;
    size_t d = (size_t)(unit - g_unit0);
    if (d % 0xf4) return -1;
    size_t i = d / 0xf4;
    return i < (size_t)g_units ? (int)i : -1;
}

static void __cdecl afterRecalc(uint8_t* unit) {
    int i = unitIndex(unit);
    if (i < 0) return;
    g_afloat[i] = 0xff; g_alt[i] = 0;
    if (!unit[0xcc]) return;                        // not alive: 0x496dd0 left the stats zero
    int w = *(const int*)(unit + 0xa8);
    if (w < 0 || w > 11) return;
    int afloat = w >= 9, transport = *(const int*)(unit + 8);
    int alt = afloat ? (transport == 1 ? 0 : *(const int*)(unit + 0x48)) : (transport == 1 ? 9 : *(const int*)(unit + 0x50));
    g_afloat[i] = (uint8_t)afloat;
    if (alt < 0 || alt > 11) return;
    // the members as 0x496dd0 collects them (0x490b00: 0 out of range)
    static const int member[3] = { 0xc, 0x10, 0x14 };
    uint8_t* ps[3];
    for (int k = 0; k < 3; k++) {
        int id = *(const int*)(unit + member[k]);
        ps[k] = id < 0 ? nullptr : personRec(g_world, id);
    }
    uint8_t stats[0x24];
    __try {
        unitStats(stats, ps, alt, *(const uint16_t*)(unit + 0x18), transport, *(const int*)(unit + 0x24), !afloat, 1);
        g_alt[i] = stats[0x20];
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// 0x496dd0 entry: sub esp, 0x14; push esi; mov esi, ecx (6 bytes) -> recalcThunk: the original (copied prologue in
// g_recalcTramp), then afterRecalc. 0x496dd0 takes no stack arguments (plain ret); 0x496f6b / 0x496f7d jump to it.
static uint8_t* g_recalcTramp = nullptr;
__declspec(naked) static void recalcThunk() {
    __asm {
        push ecx
        call g_recalcTramp
        pop ecx
        push ecx
        call afterRecalc
        add esp, 4
        ret
    }
}

// ------------------------------------------------------------------ step cost (0x56dd00)
// The table 0x9549830, or g_steer on 河 / 海 for a unit with 操舵; x4 when the hex burns (as Koei: a byte shift);
// then the shore scaling above. Impassable (255) stays 255. The callers (0x5a4598, 0x567731, 0x5a5ed8, pathfind.cpp)
// use only al.
static int __cdecl stepCost(const uint8_t* unit, int weapon, int terr, uint32_t hexFlags) {
    bool water = terr == 7 || terr == 8;
    const uint8_t* tab = water && g_steerReady && unitHasSteer(unit) ? g_steer : (const uint8_t*)(uintptr_t)g_table;
    uint8_t c = tab[weapon * 32 + terr];
    int cost = hexFlags & 0x3800000 ? (uint8_t)(c << 2) : c;
    if (g_shoreScale && c != 0xff) {
        int i = unitIndex(unit);
        if (i >= 0 && g_afloat[i] != 0xff && (g_afloat[i] != 0) != water) {
            int cur = unit[0xcc], alt = g_alt[i];
            if (cur && alt) { cost = (cost * cur + alt - 1) / alt; if (cost > 255) cost = 255; }
        }
    }
    return cost;
}

// 0x56dd42: mov ecx, [edi]; shl eax, 5 (5 bytes) -> costThunk, which finishes the function (pop edi; pop esi;
// ret 4). Here ecx = the unit (0x495480 / 0x495490 keep it), eax = weapon, esi = terrain, edi = the hex record.
__declspec(naked) static void costThunk() {
    __asm {
        push edx
        push dword ptr [edi]
        push esi
        push eax
        push ecx
        call stepCost
        add esp, 16
        pop edx
        pop edi
        pop esi
        ret 4
    }
}

// ------------------------------------------------------------------ transport on water (0x496a76)
// 0x496a76: mov cl, [esi+0x57]; mov eax, [esp+0x64] (7 bytes): esi = the weapon record, [esp+0x64] = arg 5
// (unit +8, 1 = transport). A transport uses 走舸's record on water (0x495490); its base movement there is
// move_transport_water.
static uint32_t g_transportBack = 0x496a7d;
__declspec(naked) static void transportThunk() {
    __asm {
        mov cl, [esi + 0x57]
        mov eax, [esp + 0x64]
        cmp eax, 1
        jne done
        cmp esi, g_rec9
        jne done
        mov cl, byte ptr g_transportWater
    done:
        jmp g_transportBack
    }
}

// ------------------------------------------------------------------ range search depth cap (0x5a47e0, 0x5a4543)
static void __cdecl setDepthCap(void* handle) {
    const uint8_t* u = unitOf(handle);
    int w = *(const int*)(u + 0xa8);                  // current weapon (0x496160: ship weapon on water)
    g_depthCap = w >= 9 && w <= 11 ? g_capShip : LAND_CAP;
}

// 0x5a47e0: sub esp, 0x10; push ebx; mov ebx, [esp+0x18] (8 bytes); [esp+4] = the unit handle
static uint32_t g_searchBack = 0x5a47e8;
__declspec(naked) static void searchThunk() {
    __asm {
        push eax
        push ecx
        push edx
        push dword ptr [esp + 0x10]
        call setDepthCap
        add esp, 4
        pop edx
        pop ecx
        pop eax
        sub esp, 0x10
        push ebx
        mov ebx, [esp + 0x18]
        jmp g_searchBack
    }
}

// 0x5a4543: cmp dword ptr [esp+0x20], 0xa (5 bytes; the jge follows at 0x5a454f, push / mov in between keep flags)
static uint32_t g_depthBack = 0x5a4548;
__declspec(naked) static void depthThunk() {
    __asm {
        push eax
        mov eax, g_depthCap
        cmp [esp + 0x24], eax
        pop eax
        jmp g_depthBack
    }
}

// ------------------------------------------------------------------ install
struct Site { uint32_t at; uint8_t len; uint8_t old[25]; };

static bool same(const Site& s) { return !memcmp((void*)(uintptr_t)s.at, s.old, s.len); }

static void writeCode(uint32_t at, const uint8_t* bytes, int n) {
    DWORD old;
    VirtualProtect((void*)(uintptr_t)at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void*)(uintptr_t)at, bytes, n);
    VirtualProtect((void*)(uintptr_t)at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)(uintptr_t)at, n);
}

static void writeJmp(uint32_t at, int len, void* to) {
    uint8_t b[16];
    b[0] = 0xE9; *(int32_t*)(b + 1) = (int32_t)((uintptr_t)to - (at + 5));
    for (int i = 5; i < len; i++) b[i] = 0x90;
    writeCode(at, b, len);
}

// opcode bytes + an absolute address (+ a 32-bit immediate / nop padding) replacing an obj-relative access
static void writeAbs(uint32_t at, int len, const uint8_t* op, int oplen, const void* addr, const uint8_t* tail = nullptr, int tailLen = 0) {
    uint8_t b[16]; int k = 0;
    memcpy(b, op, oplen); k = oplen;
    *(uint32_t*)(b + k) = (uint32_t)(uintptr_t)addr; k += 4;
    if (tail) { memcpy(b + k, tail, tailLen); k += tailLen; }
    while (k < len) b[k++] = 0x90;
    writeCode(at, b, len);
}

// code the hooks rely on (Koei's bytes)
static const Site SITES[] = {
    { 0x57ecb7, 10, { 0xBF, 0x30, 0x98, 0x54, 0x09, 0x8D, 0x64, 0x24, 0x00, 0x53 } },            // fill loop (+0xB9 checked below)
    { 0x490bc0, 10, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7C, 0x15, 0x83, 0xF8 } },            // weapon record
    { 0x490e70,  9, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7C, 0x17, 0x3D } },                  // unit
    { 0x490b00,  9, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7C, 0x17, 0x3D } },                  // person
    { 0x496dd0,  8, { 0x83, 0xEC, 0x14, 0x56, 0x8B, 0xF1, 0x33, 0xC9 } },                        // unit stats
    { 0x56dba0,  4, { 0x0F, 0xB7, 0x41, 0x08 } },                                                // unit of a handle
    { 0x4890f0, 10, { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7C, 0x19, 0x83, 0xF8 } },            // has skill: person +0xe8
    { 0x56dd42,  5, { 0x8B, 0x0F, 0xC1, 0xE0, 0x05 } },                                          // step cost
    { 0x56dd47,  9, { 0xF7, 0xC1, 0x00, 0x00, 0x80, 0x03, 0x8A, 0x84, 0x30 } },                  // (table at 0x56dd50)
    { 0x56dd54,  4, { 0x5F, 0x5E, 0x76, 0x03 } },
    { 0x496ee2, 25, { 0x6A, 0x01, 0x57, 0x50, 0x51, 0x8B, 0xCE, 0xFF, 0x52, 0x50, 0x50, 0x53, 0x8D, 0x44, 0x24, 0x2C,
                      0x50, 0x8D, 0x86, 0xAC, 0x00, 0x00, 0x00, 0x50, 0xE8 } },                // 0x496dd0's call of 0x496570
    { 0x496010,  5, { 0x0F, 0xB7, 0x41, 0x18, 0xC3 } },                                          // unit vfunc +0x50: word +0x18
    { 0x496a76,  7, { 0x8A, 0x4E, 0x57, 0x8B, 0x44, 0x24, 0x64 } },                              // transport base
    { 0x496b6f,  9, { 0x8B, 0x5C, 0x24, 0x54, 0x6A, 0x05, 0xE8, 0x56, 0xF0 } },                  // 操舵 check
    { 0x496b81,  4, { 0x80, 0x45, 0x20, 0x04 } },                                                // 操舵 +4
    { 0x5a47e0,  8, { 0x83, 0xEC, 0x10, 0x53, 0x8B, 0x5C, 0x24, 0x18 } },                        // range search
    { 0x5a4543,  5, { 0x83, 0x7C, 0x24, 0x20, 0x0A } },                                          // depth 10
    { 0x5a44f9,  6, { 0x8B, 0x81, 0xD4, 0x1F, 0x00, 0x00 } },                                    // pool count
    { 0x5a4503,  6, { 0x89, 0x81, 0xD4, 0x1F, 0x00, 0x00 } },
    { 0x5a450d,  7, { 0x8D, 0x94, 0x91, 0xD0, 0x00, 0x00, 0x00 } },                              // pool
    { 0x5a45c0,  6, { 0x8B, 0x8F, 0x54, 0x2D, 0x00, 0x00 } },                                    // visited
    { 0x5a45c6,  7, { 0x89, 0x9C, 0x8F, 0x20, 0x27, 0x00, 0x00 } },
    { 0x5a45cd,  6, { 0xFF, 0x87, 0x54, 0x2D, 0x00, 0x00 } },
    { 0x5a4846,  6, { 0x89, 0x8E, 0x20, 0x27, 0x00, 0x00 } },
    { 0x5a484c, 10, { 0xC7, 0x86, 0x54, 0x2D, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 } },
    { 0x5a4914,  6, { 0x8B, 0x86, 0x54, 0x2D, 0x00, 0x00 } },
    { 0x5a4920,  6, { 0x8D, 0x96, 0x20, 0x27, 0x00, 0x00 } },
    { 0x5a4957,  6, { 0x8B, 0x86, 0x54, 0x2D, 0x00, 0x00 } },
    { 0x5a497a,  6, { 0x89, 0x86, 0xD4, 0x1F, 0x00, 0x00 } },
};

static bool readSpec(const wchar_t* ini, const wchar_t* section, const wchar_t* key, const char* def, char* s, size_t n) {
    wchar_t v[256];
    GetPrivateProfileStringW(section, key, L"", v, 256, ini);
    if (v[0]) WideCharToMultiByte(CP_ACP, 0, v, -1, s, (int)n, nullptr, nullptr);
    else strcpy_s(s, n, def);
    return v[0] != 0;
}

bool movementInstall(const wchar_t* dir, const wchar_t* section, bool write, TraceLogFn log) {
    g_log = log;
    wchar_t ini[MAX_PATH]; swprintf_s(ini, L"%s\\worldmod.ini", dir);
    if (!GetPrivateProfileIntW(section, L"move", 0, ini)) { log("movement: move=0, Koei's values"); return false; }
    int bad = 0;
    for (int w = 0; w < 12; w++) {
        wchar_t key[24]; char s[256];
        swprintf_s(key, L"move_w%d", w);
        bool fromIni = readSpec(ini, section, key, DEFAULTS[w], s, sizeof s);
        int mmax = w >= 9 ? SHIP_MOVE_MAX : MOVE_MAX;
        if (!movementParseSpec(s, &g_move[w], g_cost[w], mmax)) {
            log("movement: ERROR: move_w%d=\"%s\" is not \"<movement 1..%d or ->[;<terrain 0..21>:<cost 1..255>,...]\"", w, s, mmax);
            bad++;
        } else if (fromIni) log("movement: move_w%d=%s (ini)", w, s);
        int steerMove = -1;
        for (int t = 0; t < 32; t++) g_steerCost[w][t] = -1;
        if (w < 9) continue;
        swprintf_s(key, L"move_steer_w%d", w);
        fromIni = readSpec(ini, section, key, STEER_DEFAULTS[w - 9], s, sizeof s);
        if (!movementParseSpec(s, &steerMove, g_steerCost[w], 0) || steerMove >= 0) {
            log("movement: ERROR: move_steer_w%d=\"%s\" is not \"-;<terrain 0..21>:<cost 1..255>,...\"", w, s);
            bad++;
        } else if (fromIni) log("movement: move_steer_w%d=%s (ini)", w, s);
    }
    g_transportWater = GetPrivateProfileIntW(section, L"move_transport_water", 31, ini);
    if (g_transportWater < 1 || g_transportWater > TRANSPORT_MAX) {
        log("movement: ERROR: move_transport_water=%d is not 1..%d", g_transportWater, TRANSPORT_MAX); bad++;
    }
    g_shoreScale = GetPrivateProfileIntW(section, L"move_shore_scale", 1, ini) != 0;
    g_capShip = GetPrivateProfileIntW(section, L"move_cap_ship", 16, ini);
    if (g_capShip < LAND_CAP || g_capShip > CAP_MAX) {
        log("movement: ERROR: move_cap_ship=%d is not %d..%d", g_capShip, LAND_CAP, CAP_MAX); bad++;
    }
    if (bad) { log("movement: not installed"); return false; }
    for (const Site& s : SITES)
        if (!same(s)) { log("movement: ERROR: unexpected code at %08x, not installed", s.at); return false; }
    if (*(uint8_t*)(uintptr_t)0x57ecc1 != 0xB9 || *(uint32_t*)(uintptr_t)0x56dd50 != 0x9549830) {
        log("movement: ERROR: unexpected code at 0x57ecc1 / 0x56dd50, not installed");
        return false;
    }
    g_table = *(uint32_t*)(uintptr_t)0x57ecb8;
    g_world = *(uint32_t*)(uintptr_t)0x57ecc2;        // mov ecx, world (0x7201958)
    g_rec9 = weaponRec(g_world, 9);
    int U = *(const int*)(uintptr_t)0x490e79 + 1;
    g_unit0 = unitRec(g_world, 0);
    g_units = U;
    if (!write) { log("movement: verify_only: code checked (world %08x, table %08x, %d units), nothing written", g_world, g_table, U); return false; }

    g_alt = (uint8_t*)VirtualAlloc(nullptr, 2 * (size_t)U, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_afloat = g_alt + U;
    memset(g_afloat, 0xff, U);
    writeJmp(0x57ecb7, 5, entryThunk);
    writeJmp(0x56dd42, 5, costThunk);
    // 0x496dd0: original prologue + jmp back, then the hook
    g_recalcTramp = (uint8_t*)VirtualAlloc(nullptr, 16, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    memcpy(g_recalcTramp, (void*)(uintptr_t)0x496dd0, 6);
    g_recalcTramp[6] = 0xE9; *(int32_t*)(g_recalcTramp + 7) = (int32_t)(0x496dd6 - (uintptr_t)(g_recalcTramp + 11));
    writeJmp(0x496dd0, 6, recalcThunk);
    writeJmp(0x496a76, 7, transportThunk);
    static const uint8_t add0 = 0x00;                 // 操舵 on water: add byte ptr [ebp+0x20], 0
    writeCode(0x496b84, &add0, 1);
    writeJmp(0x5a47e0, 8, searchThunk);
    writeJmp(0x5a4543, 5, depthThunk);
    // node pool and visited list -> DLL buffers
    static const uint8_t movEaxM[] = { 0xA1 }, movMEax[] = { 0xA3 }, leaEdxPool[] = { 0x8D, 0x14, 0x95 };
    static const uint8_t movEcxM[] = { 0x8B, 0x0D }, movVisEbx[] = { 0x89, 0x1C, 0x8D }, incM[] = { 0xFF, 0x05 };
    static const uint8_t movMEcx[] = { 0x89, 0x0D }, movMImm[] = { 0xC7, 0x05 }, one[] = { 1, 0, 0, 0 };
    static const uint8_t movEaxM2[] = { 0x8B, 0x05 }, movEdxImm[] = { 0xBA }, movMEax2[] = { 0x89, 0x05 };
    writeAbs(0x5a44f9, 6, movEaxM, 1, &g_poolCount);                 // mov eax, [count]
    writeAbs(0x5a4503, 6, movMEax, 1, &g_poolCount);                 // mov [count], eax
    writeAbs(0x5a450d, 7, leaEdxPool, 3, g_pool);                    // lea edx, [edx*4 + pool]  (edx = n * 5)
    writeAbs(0x5a45c0, 6, movEcxM, 2, &g_visitedCount);              // mov ecx, [visited count]
    writeAbs(0x5a45c6, 7, movVisEbx, 3, g_visited);                  // mov [ecx*4 + visited], ebx
    writeAbs(0x5a45cd, 6, incM, 2, &g_visitedCount);                 // inc dword ptr [visited count]
    writeAbs(0x5a4846, 6, movMEcx, 2, g_visited);                    // mov [visited], ecx (the start hex)
    writeAbs(0x5a484c, 10, movMImm, 2, &g_visitedCount, one, 4);     // mov dword ptr [visited count], 1
    writeAbs(0x5a4914, 6, movEaxM2, 2, &g_visitedCount);             // mov eax, [visited count]
    writeAbs(0x5a4920, 6, movEdxImm, 1, g_visited);                  // mov edx, visited
    writeAbs(0x5a4957, 6, movEaxM2, 2, &g_visitedCount);
    writeAbs(0x5a497a, 6, movMEax2, 2, &g_poolCount);                // mov [count], eax (0)
    char list[200]; int k = 0;
    for (int w = 0; w < 12; w++) k += sprintf_s(list + k, sizeof list - k, " %d", g_move[w]);
    log("movement: map entry hook at 0x57ecb7 installed (world %08x, table %08x, %d units); movement%s", g_world, g_table, U, list);
    log("movement: ships: 操舵 costs at 0x56dd42 (w9..w11 河/海 %d/%d %d/%d %d/%d), no +4 at 0x496b81; transport on water %d (0x496a76); "
        "shore scaling %d (0x496dd0); "
        "range search depth ships %d / land %d, pool and visited list %d hexes (pool %p, visited %p)",
        g_steerCost[9][7], g_steerCost[9][8], g_steerCost[10][7], g_steerCost[10][8], g_steerCost[11][7], g_steerCost[11][8],
        g_transportWater, g_shoreScale, g_capShip, LAND_CAP, RANGE_CAP, g_pool, g_visited);
    return true;
}
