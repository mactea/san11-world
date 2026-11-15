// Original D3D9 XYZRHW pipeline against production DX12. No game window.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_dx12.h"
#include "../src/object_capture.h"
#include "../src/terrain_buffer_hooks.h"
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
using Microsoft::WRL::ComPtr;
namespace {
void require(bool v,const char* s){if(!v){printf("FAIL screen: %s\n",s);exit(1);}}
void check(HRESULT h,const char* s){if(FAILED(h)){printf("FAIL screen: %s %08lx\n",s,h);exit(1);}}
#define CHECK(x) check((x),#x)
}
void testScreenRender(IDirect3DDevice9* d,int iteration) {
    ComPtr<IDirect3DStateBlock9> saved;CHECK(d->CreateStateBlock(D3DSBT_ALL,&saved));
    ComPtr<IDirect3DSurface9> original,depth,readback,resolved;
    CHECK(d->GetRenderTarget(0,&original));CHECK(d->GetDepthStencilSurface(&depth));
    CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr));
    CHECK(d->SetVertexShader(nullptr));CHECK(d->SetPixelShader(nullptr));
    auto rs=[&](D3DRENDERSTATETYPE s,DWORD v){CHECK(d->SetRenderState(s,v));};
    for(auto state:{D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_SPECULARENABLE,D3DRS_STENCILENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_SEPARATEALPHABLENDENABLE})rs(state,FALSE);
    rs(D3DRS_SHADEMODE,D3DSHADE_GOURAUD);rs(D3DRS_CULLMODE,D3DCULL_NONE);rs(D3DRS_ZENABLE,FALSE);rs(D3DRS_ZWRITEENABLE,FALSE);
    rs(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);rs(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);rs(D3DRS_BLENDOP,D3DBLENDOP_ADD);
    rs(D3DRS_ALPHAFUNC,D3DCMP_GREATER);rs(D3DRS_ALPHAREF,80);rs(D3DRS_MULTISAMPLEANTIALIAS,TRUE);rs(D3DRS_MULTISAMPLEMASK,UINT_MAX);
    CHECK(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    CHECK(d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));CHECK(d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
    CHECK(d->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_CURRENT));
    for(auto state:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})CHECK(d->SetTextureStageState(0,state,D3DTOP_MODULATE));
    for(auto state:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})CHECK(d->SetTextureStageState(0,state,D3DTA_TEXTURE));
    for(auto state:{D3DTSS_COLORARG2,D3DTSS_ALPHAARG2})CHECK(d->SetTextureStageState(0,state,D3DTA_DIFFUSE));
    for(auto state:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})CHECK(d->SetSamplerState(0,state,D3DTEXF_LINEAR));
    CHECK(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));CHECK(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0));
    for(auto state:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV})CHECK(d->SetSamplerState(0,state,D3DTADDRESS_CLAMP));
    ComPtr<IDirect3DTexture9> textures[4];const D3DFORMAT formats[]={D3DFMT_A8R8G8B8,D3DFMT_A8,D3DFMT_L8,D3DFMT_A8L8};
    auto paint=[&](UINT which,UINT phase){D3DLOCKED_RECT lock;CHECK(textures[which]->LockRect(0,&lock,nullptr,0));
        for(UINT y=0;y<32;++y)for(UINT x=0;x<32;++x){BYTE* row=(BYTE*)lock.pBits+y*lock.Pitch;
            BYTE alpha=BYTE(((x+phase)%8<4 && y%8<6)?220:20),luma=BYTE(30+y*6);
            if(which==0)((DWORD*)row)[x]=D3DCOLOR_ARGB(alpha,luma,120,200);
            else if(which==1)row[x]=alpha;else if(which==2)row[x]=luma;else ((WORD*)row)[x]=WORD((alpha<<8)|luma);}
        CHECK(textures[which]->UnlockRect(0));terrain12TextureChanged(textures[which].Get());};
    for(UINT i=0;i<4;++i){CHECK(d->CreateTexture(32,32,1,0,formats[i],D3DPOOL_MANAGED,&textures[i],nullptr));paint(i,0);}
    for(UINT samples:{1u,4u}) {
        ComPtr<IDirect3DSurface9> target=original,targetDepth=depth;
        if(samples>1){target.Reset();targetDepth.Reset();CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,(D3DMULTISAMPLE_TYPE)samples,0,FALSE,&target,nullptr));CHECK(d->CreateDepthStencilSurface(256,256,D3DFMT_D24S8,(D3DMULTISAMPLE_TYPE)samples,0,TRUE,&targetDepth,nullptr));}
        CHECK(d->SetRenderTarget(0,target.Get()));CHECK(d->SetDepthStencilSurface(targetDepth.Get()));
        for(UINT layout=0;layout<3;++layout) {
            DWORD fvf=layout==0?0x144:layout==1?0x44:0x104;UINT stride=layout==0?28:layout==1?20:24;
            CHECK(d->SetFVF(fvf));
            ComPtr<IDirect3DVertexBuffer9> vb;ComPtr<IDirect3DIndexBuffer9> ib;
            CHECK(d->CreateVertexBuffer(stride*8,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,fvf,D3DPOOL_DEFAULT,&vb,nullptr));
            CHECK(d->CreateIndexBuffer(24,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&ib,nullptr));
            IDirect3DResource9* resources[]={vb.Get(),ib.Get()};
            for(UINT i=0;i<2;++i){terrain12ForgetBuffer(resources[i]);objectCaptureRegister(resources[i],i?24:stride*8);if(iteration==2)terrain12HookBuffer(resources[i]);}
            auto write=[&](UINT stream,const void* bytes,UINT size){void* mapped;CHECK(stream?ib->Lock(0,size,&mapped,D3DLOCK_DISCARD):vb->Lock(0,size,&mapped,D3DLOCK_DISCARD));
                void* memory=iteration==2?mapped:objectCaptureLock(resources[stream],0,size,mapped,D3DLOCK_DISCARD);memcpy(memory,bytes,size);
                if(iteration!=2)objectCaptureBeforeUnlock(resources[stream]);HRESULT hr=stream?ib->Unlock():vb->Unlock();if(iteration!=2)objectCaptureAfterUnlock(resources[stream],hr);CHECK(hr);};
            CHECK(d->SetStreamSource(0,vb.Get(),stride,stride));CHECK(d->SetIndices(ib.Get()));
            for(UINT scenario=0;scenario<18;++scenario) {
                const bool lines=scenario>=12;
                const UINT atlas=lines?0:scenario==10?1:scenario==11?3:scenario>=7?scenario-6:0;
                CHECK(d->SetDepthStencilSurface(scenario==8?nullptr:targetDepth.Get()));
                CHECK(d->SetTexture(0,layout==1 || scenario>=16?nullptr:textures[atlas].Get()));
                CHECK(d->SetTextureStageState(0,D3DTSS_COLOROP,scenario==10 && layout!=1?D3DTOP_SELECTARG2:D3DTOP_MODULATE));
                rs(D3DRS_ZENABLE,scenario==11);rs(D3DRS_ZWRITEENABLE,scenario==11);
                rs(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
                rs(D3DRS_TEXTUREFACTOR,0xb060c080);
                for(auto state:{D3DTSS_COLORARG2,D3DTSS_ALPHAARG2})CHECK(d->SetTextureStageState(0,state,scenario==9 && layout!=1?D3DTA_TFACTOR:D3DTA_DIFFUSE));
                rs(D3DRS_SCISSORTESTENABLE,scenario==1 || scenario==14);RECT clip={31,27,177,169};CHECK(d->SetScissorRect(&clip));
                rs(D3DRS_ANTIALIASEDLINEENABLE,FALSE);
                rs(D3DRS_ALPHATESTENABLE,scenario==3);rs(D3DRS_ALPHABLENDENABLE,scenario==2 || scenario==5 || scenario>=7);
                D3DPRIMITIVETYPE type=lines?(scenario%2?D3DPT_LINELIST:D3DPT_LINESTRIP):(scenario==4 || scenario==10)?D3DPT_TRIANGLEFAN:scenario==6?D3DPT_TRIANGLELIST:D3DPT_TRIANGLESTRIP;
                const UINT primitives=type==D3DPT_LINESTRIP?3:2;
                bool indexed=scenario==10?false:scenario%2==0;
                WORD indices[12]={0,0,2,3,4,5};if(type==D3DPT_TRIANGLELIST){indices[5]=4;indices[6]=3;indices[7]=5;}write(1,indices,sizeof indices);
                auto drawNative=[&](){return indexed?model12Draw(d,type,1,2,4,2,primitives):model12DrawPrimitive(d,type,3,primitives);};
                auto drawOriginal=[&](){return indexed?d->DrawIndexedPrimitive(type,1,2,4,2,primitives):d->DrawPrimitive(type,3,primitives);};
                auto capture=[&](bool native){
                    paint(atlas,0);D3DVIEWPORT9 full={0,0,256,256,0,1};CHECK(d->SetViewport(&full));CHECK(d->Clear(0,nullptr,(D3DCLEAR_TARGET|(scenario==8?0:D3DCLEAR_ZBUFFER)),0xff182838,1,0));CHECK(d->BeginScene());
                    UINT count=scenario==6?1026u:2u;
                    for(UINT draw=0;draw<count;++draw){
                        D3DVIEWPORT9 vp=scenario==6?D3DVIEWPORT9{(draw%16)*16,((draw/16)%16)*16,16,16,0,1}:D3DVIEWPORT9{draw*128+7,11,113,224,0,1};if(scenario==11){vp.MinZ=.2f;vp.MaxZ=.8f;}CHECK(d->SetViewport(&vp));
                        float x=vp.X+2.25f,y=vp.Y+2.5f,w=vp.Width-5.0f,h=vp.Height-5.0f;
                        float positions[4][4]={{x,y,.4f,1},{x+w,y,.4f,scenario==2?.7f:1},{x,y+h,.4f,1},{x+w,y+h,.4f,1}};
                        if(type==D3DPT_TRIANGLEFAN)std::swap(positions[2],positions[3]);
                        const float uv[4][2]={{0,0},{1,0},{0,1},{1,1}};std::vector<BYTE> vertices(stride*8);
                        for(UINT i=0;i<4;++i){BYTE* out=vertices.data()+(4+i)*stride;memcpy(out,positions[i],16);DWORD color=0xc0f08040;
                            if(layout!=2)memcpy(out+16,&color,4);if(layout!=1)memcpy(out+(layout==0?20:16),uv[i],8);}
                        write(0,vertices.data(),UINT(vertices.size()));
                        if(draw==1 && scenario>=7)paint(atlas,3);
                        if(native)require(drawNative(),"qualified pretransformed draw accepted");else CHECK(drawOriginal());
                        if(scenario==5){if(native)terrain12Flush();CHECK(drawOriginal());}
                    }
                    if(native)terrain12Flush();CHECK(d->EndScene());
                    if(samples>1)CHECK(d->StretchRect(target.Get(),nullptr,resolved.Get(),nullptr,D3DTEXF_NONE));
                    CHECK(d->GetRenderTargetData(samples>1?resolved.Get():target.Get(),readback.Get()));
                    D3DLOCKED_RECT lock;CHECK(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));std::vector<DWORD> pixels(65536);
                    for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)pixels[y*256+x]=((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]&0xffffff;
                    CHECK(readback->UnlockRect());return pixels;
                };
                auto reference=capture(false),native=capture(true);UINT maximum=0,bad=0;
                for(UINT i=0;i<65536;++i)for(UINT shift=0;shift<24;shift+=8){UINT error=abs(int((reference[i]>>shift)&255)-int((native[i]>>shift)&255));maximum=std::max(maximum,error);if(error>2)++bad;}
                printf("Screen parity iteration=%d layout=%u samples=%u case=%u max_error=%u bad_channels=%u\n",iteration,layout,samples,scenario,maximum,bad);
                if(bad){UINT shown=0;for(UINT i=0;i<65536 && shown<5;++i)if(reference[i]!=native[i]){printf("Screen mismatch x=%u y=%u old=%06lx native=%06lx\n",i%256,i/256,reference[i],native[i]);++shown;}}
                require(std::count(reference.begin(),reference.end(),0x182838)<(lines?65400:65000),"visible reference");require(!bad,"screen RGB parity within 2/255");
            }
            auto before=terrain12TestCoverage(15+layout);
            rs(D3DRS_FOGENABLE,TRUE);require(!model12DrawPrimitive(d,D3DPT_TRIANGLESTRIP,3,2),"unsupported screen fog rejected");rs(D3DRS_FOGENABLE,FALSE);
            auto after=terrain12TestCoverage(15+layout);
            require(after.recognized==before.recognized+1 && after.rejected[2]==before.rejected[2]+1,"unsupported state recorded as fallback, not a native draw");
            for(auto resource:resources)objectCaptureForget(resource);
        }
        terrain12Flush();CHECK(d->SetRenderTarget(0,original.Get()));CHECK(d->SetDepthStencilSurface(depth.Get()));
    }
    CHECK(saved->Apply());puts("PASS screen layouts/scissor/perspective/alpha/fan/atlas rewrite/MSAA/handoff");
}
