// Load the real DLL against an inert copy of the game's address space. No game entry point runs.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    int count = atoi(argv[2]);
    // Leave the gap between code/data and the large BSS free for the harness's own heap and stack.
    const uintptr_t starts[] = {0x400000, 0x3260000};
    const size_t lengths[] = {0x500000, 0x9c83000 - 0x3260000};
    for (int i = 0; i < 2; i++) {
        if (VirtualAlloc((void*)starts[i], lengths[i], MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE) != (void*)starts[i]) {
            printf("image allocation at %p failed %lu, main %p\n", (void*)starts[i], GetLastError(), GetModuleHandleW(nullptr));
            for (uintptr_t at = starts[i]; at < starts[i] + lengths[i]; ) {
                MEMORY_BASIC_INFORMATION mi;
                if (!VirtualQuery((void*)at, &mi, sizeof mi)) break;
                if (mi.State != MEM_FREE) printf("occupied %p size %zx allocation %p type %lx\n", mi.BaseAddress, mi.RegionSize, mi.AllocationBase, mi.Type);
                at = (uintptr_t)mi.BaseAddress + mi.RegionSize;
            }
            return 5;
        }
    }
    FILE* f = nullptr;
    fopen_s(&f, argv[1], "rb");
    if (!f) return 3;
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    if (size != 0x9883000) { fclose(f); return 4; }
    for (int i = 0; i < 2; i++) {
        fseek(f, (long)(starts[i] - 0x400000), SEEK_SET);
        if (fread((void*)starts[i], 1, lengths[i], f) != lengths[i]) { fclose(f); return 4; }
    }
    fclose(f);
    auto main = (uint8_t*)GetModuleHandleW(nullptr);
    auto nt = (IMAGE_NT_HEADERS*)(main + ((IMAGE_DOS_HEADER*)main)->e_lfanew);
    DWORD old;
    VirtualProtect(nt, sizeof(*nt), PAGE_READWRITE, &old);
    nt->FileHeader.TimeDateStamp = 1168334429;
    nt->OptionalHeader.SizeOfImage = 0x9883000;
    nt->OptionalHeader.SizeOfStackReserve = 50331648; // match the world launcher header; no world-frame code is executed
    VirtualProtect(nt, sizeof(*nt), old, &old);
    if (!LoadLibraryW(L".\\d3d9.dll")) { printf("LoadLibrary error %lu\n", GetLastError()); return 6; }
    auto patched = *(uint32_t*)0x48a458;
    if (patched != (uint32_t)(count - 1)) { printf("bound %u != %d\n", patched, count - 1); return 7; }
    auto meta = (uint8_t*)(*(uint32_t*)0x48a46c - 3);
    auto manager = *(void**)0x430ee1;
    ((void(__cdecl*)())0x73be20)();
    ((void(__thiscall*)(void*))0x46dd60)(manager);
    uint8_t record[0x190] = {};
    *(int*)(record + 0x3c) = count - 1;
    meta[(count - 1) * 4 + 3] = 2;
    int got = ((int(__thiscall*)(void*))0x48a450)(record);
    if (got != count - 1) return 8;
    printf("native DLL: capacity %d, high portrait %d, metadata %p, manager %p PASS\n", count, got, meta, manager);
    return 0;
}
