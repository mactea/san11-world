#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "terrain_dx12.h"
#include "object_capture.h"
#include "terrain_replacement.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <vector>
#include <map>
#include <atomic>
#include <set>
#include <algorithm>
#include <array>
#include <tuple>
#include <mutex>
#include <cmath>
#include <string.h>
using Microsoft::WRL::ComPtr;

namespace {
// The stock terrain owns two dynamic streams and two index buffers. Capture the
// bytes in ordinary RAM and copy forward on Unlock. Reading a WRITEONLY /
// write-combined D3D9 allocation is both invalid and extremely slow.
constexpr uintptr_t TERRAIN_BUFFERS = 0x4e4d880;
#ifdef WORLDMOD_RENDER_TEST
IDirect3DResource9* terrainBuffers[4] = {};
IDirect3DVertexShader9* terrainShaders[3] = {};
IDirect3DResource9* waterBuffers[2] = {};
IDirect3DVertexShader9* waterShader = nullptr;
IDirect3DVertexShader9* objectShaders[2] = {};
#else
auto terrainBuffers = (IDirect3DResource9**)TERRAIN_BUFFERS;
auto terrainShaders = (IDirect3DVertexShader9**)(TERRAIN_BUFFERS+0x1c);
auto waterBuffers = (IDirect3DResource9**)(0x4e482fc+0x5558);
auto& waterShader = *(IDirect3DVertexShader9**)0x8f78e8;
auto objectShaders = (IDirect3DVertexShader9**)0x4ecdb24;
#endif
// Keep typical UI passes in one submission. Two sampler descriptors per draw
// remain within D3D12's 2048-entry shader-visible sampler heap limit.
// A typical measured scene uploads about 9.5 MiB between Presents. Give each
// ring room for it without widening the existing per-buffer/input limits.
constexpr UINT MAX_BUFFER_BYTES = 8 << 20, UPLOAD_BYTES = 12 << 20, MAX_DRAWS = 1024;
#ifdef WORLDMOD_RENDER_TEST
UINT64 TEXTURE_BUDGET = 96ull << 20, textureUploads = 0;
Terrain12TestWork testWork = {};
bool testPrefixUpload = false;
#else
constexpr UINT64 TEXTURE_BUDGET = 96ull << 20; // plus four 12 MiB upload rings
#endif
struct Shadow {
    std::vector<BYTE> bytes;
    void* locked = nullptr;
    D3DPOOL pool = D3DPOOL_DEFAULT;
    UINT offset = 0, size = 0;
    UINT64 version = 0;
    std::map<UINT,UINT> initialized;
};
std::map<IDirect3DResource9*, Shadow> shadows;
SRWLOCK retiredLock = SRWLOCK_INIT;
std::set<IDirect3DResource9*> retiredBuffers;
struct Texture {
    ComPtr<IDirect3DBaseTexture9> source;
    ComPtr<ID3D12Resource> resource;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT levels = 0, mapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    UINT64 allocationBytes = 0;
    bool dirty = true;
};
std::map<IDirect3DBaseTexture9*, Texture> textures;
SRWLOCK dirtyLock = SRWLOCK_INIT;
std::set<IDirect3DBaseTexture9*> dirtyTextures;
ComPtr<ID3D12Device> gpu;
ComPtr<ID3D12CommandQueue> queue;
ComPtr<ID3D12Fence> fence;
ComPtr<ID3D12RootSignature> root;
ComPtr<ID3DBlob> vs, ps, waterVs, waterPs;
std::atomic<float> waterStrength{0},waterTime{0};
ComPtr<IDirect3DTexture9> whiteTexture;
TraceLogFn logfn;
UINT64 serial = 0, textureBytes = 0;
std::atomic<UINT64> draws{0};
std::atomic<bool> enabled{true};

HANDLE eventHandle = nullptr;
std::atomic<bool> failed{false};
std::atomic<UINT64> modeDraws[21] = {};
enum RejectPhase { LayoutReject, BufferReject, StateReject, InteropReject, TextureReject, PipelineReject, UploadReject, RejectPhases };
std::atomic<UINT64> recognized[21]={}, recorded[21]={}, rejected[21][RejectPhases]={};
std::atomic<DWORD> reportedBufferRejects[21]={};
struct CallCounts { std::atomic<UINT64> attempts{0},native{0},topology[7]={}; } callCounts[4];
std::mutex fallbackMutex;
using FallbackKey=std::array<DWORD,11>;
std::map<FallbackKey,UINT64> fallbackStates;
UINT64 fallbackOverflow=0;
struct DrawCoverage {
    int mode=-1;RejectPhase phase=LayoutReject;bool accepted=false;
    ~DrawCoverage(){if(mode>=0){++recognized[mode];if(accepted)++recorded[mode];else ++rejected[mode][phase];}}
};

std::mutex modelShaderMutex;
std::map<IDirect3DVertexShader9*,int> modelShaders;

// Opt-in CPU diagnostics. Cumulative counters survive Reset so a reader can
// subtract snapshots without resetting counters underneath the render thread.
enum CpuStage { DrawCpu, StateCpu, BeginCpu, TextureCpu, PipelineCpu, UploadCpu, RecordCpu, SubmitCpu, WaitCpu,
    CloseCpu, ExecuteCpu, SignalCpu, ReturnColorCpu, ReturnDepthCpu, CpuStages };
const char* cpuNames[] = {"draw", "state", "begin", "texture", "pipeline", "upload", "record", "submit", "wait",
    "close", "execute", "signal", "return_color", "return_depth"};
std::atomic<bool> profileEnabled{false};
struct CpuCounter { std::atomic<UINT64> ticks{0}, calls{0}; } cpuCounters[CpuStages];
const char* flushNames[] = {"explicit", "clear", "end_scene", "color_target", "depth_target",
    "fallback_dp", "fallback_dip", "fallback_dpup", "fallback_dipup", "present", "texture_budget",
    "draw_limit", "upload_limit", "reset", "process_vertices"};
static_assert(sizeof(flushNames)/sizeof(*flushNames)==size_t(Terrain12FlushReason::Count),"flush reasons");
// Count only active batches, including empty batches after a rejected draw.
// An empty batch can contain texture uploads and must still be submitted.
struct FlushCounter { std::atomic<UINT64> batches{0}, draws{0}, empty{0}; } flushCounters[size_t(Terrain12FlushReason::Count)];
// Read timestamps only after the existing batch fence completes. No profiling wait.
std::mutex gpuProfileMutex;
UINT64 gpuBatches=0, gpuNanoseconds=0;
std::atomic<UINT64> gpuPending{0},gpuErrors{0};
std::atomic<UINT64> uploadBytes{0},bufferCopies{0},bufferHits{0},textureCopies{0},textureCopyBytes{0};
struct CpuTimer {
    bool active = profileEnabled.load(std::memory_order_relaxed);
    CpuStage stage;
    LARGE_INTEGER start = {};
    CpuTimer(CpuStage value):stage(value) { if(active)QueryPerformanceCounter(&start); }
    void finish() {
        if(!active)return;
        LARGE_INTEGER end;QueryPerformanceCounter(&end);
        cpuCounters[stage].ticks.fetch_add(end.QuadPart-start.QuadPart,std::memory_order_relaxed);
        cpuCounters[stage].calls.fetch_add(1,std::memory_order_relaxed);
        active=false;
    }
    void next(CpuStage value) {
        finish();stage=value;active=profileEnabled.load(std::memory_order_relaxed);
        if(active)QueryPerformanceCounter(&start);
    }
    ~CpuTimer(){finish();}
};

struct PipelineKey {
    UINT color, depth, cull, zenable, zwrite, zfunc, blend, src, dst, op, mask, water, samples, quality, waterEffect, lines;
    bool operator<(const PipelineKey& b) const { return memcmp(this, &b, sizeof b) < 0; }
};
std::map<PipelineKey, ComPtr<ID3D12PipelineState>> pipelines;
using CopyKey = std::tuple<IDirect3DResource9*, UINT64, UINT>; // resource, version, source byte offset
struct Frame {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12Resource> upload;
    BYTE* mapped = nullptr;
    UINT used = 0, count = 0, modes[21] = {};
    UINT64 complete = 0;
    ComPtr<ID3D12QueryHeap> timestamps;
    ComPtr<ID3D12Resource> timestampReadback;
    UINT64 timestampFrequency=0;
    bool timestampPending=false, timestampAttempted=false;
    ComPtr<ID3D12DescriptorHeap> srv, sampler, rtv, dsv;
    ComPtr<IDirect3DDevice9On12> bridge;
    ComPtr<IDirect3DSurface9> color9, depth9;
    ComPtr<ID3D12Resource> color12, depth12;
    std::vector<ComPtr<ID3D12Resource>> uploads;
    std::map<CopyKey, std::pair<UINT,UINT>> copies;
    std::map<ID3D12Resource*,UINT> srvSlots;
    std::map<std::array<DWORD,11>,UINT> samplerSlots;
} frames[4];
Frame* current = nullptr;
UINT frameIndex = 0;

bool ok(HRESULT hr, const char* where) {
    if (SUCCEEDED(hr)) return true;
    if (logfn) logfn("native terrain: %s failed hr=%08lx", where, hr);
    failed = true;
    return false;
}
#define TRY(x) if (!ok((x), #x)) return false
void wait(UINT64 value) {
    if (!value || fence->GetCompletedValue() >= value) return;
    CpuTimer timer(WaitCpu);
    if (SUCCEEDED(fence->SetEventOnCompletion(value, eventHandle))) WaitForSingleObject(eventHandle, INFINITE);
}
void collectGpuProfile(Frame& f) {
    if(!f.timestampPending || !fence || fence->GetCompletedValue()<f.complete)return;
    f.timestampPending=false;--gpuPending;
    UINT64* values=nullptr;D3D12_RANGE range={0,2*sizeof(UINT64)};
    HRESULT hr=f.timestampReadback->Map(0,&range,(void**)&values);
    if(FAILED(hr)){++gpuErrors;return;}
    if(values[1]>=values[0]) {
        UINT64 ns=UINT64(double(values[1]-values[0])*1e9/f.timestampFrequency);
        std::lock_guard<std::mutex> guard(gpuProfileMutex);++gpuBatches;gpuNanoseconds+=ns;
    } else ++gpuErrors;
    D3D12_RANGE noWrite={};f.timestampReadback->Unmap(0,&noWrite);
}
void prepareGpuProfile(Frame& f) {
    if(!profileEnabled.load() || f.timestampAttempted)return;
    f.timestampAttempted=true;
    D3D12_QUERY_HEAP_DESC query={};query.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;query.Count=2;
    D3D12_HEAP_PROPERTIES hp={};hp.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC rd={};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=2*sizeof(UINT64);
    rd.Height=1;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    HRESULT hr=queue->GetTimestampFrequency(&f.timestampFrequency);
    if(SUCCEEDED(hr) && f.timestampFrequency)hr=gpu->CreateQueryHeap(&query,IID_PPV_ARGS(&f.timestamps));
    if(SUCCEEDED(hr) && f.timestampFrequency)hr=gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,
        D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&f.timestampReadback));
    if(FAILED(hr) || !f.timestampFrequency) {f.timestamps.Reset();f.timestampReadback.Reset();++gpuErrors;}
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
    D3D12_RESOURCE_BARRIER t = {}; t.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    t.Transition.pResource = r; t.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    t.Transition.StateBefore = a; t.Transition.StateAfter = b; return t;
}
bool uploadBuffer(UINT64 size, ComPtr<ID3D12Resource>& result) {
    D3D12_HEAP_PROPERTIES h = {}; h.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC d = {}; d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; d.Width = size;
    d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1; d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    TRY(gpu->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result)));
    return true;
}
bool heap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool visible, ComPtr<ID3D12DescriptorHeap>& result) {
    D3D12_DESCRIPTOR_HEAP_DESC d = {}; d.Type = type; d.NumDescriptors = count;
    d.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    TRY(gpu->CreateDescriptorHeap(&d, IID_PPV_ARGS(&result))); return true;
}
const char* shader = R"(
cbuffer State : register(b0) {
    float4 c[28];
    float4 fogColor;
    float4 pixel; // x/y: D3D9 sample-center correction
    uint4 colorStage; // op, arg1, arg2, unused
    uint4 alphaStage;
    uint4 control; // fog enabled, alpha test enabled, alpha func, alpha ref
#ifdef WATER_EFFECT
    float4 waterEffect; // strength, bounded presentation-clock phase
#endif
};
cbuffer ObjectState : register(b1) {
    float4 world[16];
    float4 modelExtra[3]; // c46 UV, c47 height fade, c48 outline alpha
    uint4 secondColor;
    uint4 secondAlpha;
};
struct Input { float4 pos:POSITION; float3 normal:NORMAL; float4 color:COLOR; float2 uv:TEXCOORD; float2 secondUv:TEXCOORD1; float3 weights:BLENDWEIGHT; };
struct Varying { float4 pos:SV_Position; float4 color:COLOR; float2 uv:TEXCOORD0; float2 uv1:TEXCOORD1; float fog:FOG;
#ifdef WATER_EFFECT
    float3 water:TEXCOORD2;
#endif
};
Varying vertexMain(Input v) {
    Varying o;
#ifdef WATER_EFFECT
    o.water=float3(v.pos.xz,saturate(v.color.a*(255.0/32.0)));
#endif
    float4 p=float4(v.pos.xyz,1);
    if(pixel.z==1) p=float4(v.pos.xyz+v.normal*c[3].xyz,c[1].w);
    o.pos=float4(dot(p,c[10]),dot(p,c[11]),dot(p,c[12]),dot(p,c[13]));
    o.pos.xy+=pixel.xy*o.pos.w;
    float z=dot(p,c[8]);
    o.fog=max(z*c[26].x+c[26].y,c[26].z);
    o.color=float4(c[24].xyz+v.color.bgr,v.color.a*clamp(z*c[27].x+c[27].y,c[0].w,c[1].w));
    if(pixel.z==3) o.color.rgb=v.color.bgr;
    o.uv=v.uv;
    o.uv1=0;
    if(pixel.z==1) {
        o.color.rgb=c[0].xyz;
        o.color.a=c[3].w*clamp(z*c[27].x+c[27].y,c[0].w,c[1].w);
        o.uv=float2(dot(v.normal,c[6].xyz),dot(v.normal,c[7].xyz));
    }
    if(pixel.z==2) {
        o.uv=float2(max(dot(v.normal,c[18].xyz),c[0].x),c[2].y);
        o.color=float4(c[1].xyz,clamp(z*c[27].x+c[27].y,c[0].w,c[1].w));
    }
    if(pixel.z>=4 && pixel.z<12) {
        precise float4 position=float4(dot(p,world[0]),dot(p,world[1]),dot(p,world[2]),dot(p,world[3]));
        precise float3 normal=float3(dot(v.normal,world[0].xyz),dot(v.normal,world[1].xyz),dot(v.normal,world[2].xyz));
        float height=clamp(position.y*modelExtra[1].x+modelExtra[1].y,c[0].w,c[1].w);
        if(pixel.z>=8) {
            precise float4 weights=float4(v.weights,c[1].x-((v.weights.x+v.weights.y)+v.weights.z));
            uint matrices=pixel.z>=10?4:2;
            if(matrices==2)weights.y=c[1].x-weights.x;
            position*=weights.x;normal*=weights.x;
            [unroll] for(uint j=1;j<4;++j)if(j<matrices) {
                uint k=j*4;
                position=mad(float4(dot(p,world[k]),dot(p,world[k+1]),dot(p,world[k+2]),dot(p,world[k+3])),weights[j],position);
                normal=mad(float3(dot(v.normal,world[k].xyz),dot(v.normal,world[k+1].xyz),dot(v.normal,world[k+2].xyz)),weights[j],normal);
            }
        }
        float depth=dot(position,c[8]);
        float fade=clamp(depth*c[27].x+c[27].y,c[0].w,c[1].w);
        if(pixel.z>=6)fade*=height;
        o.fog=max(depth*c[26].x+c[26].y,c[26].z);
        o.color=float4(v.color.bgr*c[4].xyz,v.color.a*c[4].w*fade);
        if(pixel.z>=6){o.color.rgb-=c[24].xyz;o.uv+=modelExtra[0].xy;}
        o.uv1=float2(max(dot(normal,c[18].xyz),c[0].x),c[2].y);
        if((uint(pixel.z)&1)!=0) {
            position+=float4(normal,c[0].w)*c[3];
            o.color=float4(c[5].xyz,fade*c[5].w);
            if(pixel.z>=6){o.color=float4(c[0].xyz,c[4].w*modelExtra[2].w*fade);o.uv=v.uv;}
        }
        o.pos=float4(dot(position,c[10]),dot(position,c[11]),dot(position,c[12]),dot(position,c[13]));
        o.pos.xy+=pixel.xy*o.pos.w;
    }
    if(pixel.z>=12 && pixel.z<=14) {
        precise float4 position=float4(dot(p,world[0]),dot(p,world[1]),dot(p,world[2]),dot(p,world[3]));
        if(pixel.z==14)position=p;
        float depth=dot(position,c[8]);
        float fade=clamp(depth*c[27].x+c[27].y,c[0].w,c[1].w);
        o.fog=max(depth*c[26].x+c[26].y,c[26].z);
        o.color=float4(v.color.bgr,v.color.a*fade);
        if(pixel.z==13){o.color=float4(world[12].xyz,world[12].w*fade);o.uv=v.uv+world[4].xy;o.uv1=v.secondUv+world[8].xy;}
        o.pos=float4(dot(position,c[10]),dot(position,c[11]),dot(position,c[12]),dot(position,c[13]));
        o.pos.xy+=pixel.xy*o.pos.w;
    }
    if(pixel.z>=15 && pixel.z<18) {
        float w=1/v.pos.w;
        float2 screen=(v.pos.xy-modelExtra[0].xy)/modelExtra[0].zw;
        o.pos=float4(float2(screen.x*2-1,1-screen.y*2)+pixel.xy,(v.pos.z-modelExtra[1].x)/modelExtra[1].y,1)*w;
        o.color=pixel.z==17?float4(1,1,1,1):v.color.bgra;
        o.uv=v.uv;o.fog=1;
    }
    if(pixel.z>=18) {
        o.pos=float4(dot(p,world[0]),dot(p,world[1]),dot(p,world[2]),dot(p,world[3]));
        o.pos.xy+=pixel.xy*o.pos.w;o.color=pixel.z==18?v.color.bgra:float4(1,1,1,1);o.uv=v.uv;o.fog=1;
    }
    // VS 1.1 diffuse outputs are clamped before rasterizer interpolation.
    o.color=saturate(o.color);
    return o;
}
Texture2D atlas:register(t0); SamplerState filtering:register(s0);
Texture2D detailAtlas:register(t1); SamplerState detailFiltering:register(s1);
float4 arg(uint mode,float4 tex,float4 diffuse,float4 current) {
    float4 factor=float4((colorStage.w>>16)&255,(colorStage.w>>8)&255,colorStage.w&255,(colorStage.w>>24)&255)/255.0;
    float4 v=(mode&15)==3?factor:(mode&15)==2?tex:(mode&15)==1?current:diffuse;
    if(mode&32) v=v.aaaa;
    if(mode&16) v=1-v;
    return v;
}
float4 combine(uint op,float4 a,float4 b,float textureAlpha) {
    if(op==2)return a; if(op==3)return b; if(op==4)return a*b;
    if(op==13)return lerp(b,a,textureAlpha); if(op==5)return 2*a*b; if(op==7)return a+b; return 0;
}
float4 pixelMain(Varying i):SV_Target {
    float4 t=atlas.Sample(filtering,i.uv), d=saturate(i.color);
    float4 result=combine(colorStage.x,arg(colorStage.y,t,d,d),arg(colorStage.z,t,d,d),t.a);
    result.a=combine(alphaStage.x,arg(alphaStage.y,t,d,d),arg(alphaStage.z,t,d,d),t.a).a;
    result=saturate(result);
    if(pixel.w!=0) {
        float4 detail=detailAtlas.Sample(detailFiltering,i.uv1);
        // Stage 1 distinguishes DIFFUSE (0) from CURRENT (1).
        float4 a=arg(secondColor.y,detail,d,result);
        float4 b=arg(secondColor.z,detail,d,result);
        float4 aa=arg(secondAlpha.y,detail,d,result);
        float4 ab=arg(secondAlpha.z,detail,d,result);
        result=float4(combine(secondColor.x,a,b,detail.a).rgb,secondAlpha.x==1?result.a:combine(secondAlpha.x,aa,ab,detail.a).a);
        result=saturate(result);
    }
#ifdef WATER_EFFECT
    // Uses world-space waves and the existing corner opacity
    // mask, not a fabricated depth/shore-distance buffer or reflection input.
    if(pixel.z==3 && waterEffect.x>0) {
        float strength=waterEffect.x*(control.x?saturate(i.fog):1);
        float wave=sin(dot(i.water.xy,float2(2.7,1.9))+waterEffect.y)*
                   cos(dot(i.water.xy,float2(-1.3,2.1))-waterEffect.y);
        float shore=4*i.water.z*(1-i.water.z);
        float foam=shore*smoothstep(.25,.85,wave)*.16*strength;
        result.rgb=saturate(lerp(result.rgb*(1+wave*.10*strength),float3(.72,.81,.77),foam));
        float smoothMask=smoothstep(0,1,i.water.z);
        result.a*=lerp(1,i.water.z>0?smoothMask/max(i.water.z,.00001):0,strength);
    }
#endif
    if(control.y) {
        // Compare against normalized ALPHAREF without rounding the fragment alpha.
        float a=result.a,r=control.w/255.0;
        bool alphaPass=control.z==8 || (control.z==2 && a<r) || (control.z==3 && a==r) ||
          (control.z==4 && a<=r) || (control.z==5 && a>r) || (control.z==6 && a!=r) || (control.z==7 && a>=r);
        if(!alphaPass)discard;
    }
    if(control.x)result.rgb=lerp(fogColor.rgb,result.rgb,saturate(i.fog));
    return result;
}
)";
struct Constants {
    float c[28][4], fog[4], pixel[4];
    DWORD color[4], alpha[4], control[4];
    float waterEffect[4];
};
struct ObjectConstants {float world[16][4],extra[3][4];DWORD color[4],alpha[4];};
bool compileShader(const char* entry,const char* target,ComPtr<ID3DBlob>& result,bool waterEffect=false) {
    const D3D_SHADER_MACRO effect[]={{"WATER_EFFECT","1"},{nullptr,nullptr}};ComPtr<ID3DBlob> error;
    HRESULT hr=D3DCompile(shader,strlen(shader),"terrain",waterEffect?effect:nullptr,nullptr,entry,target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_ENABLE_STRICTNESS,0,&result,&error);
    if(FAILED(hr) && error && logfn)logfn("native shader: %s",(char*)error->GetBufferPointer());
    return ok(hr,entry);
}
bool initialize() {
    if (root) return true;
    TRY(gpu->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle) return false;
    D3D12_DESCRIPTOR_RANGE ranges[4] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[0].NumDescriptors = 1;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; ranges[1].NumDescriptors = 1;
    ranges[2]=ranges[0];ranges[2].BaseShaderRegister=1;
    ranges[3]=ranges[1];ranges[3].BaseShaderRegister=1;
    D3D12_ROOT_PARAMETER params[6] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    for (int i=0;i<2;++i) {
        params[i+1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[i+1].DescriptorTable.NumDescriptorRanges = 1;
        params[i+1].DescriptorTable.pDescriptorRanges = &ranges[i];
        params[i+1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    params[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[3].Descriptor.ShaderRegister=1;
    for(int i=0;i<2;++i){params[4+i]=params[1+i];params[4+i].DescriptorTable.pDescriptorRanges=&ranges[2+i];}
    D3D12_ROOT_SIGNATURE_DESC rd = {}; rd.NumParameters = 6; rd.pParameters = params;
    rd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> blob, error;
    TRY(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
    TRY(gpu->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)));
    return compileShader("vertexMain","vs_5_0",vs) && compileShader("pixelMain","ps_5_0",ps);
}
bool begin(IDirect3DDevice9* d) {
    if (current) return true;
    CpuTimer timer(BeginCpu);
    if (!initialize()) return false;
    Frame& f = frames[frameIndex++ % 4]; wait(f.complete);
    collectGpuProfile(f);
    f.uploads.clear(); f.copies.clear(); f.srvSlots.clear(); f.samplerSlots.clear(); f.used = f.count = 0;
    for(auto& value:f.modes)value=0;
    f.color12.Reset(); f.depth12.Reset(); f.color9.Reset(); f.depth9.Reset(); f.bridge.Reset();
    if (!f.allocator) {
        TRY(gpu->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&f.allocator)));
        TRY(gpu->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, f.allocator.Get(), nullptr, IID_PPV_ARGS(&f.commands)));
        TRY(f.commands->Close());
        if (!uploadBuffer(UPLOAD_BYTES, f.upload) || !heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,MAX_DRAWS*2,true,f.srv) ||
            !heap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,MAX_DRAWS*2,true,f.sampler) ||
            !heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false,f.rtv) || !heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false,f.dsv)) return false;
        D3D12_RANGE noRead = {}; TRY(f.upload->Map(0, &noRead, (void**)&f.mapped));
    }
    TRY(f.allocator->Reset()); TRY(f.commands->Reset(f.allocator.Get(), nullptr));
    prepareGpuProfile(f);
    TRY(d->QueryInterface(IID_PPV_ARGS(&f.bridge)));
    TRY(d->GetRenderTarget(0, &f.color9));
    HRESULT depthTarget=d->GetDepthStencilSurface(&f.depth9);
    if(FAILED(depthTarget) && depthTarget!=D3DERR_NOTFOUND){f.commands->Close();return ok(depthTarget,"depth target");}
    HRESULT color = f.bridge->UnwrapUnderlyingResource(f.color9.Get(),queue.Get(),IID_PPV_ARGS(&f.color12));
    if (FAILED(color)) { f.commands->Close(); return ok(color,"color checkout"); }
    HRESULT depth = f.depth9?f.bridge->UnwrapUnderlyingResource(f.depth9.Get(),queue.Get(),IID_PPV_ARGS(&f.depth12)):S_OK;
    if (FAILED(depth)) {
        f.bridge->ReturnUnderlyingResource(f.color9.Get(),0,nullptr,nullptr);
        f.commands->Close(); return ok(depth,"depth checkout");
    }
    auto colorDesc=f.color12->GetDesc(),depthDesc=f.depth12?f.depth12->GetDesc():D3D12_RESOURCE_DESC{};
    if (f.depth12 && (colorDesc.SampleDesc.Count != depthDesc.SampleDesc.Count ||
        colorDesc.SampleDesc.Quality != depthDesc.SampleDesc.Quality ||
        depthDesc.Format != DXGI_FORMAT_D24_UNORM_S8_UINT)) {
        f.bridge->ReturnUnderlyingResource(f.color9.Get(),0,nullptr,nullptr);
        f.bridge->ReturnUnderlyingResource(f.depth9.Get(),0,nullptr,nullptr);
        f.commands->Close(); return false;
    }
    gpu->CreateRenderTargetView(f.color12.Get(),nullptr,f.rtv->GetCPUDescriptorHandleForHeapStart());
    if(f.timestamps)f.commands->EndQuery(f.timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
    if(f.depth12)gpu->CreateDepthStencilView(f.depth12.Get(),nullptr,f.dsv->GetCPUDescriptorHandleForHeapStart());
    D3D12_RESOURCE_BARRIER barriers[]={transition(f.color12.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_RENDER_TARGET),
        transition(f.depth12.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_DEPTH_WRITE)};
    f.commands->ResourceBarrier(f.depth12?2:1,barriers);
    // These bindings are immutable for this command list. Rebind after every
    // batch Reset, including fallback handoffs and upload-ring reuse.
    auto rtv=f.rtv->GetCPUDescriptorHandleForHeapStart(),dsv=f.dsv->GetCPUDescriptorHandleForHeapStart();
    f.commands->OMSetRenderTargets(1,&rtv,FALSE,f.depth12?&dsv:nullptr);
    f.commands->SetGraphicsRootSignature(root.Get());
    ID3D12DescriptorHeap* heaps[]={f.srv.Get(),f.sampler.Get()};f.commands->SetDescriptorHeaps(2,heaps);
    current = &f; return true;
}
DXGI_FORMAT textureFormat(D3DFORMAT f) {
    switch(f) {
    case D3DFMT_A8: return DXGI_FORMAT_R8_UNORM;
    case D3DFMT_L8: return DXGI_FORMAT_R8_UNORM;
    case D3DFMT_A8L8: return DXGI_FORMAT_R8G8_UNORM;
    case D3DFMT_A8R8G8B8: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3DFMT_X8R8G8B8: return DXGI_FORMAT_B8G8R8X8_UNORM;
    case D3DFMT_DXT1: return DXGI_FORMAT_BC1_UNORM;
    case D3DFMT_DXT3: return DXGI_FORMAT_BC2_UNORM;
    case D3DFMT_DXT5: return DXGI_FORMAT_BC3_UNORM;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}
bool describeTexture(IDirect3DTexture9* source,D3DSURFACE_DESC& desc,D3D12_RESOURCE_DESC& td) {
    TRY(source->GetLevelDesc(0, &desc));
    DXGI_FORMAT format = textureFormat(desc.Format);
    if (desc.Pool != D3DPOOL_MANAGED || format == DXGI_FORMAT_UNKNOWN ||
        (desc.Usage & D3DUSAGE_AUTOGENMIPMAP) || source->GetLOD()) return false;
    td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;td.Width=desc.Width;td.Height=desc.Height;
    td.DepthOrArraySize=1;td.MipLevels=(UINT16)source->GetLevelCount();td.Format=format;td.SampleDesc.Count=1;
    return true;
}
bool prepareTextureBudget(IDirect3DTexture9* first,IDirect3DTexture9* second) {
    if(textureBytes<=TEXTURE_BUDGET && textures.count(first) && (!second || textures.count(second)))return true;
    UINT64 needed=0,working=0;
    for(auto source:{first,second}) {
        if(!source)continue;
        auto found=textures.find(source);UINT64 size=0;
        if(found!=textures.end())size=found->second.allocationBytes;
        else {
            D3DSURFACE_DESC desc={};D3D12_RESOURCE_DESC td={};
            if(!describeTexture(source,desc,td))return false;
            size=gpu->GetResourceAllocationInfo(0,1,&td).SizeInBytes;
            if(size>TEXTURE_BUDGET)return false;
            needed+=size;
        }
        if(size>TEXTURE_BUDGET-working)return false;
        working+=size;
    }
    if(textureBytes<=TEXTURE_BUDGET && needed<=TEXTURE_BUDGET-textureBytes)return true;
    // No Frame or Texture pointers may be held here. Submit and finish earlier
    // draws before releasing their resources, preserving this draw's pair.
    terrain12Flush(Terrain12FlushReason::TextureBudget);if(failed)return false;
    if(fence){wait(serial);if(fence->GetCompletedValue()<serial)return false;}
    for(auto it=textures.begin();it!=textures.end();) {
        if(textureBytes<=TEXTURE_BUDGET && needed<=TEXTURE_BUDGET-textureBytes)break;
        if(it->first==first || it->first==second){++it;continue;}
        textureBytes-=it->second.allocationBytes;it=textures.erase(it);
    }
    return true;
}
bool getTexture(IDirect3DTexture9* source, Texture** out) {
    AcquireSRWLockExclusive(&dirtyLock);
    for(auto t:dirtyTextures) {auto it=textures.find(t);if(it!=textures.end())it->second.dirty=true;}
    dirtyTextures.clear();ReleaseSRWLockExclusive(&dirtyLock);
    D3DSURFACE_DESC desc = {};D3D12_RESOURCE_DESC td={};
    if(!describeTexture(source,desc,td))return false;
    auto found = textures.find(source);
    bool fresh = found == textures.end();
    if (fresh) {
        auto alloc = gpu->GetResourceAllocationInfo(0,1,&td);
        if (textureBytes>TEXTURE_BUDGET || alloc.SizeInBytes>TEXTURE_BUDGET-textureBytes) return false;
        D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        Texture t;
        TRY(gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&t.resource)));
        if(desc.Format==D3DFMT_A8)t.mapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(4,4,4,0);
        if(desc.Format==D3DFMT_L8)t.mapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0,0,0,5);
        if(desc.Format==D3DFMT_A8L8)t.mapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0,0,0,1);
        t.source = source; t.format = td.Format; t.levels = td.MipLevels;
        t.allocationBytes=alloc.SizeInBytes;textureBytes+=t.allocationBytes;
        found = textures.emplace(source,std::move(t)).first;
    }
    auto& t = found->second;
    if (t.dirty) {
        auto td = t.resource->GetDesc();
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(t.levels);
        std::vector<UINT> rows(t.levels); std::vector<UINT64> rowBytes(t.levels); UINT64 size = 0;
        gpu->GetCopyableFootprints(&td,0,t.levels,0,footprints.data(),rows.data(),rowBytes.data(),&size);
        ComPtr<ID3D12Resource> upload;
        if (!uploadBuffer(size,upload)) return false;
        BYTE* mapped = nullptr; D3D12_RANGE noRead = {}; TRY(upload->Map(0,&noRead,(void**)&mapped));
        for(UINT i=0;i<t.levels;++i) {
            D3DLOCKED_RECT pixels; HRESULT hr = source->LockRect(i,&pixels,nullptr,D3DLOCK_READONLY);
            if (FAILED(hr)) { upload->Unmap(0,nullptr); return ok(hr,"terrain texture LockRect"); }
            for(UINT y=0;y<rows[i];++y) memcpy(mapped+footprints[i].Offset+y*footprints[i].Footprint.RowPitch,
                (BYTE*)pixels.pBits+y*pixels.Pitch,(size_t)rowBytes[i]);
            source->UnlockRect(i);
        }
        upload->Unmap(0,nullptr);
        if (!fresh) { auto b=transition(t.resource.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST); current->commands->ResourceBarrier(1,&b); }
        for(UINT i=0;i<t.levels;++i) {
            D3D12_TEXTURE_COPY_LOCATION from={}, to={};
            from.pResource=upload.Get(); from.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; from.PlacedFootprint=footprints[i];
            to.pResource=t.resource.Get(); to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; to.SubresourceIndex=i;
            current->commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        }
        auto b=transition(t.resource.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        current->commands->ResourceBarrier(1,&b); current->uploads.push_back(upload); t.dirty=false;
        if(profileEnabled.load()){++textureCopies;textureCopyBytes+=size;}
#ifdef WORLDMOD_RENDER_TEST
        ++textureUploads;
#endif
    }
    *out=&t; return true;
}
UINT put(const void* bytes, UINT size, UINT alignment=16) {
    UINT start=(current->used+alignment-1)&~(alignment-1);
    if(start>UPLOAD_BYTES || size>UPLOAD_BYTES-start) return UINT_MAX;
    memcpy(current->mapped+start,bytes,size); current->used=start+size;
    if(profileEnabled.load())uploadBytes+=size;
    return start;
}
UINT cachedBuffer(IDirect3DResource9* r,UINT64 version,UINT sourceOffset,UINT size) {
    auto it=current->copies.upper_bound({r,version,sourceOffset});
    if(it==current->copies.begin())return UINT_MAX;
    --it;
    if(std::get<0>(it->first)!=r || std::get<1>(it->first)!=version)return UINT_MAX;
    UINT delta=sourceOffset-std::get<2>(it->first);
    if(delta>it->second.second || size>it->second.second-delta)return UINT_MAX;
    return it->second.first+delta;
}
UINT bufferData(IDirect3DResource9* r,UINT sourceOffset,UINT size,const BYTE* bytes,UINT64 version) {
    CopyKey key={r,version,sourceOffset};
    UINT cached=cachedBuffer(r,version,sourceOffset,size);
    if(cached!=UINT_MAX) {
        if(profileEnabled.load())++bufferHits;
#ifdef WORLDMOD_RENDER_TEST
        ++testWork.hits;
#endif
        return cached;
    }
    UINT offset=put(bytes+sourceOffset,size);
    if(offset!=UINT_MAX) {
        if(profileEnabled.load())++bufferCopies;
        // Remove only cache metadata for contained ranges. Their upload bytes
        // stay immutable for previously recorded draws. Remaining range ends
        // increase with their starts, so one predecessor lookup finds a cover.
        auto it=current->copies.lower_bound(key);
        while(it!=current->copies.end() && std::get<0>(it->first)==r &&
              std::get<1>(it->first)==version &&
              UINT64(std::get<2>(it->first))+it->second.second<=UINT64(sourceOffset)+size)
            it=current->copies.erase(it);
        current->copies[key]={offset,size};
    }
#ifdef WORLDMOD_RENDER_TEST
    if(offset!=UINT_MAX) {
        ++testWork.copies;
        if(r->GetType()==D3DRTYPE_INDEXBUFFER)testWork.indexBytes+=size;else testWork.vertexBytes+=size;
    }
#endif
    return offset;
}
UINT buffer(IDirect3DResource9* r,UINT sourceOffset,UINT size) {
    auto found=shadows.find(r);if(found==shadows.end() || found->second.bytes.empty())return UINT_MAX;
    return bufferData(r,sourceOffset,size,found->second.bytes.data(),found->second.version);
}
DWORD rs(IDirect3DDevice9* d,D3DRENDERSTATETYPE s) { DWORD v=0; d->GetRenderState(s,&v); return v; }
bool operation(UINT op) { return op==2 || op==3 || op==4 || op==5 || op==7 || op==13; }
}

