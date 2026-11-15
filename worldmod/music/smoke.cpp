#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
static volatile LONG callbacks;
static void CALLBACK Timer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) { InterlockedIncrement(&callbacks); }
int main() {
    HMODULE dll = LoadLibraryW(L".\\winmm.dll");
    if (!dll) return 1;
    auto begin = reinterpret_cast<UINT (WINAPI*)(UINT)>(GetProcAddress(dll, "timeBeginPeriod"));
    auto end = reinterpret_cast<UINT (WINAPI*)(UINT)>(GetProcAddress(dll, "timeEndPeriod"));
    auto now = reinterpret_cast<DWORD (WINAPI*)()>(GetProcAddress(dll, "timeGetTime"));
    auto set = reinterpret_cast<UINT (WINAPI*)(UINT, UINT, void*, DWORD_PTR, UINT)>(GetProcAddress(dll, "timeSetEvent"));
    auto kill = reinterpret_cast<UINT (WINAPI*)(UINT)>(GetProcAddress(dll, "timeKillEvent"));
    if (!begin || !end || !now || !set || !kill || begin(1)) return 2;
    DWORD before = now();
    UINT timer = set(10, 1, reinterpret_cast<void*>(Timer), 0, 1);
    if (!timer) return 3;
    Sleep(150);
    UINT stopped = kill(timer);
    DWORD elapsed = now() - before;
    UINT ended = end(1);
    printf("forwarding: elapsed=%lu ms, callbacks=%ld, stop=%u, end=%u\n", elapsed, callbacks, stopped, ended);
    return elapsed >= 100 && callbacks > 0 && !stopped && !ended ? 0 : 4;
}
