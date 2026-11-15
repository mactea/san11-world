// Monthly income splits with skills 89/90, plus the separate between-month tax routine.
// Observations describe each actual grant, conditional on its actual pre-grant resource.
// Their counterfactuals are not an additive whole-month/season simulation.
struct RevenueCycle {
    void* city=nullptr;
    RecoveryActor actor;
    int full=0;
    bool harvest=false;
};
thread_local RevenueCycle g_goldRevenue,g_foodRevenue,g_offMonthRevenue;
BuildingValue g_revenueGoldMax=(BuildingValue)0x486d30;
BuildingValue g_revenueFoodMax=(BuildingValue)0x486ea0;
BuildingValue g_revenueHarvest=(BuildingValue)0x4829d0;
struct RevenueObservation {
    RecoveryActor actor;
    int effect=-1,before=0,normalAfter=0;
};
void __cdecl revenueSeed(int effect,int full,int activeFlag) {
    auto& cycle=effect==89?g_goldRevenue:g_foodRevenue;
    cycle=RevenueCycle{};
    if(!skilltelemetry::enabled() || !activeFlag || !g_incomeProviders.city || full<0)return;
    auto actor=g_incomeProviders.holders[effect];
    if(g_incomeProviders.generation!=skilltelemetry::contextGeneration() || !recoveryActorValid(actor,effect))return;
    cycle.city=g_incomeProviders.city;cycle.actor=actor;cycle.full=full;
    if(effect==90)cycle.harvest=g_revenueHarvest((void*)0x7201958)!=0;
}
uint32_t g_goldRevenueSeedResume=0x590544,g_foodRevenueSeedResume=0x5905f5;
__declspec(naked) void goldRevenueSeedBridge() {
    __asm {
        pushfd
        pushad
        push dword ptr [esp+0x1d8]
        push ebp
        push 89
        call revenueSeed
        add esp,12
        popad
        popfd
        mov eax,[esp+0x1b4]
        jmp dword ptr [g_goldRevenueSeedResume]
    }
}
__declspec(naked) void foodRevenueSeedBridge() {
    __asm {
        pushfd
        pushad
        push dword ptr [esp+0x1dc]
        push edi
        push 90
        call revenueSeed
        add esp,12
        popad
        popfd
        mov eax,[esp+0x1b8]
        jmp dword ptr [g_foodRevenueSeedResume]
    }
}
RevenueObservation revenueObservation(const RevenueCycle& cycle,void* building,bool gold,int normal) {
    RevenueObservation obs;
    int effect=gold?89:90;
    if(!cycle.city || normal<0 || !recoveryActorValid(cycle.actor,effect))return obs;
    obs.actor=cycle.actor;obs.effect=effect;
    obs.before=(gold?g_telemetryGold:g_telemetryFood)(building);
    int cap=(gold?g_revenueGoldMax:g_revenueFoodMax)(building);
    // Match 0x4ae2a0/0x4ae300: signed 32-bit sum, lower bound zero, native capacity.
    int sum=(int32_t)((uint32_t)obs.before+(uint32_t)normal);
    obs.normalAfter=sum<=0?0:sum<cap?sum:cap;
    return obs;
}
RevenueObservation monthlyRevenueObservation(void* building,int requested,bool gold,bool port,bool bonus) {
    auto& cycle=gold?g_goldRevenue:g_foodRevenue;
    if(!skilltelemetry::enabled() || cycle.city!=g_incomeProviders.city || !cycle.city ||
       (port?building==cycle.city:building!=cycle.city))return RevenueObservation{};
    int amount=cycle.full/2;
    if(port)amount/=5;
    if(bonus)amount/=2;
    if(amount!=requested)return RevenueObservation{};
    int normal=gold || cycle.harvest?cycle.full:0;
    if(port)normal/=5;
    if(bonus)normal/=2;
    return revenueObservation(cycle,building,gold,normal);
}
void recordRevenueGrant(const RevenueObservation& obs,void* building,uintptr_t caller,bool bonus,bool offMonth) {
    if(!recoveryActorValid(obs.actor,obs.effect))return;
    bool gold=obs.effect==89;
    const char* event=offMonth?"between_month_gold_collection":gold?
        (bonus?"monthly_gold_bonus_installment":"monthly_gold_installment"):
        (bonus?"food_bonus_installment":"food_collection_installment");
    skilltelemetry::appliedBonus(obs.actor.person,obs.actor.owned,obs.effect,
        "economy",event,gold?"gold":"food",obs.before,(gold?g_telemetryGold:g_telemetryFood)(building),
        obs.normalAfter,caller,incomeTarget(building));
}
int revenueGrant(void* proc,void* building,int requested,bool gold,bool port,uintptr_t caller) {
    auto obs=monthlyRevenueObservation(building,requested,gold,port,false);
    int result=(gold?g_goldIncomeGrantOriginal:g_foodIncomeGrantOriginal)(proc,building,requested);
    recordRevenueGrant(obs,building,caller,false,false);
    return result;
}
int __fastcall cityGoldRevenue(void* p,void*,void* b,int n){return revenueGrant(p,b,n,true,false,0x59055d);}
int __fastcall cityFoodRevenue(void* p,void*,void* b,int n){return revenueGrant(p,b,n,false,false,0x59060e);}
int __fastcall portGoldRevenue(void* p,void*,void* b,int n){return revenueGrant(p,b,n,true,true,0x5906d6);}
int __fastcall portFoodRevenue(void* p,void*,void* b,int n){return revenueGrant(p,b,n,false,true,0x59072b);}

int __fastcall offMonthTaxHas(void* building,void*,int effect) {
    typedef int(__thiscall* Has)(void*,int);
    g_offMonthRevenue=RevenueCycle{};
    auto original=(*(Has*)(*(uint8_t**)building+0x58));
    if(!skilltelemetry::enabled())return original(building,effect);
    uint64_t generation=skilltelemetry::contextGeneration();
    OffMonthTaxProbe probe;
    int result=original(building,effect);
    if(result && effect==89 && generation==skilltelemetry::contextGeneration() && recoveryActorValid(probe.actor,89)) {
        g_offMonthRevenue.city=building;g_offMonthRevenue.actor=probe.actor;
    }
    return result;
}
uint32_t g_offMonthTaxResume=0x59964e;
__declspec(naked) void offMonthTaxBridge() {
    __asm {
        call offMonthTaxHas
        test eax,eax
        jmp dword ptr [g_offMonthTaxResume]
    }
}
int offMonthTaxGrant(void* proc,void* building,int requested,bool port,uintptr_t caller) {
    RevenueObservation obs;
    if(requested>=0 && (port?building!=g_offMonthRevenue.city:building==g_offMonthRevenue.city))
        obs=revenueObservation(g_offMonthRevenue,building,true,0);
    int result=g_goldIncomeGrantOriginal(proc,building,requested);
    recordRevenueGrant(obs,building,caller,false,true);
    return result;
}
int __fastcall offMonthCityTax(void* p,void*,void* b,int n){return offMonthTaxGrant(p,b,n,false,0x599690);}
int __fastcall offMonthPortTax(void* p,void*,void* b,int n){return offMonthTaxGrant(p,b,n,true,0x5996eb);}
