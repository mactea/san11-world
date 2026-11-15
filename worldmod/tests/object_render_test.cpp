// Original world-object VS 1.1 against the production native material path.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_dx12.h"
#include "../src/terrain_buffer_hooks.h"
#include "../src/object_capture.h"
#include "../build/terrain_reference.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <string>
using Microsoft::WRL::ComPtr;
namespace {
void require(bool v,const char* s){if(!v){printf("FAIL object rendering: %s\n",s);exit(1);}}
void check(HRESULT h,const char* s){if(FAILED(h)){printf("FAIL object rendering: %s %08lx\n",s,h);exit(1);}}
#define CHECK(x) check((x),#x)
}
static void testObjectRenderFamily(IDirect3DDevice9* d,int iteration,int family,UINT samples,D3DPRIMITIVETYPE topology=D3DPT_TRIANGLELIST) {
    ComPtr<IDirect3DStateBlock9> saved;CHECK(d->CreateStateBlock(D3DSBT_ALL,&saved));
    ComPtr<IDirect3DSurface9> originalTarget,originalDepth,multisampleTarget,multisampleDepth,resolved;
    CHECK(d->GetRenderTarget(0,&originalTarget));CHECK(d->GetDepthStencilSurface(&originalDepth));
    if(samples>1){
        CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,(D3DMULTISAMPLE_TYPE)samples,0,FALSE,&multisampleTarget,nullptr));
        CHECK(d->CreateDepthStencilSurface(256,256,D3DFMT_D24S8,(D3DMULTISAMPLE_TYPE)samples,0,TRUE,&multisampleDepth,nullptr));
        CHECK(d->CreateRenderTarget(256,256,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr));
        CHECK(d->SetRenderTarget(0,multisampleTarget.Get()));CHECK(d->SetDepthStencilSurface(multisampleDepth.Get()));
    }
    wchar_t path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);wcscat_s(path,L"\\d3dcompiler_47.dll");
    HMODULE compiler=LoadLibraryW(path);require(compiler!=nullptr,"legacy compiler");
    using Assemble=HRESULT(WINAPI*)(const void*,SIZE_T,const char*,const D3D_SHADER_MACRO*,ID3DInclude*,UINT,ID3DBlob**,ID3DBlob**);
    auto assemble=(Assemble)GetProcAddress(compiler,"D3DAssemble");require(assemble!=nullptr,"legacy assembler");
    ComPtr<IDirect3DVertexShader9> shaders[2];
    const int modes=family>=4?1:2;
    for(int i=0;i<(family>=7?0:modes);++i){ComPtr<ID3DBlob> code,error;CHECK(assemble(terrainReference[family==6?3:family>=4?12+family-4:4+family*2+i],strlen(terrainReference[family==6?3:family>=4?12+family-4:4+family*2+i]),"stock-object",nullptr,nullptr,0,&code,&error));CHECK(d->CreateVertexShader((DWORD*)code->GetBufferPointer(),&shaders[i]));}
    FreeLibrary(compiler);
    const uintptr_t sources[]={0x793728,0x793f70,0x793928,0x794150,0x793bd8,0x7943e0,0x7934d0,0x769dc8,0x74e7c0};
    if(family && family<7)for(int i=0;i<modes;++i)object12ShaderCreated(shaders[i].Get(),sources[family>=4?6+family-4:(family-1)*2+i]);
    else object12TestShaders(shaders[0].Get(),shaders[1].Get());
    const auto indexedType=family==5?D3DPT_TRIANGLESTRIP:topology;
    auto drawOriginal=[&](){return family==4?d->DrawPrimitive(D3DPT_TRIANGLESTRIP,3,2):d->DrawIndexedPrimitive(indexedType,1,2,4,3,2);};
    auto drawNative=[&](){return family==4?model12DrawPrimitive(d,D3DPT_TRIANGLESTRIP,3,2):family?model12Draw(d,indexedType,1,2,4,3,2):object12Draw(d,indexedType,1,2,4,3,2);};
    const UINT weightBytes=family==3?12:family==2?4:0,stride=family==7?24:family==8?20:family==9?12:family==4 || family==6?24:family==5?28:32+weightBytes;
    const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},{0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},
        {0,24,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},{1,0,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},D3DDECL_END()};
    std::vector<D3DVERTEXELEMENT9> declaration(elements,elements+5);
    if(weightBytes){declaration[1].Offset+=WORD(weightBytes);declaration[2].Offset+=WORD(weightBytes);
        D3DVERTEXELEMENT9 weight={0,12,BYTE(weightBytes==4?D3DDECLTYPE_FLOAT1:D3DDECLTYPE_FLOAT3),0,D3DDECLUSAGE_BLENDWEIGHT,0};declaration.insert(declaration.begin()+1,weight);}
    if(family>=4){declaration={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
        {0,12,BYTE(family!=5?D3DDECLTYPE_D3DCOLOR:D3DDECLTYPE_FLOAT2),0,BYTE(family!=5?D3DDECLUSAGE_COLOR:D3DDECLUSAGE_TEXCOORD),0},
        {0,WORD(family!=5?16:20),D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,BYTE(family!=5?0:1)},D3DDECL_END()};}
    ComPtr<IDirect3DVertexDeclaration9> decl;CHECK(d->CreateVertexDeclaration(declaration.data(),&decl));CHECK(d->SetVertexDeclaration(decl.Get()));CHECK(d->SetPixelShader(nullptr));
    const DWORD fixedFvf=family==7?0x142:family==8?0x102:0x2;if(family>=7)CHECK(d->SetFVF(fixedFvf));
    struct Vertex{float x,y,z,nx,ny,nz,u,v;};
    Vertex vertices[12]={};DWORD colors[12]={};
    const Vertex quad[]={{-.8f,.8f,.2f,.2f,.3f,.8f,0,0},{.8f,.8f,.3f,.8f,.1f,.3f,1,0},{-.8f,-.8f,.4f,.3f,.8f,.1f,0,1},{.8f,-.8f,.5f,-.3f,.4f,.9f,1,1}};
    const DWORD tint[]={0x60f04080,0xff40b0e0,0xa060d040,0xd0d080f0};
    for(int i=0;i<4;++i){vertices[4+i]=quad[i];colors[4+i]=tint[i];}
    WORD indices[12]={0,0,0,2,3,4,4,3,5};if(indexedType==D3DPT_TRIANGLESTRIP)indices[6]=5;
    ComPtr<IDirect3DVertexBuffer9> vb[2];ComPtr<IDirect3DIndexBuffer9> ib;
    CHECK(d->CreateVertexBuffer(stride*12,D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&vb[0],nullptr));
    CHECK(d->CreateVertexBuffer(sizeof colors,D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&vb[1],nullptr));
    CHECK(d->CreateIndexBuffer(sizeof indices,D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib,nullptr));
    IDirect3DResource9* resources[]={vb[0].Get(),vb[1].Get(),ib.Get()};UINT sizes[]={stride*12,sizeof colors,sizeof indices};
    for(int i=0;i<3;++i){terrain12ForgetBuffer(resources[i]);objectCaptureRegister(resources[i],sizes[i],D3DPOOL_MANAGED);if(iteration==2)terrain12HookBuffer(resources[i]);}
    auto upload=[&](int stream,const void* source,UINT offset,UINT size) {
        void* mapped;CHECK(stream==2?ib->Lock(offset,size,&mapped,0):vb[stream]->Lock(offset,size,&mapped,0));
        void* memory=iteration==2?mapped:objectCaptureLock(resources[stream],offset,size,mapped,0);memcpy(memory,source,size);
        if(iteration!=2)objectCaptureBeforeUnlock(resources[stream]);
        HRESULT result=stream==2?ib->Unlock():vb[stream]->Unlock();
        if(iteration!=2)objectCaptureAfterUnlock(resources[stream],result);CHECK(result);
    };
    auto pack=[&](const Vertex* input,UINT count,UINT phase){std::vector<BYTE> bytes(count*stride);
        for(UINT i=0;i<count;++i){BYTE* out=bytes.data()+i*stride;memcpy(out,input+i,12);
            const float weights[]={.15f+.1f*((i+phase)%4),.2f,.1f};
            if(family>=7){if(family==7){DWORD color=0xc0e09060;memcpy(out+12,&color,4);memcpy(out+16,&input[i].u,8);}else if(family==8)memcpy(out+12,&input[i].u,8);
            }else if(family>=4){
                if(family!=5){DWORD color=D3DCOLOR_ARGB(80+UINT(input[i].u*150),80+UINT(input[i].v*150),180,220);memcpy(out+12,&color,4);memcpy(out+16,&input[i].u,8);}
                else{memcpy(out+12,&input[i].u,8);float uv[]={input[i].v*.7f+.1f,input[i].u*.6f+.2f};memcpy(out+20,uv,8);}
            }else{if(weightBytes)memcpy(out+12,weights,weightBytes);memcpy(out+12+weightBytes,&input[i].nx,20);}}
        return bytes;};
    auto packed=pack(vertices,12,0);upload(0,packed.data(),0,UINT(packed.size()));upload(1,colors,0,sizeof colors);upload(2,indices,0,sizeof indices);
    CHECK(d->SetStreamSource(0,vb[0].Get(),stride,stride));CHECK(d->SetStreamSource(1,vb[1].Get(),sizeof(DWORD),sizeof(DWORD)));CHECK(d->SetIndices(ib.Get()));
    ComPtr<IDirect3DTexture9> textures[3];
    for(int stage=0;stage<3;++stage) {
        CHECK(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&textures[stage],nullptr));
        D3DLOCKED_RECT pixels;CHECK(textures[stage]->LockRect(0,&pixels,nullptr,0));
        for(UINT y=0;y<32;++y)for(UINT x=0;x<32;++x)((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x]=D3DCOLOR_ARGB(40+x*6,32+y*5,40+x*6,stage?220-y*4:70+x*3);
        CHECK(textures[stage]->UnlockRect(0));if(stage==2)continue;
        CHECK(d->SetTexture(stage,textures[stage].Get()));
        for(auto state:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})CHECK(d->SetSamplerState(stage,state,D3DTEXF_LINEAR));
        CHECK(d->SetSamplerState(stage,D3DSAMP_MIPFILTER,D3DTEXF_NONE));CHECK(d->SetSamplerState(stage,D3DSAMP_SRGBTEXTURE,0));
        for(auto state:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV})CHECK(d->SetSamplerState(stage,state,stage?D3DTADDRESS_CLAMP:D3DTADDRESS_WRAP));
        CHECK(d->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,stage));CHECK(d->SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
    }
    if(family==9)CHECK(d->SetTexture(0,nullptr));
    CHECK(d->SetTextureStageState(2,D3DTSS_COLOROP,D3DTOP_DISABLE));
    auto rs=[&](D3DRENDERSTATETYPE s,DWORD v){CHECK(d->SetRenderState(s,v));};
    rs(D3DRS_LIGHTING,FALSE);rs(D3DRS_SPECULARENABLE,FALSE);
    rs(D3DRS_MULTISAMPLEANTIALIAS,TRUE);rs(D3DRS_MULTISAMPLEMASK,UINT_MAX);
    rs(D3DRS_CULLMODE,D3DCULL_NONE);rs(D3DRS_ZENABLE,TRUE);rs(D3DRS_ZWRITEENABLE,TRUE);rs(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
    rs(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);rs(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);rs(D3DRS_BLENDOP,D3DBLENDOP_ADD);rs(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
    rs(D3DRS_ALPHAFUNC,D3DCMP_GREATER);rs(D3DRS_ALPHAREF,35);rs(D3DRS_FOGCOLOR,0xff406090);rs(D3DRS_SCISSORTESTENABLE,FALSE);
    ComPtr<IDirect3DSurface9> target,readback;CHECK(d->GetRenderTarget(0,&target));CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    auto capture=[&](bool native,int test) {
        auto initial=pack(vertices,12,0);upload(0,initial.data(),0,UINT(initial.size()));
        D3DVIEWPORT9 full={0,0,256,256,0,1};CHECK(d->SetViewport(&full));CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff182838,1,0));CHECK(d->BeginScene());
        float matrixRow[4];CHECK(d->GetVertexShaderConstantF(34,matrixRow,1));
        UINT draws=test==7?1026u:2u;
        for(UINT draw=0;draw<draws;++draw) {
            if(test==7 && family==0){CHECK(d->SetTexture(0,textures[draw%3].Get()));CHECK(d->SetTexture(1,textures[(draw+(draw%5?1:0))%3].Get()));
                if(native)terrain12TestTextureBudget((draw%5?2:1)*D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);}
            if(draw==1 || draw==64){auto moved=vertices[4];moved.x+=.13f;auto changed=pack(&moved,1,draw);upload(0,changed.data(),4*stride,stride);}
            if(family && test==1){float animated[4];memcpy(animated,matrixRow,sizeof animated);animated[3]+=.05f*draw;CHECK(d->SetVertexShaderConstantF(34,animated,1));}
            D3DVIEWPORT9 viewport=test==7?D3DVIEWPORT9{(draw%16)*16,((draw/16)%16)*16,16,16,0,1}:D3DVIEWPORT9{draw*128,0,128,256,0,1};CHECK(d->SetViewport(&viewport));
            if(native){require(drawNative(),"native object draw accepted");
                if(test==7 && family==0)require(terrain12TestTextures().bytes<=terrain12TestTextures().budget,"dual textures fit budget after eviction");}
            else CHECK(drawOriginal());
            if(test==6){if(native)terrain12Flush();CHECK(drawOriginal());}
        }
        if(native)terrain12Flush();CHECK(d->EndScene());CHECK(d->SetVertexShaderConstantF(34,matrixRow,1));
        if(test==7 && family==0){CHECK(d->SetTexture(0,textures[0].Get()));CHECK(d->SetTexture(1,textures[1].Get()));}
        if(samples>1)CHECK(d->StretchRect(target.Get(),nullptr,resolved.Get(),nullptr,D3DTEXF_NONE));
        CHECK(d->GetRenderTargetData(samples>1?resolved.Get():target.Get(),readback.Get()));
        D3DLOCKED_RECT pixels;CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));std::vector<DWORD> result(256*256);
        for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)result[y*256+x]=((DWORD*)((BYTE*)pixels.pBits+y*pixels.Pitch))[x]&0xffffff;
        CHECK(readback->UnlockRect());return result;
    };
    for(int mode=0;mode<modes;++mode)for(int test=0;test<8;++test) {
        CHECK(d->SetVertexShader(shaders[mode].Get()));
        if(family==0 && topology==D3DPT_TRIANGLESTRIP && test==0){
            terrain12RecordDraw(d,1,topology,false);
            FILE* report=nullptr;require(tmpfile_s(&report)==0,"object fallback diagnostic report");terrain12Status(report);rewind(report);
            std::string text;char chunk[4096];size_t length;while((length=fread(chunk,1,sizeof chunk,report)))text.append(chunk,length);fclose(report);
            bool profiling=text.find("Native fallback census enabled=1")!=std::string::npos;
            require(!profiling || text.find(mode?"shader_mode=5 calls=":"shader_mode=4 calls=")!=std::string::npos,"fallback census identifies object base and outline shaders");
        }
        float c[49][4]={};for(int i=0;i<4;++i){c[1][i]=1;c[4][i]=.8f;c[6+i][i]=1;c[10+i][i]=1;c[30+i][i]=1;}
        c[2][1]=.5f;c[3][0]=.03f;c[3][1]=.07f;c[3][2]=.01f;c[5][0]=.1f;c[5][1]=.2f;c[5][2]=.3f;c[5][3]=.7f;
        c[18][0]=.6f;c[18][1]=.2f;c[18][2]=.8f;c[26][0]=-.6f;c[26][1]=1;c[27][0]=-.2f;c[27][1]=1;
        if(family){
            c[46][0]=.17f;c[46][1]=-.23f;c[47][0]=.4f;c[47][1]=.7f;c[48][3]=.65f;
            c[24][0]=.08f;c[24][1]=-.1f;c[24][2]=.12f;
            for(int matrix=1;matrix<4;++matrix){int k=30+matrix*4;for(int i=0;i<4;++i)c[k+i][i]=1;
                c[k][0]=1-.07f*matrix;c[k][3]=.06f*matrix;c[k+1][3]=-.05f*matrix;c[k+2][3]=.025f*matrix;}
        }
        if(family==5){c[34][0]=.17f;c[34][1]=-.2f;c[38][0]=-.31f;c[38][1]=.27f;c[42][0]=.7f;c[42][1]=.8f;c[42][2]=.6f;c[42][3]=.9f;}
        if(test==1){c[30][0]=.7f;c[30][1]=.1f;c[30][3]=.12f;c[31][0]=-.1f;c[31][1]=.6f;}
        if(test==4){c[4][0]=1.8f;c[4][1]=-.3f;c[13][0]=.2f;}
        if(family>=7){D3DMATRIX world={},view={},projection={};for(int i=0;i<4;++i){view.m[i][i]=1;for(int j=0;j<4;++j){world.m[i][j]=c[30+j][i];projection.m[i][j]=c[10+j][i];}}
            if(test==2)view.m[3][2]=.12f;CHECK(d->SetTransform(D3DTS_WORLD,&world));CHECK(d->SetTransform(D3DTS_VIEW,&view));CHECK(d->SetTransform(D3DTS_PROJECTION,&projection));}
        CHECK(d->SetVertexShaderConstantF(0,&c[0][0],49));rs(D3DRS_FOGENABLE,family<7 && (test==2 || test==6));rs(D3DRS_ALPHABLENDENABLE,test==3 || test==6);rs(D3DRS_ALPHATESTENABLE,test==5);
        CHECK(d->SetTextureStageState(0,D3DTSS_COLOROP,family==9?D3DTOP_MODULATE:mode?D3DTOP_SELECTARG2:D3DTOP_MODULATE2X));
        CHECK(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE));
        for(auto s:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})CHECK(d->SetTextureStageState(0,s,D3DTA_TEXTURE));
        for(auto s:{D3DTSS_COLORARG2,D3DTSS_ALPHAARG2})CHECK(d->SetTextureStageState(0,s,D3DTA_DIFFUSE));
        CHECK(d->SetTextureStageState(1,D3DTSS_COLOROP,mode || family==4 || family>=6?D3DTOP_DISABLE:family==5?D3DTOP_BLENDTEXTUREALPHA:D3DTOP_MODULATE2X));CHECK(d->SetTextureStageState(1,D3DTSS_ALPHAOP,family==5?D3DTOP_DISABLE:D3DTOP_SELECTARG2));
        CHECK(d->SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_TEXTURE));CHECK(d->SetTextureStageState(1,D3DTSS_COLORARG2,test==4?D3DTA_DIFFUSE|D3DTA_COMPLEMENT:D3DTA_CURRENT));
        CHECK(d->SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE));CHECK(d->SetTextureStageState(1,D3DTSS_ALPHAARG2,D3DTA_CURRENT));
        auto textureBudget=terrain12TestTextures().budget;
        if(test==7 && family==0 && mode==0){
            auto before=terrain12TestTextures();terrain12TestTextureBudget(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
            CHECK(d->SetTexture(1,textures[2].Get()));require(!drawNative(),"oversized texture pair falls back before eviction");
            auto after=terrain12TestTextures();require(after.bytes==before.bytes && after.entries==before.entries && after.uploads==before.uploads,"oversized pair leaves cache unchanged");
            CHECK(d->SetTexture(1,textures[1].Get()));
        }
        if(test==7 && family==0)terrain12TestTextureBudget(2*D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
        auto reference=capture(false,test),native=capture(true,test);unsigned maximum=0,bad=0;
        terrain12TestTextureBudget(textureBudget);
        for(size_t i=0;i<reference.size();++i)for(int shift=0;shift<24;shift+=8){unsigned delta=abs(int((reference[i]>>shift)&255)-int((native[i]>>shift)&255));maximum=std::max(maximum,delta);if(delta>2)++bad;}
        printf("Object parity iteration=%d family=%d samples=%u mode=%d case=%d topology=%u max_error=%u bad_channels=%u\n",iteration,family,samples,mode,test,UINT(indexedType),maximum,bad);
        require(std::count(reference.begin(),reference.end(),0x182838)<65000,"reference visible");require(!bad,"original object versus native RGB parity");
    }
    rs(D3DRS_SHADEMODE,D3DSHADE_FLAT);require(!drawNative(),"unimplemented flat shading falls back for every model family");rs(D3DRS_SHADEMODE,D3DSHADE_GOURAUD);
    CHECK(d->SetVertexShader(nullptr));CHECK(d->SetFVF(0x112));require(!drawNative(),"unknown layout rejected");if(family>=7)CHECK(d->SetFVF(fixedFvf));else CHECK(d->SetVertexDeclaration(decl.Get()));
    CHECK(d->SetVertexShader(shaders[0].Get()));
    CHECK(d->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_TEMP));require(!drawNative(),"TEMP output rejected");
    CHECK(d->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_CURRENT));
    if(family && family<7){object12ShaderCreated(shaders[0].Get(),0);require(!drawNative(),"unknown recreation invalidates shader identity");}
    for(auto& item:shaders)object12ShaderCreated(item.Get(),0);
    for(auto resource:resources)objectCaptureForget(resource);
    object12TestShaders(nullptr,nullptr);CHECK(d->SetRenderTarget(0,originalTarget.Get()));CHECK(d->SetDepthStencilSurface(originalDepth.Get()));CHECK(saved->Apply());puts("PASS native object world matrix/dual texture/outline/alpha/fog/rewrite/handoff");
}

void testObjectRender(IDirect3DDevice9* d,int iteration){for(UINT samples:{1u,4u}){
    for(int family=0;family<10;++family)testObjectRenderFamily(d,iteration,family,samples);
    testObjectRenderFamily(d,iteration,0,samples,D3DPT_TRIANGLESTRIP);
}}
