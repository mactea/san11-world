#define WIN32_LEAN_AND_MEAN
#include "terrain_buffer_hooks.h"
#include "terrain_dx12.h"
#include "object_capture.h"
#include <map>
#include <intrin.h>

namespace {
using Lock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DResource9*,UINT,UINT,void**,DWORD);
using Unlock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DResource9*);
using Release=ULONG(STDMETHODCALLTYPE*)(IDirect3DResource9*);
struct Hooks {Lock lock=nullptr;Unlock unlock=nullptr;Release release=nullptr;};
std::map<void**,Hooks> hooks;
using ProcessVertices=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,UINT,IDirect3DVertexBuffer9*,IDirect3DVertexDeclaration9*,DWORD);
std::map<void**,ProcessVertices> processVertices;
SRWLOCK hookLock=SRWLOCK_INIT;

Hooks original(IDirect3DResource9* resource) {
    AcquireSRWLockShared(&hookLock);auto value=hooks.at(*(void***)resource);ReleaseSRWLockShared(&hookLock);return value;
}
HRESULT lockFrom(IDirect3DResource9* resource,UINT offset,UINT size,void** data,DWORD flags,uintptr_t caller) {
    // Stock model loader allocates count * index width, but passes count to
    // Lock at 0x44a844 and memcpy's the full allocation at 0x44a85c.
    // Normalize only this verified call and exact full-buffer shape, before
    // the driver and both capture layers see the range. Other locks use bytes.
    if(caller==0x44a847 && !offset && size && !flags && resource->GetType()==D3DRTYPE_INDEXBUFFER) {
        D3DINDEXBUFFER_DESC desc={};
        if(SUCCEEDED(((IDirect3DIndexBuffer9*)resource)->GetDesc(&desc))) {
            UINT width=desc.Format==D3DFMT_INDEX16?2:desc.Format==D3DFMT_INDEX32?4:0;
            if(width && UINT64(size)*width==desc.Size)size=desc.Size;
        }
    }
    HRESULT hr=original(resource).lock(resource,offset,size,data,flags);
    if(SUCCEEDED(hr) && data && *data && !(flags&D3DLOCK_READONLY)) {
        *data=terrain12Lock(resource,offset,size,*data,flags);
        *data=objectCaptureLock(resource,offset,size,*data,flags);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE lockBuffer(IDirect3DResource9* resource,UINT offset,UINT size,void** data,DWORD flags) {
    return lockFrom(resource,offset,size,data,flags,(uintptr_t)_ReturnAddress());
}
HRESULT STDMETHODCALLTYPE unlockBuffer(IDirect3DResource9* resource) {
    // Lock redirects driver -> terrain shadow -> immutable model staging.
    // Copy back in reverse order if both capture identities temporarily match.
    objectCaptureBeforeUnlock(resource);terrain12Unlock(resource);
    HRESULT hr=original(resource).unlock(resource);objectCaptureAfterUnlock(resource,hr);return hr;
}
HRESULT STDMETHODCALLTYPE processBuffer(IDirect3DDevice9* device,UINT source,UINT destination,UINT count,IDirect3DVertexBuffer9* buffer,IDirect3DVertexDeclaration9* declaration,DWORD flags) {
    AcquireSRWLockShared(&hookLock);auto call=processVertices.at(*(void***)device);ReleaseSRWLockShared(&hookLock);
    // GPU writes bypass Lock/Unlock. Retire CPU copies even after a failed call,
    // whose partial-write behavior must not leak stale data into native draws.
    terrain12Flush(Terrain12FlushReason::ProcessVertices);
    HRESULT hr=call(device,source,destination,count,buffer,declaration,flags);
    objectCaptureForget(buffer);terrain12ForgetBuffer(buffer);return hr;
}
ULONG STDMETHODCALLTYPE releaseBuffer(IDirect3DResource9* resource) {
    ULONG count=original(resource).release(resource);
    if(!count){terrain12ForgetBuffer(resource);objectCaptureForget(resource);}
    return count;
}
}

#ifdef WORLDMOD_RENDER_TEST
HRESULT terrain12TestLockFrom(IDirect3DResource9* resource,UINT offset,UINT size,void** data,DWORD flags,uintptr_t caller) {
    return lockFrom(resource,offset,size,data,flags,caller);
}
#endif

void terrain12HookBuffer(IDirect3DResource9* resource) {
    void** table=*(void***)resource;
    AcquireSRWLockExclusive(&hookLock);
    auto& saved=hooks[table];
    // Install all three slots together: a partial Lock-only hook would redirect
    // writes without copying them back at Unlock.
    DWORD protection;
    if(table[11]!=(void*)lockBuffer && VirtualProtect(table,13*sizeof(void*),PAGE_READWRITE,&protection)) {
        saved={(Lock)table[11],(Unlock)table[12],(Release)table[2]};
        table[11]=(void*)lockBuffer;table[12]=(void*)unlockBuffer;table[2]=(void*)releaseBuffer;
        VirtualProtect(table,13*sizeof(void*),protection,&protection);
    }
    ReleaseSRWLockExclusive(&hookLock);
}

void terrain12HookBufferWrites(IDirect3DDevice9* device) {
    void** table=*(void***)device;
    AcquireSRWLockExclusive(&hookLock);
    DWORD protection;
    if(table[85]!=(void*)processBuffer && VirtualProtect(&table[85],sizeof(void*),PAGE_READWRITE,&protection)) {
        processVertices[table]=(ProcessVertices)table[85];table[85]=(void*)processBuffer;
        VirtualProtect(&table[85],sizeof(void*),protection,&protection);
    }
    ReleaseSRWLockExclusive(&hookLock);
}
