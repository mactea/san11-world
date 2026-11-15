#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "render_backend.h"
#include "terrain_dx12.h"
#include "terrain_filter.h"
#include "render_copy_msaa.h"
#include <d3d9on12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <atomic>

using Microsoft::WRL::ComPtr;
namespace {
TraceLogFn g_log = nullptr;
bool g_request12 = false, g_terrain12 = false, g_probe = false;
HMODULE g_runtime = nullptr;
UINT g_sdk = D3D_SDK_VERSION;
bool g_factory12 = false;
bool g_automation = false;
int g_presentInterval = 0; // 0: unchanged, 1: ONE, 2: IMMEDIATE, 3: IMMEDIATE capped at 60
int g_msaa = 0; // Preserve the game's targets when zero/unsupported.
Microsoft::WRL::Wrappers::HandleT<Microsoft::WRL::Wrappers::HandleTraits::HANDLENullTraits> g_frameTimer;
std::atomic<bool> g_pacing{false};
LONGLONG g_nextFrame = 0;
ComPtr<ID3D12Device> g_device;
ComPtr<ID3D12CommandQueue> g_queue;
// Keep the factory's queue alive across D3D9 Reset. Mixed terrain must submit
// to the same queue passed to Direct3DCreate9On12, not a second direct queue.
ComPtr<ID3D12Device> g_factoryDevice;
ComPtr<ID3D12CommandQueue> g_factoryQueue;
bool g_sharedFactory = false, g_nativeReady = false;
ComPtr<ID3D12InfoQueue> g_debugMessages;
SRWLOCK g_lock = SRWLOCK_INIT;
SRWLOCK g_timingLock = SRWLOCK_INIT;
double g_frameTime[8192] = {};
UINT g_timeCount = 0, g_timeHead = 0;
LARGE_INTEGER g_lastFrame = {}, g_frequency = {};
volatile LONG g_resetTiming = 0;
struct PresentSample { double interval, call; bool foreground; };
PresentSample g_presentTime[8192] = {};
UINT g_presentCount = 0, g_presentHead = 0, g_presentFailures = 0;
UINT g_deviceResets = 0;
HRESULT g_lastPresentFailure = S_OK;
LARGE_INTEGER g_lastPresent = {};
bool g_lastForeground = false;
HWND g_window = nullptr;
struct Status {
    char requested[16] = "system", actual[16] = "not_created", terrain[24] = "original";
    char adapter[128] = "", reason[160] = "";
    UINT width = 0, height = 0, format = 0, samples = 0;
    BOOL windowed = FALSE;
    UINT interval = 0;
    HRESULT color = E_PENDING, depth = E_PENDING, texture = E_PENDING, vertex = E_PENDING, index = E_PENDING;
} g_status;

void resetTimingLocked() {
    if (!InterlockedExchange(&g_resetTiming, 0)) return;
    g_timeCount = g_timeHead = g_presentCount = g_presentHead = g_presentFailures = 0;
    g_deviceResets = 0; g_lastPresentFailure = S_OK;
    terrainFilterClearCounters();
    g_lastFrame.QuadPart = g_lastPresent.QuadPart = 0;
}

void presentSummary(FILE* out, const std::vector<PresentSample>& samples, bool foreground) {
    std::vector<double> intervals, calls;
    double intervalSum = 0, callSum = 0;
    for (const auto& s : samples) if (s.foreground == foreground && s.interval > 0) {
        intervals.push_back(s.interval); calls.push_back(s.call);
        intervalSum += s.interval; callSum += s.call;
    }
    std::sort(intervals.begin(), intervals.end()); std::sort(calls.begin(), calls.end());
    size_t n = intervals.size();
    fprintf(out, "Present %s: samples=%zu mean_ms=%.3f p95_ms=%.3f p99_ms=%.3f rate=%.2f call_mean_ms=%.3f call_p95_ms=%.3f span_ms=%.3f\n",
        foreground ? "foreground" : "background", n,
        n ? intervalSum/n : 0, n ? intervals[(n-1)*95/100] : 0, n ? intervals[(n-1)*99/100] : 0,
        intervalSum > 0 ? 1000*n/intervalSum : 0, n ? callSum/n : 0, n ? calls[(n-1)*95/100] : 0, intervalSum);
}

void reason(const char* text) {
    AcquireSRWLockExclusive(&g_lock);
    strcpy_s(g_status.reason, text);
    ReleaseSRWLockExclusive(&g_lock);
    if (g_log) g_log("render: %s", text);
}
void parameters(const D3DPRESENT_PARAMETERS& pp) {
    g_pacing = g_frameTimer.IsValid() && g_device && pp.Windowed && pp.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE;
    g_nextFrame = 0;
    AcquireSRWLockExclusive(&g_lock);
    g_status.width = pp.BackBufferWidth; g_status.height = pp.BackBufferHeight;
    g_status.format = pp.BackBufferFormat; g_status.samples = pp.MultiSampleType;
    g_status.windowed = pp.Windowed; g_status.interval = pp.PresentationInterval;
    ReleaseSRWLockExclusive(&g_lock);
    if (g_log) g_log("render: backbuffer=%ux%u format=%u samples=%u/%lu windowed=%d swap=%u interval=%#x",
        pp.BackBufferWidth, pp.BackBufferHeight, pp.BackBufferFormat, pp.MultiSampleType,
        pp.MultiSampleQuality, pp.Windowed, pp.SwapEffect, pp.PresentationInterval);
}
HRESULT probeResource(IDirect3DDevice9On12* bridge, IDirect3DResource9* source, const char* label) {
    if (!source) return E_POINTER;
    ComPtr<ID3D12Resource> resource;
    HRESULT hr = bridge->UnwrapUnderlyingResource(source, g_queue.Get(), IID_PPV_ARGS(&resource));
    if (SUCCEEDED(hr)) {
        auto desc = resource->GetDesc();
        if (g_log) g_log("render interop: %s dim=%u %llux%u format=%u flags=%u",
            label, desc.Dimension, desc.Width, desc.Height, desc.Format, desc.Flags);
        // No native work was submitted. Return ownership before any D3D9 use.
        hr = bridge->ReturnUnderlyingResource(source, 0, nullptr, nullptr);
    }
    if (g_log) g_log("render interop: %s hr=%#lx", label, hr);
    return hr;
}
void debugLayer() {
    wchar_t flag[8];
    if (!GetEnvironmentVariableW(L"WORLDMOD_D3D12_DEBUG", flag, 8)) return;
    ComPtr<ID3D12Debug> debug;
    HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
    if (SUCCEEDED(hr)) debug->EnableDebugLayer();
    if (g_log) g_log("render: debug layer hr=%08lx", hr);
}
UINT factoryArgs(D3D9ON12_ARGS (&args)[2]) {
    args[0].Enable9On12 = TRUE;
    if (!g_terrain12) return 1;
    if (!g_factoryQueue) {
        HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_factoryDevice));
        D3D12_COMMAND_QUEUE_DESC q = {}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (SUCCEEDED(hr)) hr = g_factoryDevice->CreateCommandQueue(&q, IID_PPV_ARGS(&g_factoryQueue));
        if (FAILED(hr)) {
            g_factoryQueue.Reset(); g_factoryDevice.Reset();
            if (g_log) g_log("render: shared native queue creation failed hr=%08lx", hr);
            reason("shared native queue unavailable; original terrain active");
            return 1;
        }
    }
    args[0].pD3D12Device = g_factoryDevice.Get();
    args[0].ppD3D12Queues[0] = g_factoryQueue.Get(); args[0].NumQueues = 1;
    // Other adapters can still use 9on12, but cannot use this native queue.
    args[1].Enable9On12 = TRUE;
    return 2;
}
}

