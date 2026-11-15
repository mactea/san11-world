//595250 settles an area attack. Only its taken44/same-force exclusion is
// an effect; sharing its predicates with an estimate does not exclude a target.
struct AreaFriendlyScope;
thread_local AreaFriendlyScope* g_areaFriendly=nullptr;
struct AreaFriendlyScope {
    uint8_t* records;uint32_t origin,center;int tactic;
    TrapObject source,target;PersonReceipt provider;bool ready=false;
    AreaFriendlyScope* previous;
    explicit AreaFriendlyScope(uint8_t* p):records(p),origin(*(uint32_t*)(p+0x58)),
        center(*(uint32_t*)(p+0x5c)),tactic(*(int*)(p+0x50)),previous(g_areaFriendly){g_areaFriendly=this;}
    ~AreaFriendlyScope(){g_areaFriendly=previous;}
};
uint32_t g_areaFriendlyRootResume=0x595256,g_areaFriendlyHasResume=0x59537c,
    g_areaFriendlySkip=0x59548e,g_areaFriendlyContinue=0x59538e;
__declspec(naked) void areaFriendlyNative(){__asm {sub esp,0xac
    jmp dword ptr [g_areaFriendlyRootResume]}}
typedef int(__thiscall* AreaFriendlyHandler)(void*,uint8_t*);
AreaFriendlyHandler g_areaFriendlyOriginal=(AreaFriendlyHandler)areaFriendlyNative;
int __fastcall areaFriendlyHandler(void* self,void*,uint8_t* records){
    if(!skilltelemetry::enabled())return g_areaFriendlyOriginal(self,records);
    AreaFriendlyScope scope(records);return g_areaFriendlyOriginal(self,records);
}
int __cdecl areaFriendlyHas(void* source,int effect,uint8_t* records,void* target){
    typedef int(__thiscall* Has)(void*,int);
    auto native=*(Has*)(*(uint8_t**)source+0x58);auto scope=g_areaFriendly;
    if(!skilltelemetry::enabled() || !scope || scope->records!=records || effect!=44)return native(source,effect);
    scope->ready=false;scope->source=TrapObject(source);scope->target=TrapObject(target);scope->provider=PersonReceipt{};
    ProgressionProbe probe(44,&scope->provider);probe.scene="battle";probe.event="same_force_area_target_eligibility";
    int result=native(source,effect);
    scope->ready=result && probe.actor && scope->source.unit>=0 && scope->source.position==scope->origin;
    return result;
}
__declspec(naked) void areaFriendlyHasBridge(){__asm {push edi
    push esi
    push [esp+8]
    push ecx
    call areaFriendlyHas
    add esp,20
    test eax,eax
    jmp dword ptr [g_areaFriendlyHasResume]}}
void __cdecl areaFriendlyExcluded(uint8_t* records,void* source,void* target,const uint8_t* cell){
    auto scope=g_areaFriendly;
    if(!skilltelemetry::enabled() || !scope || !scope->ready || scope->records!=records)return;
    scope->ready=false;
    if(scope->source.pointer!=source || scope->target.pointer!=target || !scope->provider.valid(44) ||
       !scope->source.live() || !scope->target.live() || scope->source.force<0 ||
       scope->source.force!=scope->target.force || !afterAttackMember(source,scope->provider.person))return;
    auto affected=scope->target.unit>=0?unitTarget(target):incomeTarget(target);
    affected.actorUnit=scope->source.unit;affected.equipment=scope->source.weapon;
    affected.x=(int16_t)scope->target.position;affected.y=(int16_t)(scope->target.position>>16);
    affected.terrain=*(const uint32_t*)(cell+4)&31;
    char context[224];sprintf_s(context,"{\"tactic_id\":%d,\"command_target_x\":%d,\"command_target_y\":%d,\"exclusion_reason\":\"same_force\",\"counterfactual_evaluated\":false}",
        scope->tactic,(int16_t)scope->center,(int16_t)(scope->center>>16));affected.context=context;
    skilltelemetry::appliedEvent(scope->provider.person,scope->provider.owned,44,"battle",
        "same_force_area_target_excluded",0x595388,affected);
}
__declspec(naked) void areaFriendlyTakenBridge(){__asm {jz continueTarget
    pushfd
    pushad
    push ebx
    push edi
    push ebp
    push esi
    call areaFriendlyExcluded
    add esp,16
    popad
    popfd
    jmp dword ptr [g_areaFriendlySkip]
continueTarget:
    jmp dword ptr [g_areaFriendlyContinue]}}
const struct AreaFriendlyLayout {uint32_t start;int size;const char* bytes;} g_areaFriendlyLayouts[]={
    {0x595250,16,"\x81\xec\xac\x00\x00\x00\x53\x55\x56\x8b\xb4\x24\xbc\x00\x00\x00"},
    {0x595370,30,"\x8b\x45\x00\x6a\x2c\x8b\xcd\xff\x50\x58\x85\xc0\x74\x10\x57\x8b\xcd\xe8\x3a\x18\xf0\xff\x85\xc0\x0f\x85\x00\x01\x00\x00"},
    {0x59548e,26,"\x8b\x4c\x24\x14\x8b\x44\x24\x10\x83\xc1\x04\x48\x89\x4c\x24\x14\x89\x44\x24\x10\x0f\x85\x88\xfe\xff\xff"},
    {0x595500,6,"\x57\xe8\x6a\xc3\x01\x00"},
};
bool verifyAreaFriendlyLayout(){
    for(const auto& s:g_areaFriendlyLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
