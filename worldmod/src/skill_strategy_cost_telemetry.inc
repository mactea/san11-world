typedef int(__thiscall* StrategyCost)(void*,int);
StrategyCost g_strategyCostOriginal=(StrategyCost)0x495170;
const uint8_t* g_strategyNormalCosts=(const uint8_t*)0x79cc08;
struct StrategyCostProbe;
thread_local StrategyCostProbe* g_strategyCostProbe=nullptr;
struct StrategyCostProbe {
    bool forced=false;
    StrategyCostProbe* previous;
    StrategyCostProbe():previous(g_strategyCostProbe){g_strategyCostProbe=this;}
    ~StrategyCostProbe(){g_strategyCostProbe=previous;}
};
void __cdecl strategyCostForced(){if(g_strategyCostProbe)g_strategyCostProbe->forced=true;}
uint32_t g_strategyCostForcedResume=0x495180;
__declspec(naked) void strategyCostForcedBridge(){__asm {pushfd
    pushad
    call strategyCostForced
    popad
    popfd
    mov eax,1
    jmp dword ptr [g_strategyCostForcedResume]}}
struct StrategyCostPending {
    const void* actor=nullptr;
    void* unit=nullptr;
    PersonReceipt provider;
    int unitId=-1,force=-1,type=-1,weapon=-1,ids[3]={},plan=-1,normal=0,cost=0;
    uint32_t position=0;uintptr_t frame=0;uint64_t generation=0;
    StrategyCostPending()=default;
    StrategyCostPending(void* u,int p,uintptr_t f):unit(u),plan(p),frame(f),generation(skilltelemetry::contextGeneration()){
        unitId=g_telemetryUnitId((void*)0x7201958,u);
        if(unitId<0 || g_movementUnitById((void*)0x7201958,unitId)!=u){unitId=-1;return;}
        force=(*(BuildingValue*)(*(uint8_t**)u+0x40))(u);type=*(int*)((uint8_t*)u+8);weapon=*(int*)((uint8_t*)u+0xa8);
        typedef const uint32_t*(__thiscall* Position)(void*);position=*(*(Position*)(*(uint8_t**)u+0x3c))(u);
        memcpy(ids,(uint8_t*)u+0xc,sizeof ids);
    }
    bool valid()const{
        if(!skilltelemetry::enabled() || generation!=skilltelemetry::contextGeneration() || !provider.valid(64) ||
           unitId<0 || g_movementUnitById((void*)0x7201958,unitId)!=unit || g_telemetryUnitId((void*)0x7201958,unit)!=unitId ||
           (*(BuildingValue*)(*(uint8_t**)unit+0x40))(unit)!=force || *(int*)((uint8_t*)unit+8)!=type ||
           *(int*)((uint8_t*)unit+0xa8)!=weapon || memcmp(ids,(uint8_t*)unit+0xc,sizeof ids))return false;
        bool member=false;for(int id:ids)if(id==provider.id)member=true;
        typedef const uint32_t*(__thiscall* Position)(void*);
        return member && *(*(Position*)(*(uint8_t**)unit+0x3c))(unit)==position;
    }
};
thread_local StrategyCostPending g_strategyCostPending;
int __fastcall strategyCost(void* unit,void*,int plan) {
    if(!skilltelemetry::enabled() || (uintptr_t)_ReturnAddress()!=g_strategyCodeBase+0x193440)return g_strategyCostOriginal(unit,plan);
    g_strategyCostPending=StrategyCostPending{};
    StrategyCostPending pending(unit,plan,(uintptr_t)_AddressOfReturnAddress());
    reachCostOwner(unit,plan,pending.frame);
    ProgressionProbe eligibility(64,&pending.provider);eligibility.scene="strategy";eligibility.event="energy_cost_eligibility";
    StrategyCostProbe probe;
    int cost=g_strategyCostOriginal(unit,plan);
    if(probe.forced && cost==1 && plan>=0 && plan<9 && pending.valid()) {
        pending.actor=pending.provider.person;pending.normal=g_strategyNormalCosts[plan];pending.cost=cost;g_strategyCostPending=pending;
    }
    return cost;
}
int __fastcall strategyCostGrant(void* unit,void*,int requested) {
    uintptr_t caller=(uintptr_t)_ReturnAddress(),frame=(uintptr_t)_AddressOfReturnAddress();
    bool reach=reachObservesCost(unit,caller,frame);
    auto reachValue=reach?*(BuildingValue*)(*(uint8_t**)unit+0x54):nullptr;
    int reachBefore=reach?reachValue(unit):0;
    auto pending=g_strategyCostPending;
    bool owner=skilltelemetry::enabled() && caller==g_strategyCodeBase+0x19344a && pending.frame==frame;
    if(owner)g_strategyCostPending=StrategyCostPending{};
    bool observe=owner && pending.actor && pending.unit==unit && requested==-pending.cost && pending.valid();
    skilltelemetry::Target target;
    if(observe) {
        target=unitTarget(unit);
        observe=target.unit>=0 && g_movementUnitById((void*)0x7201958,target.unit)==unit;
    }
    auto value=observe?(*(BuildingValue*)(*(uint8_t**)unit+0x54)):nullptr;
    int before=observe?value(unit):0;
    int result=g_unitEnergyOriginal(unit,requested);
    if(reach)reachCostPaid(unit,reachBefore,reachValue(unit),requested,result,frame);
    if(observe && pending.valid()) {
        int after=value(unit);
        if(result!=after-before || !pending.valid())return result;
        // Native4964f0/496490 clamp the same energy scalar. For normal>=1,
        // min(actual after, max(0,before-normal)) is the exact no64 result,
        // including an initial value above the native100/120 upper bound.
        bool baseline=before>=0 && after>=0 && pending.normal>=1;
        int normal=before-pending.normal;if(normal<0)normal=0;if(normal>after)normal=after;
        typedef const uint32_t*(__thiscall* Position)(void*);
        uint32_t position=*(*(Position*)(*(uint8_t**)unit+0x3c))(unit);
        target.x=(int16_t)position;target.y=(int16_t)(position>>16);
        target.equipment=*(const int*)((const uint8_t*)unit+0xa8);
        char context[160];
        sprintf_s(context,"{\"strategy_id\":%d,\"normal_energy_cost\":%d,\"requested_energy_cost\":%d,\"counterfactual_evaluated\":%s}",
            pending.plan,pending.normal,pending.cost,baseline?"true":"false");target.context=context;
        if(!pending.valid())return result;
        if(baseline)skilltelemetry::appliedBonus(pending.actor,pending.provider.owned,64,"strategy","strategy_energy_cost_override",
            "energy",before,after,normal,0x593445,target);
        else skilltelemetry::applied(pending.actor,pending.provider.owned,64,"strategy","strategy_energy_cost_override",
            "energy",before,after,0x593445,target);
    }
    return result;
}