void renderConfigure(const wchar_t* backend, const wchar_t* terrain, TraceLogFn log, bool automation, int presentInterval, int anisotropy,int msaa,int waterStrength) {
    g_log = log;
    g_automation = automation;
    g_presentInterval = presentInterval;
    if (g_presentInterval < 0 || g_presentInterval > 3) {
        g_presentInterval = 0;
        reason("unknown render_present_interval; keeping game presentation settings");
    }
    QueryPerformanceFrequency(&g_frequency);
    g_request12 = !_wcsicmp(backend, L"9on12");
    g_terrain12 = !_wcsicmp(terrain, L"dx12");
    strcpy_s(g_status.requested, g_request12 ? "9on12" : "system");
    if (_wcsicmp(backend, L"system") && !g_request12) reason("unknown render_backend; using system");
    if (_wcsicmp(terrain, L"original") && !g_terrain12) reason("unknown terrain_renderer; using original");
    if (g_terrain12 && !g_request12) {
        g_terrain12 = false;
        reason("terrain_renderer=dx12 requires render_backend=9on12; using original terrain");
    }
    if (g_terrain12) reason("experimental DX12 base/outline/lighting/water; unsupported draws use original rendering");
    terrain12ConfigureWater(g_terrain12?waterStrength:0);
    terrainFilterConfigure(anisotropy, log);
    g_msaa=(msaa==2 || msaa==4 || msaa==8)?msaa:0;
}
bool renderNativeTerrainRequested() { return g_terrain12; }
#ifdef WORLDMOD_RENDER_TEST
ID3D12CommandQueue* renderTestNativeQueue() { return g_nativeReady ? g_queue.Get() : nullptr; }
#endif

