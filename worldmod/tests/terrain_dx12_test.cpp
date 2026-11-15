// Execute the stock EXE's VS 1.1 assembly and the production DX12 renderer with
// identical inputs. No game process, screenshots or hand-translated oracle.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_dx12.h"
#include "../build/terrain_reference.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
using Microsoft::WRL::ComPtr;

namespace {
void require(bool value, const char* label) {
    if(!value) { printf("FAIL native terrain: %s\n",label); exit(1); }
}
void check(HRESULT hr, const char* label) {
    if(FAILED(hr)) { printf("FAIL native terrain: %s hr=%08lx\n",label,hr); exit(1); }
}
#define CHECK(x) check((x),#x)
void logNative(const char* format, ...) {
    va_list args; va_start(args,format); vprintf(format,args); va_end(args); puts("");
}
}

void testTerrain12Textures(IDirect3DDevice9*,IDirect3DVertexShader9*,IDirect3DTexture9*,int);
void testTerrain12Performance(IDirect3DDevice9*,IDirect3DVertexShader9* const*,int);
void testTerrain12Upload(IDirect3DDevice9*,IDirect3DVertexShader9* const*,int);
void testWaterReference(IDirect3DDevice9*,IDirect3DVertexShader9* const*,int,ID3D12Device*,ID3D12CommandQueue*);
void testObjectRender(IDirect3DDevice9*,int);
void testScreenRender(IDirect3DDevice9*,int);
void testUserDraw(IDirect3DDevice9*,int);
void testNativeQuality(IDirect3DDevice9*,IDirect3DVertexShader9* const*,int);
void testTerrain12(IDirect3DDevice9* d, ID3D12Device* gpu, ID3D12CommandQueue* queue,IDirect3DTexture9* retained,int iteration) {
    ComPtr<IDirect3DStateBlock9> saved; CHECK(d->CreateStateBlock(D3DSBT_ALL,&saved));
    terrain12Init(gpu,queue,logNative); terrain12SetEnabled(true);
    // The Windows compiler exposes the legacy assembler; load only the system DLL.
    wchar_t path[MAX_PATH]; GetSystemDirectoryW(path,MAX_PATH); wcscat_s(path,L"\\d3dcompiler_47.dll");
    HMODULE compiler=LoadLibraryW(path); require(compiler!=nullptr,"legacy assembler library");
    using Assemble=HRESULT(WINAPI*)(const void*,SIZE_T,const char*,const D3D_SHADER_MACRO*,ID3DInclude*,UINT,ID3DBlob**,ID3DBlob**);
    auto assemble=(Assemble)GetProcAddress(compiler,"D3DAssemble");
    require(assemble!=nullptr,"legacy assembler export");
    ComPtr<IDirect3DVertexShader9> shaders[3];
    for(int mode=0;mode<3;++mode) {
        ComPtr<ID3DBlob> code,error;
        HRESULT hr=assemble(terrainReference[mode],strlen(terrainReference[mode]),"stock terrain",nullptr,nullptr,0,&code,&error);
        if(FAILED(hr) && error)printf("%s\n",(char*)error->GetBufferPointer());
        check(hr,"assemble original VS 1.1");
        CHECK(d->CreateVertexShader((DWORD*)code->GetBufferPointer(),&shaders[mode]));
    }
    FreeLibrary(compiler);
    const D3DVERTEXELEMENT9 elements[]={
        {0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},
        {1,0,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},
        {1,4,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    ComPtr<IDirect3DVertexDeclaration9> declaration; CHECK(d->CreateVertexDeclaration(elements,&declaration));
    struct Position { float x,y,z,nx,ny,nz; };
    struct Material { DWORD color; float u,v; };
    const Position positions[]={
        {-.8f,.8f,.2f,-.3f,.2f,.9f},{.8f,.8f,.7f,.9f,.1f,.4f},
        {-.8f,-.8f,.3f,.1f,.9f,.3f},{.8f,-.8f,.8f,.6f,-.2f,.7f}};
    const Material materials[]={{0x2080c020,0,0},{0xfff020e0,1,0},{0xc01080f0,0,1},{0x60802080,1,1}};
    const WORD indices[]={0,1,2,2,1,3};
    ComPtr<IDirect3DVertexBuffer9> vb[2]; ComPtr<IDirect3DIndexBuffer9> ib;
    CHECK(d->CreateVertexBuffer(sizeof positions,D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,0,D3DPOOL_DEFAULT,&vb[0],nullptr));
    CHECK(d->CreateVertexBuffer(sizeof materials,D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,0,D3DPOOL_DEFAULT,&vb[1],nullptr));
    CHECK(d->CreateIndexBuffer(sizeof indices,D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&ib,nullptr));
    IDirect3DResource9* buffers[]={vb[0].Get(),vb[1].Get(),ib.Get(),nullptr};
    IDirect3DVertexShader9* stock[]={shaders[0].Get(),shaders[1].Get(),shaders[2].Get()};
    terrain12TestBind(buffers,stock);
    auto upload=[&](int stream,const void* source,UINT size) {
        void* mapped=nullptr; CHECK(vb[stream]->Lock(0,size,&mapped,D3DLOCK_DISCARD));
        void* shadow=terrain12Lock(vb[stream].Get(),0,size,mapped);
        require(shadow!=mapped,"production WRITEONLY shadow capture"); memcpy(shadow,source,size);
        terrain12Unlock(vb[stream].Get()); CHECK(vb[stream]->Unlock());
    };
    upload(0,positions,sizeof positions); upload(1,materials,sizeof materials);
    void* mapped=nullptr; CHECK(ib->Lock(0,sizeof indices,&mapped,D3DLOCK_DISCARD));
    memcpy(terrain12Lock(ib.Get(),0,sizeof indices,mapped),indices,sizeof indices);
    terrain12Unlock(ib.Get()); CHECK(ib->Unlock());
    ComPtr<IDirect3DTexture9> texture;
    CHECK(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr));
    D3DLOCKED_RECT texels; CHECK(texture->LockRect(0,&texels,nullptr,0));
    for(UINT y=0;y<32;++y)for(UINT x=0;x<32;++x)
        ((DWORD*)((BYTE*)texels.pBits+y*texels.Pitch))[x]=D3DCOLOR_ARGB(64+x*6,32+x*6,40+y*6,80+x*3);
    CHECK(texture->UnlockRect(0));
    CHECK(d->SetVertexDeclaration(declaration.Get())); CHECK(d->SetPixelShader(nullptr));
    CHECK(d->SetStreamSource(0,vb[0].Get(),0,sizeof(Position))); CHECK(d->SetStreamSource(1,vb[1].Get(),0,sizeof(Material)));
    CHECK(d->SetIndices(ib.Get())); CHECK(d->SetTexture(0,texture.Get()));
    auto rs=[&](D3DRENDERSTATETYPE s,DWORD v){CHECK(d->SetRenderState(s,v));};
    rs(D3DRS_FILLMODE,D3DFILL_SOLID); rs(D3DRS_CULLMODE,D3DCULL_NONE); rs(D3DRS_ZENABLE,TRUE);
    rs(D3DRS_ZWRITEENABLE,TRUE); rs(D3DRS_ZFUNC,D3DCMP_LESSEQUAL); rs(D3DRS_LIGHTING,FALSE);
    rs(D3DRS_STENCILENABLE,FALSE); rs(D3DRS_SCISSORTESTENABLE,FALSE); rs(D3DRS_SRGBWRITEENABLE,FALSE);
    rs(D3DRS_WRAP0,0); rs(D3DRS_DEPTHBIAS,0); rs(D3DRS_SLOPESCALEDEPTHBIAS,0);
    rs(D3DRS_FOGTABLEMODE,D3DFOG_NONE); rs(D3DRS_FOGVERTEXMODE,D3DFOG_NONE); rs(D3DRS_FOGCOLOR,0xff406090);
    rs(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); rs(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    rs(D3DRS_BLENDOP,D3DBLENDOP_ADD); rs(D3DRS_SEPARATEALPHABLENDENABLE,FALSE); rs(D3DRS_COLORWRITEENABLE,15);
    rs(D3DRS_ALPHAFUNC,D3DCMP_GREATER); rs(D3DRS_ALPHAREF,64);
    CHECK(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    for(auto stage:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})CHECK(d->SetTextureStageState(0,stage,D3DTOP_MODULATE));
    for(auto stage:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})CHECK(d->SetTextureStageState(0,stage,D3DTA_TEXTURE));
    for(auto stage:{D3DTSS_COLORARG2,D3DTSS_ALPHAARG2})CHECK(d->SetTextureStageState(0,stage,D3DTA_DIFFUSE));
    CHECK(d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));
    CHECK(d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
    for(auto state:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_ADDRESSW})CHECK(d->SetSamplerState(0,state,D3DTADDRESS_CLAMP));
    CHECK(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR)); CHECK(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
    CHECK(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE)); CHECK(d->SetSamplerState(0,D3DSAMP_MAXMIPLEVEL,0));
    CHECK(d->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,0)); CHECK(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0));
    ComPtr<IDirect3DSurface9> target,readback; CHECK(d->GetRenderTarget(0,&target));
    CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    auto capture=[&](bool native,int mode,int test) {
        D3DVIEWPORT9 viewport,full={0,0,256,256,0,1}; CHECK(d->GetViewport(&viewport)); CHECK(d->SetViewport(&full));
        CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff182838,1,0));
        CHECK(d->SetViewport(&viewport)); CHECK(d->BeginScene());
        const bool layered=test==9||test==11||test==12;
        const int count=layered?3:test==10?1025:1;
        for(int layer=0;layer<count;++layer) {
            CHECK(d->SetVertexShader(shaders[layered?layer:mode].Get()));
            if(native)require(terrain12Draw(d,D3DPT_TRIANGLELIST,0,0,4,0,2),"draw accepted by native renderer");
            else CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,4,0,2));
            // Exercise native -> stock -> native ownership and shared depth.
            if((test==9||test==12) && layer==1) {
                if(native)terrain12Flush();
                ComPtr<IDirect3DVertexShader9> bound;CHECK(d->GetVertexShader(&bound));
                require(bound.Get()==shaders[1].Get(),"stock shader preserved across handoff");
                CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,4,0,2));
            }
        }
        if(native)terrain12Flush();
        CHECK(d->EndScene()); CHECK(d->GetRenderTargetData(target.Get(),readback.Get()));
        D3DLOCKED_RECT pixels; CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
        std::vector<DWORD> result(256*256);
        for(UINT y=0;y<256;++y)memcpy(result.data()+y*256,(BYTE*)pixels.pBits+y*pixels.Pitch,256*4);
        CHECK(readback->UnlockRect()); return result;
    };
    const char* cases[]={"plain","fog","alpha-blend","alpha-test","color-clamp","fog-clamp","viewport",
        "zero-alpha-shore","perspective","mixed-layers","batch-boundary","native-layers","mixed-no-depth-test"};
    bool allPassed=true;
    // Run the mixed case cold, before ALWAYS depth tests can mask first-use bugs.
    const int sequence[]={9,0,1,2,3,4,5,6,7,8,10,11,12};
    for(int mode=0;mode<3;++mode)for(int order=0;order<_countof(sequence);++order) {
        int test=sequence[order];
        float c[28][4]={};
        for(int i=0;i<4;++i)c[1][i]=1;
        c[2][1]=.5f; c[3][0]=c[3][1]=c[3][2]=.01f; c[3][3]=.7f;
        c[6][0]=1; c[7][1]=1; c[8][0]=1;
        for(int i=0;i<4;++i)c[10+i][i]=1;
        c[18][0]=1; c[26][0]=test==5?1.2f:.3f; c[26][1]=.5f;
        c[27][0]=.5f; c[27][1]=.5f;
        if(test==7)c[27][0]=.625f; // Cross zero alpha at the synthetic shoreline.
        if(test==8)c[13][0]=.2f;
        if(test==4) {c[24][0]=.6f;c[24][1]=-.4f;c[1][0]=1.5f;c[0][2]=-.3f;}
        CHECK(d->SetVertexShaderConstantF(0,&c[0][0],28)); CHECK(d->SetVertexShader(shaders[mode].Get()));
        rs(D3DRS_FOGENABLE,test==1||test==5||test==7||test==9||test==11||test==12);
        rs(D3DRS_ALPHABLENDENABLE,test==2||test==7||test==9||test==11||test==12); rs(D3DRS_ALPHATESTENABLE,test==3||test==7);
        rs(D3DRS_ZFUNC,test==12?D3DCMP_ALWAYS:D3DCMP_LESSEQUAL);
        rs(D3DRS_ALPHAREF,test==7?0:64);
        // 0x421e99: lighting uses SELECTARG1 (texture), alpha MODULATE,
        // clamp addressing, SRCALPHA/INVSRCALPHA, ALPHAFUNC GREATER/ref 0.
        CHECK(d->SetTextureStageState(0,D3DTSS_COLOROP,test==7?D3DTOP_SELECTARG1:D3DTOP_MODULATE));
        D3DVIEWPORT9 vp={0,0,256,256,0,1}; if(test==6)vp={17,23,211,197,.1f,.9f}; CHECK(d->SetViewport(&vp));
        auto reference=capture(false,mode,test),repeat=capture(false,mode,test),native=capture(true,mode,test);
        require(reference==repeat,"stock reference deterministic");
        unsigned maxError=0,bad=0,covered=0; double error=0;
        for(size_t i=0;i<reference.size();++i) {
            if((reference[i]&0xffffff)!=0x182838)++covered;
            unsigned pixelError=0;
            for(int shift=0;shift<24;shift+=8) {
                unsigned delta=abs(int((reference[i]>>shift)&255)-int((native[i]>>shift)&255));
                pixelError=std::max(pixelError,delta); error+=delta;
            }
            maxError=std::max(maxError,pixelError); if(pixelError>2)++bad;
        }
        printf("Native parity mode=%d case=%s covered=%u max_error=%u bad_pixels=%u mae=%.6f\n",
            mode,cases[test],covered,maxError,bad,error/(reference.size()*3));
        require(covered>1000,"reference actually rendered"); allPassed=allPassed && bad==0;
    }
    require(terrain12Active(),"native submission counted"); terrain12Status(stdout);
    testTerrain12Textures(d,shaders[0].Get(),retained,iteration);
    testTerrain12Performance(d,stock,iteration);
    testTerrain12Upload(d,stock,iteration);
    testNativeQuality(d,stock,iteration);
    testObjectRender(d,iteration);
    testScreenRender(d,iteration);
    testUserDraw(d,iteration);
    testWaterReference(d,stock,iteration,gpu,queue);
    terrain12ConfigureWater(25);terrain12AdvanceWater(.07);
    terrain12Reset(); terrain12TestBind(nullptr,nullptr); CHECK(saved->Apply());
    require(terrain12TestWater().strength==.25f && terrain12TestWater().phase==0,"Reset preserves water setting and restarts phase");terrain12ConfigureWater(0);
    auto cache=terrain12TestTextures();
    require(cache.bytes==0 && cache.entries==0 && cache.uploads==0,"Reset releases texture cache and counters");
    require(allPassed,"RGB parity within 2/255 per channel");
    puts("PASS native terrain: stock VS 1.1 versus production DX12 pixels");
}
