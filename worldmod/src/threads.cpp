// Stack reservation of threads other than the game's own: see threads.h.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "threads.h"

typedef HANDLE (WINAPI* CreateRemoteThreadExFn)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID,
                                               DWORD, LPPROC_THREAD_ATTRIBUTE_LIST, LPDWORD);
static CreateRemoteThreadExFn g_orig = nullptr;    // trampoline: the 5 copied bytes + jmp back
static TraceLogFn g_log = nullptr;
static uintptr_t g_exeLo = 0, g_exeHi = 0;
static SIZE_T g_reserve = 0;
static volatile LONG g_capped = 0;

static HANDLE WINAPI hook(HANDLE process, LPSECURITY_ATTRIBUTES sa, SIZE_T stack, LPTHREAD_START_ROUTINE start, LPVOID param,
                          DWORD flags, LPPROC_THREAD_ATTRIBUTE_LIST attrs, LPDWORD id) {
    uintptr_t s = (uintptr_t)start;
    bool own = s >= g_exeLo && s < g_exeHi;
    // without STACK_SIZE_PARAM_IS_A_RESERVATION the size (often 0) is a commit and the reservation is the exe's
    // 48 MB; with it the caller chose the reservation and keeps it
    if (process == GetCurrentProcess() && !own && !(flags & STACK_SIZE_PARAM_IS_A_RESERVATION)) {
        SIZE_T commit = (stack + 0xffff) & ~(SIZE_T)0xffff;
        stack = commit > g_reserve ? commit : g_reserve;
        flags |= STACK_SIZE_PARAM_IS_A_RESERVATION;
        LONG n = InterlockedIncrement(&g_capped);
        if (n <= 8 || (n & (n - 1)) == 0) {
            HMODULE m = nullptr; char name[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)start, &m))
                GetModuleFileNameA(m, name, MAX_PATH);
            const char* b = strrchr(name, '\\');
            g_log("thread stacks: thread starting in %s reserves %u KB, #%ld", b ? b + 1 : name, (unsigned)(stack >> 10), n);
        }
    }
    return g_orig(process, sa, stack, start, param, flags, attrs, id);
}

bool threadStacksInstall(const wchar_t* dir, const wchar_t* section, TraceLogFn log) {
    g_log = log;
    wchar_t ini[MAX_PATH]; swprintf_s(ini, L"%s\\worldmod.ini", dir);
    UINT kb = GetPrivateProfileIntW(section, L"thread_stack_kb", 1024, ini);
    auto base = (uint8_t*)GetModuleHandleA(nullptr);
    auto nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    if (!kb) { log("thread stacks: thread_stack_kb=0, every thread reserves the exe's %lu KB", nt->OptionalHeader.SizeOfStackReserve >> 10); return false; }
    g_reserve = (SIZE_T)kb << 10;
    if (nt->OptionalHeader.SizeOfStackReserve <= g_reserve) return false;   // the stock exe: nothing to save
    g_exeLo = (uintptr_t)base; g_exeHi = g_exeLo + nt->OptionalHeader.SizeOfImage;
    uint8_t* site = (uint8_t*)GetProcAddress(GetModuleHandleA("kernelbase.dll"), "CreateRemoteThreadEx");
    if (!site) { log("thread stacks: KernelBase!CreateRemoteThreadEx not found, not hooked"); return false; }
    // the first instruction must be 5 bytes that run anywhere: push imm32, or mov edi,edi; push ebp; mov ebp,esp
    static const uint8_t hot[5] = { 0x8b, 0xff, 0x55, 0x8b, 0xec };
    if (site[0] != 0x68 && memcmp(site, hot, 5)) {
        log("thread stacks: unexpected code at CreateRemoteThreadEx (%02x %02x %02x %02x %02x), not hooked", site[0], site[1], site[2], site[3], site[4]);
        return false;
    }
    uint8_t* tramp = (uint8_t*)VirtualAlloc(nullptr, 16, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;
    memcpy(tramp, site, 5);
    tramp[5] = 0xE9; *(int32_t*)(tramp + 6) = (int32_t)((uintptr_t)(site + 5) - (uintptr_t)(tramp + 10));
    g_orig = (CreateRemoteThreadExFn)tramp;
    DWORD old; VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    site[0] = 0xE9; *(int32_t*)(site + 1) = (int32_t)((uintptr_t)hook - (uintptr_t)(site + 5));
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    log("thread stacks: threads not started in the exe reserve %u KB (the exe's default is %lu KB)", kb, nt->OptionalHeader.SizeOfStackReserve >> 10);
    return true;
}
