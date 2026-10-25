#pragma once
// Keeps listed DLLs out of the game (dllblock.cpp).
#include "d3dtrace.h"

// Baidu Pinyin's UIPFull.dll, loaded into every GUI process, throws C++ exceptions inside the game's
// message dispatch (DispatchMessageA at 0x439f53); one that the game does not catch ends it with 0xc0000409 /
// 0xc0000028. Hooks KernelBase!LoadLibraryExW (LoadLibrary* and COM in-process servers go through it) and
// refuses paths containing one of the block_dlls substrings (worldmod.ini, comma separated, case-insensitive,
// default "BaiduPinyin"; empty = off). Other input methods still load. Returns true when hooked.
bool dllBlockInstall(const wchar_t* dir, const wchar_t* section, TraceLogFn log);
