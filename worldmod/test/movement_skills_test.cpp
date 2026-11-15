// Reproduce the initialization conflict using real executable instructions, without a game.
#define g_log movementLog
#include "../src/movement.cpp"
#undef g_log
#include "../src/skills.cpp"
#include <assert.h>
#include "steering_telemetry_cases.inc"
static uintptr_t g_boundEntry;
__declspec(naked) static int __cdecl checkTuntianBound(int holder) {
    __asm {
        push ebx
        mov eax,[esp+8]
        xor ebx,ebx
        call dword ptr [g_boundEntry]
        pop ebx
        ret
    }
}
int wmain(int argc,wchar_t** argv) {
    assert(argc==3);
    FILE* f=nullptr;assert(!_wfopen_s(&f,argv[1],L"rb") && f);
    fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);
    std::vector<uint8_t> bytes(size);assert(fread(bytes.data(),1,size,f)==size);fclose(f);
    auto dos=(IMAGE_DOS_HEADER*)bytes.data();
    auto nt=(IMAGE_NT_HEADERS32*)(bytes.data()+dos->e_lfanew);
    assert(nt->OptionalHeader.ImageBase==0x400000);
    size_t imageSize=nt->OptionalHeader.SizeOfImage;
    auto code=(uint8_t*)VirtualAlloc(nullptr,imageSize,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(code);
    g_strategyCodeBase=(uintptr_t)code;
    auto sections=IMAGE_FIRST_SECTION(nt);
    for(int i=0;i<nt->FileHeader.NumberOfSections;++i) {
        auto& s=sections[i];if(memcmp(s.Name,".text",5) && memcmp(s.Name,".rdata",6))continue;
        assert(s.VirtualAddress+s.SizeOfRawData<=imageSize && s.PointerToRawData+s.SizeOfRawData<=bytes.size());
        memcpy(code+s.VirtualAddress,bytes.data()+s.PointerToRawData,s.SizeOfRawData);
    }
    for(auto& h:g_hooks)h.at=(uint32_t)(uintptr_t)(code+h.at-0x400000);
    auto matches=[&](const Site& s) {return !memcmp(code+s.at-0x400000,s.old,s.len);};
    assert(skillsPrepare(argv[2],nullptr));
    for(const auto& s:SITES)assert(matches(s));
    puts("PASS movement prerequisites verify before skills installs its hooks");
    for(const auto& layout:g_precisionLayouts) {
        auto at=code+layout.start-0x400000;at[0]^=1;assert(!verifyPrecisionLayout());at[0]^=1;assert(verifyPrecisionLayout());
    }
    puts("PASS precision native reward/halving/points-setter layout guards reject modified code");
    auto directArea=(uint32_t*)(code+0x588cea-0x400000),uiArea=(uint32_t*)(code+0x58846a-0x400000);
    uint32_t oldArea=*directArea;assert(oldArea==*uiArea);
    *directArea=0x12345000;assert(!verify());*uiArea=*directArea;assert(verify());
    *directArea=*uiArea=0;assert(!verify());*directArea=*uiArea=oldArea;assert(verify());
    puts("PASS M7 paired AREAHEX relocation accepted; mismatched/null consumers rejected");
    // M9 commits its supported long-false-report setter cave before the skill
    // installer re-verifies. Unknown payloads must still reject installation.
    auto setter=code+0x496350-0x400000;
    auto statusCave=(uint8_t*)VirtualAlloc(nullptr,33,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(statusCave);
    const uint8_t statusCode[]={0x8a,0x44,0x24,0x04,0x83,0x79,0x24,0x02,0x75,0x0c,0x3c,0x1e,0x76,0x02,0xb0,0x1e,0x88,0x41,0x28,0xc2,0x04,0x00,0x8a,0x44,0x24,0x04,0x84,0xc0,0xe9};
    memcpy(statusCave,statusCode,sizeof statusCode);*(int32_t*)(statusCave+29)=(int32_t)(setter+6-statusCave-33);
    setter[0]=0xe9;*(int32_t*)(setter+1)=(int32_t)(statusCave-setter-5);setter[5]=0x90;
    assert(verify());statusCave[7]^=1;assert(!verify());statusCave[7]^=1;assert(verify());
    puts("PASS M9 supported status setter cave passes skill re-verification; changed payload is rejected");
    auto treasury=code+0x484e06-0x400000;
    auto treasuryCave=(uint8_t*)VirtualAlloc(nullptr,16,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(treasuryCave);
    const uint8_t treasuryCode[]={0x3d,0,0,0,0,0x0f,0x8f,0,0,0,0,0xe9,0,0,0,0};
    memcpy(treasuryCave,treasuryCode,16);*(int*)(treasuryCave+1)=529;
    *(int32_t*)(treasuryCave+7)=(int32_t)(treasury+8-treasuryCave-11);
    *(int32_t*)(treasuryCave+12)=(int32_t)(treasury+5-treasuryCave-16);
    treasury[0]=0xe9;*(int32_t*)(treasury+1)=(int32_t)(treasuryCave-treasury-5);assert(verify());
    treasuryCave[5]^=1;assert(!verify());treasuryCave[5]^=1;
    ++*(int32_t*)(treasuryCave+12);assert(!verify());--*(int32_t*)(treasuryCave+12);
    *(int*)(treasuryCave+1)=40;assert(!verify());*(int*)(treasuryCave+1)=529;assert(verify());
    uint8_t item[0x54]={};typedef void(__thiscall* ItemOwner)(void*,int,int);auto setOwner=(ItemOwner)(code+0x484de0-0x400000);
    for(int force:{-1,0,41,42,529,530}){setOwner(item,-1,force);
        assert(*(int*)(item+0x40)==-1 && *(int*)(item+0x44)==(force>=0 && force<=529?force:-1));}
    puts("PASS expanded item treasury cave: complete payload/returns, rejection cases and actual force-bound stores");
    // The real installer commits M10 between skillsPrepare and skillsInstall.
    g_personLimitOperand=(const int*)(code+0x490b09-0x400000);
    *(int*)(code+0x490b09-0x400000)=14612;
    *(int*)(code+0x59bfd5-0x400000)=14612;
    assert(!verify()); // the actual item recipient setter is also patched by M10
    *(int*)(code+0x484df2-0x400000)=14612;
    assert(verify());
    for(int owner:{1099,1100,14162,14612,14613}){setOwner(item,owner,529);
        assert(*(int*)(item+0x40)==(owner<=14612?owner:-1) && *(int*)(item+0x44)==(owner<=14612?-1:529));}
    assert(skillsInstall());
    assert(g_tuntianMax==14612);
    g_tuntianSkip=(uint32_t)(code+0x59c014-0x400000);g_tuntianConsume=(uint32_t)(code+0x59bfdb-0x400000);
    memcpy((void*)g_tuntianSkip,"\xb8\x01\x00\x00\x00\xc3",6);
    memcpy((void*)g_tuntianConsume,"\x33\xc0\xc3",3);g_boundEntry=(uintptr_t)(code+0x59bfd0-0x400000);
    for(int holder:{1099,1100,14162,14612})assert(checkTuntianBound(holder)==1);
    assert(checkTuntianBound(-1)==0 && checkTuntianBound(14613)==0);
    puts("PASS M10 prepare/commit/install order and native Tuntian branch retain the installed 14612 holder limit");
    int conflicts=0;
    for(const auto& s:SITES)if(!matches(s)) {assert(s.at==0x4890f0);++conflicts;}
    assert(conflicts==1);
    puts("PASS old skills-first order reproduces the exact 0x4890f0 conflict; other movement prerequisites remain intact");
    steeringTelemetryTests(code);
    VirtualFree(treasuryCave,0,MEM_RELEASE);VirtualFree(statusCave,0,MEM_RELEASE);VirtualFree(code,0,MEM_RELEASE);return 0;
}
