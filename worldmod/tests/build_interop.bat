@echo off
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR exit /b 1
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0.."
if not exist build mkdir build
python tests\extract_terrain_shaders.py
if errorlevel 1 exit /b 1
python tests\extract_water_fixture.py
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /std:c++17 /DWORLDMOD_RENDER_TEST tests\render_interop.cpp tests\copy_msaa_hooks_test.cpp src\d3dtrace.cpp tests\terrain_filter_test.cpp tests\terrain_filter_fault_test.cpp tests\terrain_filter_image_test.cpp tests\terrain_dx12_test.cpp tests\terrain_texture_test.cpp tests\terrain_performance_test.cpp tests\terrain_upload_test.cpp tests\water_reference_test.cpp tests\native_quality_test.cpp tests\object_render_test.cpp tests\screen_render_test.cpp tests\object_capture_test.cpp tests\process_vertices_test.cpp tests\user_draw_test.cpp tests\default_texture_contract_test.cpp src\terrain_filter.cpp src\terrain_dx12.cpp src\terrain_texture_hooks.cpp src\terrain_replacement.cpp src\terrain_buffer_hooks.cpp src\object_capture.cpp src\render_backend.cpp src\render_copy_msaa.cpp /Fobuild\ /Febuild\render_interop.exe /link user32.lib d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 1
if /i "%~1"=="--build-only" exit /b 0
build\render_interop.exe %*
exit /b %errorlevel%
