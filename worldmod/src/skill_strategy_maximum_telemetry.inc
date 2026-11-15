// Exact native max-stat comparison sites within5ae980. Only their winning
// branches may attribute a forced actual command decision to a provider.
uintptr_t g_strategyCodeBase=0x400000;
struct StrategyMaximumSite {uint32_t call,start;int effect;bool defensive;int size;const char* bytes;};
const StrategyMaximumSite g_strategyMaximumSites[]={
    {0x5aebd4,0x5aebce,60,true,19,"\x6a\x02\x6a\x3c\x8b\xcb\xe8\xa7\x6a\xee\xff\x3b\xc6\x0f\x8f\x8e\x06\x00\x00"},
    {0x5aebe7,0x5aebe1,63,true,19,"\x6a\x02\x6a\x3f\x8b\xcb\xe8\x94\x6a\xee\xff\x3b\xc6\x0f\x8f\x7b\x06\x00\x00"},
    {0x5aebfe,0x5aebf8,53,false,15,"\x6a\x02\x6a\x35\x8b\xce\xe8\x7d\x6a\xee\xff\x3b\xc7\x7f\x31"},
    {0x5aec0d,0x5aec07,57,false,15,"\x6a\x02\x6a\x39\x8b\xce\xe8\x6e\x6a\xee\xff\x3b\xc7\x7f\x22"},
    {0x5aec1c,0x5aec16,62,false,15,"\x6a\x02\x6a\x3e\x8b\xce\xe8\x5f\x6a\xee\xff\x3b\xc7\x7f\x13"},
    {0x5aec2b,0x5aec25,63,false,19,"\x6a\x02\x6a\x3f\x8b\xce\xe8\x50\x6a\xee\xff\x3b\xc7\x0f\x8e\x3b\x08\x00\x00"},
    {0x5aec4f,0x5aec49,53,false,15,"\x6a\x02\x6a\x35\x8b\xce\xe8\x2c\x6a\xee\xff\x3b\xc7\x7f\xe0"},
    {0x5aec5e,0x5aec58,57,false,15,"\x6a\x02\x6a\x39\x8b\xce\xe8\x1d\x6a\xee\xff\x3b\xc7\x7f\xd1"},
    {0x5aec6d,0x5aec67,62,false,15,"\x6a\x02\x6a\x3e\x8b\xce\xe8\x0e\x6a\xee\xff\x3b\xc7\x7f\xc2"},
    {0x5aec7c,0x5aec76,63,false,15,"\x6a\x02\x6a\x3f\x8b\xce\xe8\xff\x69\xee\xff\x3b\xc7\x7f\xb3"},
    {0x5aed28,0x5aed20,54,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x36\xe8\x53\x69\xee\xff\x3b\xc7\x0f\x8f\x03\xff\xff\xff"},
    {0x5aed3d,0x5aed35,57,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x39\xe8\x3e\x69\xee\xff\x3b\xc7\x0f\x8f\xee\xfe\xff\xff"},
    {0x5aed52,0x5aed4a,63,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x3f\xe8\x29\x69\xee\xff\x3b\xc7\x0f\x8f\xd9\xfe\xff\xff"},
    {0x5aed69,0x5aed63,60,true,19,"\x6a\x02\x6a\x3c\x8b\xcb\xe8\x12\x69\xee\xff\x3b\xc6\x0f\x8f\x8d\x01\x00\x00"},
    {0x5aed7c,0x5aed76,63,true,19,"\x6a\x02\x6a\x3f\x8b\xcb\xe8\xff\x68\xee\xff\x3b\xc6\x0f\x8f\x7a\x01\x00\x00"},
    {0x5aeeaa,0x5aeea2,55,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x37\xe8\xd1\x67\xee\xff\x3b\xc7\x0f\x8f\x81\xfd\xff\xff"},
    {0x5aeebf,0x5aeeb7,57,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x39\xe8\xbc\x67\xee\xff\x3b\xc7\x0f\x8f\x6c\xfd\xff\xff"},
    {0x5aeed4,0x5aeecc,63,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x3f\xe8\xa7\x67\xee\xff\x3b\xc7\x0f\x8f\x57\xfd\xff\xff"},
    {0x5aeeeb,0x5aeee5,60,true,15,"\x6a\x02\x6a\x3c\x8b\xcb\xe8\x90\x67\xee\xff\x3b\xc6\x7f\x0f"},
    {0x5aeefa,0x5aeef4,63,true,15,"\x6a\x02\x6a\x3f\x8b\xcb\xe8\x81\x67\xee\xff\x3b\xc6\x7e\x15"},
    {0x5af012,0x5af00c,57,false,19,"\x6a\x02\x6a\x39\x8b\xce\xe8\x69\x66\xee\xff\x3b\xc7\x0f\x8f\x19\xfc\xff\xff"},
    {0x5af025,0x5af01f,63,false,19,"\x6a\x02\x6a\x3f\x8b\xce\xe8\x56\x66\xee\xff\x3b\xc7\x0f\x8f\x06\xfc\xff\xff"},
    {0x5af0cb,0x5af0c3,57,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x39\xe8\xb0\x65\xee\xff\x3b\xc7\x0f\x8f\x60\xfb\xff\xff"},
    {0x5af0e0,0x5af0d8,63,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x3f\xe8\x9b\x65\xee\xff\x3b\xc7\x0f\x8f\x4b\xfb\xff\xff"},
    {0x5af0f7,0x5af0f1,60,true,19,"\x6a\x02\x6a\x3c\x8b\xcb\xe8\x84\x65\xee\xff\x3b\xc6\x0f\x8f\x6b\x01\x00\x00"},
    {0x5af10a,0x5af104,63,true,19,"\x6a\x02\x6a\x3f\x8b\xcb\xe8\x71\x65\xee\xff\x3b\xc6\x0f\x8f\x58\x01\x00\x00"},
    {0x5af216,0x5af20e,56,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x38\xe8\x65\x64\xee\xff\x3b\xc7\x0f\x8f\x15\xfa\xff\xff"},
    {0x5af22b,0x5af223,57,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x39\xe8\x50\x64\xee\xff\x3b\xc7\x0f\x8f\x00\xfa\xff\xff"},
    {0x5af240,0x5af238,63,false,21,"\x8b\x4c\x24\x20\x6a\x02\x6a\x3f\xe8\x3b\x64\xee\xff\x3b\xc7\x0f\x8f\xeb\xf9\xff\xff"},
    {0x5af257,0x5af251,60,true,15,"\x6a\x02\x6a\x3c\x8b\xcb\xe8\x24\x64\xee\xff\x3b\xc6\x7f\x0f"},
    {0x5af266,0x5af260,63,true,15,"\x6a\x02\x6a\x3f\x8b\xcb\xe8\x15\x64\xee\xff\x3b\xc6\x7e\x15"},
};
bool verifyStrategyMaximumSites(){
    for(const auto& s:g_strategyMaximumSites)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected strategy stat comparison at %08x; rejected",s.call);return false;
    }
    return true;
}
const StrategyMaximumSite* strategyMaximumSite(uintptr_t caller){
    for(const auto& s:g_strategyMaximumSites)if(caller==g_strategyCodeBase+s.call-0x400000+5)return &s;
    return nullptr;
}
bool strategyMaximumActive(uintptr_t caller){return g_strategyImmunity && strategyMaximumSite(caller);}
CityBuilding g_strategyGroundBase=(CityBuilding)0x483b20;
const uint8_t* strategyGroundCell(uint32_t position){
    int x=(int16_t)position,y=(int16_t)(position>>16);
    int h=*(const int*)(g_strategyCodeBase+0x1a0783),w=*(const int*)(g_strategyCodeBase+0x1a078f);
    auto cells=*(const uint8_t* const*)(g_strategyCodeBase+0x1ae9d0);
    if(!cells || h<1 || w<1 || x<0 || x>=h || y<0 || y>=w)return nullptr;
    return cells+20*((size_t)x*w+y);
}
void strategyGroundPrepare(StrategyImmunityScope& scope,uint32_t to,int plan){
    if(plan!=1)return;auto cell=strategyGroundCell(to);if(!cell)return;
    int kind=*(const uint32_t*)cell&3;if(kind!=0 && kind!=2)return;
    scope.groundPosition=to;scope.groundKind=kind;scope.groundCell=cell;
    memcpy(scope.groundSnapshot,cell,sizeof scope.groundSnapshot);
    if(kind==2){
        scope.groundBase=g_strategyGroundBase((void*)cell);if(!scope.groundBase){scope.groundCell=nullptr;return;}
        auto target=incomeTarget(scope.groundBase);scope.groundBaseId=target.base;scope.groundForce=target.force;
        if(target.base<0 || g_baseById((void*)0x7201958,target.base)!=scope.groundBase)scope.groundCell=nullptr;
    }
}
bool strategyGroundLive(const StrategyImmunityScope& scope,uint32_t to){
    if(!scope.groundCell || scope.groundPosition!=to || strategyGroundCell(to)!=scope.groundCell ||
       memcmp(scope.groundSnapshot,scope.groundCell,sizeof scope.groundSnapshot) || !scope.groundProvider.valid(scope.maximumEffect))return false;
    return scope.groundKind==0 || (g_strategyGroundBase((void*)scope.groundCell)==scope.groundBase &&
        g_baseById((void*)0x7201958,scope.groundBaseId)==scope.groundBase &&
        g_telemetryBaseId((void*)0x7201958,scope.groundBase)==scope.groundBaseId &&
        (*(BuildingValue*)(*(uint8_t**)scope.groundBase+0x40))(scope.groundBase)==scope.groundForce);
}
void strategyMaximumObserved(void* unit,int effect,int kind,int value,const void* actor,uintptr_t caller){
    auto scope=g_strategyImmunity;auto site=strategyMaximumSite(caller);
    if(!scope || !site || !actor || kind!=2 || effect!=site->effect ||
       scope->generation!=skilltelemetry::contextGeneration() ||
       unit!=(site->defensive?scope->defender:scope->source))return;
    bool ground=!scope->defender && scope->groundCell && !site->defensive &&
        (site->call==0x5aec4f || site->call==0x5aec5e || site->call==0x5aec6d || site->call==0x5aec7c);
    if(!scope->defender && !ground)return;
    void* opponent=site->defensive?scope->source:scope->defender;
    int threshold=ground?0:((const uint8_t*)opponent)[0xc6];
    // Native comparisons are strictly greater and immediately force0/100.
    // Plan1 retains EDI=0 when the target cell has no unit.
    if(value<=threshold)return;
    scope->maximumUnit=unit;scope->maximumActor=actor;scope->maximumEffect=effect;
    scope->maximumId=growthPersonId((void*)actor);scope->maximumOwned=*(const int*)((const uint8_t*)actor+0xe8);
    scope->maximumValue=value;scope->threshold=threshold;scope->maximumDefensive=site->defensive;
    scope->maximumSite=site->call;
    scope->maximumGround=ground;if(ground)scope->groundProvider=PersonReceipt(actor);
}
StrategyAttempt g_strategyChanceOriginal=(StrategyAttempt)0x5ae980;
int __cdecl strategyChance(void* source,uint32_t from,uint32_t to,int plan,int* detail){
    if(g_strategyImmunity && g_strategyImmunity->source==source)strategyGroundPrepare(*g_strategyImmunity,to,plan);
    int chance=g_strategyChanceOriginal(source,from,to,plan,detail);
    if(g_strategyImmunity)g_strategyImmunity->chance=chance;
    return chance;
}
MovementUnitById g_strategyDefenderOriginal=(MovementUnitById)0x490e70;
void* __fastcall strategyDefender(void* world,void*,int id){
    void* unit=g_strategyDefenderOriginal(world,id);if(g_strategyImmunity)g_strategyImmunity->defender=unit;return unit;
}
void strategyMaximumOutcome(const StrategyImmunityScope& scope,uint32_t to,int plan,int result,const int* detail){
    int chance=scope.maximumDefensive?0:100;
    if(!scope.maximumActor || scope.maximumId<0 || scope.chance!=chance ||
       (result!=0)!=(chance==100) || detail[1]!=(scope.maximumDefensive?1:0) ||
       scope.generation!=skilltelemetry::contextGeneration() || !g_growthValid(scope.source) || (!scope.maximumGround && !g_growthValid(scope.defender)) ||
       growthPersonId((void*)scope.maximumActor)!=scope.maximumId ||
       *(const int*)((const uint8_t*)scope.maximumActor+0xe8)!=scope.maximumOwned ||
       !contains(scope.maximumOwned,scope.maximumEffect) || !afterAttackMember(scope.maximumUnit,scope.maximumActor))return;
    int sourceId=g_telemetryUnitId((void*)0x7201958,scope.source);skilltelemetry::Target target;
    if(sourceId<0 || g_movementUnitById((void*)0x7201958,sourceId)!=scope.source)return;
    if(scope.maximumGround){
        if(plan!=1 || !strategyGroundLive(scope,to))return;
        if(scope.groundKind==2)target=incomeTarget(scope.groundBase);
        target.terrain=*(const uint32_t*)(scope.groundCell+4)&31;
    }else{
        target=unitTarget(scope.defender);
        if(target.unit<0 || g_movementUnitById((void*)0x7201958,target.unit)!=scope.defender)return;
        typedef const uint32_t*(__thiscall* Position)(void*);
        if(*(*(Position*)(*(uint8_t**)scope.defender+0x3c))(scope.defender)!=to)return;
    }
    target.actorUnit=scope.maximumDefensive?target.unit:sourceId;
    target.equipment=*(const int*)((const uint8_t*)scope.maximumUnit+0xa8);
    target.x=(int16_t)to;target.y=(int16_t)(to>>16);
    auto attacking=unitTarget(scope.source);char context[384];
    const char* ground=scope.maximumGround?(scope.groundKind==2?",\"target_kind\":\"building\"":",\"target_kind\":\"terrain\""):"";
    sprintf_s(context,"{\"strategy_id\":%d,\"attacking_unit\":%d,\"attacking_force\":%d,\"stat_id\":2,\"provider_value\":%d,\"opponent_intelligence\":%d,\"success_chance\":%d,\"native_immunity_flag\":%d,\"decision_site\":%u,\"counterfactual_evaluated\":false%s}",
        plan,sourceId,attacking.force,scope.maximumValue,scope.threshold,chance,detail[1],(unsigned)scope.maximumSite,ground);target.context=context;
    skilltelemetry::appliedEvent(scope.maximumActor,scope.maximumOwned,scope.maximumEffect,"strategy",
        scope.maximumDefensive?"strategy_stat_forced_failure":"strategy_stat_forced_success",0x593402,target);
}
const struct StrategyGroundLayout {uint32_t start;int size;const char* bytes;} g_strategyGroundLayouts[]={
    {0x5ae9da,2,"\x33\xff"}, // non-unit opponent intelligence starts at0
    {0x5af4c8,4,"\x45\xec\x5a\x00"}, // plan1 enters the four guarded source comparisons
};
bool verifyStrategyGroundLayout(){
    for(const auto& s:g_strategyGroundLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
