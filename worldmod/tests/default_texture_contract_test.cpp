// Read-only interop contract probe. No production DEFAULT-pool sampling yet.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d9on12.h>
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace {
void require(bool v,const char* s){if(!v){printf("FAIL default texture: %s\n",s);exit(1);}}
void check(HRESULT hr,const char* s){if(FAILED(hr)){printf("FAIL default texture: %s hr=%08lx\n",s,hr);exit(1);}}
#define CHECK(x) check((x),#x)
}
void testDefaultTextureContract(IDirect3DDevice9* device,ID3D12Device* gpu,ID3D12CommandQueue* queue,int iteration) {
    ComPtr<IDirect3DDevice9On12> bridge;CHECK(device->QueryInterface(IID_PPV_ARGS(&bridge)));
    ComPtr<ID3D12Fence> fence;CHECK(gpu->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));UINT64 serial=0;
    char probe[32]={};GetEnvironmentVariableA("WORLDMOD_DEFAULT_PROBE",probe,sizeof probe);
    // Direct checkout of newly updated resources crashed d3d9on12.dll in the
    // archived probes. Materialize through an original sampled draw first.
    for(auto format:{D3DFMT_A8R8G8B8,D3DFMT_DXT5}) {
        const UINT levels=3;const bool dynamic=false;
        ComPtr<IDirect3DTexture9> staging,texture;
        CHECK(device->CreateTexture(32,32,levels,0,format,D3DPOOL_SYSTEMMEM,&staging,nullptr));
        CHECK(device->CreateTexture(32,32,levels,0,format,D3DPOOL_DEFAULT,&texture,nullptr));
        std::vector<std::vector<BYTE>> expected(levels);
        auto fill=[&](IDirect3DTexture9* target,UINT phase,DWORD flags){
            for(UINT level=0;level<levels;++level){UINT width=32>>level,rows=format==D3DFMT_DXT5?width/4:width,rowBytes=format==D3DFMT_DXT5?width/4*16:width*4;
                expected[level].resize(size_t(rows)*rowBytes);D3DLOCKED_RECT lock;CHECK(target->LockRect(level,&lock,nullptr,flags));
                for(UINT y=0;y<rows;++y)for(UINT x=0;x<rowBytes;++x){BYTE value=BYTE(x*7+y*11+level*13+phase*19);expected[level][y*rowBytes+x]=value;((BYTE*)lock.pBits+y*lock.Pitch)[x]=value;}
                CHECK(target->UnlockRect(level));}
        };
        auto verify=[&](const char* operation,bool viaSurface){
            if(!strcmp(probe,"direct-surface"))viaSurface=true;
            printf("Begin default texture iteration=%d format=%u dynamic=%u operation=%s\n",iteration,format,dynamic,operation);fflush(stdout);
            if(!probe[0] || !strcmp(probe,"sample") || !strcmp(probe,"sample-only") || !strcmp(probe,"bind")) {
            ComPtr<IDirect3DStateBlock9> state;CHECK(device->CreateStateBlock(D3DSBT_ALL,&state));
            CHECK(device->SetVertexShader(nullptr));CHECK(device->SetPixelShader(nullptr));CHECK(device->SetFVF(0x104));CHECK(device->SetTexture(0,texture.Get()));
            for(auto setting:{D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_ALPHATESTENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_STENCILENABLE})CHECK(device->SetRenderState(setting,FALSE));
            CHECK(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));CHECK(device->SetRenderState(D3DRS_COLORWRITEENABLE,15));
            CHECK(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
            for(auto setting:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})CHECK(device->SetTextureStageState(0,setting,D3DTOP_SELECTARG1));
            for(auto setting:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})CHECK(device->SetTextureStageState(0,setting,D3DTA_TEXTURE));
            CHECK(device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));CHECK(device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
            for(auto setting:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})CHECK(device->SetSamplerState(0,setting,D3DTEXF_POINT));CHECK(device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
            struct Vertex{float x,y,z,rhw,u,v;};const Vertex quad[]={{0,0,0,1,0,0},{256,0,0,1,1,0},{0,256,0,1,0,1},{256,256,0,1,1,1}};
            D3DVIEWPORT9 vp={0,0,256,256,0,1};CHECK(device->SetViewport(&vp));if(strcmp(probe,"bind")){CHECK(device->BeginScene());CHECK(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex)));CHECK(device->EndScene());}
            ComPtr<IDirect3DSurface9> sampled,completed;CHECK(device->GetRenderTarget(0,&sampled));D3DSURFACE_DESC sampledDesc;CHECK(sampled->GetDesc(&sampledDesc));
            if(strcmp(probe,"sample-only") && strcmp(probe,"bind")){CHECK(device->CreateOffscreenPlainSurface(sampledDesc.Width,sampledDesc.Height,sampledDesc.Format,D3DPOOL_SYSTEMMEM,&completed,nullptr));CHECK(device->GetRenderTargetData(sampled.Get(),completed.Get()));}
            CHECK(state->Apply());
            } else if(!strcmp(probe,"preload")){texture->PreLoad();}
            else if(!strcmp(probe,"flush")){
                ComPtr<IDirect3DQuery9> query;CHECK(device->CreateQuery(D3DQUERYTYPE_EVENT,&query));CHECK(query->Issue(D3DISSUE_END));
                HRESULT hr=query->GetData(nullptr,0,D3DGETDATA_FLUSH);CHECK(hr);
            } else if(!strcmp(probe,"drain")){
                ComPtr<IDirect3DSurface9> target,read;CHECK(device->GetRenderTarget(0,&target));D3DSURFACE_DESC desc;CHECK(target->GetDesc(&desc));
                CHECK(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff182838,1,0));CHECK(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&read,nullptr));CHECK(device->GetRenderTargetData(target.Get(),read.Get()));
            }
            ComPtr<IDirect3DSurface9> surface;ComPtr<IDirect3DResource9> source;
            if(viaSurface){CHECK(texture->GetSurfaceLevel(0,&surface));source=surface;}else source=texture;
            if(probe[0]){puts("Probe checkpoint: before Unwrap");fflush(stdout);}
            ComPtr<ID3D12Resource> underlying;CHECK(bridge->UnwrapUnderlyingResource(source.Get(),queue,IID_PPV_ARGS(&underlying)));
            if(probe[0]){puts("Probe checkpoint: after Unwrap");fflush(stdout);}
            auto desc=underlying->GetDesc();require(desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.Width==32 && desc.Height==32 && desc.MipLevels==levels && desc.DepthOrArraySize==1,"unwrapped texture shape and mip chain");
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(levels);std::vector<UINT> rows(levels);std::vector<UINT64> rowBytes(levels);UINT64 bytes=0;
            gpu->GetCopyableFootprints(&desc,0,levels,0,footprints.data(),rows.data(),rowBytes.data(),&bytes);
            D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC buffer={};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ComPtr<ID3D12Resource> readback;CHECK(gpu->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
            ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> commands;
            CHECK(gpu->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
            CHECK(gpu->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)));
            D3D12_RESOURCE_BARRIER barrier={};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=underlying.Get();barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COMMON;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
            commands->ResourceBarrier(1,&barrier);
            for(UINT level=0;level<levels;++level){D3D12_TEXTURE_COPY_LOCATION from={},to={};from.pResource=underlying.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;from.SubresourceIndex=level;to.pResource=readback.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprints[level];commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);}
            std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);commands->ResourceBarrier(1,&barrier);CHECK(commands->Close());
            ID3D12CommandList* lists[]={commands.Get()};queue->ExecuteCommandLists(1,lists);
            UINT64 value=++serial;CHECK(queue->Signal(fence.Get(),value));ID3D12Fence* fences[]={fence.Get()};
            CHECK(bridge->ReturnUnderlyingResource(source.Get(),1,&value,fences));
            HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"fence event");CHECK(fence->SetEventOnCompletion(value,event));require(WaitForSingleObject(event,10000)==WAIT_OBJECT_0,"copy fence completes");CloseHandle(event);
            BYTE* mapped;D3D12_RANGE range={0,size_t(bytes)};CHECK(readback->Map(0,&range,(void**)&mapped));
            for(UINT level=0;level<levels;++level){require(rowBytes[level]*rows[level]==expected[level].size(),"payload footprint");for(UINT y=0;y<rows[level];++y)require(memcmp(mapped+footprints[level].Offset+y*footprints[level].Footprint.RowPitch,expected[level].data()+y*size_t(rowBytes[level]),size_t(rowBytes[level]))==0,"all mip bytes match after ownership handoff");}
            D3D12_RANGE noWrites={};readback->Unmap(0,&noWrites);
            printf("Default texture contract iteration=%d format=%u dynamic=%u operation=%s surface=%u levels=%u exact=1\n",iteration,format,dynamic,operation,viaSurface,levels);
        };
        fill(staging.Get(),1,0);CHECK(device->UpdateTexture(staging.Get(),texture.Get()));verify("UpdateTexture",false);
        fill(staging.Get(),2,0);CHECK(device->UpdateTexture(staging.Get(),texture.Get()));verify("rewrite",true);
        ComPtr<IDirect3DSurface9> from,to;CHECK(staging->GetSurfaceLevel(0,&from));CHECK(texture->GetSurfaceLevel(0,&to));
        auto old=expected;fill(staging.Get(),3,0);auto patch=expected;expected=old;
        RECT rect={0,0,8,8};POINT point={4,4};CHECK(device->UpdateSurface(from.Get(),&rect,to.Get(),&point));
        UINT block=format==D3DFMT_DXT5?4:1,unit=format==D3DFMT_DXT5?16:4,rowBytes=32/block*unit;
        for(UINT y=0;y<8/block;++y)memcpy(expected[0].data()+(4/block+y)*rowBytes+4/block*unit,patch[0].data()+y*rowBytes,8/block*unit);
        verify("partial-UpdateSurface",false);
    }
    // Keep the returned fence alive until D3D9 has consumed every handoff.
    ComPtr<IDirect3DSurface9> target,drained;CHECK(device->GetRenderTarget(0,&target));D3DSURFACE_DESC desc;CHECK(target->GetDesc(&desc));
    CHECK(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff182838,1,0));
    CHECK(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&drained,nullptr));
    CHECK(device->GetRenderTargetData(target.Get(),drained.Get()));
    puts("PASS non-dynamic default texture ownership/update/mips/partial-surface; all references released before Reset");
}
