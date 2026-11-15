typedef int(__thiscall* LuckyDuelResponseCalc)(void*);
uint32_t g_luckyDuelResponseResume=0x509a99;
__declspec(naked) void luckyDuelResponseNative(){__asm {push esi
    mov esi,ecx
    lea eax,[esi+0x578]
    jmp dword ptr [g_luckyDuelResponseResume]}}
LuckyDuelResponseCalc g_luckyDuelResponseOriginal=(LuckyDuelResponseCalc)luckyDuelResponseNative;
int __fastcall luckyDuelResponse(uint8_t* duel,void*){
    uintptr_t caller=(uintptr_t)_ReturnAddress();bool alternate=caller==g_strategyCodeBase+0x10ec01;
    if(!skilltelemetry::enabled() || (!alternate && caller!=g_strategyCodeBase+0x10a260))return g_luckyDuelResponseOriginal(duel);
    uint64_t generation=skilltelemetry::contextGeneration();
    int side=*(int*)(duel+0x578),slot=*(int*)(duel+0x57c),phase=*(int*)(duel+0xc),alternatePhase=*(int*)(duel+0x59c);
    int before=*(int*)(duel+0x584);
    if(side<0 || side>1 || slot<0 || slot>3 || *(int*)(duel+0x580)!=3)return g_luckyDuelResponseOriginal(duel);
    PersonReceipt provider(g_luckyDuelPersonOriginal(duel,side,slot)),observed;
    if(!provider.valid(32) || generation!=skilltelemetry::contextGeneration())return g_luckyDuelResponseOriginal(duel);
    ProgressionProbe probe(32,&observed);probe.scene="duel";probe.event="lucky_response_eligibility";
    probe.nativeCaller=g_strategyCodeBase+0x106ab4;
    int result=g_luckyDuelResponseOriginal(duel);
    auto live=[&]{return result==0 && *(int*)(duel+0x584)==0 && generation==skilltelemetry::contextGeneration() &&
        *(int*)(duel+0x578)==side && *(int*)(duel+0x57c)==slot && *(int*)(duel+0x580)==3 &&
        *(int*)(duel+0xc)==phase && *(int*)(duel+0x59c)==alternatePhase && provider.valid(32) &&
        observed.person==provider.person && observed.owned==provider.owned && observed.valid(32) &&
        g_luckyDuelPersonOriginal(duel,side,slot)==provider.person;};
    if(live()){
        auto target=growthTarget((uint8_t*)provider.person,provider.id);char context[256];
        sprintf_s(context,"{\"duel_action\":3,\"current_side\":%d,\"selected_slot\":%d,\"previous_response_code\":%d,\"response_code\":0,\"controller_phase\":%d,\"result_scope\":\"native_duel_response_cache\",\"counterfactual_evaluated\":false}",
            side,slot,before,alternate?alternatePhase:phase);target.context=context;
        if(live())skilltelemetry::appliedEvent(provider.person,provider.owned,32,"duel","duel_lucky_response_cached",
            alternate?0x50ec01:0x50a260,target);
    }
    return result;
}
const struct LuckyDuelResponseLayout {uint32_t start;int size;const char* bytes;} g_luckyDuelResponseLayouts[]={
    {0x509a9f,8,"\x89\x86\x84\x05\x00\x00\x5e\xc3"},
    {0x507e02,13,"\xe8\x19\xec\xff\xff\xf7\xd8\x1b\xc0\x40\xc2\x04\x00"},
    {0x506aa9,15,"\x8b\x4c\x24\x10\x6a\x20\xe8\x3c\x26\xf8\xff\x85\xc0\x75\x42"},
    {0x506afa,15,"\x5e\x5f\x5d\xb8\x01\x00\x00\x00\x5b\x83\xc4\x08\xc2\x08\x00"},
    {0x50a25b,14,"\xe8\x30\xf8\xff\xff\xc7\x46\x0c\x04\x00\x00\x00\xeb\x07"},
    {0x50ebfc,16,"\xe8\x8f\xae\xff\xff\x5f\xc7\x86\x9c\x05\x00\x00\x04\x00\x00\x00"},
};
bool verifyLuckyDuelResponseLayout(){
    for(const auto& s:g_luckyDuelResponseLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
