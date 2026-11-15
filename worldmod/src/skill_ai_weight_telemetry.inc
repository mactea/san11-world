// Native AI role calculation selects one weight for an OR of eight traits.
// This is a calculation event, not proof that a unit subsequently casts a plan.
struct AIWeightSite {uint32_t at;int effect;};
const AIWeightSite g_aiWeightSites[]={
    {0x5abf62,64},{0x5abf6f,63},{0x5abf7c,62},{0x5abf89,65},
    {0x5abf96,66},{0x5abfa3,70},{0x5abfb0,71},{0x5abfbd,67},
};
struct AIWeightPending {
    RecoveryActor actor;void* unit=nullptr;int unitId=-1,actorId=-1,force=-1,weapon=-1,type=-1,effect=-1,priority=-1;
    int members[3]={},affiliation[4]={};uint32_t position=0;uint64_t generation=0;
};
thread_local AIWeightPending g_aiWeightPending;
int __fastcall aiWeightHas(void* unit,void*,int effect,uintptr_t caller){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);int priority=-1;
    for(int n=0;n<8;++n)if(effect==g_aiWeightSites[n].effect && caller==g_strategyCodeBase+g_aiWeightSites[n].at-0x400000+5)priority=n;
    if(!skilltelemetry::enabled() || priority<0)return native(unit,effect);
    g_aiWeightPending=AIWeightPending{};AIWeightPending observed;observed.unit=unit;
    observed.generation=skilltelemetry::contextGeneration();observed.unitId=g_telemetryUnitId((void*)0x7201958,unit);
    RecoveryActor members[3];int affiliations[3][4]={};
    if(observed.unitId>=0 && g_movementUnitById((void*)0x7201958,observed.unitId)==unit){
        observed.force=unitTarget(unit).force;observed.weapon=*(const int*)((uint8_t*)unit+0xa8);observed.type=*(const int*)((uint8_t*)unit+8);
        memcpy(observed.members,(uint8_t*)unit+0xc,sizeof observed.members);
        typedef const uint32_t*(__thiscall* Position)(void*);observed.position=*(*(Position*)(*(uint8_t**)unit+0x3c))(unit);
        for(int n=0;n<3;++n)if(observed.members[n]>=0){
            members[n]=recoveryActor(g_telemetryPerson((void*)0x7201958,observed.members[n]));
            if(members[n].person)memcpy(affiliations[n],(const uint8_t*)members[n].person+0x98,sizeof affiliations[n]);
        }
    }
    ProgressionProbe eligibility(effect);eligibility.scene="ai_evaluation";eligibility.event="strategy_role_weight_eligibility";
    int result=native(unit,effect);
    g_aiWeightPending=AIWeightPending{};
    if(result && eligibility.actor && observed.generation==skilltelemetry::contextGeneration())for(int n=0;n<3;++n)
        if(members[n].person==eligibility.actor){
            observed.actor=members[n];observed.actorId=observed.members[n];observed.effect=effect;observed.priority=priority;
            memcpy(observed.affiliation,affiliations[n],sizeof observed.affiliation);g_aiWeightPending=observed;break;
        }
    return result;
}
__declspec(naked) void aiWeightHasBridge(){__asm {mov eax,[esp]
    push eax
    push [esp+8]
    call aiWeightHas
    test eax,eax
    ret 4}}
void __cdecl aiWeightSelected(void* unit,int before,int after){
    auto p=g_aiWeightPending;g_aiWeightPending=AIWeightPending{};
    if(!skilltelemetry::enabled() || p.unit!=unit || before!=524 || after!=1024 || !p.actor.person || p.priority<0 ||
       p.generation!=skilltelemetry::contextGeneration() || !recoveryActorValid(p.actor,p.effect) || p.unitId<0 ||
       g_movementUnitById((void*)0x7201958,p.unitId)!=unit || growthPersonId((void*)p.actor.person)!=p.actorId ||
       memcmp(p.members,(const uint8_t*)unit+0xc,sizeof p.members) || memcmp(p.affiliation,(const uint8_t*)p.actor.person+0x98,sizeof p.affiliation) ||
       unitTarget(unit).force!=p.force || *(const int*)((const uint8_t*)unit+0xa8)!=p.weapon || *(const int*)((const uint8_t*)unit+8)!=p.type)return;
    typedef const uint32_t*(__thiscall* Position)(void*);
    if(*(*(Position*)(*(uint8_t**)unit+0x3c))(unit)!=p.position)return;
    auto target=unitTarget(unit);target.actorUnit=p.unitId;target.equipment=p.weapon;
    target.x=(int16_t)p.position;target.y=(int16_t)(p.position>>16);char context[384];
    sprintf_s(context,"{\"native_query_priority\":%d,\"calculation_only\":true,\"calculation_stage\":\"before_intelligence_and_other_score_terms\",\"result_scope\":\"native_ai_role_weight_not_command_execution\",\"counterfactual_evaluated\":false}",p.priority);target.context=context;
    skilltelemetry::applied(p.actor.person,p.actor.owned,p.effect,"ai_evaluation","strategy_role_weight_selected",
        "ai_strategy_role_weight",before,after,0x5abfc4,target);
}
uint32_t g_aiWeightResume=0x5abfc9;
__declspec(naked) void aiWeightSelectedBridge(){__asm {pushfd
    pushad
    mov eax,ebp
    mov ebp,0x400
    mov [esp+8],ebp
    push ebp
    push eax
    push edi
    call aiWeightSelected
    add esp,12
    popad
    popfd
    jmp dword ptr [g_aiWeightResume]}}

struct AIWeightLayout {uint32_t at;int size;const char* bytes;};
const AIWeightLayout g_aiWeightLayouts[]={
    {0x5abf4e,9,"\xe8\x1d\x96\xff\xff\x85\xc0\x74\x72"},
    {0x5abf57,18,"\x8b\x17\x6a\x40\x8b\xcf\xbd\x0c\x02\x00\x00\xff\x52\x58\x85\xc0\x75\x5b"},
    {0x5abf69,13,"\x8b\x07\x6a\x3f\x8b\xcf\xff\x50\x58\x85\xc0\x75\x4e"},
    {0x5abf76,13,"\x8b\x17\x6a\x3e\x8b\xcf\xff\x52\x58\x85\xc0\x75\x41"},
    {0x5abf83,13,"\x8b\x07\x6a\x41\x8b\xcf\xff\x50\x58\x85\xc0\x75\x34"},
    {0x5abf90,13,"\x8b\x17\x6a\x42\x8b\xcf\xff\x52\x58\x85\xc0\x75\x27"},
    {0x5abf9d,13,"\x8b\x07\x6a\x46\x8b\xcf\xff\x50\x58\x85\xc0\x75\x1a"},
    {0x5abfaa,13,"\x8b\x17\x6a\x47\x8b\xcf\xff\x52\x58\x85\xc0\x75\x0d"},
    {0x5abfb7,13,"\x8b\x07\x6a\x43\x8b\xcf\xff\x50\x58\x85\xc0\x74\x05"},
    {0x5abfc4,5,"\xbd\x00\x04\x00\x00"},
};
bool verifyAIWeightLayout(){
    for(auto& s:g_aiWeightLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.at-0x400000),s.bytes,s.size))return false;
    return true;
}