void renderAdjustPresentation(D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9* device,IDirect3D9* api,UINT adapter,D3DDEVTYPE type) {
    if(!pp)return;
    if(g_msaa && g_terrain12 && pp->SwapEffect==D3DSWAPEFFECT_DISCARD && pp->EnableAutoDepthStencil &&
       pp->AutoDepthStencilFormat==D3DFMT_D24S8 && pp->MultiSampleType==D3DMULTISAMPLE_NONE &&
       !(pp->Flags&D3DPRESENTFLAG_LOCKABLE_BACKBUFFER)) {
        ComPtr<IDirect3D9> owner;
        if(device) {
            D3DDEVICE_CREATION_PARAMETERS creation={};
            if(SUCCEEDED(device->GetDirect3D(&owner)) && SUCCEEDED(device->GetCreationParameters(&creation))) {
                api=owner.Get();adapter=creation.AdapterOrdinal;type=creation.DeviceType;
            }
        }
        D3DFORMAT format=pp->BackBufferFormat;
        if(api && format==D3DFMT_UNKNOWN && pp->Windowed) {
            D3DDISPLAYMODE display={};if(SUCCEEDED(api->GetAdapterDisplayMode(adapter,&display)))format=display.Format;
        }
        DWORD colorLevels=0,depthLevels=0;
        if(api && SUCCEEDED(api->CheckDeviceMultiSampleType(adapter,type,format,pp->Windowed,D3DMULTISAMPLE_TYPE(g_msaa),&colorLevels)) &&
           SUCCEEDED(api->CheckDeviceMultiSampleType(adapter,type,pp->AutoDepthStencilFormat,pp->Windowed,D3DMULTISAMPLE_TYPE(g_msaa),&depthLevels)) && colorLevels && depthLevels) {
            pp->MultiSampleType=D3DMULTISAMPLE_TYPE(g_msaa);pp->MultiSampleQuality=0;
        } else if(g_log)g_log("render: requested MSAA %d unsupported; keeping game targets",g_msaa);
    }
    if (!g_presentInterval || !pp->Windowed) return;
    bool on12 = g_factory12;
    if (device) {
        ComPtr<IDirect3DDevice9On12> bridge;
        on12 = SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&bridge)));
    }
    if (!on12) return;
    if (g_presentInterval == 3 && !g_frameTimer.IsValid()) {
        g_frameTimer.Attach(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));
        if (!g_frameTimer.IsValid()) {
            reason("high-resolution frame timer unavailable; keeping game presentation settings");
            return;
        }
    }
    pp->PresentationInterval = g_presentInterval == 1 ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
}

