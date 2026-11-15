#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/terrain_filter.h"
#include <stdlib.h>
#include <string.h>

namespace {
unsigned failMask = 0, calls = 0;
void require(bool value, const char* label) {
    if (!value) { printf("FAIL filter faults: %s\n", label); exit(1); }
}
}

HRESULT terrainFilterTestSetState(IDirect3DDevice9* d, D3DSAMPLERSTATETYPE state, DWORD value) {
    if (failMask && (failMask & (1u << calls++))) return D3DERR_INVALIDCALL;
    return d->SetSamplerState(0, state, value);
}

// Called with an eligible real texture/shader bound by the integration fixture.
void testTerrainFilterFaults(IDirect3DDevice9* d, IDirect3DVertexShader9* shader, UINT limit) {
    struct Case { unsigned mask, writes, errors; DWORD min, max; };
    const Case cases[] = {
        {1,1,1,D3DTEXF_LINEAR,1},                 // First apply fails; nothing changed.
        {2,3,1,D3DTEXF_LINEAR,1},                 // Second apply fails; rollback succeeds.
        {6,3,2,D3DTEXF_LINEAR,limit},             // Second apply and rollback both fail.
        {4,4,1,D3DTEXF_ANISOTROPIC,1},            // MIN restore fails; MAX restore still attempted.
        {8,4,1,D3DTEXF_LINEAR,limit},             // MAX restore fails; MIN was restored.
        {12,4,2,D3DTEXF_ANISOTROPIC,limit},        // Both restores fail.
    };
    for (const auto& c : cases) {
        require(SUCCEEDED(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR)), "fixture MIN");
        require(SUCCEEDED(d->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,1)), "fixture MAX");
        terrainFilterConfigure(8, nullptr); terrainFilterOnDevice(d);
        calls = 0; failMask = c.mask;
        { TerrainFilterScope filter; filter.apply(d, shader); }
        require(calls == c.writes, "expected rollback/restore calls");
        require(!terrainFilterEnabled(), "disable after failure");
        { TerrainFilterScope filter; filter.apply(d, shader); }
        require(calls == c.writes, "no subsequent writes while disabled");
        failMask = 0;
        DWORD min = 0, max = 0;
        require(SUCCEEDED(d->GetSamplerState(0,D3DSAMP_MINFILTER,&min)), "read MIN");
        require(SUCCEEDED(d->GetSamplerState(0,D3DSAMP_MAXANISOTROPY,&max)), "read MAX");
        require(min == c.min && max == c.max, "exact state after injected failures");
        FILE* file = nullptr; require(!tmpfile_s(&file) && file, "status capture");
        terrainFilterStatus(file); rewind(file); char status[512] = {}; fgets(status,sizeof status,file); fclose(file);
        char errorCount[32]; sprintf_s(errorCount,"errors=%u ",c.errors);
        require(strstr(status,errorCount) && strstr(status,"last_error=8876086c"), "persist error count/HRESULT");
        terrainFilterClearCounters();
        require(!terrainFilterEnabled(), "clearing metrics cannot re-enable a failed override");
        // A driver rejecting restoration can leave state changed. Repair the fixture explicitly;
        // production must not claim that disabling the override repaired that failed driver call.
        require(SUCCEEDED(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR)), "repair MIN");
        require(SUCCEEDED(d->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,1)), "repair MAX");
        terrainFilterReset(); terrainFilterOnDevice(d);
        require(terrainFilterEnabled(), "re-enable after device lifecycle reinitialization");
    }
    puts("PASS terrain filter: six injected apply/rollback/restore failure cases");
}
