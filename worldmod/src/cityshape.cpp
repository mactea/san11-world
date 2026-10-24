// cityshape: 3-hex and 1-hex cities (M11_MAP.md, "worldmod 实现").
//
// city_shapes.bin (data_dir, tools/gen_bases.py): "WCSH", u32 version 1, u32 C, C x {u8 shape, u8 tier, u8 variant,
// u8 flags}. shape 0 = Koei's 7 hexes, 1..6 = triangle (centre + hex_nbrs[ROT[k]] + hex_nbrs[ROT[k+1]], k = shape-1),
// 7 = 1 hex. A city's centre is its building position (building +0x1e), the hex its OBJS model stands on.
//
// 1. Footprint of 3D objects. 0x41bce0(out, category) gives the object grid sub-cells an object covers, as
//    {i16 drow, i16 dcol} offsets in "ou" units (2x2 per hex) from the object's position: 28 entries (7 hexes) for
//    categories 0..5 (city models), else 4 (1 hex). Its four callers are redirected: place 0x416690 (ebx = object),
//    remove 0x416820 (ebp = object), 0x417530 (edi = object) use the city the object was assigned when its matrix
//    was first built; the free-cell check 0x4169a0 (before the object exists) maps its position to the city.
//    place also writes the object id into HEX20 for every covered hex (0x4848f0): the city's hexes follow the list.
// 2. Model matrix. 0x41c2e0 builds obj+0x10..0x4f (rotation rows, translation +0x40/+0x48 = ou * 10). After it,
//    city models of shaped / smaller cities get their 3x3 part scaled (tier 2 / 3) and triangle cities move to the
//    vertex the three hexes share (the centroid of their centres). Everything that copies the matrix later
//    (0x41d0c6 for the ring models) copies the adjusted one.
// 3. Building radius. Koei's code treats a city as "the hexes within 1 of its centre": walks over the hexes at
//    distance 1..count from a building centre (0x4843a0; the centre itself is not visited) and rings at distance R
//    (0x483c80) use count / R one larger for cities (`type == 0 ? 2 : 1` and similar). For a 7-hex city F
//    (footprint), dist(centre) == 1 is "F minus the centre" and dist(centre) == k >= 2 is dist(F) == k-1. The
//    redirected call sites therefore give, for a shaped city at `pos`: walk(count) = (F minus the centre), then the
//    hexes with dist(F) = 1, ..., count-1; ring(R) = F minus the centre for R 1, else {hex : dist(F) == R-1} (with
//    the terrain filter 0x8a5df8 of Koei's ring). For a 7-hex city these are exactly Koei's sets, so every other
//    building and every Koei city goes to the original functions unchanged.
//
// A city is "at pos" when pos (lo | hi << 16) is a hex whose area belongs to city c (AREA20 -> AREA2BASE), building
// c is a city (+8 == 0) standing at pos (+0x1e) and its shape is 1..7. Objects are mapped by the area of their hex
// (as 0x57e910 links them to buildings on map entry; the SHEX areas are loaded before the OBJS objects, 0x5a20a7 /
// 0x5a2172), only for categories 0..5 standing exactly on a hex centre.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "cityshape.h"

