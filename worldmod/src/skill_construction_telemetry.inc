typedef int(__cdecl* ConstructionAmount)(void*);
typedef uint16_t(__thiscall* ConstructionMax)(void*);
ConstructionAmount g_constructionAmountOriginal=(ConstructionAmount)0x5a3520;
IncomeGrant g_constructionGrantOriginal=(IncomeGrant)0x4ae360;
ConstructionMax g_constructionMax=(ConstructionMax)0x487150;
struct ConstructionProbe;
thread_local ConstructionProbe* g_constructionProbe=nullptr;
struct ConstructionProbe {
    void* unit;
    int normal=0;
    ConstructionProbe* previous;
    explicit ConstructionProbe(void* u):unit(u),previous(g_constructionProbe){g_constructionProbe=this;}
    ~ConstructionProbe(){g_constructionProbe=previous;}
};
struct ConstructionPending {
    RecoveryActor actor;void* unit=nullptr;
    int requested=0,normal=0,actorId=-1,unitId=-1,force=-1,type=-1,weapon=-1;
    int ids[3]={},affiliation[5]={};uint32_t position=0;
};
thread_local ConstructionPending g_constructionPending;
uint32_t g_constructionAmountCaller=0x5850b7,g_constructionGrantCaller=0x5850cf;
bool constructionLive(const ConstructionPending& p){
    if(!p.actor.person || p.actor.generation!=skilltelemetry::contextGeneration() || p.unitId<0 ||
       g_movementUnitById((void*)0x7201958,p.unitId)!=p.unit || !g_growthValid(p.unit) ||
       growthPersonId((void*)p.actor.person)!=p.actorId || !recoveryActorValid(p.actor,77) ||
       memcmp(p.ids,(const uint8_t*)p.unit+0xc,sizeof p.ids) || memcmp(p.affiliation,(const uint8_t*)p.actor.person+0x94,sizeof p.affiliation) ||
       unitTarget(p.unit).force!=p.force || *(const int*)((const uint8_t*)p.unit+8)!=p.type ||
       *(const int*)((const uint8_t*)p.unit+0xa8)!=p.weapon)return false;
    typedef const uint32_t*(__thiscall* Position)(void*);
    return *(*(Position*)(*(uint8_t**)p.unit+0x3c))(p.unit)==p.position;
}
int __fastcall constructionHas(void* unit,void*,int normal,int effect) {
    typedef int(__thiscall* Has)(void*,int);
    int result=(*(Has*)(*(uint8_t**)unit+0x58))(unit,effect);
    if(result && effect==77 && g_constructionProbe && g_constructionProbe->unit==unit)
        g_constructionProbe->normal=normal;
    return result;
}
uint32_t g_constructionHasResume=0x5a35dc;
__declspec(naked) void constructionHasBridge() {
    __asm {push esi
        call constructionHas
        test eax,eax
        jmp dword ptr [g_constructionHasResume]}
}
int __cdecl constructionAmount(void* unit) {
    g_constructionPending=ConstructionPending{};
    if(!skilltelemetry::enabled() || (uintptr_t)_ReturnAddress()!=g_constructionAmountCaller)return g_constructionAmountOriginal(unit);
    uint64_t generation=skilltelemetry::contextGeneration();ConstructionPending pending;pending.unit=unit;
    pending.unitId=g_telemetryUnitId((void*)0x7201958,unit);RecoveryActor members[3];int affiliations[3][5]={};
    if(pending.unitId>=0 && g_movementUnitById((void*)0x7201958,pending.unitId)==unit && g_growthValid(unit)){
        memcpy(pending.ids,(const uint8_t*)unit+0xc,sizeof pending.ids);
        pending.force=unitTarget(unit).force;pending.type=*(const int*)((const uint8_t*)unit+8);pending.weapon=*(const int*)((const uint8_t*)unit+0xa8);
        typedef const uint32_t*(__thiscall* Position)(void*);pending.position=*(*(Position*)(*(uint8_t**)unit+0x3c))(unit);
        for(int n=0;n<3;++n)if(pending.ids[n]>=0){
            members[n]=recoveryActor(g_telemetryPerson((void*)0x7201958,pending.ids[n]));
            if(members[n].person)memcpy(affiliations[n],(const uint8_t*)members[n].person+0x94,sizeof affiliations[n]);
        }
    }
    ProgressionProbe eligibility(77);eligibility.scene="construction";eligibility.event="construction_hp_eligibility";
    ConstructionProbe probe(unit);
    // The native calculator saves/restores its random seed. Capture the same
    // execution's value just before doubling; do not run its samples a second time.
    int result=g_constructionAmountOriginal(unit);
    if(eligibility.actor && probe.normal>0 && result==probe.normal*2 && generation==skilltelemetry::contextGeneration())
        for(int n=0;n<3;++n)if(members[n].person==eligibility.actor){
            pending.actor=members[n];pending.actorId=pending.ids[n];pending.requested=result;pending.normal=probe.normal;
            memcpy(pending.affiliation,affiliations[n],sizeof pending.affiliation);
            if(constructionLive(pending))g_constructionPending=pending;break;
        }
    return result;
}
int __fastcall constructionGrant(void* self,void*,uint8_t* base,int requested) {
    auto pending=g_constructionPending;g_constructionPending=ConstructionPending{};
    int baseId=-1,baseForce=-1,baseKind=-1;
    bool observe=skilltelemetry::enabled() && (uintptr_t)_ReturnAddress()==g_constructionGrantCaller &&
        constructionLive(pending) && pending.requested==requested && g_growthValid(base);
    if(observe){baseId=g_telemetryBaseId((void*)0x7201958,base);observe=baseId>=0 && g_baseById((void*)0x7201958,baseId)==base;
        if(observe){baseForce=incomeTarget(base).force;baseKind=*(int*)(base+8);}}
    int before=observe?*(int16_t*)(base+0x10):0;
    int maximum=observe?g_constructionMax(base):0;
    int completed=observe?*(int*)(base+0x14):0;
    int result=g_constructionGrantOriginal(self,base,requested);
    if(observe && constructionLive(pending) && g_baseById((void*)0x7201958,baseId)==base &&
       g_telemetryBaseId((void*)0x7201958,base)==baseId && incomeTarget(base).force==baseForce && *(int*)(base+8)==baseKind) {
        auto target=incomeTarget(base);
        target.actorUnit=g_telemetryUnitId((void*)0x7201958,pending.unit);
        if(target.base<0 || g_baseById((void*)0x7201958,target.base)!=base || target.actorUnit<0 ||
           g_movementUnitById((void*)0x7201958,target.actorUnit)!=pending.unit)return result;
        int normal=before+pending.normal;if(normal<=0)normal=0;else if(normal>maximum)normal=maximum;
        // Native487e20 compares signed words after the outer0..maximum clamp.
        int16_t word=(int16_t)normal,limit=(int16_t)maximum;
        normal=word<=0?0:limit<=word?limit:word;
        char context[176];
        sprintf_s(context,"{\"facility_kind\":%d,\"base_hp_amount\":%d,\"requested_hp_amount\":%d,\"max_hp\":%d,\"completed_before\":%d}",
            *(int*)(base+8),pending.normal,requested,maximum,completed);target.context=context;
        skilltelemetry::appliedBonus(pending.actor.person,pending.actor.owned,77,
            "construction",completed?"facility_repair_hp_bonus":"facility_construction_hp_bonus",
            "facility_hp",before,*(int16_t*)(base+0x10),normal,0x5850cf,target);
    }
    return result;
}
