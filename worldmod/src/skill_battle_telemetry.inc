// Observe committed unit resource changes, independently of their animation/message paths.
thread_local RecoveryActor g_rageActor;
thread_local RecoveryActor g_heartActor;
thread_local RecoveryActor g_musicActor;
thread_local RecoveryActor g_poetryActor;
typedef int(__thiscall* UnitEnergy)(void*,int);
UnitEnergy g_unitEnergyOriginal=(UnitEnergy)0x4964f0;
IncomeGrant g_unitTroopsOriginal=(IncomeGrant)0x4ae4a0;
BuildingId g_telemetryUnitId=(BuildingId)0x4917c0;

skilltelemetry::Target unitTarget(void* unit) {
    skilltelemetry::Target target;
    target.unit=g_telemetryUnitId((void*)0x7201958,unit);
    target.actorUnit=target.unit;
    target.force=(*(BuildingValue*)(*(uint8_t**)unit+0x40))(unit);
    return target;
}
uint8_t* __fastcall rageActorLookup(void* world,void*,int id) {
    uint64_t generation=skilltelemetry::contextGeneration();
    auto actor=g_telemetryPerson(world,id);
    g_rageActor=recoveryActor(actor);
    if(generation!=g_rageActor.generation || !recoveryActorValid(g_rageActor,30))g_rageActor={};
    return actor;
}
uint8_t* __fastcall heartActorLookup(void* world,void*,int id) {
    uint64_t generation=skilltelemetry::contextGeneration();
    auto actor=g_telemetryPerson(world,id);
    g_heartActor=recoveryActor(actor);
    if(generation!=g_heartActor.generation || !recoveryActorValid(g_heartActor,22))g_heartActor={};
    return actor;
}
int unitEnergyEffect(void* unit,int requested,RecoveryActor& source,int effect,const char* scene,const char* event,uintptr_t caller) {
    auto actor=source;source={};
    bool observe=recoveryActorValid(actor,effect);
    auto energy=observe?(*(BuildingValue*)(*(uint8_t**)unit+0x54)):nullptr;
    int before=observe?energy(unit):0;
    int result=g_unitEnergyOriginal(unit,requested);
    if(observe && recoveryActorValid(actor,effect))skilltelemetry::appliedBonus(actor.person,actor.owned,effect,scene,event,
        "energy",before,energy(unit),before,caller,unitTarget(unit));
    return result;
}
int __fastcall rageEnergy(void* unit,void*,int requested) {
    return unitEnergyEffect(unit,requested,g_rageActor,30,"battle","attacked_energy_recovery",(uintptr_t)_ReturnAddress());
}
int __fastcall elationEnergy(void* unit,void*,int requested) {
    return unitEnergyEffect(unit,requested,g_elationActor,11,"battle","defeat_energy_recovery",(uintptr_t)_ReturnAddress());
}
int __fastcall musicEnergy(void* unit,void*,int requested) {
    return unitEnergyEffect(unit,requested,g_musicActor,75,"turn_update","field_energy_recovery",(uintptr_t)_ReturnAddress());
}
int __fastcall poetryEnergy(void* unit,void*,int requested) {
    return unitEnergyEffect(unit,requested,g_poetryActor,76,"turn_update","military_band_energy_bonus",(uintptr_t)_ReturnAddress());
}
int __fastcall heartTroops(void* proc,void*,void* unit,int requested) {
    auto actor=g_heartActor;g_heartActor={};
    bool observe=recoveryActorValid(actor,22);
    auto troops=observe?(*(BuildingValue*)(*(uint8_t**)unit+0x50)):nullptr;
    int before=observe?troops(unit):0;
    int result=g_unitTroopsOriginal(proc,unit,requested);
    if(observe && recoveryActorValid(actor,22))skilltelemetry::appliedBonus(actor.person,actor.owned,22,"battle",
        "attack_troops_absorbed","troops",before,troops(unit),before,(uintptr_t)_ReturnAddress(),unitTarget(unit));
    return result;
}
int __fastcall restFind(void* unit,void*,int queried) {
    typedef int(__thiscall* Find)(void*,int);
    if(queried==75)g_musicActor=g_poetryActor={};
    else g_poetryActor={};
    uint64_t generation=skilltelemetry::contextGeneration();
    QueryOrigin origin(queried==75?0x59a2b7:0x59a2c4);
    int id=(*(Find*)(*(uint8_t**)unit+0x5c))(unit,queried);
    const void* actor=nullptr;
    if(skilltelemetry::enabled() && id>=0) {
        auto p=g_telemetryPerson((void*)0x7201958,id);
        if(p && contains(*(int*)(p+0xe8),queried))actor=p;
    }
    auto pending=recoveryActor(actor);
    if(pending.generation!=generation)pending={};
    if(queried==75)g_musicActor=pending;else g_poetryActor=pending;
    return id;
}
uint32_t g_musicFindResume=0x59a2bb,g_poetryFindResume=0x59a2c8;
__declspec(naked) void musicFindBridge() {
    __asm {
        call restFind
        mov [esp+0x18],eax
        jmp dword ptr [g_musicFindResume]
    }
}
__declspec(naked) void poetryFindBridge() {
    __asm {
        call restFind
        mov edx,[esi]
        mov ecx,esi
        jmp dword ptr [g_poetryFindResume]
    }
}

