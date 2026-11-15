#define WIN32_LEAN_AND_MEAN
#include "../src/scene_allocator_trace.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int initialized=0, beforeDelete=0, deleted=0, late=0, calls=0;
static void logTrace(const char* format, ...) {
    char text[512];va_list args;va_start(args,format);vsprintf_s(text,format,args);va_end(args);
    if(strstr(text,": initialized "))++initialized;
    if(strstr(text,": before delete "))++beforeDelete;
    if(strstr(text,": deleted "))++deleted;
    if(strstr(text,": enter after delete "))++late;
}
static void WINAPI faultingEnter(LPCRITICAL_SECTION) {
    ++calls;RaiseException(0xe0424243,0,0,nullptr);
}
static bool catchOriginalException(SceneAllocatorTrace* trace,LPCRITICAL_SECTION cs) {
    __try {trace->enter(cs,nullptr);}
    __except(GetExceptionCode()==0xe0424243 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {return true;}
    return false;
}
#define CHECK(condition) do {if(!(condition)) {printf("FAIL line %d: %s\n",__LINE__,#condition);return 1;}} while(0)
int main() {
    CRITICAL_SECTION target={},other={};
    SceneAllocatorTrace trace;trace.target=&target;trace.log=logTrace;
    trace.initialize(&other,nullptr);trace.enter(&other,nullptr);
    CHECK(other.RecursionCount==1);LeaveCriticalSection(&other);trace.erase(&other,nullptr);
    CHECK(initialized==0 && beforeDelete==0 && deleted==0 && late==0 && trace.deleted==0);
    trace.initialize(&target,nullptr);trace.enter(&target,nullptr);
    CHECK(target.RecursionCount==1);LeaveCriticalSection(&target);trace.erase(&target,nullptr);
    CHECK(initialized==1 && beforeDelete==1 && deleted==1 && trace.deleted==1);
    trace.initialize(&other,nullptr);trace.enter(&other,nullptr);
    CHECK(other.RecursionCount==1);LeaveCriticalSection(&other);trace.erase(&other,nullptr);
    CHECK(initialized==1 && beforeDelete==1 && deleted==1 && late==0 && trace.deleted==1);
    // Deterministic downstream fault: observation must never swallow it or skip the call.
    trace.enterCall=faultingEnter;
    for(int i=0;i<12;++i)CHECK(catchOriginalException(&trace,&target));
    CHECK(calls==12 && late==8 && trace.lateEntries==12);
    trace.enterCall=EnterCriticalSection;trace.initialize(&target,nullptr);
    CHECK(trace.deleted==0 && trace.lateEntries==0);
    trace.enter(&target,nullptr);CHECK(target.RecursionCount==1);LeaveCriticalSection(&target);
    CHECK(initialized==2 && late==8);trace.erase(&target,nullptr);
    CHECK(beforeDelete==2 && deleted==2);
    puts("PASS scene allocator tracing: passthrough, isolation, reinitialization, bounded logs, original exceptions");
    return 0;
}
