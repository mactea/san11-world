// x86 diagnostic launcher: preserves unhandled exceptions and captures the stopped
// process externally. No hooks, registry changes or debugger memory writes.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <filesystem>
#include <stdio.h>
#include <string>

static bool writeDump(HANDLE process,const DEBUG_EVENT& event,const std::filesystem::path& path) {
    HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,event.dwThreadId);
    if(!thread)return false;
    CONTEXT context={};context.ContextFlags=CONTEXT_ALL;
    BOOL captured=GetThreadContext(thread,&context);CloseHandle(thread);
    if(!captured)return false;
    EXCEPTION_RECORD record=event.u.Exception.ExceptionRecord;
    EXCEPTION_POINTERS pointers={&record,&context};
    MINIDUMP_EXCEPTION_INFORMATION info={event.dwThreadId,&pointers,FALSE};
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BOOL ok=MiniDumpWriteDump(process,event.dwProcessId,file,
        MINIDUMP_TYPE(MiniDumpWithFullMemory|MiniDumpWithThreadInfo|MiniDumpWithUnloadedModules),&info,nullptr,nullptr);
    DWORD error=GetLastError();CloseHandle(file);SetLastError(error);
    return ok!=FALSE;
}

int wmain(int argc,wchar_t** argv) {
    if(argc!=3){fwprintf(stderr,L"Usage: render_crash_watch.exe <x86-exe> <new-output-directory>\n");return 2;}
    auto exe=std::filesystem::absolute(argv[1]);
    auto output=std::filesystem::absolute(argv[2]);
    if(!std::filesystem::is_regular_file(exe) || std::filesystem::exists(output))return 2;
    if(!std::filesystem::create_directories(output))return 2;
    std::wstring command=L"\""+exe.wstring()+L"\"";
    STARTUPINFOW startup={sizeof(startup)};
    PROCESS_INFORMATION process={};
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,DEBUG_ONLY_THIS_PROCESS,
        nullptr,exe.parent_path().c_str(),&startup,&process)) {
        printf("LAUNCH_FAILED error=%lu\n",GetLastError());return 2;
    }
    // A stopped/disconnected watcher must not kill the user's test game.
    if(!DebugSetProcessKillOnExit(FALSE)) {
        DWORD error=GetLastError();DebugActiveProcessStop(process.dwProcessId);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
        printf("SETUP_FAILED error=%lu\n",error);return 2;
    }
    printf("START pid=%lu exe=%ls\n",process.dwProcessId,exe.c_str());fflush(stdout);
    bool initialBreakpoint=true,failed=false;
    int result=0;
    for(;;) {
        DEBUG_EVENT event={};
        if(!WaitForDebugEvent(&event,1000)) {
            if(GetLastError()==ERROR_SEM_TIMEOUT)continue;
            printf("WAIT_FAILED error=%lu\n",GetLastError());result=2;break;
        }
        DWORD status=DBG_CONTINUE;
        if(event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT && event.u.CreateProcessInfo.hFile)
            CloseHandle(event.u.CreateProcessInfo.hFile);
        if(event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT && event.u.LoadDll.hFile)
            CloseHandle(event.u.LoadDll.hFile);
        if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT) {
            auto& exception=event.u.Exception;
            status=DBG_EXCEPTION_NOT_HANDLED;
            if(initialBreakpoint && exception.dwFirstChance && exception.ExceptionRecord.ExceptionCode==EXCEPTION_BREAKPOINT) {
                initialBreakpoint=false;status=DBG_CONTINUE;
            } else if(!exception.dwFirstChance) {
                auto path=output/(L"crash-"+std::to_wstring(event.dwThreadId)+L".dmp");
                bool ok=writeDump(process.hProcess,event,path);
                printf("UNHANDLED code=%08lx address=%p thread=%lu dump=%ls success=%d error=%lu\n",
                    exception.ExceptionRecord.ExceptionCode,exception.ExceptionRecord.ExceptionAddress,
                    event.dwThreadId,path.c_str(),ok,ok?0:GetLastError());
                fflush(stdout);failed=true;result=ok?1:2;
            }
        }
        bool exited=event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT;
        if(exited) {
            printf("EXIT code=%08lx\n",event.u.ExitProcess.dwExitCode);fflush(stdout);
            if(event.u.ExitProcess.dwExitCode && !failed)result=1;
        }
        if(!ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status)) {
            printf("CONTINUE_FAILED error=%lu\n",GetLastError());result=2;break;
        }
        if(exited)break;
    }
    DebugActiveProcessStop(process.dwProcessId);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    return result;
}
