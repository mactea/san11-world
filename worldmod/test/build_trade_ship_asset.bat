@echo off
setlocal
cd /d "%~dp0\..\.."
if "%~1"=="" (
  echo Usage: build_trade_ship_asset.bat "game directory"
  exit /b 2
)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
if not exist outputs\trade-ship-3d mkdir outputs\trade-ship-3d
cl /nologo /EHsc /W4 /O2 worldmod\test\trade_ship_asset_test.cpp /Fooutputs\trade-ship-3d\native-test.obj /Feoutputs\trade-ship-3d\native-test.exe /link /BASE:0x10000000 /DYNAMICBASE:NO d3d9.lib user32.lib
if errorlevel 1 exit /b 1
outputs\trade-ship-3d\native-test.exe "%~1\san11pk_world.exe" data\trade_ships\merchant\merchant_ship.wkm
exit /b %errorlevel%
