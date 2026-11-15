#pragma once
#include <windows.h>

// Cold-path diagnostic only. Keep arguments, results, last-error and exceptions intact.
struct GraphicsLifecycleTrace {
    using SettingsFn = void (__fastcall*)(void*, void*, UINT, const char*);
    using InitializeFn = HRESULT (__fastcall*)(void*, void*, HWND, void*);
    using PostFn = BOOL (WINAPI*)(HWND, UINT, WPARAM, LPARAM);
    using LogFn = void (*)(const char*, ...);
    SettingsFn settingsCall = nullptr;
    InitializeFn initializeCall = nullptr;
    PostFn postCall = nullptr;
    LogFn log = nullptr;

    template<class... Args> void record(const char* format, Args... args) {
        DWORD error = GetLastError();
        if (log) log(format, args...);
        SetLastError(error);
    }
    void settings(void* self, UINT action, const char* section) {
        // Log the pointer only: system message strings need not remain valid outside the call.
        record("graphics lifetime: settings begin self=%p action=%u section=%p thread=%lu",
            self, action, (const void*)section, GetCurrentThreadId());
        settingsCall(self, nullptr, action, section);
        record("graphics lifetime: settings end self=%p thread=%lu", self, GetCurrentThreadId());
    }
    HRESULT initialize(void* self, HWND window, void* callback, void* caller) {
        record("graphics lifetime: initialize begin self=%p window=%p callback=%p caller=%p thread=%lu",
            self, window, callback, caller, GetCurrentThreadId());
        HRESULT hr = initializeCall(self, nullptr, window, callback);
        record("graphics lifetime: initialize end self=%p hr=%08lx caller=%p thread=%lu",
            self, hr, caller, GetCurrentThreadId());
        return hr;
    }
    BOOL post(HWND window, UINT message, WPARAM w, LPARAM l, void* caller) {
        BOOL result = postCall(window, message, w, l);
        if (message == WM_CLOSE)
            record("graphics lifetime: post close window=%p w=%#lx l=%#lx caller=%p result=%d error=%lu thread=%lu",
                window, (unsigned long)w, (unsigned long)l, caller, result, GetLastError(), GetCurrentThreadId());
        return result;
    }
};
