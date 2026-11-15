@echo off
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR exit /b 1
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0.."
if not exist build mkdir build
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 tests\skills_test.cpp /Fobuild\ /Febuild\skills_test.exe
if errorlevel 1 exit /b 1
build\skills_test.exe "%~1" worldmod_skills.txt
exit /b %errorlevel%
