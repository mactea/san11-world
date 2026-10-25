@echo off
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR exit /b 1
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0..\.."
if not exist build\portrait_native mkdir build\portrait_native
cl /nologo /utf-8 /MT /EHsc "%~dp0portrait_native_check.cpp" /Fobuild\portrait_native\ /Febuild\portrait_native\portrait_native_check.exe /link /BASE:0x10000000 /FIXED:NO /DYNAMICBASE /LARGEADDRESSAWARE