static void retireBuffers();
void terrain12Init(ID3D12Device* d,ID3D12CommandQueue* q,TraceLogFn log) {
    terrain12Reset(); gpu=d; queue=q; logfn=log; failed=false; draws=0; for(auto& value:modeDraws)value=0;for(auto& value:recognized)value=0;for(auto& value:recorded)value=0;for(auto& row:rejected)for(auto& value:row)value=0;
    objectCaptureEnable(true);
    wchar_t flag[8]={};
    profileEnabled=GetEnvironmentVariableW(L"WORLDMOD_TERRAIN_PROFILE",flag,8)==1 && flag[0]==L'1';
}
void terrain12Reset() {
    objectCaptureReset();
    terrain12Flush(Terrain12FlushReason::Reset);
    if(fence)wait(serial);
    for(auto& f:frames)collectGpuProfile(f);
    current=nullptr; for(auto& f:frames) f=Frame{};
    textures.clear();whiteTexture.Reset();terrainReplacementReset(); pipelines.clear(); textureBytes=0;
    // The stock water IB is managed and filled only once. Keep its CPU bytes
    // across device Reset; drain releases first so dead identities cannot revive.
    retireBuffers();
    for(auto it=shadows.begin();it!=shadows.end();) {
        if(it->second.pool!=D3DPOOL_MANAGED || it->second.locked)it=shadows.erase(it);else ++it;
    }
#ifdef WORLDMOD_RENDER_TEST
    textureUploads=0; TEXTURE_BUDGET=96ull<<20;
    testWork={};
#endif
    AcquireSRWLockExclusive(&dirtyLock);dirtyTextures.clear();ReleaseSRWLockExclusive(&dirtyLock);
    root.Reset();vs.Reset();ps.Reset();waterVs.Reset();waterPs.Reset();waterTime=0;fence.Reset();gpu.Reset();queue.Reset();serial=0;
    draws=0; for(auto& value:modeDraws)value=0;for(auto& value:recognized)value=0;for(auto& value:recorded)value=0;for(auto& row:rejected)for(auto& value:row)value=0;
    for(auto& value:reportedBufferRejects)value=0;
    for(auto& counts:callCounts){counts.attempts=0;counts.native=0;for(auto& value:counts.topology)value=0;}
    {std::lock_guard<std::mutex> guard(fallbackMutex);fallbackStates.clear();fallbackOverflow=0;}
    if(eventHandle)CloseHandle(eventHandle);eventHandle=nullptr;
}
bool trackedBuffer(IDirect3DResource9* r) {
    for(int i=0;i<4;++i)if(r==terrainBuffers[i])return true;
    for(int i=0;i<2;++i)if(r==waterBuffers[i])return true;
    return false;
}
void terrain12ForgetBuffer(IDirect3DResource9* r) {
    // COM releases can originate on the model loader thread. Retire identities
    // here; only the render thread may mutate shadows or an active upload batch.
    AcquireSRWLockExclusive(&retiredLock);retiredBuffers.insert(r);ReleaseSRWLockExclusive(&retiredLock);
}
static void retireBuffers() {
    std::set<IDirect3DResource9*> retired;
    AcquireSRWLockExclusive(&retiredLock);retired.swap(retiredBuffers);ReleaseSRWLockExclusive(&retiredLock);
    for(auto r:retired)shadows.erase(r);
    if(current && !retired.empty())for(auto it=current->copies.begin();it!=current->copies.end();) {
        if(retired.count(std::get<0>(it->first)))it=current->copies.erase(it);else ++it;
    }
}
void* terrain12Lock(IDirect3DResource9* r,UINT offset,UINT size,void* data,DWORD flags) {
    if(!trackedBuffer(r))return data;
    if(!gpu || failed)return data;
    retireBuffers();
    UINT length=0,usage=0;D3DPOOL pool=D3DPOOL_DEFAULT;
    if(r->GetType()==D3DRTYPE_VERTEXBUFFER) {D3DVERTEXBUFFER_DESC d={};((IDirect3DVertexBuffer9*)r)->GetDesc(&d);length=d.Size;usage=d.Usage;pool=d.Pool;}
    else {D3DINDEXBUFFER_DESC d={};((IDirect3DIndexBuffer9*)r)->GetDesc(&d);length=d.Size;usage=d.Usage;pool=d.Pool;}
    if(!(usage&D3DUSAGE_WRITEONLY))return data;
    if(offset>length || size>length-offset || length>MAX_BUFFER_BYTES)return data;
    for(auto it=shadows.begin();it!=shadows.end();) {
        bool active=trackedBuffer(it->first);
        if(!active)it=shadows.erase(it);else ++it;
    }
    auto& s=shadows[r];s.bytes.resize(length);s.locked=data;s.pool=pool;s.offset=offset;s.size=size?size:length-offset;
    if(flags&D3DLOCK_DISCARD)s.initialized.clear();
    return s.bytes.data()+offset;
}
void terrain12Unlock(IDirect3DResource9* r) {
    if(!trackedBuffer(r))return;
    auto it=shadows.find(r);if(it==shadows.end() || !it->second.locked)return;
    auto& s=it->second;memcpy(s.locked,s.bytes.data()+s.offset,s.size);s.locked=nullptr;++s.version;
    UINT first=s.offset,last=s.offset+s.size;
    auto range=s.initialized.lower_bound(first);
    if(range!=s.initialized.begin()) {auto previous=std::prev(range);if(previous->second>=first)range=previous;}
    while(range!=s.initialized.end() && range->first<=last) {
        first=std::min(first,range->first);last=std::max(last,range->second);range=s.initialized.erase(range);
    }
    s.initialized[first]=last;
}
void terrain12TextureChanged(IDirect3DBaseTexture9* t) {
    AcquireSRWLockExclusive(&dirtyLock);dirtyTextures.insert(t);ReleaseSRWLockExclusive(&dirtyLock);
}
void terrain12SetEnabled(bool value){enabled.store(value);}
void terrain12ConfigureWater(int percent){waterStrength=float(std::max(0,std::min(100,percent)))/100;}
void terrain12AdvanceWater(double seconds) {
    if(waterStrength.load()<=0 || !std::isfinite(seconds) || seconds<=0)return;
    waterTime=std::fmod(waterTime.load()+float(std::min(seconds,.1)),6.28318530718f);
}
bool terrain12Active(){return enabled.load() && !failed.load() && draws.load()!=0;}
void terrain12RecordDraw(IDirect3DDevice9* d,UINT kind,D3DPRIMITIVETYPE type,bool native,UINT upStride) {
    if(!gpu || kind>=4)return;
    ++callCounts[kind].attempts;if(native)++callCounts[kind].native;
    ++callCounts[kind].topology[type>=D3DPT_POINTLIST && type<=D3DPT_TRIANGLEFAN?UINT(type):0];
    if(native || !profileEnabled.load())return;
    // Bounded, opt-in state census; no raw shader/texture pointers retained.
    FallbackKey key={kind,UINT(type),0,upStride,0,0,0,rs(d,D3DRS_LIGHTING),rs(d,D3DRS_FOGENABLE),0};
    d->GetFVF(&key[2]);
    ComPtr<IDirect3DVertexShader9> vs9;ComPtr<IDirect3DPixelShader9> ps9;
    d->GetVertexShader(&vs9);d->GetPixelShader(&ps9);key[9]=ps9?1:0;
    if(vs9){key[4]=2;
        {std::lock_guard<std::mutex> guard(modelShaderMutex);auto it=modelShaders.find(vs9.Get());if(it!=modelShaders.end()){key[4]=1;key[10]=it->second+1;}}
        for(int i=0;i<3;++i)if(terrainShaders[i]==vs9.Get()){key[4]=1;key[10]=i+1;}
        if(waterShader==vs9.Get()){key[4]=1;key[10]=4;}
        for(int i=0;i<2;++i)if(objectShaders[i]==vs9.Get()){key[4]=1;key[10]=i+5;}
    }
    if(kind<2){ComPtr<IDirect3DVertexBuffer9> buffer;UINT offset=0,stride=0;d->GetStreamSource(0,&buffer,&offset,&stride);key[3]=stride;}
    ComPtr<IDirect3DBaseTexture9> texture;d->GetTexture(0,&texture);
    if(texture && texture->GetType()==D3DRTYPE_TEXTURE){D3DSURFACE_DESC desc={};if(SUCCEEDED(((IDirect3DTexture9*)texture.Get())->GetLevelDesc(0,&desc))){key[5]=desc.Format;key[6]=desc.Pool;}}
    std::lock_guard<std::mutex> guard(fallbackMutex);
    auto it=fallbackStates.find(key);if(it!=fallbackStates.end())++it->second;
    else if(fallbackStates.size()<128)fallbackStates.emplace(key,1);else ++fallbackOverflow;
}
void terrain12Status(FILE* out) {
    terrainReplacementStatus(out);
    fprintf(out,"Native water effect: strength=%.3f phase=%.6f clock=presentation max_step_seconds=0.1\n",waterStrength.load(),waterTime.load());
    fprintf(out,"native_submitted_draws=%llu base=%llu outline=%llu lighting=%llu enabled=%d failed=%d\n",
        draws.load(),modeDraws[0].load(),modeDraws[1].load(),modeDraws[2].load(),enabled.load(),failed.load());
    fprintf(out,"Native water: submitted_draws=%llu\n",modeDraws[3].load());
    fprintf(out,"Native objects: base=%llu outline=%llu snapshot_bytes=%llu\n",modeDraws[4].load(),modeDraws[5].load(),objectCaptureBytes());
    fprintf(out,"Native models: rigid=%llu/%llu blend2=%llu/%llu blend4=%llu/%llu\n",modeDraws[6].load(),modeDraws[7].load(),modeDraws[8].load(),modeDraws[9].load(),modeDraws[10].load(),modeDraws[11].load());
    fprintf(out,"Native additional models: unlit=%llu dual_uv=%llu\n",modeDraws[12].load(),modeDraws[13].load());
    fprintf(out,"Native screen: textured=%llu colored=%llu texture_only=%llu shared_unlit=%llu\n",modeDraws[15].load(),modeDraws[16].load(),modeDraws[17].load(),modeDraws[14].load());
    fprintf(out,"Native fixed unlit: color_uv=%llu uv=%llu position=%llu\n",modeDraws[18].load(),modeDraws[19].load(),modeDraws[20].load());
    for(int mode=0;mode<21;++mode) {
        fprintf(out,"Native coverage mode=%d recognized=%llu recorded=%llu submitted=%llu reject_layout=%llu reject_buffer=%llu reject_state=%llu reject_interop=%llu reject_texture=%llu reject_pipeline=%llu reject_upload=%llu\n",
            mode,recognized[mode].load(),recorded[mode].load(),modeDraws[mode].load(),rejected[mode][0].load(),rejected[mode][1].load(),rejected[mode][2].load(),rejected[mode][3].load(),rejected[mode][4].load(),rejected[mode][5].load(),rejected[mode][6].load());
    }
    const char* names[]={"DP","DIP","DPUP","DIPUP"};
    for(UINT kind=0;kind<4;++kind){auto& counts=callCounts[kind];
        fprintf(out,"Native API %s attempts=%llu native=%llu topology_invalid=%llu points=%llu lines=%llu line_strip=%llu triangles=%llu triangle_strip=%llu triangle_fan=%llu\n",names[kind],counts.attempts.load(),counts.native.load(),counts.topology[0].load(),counts.topology[1].load(),counts.topology[2].load(),counts.topology[3].load(),counts.topology[4].load(),counts.topology[5].load(),counts.topology[6].load());}
    {std::lock_guard<std::mutex> guard(fallbackMutex);
        fprintf(out,"Native fallback census enabled=%d entries=%zu overflow=%llu shader_class=0:none,1:known,2:unknown\n",profileEnabled.load(),fallbackStates.size(),fallbackOverflow);
        for(auto& row:fallbackStates){const auto& k=row.first;fprintf(out,"Native fallback api=%s type=%lu fvf=%lx stride=%lu shader_class=%lu texture_format=%lu texture_pool=%lu lighting=%lu fog=%lu pixel_shader=%lu shader_mode=%d calls=%llu\n",names[k[0]],k[1],k[2],k[3],k[4],k[5],k[6],k[7],k[8],k[9],int(k[10])-1,row.second);}}
    LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
    fprintf(out,"Native CPU profile: enabled=%d cumulative=1 draw_includes_stages=1 begin_includes_wait=1 submit_includes_stages=1\n",profileEnabled.load());
    for(int i=0;i<CpuStages;++i)fprintf(out,"Native CPU %s: calls=%llu total_ms=%.6f\n",cpuNames[i],
        cpuCounters[i].calls.load(),1000.0*cpuCounters[i].ticks.load()/frequency.QuadPart);
    fprintf(out,"Native flush profile: enabled=%d cumulative=1 scope=active_batches\n",profileEnabled.load());
    for(size_t i=0;i<size_t(Terrain12FlushReason::Count);++i)fprintf(out,"Native flush %s: batches=%llu draws=%llu empty=%llu\n",
        flushNames[i],flushCounters[i].batches.load(),flushCounters[i].draws.load(),flushCounters[i].empty.load());
    {std::lock_guard<std::mutex> guard(gpuProfileMutex);
        fprintf(out,"Native GPU profile: enabled=%d cumulative=1 batches=%llu total_ms=%.6f pending=%llu errors=%llu scope=native_batches\n",
            profileEnabled.load(),gpuBatches,gpuNanoseconds/1e6,gpuPending.load(),gpuErrors.load());}
    fprintf(out,"Native upload profile: enabled=%d cumulative=1 bytes=%llu buffer_copies=%llu buffer_hits=%llu texture_copies=%llu texture_bytes=%llu\n",
        profileEnabled.load(),uploadBytes.load(),bufferCopies.load(),bufferHits.load(),textureCopies.load(),textureCopyBytes.load());
}

