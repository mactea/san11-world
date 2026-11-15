#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "terrain_filter.h"
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
using Microsoft::WRL::ComPtr;

#ifdef WORLDMOD_RENDER_TEST
// Test executable only: fail selected calls while forwarding all others to D3D.
HRESULT terrainFilterTestSetState(IDirect3DDevice9*, D3DSAMPLERSTATETYPE, DWORD);
#endif
namespace {
int requested = 0;
TraceLogFn logfn = nullptr;
std::atomic<UINT> effective{0};
std::atomic<UINT64> applied{0}, skipped{0}, errors{0};
std::atomic<HRESULT> lastError{S_OK};
HRESULT setState(IDirect3DDevice9* d, D3DSAMPLERSTATETYPE state, DWORD value) {
#ifdef WORLDMOD_RENDER_TEST
    return terrainFilterTestSetState(d, state, value);
#else
    return d->SetSamplerState(0, state, value);
#endif
}
void failure(HRESULT hr) {
    effective = 0; // Stop overrides after a driver/state failure until device reset.
    lastError = hr;
    if (errors.fetch_add(1) == 0 && logfn) logfn("terrain filter: state failure hr=%08lx; disabled", hr);
}
}

UINT terrainFilterLimit(int value, const D3DCAPS9& caps) {
    if (value != 2 && value != 4 && value != 8 && value != 16) return 0;
    if (!(caps.RasterCaps & D3DPRASTERCAPS_ANISOTROPY) ||
        !(caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) || caps.MaxAnisotropy < 2) return 0;
    return std::min(UINT(value), UINT(caps.MaxAnisotropy));
}
void terrainFilterConfigure(int value, TraceLogFn log) {
    requested = value; logfn = log; effective = 0;
    terrainFilterClearCounters();
}
void terrainFilterOnDevice(IDirect3DDevice9* device) {
    D3DCAPS9 caps = {};
    effective = requested && SUCCEEDED(device->GetDeviceCaps(&caps)) ? terrainFilterLimit(requested, caps) : 0;
    if (requested && logfn) logfn("terrain filter: requested=%d effective=%u", requested, effective.load());
}
void terrainFilterReset() { effective = 0; }
void terrainFilterClearCounters() { applied = 0; skipped = 0; errors = 0; lastError = S_OK; }
bool terrainFilterEnabled() { return effective.load() > 1; }
void terrainFilterStatus(FILE* out) {
    fprintf(out, "Terrain filtering: requested=%d effective=%u applied_draws=%llu skipped_draws=%llu errors=%llu last_error=%08lx\n",
        requested, effective.load(), applied.load(), skipped.load(), errors.load(), lastError.load());
}

void TerrainFilterScope::apply(IDirect3DDevice9* d, IDirect3DVertexShader9* baseShader) {
    const UINT limit = effective.load();
    if (limit < 2 || device) return;
    ++skipped;
    if (!baseShader) return;
    ComPtr<IDirect3DVertexShader9> vs;
    ComPtr<IDirect3DPixelShader9> ps;
    ComPtr<IDirect3DBaseTexture9> texture;
    DWORD stage1 = 0, mip = 0, mag = 0;
    if (FAILED(d->GetVertexShader(&vs)) || vs.Get() != baseShader ||
        FAILED(d->GetPixelShader(&ps)) || ps ||
        FAILED(d->GetTextureStageState(1, D3DTSS_COLOROP, &stage1)) || stage1 != D3DTOP_DISABLE ||
        FAILED(d->GetTexture(0, &texture)) || !texture || texture->GetType() != D3DRTYPE_TEXTURE ||
        texture->GetLevelCount() < 2 ||
        FAILED(d->GetSamplerState(0, D3DSAMP_MIPFILTER, &mip)) || mip == D3DTEXF_NONE ||
        FAILED(d->GetSamplerState(0, D3DSAMP_MAGFILTER, &mag)) ||
        FAILED(d->GetSamplerState(0, D3DSAMP_MINFILTER, &minFilter)) ||
        FAILED(d->GetSamplerState(0, D3DSAMP_MAXANISOTROPY, &maxAnisotropy))) return;
    // Preserve intentional point sampling and any stronger existing filtering.
    if (minFilter != D3DTEXF_LINEAR && minFilter != D3DTEXF_ANISOTROPIC) return;
    if (minFilter == D3DTEXF_ANISOTROPIC && maxAnisotropy >= limit) return;
    if (mag == D3DTEXF_ANISOTROPIC && maxAnisotropy > limit) return;
    HRESULT hr = setState(d, D3DSAMP_MAXANISOTROPY, limit);
    if (FAILED(hr)) { failure(hr); return; }
    device = d; // Restore even when the following state change fails.
    hr = setState(d, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
    if (FAILED(hr)) {
        failure(hr);
        hr = setState(d, D3DSAMP_MAXANISOTROPY, maxAnisotropy);
        if (FAILED(hr)) failure(hr);
        device = nullptr;
        return;
    }
    --skipped; ++applied;
}
TerrainFilterScope::~TerrainFilterScope() {
    if (!device) return;
    HRESULT hr = setState(device, D3DSAMP_MINFILTER, minFilter);
    if (FAILED(hr)) failure(hr);
    hr = setState(device, D3DSAMP_MAXANISOTROPY, maxAnisotropy);
    if (FAILED(hr)) failure(hr);
}
