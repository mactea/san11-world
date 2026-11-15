#include "../src/object_capture.h"
#include <thread>
#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
namespace {
void require(bool value,const char* label){if(!value){printf("FAIL object snapshot: %s\n",label);exit(1);}}
}
void testObjectCapture() {
    auto resource=(IDirect3DResource9*)0x1000;
    BYTE gpuBytes[4096]={};
    objectCaptureReset();objectCaptureEnable(true);
    objectCaptureRegister(resource,sizeof gpuBytes,D3DPOOL_MANAGED,0);
    memset(gpuBytes,0x7b,sizeof gpuBytes);
    auto editable=(BYTE*)objectCaptureLock(resource,8,16,gpuBytes+8,0);
    require(editable[0]==0x7b && editable[15]==0x7b,"first readable Lock preserves driver contents");
    editable[3]=0x29;objectCaptureBeforeUnlock(resource);objectCaptureAfterUnlock(resource,S_OK);
    require(gpuBytes[8]==0x7b && gpuBytes[11]==0x29 && gpuBytes[23]==0x7b,"read-modify-write preserves untouched locked bytes");
    auto readable=objectCaptureRead(resource);
    require(readable && readable->contains(8,24) && !readable->contains(0,24),"only the readable locked range is published");
    objectCaptureForget(resource);readable.reset();
    objectCaptureReset();objectCaptureEnable(true);objectCaptureRegister(resource,sizeof gpuBytes,D3DPOOL_MANAGED);
    auto write=[&](BYTE value,UINT offset,UINT length,DWORD flags,HRESULT result=S_OK) {
        void* target=objectCaptureLock(resource,offset,length,gpuBytes+offset,flags);
        require(target!=gpuBytes+offset,"writes redirected to CPU memory");memset(target,value,length);
        require(!objectCaptureRead(resource),"in-flight write not readable");
        objectCaptureBeforeUnlock(resource);require(gpuBytes[offset]==value,"bytes copied forward before driver Unlock");
        objectCaptureAfterUnlock(resource,result);
    };
    write(7,0,sizeof gpuBytes,D3DLOCK_DISCARD);auto first=objectCaptureRead(resource);
    require(first && first->contains(0,sizeof gpuBytes),"full publication initialized");
    write(9,10,20,0);auto second=objectCaptureRead(resource);
    require(first->bytes[10]==7 && second->bytes[10]==9 && second->bytes[9]==7 && first->version!=second->version,"immutable versions and partial preservation");
    objectCaptureReset();objectCaptureEnable(true);
    require(objectCaptureRead(resource)==second,"managed snapshot survives Reset");
    write(2,10,20,D3DLOCK_DISCARD);auto partial=objectCaptureRead(resource);
    require(partial->contains(10,30) && !partial->contains(0,30),"discard invalidates unwritten data");
    write(3,0,10,0);write(4,30,10,0);
    require(objectCaptureRead(resource)->contains(0,40),"adjacent initialized ranges merged");
    write(1,0,40,0,E_FAIL);require(!objectCaptureRead(resource),"failed Unlock not published");
    objectCaptureRegister(resource,sizeof gpuBytes);
    void* target=objectCaptureLock(resource,0,sizeof gpuBytes,gpuBytes,D3DLOCK_DISCARD);
    objectCaptureReset();memset(target,5,sizeof gpuBytes);objectCaptureBeforeUnlock(resource);objectCaptureAfterUnlock(resource,S_OK);
    objectCaptureEnable(true);require(!objectCaptureRead(resource) && gpuBytes[0]==5,"Reset retains pending pointer but rejects stale publication");
    objectCaptureRegister(resource,sizeof gpuBytes);write(1,0,sizeof gpuBytes,D3DLOCK_DISCARD);
    std::atomic<bool> done{false};
    std::thread writer([&]{for(int i=0;i<1000;++i)write(BYTE(i),0,sizeof gpuBytes,D3DLOCK_DISCARD);done=true;});
    while(!done.load()) {
        auto snapshot=objectCaptureRead(resource);
        if(snapshot)for(BYTE value:snapshot->bytes)require(value==snapshot->bytes[0],"reader never observes torn publication");
    }
    writer.join();auto retained=objectCaptureRead(resource);objectCaptureForget(resource);
    require(!objectCaptureRead(resource) && retained->bytes[0]==BYTE(999),"retained snapshot survives release");
    objectCaptureRegister(resource,sizeof gpuBytes);write(6,0,sizeof gpuBytes,D3DLOCK_DISCARD);
    require(objectCaptureRead(resource)->version>retained->version,"reused address has new version");
    objectCaptureForget(resource);first.reset();second.reset();partial.reset();retained.reset();
    require(objectCaptureBytes()==0,"snapshot allocations released");
    objectCaptureRegister(resource,sizeof gpuBytes);
    write(7,0,sizeof gpuBytes,D3DLOCK_DISCARD);
    UINT64 version=objectCaptureRead(resource)->version;
    for(UINT i=0;i<128;++i) {
        UINT offset=8+i*8;void* memory=objectCaptureLock(resource,offset,8,gpuBytes+offset,0);
        require(memory!=gpuBytes+offset && objectCaptureBytes()==sizeof gpuBytes,"unique snapshot storage reused without a full-buffer clone");
        memset(memory,BYTE(i),8);objectCaptureBeforeUnlock(resource);objectCaptureAfterUnlock(resource,S_OK);
        auto next=objectCaptureRead(resource);require(next && next->version>version && next->bytes[0]==7 && next->bytes[offset]==BYTE(i),"reuse preserves untouched data and advances version");version=next->version;
    }
    objectCaptureForget(resource);require(!objectCaptureBytes(),"reused snapshot storage released");
    // Retained readers count against the budget even after resource release.
    constexpr UINT largeSize=8u<<20;
    std::vector<BYTE> largeGpu(largeSize);
    std::vector<std::shared_ptr<const ObjectSnapshot>> held;
    for(UINT i=0;i<8;++i) {
        auto key=(IDirect3DResource9*)(uintptr_t(0x2000)+i*16);objectCaptureRegister(key,largeSize);
        void* memory=objectCaptureLock(key,0,0,largeGpu.data(),D3DLOCK_DISCARD);
        require(memory!=largeGpu.data(),"zero-size Lock captures remaining buffer");memset(memory,BYTE(i),largeSize);
        objectCaptureBeforeUnlock(key);objectCaptureAfterUnlock(key,S_OK);
        auto snapshot=objectCaptureRead(key);require(snapshot && snapshot->contains(0,largeSize),"large snapshot published");
        require(objectCaptureLock(key,0,largeSize,largeGpu.data(),D3DLOCK_READONLY)==largeGpu.data(),"readonly Lock passes through");
        objectCaptureBeforeUnlock(key);objectCaptureAfterUnlock(key,S_OK);
        require(objectCaptureRead(key)==snapshot,"readonly Unlock preserves version");held.push_back(snapshot);objectCaptureForget(key);
    }
    require(objectCaptureBytes()==(64ull<<20),"retained snapshot budget accounted");
    objectCaptureRegister(resource,largeSize);
    require(objectCaptureLock(resource,0,0,largeGpu.data(),0)==largeGpu.data(),"budget saturation falls back");
    require(!objectCaptureRead(resource),"budget fallback cannot expose stale bytes");
    objectCaptureForget(resource);held.clear();require(!objectCaptureBytes(),"retained payload budget released");
    objectCaptureReset();puts("PASS object snapshots: concurrent publication/partial writes/failed Unlock/Reset/reuse/64MiB budget/readonly");
}
