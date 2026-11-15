#pragma once
#include <d3d9.h>
#include <stdio.h>

// Optional, exact-content replacement for the base terrain pass only.
// Configuration is read at startup; Reset drops device objects, not the package.
bool terrainReplacementConfigure(const wchar_t* path);
void terrainReplacementReset();
bool terrainReplacementFind(IDirect3DTexture9* source,IDirect3DTexture9** replacement);
void terrainReplacementStatus(FILE* out);
