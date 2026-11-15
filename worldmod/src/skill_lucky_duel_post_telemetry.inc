typedef int(__thiscall* LuckyDuelPostCheck)(void*,int);
LuckyDuelPostCheck g_luckyDuelPostOriginal[]={(LuckyDuelPostCheck)0x509130,(LuckyDuelPostCheck)0x509240};
int luckyDuelPostCheck(uint8_t* duel,int side,int kind,uintptr_t caller){
    auto original=g_luckyDuelPostOriginal[kind];
    if(!skilltelemetry::enabled() || caller!=g_strategyCodeBase+(kind?0x10a5ab:0x10a52e))return original(duel,side);
    uint64_t generation=skilltelemetry::contextGeneration();
    int phase=*(int*)(duel+0xc),action=*(int*)(duel+0x580),mode=*(int*)(duel+0x1c);
    int slots[]={*(int*)(duel+0xe8),*(int*)(duel+0x1d4)};
    if(side<0 || side>1 || *(int*)(duel+0x58c)!=side || slots[side]<0 || slots[side]>3)return original(duel,side);
    PersonReceipt provider(g_luckyDuelPersonOriginal(duel,side,slots[side])),observed;
    if(!provider.valid(32) || generation!=skilltelemetry::contextGeneration())return original(duel,side);
    ProgressionProbe probe(32,&observed);probe.scene="duel";probe.event="lucky_postcheck_eligibility";
    probe.nativeCaller=g_strategyCodeBase+(kind?0x1092c1:0x1091a3);
    int result=original(duel,side);
    auto live=[&]{return !result && generation==skilltelemetry::contextGeneration() && *(int*)(duel+0x58c)==side &&
        *(int*)(duel+0xc)==phase && *(int*)(duel+0x580)==action && *(int*)(duel+0x1c)==mode &&
        *(int*)(duel+0xe8)==slots[0] && *(int*)(duel+0x1d4)==slots[1] && provider.valid(32) &&
        observed.person==provider.person && observed.owned==provider.owned && observed.valid(32) &&
        g_luckyDuelPersonOriginal(duel,side,slots[side])==provider.person;};
    if(live()){
        auto target=growthTarget((uint8_t*)provider.person,provider.id);char context[256];
        sprintf_s(context,"{\"check_id\":%d,\"selected_side\":%d,\"selected_slot\":%d,\"controller_phase\":%d,\"duel_action\":%d,\"native_result\":0,\"result_scope\":\"native_duel_followup_check\",\"counterfactual_evaluated\":false}",
            kind+1,side,slots[side],phase,action);target.context=context;
        if(live())skilltelemetry::appliedEvent(provider.person,provider.owned,32,"duel","duel_lucky_postcheck_blocked",
            kind?0x50a5ab:0x50a52e,target);
    }
    return result;
}
int __fastcall luckyDuelPostFirst(uint8_t* duel,void*,int side){return luckyDuelPostCheck(duel,side,0,(uintptr_t)_ReturnAddress());}
int __fastcall luckyDuelPostSecond(uint8_t* duel,void*,int side){return luckyDuelPostCheck(duel,side,1,(uintptr_t)_ReturnAddress());}
const struct LuckyDuelPostLayout {uint32_t start;int size;const char* bytes;} g_luckyDuelPostLayouts[]={
    {0x50919a,22,"\x6a\x20\x8b\xcd\xe8\x4d\xff\xf7\xff\x85\xc0\x74\x09\x5d\x5f\x5e\x33\xc0\x5b\xc2\x04\x00"},
    {0x5092b8,22,"\x6a\x20\x8b\xcd\xe8\x2f\xfe\xf7\xff\x85\xc0\x74\x09\x5d\x5e\x5b\x33\xc0\x5f\xc2\x04\x00"},
    {0x50a529,9,"\xe8\x02\xec\xff\xff\x85\xc0\x74\x47"},
    {0x50a5a6,9,"\xe8\x95\xec\xff\xff\x85\xc0\x74\x42"},
};
bool verifyLuckyDuelPostLayout(){
    for(const auto& s:g_luckyDuelPostLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