namespace {

#pragma pack(push, 1)
struct Rec { uint8_t shape, tier, variant, flags; };
#pragma pack(pop)

TraceLogFn g_log = nullptr;
Rec* g_rec = nullptr;
int g_C = 0, g_N = 0, g_W = 0, g_H = 0;
const uint8_t* g_area20 = nullptr;
const int32_t* g_area2base = nullptr;
float g_scale[4] = { 1.0f, 1.0f, 0.75f, 0.5f };           // by tier (0 Koei, 1 large, 2 middle, 3 small)

const uintptr_t BLD = 0x728b088, BLD_SIZE = 0x38;          // building table (world + 0x89730)
const uintptr_t OBJ_BASE = 0x4648350, OBJ_SIZE = 0x80;     // object instances (S + 0x13e7af0), ids 0..0xfffe
const uintptr_t RING_TERRAIN = 0x8a5df8;                   // int per terrain type: hex may be in a ring (0x483c80)
const uint16_t NONE = 0xffff;
uint16_t g_objCity[0x10000];                               // city whose model the object in that slot is

// hex_nbrs order (tools/gen_bases.py): 0 W, 1 E, 2 up-left, 3 up-right, 4 down-left, 5 down-right
const int8_t NB[2][6][2] = { { { 0, -1 }, { 0, 1 }, { -1, -1 }, { -1, 0 }, { 1, -1 }, { 1, 0 } },
                             { { 0, -1 }, { 0, 1 }, { -1, 0 }, { -1, 1 }, { 1, 0 }, { 1, 1 } } };
const int ROT[6] = { 1, 5, 4, 0, 2, 3 };                   // clockwise from E
// centre of each neighbour in ou units (row, col) relative to the hex's own: ouc = 2 hi + 57 + (lo & 1)
const int8_t OU[6][2] = { { 0, -2 }, { 0, 2 }, { -2, -1 }, { -2, 1 }, { 2, -1 }, { 2, 1 } };

int16_t g_tri[6][12][2];                                   // footprint sub-cell lists, triangle k
int16_t g_one[4][2];                                       // 1 hex (same cells as Koei's 0x77a930)

void buildLists() {
    static const int8_t cell[4][2] = { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 } };   // a hex's sub-cells (0x77a930)
    for (int i = 0; i < 4; i++) { g_one[i][0] = cell[i][0]; g_one[i][1] = cell[i][1]; }
    for (int k = 0; k < 6; k++) {
        int hx[3][2] = { { 0, 0 }, { OU[ROT[k]][0], OU[ROT[k]][1] }, { OU[ROT[(k + 1) % 6]][0], OU[ROT[(k + 1) % 6]][1] } };
        for (int h = 0; h < 3; h++)
            for (int i = 0; i < 4; i++) {
                g_tri[k][h * 4 + i][0] = (int16_t)(hx[h][0] + cell[i][0]);
                g_tri[k][h * 4 + i][1] = (int16_t)(hx[h][1] + cell[i][1]);
            }
    }
}

// ---------------------------------------------------------------- hex geometry
inline void nbr(int lo, int hi, int dir, int& nlo, int& nhi) { nlo = lo + NB[lo & 1][dir][0]; nhi = hi + NB[lo & 1][dir][1]; }
inline int hexDist(int lo1, int hi1, int lo2, int hi2) {       // odd rows shifted half a hex towards +hi
    int x1 = hi1 - ((lo1 - (lo1 & 1)) >> 1), x2 = hi2 - ((lo2 - (lo2 & 1)) >> 1);
    int dx = x1 - x2, dz = lo1 - lo2, dy = -dx - dz;
    if (dx < 0) dx = -dx; if (dy < 0) dy = -dy; if (dz < 0) dz = -dz;
    return dx > dy ? (dx > dz ? dx : dz) : (dy > dz ? dy : dz);
}
struct Foot { int n; int lo[3], hi[3]; };
Foot footprint(int shape, int lo, int hi) {
    Foot f; f.n = 1; f.lo[0] = lo; f.hi[0] = hi;
    if (shape >= 1 && shape <= 6) {
        nbr(lo, hi, ROT[shape - 1], f.lo[1], f.hi[1]);
        nbr(lo, hi, ROT[shape % 6], f.lo[2], f.hi[2]);
        f.n = 3;
    }
    return f;
}
inline int distF(const Foot& f, int lo, int hi) {
    int d = hexDist(f.lo[0], f.hi[0], lo, hi);
    for (int i = 1; i < f.n; i++) { int e = hexDist(f.lo[i], f.hi[i], lo, hi); if (e < d) d = e; }
    return d;
}
inline bool inMap(int lo, int hi) { return lo >= 0 && lo < g_H && hi >= 0 && hi < g_W; }
inline uint32_t pack(int lo, int hi) { return (uint32_t)(uint16_t)lo | ((uint32_t)(uint16_t)hi << 16); }

