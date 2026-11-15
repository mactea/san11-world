// x86 integration test for system D3D9 and D3D9On12, independent of game hooks.
// Also verifies D3D9 -> D3D12 -> D3D9 ownership when On12 is actually available.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9on12.h>
#include <wrl/client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/render_backend.h"
#include "../src/render_copy_msaa.h"
#include "../src/terrain_dx12.h"
using Microsoft::WRL::ComPtr;

static void check(HRESULT hr, const char* what) {
    if (FAILED(hr)) { printf("FAIL %s: %08lx\n", what, hr); exit(1); }
}
#define CHECK(x) check((x), #x)
void testTerrainFilter(IDirect3DDevice9* device);
void testTerrain12(IDirect3DDevice9* device, ID3D12Device* gpu, ID3D12CommandQueue* queue,IDirect3DTexture9* retained,int iteration);
void testDefaultTextureContract(IDirect3DDevice9*,ID3D12Device*,ID3D12CommandQueue*,int);

static void testCpuProfile(int iteration) {
    static UINT64 previous[14]={},previousFlush[15][3]={};
    FILE* file=nullptr;
    if(tmpfile_s(&file) || !file) {puts("FAIL profile temporary file");exit(1);}
    terrain12Status(file);rewind(file);
    char line[256],name[32];UINT64 calls;double ms;int stages=0,gpuLines=0,uploadLines=0,flushLines=0;
    static UINT64 previousBatches=0,previousBytes=0;
    while(fgets(line,sizeof line,file)) {
        int active=0;UINT64 batches=0,pending=0,errors=0,bytes=0,copies=0,hits=0,textureCopies=0,textureBytes=0;
        if(sscanf_s(line,"Native GPU profile: enabled=%d cumulative=1 batches=%llu total_ms=%lf pending=%llu errors=%llu",
            &active,&batches,&ms,&pending,&errors)==5) {
            if(active!=(iteration==1) || errors || pending>4 || batches<previousBatches ||
               (iteration==0 && (batches || ms || pending)) || (iteration==1 && (!batches || ms<=0))) {
                puts("FAIL GPU timestamps enable/disable/completion/Reset");exit(1);
            }
            previousBatches=batches;++gpuLines;continue;
        }
        if(sscanf_s(line,"Native upload profile: enabled=%d cumulative=1 bytes=%llu buffer_copies=%llu buffer_hits=%llu texture_copies=%llu texture_bytes=%llu",
            &active,&bytes,&copies,&hits,&textureCopies,&textureBytes)==6) {
            if(active!=(iteration==1) || (iteration==0 && (bytes || copies || hits || textureCopies || textureBytes)) ||
               (iteration==1 && (!bytes || !copies || !hits || !textureCopies || !textureBytes)) ||
               (iteration==2 && bytes!=previousBytes)) {puts("FAIL upload profile enable/disable/Reset");exit(1);}
            previousBytes=bytes;++uploadLines;continue;
        }
        UINT64 flushDraws=0,empty=0;
        if(sscanf_s(line,"Native flush %31s batches=%llu draws=%llu empty=%llu",name,(unsigned)sizeof name,&batches,&flushDraws,&empty)==4) {
            UINT64 values[]={batches,flushDraws,empty};
            if(flushLines>=15 || empty>batches) {puts("FAIL flush census shape");exit(1);}
            for(int i=0;i<3;++i) {
                if((iteration==0 && values[i]) || values[i]<previousFlush[flushLines][i] ||
                   (iteration==2 && values[i]!=previousFlush[flushLines][i])) {puts("FAIL flush census enable/disable/Reset");exit(1);}
                previousFlush[flushLines][i]=values[i];
            }
            ++flushLines;continue;
        }
        if(sscanf_s(line,"Native CPU %31s calls=%llu total_ms=%lf",name,(unsigned)sizeof name,&calls,&ms)!=3)continue;
        if(stages>=14 || ms<0 || (iteration==0 && (calls || ms)) ||
           (iteration==1 && stages!=8 && (!calls || ms<=0)) || (iteration==2 && calls!=previous[stages])) {
            puts("FAIL CPU profile enable/disable/cumulative counters");exit(1);
        }
        previous[stages++]=calls;
    }
    fclose(file);
    if(stages!=14 || gpuLines!=1 || uploadLines!=1 || flushLines!=15) {puts("FAIL profile missing stages");exit(1);}
    printf("PASS CPU profile enabled=%d stages=%d iteration=%d\n",iteration==1,stages,iteration);
    printf("PASS flush census enabled=%d reasons=%d cumulative_across_reset=1 iteration=%d\n",iteration==1,flushLines,iteration);
    printf("PASS GPU/upload profile enabled=%d cumulative_batches=%llu bytes=%llu iteration=%d\n",iteration==1,previousBatches,previousBytes,iteration);
}

