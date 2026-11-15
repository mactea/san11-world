#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "object_capture.h"
#include <atomic>
#include <mutex>
#include <algorithm>
#include <string.h>

namespace {
constexpr UINT64 BUDGET=64ull<<20;
std::atomic<UINT64> allocated{0};
std::mutex mutex;
bool enabled=false;
UINT64 serial=0,epoch=0;
struct Entry {
    UINT size=0,offset=0,length=0;
    UINT64 generation=0;
    D3DPOOL pool=D3DPOOL_DEFAULT;
    DWORD usage=D3DUSAGE_WRITEONLY;
    void* mapped=nullptr;
    std::shared_ptr<const ObjectSnapshot> published;
    std::shared_ptr<ObjectSnapshot> writing;
};
std::map<IDirect3DResource9*,Entry> entries;
}
bool ObjectSnapshot::contains(UINT64 first,UINT64 last) const {
    if(first>last || last>bytes.size())return false;
    auto range=initialized.upper_bound(UINT(first));
    if(range==initialized.begin())return false;
    return std::prev(range)->second>=last;
}
void objectCaptureEnable(bool value){std::lock_guard<std::mutex> guard(mutex);enabled=value;}
void objectCaptureReset() {
    std::lock_guard<std::mutex> guard(mutex);enabled=false;++epoch;
    for(auto it=entries.begin();it!=entries.end();) {
        // An in-flight Lock owns the redirected pointer until Unlock, even if
        // Reset happens meanwhile. It may copy back, but may not publish again.
        if(it->second.writing){it->second.published.reset();++it;}
        else if(it->second.pool==D3DPOOL_MANAGED){it->second.generation=epoch;++it;}
        else it=entries.erase(it);
    }
}
void objectCaptureRegister(IDirect3DResource9* resource,UINT bytes,D3DPOOL pool,DWORD usage) {
    std::lock_guard<std::mutex> guard(mutex);
    entries.erase(resource);
    if(enabled && bytes && bytes<=(8u<<20) && entries.size()<16384) {
        auto& entry=entries[resource];entry.size=bytes;entry.generation=epoch;entry.pool=pool;entry.usage=usage;
    }
}
void objectCaptureForget(IDirect3DResource9* resource) {
    std::lock_guard<std::mutex> guard(mutex);entries.erase(resource);
}
void* objectCaptureLock(IDirect3DResource9* resource,UINT offset,UINT bytes,void* mapped,DWORD flags) {
    std::lock_guard<std::mutex> guard(mutex);
    auto it=entries.find(resource);if(it==entries.end())return mapped;
    auto& entry=it->second;
    if(flags&D3DLOCK_READONLY)return mapped;
    if(!enabled || entry.generation!=epoch || entry.writing || offset>entry.size || bytes>entry.size-offset) {
        entry.published.reset();return mapped;
    }
    std::shared_ptr<ObjectSnapshot> next;
    if(entry.published && entry.published.use_count()==1) {
        // All capture readers retain an owning shared_ptr. While the registry
        // lock is held no new reader can acquire this now-unobserved version.
        next=std::const_pointer_cast<ObjectSnapshot>(entry.published);entry.published.reset();
        if(flags&D3DLOCK_DISCARD)next->initialized.clear();
    } else {
        if(allocated.load()+entry.size>BUDGET){entry.published.reset();return mapped;}
        allocated+=entry.size;
        ObjectSnapshot* raw=new(std::nothrow) ObjectSnapshot;
        if(!raw){allocated-=entry.size;entry.published.reset();return mapped;}
        try {
            next=std::shared_ptr<ObjectSnapshot>(raw,[size=entry.size](ObjectSnapshot* p){delete p;allocated-=size;});
            next->bytes.resize(entry.size);
            if(entry.published && !(flags&D3DLOCK_DISCARD)) {
                next->bytes=entry.published->bytes;next->initialized=entry.published->initialized;
            }
        } catch(const std::bad_alloc&) {entry.published.reset();return mapped;}
    }
    entry.offset=offset;entry.length=bytes?bytes:entry.size-offset;entry.mapped=mapped;
    // A readable Lock can modify only part of its declared range. Seed it from
    // the driver so the redirected pointer preserves read/modify/write semantics.
    // Never read WRITEONLY or discarded driver storage.
    if(!(entry.usage&D3DUSAGE_WRITEONLY) && !(flags&D3DLOCK_DISCARD))
        memcpy(next->bytes.data()+offset,mapped,entry.length);
    entry.writing=std::move(next);
    return entry.writing->bytes.data()+offset;
}
void objectCaptureBeforeUnlock(IDirect3DResource9* resource) {
    std::lock_guard<std::mutex> guard(mutex);
    auto it=entries.find(resource);if(it==entries.end() || !it->second.writing)return;
    auto& entry=it->second;
    memcpy(entry.mapped,entry.writing->bytes.data()+entry.offset,entry.length);
}
void objectCaptureAfterUnlock(IDirect3DResource9* resource,HRESULT result) {
    std::lock_guard<std::mutex> guard(mutex);
    auto it=entries.find(resource);if(it==entries.end() || !it->second.writing)return;
    auto& entry=it->second;
    if(FAILED(result) || entry.generation!=epoch || !enabled){entries.erase(it);return;}
    auto& snapshot=*entry.writing;
    UINT first=entry.offset,last=first+entry.length;
    auto range=snapshot.initialized.lower_bound(first);
    if(range!=snapshot.initialized.begin()){auto previous=std::prev(range);if(previous->second>=first)range=previous;}
    while(range!=snapshot.initialized.end() && range->first<=last) {
        first=std::min(first,range->first);last=std::max(last,range->second);range=snapshot.initialized.erase(range);
    }
    try {snapshot.initialized[first]=last;}catch(const std::bad_alloc&){entries.erase(it);return;}
    snapshot.version=++serial;
    entry.published=std::move(entry.writing);entry.mapped=nullptr;
}
std::shared_ptr<const ObjectSnapshot> objectCaptureRead(IDirect3DResource9* resource) {
    std::lock_guard<std::mutex> guard(mutex);
    auto it=entries.find(resource);
    return enabled && it!=entries.end() && !it->second.writing?it->second.published:nullptr;
}
UINT64 objectCaptureBytes(){return allocated.load();}
