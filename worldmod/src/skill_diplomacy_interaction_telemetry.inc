// Actual native modal-state writes precede the UI loop and can occur even when
// the surrounding command later returns0. They do not prove a debate started.
typedef void(__thiscall* DiplomacyModalClear)(void*);
DiplomacyModalClear g_diplomacyModalClearOriginal=(DiplomacyModalClear)0x47be50;
uint8_t* g_diplomacyModalState=(uint8_t*)0x9c57074;
void __fastcall diplomacyInteractionState(void* self,void*){
    auto scope=g_diplomacyInteractionScope;auto execution=scope?scope->execution:nullptr;
    if(skilltelemetry::enabled() && (uintptr_t)_ReturnAddress()==g_strategyCodeBase+0x1bd7c4 &&
       execution && execution==g_diplomacyExecution && execution->command==scope->command &&
       scope->initial==1 && execution->initial==1 && execution->valid() && self==g_diplomacyModalState+4 &&
       *(int*)g_diplomacyModalState==2 && *(void**)(g_diplomacyModalState+0x24)==execution->actor &&
       *(int*)(g_diplomacyModalState+0x28)==1 && *(int*)(g_diplomacyModalState+0x2c)==1){
        auto target=execution->target;char context[240];
        sprintf_s(context,"{\"command_type\":%d,\"initial_result_code\":1,\"native_mode\":2,\"native_flag\":1,\"result_scope\":\"native_interaction_state_before_ui\",\"target_role\":\"force_ruler\",\"counterfactual_evaluated\":false}",execution->kind);
        target.context=context;
        if(execution->valid())skilltelemetry::appliedEvent(execution->actor,execution->owned,86,"diplomacy",
            "negotiation_interaction_state_written",0x5bd7bf,target);
        diplomacyContestPublish(scope);
    }
    g_diplomacyModalClearOriginal(self);
}
const struct DiplomacyInteractionLayout {uint32_t start;int size;const char* bytes;} g_diplomacyInteractionLayouts[]={
    {0x5bd790,47,"\x8b\x44\x24\x04\x8b\x40\x04\x8b\x4c\x24\x08\x89\x0d\xa0\x70\xc5\x09\xb9\x78\x70\xc5\x09\xc7\x05\x74\x70\xc5\x09\x02\x00\x00\x00\xa3\x98\x70\xc5\x09\xc7\x05\x9c\x70\xc5\x09\x01\x00\x00\x00"},
    {0x5bd7c4,34,"\x6a\x01\x6a\x01\x6a\x1d\x6a\x19\xb9\xa0\x9f\x1b\x09\xe8\xba\x7a\xf1\xff\xb9\xd8\x59\x6a\x09\xe8\xc0\x05\xfc\xff\xa1\xa0\x70\xc5\x09\xc3"},
    {0x5b22c8,8,"\xe8\xc3\xb4\x00\x00\x83\xc4\x08"},
    {0x5b655a,8,"\xe8\x31\x72\x00\x00\x83\xc4\x08"},
    {0x5cc6d9,8,"\xe8\xb2\x10\xff\xff\x83\xc4\x08"},
};
bool verifyDiplomacyInteractionLayout(){
    for(const auto& s:g_diplomacyInteractionLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
