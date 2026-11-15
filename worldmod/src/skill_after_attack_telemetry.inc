struct AfterAttackScope;
thread_local AfterAttackScope* g_afterAttack=nullptr;
struct AfterAttackScope {
    int effect;
    void* source;
    void* target;
    const void* actor=nullptr;
    const void* guard=nullptr;
    const void* excluded[3]={};
    int excludedCount=0;
    uint64_t generation;
    AfterAttackScope* previous;
    AfterAttackScope(int e,void* s,void* t):effect(e),source(s),target(t),
        generation(skilltelemetry::contextGeneration()),previous(g_afterAttack){g_afterAttack=this;}
    ~AfterAttackScope(){g_afterAttack=previous;}
};
bool afterAttackMember(void* unit,const void* person) {
    int id=growthPersonId((void*)person);if(id<0)return false;
    for(int n=0;n<3;++n)if(*(const int*)((const uint8_t*)unit+0xc+4*n)==id)return true;
    return false;
}
bool afterAttackValid(const AfterAttackScope& scope) {
    if(!skilltelemetry::enabled() || scope.generation!=skilltelemetry::contextGeneration() || !scope.actor ||
       !g_growthValid(scope.source) || !g_growthValid(scope.target) ||
       !contains(*(const int*)((const uint8_t*)scope.actor+0xe8),scope.effect) || !afterAttackMember(scope.source,scope.actor))return false;
    int from=g_telemetryUnitId((void*)0x7201958,scope.source),to=g_telemetryUnitId((void*)0x7201958,scope.target);
    return from>=0 && to>=0 && g_movementUnitById((void*)0x7201958,from)==scope.source &&
        g_movementUnitById((void*)0x7201958,to)==scope.target;
}
skilltelemetry::Target afterAttackTarget(const AfterAttackScope& scope,bool defensive) {
    auto target=unitTarget(scope.target);
    void* actorUnit=defensive?scope.target:scope.source;
    target.actorUnit=g_telemetryUnitId((void*)0x7201958,actorUnit);
    target.equipment=*(const int*)((const uint8_t*)actorUnit+0xa8);
    typedef const uint32_t*(__thiscall* Position)(void*);
    uint32_t position=*(*(Position*)(*(uint8_t**)scope.target+0x3c))(scope.target);
    target.x=(int16_t)position;target.y=(int16_t)(position>>16);return target;
}
uint8_t* __fastcall afterAttackProvider(void* world,void*,int id) {
    auto actor=g_telemetryPerson(world,id);
    if(g_afterAttack)g_afterAttack->actor=actor;
    return actor;
}
typedef void(__cdecl* DashingAttempt)(void*,void*);
uint32_t g_dashingResume=0x593b46;
__declspec(naked) void dashingNative(){__asm {sub esp,0x3d0
    jmp dword ptr [g_dashingResume]}}
DashingAttempt g_dashingOriginal=(DashingAttempt)dashingNative;
void __cdecl dashingAttempt(void* source,void* target) {
    if(!skilltelemetry::enabled()){g_dashingOriginal(source,target);return;}
    AfterAttackScope scope(49,source,target);ProgressionProbe eligibility(49);
    eligibility.scene="battle";eligibility.event="after_attack_eligibility";
    g_dashingOriginal(source,target);
}
typedef void(__thiscall* StatusGrant)(void*,void*,void*,int,int,int);
StatusGrant g_dashingStatusOriginal=(StatusGrant)0x4aea70;
struct StatusWriteProbe;
thread_local StatusWriteProbe* g_statusWriteProbe=nullptr;
struct StatusWriteProbe {
    void* unit;
    bool written=false;
    StatusWriteProbe* previous;
    explicit StatusWriteProbe(void* u):unit(u),previous(g_statusWriteProbe){g_statusWriteProbe=this;}
    ~StatusWriteProbe(){g_statusWriteProbe=previous;}
};
typedef void(__thiscall* StatusTurnsSet)(void*,int);
StatusTurnsSet g_statusTurnsOriginal=(StatusTurnsSet)0x496350;
void __fastcall statusTurnsSet(void* unit,void*,int value) {
    g_statusTurnsOriginal(unit,value);
    if(g_statusWriteProbe && g_statusWriteProbe->unit==unit)g_statusWriteProbe->written=true;
}
void __fastcall dashingStatus(void* self,void*,uint8_t* unit,void* source,int status,int turns,int display) {
    auto scope=g_afterAttack;
    bool observe=scope && scope->effect==49 && scope->source==source && scope->target==unit && status==1 && afterAttackValid(*scope);
    int before=observe?unit[0x28]:0,previousStatus=observe?*(int*)(unit+0x24):0;
    StatusWriteProbe write(observe?unit:nullptr);
    g_dashingStatusOriginal(self,unit,source,status,turns,display);
    if(observe && write.written && scope->generation==skilltelemetry::contextGeneration() && *(int*)(unit+0x24)==status) {
        auto target=afterAttackTarget(*scope,false);char context[176];
        // Native4aec56 consumes AL only; the duration sampler's upper EAX bits
        // are not a turn count. Preserve the raw argument when calling native.
        sprintf_s(context,"{\"status_before\":%d,\"status_after\":%d,\"requested_turns\":%d,\"attacking_power\":%u,\"defending_power\":%u,\"counterfactual_evaluated\":false}",
            previousStatus,status,(int)(uint8_t)turns,((uint8_t*)source)[0xc9],unit[0xc9]);target.context=context;
        skilltelemetry::applied(scope->actor,*(const int*)((const uint8_t*)scope->actor+0xe8),49,
            "battle","dashing_status_applied","status_turns",before,unit[0x28],0x593be1,target);
    }
}
typedef int(__thiscall* PersonSkill)(void*,int);
PersonSkill g_injurySkillOriginal=(PersonSkill)0x4890f0;
#include "skill_casualty_telemetry.inc"
int __fastcall injuryExclude(void* person,void*,int effect) {
    if(g_casualty){
        ProgressionProbe probe(32);probe.scene="battle";probe.event="casualty_protection_eligibility";
        int found=g_injurySkillOriginal(person,effect);auto scope=g_casualty;
        if(found && effect==32 && afterAttackMember(scope->unit,person)){
            bool duplicate=false;for(int n=0;n<scope->excludedCount;++n)if(scope->excluded[n]==person)duplicate=true;
            if(!duplicate && scope->excludedCount<3)scope->excluded[scope->excludedCount++]=person;
        }
        return found;
    }
    auto scope=g_afterAttack;
    if(!scope || scope->effect!=51)return g_injurySkillOriginal(person,effect);
    ProgressionProbe probe(32);probe.scene="battle";probe.event="injury_protection_eligibility";
    int found=g_injurySkillOriginal(person,effect);
    if(found && effect==32 && afterAttackMember(scope->target,person)) {
        bool duplicate=false;for(int n=0;n<scope->excludedCount;++n)if(scope->excluded[n]==person)duplicate=true;
        if(!duplicate && scope->excludedCount<3)scope->excluded[scope->excludedCount++]=person;
    }
    return found;
}
int __fastcall injuryGuard(void* unit,void*,int effect) {
    typedef int(__thiscall* Has)(void*,int);
    auto native=*(Has*)(*(uint8_t**)unit+0x58);auto scope=g_afterAttack;
    if(g_casualty && g_casualty->unit==unit){
        ProgressionProbe probe(52);probe.scene="battle";probe.event="casualty_protection_eligibility";
        int found=native(unit,effect);if(found && effect==52)g_casualty->guard=probe.actor;return found;
    }
    if(!scope || scope->effect!=51 || scope->target!=unit)return native(unit,effect);
    ProgressionProbe probe(52);probe.scene="battle";probe.event="injury_protection_eligibility";
    int found=native(unit,effect);
    if(found && effect==52)scope->guard=probe.actor;
    return found;
}
uint32_t g_injuryGuardResume=0x597220;
__declspec(naked) void injuryGuardBridge(){__asm {call injuryGuard
    test eax,eax
    jmp dword ptr [g_injuryGuardResume]}}
