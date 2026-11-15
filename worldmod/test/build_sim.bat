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
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 garrison_test.cpp /Fo..\build\ /Fe..\build\garrison_test.exe || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 garrison_runtime_test.cpp /Fo..\build\ /Fe..\build\garrison_runtime_test.exe /link /BASE:0x10000000 /DYNAMICBASE:NO || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_auto_test.cpp /Fo..\build\ /Fe..\build\trade_auto_test.exe || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_sim.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\trade_sim.exe || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_panel_test.cpp ..\src\trade_panel.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\trade_panel_test.exe /link user32.lib gdi32.lib || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_regression_test.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\trade_regression_test.exe || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_pricing_test.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\trade_pricing_test.exe || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_procurement_calibration.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\trade_procurement_calibration.exe || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_procurement_test.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\trade_procurement_test.exe || exit /b 1
rem The serializer fixture reserves the native world address; keep its executable away from it.
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 trade_runtime_test.cpp ..\src\skill_telemetry.cpp ..\src\trade_ship_visual.cpp ..\src\trade_ship_ui.cpp ..\src\trade_images.cpp ..\src\trade_panel.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\trade_runtime_test.exe /link /BASE:0x10000000 /DYNAMICBASE:NO user32.lib gdi32.lib cabinet.lib gdiplus.lib || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 pathfind_route_test.cpp ..\src\units.cpp /Fo..\build\ /Fe..\build\pathfind_route_test.exe || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 movement_trade_test.cpp /Fo..\build\ /Fe..\build\movement_trade_test.exe || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 sea_route_test.cpp ..\src\trade_model.cpp /Fo..\build\ /Fe..\build\sea_route_test.exe || exit /b 1
exit /b 0
