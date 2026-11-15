// The turn-update CALL owns this scope. A direct query or handler preview
// cannot turn a matching protection predicate into an applied event.
typedef int(__thiscall* FireAftermath)(void*,void*);
FireAftermath g_fireAftermathOriginal=(FireAftermath)0x582fb0;
struct FireAftermathScope;
thread_local FireAftermathScope* g_fireAftermath=nullptr;
struct FireAftermathScope {
    void* unit;int id=-1,force=-1,type=-1,weapon=-1,ids[3]={-1,-1,-1},effect=-1;
    uint32_t position=0;RecoveryActor members[3];const void* provider=nullptr;
    uint64_t generation=skilltelemetry::contextGeneration();FireAftermathScope* previous;
    explicit FireAftermathScope(void* u):unit(u),previous(g_fireAftermath){
        g_fireAftermath=this;if(!u)return;
        id=g_telemetryUnitId((void*)0x7201958,u);
        if(id<0 || g_movementUnitById((void*)0x7201958,id)!=u || !g_growthValid(u)){id=-1;return;}
        memcpy(ids,(const uint8_t*)u+0xc,sizeof ids);force=unitTarget(u).force;
        type=*(const int*)((const uint8_t*)u+8);weapon=*(const int*)((const uint8_t*)u+0xa8);
        typedef const uint32_t*(__thiscall* Position)(void*);
        position=*(*(Position*)(*(uint8_t**)u+0x3c))(u);
        for(int n=0;n<3;++n)if(ids[n]>=0)members[n]=recoveryActor(g_telemetryPerson((void*)0x7201958,ids[n]));
    }
    ~FireAftermathScope(){g_fireAftermath=previous;}
    void emit(){
        if(!provider || !skilltelemetry::enabled() || generation!=skilltelemetry::contextGeneration() || id<0 || type<0 ||
           g_movementUnitById((void*)0x7201958,id)!=unit || g_telemetryUnitId((void*)0x7201958,unit)!=id ||
           !g_growthValid(unit) || unitTarget(unit).force!=force || *(const int*)((const uint8_t*)unit+8)!=type ||
           *(const int*)((const uint8_t*)unit+0xa8)!=weapon || memcmp(ids,(const uint8_t*)unit+0xc,sizeof ids))return;
        typedef const uint32_t*(__thiscall* Position)(void*);
        if(*(*(Position*)(*(uint8_t**)unit+0x3c))(unit)!=position)return;
        for(int n=0;n<3;++n)if(members[n].person==provider && recoveryActorValid(members[n],effect) && growthPersonId((void*)provider)==ids[n]){
            auto target=unitTarget(unit);target.equipment=weapon;target.x=(int16_t)position;target.y=(int16_t)(position>>16);
            target.context="{\"native_handler\":\"0x582fb0\",\"scope\":\"turn_update_fire_casualties\",\"counterfactual_evaluated\":false}";
            skilltelemetry::appliedEvent(provider,members[n].owned,effect,"turn_update","fire_casualty_processing_skipped",
                effect==52?0x582fed:0x583012,target);return;
        }
    }
};
int __fastcall fireAftermathHas(void* unit,void*,int effect){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);
    auto scope=g_fireAftermath;
    if(!scope || scope->unit!=unit)return native(unit,effect);
    ProgressionProbe probe(effect);probe.scene="turn_update";probe.event="fire_casualty_protection_eligibility";
    int found=native(unit,effect);
    if(found && (effect==52 || effect==62)){scope->provider=probe.actor;scope->effect=effect;}
    return found;
}
int __fastcall fireAftermath(void* self,void*,void* unit){
    if(!skilltelemetry::enabled())return g_fireAftermathOriginal(self,unit);
    FireAftermathScope scope(unit);int result=g_fireAftermathOriginal(self,unit);
    scope.emit();return result;
}
uint32_t g_fireAftermathGuardResume=0x582feb,g_fireAftermathGodResume=0x583010;
__declspec(naked) void fireAftermathGuardBridge(){__asm {call fireAftermathHas
    test eax,eax
    jmp dword ptr [g_fireAftermathGuardResume]}}
__declspec(naked) void fireAftermathGodBridge(){__asm {call fireAftermathHas
    test eax,eax
    jmp dword ptr [g_fireAftermathGodResume]}}
bool verifyFireAftermathLayout(){
    const struct {uint32_t at;int size;const char* bytes;} guards[]={
        {0x582feb,10,"\x74\x18\x85\xed\x0f\x85\x6b\x01\x00\x00"},
        {0x583000,5,"\xe9\x5b\x01\x00\x00"},
        {0x583010,10,"\x74\x28\x85\xed\x0f\x85\x46\x01\x00\x00"},
        {0x583035,5,"\xe9\x26\x01\x00\x00"}};
    for(auto& g:guards)if(memcmp((const void*)(g_strategyCodeBase+g.at-0x400000),g.bytes,g.size))return false;
    return true;
}
