// Synthetic texture lifecycle tests against the stock shader and D3D9 rasterizer.
// Texture and surface mutations exercise the production vtable hooks.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_dx12.h"
#include "../src/terrain_texture_hooks.h"
#include "../src/terrain_replacement.h"
#include <wrl/client.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <stdlib.h>
#include <string.h>
using Microsoft::WRL::ComPtr;

namespace {
void require(bool value,const char* label) {
    if(!value) { printf("FAIL texture: %s\n",label); exit(1); }
}
void check(HRESULT hr,const char* label) {
    if(FAILED(hr)) { printf("FAIL texture: %s hr=%08lx\n",label,hr); exit(1); }
}
#define CHECK(x) check((x),#x)
using Pixels=std::vector<DWORD>;
void compare(const char* name,const Pixels& reference,const Pixels& actual) {
    unsigned maximum=0,bad=0,covered=0; double sum=0;
    for(size_t i=0;i<reference.size();++i) {
        if(reference[i]!=0x182838)++covered;
        unsigned error=0;
        for(int shift=0;shift<24;shift+=8) {
            unsigned delta=abs(int((reference[i]>>shift)&255)-int((actual[i]>>shift)&255));
            error=std::max(error,delta); sum+=delta;
        }
        maximum=std::max(maximum,error); if(error>2)++bad;
    }
    printf("Texture parity case=%s covered=%u max_error=%u bad_pixels=%u mae=%.6f\n",
        name,covered,maximum,bad,sum/(reference.size()*3));
    require(covered>1000,"reference actually rendered");
    require(bad==0,"RGB parity within 2/255 per channel");
}
void fill(IDirect3DTexture9* texture,unsigned phase,int onlyLevel=-1,bool viaSurface=false,DWORD flags=0) {
    for(UINT level=0;level<texture->GetLevelCount();++level) {
        if(onlyLevel>=0 && level!=UINT(onlyLevel))continue;
        D3DSURFACE_DESC desc; CHECK(texture->GetLevelDesc(level,&desc));
        ComPtr<IDirect3DSurface9> surface;
        if(viaSurface)CHECK(texture->GetSurfaceLevel(level,&surface));
        D3DLOCKED_RECT lock;
        CHECK(viaSurface?surface->LockRect(&lock,nullptr,flags):texture->LockRect(level,&lock,nullptr,flags));
        if(desc.Format==D3DFMT_A4R4G4B4) {
            for(UINT y=0;y<desc.Height;++y)for(UINT x=0;x<desc.Width;++x)
                ((WORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]=0x8000|(((x+phase*3)&15)<<8)|(((y+phase*5)&15)<<4)|9;
        } else {
            const UINT bytes=desc.Format==D3DFMT_DXT1?8:16;
            for(UINT y=0;y<std::max(1u,(desc.Height+3)/4);++y)
                for(UINT x=0;x<std::max(1u,(desc.Width+3)/4);++x) {
                    BYTE* block=(BYTE*)lock.pBits+y*lock.Pitch+x*bytes;
                    unsigned seed=x+y*3+level*7+phase*11;
                    WORD hi=WORD(0x8000|(((seed*5)&31)<<10)|(((seed*9)&63)<<4)|15);
                    WORD lo=WORD(0x0400|((seed*13)&0x3ff));
                    BYTE* color=block+(bytes-8);
                    WORD ends[]={hi,lo};
                    if(desc.Format==D3DFMT_DXT1 && (seed&1))std::swap(ends[0],ends[1]);
                    memcpy(color,ends,4);
                    DWORD indices=0;
                    for(UINT p=0;p<16;++p)indices|=((p+seed)&3)<<(p*2);
                    memcpy(color+4,&indices,4);
                    if(desc.Format==D3DFMT_DXT3) {
                        UINT64 alpha=0;
                        for(UINT p=0;p<16;++p)alpha|=UINT64(((p+seed)&3)*5)<<(p*4);
                        memcpy(block,&alpha,8);
                    } else if(desc.Format==D3DFMT_DXT5) {
                        block[0]=(seed&1)?30:230; block[1]=(seed&1)?210:20;
                        UINT64 alpha=0;
                        for(UINT p=0;p<16;++p)alpha|=UINT64((p+seed)&7)<<(p*3);
                        memcpy(block+2,&alpha,6);
                    }
                }
        }
        CHECK(viaSurface?surface->UnlockRect():texture->UnlockRect(level));
    }
    // D3D9 tracks dirty regions at level zero, including edits to lower mips.
    if(onlyLevel>0)CHECK(texture->AddDirtyRect(nullptr));
}
}

void testTerrain12Textures(IDirect3DDevice9* d,IDirect3DVertexShader9* shader,IDirect3DTexture9* retained,int iteration) {
    terrain12HookTextureUpdates(d);
    terrain12HookTexture(retained);
    float c[28][4]={}; for(int i=0;i<4;++i) {c[1][i]=1;c[10+i][i]=1;} c[27][1]=1;
    CHECK(d->SetVertexShaderConstantF(0,&c[0][0],28)); CHECK(d->SetVertexShader(shader));
    for(auto state:{D3DRS_FOGENABLE,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHATESTENABLE})
        CHECK(d->SetRenderState(state,FALSE));
    CHECK(d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE));
    for(auto state:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})CHECK(d->SetTextureStageState(0,state,D3DTOP_SELECTARG1));
    for(auto state:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV})CHECK(d->SetSamplerState(0,state,D3DTADDRESS_WRAP));
    auto sampler=[&](DWORD filter,DWORD mip,DWORD level=0) {
        CHECK(d->SetSamplerState(0,D3DSAMP_MINFILTER,filter)); CHECK(d->SetSamplerState(0,D3DSAMP_MAGFILTER,filter));
        CHECK(d->SetSamplerState(0,D3DSAMP_MIPFILTER,mip)); CHECK(d->SetSamplerState(0,D3DSAMP_MAXMIPLEVEL,level));
    };
    ComPtr<IDirect3DVertexBuffer9> material; UINT offset,stride;
    CHECK(d->GetStreamSource(1,&material,&offset,&stride)); require(stride==12 && offset==0,"fixture material layout");
    auto uv=[&](float scale,bool shore=false) {
        struct Material {DWORD color;float u,v;};
        Material values[]={{0xffffffff,0,0},{0xffffffff,scale,0},{0xffffffff,0,scale},{0xffffffff,scale,scale}};
        if(shore) {
            const DWORD colors[]={0x002080c0,0xffc04020,0x4010e080,0xc08020f0};
            for(int i=0;i<4;++i) { values[i].color=colors[i]; values[i].u-=.25f; values[i].v-=.25f; }
        }
        void* mapped; CHECK(material->Lock(0,sizeof values,&mapped,D3DLOCK_DISCARD));
        memcpy(terrain12Lock(material.Get(),0,sizeof values,mapped),values,sizeof values);
        terrain12Unlock(material.Get()); CHECK(material->Unlock());
    };
    ComPtr<IDirect3DSurface9> target,readback; CHECK(d->GetRenderTarget(0,&target));
    CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    auto draw=[&](bool native,IDirect3DTexture9* texture,bool accepted=true) {
        CHECK(d->SetTexture(0,texture));
        if(native) {
            bool handled=terrain12Draw(d,D3DPT_TRIANGLELIST,0,0,4,0,2);
            require(handled==accepted,"expected native/fallback route");
            if(handled)return;
            // Matches hDIP: flush pending native work before the original draw.
            terrain12Flush();
        }
        CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,4,0,2));
    };
    auto capture=[&](bool native,const std::function<void(bool)>& commands) {
        D3DVIEWPORT9 full={0,0,256,256,0,1}; CHECK(d->SetViewport(&full));
        CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff182838,1,0));
        CHECK(d->BeginScene()); commands(native); if(native)terrain12Flush(); CHECK(d->EndScene());
        CHECK(d->GetRenderTargetData(target.Get(),readback.Get()));
        D3DLOCKED_RECT lock; CHECK(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));
        Pixels result(256*256);
        for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)
            result[y*256+x]=((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]&0xffffff;
        CHECK(readback->UnlockRect()); return result;
    };
    auto parity=[&](const char* name,IDirect3DTexture9* texture,bool accepted=true) {
        auto commands=[&](bool native){draw(native,texture,accepted);};
        auto reference=capture(false,commands); compare(name,reference,capture(true,commands)); return reference;
    };
    auto create=[&](D3DFORMAT format,D3DPOOL pool=D3DPOOL_MANAGED) {
        ComPtr<IDirect3DTexture9> texture;
        CHECK(d->CreateTexture(64,64,0,0,format,pool,&texture,nullptr));
        terrain12HookTexture(texture.Get()); return texture;
    };
    ComPtr<IDirect3DTexture9> textures[3]; const D3DFORMAT formats[]={D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5};
    const char* names[]={"DXT1","DXT3","DXT5"};
    for(int f=0;f<3;++f) {
        textures[f]=create(formats[f]); fill(textures[f].Get(),0);
        for(DWORD filter:{DWORD(D3DTEXF_POINT),DWORD(D3DTEXF_LINEAR)}) {
            Pixels previous;
            for(float scale:{1.f,8.f,32.f,128.f,256.f}) {
                uv(scale); sampler(filter,filter);
                char name[100]; sprintf_s(name,"%s-filter%lu-uv%.0f",names[f],filter,scale);
                auto pixels=parity(name,textures[f].Get());
                if(!previous.empty())require(previous!=pixels,"minification changes sampled mip"); previous=pixels;
            }
        }
        uv(1); sampler(D3DTEXF_LINEAR,D3DTEXF_NONE,3); parity("mip-disabled-maxlevel3",textures[f].Get());
        sampler(D3DTEXF_LINEAR,D3DTEXF_POINT,3); parity("forced-mip3",textures[f].Get());
        sampler(D3DTEXF_POINT,D3DTEXF_NONE);
        CHECK(d->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE)); CHECK(d->SetRenderState(D3DRS_ALPHAREF,64));
        parity("compressed-alpha-test",textures[f].Get()); CHECK(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));
    }
    // Transparent borders and interpolated diffuse alpha exercise synthetic
    // shoreline states; the reference still executes the stock base shader.
    struct Stage {DWORD op,arg1,arg2;};
    const Stage stages[]={
        {D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE},
        {D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE},
        {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE},
        {D3DTOP_MODULATE2X,D3DTA_TEXTURE,D3DTA_DIFFUSE},
        {D3DTOP_ADD,D3DTA_TEXTURE,D3DTA_DIFFUSE},
        {D3DTOP_MODULATE,D3DTA_TEXTURE|D3DTA_COMPLEMENT,D3DTA_DIFFUSE},
        {D3DTOP_MODULATE,D3DTA_TEXTURE|D3DTA_ALPHAREPLICATE,D3DTA_DIFFUSE},
        {D3DTOP_MODULATE,D3DTA_TEXTURE|D3DTA_ALPHAREPLICATE|D3DTA_COMPLEMENT,D3DTA_CURRENT},
        {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE|D3DTA_COMPLEMENT},
        {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE|D3DTA_ALPHAREPLICATE}
    };
    uv(1.5f,true); sampler(D3DTEXF_LINEAR,D3DTEXF_NONE);
    CHECK(d->SetSamplerState(0,D3DSAMP_BORDERCOLOR,0x00306090));
    c[26][1]=.5f; CHECK(d->SetVertexShaderConstantF(26,c[26],1));
    CHECK(d->SetRenderState(D3DRS_FOGENABLE,TRUE));
    CHECK(d->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE)); CHECK(d->SetRenderState(D3DRS_ALPHAREF,0));
    CHECK(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE));
    for(DWORD address:{DWORD(D3DTADDRESS_WRAP),DWORD(D3DTADDRESS_CLAMP),DWORD(D3DTADDRESS_BORDER)}) {
        for(auto state:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV})CHECK(d->SetSamplerState(0,state,address));
        for(int i=0;i<_countof(stages);++i) {
            CHECK(d->SetTextureStageState(0,D3DTSS_COLOROP,stages[i].op));
            CHECK(d->SetTextureStageState(0,D3DTSS_COLORARG1,stages[i].arg1));
            CHECK(d->SetTextureStageState(0,D3DTSS_COLORARG2,stages[i].arg2));
            char name[100]; sprintf_s(name,"shore-address%lu-stage%d",address,i);
            parity(name,textures[2].Get());
        }
    }
    CHECK(d->SetRenderState(D3DRS_FOGENABLE,FALSE)); CHECK(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));
    for(auto state:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})CHECK(d->SetTextureStageState(0,state,D3DTOP_SELECTARG1));
    CHECK(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE));
    CHECK(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE));
    for(auto state:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV})CHECK(d->SetSamplerState(0,state,D3DTADDRESS_WRAP));
    uv(1); sampler(D3DTEXF_LINEAR,D3DTEXF_NONE);
    auto a=textures[2].Get(); auto b=textures[1].Get();
    auto before=parity("cached",a); auto stats=terrain12TestTextures();
    parity("cached-repeat",a); parity("texture-switch",b); compare("cached-return",before,parity("texture-return",a));
    D3DLOCKED_RECT read; CHECK(a->LockRect(0,&read,nullptr,D3DLOCK_READONLY)); CHECK(a->UnlockRect(0));
    parity("readonly-lock",a); require(terrain12TestTextures().uploads==stats.uploads,"cache reuse without upload");
    fill(a,1); terrain12TextureChanged(a); terrain12TextureChanged(a);
    auto updated=parity("season-rewrite",a); require(before!=updated,"rewritten texture visible");
    require(terrain12TestTextures().uploads==stats.uploads+1,"duplicate invalidations coalesce");
    sampler(D3DTEXF_LINEAR,D3DTEXF_POINT,3); before=parity("mip-before",a); stats=terrain12TestTextures();
    fill(a,2,3); updated=parity("mip-rewrite",a); require(before!=updated,"nonzero mip rewrite visible");
    require(terrain12TestTextures().uploads==stats.uploads+1,"mip edit refreshes cached texture");
    sampler(D3DTEXF_LINEAR,D3DTEXF_NONE);
    auto pending=[&](bool native) {
        fill(a,3); D3DVIEWPORT9 left={0,0,128,256,0,1}; CHECK(d->SetViewport(&left)); draw(native,a);
        fill(a,4); D3DVIEWPORT9 right={128,0,128,256,0,1}; CHECK(d->SetViewport(&right)); draw(native,a);
    };
    auto original=capture(false,pending); compare("rewrite-with-pending-draw",original,capture(true,pending));

    static Pixels retainedExpected;
    if(iteration==0)fill(retained,0);
    auto survived=parity("managed-texture-after-reset",retained);
    if(iteration>0)compare("managed-content-survives-reset",retainedExpected,survived);
    fill(retained,iteration+1); retainedExpected=parity("managed-texture-new-season",retained);
    require(survived!=retainedExpected,"retained texture update visible");

    auto unsupported=create(D3DFMT_A4R4G4B4); fill(unsupported.Get(),0);
    ComPtr<IDirect3DTexture9> rejected;
    CHECK(d->CreateTexture(2048,2048,0,0,D3DFMT_DXT5,D3DPOOL_MANAGED,&rejected,nullptr));
    terrain12HookTexture(rejected.Get());fill(rejected.Get(),7);
    auto mixed=[&](IDirect3DTexture9* middle,const char* name) {
        auto commands=[&](bool native){draw(native,retained);draw(native,middle,false);draw(native,b);};
        auto reference=capture(false,commands); compare(name,reference,capture(true,commands));
    };
    mixed(unsupported.Get(),"unsupported-between-native-draws");
    stats=terrain12TestTextures(); terrain12TestTextureBudget(stats.bytes);
    mixed(rejected.Get(),"budget-fallback-between-native-draws");
    auto exhausted=terrain12TestTextures();
    require(exhausted.bytes==stats.bytes && exhausted.entries==stats.entries,"budget rejection leaves cache unchanged");
    require(exhausted.uploads==stats.uploads,"cached textures work at budget limit");
    terrain12TestTextureBudget(stats.budget); parity("budget-restored",rejected.Get());
    require(terrain12TestTextures().uploads==stats.uploads+1,"rejected texture can later upload");
    a->SetLOD(2); mixed(a,"nonzero-LOD-fallback"); a->SetLOD(0);
    parity("LOD-restored",a);
    // Repeated rewrites while a draw is pending must preserve both versions,
    // recycle all four frame slots, and keep the resident texture cache bounded.
    stats=terrain12TestTextures();
    for(unsigned cycle=0;cycle<128;++cycle) {
        auto rewrite=[&](bool native) {
            fill(a,cycle*2+13);
            D3DVIEWPORT9 left={0,0,128,256,0,1}; CHECK(d->SetViewport(&left)); draw(native,a);
            fill(a,cycle*2+14);
            D3DVIEWPORT9 right={128,0,128,256,0,1}; CHECK(d->SetViewport(&right)); draw(native,a);
        };
        char name[100]; sprintf_s(name,"rewrite-stress-%u",cycle);
        auto reference=capture(false,rewrite); compare(name,reference,capture(true,rewrite));
        auto after=terrain12TestTextures();
        require(after.entries==stats.entries && after.bytes==stats.bytes,"rewrite stress keeps cache bounded");
        require(after.uploads==stats.uploads+2*(cycle+1),"each pending version uploads exactly once");
        if(cycle%16==15) {
            parity("rewrite-stress-cache-reuse",a);
            require(terrain12TestTextures().uploads==after.uploads,"stress cache hit avoids upload");
        }
    }
    before=parity("surface-before",a); stats=terrain12TestTextures();
    fill(a,91,-1,true);
    updated=parity("surface-rewrite",a); require(before!=updated,"surface mutation changes reference");
    require(terrain12TestTextures().uploads==stats.uploads+1,"surface writes invalidate native cache");
    sampler(D3DTEXF_LINEAR,D3DTEXF_POINT,3); before=parity("surface-mip-before",a); stats=terrain12TestTextures();
    fill(a,92,3,true); updated=parity("surface-mip-rewrite",a);
    require(before!=updated,"surface mip mutation changes reference");
    require(terrain12TestTextures().uploads==stats.uploads+1,"surface mip writes invalidate native cache");
    ComPtr<IDirect3DSurface9> surface; CHECK(a->GetSurfaceLevel(3,&surface));
    stats=terrain12TestTextures();
    CHECK(surface->LockRect(&read,nullptr,D3DLOCK_READONLY)); CHECK(surface->UnlockRect());
    require(FAILED(a->LockRect(a->GetLevelCount(),&read,nullptr,0)),"invalid mip lock fails");
    parity("surface-readonly-and-failed-texture-lock",a);
    require(terrain12TestTextures().uploads==stats.uploads,"readonly and failed locks do not invalidate cache");
    sampler(D3DTEXF_LINEAR,D3DTEXF_NONE);
    auto staging=create(D3DFMT_DXT5,D3DPOOL_SYSTEMMEM),destination=create(D3DFMT_DXT5,D3DPOOL_DEFAULT);
    fill(staging.Get(),101); CHECK(d->UpdateTexture(staging.Get(),destination.Get()));
    stats=terrain12TestTextures(); before=parity("UpdateTexture-default-pool",destination.Get(),false);
    fill(staging.Get(),102); CHECK(d->UpdateTexture(staging.Get(),destination.Get()));
    updated=parity("UpdateTexture-rewrite",destination.Get(),false);
    require(before!=updated,"UpdateTexture changes sampled content");
    mixed(destination.Get(),"UpdateTexture-mixed-fallback");
    ComPtr<IDirect3DSurface9> from,to;
    CHECK(staging->GetSurfaceLevel(0,&from)); CHECK(destination->GetSurfaceLevel(0,&to));
    fill(staging.Get(),103); CHECK(d->UpdateSurface(from.Get(),nullptr,to.Get(),nullptr));
    before=parity("UpdateSurface-default-pool",destination.Get(),false);
    require(before!=updated,"UpdateSurface changes sampled content");
    mixed(destination.Get(),"UpdateSurface-mixed-fallback");
    auto afterCopy=terrain12TestTextures();
    require(afterCopy.entries==stats.entries && afterCopy.bytes==stats.bytes && afterCopy.uploads==stats.uploads,
        "default-pool copies remain outside native managed cache");
    ComPtr<IDirect3DTexture9> dynamic;
    CHECK(d->CreateTexture(64,64,1,D3DUSAGE_DYNAMIC,D3DFMT_DXT5,D3DPOOL_DEFAULT,&dynamic,nullptr));terrain12HookTexture(dynamic.Get());
    for(UINT phase=0;phase<4;++phase){fill(dynamic.Get(),120+phase,-1,false,D3DLOCK_DISCARD);auto pixels=parity("dynamic-DISCARD-safe-fallback",dynamic.Get(),false);if(phase)require(pixels!=before,"dynamic rewritten content visible");before=pixels;}
    require(terrain12TestTextures().uploads==stats.uploads,"dynamic default texture never enters native cache");
    before=parity("failed-copy-before",a); stats=terrain12TestTextures();
    require(FAILED(d->UpdateTexture(staging.Get(),a)),"UpdateTexture rejects managed destination");
    ComPtr<IDirect3DSurface9> managed; CHECK(a->GetSurfaceLevel(0,&managed));
    require(FAILED(d->UpdateSurface(from.Get(),nullptr,managed.Get(),nullptr)),"UpdateSurface rejects managed destination");
    compare("failed-copy-content-unchanged",before,parity("failed-copy-after",a));
    require(terrain12TestTextures().uploads==stats.uploads,"failed copy does not invalidate managed cache");
    RECT scissor={0,0,128,256}; CHECK(d->SetScissorRect(&scissor));
    const float plane[]={1,0,0,0}; CHECK(d->SetClipPlane(0,plane));
    struct FallbackState {const char* name;D3DRENDERSTATETYPE state;DWORD value;};
    const FallbackState fallbackStates[]={
        {"wireframe",D3DRS_FILLMODE,D3DFILL_WIREFRAME},
        {"scissor",D3DRS_SCISSORTESTENABLE,TRUE},
        {"srgb-write",D3DRS_SRGBWRITEENABLE,TRUE},
        {"separate-alpha",D3DRS_SEPARATEALPHABLENDENABLE,TRUE},
        {"stencil",D3DRS_STENCILENABLE,TRUE},
        {"wrap-coordinate",D3DRS_WRAP0,D3DWRAP_U},
        {"table-fog",D3DRS_FOGTABLEMODE,D3DFOG_LINEAR},
        {"user-clip-plane",D3DRS_CLIPPLANEENABLE,1}
    };
    for(const auto& test:fallbackStates) {
        DWORD saved; CHECK(d->GetRenderState(test.state,&saved));
        auto commands=[&](bool native) {
            draw(native,retained);
            CHECK(d->SetRenderState(test.state,test.value));
            draw(native,a,false);
            CHECK(d->SetRenderState(test.state,saved));
            draw(native,b);
        };
        char name[100]; sprintf_s(name,"state-fallback-%s",test.name);
        auto reference=capture(false,commands); compare(name,reference,capture(true,commands));
    }
    // Keep earlier draws visible: overwriting an in-flight descriptor would
    // corrupt their tiles, even if the last draw still looked correct.
    DWORD sampling[12][12]={};
    for(UINT state=1;state<=11;++state)CHECK(d->GetSamplerState(0,(D3DSAMPLERSTATETYPE)state,&sampling[0][state]));
    for(int i=1;i<12;++i)memcpy(sampling[i],sampling[0],sizeof sampling[0]);
    sampling[1][D3DSAMP_MINFILTER]=D3DTEXF_POINT;
    sampling[1][D3DSAMP_MAGFILTER]=D3DTEXF_POINT;
    sampling[2][D3DSAMP_ADDRESSW]=D3DTADDRESS_BORDER;
    sampling[3][D3DSAMP_MIPFILTER]=D3DTEXF_POINT;
    sampling[4][D3DSAMP_MIPFILTER]=D3DTEXF_LINEAR;
    sampling[5][D3DSAMP_ADDRESSU]=D3DTADDRESS_CLAMP;
    sampling[6][D3DSAMP_ADDRESSV]=D3DTADDRESS_MIRROR;
    sampling[7][D3DSAMP_ADDRESSU]=D3DTADDRESS_BORDER; sampling[7][D3DSAMP_BORDERCOLOR]=0xffc03070;
    sampling[8][D3DSAMP_ADDRESSU]=D3DTADDRESS_BORDER; sampling[8][D3DSAMP_BORDERCOLOR]=0xff20b040;
    const float bias=.75f; memcpy(&sampling[9][D3DSAMP_MIPMAPLODBIAS],&bias,sizeof bias);
    sampling[9][D3DSAMP_MIPFILTER]=D3DTEXF_LINEAR;
    sampling[10][D3DSAMP_MIPFILTER]=D3DTEXF_POINT; sampling[10][D3DSAMP_MAXMIPLEVEL]=3;
    sampling[11][D3DSAMP_MAXANISOTROPY]=2; // Distinct state even with linear filtering.
    auto setSampling=[&](const DWORD* values) {
        for(UINT state=1;state<=11;++state)CHECK(d->SetSamplerState(0,(D3DSAMPLERSTATETYPE)state,values[state]));
    };
    uv(1.5f,true);
    auto descriptorTiles=[&](bool native) {
        for(UINT repeat=0;repeat<2;++repeat)for(UINT i=0;i<12;++i) {
            setSampling(sampling[i]);
            D3DVIEWPORT9 tile={(i%6)*42,(i/6+repeat*2)*64,42,64,0,1}; CHECK(d->SetViewport(&tile));
            draw(native,textures[(i+repeat)%3].Get());
        }
    };
    original=capture(false,descriptorTiles); terrain12TestClearWork();
    auto descriptorPixels=capture(true,descriptorTiles);
    for(UINT repeat=0;repeat<2;++repeat)for(UINT i=0;i<12;++i) {
        unsigned maximum=0,bad=0;
        for(UINT y=(i/6+repeat*2)*64;y<(i/6+repeat*2+1)*64;++y)for(UINT x=(i%6)*42;x<(i%6+1)*42;++x) {
            unsigned error=0;
            for(int shift=0;shift<24;shift+=8)error=std::max(error,unsigned(abs(int((original[y*256+x]>>shift)&255)-int((descriptorPixels[y*256+x]>>shift)&255))));
            maximum=std::max(maximum,error); if(error>2)++bad;
        }
        if(bad)printf("Descriptor mismatch state=%u texture=%u max_error=%u bad_pixels=%u\n",i,(i+repeat)%3,maximum,bad);
    }
    compare("descriptor-texture-and-sampler-switch",original,descriptorPixels);
    auto descriptorWork=terrain12TestWork();
    printf("Descriptor work case=switch srvs=%llu samplers=%llu batches=%llu\n",descriptorWork.srvs,descriptorWork.samplers,descriptorWork.batches);
    require(descriptorWork.srvs==3 && descriptorWork.samplers==12 && descriptorWork.batches==1,"descriptor switch cache counts");
    auto descriptorBoundary=[&](bool native) {
        setSampling(sampling[7]);
        for(UINT i=0;i<1025;++i) {
            CHECK(d->SetSamplerState(0,D3DSAMP_BORDERCOLOR,0xff000000|(i*0x010307)));
            D3DVIEWPORT9 tile={(i%16)*16,((i/16)%16)*16,16,16,0,1}; CHECK(d->SetViewport(&tile));
            draw(native,textures[i%3].Get());
        }
    };
    original=capture(false,descriptorBoundary); terrain12TestClearWork();
    compare("descriptor-1025-unique-samplers",original,capture(true,descriptorBoundary));
    descriptorWork=terrain12TestWork();
    printf("Descriptor work case=boundary srvs=%llu samplers=%llu batches=%llu\n",descriptorWork.srvs,descriptorWork.samplers,descriptorWork.batches);
    require(descriptorWork.srvs==4 && descriptorWork.samplers==1025 && descriptorWork.batches==2,"descriptor capacity and new batch rebuild");
    setSampling(sampling[0]); uv(1);
    for(DWORD minFilter:{DWORD(D3DTEXF_POINT),DWORD(D3DTEXF_LINEAR)}) {
        auto mixedFilter=[&](bool native) {
            sampler(D3DTEXF_LINEAR,D3DTEXF_NONE); draw(native,retained);
            sampler(minFilter,D3DTEXF_NONE);
            CHECK(d->SetSamplerState(0,D3DSAMP_MAGFILTER,minFilter==D3DTEXF_POINT?D3DTEXF_LINEAR:D3DTEXF_POINT));
            draw(native,a,false); sampler(D3DTEXF_LINEAR,D3DTEXF_NONE); draw(native,b);
        };
        original=capture(false,mixedFilter); compare("mixed-min-mag-filter-fallback",original,capture(true,mixedFilter));
    }
    original=capture(false,pending); terrain12TestClearWork();
    compare("descriptor-reuse-across-texture-rewrite",original,capture(true,pending));
    descriptorWork=terrain12TestWork();
    printf("Descriptor work case=rewrite srvs=%llu samplers=%llu batches=%llu\n",descriptorWork.srvs,descriptorWork.samplers,descriptorWork.batches);
    require(descriptorWork.srvs==1 && descriptorWork.samplers==1 && descriptorWork.batches==1,"content rewrite preserves immutable descriptors");
    // Rotate fresh textures under a two-texture budget while earlier tiles are
    // still queued. Reclamation must preserve those draws and permit reuploads.
    ComPtr<IDirect3DTexture9> rotating[3];
    for(UINT i=0;i<3;++i){rotating[i]=create(D3DFMT_DXT5);fill(rotating[i].Get(),30+i);}
    stats=terrain12TestTextures();terrain12TestTextureBudget(2*D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    auto pressure=[&](bool native) {
        for(UINT i=0;i<24;++i){D3DVIEWPORT9 tile={(i%6)*42,(i/6)*64,42,64,0,1};CHECK(d->SetViewport(&tile));
            draw(native,rotating[i%3].Get());
            if(native)require(terrain12TestTextures().bytes<=terrain12TestTextures().budget,"eviction stays within texture budget");}
    };
    original=capture(false,pressure);compare("budget-rotation-in-flight",original,capture(true,pressure));
    require(terrain12TestTextures().uploads>stats.uploads+3,"evicted textures can be uploaded again");
    terrain12TestTextureBudget(stats.budget);
    // Exercise the generated sample and its complete mip chain on the same
    // original/native samplers; this validates the fixture, not its art quality.
    ComPtr<IDirect3DTexture9> sample;
    CHECK(d->CreateTexture(1024,1024,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&sample,nullptr));terrain12HookTexture(sample.Get());
    FILE* sampleFile=nullptr;require(fopen_s(&sampleFile,"build/terrain-assets/sample-v1/sample-mips.bgra","rb")==0,"redraw sample fixture exists");
    require(sample->GetLevelCount()==11,"sample complete mip chain");
    for(UINT level=0;level<sample->GetLevelCount();++level){D3DSURFACE_DESC desc;CHECK(sample->GetLevelDesc(level,&desc));D3DLOCKED_RECT lock;CHECK(sample->LockRect(level,&lock,nullptr,0));
        for(UINT y=0;y<desc.Height;++y)require(fread((BYTE*)lock.pBits+y*lock.Pitch,4,desc.Width,sampleFile)==desc.Width,"sample mip payload complete");CHECK(sample->UnlockRect(level));}
    require(fgetc(sampleFile)==EOF,"no unexplained sample bytes");fclose(sampleFile);
    for(UINT scale:{1u,4u,16u,64u}){uv(float(scale));sampler(D3DTEXF_LINEAR,D3DTEXF_LINEAR);parity("redraw-sample-mips",sample.Get());}
    uv(1);sampler(D3DTEXF_LINEAR,D3DTEXF_NONE);
    // Exercise the production replacement selector, not just a standalone HD
    // texture. Reference draws explicitly bind the package target in D3D9.
    FILE* packFile=nullptr;require(fopen_s(&packFile,"build/terrain-assets/replacement-v2.wmtr","rb")==0,"replacement package fixture");
    fseek(packFile,0,SEEK_END);long packSize=ftell(packFile);rewind(packFile);require(packSize>40 && packSize<18*1024*1024,"package fixture bound");
    std::vector<BYTE> pack(packSize);require(fread(pack.data(),1,pack.size(),packFile)==pack.size(),"package read");fclose(packFile);
    UINT dims[7];memcpy(dims,pack.data()+12,sizeof dims);require(dims[0]==64 && dims[1]==64 && dims[2]==21 && dims[3]==7 && dims[4]==1024 && dims[5]==1024 && dims[6]==11,"fixture package header");
    size_t position=40;
    auto fromPackage=[&](UINT width,UINT height,UINT levels,D3DFORMAT format=D3DFMT_A8R8G8B8){
        ComPtr<IDirect3DTexture9> value;CHECK(d->CreateTexture(width,height,levels,0,format,D3DPOOL_MANAGED,&value,nullptr));terrain12HookTexture(value.Get());
        for(UINT level=0;level<levels;++level){D3DLOCKED_RECT lock;CHECK(value->LockRect(level,&lock,nullptr,0));
            for(UINT y=0;y<height;++y){require(position+width*4<=pack.size(),"fixture row bound");memcpy((BYTE*)lock.pBits+y*lock.Pitch,pack.data()+position,width*4);position+=width*4;}
            CHECK(value->UnlockRect(level));width=std::max(1u,width/2);height=std::max(1u,height/2);}
        return value;
    };
    auto source=fromPackage(64,64,7),expected=fromPackage(1024,1024,11);require(position+28<pack.size(),"second season fixture present");
    size_t secondHeader=position;position+=28;auto winterSource=fromPackage(64,64,7),winterExpected=fromPackage(1024,1024,11);require(position==pack.size(),"fixture consumed exactly");
    require(terrainReplacementConfigure(nullptr),"replacement default off");
    auto baseline=parity("replacement-off",source.Get());
    require(!terrainReplacementConfigure(L"build/terrain-assets/nonexistent.wmtr"),"missing package rejected");parity("replacement-missing-fallback",source.Get());
    for(UINT kind=0;kind<7;++kind){auto bad=pack;
        if(kind==0)bad[0]^=1;
        if(kind==1)bad.resize(bad.size()-1);
        if(kind==2)bad.push_back(0);
        if(kind==3){UINT huge=0xffffffff;memcpy(bad.data()+28,&huge,4);}
        if(kind==4){UINT extra=12;memcpy(bad.data()+36,&extra,4);}
        if(kind==5){UINT huge=5;memcpy(bad.data()+8,&huge,4);}
        if(kind==6)memcpy(bad.data()+secondHeader,pack.data()+12,secondHeader-12);
        FILE* file=nullptr;require(fopen_s(&file,"build/terrain-assets/rejected.wmtr","wb")==0,"bad package fixture");require(fwrite(bad.data(),1,bad.size(),file)==bad.size(),"bad package written");fclose(file);
        require(!terrainReplacementConfigure(L"build/terrain-assets/rejected.wmtr"),"invalid package rejected");
        ComPtr<IDirect3DTexture9> none;require(!terrainReplacementFind(source.Get(),&none) && !none,"failed load disables replacement");
    }
    require(terrainReplacementConfigure(L"build/terrain-assets/replacement-v2.wmtr"),"valid package loaded");
    auto replaced=[&](){return capture(true,[&](bool native){draw(native,source.Get());});};
    for(UINT scale:{1u,4u,16u,64u}){uv(float(scale));sampler(D3DTEXF_LINEAR,D3DTEXF_LINEAR);
        auto reference=capture(false,[&](bool native){draw(native,expected.Get());});auto actual=replaced();compare("replacement-active-mips",reference,actual);
        if(scale==1)require(actual!=baseline,"replacement changes visible pixels");
        ComPtr<IDirect3DBaseTexture9> bound;CHECK(d->GetTexture(0,&bound));require(bound.Get()==source.Get(),"replacement does not change D3D9 bindings");
    }
    uv(1);sampler(D3DTEXF_LINEAR,D3DTEXF_NONE);
    for(UINT level:{0u,3u}){D3DLOCKED_RECT lock;CHECK(source->LockRect(level,&lock,nullptr,0));*(BYTE*)lock.pBits^=1;CHECK(source->UnlockRect(level));
        parity("replacement-edited-source-fallback",source.Get());
        CHECK(source->LockRect(level,&lock,nullptr,0));*(BYTE*)lock.pBits^=1;CHECK(source->UnlockRect(level));}
    for(UINT turn=0;turn<8;++turn){auto selected=turn%2?winterSource.Get():source.Get();auto target=turn%2?winterExpected.Get():expected.Get();
        auto reference=capture(false,[&](bool native){draw(native,target);});compare("replacement-season-switch",reference,capture(true,[&](bool native){draw(native,selected);}));}
    stats=terrain12TestTextures();terrain12TestTextureBudget(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    compare("replacement-budget-fallback",baseline,replaced());require(terrain12TestTextures().bytes<=D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,"replacement respects native budget");terrain12TestTextureBudget(stats.budget);
    terrainReplacementReset();compare("replacement-device-recreate",capture(false,[&](bool native){draw(native,expected.Get());}),replaced());
    // A captured compressed signature must compare block rows, including the
    // final 2x2/1x1 mips. These are fixtures, not inferred game compressor output.
    for(UINT formatIndex=0;formatIndex<3;++formatIndex){
        auto compressed=textures[formatIndex].Get();std::vector<BYTE> encoded(40);
        memcpy(encoded.data(),"WMTR0002",8);UINT one=1;memcpy(encoded.data()+8,&one,4);
        UINT fields[]={64,64,UINT(formats[formatIndex]),7,1024,1024,11};memcpy(encoded.data()+12,fields,sizeof fields);
        for(UINT level=0;level<7;++level){D3DSURFACE_DESC desc;CHECK(compressed->GetLevelDesc(level,&desc));D3DLOCKED_RECT lock;CHECK(compressed->LockRect(level,&lock,nullptr,D3DLOCK_READONLY));
            UINT row=std::max(1u,(desc.Width+3)/4)*(formatIndex==0?8:16),rows=std::max(1u,(desc.Height+3)/4);
            for(UINT y=0;y<rows;++y)encoded.insert(encoded.end(),(BYTE*)lock.pBits+y*lock.Pitch,(BYTE*)lock.pBits+y*lock.Pitch+row);CHECK(compressed->UnlockRect(level));}
        encoded.insert(encoded.end(),pack.begin()+40+21844,pack.begin()+secondHeader);
        FILE* file=nullptr;require(fopen_s(&file,"build/terrain-assets/compressed.wmtr","wb")==0,"compressed package fixture");require(fwrite(encoded.data(),1,encoded.size(),file)==encoded.size(),"compressed fixture written");fclose(file);
        require(terrainReplacementConfigure(L"build/terrain-assets/compressed.wmtr"),"compressed identity package loaded");
        auto reference=capture(false,[&](bool native){draw(native,expected.Get());});compare("replacement-compressed-identity",reference,capture(true,[&](bool native){draw(native,compressed);}));
    }
    require(terrainReplacementConfigure(nullptr),"replacement disabled again");compare("replacement-off-restored",baseline,replaced());
    printf("PASS replacement selector iteration=%d exact_all_mips=1 bounds=1 mutation_fallback=1 budget_fallback=1 off_restored=1\n",iteration);
    // Independently derived loader profiles: one X8 level, full/half size,
    // common/winter source. Still not a capture of an actual game session.
    require(fopen_s(&packFile,"build/terrain-assets/replacement-loader-profiles.wmtr","rb")==0,"loader profiles fixture");
    fseek(packFile,0,SEEK_END);packSize=ftell(packFile);rewind(packFile);require(packSize>40 && packSize<32*1024*1024,"loader profiles bound");
    pack.resize(packSize);require(fread(pack.data(),1,pack.size(),packFile)==pack.size(),"loader profiles read");fclose(packFile);
    UINT profiles=0;memcpy(&profiles,pack.data()+8,4);require(profiles==4 && !memcmp(pack.data(),"WMTR0002",8),"four loader profiles");position=12;
    for(UINT profile=0;profile<profiles;++profile){
        require(position+28<=pack.size(),"loader profile header bound");memcpy(dims,pack.data()+position,28);position+=28;
        UINT width=profile%2?32:64;require(dims[0]==width && dims[1]==width && dims[2]==22 && dims[3]==1 && dims[4]==1024 && dims[5]==1024 && dims[6]==11,"single-level X8 loader profile");
        auto originalTexture=fromPackage(width,width,1,D3DFMT_X8R8G8B8),target=fromPackage(1024,1024,11);
        uv(1);sampler(D3DTEXF_LINEAR,D3DTEXF_LINEAR);parity("loader-profile-off",originalTexture.Get());
        require(terrainReplacementConfigure(L"build/terrain-assets/replacement-loader-profiles.wmtr"),"load static identity profiles");
        for(UINT scale:{1u,4u,16u,64u}){uv(float(scale));
            auto reference=capture(false,[&](bool native){draw(native,target.Get());});
            compare("loader-profile-active",reference,capture(true,[&](bool native){draw(native,originalTexture.Get());}));}
        D3DLOCKED_RECT lock;CHECK(originalTexture->LockRect(0,&lock,nullptr,0));*(BYTE*)lock.pBits^=1;CHECK(originalTexture->UnlockRect(0));
        parity("loader-profile-mutation-fallback",originalTexture.Get());
        require(terrainReplacementConfigure(nullptr),"disable static profiles");
    }
    require(position==pack.size(),"all loader profile bytes consumed");uv(1);sampler(D3DTEXF_LINEAR,D3DTEXF_NONE);
    printf("PASS ground loader profiles iteration=%d profiles=4 scales=4 source_levels=1 mutation_fallback=1\n",iteration);
    printf("PASS texture lifecycle: iteration=%d cache_entries=%zu uploads=%llu bytes=%llu\n",iteration,
        terrain12TestTextures().entries,terrain12TestTextures().uploads,terrain12TestTextures().bytes);
}
