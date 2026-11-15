#pragma once
#include "d3dtrace.h"

// Prepare reads UTF-8 definitions and checks every instruction before any game code is written.
bool skillsPrepare(const wchar_t* path, TraceLogFn log);
bool skillsInstall();
