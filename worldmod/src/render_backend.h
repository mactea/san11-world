#pragma once
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include "d3dtrace.h"

// Uses the OS runtime; never loads another d3d9.dll beside the proxy.
void renderConfigure(const wchar_t* backend, const wchar_t* terrain, TraceLogFn log, bool automation, int presentInterval, int anisotropy, int msaa=4, int waterStrength=0);
void renderAdjustPresentation(D3DPRESENT_PARAMETERS* parameters, IDirect3DDevice9* device = nullptr,
                              IDirect3D9* api=nullptr, UINT adapter=0, D3DDEVTYPE type=D3DDEVTYPE_HAL);
IDirect3D9* renderCreate9(HMODULE runtime, UINT sdk);
HRESULT renderCreate9Ex(HMODULE runtime, UINT sdk, IDirect3D9Ex** out);
IDirect3D9* renderFallback9();
void renderDeviceFallback(HRESULT initialFailure);
void renderOnDevice(IDirect3D9* api, UINT adapter, IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS& pp);
void renderBeforeReset();
void renderAfterReset(IDirect3DDevice9* device, HRESULT result, const D3DPRESENT_PARAMETERS& pp);
void renderStatus(FILE* out);
// Read-only capability check at a real terrain draw; does not replace that draw.
void renderProbeTerrain(IDirect3DDevice9* device);
bool renderNativeTerrainRequested();
void renderOnFrame();
void renderWaitForFrame();
void renderOnPresent(LARGE_INTEGER begin, LARGE_INTEGER end, HRESULT result, HWND window);
void renderTimingReset();
#ifdef WORLDMOD_RENDER_TEST
struct ID3D12CommandQueue;
// Borrowed pointer, only exposed to the standalone integration test.
ID3D12CommandQueue* renderTestNativeQueue();
#endif