// city (= base id < C) whose territory holds the hex, or -1
int cityOfHex(int lo, int hi) {
    if (!inMap(lo, hi)) return -1;
    unsigned area = *(const uint16_t*)(g_area20 + ((size_t)lo * g_W + hi) * 20 + 4);
    if ((int)area >= g_N + 6) return -1;
    int b = g_area2base[area];
    return b >= 0 && b < g_C ? b : -1;
}
// shaped city standing at pos (lo | hi << 16), or -1
int cityAt(uint32_t pos) {
    int lo = (int16_t)(pos & 0xffff), hi = (int16_t)(pos >> 16);
    int c = cityOfHex(lo, hi);
    if (c < 0 || g_rec[c].shape < 1 || g_rec[c].shape > 7) return -1;
    const uint8_t* b = (const uint8_t*)(BLD + (uintptr_t)c * BLD_SIZE);
    if (*(const int32_t*)(b + 8) != 0 || *(const uint32_t*)(b + 0x1e) != pos) return -1;
    return c;
}
// city whose model may stand at object position (our, ouc): a hex centre in a city's territory
int cityOfOu(int our, int ouc) {
    int lo = (2 * our - 0x70) / 4, hi = (2 * ouc - 0x70 - 2 * (lo & 1)) / 4;
    if (our != 2 * lo + 57 || ouc != 2 * hi + 57 + (lo & 1)) return -1;
    return cityOfHex(lo, hi);
}
inline bool special(int c) { return c >= 0 && ((g_rec[c].shape >= 1 && g_rec[c].shape <= 7) || g_rec[c].tier >= 2); }

// ---------------------------------------------------------------- footprint (0x41bce0 callers)
typedef int(__cdecl* FootprintFn)(const int16_t** out, int category);
int shapeList(int c, const int16_t** out) {
    int s = g_rec[c].shape;
    if (s >= 1 && s <= 6) { *out = &g_tri[s - 1][0][0]; return 12; }
    if (s == 7) { *out = &g_one[0][0]; return 4; }
    return 0;
}
int objIndex(const uint8_t* obj) {
    uintptr_t d = (uintptr_t)obj - OBJ_BASE;
    return (uintptr_t)obj >= OBJ_BASE && d % OBJ_SIZE == 0 && d / OBJ_SIZE < 0xffff ? (int)(d / OBJ_SIZE) : -1;
}

}  // namespace

extern "C" int __cdecl csFootprintObj(const int16_t** out, int category, const uint8_t* obj) {
    if ((uint8_t)category <= 5) {
        int i = objIndex(obj);
        if (i >= 0 && g_objCity[i] != NONE) { int n = shapeList(g_objCity[i], out); if (n) return n; }
    }
    return ((FootprintFn)0x41bce0)(out, category);
}
extern "C" int __cdecl csFootprintPos(const int16_t** out, int category, int our, int ouc) {
    if ((uint8_t)category <= 5) {
        int c = cityOfOu(our, ouc);
        if (c >= 0) { int n = shapeList(c, out); if (n) return n; }
    }
    return ((FootprintFn)0x41bce0)(out, category);
}

// 0x4166ad: place (object in ebx), 0x41683d: remove (ebp), 0x41754b: 0x417530 (edi); cdecl (out, category)
__declspec(naked) static void fpEbx() {
    __asm {
        push ecx
        push edx
        push ebx
        push dword ptr [esp + 0x14]
        push dword ptr [esp + 0x14]
        call csFootprintObj
        add esp, 0xc
        pop edx
        pop ecx
        ret
    }
}
__declspec(naked) static void fpEbp() {
    __asm {
        push ecx
        push edx
        push ebp
        push dword ptr [esp + 0x14]
        push dword ptr [esp + 0x14]
        call csFootprintObj
        add esp, 0xc
        pop edx
        pop ecx
        ret
    }
}
__declspec(naked) static void fpEdi() {
    __asm {
        push ecx
        push edx
        push edi
        push dword ptr [esp + 0x14]
        push dword ptr [esp + 0x14]
        call csFootprintObj
        add esp, 0xc
        pop edx
        pop ecx
        ret
    }
}
// 0x416a28 in 0x4169a0(category, our, ouc): its arguments our / ouc are at [esp + 0x24] / [esp + 0x28] here
__declspec(naked) static void fpPos() {
    __asm {
        push ecx
        push edx
        push dword ptr [esp + 0x30]      // ouc
        push dword ptr [esp + 0x30]      // our
        push dword ptr [esp + 0x18]      // category
        push dword ptr [esp + 0x18]      // out
        call csFootprintPos
        add esp, 0x10
        pop edx
        pop ecx
        ret
    }
}

