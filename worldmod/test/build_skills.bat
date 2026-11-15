@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0.."
if not exist build mkdir build
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 test\skills_test.cpp src\skill_telemetry.cpp /Fobuild\ /Febuild\skills_test.exe
if errorlevel 1 exit /b 1
build\skills_test.exe "%~1" worldmod_skills.txt
exit /b %errorlevel%