void renderWaitForFrame() {
    if (!g_pacing) return;
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    const LONGLONG period = g_frequency.QuadPart / 60;
    if (g_nextFrame > now.QuadPart) {
        LARGE_INTEGER due;
        due.QuadPart = -((g_nextFrame-now.QuadPart)*10000000/g_frequency.QuadPart + 1);
        if (SetWaitableTimer(g_frameTimer.Get(), &due, 0, nullptr, nullptr, FALSE))
            WaitForSingleObject(g_frameTimer.Get(), 100);
        QueryPerformanceCounter(&now);
    }
    // No catch-up burst after loading, focus changes, or a slow frame.
    if (!g_nextFrame || now.QuadPart >= g_nextFrame + period) g_nextFrame = now.QuadPart;
    g_nextFrame += period;
}

IDirect3D9* renderCreate9(HMODULE runtime, UINT sdk) {
    if (!runtime) return nullptr;
    g_runtime = runtime; g_sdk = sdk; g_factory12 = false; g_sharedFactory = false;
    if (g_request12) {
        debugLayer();
        auto create = (PFN_Direct3DCreate9On12)GetProcAddress(runtime, "Direct3DCreate9On12");
        if (create) {
            D3D9ON12_ARGS args[2] = {}; UINT count = factoryArgs(args);
            if (auto api = create(sdk, args, count)) { g_factory12 = true; g_sharedFactory = count==2; return api; }
        }
        reason("Direct3DCreate9On12 unavailable/failed; using system D3D9");
    }
    auto create = (IDirect3D9*(WINAPI*)(UINT))GetProcAddress(runtime, "Direct3DCreate9");
    return create ? create(sdk) : nullptr;
}
IDirect3D9* renderFallback9() {
    if (!g_factory12 || !g_runtime) return nullptr;
    auto create = (IDirect3D9*(WINAPI*)(UINT))GetProcAddress(g_runtime, "Direct3DCreate9");
    return create ? create(g_sdk) : nullptr;
}
void renderDeviceFallback(HRESULT failure) {
    g_sharedFactory = false;
    if (g_log) g_log("render: 9on12 CreateDevice failed hr=%08lx; system CreateDevice succeeded", failure);
    reason("9on12 device creation failed; using system D3D9 and original terrain");
}

HRESULT renderCreate9Ex(HMODULE runtime, UINT sdk, IDirect3D9Ex** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!runtime) return E_FAIL;
    g_sharedFactory = false;
    if (g_request12) {
        debugLayer();
        auto create = (PFN_Direct3DCreate9On12Ex)GetProcAddress(runtime, "Direct3DCreate9On12Ex");
        if (create) {
            D3D9ON12_ARGS args[2] = {}; UINT count = factoryArgs(args);
            HRESULT hr = create(sdk, args, count, out);
            if (SUCCEEDED(hr)) { g_sharedFactory = count==2; return hr; }
        }
        reason("Direct3DCreate9On12Ex unavailable/failed; using system D3D9Ex");
    }
    auto create = (HRESULT(WINAPI*)(UINT, IDirect3D9Ex**))GetProcAddress(runtime, "Direct3DCreate9Ex");
    return create ? create(sdk, out) : E_FAIL;
}

