#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <gdiplus.h>
#include <string>
#include <cstring>
#include "trade_ship_ui.h"

namespace shipui {
TradeShipUiIdentity identity=nullptr;
TradeShipUiRead readDetails=nullptr;
TradeShipUiOpen openDetails=nullptr;
using TextFn=int(__cdecl*)(void*,const uint8_t*,int);
using LabelFn=void(__thiscall*)(void*,const char*);
TextFn textNative=nullptr;
LabelFn labelNative=(LabelFn)0x4eabb0;
using SetLabelFn=void(__thiscall*)(void*,int,const char*);
SetLabelFn setLabelNative=(SetLabelFn)0x4d94c0;
using AssignFn=void(__thiscall*)(void*,const char*);
AssignFn assignNative=(AssignFn)0x4719f0;
std::string typeName,shipName;
std::wstring cardFile;
bool installed=false;
SRWLOCK queueLock=SRWLOCK_INIT;
bool pending=false;
POINT position={};
bool modal=false;
uint8_t* pressedPanel=nullptr;
const uint8_t* pressedUnit=nullptr;
int pressedLeader=-1;
using MouseFn=int(__fastcall*)(uint8_t*,void*,void*,uint32_t,POINT*);
MouseFn downNative=(MouseFn)0x4edc40,upNative=(MouseFn)0x4dd860;
using ChildFn=uint8_t*(__thiscall*)(void*,int);
using PosFn=void(__thiscall*)(void*,POINT*);
ChildFn childNative=(ChildFn)0x4dc620;
PosFn posNative=(PosFn)0x475880;
using ShowFn=void(__thiscall*)(void*,int);
void showPanel(uint8_t* panel,int visible) {((ShowFn)(*(void***)panel)[0x4c/4])(panel,visible);}
using ShowPanelFn=void(*)(uint8_t*,int);
ShowPanelFn showNative=showPanel;
struct Layout { int16_t group,reserved,id,type,parent,image,flags,x,y,w,h,textFlags; };
static_assert(sizeof(Layout)==24,"native layout record");
using CreateFn=uint8_t*(__thiscall*)(void*,int,const Layout*,void*);
CreateFn createNative=(CreateFn)0x4db050;
using LayoutFn=const Layout*(*)(int);
const Layout* layoutAt(int id){return (const Layout*)(0x7f1ad0+(size_t)id*24);}
LayoutFn layoutNative=layoutAt;
size_t transportOffset=0x708;
const int SHIP=0x7700,INFO=0x7710;
const int shipTemplates[]={0x1b22,0x1b23,0x1b24,0x1b26,0x1b27};
uint8_t* cachedPanel=nullptr;
int cachedKind=-1;
TradeShipUiDetails cachedInfo;
IDirect3DDevice9* owner=nullptr;
IDirect3DTexture9* texture=nullptr;
bool failed=false;
const int W=48,H=48;

std::string encode(const wchar_t* s) {
    int n=WideCharToMultiByte(950,0,s,-1,nullptr,0,nullptr,nullptr);
    std::string out(n,'\0');if(n){WideCharToMultiByte(950,0,s,-1,&out[0],n,nullptr,nullptr);out.pop_back();}return out;
}
int kind(const uint8_t* unit) { return unit && identity ? identity(unit) : 0; }
const char* caravanName(const uint8_t* unit) {
    return kind(unit) ? typeName.c_str() : nullptr;
}
const char* replacement(const uint8_t* unit,int property) {
    if(property==1 || property==3)return caravanName(unit);
    return property==26 && kind(unit)==2 ? shipName.c_str() : nullptr;
}
int __cdecl textHook(void* out,const uint8_t* unit,int property) {
    const char* text=replacement(unit,property);
    if(!text)return textNative(out,unit,property);
    assignNative(out,text);return *(int*)((uint8_t*)out+8);
}
// ESI is the owner panel at these two call sites, not the label widget.
void __fastcall landLabel(void* label,uint8_t* panel,const char* text) {
    auto unit=*(const uint8_t**)(panel+0x1a4);
    const char* name=caravanName(unit);
    labelNative(label,name?name:text);
}
void __fastcall seaLabel(void* label,uint8_t* panel,const char* text) {
    auto unit=*(const uint8_t**)(panel+0x1a4);
    labelNative(label,kind(unit)==2?shipName.c_str():text);
}
void __fastcall titleLabel(void* panel,void*,int id,const char* text) {
    auto unit=*(const uint8_t**)((uint8_t*)panel+0x18c);
    const char* name=caravanName(unit);
    setLabelNative(panel,id,name?name:text);
}
__declspec(naked) void landBridge() { __asm { mov edx,esi
    jmp landLabel } }
__declspec(naked) void seaBridge() { __asm { mov edx,esi
    jmp seaLabel } }
// Clone the original stat cells and vessel tiles through the game's own layout
// factory. The parent owns every widget; existing inventory controls stay intact.
bool transportReady(uint8_t* panel) {
    uintptr_t buckets=*(uintptr_t*)(panel+transportOffset+0x7c);
    uint32_t count=*(uint32_t*)(panel+transportOffset+0x80);
    return buckets && buckets!=0xffffffffu && count && count<65536;
}
uint8_t* copyWidget(uint8_t* panel,int id,int source,int dx,int dy,int width=0) {
    if(auto existing=childNative(panel,id))return existing;
    auto original=childNative(panel+transportOffset,source);if(!original)return nullptr;
    POINT p={},origin={};posNative(original,&p);posNative(panel,&origin);
    Layout layout=*layoutNative(source);
    // Geometry helpers index the executable's static table by layout.id. -1
    // means absolute parent-relative geometry; the separate id owns registration.
    layout.id=-1;layout.parent=-1;
    layout.x=(int16_t)(p.x-origin.x+dx);layout.y=(int16_t)(p.y-origin.y+dy);
    if(width)layout.w=(int16_t)width;
    return createNative(panel,id,&layout,nullptr);
}
void visibility(uint8_t* panel,int type) {
    for(int i=0;i<5;i++)if(auto w=childNative(panel,SHIP+i))showNative(w,type==2);
    for(int i=0;i<12;i++)if(auto w=childNative(panel,INFO+i))showNative(w,type!=0);
}
bool ensureCards(uint8_t* panel) {
    // Native creation pumps drawing before the embedded inventory is initialized.
    // Its uninitialized hash table uses -1; looking up a control then is unsafe.
    if(!transportReady(panel))return false;
    bool fresh=!childNative(panel,INFO+11);
    for(int i=0;i<5;i++)if(!copyWidget(panel,SHIP+i,shipTemplates[i],66,0))return false;
    for(int row=0;row<3;row++)for(int col=0;col<4;col++)
        if(!copyWidget(panel,INFO+row*4+col,0x1ad2+col,0,162+row*26,col==2?207:col==3?197:0))return false;
    if(fresh) {
        cachedPanel=nullptr;
        const wchar_t* titles[]={L"商路",L"載量",L"載貨"};
        for(int i=0;i<3;i++)setLabelNative(panel,INFO+i*4+1,encode(titles[i]).c_str());
        setLabelNative(panel,SHIP+1,encode(L"海船").c_str());
        setLabelNative(panel,SHIP+4,encode(L"大型").c_str());
    }
    return true;
}
std::wstring fit(const std::wstring& text) {
    int units=0;size_t n=0;
    for(wchar_t ch:text)units+=ch<128?1:2;
    if(units<=24)return text;
    units=0;while(n<text.size() && units+(text[n]<128?1:2)<=21){units+=text[n]<128?1:2;++n;}
    return text.substr(0,n)+L"...";
}
void updateCards(uint8_t* panel,int type,const TradeShipUiDetails& info) {
    if(type && !ensureCards(panel)){visibility(panel,0);return;}
    if(cachedPanel!=panel || cachedKind!=type)visibility(panel,type);
    if(type && (cachedPanel!=panel || cachedKind!=type || cachedInfo.route!=info.route ||
        cachedInfo.load!=info.load || cachedInfo.cargo!=info.cargo)) {
        std::wstring cargo=info.cargo.compare(0,3,L"載貨：")==0?info.cargo.substr(3):info.cargo;
        const std::wstring values[]={info.route,info.load,cargo};
        for(int i=0;i<3;i++)setLabelNative(panel,INFO+i*4+3,encode(fit(values[i]).c_str()).c_str());
    }
    cachedPanel=panel;cachedKind=type;cachedInfo=info;
}
bool widgetRect(uint8_t* panel,int id,RECT& r) {
    auto w=childNative(panel,id);if(!w)return false;
    POINT p={};posNative(w,&p);r={p.x,p.y,p.x+*(int*)(w+0x14),p.y+*(int*)(w+0x18)};return true;
}
bool cardPosition(uint8_t* panel,POINT& p) {
    if(!transportReady(panel))return false;
    auto icon=childNative(panel+transportOffset,0x1b25);if(!icon)return false;
    posNative(icon,&p);p.x+=66;
    return true;
}
bool hit(uint8_t* panel,POINT* pt) {
    if(modal || !openDetails || !pt || !kind(*(const uint8_t**)(panel+0x18c)))return false;
    POINT origin={};RECT r={},bottom={};
    posNative(panel,&origin); // Native event points are relative to this dialog.
    POINT absolute={pt->x+origin.x,pt->y+origin.y};
    if(widgetRect(panel,INFO,r) && widgetRect(panel,INFO+10,bottom)) {
        r.right=bottom.right;r.bottom=bottom.bottom;if(PtInRect(&r,absolute))return true;
    }
    if(kind(*(const uint8_t**)(panel+0x18c))==2 && widgetRect(panel,SHIP,r)) {
        r.bottom+=84;return PtInRect(&r,absolute)!=0;
    }
    return false;
}
int __fastcall panelDown(uint8_t* panel,void* edx,void* target,uint32_t wp,POINT* pt) {
    pressedPanel=nullptr;pressedUnit=nullptr;
    if(!hit(panel,pt))return downNative(panel,edx,target,wp,pt);
    pressedPanel=panel;pressedUnit=*(const uint8_t**)(panel+0x18c);
    pressedLeader=*(const int*)(pressedUnit+0xc);return 1;
}
int __fastcall panelUp(uint8_t* panel,void* edx,void* target,uint32_t wp,POINT* pt) {
    bool own=pressedPanel==panel;
    const uint8_t* unit=pressedUnit;int leader=pressedLeader;pressedPanel=nullptr;pressedUnit=nullptr;
    if(!own)return upNative(panel,edx,target,wp,pt);
    if(unit==*(const uint8_t**)(panel+0x18c) && *(const int*)(unit+0xc)==leader && hit(panel,pt)) {
        modal=true;AcquireSRWLockExclusive(&queueLock);pending=false;ReleaseSRWLockExclusive(&queueLock);
        // Native modal dialogs replace the active input window. Re-showing the
        // parent restores its focus/registration, just as the city trade menu does.
        showNative(panel,0);
        try {openDetails(unit);} catch(...) {showNative(panel,1);modal=false;throw;}
        showNative(panel,1);
        modal=false;
    }
    return 1;
}
void __fastcall panelDraw(uint8_t* panel,void*,void*) {
    AcquireSRWLockExclusive(&queueLock);pending=false;ReleaseSRWLockExclusive(&queueLock);
    auto unit=*(const uint8_t**)(panel+0x18c);int type=kind(unit);
    if(modal)return;
    TradeShipUiDetails info;
    if(type && (!readDetails || !readDetails(unit,info)))info={L"",L"資料更新中",L"資料更新中",L"—"};
    updateCards(panel,type,info);
    POINT p={};if(type!=2 || !cardPosition(panel,p))return;
    AcquireSRWLockExclusive(&queueLock);position=p;pending=true;ReleaseSRWLockExclusive(&queueLock);
}
bool isCall(uintptr_t at,uintptr_t target) {
    return *(uint8_t*)at==0xe8 && at+5+*(int32_t*)(at+1)==target;
}
bool load(IDirect3DDevice9* d) {
    if(owner!=d){tradeShipUiReset();owner=d;}
    if(texture)return true;
    if(failed)return false;failed=true;
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return false;
    bool ok=false;
    {
        Gdiplus::Bitmap bitmap(W,H,PixelFormat32bppARGB);
        {
            Gdiplus::Graphics g(&bitmap);g.Clear(Gdiplus::Color(0,0,0,0));
            Gdiplus::Bitmap ship(cardFile.c_str());
            if(ship.GetLastStatus()==Gdiplus::Ok) {
                g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                g.DrawImage(&ship,Gdiplus::Rect(0,6,48,36),0,0,62,46,Gdiplus::UnitPixel);
            }
        }
        if(SUCCEEDED(d->CreateTexture(W,H,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr))) {
            Gdiplus::Rect rect(0,0,W,H);Gdiplus::BitmapData pixels={};
            if(bitmap.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&pixels)==Gdiplus::Ok) {
                D3DLOCKED_RECT bits={};
                if(SUCCEEDED(texture->LockRect(0,&bits,nullptr,0))) {
                    for(int y=0;y<H;y++)memcpy((uint8_t*)bits.pBits+y*bits.Pitch,(uint8_t*)pixels.Scan0+y*pixels.Stride,W*4);
                    ok=SUCCEEDED(texture->UnlockRect(0));
                }
                bitmap.UnlockBits(&pixels);
            }
        }
    }
    Gdiplus::GdiplusShutdown(token);
    if(!ok && texture){texture->Release();texture=nullptr;}
    return ok;
}
}

