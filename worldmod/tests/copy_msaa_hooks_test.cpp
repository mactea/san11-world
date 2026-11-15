// Link the production vtable hooks; only game automation is absent in this process.
#define WIN32_LEAN_AND_MEAN
#include "../src/render_backend.h"
#include "../src/render_copy_msaa.h"
#include "../src/terrain_dx12.h"
#include <wrl/client.h>
#include <stdlib.h>
using Microsoft::WRL::ComPtr;
void automationOnEndScene(IDirect3DDevice9*) {}
namespace {
void check(HRESULT hr,const char* what){if(FAILED(hr)){printf("FAIL COPY hooks %s: %08lx\n",what,hr);exit(1);}}
void require(bool ok,const char* what){if(!ok){printf("FAIL COPY hooks %s\n",what);exit(1);}}
#define CHECK(x) check((x),#x)
HRESULT STDMETHODCALLTYPE failedResolve(IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,IDirect3DSurface9*,const RECT*,D3DTEXTUREFILTERTYPE) {
    return E_INVALIDARG;
}
unsigned long long resolves() {
    FILE* f=nullptr;require(!tmpfile_s(&f),"status file");copyMsaaStatus(f);rewind(f);
    int count=0;unsigned long long n=0;require(fscanf_s(f,"COPY MSAA: samples=%d resolves=%llu",&count,&n)==2,"status fields");fclose(f);
    require(count==4,"default four samples");return n;
}
}
void testCopyMsaaHooks(IDirect3D9* api,UINT adapter,D3DPRESENT_PARAMETERS pp) {
    renderConfigure(L"9on12",L"dx12",nullptr,false,0,0);
    traceInit(L".",L"offline",nullptr,false);traceWrapDirect3D9(api);
    ComPtr<IDirect3DDevice9> d;
    CHECK(api->CreateDevice(adapter,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&d));
    for(int iteration=0;iteration<2;++iteration) {
        {
            ComPtr<IDirect3DSurface9> back,target,depth,other,readback;
            ComPtr<IDirect3DSwapChain9> chain;
            CHECK(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));CHECK(d->GetRenderTarget(0,&target));CHECK(d->GetDepthStencilSurface(&depth));
            CHECK(d->GetSwapChain(0,&chain));
            D3DSURFACE_DESC desc={};CHECK(target->GetDesc(&desc));require(desc.MultiSampleType==4,"CreateDevice/Reset target active");
            CHECK(depth->GetDesc(&desc));require(desc.Format==pp.AutoDepthStencilFormat && desc.MultiSampleType==4,"preserve requested D24S8/D24X8 depth format");
            CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&other,nullptr));
            CHECK(d->SetRenderTarget(0,other.Get()));
            ComPtr<IDirect3DSurface9> singleDepth;CHECK(d->GetDepthStencilSurface(&singleDepth));
            CHECK(singleDepth->GetDesc(&desc));require(desc.MultiSampleType==D3DMULTISAMPLE_NONE,"offscreen target gets matching original depth");
            CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff445566,1,0));
            CHECK(d->SetRenderTarget(0,back.Get()));
            ComPtr<IDirect3DSurface9> restoredDepth;CHECK(d->GetDepthStencilSurface(&restoredDepth));
            require(restoredDepth.Get()==depth.Get(),"return to backbuffer restores MSAA depth");
            CHECK(d->SetDepthStencilSurface(singleDepth.Get()));
            restoredDepth.Reset();CHECK(d->GetDepthStencilSurface(&restoredDepth));require(restoredDepth.Get()==depth.Get(),"saved auto depth redirects on MSAA target");
            CHECK(d->SetDepthStencilSurface(nullptr));CHECK(d->SetRenderTarget(0,other.Get()));
            restoredDepth.Reset();require(d->GetDepthStencilSurface(&restoredDepth)==D3DERR_NOTFOUND,"explicit null depth remains null");
            CHECK(d->SetRenderTarget(0,back.Get()));CHECK(d->SetDepthStencilSurface(depth.Get()));
            ComPtr<IDirect3DSurface9> restored;CHECK(d->GetRenderTarget(0,&restored));require(restored.Get()==target.Get(),"real SetRenderTarget hook redirects backbuffer");
            CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
            for(int viaChain=0;viaChain<2;++viaChain) {
                DWORD expected=viaChain?0xabcdef:0x123456;
                CHECK(d->BeginScene());CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000|expected,1,0));CHECK(d->EndScene());
                auto before=resolves();
                if(viaChain)CHECK(chain->Present(nullptr,nullptr,nullptr,nullptr,0));
                else CHECK(d->Present(nullptr,nullptr,nullptr,nullptr));
                require(resolves()==before+1,"exactly one resolve per outer Present");
                CHECK(d->GetRenderTargetData(back.Get(),readback.Get()));
                // Same temporary MSAA resolve used by screenshots and the startup pixel probe.
                // The real Reset below catches any retained DEFAULT-pool resolve surface.
                CHECK(renderReadback(d.Get(),target.Get(),readback.Get()));
                D3DLOCKED_RECT pixels;CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
                require((*(DWORD*)pixels.pBits&0xffffff)==expected,"Present hook resolves latest color");CHECK(readback->UnlockRect());
            }
        }
        // The real Reset hook must release the retained backbuffer and both MSAA surfaces.
        CHECK(d->Reset(&pp));
    }
    renderBeforeReset();
    printf("PASS production COPY hooks: CreateDevice/target redirect/device Present/chain Present/single resolve/Reset depth_format=%u\n",pp.AutoDepthStencilFormat);
}