// Damage records are 0x80 bytes. The two native attack calculators zero them and
// assign +0x14 only in the sweep/wind branches (5 or 20). +0x70 is 0 or 3.
// The ambush wrapper overwrites +0x14 with 10/20 and tags +0x70=1, so it
// must not be attributed to pressure merely because its numeric cost is 20.
int __fastcall pressureEnergy(void* unit,void*,int requested,void* attacker,const uint8_t* damage) {
    const uint8_t* actor=nullptr;
    int effect=-1,normalLoss=0;
    int kind=damage?*(const int*)(damage+0x70):-1;
    if(skilltelemetry::enabled() && attacker && damage && (kind==0 || kind==3) &&
       (requested==-5 || requested==-20) && *(const int*)(damage+0x14)==-requested) {
        typedef const uint32_t*(__thiscall* Position)(void*);
        auto sourcePosition=(*(Position*)(*(uint8_t**)attacker+0x3c))(attacker);
        auto targetPosition=(*(Position*)(*(uint8_t**)unit+0x3c))(unit);
        if(*sourcePosition==*(const uint32_t*)(damage+0x58) && *targetPosition==*(const uint32_t*)(damage+0x5c)) {
            typedef int(__thiscall* Find)(void*,int);
            auto find=(*(Find*)(*(uint8_t**)attacker+0x5c));
            QueryOrigin origin(0x584ff7);
            int wind=find(attacker,10),holder=-1;
            if(requested==-20 && wind>=0) {
                effect=10;holder=wind;normalLoss=find(attacker,9)>=0?5:0;
            } else if(requested==-5 && wind<0) {
                effect=9;holder=find(attacker,9);
            }
            if(holder>=0) {
                auto person=g_telemetryPerson((void*)0x7201958,holder);
                if(person && contains(*(int*)(person+0xe8),effect))actor=person;
            }
        }
    }
    auto energy=actor?(*(BuildingValue*)(*(uint8_t**)unit+0x54)):nullptr;
    int before=actor?energy(unit):0;
    int result=g_unitEnergyOriginal(unit,requested);
    if(actor) {
        int baseline=before>normalLoss?before-normalLoss:0;
        auto target=unitTarget(unit);
        target.actorUnit=g_telemetryUnitId((void*)0x7201958,attacker);
        skilltelemetry::appliedBonus(actor,*(const int*)(actor+0xe8),effect,"battle","attack_energy_pressure",
            "energy",before,energy(unit),baseline,0x584ff7,target);
    }
    return result;
}
uint32_t g_pressureEnergyResume=0x584ffc;
__declspec(naked) void pressureEnergyBridge() {
    __asm {
        // The native apply routine has already resolved EBX=attacker, ESI=target,
        // EDI=the queued damage record. Keep its original requested argument intact.
        push edi
        push ebx
        push dword ptr [esp+8]
        call pressureEnergy
        add esp,4
        jmp dword ptr [g_pressureEnergyResume]
    }
}
