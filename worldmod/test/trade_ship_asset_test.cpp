// Offline test: copy the owner's native model code WITHOUT running the game entry point.
// Exercise only the identified model bind/upload/release functions on a hidden D3D9 device.
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <string>

static void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
static std::vector<unsigned char> read(const char* name) {
    std::ifstream f(name,std::ios::binary);require(!!f,"open input");
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(f),{});
}
static unsigned u32(const std::vector<unsigned char>& b,size_t at) {
    require(at+4<=b.size(),"field bounds");unsigned v;memcpy(&v,b.data()+at,4);return v;
}
static void render(IDirect3DDevice9* dev,IDirect3DVertexBuffer9* vb,IDirect3DIndexBuffer9* ib,
                   unsigned count,const char* modelPath) {
    std::string texturePath=modelPath;texturePath.replace(texturePath.size()-3,3,"wft");auto tex=read(texturePath.c_str());
    require(tex.size()==24+256*16*4 && !memcmp(tex.data(),"WFTX0010",8),"palette texture");
    IDirect3DTexture9* texture=nullptr;
    require(SUCCEEDED(dev->CreateTexture(256,16,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr)),"GPU texture");
    D3DLOCKED_RECT lock={};require(SUCCEEDED(texture->LockRect(0,&lock,nullptr,0)),"texture lock");
    for(int y=0;y<16;y++)memcpy((char*)lock.pBits+y*lock.Pitch,tex.data()+24+y*1024,1024);texture->UnlockRect(0);
    D3DMATRIX identity={};identity._11=identity._22=identity._33=identity._44=1;
    for(int i=0;i<4;i++)dev->SetTransform(D3DTS_WORLDMATRIX(i),&identity);
    const float yaw=-.9f,elev=.42f,sy=sinf(yaw),cy=cosf(yaw),se=sinf(elev),ce=cosf(elev);
    // Frame the asset bounds so enlarged masts remain inside the readback image.
    auto mesh=read(modelPath);require(mesh.size()>=48,"render bounds");
    float bounds[6],center[3],diameter2=0;memcpy(bounds,mesh.data()+24,sizeof(bounds));
    for(int i=0;i<3;i++){center[i]=(bounds[i]+bounds[i+3])*.5f;float span=bounds[i+3]-bounds[i];diameter2+=span*span;}
    float height=sqrtf(diameter2)*1.1f;
    D3DMATRIX view={};view._11=-cy;view._31=sy;view._12=-sy*se;view._22=ce;view._32=-cy*se;
    view._13=-sy*ce;view._23=-se;view._33=-cy*ce;view._44=1;
    view._41=-(center[0]*view._11+center[2]*view._31);
    view._42=-(center[0]*view._12+center[1]*view._22+center[2]*view._32);
    view._43=30-(center[0]*view._13+center[1]*view._23+center[2]*view._33);
    D3DMATRIX projection={};projection._11=2/(height*4/3);projection._22=2/height;projection._33=1/100.f;projection._44=1;
    dev->SetTransform(D3DTS_VIEW,&view);dev->SetTransform(D3DTS_PROJECTION,&projection);
    dev->SetRenderState(D3DRS_VERTEXBLEND,D3DVBF_3WEIGHTS);dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_CCW);
    dev->SetRenderState(D3DRS_ZENABLE,TRUE);dev->SetRenderState(D3DRS_LIGHTING,TRUE);dev->SetRenderState(D3DRS_NORMALIZENORMALS,TRUE);
    dev->SetRenderState(D3DRS_AMBIENT,0x007a7a7a);dev->SetRenderState(D3DRS_COLORVERTEX,FALSE);
    D3DMATERIAL9 material={};material.Diffuse=material.Ambient={1,1,1,1};dev->SetMaterial(&material);
    D3DLIGHT9 light={};light.Type=D3DLIGHT_DIRECTIONAL;light.Diffuse={.52f,.52f,.52f,1};light.Direction={.447f,-.716f,-.537f};dev->SetLight(0,&light);dev->LightEnable(0,TRUE);
    dev->SetTexture(0,texture);dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
    dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);dev->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);dev->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
    require(SUCCEEDED(dev->SetFVF(0x11a)) && SUCCEEDED(dev->SetStreamSource(0,vb,0,44)) && SUCCEEDED(dev->SetIndices(ib)),"native vertex layout");
    dev->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff19252b,1,0);
    require(SUCCEEDED(dev->BeginScene()),"begin draw");require(SUCCEEDED(dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,count,0,count/3)),"draw native triangle batch");dev->EndScene();
    IDirect3DSurface9 *target=nullptr,*copy=nullptr;dev->GetRenderTarget(0,&target);
    D3DSURFACE_DESC desc={};target->GetDesc(&desc);
    require(SUCCEEDED(dev->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr)),"readback surface");
    require(SUCCEEDED(dev->GetRenderTargetData(target,copy)),"read back actual GPU render");
    require(SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"readback lock");
    BITMAPFILEHEADER file={};BITMAPINFOHEADER info={};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+desc.Width*desc.Height*4;
    info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-(LONG)desc.Height;info.biPlanes=1;info.biBitCount=32;info.biSizeImage=desc.Width*desc.Height*4;
    std::string output=modelPath;output=output.substr(0,output.find_last_of("/\\")+1)+"native-d3d9.bmp";
    std::ofstream f(output,std::ios::binary);require(!!f,"render output");f.write((char*)&file,sizeof(file));f.write((char*)&info,sizeof(info));
    unsigned visible=0;
    for(unsigned y=0;y<desc.Height;y++){auto row=(DWORD*)((char*)lock.pBits+y*lock.Pitch);f.write((char*)row,desc.Width*4);for(unsigned x=0;x<desc.Width;x++)visible+=(row[x]&0xffffff)!=0x19252b;}
    require(visible>10000,"render contains visible geometry");
    copy->UnlockRect();copy->Release();target->Release();dev->SetTexture(0,nullptr);texture->Release();
    printf("PASS native-format D3D9 draw/readback: visible pixels=%u\n",visible);
}
#include "trade_ship_visual_cases.inc"

