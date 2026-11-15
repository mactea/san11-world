@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0.."
if not exist build mkdir build
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 test\skill_telemetry_benchmark.cpp /Fobuild\ /Febuild\skill_telemetry_benchmark.exe /link psapi.lib
if errorlevel 1 exit /b 1
if "%~1"=="" (
    build\skill_telemetry_benchmark.exe build\telemetry-benchmark
) else (
    build\skill_telemetry_benchmark.exe "%~1"
)
exit /b %errorlevel%
