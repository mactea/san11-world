#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "terrain_replacement.h"
#include <wrl/client.h>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <atomic>
using Microsoft::WRL::ComPtr;

namespace {
// WMTR0002, entry count, followed by each header and tightly packed mip bytes.
struct Header { UINT sw,sh,format,sl,dw,dh,dl; };
static_assert(sizeof(Header)==28,"package layout");
struct Level { UINT row,rows; size_t offset; };
struct Entry {
    Header header={};std::vector<BYTE> original,pixels;
    std::vector<Level> sourceLevels,targetLevels;ComPtr<IDirect3DTexture9> texture;
};
std::vector<Entry> entries;
std::atomic<bool> loaded{false};
std::mutex guard;
UINT64 matches=0,misses=0,failures=0;
bool layout(UINT w,UINT h,UINT format,UINT count,std::vector<Level>& levels,size_t& bytes) {
    if(!w || !h || w>1024 || h>1024 || !count || count>11)return false;
    UINT unit=0,block=1;
    if(format==D3DFMT_A8R8G8B8 || format==D3DFMT_X8R8G8B8)unit=4;
    else if(format==D3DFMT_DXT1){unit=8;block=4;}
    else if(format==D3DFMT_DXT3 || format==D3DFMT_DXT5){unit=16;block=4;}
    else return false;
    bytes=0;levels.clear();
    for(UINT i=0;i<count;++i){
        UINT row=((w+block-1)/block)*unit,rows=(h+block-1)/block;
        levels.push_back({row,rows,bytes});bytes+=size_t(row)*rows;
        if(w==1 && h==1 && i+1<count)return false;
        w=std::max(1u,w/2);h=std::max(1u,h/2);
    }
    return bytes<=8u*1024*1024;
}
}
bool terrainReplacementConfigure(const wchar_t* path) {
    std::lock_guard<std::mutex> lock(guard);
    loaded=false;entries.clear();matches=misses=failures=0;
    if(!path || !*path)return true;
    FILE* raw=nullptr;if(_wfopen_s(&raw,path,L"rb") || !raw){++failures;return false;}
    std::unique_ptr<FILE,decltype(&fclose)> file(raw,fclose);
    char magic[8];UINT count=0;
    if(fread(magic,8,1,raw)!=1 || memcmp(magic,"WMTR0002",8) || fread(&count,4,1,raw)!=1 || !count || count>4){++failures;return false;}
    try {
        std::vector<Entry> loaded(count);size_t total=0;
        for(auto& entry:loaded){
            auto& h=entry.header;size_t sourceBytes=0,targetBytes=0;
            bool valid=fread(&h,sizeof h,1,raw)==1 &&
                layout(h.sw,h.sh,h.format,h.sl,entry.sourceLevels,sourceBytes) && sourceBytes<=1024*1024 &&
                layout(h.dw,h.dh,D3DFMT_A8R8G8B8,h.dl,entry.targetLevels,targetBytes) &&
                h.dw>=h.sw && h.dh>=h.sh && UINT64(h.dw)*h.sh==UINT64(h.dh)*h.sw;
            total+=sourceBytes+targetBytes;
            if(!valid || total>32u*1024*1024){++failures;return false;}
            entry.original.resize(sourceBytes);entry.pixels.resize(targetBytes);
            if(fread(entry.original.data(),1,sourceBytes,raw)!=sourceBytes || fread(entry.pixels.data(),1,targetBytes,raw)!=targetBytes){++failures;return false;}
        }
        if(fgetc(raw)!=EOF || ferror(raw)){++failures;return false;}
        // Ambiguous identities could choose the wrong season: reject the package.
        for(size_t i=0;i<loaded.size();++i)for(size_t j=0;j<i;++j)
            if(!memcmp(&loaded[i].header,&loaded[j].header,4*sizeof(UINT)) && loaded[i].original==loaded[j].original){++failures;return false;}
        entries=std::move(loaded);::loaded=true;return true;
    }catch(const std::bad_alloc&){++failures;return false;}
}
void terrainReplacementReset(){std::lock_guard<std::mutex> lock(guard);for(auto& e:entries)e.texture.Reset();}
bool terrainReplacementFind(IDirect3DTexture9* source,IDirect3DTexture9** result) {
    if(!result)return false;*result=nullptr;
    if(!loaded.load())return false;
    std::lock_guard<std::mutex> lock(guard);
    if(entries.empty() || !source)return false;
    for(auto& entry:entries){
    auto& header=entry.header;auto& original=entry.original;auto& pixels=entry.pixels;
    auto& sourceLevels=entry.sourceLevels;auto& targetLevels=entry.targetLevels;auto& texture=entry.texture;
    D3DSURFACE_DESC desc={};
    if(FAILED(source->GetLevelDesc(0,&desc)) || desc.Pool!=D3DPOOL_MANAGED || desc.Usage || source->GetLOD() ||
       desc.Width!=header.sw || desc.Height!=header.sh || UINT(desc.Format)!=header.format || source->GetLevelCount()!=header.sl)continue;
    // Do not cache by COM address: edits, season changes and address reuse must
    // immediately stop matching. Package source identity includes every mip.
    bool matched=true;
    for(UINT i=0;i<header.sl;++i){
        D3DLOCKED_RECT data={};if(FAILED(source->LockRect(i,&data,nullptr,D3DLOCK_READONLY))){++failures;return false;}
        const auto& level=sourceLevels[i];bool equal=data.Pitch>=int(level.row);
        for(UINT y=0;equal && y<level.rows;++y)equal=memcmp((BYTE*)data.pBits+y*data.Pitch,original.data()+level.offset+size_t(y)*level.row,level.row)==0;
        HRESULT hr=source->UnlockRect(i);if(FAILED(hr)){++failures;return false;}if(!equal){matched=false;break;}
    }
    if(!matched)continue;
    ComPtr<IDirect3DDevice9> device;if(FAILED(source->GetDevice(&device))){++failures;return false;}
    if(texture){ComPtr<IDirect3DDevice9> owner;texture->GetDevice(&owner);if(owner.Get()!=device.Get())texture.Reset();}
    if(!texture){
        ComPtr<IDirect3DTexture9> created;
        if(FAILED(device->CreateTexture(header.dw,header.dh,header.dl,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&created,nullptr))){++failures;return false;}
        for(UINT i=0;i<header.dl;++i){
            D3DLOCKED_RECT data={};if(FAILED(created->LockRect(i,&data,nullptr,0))){++failures;return false;}
            const auto& level=targetLevels[i];bool valid=data.Pitch>=int(level.row);
            if(valid)for(UINT y=0;y<level.rows;++y)memcpy((BYTE*)data.pBits+y*data.Pitch,pixels.data()+level.offset+size_t(y)*level.row,level.row);
            HRESULT hr=created->UnlockRect(i);if(!valid || FAILED(hr)){++failures;return false;}
        }
        texture=created;
    }
    ++matches;*result=texture.Get();(*result)->AddRef();return true;
    }
    ++misses;return false;
}
void terrainReplacementStatus(FILE* out){std::lock_guard<std::mutex> lock(guard);size_t source=0,target=0;for(auto& e:entries){source+=e.original.size();target+=e.pixels.size();}fprintf(out,"Terrain replacement: entries=%zu source_bytes=%zu target_bytes=%zu matches=%llu misses=%llu failures=%llu\n",entries.size(),source,target,matches,misses,failures);}
