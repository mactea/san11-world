#pragma once
#include <d3d9.h>

// Shared by the production proxy and standalone lifecycle integration tests.
void terrain12HookBuffer(IDirect3DResource9* buffer);

void terrain12HookBufferWrites(IDirect3DDevice9* device);
#ifdef WORLDMOD_RENDER_TEST
HRESULT terrain12TestLockFrom(IDirect3DResource9* resource,UINT offset,UINT size,void** data,DWORD flags,uintptr_t caller);
#endif