void renderOnDevice(IDirect3D9* api, UINT adapter, IDirect3DDevice9* device, const D3DPRESENT_PARAMETERS& pp) {
    renderBeforeReset();
    g_debugMessages.Reset(); g_queue.Reset(); g_device.Reset(); g_nativeReady = false;
    D3DADAPTER_IDENTIFIER9 id = {}; api->GetAdapterIdentifier(adapter, 0, &id);
    ComPtr<IDirect3DDevice9On12> bridge;
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&bridge));
    bool on12 = SUCCEEDED(hr);
    if (on12) {
        hr = bridge->GetD3D12Device(IID_PPV_ARGS(&g_device));
        if (SUCCEEDED(hr)) {
            g_device.As(&g_debugMessages);
            if (g_terrain12) {
                ComPtr<IUnknown> actual, supplied;
                g_device.As(&actual);
                if (g_factoryDevice) g_factoryDevice.As(&supplied);
                if (g_sharedFactory && g_factoryQueue && actual.Get()==supplied.Get()) {
                    g_queue = g_factoryQueue; g_nativeReady = true;
                    reason("native terrain shares the 9on12 direct queue");
                } else {
                    reason("device has no matching shared native queue; original terrain active");
                }
            } else {
                D3D12_COMMAND_QUEUE_DESC q = {}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
                hr = g_device->CreateCommandQueue(&q, IID_PPV_ARGS(&g_queue));
            }
        }
        if (FAILED(hr)) reason("D3D12 device/queue initialization failed; original terrain active");
    }
    AcquireSRWLockExclusive(&g_lock);
    strcpy_s(g_status.actual, on12 ? "9on12" : "system");
    strncpy_s(g_status.adapter, id.Description, _TRUNCATE);
    ReleaseSRWLockExclusive(&g_lock);
    if (g_log) g_log("render: requested=%s actual=%s adapter=%u vendor=%04x device=%04x name=%s native_queue_hr=%#lx",
        g_status.requested, on12 ? "9on12" : "system", adapter, id.VendorId, id.DeviceId, id.Description, hr);
    if (g_request12 && !on12) reason("requested 9on12 but runtime device is native D3D9");
    if (g_nativeReady) terrain12Init(g_device.Get(), g_queue.Get(), g_log);
    terrainFilterOnDevice(device);
    if(g_nativeReady)copyMsaaInit(device,pp,g_msaa);
    parameters(pp);
}

