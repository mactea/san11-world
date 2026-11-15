#pragma once
#include "d3dtrace.h"

constexpr int SKILL_MERCHANT = 147;

// Prepare reads UTF-8 definitions and checks every instruction before any game code is written.
bool skillsPrepare(const wchar_t* path, TraceLogFn log);
bool skillsInstall();
// Invalidate queued skill provenance after a world read, retaining the CSV file.
void skillsTelemetryStream(int mode,int kind);
// Read the active M12 input tables without evaluating a path or sea-cost callback.
struct SkillSteerCosts {
    const void* actor=nullptr;
    int weapon=-1, normal=-1, steering=-1;
};
using SkillSteerDescribe = bool (*)(const unsigned char*,int,SkillSteerCosts&);
void skillsSetSteeringDescribe(SkillSteerDescribe describe);
