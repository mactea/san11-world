@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0.."
if not exist build mkdir build
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 test\skill_telemetry_test.cpp /Fobuild\ /Febuild\skill_telemetry_test.exe
if errorlevel 1 exit /b 1
build\skill_telemetry_test.exe build\skill_telemetry_test.csv
exit /b %errorlevel%
