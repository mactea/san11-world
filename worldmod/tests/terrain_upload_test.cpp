// Range-cache correctness against actual D3D9 draws, without starting the game.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_dx12.h"
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
using Microsoft::WRL::ComPtr;
namespace {
void require(bool value,const char* label) {if(!value){printf("FAIL upload: %s\n",label);exit(1);}}
void check(HRESULT hr,const char* label) {if(FAILED(hr)){printf("FAIL upload: %s %08lx\n",label,hr);exit(1);}}
#define CHECK(x) check((x),#x)
struct Position {float x,y,z,nx,ny,nz;};
struct Material {DWORD color;float u,v;};
}
void testTerrain12Upload(IDirect3DDevice9* d,IDirect3DVertexShader9* const* shaders,int iteration) {
    float c[28][4]={};for(int i=0;i<4;++i){c[1][i]=1;c[10+i][i]=1;}c[27][1]=1;
    CHECK(d->SetVertexShaderConstantF(0,&c[0][0],28));CHECK(d->SetVertexShader(shaders[0]));
    for(auto s:{D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_FOGENABLE,D3DRS_ALPHATESTENABLE,D3DRS_ALPHABLENDENABLE})CHECK(d->SetRenderState(s,FALSE));
    const Position quad[]={{-.8f,.8f,.2f,0,0,1},{.8f,.8f,.2f,0,0,1},{-.8f,-.8f,.2f,0,0,1},{.8f,-.8f,.2f,0,0,1}};
    const Material color[]={{0xffffffff,0,0},{0xffffffff,1,0},{0xffffffff,0,1},{0xffffffff,1,1}};
    const UINT minimum[]={8,0,4,4,16,4,0,12,8,8},length[]={4,20,12,16,4,24,32,20,4,4};
    const WORD order[]={0,1,2,2,1,3};
    ComPtr<IDirect3DSurface9> target,readback;CHECK(d->GetRenderTarget(0,&target));
    CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    auto pixels=[&]() {
        CHECK(d->GetRenderTargetData(target.Get(),readback.Get()));D3DLOCKED_RECT lock;
        CHECK(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));std::vector<DWORD> result(256*256);
        for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)result[y*256+x]=((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]&0xffffff;
        CHECK(readback->UnlockRect());return result;
    };
    // A larger upload ring must not expand the per-resource capture limit.
    for(UINT bytes:{8u<<20,(8u<<20)+16}) {
        ComPtr<IDirect3DVertexBuffer9> buffer;
        CHECK(d->CreateVertexBuffer(bytes,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&buffer,nullptr));
        IDirect3DResource9* bindings[]={buffer.Get(),nullptr,nullptr,nullptr};terrain12TestBind(bindings,shaders);
        void* mapped;CHECK(buffer->Lock(0,0,&mapped,D3DLOCK_DISCARD));
        void* captured=terrain12Lock(buffer.Get(),0,0,mapped);
        require((captured!=mapped)==(bytes==(8u<<20)),"single-buffer capture retains 8 MiB limit");
        *(DWORD*)captured=0x12345678;terrain12Unlock(buffer.Get());CHECK(buffer->Unlock());
    }
    // Retain the former 8 MiB boundary and exercise both sides of the 12 MiB
    // boundary, including constants/alignment and reuse of the first draw.
    for(UINT count:{32u,232970u,233002u,349478u,349510u}) {
        const bool ranges=count==32,oversized=count==349510;
        std::vector<Position> positions(count);std::vector<Material> materials(count);
        for(UINT start: ranges?std::vector<UINT>{8,16}:std::vector<UINT>{0}) {
            std::copy(quad,quad+4,positions.begin()+start);std::copy(color,color+4,materials.begin()+start);
        }
        WORD indices[48];for(UINT draw=0;draw<8;++draw)for(UINT i=0;i<6;++i)
            indices[draw*6+i]=WORD(order[i]+(ranges?(draw==4||draw==7?16:8):0));
        ComPtr<IDirect3DVertexBuffer9> vb[2];ComPtr<IDirect3DIndexBuffer9> ib;
        CHECK(d->CreateVertexBuffer(count*sizeof(Position),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb[0],nullptr));
        CHECK(d->CreateVertexBuffer(count*sizeof(Material),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb[1],nullptr));
        CHECK(d->CreateIndexBuffer(sizeof indices,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&ib,nullptr));
        IDirect3DResource9* bindings[]={vb[0].Get(),vb[1].Get(),ib.Get(),nullptr};terrain12TestBind(bindings,shaders);
        void* mapped;CHECK(ib->Lock(0,0,&mapped,D3DLOCK_DISCARD));
        memcpy(terrain12Lock(ib.Get(),0,0,mapped),indices,sizeof indices);terrain12Unlock(ib.Get());CHECK(ib->Unlock());CHECK(d->SetIndices(ib.Get()));
        auto capture=[&](bool native) {
            for(UINT stream=0;stream<2;++stream) {
                UINT stride=stream?sizeof(Material):sizeof(Position);
                CHECK(vb[stream]->Lock(0,0,&mapped,D3DLOCK_DISCARD));
                memcpy(terrain12Lock(vb[stream].Get(),0,0,mapped),stream?(void*)materials.data():(void*)positions.data(),count*stride);
                terrain12Unlock(vb[stream].Get());CHECK(vb[stream]->Unlock());CHECK(d->SetStreamSource(stream,vb[stream].Get(),0,stride));
            }
            D3DVIEWPORT9 full={0,0,256,256,0,1};CHECK(d->SetViewport(&full));
            CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff182838,1,0));CHECK(d->BeginScene());terrain12TestClearWork();
            if(native) {
                require(!terrain12Draw(d,D3DPT_TRIANGLELIST,-1,0,4,0,2),"negative base rejected");
                require(!terrain12Draw(d,D3DPT_TRIANGLELIST,INT_MAX,UINT_MAX,4,0,2),"overflowing vertex range rejected");
                require(!terrain12Draw(d,D3DPT_TRIANGLELIST,0,0,count+1,0,2),"vertex range beyond shadow rejected");
                require(!terrain12Draw(d,D3DPT_TRIANGLELIST,0,0,4,UINT_MAX,2),"overflowing index range rejected");
                require(terrain12TestWork().copies==0,"rejected ranges make no upload copies");
            }
            for(UINT draw=0;draw<(ranges?10u:2u);++draw) {
                UINT lo=ranges?minimum[draw]:0,n=ranges?length[draw]:count,start=ranges?(draw<8?draw*6:0):0;
                if(ranges && draw==8) {
                    Position changed=quad[0];changed.x=-.1f;
                    CHECK(vb[0]->Lock(8*sizeof(Position),sizeof changed,&mapped,0));
                    memcpy(terrain12Lock(vb[0].Get(),8*sizeof(Position),sizeof changed,mapped),&changed,sizeof changed);
                    terrain12Unlock(vb[0].Get());CHECK(vb[0]->Unlock());
                }
                D3DVIEWPORT9 tile={ranges?draw*24:draw*128,0,ranges?24u:128u,256,0,1};CHECK(d->SetViewport(&tile));
                bool handled=native && terrain12Draw(d,D3DPT_TRIANGLELIST,0,lo,n,start,2);
                if(native)require(handled!=oversized,"near-capacity admission/fallback");
                if(!handled) {if(native)terrain12Flush();CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,lo,n,start,2));}
            }
            if(oversized) {
                if(native) {
                    auto work=terrain12TestWork();
                    printf("Oversized fallback submissions=%llu expected=2 iteration=%d\n",work.submissions,iteration);
                    require(work.submissions==2,"oversized draw does not submit a fresh empty ring before fallback");
                }
                // A rejected oversized draw must not poison the next native
                // batch or change its ordering relative to the fallback draws.
                D3DVIEWPORT9 tile={64,128,128,128,0,1};CHECK(d->SetViewport(&tile));
                if(native)require(terrain12Draw(d,D3DPT_TRIANGLELIST,0,0,4,0,2),"native draw recovers after oversized fallback");
                else CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,4,0,2));
            }
            if(native)terrain12Flush();CHECK(d->EndScene());return pixels();
        };
        auto reference=capture(false),actual=capture(true);require(reference==actual,"range growth/overlap/capacity RGB parity");
        require(std::count(reference.begin(),reference.end(),0x182838)<60000,"reference rendered");
        auto work=terrain12TestWork();
        if(ranges)require(work.vertexBytes==2976 && work.indexBytes==96 && work.copies==17 && work.batches==1,"contained ranges reuse larger uploads after growth and rewrite");
        if(!ranges && !oversized)require(work.vertexBytes==UINT64(count)*36 && work.copies==3 && work.batches==1,"near-full cached draw does not flush");
        if(oversized) {
            require(work.submissions==3 && work.batches==1,"two fallback handoffs followed by one native batch");
        }
        printf("PASS upload range/capacity count=%u iteration=%d vertex_bytes=%llu copies=%llu batches=%llu parity=exact\n",count,iteration,work.vertexBytes,work.copies,work.batches);
    }
}