int main(int argc,char**argv) { try {
    require(argc==3,"usage: test game-exe merchant_ship.wkm");
    auto exe=read(argv[1]),blob=read(argv[2]);
    require(exe.size()>sizeof(IMAGE_DOS_HEADER),"PE header");
    auto dos=(IMAGE_DOS_HEADER*)exe.data();
    require(dos->e_magic==IMAGE_DOS_SIGNATURE && size_t(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS32)<exe.size(),"PE signature bounds");
    auto nt=(IMAGE_NT_HEADERS32*)(exe.data()+dos->e_lfanew);
    require(nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 && nt->OptionalHeader.ImageBase==0x400000,"supported x86 image");
    // Version guard: these functions must match the inspected executable.
    const unsigned char bindSig[]={0x8b,0x44,0x24,0x04,0x85,0xc0,0x53,0x56,0x57,0x8b,0xd9};
    // Only this relocatable code range is needed: the exercised success paths
    // use relative calls and COM dispatch, without game globals/imports.
    auto base=(unsigned char*)VirtualAlloc(nullptr,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    require(base!=nullptr,"allocate native code test segment");
    auto sec=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++) {
        require(size_t(sec[i].PointerToRawData)+sec[i].SizeOfRawData<=exe.size() && size_t(sec[i].VirtualAddress)+sec[i].SizeOfRawData<=nt->OptionalHeader.SizeOfImage,"PE section bounds");
        if(sec[i].VirtualAddress<=0x40000 && sec[i].VirtualAddress+sec[i].SizeOfRawData>=0x50000)
            memcpy(base,exe.data()+sec[i].PointerToRawData+0x40000-sec[i].VirtualAddress,0x10000);
    }
    require(!memcmp(base+0xa5d0,bindSig,sizeof(bindSig)),"native bind version signature");
    const unsigned char uploadSig[]={0x83,0xec,0x18,0x53,0x8b,0x59,0x24};
    require(!memcmp(base+0xa660,uploadSig,sizeof(uploadSig)),"native upload version signature");
    require(blob.size()>=1000 && !memcmp(blob.data(),"WKMD0010",8) && u32(blob,8)==blob.size(),"WKMD length/signature");
    require(u32(blob,68)==1 && u32(blob,80)==0x11a && u32(blob,84)==44,"supported vertex descriptor");
    unsigned count=u32(blob,88),offset=u32(blob,92),indices=u32(blob,116),ioffset=u32(blob,120);
    require(count<=65535 && size_t(offset)+count*44<=blob.size() && size_t(ioffset)+indices*2<=blob.size(),"geometry bounds");
    auto api=Direct3DCreate9(D3D_SDK_VERSION);require(api!=nullptr,"D3D9 available");
    UINT adapter=D3DADAPTER_DEFAULT;
    for(UINT i=0;i<api->GetAdapterCount();i++) { D3DADAPTER_IDENTIFIER9 id={};api->GetAdapterIdentifier(i,0,&id);printf("adapter %u: %s\n",i,id.Description);if(strstr(id.Description,"RTX"))adapter=i; }
    auto window=CreateWindowExA(0,"STATIC","Offline ship asset validation",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    require(window!=nullptr,"hidden test window");
    D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=1024;pp.BackBufferHeight=768;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;
    IDirect3DDevice9* dev=nullptr;
    require(SUCCEEDED(api->CreateDevice(adapter,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev)),"create D3D9 HAL device");
    unsigned char assetModel[0x2c]={};
    using AssetBind=int(__thiscall*)(void*,void*,void*);
    using Upload=HRESULT(__thiscall*)(void*,IDirect3DDevice9*);
    using Release=HRESULT(__thiscall*)(void*,void*);
    require(((AssetBind)(base+0xa5d0))(assetModel,nullptr,blob.data())==1,"native assetModel bind");
    require(SUCCEEDED(((Upload)(base+0xa660))(assetModel,dev)),"native GPU upload");
    auto vb=*(IDirect3DVertexBuffer9**)(blob.data()+124);
    auto ib=*(IDirect3DIndexBuffer9**)(blob.data()+144);
    require(vb&&ib,"native GPU buffers created");
    D3DVERTEXBUFFER_DESC vd={};D3DINDEXBUFFER_DESC ix={};vb->GetDesc(&vd);ib->GetDesc(&ix);
    require(vd.Size==count*44 && ix.Size==indices*2,"native GPU buffer sizes");
    render(dev,vb,ib,count,argv[2]);
    // WRITEONLY native buffers cannot portably be read back; sizes and API results are validated.
    require(SUCCEEDED(((Release)(base+0xa8b0))(assetModel,nullptr)),"native GPU release");
    require(!u32(blob,124)&&!u32(blob,128)&&!u32(blob,144),"native pointers cleared");
    printf("PASS native bind/upload/release: vertices=%u indices=%u adapter=%u\n",count,indices,adapter);
    visualCases(dev,base,argv[1],argv[2]);
    dev->Release();api->Release();DestroyWindow(window);VirtualFree(base,0,MEM_RELEASE);return 0;
} catch(const std::exception&e) {fprintf(stderr,"FAIL %s\n",e.what());return 1;} }