struct UserDraw { const BYTE* vertices;const WORD* indices;UINT stride; };
static bool drawNative(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minimum,UINT vertices,UINT start,UINT primitives,UINT path,bool indexed=true,const UserDraw* user=nullptr) {
    const bool water=path==1,object=path>=2,lines=type==D3DPT_LINELIST || type==D3DPT_LINESTRIP;
    if(!gpu || failed || !enabled.load() || (!lines && type!=D3DPT_TRIANGLELIST && type!=D3DPT_TRIANGLESTRIP && type!=D3DPT_TRIANGLEFAN) || base<0)return false;
    if(path<3 && type!=(water?D3DPT_TRIANGLESTRIP:D3DPT_TRIANGLELIST) && !(path==2 && type==D3DPT_TRIANGLESTRIP))return false;
    retireBuffers();
    UINT64 indexCount64=lines?(type==D3DPT_LINELIST?UINT64(primitives)*2:UINT64(primitives)+1):type!=D3DPT_TRIANGLELIST?UINT64(primitives)+2:UINT64(primitives)*3;
    if(!primitives || !vertices || indexCount64>UINT_MAX/2 || minimum>INT_MAX)return false;
    UINT indexCount=UINT(indexCount64);
    CpuTimer total(DrawCpu), timer(StateCpu);
    ComPtr<IDirect3DVertexShader9> shader9;d->GetVertexShader(&shader9);
    DrawCoverage coverage;int& mode=coverage.mode;
    if(water || object) {
        if(water && shader9 && shader9.Get()==waterShader)mode=3;
        if(path==2)for(int i=0;i<2;++i)if(shader9 && shader9.Get()==objectShaders[i])mode=4+i;
        if(path==3){std::lock_guard<std::mutex> guard(modelShaderMutex);auto it=modelShaders.find(shader9.Get());if(it!=modelShaders.end())mode=it->second;}
        if(path==3 && !shader9){DWORD fvf=0;if(SUCCEEDED(d->GetFVF(&fvf)))mode=fvf==0x144?15:fvf==0x44?16:fvf==0x104?17:fvf==0x142?18:fvf==0x102?19:fvf==0x2?20:-1;}
        if(mode<0)return false;
        if(mode<15){
        ComPtr<IDirect3DVertexDeclaration9> decl;UINT count=0;
        if(FAILED(d->GetVertexDeclaration(&decl)) || !decl || FAILED(decl->GetDeclaration(nullptr,&count)) || count!=(mode>=12?4u:mode>=8?6u:object?5u:4u))return false;
        D3DVERTEXELEMENT9 actual[6];
        const D3DVERTEXELEMENT9 expected[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
            {0,12,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},{0,16,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
        const D3DVERTEXELEMENT9 model[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
            {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},{0,24,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},
            {1,0,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},D3DDECL_END()};
        const WORD weightBytes=mode>=10?12:4;
        const D3DVERTEXELEMENT9 skinned[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
            {0,12,BYTE(mode>=10?D3DDECLTYPE_FLOAT3:D3DDECLTYPE_FLOAT1),0,D3DDECLUSAGE_BLENDWEIGHT,0},
            {0,WORD(12+weightBytes),D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},
            {0,WORD(24+weightBytes),D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},
            {1,0,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},D3DDECL_END()};
        const D3DVERTEXELEMENT9 dualUv[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
            {0,12,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},{0,20,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,1},D3DDECL_END()};
        if(FAILED(decl->GetDeclaration(actual,&count)) || memcmp(actual,mode==13?dualUv:mode==12 || mode==14?expected:mode>=8?skinned:object?model:expected,count*sizeof actual[0]))return false;
        }
    } else for(int i=0;i<3;++i)if(shader9 && shader9.Get()==terrainShaders[i])mode=i;
    if(mode<0)return false;
    if(user && mode<15)return false; // Start with the six verified FVF layouts.
    coverage.phase=StateReject;
    ComPtr<IDirect3DPixelShader9> pixelShader;d->GetPixelShader(&pixelShader);if(pixelShader)return false;
    const bool ui=mode>=15 && mode<18,fixed=mode>=18,fan=type==D3DPT_TRIANGLEFAN;
    if(lines && (!ui || rs(d,D3DRS_ANTIALIASEDLINEENABLE)))return false;
    if(fixed && (rs(d,D3DRS_LIGHTING) || rs(d,D3DRS_VERTEXBLEND) || rs(d,D3DRS_INDEXEDVERTEXBLENDENABLE)))return false;
    if(rs(d,D3DRS_SHADEMODE)!=D3DSHADE_GOURAUD)return false;
    if((ui || fixed) && (rs(d,D3DRS_FOGENABLE) || rs(d,D3DRS_SPECULARENABLE)))return false;
    ComPtr<IDirect3DBaseTexture9> tex;d->GetTexture(0,&tex);const bool untextured=!tex;
    if((!tex && !ui && mode!=20) || (tex && tex->GetType()!=D3DRTYPE_TEXTURE))return false;
    coverage.phase=BufferReject;
    // Only diagnose the first instance of each buffer reason/mode after Reset.
    auto rejectBuffer=[&](UINT reason,const char* detail,UINT64 first=0,UINT64 last=0) {
        if(profileEnabled.load() && logfn && !(reportedBufferRejects[mode].fetch_or(1u<<reason)&(1u<<reason)))
            logfn("native buffer reject: mode=%d detail=%s type=%u base=%d minimum=%u vertices=%u start=%u primitives=%u first=%llu last=%llu",
                mode,detail,UINT(type),base,minimum,vertices,start,primitives,first,last);
        return false;
    };
    ComPtr<IDirect3DVertexBuffer9> vb[2];UINT offset[2]={},stride[2]={};
    const int streams=water || mode>=12?1:2;
    if(user){UINT frequency=0;if(FAILED(d->GetStreamSourceFreq(0,&frequency)) || frequency!=1)return false;}
    if(user)stride[0]=user->stride;else for(int i=0;i<streams;++i)d->GetStreamSource(i,&vb[i],&offset[i],&stride[i]);
    const UINT weightBytes=mode>=12?0u:mode>=10?12u:mode>=8?4u:0u;
    if(stride[0]!=(mode==18?24u:mode==19?20u:mode==20?12u:mode==15?28u:mode==16?20u:mode==17?24u:mode==13?28u:mode==12 || mode==14?24u:object?32u+weightBytes:24u) || (streams==2 && stride[1]!=(object?4u:12u)))return rejectBuffer(0,"stride",stride[0],stride[1]);
    ComPtr<IDirect3DIndexBuffer9> ib;
    if(indexed && !user){d->GetIndices(&ib);if(!ib)return false;D3DINDEXBUFFER_DESC id={};ib->GetDesc(&id);if(id.Format!=D3DFMT_INDEX16)return false;}
    if(water && (vb[0].Get()!=waterBuffers[0] || ib.Get()!=waterBuffers[1]))return false;
    std::shared_ptr<const ObjectSnapshot> snapshots[3];
    if(object && !user) {
        for(int i=0;i<streams;++i){UINT frequency=0;if(FAILED(d->GetStreamSourceFreq(i,&frequency)) || frequency!=1)return false;snapshots[i]=objectCaptureRead(vb[i].Get());}
        if(indexed)snapshots[2]=objectCaptureRead(ib.Get());
        for(int i=0;i<streams;++i)if(!snapshots[i])return rejectBuffer(1,"vertex_snapshot",i);
        if(indexed && !snapshots[2])return rejectBuffer(2,"index_snapshot");
    }
    auto initialized=[&](IDirect3DResource9* resource,UINT64 first,UINT64 last) {
        auto it=shadows.find(resource);if(it==shadows.end() || it->second.locked || last>it->second.bytes.size())return false;
        auto range=it->second.initialized.upper_bound(UINT(first));
        if(range==it->second.initialized.begin())return false;
        --range;return range->second>=last;
    };
    for(int i=0;i<streams;++i) {
        if(user)continue;
        if(object) {
            if(!snapshots[i]->contains(UINT64(offset[i])+(UINT64(base)+minimum)*stride[i],UINT64(offset[i])+(UINT64(base)+minimum+vertices)*stride[i]))return rejectBuffer(3,"vertex_range",UINT64(offset[i])+(UINT64(base)+minimum)*stride[i],UINT64(offset[i])+(UINT64(base)+minimum+vertices)*stride[i]);
            continue;
        }
        auto it=shadows.find(vb[i].Get());
        if(it==shadows.end() || UINT64(offset[i])+(UINT64(base)+minimum+vertices)*stride[i]>it->second.bytes.size())return false;
        if(!initialized(vb[i].Get(),UINT64(offset[i])+(UINT64(base)+minimum)*stride[i],UINT64(offset[i])+(UINT64(base)+minimum+vertices)*stride[i]))return false;
    }
    auto is=shadows.find(ib.Get());
    if(indexed && !user && object) {if(!snapshots[2]->contains(UINT64(start)*2,(UINT64(start)+indexCount)*2))return rejectBuffer(4,"index_range",UINT64(start)*2,(UINT64(start)+indexCount)*2);}
    else if(indexed && !user && (is==shadows.end() || !initialized(ib.Get(),UINT64(start)*2,(UINT64(start)+indexCount)*2)))return false;
    if(water || object) {
        if(indexed) {const WORD* indices=user?user->indices:(const WORD*)((object?snapshots[2]->bytes.data():is->second.bytes.data())+start*2);
        for(UINT i=0;i<indexCount;++i)if(indices[i]<minimum || UINT64(indices[i])>=UINT64(minimum)+vertices)return rejectBuffer(5,"index_value",i,indices[i]);}
        DWORD coordinate=0,transform=0;
        if(FAILED(d->GetTextureStageState(0,D3DTSS_TEXCOORDINDEX,&coordinate)) || coordinate!=0 ||
           FAILED(d->GetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,&transform)) || transform!=D3DTTFF_DISABLE)return false;
    }
    if(ui){
        const BYTE* data=user?user->vertices:snapshots[0]->bytes.data()+offset[0]+(UINT64(base)+minimum)*stride[0];
        for(UINT v=0;v<vertices;++v){float position[4];memcpy(position,data+UINT64(v)*stride[0],16);
            for(float x:position)if(!std::isfinite(x))return rejectBuffer(6,"nonfinite_position",v);if(position[3]<=0)return rejectBuffer(7,"nonpositive_rhw",v);}
    }
    std::vector<WORD> fanIndices;
    if(fan){
        if(!indexed && vertices>65536)return false;
        if(UINT64(primitives)*6>MAX_BUFFER_BYTES)return false;
        fanIndices.resize(size_t(primitives)*3);
        const WORD* source=indexed?(user?user->indices:(const WORD*)(snapshots[2]->bytes.data()+start*2)):nullptr;
        for(UINT i=0;i<primitives;++i){fanIndices[i*3]=source?source[0]:0;fanIndices[i*3+1]=source?source[i+1]:WORD(i+1);fanIndices[i*3+2]=source?source[i+2]:WORD(i+2);}
    }
    coverage.phase=StateReject;
    Constants c={};d->GetVertexShaderConstantF(0,&c.c[0][0],28);
    if(water){c.waterEffect[0]=waterStrength.load();c.waterEffect[1]=waterTime.load();}
    ObjectConstants oc={};if(object && !ui && !fixed && FAILED(d->GetVertexShaderConstantF(30,&oc.world[0][0],mode>=6?19:4)))return false;
    if(fixed){
        D3DMATRIX world,view,projection;if(FAILED(d->GetTransform(D3DTS_WORLD,&world)) || FAILED(d->GetTransform(D3DTS_VIEW,&view)) || FAILED(d->GetTransform(D3DTS_PROJECTION,&projection)))return false;
        auto multiply=[](const D3DMATRIX& a,const D3DMATRIX& b){D3DMATRIX result={};for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)result.m[i][j]+=a.m[i][k]*b.m[k][j];return result;};
        auto combined=multiply(multiply(world,view),projection);for(int i=0;i<4;++i)for(int j=0;j<4;++j)oc.world[i][j]=combined.m[j][i];
    }
    c.color[3]=rs(d,D3DRS_TEXTUREFACTOR);
    for(UINT i=0;i<3;++i) {d->GetTextureStageState(0,(D3DTEXTURESTAGESTATETYPE)(1+i),&c.color[i]);d->GetTextureStageState(0,(D3DTEXTURESTAGESTATETYPE)(4+i),&c.alpha[i]);}
    DWORD stage1=0;d->GetTextureStageState(1,D3DTSS_COLOROP,&stage1);
    const bool dual=object && stage1!=D3DTOP_DISABLE;
    if((ui || fixed) && dual)return false;
    if(object)for(UINT stage=0;stage<(dual?2u:1u);++stage){DWORD result=0;if(FAILED(d->GetTextureStageState(stage,D3DTSS_RESULTARG,&result)) || result!=D3DTA_CURRENT)return false;}
    if((!object && stage1!=D3DTOP_DISABLE) || !operation(c.color[0]) || !operation(c.alpha[0]))return false;
    if(untextured && (c.color[0]!=D3DTOP_MODULATE || c.alpha[0]!=D3DTOP_MODULATE ||
        c.color[1]!=D3DTA_TEXTURE || c.alpha[1]!=D3DTA_TEXTURE || (c.color[2]!=D3DTA_CURRENT && c.color[2]!=D3DTA_DIFFUSE) ||
        (c.alpha[2]!=D3DTA_CURRENT && c.alpha[2]!=D3DTA_DIFFUSE)))return false;
    for(int i=1;i<3;++i)if((c.color[i]&15)>3 || (c.alpha[i]&15)>3)return false;
    ComPtr<IDirect3DBaseTexture9> secondTexture;DWORD secondSampler[12]={};
    if(dual) {
        DWORD stage2=0,coordinate=0,transform=0;
        if(FAILED(d->GetTexture(1,&secondTexture)) || !secondTexture || secondTexture->GetType()!=D3DRTYPE_TEXTURE ||
           FAILED(d->GetTextureStageState(2,D3DTSS_COLOROP,&stage2)) || stage2!=D3DTOP_DISABLE ||
           FAILED(d->GetTextureStageState(1,D3DTSS_TEXCOORDINDEX,&coordinate)) || coordinate!=1 ||
           FAILED(d->GetTextureStageState(1,D3DTSS_TEXTURETRANSFORMFLAGS,&transform)) || transform!=D3DTTFF_DISABLE)return false;
        for(UINT i=0;i<3;++i){d->GetTextureStageState(1,(D3DTEXTURESTAGESTATETYPE)(1+i),&oc.color[i]);d->GetTextureStageState(1,(D3DTEXTURESTAGESTATETYPE)(4+i),&oc.alpha[i]);}
        if(!operation(oc.color[0]) || (oc.alpha[0]!=D3DTOP_DISABLE && !operation(oc.alpha[0])))return false;
        for(int i=1;i<3;++i)if((oc.color[i]&15)>3 || (oc.alpha[i]&15)>3)return false;
        for(UINT i=1;i<=11;++i)d->GetSamplerState(1,(D3DSAMPLERSTATETYPE)i,&secondSampler[i]);
    }
    if(rs(d,D3DRS_FILLMODE)!=D3DFILL_SOLID || rs(d,D3DRS_ZENABLE)==D3DZB_USEW ||
       rs(d,D3DRS_WRAP0) || (dual && rs(d,D3DRS_WRAP1)) || rs(d,D3DRS_CLIPPLANEENABLE) || rs(d,D3DRS_STENCILENABLE) || rs(d,D3DRS_SRGBWRITEENABLE) || (!ui && rs(d,D3DRS_SCISSORTESTENABLE)) ||
       rs(d,D3DRS_FOGTABLEMODE)!=D3DFOG_NONE || rs(d,D3DRS_DEPTHBIAS) || rs(d,D3DRS_SLOPESCALEDEPTHBIAS))return false;
    c.control[0]=rs(d,D3DRS_FOGENABLE);c.control[1]=rs(d,D3DRS_ALPHATESTENABLE);
    c.control[2]=rs(d,D3DRS_ALPHAFUNC);c.control[3]=rs(d,D3DRS_ALPHAREF);
    DWORD fog=rs(d,D3DRS_FOGCOLOR);for(int i=0;i<3;++i)c.fog[i]=float((fog>>(16-i*8))&255)/255;
    D3DVIEWPORT9 vp={};d->GetViewport(&vp);c.pixel[0]=1.0f/vp.Width;c.pixel[1]=-1.0f/vp.Height;c.pixel[2]=(float)mode;
    c.pixel[3]=dual?1.0f:0.0f;
    if(ui){if(!vp.Width || !vp.Height || vp.MaxZ<=vp.MinZ)return false;
        oc.extra[0][0]=float(vp.X);oc.extra[0][1]=float(vp.Y);oc.extra[0][2]=float(vp.Width);oc.extra[0][3]=float(vp.Height);
        oc.extra[1][0]=vp.MinZ;oc.extra[1][1]=vp.MaxZ-vp.MinZ;}
    if(untextured){
        if(!whiteTexture){ComPtr<IDirect3DTexture9> white;D3DLOCKED_RECT rect;
            if(FAILED(d->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&white,nullptr)) || FAILED(white->LockRect(0,&rect,nullptr,0)))return false;
            *(DWORD*)rect.pBits=0xffffffff;if(FAILED(white->UnlockRect(0)))return false;whiteTexture=white;}
        tex=whiteTexture;
    }
    DWORD samp[12]={};for(UINT i=1;i<=11;++i)d->GetSamplerState(0,(D3DSAMPLERSTATETYPE)i,&samp[i]);
    if(samp[D3DSAMP_SRGBTEXTURE] || samp[D3DSAMP_MINFILTER]>D3DTEXF_ANISOTROPIC || samp[D3DSAMP_MAGFILTER]>D3DTEXF_ANISOTROPIC)return false;
    // Mixed point/linear minification and magnification differ from the stock
    // 9on12 path on NVIDIA. Preserve its sampling through the existing fallback.
    if((samp[D3DSAMP_MINFILTER]==D3DTEXF_POINT && samp[D3DSAMP_MAGFILTER]==D3DTEXF_LINEAR) ||
       (samp[D3DSAMP_MINFILTER]==D3DTEXF_LINEAR && samp[D3DSAMP_MAGFILTER]==D3DTEXF_POINT))return false;
    if(dual && (secondSampler[11] || secondSampler[5]>3 || secondSampler[6]>3 ||
        (secondSampler[6]==1 && secondSampler[5]==2) || (secondSampler[6]==2 && secondSampler[5]==1)))return false;
    timer.next(TextureCpu);coverage.phase=TextureReject;
    ComPtr<IDirect3DBaseTexture9> stockTexture;ComPtr<IDirect3DTexture9> replacement;
    if(mode==0 && terrainReplacementFind((IDirect3DTexture9*)tex.Get(),&replacement)){stockTexture=tex;tex=replacement;}
    if(!prepareTextureBudget((IDirect3DTexture9*)tex.Get(),dual && secondTexture.Get()!=tex.Get()?(IDirect3DTexture9*)secondTexture.Get():nullptr)){
        if(!replacement)return false;
        tex=stockTexture;
        if(!prepareTextureBudget((IDirect3DTexture9*)tex.Get(),dual && secondTexture.Get()!=tex.Get()?(IDirect3DTexture9*)secondTexture.Get():nullptr))return false;
    }
    timer.finish();
    coverage.phase=InteropReject;
    if(!begin(d))return false;
    // Bound both descriptor and CPU upload consumption; flush before wrapping.
    // Upload only the declared draw ranges, not the unused buffer prefixes.
    // Rebase both vertex views and the index draw to preserve D3D9 addressing.
    UINT sourceOffsets[2]={offset[0]+(base+minimum)*stride[0],offset[1]+(base+minimum)*stride[1]};
    UINT spans[2]={vertices*stride[0],vertices*stride[1]};
    UINT indexOffset=start*2,indexSpan=fan?UINT(fanIndices.size()*2):indexed?indexCount*2:0;
    UINT viewOffsets[2]={},drawStart=0;INT drawBase=indexed?-INT(minimum):0;
