// Synthetic CPU submission benchmark, not game FPS or GPU rendering time.
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
void require(bool value,const char* label) {
    if(!value) { printf("FAIL performance: %s\n",label); exit(1); }
}
void check(HRESULT hr,const char* label) {
    if(FAILED(hr)) { printf("FAIL performance: %s hr=%08lx\n",label,hr); exit(1); }
}
#define CHECK(x) check((x),#x)
UINT64 threadTime() {
    FILETIME created,exited,kernel,user; require(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)!=0,"thread times");
    return ((UINT64(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime)+((UINT64(user.dwHighDateTime)<<32)|user.dwLowDateTime);
}
UINT64 flushBatches(const char* reason) {
    FILE* f=nullptr;require(!tmpfile_s(&f) && f,"flush status file");terrain12Status(f);rewind(f);
    char line[256],name[32];UINT64 batches=0;
    while(fgets(line,sizeof line,f))if(sscanf_s(line,"Native flush %31s batches=%llu",name,(unsigned)sizeof name,&batches)==2) {
        if(!strcmp(name,reason)){fclose(f);return batches;}
    }
    fclose(f);require(false,"flush reason present");return 0;
}
}

void testTerrain12Performance(IDirect3DDevice9* d,IDirect3DVertexShader9* const* shaders,int iteration) {
    float c[28][4]={}; for(int i=0;i<4;++i) {c[1][i]=1;c[10+i][i]=1;} c[27][1]=1;
    CHECK(d->SetVertexShaderConstantF(0,&c[0][0],28)); CHECK(d->SetVertexShader(shaders[0]));
    for(auto state:{D3DRS_FOGENABLE,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHATESTENABLE,D3DRS_ALPHABLENDENABLE})
        CHECK(d->SetRenderState(state,FALSE));
    for(auto state:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})CHECK(d->SetTextureStageState(0,state,D3DTOP_SELECTARG1));
    D3DVIEWPORT9 viewport={0,0,256,256,0,1}; CHECK(d->SetViewport(&viewport));
    ComPtr<IDirect3DSurface9> target,readback; CHECK(d->GetRenderTarget(0,&target));
    CHECK(d->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
    auto pixels=[&]() {
        CHECK(d->GetRenderTargetData(target.Get(),readback.Get()));
        D3DLOCKED_RECT lock; CHECK(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));
        std::vector<DWORD> result(256*256);
        for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x)result[y*256+x]=((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]&0xffffff;
        CHECK(readback->UnlockRect()); return result;
    };
    LARGE_INTEGER frequency; QueryPerformanceFrequency(&frequency);
    // Sparse draws should not upload the unused prefix. A declared full span
    // still exercises capacity splits. The 9.5 MiB workload targets the observed
    // upload volume, not a replay of the game's exact draw sequence.
    for(int scenario=0;scenario<5;++scenario) {
        const UINT count=scenario==4?92160:scenario?131072:4,base=scenario>=3?0:count-4;
        const UINT vertices=scenario>=3?count:4;
        const char* name=scenario==0?"small-reuse":scenario==1?"large-reuse":scenario==2?"large-rewrite":scenario==3?"full-span-rewrite":"9.5MiB-rewrite";
        struct Position {float x,y,z,nx,ny,nz;};
        struct Material {DWORD color;float u,v;};
        const Position quad[]={{-.8f,.8f,.2f,0,0,1},{.8f,.8f,.2f,0,0,1},{-.8f,-.8f,.2f,0,0,1},{.8f,-.8f,.2f,0,0,1}};
        const Material colors[]={{0xffffffff,0,0},{0xffffffff,1,0},{0xffffffff,0,1},{0xffffffff,1,1}};
        const WORD indices[]={0,1,2,2,1,3};
        ComPtr<IDirect3DVertexBuffer9> vb[2]; ComPtr<IDirect3DIndexBuffer9> ib;
        CHECK(d->CreateVertexBuffer(count*sizeof(Position),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb[0],nullptr));
        CHECK(d->CreateVertexBuffer(count*sizeof(Material),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb[1],nullptr));
        CHECK(d->CreateIndexBuffer(sizeof indices,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&ib,nullptr));
        IDirect3DResource9* buffers[]={vb[0].Get(),vb[1].Get(),ib.Get(),nullptr}; terrain12TestBind(buffers,shaders);
        for(int stream=0;stream<2;++stream) {
            UINT stride=stream?sizeof(Material):sizeof(Position); void* mapped;
            CHECK(vb[stream]->Lock(0,0,&mapped,D3DLOCK_DISCARD));
            BYTE* shadow=(BYTE*)terrain12Lock(vb[stream].Get(),0,0,mapped);
            memset(shadow,0,count*stride); memcpy(shadow+base*stride,stream?(const void*)colors:(const void*)quad,4*stride);
            terrain12Unlock(vb[stream].Get()); CHECK(vb[stream]->Unlock()); CHECK(d->SetStreamSource(stream,vb[stream].Get(),0,stride));
        }
        void* mapped; CHECK(ib->Lock(0,0,&mapped,D3DLOCK_DISCARD));
        memcpy(terrain12Lock(ib.Get(),0,0,mapped),indices,sizeof indices); terrain12Unlock(ib.Get()); CHECK(ib->Unlock()); CHECK(d->SetIndices(ib.Get()));
        auto commands=[&](bool native) {
            for(int draw=0;draw<32;++draw) {
                if(scenario>=2 && draw%8==0) {
                    Position changed=quad[0]; changed.x=(draw/8)&1?-.4f:-.8f;
                    CHECK(vb[0]->Lock(base*sizeof(Position),sizeof changed,&mapped,0));
                    memcpy(terrain12Lock(vb[0].Get(),base*sizeof(Position),sizeof changed,mapped),&changed,sizeof changed);
                    terrain12Unlock(vb[0].Get()); CHECK(vb[0]->Unlock());
                    D3DVIEWPORT9 strip={UINT(draw/8)*64,0,64,256,0,1}; CHECK(d->SetViewport(&strip));
                }
                if(native)require(terrain12Draw(d,D3DPT_TRIANGLELIST,base,0,vertices,0,2),"native benchmark draw accepted");
                else CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,base,0,vertices,0,2));
            }
            if(native)terrain12Flush();
        };
        CHECK(d->SetViewport(&viewport)); CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff182838,1,0)); CHECK(d->BeginScene()); commands(false); CHECK(d->EndScene());
        auto reference=pixels(); require(std::count(reference.begin(),reference.end(),0x182838)<60000,"benchmark reference rendered");
        std::vector<double> elapsed[2];
        for(int sample=-1;sample<7;++sample)for(int order=0;order<2;++order) {
            // Alternate ordering to reduce sequential warmup/clock bias.
            const bool prefix=((sample+1+order)&1)!=0;
            terrain12TestPrefixUpload(prefix);
            CHECK(d->SetViewport(&viewport)); CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff182838,1,0)); CHECK(d->BeginScene());
            terrain12TestClearWork(); auto textureBefore=terrain12TestTextures().uploads;
            auto uploadBefore=flushBatches("upload_limit:"),explicitBefore=flushBatches("explicit:");
            LARGE_INTEGER begin,end; UINT64 cpu=threadTime(); QueryPerformanceCounter(&begin);
            commands(true);
            QueryPerformanceCounter(&end); cpu=threadTime()-cpu;
            CHECK(d->EndScene()); auto actual=pixels();
            require(reference==actual,"benchmark stock/native RGB pixels identical");
            auto work=terrain12TestWork();
            bool fullRewrite=scenario==3 || (prefix && scenario==2);
            if(scenario==4 && sample<0)printf("9.5 MiB capacity probe batches=%llu expected=1 vertex_bytes=%llu copies=%llu parity=exact\n",work.batches,work.vertexBytes,work.copies);
            require(work.batches==UINT64(fullRewrite?2:1),"cached spans do not force premature batch flushes");
            require(flushBatches("upload_limit:")-uploadBefore==UINT64(iteration==1 && fullRewrite),"only upload capacity causes an upload-limit batch");
            require(flushBatches("explicit:")-explicitBefore==UINT64(iteration==1),"explicit final batch counted only with profiling");
            auto finished=flushBatches("explicit:");terrain12Flush();terrain12Flush();
            require(flushBatches("explicit:")==finished,"no-op flushes do not inflate batch census");
            require(work.copies==UINT64(fullRewrite?8:scenario>=2?6:3),"only missing buffer versions are copied");
            require(work.vertexBytes==UINT64(fullRewrite?15728640:scenario==4?count*108:prefix && scenario==1?4718592:scenario==2?432:144),"declared or reference vertex range uploaded");
            require(work.indexBytes==UINT64(fullRewrite?24:12),"index buffer reused within batch");
            require(work.srvs==work.batches && work.samplers==work.batches,"one unchanged view and sampler per batch");
            if(sample<0)continue;
            double us=double(end.QuadPart-begin.QuadPart)*1000000/frequency.QuadPart;
            elapsed[prefix].push_back(us);
            printf("Terrain perf scenario=%s upload=%s iteration=%d sample=%d draws=32 submit_us=%.3f thread_us=%.3f vertex_bytes=%llu index_bytes=%llu copies=%llu hits=%llu batches=%llu srvs=%llu samplers=%llu texture_uploads=%llu parity=exact\n",
                name,prefix?"prefix":"compact",iteration,sample,us,double(cpu)/10,work.vertexBytes,work.indexBytes,work.copies,work.hits,work.batches,work.srvs,work.samplers,terrain12TestTextures().uploads-textureBefore);
        }
        terrain12TestPrefixUpload(false);
        for(int prefix=0;prefix<2;++prefix) {
            std::sort(elapsed[prefix].begin(),elapsed[prefix].end());
            printf("Terrain perf median scenario=%s upload=%s iteration=%d submit_us=%.3f samples=7\n",name,prefix?"prefix":"compact",iteration,elapsed[prefix][3]);
        }
    }

    // Different base/minimum/stream/start-index offsets must not alias a cached
    // range. Separate tiles preserve every draw across a rewrite and batch wrap.
    struct Position {float x,y,z,nx,ny,nz;};
    struct Material {DWORD color;float u,v;};
    Position positions[32]={};Material materials[32]={};WORD indices[15]={};
    const UINT bases[]={6,14},minimums[]={2,4},starts[]={3,9};
    const Position quad[]={{-.8f,.8f,.2f,0,0,1},{.8f,.8f,.2f,0,0,1},{-.8f,-.8f,.2f,0,0,1},{.8f,-.8f,.2f,0,0,1}};
    const WORD order[]={0,1,2,2,1,3};
    for(UINT variant=0;variant<2;++variant) {
        UINT first=bases[variant]+minimums[variant];
        for(UINT i=0;i<4;++i) {
            positions[first+1+i]=quad[i];
            if(variant)positions[first+1+i].x*=.5f;
            materials[first+2+i]={variant?0xff40a0ff:0xffffa040,float(i&1),float(i/2)};
        }
        for(UINT i=0;i<6;++i)indices[starts[variant]+i]=WORD(order[i]+minimums[variant]);
    }
    ComPtr<IDirect3DVertexBuffer9> vb[2];ComPtr<IDirect3DIndexBuffer9> ib;
    CHECK(d->CreateVertexBuffer(sizeof positions,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb[0],nullptr));
    CHECK(d->CreateVertexBuffer(sizeof materials,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb[1],nullptr));
    CHECK(d->CreateIndexBuffer(sizeof indices,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&ib,nullptr));
    IDirect3DResource9* buffers[]={vb[0].Get(),vb[1].Get(),ib.Get(),nullptr};terrain12TestBind(buffers,shaders);
    void* mapped;CHECK(ib->Lock(0,0,&mapped,D3DLOCK_DISCARD));
    memcpy(terrain12Lock(ib.Get(),0,0,mapped),indices,sizeof indices);terrain12Unlock(ib.Get());CHECK(ib->Unlock());
    CHECK(d->SetIndices(ib.Get()));
    CHECK(d->SetStreamSource(0,vb[0].Get(),sizeof(Position),sizeof(Position)));
    CHECK(d->SetStreamSource(1,vb[1].Get(),2*sizeof(Material),sizeof(Material)));
    auto ranges=[&](bool native) {
        for(int stream=0;stream<2;++stream) {
            CHECK(vb[stream]->Lock(0,0,&mapped,D3DLOCK_DISCARD));
            memcpy(terrain12Lock(vb[stream].Get(),0,0,mapped),stream?(void*)materials:(void*)positions,stream?sizeof materials:sizeof positions);
            terrain12Unlock(vb[stream].Get());CHECK(vb[stream]->Unlock());
        }
        CHECK(d->SetViewport(&viewport));CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff182838,1,0));CHECK(d->BeginScene());
        terrain12TestClearWork();
        for(UINT draw=0;draw<1025;++draw) {
            UINT variant=draw%2;
            if(draw==64) {
                const UINT at=bases[0]+minimums[0]+1;
                Position changed=positions[at];changed.x=-.2f;
                CHECK(vb[0]->Lock(at*sizeof(Position),sizeof changed,&mapped,0));
                memcpy(terrain12Lock(vb[0].Get(),at*sizeof(Position),sizeof changed,mapped),&changed,sizeof changed);
                terrain12Unlock(vb[0].Get());CHECK(vb[0]->Unlock());
            }
            D3DVIEWPORT9 tile={(draw%16)*16,((draw/16)%16)*16,16,16,0,1};CHECK(d->SetViewport(&tile));
            if(native)require(terrain12Draw(d,D3DPT_TRIANGLELIST,bases[variant],minimums[variant],4,starts[variant],2),"offset range draw accepted");
            else CHECK(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,bases[variant],minimums[variant],4,starts[variant],2));
        }
        if(native)terrain12Flush();CHECK(d->EndScene());return pixels();
    };
    auto reference=ranges(false);auto limitBefore=flushBatches("draw_limit:");auto actual=ranges(true);
    require(reference==actual,"offset ranges/rewrite/batch-wrap stock/native RGB identical");
    require(std::count(reference.begin(),reference.end(),0x182838)<64000,"offset range reference rendered");
    auto work=terrain12TestWork();
    require(work.batches==2 && work.copies==11 && work.vertexBytes==624 && work.indexBytes==36,"range keys and compact upload accounting");
    require(flushBatches("draw_limit:")-limitBefore==UINT64(iteration==1),"descriptor draw limit attributed separately from upload limit");
    printf("PASS compact upload offsets/rewrite/batch-wrap iteration=%d vertex_bytes=%llu index_bytes=%llu copies=%llu batches=%llu parity=exact\n",
        iteration,work.vertexBytes,work.indexBytes,work.copies,work.batches);
}
