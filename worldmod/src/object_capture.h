#pragma once
#include <d3d9.h>
#include <map>
#include <memory>
#include <vector>

// Immutable CPU bytes published only after a successful D3D9 Unlock. Readers
// retain a version while the loader prepares another one; no GPU readback.
// Internal readers must retain the owning shared_ptr for their whole access;
// raw pointers and weak_ptr views must not outlive that owning reference.
struct ObjectSnapshot {
    std::vector<BYTE> bytes;
    std::map<UINT,UINT> initialized;
    UINT64 version=0;
    bool contains(UINT64 first,UINT64 last) const;
};
void objectCaptureEnable(bool enabled);
void objectCaptureReset();
void objectCaptureRegister(IDirect3DResource9* resource,UINT bytes,D3DPOOL pool=D3DPOOL_DEFAULT,DWORD usage=D3DUSAGE_WRITEONLY);
void objectCaptureForget(IDirect3DResource9* resource);
void* objectCaptureLock(IDirect3DResource9* resource,UINT offset,UINT bytes,void* mapped,DWORD flags);
void objectCaptureBeforeUnlock(IDirect3DResource9* resource);
void objectCaptureAfterUnlock(IDirect3DResource9* resource,HRESULT result);
std::shared_ptr<const ObjectSnapshot> objectCaptureRead(IDirect3DResource9* resource);
UINT64 objectCaptureBytes();