#ifdef WORLDMOD_RENDER_TEST
    // Historical prefix-upload reference, available only to the offline A/B fixture.
    if(testPrefixUpload && !water && !object) {
        for(int i=0;i<2;++i) {spans[i]+=sourceOffsets[i];sourceOffsets[i]=0;viewOffsets[i]=offset[i];}
        indexSpan+=indexOffset;indexOffset=0;drawStart=start;drawBase=base;
    }
#endif
    // Cached buffer versions consume no new upload space. Counting them again
    // can flush a large, reusable span on every draw and defeat the copy cache.
    // Avoid extra cache lookups when even the worst-case allocation fits.
    const UINT extra=object?sizeof(oc)+255:0;
    UINT64 end=UINT64(current->used)+spans[0]+spans[1]+indexSpan+sizeof(c)+3*15+255+extra;
    if(end>UPLOAD_BYTES && current->count<MAX_DRAWS && !user) {
        end=current->used;
        auto reserveBuffer=[&](IDirect3DResource9* resource,UINT sourceOffset,UINT size,int index) {
            auto shadow=shadows.find(resource);
            if(cachedBuffer(resource,object?snapshots[index]->version:shadow->second.version,sourceOffset,size)==UINT_MAX)
                end=((end+15)&~UINT64(15))+size;
        };
        for(int i=0;i<streams;++i)reserveBuffer(vb[i].Get(),sourceOffsets[i],spans[i],i);
        if(fan)end=((end+15)&~UINT64(15))+indexSpan;else if(indexed)reserveBuffer(ib.Get(),indexOffset,indexSpan,2);
        end=((end+255)&~UINT64(255))+sizeof(c)+extra;
    }
    // Restarting an unused ring cannot make an oversized draw fit. Keep any
    // pending texture commands for the normal fallback handoff instead.
    if(current->count==MAX_DRAWS || (end>UPLOAD_BYTES && current->used)) {
        terrain12Flush(current->count==MAX_DRAWS?Terrain12FlushReason::DrawLimit:Terrain12FlushReason::UploadLimit);
        if(!begin(d))return false;
    }
    Frame& f=*current;
    timer.next(TextureCpu);coverage.phase=TextureReject;
    Texture* texture=nullptr;if(!getTexture((IDirect3DTexture9*)tex.Get(),&texture))return false;
    Texture* detailTexture=texture;if(dual && !getTexture((IDirect3DTexture9*)secondTexture.Get(),&detailTexture))return false;
    timer.next(PipelineCpu);coverage.phase=PipelineReject;
    auto colorDesc=f.color12->GetDesc(),depthDesc=f.depth12?f.depth12->GetDesc():D3D12_RESOURCE_DESC{};
    if(!f.depth12 && rs(d,D3DRS_ZENABLE))return false;
    if(colorDesc.SampleDesc.Count>1 && (!rs(d,D3DRS_MULTISAMPLEANTIALIAS) || rs(d,D3DRS_MULTISAMPLEMASK)!=UINT_MAX))return false;
    PipelineKey k={(UINT)colorDesc.Format,(UINT)depthDesc.Format,
        rs(d,D3DRS_CULLMODE),rs(d,D3DRS_ZENABLE),rs(d,D3DRS_ZWRITEENABLE),rs(d,D3DRS_ZFUNC),
        rs(d,D3DRS_ALPHABLENDENABLE),rs(d,D3DRS_SRCBLEND),rs(d,D3DRS_DESTBLEND),rs(d,D3DRS_BLENDOP),rs(d,D3DRS_COLORWRITEENABLE),(mode>=12?UINT(mode+16):path+weightBytes),colorDesc.SampleDesc.Count,colorDesc.SampleDesc.Quality,water && c.waterEffect[0]>0?1u:0u,lines?1u:0u};
    if(k.src>10 || k.dst>10 || rs(d,D3DRS_SEPARATEALPHABLENDENABLE))return false;
    auto& pipeline=pipelines[k];
    if(!pipeline) {
        if(k.waterEffect){if(!waterVs && !compileShader("vertexMain","vs_5_0",waterVs,true))return false;if(!waterPs && !compileShader("pixelMain","ps_5_0",waterPs,true))return false;}
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p={};p.pRootSignature=root.Get();
        auto vertex=k.waterEffect?waterVs.Get():vs.Get(),pixel=k.waterEffect?waterPs.Get():ps.Get();
        p.VS={vertex->GetBufferPointer(),vertex->GetBufferSize()};p.PS={pixel->GetBufferPointer(),pixel->GetBufferSize()};
        D3D12_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
            {"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,1,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,1,4,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
            {"BLENDWEIGHT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
            {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
        if(water) {input[1].AlignedByteOffset=0;input[2].InputSlot=input[3].InputSlot=0;input[2].AlignedByteOffset=12;input[3].AlignedByteOffset=16;}
        if(object){input[1].AlignedByteOffset=12+weightBytes;input[3].InputSlot=0;input[3].AlignedByteOffset=24+weightBytes;}
        if(weightBytes){input[4].AlignedByteOffset=12;input[4].Format=weightBytes==4?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R32G32B32_FLOAT;}
        if(mode>=12){input[1].AlignedByteOffset=0;input[2].InputSlot=input[3].InputSlot=0;
            input[2].AlignedByteOffset=mode==13?0:12;input[3].AlignedByteOffset=mode==13?12:16;input[5].AlignedByteOffset=mode==13?20:16;}
        if(ui){input[0].Format=DXGI_FORMAT_R32G32B32A32_FLOAT;input[2].AlignedByteOffset=mode==17?0:16;
            input[3].AlignedByteOffset=mode==15?20:mode==17?16:0;input[5].AlignedByteOffset=0;}
        if(fixed){input[2].AlignedByteOffset=mode==18?12:0;input[3].AlignedByteOffset=mode==18?16:mode==19?12:0;input[5].AlignedByteOffset=0;}
        p.InputLayout={input,6};p.SampleMask=UINT_MAX;p.SampleDesc={k.samples,k.quality};p.NumRenderTargets=1;
        p.RTVFormats[0]=(DXGI_FORMAT)k.color;p.DSVFormat=(DXGI_FORMAT)k.depth;p.PrimitiveTopologyType=k.lines?D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE:D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=k.cull==1?D3D12_CULL_MODE_NONE:D3D12_CULL_MODE_BACK;
        p.RasterizerState.FrontCounterClockwise=k.cull==2;p.RasterizerState.DepthClipEnable=TRUE;
        p.RasterizerState.MultisampleEnable=k.samples>1;
        p.DepthStencilState.DepthEnable=k.zenable!=0;p.DepthStencilState.DepthWriteMask=k.zwrite?D3D12_DEPTH_WRITE_MASK_ALL:D3D12_DEPTH_WRITE_MASK_ZERO;
        p.DepthStencilState.DepthFunc=(D3D12_COMPARISON_FUNC)k.zfunc;
        auto& blend=p.BlendState.RenderTarget[0];blend.BlendEnable=k.blend;blend.SrcBlend=(D3D12_BLEND)k.src;blend.DestBlend=(D3D12_BLEND)k.dst;
        blend.BlendOp=(D3D12_BLEND_OP)k.op;blend.SrcBlendAlpha=(D3D12_BLEND)k.src;blend.DestBlendAlpha=(D3D12_BLEND)k.dst;
        // D3D12 alpha blending uses the corresponding alpha factor for COLOR factors.
        if(k.src==3)blend.SrcBlendAlpha=D3D12_BLEND_SRC_ALPHA;if(k.src==4)blend.SrcBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;
        if(k.dst==3)blend.DestBlendAlpha=D3D12_BLEND_SRC_ALPHA;if(k.dst==4)blend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;
        if(k.src==9)blend.SrcBlendAlpha=D3D12_BLEND_DEST_ALPHA;if(k.src==10)blend.SrcBlendAlpha=D3D12_BLEND_INV_DEST_ALPHA;
        if(k.dst==9)blend.DestBlendAlpha=D3D12_BLEND_DEST_ALPHA;if(k.dst==10)blend.DestBlendAlpha=D3D12_BLEND_INV_DEST_ALPHA;
        blend.BlendOpAlpha=(D3D12_BLEND_OP)k.op;blend.RenderTargetWriteMask=(UINT8)k.mask;
        TRY(gpu->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&pipeline)));
    }
    timer.next(UploadCpu);coverage.phase=UploadReject;
    auto copyBuffer=[&](IDirect3DResource9* resource,UINT offset,UINT size,int index) {
        if(user)return put(index==2?(const void*)user->indices:(const void*)user->vertices,size);
        return object?bufferData(resource,offset,size,snapshots[index]->bytes.data(),snapshots[index]->version):buffer(resource,offset,size);
    };
    UINT vo[2]={copyBuffer(vb[0].Get(),sourceOffsets[0],spans[0],0),streams==1?0:copyBuffer(vb[1].Get(),sourceOffsets[1],spans[1],1)},
        io=fan?put(fanIndices.data(),indexSpan):indexed?copyBuffer(ib.Get(),indexOffset,indexSpan,2):0,co=put(&c,sizeof c,256),objectOffset=object?put(&oc,sizeof oc,256):co;
    if(vo[0]==UINT_MAX || vo[1]==UINT_MAX || io==UINT_MAX || co==UINT_MAX || objectOffset==UINT_MAX)return false;
    timer.next(RecordCpu);
    // A cached texture has immutable format/mip metadata. Content rewrites do
    // not change its view. Slots are never overwritten while this batch is live.
    auto bindTexture=[&](Texture* texture) {
    auto viewSlot=f.srvSlots.try_emplace(texture->resource.Get(),UINT(f.srvSlots.size()));
    if(viewSlot.second) {
        auto srv=f.srv->GetCPUDescriptorHandleForHeapStart();srv.ptr+=viewSlot.first->second*gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_SHADER_RESOURCE_VIEW_DESC sd={};sd.Format=texture->format;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
        sd.Shader4ComponentMapping=texture->mapping;sd.Texture2D.MipLevels=texture->levels;
        gpu->CreateShaderResourceView(texture->resource.Get(),&sd,srv);
#ifdef WORLDMOD_RENDER_TEST
        ++testWork.srvs;
#endif
    }
        return viewSlot.first->second;
    };
    UINT mainSrv=bindTexture(texture),detailSrv=dual?bindTexture(detailTexture):mainSrv;
    auto bindSampler=[&](const DWORD* samp) {
    std::array<DWORD,11> samplerKey;std::copy(samp+1,samp+12,samplerKey.begin());
    auto samplerSlot=f.samplerSlots.try_emplace(samplerKey,UINT(f.samplerSlots.size()));
    if(samplerSlot.second) {
        D3D12_SAMPLER_DESC sm={};sm.AddressU=(D3D12_TEXTURE_ADDRESS_MODE)samp[1];sm.AddressV=(D3D12_TEXTURE_ADDRESS_MODE)samp[2];sm.AddressW=(D3D12_TEXTURE_ADDRESS_MODE)samp[3];
        sm.Filter=(samp[6]==3 || samp[5]==3)?D3D12_FILTER_ANISOTROPIC:(D3D12_FILTER)((samp[6]==2?16:0)|(samp[5]==2?4:0)|(samp[7]==2?1:0));
        memcpy(&sm.MipLODBias,&samp[8],4);sm.MaxAnisotropy=std::max(1ul,samp[10]);sm.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
        // Match stock D3D9On12: disabled mipmapping ignores MAXMIPLEVEL.
        sm.MinLOD=samp[7]?float(samp[9]):0;sm.MaxLOD=samp[7]?D3D12_FLOAT32_MAX:0;
        for(int i=0;i<4;++i)sm.BorderColor[i]=float((samp[4]>>(i==3?24:16-i*8))&255)/255;
        auto sh=f.sampler->GetCPUDescriptorHandleForHeapStart();sh.ptr+=samplerSlot.first->second*gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        gpu->CreateSampler(&sm,sh);
#ifdef WORLDMOD_RENDER_TEST
        ++testWork.samplers;
#endif
    }
        return samplerSlot.first->second;
    };
    UINT mainSampler=bindSampler(samp),detailSampler=dual?bindSampler(secondSampler):mainSampler;
    f.commands->SetPipelineState(pipeline.Get());
    auto sg=f.srv->GetGPUDescriptorHandleForHeapStart();sg.ptr+=mainSrv*gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto sampler=f.sampler->GetGPUDescriptorHandleForHeapStart();sampler.ptr+=mainSampler*gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    auto address=f.upload->GetGPUVirtualAddress();f.commands->SetGraphicsRootConstantBufferView(0,address+co);
    f.commands->SetGraphicsRootDescriptorTable(1,sg);f.commands->SetGraphicsRootDescriptorTable(2,sampler);
    f.commands->SetGraphicsRootConstantBufferView(3,address+objectOffset);
    auto detailView=f.srv->GetGPUDescriptorHandleForHeapStart();detailView.ptr+=detailSrv*gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto detailFilter=f.sampler->GetGPUDescriptorHandleForHeapStart();detailFilter.ptr+=detailSampler*gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    f.commands->SetGraphicsRootDescriptorTable(4,detailView);f.commands->SetGraphicsRootDescriptorTable(5,detailFilter);
    D3D12_VIEWPORT viewport={(float)vp.X,(float)vp.Y,(float)vp.Width,(float)vp.Height,vp.MinZ,vp.MaxZ};D3D12_RECT scissor={(LONG)vp.X,(LONG)vp.Y,(LONG)(vp.X+vp.Width),(LONG)(vp.Y+vp.Height)};
    if(ui && rs(d,D3DRS_SCISSORTESTENABLE)){RECT clip;if(FAILED(d->GetScissorRect(&clip)))return false;
        scissor.left=std::max(scissor.left,clip.left);scissor.top=std::max(scissor.top,clip.top);
        scissor.right=std::max(scissor.left,std::min(scissor.right,clip.right));scissor.bottom=std::max(scissor.top,std::min(scissor.bottom,clip.bottom));}
    f.commands->RSSetViewports(1,&viewport);f.commands->RSSetScissorRects(1,&scissor);
    D3D12_VERTEX_BUFFER_VIEW view[2];for(int i=0;i<streams;++i)view[i]={address+vo[i]+viewOffsets[i],spans[i]-viewOffsets[i],stride[i]};
    D3D12_INDEX_BUFFER_VIEW index={address+io,indexSpan,DXGI_FORMAT_R16_UINT};
    f.commands->IASetVertexBuffers(0,streams,view);f.commands->IASetIndexBuffer(indexed || fan?&index:nullptr);f.commands->IASetPrimitiveTopology(lines?(type==D3DPT_LINESTRIP?D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D_PRIMITIVE_TOPOLOGY_LINELIST):type==D3DPT_TRIANGLESTRIP?D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if(indexed || fan)f.commands->DrawIndexedInstanced(fan?UINT(fanIndices.size()):indexCount,1,drawStart,drawBase,0);else f.commands->DrawInstanced(vertices,1,0,0);++f.count;++f.modes[mode];coverage.accepted=true;return true;
}

bool terrain12Draw(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minimum,UINT vertices,UINT start,UINT primitives) {
    return drawNative(d,type,base,minimum,vertices,start,primitives,0);
}
bool water12Draw(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minimum,UINT vertices,UINT start,UINT primitives) {
    return drawNative(d,type,base,minimum,vertices,start,primitives,1);
}
bool object12Draw(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minimum,UINT vertices,UINT start,UINT primitives) {
    return drawNative(d,type,base,minimum,vertices,start,primitives,2);
}
void object12ShaderCreated(IDirect3DVertexShader9* value,uintptr_t source) {
    std::lock_guard<std::mutex> guard(modelShaderMutex);
    modelShaders.erase(value);
    const uintptr_t sources[]={0x793728,0x793f70,0x793928,0x794150,0x793bd8,0x7943e0,0x7934d0,0x769dc8,0x74e7c0};
    if(value && modelShaders.size()<4096)for(int i=0;i<9;++i)if(source==sources[i]){modelShaders[value]=6+i;break;}
}
bool model12Draw(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minimum,UINT vertices,UINT start,UINT primitives) {
    return drawNative(d,type,base,minimum,vertices,start,primitives,3);
}

bool model12DrawPrimitive(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,UINT start,UINT primitives) {
    UINT64 count=type==D3DPT_LINELIST?UINT64(primitives)*2:type==D3DPT_LINESTRIP?UINT64(primitives)+1:type!=D3DPT_TRIANGLELIST?UINT64(primitives)+2:UINT64(primitives)*3;
    if(count>UINT_MAX)return false;
    return drawNative(d,type,0,start,UINT(count),0,primitives,3,false);
}

static bool drawUser(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,UINT minimum,UINT vertices,UINT primitives,const void* indices,D3DFORMAT format,const void* data,UINT stride,bool indexed) {
    if(!gpu || failed || !enabled.load() || !data || !stride || !primitives ||
        (type!=D3DPT_TRIANGLELIST && type!=D3DPT_TRIANGLESTRIP && type!=D3DPT_TRIANGLEFAN))return false;
    UINT64 count=type==D3DPT_TRIANGLELIST?UINT64(primitives)*3:UINT64(primitives)+2;
    if(!indexed){if(count>UINT_MAX)return false;vertices=UINT(count);}
    if(!vertices || UINT64(minimum)+vertices>MAX_BUFFER_BYTES/stride ||
        UINT64(vertices)*stride+UINT64(primitives)*6+2048>MAX_BUFFER_BYTES)return false;
    std::vector<WORD> rebased;
    if(indexed){
        if(!indices || vertices>65536 || (format!=D3DFMT_INDEX16 && format!=D3DFMT_INDEX32))return false;
        rebased.resize(size_t(count));
        for(UINT i=0;i<count;++i){UINT value=0;
            if(format==D3DFMT_INDEX16){WORD word;memcpy(&word,(const BYTE*)indices+i*2,2);value=word;}
            else memcpy(&value,(const BYTE*)indices+i*4,4);
            if(value<minimum || UINT64(value)>=UINT64(minimum)+vertices)return false;
            rebased[i]=WORD(value-minimum);
        }
    }
    UserDraw user={(const BYTE*)data+size_t(minimum)*stride,indexed?rebased.data():nullptr,stride};
    if(!drawNative(d,type,0,0,vertices,0,primitives,3,indexed,&user))return false;
    // Match the stock UP API's postconditions. CPU input is already copied to
    // this batch's upload allocation and may be modified on return.
    d->SetStreamSource(0,nullptr,0,0);if(indexed)d->SetIndices(nullptr);
    return true;
}
bool model12DrawUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,UINT primitives,const void* vertices,UINT stride) {
    return drawUser(d,type,0,0,primitives,nullptr,D3DFMT_UNKNOWN,vertices,stride,false);
}
bool model12DrawIndexedUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,UINT minimum,UINT vertices,UINT primitives,const void* indices,D3DFORMAT format,const void* data,UINT stride) {
    return drawUser(d,type,minimum,vertices,primitives,indices,format,data,stride,true);
}

