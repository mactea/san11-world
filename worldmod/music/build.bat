@echo off
setlocal
cd /d "%~dp0"
python generate_exports.py
if errorlevel 1 exit /b 1
if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR exit /b 1
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul
:build
if not exist build mkdir build
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 %MUSIC_TEST_FLAGS% /LD music.cpp /Fobuild\ /Febuild\winmm.dll /link /DEF:winmm.def user32.lib kernel32.lib
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 smoke.cpp /Fobuild\ /Febuild\music_smoke.exe
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 rotation_test.cpp /Fobuild\ /Febuild\rotation_test.exe /link user32.lib
exit /b %errorlevel%
