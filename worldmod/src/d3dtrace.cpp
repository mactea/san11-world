// d3dtrace: optional Direct3D 9 call tracer used to find which game code builds and draws the terrain.
// Hooks IDirect3D9::CreateDevice and a few IDirect3DDevice9 methods by patching their vtables,
// aggregates calls by (method, game call sites) and periodically writes worldmod_trace_<section>.log.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <intrin.h>
#include <stdio.h>
#include <stdint.h>
#include <map>
#include <tuple>
#include <vector>
#include <algorithm>
#include <wrl/client.h>
#include "d3dtrace.h"
#include "automation.h"
#include "render_backend.h"
#include "render_copy_msaa.h"
#include "terrain_dx12.h"
#include "terrain_buffer_hooks.h"
#include "object_capture.h"
#include "terrain_texture_hooks.h"
#include "terrain_filter.h"

namespace {

const uint32_t CODE_LO = 0x401000, CODE_HI = 0x74f000;   // san11pk.exe .text

struct Key {
    int method; uint32_t c[3];
    bool operator<(const Key& o) const { return std::tie(method, c[0], c[1], c[2]) < std::tie(o.method, o.c[0], o.c[1], o.c[2]); }
};
struct Stat { uint64_t calls = 0, prims = 0; uint32_t a = 0, b = 0, c = 0, d = 0; };

CRITICAL_SECTION g_cs;
std::map<Key, Stat> g_stats;
wchar_t g_path[MAX_PATH];
uint64_t g_frames = 0;
TraceLogFn g_tlog = nullptr;
bool g_recording = false;      // false: only backend control, native terrain and screenshots
uint32_t g_curTexWh = 0;       // 当前 stage 0 纹理 (w<<16|h)，由 hSetTexture 维护

const char* kNames[] = { "CreateTexture", "CreateVertexBuffer", "CreateIndexBuffer", "SetTransform",
                         "DrawPrimitive", "DrawIndexedPrimitive", "DrawPrimitiveUP", "DrawIndexedPrimitiveUP", "SetTexture" };
enum { M_TEX, M_VB, M_IB, M_XFORM, M_DP, M_DIP, M_DPUP, M_DIPUP, M_SETTEX };

bool looksLikeReturn(uint32_t v) {
    if (v < CODE_LO + 8 || v >= CODE_HI) return false;
    const uint8_t* p = (const uint8_t*)(uintptr_t)v;
    return p[-5] == 0xE8 || (p[-6] == 0xFF && (p[-5] & 0x38) == 0x10) ||
           (p[-2] == 0xFF && (p[-1] & 0x38) == 0x10) || (p[-3] == 0xFF && (p[-2] & 0x38) == 0x10);
}

// Collects up to three plausible game return addresses above `frame`.
void callers(void* frame, uint32_t out[3]) {
    out[0] = out[1] = out[2] = 0;
    uint32_t* sp = (uint32_t*)frame;
    for (int i = 0, n = 0; i < 96 && n < 3; i++) {
        uint32_t v = sp[i];
        if (looksLikeReturn(v)) out[n++] = v;
    }
}

void record(int method, void* frame, uint32_t prims, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0) {
    if (!g_recording) return;
    Key k{ method, {} };
    callers(frame, k.c);
    if (method == M_SETTEX) k.c[2] = d;            // one entry per call site and texture
    EnterCriticalSection(&g_cs);
    Stat& s = g_stats[k];
    s.calls++; s.prims += prims; s.a = a; s.b = b; s.c = c; s.d = d;
    LeaveCriticalSection(&g_cs);
}

void dump() {
    EnterCriticalSection(&g_cs);
    std::vector<std::pair<Key, Stat>> v(g_stats.begin(), g_stats.end());
    LeaveCriticalSection(&g_cs);
    std::sort(v.begin(), v.end(), [](auto& x, auto& y) {
        return x.first.method != y.first.method ? x.first.method < y.first.method : x.second.calls > y.second.calls; });
    FILE* f = _wfsopen(g_path, L"w", 0x40 /*_SH_DENYNO*/);
    if (!f) { if (g_tlog) g_tlog("trace: cannot write trace file"); return; }
    fprintf(f, "frames=%llu  (a,b,c,d = last call parameters, see d3dtrace.cpp)\n", g_frames);
    for (auto& [k, s] : v)
        fprintf(f, "%-24s calls=%-9llu prims=%-10llu callers=%08x %08x %08x  a=%u b=%u c=%u d=%u\n",
                kNames[k.method], s.calls, s.prims, k.c[0], k.c[1], k.c[2], s.a, s.b, s.c, s.d);
    fclose(f);
}

// ---- hooked methods (vtable order from d3d9.h)
typedef HRESULT(STDMETHODCALLTYPE* CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
typedef HRESULT(STDMETHODCALLTYPE* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT(STDMETHODCALLTYPE* CreateTexture_t)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
typedef HRESULT(STDMETHODCALLTYPE* CreateVB_t)(IDirect3DDevice9*, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE*);
typedef HRESULT(STDMETHODCALLTYPE* CreateIB_t)(IDirect3DDevice9*, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9**, HANDLE*);
typedef HRESULT(STDMETHODCALLTYPE* SetTransform_t)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
typedef HRESULT(STDMETHODCALLTYPE* DP_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* DIP_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* DPUP_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
typedef HRESULT(STDMETHODCALLTYPE* DIPUP_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void*, D3DFORMAT, const void*, UINT);
typedef HRESULT(STDMETHODCALLTYPE* SetTexture_t)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
typedef HRESULT(STDMETHODCALLTYPE* SetPSCF_t)(IDirect3DDevice9*, UINT, const float*, UINT);

CreateDevice_t oCreateDevice; EndScene_t oEndScene; CreateTexture_t oCreateTexture; CreateVB_t oCreateVB; CreateIB_t oCreateIB;
SetTransform_t oSetTransform; DP_t oDP; DIP_t oDIP; DPUP_t oDPUP; DIPUP_t oDIPUP; SetPSCF_t oSetPSCF;
typedef HRESULT(STDMETHODCALLTYPE* SetRS_t)(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD);
SetRS_t oSetRS;
HRESULT STDMETHODCALLTYPE hSetRS(IDirect3DDevice9* d, D3DRENDERSTATETYPE st, DWORD v) {
    if (g_recording && g_tlog && (st == 139 || st == 34 || st == 36 || st == 37)) {
        static int n = 0;
        if (n++ < 60) g_tlog("SetRS state=%d value=%08x", (int)st, v);
    }
    return oSetRS(d, st, v);
}
typedef HRESULT(STDMETHODCALLTYPE* CreatePS_t)(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**);
CreatePS_t oCreatePS;
HRESULT STDMETHODCALLTYPE hCreatePS(IDirect3DDevice9* d, const DWORD* fn, IDirect3DPixelShader9** ps) {
    if (g_recording && g_tlog && fn) {
        static int n = 0;
        if (n++ < 6) {
            g_tlog("CreatePS @%p:", fn);
            for (int k = 0; k < 32; k++) g_tlog("  [%02d] %08x", k, fn[k]);
        }
    }
    return oCreatePS(d, fn, ps);
}
typedef HRESULT(STDMETHODCALLTYPE* Clear_t)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD);
Clear_t oClear;
HRESULT STDMETHODCALLTYPE hClear(IDirect3DDevice9* d, DWORD cnt, const D3DRECT* r, DWORD flags, D3DCOLOR color, float z, DWORD stencil) {
    if (g_recording && g_tlog) {
        static int n = 0; static D3DCOLOR last = 0xffffffff;
        if (n++ < 30 || color != last) { g_tlog("Clear color=%08x flags=%x", color, flags); last = color; }
    }
    terrain12Flush(Terrain12FlushReason::Clear);
    return oClear(d, cnt, r, flags, color, z, stencil);
}
typedef HRESULT(STDMETHODCALLTYPE* SetVSCF_t)(IDirect3DDevice9*, UINT, const float*, UINT);
SetVSCF_t oSetVSCF;
HRESULT STDMETHODCALLTYPE hSetVSCF(IDirect3DDevice9* d, UINT reg, const float* data, UINT count) {
    if (g_recording && g_tlog && data && (reg == 24 || reg == 26 || reg == 42) && count >= 1) {
        static int n = 0;
        if (n++ < 60) g_tlog("SetVSCF c%u = (%g, %g, %g, %g)", reg, data[0], data[1], data[2], data[3]);
    }
    return oSetVSCF(d, reg, data, count);
}

#define FRAME() _AddressOfReturnAddress()

// Keep EndScene timing separate from Present timing.
HRESULT STDMETHODCALLTYPE hEndScene(IDirect3DDevice9* d) {
    terrain12Flush(Terrain12FlushReason::EndScene);
    g_frames++;
    HRESULT hr = oEndScene(d);
    renderOnFrame();
    automationOnEndScene(d);
    return hr;
}

DWORD WINAPI dumpThread(void*) {
    if (g_tlog) g_tlog("trace: dump thread running");
    Sleep(3000);
    for (;;) { dump(); if (g_tlog) g_tlog("trace: dumped (%llu frames, %zu call sites)", g_frames, g_stats.size()); Sleep(10000); }
}
SetTexture_t oSetTexture;
HRESULT STDMETHODCALLTYPE hSetPixelShaderConstantF(IDirect3DDevice9* d, UINT reg, const float* data, UINT count) {
    // 打印常量，找雾色 0.69（灰白 176/255）—— 只打包含 0.4~1.0 分量的寄存器
    if (g_recording && g_tlog && data && count <= 8) {
        for (UINT i = 0; i < count; i++) {
            float a = data[i * 4], b = data[i * 4 + 1], c = data[i * 4 + 2], e = data[i * 4 + 3];
            if ((a > 0.4f && a < 1.0f) || (b > 0.4f && b < 1.0f) || (c > 0.4f && c < 1.0f) || (e > 0.4f && e < 1.0f)) {
                if (g_tlog) g_tlog("psc[%u] = (%.4f, %.4f, %.4f, %.4f)", reg + i, a, b, c, e);
            }
        }
    }
    return oSetPSCF(d, reg, data, count);
}
template <class T> void patchSlot(void** vtbl, int index, T hook, T* orig);
typedef HRESULT(STDMETHODCALLTYPE* SetTarget_t)(IDirect3DDevice9*,DWORD,IDirect3DSurface9*);
typedef HRESULT(STDMETHODCALLTYPE* SetDepth_t)(IDirect3DDevice9*,IDirect3DSurface9*);
SetTarget_t oSetTarget;SetDepth_t oSetDepth;
HRESULT STDMETHODCALLTYPE hSetTarget(IDirect3DDevice9* d,DWORD i,IDirect3DSurface9* s) {
    terrain12Flush(Terrain12FlushReason::ColorTarget);
    HRESULT hr=oSetTarget(d,i,copyMsaaColor(d,i,s));
    return SUCCEEDED(hr)?copyMsaaTargetChanged(d,i):hr;
}
HRESULT STDMETHODCALLTYPE hSetDepth(IDirect3DDevice9* d,IDirect3DSurface9* s) {terrain12Flush(Terrain12FlushReason::DepthTarget);return oSetDepth(d,copyMsaaDepth(d,s));}
HRESULT STDMETHODCALLTYPE hCreateTexture(IDirect3DDevice9* d, UINT w, UINT h, UINT l, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DTexture9** t, HANDLE* s) {
    HRESULT hr = oCreateTexture(d, w, h, l, u, f, p, t, s);
    if(renderNativeTerrainRequested() && SUCCEEDED(hr) && t && *t)terrain12HookTexture(*t);
    // d = the new texture (pointer), to match it with SetTexture records
    record(M_TEX, FRAME(), 0, w, h, ((UINT)f << 8) | (UINT)p, SUCCEEDED(hr) && t ? (UINT)(uintptr_t)*t : 0);
    return hr;
}
static int g_grayLog = 0;
HRESULT STDMETHODCALLTYPE hSetTexture(IDirect3DDevice9* d, DWORD stage, IDirect3DBaseTexture9* tex) {
    if (!g_recording) return oSetTexture(d, stage, tex);
    UINT wh = 0, fmt = 0;
    if (tex && tex->GetType() == D3DRTYPE_TEXTURE) {
        D3DSURFACE_DESC desc = {};
        if (SUCCEEDED(((IDirect3DTexture9*)tex)->GetLevelDesc(0, &desc))) { wh = (desc.Width << 16) | desc.Height; fmt = desc.Format; }
        // 检测灰白云海纹理（雾候选）：中心/四角像素接近中性灰白
        if (stage == 0 && g_grayLog < 40 && desc.Width >= 128 && desc.Height >= 64 &&
            (desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8)) {
            D3DLOCKED_RECT lr;
            if (SUCCEEDED(((IDirect3DTexture9*)tex)->LockRect(0, &lr, nullptr, D3DLOCK_READONLY))) {
                int W = (int)desc.Width, H = (int)desc.Height, pit = (int)lr.Pitch;
                uint8_t* base = (uint8_t*)lr.pBits;
                int xs[5] = { W/2, 0, W-1, W/2, W/2 }, ys[5] = { H/2, H/2, H/2, 0, H-1 };
                bool gray = true; int r = 0, g = 0, b = 0;
                for (int k = 0; k < 5; k++) {
                    uint8_t* p = base + ys[k] * pit + xs[k] * 4;
                    b = p[0]; g = p[1]; r = p[2];           // A8R8G8B8: BGRA
                    int d1 = r > g ? r - g : g - r, d2 = g > b ? g - b : b - g;
                    if (d1 > 24 || d2 > 24 || r < 110 || r > 230) { gray = false; break; }
                }
                if (gray) {
                    uint32_t c[3]; callers(FRAME(), c);
                    g_grayLog++;
                    g_tlog("GRAY TEX %ux%u px=(%d,%d,%d) caller=%08x %08x %08x", W, H, r, g, b, c[0], c[1], c[2]);
                }
                ((IDirect3DTexture9*)tex)->UnlockRect(0);
            }
        }
    }
    if (stage == 0) g_curTexWh = wh;
    record(M_SETTEX, FRAME(), 0, stage, wh, fmt, (UINT)(uintptr_t)tex);
    return oSetTexture(d, stage, tex);
}
using CreateVS_t=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,const DWORD*,IDirect3DVertexShader9**);
CreateVS_t oCreateVS;
HRESULT STDMETHODCALLTYPE hCreateVS(IDirect3DDevice9* d,const DWORD* code,IDirect3DVertexShader9** out) {
    HRESULT hr=oCreateVS(d,code,out);
    if(SUCCEEDED(hr) && out && *out) {
        // Verified 0x44d1e0 wrapper: COM args, saved EDI/ESI, blob local,
        // wrapper return, device, then original assembly-source pointer.
        auto frame=(const uintptr_t*)_AddressOfReturnAddress();
        object12ShaderCreated(*out,frame[0]==0x44d22f?frame[9]:0);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE hCreateVB(IDirect3DDevice9* d, UINT len, DWORD u, DWORD fvf, D3DPOOL p, IDirect3DVertexBuffer9** vb, HANDLE* s) {
    record(M_VB, FRAME(), 0, len, u, fvf, (UINT)p);
    HRESULT hr=oCreateVB(d,len,u,fvf,p,vb,s);
    if(renderNativeTerrainRequested() && SUCCEEDED(hr) && vb && *vb) {
        terrain12ForgetBuffer(*vb);
        auto frame=(const uintptr_t*)_AddressOfReturnAddress();
        if(frame[0]==0x44d065 && ((frame[8]>=0x42e600 && frame[8]<0x432500) ||
           (frame[8]>=0x44a600 && frame[8]<0x44a900) || frame[8]==0x412c9b || frame[8]==0x402896))objectCaptureRegister(*vb,len,p,u);
        if((frame[0]==0x44d4e7 && frame[8]==0x4421e4) || ((fvf==0x44 || fvf==0x104 || fvf==0x144 || fvf==0x142 || fvf==0x102 || fvf==0x2) && !(frame[0]==0x44d065 && frame[8]==0x422732)))objectCaptureRegister(*vb,len,p,u);
        terrain12HookBuffer(*vb);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE hCreateIB(IDirect3DDevice9* d, UINT len, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DIndexBuffer9** ib, HANDLE* s) {
    record(M_IB, FRAME(), 0, len, u, (UINT)f, (UINT)p);
    HRESULT hr=oCreateIB(d,len,u,f,p,ib,s);
    if(renderNativeTerrainRequested() && SUCCEEDED(hr) && ib && *ib) {
        terrain12ForgetBuffer(*ib);
        auto frame=(const uintptr_t*)_AddressOfReturnAddress();
        if(frame[0]==0x44d095 && ((frame[8]>=0x42e600 && frame[8]<0x432500) ||
           (frame[8]>=0x44a600 && frame[8]<0x44a900) || frame[8]==0x412566 || frame[8]==0x40338f || frame[8]==0x403808 || frame[8]==0x40f0d9 || frame[8]==0x40f783))objectCaptureRegister(*ib,len,p,u);
        terrain12HookBuffer(*ib);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE hSetTransform(IDirect3DDevice9* d, D3DTRANSFORMSTATETYPE st, const D3DMATRIX* m) {
    // 投影矩阵：打印关键元素（正交投影 _11=2/w, _22=2/h, _33=1/far, _41/_42=中心偏移）
    if (g_recording && g_tlog && st == D3DTS_PROJECTION && m) {
        static int n = 0;
        if (n++ < 12) g_tlog("PROJ _11=%g _22=%g _33=%g _41=%g _42=%g _43=%g",
                             m->_11, m->_22, m->_33, m->_41, m->_42, m->_43);
    }
    // c,d = world-space translation (x,z) for world matrices, truncated to integers
    record(M_XFORM, FRAME(), 0, (UINT)st, 0, m ? (UINT)(INT)m->_41 : 0, m ? (UINT)(INT)m->_43 : 0);
    return oSetTransform(d, st, m);
}
HRESULT STDMETHODCALLTYPE hDP(IDirect3DDevice9* d, D3DPRIMITIVETYPE t, UINT sv, UINT pc) {
    bool native=model12DrawPrimitive(d,t,sv,pc);terrain12RecordDraw(d,0,t,native);
    if(native)return D3D_OK;
    terrain12Flush(Terrain12FlushReason::FallbackDP);
    record(M_DP, FRAME(), pc, (UINT)t, sv, pc); return oDP(d, t, sv, pc);
}
HRESULT STDMETHODCALLTYPE hDIP(IDirect3DDevice9* d, D3DPRIMITIVETYPE t, INT bv, UINT mi, UINT nv, UINT si, UINT pc) {
    // The game calls DIP through 0x44d190: return address + seven COM args,
    // then the wrapper's caller. Match both sites instead of scanning the stack.
    auto frame = (const uintptr_t*)_AddressOfReturnAddress();
    TerrainFilterScope filter;
    if(frame[0]==0x44d1bb && frame[8]==0x422d29 && water12Draw(d,t,bv,mi,nv,si,pc)){terrain12RecordDraw(d,1,t,true);return D3D_OK;}
    if(frame[0]==0x44d1bb && frame[8]==0x41b391 && object12Draw(d,t,bv,mi,nv,si,pc)){terrain12RecordDraw(d,1,t,true);return D3D_OK;}
    if (frame[0] == 0x44d1bb && frame[8] >= 0x41df50 && frame[8] < 0x421b10) {
        if(frame[8] == 0x41f745)renderProbeTerrain(d);
        if (terrainFilterEnabled() && t == D3DPT_TRIANGLELIST)
            filter.apply(d, *(IDirect3DVertexShader9**)(0x4e4d880 + 0x1c));
        if (terrain12Draw(d,t,bv,mi,nv,si,pc)){terrain12RecordDraw(d,1,t,true);return D3D_OK;}
    }
    bool native=model12Draw(d,t,bv,mi,nv,si,pc);terrain12RecordDraw(d,1,t,native);
    if(native)return D3D_OK;
    terrain12Flush(Terrain12FlushReason::FallbackDIP);
    if (g_recording && g_tlog && nv > 5000) {
        uint32_t c[3]; callers(FRAME(), c);
        g_tlog("BIG DIP: nv=%u prim=%u type=%u texWh=%08x (w=%u h=%u) caller=%08x %08x %08x",
               nv, pc, (UINT)t, g_curTexWh, g_curTexWh >> 16, g_curTexWh & 0xffff, c[0], c[1], c[2]);
    }
    record(M_DIP, FRAME(), pc, (UINT)t, nv, si, (UINT)bv); return oDIP(d, t, bv, mi, nv, si, pc);
}
HRESULT STDMETHODCALLTYPE hDPUP(IDirect3DDevice9* d, D3DPRIMITIVETYPE t, UINT pc, const void* v, UINT stride) {
    bool native=model12DrawUP(d,t,pc,v,stride);terrain12RecordDraw(d,2,t,native,stride);
    if(native)return D3D_OK;
    terrain12Flush(Terrain12FlushReason::FallbackDPUP);
    record(M_DPUP, FRAME(), pc, (UINT)t, pc, stride); return oDPUP(d, t, pc, v, stride);
}
HRESULT STDMETHODCALLTYPE hDIPUP(IDirect3DDevice9* d, D3DPRIMITIVETYPE t, UINT mi, UINT nv, UINT pc, const void* i, D3DFORMAT f, const void* v, UINT stride) {
    bool native=model12DrawIndexedUP(d,t,mi,nv,pc,i,f,v,stride);terrain12RecordDraw(d,3,t,native,stride);
    if(native)return D3D_OK;
    terrain12Flush(Terrain12FlushReason::FallbackDIPUP);
    record(M_DIPUP, FRAME(), pc, (UINT)t, nv, pc, stride); return oDIPUP(d, t, mi, nv, pc, i, f, v, stride);
}

template <class T> void patchSlot(void** vtbl, int index, T hook, T* orig) {
    if (vtbl[index] == (void*)hook) return;
    *orig = (T)vtbl[index];
    DWORD old; VirtualProtect(&vtbl[index], 4, PAGE_READWRITE, &old);
    vtbl[index] = (void*)hook;
    VirtualProtect(&vtbl[index], 4, old, &old);
}

typedef HRESULT(STDMETHODCALLTYPE* SwapPresent_t)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
typedef HRESULT(STDMETHODCALLTYPE* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT(STDMETHODCALLTYPE* GetSwapChain_t)(IDirect3DDevice9*, UINT, IDirect3DSwapChain9**);
std::map<void**, SwapPresent_t> swapPresents;
std::map<void**, Present_t> devicePresents;
SRWLOCK swapHookLock = SRWLOCK_INIT;
thread_local unsigned presentDepth = 0;
GetSwapChain_t oGetSwapChain;
void logPresentFailure(HRESULT hr,IDirect3DSwapChain9* chain,const RECT* src,const RECT* dst,HWND window,const RGNDATA* dirty,DWORD flags) {
    static LONG failures=0;
    if(SUCCEEDED(hr) || !g_tlog || InterlockedIncrement(&failures)>8)return;
    D3DPRESENT_PARAMETERS pp={};if(chain)chain->GetPresentParameters(&pp);
    HWND target=window?window:pp.hDeviceWindow;
    RECT client={};GetClientRect(target,&client);
    RECT s=src?*src:RECT{},d=dst?*dst:RECT{};
    g_tlog("render: Present failed hr=%08lx window=%p target=%p valid=%d foreground=%p client=%ld,%ld format=%u size=%ux%u swap=%u flags=%lx src=%d:%ld,%ld,%ld,%ld dst=%d:%ld,%ld,%ld,%ld dirty=%p",
        hr,window,target,IsWindow(target),GetForegroundWindow(),client.right,client.bottom,pp.BackBufferFormat,pp.BackBufferWidth,pp.BackBufferHeight,pp.SwapEffect,flags,
        src!=nullptr,s.left,s.top,s.right,s.bottom,dst!=nullptr,d.left,d.top,d.right,d.bottom,dirty);
}
HRESULT STDMETHODCALLTYPE hPresent(IDirect3DDevice9* d, const RECT* src, const RECT* dst, HWND window, const RGNDATA* dirty) {
    AcquireSRWLockShared(&swapHookLock); auto call = devicePresents.at(*(void***)d); ReleaseSRWLockShared(&swapHookLock);
    HWND target = window;
    if (!target) {
        Microsoft::WRL::ComPtr<IDirect3DSwapChain9> chain;
        D3DPRESENT_PARAMETERS pp = {};
        if (SUCCEEDED(d->GetSwapChain(0, &chain)) && SUCCEEDED(chain->GetPresentParameters(&pp))) target = pp.hDeviceWindow;
        if (!target) { D3DDEVICE_CREATION_PARAMETERS cp = {}; d->GetCreationParameters(&cp); target = cp.hFocusWindow; }
    }
    ++presentDepth;
    if (presentDepth == 1) renderWaitForFrame();
    LARGE_INTEGER begin, end; QueryPerformanceCounter(&begin);
    HRESULT resolve=S_OK;
    if (presentDepth == 1) {terrain12Flush(Terrain12FlushReason::Present);resolve=copyMsaaResolve(d);}
    HRESULT hr = FAILED(resolve)?resolve:call(d, src, dst, window, dirty);
    QueryPerformanceCounter(&end);
    if(FAILED(hr) && presentDepth==1) {
        Microsoft::WRL::ComPtr<IDirect3DSwapChain9> chain;d->GetSwapChain(0,&chain);
        logPresentFailure(hr,chain.Get(),src,dst,window,dirty,0);
    }
    if (!--presentDepth) renderOnPresent(begin, end, hr, target);
    return hr;
}
HRESULT STDMETHODCALLTYPE hSwapPresent(IDirect3DSwapChain9* s, const RECT* src, const RECT* dst, HWND window, const RGNDATA* dirty, DWORD flags) {
    AcquireSRWLockShared(&swapHookLock); auto call = swapPresents.at(*(void***)s); ReleaseSRWLockShared(&swapHookLock);
    D3DPRESENT_PARAMETERS pp = {}; if (!window) s->GetPresentParameters(&pp);
    ++presentDepth;
    if (presentDepth == 1) renderWaitForFrame();
    LARGE_INTEGER begin, end; QueryPerformanceCounter(&begin);
    HRESULT resolve=S_OK;
    if (presentDepth == 1) {
        terrain12Flush(Terrain12FlushReason::Present);
        Microsoft::WRL::ComPtr<IDirect3DDevice9> device;
        Microsoft::WRL::ComPtr<IDirect3DSwapChain9> primary;
        if(SUCCEEDED(s->GetDevice(&device)) && SUCCEEDED(device->GetSwapChain(0,&primary)) && primary.Get()==s)
            resolve=copyMsaaResolve(device.Get());
    }
    HRESULT hr = FAILED(resolve)?resolve:call(s, src, dst, window, dirty, flags);
    QueryPerformanceCounter(&end);
    if(FAILED(hr) && presentDepth==1)logPresentFailure(hr,s,src,dst,window,dirty,flags);
    if (!--presentDepth) renderOnPresent(begin, end, hr, window ? window : pp.hDeviceWindow);
    return hr;
}
HRESULT STDMETHODCALLTYPE hGetSwapChain(IDirect3DDevice9* d, UINT index, IDirect3DSwapChain9** out) {
    HRESULT hr = oGetSwapChain(d, index, out);
    if (SUCCEEDED(hr) && out && *out) {
        void** v = *(void***)*out;
        AcquireSRWLockExclusive(&swapHookLock);
        patchSlot(v, 3, (SwapPresent_t)hSwapPresent, &swapPresents[v]);
        ReleaseSRWLockExclusive(&swapHookLock);
    }
    return hr;
}

void hookDevice(IDirect3DDevice9* device);
typedef HRESULT(STDMETHODCALLTYPE* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
Reset_t oReset;
HRESULT STDMETHODCALLTYPE hReset(IDirect3DDevice9* d, D3DPRESENT_PARAMETERS* pp) {
    D3DPRESENT_PARAMETERS requested=pp?*pp:D3DPRESENT_PARAMETERS{};
    renderBeforeReset();
    renderAdjustPresentation(pp, d);
    bool changedSamples=pp && pp->MultiSampleType!=requested.MultiSampleType;
    HRESULT hr = oReset(d, pp);
    if(FAILED(hr) && changedSamples) {
        if(g_tlog)g_tlog("render: MSAA Reset failed hr=%08lx; retrying game targets",hr);
        *pp=requested;hr=oReset(d,pp);
    }
    // D3D9 can rewrite the device vtable even when Reset fails. Reinstall
    // before returning so the next Reset and recovered draws still reach us.
    hookDevice(d);
    if (pp) renderAfterReset(d, hr, *pp);
    if (SUCCEEDED(hr)) { Microsoft::WRL::ComPtr<IDirect3DSwapChain9> chain; d->GetSwapChain(0, &chain); }
    return hr;
}

void hookDevice(IDirect3DDevice9* device) {
    void** v = *(void***)device;
    patchSlot(v, 42, (EndScene_t)hEndScene, &oEndScene);
    patchSlot(v, 16, (Reset_t)hReset, &oReset);
    patchSlot(v, 14, (GetSwapChain_t)hGetSwapChain, &oGetSwapChain);
    AcquireSRWLockExclusive(&swapHookLock);
    patchSlot(v, 17, (Present_t)hPresent, &devicePresents[v]);
    ReleaseSRWLockExclusive(&swapHookLock);
    patchSlot(v, 82, (DIP_t)hDIP, &oDIP);
    if (g_recording || renderNativeTerrainRequested()) {
        patchSlot(v, 43, (Clear_t)hClear, &oClear);
        patchSlot(v, 23, (CreateTexture_t)hCreateTexture, &oCreateTexture);
        patchSlot(v, 26, (CreateVB_t)hCreateVB, &oCreateVB);
        patchSlot(v, 27, (CreateIB_t)hCreateIB, &oCreateIB);
        patchSlot(v, 81, (DP_t)hDP, &oDP);
        patchSlot(v, 83, (DPUP_t)hDPUP, &oDPUP);
        patchSlot(v, 84, (DIPUP_t)hDIPUP, &oDIPUP);
    }
    if (renderNativeTerrainRequested()) {
        terrain12HookTextureUpdates(device);
        patchSlot(v,91,(CreateVS_t)hCreateVS,&oCreateVS);
        terrain12HookBufferWrites(device);
        patchSlot(v,37,(SetTarget_t)hSetTarget,&oSetTarget);
        patchSlot(v,39,(SetDepth_t)hSetDepth,&oSetDepth);
    }
    if (g_recording) {
        patchSlot(v, 44, (SetTransform_t)hSetTransform, &oSetTransform);
        patchSlot(v, 65, (SetTexture_t)hSetTexture, &oSetTexture);
        patchSlot(v, 57, (SetRS_t)hSetRS, &oSetRS);
        patchSlot(v, 94, (SetVSCF_t)hSetVSCF, &oSetVSCF);
        patchSlot(v, 76, (CreatePS_t)hCreatePS, &oCreatePS);
    }
}

HRESULT STDMETHODCALLTYPE hCreateDevice(IDirect3D9* self, UINT ad, D3DDEVTYPE ty, HWND w, DWORD fl, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out) {
    D3DPRESENT_PARAMETERS requested = pp ? *pp : D3DPRESENT_PARAMETERS{};
    renderAdjustPresentation(pp,nullptr,self,ad,ty);
    bool changedSamples=pp && pp->MultiSampleType!=requested.MultiSampleType;
    HRESULT hr = oCreateDevice(self, ad, ty, w, fl, pp, out);
    if(FAILED(hr) && changedSamples && out) {
        if(g_tlog)g_tlog("render: MSAA CreateDevice failed hr=%08lx; retrying game targets",hr);
        *pp=requested;*out=nullptr;hr=oCreateDevice(self,ad,ty,w,fl,pp,out);
    }
    Microsoft::WRL::ComPtr<IDirect3D9> fallback;
    if (FAILED(hr) && pp && out) {
        fallback.Attach(renderFallback9());
        if (fallback) {
            auto create = (CreateDevice_t)(*(void***)fallback.Get())[16];
            if (create == hCreateDevice) create = oCreateDevice;
            HRESULT initialFailure = hr;
            *pp = requested; *out = nullptr;
            hr = create(fallback.Get(), ad, ty, w, fl, pp, out);
            if (SUCCEEDED(hr)) { self = fallback.Get(); renderDeviceFallback(initialFailure); }
        }
    }
    if (g_tlog) g_tlog("trace: CreateDevice type=%d flags=%#lx hr=%#lx device=%p", (int)ty, fl, hr, out ? *out : nullptr);
    if (SUCCEEDED(hr) && out && *out) {
        void** v = *(void***)*out;
        if (g_tlog) g_tlog("vtable: [57]=%p [65]=%p [74]=%p [75]=%p [88]=%p [89]=%p [94]=%p",
                           v[57], v[65], v[74], v[75], v[88], v[89], v[94]);
        hookDevice(*out);
        { Microsoft::WRL::ComPtr<IDirect3DSwapChain9> chain; (*out)->GetSwapChain(0, &chain); }
        if (pp) renderOnDevice(self, ad, *out, *pp);
        if (g_tlog) g_tlog("trace: device vtable %p hooked", v);
        static bool started = false;
        if (g_recording && !started) { started = true; HANDLE t = CreateThread(nullptr, 256 << 10, dumpThread, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr); if (g_tlog) g_tlog("trace: dump thread %p", t); }
    }
    return hr;
}

}  // namespace

void traceInit(const wchar_t* dir, const wchar_t* section, TraceLogFn log, bool recordCalls) {
    g_tlog = log;
    g_recording = recordCalls;
    InitializeCriticalSection(&g_cs);
    swprintf_s(g_path, L"%s\\worldmod_trace_%s.log", dir, section);
}

void* traceWrapDirect3D9(void* d3d) {
    if (d3d) patchSlot(*(void***)d3d, 16, (CreateDevice_t)hCreateDevice, &oCreateDevice);
    if (g_tlog) g_tlog("trace: Direct3DCreate9 -> %p, CreateDevice hooked", d3d);
    return d3d;
}