void testCopyMsaaTransitions(IDirect3D9* api,UINT adapter,D3DPRESENT_PARAMETERS pp) {
    traceInit(L".",L"offline",nullptr,false);traceWrapDirect3D9(api);
    pp.AutoDepthStencilFormat=D3DFMT_D24X8;
    const D3DFORMAT formats[]={D3DFMT_X8R8G8B8,D3DFMT_A2R10G10B10,D3DFMT_X8R8G8B8};
    const int sampleCounts[]={0,4};
    for(int samples:sampleCounts) {
        renderConfigure(L"9on12",L"dx12",nullptr,false,0,0,samples);
        ComPtr<IDirect3DDevice9> d;
        for(int step=0;step<3;++step) {
            pp.BackBufferFormat=formats[step];pp.BackBufferWidth=step==1?320:256;
            if(d)CHECK(d->Reset(&pp));
            else CHECK(api->CreateDevice(adapter,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&d));
            ComPtr<IDirect3DSurface9> target,back,readback;
            CHECK(d->GetRenderTarget(0,&target));CHECK(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));
            D3DSURFACE_DESC desc={};CHECK(target->GetDesc(&desc));
            require(desc.Format==pp.BackBufferFormat && desc.Width==pp.BackBufferWidth,"format/size transition rebuilds target");
            require(desc.MultiSampleType==samples,"transition sample count");
            CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff00ff00,1,0));
            HRESULT present=d->Present(nullptr,nullptr,nullptr,nullptr);
            printf("COPY transition samples=%d format=%u width=%u Present=%08lx\n",samples,pp.BackBufferFormat,pp.BackBufferWidth,present);fflush(stdout);
            CHECK(present);
            CHECK(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&readback,nullptr));
            CHECK(d->GetRenderTargetData(back.Get(),readback.Get()));
            D3DLOCKED_RECT pixels;CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
            DWORD mask=desc.Format==D3DFMT_A2R10G10B10?0x3fffffff:0xffffff;
            DWORD expected=desc.Format==D3DFMT_A2R10G10B10?0xffc00:0xff00;
            require((*(DWORD*)pixels.pBits&mask)==expected,"format transition resolved pixel");CHECK(readback->UnlockRect());
        }
        renderBeforeReset();
    }
    renderConfigure(L"9on12",L"dx12",nullptr,false,0,0);
    ComPtr<IDirect3DDevice9> oldDevice,newDevice;
    CHECK(api->CreateDevice(adapter,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&oldDevice));
    D3DVIEWPORT9 savedViewport={4,8,128,96,0.2f,0.8f};CHECK(oldDevice->SetViewport(&savedViewport));
    CHECK(api->CreateDevice(adapter,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&newDevice));
    ComPtr<IDirect3DSurface9> oldTarget,oldBack,oldDepth,readback;
    CHECK(oldDevice->GetRenderTarget(0,&oldTarget));CHECK(oldDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&oldBack));
    require(oldTarget.Get()==oldBack.Get(),"device replacement restores old backbuffer instead of orphaning MSAA output");
    CHECK(oldDevice->GetDepthStencilSurface(&oldDepth));D3DSURFACE_DESC depthDesc={};CHECK(oldDepth->GetDesc(&depthDesc));
    require(depthDesc.MultiSampleType==D3DMULTISAMPLE_NONE,"device replacement restores old depth");
    D3DVIEWPORT9 viewport={};CHECK(oldDevice->GetViewport(&viewport));
    require(!memcmp(&viewport,&savedViewport,sizeof viewport),"device replacement preserves viewport");
    CHECK(oldDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0));
    CHECK(oldDevice->Present(nullptr,nullptr,nullptr,nullptr));
    CHECK(oldDevice->CreateOffscreenPlainSurface(pp.BackBufferWidth,pp.BackBufferHeight,pp.BackBufferFormat,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    CHECK(oldDevice->GetRenderTargetData(oldBack.Get(),readback.Get()));
    D3DLOCKED_RECT pixels;CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
    DWORD pixel=*(DWORD*)((BYTE*)pixels.pBits+8*pixels.Pitch+4*sizeof(DWORD));CHECK(readback->UnlockRect());
    require((pixel&0xffffff)==0x123456,"old device keeps presenting newly drawn pixels");
    CHECK(newDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xffabcdef,1,0));
    CHECK(newDevice->Present(nullptr,nullptr,nullptr,nullptr));require(resolves()==1,"new device retains active MSAA");
    // Inject only the driver resolve failure; exercise the real Present hook and
    // subsequent rendering/readback rather than a stand-in state machine.
    void** slot=*(void***)newDevice.Get()+34;void* original=*slot;DWORD protection=0;
    require(VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection)!=0,"patch resolve slot");
    *slot=(void*)failedResolve;
    HRESULT failed=newDevice->Present(nullptr,nullptr,nullptr,nullptr);
    *slot=original;DWORD ignored=0;VirtualProtect(slot,sizeof(void*),protection,&ignored);
    require(failed==E_INVALIDARG,"resolve error remains visible to caller");
    ComPtr<IDirect3DSurface9> recoveredTarget,recoveredBack;
    CHECK(newDevice->GetRenderTarget(0,&recoveredTarget));CHECK(newDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&recoveredBack));
    require(recoveredTarget.Get()==recoveredBack.Get(),"failed resolve restores direct backbuffer");
    CHECK(newDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff654321,1,0));
    CHECK(newDevice->Present(nullptr,nullptr,nullptr,nullptr));
    readback.Reset();
    CHECK(newDevice->CreateOffscreenPlainSurface(pp.BackBufferWidth,pp.BackBufferHeight,pp.BackBufferFormat,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    CHECK(newDevice->GetRenderTargetData(recoveredBack.Get(),readback.Get()));
    CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));pixel=*(DWORD*)pixels.pBits;CHECK(readback->UnlockRect());
    require((pixel&0xffffff)==0x654321,"frame after failed resolve presents new pixels");
    recoveredTarget.Reset();recoveredBack.Reset();
    CHECK(newDevice->Reset(&pp));CHECK(newDevice->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff112233,1,0));
    CHECK(newDevice->Present(nullptr,nullptr,nullptr,nullptr));require(resolves()==1,"Reset re-enables MSAA after failed resolve");
    renderBeforeReset();
    puts("PASS COPY format/size transitions with MSAA disabled/enabled");
    puts("PASS COPY device replacement restores old targets/viewport and keeps new MSAA active");
    puts("PASS COPY failed resolve reports error/restores drawing/Reset re-enables MSAA");
}

