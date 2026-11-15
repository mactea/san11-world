#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <stdio.h>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "trade_ship_visual.h"

namespace {
const uintptr_t ACTORS=0x9d2210, INSTANCES=0xa24220;
int actorUnits[4096]={},instanceUnits[128]={}; // unitId+1; overwritten on every reuse
TradeShipIdentityFn identityFn=nullptr;
TraceLogFn visualLog=nullptr;
std::wstring assetDir;
std::vector<uint8_t> mesh,textureBytes;
uint8_t model[0x2c]={};
uint8_t* boatModel=(uint8_t*)(0x8a0f2c+327*0x2c);
IDirect3DDevice9* owner=nullptr; // borrowed; resources released before device recreation
IDirect3DTexture9* texture=nullptr;
bool failed=false,installed=false;
using Enqueue=void*(__thiscall*)(void*,void*,void*);
using Bind=int(__thiscall*)(void*,void*);
using Load=int(__thiscall*)(void*,void*,void*);
using Upload=HRESULT(__thiscall*)(void*,IDirect3DDevice9*);
using Release=HRESULT(__thiscall*)(void*,void*);
using Draw=HRESULT(__thiscall*)(void*,IDirect3DDevice9*,void*,void*,int,void*,unsigned);
Enqueue enqueueNative=(Enqueue)0x411f50;
Bind bindNative=(Bind)0x44a460;
Load loadNative=(Load)0x44a5d0;
Upload uploadNative=(Upload)0x44a660;
Release releaseNative=(Release)0x44a8b0;
Draw drawNative=(Draw)0x44ae20;
unsigned* outlineEnabled=(unsigned*)0x8a5bb8;

unsigned word32(const uint8_t* b,size_t at) { unsigned v;memcpy(&v,b+at,4);return v; }
int slot(const void* p,uintptr_t base,size_t stride,size_t count) {
    uintptr_t at=(uintptr_t)p;
    return at>=base && (at-base)%stride==0 && (at-base)/stride<count ? int((at-base)/stride) : -1;
}
bool readFile(const std::wstring& path,std::vector<uint8_t>& out) {
    FILE* f=nullptr;if(_wfopen_s(&f,path.c_str(),L"rb") || !f)return false;
    fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    if(size<24 || size>4*1024*1024){fclose(f);return false;}
    out.resize(size);bool ok=fread(out.data(),1,out.size(),f)==out.size();fclose(f);return ok;
}
bool validAsset(const std::vector<uint8_t>& b,const std::vector<uint8_t>& t,const uint8_t* rig) {
    if(b.size()<1000 || !rig || memcmp(b.data(),"WKMD0010",8) || word32(b.data(),8)!=b.size())return false;
    // Require the exact native boat rig and descriptor structure; only geometry,
    // bounds and draw range may differ. Never trust file-supplied GPU pointers.
    for(size_t i=0;i<1000;i++) {
        if((i>=8&&i<12)||(i>=24&&i<48)||(i>=76&&i<80)||(i>=88&&i<92)||
           (i>=96&&i<100)||(i>=116&&i<124)||(i>=124&&i<156)||(i>=192&&i<212))continue;
        if(b[i]!=rig[i])return false;
    }
    auto u=[&](size_t at){return word32(b.data(),at);};
    unsigned n=u(88);
    if(!n || n>65535 || n%3 || u(76)!=0 || u(92)!=1000 || u(96)!=1000+n*44 ||
       u(116)!=n || u(120)!=1000+n*48 || b.size()!=1000+n*50 ||
       u(192)!=D3DPT_TRIANGLELIST || u(196)!=0 || u(200)!=n || u(204)!=0 || u(208)!=n/3)return false;
    for(size_t at=124;at<156;at+=4)if(u(at))return false;
    for(unsigned i=0;i<n;i++) {
        float v[11];memcpy(v,b.data()+1000+i*44,sizeof(v));
        for(float x:v)if(!std::isfinite(x))return false;
        if(v[3]!=1 || v[4]!=0 || v[5]!=0 || v[9]<0 || v[9]>1 || v[10]<0 || v[10]>1)return false;
        for(int axis=0;axis<3;axis++)if(fabsf(v[axis])>64)return false;
        uint16_t index;memcpy(&index,b.data()+u(120)+i*2,2);if(index>=n)return false;
    }
    for(size_t i=24;i<48;i+=4){float f;memcpy(&f,b.data()+i,4);if(!std::isfinite(f)||fabsf(f)>64)return false;}
    return t.size()==24+256*16*4 && !memcmp(t.data(),"WFTX0010",8) && word32(t.data(),8)==t.size() &&
        word32(t.data(),12)==1 && word32(t.data(),16)==0x00100100 && word32(t.data(),20)==32;
}
void releaseResources() {
    if(!mesh.empty() && *(void**)(model+0x24))releaseNative(model,nullptr);
    if(texture){texture->Release();texture=nullptr;}
    memset(model,0,sizeof(model));owner=nullptr;
}
bool resources(IDirect3DDevice9* device) {
    if(owner && owner!=device){releaseResources();failed=false;}
    if(failed)return false;
    if(texture && owner==device)return true;
    failed=true; // one attempt per device/reset; avoid disk work on every frame
    auto rig=*(const uint8_t**)(boatModel+0x24);
    if(!readFile(assetDir+L"\\merchant_ship.wkm",mesh) || !readFile(assetDir+L"\\merchant_ship.wft",textureBytes) ||
       !validAsset(mesh,textureBytes,rig)) {
        if(visualLog)visualLog("trade ship visual: invalid/missing asset; keeping native boat");return false;
    }
    memcpy(model,boatModel,sizeof(model));*(void**)(model+0x24)=nullptr;
    if(loadNative(model,nullptr,mesh.data())!=1 || FAILED(uploadNative(model,device)) ||
       FAILED(device->CreateTexture(256,16,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr))) {
        releaseResources();if(visualLog)visualLog("trade ship visual: GPU allocation failed; keeping native boat");return false;
    }
    D3DLOCKED_RECT bits={};
    if(FAILED(texture->LockRect(0,&bits,nullptr,0))){releaseResources();return false;}
    for(int y=0;y<16;y++)memcpy((char*)bits.pBits+y*bits.Pitch,textureBytes.data()+24+y*1024,1024);
    if(FAILED(texture->UnlockRect(0))){releaseResources();return false;}
    owner=device;failed=false;
    if(visualLog)visualLog("trade ship visual: model ready, %u vertices",word32(mesh.data(),88));return true;
}
void rememberActor(void* actor,int id) {
    int i=slot(actor,ACTORS,0x50,4096);if(i>=0)actorUnits[i]=id>=0?id+1:0;
}
void* __fastcall enqueueHook(void* manager,uint8_t* visual,void* camera,void* descriptor) {
    void* actor=enqueueNative(manager,camera,descriptor);
    // 0x561990 -> 0x56dc30 confirms parent->unitObject->uint16(+8) is the unit ID.
    auto parent=*(uint8_t**)visual;
    auto unit=parent?*(uint8_t**)parent:nullptr;
    int id=unit?*(uint16_t*)(unit+8):-1;
    rememberActor(actor,id>=0 && identityFn && identityFn(id)?id:-1);return actor;
}
int __fastcall bindHook(void* instance,void* actor,void* definition) {
    int i=slot(instance,INSTANCES,0x770,128),a=slot(actor,ACTORS,0x50,4096);
    if(i>=0)instanceUnits[i]=a>=0?actorUnits[a]:0;
    return bindNative(instance,definition); // keep native animation rig and transform intact
}
__declspec(naked) void enqueueBridge() { __asm { mov edx,esi
    jmp enqueueHook } }
__declspec(naked) void bindBridge() { __asm { mov edx,ebx
    jmp bindHook } }
HRESULT __fastcall drawHook(void* instance,void*,IDirect3DDevice9* device,void* textures,void* definition,
                            int textureOffset,void* batches,unsigned count) {
    int i=slot(instance,INSTANCES,0x770,128);
    int tag=i>=0?instanceUnits[i]:0;
    if(!tag || !identityFn || !identityFn(tag-1) || definition!=boatModel || !count || !resources(device))
        return drawNative(instance,device,textures,definition,textureOffset,batches,count);
    auto original=*(uint8_t**)(boatModel+0x24);
    // Merchant geometry has one opaque batch. Do not draw the old translucent oars.
    if(batches!=original+160)return S_OK;
    IDirect3DBaseTexture9* previous=nullptr;
    if(FAILED(device->GetTexture(0,&previous)))return drawNative(instance,device,textures,definition,textureOffset,batches,count);
    if(FAILED(device->SetTexture(0,texture))){if(previous)previous->Release();return drawNative(instance,device,textures,definition,textureOffset,batches,count);}
    // Null texture context is the native renderer's supported "already bound" path.
    // Restore the actual texture so the native texture-index cache remains correct.
    // The native expanded-backface outline intersects our double-sided sails and
    // fine rigging (0.1-unit outline, 0.018-unit ropes). Scope its suppression to
    // this synchronous draw on the render thread; ordinary boats retain it.
    unsigned outline=*outlineEnabled;*outlineEnabled=0;
    HRESULT hr=drawNative(instance,device,nullptr,model,0,mesh.data()+160,1);
    *outlineEnabled=outline;
    device->SetTexture(0,previous);if(previous)previous->Release();
    if(FAILED(hr)) {
        failed=true;
        if(visualLog)visualLog("trade ship visual: native draw failed %08lx; keeping native boat until reset",hr);
        return drawNative(instance,device,textures,definition,textureOffset,batches,count);
    }
    return hr;
}
bool isCall(uintptr_t at,uintptr_t target) {
    return *(uint8_t*)at==0xe8 && at+5+*(int32_t*)(at+1)==target;
}
}

