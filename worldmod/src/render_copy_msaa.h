#pragma once
#include <d3d9.h>
#include <stdio.h>

// Persistent offscreen targets keep COPY presentation single-sampled.
void copyMsaaInit(IDirect3DDevice9*,const D3DPRESENT_PARAMETERS&,int samples);
void copyMsaaReset();
IDirect3DSurface9* copyMsaaColor(IDirect3DDevice9*,DWORD,IDirect3DSurface9*);
IDirect3DSurface9* copyMsaaDepth(IDirect3DDevice9*,IDirect3DSurface9*);
HRESULT copyMsaaTargetChanged(IDirect3DDevice9*,DWORD);
HRESULT copyMsaaResolve(IDirect3DDevice9*);
// Screenshot readback outside a scene; also supports direct DISCARD MSAA targets.
HRESULT renderReadback(IDirect3DDevice9*,IDirect3DSurface9*,IDirect3DSurface9*);
void copyMsaaStatus(FILE*);
