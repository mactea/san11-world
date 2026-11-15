@echo off
if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR exit /b 1
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul
:build
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 research_progress_test.cpp /Fo..\build\ /Fe..\build\research_progress_test.exe /link /BASE:0x10000000 /DYNAMICBASE:NO
exit /b %errorlevel%