void testObjectCapture();
void testProcessVerticesFaults();
void testProcessVerticesDevice(IDirect3DDevice9*);
void testCopyMsaaHooks(IDirect3D9*,UINT,D3DPRESENT_PARAMETERS);
void testCopyMsaaTransitions(IDirect3D9*,UINT,D3DPRESENT_PARAMETERS);
void testCopyMsaaFailedReset(IDirect3D9*,UINT,D3DPRESENT_PARAMETERS);
int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    if(GetEnvironmentVariableA("WORLDMOD_DEFAULT_PROBE",nullptr,0))AddVectoredExceptionHandler(1,[](PEXCEPTION_POINTERS e)->LONG{
        if(e->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION){
            HMODULE module=nullptr;wchar_t path[MAX_PATH]={};
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)e->ExceptionRecord->ExceptionAddress,&module);
            GetModuleFileNameW(module,path,MAX_PATH);
            printf("Probe exception code=%08lx module=%ls offset=%p eax=%08lx ecx=%08lx edx=%08lx\n",e->ExceptionRecord->ExceptionCode,path,(void*)((uintptr_t)e->ExceptionRecord->ExceptionAddress-(uintptr_t)module),e->ContextRecord->Eax,e->ContextRecord->Ecx,e->ContextRecord->Edx);fflush(stdout);
        }return EXCEPTION_CONTINUE_SEARCH;
    });
    testObjectCapture();testProcessVerticesFaults();
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--system") && strcmp(argv[1], "--9on12") && strcmp(argv[1], "--native") && strcmp(argv[1], "--copy-msaa"))) {
        puts("Usage: render_interop.exe [--system|--9on12|--native|--copy-msaa]"); return 2;
    }
    const bool system = argc == 2 && !strcmp(argv[1], "--system");
    const bool copyOnly = argc == 2 && !strcmp(argv[1], "--copy-msaa");
    const bool native = copyOnly || (argc == 2 && !strcmp(argv[1], "--native"));
    SetEnvironmentVariableW(L"WORLDMOD_TERRAIN_PROFILE",nullptr);
    wchar_t path[MAX_PATH]; GetSystemDirectoryW(path, MAX_PATH); wcscat_s(path, L"\\d3d9.dll");
    HMODULE runtime = LoadLibraryW(path);
    ComPtr<IDirect3D9> api;
    ComPtr<ID3D12Device> d12;
    ComPtr<ID3D12CommandQueue> queue;
    renderConfigure(system?L"system":L"9on12",native?L"dx12":L"original",nullptr,false,0,0);
    api.Attach(renderCreate9(runtime,D3D_SDK_VERSION));
    if (!api) { puts("FAIL requested D3D9 enumerator unavailable"); return 1; }
    UINT adapter = D3DADAPTER_DEFAULT;
    for (UINT i = 0; i < api->GetAdapterCount(); ++i) {
        D3DADAPTER_IDENTIFIER9 id = {}; CHECK(api->GetAdapterIdentifier(i, 0, &id));
        printf("adapter %u: %s vendor=%04x\n", i, id.Description, id.VendorId);
        if (id.VendorId == 0x10de) adapter = i;
    }
    WNDCLASSW wc = {}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"WorldmodRenderInterop";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Render interop test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, wc.hInstance, nullptr);
    D3DPRESENT_PARAMETERS pp = {}; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    pp.BackBufferWidth = 256; pp.BackBufferHeight = 256; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_COPY; pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    if(copyOnly) {testCopyMsaaTransitions(api.Get(),adapter,pp);testCopyMsaaFailedReset(api.Get(),adapter,pp);return 0;}
    ComPtr<IDirect3DDevice9> d9;
    D3DPRESENT_PARAMETERS initial=pp;
    if(native) {
        renderConfigure(L"9on12",L"dx12",nullptr,false,0,0);
        initial.SwapEffect=D3DSWAPEFFECT_DISCARD;
        renderAdjustPresentation(&initial,nullptr,api.Get(),adapter);
        if(initial.MultiSampleType!=D3DMULTISAMPLE_4_SAMPLES){puts("FAIL default presentation MSAA is not 4x");return 1;}
    }
    CHECK(api->CreateDevice(adapter, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &initial, &d9));
    testProcessVerticesDevice(d9.Get());
    if(native) {
        const int disabledValues[]={0,3};
        for(int disabled:disabledValues) {
            renderConfigure(L"9on12",L"dx12",nullptr,false,0,0,disabled);
            auto unchanged=pp;unchanged.SwapEffect=D3DSWAPEFFECT_DISCARD;
            renderAdjustPresentation(&unchanged,d9.Get());
            if(unchanged.MultiSampleType!=D3DMULTISAMPLE_NONE){puts("FAIL disabled/invalid MSAA changed targets");return 1;}
        }
        puts("PASS default MSAA 4x CreateDevice; explicit zero/invalid preserve targets");
        const int samples[]={2,4,8};
        for(int count:samples) {
            renderConfigure(L"9on12",L"dx12",nullptr,false,0,0,count);
            D3DPRESENT_PARAMETERS copy=pp;renderAdjustPresentation(&copy,d9.Get());
            if(copy.MultiSampleType!=D3DMULTISAMPLE_NONE){puts("FAIL COPY swap effect changed");return 1;}
            copy=pp;copy.SwapEffect=D3DSWAPEFFECT_DISCARD;copy.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
            renderAdjustPresentation(&copy,d9.Get());
            if(copy.MultiSampleType!=D3DMULTISAMPLE_NONE){puts("FAIL lockable backbuffer changed");return 1;}
            copy.Flags=0;renderAdjustPresentation(&copy,d9.Get());
            if(copy.MultiSampleType!=D3DMULTISAMPLE_TYPE(count)){puts("FAIL presentation MSAA selection");return 1;}
            CHECK(d9->Reset(&copy));CHECK(d9->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff203040,1,0));
            CHECK(d9->Present(nullptr,nullptr,nullptr,nullptr));
            ComPtr<IDirect3DSurface9> back;CHECK(d9->GetRenderTarget(0,&back));D3DSURFACE_DESC desc;CHECK(back->GetDesc(&desc));
            if(desc.MultiSampleType!=D3DMULTISAMPLE_TYPE(count)){puts("FAIL actual presentation sample count");return 1;}
            printf("PASS presentation MSAA=%d Reset/clear/Present and protected modes\n",count);
        }
        renderConfigure(L"9on12",L"dx12",nullptr,false,0,0,0);
        CHECK(d9->Reset(&pp));
    }
    ComPtr<IDirect3DDevice9On12> bridge;
    HRESULT bridgeHr = d9.As(&bridge);
    if (!system) CHECK(bridgeHr);
    else if (bridgeHr != E_NOINTERFACE) CHECK(bridgeHr);
    D3DADAPTER_IDENTIFIER9 selected = {}; CHECK(api->GetAdapterIdentifier(adapter,0,&selected));
    printf("Test backend: requested=%s actual=%s vendor=%04x adapter=%s\n",
        system ? "system" : "9on12", bridge ? "9on12" : "system", selected.VendorId, selected.Description);
    renderOnDevice(api.Get(),adapter,d9.Get(),pp);
    if (bridge) {
        CHECK(bridge->GetD3D12Device(IID_PPV_ARGS(&d12)));
        // Capability evidence for later quality work; never enables MSAA.
        D3DCAPS9 caps={};CHECK(d9->GetDeviceCaps(&caps));
        printf("Quality caps: max_anisotropy=%lu\n",caps.MaxAnisotropy);
        const DXGI_FORMAT formats[]={DXGI_FORMAT_B8G8R8X8_UNORM,DXGI_FORMAT_D24_UNORM_S8_UINT};
        const UINT sampleCounts[]={2,4,8};
        for(auto format:formats)for(UINT samples:sampleCounts) {
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality={format,samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            HRESULT result=d12->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof quality);
            printf("Quality caps: format=%u samples=%u levels=%u hr=%08lx\n",format,samples,quality.NumQualityLevels,result);
        }
        if(native) {
            queue=renderTestNativeQueue();
            if(!queue) {puts("FAIL production shared native queue unavailable");return 1;}
            puts("PASS production factory/device shared native queue");
        } else {
            D3D12_COMMAND_QUEUE_DESC q={};CHECK(d12->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
        }
    }
    printf("Test native: requested=%d shared_queue=%d\n",native,renderTestNativeQueue()!=nullptr);
    char probe[32]={};
    if(bridge && GetEnvironmentVariableA("WORLDMOD_DEFAULT_PROBE",probe,sizeof probe)){
        for(int i=0;i<12;++i){testDefaultTextureContract(d9.Get(),d12.Get(),queue.Get(),i);renderBeforeReset();HRESULT hr=d9->Reset(&pp);renderAfterReset(d9.Get(),hr,pp);CHECK(hr);}
        printf("PASS isolated DEFAULT probe mode=%s cycles=12\n",probe);return 0;
    }
    ComPtr<IDirect3DTexture9> retained;
    if(native)CHECK(d9->CreateTexture(64,64,0,0,D3DFMT_DXT5,D3DPOOL_MANAGED,&retained,nullptr));
    for (int iteration = 0; iteration < 3; ++iteration) {
        testTerrainFilter(d9.Get());
        if (!bridge) {
            renderBeforeReset(); HRESULT reset=d9->Reset(&pp);renderAfterReset(d9.Get(),reset,pp);CHECK(reset);
            printf("PASS system D3D9 filter and Reset iteration=%d\n", iteration);
            continue;
        }
        if(native) {
            testTerrain12(d9.Get(),d12.Get(),queue.Get(),retained.Get(),iteration);
            testCpuProfile(iteration);
            SetEnvironmentVariableW(L"WORLDMOD_TERRAIN_PROFILE",iteration==0?L"1":nullptr);
        }
        testDefaultTextureContract(d9.Get(),d12.Get(),queue.Get(),iteration);
        ComPtr<IDirect3DSurface9> color9, depth9;
        CHECK(d9->GetRenderTarget(0, &color9)); CHECK(d9->GetDepthStencilSurface(&depth9));
        CHECK(d9->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1, 0));
        ComPtr<ID3D12Resource> color12, depth12;
        CHECK(bridge->UnwrapUnderlyingResource(color9.Get(), queue.Get(), IID_PPV_ARGS(&color12)));
        HRESULT depthHr = bridge->UnwrapUnderlyingResource(depth9.Get(), queue.Get(), IID_PPV_ARGS(&depth12));
        CHECK(depthHr);
        CHECK(bridge->ReturnUnderlyingResource(depth9.Get(), 0, nullptr, nullptr));
        ComPtr<ID3D12DescriptorHeap> heap;
        D3D12_DESCRIPTOR_HEAP_DESC hd = {}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 1;
        CHECK(d12->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)));
        auto rtv = heap->GetCPUDescriptorHandleForHeapStart();
        d12->CreateRenderTargetView(color12.Get(), nullptr, rtv);
        ComPtr<ID3D12CommandAllocator> allocator;
        CHECK(d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
        ComPtr<ID3D12GraphicsCommandList> commands;
        CHECK(d12->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commands)));
        D3D12_RESOURCE_BARRIER b = {}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = color12.Get(); b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commands->ResourceBarrier(1, &b);
        const float rgba[] = {0.25f, 0.5f, 0.75f, 1.0f};
        commands->ClearRenderTargetView(rtv, rgba, 0, nullptr);
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        commands->ResourceBarrier(1, &b); CHECK(commands->Close());
        ID3D12CommandList* lists[] = {commands.Get()}; queue->ExecuteCommandLists(1, lists);
        ComPtr<ID3D12Fence> fence; CHECK(d12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        CHECK(queue->Signal(fence.Get(), 1)); UINT64 value = 1; ID3D12Fence* fences[] = {fence.Get()};
        CHECK(bridge->ReturnUnderlyingResource(color9.Get(), 1, &value, fences));
        ComPtr<IDirect3DSurface9> readback;
        CHECK(d9->CreateOffscreenPlainSurface(256, 256, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &readback, nullptr));
        CHECK(d9->GetRenderTargetData(color9.Get(), readback.Get()));
        D3DLOCKED_RECT pixels; CHECK(readback->LockRect(&pixels, nullptr, D3DLOCK_READONLY));
        DWORD pixel = *(DWORD*)pixels.pBits & 0xffffff;
        CHECK(readback->UnlockRect());
        // UNORM conversion may round a half-way value either way across drivers.
        if (abs(int((pixel >> 16) & 255) - 64) > 1 ||
            abs(int((pixel >> 8) & 255) - 128) > 1 || abs(int(pixel & 255) - 191) > 1) {
            printf("FAIL native clear readback=%06lx\n", pixel); return 1;
        }
        printf("PASS native clear/return/readback iteration=%d pixel=%06lx\n", iteration, pixel);
        // Release all references to default-pool surfaces before Reset.
        color12.Reset(); depth12.Reset(); color9.Reset(); depth9.Reset(); readback.Reset();
        renderBeforeReset(); HRESULT reset=d9->Reset(&pp);renderAfterReset(d9.Get(),reset,pp);CHECK(reset);
        if(native && renderTestNativeQueue()!=queue.Get()) {puts("FAIL shared queue changed after Reset");return 1;}
    }
    if(native) {
        // Exercise production initialization and pre-Reset release, including 4x default.
        const int copyCounts[]={0,2,4,8,0};
        for(int count:copyCounts) {
            renderBeforeReset();
            if(count==4)renderConfigure(L"9on12",L"dx12",nullptr,false,0,0);
            else renderConfigure(L"9on12",L"dx12",nullptr,false,0,0,count);
            HRESULT hr=d9->Reset(&pp);CHECK(hr);renderAfterReset(d9.Get(),hr,pp);
            ComPtr<IDirect3DSurface9> target,back,depth;
            CHECK(d9->GetRenderTarget(0,&target));CHECK(d9->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));CHECK(d9->GetDepthStencilSurface(&depth));
            D3DSURFACE_DESC td,bd,zd;CHECK(target->GetDesc(&td));CHECK(back->GetDesc(&bd));CHECK(depth->GetDesc(&zd));
            if(td.MultiSampleType!=count || zd.MultiSampleType!=count || bd.MultiSampleType!=D3DMULTISAMPLE_NONE) {
                puts("FAIL production COPY target/depth/backbuffer sample counts");return 1;
            }
            CHECK(d9->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0));CHECK(copyMsaaResolve(d9.Get()));
            CHECK(d9->Present(nullptr,nullptr,nullptr,nullptr));
            ComPtr<IDirect3DSurface9> readback;CHECK(d9->CreateOffscreenPlainSurface(256,256,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr));
            CHECK(renderReadback(d9.Get(),target.Get(),readback.Get()));
            D3DLOCKED_RECT pixels;CHECK(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY));
            bool correct=(*(DWORD*)pixels.pBits&0xffffff)==0x123456;CHECK(readback->UnlockRect());
            if(!correct){puts("FAIL production COPY clear/readback");return 1;}
            printf("PASS production COPY MSAA=%d Reset/resolve/Present/readback\n",count);
        }
        testCopyMsaaHooks(api.Get(),adapter,pp);
        auto gameDepth=pp;gameDepth.AutoDepthStencilFormat=D3DFMT_D24X8;
        testCopyMsaaHooks(api.Get(),adapter,gameDepth);
        testCopyMsaaTransitions(api.Get(),adapter,gameDepth);
        testCopyMsaaFailedReset(api.Get(),adapter,gameDepth);
        // A fallback notification must prevent reusing a queue from the old factory,
        // even if the replacement happens to expose the same D3D12 device identity.
        renderDeviceFallback(E_FAIL);renderOnDevice(api.Get(),adapter,d9.Get(),pp);
        if(renderTestNativeQueue() || terrain12Draw(d9.Get(),D3DPT_TRIANGLELIST,0,0,4,0,2)) {
            puts("FAIL fallback retained native renderer");return 1;
        }
        puts("PASS production fallback disables native queue");
    }
    puts(bridge ? "PASS D3D9On12 color interop and Reset" : "PASS system D3D9 filtering and Reset");
    return 0;
}
