// Conditional expansion of native hidden research and teaching quotas.
// Research records: world+86dd8, 98 * 6c; fixed 0..47, hidden 48..97.
// Force: completed bits +b0, used counts +b8; selected hidden IDs +e8,
// completed hidden bits +110, used hidden counts +114. All are saved natively.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <stdint.h>
#include <string.h>
#include "research_progress.h"

namespace {
constexpr uintptr_t WORLD = 0x7201958;
constexpr int FIXED = 48, TOTAL = 98, RECORD = 0x6c;
const char SKILL_GRADES[] =
    "SAABCBBACBABABBABAASBCAAABABBCBABBAAAABASSSAAAABSABABBBABSBCBBSSAABACBAACBBBBABAABBCABAAAAACCCCCBCCC";
static_assert(sizeof(SKILL_GRADES) == 101, "100 original skill grades");
Forces* g_forces = nullptr;
TraceLogFn g_log = nullptr;
bool g_on = false;
using ValidFn = int(__cdecl*)(void*);
ValidFn isValid = (ValidFn)0x47a630;
using BaseFn = uint8_t*(__thiscall*)(uintptr_t, int);
BaseFn getBase = (BaseFn)0x490d00;
int g_cityCount = 42;
constexpr int PAID_TEACHING = 0x54450000;
using TeachingFn = int(__cdecl*)(uint8_t*);
TeachingFn startTeachingOriginal = nullptr;
using ForceFn = uint8_t*(__thiscall*)(uintptr_t, int);
ForceFn getForce = (ForceFn)0x490aa0;
using CompletedFn = int(__thiscall*)(uint8_t*, int);
CompletedFn isCompleted = (CompletedFn)0x481970;
using PointsFn = void(__thiscall*)(uint8_t*, int);
PointsFn setPoints = (PointsFn)0x481a80;
bool __cdecl isManual(uint8_t* force) {
    auto method = (int(__thiscall*)(void*))(*(void***)force)[0x48 / 4];
    return method(force) != 0;
}
bool (__cdecl* manualForce)(uint8_t*) = isManual;

int cityStage(int cities) {
    return cities >= 100 ? 4 : cities >= 80 ? 3 : cities >= 40 ? 2 : cities >= 20 ? 1 : 0;
}
int ownedCities(int fid) {
    int n = 0;
    for (int id = 0; id < g_cityCount; ++id) {
        auto city = getBase(WORLD, id);
        if (city && isValid(city)) {
            auto owner = (int(__thiscall*)(void*))(*(void***)city)[0x40 / 4];
            n += owner(city) == fid;
        }
    }
    return n; // ports and gates begin at g_cityCount and are never counted
}

uint8_t* record(int id) {
    return id >= 0 && id < TOTAL ? (uint8_t*)(WORLD + 0x86dd8 + id * RECORD) : nullptr;
}
int forceId(uint8_t* force) {
    uintptr_t first = g_forces ? (uintptr_t)g_forces->FORCEARR : WORLD + 0x7af8;
    uintptr_t p = (uintptr_t)force;
    int regular = g_forces ? g_forces->R : 42;
    return p >= first && (p - first) % 0x12c == 0 && (p - first) / 0x12c < (unsigned)regular
        ? (int)((p - first) / 0x12c) : -1;
}
int completed(uint8_t* force) {
    int n = 0;
    for (int i = 0; i < FIXED; ++i) n += (force[0xb0 + i / 8] >> (i % 8)) & 1;
    return n;
}
int stage(uint8_t* force) { return completed(force) / 12; }
int nativeLimit(uint8_t* r) {
    int kind = *(int*)(r + 0x44);
    return kind == 0 ? 255 : kind == 1 ? 1 : kind == 2 ? 3 : kind == 3 ? 5 : 0;
}
int quota(uint8_t* r, int level) {
    int original = nativeLimit(r);
    if (*(int*)(r + 0x48) != 2 || original == 255) return original;
    int skill = *(int*)(r + 0x58);
    if (skill < 0 || skill >= 100) return original;
    char grade = SKILL_GRADES[skill];
    int target = grade == 'S' ? 3 : grade == 'A' ? 10 : grade == 'B' ? 20 : 30;
    target = std::max(original, target); // never reduce an original five-use S skill
    return original + (target - original) * std::min(4, std::max(0, level)) / 4;
}

void unlock(uint8_t* force) {
    int fid = forceId(force);
    if (!g_on || fid < 0 || !isValid(force)) return;
    int level = stage(force), goal = level == 4 ? 10 : 5 + level;
    int* chosen = (int*)(force + 0xe8);
    int occupied = 0;
    for (int g = 0; g < 10; ++g) occupied += chosen[g] >= FIXED && chosen[g] < TOTAL;
    // Existing choices, completed flags and use counts are preserved. A pure
    // deterministic choice avoids consuming the game's random-number stream.
    for (int g = 0; g < 10 && occupied < goal; ++g) {
        if (chosen[g] != -1) continue;
        uint32_t seed = (uint32_t)(fid + 1) * 0x9e3779b9u ^
                        (uint32_t)*(int*)(force + 4) * 0x85ebca6bu ^ (uint32_t)g * 0xc2b2ae35u;
        seed ^= seed >> 16;
        int id = FIXED + 5 * g + seed % 5;
        auto r = record(id);
        if (*(int*)(r + 0x64) != g || *(int*)(r + 0x48) != 2) continue;
        chosen[g] = id;
        ++occupied;
        if (g_log) g_log("research: force=%d completed=%d/48 hidden_slot=%d ability=%d skill=%d total=%d/%d",
                         fid, completed(force), g, id, *(int*)(r + 0x58), occupied, goal);
    }
}

int __fastcall selected(uint8_t* force, void*, int group) {
    unlock(force);
    return group >= 0 && group < 10 ? ((int*)(force + 0xe8))[group] : -1;
}
int __fastcall hasHidden(uint8_t* force, void*, int id) {
    unlock(force);
    if (id < FIXED || id >= TOTAL) return 0;
    for (int g = 0; g < 10; ++g) if (((int*)(force + 0xe8))[g] == id) return 1;
    return 0;
}
int usedCount(uint8_t* force, int id) {
    if (id >= 0 && id < FIXED) return force[0xb8 + id];
    for (int g = 0; g < 10; ++g)
        if (((int*)(force + 0xe8))[g] == id) return force[0x114 + g];
    return 0;
}
int recordId(uint8_t* r) {
    uintptr_t offset = (uintptr_t)r - (uintptr_t)record(0);
    return offset < TOTAL * RECORD && offset % RECORD == 0 ? (int)(offset / RECORD) : -1;
}
int exchangePrice(uint8_t* force, uint8_t* r) {
    int id = recordId(r), fid = forceId(force);
    if (!g_on || fid < 0 || !isValid(force) || id < 0 || *(int*)(r + 0x48) != 2 ||
        nativeLimit(r) == 255 || !isCompleted(force, id)) return 0;
    if (usedCount(force, id) < quota(r, cityStage(ownedCities(fid)))) return 0;
    int skill = *(int*)(r + 0x58);
    if (skill < 0 || skill >= 100) return 0;
    char grade = SKILL_GRADES[skill];
    return grade == 'S' ? 3000 : grade == 'A' ? 1500 : grade == 'B' ? 800 : 400;
}
uint8_t __fastcall limit(uint8_t* r, uint8_t* force) {
    int fid = forceId(force);
    if (!g_on || fid < 0 || !isValid(force) || *(int*)(r + 0x48) != 2)
        return (uint8_t)nativeLimit(r);
    int total = quota(r, cityStage(ownedCities(fid)));
    // Territory losses may lower the total below already spent uses. Show zero
    // remaining instead of a negative count, without erasing any spent uses.
    uintptr_t offset = (uintptr_t)r - (uintptr_t)record(0);
    if (offset < TOTAL * RECORD && offset % RECORD == 0) {
        int id = (int)(offset / RECORD), used = usedCount(force, id);
        total = std::max(total, used);
        int price = exchangePrice(force, r);
        if (used < 255 && price > 0 && *(uint16_t*)(force + 0xa2) >= price)
            total = std::max(total, used + 1);
    }
    return (uint8_t)total;
}
int __cdecl startTeaching(uint8_t* command) {
    if (!command || !isValid(*(void**)command) || !isValid(*(void**)(command+4)))
        return startTeachingOriginal(command);
    auto base = *(uint8_t**)command;
    auto owner = (int(__thiscall*)(void*))(*(void***)base)[0x40 / 4];
    auto force = getForce(WORLD, owner(base));
    int id = *(int*)(command + 8);
    auto r = record(id);
    int price = r ? exchangePrice(force, r) : 0;
    if (price > 0) {
        int balance = *(uint16_t*)(force + 0xa2);
        if (balance < price || (manualForce(force) && !researchExchangeConfirm(price, balance, r))) return 0;
        // Recheck after the modal. No debit occurs during preview or cancellation.
        if (*(uint16_t*)(force + 0xa2) < price) return 0;
    }
    int result = startTeachingOriginal(command);
    if (result && price > 0) {
        auto person = *(uint8_t**)(command + 4);
        if (*(int*)(person + 0x13c) == 42 && *(int*)(person + 0x140) == id) {
            setPoints(force, *(uint16_t*)(force + 0xa2) - price);
            // Native saved order parameter 4 is unused by skill teaching. It
            // survives saving mid-training; completed paid uses never alter
            // the free-use counter, including after territory loss/regain.
            *(int*)(person + 0x150) = PAID_TEACHING | id;
            if (g_log) g_log("research exchange: force=%d research=%d price=%d remaining_tp=%d",
                forceId(force), id, price, *(uint16_t*)(force + 0xa2));
        }
    }
    return result;
}
bool __cdecl paidTeaching(uint8_t* person, int id) {
    return *(int*)(person + 0x13c) == 42 && *(int*)(person + 0x140) == id &&
           *(int*)(person + 0x150) == (PAID_TEACHING | id);
}
__declspec(naked) void finishTeaching() { __asm {
    push ecx
    push edx
    push eax
    push dword ptr [ebp+0ch]
    push ebx
    call paidTeaching
    add esp,8
    test al,al
    pop eax
    pop edx
    pop ecx
    jnz paid
    mov eax,[esi+44h]
    test eax,eax
    ret
paid:
    xor eax,eax
    test eax,eax
    ret
} }
void __cdecl remainingText(void* out, const char* format, int count, uint8_t* r, uint8_t* force) {
    int price = exchangePrice(force, r);
    researchExchangeRemaining(out, format, count,
        price > 0 && *(uint16_t*)(force + 0xa2) >= price ? price : 0);
}
__declspec(naked) void remainingUi() { __asm {
    mov eax,esp
    push dword ptr [eax+20h]
    push ebx
    push dword ptr [eax+0ch]
    push dword ptr [eax+8]
    push dword ptr [eax+4]
    call remainingText
    add esp,20
    ret
} }
// Per-force context is explicit at each native call site, including AI ranking
// and all three UI descriptions. No global or last-selected-force state.
__declspec(naked) void limitCheck() { __asm {
    mov edx,edi
    jmp limit
} }
__declspec(naked) void limitAiFirst() { __asm {
    mov edx,[esp+20h]
    jmp limit
} }
__declspec(naked) void limitAiSecond() { __asm {
    mov edx,ebp
    jmp limit
} }
__declspec(naked) void limitUi() { __asm {
    mov edx,[esp+14h]
    jmp limit
} }

struct Site { uintptr_t at; const char* expected; int size; const void* target; bool call; };
Site sites[] = {
    {0x4813a0, "\x8b\x44\x24\x04\x85\xc0", 6, selected, false},
    {0x481a40, "\x8b\x54\x24\x04\x83\xfa\x2f", 7, hasHidden, false},
    {0x4cdb4c, "\xe8\x2f\x74\xfc\xff", 5, limitCheck, true},
    {0x5deb35, "\xe8\x46\x64\xeb\xff", 5, limitAiFirst, true},
    {0x5deb5b, "\xe8\x20\x64\xeb\xff", 5, limitAiSecond, true},
    {0x6127e6, "\xe8\x95\x27\xe8\xff", 5, limitUi, true},
    {0x613d52, "\xe8\x29\x12\xe8\xff", 5, limitUi, true},
    {0x614c92, "\xe8\xe9\x02\xe8\xff", 5, limitUi, true},
    {0x5da6e0, "\x55\x8b\xec\x83\xe4\xf8", 6, startTeaching, false},
    {0x5daab5, "\x8b\x46\x44\x85\xc0", 5, finishTeaching, true},
    {0x614cc3, "\xe8\x48\xd0\xe5\xff", 5, remainingUi, true},
};
}

