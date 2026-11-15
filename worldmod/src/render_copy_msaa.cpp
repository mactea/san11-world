#define WIN32_LEAN_AND_MEAN
#include "render_copy_msaa.h"
#include <wrl/client.h>
#include <atomic>
using Microsoft::WRL::ComPtr;
namespace {
IDirect3DDevice9* owner=nullptr; // Surfaces retain the device until pre-Reset cleanup.
ComPtr<IDirect3DSurface9> back,originalDepth,color,depth;
std::atomic<int> activeSamples{0};
std::atomic<HRESULT> lastResult{S_FALSE};
std::atomic<unsigned long long> resolves{0};
}
void copyMsaaReset() {
    IDirect3DDevice9* d=owner;
    // Disable redirection before rebinding the original targets. A replacement
    // device may be created while the previous device is still being rendered.
    owner=nullptr;activeSamples=0;resolves=0;lastResult=S_FALSE;
    if(d) {
        ComPtr<IDirect3DSurface9> currentColor,currentDepth;
        d->GetRenderTarget(0,&currentColor);d->GetDepthStencilSurface(&currentDepth);
        bool restoreColor=currentColor.Get()==color.Get(),restoreDepth=currentDepth.Get()==depth.Get();
        D3DVIEWPORT9 viewport={};bool haveViewport=restoreColor && SUCCEEDED(d->GetViewport(&viewport));
        if(restoreDepth)d->SetDepthStencilSurface(nullptr);
        if(restoreColor)d->SetRenderTarget(0,back.Get());
        if(restoreDepth)d->SetDepthStencilSurface(originalDepth.Get());
        if(haveViewport)d->SetViewport(&viewport);
    }
    depth.Reset();color.Reset();originalDepth.Reset();back.Reset();
}
void copyMsaaInit(IDirect3DDevice9* d,const D3DPRESENT_PARAMETERS& pp,int samples) {
    copyMsaaReset();
    if((samples!=2 && samples!=4 && samples!=8) || pp.SwapEffect!=D3DSWAPEFFECT_COPY ||
       pp.MultiSampleType!=D3DMULTISAMPLE_NONE || !pp.EnableAutoDepthStencil ||
       (pp.AutoDepthStencilFormat!=D3DFMT_D24S8 && pp.AutoDepthStencilFormat!=D3DFMT_D24X8) ||
       (pp.Flags&D3DPRESENTFLAG_LOCKABLE_BACKBUFFER))return;
    ComPtr<IDirect3DSurface9> b,z,c,m;
    ComPtr<IDirect3D9> api;
    D3DDEVICE_CREATION_PARAMETERS cp={};D3DSURFACE_DESC desc={};
    D3DVIEWPORT9 viewport={};DWORD colorLevels=0,depthLevels=0;
    auto create=[&]()->HRESULT {
#define TRY(x) {HRESULT hr=(x);if(FAILED(hr))return hr;}
        TRY(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&b));TRY(b->GetDesc(&desc));
        TRY(d->GetDepthStencilSurface(&z));TRY(d->GetViewport(&viewport));
        TRY(d->GetDirect3D(&api));TRY(d->GetCreationParameters(&cp));
        TRY(api->CheckDeviceMultiSampleType(cp.AdapterOrdinal,cp.DeviceType,desc.Format,pp.Windowed,D3DMULTISAMPLE_TYPE(samples),&colorLevels));
        TRY(api->CheckDeviceMultiSampleType(cp.AdapterOrdinal,cp.DeviceType,pp.AutoDepthStencilFormat,pp.Windowed,D3DMULTISAMPLE_TYPE(samples),&depthLevels));
        if(!colorLevels || !depthLevels)return D3DERR_NOTAVAILABLE;
        TRY(d->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_TYPE(samples),0,FALSE,&c,nullptr));
        // Preserve depth between scenes/Present, just like the retained COPY color.
        TRY(d->CreateDepthStencilSurface(desc.Width,desc.Height,pp.AutoDepthStencilFormat,D3DMULTISAMPLE_TYPE(samples),0,FALSE,&m,nullptr));
        TRY(d->SetDepthStencilSurface(nullptr));TRY(d->SetRenderTarget(0,c.Get()));
        TRY(d->SetDepthStencilSurface(m.Get()));TRY(d->SetViewport(&viewport));
        return S_OK;
#undef TRY
    };
    lastResult=create();
    if(FAILED(lastResult)) {
        // Creation failure leaves the game's original single-sampled path available.
        if(c) {d->SetDepthStencilSurface(nullptr);d->SetRenderTarget(0,b.Get());d->SetDepthStencilSurface(z.Get());d->SetViewport(&viewport);}
        return;
    }
    back=b;originalDepth=z;color=c;depth=m;owner=d;activeSamples=samples;
}
IDirect3DSurface9* copyMsaaColor(IDirect3DDevice9* d,DWORD index,IDirect3DSurface9* surface) {
    return d==owner && index==0 && surface==back.Get()?color.Get():surface;
}
IDirect3DSurface9* copyMsaaDepth(IDirect3DDevice9* d,IDirect3DSurface9* surface) {
    if(d!=owner || (surface!=originalDepth.Get() && surface!=depth.Get()))return surface;
    ComPtr<IDirect3DSurface9> target;
    if(FAILED(d->GetRenderTarget(0,&target)))return surface;
    return target.Get()==color.Get()?depth.Get():originalDepth.Get();
}
HRESULT copyMsaaTargetChanged(IDirect3DDevice9* d,DWORD index) {
    if(d!=owner || index!=0)return S_OK;
    ComPtr<IDirect3DSurface9> current;
    if(FAILED(d->GetDepthStencilSurface(&current)))return S_OK; // No depth attached.
    auto matching=copyMsaaDepth(d,current.Get());
    return matching!=current.Get()?d->SetDepthStencilSurface(matching):S_OK;
}
HRESULT copyMsaaResolve(IDirect3DDevice9* d) {
    if(d!=owner)return S_OK;
    // Call only outside BeginScene/EndScene, after native resource return.
    HRESULT hr=d->StretchRect(color.Get(),nullptr,back.Get(),nullptr,D3DTEXF_NONE);
    if(SUCCEEDED(hr))++resolves;
    // An unsupported resolve must not trap every following frame on an
    // offscreen target. Keep the failure visible, then draw directly until Reset.
    else if(hr==E_INVALIDARG || hr==D3DERR_INVALIDCALL || hr==D3DERR_NOTAVAILABLE)copyMsaaReset();
    lastResult=hr;
    return hr;
}
void copyMsaaStatus(FILE* out) {
    fprintf(out,"COPY MSAA: samples=%d resolves=%llu last_hr=%08lx\n",activeSamples.load(),resolves.load(),lastResult.load());
}
HRESULT renderReadback(IDirect3DDevice9* d,IDirect3DSurface9* source,IDirect3DSurface9* destination) {
    D3DSURFACE_DESC desc={};HRESULT hr=source->GetDesc(&desc);
    if(FAILED(hr))return hr;
    ComPtr<IDirect3DSurface9> resolved;
    if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE) {
        hr=d->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr);
        if(SUCCEEDED(hr))hr=d->StretchRect(source,nullptr,resolved.Get(),nullptr,D3DTEXF_NONE);
        if(FAILED(hr))return hr;
        source=resolved.Get();
    }
    return d->GetRenderTargetData(source,destination);
}
