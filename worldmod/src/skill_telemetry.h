#pragma once
#include <stdint.h>
#include "d3dtrace.h"

namespace skilltelemetry {
struct Actor {
    int person=-1, force=-1, ruler=-1, city=-1, year=0, month=0, day=0;
    char name[16]={}, rulerName[16]={}; // native Big5; CSV is UTF-8
};
using Resolve = bool (*)(const void*, Actor&);
struct Target {
    int person=-1, force=-1, city=-1, base=-1, unit=-1, equipment=-1, actorUnit=-1;
    int x=-1, y=-1, terrain=-1;
    const char* context=nullptr; // optional UTF-8 JSON, consumed synchronously
};
bool open(const wchar_t* path, Resolve resolve, TraceLogFn log);
void close(); // call from normal shutdown/tests, never wait inside DllMain
bool enabled();
// Pending provenance cannot cross log sessions or a loaded world replacement.
uint64_t contextGeneration();
void invalidateContext();
// Matching a predicate is not proof of a gameplay effect.
void matched(const void* person, int owned, int effect, uintptr_t caller,
             const char* scene="unclassified_query", const char* event="skill_predicate");
void matched(const void* person,int owned,int effect,uintptr_t caller,
             const char* scene,const char* event,const Target& target);
void applied(const void* person, int owned, int effect, const char* scene,
             const char* event, int targetPerson, int targetForce, int targetCity,
             const char* metric, int64_t before, int64_t after, uintptr_t caller=0,
             int targetBase=-1, int targetUnit=-1, int equipment=-1, int actorUnit=-1);
// Actual numerical change with context, without inventing a counterfactual.
void applied(const void* person,int owned,int effect,const char* scene,const char* event,
             const char* metric,int64_t before,int64_t after,uintptr_t caller,const Target& target);
// A committed categorical effect with no measured numerical counterfactual.
void appliedEvent(const void* person,int owned,int effect,const char* scene,const char* event,
                  uintptr_t caller,const Target& target);
// Preserve actual state before/after; keep the no-skill counterfactual in separate columns.
void appliedBonus(const void* person,int owned,int effect,const char* scene,const char* event,
                  const char* metric,int64_t before,int64_t after,int64_t baselineAfter,uintptr_t caller,
                  const Target& target=Target{});
}
