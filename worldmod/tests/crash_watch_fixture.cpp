#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int) {
    // The watcher must let the application's handler process first-chance faults.
    bool handled=false;
    __try { RaiseException(0xe0424242,0,0,nullptr); }
    __except(EXCEPTION_EXECUTE_HANDLER) {handled=true;}
    if(!handled)return 3;
    wchar_t crash[2];
    if(GetEnvironmentVariableW(L"WORLDMOD_TEST_WAIT",crash,2))Sleep(3000);
    if(GetEnvironmentVariableW(L"WORLDMOD_TEST_CRASH",crash,2))
        *reinterpret_cast<volatile DWORD*>(0x14)=1;
    return 0;
}