void injuryProtection(const AfterAttackScope& scope,const void* actor,int effect) {
    if(!actor || !afterAttackValid(scope) || !afterAttackMember(scope.target,actor) ||
       !contains(*(const int*)((const uint8_t*)actor+0xe8),effect))return;
    auto target=afterAttackTarget(scope,true);
    if(effect==32)target.person=growthPersonId((void*)actor);
    char context[144];
    sprintf_s(context,"{\"attacking_unit\":%d,\"attacking_force\":%d,\"selection_scope\":\"mighty_after_attack\",\"counterfactual_evaluated\":false}",
        g_telemetryUnitId((void*)0x7201958,scope.source),
        (*(BuildingValue*)(*(uint8_t**)scope.source+0x40))(scope.source));target.context=context;
    skilltelemetry::appliedEvent(actor,*(const int*)((const uint8_t*)actor+0xe8),effect,"battle",
        effect==52?"injury_selection_blocked":"injury_candidate_excluded",effect==52?0x597220:0x59729e,target);
}
typedef void(__thiscall* MightyAttempt)(void*,void*,void*);
uint32_t g_mightyResume=0x5977f5;
__declspec(naked) void mightyNative(){__asm {push esi
    mov esi,[esp+0xc]
    jmp dword ptr [g_mightyResume]}}
MightyAttempt g_mightyOriginal=(MightyAttempt)mightyNative;
void __fastcall mightyAttempt(void* self,void*,void* target,void* source) {
    if(!skilltelemetry::enabled()){g_mightyOriginal(self,target,source);return;}
    AfterAttackScope scope(51,source,target);ProgressionProbe eligibility(51);
    eligibility.scene="battle";eligibility.event="after_attack_eligibility";
    g_mightyOriginal(self,target,source);
    injuryProtection(scope,scope.guard,52);
    for(int n=0;n<scope.excludedCount;++n)injuryProtection(scope,scope.excluded[n],32);
}
typedef void(__thiscall* InjuryGrant)(void*,void*,int);
InjuryGrant g_mightyInjuryOriginal=(InjuryGrant)0x4a5690;
void __fastcall mightyInjury(void* self,void*,uint8_t* person,int requested) {
    auto scope=g_afterAttack;
    bool observe=scope && scope->effect==51 && afterAttackValid(*scope) && afterAttackMember(scope->target,person);
    int before=observe?*(int*)(person+0x15c):0;
    g_mightyInjuryOriginal(self,person,requested);
    if(observe && scope->generation==skilltelemetry::contextGeneration() &&
       *(int*)(person+0x15c)==requested && requested>before) {
        auto target=afterAttackTarget(*scope,false);target.person=growthPersonId(person);char context[96];
        sprintf_s(context,"{\"requested_injury_level\":%d,\"counterfactual_evaluated\":false}",requested);target.context=context;
        skilltelemetry::applied(scope->actor,*(const int*)((const uint8_t*)scope->actor+0xe8),51,"battle",
            "mighty_injury_level_increased","injury_level",before,requested,0x59775e,target);
    }
}
