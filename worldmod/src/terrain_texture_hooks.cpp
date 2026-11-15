#define WIN32_LEAN_AND_MEAN
#include "terrain_texture_hooks.h"
#include "terrain_dx12.h"
#include <map>
#include <wrl/client.h>

namespace {
using TextureLock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9*,UINT,D3DLOCKED_RECT*,const RECT*,DWORD);
using GetSurface=HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9*,UINT,IDirect3DSurface9**);
using SurfaceLock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DSurface9*,D3DLOCKED_RECT*,const RECT*,DWORD);
using UpdateTexture=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,IDirect3DBaseTexture9*,IDirect3DBaseTexture9*);
std::map<void**,TextureLock> textureLocks;
std::map<void**,GetSurface> surfaceGetters;
std::map<void**,SurfaceLock> surfaceLocks;
std::map<void**,UpdateTexture> textureUpdates;
SRWLOCK hookLock=SRWLOCK_INIT;

template<class T> void patch(void** table,int slot,T hook,T& original) {
    if(table[slot]==(void*)hook)return;
    DWORD protection;
    if(!VirtualProtect(&table[slot],sizeof(void*),PAGE_READWRITE,&protection))return;
    original=(T)table[slot]; table[slot]=(void*)hook;
    VirtualProtect(&table[slot],sizeof(void*),protection,&protection);
}
HRESULT STDMETHODCALLTYPE lockTexture(IDirect3DTexture9* texture,UINT level,D3DLOCKED_RECT* data,const RECT* rect,DWORD flags) {
    AcquireSRWLockShared(&hookLock); auto call=textureLocks.at(*(void***)texture); ReleaseSRWLockShared(&hookLock);
    HRESULT hr=call(texture,level,data,rect,flags);
    if(SUCCEEDED(hr) && !(flags&D3DLOCK_READONLY))terrain12TextureChanged(texture);
    return hr;
}
HRESULT STDMETHODCALLTYPE lockSurface(IDirect3DSurface9* surface,D3DLOCKED_RECT* data,const RECT* rect,DWORD flags) {
    AcquireSRWLockShared(&hookLock); auto call=surfaceLocks.at(*(void***)surface); ReleaseSRWLockShared(&hookLock);
    HRESULT hr=call(surface,data,rect,flags);
    if(SUCCEEDED(hr) && !(flags&D3DLOCK_READONLY)) {
        // Resolve the owner rather than retaining surface pointers across Reset.
        Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;
        if(SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&texture))))terrain12TextureChanged(texture.Get());
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE getSurface(IDirect3DTexture9* texture,UINT level,IDirect3DSurface9** surface) {
    AcquireSRWLockShared(&hookLock); auto call=surfaceGetters.at(*(void***)texture); ReleaseSRWLockShared(&hookLock);
    HRESULT hr=call(texture,level,surface);
    if(SUCCEEDED(hr) && surface && *surface) {
        void** table=*(void***)*surface;
        AcquireSRWLockExclusive(&hookLock);
        patch(table,13,lockSurface,surfaceLocks[table]);
        ReleaseSRWLockExclusive(&hookLock);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE updateTexture(IDirect3DDevice9* device,IDirect3DBaseTexture9* from,IDirect3DBaseTexture9* to) {
    AcquireSRWLockShared(&hookLock); auto call=textureUpdates.at(*(void***)device); ReleaseSRWLockShared(&hookLock);
    HRESULT hr=call(device,from,to);
    if(SUCCEEDED(hr))terrain12TextureChanged(to);
    return hr;
}
}

void terrain12HookTexture(IDirect3DTexture9* texture) {
    void** table=*(void***)texture;
    AcquireSRWLockExclusive(&hookLock);
    patch(table,19,lockTexture,textureLocks[table]);
    patch(table,18,getSurface,surfaceGetters[table]);
    ReleaseSRWLockExclusive(&hookLock);
}

void terrain12HookTextureUpdates(IDirect3DDevice9* device) {
    void** table=*(void***)device;
    AcquireSRWLockExclusive(&hookLock);
    patch(table,31,updateTexture,textureUpdates[table]);
    ReleaseSRWLockExclusive(&hookLock);
}
