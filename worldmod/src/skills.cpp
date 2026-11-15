// SIRE-style composition, implemented independently for worldmod. No fixed person array or count.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include "skills.h"

namespace {
constexpr int LIMIT = 250, BASE = 100, RECORD = 0x6c;
struct Skill {
    bool defined = false;
    char name[5] = {}, description[67] = {};
    bool effects[LIMIT] = {};
};
Skill g_skills[LIMIT];
uint8_t g_records[LIMIT][RECORD];
int g_count = BASE;
TraceLogFn g_log = nullptr;
bool g_prepared = false;

bool valid(int id) { return id >= 0 && id < LIMIT && (id < BASE || g_skills[id].defined); }
bool effectId(int id) { return (id >= 0 && id < BASE) || id == 146; }
bool contains(int owned, int queried) {
    return valid(owned) && (valid(queried) || queried == 146) &&
        (owned == queried || (effectId(queried) && g_skills[owned].effects[queried]));
}

bool number(const std::string& s, int& n) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos || s.size() > 3) return false;
    n = atoi(s.c_str()); return true;
}

bool big5(const std::string& text, char* out, int capacity) {
    if (text.empty()) { out[0] = 0; return true; }
    wchar_t wide[2048];
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, wide, 2048);
    if (!n) return false;
    BOOL replaced = FALSE;
    n = WideCharToMultiByte(950, WC_NO_BEST_FIT_CHARS, wide, -1, out, capacity, nullptr, &replaced);
    return n > 0 && !replaced;
}

// id|name|description|comma-separated effects. One level only; original effects plus implemented Bole (146).
bool parse(FILE* file, std::string& error) {
    for (auto& s : g_skills) s = Skill{};
    g_count = BASE;
    char buf[2048]; int line = 0;
    while (fgets(buf, sizeof buf, file)) {
        ++line;
        std::string text(buf);
        if (line == 1 && text.compare(0, 3, "\xef\xbb\xbf") == 0) text.erase(0, 3);
        if (!text.empty() && text.back() == '\n') text.pop_back();
        if (!text.empty() && text.back() == '\r') text.pop_back();
        if (text.empty() || text[0] == '#') continue;
        std::string fields[4]; size_t start = 0;
        bool ok = text.size() < sizeof buf - 1;
        for (int i = 0; i < 3 && ok; ++i) {
            size_t end = text.find('|', start);
            if (end == std::string::npos) { ok = false; break; }
            fields[i] = text.substr(start, end - start); start = end + 1;
        }
        fields[3] = text.substr(start);
        int id = -1;
        ok = ok && fields[3].find('|') == std::string::npos && number(fields[0], id) && id < LIMIT;
        if (ok) {
            Skill& s = g_skills[id];
            ok = !s.defined && !fields[1].empty() && big5(fields[1], s.name, sizeof s.name) &&
                big5(fields[2], s.description, sizeof s.description);
            int slots = 0;
            std::istringstream effects(fields[3]); std::string item;
            while (ok && std::getline(effects, item, ',')) {
                int effect = -1;
                ok = number(item, effect) && effectId(effect) && ++slots <= 8;
                if (ok) s.effects[effect] = true;
            }
            ok = ok && (fields[3].empty() || fields[3].back() != ',') && (id < BASE || slots > 0);
            if (ok) { s.defined = true; if (id >= g_count) g_count = id + 1; }
        }
        if (!ok) { error = "invalid definition at line " + std::to_string(line); return false; }
    }
    if (ferror(file)) { error = "could not read definitions"; return false; }
    return true;
}

int __fastcall personHas(uint8_t* self, void*, int queried) {
    return contains(*(int*)(self + 0xe8), queried);
}
void __fastcall personSet(uint8_t* self, void*, int id) {
    if (id == -1 || valid(id)) *(int*)(self + 0xe8) = id;
}
int __fastcall unitHas(void* self, void*, int queried) {
    // The native vtable+5c already resolves a matching member via personHas. Its -1 sentinel is enough;
    // do not retain the original 1099 upper bound, which breaks expanded person arrays.
    typedef int(__thiscall* Find)(void*, int);
    return (*(Find*)(*(uint8_t**)self + 0x5c))(self, queried) >= 0;
}