// ---------------------------------------------------------------- object matrix (0x41c2e0)
static void* g_trMatrix = nullptr;
static volatile LONG g_assigned = 0;
extern "C" void __cdecl csMatrixAfter(uint8_t* obj, uint32_t ret) {
    int i = objIndex(obj);
    uint16_t c = NONE;
    if (ret == 0x41cf3c) {                       // object init 0x41cf00: (re)assign the slot
        if (obj[0x72] <= 5) {
            int k = cityOfOu(*(uint16_t*)(obj + 0x6e), *(uint16_t*)(obj + 0x70));
            if (special(k)) c = (uint16_t)k;
        }
        if (i >= 0) g_objCity[i] = c;
        if (c != NONE) {
            LONG n = InterlockedIncrement(&g_assigned);
            if (n <= 30 && g_log) g_log("city shapes: object %d (category %d at ou %u,%u) is city %d's model: shape %d tier %d",
                                        i, obj[0x72], *(uint16_t*)(obj + 0x6e), *(uint16_t*)(obj + 0x70), c, g_rec[c].shape, g_rec[c].tier);
        }
    } else if (i >= 0) c = g_objCity[i];
    if (c == NONE || obj[0x72] > 5) return;
    float* m = (float*)(obj + 0x10);
    float s = g_scale[g_rec[c].tier & 3];
    if (s != 1.0f)
        for (int r = 0; r < 3; r++) for (int k = 0; k < 3; k++) m[r * 4 + k] *= s;
    int sh = g_rec[c].shape;
    if (sh >= 1 && sh <= 6) {
        float unit = 2.0f * *(const float*)0x779bc4;          // world units per ou (0x41c2e0: ou * 2 * 5.0)
        int a = ROT[sh - 1], b = ROT[sh % 6];
        m[12] += unit * (OU[a][0] + OU[b][0]) / 3.0f;
        m[14] += unit * (OU[a][1] + OU[b][1]) / 3.0f;
    }
}
__declspec(naked) static void matrixHook() {            // thiscall (object), no arguments
    __asm {
        push ecx
        call g_trMatrix
        mov ecx, [esp]
        push dword ptr [esp + 4]         // our return address
        push ecx
        call csMatrixAfter
        add esp, 8
        pop ecx
        ret
    }
}

// ---------------------------------------------------------------- building radius (0x4843a0 walks, 0x483c80 rings)
typedef int(__cdecl* WalkCb)(uint8_t* hex, uint32_t pos, int a, int b);
typedef int(__thiscall* WalkFn)(uint8_t* hex20, uint32_t pos, int count, WalkCb fn, int a, int b);
typedef int(__thiscall* RingFn)(uint8_t* hex20, uint32_t* out, int r, uint32_t pos);

// thiscall (HEX20, pos, count, fn, a, b), ret 0x14, like 0x4843a0: fn(hex, pos, a, b) for each hex at distance
// 1..count of pos (ring by ring); returns 1 as soon as fn returns non-zero, else 0
extern "C" int __fastcall csWalk(uint8_t* hex20, int, uint32_t pos, int count, WalkCb fn, int a, int b) {
    int c = count >= 1 ? cityAt(pos) : -1;
    if (c < 0) return ((WalkFn)0x4843a0)(hex20, pos, count, fn, a, b);
    int lo = (int16_t)(pos & 0xffff), hi = (int16_t)(pos >> 16), dmax = count - 1;
    Foot f = footprint(g_rec[c].shape, lo, hi);
    for (int d = 0; d <= dmax; d++)
        for (int y = lo - dmax - 1; y <= lo + dmax + 1; y++)
            for (int x = hi - dmax - 2; x <= hi + dmax + 2; x++) {
                if (!inMap(y, x) || distF(f, y, x) != d || (y == lo && x == hi)) continue;
                if (fn(hex20 + ((size_t)y * g_W + x) * 20, pack(y, x), a, b)) return 1;
            }
    return 0;
}
// thiscall (HEX20, out, R, pos), ret 0xc, like 0x483c80: the hexes at distance R from pos whose terrain allows it
extern "C" int __fastcall csRing(uint8_t* hex20, int, uint32_t* out, int r, uint32_t pos) {
    int c = r >= 1 ? cityAt(pos) : -1;
    if (c < 0) return ((RingFn)0x483c80)(hex20, out, r, pos);
    int lo = (int16_t)(pos & 0xffff), hi = (int16_t)(pos >> 16), d = r - 1, n = 0;
    Foot f = footprint(g_rec[c].shape, lo, hi);
    for (int y = lo - d - 1; y <= lo + d + 1; y++)
        for (int x = hi - d - 2; x <= hi + d + 2; x++) {
            if (!inMap(y, x) || distF(f, y, x) != d || (r == 1 && y == lo && x == hi)) continue;
            uint32_t t = *(const uint32_t*)(hex20 + ((size_t)y * g_W + x) * 20 + 4) & 0x1f;
            if (*(const int32_t*)(RING_TERRAIN + t * 4)) out[n++] = pack(y, x);
        }
    return n;
}

