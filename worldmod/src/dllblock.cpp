// Keeps listed DLLs out of the game: see dllblock.h.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <wchar.h>
#include "dllblock.h"

typedef HMODULE (WINAPI* LoadLibraryExWFn)(LPCWSTR, HANDLE, DWORD);
static LoadLibraryExWFn g_orig = nullptr;          // trampoline: the 5 copied bytes + jmp back
static TraceLogFn g_log = nullptr;
static wchar_t g_list[8][64];                       // lower-case substrings
static int g_count = 0;
static volatile LONG g_blocked = 0;

static HMODULE WINAPI hook(LPCWSTR name, HANDLE file, DWORD flags) {
    if (name) {
        wchar_t low[MAX_PATH]; wcsncpy_s(low, name, _TRUNCATE); _wcslwr_s(low);
        for (int i = 0; i < g_count; i++) {
            if (wcsstr(low, g_list[i])) {
                LONG n = InterlockedIncrement(&g_blocked);
                if (n <= 20 || (n & (n - 1)) == 0) g_log("dll block: refused %ls, #%ld", name, n);
                SetLastError(ERROR_ACCESS_DENIED);
                return nullptr;
            }
        }
    }
    return g_orig(name, file, flags);
}

bool dllBlockInstall(const wchar_t* dir, const wchar_t* section, TraceLogFn log) {
    g_log = log;
    wchar_t ini[MAX_PATH]; swprintf_s(ini, L"%s\\worldmod.ini", dir);
    wchar_t spec[512];
    GetPrivateProfileStringW(section, L"block_dlls", L"BaiduPinyin", spec, 512, ini);
    wchar_t* ctx = nullptr;
    for (wchar_t* t = wcstok_s(spec, L", ", &ctx); t && g_count < 8; t = wcstok_s(nullptr, L", ", &ctx)) {
        wcsncpy_s(g_list[g_count], t, _TRUNCATE); _wcslwr_s(g_list[g_count]); g_count++;
    }
    if (!g_count) return false;
    uint8_t* site = (uint8_t*)GetProcAddress(GetModuleHandleA("kernelbase.dll"), "LoadLibraryExW");
    static const uint8_t hot[5] = { 0x8b, 0xff, 0x55, 0x8b, 0xec };   // mov edi,edi; push ebp; mov ebp,esp
    if (!site || memcmp(site, hot, 5)) { log("dll block: unexpected code at KernelBase!LoadLibraryExW, not hooked"); return false; }
    uint8_t* tramp = (uint8_t*)VirtualAlloc(nullptr, 16, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;
    memcpy(tramp, site, 5);
    tramp[5] = 0xE9; *(int32_t*)(tramp + 6) = (int32_t)((uintptr_t)(site + 5) - (uintptr_t)(tramp + 10));
    g_orig = (LoadLibraryExWFn)tramp;
    DWORD old; VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    site[0] = 0xE9; *(int32_t*)(site + 1) = (int32_t)((uintptr_t)hook - (uintptr_t)(site + 5));
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    log("dll block: DLLs whose path contains %ls are not loaded (block_dlls)", spec[0] ? g_list[0] : L"?");
    return true;
}