bool tradeShipUiInstall(const wchar_t* dir,TradeShipUiIdentity identity,TradeShipUiRead read,TradeShipUiOpen open,TraceLogFn log) {
    using namespace shipui;
    if(installed)return true;
    const uint8_t prologue[]={0x83,0xec,0x44,0xa1,0xac,0x0b,0x8e,0};
    const uintptr_t sites[]={0x4cd090,0x642a4c,0x642ae5,0x62e7ac,0x8571b4,0x857228,0x85722c};
    const SIZE_T sizes[]={8,5,5,5,4,4,4};DWORD old[7]={};
    if(!identity || !read || !open || memcmp((void*)sites[0],prologue,8) ||
       !isCall(sites[1],0x4eabb0) || !isCall(sites[2],0x4eabb0) ||
       !isCall(sites[3],0x4d94c0) || *(uintptr_t*)sites[4]!=0x423230 ||
       *(uintptr_t*)sites[5]!=0x4edc40 || *(uintptr_t*)sites[6]!=0x4dd860 ||
       *(uint8_t*)0x4db050!=0x8b || layoutAt(0x1b22)->image!=393 || layoutAt(0x1b23)->type!=6 ||
       *(uint16_t*)0x62ec31!=0x8e8d || !isCall(0x62ec37,0x62e280)) {
        if(log)log("trade ship UI: incompatible code; native UI retained");return false;
    }
    auto trampoline=(uint8_t*)VirtualAlloc(nullptr,13,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    if(!trampoline)return false;
    memcpy(trampoline,prologue,8);trampoline[8]=0xe9;
    *(int32_t*)(trampoline+9)=(int32_t)(sites[0]+8-(uintptr_t)(trampoline+13));
    for(int i=0;i<7;i++)if(!VirtualProtect((void*)sites[i],sizes[i],PAGE_EXECUTE_READWRITE,&old[i])) {
        while(i--) {DWORD unused;VirtualProtect((void*)sites[i],sizes[i],old[i],&unused);}
        VirtualFree(trampoline,0,MEM_RELEASE);return false;
    }
    shipui::identity=identity;textNative=(TextFn)trampoline;
    // The expanded officer/UI pools relocate this embedded panel at load time.
    // Read the same LEA displacement used by the native properties population.
    transportOffset=*(uint32_t*)0x62ec33;
    readDetails=read;openDetails=open;
    typeName=encode(L"商隊");shipName=encode(L"大型海船");
    cardFile=std::wstring(dir)+L"\\trade_ships\\merchant\\properties.png";
    const void* hooks[]={textHook,landBridge,seaBridge,titleLabel,panelDraw};
    for(int i=0;i<4;i++) {
        *(uint8_t*)sites[i]=i?0xe8:0xe9;
        *(int32_t*)(sites[i]+1)=(int32_t)((uintptr_t)hooks[i]-sites[i]-5);
    }
    memset((void*)(sites[0]+5),0x90,3);*(const void**)sites[4]=hooks[4];
    *(const void**)sites[5]=panelDown;*(const void**)sites[6]=panelUp;
    for(int i=0;i<7;i++){DWORD unused;VirtualProtect((void*)sites[i],sizes[i],old[i],&unused);FlushInstructionCache(GetCurrentProcess(),(void*)sites[i],sizes[i]);}
    FlushInstructionCache(GetCurrentProcess(),trampoline,13);
    installed=true;if(log)log("trade ship UI: native information cells and vessel tile installed (inventory offset %x)",(unsigned)transportOffset);return true;
}
void tradeShipUiReset() {
    using namespace shipui;
    if(texture){texture->Release();texture=nullptr;}owner=nullptr;failed=false;
    AcquireSRWLockExclusive(&queueLock);pending=false;ReleaseSRWLockExclusive(&queueLock);
}
void tradeShipUiDraw(IDirect3DDevice9* d) {
    using namespace shipui;
    AcquireSRWLockExclusive(&queueLock);bool show=pending;POINT p=position;pending=false;ReleaseSRWLockExclusive(&queueLock);
    if(!show || !load(d))return;
    IDirect3DStateBlock9* state=nullptr;if(FAILED(d->CreateStateBlock(D3DSBT_ALL,&state)))return;
    d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
    d->SetRenderState(D3DRS_ZENABLE,FALSE);d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);d->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
    d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    d->SetRenderState(D3DRS_STENCILENABLE,FALSE);d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);d->SetRenderState(D3DRS_FOGENABLE,FALSE);
    d->SetRenderState(D3DRS_COLORWRITEENABLE,15);d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
    d->SetTexture(0,texture);d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
    d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
    d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
    d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
    struct Vertex {float x,y,z,rhw,u,v;};float x=p.x-.5f,y=p.y-.5f;
    Vertex vertices[]={{x,y,0,1,0,0},{x+W,y,0,1,1,0},{x,y+H,0,1,0,1},{x+W,y+H,0,1,1,1}};
    d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));state->Apply();state->Release();
}
