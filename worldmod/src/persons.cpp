// persons: memory for more officers (武将) than Koei's 1100 (M10_PERSONS.md).
//
// The world object keeps 1100 officer records of 0x190 bytes at world+0xC0BC; the treasures follow right after them,
// so the array moves here (PT records, constructed and destroyed by the world's own ctor / dtor through the patched eh
// vector iterators). The generic list node pool moves here as well (its static storage holds only ~6500 blocks).
// Scenario streams store the scenario officers; PSTREAM is the count of the stream being read or written (the header
// mark says how many, Koei's files have 850), so Koei's scenarios still load with the extended officers empty.
// Koei's biographies are messages of S11MSG02.S11 (0x48e850); the extended officers' come from persons_bio.bin.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include "persons.h"

namespace {

uint8_t* alloc(size_t size) {
    size = (size + 15) & ~(size_t)15;
    return (uint8_t*)VirtualAlloc(nullptr, size + 64, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);   // zero-filled
}

const uint32_t PERSON_SIZE = 0x190;
int g_pstream = 850;
TraceLogFn g_log = nullptr;

// officer snapshot objects (vt 0x862d28) live on the stack of four dialogs with room for 1100 officers; their data
// methods run on a buffer of the same layout (records 0xa4 at +4, a backup copy after PT records, flags after
// 2 x PT records) kept per object address
struct Shadow { uint8_t* self; uint8_t* buf; };
Shadow g_shadow[16];
int g_nshadow = 0, g_nextShadow = 0;
size_t g_shadowSize = 0;
Persons* g_p = nullptr;

// persons_bio.bin (tools/roster): "WPBI", u32 first id, u32 n, u32 offsets[n + 1] (from the end of the offsets),
// then the texts, each NUL-terminated
wchar_t g_bioPath[MAX_PATH] = L"";
bool g_bioTried = false;
std::vector<char> g_bio;
uint32_t g_bioFirst = 0, g_bioN = 0;

void loadBio() {
    g_bioTried = true;
    FILE* f = nullptr;
    if (!g_bioPath[0] || _wfopen_s(&f, g_bioPath, L"rb") || !f) return;
    uint32_t h[3];
    if (fread(h, 4, 3, f) == 3 && h[0] == 'IBPW') {
        fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 12, SEEK_SET);
        if (size > 12) {
            g_bio.resize((size_t)size - 12);
            if (fread(g_bio.data(), 1, g_bio.size(), f) == g_bio.size() && (uint64_t)(h[2] + 1) * 4 <= g_bio.size()) {
                g_bioFirst = h[1]; g_bioN = h[2];
            }
        }
    }
    fclose(f);
    if (g_log) g_log("persons: %u biographies from id %u", g_bioN, g_bioFirst);
}

}  // namespace

extern "C" uint8_t* __cdecl personSnapShadow(uint8_t* self) {
    for (int i = 0; i < g_nshadow; i++)
        if (g_shadow[i].self == self || g_shadow[i].buf == self) return g_shadow[i].buf;
    uint8_t* b = alloc(g_shadowSize);
    if (!b) { if (g_log) g_log("ERROR: persons: snapshot buffer allocation failed"); return self; }
    int k = g_nshadow < 16 ? g_nshadow++ : g_nextShadow++ % 16;   // old buffers stay allocated (still in use?)
    g_shadow[k] = { self, b };
    return b;
}

extern "C" const char* __cdecl personBio(int id) {
    if (!g_p || id < 850 || id >= g_p->PS) return nullptr;
    if (!g_bioTried) loadBio();
    uint32_t k = (uint32_t)id - g_bioFirst;
    if (k >= g_bioN) return nullptr;
    const uint32_t* off = (const uint32_t*)g_bio.data();
    size_t base = (size_t)(g_bioN + 1) * 4, a = base + off[k], b = base + off[k + 1];
    if (a >= b || b > g_bio.size() || g_bio[b - 1] != 0) return nullptr;
    return g_bio.data() + a;
}

bool personsSetup(int PS, const wchar_t* dataDir, Persons& p, TraceLogFn log) {
    // officer ids are i16 in the files and the virtual fathers move to 32000..32060 above 2000 officers
    const int maxPS = 32000 - 250;
    if (PS < 850 || PS > maxPS) { if (log) log("persons: scenario officer count %d not supported (850..%d), using 850", PS, maxPS); PS = 850; }
    p.PS = PS;
    p.PT = PS + 250;
    p.VF = p.PT <= 2000 ? 2000 : 32000;
    p.PERSONARR = alloc((size_t)p.PT * PERSON_SIZE);
    p.PPOOL = alloc(0x2000 + 0x10000 * 0x7c);
    p.PSTREAM = &g_pstream;
    g_log = log;
    g_shadowSize = 4 + (size_t)p.PT * 0x14c;
    if (!p.PERSONARR || !p.PPOOL) { if (log) log("persons: allocation failed"); return false; }
    g_p = &p;
    if (dataDir && dataDir[0]) swprintf_s(g_bioPath, L"%s\\persons_bio.bin", dataDir);
    if (log) log("persons: PS=%d PT=%d VF=%d, arrays allocated (PERSONARR %p, PPOOL %p)", p.PS, p.PT, p.VF, p.PERSONARR, p.PPOOL);
    return true;
}

uint8_t* personPtr(int id) {
    return g_p && id >= 0 && id < g_p->PT ? g_p->PERSONARR + (size_t)id * PERSON_SIZE : nullptr;
}

typedef int(__cdecl* IsValidFn)(void*);
typedef void(__thiscall* AddTailFn)(void*, void*);
extern "C" void __cdecl personsRebuildOrders(uint8_t* list) {
    if (!g_p) return;
    // CList (vtbl 0x79bf44): +4 head; node +0 next, +8 data (officer pointer)
    std::vector<uint8_t> present(g_p->PT);
    for (uint8_t* n = *(uint8_t**)(list + 4); n; n = *(uint8_t**)n) {
        size_t off = (size_t)(*(uint8_t**)(n + 8) - g_p->PERSONARR);
        if (off < (size_t)g_p->PT * PERSON_SIZE) present[off / PERSON_SIZE] = 1;
    }
    IsValidFn valid = (IsValidFn)0x47a630;
    AddTailFn addTail = (AddTailFn)0x47c1b0;
    int added = 0;
    for (int i = 0; i < g_p->PT; i++) {
        uint8_t* p = g_p->PERSONARR + (size_t)i * PERSON_SIZE;
        int order = *(int*)(p + 0x13c);
        if (present[i] || order < 0 || order > 43 || !valid(p)) continue;
        addTail(list, p);
        added++;
    }
    if (added && g_log) g_log("persons: %d officers with an order in progress restored to the order list", added);
}
