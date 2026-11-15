// M12 uses a separate raw-skill-5 water cost table. Record its base input on
// an actually committed route; never rerun A*, RNG or a monsoon callback.
SkillSteerDescribe g_steeringDescribe=nullptr;
typedef void(__thiscall* SteeringMove)(void*,int,uint32_t,int);
SteeringMove g_steeringMoveOriginal=(SteeringMove)0x56f120;
struct SteeringRoute {
    struct Cell {uint32_t position;int terrain;SkillSteerCosts costs;};
    Cell cells[40];int used=0,actorId=-1,force=-1;
    uint32_t snapshot[44];uint64_t generation=skilltelemetry::contextGeneration();
    uint8_t* unit=nullptr;const void* actor=nullptr;
    int id;uint32_t to;const uint32_t* command;bool observe=false;
    SteeringRoute(int unitId,uint32_t destination,int flag,const uint32_t* route):id(unitId),to(destination),command(route){
    observe=skilltelemetry::enabled() && g_steeringDescribe && command && flag==1;
    if(observe) {
        memcpy(snapshot,command,sizeof snapshot);
        unit=(uint8_t*)g_movementUnitById((void*)0x7201958,id);
        observe=unit && snapshot[0]==(uint32_t)id && snapshot[2]==to && snapshot[1]!=to &&
            snapshot[43]>0 && snapshot[43]<=40 && snapshot[2+snapshot[43]]==to &&
            g_telemetryUnitId((void*)0x7201958,unit)==id && *(uint32_t*)(unit+0x3c)==snapshot[1];
    }
    if(observe) {
        force=unitTarget(unit).force;uint32_t previous=snapshot[1];
        for(unsigned n=0;n<snapshot[43];++n) {
            uint32_t p=snapshot[3+n];int terrain=movementTerrain(p);
            if(terrain<0 || reachDistance(previous,p)!=1){observe=false;break;}
            previous=p;SkillSteerCosts costs;
            if(g_steeringDescribe(unit,terrain,costs)) {
                if(!costs.actor || *(const int*)((const uint8_t*)costs.actor+0xe8)!=5 || (actor && actor!=costs.actor)) {observe=false;break;}
                actor=costs.actor;cells[used++]={p,terrain,costs};
            }
        }
        if(actor)for(int n=0;n<3;++n){int member=*(int*)(unit+0xc+4*n);
            if(member>=0 && g_telemetryPerson((void*)0x7201958,member)==actor){actorId=member;break;}}
        observe=observe && used && actorId>=0;
    }
    }
    bool live() const {
    if(!observe || !skilltelemetry::enabled() || generation!=skilltelemetry::contextGeneration() ||
       memcmp(snapshot,command,sizeof snapshot) || g_movementUnitById((void*)0x7201958,id)!=unit ||
       g_telemetryUnitId((void*)0x7201958,unit)!=id || unitTarget(unit).force!=force ||
       g_telemetryPerson((void*)0x7201958,actorId)!=actor || *(const int*)((const uint8_t*)actor+0xe8)!=5)return false;
    bool member=false;for(int n=0;n<3;++n)if(*(int*)(unit+0xc+4*n)==actorId)member=true;
    return member;
    }
    void record(uintptr_t caller,const skilltelemetry::Target* entry=nullptr,bool transfer=false) const {
    for(int n=0;n<used;++n) {
        if(!live())return;
        const auto& c=cells[n];auto target=entry?*entry:unitTarget(unit);target.equipment=c.costs.weapon;
        target.x=(int16_t)c.position;target.y=(int16_t)(c.position>>16);target.terrain=c.terrain;
        char extra[96]={};if(entry)sprintf_s(extra,",\"entered_base\":%d,\"member_location_after\":%d",entry->base,transfer?*(const int*)((const uint8_t*)actor+0x98):entry->base);
        char context[480];sprintf_s(context,"{\"from\":%u,\"to\":%u,\"route_cells\":%u,\"base_cost_without_5\":%d,\"base_cost_with_5\":%d,\"result_scope\":\"%s\",\"final_step_cost_evaluated\":false,\"monsoon_trade_fire_shore_adjustments_evaluated\":false%s}",
            snapshot[1],to,snapshot[43],c.costs.normal,c.costs.steering,entry?(transfer?"native_base_entry_transfer_committed":"base_entry_member_location_committed"):"committed_route_base_terrain_input",extra);target.context=context;
        skilltelemetry::appliedEvent(actor,5,5,"movement",entry?"steering_water_cell_entered_base":"steering_water_cell_committed",caller,target);
    }
    }
};
void __fastcall steeringMove(void* controller,void*,int id,uint32_t to,int flag,const uint32_t* command) {
    ZocRouteScope zoc(id,to,flag,command);SteeringRoute route(id,to,flag,command);
    g_steeringMoveOriginal(controller,id,to,flag);zoc.finish();
    if(route.live() && *(uint32_t*)(route.unit+0x3c)==to)route.record(0x584ad1);
}
__declspec(naked) void steeringMoveBridge(){__asm {
    push esi
    push dword ptr [esp+16]
    push dword ptr [esp+16]
    push dword ptr [esp+16]
    call steeringMove
    ret 12}}

