// Actual legacy water VS/layout/strip versus the production native water path.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_dx12.h"
#include "../src/terrain_buffer_hooks.h"
#include "../build/terrain_reference.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <limits>
#include <cmath>
using Microsoft::WRL::ComPtr;
namespace {
void require(bool v,const char* s){if(!v){printf("FAIL water: %s\n",s);exit(1);}}
void check(HRESULT h,const char* s){if(FAILED(h)){printf("FAIL water: %s %08lx\n",s,h);exit(1);}}
#define CHECK(x) check((x),#x)
struct WaterVertex {float x,y,z;DWORD color;float u,v;};
}
void testWaterReference(IDirect3DDevice9* d,IDirect3DVertexShader9* const* shaders,int iteration,ID3D12Device* gpu,ID3D12CommandQueue* queue) {
    ComPtr<IDirect3DStateBlock9> saved;CHECK(d->CreateStateBlock(D3DSBT_ALL,&saved));
    wchar_t path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);wcscat_s(path,L"\\d3dcompiler_47.dll");
    HMODULE compiler=LoadLibraryW(path);require(compiler!=nullptr,"system assembler");
    using Assemble=HRESULT(WINAPI*)(const void*,SIZE_T,const char*,const D3D_SHADER_MACRO*,ID3DInclude*,UINT,ID3DBlob**,ID3DBlob**);
    auto assemble=(Assemble)GetProcAddress(compiler,"D3DAssemble");require(assemble!=nullptr,"assembler export");
    ComPtr<ID3DBlob> code,error;CHECK(assemble(terrainReference[3],strlen(terrainReference[3]),"stock-water",nullptr,nullptr,0,&code,&error));
    ComPtr<IDirect3DVertexShader9> water;CHECK(d->CreateVertexShader((DWORD*)code->GetBufferPointer(),&water));FreeLibrary(compiler);
    const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},{0,16,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    ComPtr<IDirect3DVertexDeclaration9> waterDecl;CHECK(d->CreateVertexDeclaration(elements,&waterDecl));
    ComPtr<IDirect3DVertexBuffer9> vb;ComPtr<IDirect3DIndexBuffer9> ib;
    CHECK(d->CreateVertexBuffer(16*sizeof(WaterVertex),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb,nullptr));
    // Stock 0x423038 creates a WRITEONLY managed IB, filled once at 0x423093.
    CHECK(d->CreateIndexBuffer(48,D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib,nullptr));
    water12TestBind(vb.Get(),ib.Get(),water.Get());
    terrain12ForgetBuffer(vb.Get());terrain12ForgetBuffer(ib.Get());
    // Managed IB hooks may already be shared with model tests. Dynamic VB hooks
    // are installed only on the final iteration, after the manual terrain probes.
    terrain12HookBuffer(ib.Get());
    if(iteration==2)terrain12HookBuffer(vb.Get());
    auto shadow=[&](UINT offset,UINT size,void* data,DWORD flags) {
        return iteration==2?data:terrain12Lock(vb.Get(),offset,size,data,flags);
    };
    auto unlockShadow=[&](){if(iteration!=2)terrain12Unlock(vb.Get());};
    // Nonzero stream/base/minimum/index offsets; two quads connected by degenerates.
    const WORD strip[24]={0,0,0,2,2,3,4,5,5,6,6,7,8,9,9};void* mapped;
    CHECK(ib->Lock(0,0,&mapped,0));memcpy(mapped,strip,sizeof strip);CHECK(ib->Unlock());
    CHECK(d->SetVertexDeclaration(waterDecl.Get()));CHECK(d->SetVertexShader(water.Get()));
    CHECK(d->SetStreamSource(0,vb.Get(),sizeof(WaterVertex),sizeof(WaterVertex)));CHECK(d->SetIndices(ib.Get()));
    ComPtr<IDirect3DTexture9> atlas;CHECK(d->CreateTexture(256,256,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&atlas,nullptr));
    D3DLOCKED_RECT texels;CHECK(atlas->LockRect(0,&texels,nullptr,0));
    // Deterministic synthetic atlas, not a claim of decoded water.wft pixels.
    for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)
        ((DWORD*)((BYTE*)texels.pBits+y*texels.Pitch))[x]=D3DCOLOR_ARGB((x+y)%251,40+(x/32)*20,50+(y/32)*20,80+(x+y)%170);
    CHECK(atlas->UnlockRect(0));CHECK(d->SetTexture(0,atlas.Get()));CHECK(d->SetPixelShader(nullptr));
    ComPtr<IDirect3DTexture9> stockAtlases[2];
    for(int i=0;i<2;++i) {
        char name[64];sprintf_s(name,"build/water-atlas-%d.bgra",i);FILE* file=nullptr;
        require(fopen_s(&file,name,"rb")==0,"hash-checked stock atlas fixture exists");
        std::vector<DWORD> pixels(256*256);require(fread(pixels.data(),4,pixels.size(),file)==pixels.size() && fgetc(file)==EOF,"stock atlas length");fclose(file);
        CHECK(d->CreateTexture(256,256,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&stockAtlases[i],nullptr));
        CHECK(stockAtlases[i]->LockRect(0,&texels,nullptr,0));
        for(UINT y=0;y<256;++y)memcpy((BYTE*)texels.pBits+y*texels.Pitch,pixels.data()+y*256,256*4);
        CHECK(stockAtlases[i]->UnlockRect(0));
    }
    auto rs=[&](D3DRENDERSTATETYPE s,DWORD v){CHECK(d->SetRenderState(s,v));};
    rs(D3DRS_CULLMODE,D3DCULL_NONE);rs(D3DRS_ZENABLE,TRUE);rs(D3DRS_ZWRITEENABLE,FALSE);rs(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
    rs(D3DRS_ALPHATESTENABLE,TRUE);rs(D3DRS_ALPHAFUNC,D3DCMP_GREATER);rs(D3DRS_ALPHAREF,0);
    rs(D3DRS_ALPHABLENDENABLE,TRUE);rs(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);rs(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    rs(D3DRS_BLENDOP,D3DBLENDOP_ADD);rs(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);rs(D3DRS_FOGCOLOR,0xff406090);
    CHECK(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    for(auto s:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})CHECK(d->SetTextureStageState(0,s,D3DTOP_MODULATE));
    for(auto s:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})CHECK(d->SetTextureStageState(0,s,D3DTA_TEXTURE));
    for(auto s:{D3DTSS_COLORARG2,D3DTSS_ALPHAARG2})CHECK(d->SetTextureStageState(0,s,D3DTA_CURRENT));
    for(auto s:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})CHECK(d->SetSamplerState(0,s,D3DTEXF_LINEAR));
    for(auto s:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV})CHECK(d->SetSamplerState(0,s,D3DTADDRESS_WRAP));
    CHECK(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));CHECK(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0));
    ComPtr<IDirect3DSurface9> target,readback;CHECK(d->GetRenderTarget(0,&target));
    CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    bool previewScene=false;ComPtr<IDirect3DSurface9> resolvedPreview;
    auto capture=[&](bool native,UINT phase,int scenario) {
        bool stockTextures=scenario>=3;scenario%=3;
        D3DVIEWPORT9 full={0,0,256,256,0,1};CHECK(d->SetViewport(&full));
        CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff182838,scenario==2?.5f:1,0));CHECK(d->BeginScene());
        for(UINT mask=0;mask<(phase==63?65u:16u);++mask)for(UINT layer=0;layer<2;++layer) {
            CHECK(d->SetTexture(0,stockTextures?stockAtlases[layer].Get():atlas.Get()));
            UINT frame=(phase+layer*17)%64;float u=float(frame%8)/8,v=float(frame/8)/8;
            WaterVertex vertices[]={{-.8f,.8f,.2f,0,u,v},{.8f,.8f,.2f,0,u+.125f,v},
                {-.8f,-.8f,scenario==2?.8f:.2f,0,u,v+.125f},{.8f,-.8f,scenario==2?.8f:.2f,0,u+.125f,v+.125f}};
            for(UINT i=0;i<4;++i) {vertices[i].color=(mask&(1<<i))?0x00ffffff:0x20ffffff;vertices[i].z+=layer*.1f;}
            if(previewScene)for(auto& vertex:vertices){float height=vertex.z;vertex.x*=20;vertex.z=vertex.y*20;vertex.y=height;}
            D3DVIEWPORT9 tile={(mask%4)*64,((mask%16)/4)*64,64,64,0,1};CHECK(d->SetViewport(&tile));
            WaterVertex storage[16]={};
            for(UINT q=0;q<2;++q)for(UINT i=0;i<4;++i) {auto w=vertices[i];w.x=w.x*.5f+(q?1:-1)*(previewScene?8.f:.4f);storage[4+q*4+i]=w;}
            CHECK(vb->Lock(0,0,&mapped,D3DLOCK_DISCARD));memcpy(shadow(0,0,mapped,D3DLOCK_DISCARD),storage,sizeof storage);unlockShadow();CHECK(vb->Unlock());
            if(phase==31) {
                storage[4].x+=.1f;
                CHECK(vb->Lock(4*sizeof(WaterVertex),sizeof(WaterVertex),&mapped,0));
                memcpy(shadow(4*sizeof(WaterVertex),sizeof(WaterVertex),mapped,0),storage+4,sizeof(WaterVertex));unlockShadow();CHECK(vb->Unlock());
            }
            if(native)require(water12Draw(d,D3DPT_TRIANGLESTRIP,1,2,8,3,10),"production water draw accepted");
            else CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,1,2,8,3,10));
            // Explicit native -> original handoff while retaining all preceding pixels.
            if(scenario==2 && mask==7 && layer==1) {
                if(native)terrain12Flush();
                CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,1,2,8,3,10));
            }
        }
        if(native)terrain12Flush();CHECK(d->EndScene());
        if(resolvedPreview)CHECK(d->StretchRect(target.Get(),nullptr,resolvedPreview.Get(),nullptr,D3DTEXF_NONE));
        CHECK(d->GetRenderTargetData(resolvedPreview?resolvedPreview.Get():target.Get(),readback.Get()));
        D3DLOCKED_RECT lock;CHECK(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));std::vector<DWORD> result(256*256);
        for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)result[y*256+x]=((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]&0xffffff;
        CHECK(readback->UnlockRect());return result;
    };
    for(int scenario=0;scenario<6;++scenario) {
        float c[28][4]={};for(int i=0;i<4;++i){c[1][i]=1;c[6+i][i]=1;c[10+i][i]=1;}c[26][0]=-.7f;c[26][1]=1;c[27][0]=-.3f;c[27][1]=1;
        if(scenario%3==1)c[13][0]=.2f;
        c[24][0]=.8f;c[24][1]=-.2f; // Water must ignore terrain's added diffuse constant.
        CHECK(d->SetVertexShaderConstantF(0,&c[0][0],28));rs(D3DRS_FOGENABLE,scenario%3!=0);
        for(UINT phase=0;phase<64;++phase) {
            auto reference=capture(false,phase,scenario),actual=capture(true,phase,scenario);
            unsigned maximum=0,bad=0;for(size_t i=0;i<reference.size();++i)for(int shift=0;shift<24;shift+=8){unsigned delta=abs(int((reference[i]>>shift)&255)-int((actual[i]>>shift)&255));maximum=std::max(maximum,delta);if(delta>2)++bad;}
            require(std::count(reference.begin(),reference.end(),0x182838)<65000,"reference water visible");require(bad==0,"stock water versus converted DX12 RGB parity");
            printf("Water parity iteration=%d scenario=%d phase=%u masks=16 layers=2 max_error=%u bad_channels=%u\n",iteration,scenario,phase,maximum,bad);
        }
    }
    // Reset/Init clears GPU resources twice in production. Managed indices are
    // not rewritten; only the dynamic VB is refreshed by the next water frame.
    auto beforeReset=capture(false,7,3);
    terrain12Reset();terrain12Init(gpu,queue,nullptr);
    require(!water12Draw(d,D3DPT_TRIANGLESTRIP,1,2,8,3,10),"default-pool vertex shadow invalidated by Reset");
    require(beforeReset==capture(true,7,3),"managed water indices survive renderer Reset/Init without rewrite");
    puts("PASS managed water IB survives renderer Reset/Init; pixels unchanged");
    // Water-only visual prototype: actual stock atlases, synthetic flat world
    // geometry and all 16 original corner masks. This is not a game screenshot.
    ComPtr<IDirect3DSurface9> originalTarget=target,originalDepth;CHECK(d->GetDepthStencilSurface(&originalDepth));
    for(UINT samples:{1u,4u}){
    ComPtr<IDirect3DSurface9> effectDepth;
    if(samples==4){target.Reset();CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_4_SAMPLES,0,FALSE,&target,nullptr));
        CHECK(d->CreateDepthStencilSurface(256,256,D3DFMT_D24S8,D3DMULTISAMPLE_4_SAMPLES,0,TRUE,&effectDepth,nullptr));
        CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&resolvedPreview,nullptr));
        CHECK(d->SetRenderTarget(0,target.Get()));CHECK(d->SetDepthStencilSurface(effectDepth.Get()));}
    previewScene=true;
    float previewConstants[28][4]={};for(int i=0;i<4;++i)previewConstants[1][i]=1;
    previewConstants[8][1]=1;previewConstants[10][0]=.05f;previewConstants[11][2]=.05f;previewConstants[12][1]=1;previewConstants[13][3]=1;previewConstants[27][1]=1;
    CHECK(d->SetVertexShaderConstantF(0,&previewConstants[0][0],28));rs(D3DRS_FOGENABLE,FALSE);
    terrain12TestWaterPreview(0,0);terrain12ConfigureWater(-10);terrain12AdvanceWater(1);
    require(terrain12TestWater().strength==0 && terrain12TestWater().phase==0,"disabled water and negative configuration");
    terrain12ConfigureWater(150);require(terrain12TestWater().strength==1,"water strength capped");
    terrain12AdvanceWater(-1);terrain12AdvanceWater(std::numeric_limits<double>::quiet_NaN());terrain12AdvanceWater(std::numeric_limits<double>::infinity());
    require(terrain12TestWater().phase==0,"invalid elapsed time ignored");
    terrain12AdvanceWater(30);require(std::abs(terrain12TestWater().phase-.1f)<.000001f,"stalled frame step capped");
    for(UINT i=0;i<100;++i)terrain12AdvanceWater(.1);require(terrain12TestWater().phase>=0 && terrain12TestWater().phase<6.283186f,"water phase wraps");
    terrain12TestWaterPreview(0,0);auto baseline=capture(true,7,3);
    require(baseline==capture(false,7,3),"disabled preview preserves original pixels exactly");
    terrain12ConfigureWater(100);auto first=capture(true,7,3);
    require(first==capture(true,7,3),"preview deterministic at fixed phase");
    for(UINT step=0;step<10;++step)terrain12AdvanceWater(.1);auto second=capture(true,7,3);
    UINT changed=0,animated=0;
    for(UINT i=0;i<65536;++i){changed+=baseline[i]!=first[i];animated+=first[i]!=second[i];}
    require(changed>1000 && animated>1000,"preview visibly changes water and animates");
    for(UINT y=192;y<256;++y)for(UINT x=192;x<256;++x)require(first[y*256+x]==0x182838 && second[y*256+x]==0x182838,"fully transparent shore mask stays empty");
    rs(D3DRS_FOGENABLE,TRUE);terrain12TestWaterPreview(0,0);auto fogged=capture(true,7,3);
    terrain12TestWaterPreview(1,1);require(fogged==capture(true,7,3),"full fog suppresses preview");
    rs(D3DRS_FOGENABLE,FALSE);terrain12TestWaterPreview(0,8);require(baseline==capture(true,7,3),"return to baseline after preview");
    if(iteration==0 && samples==1){
        ComPtr<IDirect3D9> api;CHECK(d->GetDirect3D(&api));D3DDEVICE_CREATION_PARAMETERS creation;CHECK(d->GetCreationParameters(&creation));
        D3DADAPTER_IDENTIFIER9 adapter;CHECK(api->GetAdapterIdentifier(creation.AdapterOrdinal,0,&adapter));
        auto save=[&](const char* name,const std::vector<DWORD>& pixels){
            char path[128];sprintf_s(path,"build/water-preview-%04x-%s.bmp",adapter.VendorId,name);FILE* file=nullptr;require(fopen_s(&file,path,"wb")==0,"preview output");
            BITMAPFILEHEADER fileHeader={};fileHeader.bfType=0x4d42;fileHeader.bfOffBits=sizeof(fileHeader)+sizeof(BITMAPINFOHEADER);fileHeader.bfSize=fileHeader.bfOffBits+256*256*4;
            BITMAPINFOHEADER info={};info.biSize=sizeof(info);info.biWidth=256;info.biHeight=-256;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
            require(fwrite(&fileHeader,sizeof fileHeader,1,file)==1 && fwrite(&info,sizeof info,1,file)==1 && fwrite(pixels.data(),4,pixels.size(),file)==pixels.size(),"preview pixels written");fclose(file);
        };save("baseline",baseline);save("phase0",first);save("phase1",second);
    }
    printf("PASS water preview iteration=%d samples=%u changed_pixels=%u animated_pixels=%u disabled_exact=1 transparent_mask=1 full_fog=1\n",iteration,samples,changed,animated);
    terrain12TestWaterPreview(0,0);
    puts("PASS water production setting/clock/clamp/wrap/invalid-time/off restoration");
    CHECK(d->SetRenderTarget(0,originalTarget.Get()));CHECK(d->SetDepthStencilSurface(originalDepth.Get()));target=originalTarget;resolvedPreview.Reset();
    }
    // Reject unsupported inputs, partial DISCARD data, and a forgotten/recycled buffer.
    require(!water12Draw(d,D3DPT_TRIANGLELIST,1,2,8,3,10),"wrong topology rejected");
    require(!water12Draw(d,D3DPT_TRIANGLESTRIP,1,2,8,3,UINT_MAX),"index overflow rejected");
    CHECK(d->SetVertexShader(shaders[0]));require(!water12Draw(d,D3DPT_TRIANGLESTRIP,1,2,8,3,10),"wrong shader rejected");CHECK(d->SetVertexShader(water.Get()));
    CHECK(vb->Lock(0,sizeof(WaterVertex),&mapped,D3DLOCK_DISCARD));memset(shadow(0,sizeof(WaterVertex),mapped,D3DLOCK_DISCARD),0,sizeof(WaterVertex));unlockShadow();CHECK(vb->Unlock());
    require(!water12Draw(d,D3DPT_TRIANGLESTRIP,1,2,8,3,10),"discarded unwritten range rejected");
    // Refresh the VB so a missing IB cannot be masked by the partial DISCARD.
    capture(false,7,3);
    terrain12ForgetBuffer(ib.Get());terrain12Reset();terrain12Init(gpu,queue,nullptr);
    capture(false,7,3);
    require(!water12Draw(d,D3DPT_TRIANGLESTRIP,1,2,8,3,10),"retired managed buffer not resurrected by Reset");
    terrain12ForgetBuffer(vb.Get());water12TestBind(nullptr,nullptr,nullptr);
    CHECK(saved->Apply());puts("PASS production water layout/strip/offsets/rewrite/handoff/lifecycle");
}
