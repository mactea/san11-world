// A deterministic anisotropic footprint: 8 texels/pixel horizontally, 1 vertically.
// Horizontal stripes should survive AF better than isotropic mip selection.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_filter.h"
#include <wrl/client.h>
#include <vector>
#include <stdlib.h>
using Microsoft::WRL::ComPtr;

namespace {
void require(bool value, const char* label) {
    if (!value) { printf("FAIL filter image: %s\n", label); exit(1); }
}
void check(HRESULT hr) { require(SUCCEEDED(hr), "D3D fixture operation"); }
}

void testTerrainFilterImage(IDirect3DDevice9* d, IDirect3DVertexShader9* shader) {
    ComPtr<IDirect3DTexture9> texture;
    check(d->CreateTexture(256,256,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr));
    for (UINT level = 0; level < texture->GetLevelCount(); ++level) {
        D3DLOCKED_RECT pixels; D3DSURFACE_DESC desc;
        check(texture->GetLevelDesc(level,&desc)); check(texture->LockRect(level,&pixels,nullptr,0));
        const UINT scale = 1u << level;
        for (UINT y = 0; y < desc.Height; ++y) {
            UINT sum = 0;
            for (UINT i = 0; i < scale; ++i) sum += (((y*scale+i)/4)%2) * 255;
            const DWORD value = sum/scale;
            for (UINT x = 0; x < desc.Width; ++x)
                ((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x] = 0xff000000 | value*0x010101;
        }
        check(texture->UnlockRect(level));
    }
    check(d->SetTexture(0,texture.Get()));
    check(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));
    check(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
    check(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR));
    check(d->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,1));
    const float quad[6][6] = {
        {-1,1,0,1,0,0},{1,1,0,1,8,0},{-1,-1,0,1,0,1},
        {-1,-1,0,1,0,1},{1,1,0,1,8,0},{1,-1,0,1,8,1}};
    ComPtr<IDirect3DSurface9> target, readback;
    check(d->GetRenderTarget(0,&target)); D3DSURFACE_DESC desc; check(target->GetDesc(&desc));
    require(desc.Width == 256 && desc.Height == 256, "256x256 fixture required");
    check(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    auto capture = [&](int anisotropy) {
        terrainFilterConfigure(anisotropy,nullptr); terrainFilterOnDevice(d);
        check(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0));
        check(d->BeginScene());
        { TerrainFilterScope filter; filter.apply(d,shader);
          check(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,quad,sizeof quad[0])); }
        check(d->EndScene());
        check(d->GetRenderTargetData(target.Get(),readback.Get()));
        D3DLOCKED_RECT pixels; check(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
        std::vector<BYTE> result;
        for (UINT y = 32; y < 224; ++y)
            for (UINT x = 32; x < 224; ++x)
                result.push_back(BYTE(((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x]));
        check(readback->UnlockRect()); return result;
    };
    const auto linear = capture(0), anisotropic = capture(8), restored = capture(0);
    require(linear == restored, "off/on/off returns identical baseline pixels");
    double linearContrast = 0, afContrast = 0, difference = 0;
    for (size_t i = 0; i < linear.size(); ++i) {
        linearContrast += abs(int(linear[i])-127);
        afContrast += abs(int(anisotropic[i])-127);
        difference += abs(int(anisotropic[i])-int(linear[i]));
    }
    linearContrast /= linear.size(); afContrast /= linear.size(); difference /= linear.size();
    printf("Filter stripe fixture: linear_contrast=%.3f af_contrast=%.3f mean_difference=%.3f (0..255)\n",
        linearContrast,afContrast,difference);
    require(difference > 10 && afContrast > linearContrast+10, "AF preserves stripe contrast in this fixture");
    puts("PASS terrain filter: real sampling effect and exact off/on/off image restoration");
    check(d->SetTexture(0,nullptr));
}
