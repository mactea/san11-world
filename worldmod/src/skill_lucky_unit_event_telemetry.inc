//58308a is the actual unit-event member selector, followed immediately by the
// zero-result branch around candidate append. A standalone580ab0 call is a query.
typedef int(__cdecl* LuckyUnitSelector)(void*);
LuckyUnitSelector g_luckyUnitSelectorOriginal=(LuckyUnitSelector)0x580ab0;
int __cdecl luckyUnitEvent(void* person,void* unit,uintptr_t caller){
    if(!skilltelemetry::enabled() || caller!=g_strategyCodeBase+0x18308f)return g_luckyUnitSelectorOriginal(person);
    TrapObject source(unit);PersonReceipt provider(person);
    ProgressionProbe probe(32);probe.scene="unit_event";probe.event="casualty_candidate_eligibility";
    int result=g_luckyUnitSelectorOriginal(person);
    bool member=false;for(int id:source.ids)if(id==provider.id)member=true;
    if(!result && probe.actor==person && source.unit>=0 && member && source.live() && provider.valid(32)){
        auto target=unitTarget(unit);target.person=provider.id;target.equipment=source.weapon;
        target.x=(int16_t)source.position;target.y=(int16_t)(source.position>>16);
        target.context="{\"selector_call\":5779594,\"selector_result\":0,\"result_scope\":\"excluded_from_unit_event_candidate_list\",\"counterfactual_evaluated\":false}";
        if(source.live() && provider.valid(32))skilltelemetry::appliedEvent(person,provider.owned,32,"unit_event",
            "unit_event_casualty_candidate_excluded",0x58308f,target);
    }
    return result;
}
__declspec(naked) void luckyUnitEventBridge(){__asm {mov eax,esp
    push [eax]
    push esi
    push [eax+4]
    call luckyUnitEvent
    add esp,12
    ret}}
const struct LuckyUnitEventLayout {uint32_t start;int size;const char* bytes;} g_luckyUnitEventLayouts[]={
    {0x580ab0,22,"\x55\x8b\x6c\x24\x08\x6a\x20\x8b\xcd\xe8\x32\x86\xf0\xff\x85\xc0\x74\x04\x33\xc0\x5d\xc3"},
    {0x58308f,17,"\x83\xc4\x04\x85\xc0\x74\x0a\x57\x8d\x4c\x24\x18\xe8\x10\x91\xef\xff"},
};
bool verifyLuckyUnitEventLayout(){
    for(const auto& s:g_luckyUnitEventLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
