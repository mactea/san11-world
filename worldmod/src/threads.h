#pragma once
// Stack reservation of threads other than the game's own (threads.cpp).
#include "d3dtrace.h"

// san11pk_world.exe reserves 48 MB of stack (make_world_exe.py) for the main thread's W*H-sized frames, and
// Windows gives that default to every thread created without an explicit reservation: the D3D drivers, sound,
// input method and thread pool threads (40+) then take 1.5 GB of the 4 GB address space and the game runs
// out of it after some play (bad_alloc -> 0xe06d7363 / 0xc0000409). Hooks KernelBase!CreateRemoteThreadEx so
// that a thread whose start routine is outside the exe gets thread_stack_kb (worldmod.ini, default 1024,
// 0 = off) reserved instead. Threads of the exe itself keep the default. Returns true when hooked.
bool threadStacksInstall(const wchar_t* dir, const wchar_t* section, TraceLogFn log);
