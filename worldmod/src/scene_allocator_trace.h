#pragma once
#include <windows.h>

// Opt-in observation of one game allocator. Never skips or repairs a lock call.
struct SceneAllocatorTrace {
    using LockFn = void (WINAPI*)(LPCRITICAL_SECTION);
    using LogFn = void (*)(const char*, ...);
    LPCRITICAL_SECTION target = nullptr;
    LockFn initializeCall = InitializeCriticalSection;
    LockFn deleteCall = DeleteCriticalSection;
    LockFn enterCall = EnterCriticalSection;
    LogFn log = nullptr;
    volatile LONG deleted = 0, lateEntries = 0;

    void record(const char* event, void* caller) {
        if (log) log("scene allocator: %s cs=%p caller=%p thread=%lu debug=%p lock=%ld recursion=%ld owner=%p semaphore=%p spin=%#lx",
            event, target, caller, GetCurrentThreadId(), target->DebugInfo,
            target->LockCount, target->RecursionCount, target->OwningThread,
            target->LockSemaphore, (unsigned long)target->SpinCount);
    }
    void initialize(LPCRITICAL_SECTION cs, void* caller) {
        initializeCall(cs);
        if (cs == target) {
            InterlockedExchange(&deleted, 0);
            InterlockedExchange(&lateEntries, 0);
            record("initialized", caller);
        }
    }
    void erase(LPCRITICAL_SECTION cs, void* caller) {
        if (cs == target) record("before delete", caller);
        deleteCall(cs);
        if (cs == target) {
            InterlockedExchange(&deleted, 1);
            record("deleted", caller);
        }
    }
    void enter(LPCRITICAL_SECTION cs, void* caller) {
        if (cs == target && InterlockedCompareExchange(&deleted, 0, 0) &&
            InterlockedIncrement(&lateEntries) <= 8) record("enter after delete", caller);
        enterCall(cs);
    }
};
