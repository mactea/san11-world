#define WIN32_LEAN_AND_MEAN
#include "../src/graphics_lifecycle_trace.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <initializer_list>

static GraphicsLifecycleTrace trace;
static int settingsCalls=0, initializeCalls=0, postCalls=0, logs=0;
static bool arguments=true, fault=false;
static HRESULT result=S_OK;
static BOOL postResult=FALSE;
static void* self=(void*)0x1234;
static HWND window=(HWND)0x5678;
static void* callback=(void*)0x9abc;
static const char* section=(const char*)1; // Invalid to dereference; diagnostics must not read it.
static void logTrace(const char* format, ...) {
    char line[512];va_list args;va_start(args,format);vsprintf_s(line,format,args);va_end(args);
    ++logs;SetLastError(999); // Logging must not leak its own last-error into the game.
}
static HRESULT __fastcall originalInitialize(void* object,void*,HWND target,void* cb) {
    ++initializeCalls;
    arguments &= object==self && target==window && cb==callback && GetLastError()==123;
    if(fault)RaiseException(0xe0424243,0,0,nullptr);
    SetLastError(456);return result;
}
static void __fastcall originalSettings(void* object,void*,UINT action,const char* text) {
    ++settingsCalls;
    arguments &= object==self && action==17 && text==section && GetLastError()==123;
    HRESULT hr=trace.initialize(object,window,callback,(void*)0x43d8be);
    arguments &= hr==result && GetLastError()==456;
    SetLastError(789);
}
static BOOL WINAPI originalPost(HWND target,UINT message,WPARAM w,LPARAM l) {
    ++postCalls;
    arguments &= target==window && (message==WM_CLOSE || message==WM_NULL) && w==7 && l==9 && GetLastError()==123;
    SetLastError(321);return postResult;
}
static bool catchException() {
    __try {trace.settings(self,17,section);}
    __except(GetExceptionCode()==0xe0424243 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {return true;}
    return false;
}
#define CHECK(condition) do {if(!(condition)) {printf("FAIL line %d: %s\n",__LINE__,#condition);return 1;}} while(0)
int main() {
    trace.settingsCall=originalSettings;trace.initializeCall=originalInitialize;trace.postCall=originalPost;trace.log=logTrace;
    for(HRESULT hr : {S_OK, (HRESULT)0x82000002, (HRESULT)0x82000003, (HRESULT)0x82000004}) {
        result=hr;SetLastError(123);trace.settings(self,17,section);
        CHECK(arguments && GetLastError()==789);
    }
    CHECK(settingsCalls==4 && initializeCalls==4 && logs==16);
    for(UINT message : {WM_NULL, WM_CLOSE}) {
        SetLastError(123);CHECK(!trace.post(window,message,7,9,(void*)0x43d933));
        CHECK(arguments && GetLastError()==321);
    }
    CHECK(postCalls==2 && logs==17);
    postResult=TRUE;SetLastError(123);CHECK(trace.post(window,WM_CLOSE,7,9,(void*)0x43d933));
    CHECK(arguments && GetLastError()==321 && postCalls==3 && logs==18);
    fault=true;SetLastError(123);CHECK(catchException());
    CHECK(arguments && settingsCalls==5 && initializeCalls==5 && logs==20);
    // No end event after an exception; no synthetic success or retry.
    trace.log=nullptr;fault=false;SetLastError(123);trace.settings(self,17,section);
    CHECK(arguments && GetLastError()==789 && logs==20);
    puts("PASS graphics lifecycle tracing: nested calls, exact failure returns, arguments, last-error, pointer-only logging, exception passthrough");
    return 0;
}
