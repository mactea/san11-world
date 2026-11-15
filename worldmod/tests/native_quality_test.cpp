// Production native sampling and multisampled target handoff; no game process.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_dx12.h"
#include "../src/terrain_filter.h"
#include "../src/render_copy_msaa.h"
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <stdlib.h>
using Microsoft::WRL::ComPtr;
namespace {
void require(bool v,const char* s){if(!v){printf("FAIL native quality: %s\n",s);exit(1);}}
void check(HRESULT h,const char* s){if(FAILED(h)){printf("FAIL native quality: %s %08lx\n",s,h);exit(1);}}
#define CHECK(x) check((x),#x)
}
void testNativeQuality(IDirect3DDevice9* d,IDirect3DVertexShader9* const* shaders,int iteration) {
    ComPtr<IDirect3DStateBlock9> saved;CHECK(d->CreateStateBlock(D3DSBT_ALL,&saved));
    ComPtr<IDirect3DSurface9> originalTarget,originalDepth,readback,resolved;
    CHECK(d->GetRenderTarget(0,&originalTarget));CHECK(d->GetDepthStencilSurface(&originalDepth));
    CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr));
    struct Position {float x,y,z,nx,ny,nz;};struct Material {DWORD color;float u,v;};
    const Position quad[]={{-1,1,.2f,0,0,1},{1,1,.2f,0,0,1},{-1,-1,.2f,0,0,1},{1,-1,.2f,0,0,1}};
    const Material materials[]={{0xffffffff,0,0},{0xffffffff,8,0},{0xffffffff,0,1},{0xffffffff,8,1}};
    const WORD indices[]={0,1,2,2,1,3};
    ComPtr<IDirect3DVertexBuffer9> vb[2];ComPtr<IDirect3DIndexBuffer9> ib;
    CHECK(d->CreateVertexBuffer(sizeof quad,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb[0],nullptr));
    CHECK(d->CreateVertexBuffer(sizeof materials,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb[1],nullptr));
    CHECK(d->CreateIndexBuffer(sizeof indices,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&ib,nullptr));
    IDirect3DResource9* buffers[]={vb[0].Get(),vb[1].Get(),ib.Get(),nullptr};terrain12TestBind(buffers,shaders);
    void* mapped;
    for(UINT s=0;s<2;++s){CHECK(vb[s]->Lock(0,0,&mapped,D3DLOCK_DISCARD));memcpy(terrain12Lock(vb[s].Get(),0,0,mapped,D3DLOCK_DISCARD),s?(void*)materials:(void*)quad,s?sizeof materials:sizeof quad);terrain12Unlock(vb[s].Get());CHECK(vb[s]->Unlock());}
    CHECK(ib->Lock(0,0,&mapped,D3DLOCK_DISCARD));memcpy(terrain12Lock(ib.Get(),0,0,mapped,D3DLOCK_DISCARD),indices,sizeof indices);terrain12Unlock(ib.Get());CHECK(ib->Unlock());
    CHECK(d->SetStreamSource(0,vb[0].Get(),0,sizeof(Position)));CHECK(d->SetStreamSource(1,vb[1].Get(),0,sizeof(Material)));CHECK(d->SetIndices(ib.Get()));
    CHECK(d->SetVertexShader(shaders[0]));CHECK(d->SetPixelShader(nullptr));
    const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},{0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},
        {1,0,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},{1,4,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    ComPtr<IDirect3DVertexDeclaration9> decl;CHECK(d->CreateVertexDeclaration(elements,&decl));CHECK(d->SetVertexDeclaration(decl.Get()));
    float c[28][4]={};for(int i=0;i<4;++i){c[1][i]=1;c[6+i][i]=1;c[10+i][i]=1;}c[27][1]=1;
    CHECK(d->SetVertexShaderConstantF(0,&c[0][0],28));
    for(auto state:{D3DRS_FOGENABLE,D3DRS_ALPHATESTENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_SCISSORTESTENABLE})CHECK(d->SetRenderState(state,FALSE));
    CHECK(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));CHECK(d->SetRenderState(D3DRS_ZENABLE,TRUE));CHECK(d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE));CHECK(d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL));
    CHECK(d->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,TRUE));CHECK(d->SetRenderState(D3DRS_MULTISAMPLEMASK,UINT_MAX));
    CHECK(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    for(auto state:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})CHECK(d->SetTextureStageState(0,state,D3DTOP_MODULATE));
    for(auto state:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})CHECK(d->SetTextureStageState(0,state,D3DTA_TEXTURE));
    for(auto state:{D3DTSS_COLORARG2,D3DTSS_ALPHAARG2})CHECK(d->SetTextureStageState(0,state,D3DTA_DIFFUSE));
    ComPtr<IDirect3DTexture9> texture;CHECK(d->CreateTexture(256,256,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr));
    for(UINT level=0;level<texture->GetLevelCount();++level) {
        D3DSURFACE_DESC desc;D3DLOCKED_RECT pixels;CHECK(texture->GetLevelDesc(level,&desc));CHECK(texture->LockRect(level,&pixels,nullptr,0));
        UINT scale=1u<<level;
        for(UINT y=0;y<desc.Height;++y){UINT sum=0;for(UINT i=0;i<scale;++i)sum+=(((y*scale+i)/4)%2)*255;
            for(UINT x=0;x<desc.Width;++x)((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x]=0xff000000|(sum/scale)*0x010101;}
        CHECK(texture->UnlockRect(level));
    }
    CHECK(d->SetTexture(0,texture.Get()));
    for(auto state:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER})CHECK(d->SetSamplerState(0,state,D3DTEXF_LINEAR));
    CHECK(d->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,1));CHECK(d->SetSamplerState(0,D3DSAMP_MAXMIPLEVEL,0));CHECK(d->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,0));
    auto capture=[&](bool native,int anisotropy,IDirect3DSurface9* target,bool multisample,bool copy=false) {
        terrainFilterConfigure(anisotropy,nullptr);terrainFilterOnDevice(d);
        D3DVIEWPORT9 viewport={0,0,256,256,0,1};CHECK(d->SetViewport(&viewport));
        CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff182838,1,0));CHECK(d->BeginScene());
        {TerrainFilterScope filter;filter.apply(d,shaders[0]);
            if(native)require(terrain12Draw(d,D3DPT_TRIANGLELIST,0,0,4,0,2),"quality draw accepted");
            else CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,4,0,2));}
        if(native)terrain12Flush();CHECK(d->EndScene());
        DWORD restored;CHECK(d->GetSamplerState(0,D3DSAMP_MINFILTER,&restored));require(restored==D3DTEXF_LINEAR,"MINFILTER restored");
        CHECK(d->GetSamplerState(0,D3DSAMP_MAXANISOTROPY,&restored));require(restored==1,"anisotropy restored");
        if(copy)CHECK(copyMsaaResolve(d));
        else if(multisample)CHECK(d->StretchRect(target,nullptr,resolved.Get(),nullptr,D3DTEXF_NONE));
        CHECK(d->GetRenderTargetData(multisample&&!copy?resolved.Get():target,readback.Get()));
        D3DLOCKED_RECT pixels;CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));std::vector<DWORD> result(256*256);
        for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)result[y*256+x]=((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x]&0xffffff;
        CHECK(readback->UnlockRect());return result;
    };
    auto compare=[&](const std::vector<DWORD>& a,const std::vector<DWORD>& b){unsigned maximum=0;
        for(size_t i=0;i<a.size();++i)for(int shift=0;shift<24;shift+=8)maximum=std::max(maximum,unsigned(abs(int((a[i]>>shift)&255)-int((b[i]>>shift)&255))));
        require(maximum<=2,"stock/native RGB tolerance");return maximum;};
    auto baseline=capture(true,0,originalTarget.Get(),false);
    for(int af:{2,4,8,16}) {
        auto reference=capture(false,af,originalTarget.Get(),false),native=capture(true,af,originalTarget.Get(),false);
        unsigned maximum=compare(reference,native);double contrast=0,linear=0;
        for(UINT y=32;y<224;++y)for(UINT x=32;x<224;++x){contrast+=abs(int(native[y*256+x]&255)-127);linear+=abs(int(baseline[y*256+x]&255)-127);}
        if(af>=8)require(contrast>linear+10*192*192,"AF preserves stripe contrast");
        printf("Native quality AF=%d iteration=%d max_error=%u contrast=%.3f baseline=%.3f\n",af,iteration,maximum,contrast/(192*192),linear/(192*192));
    }
    require(baseline==capture(true,0,originalTarget.Get(),false),"native AF off/on/off exact baseline");
    // Sloped edges and shared depth, resolved through D3D9 after native resource return.
    Position sloped[4];memcpy(sloped,quad,sizeof quad);sloped[0].x=-.73f;sloped[1].y=.63f;sloped[2].x=-.21f;sloped[3].y=-.81f;
    CHECK(vb[0]->Lock(0,0,&mapped,D3DLOCK_DISCARD));memcpy(terrain12Lock(vb[0].Get(),0,0,mapped,D3DLOCK_DISCARD),sloped,sizeof sloped);terrain12Unlock(vb[0].Get());CHECK(vb[0]->Unlock());
    auto jagged=capture(true,0,originalTarget.Get(),false);
    ComPtr<IDirect3DSwapChain9> chain;CHECK(d->GetSwapChain(0,&chain));
    D3DPRESENT_PARAMETERS pp={};CHECK(chain->GetPresentParameters(&pp));
    require(pp.SwapEffect==D3DSWAPEFFECT_COPY,"COPY fixture");
    for(UINT count:{2u,4u,8u}) {
        copyMsaaInit(d,pp,count);
        ComPtr<IDirect3DSurface9> color,depth;
        CHECK(d->GetRenderTarget(0,&color));CHECK(d->GetDepthStencilSurface(&depth));
        D3DSURFACE_DESC desc={};CHECK(color->GetDesc(&desc));require(desc.MultiSampleType==count,"COPY color samples");
        CHECK(depth->GetDesc(&desc));require(desc.MultiSampleType==count,"COPY shared depth samples");
        auto reference=capture(false,0,originalTarget.Get(),true,true),native=capture(true,0,originalTarget.Get(),true,true);
        unsigned edgePixels=0;
        for(size_t i=0;i<native.size();++i)if(jagged[i]==0x182838 && native[i]!=jagged[i])++edgePixels;
        require(edgePixels>50,"MSAA adds partial coverage on sloped edges");
        printf("Native quality COPY MSAA=%u iteration=%d max_error=%u edge_pixels=%u\n",count,iteration,compare(reference,native),edgePixels);
        CHECK(renderReadback(d,color.Get(),readback.Get()));
        D3DLOCKED_RECT pixels;CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
        for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)
            require((((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x]&0xffffff)==native[y*256+x],"screenshot matches resolved COPY backbuffer");
        CHECK(readback->UnlockRect());
        CHECK(d->Present(nullptr,nullptr,nullptr,nullptr));
        // Compatibility work in a later scene must retain native color AND depth.
        CHECK(d->BeginScene());CHECK(d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESS));
        CHECK(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));CHECK(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TFACTOR));
        CHECK(d->SetRenderState(D3DRS_TEXTUREFACTOR,0xffff0000));
        CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,4,0,2));CHECK(d->EndScene());
        CHECK(copyMsaaResolve(d));CHECK(d->GetRenderTargetData(originalTarget.Get(),readback.Get()));
        CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
        for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)
            require((((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x]&0xffffff)==native[y*256+x],"COPY retains native depth against equal-depth fallback");
        CHECK(readback->UnlockRect());
        CHECK(d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL));CHECK(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE));CHECK(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE));
        D3DRECT rect={4,4,12,12};CHECK(d->Clear(1,&rect,D3DCLEAR_TARGET,0xffabcdef,1,0));
        CHECK(copyMsaaResolve(d));CHECK(d->Present(nullptr,nullptr,nullptr,nullptr));
        CHECK(d->GetRenderTargetData(originalTarget.Get(),readback.Get()));CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
        for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)
            require((((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x]&0xffffff)==(x>=4&&x<12&&y>=4&&y<12?0xabcdef:native[y*256+x]),"COPY partial update retains previous frame");
        CHECK(readback->UnlockRect());
        require(copyMsaaColor(d,0,originalTarget.Get())==color.Get() && copyMsaaDepth(d,originalDepth.Get())==depth.Get(),"saved backbuffer/depth redirect");
        require(copyMsaaColor(d,1,originalTarget.Get())==originalTarget.Get() && copyMsaaColor(d,0,resolved.Get())==resolved.Get() && !copyMsaaDepth(d,nullptr),"unrelated targets unchanged");
        CHECK(d->SetDepthStencilSurface(nullptr));CHECK(d->SetRenderTarget(0,resolved.Get()));
        CHECK(d->SetRenderTarget(0,copyMsaaColor(d,0,originalTarget.Get())));CHECK(d->SetDepthStencilSurface(copyMsaaDepth(d,originalDepth.Get())));
        copyMsaaReset();
        CHECK(d->SetDepthStencilSurface(nullptr));CHECK(d->SetRenderTarget(0,originalTarget.Get()));CHECK(d->SetDepthStencilSurface(originalDepth.Get()));
    }
    copyMsaaInit(d,pp,0);require(jagged==capture(true,0,originalTarget.Get(),false),"COPY MSAA off/on/off exact baseline");
    terrainFilterConfigure(0,nullptr);terrainFilterOnDevice(d);
    for(auto r:buffers)if(r)terrain12ForgetBuffer(r);
    CHECK(saved->Apply());puts("PASS native AF and multisample targets/resolve");
}
