#pragma once
#include <stdio.h>
#include "bases.h"
#include "units.h"
struct GarrisonSetup {
    const wchar_t* dir;
    const wchar_t* section;
    const Bases* bases;
    const Units* units;
    const uint8_t* hex20;
    int width,height,forces;
    bool write;
};
bool garrisonInstall(const GarrisonSetup& setup,TraceLogFn log);
void garrisonReset();
// Read-only diagnostics of evaluations already made by the AI logic thread.
void garrisonDump(FILE* out);
