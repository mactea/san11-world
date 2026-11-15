#pragma once
#include <d3d9.h>
#include <stdio.h>
#include "d3dtrace.h"

UINT terrainFilterLimit(int requested, const D3DCAPS9& caps);
void terrainFilterConfigure(int requested, TraceLogFn log);
void terrainFilterOnDevice(IDirect3DDevice9* device);
void terrainFilterReset();
void terrainFilterClearCounters();
bool terrainFilterEnabled();
void terrainFilterStatus(FILE* out);

// Stack-scoped override for one identified base-terrain draw, never a frame.
class TerrainFilterScope {
    IDirect3DDevice9* device = nullptr;
    DWORD minFilter = 0, maxAnisotropy = 0;
public:
    TerrainFilterScope() = default;
    TerrainFilterScope(const TerrainFilterScope&) = delete;
    TerrainFilterScope& operator=(const TerrainFilterScope&) = delete;
    void apply(IDirect3DDevice9* device, IDirect3DVertexShader9* baseShader);
    ~TerrainFilterScope();
};