void terrain12Flush(Terrain12FlushReason reason) {
    if(!current)return;
    CpuTimer timer(SubmitCpu);
    Frame& f=*current;current=nullptr;
#ifdef WORLDMOD_RENDER_TEST
    ++testWork.submissions;
#endif
    if(profileEnabled.load(std::memory_order_relaxed)) {
        auto& counter=flushCounters[size_t(reason)];
        ++counter.batches;counter.draws+=f.count;if(!f.count)++counter.empty;
    }
    D3D12_RESOURCE_BARRIER barriers[]={transition(f.color12.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON),
        transition(f.depth12.Get(),D3D12_RESOURCE_STATE_DEPTH_WRITE,D3D12_RESOURCE_STATE_COMMON)};
    f.commands->ResourceBarrier(f.depth12?2:1,barriers);
    if(f.timestamps) {
        f.commands->EndQuery(f.timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);
        f.commands->ResolveQueryData(f.timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,f.timestampReadback.Get(),0);
    }
    CpuTimer stage(CloseCpu);
    HRESULT hr=f.commands->Close();stage.finish();
    if(SUCCEEDED(hr)) {
        ID3D12CommandList* lists[]={f.commands.Get()};
        stage.next(ExecuteCpu);queue->ExecuteCommandLists(1,lists);stage.finish();
        f.complete=++serial;
        stage.next(SignalCpu);hr=queue->Signal(fence.Get(),serial);stage.finish();
        if(f.timestamps && SUCCEEDED(hr)){f.timestampPending=true;++gpuPending;}
        else if(f.timestamps)++gpuErrors;
    }
    ID3D12Fence* sync[]={fence.Get()};UINT count=SUCCEEDED(hr)?1:0;
    stage.next(ReturnColorCpu);
    HRESULT ca=f.bridge->ReturnUnderlyingResource(f.color9.Get(),count,&f.complete,sync);stage.finish();
    HRESULT cb=S_OK;
    if(f.depth9) {
        stage.next(ReturnDepthCpu);cb=f.bridge->ReturnUnderlyingResource(f.depth9.Get(),count,&f.complete,sync);stage.finish();
    }
    if(ok(hr,"native submission") && ok(ca,"color return") && ok(cb,"depth return")) {
#ifdef WORLDMOD_RENDER_TEST
        if(f.count)++testWork.batches;
#endif
        if(!draws.load() && f.count && logfn)logfn("native terrain: first DX12 terrain batch submitted (%u draws)",f.count);
        draws+=f.count;for(int i=0;i<21;++i)modeDraws[i]+=f.modes[i];
    }
}