uint8_t* __fastcall skillRecord(uint8_t* world, void*, int id) {
    if (id < 0 || id >= g_count) return nullptr;
    if (id < BASE && !g_skills[id].defined) return world + 0x80ef8 + id * RECORD;
    int templateId = id < BASE ? id : 0;
    if (id >= BASE)
        for (int i = 0; i < BASE; ++i) if (g_skills[id].effects[i]) { templateId = i; break; }
    uint8_t* r = g_records[id];
    // Preserve the game's native vtable, category/level and derived fields from an original effect.
    memcpy(r, world + 0x80ef8 + templateId * RECORD, RECORD);
    memset(r + 4, 0, 5); memset(r + 9, 0, 19); memset(r + 0x1c, 0, 67);
    if (g_skills[id].defined) {
        memcpy(r + 4, g_skills[id].name, sizeof g_skills[id].name);
        memcpy(r + 9, g_skills[id].name, sizeof g_skills[id].name);
        memcpy(r + 0x1c, g_skills[id].description, sizeof g_skills[id].description);
    } else {
        memcpy(r + 4, "----", 5);
        memcpy(r + 9, "Reserved", 9); // holes stay valid objects when native lists enumerate 0..count-1
    }
    return r;
}
int __fastcall skillId(uint8_t* world, void*, const void* record) {
    uintptr_t p = (uintptr_t)record, first = (uintptr_t)g_records;
    if (p >= first && p < first + sizeof g_records && (p - first) % RECORD == 0)
        return (int)((p - first) / RECORD);
    first = (uintptr_t)world + 0x80ef8;
    if (p >= first && p < first + BASE * RECORD && (p - first) % RECORD == 0)
        return (int)((p - first) / RECORD);
    return -1;
}
int __fastcall skillOrder(uint8_t* record, void*) {
    int id = skillId((uint8_t*)0x7201958, nullptr, record);
    return id >= 0 && id < BASE ? ((int*)0x79c948)[id] : id;
}

typedef void*(__thiscall* Serialize)(void*, int*);
Serialize g_serializeOriginal = (Serialize)0x48a970;
void* __fastcall serializeSkill(uint8_t* stream, void*, int* value) {
    void* result = g_serializeOriginal(stream, value);
    // Only the skill field's call site is replaced. Other signed-byte fields retain native semantics.
    if (*(int*)(stream + 8) == 1) {
        unsigned id = (uint8_t)*value;
        *value = id == 255 ? -1 : id < LIMIT ? (int)id : -1;
    }
    return result;
}
void __cdecl incomeFlags(int* flags, int owned) {
    if (owned >= 0 && owned < BASE) flags[owned] = 1;
    if (valid(owned))
        for (int i = 0; i < BASE; ++i) if (g_skills[owned].effects[i]) flags[i] = 1;
}
uint32_t g_incomeResume = 0x4cea51;
__declspec(naked) void incomeBridge() {
    __asm {
        pushad
        push eax
        push ebx
        call incomeFlags
        add esp, 8
        popad
        jmp dword ptr [g_incomeResume]
    }
}

typedef int(__thiscall* Affinity)(void*, int);
Affinity g_affinityOriginal = (Affinity)0x489f80;
int __cdecl boleRecruit(void* building, void* target) {
    typedef int(__thiscall* Find)(void*, int);
    int recruiter = (*(Find*)(*(uint8_t**)building + 0x5c))(building, 146);
    return recruiter >= 0 && g_affinityOriginal(target, recruiter) <= 25;
}
uint32_t g_boleAllow = 0x4cf717, g_boleReject = 0x4cf7a7;
__declspec(naked) void boleBridge() {
    __asm {
        jz allowed
        pushad
        mov eax, [esp + 0x38] // original stack+0x18: recruiting building
        push esi             // candidate free officer
        push eax
        call boleRecruit
        add esp, 8
        test eax, eax
        popad
        jnz allowed
        jmp dword ptr [g_boleReject]
    allowed:
        jmp dword ptr [g_boleAllow]
    }
}

