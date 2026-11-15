#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include <cstring>

// Standalone WinMM proxy: keeps the existing worldmod DLL and resource archive intact.
static HMODULE module;
static wchar_t root[MAX_PATH];
static bool installed, active;
static DWORD owner, started, polled;
static int season = 3, last;
static std::vector<unsigned char> track;
static struct { DWORD offset, size; } records[2];
static int customCount = 1;
static bool heroOnly;
using Play = int (__thiscall*)(void*, int, int, int, int, float);
using Resource = void* (__thiscall*)(void*, int, int);
static Play original;
static void* const manager = reinterpret_cast<void*>(0x09119810);
#ifdef MUSIC_UNIT_TEST
static int testCurrentTrack = -1;
#endif
static int CurrentTrack(void* self) {
#ifdef MUSIC_UNIT_TEST
    return testCurrentTrack;
#else
    return reinterpret_cast<int (__thiscall*)(void*)>(0x4cfe10)(self);
#endif
}
#ifdef MUSIC_TEST
extern "C" __declspec(dllexport) volatile LONG MusicTestRequest = -999;
static DWORD testOwner;
#endif

static void Log(const char* message, int value = 0) {
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%sworldmusic.log", root);
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"ab") == 0) {
        fprintf(file, "%lu %s %d\n", GetTickCount(), message, value);
        fclose(file);
    }
}

static int NextTrack() {
    if (last < 30) return 30 + customCount - 1;
    return last > 30 ? last - 1 : season;
}

static int __fastcall PlayHook(void* self, void*, int id, int loop, int fade, int flag, float volume) {
#ifdef MUSIC_TEST
    testOwner = GetCurrentThreadId();
#endif
    if (heroOnly && ((id >= 3 && id <= 10) || (id >= 30 && id < 30 + customCount))) {
        active = false;
        if (CurrentTrack(self) == 31) return id;
        Log("fixed map music", 31);
        return original(self, 31, 1, fade, flag, volume);
    }
    if (id >= 3 && id <= 10) {
        season = id;
#ifdef MUSIC_TEST
        if (!active) id = 30; // Integration build starts the added track immediately.
#endif
        owner = GetCurrentThreadId();
        // Map music requests recur frequently, including the wartime map cues.
        // Keep the playing track and its completion timer until it actually ends.
        if (active && (last >= 30 || last == id) && CurrentTrack(self) == last)
            return id;
        if (active && GetTickCount() - started >= 1500 &&
            CurrentTrack(self) == -1)
            id = NextTrack();
        active = true;
        last = id;
        started = GetTickCount();
        loop = 0;
    } else if (id >= 30 && id < 30 + customCount) {
        active = true;
        owner = GetCurrentThreadId();
        last = id;
        started = GetTickCount();
        loop = 0;
    } else {
        active = false;
    }
    Log("game music", id);
    return original(self, id, loop, fade, flag, volume);
}

static void* __fastcall ResourceHook(void* self, void*, int archive, int id) {
    if (id >= 2237 + 30 && id < 2237 + 30 + customCount) {
        Log("custom resource loaded", id);
        return &records[id - 2237 - 30];
    }
    return reinterpret_cast<Resource>(0x46dae0)(self, archive, id);
}

static bool Write(void* address, const void* data, size_t size) {
    DWORD before;
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &before)) return false;
    memcpy(address, data, size);
    FlushInstructionCache(GetCurrentProcess(), address, size);
    DWORD ignored;
    VirtualProtect(address, size, before, &ignored);
    return true;
}

static void Branch(unsigned char* output, unsigned char opcode, DWORD from, void* to) {
    output[0] = opcode;
    *reinterpret_cast<DWORD*>(output + 1) = reinterpret_cast<DWORD>(to) - from - 5;
}

