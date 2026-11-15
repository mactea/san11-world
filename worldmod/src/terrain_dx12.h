#pragma once
#include <d3d9on12.h>
#include "d3dtrace.h"
#include <stdio.h>

void terrain12Init(ID3D12Device* device, ID3D12CommandQueue* queue, TraceLogFn log);
void terrain12Reset();
void* terrain12Lock(IDirect3DResource9* buffer, UINT offset, UINT size, void* data, DWORD flags=0);
void terrain12Unlock(IDirect3DResource9* buffer);
void terrain12ForgetBuffer(IDirect3DResource9* buffer);
bool water12Draw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT base, UINT minimum,
                 UINT vertices, UINT start, UINT primitives);
bool object12Draw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT base, UINT minimum,
                  UINT vertices, UINT start, UINT primitives);
// Called for every successful shader creation, including unknown sources, so
// reused COM addresses cannot retain an earlier model classification.
void object12ShaderCreated(IDirect3DVertexShader9* shader, uintptr_t source);
bool model12Draw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT base, UINT minimum,
                 UINT vertices, UINT start, UINT primitives);
bool terrain12Draw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT base, UINT minimum,
                   UINT vertices, UINT start, UINT primitives);
bool model12DrawPrimitive(IDirect3DDevice9* device,D3DPRIMITIVETYPE type,UINT start,UINT primitives);
bool model12DrawUP(IDirect3DDevice9* device,D3DPRIMITIVETYPE type,UINT primitives,const void* vertices,UINT stride);
bool model12DrawIndexedUP(IDirect3DDevice9* device,D3DPRIMITIVETYPE type,UINT minimum,UINT vertices,UINT primitives,const void* indices,D3DFORMAT format,const void* data,UINT stride);
// One record per intercepted API call, including calls with unknown identities.
void terrain12RecordDraw(IDirect3DDevice9* device,UINT kind,D3DPRIMITIVETYPE type,bool native,UINT upStride=0);
enum class Terrain12FlushReason { Explicit, Clear, EndScene, ColorTarget, DepthTarget,
    FallbackDP, FallbackDIP, FallbackDPUP, FallbackDIPUP, Present, TextureBudget,
    DrawLimit, UploadLimit, Reset, ProcessVertices, Count };
void terrain12Flush(Terrain12FlushReason reason=Terrain12FlushReason::Explicit);
void terrain12TextureChanged(IDirect3DBaseTexture9* texture);
void terrain12SetEnabled(bool enabled);
void terrain12ConfigureWater(int strengthPercent);
void terrain12AdvanceWater(double elapsedSeconds);
bool terrain12Active();
void terrain12Status(FILE* out);
#ifdef WORLDMOD_RENDER_TEST
void terrain12TestWaterPreview(float strength,float time);
struct Terrain12TestWater {float strength,phase;};
Terrain12TestWater terrain12TestWater();
struct Terrain12TestCoverage { UINT64 recognized,rejected[7]; };
Terrain12TestCoverage terrain12TestCoverage(UINT mode);
void object12TestShaders(IDirect3DVertexShader9* base,IDirect3DVertexShader9* outline);
void water12TestBind(IDirect3DResource9* vertex, IDirect3DResource9* index, IDirect3DVertexShader9* shader);
void terrain12TestPrefixUpload(bool enabled);
// Fixture bindings and cache observability; drawing/upload code is production code.
void terrain12TestBind(IDirect3DResource9* const* buffers, IDirect3DVertexShader9* const* shaders);
struct Terrain12TestTextures { UINT64 bytes, uploads, budget; size_t entries; };
Terrain12TestTextures terrain12TestTextures();
void terrain12TestTextureBudget(UINT64 bytes);
struct Terrain12TestWork { UINT64 vertexBytes, indexBytes, copies, hits, batches, srvs, samplers, submissions; };
Terrain12TestWork terrain12TestWork();
void terrain12TestClearWork();
#endif