struct Hook { uint32_t at; const char* expected; size_t size; const void* target; bool call; DWORD protection; const char* replacement; };
Hook g_hooks[] = {
    {0x4890f0, "\x8b\x44\x24\x04\x85\xc0", 6, personHas},
    {0x4899d0, "\x8b\x44\x24\x04\x83\xf8\xff", 7, personSet},
    {0x47a8a0, "\x8b\x54\x24\x04\x8b\x01", 6, unitHas},
    {0x490c50, "\x8b\x44\x24\x04\x85\xc0", 6, skillRecord},
    {0x4915e0, "\x56\x8b\x74\x24\x08", 5, skillId},
    {0x4945d0, "\x8b\x01\xff\x50\x28", 5, skillOrder},
    // Native officer-list sorting must use the skill ID rather than the original 100-entry order table.
    {0x4c93e2, "\x6a\x01", 2, nullptr, false, 0, "\x6a\x05"},
    {0x48b97c, "\xe8\xef\xef\xff\xff", 5, serializeSkill, true},
    {0x4cea41, "\x85\xc0\x7c\x0c\x83\xf8\x63\x7f\x07\xc7\x04\x83\x01\x00\x00\x00", 16, incomeBridge},
    {0x4cf711, "\x0f\x85\x90\x00\x00\x00", 6, boleBridge},
};
bool verify() {
    for (auto& h : g_hooks) {
        if (memcmp((void*)h.at, h.expected, h.size)) {
            if (g_log) g_log("skills: unexpected instructions at %08x; rejected (another mod?)", h.at);
            return false;
        }
    }
    return true;
}
} // namespace

bool skillsPrepare(const wchar_t* path, TraceLogFn log) {
    g_log = log; g_prepared = false;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") || !f) {
        if (log) log("skills: cannot open %ls", path);
        return false;
    }
    std::string error;
    bool ok = parse(f, error); fclose(f);
    if (!ok) { if (log) log("skills: %s; rejected", error.c_str()); return false; }
    if (!verify()) return false;
    g_prepared = true;
    if (log) log("skills: definitions verified, count=%d (original effects + Bole 146)", g_count);
    return true;
}

bool skillsInstall() {
    if (!g_prepared || !verify()) return false;
    // Acquire write access for all sites before modifying any of them.
    size_t acquired = 0;
    for (auto& h : g_hooks) {
        if (!VirtualProtect((void*)h.at, h.size, PAGE_EXECUTE_READWRITE, &h.protection)) {
            for (size_t i = 0; i < acquired; ++i) {
                DWORD old; VirtualProtect((void*)g_hooks[i].at, g_hooks[i].size, g_hooks[i].protection, &old);
            }
            if (g_log) g_log("skills: could not make patch sites writable; nothing installed");
            return false;
        }
        ++acquired;
    }
    for (auto& h : g_hooks) {
        uint8_t* p = (uint8_t*)h.at;
        if (h.replacement) memcpy(p, h.replacement, h.size);
        else {
            p[0] = h.call ? 0xe8 : 0xe9;
            *(int32_t*)(p + 1) = (int32_t)((uintptr_t)h.target - h.at - 5);
            memset(p + 5, 0x90, h.size - 5);
        }
    }
    for (auto& h : g_hooks) {
        DWORD old; VirtualProtect((void*)h.at, h.size, h.protection, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)h.at, h.size);
    }
    g_prepared = false;
    if (g_log) g_log("skills: installed %u hooks, count=%d", (unsigned)(sizeof g_hooks / sizeof *g_hooks), g_count);
    return true;
}
