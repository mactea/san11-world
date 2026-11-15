typedef int(__thiscall* LuckyDuelRoll)(void*,int*);
typedef uint8_t*(__thiscall* LuckyDuelPerson)(void*,int,int);
LuckyDuelRoll g_luckyDuelRollOriginal=(LuckyDuelRoll)0x509bb0;
LuckyDuelPerson g_luckyDuelPersonOriginal=(LuckyDuelPerson)0x505f70;
int __fastcall luckyDuelRoll(uint8_t* duel,void*,int* option){
    uintptr_t caller=(uintptr_t)_ReturnAddress();
    bool resetPath=caller==g_strategyCodeBase+0x10aadf;
    if(!skilltelemetry::enabled() || (!resetPath && caller!=g_strategyCodeBase+0x10ab61))return g_luckyDuelRollOriginal(duel,option);
    int side=*(int*)(duel+0x578),action=*(int*)(duel+0x580),record=*(int*)(duel+0x4c4);
    int slots[]={*(int*)(duel+0xe8),*(int*)(duel+0x1d4)};
    if(option!=(int*)(duel+0x578) || side<0 || side>1 || slots[0]<0 || slots[0]>3 || slots[1]<0 || slots[1]>3 ||
       (resetPath?action!=5:action!=4 && action!=7))return g_luckyDuelRollOriginal(duel,option);
    PersonReceipt attacker(g_luckyDuelPersonOriginal(duel,side,slots[side]));
    PersonReceipt provider(g_luckyDuelPersonOriginal(duel,1-side,slots[1-side])),observed;
    if(!attacker.valid() || !provider.valid(32))return g_luckyDuelRollOriginal(duel,option);
    ProgressionProbe probe(32,&observed);probe.scene="duel";probe.event="lucky_roll_eligibility";
    probe.nativeCaller=g_strategyCodeBase+0x109d7e;
    int result=g_luckyDuelRollOriginal(duel,option); // native32 sets chance0; do not replay its RNG
    auto live=[&]{return !result && *(int*)(duel+0x578)==side && *(int*)(duel+0x580)==action && *(int*)(duel+0x4c4)==record &&
        *(int*)(duel+0xe8)==slots[0] && *(int*)(duel+0x1d4)==slots[1] && attacker.valid() && provider.valid(32) &&
        observed.person==provider.person && observed.owned==provider.owned && observed.valid(32) &&
        g_luckyDuelPersonOriginal(duel,side,slots[side])==attacker.person &&
        g_luckyDuelPersonOriginal(duel,1-side,slots[1-side])==provider.person;};
    if(live()){
        auto target=growthTarget((uint8_t*)provider.person,provider.id);char context[288];
        sprintf_s(context,"{\"duel_action\":%d,\"current_side\":%d,\"opponent_person\":%d,\"record_index\":%d,\"selector_result\":0,\"result_scope\":\"native_duel_action_result_flag\",\"counterfactual_evaluated\":false}",
            action,side,attacker.id,record);target.context=context;
        if(live())skilltelemetry::appliedEvent(provider.person,provider.owned,32,"duel","duel_lucky_roll_suppressed",
            resetPath?0x50aadf:0x50ab61,target);
    }
    return result;
}
const struct LuckyDuelLayout {uint32_t start;int size;const char* bytes;} g_luckyDuelLayouts[]={
    {0x509d75,34,"\x6a\x20\x8b\xcf\xe8\x72\xf3\xf7\xff\x85\xc0\x74\x02\x33\xf6\x56\xe8\x46\x84\xf6\xff\x83\xc4\x04\x5e\x5f\x5d\x5b\x83\xc4\x08\xc2\x04\x00"},
    {0x50aada,14,"\xe8\xd1\xf0\xff\xff\x33\xc9\x85\xc0\x0f\x9f\xc1\x8b\xe9"},
    {0x50ab5c,9,"\xe8\x4f\xf0\xff\xff\x8b\xe8\xeb\x9f"},
    {0x50ab70,28,"\xae\xaa\x50\x00\xbe\xaa\x50\x00\x04\xab\x50\x00\x53\xab\x50\x00\xd1\xaa\x50\x00\xe8\xaa\x50\x00\x53\xab\x50\x00"},
};
bool verifyLuckyDuelLayout(){
    for(const auto& s:g_luckyDuelLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