bool tradeShipVisualInstall(const wchar_t* dir,TradeShipIdentityFn identity,TraceLogFn log) {
    if(installed)return true;
    visualLog=log;assetDir=std::wstring(dir?dir:L"")+L"\\trade_ships\\merchant";
    if(GetFileAttributesW((assetDir+L"\\merchant_ship.wkm").c_str())==INVALID_FILE_ATTRIBUTES) {
        if(log)log("trade ship visual: asset absent; native appearance retained");return false;
    }
    const uint8_t upload[]={0x83,0xec,0x18,0x53,0x8b,0x59,0x24};
    if(!identity || !isCall(0x5688e8,0x411f50) || !isCall(0x411d79,0x44a460) ||
       *(uintptr_t*)0x769c34!=0x44ae20 || memcmp((void*)0x44a660,upload,sizeof(upload))) {
        if(log)log("trade ship visual: incompatible hooks; native appearance retained");return false;
    }
    // The world-map pool uses the derived vtable 0x769c20, not base 0x794758.
    uintptr_t sites[]={0x5688e8,0x411d79,0x769c34};SIZE_T sizes[]={5,5,4};DWORD old[3]={};
    for(int i=0;i<3;i++)if(!VirtualProtect((void*)sites[i],sizes[i],PAGE_EXECUTE_READWRITE,&old[i])) {
        for(int j=i-1;j>=0;j--){DWORD unused;VirtualProtect((void*)sites[j],sizes[j],old[j],&unused);}return false;
    }
    identityFn=identity;
    *(int32_t*)0x5688e9=(int32_t)((uintptr_t)enqueueBridge-0x5688ed);
    *(int32_t*)0x411d7a=(int32_t)((uintptr_t)bindBridge-0x411d7e);
    *(void**)0x769c34=(void*)drawHook;
    for(int i=2;i>=0;i--){DWORD unused;VirtualProtect((void*)sites[i],sizes[i],old[i],&unused);FlushInstructionCache(GetCurrentProcess(),(void*)sites[i],sizes[i]);}
    installed=true;if(log)log("trade ship visual: identity-specific native draw hooks installed");return true;
}
void tradeShipVisualReset() { releaseResources();failed=false;memset(actorUnits,0,sizeof(actorUnits));memset(instanceUnits,0,sizeof(instanceUnits)); }
