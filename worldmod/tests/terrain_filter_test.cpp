// Real-device tests of the production sampler override, with no game or hooks.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_filter.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <stdlib.h>
#include <string.h>
#include <initializer_list>
using Microsoft::WRL::ComPtr;
void testTerrainFilterFaults(IDirect3DDevice9*, IDirect3DVertexShader9*, UINT);
void testTerrainFilterImage(IDirect3DDevice9*, IDirect3DVertexShader9*);

namespace {
void require(bool value, const char* label) {
    if (!value) { printf("FAIL terrain filter: %s\n", label); exit(1); }
}
void check(HRESULT hr) { require(SUCCEEDED(hr), "D3D fixture operation"); }
void snapshot(IDirect3DDevice9* d, DWORD (&states)[2][13]) {
    for (UINT sampler = 0; sampler < 2; ++sampler)
        for (UINT state = 1; state <= 13; ++state)
            check(d->GetSamplerState(sampler, D3DSAMPLERSTATETYPE(state), &states[sampler][state-1]));
}
}

void testTerrainFilter(IDirect3DDevice9* d) {
    D3DCAPS9 caps = {};
    caps.RasterCaps = D3DPRASTERCAPS_ANISOTROPY;
    caps.TextureFilterCaps = D3DPTFILTERCAPS_MINFANISOTROPIC;
    caps.MaxAnisotropy = 16;
    for (int invalid : {-1, 0, 1, 3, 17}) require(!terrainFilterLimit(invalid, caps), "invalid/off config");
    for (int value : {2, 4, 8, 16}) require(terrainFilterLimit(value, caps) == UINT(value), "accepted config");
    caps.MaxAnisotropy = 4;
    require(terrainFilterLimit(16, caps) == 4, "capability clamp");
    caps.MaxAnisotropy = 1;
    require(!terrainFilterLimit(8, caps), "unsupported anisotropy degree");
    caps.MaxAnisotropy = 16; caps.TextureFilterCaps = 0;
    require(!terrainFilterLimit(8, caps), "unsupported minification filter");
    caps.TextureFilterCaps = D3DPTFILTERCAPS_MINFANISOTROPIC; caps.RasterCaps = 0;
    require(!terrainFilterLimit(8, caps), "unsupported raster capability");
    check(d->GetDeviceCaps(&caps));
    UINT limit = terrainFilterLimit(8, caps);
    require(limit > 1, "test adapter must support anisotropic minification");

    const char* source = "struct V { float4 p:POSITION; float2 uv:TEXCOORD0; }; V main(V v) { return v; }";
    ComPtr<ID3DBlob> code;
    check(D3DCompile(source, strlen(source), nullptr, nullptr, nullptr, "main", "vs_2_0", 0, 0, &code, nullptr));
    ComPtr<IDirect3DVertexShader9> vs;
    check(d->CreateVertexShader((DWORD*)code->GetBufferPointer(), &vs));
    check(d->SetVertexShader(vs.Get()));
    check(d->SetPixelShader(nullptr));
    const D3DVERTEXELEMENT9 elements[] = {{0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITION,0},
        {0,16,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0}, D3DDECL_END()};
    ComPtr<IDirect3DVertexDeclaration9> declaration;
    check(d->CreateVertexDeclaration(elements, &declaration));
    check(d->SetVertexDeclaration(declaration.Get()));
    ComPtr<IDirect3DTexture9> texture, singleLevel;
    check(d->CreateTexture(32,32,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr));
    check(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&singleLevel,nullptr));
    for (UINT level = 0; level < texture->GetLevelCount(); ++level) {
        D3DLOCKED_RECT pixels; D3DSURFACE_DESC desc;
        check(texture->GetLevelDesc(level, &desc)); check(texture->LockRect(level, &pixels, nullptr, 0));
        for (UINT y = 0; y < desc.Height; ++y)
            for (UINT x = 0; x < desc.Width; ++x) ((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x] = 0xff336699;
        check(texture->UnlockRect(level));
    }
    check(d->SetTexture(0, texture.Get()));
    check(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    check(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));
    check(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE));
    check(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));
    check(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR));
    check(d->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,1));
    DWORD before[2][13], after[2][13];
    auto exercise = [&](IDirect3DVertexShader9* expected, bool shouldApply) {
        snapshot(d, before);
        {
            TerrainFilterScope filter; filter.apply(d, expected);
            snapshot(d, after);
            require(after[0][D3DSAMP_MINFILTER-1] == (shouldApply ? D3DTEXF_ANISOTROPIC : before[0][D3DSAMP_MINFILTER-1]), "in-scope min filter");
            require(after[0][D3DSAMP_MAXANISOTROPY-1] == (shouldApply ? limit : before[0][D3DSAMP_MAXANISOTROPY-1]), "in-scope anisotropy");
        }
        snapshot(d, after);
        require(!memcmp(before, after, sizeof before), "all sampler states restored, including other sampler");
    };
    terrainFilterConfigure(0, nullptr); terrainFilterOnDevice(d); exercise(vs.Get(), false);
    terrainFilterConfigure(8, nullptr); terrainFilterOnDevice(d); exercise(vs.Get(), true);
    exercise(nullptr, false);
    check(d->SetVertexShader(nullptr)); exercise(vs.Get(), false); check(d->SetVertexShader(vs.Get()));
    ComPtr<IDirect3DPixelShader9> pixelShader;
    const char* pixelSource = "float4 main():COLOR { return float4(1,0,0,1); }";
    code.Reset();
    check(D3DCompile(pixelSource, strlen(pixelSource), nullptr, nullptr, nullptr, "main", "ps_2_0", 0, 0, &code, nullptr));
    check(d->CreatePixelShader((DWORD*)code->GetBufferPointer(), &pixelShader));
    check(d->SetPixelShader(pixelShader.Get())); exercise(vs.Get(), false); check(d->SetPixelShader(nullptr));
    ComPtr<IDirect3DCubeTexture9> cube;
    check(d->CreateCubeTexture(16,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&cube,nullptr));
    check(d->SetTexture(0, cube.Get())); exercise(vs.Get(), false);
    check(d->SetTexture(0, nullptr)); exercise(vs.Get(), false);
    check(d->SetTexture(0, singleLevel.Get())); exercise(vs.Get(), false); check(d->SetTexture(0, texture.Get()));
    check(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE)); exercise(vs.Get(), false);
    check(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR));
    check(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT)); exercise(vs.Get(), false);
    check(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_ANISOTROPIC));
    check(d->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,caps.MaxAnisotropy)); exercise(vs.Get(), false);
    check(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));
    if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) && caps.MaxAnisotropy > limit) {
        DWORD mag; check(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag));
        check(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_ANISOTROPIC)); exercise(vs.Get(), false);
        check(d->SetSamplerState(0,D3DSAMP_MAGFILTER,mag));
    }
    check(d->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,1));
    check(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_MODULATE)); exercise(vs.Get(), false);
    check(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    terrainFilterReset(); exercise(vs.Get(), false); terrainFilterOnDevice(d); exercise(vs.Get(), true);

    check(d->SetRenderState(D3DRS_ZENABLE,FALSE)); check(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
    check(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0));
    const float vertices[3][6] = {{-1,-1,0,1,0,0},{0,1,0,1,0.5f,1},{1,-1,0,1,1,0}};
    snapshot(d, before);
    auto draw = [&]() {
        TerrainFilterScope filter; filter.apply(d, vs.Get());
        return d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,vertices,sizeof vertices[0]);
    };
    check(d->BeginScene()); check(draw()); check(d->EndScene());
    snapshot(d, after); require(!memcmp(before, after, sizeof before), "restore on draw return");
    ComPtr<IDirect3DSurface9> target, readback;
    check(d->GetRenderTarget(0, &target)); D3DSURFACE_DESC desc; check(target->GetDesc(&desc));
    check(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    check(d->GetRenderTargetData(target.Get(),readback.Get()));
    D3DLOCKED_RECT pixels; check(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
    DWORD pixel = ((DWORD*)((BYTE*)pixels.pBits+(desc.Height/2)*pixels.Pitch))[desc.Width/2] & 0xffffff;
    check(readback->UnlockRect()); require(pixel == 0x336699, "filtered draw readback");
    terrainFilterStatus(stdout);
    puts("PASS terrain filter: caps, opt-out, draw guards, exact sampler restoration, filtered draw/readback");
    testTerrainFilterFaults(d,vs.Get(),limit);
    testTerrainFilterImage(d,vs.Get());
    check(d->SetTexture(0,nullptr)); check(d->SetVertexShader(nullptr)); check(d->SetVertexDeclaration(nullptr));
    terrainFilterReset();
}