namespace {

// call sites whose radius depends on the building type (see M11_MAP.md, "worldmod 实现")
struct Site { uint32_t at, target; const void* to; const char* what; };
const Site SITES[] = {
    { 0x4166ad, 0x41bce0, (const void*)fpEbx, "place object footprint (0x416690)" },
    { 0x41683d, 0x41bce0, (const void*)fpEbp, "remove object footprint (0x416820)" },
    { 0x41754b, 0x41bce0, (const void*)fpEdi, "footprint cells (0x417530)" },
    { 0x416a28, 0x41bce0, (const void*)fpPos, "free-cell check (0x4169a0)" },
    { 0x57eb2b, 0x4843a0, (const void*)csWalk, "map entry: clear HEX20 bit 21 round buildings (count 3/2)" },
    { 0x5a84b3, 0x4843a0, (const void*)csWalk, "0x5a8440: unit next to the building (count 2/1)" },
    { 0x5db419, 0x4843a0, (const void*)csWalk, "0x5db3f0: free hex next to the building (count 2/1)" },
    { 0x4b99e9, 0x4843a0, (const void*)csWalk, "0x4b99b0: around the building (count 3/2)" },
    { 0x4b9a2f, 0x4843a0, (const void*)csWalk, "0x4b9a00 (0x5e58c6: count 5/4)" },
    { 0x49d662, 0x4843a0, (const void*)csWalk, "0x49d640: target within count (0x4b3615: count 2/1)" },
    { 0x5a43b0, 0x483c80, (const void*)csRing, "0x5a4340: ring next to the building (R 2/1)" },
    { 0x5a817b, 0x483c80, (const void*)csRing, "0x5a8160: best hex next to the building (0x5a8338: R 2/1)" },
    { 0x5a0ec9, 0x483c80, (const void*)csRing, "0x5a0e80: attack range rings (min..max + 1 for cities)" },
    { 0x5ee9b9, 0x483c80, (const void*)csRing, "0x5ee750: rings 1..3/2 (5/4) round the building" },
    { 0x5a180f, 0x483c80, (const void*)csRing, "0x5a1770: highlight of a city (centre + ring 1)" },
};

bool callsTo(uint32_t at, uint32_t target) {
    const uint8_t* p = (const uint8_t*)(uintptr_t)at;
    return p[0] == 0xE8 && at + 5 + *(const int32_t*)(p + 1) == target;
}
void setCall(uint32_t at, const void* to) {
    DWORD old; VirtualProtect((void*)(uintptr_t)at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(int32_t*)(uintptr_t)(at + 1) = (int32_t)((uintptr_t)to - (at + 5));
    VirtualProtect((void*)(uintptr_t)at, 5, old, &old);
}

bool loadShapes(const wchar_t* dir, int C) {
    wchar_t p[MAX_PATH]; swprintf_s(p, L"%s\\city_shapes.bin", dir);
    FILE* f = nullptr; _wfopen_s(&f, p, L"rb");
    if (!f) { if (g_log) g_log("city shapes: no city_shapes.bin in the data directory: every city keeps 7 hexes"); return false; }
    uint32_t hdr[3] = {};
    bool ok = fread(hdr, 4, 3, f) == 3 && !memcmp(hdr, "WCSH", 4) && hdr[1] == 1;
    if (ok && hdr[2] != (uint32_t)C) { if (g_log) g_log("city shapes: city_shapes.bin is for %u cities, not %d: not used", hdr[2], C); ok = false; }
    else if (!ok && g_log) g_log("city shapes: city_shapes.bin: bad header (\"WCSH\", version 1 expected): not used");
    if (ok) {
        g_rec = (Rec*)VirtualAlloc(nullptr, (size_t)C * sizeof(Rec), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        ok = g_rec && fread(g_rec, sizeof(Rec), C, f) == (size_t)C;
        if (!ok && g_log) g_log("city shapes: city_shapes.bin is truncated: not used");
    }
    fclose(f);
    return ok;
}

}  // namespace

bool cityShapesInstall(const CityShapeSetup& s, TraceLogFn log) {
    g_log = log;
    g_C = s.C; g_N = s.N; g_W = s.W; g_H = s.H; g_area20 = s.area20; g_area2base = s.area2base;
    g_scale[2] = s.scaleMid; g_scale[3] = s.scaleSmall;
    if (!g_area20 || !g_area2base) { if (log) log("city shapes: needs bases=1: not used"); return false; }
    if (!loadShapes(s.dataDir, s.C)) return false;
    int n7 = 0, n3 = 0, n1 = 0, bad = 0, scaled = 0;
    for (int c = 0; c < s.C; c++) {
        Rec& r = g_rec[c];
        if (r.shape > 7 || r.tier > 3) { bad++; r.shape = 0; r.tier = 0; }
        if (r.shape == 0) n7++; else if (r.shape == 7) n1++; else n3++;
        if (g_scale[r.tier & 3] != 1.0f) scaled++;
    }
    if (log) log("city shapes: %d cities with 7 hexes, %d with 3, %d with 1; %d models scaled (middle %.2f, small %.2f)%s",
                 n7, n3, n1, scaled, g_scale[2], g_scale[3], bad ? "; invalid records reset to Koei's shape" : "");
    int wrong = 0;
    for (const Site& t : SITES)
        if (!callsTo(t.at, t.target)) { wrong++; if (log) log("city shapes: %08x is not a call to %08x (%s)", t.at, t.target, t.what); }
    static const uint8_t head[] = { 0x51, 0x33, 0xc0, 0x89, 0x41, 0x48 };    // push ecx; xor eax,eax; mov [ecx+48h],eax
    if (memcmp((void*)0x41c2e0, head, sizeof head)) { wrong++; if (log) log("city shapes: 0x41c2e0: unexpected bytes"); }
    if (wrong) { if (log) log("city shapes: ERROR: %d code sites differ: NOT installed (cities keep 7 hexes)", wrong); return false; }
    if (!s.write) { if (log) log("city shapes: %d call sites and 0x41c2e0 checked (verify_only)", (int)(sizeof SITES / sizeof SITES[0])); return true; }
    buildLists();
    for (int i = 0; i < 0x10000; i++) g_objCity[i] = NONE;
    uint8_t* t = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!t) return false;
    memcpy(t, head, sizeof head);
    t[sizeof head] = 0xE9; *(int32_t*)(t + sizeof head + 1) = (int32_t)((0x41c2e0 + sizeof head) - ((uintptr_t)t + sizeof head + 5));
    g_trMatrix = t;
    DWORD old; VirtualProtect((void*)0x41c2e0, 5, PAGE_EXECUTE_READWRITE, &old);
    *(uint8_t*)0x41c2e0 = 0xE9; *(int32_t*)0x41c2e1 = (int32_t)((uintptr_t)matrixHook - (0x41c2e0 + 5));
    VirtualProtect((void*)0x41c2e0, 5, old, &old);
    for (const Site& t2 : SITES) setCall(t2.at, t2.to);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    if (log) log("city shapes: installed (0x41c2e0 hook, %d call sites)", (int)(sizeof SITES / sizeof SITES[0]));
    return true;
}