void testCopyMsaaFailedReset(IDirect3D9* api,UINT adapter,D3DPRESENT_PARAMETERS pp) {
    traceInit(L".",L"offline",nullptr,false);traceWrapDirect3D9(api);
    pp.AutoDepthStencilFormat=D3DFMT_D24X8;
    const D3DFORMAT formats[]={D3DFMT_X8R8G8B8,D3DFMT_A2R10G10B10,D3DFMT_X8R8G8B8};
    const int sampleCounts[]={0,4};
    for(int samples:sampleCounts) {
        renderConfigure(L"9on12",L"dx12",nullptr,false,0,0,samples);
        ComPtr<IDirect3DDevice9> d;
        pp.BackBufferFormat=D3DFMT_X8R8G8B8;
        CHECK(api->CreateDevice(adapter,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&d));
        for(auto format:formats) {
            // COPY permits only one backbuffer. Exercise an actual runtime Reset
            // rejection, then recover through the same production hooks.
            auto invalid=pp;invalid.BackBufferCount=2;
            void** before=*(void***)d.Get();
            void* resetBefore=before[16];void* upBefore=before[83];
            HRESULT rejected=d->Reset(&invalid);
            void** lost=*(void***)d.Get();
            void* resetLost=lost[16];void* upLost=lost[83];
            require(FAILED(rejected),"invalid COPY Reset is rejected");
            require(!terrain12Active(),"failed Reset leaves native renderer inactive");
            FILE* status=nullptr;require(!tmpfile_s(&status),"failed Reset status file");
            copyMsaaStatus(status);rewind(status);int active=-1;
            require(fscanf_s(status,"COPY MSAA: samples=%d",&active)==1,"failed Reset status");fclose(status);
            require(active==0,"failed Reset releases MSAA targets");
            pp.BackBufferFormat=format;pp.BackBufferCount=1;
            CHECK(d->Reset(&pp));
            printf("Reset recovery tables before=%p lost=%p recovered=%p Reset=%p/%p/%p UP=%p/%p/%p\n",before,lost,*(void***)d.Get(),resetBefore,resetLost,(*(void***)d.Get())[16],upBefore,upLost,(*(void***)d.Get())[83]);
            {
                ComPtr<IDirect3DSurface9> target,depth,back,readback;
                CHECK(d->GetRenderTarget(0,&target));CHECK(d->GetDepthStencilSurface(&depth));
                CHECK(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));
                D3DSURFACE_DESC desc={},z={};CHECK(target->GetDesc(&desc));CHECK(depth->GetDesc(&z));
                require(desc.Format==format && desc.MultiSampleType==samples && z.MultiSampleType==samples && z.Format==D3DFMT_D24X8,"retry restores format and samples");
                CHECK(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));
                CHECK(d->SetRenderState(D3DRS_LIGHTING,FALSE));CHECK(d->SetRenderState(D3DRS_ZENABLE,FALSE));
                CHECK(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
                CHECK(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE));
                CHECK(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE));
                CHECK(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE));
                CHECK(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE));
                CHECK(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE));
                CHECK(d->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE));
                struct Vertex {float x,y,z,rhw;DWORD color;};
                Vertex quad[]={{32,32,.5f,1,0xff00ff00},{224,32,.5f,1,0xff00ff00},{32,224,.5f,1,0xff00ff00},{224,224,.5f,1,0xff00ff00}};
                CHECK(d->BeginScene());CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0));
                CHECK(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex)));CHECK(d->EndScene());
                if(!terrain12Active())terrain12Status(stdout);
                require(terrain12Active(),"successful retry submits native drawing");
                CHECK(d->Present(nullptr,nullptr,nullptr,nullptr));
                CHECK(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,format,D3DPOOL_SYSTEMMEM,&readback,nullptr));
                CHECK(d->GetRenderTargetData(back.Get(),readback.Get()));
                D3DLOCKED_RECT pixels;CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
                DWORD actual=*(DWORD*)((BYTE*)pixels.pBits+128*pixels.Pitch+128*sizeof(DWORD));
                DWORD outside=*(DWORD*)pixels.pBits;CHECK(readback->UnlockRect());
                DWORD mask=format==D3DFMT_A2R10G10B10?0x3fffffff:0xffffff;
                DWORD expected=format==D3DFMT_A2R10G10B10?0xffc00:0xff00;
                require((actual&mask)==expected,"retry presents newly rendered pixel");
                require((outside&mask)==0,"retry preserves background outside native draw");
            }
            printf("PASS COPY failed Reset/retry samples=%d format=%u rejected=%08lx pixel=green\n",samples,format,rejected);
        }
        renderBeforeReset();
    }
}
