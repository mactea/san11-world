// Native5b0411..5b0419 halves the current packet damage. Later parent
// coefficients still apply; never equate this local step with final troop loss.
int __fastcall vineHas(void* unit,void*,uint8_t* packet,int effect){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);auto c=g_criticalCalculation;
    if(!skilltelemetry::enabled() || !c || c->packet!=packet || effect!=31)return native(unit,effect);
    c->vine=VineAttribution{};auto& v=c->vine;v.unit=unit;v.unitId=g_telemetryUnitId((void*)0x7201958,unit);
    RecoveryActor members[3];int affiliations[3][4]={};
    if(v.unitId>=0 && g_movementUnitById((void*)0x7201958,v.unitId)==unit){
        v.force=unitTarget(unit).force;v.weapon=*(const int*)((uint8_t*)unit+0xa8);v.type=*(const int*)((uint8_t*)unit+8);
        memcpy(v.ids,(uint8_t*)unit+0xc,sizeof v.ids);
        typedef const uint32_t*(__thiscall* Position)(void*);v.position=*(*(Position*)(*(uint8_t**)unit+0x3c))(unit);
        for(int n=0;n<3;++n)if(v.ids[n]>=0){
            members[n]=recoveryActor(g_telemetryPerson((void*)0x7201958,v.ids[n]));
            if(members[n].person)memcpy(affiliations[n],(const uint8_t*)members[n].person+0x98,sizeof affiliations[n]);
        }
    }
    v.before=*(const int*)(packet+0xc);ProgressionProbe probe(31);probe.scene="battle";probe.event="vine_physical_reduction_eligibility";
    int found=native(unit,effect);
    if(found && probe.actor && (!c->target || c->target==unit))for(int n=0;n<3;++n)if(members[n].person==probe.actor){
        v.actor=members[n];v.actorId=v.ids[n];memcpy(v.affiliation,affiliations[n],sizeof v.affiliation);c->target=unit;break;
    }
    return found;
}
void __cdecl vineStored(uint8_t* packet,int after){
    auto c=g_criticalCalculation;if(c && c->packet==packet && c->generation==skilltelemetry::contextGeneration() && c->vine.actor.person){
        c->vine.after=after;c->vine.stored=true;
    }
}
uint32_t g_vineHasResume=0x5b040f,g_vineStoreResume=0x5b0419;
uint32_t g_vineAssistHasResume=0x5b0c14,g_vineAssistStoreResume=0x5b0c1e;
__declspec(naked) void vineHasBridge(){__asm {push ebp
    call vineHas
    test eax,eax
    jmp dword ptr [g_vineHasResume]}}
// Keep041c intact: the native no-vine branch jumps there. Observe the
// signed-halving instructions before the original unconditionally taken store.
__declspec(naked) void vineStoreBridge(){__asm {cdq
    sub eax,edx
    sar eax,1
    pushfd
    pushad
    push eax
    push ebp
    call vineStored
    add esp,8
    popad
    popfd
    jmp dword ptr [g_vineStoreResume]}}
__declspec(naked) void vineAssistHasBridge(){__asm {push ebp
    call vineHas
    test eax,eax
    jmp dword ptr [g_vineAssistHasResume]}}
__declspec(naked) void vineAssistStoreBridge(){__asm {cdq
    sub eax,edx
    sar eax,1
    pushfd
    pushad
    push eax
    push ebp
    call vineStored
    add esp,8
    popad
    popfd
    jmp dword ptr [g_vineAssistStoreResume]}}
bool vineGrantMatches(const CriticalRecord& r,void* source,const uint8_t* packet,void* unit,int requested){
    const auto& v=r.vine;
    if(!v.stored || !v.actor.person || !recoveryActorValid(v.actor,31) || r.source!=source || r.target!=unit || v.unit!=unit ||
       requested!=-*(const int*)(packet+0xc) || *(const int*)(packet+0x70)!=criticalKind(r) || memcmp(r.snapshot,packet,sizeof r.snapshot) ||
       v.unitId<0 || g_movementUnitById((void*)0x7201958,v.unitId)!=unit || growthPersonId((void*)v.actor.person)!=v.actorId ||
       memcmp(v.affiliation,(const uint8_t*)v.actor.person+0x98,sizeof v.affiliation) || memcmp(v.ids,(uint8_t*)unit+0xc,sizeof v.ids) ||
       unitTarget(unit).force!=v.force || *(const int*)((uint8_t*)unit+0xa8)!=v.weapon || *(const int*)((uint8_t*)unit+8)!=v.type)return false;
    typedef const uint32_t*(__thiscall* Position)(void*);
    return v.type>=0 && *(*(Position*)(*(uint8_t**)unit+0x3c))(unit)==v.position;
}
void vineGrantCommitted(const CriticalRecord& r,void*,int before,int after,uintptr_t caller,skilltelemetry::Target target){
    int attacker=target.actorUnit;const auto& v=r.vine;target.actorUnit=v.unitId;target.equipment=v.weapon;
    target.x=(int16_t)v.position;target.y=(int16_t)(v.position>>16);char context[512];
    sprintf_s(context,"{\"tactic_id\":%d,\"record_kind\":%d,\"native_attack_mode\":%d,\"attacking_unit\":%d,\"calculation_stage\":\"before_parent_coefficients\",\"settled_packet_damage\":%d,\"resource_before\":%d,\"resource_after\":%d,\"damage_attribution\":\"native_halving_step_with_whole_commit_context\",\"counterfactual_evaluated\":false}",
        r.tactic,*(const int*)(r.packet+0x70),r.attackMode,attacker,*(const int*)(r.packet+0xc),before,after);
    target.context=context;skilltelemetry::applied(v.actor.person,v.actor.owned,31,"battle","vine_physical_reduction_settled",
        "calculated_physical_troop_damage",v.before,v.after,caller,target);
}

const struct VineLayout {uint32_t at;int size;const char* bytes;} g_vineLayouts[]={
    {0x5b0419,6,"\x89\x45\x0c\x8b\x45\x08"},
    {0x5b040f,2,"\x74\x0b"},
    {0x5b0411,8,"\x8b\x45\x0c\x99\x2b\xc2\xd1\xf8"},
    {0x5b041f,50,"\x83\xe8\x03\x74\x1c\x48\x74\x0e\x48\x75\x27\xdb\x45\x0c\xd8\x0d\xd0\xb6\x84\x00\xeb\x14\xdb\x45\x0c\xd8\x0d\x88\xb3\x77\x00\xeb\x09\xdb\x45\x0c\xd8\x0d\xdc\xb6\x84\x00\xe8\x26\x76\x15\x00\x89\x45\x0c"},
};
const VineLayout g_vineAssistLayouts[]={
    {0x5b0c14,2,"\x74\x0b"},
    {0x5b0c16,8,"\x8b\x45\x0c\x99\x2b\xc2\xd1\xf8"},
    {0x5b0c1e,6,"\x89\x45\x0c\x8b\x45\x08"},
};
bool verifyVineLayout(){
    for(auto& g:g_vineLayouts)if(memcmp((const void*)(g_strategyCodeBase+g.at-0x400000),g.bytes,g.size))return false;
    for(auto& g:g_vineAssistLayouts)if(memcmp((const void*)(g_strategyCodeBase+g.at-0x400000),g.bytes,g.size))return false;
    return true;
}
