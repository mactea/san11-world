@echo off
setlocal
cd /d "%~dp0\..\.."
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
if not exist outputs\trade-ship-properties mkdir outputs\trade-ship-properties
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 worldmod\test\trade_ship_ui_test.cpp /Fooutputs\trade-ship-properties\ui-test.obj /Feoutputs\trade-ship-properties\ui-test.exe /link /BASE:0x10000000 /DYNAMICBASE:NO user32.lib gdi32.lib gdiplus.lib
if errorlevel 1 exit /b 1
outputs\trade-ship-properties\ui-test.exe
exit /b %errorlevel%
