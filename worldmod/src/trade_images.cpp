#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <d3d9.h>
#include <gdiplus.h>
#include <algorithm>
#include <vector>
#include "trade_images.h"

namespace {
const int W=320, H=80;
struct Picture {
    const wchar_t *name, *file;
    std::vector<uint32_t> pixels;
    IDirect3DTexture9* texture=nullptr;
};
Picture pictures[]={
#include "trade_goods_images.inc"
    {L"商品",L""}
};
const int PICTURE_COUNT=sizeof(pictures)/sizeof(pictures[0])-1;
SRWLOCK lock=SRWLOCK_INIT;
int pending=-1, number=-1, px=0, py=0;
bool ready=false;
std::wstring imageDir;
TraceLogFn imageLog=nullptr;
IDirect3DDevice9* owner=nullptr;
int index(const std::wstring& name) {
    for(int i=0;i<PICTURE_COUNT;++i) if(name==pictures[i].name) return i;
    return PICTURE_COUNT;
}
}

void tradeImagesInit(const wchar_t* dir, TraceLogFn log) {
    // Installation runs under DllMain's loader lock. Decode lazily on the
    // first visible trade window: GDI+ startup/shutdown must run outside it.
    imageDir=dir;imageLog=log;
}
static void loadPictures() {
    Gdiplus::GdiplusStartupInput input; ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok) return;
    int loaded=0;
    for(auto& p:pictures) {
        Gdiplus::Bitmap card(W,H,PixelFormat32bppARGB);
        {
            Gdiplus::Graphics g(&card);
            g.Clear(Gdiplus::Color(255,29,46,53));
            Gdiplus::Pen edge(Gdiplus::Color(255,132,149,148));
            g.DrawRectangle(&edge,0,0,W-1,H-1);
            bool found=false;
            if(*p.file) {
                std::wstring path=imageDir+L"\\trade_goods\\"+p.file;
                Gdiplus::Bitmap icon(path.c_str());
                if(icon.GetLastStatus()==Gdiplus::Ok && icon.GetWidth() && icon.GetHeight()) {
                    float scale=64.0f/std::max(icon.GetWidth(),icon.GetHeight());
                    int w=(int)(icon.GetWidth()*scale),h=(int)(icon.GetHeight()*scale);
                    g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
                    g.DrawImage(&icon,8+(64-w)/2,8+(64-h)/2,w,h);
                    found=true; ++loaded;
                }
            }
            Gdiplus::Font title(L"Microsoft JhengHei",22,Gdiplus::FontStyleBold,Gdiplus::UnitPixel);
            Gdiplus::Font caption(L"Microsoft JhengHei",16,Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);
            Gdiplus::SolidBrush gold(Gdiplus::Color(255,235,216,159)),dim(Gdiplus::Color(255,184,199,196));
            g.DrawString(p.name,-1,&title,Gdiplus::PointF(86,12),&gold);
            g.DrawString(found ? L"交易商品" : *p.file ? L"暫無圖片" : L"指向商品查看圖片",-1,&caption,Gdiplus::PointF(86,44),&dim);
        }
        Gdiplus::Rect rect(0,0,W,H); Gdiplus::BitmapData bits;
        if(card.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&bits)==Gdiplus::Ok) {
            p.pixels.resize(W*H);
            for(int y=0;y<H;++y) memcpy(p.pixels.data()+y*W,(uint8_t*)bits.Scan0+y*bits.Stride,W*4);
            card.UnlockBits(&bits);
        }
    }
    Gdiplus::GdiplusShutdown(token);
    ready=true;
    if(imageLog) imageLog("trade: commodity pictures loaded %d/%d",loaded,PICTURE_COUNT);
}
void tradeImageQueue(const std::wstring& name,int x,int y) {
    AcquireSRWLockExclusive(&lock); pending=index(name); px=x; py=y; ReleaseSRWLockExclusive(&lock);
}
void tradeImageNumber(const std::wstring& name) {
    AcquireSRWLockExclusive(&lock); number=name.empty() ? -1 : index(name); pending=-1; ReleaseSRWLockExclusive(&lock);
}
void tradeImagesReset() {
    for(auto& p:pictures) if(p.texture) { p.texture->Release(); p.texture=nullptr; }
    owner=nullptr;
}
void tradeImagesDraw(IDirect3DDevice9* d) {
    int id,x,y,n;
    AcquireSRWLockExclusive(&lock); id=pending;x=px;y=py;n=number;pending=-1;ReleaseSRWLockExclusive(&lock);
    if(id<0 && n<0) return;
    if(!ready && !imageDir.empty()) loadPictures();
    if(!ready) return;
    D3DVIEWPORT9 vp; if(FAILED(d->GetViewport(&vp))) return;
    if(n>=0) {
        id=n;x=(int)vp.Width/2-140-W-12;y=(int)vp.Height/2-150;
        if(x<8) { x=((int)vp.Width-W)/2;y=std::max(8,(int)vp.Height/2-190-H-8); }
    }
    if(owner!=d) { tradeImagesReset();owner=d; }
    Picture& p=pictures[id];
    if(p.pixels.empty()) return;
    if(!p.texture) {
        if(FAILED(d->CreateTexture(W,H,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&p.texture,nullptr))) return;
        D3DLOCKED_RECT r;
        if(FAILED(p.texture->LockRect(0,&r,nullptr,0))) { p.texture->Release();p.texture=nullptr;return; }
        for(int row=0;row<H;++row) memcpy((uint8_t*)r.pBits+row*r.Pitch,p.pixels.data()+row*W,W*4);
        p.texture->UnlockRect(0);
    }
    IDirect3DStateBlock9* state=nullptr;
    if(FAILED(d->CreateStateBlock(D3DSBT_ALL,&state))) return;
    d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);
    d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
    d->SetRenderState(D3DRS_ZENABLE,FALSE);d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    d->SetRenderState(D3DRS_STENCILENABLE,FALSE);d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);d->SetRenderState(D3DRS_FOGENABLE,FALSE);
    d->SetRenderState(D3DRS_COLORWRITEENABLE,15);d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
    d->SetTexture(0,p.texture);
    d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
    d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
    d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
    d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
    d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
    float l=(float)x-0.5f,t=(float)y-0.5f;
    struct Vertex { float x,y,z,rhw,u,v; };
    Vertex v[]={{l,t,0,1,0,0},{l+W,t,0,1,1,0},{l,t+H,0,1,0,1},{l+W,t+H,0,1,1,1}};
    d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));
    state->Apply();state->Release();
}
