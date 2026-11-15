// CPU-memory draws against the original UP APIs; independent hidden device.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_dx12.h"
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <string>
using Microsoft::WRL::ComPtr;
namespace {
void require(bool value,const char* label){if(!value){printf("FAIL UP: %s\n",label);exit(1);}}
void check(HRESULT hr,const char* label){if(FAILED(hr)){printf("FAIL UP: %s hr=%08lx\n",label,hr);exit(1);}}
#define CHECK(x) check((x),#x)
}
void testUserDraw(IDirect3DDevice9* d,int iteration) {
    ComPtr<IDirect3DStateBlock9> saved;CHECK(d->CreateStateBlock(D3DSBT_ALL,&saved));
    ComPtr<IDirect3DSurface9> original,depth,readback,resolved;
    CHECK(d->GetRenderTarget(0,&original));CHECK(d->GetDepthStencilSurface(&depth));
    CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr));
    ComPtr<IDirect3DVertexBuffer9> sentinel;ComPtr<IDirect3DIndexBuffer9> sentinelIndex;
    CHECK(d->CreateVertexBuffer(128,0,0,D3DPOOL_MANAGED,&sentinel,nullptr));
    CHECK(d->CreateIndexBuffer(12,0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&sentinelIndex,nullptr));
    ComPtr<IDirect3DTexture9> texture;CHECK(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr));
    D3DLOCKED_RECT lock;CHECK(texture->LockRect(0,&lock,nullptr,0));
    for(UINT y=0;y<32;++y)for(UINT x=0;x<32;++x)((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]=D3DCOLOR_ARGB(100+x*4,50+y*5,180,80+x*3);
    CHECK(texture->UnlockRect(0));CHECK(d->SetVertexShader(nullptr));CHECK(d->SetPixelShader(nullptr));
    for(auto state:{D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_SPECULARENABLE,D3DRS_STENCILENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHATESTENABLE,D3DRS_SCISSORTESTENABLE})CHECK(d->SetRenderState(state,FALSE));
    CHECK(d->SetRenderState(D3DRS_SHADEMODE,D3DSHADE_GOURAUD));CHECK(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
    CHECK(d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE));CHECK(d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA));CHECK(d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA));CHECK(d->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD));
    CHECK(d->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,TRUE));CHECK(d->SetRenderState(D3DRS_MULTISAMPLEMASK,UINT_MAX));
    D3DMATRIX identity={};for(int i=0;i<4;++i)identity.m[i][i]=1;
    for(auto state:{D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION})CHECK(d->SetTransform(state,&identity));
    CHECK(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    CHECK(d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));CHECK(d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));CHECK(d->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_CURRENT));
    for(auto state:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})CHECK(d->SetTextureStageState(0,state,D3DTOP_MODULATE));
    for(auto state:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})CHECK(d->SetTextureStageState(0,state,D3DTA_TEXTURE));
    for(auto state:{D3DTSS_COLORARG2,D3DTSS_ALPHAARG2})CHECK(d->SetTextureStageState(0,state,D3DTA_DIFFUSE));
    for(auto state:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})CHECK(d->SetSamplerState(0,state,D3DTEXF_LINEAR));
    CHECK(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));CHECK(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0));
    for(auto state:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV})CHECK(d->SetSamplerState(0,state,D3DTADDRESS_CLAMP));
    const DWORD fvfs[]={0x144,0x44,0x104,0x142,0x102,0x2};const UINT strides[]={28,20,24,24,20,12};
    for(UINT samples:{1u,4u}) {
        ComPtr<IDirect3DSurface9> target=original,targetDepth=depth;
        if(samples>1){target.Reset();targetDepth.Reset();CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,(D3DMULTISAMPLE_TYPE)samples,0,FALSE,&target,nullptr));CHECK(d->CreateDepthStencilSurface(256,256,D3DFMT_D24S8,(D3DMULTISAMPLE_TYPE)samples,0,TRUE,&targetDepth,nullptr));}
        CHECK(d->SetRenderTarget(0,target.Get()));CHECK(d->SetDepthStencilSurface(targetDepth.Get()));
        for(UINT layout=0;layout<6;++layout)for(auto topology:{D3DPT_TRIANGLELIST,D3DPT_TRIANGLESTRIP,D3DPT_TRIANGLEFAN})for(UINT kind=0;kind<3;++kind) {
            CHECK(d->SetFVF(fvfs[layout]));CHECK(d->SetTexture(0,layout==1 || layout==5?nullptr:texture.Get()));
            const UINT stride=strides[layout],minimum=kind==2?65538:kind?2:0,count=topology==D3DPT_TRIANGLELIST?6:4;
            const UINT order[]={0,1,2,2,1,3};
            auto capture=[&](bool native){
                D3DVIEWPORT9 full={0,0,256,256,0,1};CHECK(d->SetViewport(&full));CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff182838,1,0));CHECK(d->BeginScene());
                const UINT draws=layout==0 && topology==D3DPT_TRIANGLELIST && kind==2?1026:2;
                std::vector<BYTE> data(size_t(minimum+count)*stride);std::vector<DWORD> indices(count);std::vector<WORD> shortIndices(count);
                for(UINT draw=0;draw<draws;++draw){
                    D3DVIEWPORT9 vp=draws==1026?D3DVIEWPORT9{(draw%16)*16,((draw/16)%16)*16,16,16,0,1}:D3DVIEWPORT9{draw*128,0,128,256,0,1};CHECK(d->SetViewport(&vp));
                    const float uv[4][2]={{0,0},{1,0},{0,1},{1,1}};
                    for(UINT i=0;i<count;++i){UINT corner=topology==D3DPT_TRIANGLELIST?order[i]:topology==D3DPT_TRIANGLEFAN?(i==2?3:i==3?2:i):i;
                        BYTE* out=data.data()+size_t(minimum+i)*stride;float p[]={layout<3?vp.X+2.25f+uv[corner][0]*(vp.Width-5):-.8f+uv[corner][0]*1.6f,layout<3?vp.Y+2.5f+uv[corner][1]*(vp.Height-5):.8f-uv[corner][1]*1.6f,.3f,1};
                        memcpy(out,p,layout<3?16:12);DWORD color=0xc0f08050;
                        if(layout==0 || layout==1 || layout==3)memcpy(out+(layout<3?16:12),&color,4);
                        if(layout!=1 && layout!=5)memcpy(out+stride-8,uv[corner],8);
                        indices[i]=minimum+i;shortIndices[i]=WORD(minimum+i);
                    }
                    CHECK(d->SetStreamSource(0,sentinel.Get(),4,20));CHECK(d->SetStreamSource(1,sentinel.Get(),8,4));CHECK(d->SetIndices(sentinelIndex.Get()));
                    const void* indexData=kind==2?(const void*)indices.data():(const void*)shortIndices.data();D3DFORMAT format=kind==2?D3DFMT_INDEX32:D3DFMT_INDEX16;
                    if(native){bool handled=kind?model12DrawIndexedUP(d,topology,minimum,count,2,indexData,format,data.data(),stride):model12DrawUP(d,topology,2,data.data(),stride);
                        terrain12RecordDraw(d,kind?3:2,topology,handled,stride);require(handled,"qualified UP draw accepted");}
                    else CHECK(kind?d->DrawIndexedPrimitiveUP(topology,minimum,count,2,indexData,format,data.data(),stride):d->DrawPrimitiveUP(topology,2,data.data(),stride));
                    ComPtr<IDirect3DVertexBuffer9> vb;UINT offset,step;CHECK(d->GetStreamSource(0,&vb,&offset,&step));require(!vb,"UP clears stream zero");
                    CHECK(d->GetStreamSource(1,&vb,&offset,&step));require(vb.Get()==sentinel.Get() && offset==8 && step==4,"UP preserves other streams");
                    ComPtr<IDirect3DIndexBuffer9> ib;CHECK(d->GetIndices(&ib));require(kind?!ib:ib.Get()==sentinelIndex.Get(),"UP index binding postcondition");
                    // Queued native commands must own copies, not these caller arrays.
                    memset(data.data()+size_t(minimum)*stride,0,count*stride);std::fill(indices.begin(),indices.end(),0);std::fill(shortIndices.begin(),shortIndices.end(),0);
                }
                if(native)terrain12Flush();CHECK(d->EndScene());
                if(samples>1)CHECK(d->StretchRect(target.Get(),nullptr,resolved.Get(),nullptr,D3DTEXF_NONE));
                CHECK(d->GetRenderTargetData(samples>1?resolved.Get():target.Get(),readback.Get()));
                D3DLOCKED_RECT pixels;CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));std::vector<DWORD> result(65536);
                for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)result[y*256+x]=((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x]&0xffffff;
                CHECK(readback->UnlockRect());return result;
            };
            auto reference=capture(false),actual=capture(true);UINT maximum=0,bad=0;
            for(UINT i=0;i<65536;++i)for(UINT shift=0;shift<24;shift+=8){UINT delta=abs(int((reference[i]>>shift)&255)-int((actual[i]>>shift)&255));maximum=std::max(maximum,delta);if(delta>2)++bad;}
            printf("UP parity iteration=%d layout=%u samples=%u topology=%u kind=%u max_error=%u bad_channels=%u\n",iteration,layout,samples,topology,kind,maximum,bad);
            require(std::count(reference.begin(),reference.end(),0x182838)<64500,"reference visible");require(!bad,"UP RGB parity within 2/255");
        }
        CHECK(d->SetRenderTarget(0,original.Get()));CHECK(d->SetDepthStencilSurface(depth.Get()));
    }
    CHECK(d->SetFVF(0x44));CHECK(d->SetTexture(0,nullptr));CHECK(d->SetStreamSource(0,sentinel.Get(),4,20));CHECK(d->SetIndices(sentinelIndex.Get()));
    BYTE data[120]={};WORD invalid[]={0,1,9};float rhw=1;
    for(UINT i=0;i<3;++i)memcpy(data+i*20+12,&rhw,4);
    require(!model12DrawUP(d,D3DPT_TRIANGLELIST,UINT_MAX,data,20),"overflow refused");
    require(!model12DrawIndexedUP(d,D3DPT_TRIANGLELIST,UINT_MAX,UINT_MAX,1,invalid,D3DFMT_INDEX32,data,UINT_MAX),"combined range overflow refused before reading caller memory");
    require(!model12DrawUP(d,D3DPT_TRIANGLELIST,1,nullptr,20),"null memory refused");
    require(!model12DrawUP(d,D3DPT_TRIANGLELIST,1,data,24),"FVF stride mismatch refused");
    require(!model12DrawIndexedUP(d,D3DPT_TRIANGLELIST,0,3,1,invalid,D3DFMT_INDEX16,data,20),"out of range index refused");
    require(!model12DrawUP(d,D3DPT_LINELIST,1,data,20),"unsupported topology refused");
    CHECK(d->SetStreamSourceFreq(0,D3DSTREAMSOURCE_INDEXEDDATA|2));require(!model12DrawUP(d,D3DPT_TRIANGLELIST,1,data,20),"instanced UP falls back");CHECK(d->SetStreamSourceFreq(0,1));
    CHECK(d->SetRenderState(D3DRS_STENCILENABLE,TRUE));require(!model12DrawUP(d,D3DPT_TRIANGLELIST,1,data,20),"unsupported state refused");CHECK(d->SetRenderState(D3DRS_STENCILENABLE,FALSE));
    ComPtr<IDirect3DVertexBuffer9> bound;UINT offset,stride;CHECK(d->GetStreamSource(0,&bound,&offset,&stride));require(bound.Get()==sentinel.Get() && offset==4 && stride==20,"fallback preserves stream binding");
    ComPtr<IDirect3DIndexBuffer9> index;CHECK(d->GetIndices(&index));require(index.Get()==sentinelIndex.Get(),"fallback preserves index binding");
    terrain12RecordDraw(d,2,D3DPT_LINELIST,false,20);
    CHECK(d->SetFVF(0x112));require(!model12DrawUP(d,D3DPT_TRIANGLELIST,1,data,32),"unknown FVF stays on compatibility route");
    terrain12RecordDraw(d,2,D3DPT_TRIANGLELIST,false,32);CHECK(d->SetFVF(0x44));
    for(UINT i=1;i<=140;++i)terrain12RecordDraw(d,2,D3DPT_LINESTRIP,false,i);
    FILE* report=nullptr;require(tmpfile_s(&report)==0,"diagnostic report");terrain12Status(report);rewind(report);
    std::string text;char chunk[4096];size_t length;while((length=fread(chunk,1,sizeof chunk,report)))text.append(chunk,length);fclose(report);
    require(text.find("Native API DPUP attempts=214 native=72")!=std::string::npos && text.find("Native API DIPUP attempts=2192 native=2192")!=std::string::npos,"one API counter per accepted or fallback call");
    auto census=text.find("Native fallback census enabled=");require(census!=std::string::npos,"fallback census present");
    unsigned enabled=0;size_t entries=0;unsigned long long overflow=0;
    require(sscanf_s(text.c_str()+census,"Native fallback census enabled=%u entries=%zu overflow=%llu",&enabled,&entries,&overflow)==3,"census fields readable");
    require(enabled?(entries==128 && overflow>=14):(entries==0 && overflow==0),"opt-in fallback census is bounded with overflow counted");
    terrain12Status(stdout);CHECK(saved->Apply());puts("PASS UP caller lifetime/state clearing/index16/index32/rebase/fallback/MSAA");
}
