// Only the route-cell call in the actual movement settlement (0x584820).
// Calling the shared calculator for a preview must not emit an applied event.
typedef int(__cdecl* MovementHazard)(int*,void*,uint32_t);
MovementHazard g_movementHazardOriginal=(MovementHazard)0x5ae500;
const uint32_t* g_movementMapOperand=(const uint32_t*)0x5a07aa;
int movementTerrain(uint32_t hex) {
    const uint8_t* code=(const uint8_t*)g_movementMapOperand-0x3a;
    int height=*(const int*)(code+0x13),width=*(const int*)(code+0x1f),stride=*(const int*)(code+0x2a);
    int x=(int16_t)hex,y=(int16_t)(hex>>16);
    auto cells=(const uint8_t*)(uintptr_t)*g_movementMapOperand;
    if(!cells || width<1 || height<1 || width!=stride || x<0 || x>=height || y<0 || y>=width)return -1;
    return *(const uint32_t*)(cells+20*((size_t)x*width+y)+4)&31;
}
int __cdecl movementHazard(int* hazard,void* unit,uint32_t hex) {
    if(!skilltelemetry::enabled())return g_movementHazardOriginal(hazard,unit,hex);
    TerrainProbe probe;
    int result=g_movementHazardOriginal(hazard,unit,hex);
    // Native fire immunity returns 1 with zero cause/damage; cliff and poison return 0.
    // Never rerun this RNG-consuming calculator to invent an amount of prevented loss.
    bool immune=probe.effect!=31 && hazard[0]==0 && hazard[1]==0;
    bool amplified=probe.effect==31 && result==1 && hazard[0]==3 && hazard[1]>0 && !(hazard[1]&1);
    if(probe.actor && (immune || amplified)) {
        auto target=unitTarget(unit);
        target.x=(int16_t)hex;target.y=(int16_t)(hex>>16);
        target.terrain=movementTerrain(hex);
        int owned=*(const int*)((const uint8_t*)probe.actor+0xe8);
        if(amplified) {
            // Native 0x5ae570..0x5ae575 doubles the same RNG sample already in hazard[1].
            // This is the damage of one route cell, not the eventual capped unit troop loss.
            int normal=hazard[1]/2;
            skilltelemetry::appliedBonus(probe.actor,owned,31,"movement","terrain_fire_damage_doubled",
                "route_hazard_damage",normal,hazard[1],normal,0x5848a9,target);
        } else {
            const char* event=probe.effect==6?"cliff_immunity":probe.effect==8?"poison_immunity":"terrain_fire_immunity";
            skilltelemetry::appliedEvent(probe.actor,owned,probe.effect,"movement",event,0x5848a9,target);
        }
    }
    return result;
}

typedef void(__cdecl* MovementStats)(uint8_t*,void**,int,int,int,int,int,int);
MovementStats g_movementStatsOriginal=(MovementStats)0x496570;
typedef int(__cdecl* MovementPersonValid)(void*);
MovementPersonValid g_movementPersonValid=(MovementPersonValid)0x47a630;
typedef void*(__thiscall* MovementUnitById)(void*,int);
MovementUnitById g_movementUnitById=(MovementUnitById)0x490e70;
void __cdecl movementStats(uint8_t* stats,void** members,int weapon,int troops,int transport,int option,
                          int water,int full,uint8_t* unit) {
    if(!skilltelemetry::enabled()) {
        g_movementStatsOriginal(stats,members,weapon,troops,transport,option,water,full);return;
    }
    MovementStatProbe probe;
    int before=stats[0x20];
    g_movementStatsOriginal(stats,members,weapon,troops,transport,option,water,full);
    if(probe.actor && stats==unit+0xac) {
        auto target=unitTarget(unit);
        // A UI-created temporary unit can also recalculate its own stats. Require the
        // native ID -> pointer round trip before calling this a persistent world write.
        if(target.unit<0 || g_movementUnitById((void*)0x7201958,target.unit)!=unit)return;
        // Native 0x496b0d..0x496b64: strong march overrides long drive for horses.
        // The selected branch adds 5 (strong/transport) or 3 (long drive) to a byte.
        // Without strong march a horse unit can fall back to long drive (+3).
        // Reuse the native read-only validity check, without another predicate log or stat run.
        int fallback=0;
        if(probe.effect==2 && weapon==4 && !transport)for(int i=0;i<3;++i) {
            if(g_movementPersonValid(members[i]) && contains(*(const int*)((const uint8_t*)members[i]+0xe8),3)) {
                fallback=3;break;
            }
        }
        int after=stats[0x20],normal=(uint8_t)(after-(probe.effect==3?3:5)+fallback);
        target.equipment=weapon;
        skilltelemetry::appliedBonus(probe.actor,*(const int*)((const uint8_t*)probe.actor+0xe8),probe.effect,
            "unit_stats","movement_capacity_refreshed","movement_points",before,after,normal,0x496eff,target);
    }
}
__declspec(naked) void movementStatsBridge() {
    __asm {
        // Original cdecl arguments stay in place for the native caller's cleanup.
        // ESI is the actual unit; append it to a copied argument list for the observer.
        push esi
        push dword ptr [esp+36]
        push dword ptr [esp+36]
        push dword ptr [esp+36]
        push dword ptr [esp+36]
        push dword ptr [esp+36]
        push dword ptr [esp+36]
        push dword ptr [esp+36]
        push dword ptr [esp+36]
        call movementStats
        add esp,36
        ret
    }
}