void renderBeforeReset() {
    g_pacing = false; g_nextFrame = 0;
    terrain12Reset();
    copyMsaaReset();
    terrainFilterReset();
    AcquireSRWLockExclusive(&g_timingLock);
    resetTimingLocked();
    ++g_deviceResets;
    g_lastFrame.QuadPart = g_lastPresent.QuadPart = 0;
    ReleaseSRWLockExclusive(&g_timingLock);
    g_probe = false;
    // No retained D3D9 default-pool resources may survive Reset.
    AcquireSRWLockExclusive(&g_lock);
    g_status.color = g_status.depth = g_status.texture = g_status.vertex = g_status.index = E_PENDING;
    ReleaseSRWLockExclusive(&g_lock);
}
void renderAfterReset(IDirect3DDevice9* device, HRESULT hr, const D3DPRESENT_PARAMETERS& pp) {
    if (g_log) g_log("render: Reset hr=%#lx", hr);
    if (FAILED(hr) && g_device && g_log)
        g_log("render: device removed reason=%08lx", g_device->GetDeviceRemovedReason());
    if (SUCCEEDED(hr)) {
        terrainFilterOnDevice(device);
        parameters(pp);
        if (g_nativeReady) terrain12Init(g_device.Get(), g_queue.Get(), g_log);
        if (g_nativeReady) copyMsaaInit(device,pp,g_msaa);
    }
}
void renderStatus(FILE* out) {
    AcquireSRWLockShared(&g_lock); Status s = g_status; ReleaseSRWLockShared(&g_lock);
    if (terrain12Active()) strcpy_s(s.terrain, "dx12_mixed");
    fprintf(out, "render requested=%s actual=%s terrain=%s adapter=%s size=%ux%u format=%u samples=%u\n"
        "interop color=%08lx depth=%08lx texture=%08lx vertex=%08lx index=%08lx\nreason=%s\n",
        s.requested, s.actual, s.terrain, s.adapter, s.width, s.height, s.format, s.samples,
        s.color, s.depth, s.texture, s.vertex, s.index, s.reason);
    terrain12Status(out);
    copyMsaaStatus(out);
    fprintf(out,"native_shared_queue=%d\n",g_nativeReady);
    terrainFilterStatus(out);
    fprintf(out, "Presentation: windowed=%d interval=%08x frame_limit=%u\n", s.windowed, s.interval, g_pacing ? 60u : 0u);
    AcquireSRWLockShared(&g_timingLock);
    std::vector<double> samples(g_frameTime, g_frameTime + g_timeCount);
    std::vector<PresentSample> presents(g_presentTime, g_presentTime + g_presentCount);
    UINT failures = g_presentFailures;
    UINT resets = g_deviceResets;
    HRESULT lastFailure = g_lastPresentFailure;
    ReleaseSRWLockShared(&g_timingLock);
    if (!samples.empty()) {
        double sum = 0; for (double value : samples) sum += value;
        std::sort(samples.begin(), samples.end());
        fprintf(out, "EndScene intervals (not display FPS): samples=%zu mean_ms=%.3f p50_ms=%.3f p95_ms=%.3f rate=%.2f\n",
            samples.size(), sum / samples.size(), samples[samples.size()/2],
            samples[(samples.size()-1)*95/100], 1000 * samples.size()/sum);
    }
    fprintf(out, "Present measurement: CPU calls, not display FPS/GPU time; automation=%d failed_calls=%u device_resets=%u last_failure=%08lx\n", g_automation, failures, resets, lastFailure);
    presentSummary(out, presents, true); presentSummary(out, presents, false);
}
void renderTimingReset() { InterlockedExchange(&g_resetTiming, 1); }
void renderOnPresent(LARGE_INTEGER begin, LARGE_INTEGER end, HRESULT hr, HWND window) {
    // Use the real OS foreground window, unaffected by the game's automation IAT hooks.
    HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
    bool foreground = root && root == GetForegroundWindow() && !IsIconic(root);
    AcquireSRWLockExclusive(&g_timingLock);
    resetTimingLocked();
    if (FAILED(hr)) {
        ++g_presentFailures; g_lastPresentFailure = hr; g_lastPresent.QuadPart = 0;
    } else {
        double interval = g_lastPresent.QuadPart && g_lastForeground == foreground && g_window == window
            ? (end.QuadPart-g_lastPresent.QuadPart)*1000.0/g_frequency.QuadPart : 0;
        g_presentTime[g_presentHead] = {interval, (end.QuadPart-begin.QuadPart)*1000.0/g_frequency.QuadPart, foreground};
        g_presentHead = (g_presentHead+1)%8192;
        if (g_presentCount < 8192) ++g_presentCount;
        g_lastPresent = end; g_lastForeground = foreground; g_window = window;
    }
    ReleaseSRWLockExclusive(&g_timingLock);
}
void renderOnFrame() {
    if (g_debugMessages) {
        UINT64 count = g_debugMessages->GetNumStoredMessagesAllowedByRetrievalFilter();
        for (UINT64 i = 0; i < count && i < 64; ++i) {
            SIZE_T size = 0; g_debugMessages->GetMessage(i, nullptr, &size);
            std::vector<char> bytes(size);
            auto message = (D3D12_MESSAGE*)bytes.data();
            if (size && SUCCEEDED(g_debugMessages->GetMessage(i, message, &size)) &&
                message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING && g_log)
                g_log("D3D12 validation [%u/%u]: %s", message->Severity, message->ID, message->pDescription);
        }
        g_debugMessages->ClearStoredMessages();
    }
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    AcquireSRWLockExclusive(&g_timingLock);
    resetTimingLocked();
    double waterElapsed=g_lastFrame.QuadPart?(now.QuadPart-g_lastFrame.QuadPart)/double(g_frequency.QuadPart):0;
    if (g_lastFrame.QuadPart) {
        g_frameTime[g_timeHead] = (now.QuadPart-g_lastFrame.QuadPart)*1000.0/g_frequency.QuadPart;
        g_timeHead = (g_timeHead+1)%8192;
        if (g_timeCount < 8192) ++g_timeCount;
    }
    g_lastFrame = now;
    ReleaseSRWLockExclusive(&g_timingLock);
    if(g_nativeReady)terrain12AdvanceWater(waterElapsed);
}
void renderProbeTerrain(IDirect3DDevice9* device) {
    if (!g_terrain12 || g_probe || !g_queue) return;
    g_probe = true;
    ComPtr<IDirect3DDevice9On12> bridge;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&bridge)))) return;
    ComPtr<IDirect3DSurface9> color, depth;
    ComPtr<IDirect3DBaseTexture9> texture;
    ComPtr<IDirect3DVertexBuffer9> vertex;
    ComPtr<IDirect3DIndexBuffer9> index;
    UINT offset = 0, stride = 0;
    device->GetRenderTarget(0, &color); device->GetDepthStencilSurface(&depth);
    device->GetTexture(0, &texture); device->GetStreamSource(0, &vertex, &offset, &stride); device->GetIndices(&index);
    if (g_log) {
        g_log("terrain stream0: offset=%u stride=%u", offset, stride);
        const D3DRENDERSTATETYPE states[] = {D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ZFUNC,
            D3DRS_CULLMODE, D3DRS_ALPHABLENDENABLE, D3DRS_SRCBLEND, D3DRS_DESTBLEND,
            D3DRS_ALPHATESTENABLE, D3DRS_ALPHAREF, D3DRS_ALPHAFUNC, D3DRS_FOGENABLE,
            D3DRS_FOGCOLOR, D3DRS_FOGTABLEMODE, D3DRS_FOGVERTEXMODE};
        for (auto state : states) { DWORD value = 0; device->GetRenderState(state, &value); g_log("terrain RS %u=%08lx", state, value); }
        for (UINT stage = 0; stage < 2; ++stage)
            for (UINT state = 1; state <= 6; ++state) {
                DWORD value = 0; device->GetTextureStageState(stage, (D3DTEXTURESTAGESTATETYPE)state, &value);
                g_log("terrain TSS %u/%u=%08lx", stage, state, value);
            }
        ComPtr<IDirect3DPixelShader9> ps; device->GetPixelShader(&ps);
        g_log("terrain pixel shader=%p", ps.Get());
        if (vertex) {
            D3DVERTEXBUFFER_DESC desc = {}; vertex->GetDesc(&desc);
            g_log("terrain VB usage=%x pool=%u size=%u", desc.Usage, desc.Pool, desc.Size);
        }
        if (texture && texture->GetType() == D3DRTYPE_TEXTURE) {
            D3DSURFACE_DESC desc = {}; ((IDirect3DTexture9*)texture.Get())->GetLevelDesc(0, &desc);
            g_log("terrain texture usage=%x pool=%u format=%u", desc.Usage, desc.Pool, desc.Format);
        }
    }
    HRESULT a = probeResource(bridge.Get(), color.Get(), "color");
    HRESULT b = probeResource(bridge.Get(), depth.Get(), "depth");
    HRESULT c = probeResource(bridge.Get(), texture.Get(), "texture");
    HRESULT d = probeResource(bridge.Get(), vertex.Get(), "vertex");
    HRESULT e = probeResource(bridge.Get(), index.Get(), "index");
    AcquireSRWLockExclusive(&g_lock);
    g_status.color = a; g_status.depth = b; g_status.texture = c; g_status.vertex = d; g_status.index = e;
    ReleaseSRWLockExclusive(&g_lock);
}
