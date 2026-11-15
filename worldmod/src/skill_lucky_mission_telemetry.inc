// Only the actual executor CALLs own these result classifications. Direct
// calculator calls made by candidate/UI/AI code remain predicate queries.
typedef int(__cdecl* LuckyMissionCalc3)(void*,void*,int);
typedef int(__cdecl* LuckyMissionCalc4)(void*,void*,void*,int);
LuckyMissionCalc3 g_luckyMissionFirstOriginal=(LuckyMissionCalc3)0x5b79a0;
LuckyMissionCalc4 g_luckyMissionSecondOriginal=(LuckyMissionCalc4)0x5bf650;
LuckyMissionCalc3 g_luckyMissionThirdOriginal=(LuckyMissionCalc3)0x5d05f0;
const uint32_t g_luckyMissionReturns[]={0x5b7b77,0x5bffb0,0x5d1555};
const uint32_t g_luckyMissionPredicates[]={0x5b7aa9,0x5bf759,0x5d073b};
template<typename Calculate> int luckyMissionResult(void* person,int family,uintptr_t caller,Calculate calculate){
    if(!skilltelemetry::enabled() || caller!=g_strategyCodeBase+g_luckyMissionReturns[family]-0x400000)return calculate();
    PersonReceipt provider(person),observed;
    if(!provider.valid(32))return calculate();
    ProgressionProbe probe(32,&observed);probe.scene="mission";probe.event="lucky_result_eligibility";
    probe.nativeCaller=g_strategyCodeBase+g_luckyMissionPredicates[family]-0x400000;
    int result=calculate();
    auto live=[&]{return result==2 && provider.valid(32) && observed.person==provider.person &&
        observed.owned==provider.owned && observed.valid(32);};
    if(live()){
        auto target=growthTarget((uint8_t*)person,provider.id);char context[224];
        sprintf_s(context,"{\"result_family\":%d,\"native_result_code\":2,\"result_scope\":\"actual_mission_classifier_dispatch\",\"counterfactual_evaluated\":false}",family+1);
        target.context=context;
        if(live())skilltelemetry::appliedEvent(person,provider.owned,32,"mission","mission_lucky_result_classified",
            g_luckyMissionReturns[family],target);
    }
    return result;
}
int __cdecl luckyMissionFirst(void* person,void* other,int roll){
    return luckyMissionResult(person,0,(uintptr_t)_ReturnAddress(),[&]{return g_luckyMissionFirstOriginal(person,other,roll);});
}
int __cdecl luckyMissionSecond(void* person,void* first,void* second,int roll){
    return luckyMissionResult(person,1,(uintptr_t)_ReturnAddress(),[&]{return g_luckyMissionSecondOriginal(person,first,second,roll);});
}
int __cdecl luckyMissionThird(void* person,void* other,int roll){
    return luckyMissionResult(person,2,(uintptr_t)_ReturnAddress(),[&]{return g_luckyMissionThirdOriginal(person,other,roll);});
}
const struct LuckyMissionLayout {uint32_t start;int size;const char* bytes;} g_luckyMissionLayouts[]={
    {0x5b7aa0,17,"\x6a\x20\x8b\xcf\xe8\x47\x16\xed\xff\x85\xc0\x0f\x85\x63\xff\xff\xff"},
    {0x5bf750,17,"\x6a\x20\x8b\xce\xe8\x97\x99\xec\xff\x85\xc0\x0f\x85\x5a\xff\xff\xff"},
    {0x5d0732,17,"\x6a\x20\x8b\xce\xe8\xb5\x89\xeb\xff\x85\xc0\x0f\x85\x36\xff\xff\xff"},
    {0x5b7a14,13,"\x5b\x5d\x5f\xb8\x02\x00\x00\x00\x5e\x83\xc4\x0c\xc3"},
    {0x5bf6bb,11,"\x5d\x5f\x5e\xb8\x02\x00\x00\x00\x5b\x59\xc3"},
    {0x5d0679,13,"\x5d\x5f\x5e\xb8\x02\x00\x00\x00\x5b\x83\xc4\x08\xc3"},
    {0x5b7b77,15,"\x83\xc4\x0c\x83\xf8\x03\x77\x3a\xff\x24\x85\xc8\x7b\x5b\x00"},
    {0x5bffb0,15,"\x83\xc4\x10\x83\xf8\x03\x77\x4e\xff\x24\x85\x18\x00\x5c\x00"},
    {0x5d1555,15,"\x83\xc4\x0c\x83\xf8\x03\x77\x47\xff\x24\x85\xb4\x15\x5d\x00"},
    {0x5b7bd0,4,"\x9a\x7b\x5b\x00"},
    {0x5c0020,4,"\xe7\xff\x5b\x00"},
    {0x5d15bc,4,"\x88\x15\x5d\x00"},
};
bool verifyLuckyMissionLayout(){
    for(const auto& s:g_luckyMissionLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