bool researchProgressInstall(const wchar_t* dir, const wchar_t* section, Forces* forces,
                             bool write, TraceLogFn log) {
    wchar_t ini[MAX_PATH]; swprintf_s(ini, L"%s\\worldmod.ini", dir);
    if (!GetPrivateProfileIntW(section, L"research_progress", 0, ini)) return true;
    if (g_on) return false;
    for (const auto& s : sites) if (memcmp((void*)s.at, s.expected, s.size)) {
        log("research: unexpected code at %08x; no research hooks installed", s.at); return false;
    }
    if (!write) { log("research: eleven native sites verified; no changes"); return true; }
    auto trampoline = (uint8_t*)VirtualAlloc(nullptr, 16, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!trampoline) return false;
    memcpy(trampoline, (void*)sites[8].at, 6);
    trampoline[6] = 0xe9; *(int32_t*)(trampoline+7) = (int32_t)(sites[8].at+6 - ((uintptr_t)trampoline+11));
    startTeachingOriginal = (TeachingFn)trampoline;
    // Acquire all patch pages before the first instruction is changed.
    DWORD old[sizeof sites / sizeof *sites];
    size_t k = 0;
    for (; k < sizeof sites / sizeof *sites; ++k) {
        const auto& s = sites[k];
        if (!VirtualProtect((void*)s.at, s.size, PAGE_EXECUTE_READWRITE, old + k)) break;
    }
    if (k != sizeof sites / sizeof *sites) {
        while (k) { --k; DWORD ignored; VirtualProtect((void*)sites[k].at, sites[k].size, old[k], &ignored); }
        VirtualFree(trampoline, 0, MEM_RELEASE); startTeachingOriginal = nullptr;
        return false;
    }
    g_forces = forces; g_log = log;
    g_cityCount = (int)GetPrivateProfileIntW(section, L"cities", 42, ini);
    g_cityCount = std::min(1000, std::max(42, g_cityCount));
    for (size_t i = 0; i < sizeof sites / sizeof *sites; ++i) {
        const auto& s = sites[i]; auto p = (uint8_t*)s.at;
        p[0] = s.call ? 0xe8 : 0xe9;
        *(int32_t*)(p + 1) = (int32_t)((uintptr_t)s.target - (s.at + 5));
        if (s.size > 5) memset(p + 5, 0x90, s.size - 5);
    }
    // Several sites share a page; restore protections in reverse order.
    for (size_t i = sizeof sites / sizeof *sites; i-- > 0;) {
        DWORD ignored; VirtualProtect((void*)sites[i].at, sites[i].size, old[i], &ignored);
        FlushInstructionCache(GetCurrentProcess(), (void*)sites[i].at, sites[i].size);
    }
    g_on = true;
    log("research: installed; fixed completion 12/24/36/48 -> hidden 6/7/8/10; owned cities 20/40/80/100 -> teaching S>=3/A10/B20/C30; spent uses and prerequisites preserved");
    log("research exchange: exhausted researched skills cost S3000/A1500/B800/C400 TP per training; paid orders saved natively");
    return true;
}
