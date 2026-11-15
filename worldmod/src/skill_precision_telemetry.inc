// A capped balance is not evidence that the native reward accepted its force.
// Require its actual setter CALL as well as a live captured20 provider.
typedef int(__thiscall* PrecisionReward)(void*,void*,int,void*);
typedef int(__thiscall* PrecisionPoints)(void*,int);
PrecisionReward g_precisionRewardOriginal=(PrecisionReward)0x4b6580;
PrecisionPoints g_precisionPointsOriginal=(PrecisionPoints)0x481a80;
BaseById g_precisionForceById=(BaseById)0x490aa0;
bool precisionProviderLive(const DefeatScope* s) {
    if(!s || s!=g_defeat || s->generation!=skilltelemetry::contextGeneration() ||
       !recoveryActorValid(s->precision,20) || growthPersonId((void*)s->precision.person)!=s->precisionId ||
       growthTarget((uint8_t*)s->precision.person,s->precisionId).force!=s->precisionProviderForce ||
       s->target.unit<0 || g_movementUnitById((void*)0x7201958,s->target.unit)!=s->defeated ||
       unitTarget(s->defeated).force!=s->target.force)return false;
    if(s->precisionSource<0 || (s->precisionBase?
        g_baseById((void*)0x7201958,s->precisionSource):g_movementUnitById((void*)0x7201958,s->precisionSource))!=s->attacker)return false;
    if((*(BuildingValue*)(*(uint8_t**)s->attacker+0x40))(s->attacker)!=s->precisionForce)return false;
    if(!s->precisionBase)return afterAttackMember(s->attacker,s->precision.person);
    // Native4cf2f0 tests home/affiliated base+9c, not current location+98.
    int status=*(const int*)((const uint8_t*)s->precision.person+0xa0);
    return *(const int*)((const uint8_t*)s->precision.person+0x9c)==s->precisionSource && status>=0 && status<4;
}
void precisionProvider(const void* person,bool found,uintptr_t caller) {
    auto s=g_defeat;if(!s || s->precisionConsumed)return;s->precision={};
    if(!found || !skilltelemetry::enabled() || s->generation!=skilltelemetry::contextGeneration() || !s->attacker)return;
    int unit=g_telemetryUnitId((void*)0x7201958,s->attacker);
    s->precisionBase=unit<0;s->precisionSource=s->precisionBase?g_telemetryBaseId((void*)0x7201958,s->attacker):unit;
    if(caller!=(s->precisionBase?0x4bf126u:0x4bf079u))return;
    s->precision=recoveryActor(person);s->precisionId=growthPersonId((void*)person);
    s->precisionForce=(*(BuildingValue*)(*(uint8_t**)s->attacker+0x40))(s->attacker);
    s->precisionProviderForce=growthTarget((uint8_t*)person,s->precisionId).force;
    if(!precisionProviderLive(s))s->precision={};
}
struct PrecisionStoreScope;
thread_local PrecisionStoreScope* g_precisionStore=nullptr;
struct PrecisionStoreScope {
    void* force;
    int storedBefore=-1,after=-1,requested=-1,forceId=-1,writes=0;
    uint64_t generation;
    PrecisionStoreScope* previous;
    explicit PrecisionStoreScope(void* f):force(f),generation(skilltelemetry::contextGeneration()),previous(g_precisionStore){g_precisionStore=this;}
    ~PrecisionStoreScope(){g_precisionStore=previous;}
};
int __fastcall precisionPoints(uint8_t* force,void*,int requested) {
    auto s=g_precisionStore;
    bool observe=skilltelemetry::enabled() && s && s->force==force && s->generation==skilltelemetry::contextGeneration();
    int before=observe?*(const uint16_t*)(force+0xa2):0;
    int result=g_precisionPointsOriginal(force,requested);
    if(observe && s->generation==skilltelemetry::contextGeneration()) {
        ++s->writes;s->storedBefore=before;s->requested=requested;s->after=*(const uint16_t*)(force+0xa2);
        typedef int(__thiscall* ForceId)(void*);
        s->forceId=(*(ForceId*)(*(uint8_t**)force+0x28))(force);
    }
    return result;
}
int __fastcall precisionReward(void* self,void*,uint8_t* force,int requested,void* object) {
    auto parent=g_defeat;
    bool ownsPending=parent && !parent->precisionConsumed;
    bool observe=force && requested==100 && ownsPending && precisionProviderLive(parent) &&
        g_precisionForceById((void*)0x7201958,parent->precisionForce)==force;
    auto actor=observe?parent->precision:RecoveryActor{};
    if(ownsPending)parent->precisionConsumed=true;
    int before=observe?*(const uint16_t*)(force+0xa2):0;
    PrecisionStoreScope store(observe?force:nullptr);
    int result=g_precisionRewardOriginal(self,force,requested,object);
    int expected=before+50;if(expected>10000)expected=10000;
    if(observe && precisionProviderLive(parent) && store.forceId==parent->precisionForce && store.writes==1 && store.storedBefore==before &&
       store.requested==expected && store.after==expected && *(const uint16_t*)(force+0xa2)==expected) {
        // Guarded native4b6460 halves positive requests and caps at10000.
        int without=before+25;if(without>10000)without=10000;
        skilltelemetry::Target target;target.force=store.forceId;
        target.actorUnit=parent->precisionBase?-1:parent->precisionSource;
        char context[256];sprintf_s(context,"{\"source_base\":%d,\"source_unit\":%d,\"defeated_unit\":%d,\"defeated_force\":%d,\"provider_force\":%d,\"reward_force\":%d,\"defeat_root\":\"0x4be790\"}",
            parent->precisionBase?parent->precisionSource:-1,target.actorUnit,parent->target.unit,parent->target.force,parent->precisionProviderForce,store.forceId);
        target.context=context;
        skilltelemetry::appliedBonus(actor.person,actor.owned,20,"battle","defeat_technology_reward_bonus",
            "technology_points",before,store.after,without,0x4bf16f,target);
    }
    if(ownsPending && parent==g_defeat)parent->precision={};
    return result;
}
struct PrecisionLayout {uint32_t start;int size;const char* bytes;};
const PrecisionLayout g_precisionLayouts[]={
    {0x4cf32b,8,"\x3b\xae\x9c\x00\x00\x00\x75\x18"},
    {0x489fe0,23,"\x8b\x44\x24\x04\x8b\x89\xa0\x00\x00\x00\x50\x51\xe8\x3f\xeb\xff\xff\x83\xc4\x08\xc2\x04\x00"},
    {0x488b30,148,"\x8b\x44\x24\x04\x40\x83\xf8\x09\x77\x5d\xff\x24\x85\x9c\x8b\x48\x00\x8b\x44\x24\x08\x83\xe0\x01\xc3\x8b\x44\x24\x08\x83\xe0\x02\xc3\x8b\x44\x24\x08\x83\xe0\x04\xc3\x8b\x44\x24\x08\x83\xe0\x08\xc3\x8b\x44\x24\x08\x83\xe0\x10\xc3\x8b\x44\x24\x08\x83\xe0\x20\xc3\x8b\x44\x24\x08\x83\xe0\x40\xc3\x8b\x44\x24\x08\x25\x80\x00\x00\x00\xc3\x8b\x44\x24\x08\x25\x00\x01\x00\x00\xc3\x8b\x44\x24\x08\x25\x00\x10\x00\x00\xc3\x33\xc0\xc3\x8b\xff\x8d\x8b\x48\x00\x41\x8b\x48\x00\x49\x8b\x48\x00\x51\x8b\x48\x00\x59\x8b\x48\x00\x61\x8b\x48\x00\x69\x8b\x48\x00\x71\x8b\x48\x00\x79\x8b\x48\x00\x83\x8b\x48\x00"},
    {0x4b6460,107,"\x83\xec\x10\x53\x56\x8b\x74\x24\x20\x85\xf6\x7e\x13\x8b\xc6\x99\x2b\xc2\xd1\xf8\x83\xf8\x01\xbe\x01\x00\x00\x00\x7c\x02\x8b\xf0\x8b\x5c\x24\x1c\x53\xe8\xa6\x41\xfc\xff\x83\xc4\x04\x85\xc0\x0f\x84\xdc\x00\x00\x00\x8b\xcb\xe8\xb4\xae\xfc\xff\x85\xc0\x0f\x84\xcd\x00\x00\x00\x57\x0f\xb7\xbb\xa2\x00\x00\x00\x03\xf7\x85\xf6\x7f\x04\x33\xf6\xeb\x0d\x81\xfe\x10\x27\x00\x00\x7c\x05\xbe\x10\x27\x00\x00\x56\x8b\xcb\xe8\xb5\xb5\xfc\xff"},
    {0x4b6580,83,"\x53\x55\x8b\x6c\x24\x0c\x55\x8b\xd9\xe8\xa2\x40\xfc\xff\x83\xc4\x04\x85\xc0\x74\x37\x56\x8b\x35\xd0\xc1\x79\x00\x57\x8b\x7c\x24\x1c\x57\xe8\x89\x40\xfc\xff\x83\xc4\x04\x85\xc0\x74\x09\x8b\x07\x8b\xcf\xff\x50\x3c\x8b\x30\x8b\x4c\x24\x18\x56\x51\x55\x8b\xcb\xe8\x9b\xfe\xff\xff\x5f\x5e\x5d\x5b\xc2\x0c\x00\x5d\x33\xc0\x5b\xc2\x0c\x00"},
    {0x481a80,42,"\x8b\x44\x24\x04\x66\x85\xc0\x77\x0c\x33\xc0\x66\x89\x81\xa2\x00\x00\x00\xc2\x04\x00\x66\x3d\x10\x27\x72\x05\xb8\x10\x27\x00\x00\x66\x89\x81\xa2\x00\x00\x00\xc2\x04\x00"},
};
bool verifyPrecisionLayout() {
    for(const auto& layout:g_precisionLayouts)if(memcmp((const void*)(g_strategyCodeBase+layout.start-0x400000),layout.bytes,layout.size))return false;
    return true;
}
