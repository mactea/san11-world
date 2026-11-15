@echo off
setlocal
if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto novs
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto novs
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
:build
cd /d "%~dp0.."
if not exist worldmod\build mkdir worldmod\build
cl /nologo /utf-8 /O2 /MT /W4 /EHsc /std:c++17 tools\render_crash_watch.cpp /Foworldmod\build\render_crash_watch.obj /Feworldmod\build\render_crash_watch.exe /link dbghelp.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /W4 worldmod\tests\crash_watch_fixture.cpp /Foworldmod\build\crash_watch_fixture.obj /Feworldmod\build\crash_watch_fixture.exe /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /W4 /EHsc /std:c++17 worldmod\tests\scene_allocator_trace_test.cpp /Foworldmod\build\scene_allocator_trace_test.obj /Feworldmod\build\scene_allocator_trace_test.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /W4 /EHsc /std:c++17 worldmod\tests\graphics_lifecycle_trace_test.cpp /Foworldmod\build\graphics_lifecycle_trace_test.obj /Feworldmod\build\graphics_lifecycle_trace_test.exe /link user32.lib
exit /b %errorlevel%
:novs
echo Visual Studio x86 C++ build tools not found.
exit /b 1
