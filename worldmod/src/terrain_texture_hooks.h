#pragma once
#include <d3d9.h>

// Install the same texture mutation hooks in the game and standalone tests.
void terrain12HookTexture(IDirect3DTexture9* texture);
void terrain12HookTextureUpdates(IDirect3DDevice9* device);
