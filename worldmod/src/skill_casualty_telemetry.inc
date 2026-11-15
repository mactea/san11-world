// These four CALL sites belong to native casualty handlers. Direct selector
// queries (including previews) do not create a casualty attribution scope.
const uint32_t g_casualtyCallers[]={0x59736d,0x59742c,0x59750d,0x59760d};
const char* g_casualtyKinds[]={"ordinary_death","ordinary_injury","tactic_injury","tactic_death"};
uint32_t g_casualtySelectorEntry=0x5971f0;
struct CasualtyScope;
thread_local CasualtyScope* g_casualty=nullptr;
struct CasualtyScope {
    void* unit;int kind,id=-1,force=-1,type=-1,equipment=-1,ids[3];uint32_t position=0;
    RecoveryActor members[3];const void* guard=nullptr;const void* excluded[3]={};int excludedCount=0;
    uint64_t generation=skilltelemetry::contextGeneration();CasualtyScope* previous;
    CasualtyScope(void* u,int k):unit(u),kind(k),previous(g_casualty){
        g_casualty=this;if(!unit)return;
        id=g_telemetryUnitId((void*)0x7201958,unit);if(id<0 || g_movementUnitById((void*)0x7201958,id)!=unit || !g_growthValid(unit)){id=-1;return;}
        memcpy(ids,(const uint8_t*)unit+0xc,sizeof ids);
        force=unitTarget(unit).force;type=*(const int*)((const uint8_t*)unit+8);equipment=*(const int*)((const uint8_t*)unit+0xa8);
        typedef const uint32_t*(__thiscall* Position)(void*);position=*(*(Position*)(*(uint8_t**)unit+0x3c))(unit);
        for(int n=0;n<3;++n)if(ids[n]>=0)members[n]=recoveryActor(g_telemetryPerson((void*)0x7201958,ids[n]));
    }
    ~CasualtyScope(){g_casualty=previous;}
    bool live() const {
        typedef const uint32_t*(__thiscall* Position)(void*);
        return skilltelemetry::enabled() && generation==skilltelemetry::contextGeneration() && id>=0 && type>=0 &&
            g_movementUnitById((void*)0x7201958,id)==unit && g_telemetryUnitId((void*)0x7201958,unit)==id &&
            g_growthValid(unit) && unitTarget(unit).force==force && *(const int*)((const uint8_t*)unit+8)==type &&
            *(const int*)((const uint8_t*)unit+0xa8)==equipment && !memcmp(ids,(const uint8_t*)unit+0xc,sizeof ids) &&
            *(*(Position*)(*(uint8_t**)unit+0x3c))(unit)==position;
    }
    void emit(const void* person,int effect,int selected){
        if(!person || !live())return;
        for(int n=0;n<3;++n)if(members[n].person==person && recoveryActorValid(members[n],effect) && growthPersonId((void*)person)==ids[n]){
            auto target=unitTarget(unit);target.actorUnit=id;target.equipment=equipment;
            target.person=effect==32?ids[n]:-1;target.x=(int16_t)position;target.y=(int16_t)(position>>16);
            char context[224];sprintf_s(context,"{\"selection_scope\":\"%s\",\"native_selector_caller\":%u,\"selected_person\":%d,\"counterfactual_evaluated\":false}",
                g_casualtyKinds[kind],g_casualtyCallers[kind],selected);target.context=context;
            skilltelemetry::appliedEvent(person,members[n].owned,effect,"battle",effect==52?"casualty_selection_blocked":"casualty_candidate_excluded",effect==52?0x597220:0x59729e,target);return;
        }
    }
};
__declspec(naked) void* __cdecl casualtySelect(void*){__asm {
    push ebx
    mov ebx,[esp+8]
    call dword ptr [g_casualtySelectorEntry]
    pop ebx
    ret}}
void* __cdecl casualtySelection(int kind,void* unit){
    if(!skilltelemetry::enabled())return casualtySelect(unit);
    CasualtyScope scope(unit,kind);void* result=casualtySelect(unit);int selected=result?growthPersonId(result):-1;
    if(!result)scope.emit(scope.guard,52,selected);
    for(int n=0;n<scope.excludedCount;++n)scope.emit(scope.excluded[n],32,selected);
    return result;
}
__declspec(naked) void casualtyOrdinaryDeathBridge(){__asm {push ebx
    push 0
    call casualtySelection
    add esp,8
    ret}}
__declspec(naked) void casualtyOrdinaryInjuryBridge(){__asm {push ebx
    push 1
    call casualtySelection
    add esp,8
    ret}}
__declspec(naked) void casualtyTacticInjuryBridge(){__asm {push ebx
    push 2
    call casualtySelection
    add esp,8
    ret}}
__declspec(naked) void casualtyTacticDeathBridge(){__asm {push ebx
    push 3
    call casualtySelection
    add esp,8
    ret}}
