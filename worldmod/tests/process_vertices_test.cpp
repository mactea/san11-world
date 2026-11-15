// Fault injection uses the production vtable hook, not a duplicate invalidator.
#define WIN32_LEAN_AND_MEAN
#include "../src/terrain_buffer_hooks.h"
#include "../src/object_capture.h"
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace {
void require(bool value,const char* message){if(!value){printf("FAIL ProcessVertices: %s\n",message);exit(1);}}
HRESULT result=S_OK;
UINT calls=0;
IDirect3DVertexBuffer9* destinationBuffer=nullptr;
HRESULT STDMETHODCALLTYPE driver(IDirect3DDevice9*,UINT source,UINT destination,UINT count,IDirect3DVertexBuffer9* buffer,IDirect3DVertexDeclaration9* declaration,DWORD flags) {
    ++calls;require(source==2 && destination==3 && count==4 && buffer==destinationBuffer && !declaration && flags==D3DPV_DONOTCOPYDATA,"driver arguments preserved");
    require(bool(objectCaptureRead(buffer)),"published version exists until driver call");return result;
}
}
void testProcessVerticesFaults() {
    void* table[86]={};table[85]=(void*)driver;void** tablePointer=table;
    auto device=(IDirect3DDevice9*)&tablePointer;
    destinationBuffer=(IDirect3DVertexBuffer9*)uintptr_t(0x2000);
    terrain12HookBufferWrites(device);terrain12HookBufferWrites(device);
    objectCaptureEnable(true);
    for(HRESULT failure:{S_OK,E_FAIL,D3DERR_INVALIDCALL}) {
        BYTE mapped[32]={};objectCaptureRegister(destinationBuffer,sizeof mapped);
        memset(objectCaptureLock(destinationBuffer,0,sizeof mapped,mapped,0),7,sizeof mapped);
        objectCaptureBeforeUnlock(destinationBuffer);objectCaptureAfterUnlock(destinationBuffer,S_OK);
        auto retained=objectCaptureRead(destinationBuffer);result=failure;
        require(device->ProcessVertices(2,3,4,destinationBuffer,nullptr,D3DPV_DONOTCOPYDATA)==failure,"HRESULT preserved");
        require(!objectCaptureRead(destinationBuffer),"GPU write invalidates captured version on every result");
        require(retained && retained->bytes[0]==7,"previous reader version remains immutable");
    }
    require(calls==3,"one driver call per hook call, repeated installation is safe");
    objectCaptureReset();puts("PASS ProcessVertices production hook: success/failure/invalid-call/retained reader/idempotent installation");
}
void testProcessVerticesDevice(IDirect3DDevice9* device) {
    // A readable buffer may have been initialized before capture was attached.
    ComPtr<IDirect3DVertexBuffer9> readable;
    require(SUCCEEDED(device->CreateVertexBuffer(32,0,0,D3DPOOL_MANAGED,&readable,nullptr)),"readable buffer creation");
    terrain12HookBuffer(readable.Get());void* editable=nullptr;
    require(SUCCEEDED(readable->Lock(0,0,&editable,0)),"readable initialization Lock");
    memset(editable,0x7b,32);require(SUCCEEDED(readable->Unlock()),"readable initialization Unlock");
    objectCaptureEnable(true);objectCaptureRegister(readable.Get(),32,D3DPOOL_MANAGED,0);
    require(SUCCEEDED(readable->Lock(8,16,&editable,0)),"read-modify-write Lock");
    require(((BYTE*)editable)[0]==0x7b && ((BYTE*)editable)[15]==0x7b,"readable hook preserves existing bytes");
    ((BYTE*)editable)[3]=0x29;require(SUCCEEDED(readable->Unlock()),"read-modify-write Unlock");
    auto captured=objectCaptureRead(readable.Get());
    require(captured && captured->contains(8,24) && captured->bytes[11]==0x29 && captured->bytes[8]==0x7b,"readable snapshot matches edit");
    objectCaptureForget(readable.Get());
    require(SUCCEEDED(readable->Lock(0,0,&editable,D3DLOCK_READONLY)),"readback readable edit");
    for(UINT i=0;i<32;++i)require(((BYTE*)editable)[i]==(i==11?0x29:0x7b),"driver retains every untouched byte");
    require(SUCCEEDED(readable->Unlock()),"readback Unlock");
    puts("PASS readable buffer hook: first partial read-modify-write matches driver bytes");
    // Stock loader 0x44a844 passes an index count to Lock, then copies bytes.
    // Exercise the production hook and verify BOTH driver storage and snapshot.
    for(D3DFORMAT format:{D3DFMT_INDEX16,D3DFMT_INDEX32}) {
        const UINT width=format==D3DFMT_INDEX16?2:4,count=9,bytes=count*width;
        ComPtr<IDirect3DIndexBuffer9> index;
        require(SUCCEEDED(device->CreateIndexBuffer(bytes,0,format,D3DPOOL_MANAGED,&index,nullptr)),"model index creation");
        terrain12HookBuffer(index.Get());
        void* memory=nullptr;
        require(SUCCEEDED(index->Lock(0,0,&memory,0)),"initialize driver indices");memset(memory,0,bytes);require(SUCCEEDED(index->Unlock()),"initialize driver Unlock");
        objectCaptureEnable(true);objectCaptureRegister(index.Get(),bytes,D3DPOOL_MANAGED);
        require(SUCCEEDED(terrain12TestLockFrom(index.Get(),0,count,&memory,0,0x44a847)),"stock model index Lock");
        memset(memory,0x35,bytes);require(SUCCEEDED(index->Unlock()),"stock model index Unlock");
        auto snapshot=objectCaptureRead(index.Get());
        require(snapshot && snapshot->contains(0,bytes),"stock model index count normalized to full byte range");
        objectCaptureForget(index.Get());
        require(SUCCEEDED(index->Lock(0,0,&memory,0)),"inspect driver index contents");
        for(UINT i=0;i<bytes;++i)require(((BYTE*)memory)[i]==0x35,"full index payload reaches original renderer");
        require(SUCCEEDED(index->Unlock()),"inspect driver Unlock");
        // An ordinary caller's partial Lock must retain byte semantics.
        objectCaptureRegister(index.Get(),bytes,D3DPOOL_MANAGED);
        require(SUCCEEDED(terrain12TestLockFrom(index.Get(),0,count,&memory,0,0)),"ordinary partial Lock");
        memset(memory,0x67,count);require(SUCCEEDED(index->Unlock()),"ordinary partial Unlock");
        snapshot=objectCaptureRead(index.Get());
        require(snapshot && snapshot->contains(0,count) && !snapshot->contains(0,bytes),"ordinary partial byte range unchanged");
        objectCaptureForget(index.Get());
    }
    puts("PASS stock model index loader: INDEX16/INDEX32 full payload and ordinary partial locks");
    ComPtr<IDirect3DStateBlock9> saved;require(SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&saved)),"save state");
    struct Input{float x,y,z;DWORD color;float u,v;};
    const Input vertices[]={{-.5f,.5f,.2f,0xff804020,0,0},{.5f,.5f,.3f,0xff408020,1,0},{0,-.5f,.4f,0xff204080,.5f,1}};
    ComPtr<IDirect3DVertexBuffer9> source,destination;
    require(SUCCEEDED(device->CreateVertexBuffer(sizeof vertices,0,0x142,D3DPOOL_MANAGED,&source,nullptr)),"source buffer");
    require(SUCCEEDED(device->CreateVertexBuffer(3*28,0,0x144,D3DPOOL_MANAGED,&destination,nullptr)),"destination buffer");
    void* mapped;require(SUCCEEDED(source->Lock(0,0,&mapped,0)),"source Lock");memcpy(mapped,vertices,sizeof vertices);require(SUCCEEDED(source->Unlock()),"source Unlock");
    require(SUCCEEDED(device->SetStreamSource(0,source.Get(),0,sizeof(Input))) && SUCCEEDED(device->SetFVF(0x142)) && SUCCEEDED(device->SetVertexShader(nullptr)),"source state");
    D3DMATRIX identity={};for(int i=0;i<4;++i)identity.m[i][i]=1;
    for(auto state:{D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION})require(SUCCEEDED(device->SetTransform(state,&identity)),"identity transform");
    device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_FOGENABLE,FALSE);
    objectCaptureEnable(true);objectCaptureRegister(destination.Get(),84,D3DPOOL_MANAGED);
    require(SUCCEEDED(destination->Lock(0,0,&mapped,0)),"destination Lock");
    memset(objectCaptureLock(destination.Get(),0,84,mapped,0),0,84);objectCaptureBeforeUnlock(destination.Get());
    HRESULT unlocked=destination->Unlock();objectCaptureAfterUnlock(destination.Get(),unlocked);require(SUCCEEDED(unlocked),"destination Unlock");
    require(bool(objectCaptureRead(destination.Get())),"destination capture populated");
    terrain12HookBufferWrites(device);
    HRESULT hr=device->ProcessVertices(0,0,3,destination.Get(),nullptr,0);
    require(!objectCaptureRead(destination.Get()),"real device call cannot retain stale CPU snapshot");
    if(SUCCEEDED(hr)) {
        D3DVIEWPORT9 viewport;require(SUCCEEDED(device->GetViewport(&viewport)),"destination viewport");
        require(SUCCEEDED(destination->Lock(0,0,&mapped,D3DLOCK_READONLY)),"processed data readable");
        const float* position=(const float*)mapped;
        require(position[0]==viewport.X+viewport.Width*.25f && position[1]==viewport.Y+viewport.Height*.25f && position[3]==1,"real driver transformed vertex unchanged by hook");
        require(SUCCEEDED(destination->Unlock()),"processed data Unlock");
    }
    printf("PASS ProcessVertices device: driver_hr=%08lx capture_invalidated=1\n",hr);
    require(SUCCEEDED(saved->Apply()),"restore state");objectCaptureReset();
}