static void Install() {
    wchar_t exe[MAX_PATH], ini[MAX_PATH], relative[MAX_PATH], path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t* name = wcsrchr(exe, L'\\');
    if (!name || _wcsicmp(name + 1, L"san11pk_world.exe")) return;
    GetModuleFileNameW(module, root, MAX_PATH);
    wchar_t* slash = wcsrchr(root, L'\\');
    if (!slash) return;
    slash[1] = 0;
    swprintf_s(ini, L"%sworldmusic.ini", root);
    if (!GetPrivateProfileIntW(L"music", L"enabled", 1, ini)) return;
    for (int index = 0; index < 2; ++index) {
        GetPrivateProfileStringW(L"music", index ? L"track2" : L"track",
            index ? L"" : L"Media\\custom_music\\langshu.kovs", relative, MAX_PATH, ini);
        if (!relative[0]) break;
        swprintf_s(path, L"%s%s", root, relative);
        FILE* file = nullptr;
        if (_wfopen_s(&file, path, L"rb")) { Log("track open failed"); return; }
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);
        if (size <= 288 || size > 200 * 1024 * 1024) { fclose(file); Log("invalid track size"); return; }
        track.resize(size);
        size_t read = fread(track.data(), 1, size, file);
        fclose(file);
        if (read != static_cast<size_t>(size) || memcmp(track.data(), "KOVS", 4) ||
            *reinterpret_cast<DWORD*>(&track[4]) != size - 32) { Log("invalid KOVS"); return; }
        auto& record = records[index];
        record.offset = GetPrivateProfileIntW(L"music", index ? L"archive_offset2" : L"archive_offset", 0, ini);
        record.size = size;
        swprintf_s(path, L"%sMedia\\san11pkres.bin", root);
        if (!record.offset || _wfopen_s(&file, path, L"rb")) { Log("missing appended resource"); return; }
        fseek(file, record.offset, SEEK_SET);
        std::vector<unsigned char> archived(size);
        read = fread(archived.data(), 1, size, file);
        fclose(file);
        if (read != static_cast<size_t>(size) || archived != track) { Log("appended resource mismatch"); return; }
        track.clear();
        track.shrink_to_fit();
        customCount = index + 1;
    }
    heroOnly = customCount == 2 && GetPrivateProfileIntW(L"music", L"hero_only", 0, ini) != 0;
    const unsigned char prologue[] = {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x34};
    const unsigned char resourceCall[] = {0xe8, 0xdb, 0xe0, 0xf9, 0xff};
    const unsigned char bound[] = {0x83, 0xff, 0x1e};
    if (memcmp(reinterpret_cast<void*>(0x4d04f0), prologue, 6) ||
        memcmp(reinterpret_cast<void*>(0x4cfa00), resourceCall, 5) ||
        memcmp(reinterpret_cast<void*>(0x4cfc57), bound, 3)) {
        Log("unsupported game binary; no hooks installed"); return;
    }
    auto trampoline = static_cast<unsigned char*>(VirtualAlloc(nullptr, 11, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!trampoline) return;
    memcpy(trampoline, prologue, 6);
    Branch(trampoline + 6, 0xe9, reinterpret_cast<DWORD>(trampoline + 6), reinterpret_cast<void*>(0x4d04f6));
    FlushInstructionCache(GetCurrentProcess(), trampoline, 11);
    original = reinterpret_cast<Play>(trampoline);
    unsigned char hook[6] = {0, 0, 0, 0, 0, 0x90}, call[5], count = 30 + customCount;
    Branch(hook, 0xe9, 0x4d04f0, reinterpret_cast<void*>(PlayHook));
    Branch(call, 0xe8, 0x4cfa00, reinterpret_cast<void*>(ResourceHook));
    if (!Write(reinterpret_cast<void*>(0x4cfc59), &count, 1) ||
        !Write(reinterpret_cast<void*>(0x4cfa00), call, 5) ||
        !Write(reinterpret_cast<void*>(0x4d04f0), hook, 6)) {
        Write(reinterpret_cast<void*>(0x4cfc59), bound + 2, 1);
        Write(reinterpret_cast<void*>(0x4cfa00), resourceCall, 5);
        Write(reinterpret_cast<void*>(0x4d04f0), prologue, 6);
        Log("hook installation failed"); return;
    }
    installed = true;
    Log("installed playlist; custom tracks", customCount);
}

static void Tick() {
#ifdef MUSIC_TEST
    if (installed && testOwner == GetCurrentThreadId() && MusicTestRequest != -999) {
        int request = InterlockedExchange(&MusicTestRequest, -999);
        if (request == -998)
            reinterpret_cast<int (__thiscall*)(void*, int)>(0x4cfda0)(manager, 0);
        else PlayHook(manager, nullptr, request, 1, 0, 0, -1.0f);
    }
#endif
    if (!installed || !active || owner != GetCurrentThreadId()) return;
    DWORD now = GetTickCount();
    if (now - polled < 250 || now - started < 1500) return;
    polled = now;
    if (!*reinterpret_cast<int*>(0x09119810 + 0x58)) return;
    DWORD foreground = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground);
#ifndef MUSIC_TEST
    if (foreground != GetCurrentProcessId()) return;
#endif
    int current = CurrentTrack(manager);
    if (current != -1) return;
    last = NextTrack();
    started = now;
    Log("playlist next", last);
    original(manager, last, 0, 1000, 0, -1.0f);
}

// The generated export thunks preserve every register and forward the original ABI.
#include "exports.inc"
static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK LoadReal(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\winmm.dll");
    HMODULE real = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!real) return FALSE;
    for (unsigned i = 0; i < exportCount; ++i) {
        exports[i] = GetProcAddress(real, MAKEINTRESOURCEA(ordinals[i]));
        if (!exports[i]) return FALSE;
    }
    Install();
    return TRUE;
}
extern "C" void EnsureLoaded() {
    if (!InitOnceExecuteOnce(&once, LoadReal, nullptr, nullptr))
        TerminateProcess(GetCurrentProcess(), ERROR_DLL_INIT_FAILED);
}
extern "C" DWORD WINAPI MusicTimeGetTime() {
    EnsureLoaded();
    DWORD result = reinterpret_cast<DWORD (WINAPI*)()>(exports[timeIndex])();
    static thread_local bool busy;
    if (!busy) { busy = true; Tick(); busy = false; }
    return result;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { module = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