#include "skill_steering_entry_telemetry.inc"

struct SteeringLayout {uint32_t start;int size;const char* bytes;};
const SteeringLayout g_steeringLayouts[]={
    {0x4bf636,24,"\x57\xb9\x58\x19\x20\x07\xe8\x7f\x21\xfd\xff\x50\x8b\xcb\xe8\x17\x0f\xff\xff\x57\xb9\xd8\x59\x6a"},
    {0x584aab,16,"\x8b\x4e\x08\x8b\x16\x51\x52\xe8\x39\xab\x01\x00\x83\xc4\x08\x33"},
    {0x59f76a,14,"\x56\x53\xb9\x5c\x89\x99\x07\xe8\x7a\xfa\xf1\xff\x85\xc0"},
    {0x4bf612,13,"\x8b\x44\x24\x24\x50\x55\x8b\xcb\xe8\xc1\x3b\xfe\xff"},
    {0x4a3270,8,"\x85\xff\x89\xae\x98\x00\x00\x00"},
    {0x584abe,19,"\x8b\x46\x08\x8b\x0e\x6a\x01\x50\x51\xb9\xb0\x99\x54\x09\xe8\x4f\xa6\xfe\xff"},
    {0x495490,15,"\x83\x79\x08\x01\xb8\x09\x00\x00\x00\x74\x03\x8b\x41\x50\xc3"},
    {0x5ae500,35,"\x0f\xbf\x44\x24\x0c\x8b\x4c\x24\x0c\x69\xc0\xc8\x00\x00\x00\xc1\xf9\x10\x03\xc1\x56\x8b\x74\x24\x08\x8d\x04\x80\x8d\x04\x85\x68\x0e\xfb\x06"},
};
struct SteeringMapImmediate {uint32_t at;int kind;};
const SteeringMapImmediate g_steeringMapImmediates[]={
    {0x5ae50b,0}, // 005ae509 2 4 69c0c8000000 W
    {0x5ae51f,1}, // 005ae51c 3 4 8d0485680efb06 B20
};
bool verifySteeringLayout(){
    for(const auto& s:g_steeringLayouts){auto p=(const uint8_t*)(g_strategyCodeBase+s.start-0x400000);
        for(int n=0;n<s.size;++n){bool operand=false;
            for(const auto& m:g_steeringMapImmediates)if(s.start+n>=m.at && s.start+n<m.at+4)operand=true;
            if(!operand && p[n]!=(uint8_t)s.bytes[n])return false;
        }
    }
    const uint8_t* hazard=(const uint8_t*)(g_strategyCodeBase+0x5ae500-0x400000);
    const uint8_t* map=(const uint8_t*)(g_strategyCodeBase+0x5a0770-0x400000);
    return !memcmp(hazard+9,"\x69\xc0",2) && !memcmp(hazard+0x1c,"\x8d\x04\x85",3) &&
        *(const uint32_t*)(hazard+11)==*(const uint32_t*)(map+0x1f) &&
        *(const uint32_t*)(hazard+0x1f)==*(const uint32_t*)(map+0x3a);
}
