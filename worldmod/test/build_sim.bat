@echo off
rem Builds the offline economy simulation (trade_sim.exe) from the same trade_model.cpp the DLL uses,
rem and the trade panel test (trade_panel_test.exe: hit testing, numbers, renders; M13 phase 2).
if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1
set "VSDIR="
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
for /f "usebackq delims=" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR exit /b 1
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul
:build
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_sim.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\trade_sim.exe || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_panel_test.cpp ..\src\trade_panel.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\trade_panel_test.exe /link user32.lib gdi32.lib || exit /b 1
exit /b 0