#ifdef WORLDMOD_RENDER_TEST
void terrain12TestWaterPreview(float strength,float time){waterStrength=std::isfinite(strength)?std::max(0.f,std::min(1.f,strength)):0;waterTime=std::isfinite(time)?time:0;}
Terrain12TestWater terrain12TestWater(){return {waterStrength.load(),waterTime.load()};}
Terrain12TestCoverage terrain12TestCoverage(UINT mode) {
    Terrain12TestCoverage result={};if(mode>=21)return result;
    result.recognized=recognized[mode].load();for(int i=0;i<RejectPhases;++i)result.rejected[i]=rejected[mode][i].load();return result;
}
void object12TestShaders(IDirect3DVertexShader9* base,IDirect3DVertexShader9* outline){objectShaders[0]=base;objectShaders[1]=outline;}
void water12TestBind(IDirect3DResource9* vertex,IDirect3DResource9* index,IDirect3DVertexShader9* shader) {
    waterBuffers[0]=vertex;waterBuffers[1]=index;waterShader=shader;
}
void terrain12TestPrefixUpload(bool value) { testPrefixUpload=value; }
Terrain12TestWork terrain12TestWork() { return testWork; }
void terrain12TestClearWork() { testWork={}; }
Terrain12TestTextures terrain12TestTextures() { return {textureBytes,textureUploads,TEXTURE_BUDGET,textures.size()}; }
void terrain12TestTextureBudget(UINT64 bytes) { TEXTURE_BUDGET=bytes; }
void terrain12TestBind(IDirect3DResource9* const* buffers, IDirect3DVertexShader9* const* shaders) {
    for(int i=0;i<4;++i)terrainBuffers[i]=buffers?buffers[i]:nullptr;
    for(int i=0;i<3;++i)terrainShaders[i]=shaders?shaders[i]:nullptr;
}
#endif
