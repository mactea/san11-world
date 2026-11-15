// Follow the native move flags and control-zone stop test to an actual route
// position commit. Only intermediate empty cells are attributable here: ending
// at a controlled cell is legal without bypassing its expansion stop.
typedef void(__cdecl* ZocFlags)(uint32_t*,void*);
typedef int(__thiscall* ZocTest)(void*,uint32_t);
typedef void(__thiscall* ZocNeighbor)(void*,uint32_t*,uint32_t,int);
typedef int(__thiscall* ZocRelation)(void*,void*);
ZocFlags g_zocFlagsOriginal=(ZocFlags)0x5a3680;
ZocTest g_zocTestOriginal=(ZocTest)0x56e0e0;
ZocNeighbor g_zocNeighborOriginal=(ZocNeighbor)0x483a50;
ZocRelation g_zocRelationOriginal=(ZocRelation)0x495c50;
AttackReachHandleUnit g_zocHandleUnit=(AttackReachHandleUnit)0x56dba0;
ReachObject g_zocObjectFromCell=(ReachObject)0x483b20;
struct ZocEnemyProbe;
thread_local ZocEnemyProbe* g_zocEnemyProbe=nullptr;
struct ZocEnemyProbe {
    void* source;
    void* enemy=nullptr;
    bool base=false;
    ZocEnemyProbe* previous;
    explicit ZocEnemyProbe(void* s):source(s),previous(g_zocEnemyProbe){g_zocEnemyProbe=this;}
    ~ZocEnemyProbe(){g_zocEnemyProbe=previous;}
};
int zocRelationObserved(void* source,void* enemy,bool base){
    int result=g_zocRelationOriginal(source,enemy);
    if(result && g_zocEnemyProbe && g_zocEnemyProbe->source==source){g_zocEnemyProbe->enemy=enemy;g_zocEnemyProbe->base=base;}
    return result;
}
int __fastcall zocUnitRelation(void* source,void*,void* enemy){return zocRelationObserved(source,enemy,false);}
int __fastcall zocBaseRelation(void* source,void*,void* enemy){return zocRelationObserved(source,enemy,true);}
const uint8_t* zocCell(uint32_t position){
    if(movementTerrain(position)<0)return nullptr;
    int width=*(const int*)((const uint8_t*)g_movementMapOperand-0x1b);
    auto map=(const uint8_t*)(uintptr_t)*g_movementMapOperand;
    return map+20*((size_t)(int16_t)position*width+(int16_t)(position>>16));
}
bool zocSafeNeighbors(uint32_t position){
    auto map=(void*)(uintptr_t)*g_movementMapOperand;
    for(int n=0;n<6;++n){uint32_t p;g_zocNeighborOriginal(map,&p,position,n);auto cell=zocCell(p);if(!cell)continue;
        int kind=*(const uint32_t*)cell&3;
        // Object IDs0..fffe and world building IDs are different namespaces.
        // The guarded inverse accessor only does bounded address arithmetic;
        // reject noncanonical proxy pointers before any object virtual call.
        if(kind==2){if(*(const uint16_t*)(cell+8)==0xffff)return false;
            auto base=g_zocObjectFromCell((void*)cell);if(!base)return false;
            int id=g_telemetryBaseId((void*)0x7201958,base);
            if(id<0 || g_baseById((void*)0x7201958,id)!=base || !g_growthValid(base))return false;}
        if(kind==1){int id=*(const uint16_t*)(cell+8);auto unit=g_movementUnitById((void*)0x7201958,id);
            if(!unit || !g_growthValid(unit))return false;}
    }
    return true;
}
struct ZocRouteScope {
    struct Cell {
        uint32_t position,enemyPosition,basePosition;
        const void* actor;void* enemy;
        int actorId,owned,effect,terrain,enemyId,enemyForce,enemyStatus,stepsAfter,objectId;
        bool base,written=false;
    };
    Cell cells[40];int count=0,id,force=-1,type=-1,landWeapon=-1;
    void* unit=nullptr;const uint32_t* command;uint32_t snapshot[44];uint64_t generation;
    ZocRouteScope(int sourceId,uint32_t to,int flag,const uint32_t* c):id(sourceId),command(c),generation(skilltelemetry::contextGeneration()){
        if(!skilltelemetry::enabled() || flag!=1 || !c || id<0)return;
        unit=g_movementUnitById((void*)0x7201958,id);
        if(!unit || g_telemetryUnitId((void*)0x7201958,unit)!=id)return;
        memcpy(snapshot,c,sizeof snapshot);
        if(snapshot[0]!=(uint32_t)id || snapshot[2]!=to || snapshot[1]==to || snapshot[43]<2 || snapshot[43]>40 ||
           snapshot[2+snapshot[43]]!=to || *(const uint32_t*)((const uint8_t*)unit+0x3c)!=snapshot[1])return;
        bool candidate=false;
        for(int n=0;n<3;++n){int member=*(const int*)((const uint8_t*)unit+0xc+4*n);auto p=member>=0?g_telemetryPerson((void*)0x7201958,member):nullptr;
            if(p){int owned=*(const int*)(p+0xe8);if(contains(owned,0) || contains(owned,1) || contains(owned,4))candidate=true;}}
        if(!candidate || !g_growthValid(unit))return;
        uint32_t previous=snapshot[1];
        for(unsigned n=0;n<snapshot[43];++n){uint32_t p=snapshot[3+n];
            if(!zocCell(p) || reachDistance(previous,p)!=1)return;previous=p;}
        auto views=(uint8_t*)(uintptr_t)*(const uint32_t*)(g_strategyCodeBase+0x5a5130-0x400000);
        if(!views)return;
        void* view=views+16*(size_t)id;
        if(g_zocHandleUnit(view)!=unit)return;
        force=unitTarget(unit).force;type=*(const int*)((const uint8_t*)unit+8);landWeapon=*(const int*)((const uint8_t*)unit+0x48);
        uint32_t flags[4];const void* actors[5];
        {ZocFlagProbe probe;g_zocFlagsOriginal(flags,unit);memcpy(actors,probe.actors,sizeof actors);}
        for(unsigned n=0;n+1<snapshot[43];++n){uint32_t p=snapshot[3+n];auto cell=zocCell(p);
            if((*(const uint32_t*)cell&3)!=0)continue;
            int terrain=movementTerrain(p);bool water=terrain==7 || terrain==8;
            int effect=water?4:actors[0]?0:1;
            auto actor=actors[effect];if(!actor || flags[water?1:0]!=0 || !zocSafeNeighbors(p))continue;
            ZocEnemyProbe probe(unit);if(!g_zocTestOriginal(view,p) || !probe.enemy)continue;
            int enemyId=(probe.base?g_telemetryBaseId:g_telemetryUnitId)((void*)0x7201958,probe.enemy);
            if(enemyId<0 || (probe.base?g_baseById((void*)0x7201958,enemyId):g_movementUnitById((void*)0x7201958,enemyId))!=probe.enemy || !g_growthValid(probe.enemy))continue;
            typedef const uint32_t*(__thiscall* Position)(void*);
            uint32_t basePosition=*(*(Position*)(*(uint8_t**)probe.enemy+0x3c))(probe.enemy),enemyPosition=basePosition;int objectId=-1;
            if(probe.base){for(int direction=0;direction<6;++direction){uint32_t neighbor;
                    g_zocNeighborOriginal((void*)(uintptr_t)*g_movementMapOperand,&neighbor,p,direction);auto blocker=zocCell(neighbor);
                    if(blocker && (*(const uint32_t*)blocker&3)==2 && *(const uint16_t*)(blocker+8)!=0xffff && g_zocObjectFromCell((void*)blocker)==probe.enemy){
                        enemyPosition=neighbor;objectId=*(const uint16_t*)(blocker+8);break;}}
                if(objectId<0)continue;}
            int actorId=growthPersonId((void*)actor);
            if(actorId<0 || reachDistance(p,enemyPosition)!=1 || !afterAttackMember(unit,actor))continue;
            cells[count++]={p,enemyPosition,basePosition,actor,probe.enemy,actorId,*(const int*)((const uint8_t*)actor+0xe8),effect,terrain,
                enemyId,(*(BuildingValue*)(*(uint8_t**)probe.enemy+0x40))(probe.enemy),*(const int*)((const uint8_t*)probe.enemy+(probe.base?0x14:0x24)),(int)snapshot[43]-(int)n-1,objectId,probe.base};
        }
    }
    bool live(bool entry=false) const {
        if(!count || !skilltelemetry::enabled() || generation!=skilltelemetry::contextGeneration() || memcmp(snapshot,command,sizeof snapshot) ||
           g_movementUnitById((void*)0x7201958,id)!=unit || !g_growthValid(unit) || unitTarget(unit).force!=force ||
           *(const int*)((const uint8_t*)unit+8)!=type || *(const int*)((const uint8_t*)unit+0x48)!=landWeapon ||
           *(const uint32_t*)((const uint8_t*)unit+0x3c)!=snapshot[entry?1:2])return false;
        return true;
    }
    bool pending(const void* person) const {
        for(int n=0;n<count;++n)if(!cells[n].written && cells[n].actor==person)return true;
        return false;
    }
    void finish(const skilltelemetry::Target* entry=nullptr,const void* person=nullptr,bool transfer=false){
        if(!live(entry!=nullptr))return;
        for(int n=0;n<count;++n){auto& c=cells[n];
            if(!live(entry!=nullptr))return;
            if(entry && (c.written || (person && c.actor!=person)))continue;
            if(growthPersonId((void*)c.actor)!=c.actorId || *(const int*)((const uint8_t*)c.actor+0xe8)!=c.owned ||
               !contains(c.owned,c.effect) || !afterAttackMember(unit,c.actor) ||
               (c.base?g_baseById((void*)0x7201958,c.enemyId):g_movementUnitById((void*)0x7201958,c.enemyId))!=c.enemy || !g_growthValid(c.enemy) ||
               (*(BuildingValue*)(*(uint8_t**)c.enemy+0x40))(c.enemy)!=c.enemyForce ||
               (c.base?*(const int*)((const uint8_t*)c.enemy+0x14)==0:*(const int*)((const uint8_t*)c.enemy+0x24)!=c.enemyStatus))continue;
            typedef const uint32_t*(__thiscall* Position)(void*);
            if(*(*(Position*)(*(uint8_t**)c.enemy+0x3c))(c.enemy)!=(c.base?c.basePosition:c.enemyPosition))continue;
            if(c.base){auto blocker=zocCell(c.enemyPosition);
                if(!blocker || (*(const uint32_t*)blocker&3)!=2 || *(const uint16_t*)(blocker+8)!=c.objectId || g_zocObjectFromCell((void*)blocker)!=c.enemy)continue;}
            auto target=entry?*entry:unitTarget(unit);target.equipment=*(const int*)((const uint8_t*)unit+0xa8);
            if(entry)target.person=c.actorId;
            target.x=(int16_t)c.position;target.y=(int16_t)(c.position>>16);target.terrain=c.terrain;
            char extra[96]={};if(c.base)sprintf_s(extra,",\"blocker_object\":%d,\"blocker_base_position\":%u",c.objectId,c.basePosition);
            char entered[96]={};if(entry)sprintf_s(entered,",\"entered_base\":%d,\"member_location_after\":%d",entry->base,transfer?*(const int*)((const uint8_t*)c.actor+0x98):entry->base);
            char context[544];sprintf_s(context,"{\"from\":%u,\"to\":%u,\"route_cells\":%u,\"steps_after_cell\":%d,\"%s\":%d,\"blocker_force\":%d,\"blocker_position\":%u%s,\"result_scope\":\"%s\",\"movement_point_saving_evaluated\":false,\"counterfactual_path_evaluated\":false%s}",
                snapshot[1],snapshot[2],snapshot[43],c.stepsAfter,c.base?"blocker_base":"blocker_unit",c.enemyId,c.enemyForce,c.enemyPosition,extra,entry?(transfer?"native_base_entry_transfer_continued_past_native_zoc_stop":"base_entry_route_continued_past_native_zoc_stop"):"committed_route_continued_past_native_zoc_stop",entered);target.context=context;
            skilltelemetry::appliedEvent(c.actor,c.owned,c.effect,"movement",entry?"zoc_bypass_route_cell_entered_base":"zoc_bypass_route_cell_committed",entry?(transfer?0x4bf636:0x4bf61f):0x584ad1,target);
            if(entry)c.written=true;
        }
    }
};

struct ZocLayout {uint32_t start;int size;const char* bytes;};
const ZocLayout g_zocLayouts[]={
    {0x491770,76,"\x56\x8b\x74\x24\x08\x85\xf6\x57\x74\x3a\x8d\xb9\x30\x97\x08\x00\x85\xff\x74\x30\x53\xe8\x46\x7e\xf8\xff\x8b\xd8\xe8\x3f\x7e\xf8\xff\x3b\xd8\x5b\x75\x1e\x2b\xf7\xb8\x93\x24\x49\x92\xf7\xee\x03\xd6\xc1\xfa\x05\x8b\xc2\xc1\xe8\x1f\x03\xc2\x78\x07\x3d\xff\x3f\x00\x00\x7e\x03\x83\xc8\xff\x5f\x5e\xc2\x04\x00"},
    {0x4195d0,4,"\x83\xc8\xff\xc3"},
    {0x483b20,25,"\x8b\x01\x24\x03\x3c\x02\x74\x03\x33\xc0\xc3\x0f\xb7\x49\x08\xc1\xe1\x07\x8b\x81\xb4\x83\x64\x04\xc3"},
    {0x490d00,33,"\x8b\x44\x24\x04\x85\xc0\x7c\x14\x3d\xff\x3f\x00\x00\x7f\x0d\x6b\xc0\x38\x8d\x84\x08\x30\x97\x08\x00\xc2\x04\x00\x33\xc0\xc2\x04\x00"},
    {0x5a3680,167,"\x56\x8b\x74\x24\x0c\x57\x8b\x7c\x24\x0c\x33\xc0\x8b\xcf\x89\x01\x89\x41\x04\x89\x41\x08\x89\x41\x0c\x8b\xce\xc7\x07\x01\x00\x00\x00\xc7\x47\x04\x01\x00\x00\x00\xe8\xd3\x1d\xef\xff\x50\xe8\xcd\xb5\xed\xff\x83\xc4\x04\x83\xf8\x04\x74\x20\x8b\x16\x6a\x00\x8b\xce\xff\x52\x58\x85\xc0\x75\x0d\x8b\x06\x6a\x01\x8b\xce\xff\x50\x58\x85\xc0\x74\x06\xc7\x07\x00\x00\x00\x00\x8b\x16\x6a\x04\x8b\xce\xff\x52\x58\x85\xc0\x74\x07\xc7\x47\x04\x00\x00\x00\x00\x8b\x06\x6a\x11\x8b\xce\xff\x50\x60\x85\xc0\x75\x0d\x8b\x16\x6a\x06\x8b\xce\xff\x52\x58\x85\xc0\x74\x07\xc7\x47\x0c\x01\x00\x00\x00\x8b\x06\x6a\x08\x8b\xce\xff\x50\x58\x85\xc0\x74\x07\xc7\x47\x08\x01\x00\x00\x00\x5f\x5e\xc3"},
    {0x56e0e0,220,"\x53\x56\x57\x8b\x7c\x24\x10\x8b\xd9\x33\xf6\xeb\x03\x8d\x49\x00\x56\x57\x8d\x44\x24\x18\x50\xb9\x68\x0e\xfb\x06\xe8\x4f\x59\xf1\xff\x0f\xbf\x44\x24\x10\x85\xc0\x0f\xbf\x4c\x24\x12\x0f\x8c\x8c\x00\x00\x00\x3d\xc8\x00\x00\x00\x0f\x8d\x81\x00\x00\x00\x85\xc9\x7c\x7d\x81\xf9\xc8\x00\x00\x00\x7d\x75\x69\xc0\xc8\x00\x00\x00\x03\xc1\x8d\x0c\x80\x8b\x04\x8d\x68\x0e\xfb\x06\x8d\x0c\x8d\x68\x0e\xfb\x06\x83\xe0\x03\x48\x74\x27\x48\x75\x53\xe8\xcf\x59\xf1\xff\x8b\x48\x14\x85\xc9\x74\x47\x0f\xb7\x4b\x08\x69\xc9\xf4\x00\x00\x00\x50\x81\xc1\x88\xb0\x36\x07\xe8\xe2\x7a\xf2\xff\xeb\x2b\x0f\xb7\x41\x08\x50\xb9\x58\x19\x20\x07\xe8\xf1\x2c\xf2\xff\x83\x78\x24\x01\x74\x1a\x0f\xb7\x4b\x08\x69\xc9\xf4\x00\x00\x00\x50\x81\xc1\x88\xb0\x36\x07\xe8\xb5\x7a\xf2\xff\x85\xc0\x75\x12\x46\x83\xfe\x06\x0f\x8c\x47\xff\xff\xff\x5f\x5e\x33\xc0\x5b\xc2\x04\x00\x5f\x5e\xb8\x01\x00\x00\x00\x5b\xc2\x04\x00"},
    {0x5a470e,75,"\x8b\x46\x04\x83\xe0\x1f\x83\xf8\x07\x66\x89\x6e\x0a\x74\x05\x83\xf8\x08\x75\x28\x8b\x47\x04\x85\xc0\x74\x30\x8b\x4c\x24\x1c\x53\xe8\xad\x99\xfc\xff\x85\xc0\x74\x22\x8b\x06\x5d\x0d\x00\x00\x02\x00\x89\x06\x5e\x5b\x5f\x83\xc4\x08\xc2\x18\x00\x8b\xce\xe8\xdf\xee\xff\xff\x85\xc0\x75\x04\x8b\x07\xeb\xcc"},
};
struct ZocImmediate {uint32_t at;int kind;};
const ZocImmediate g_zocImmediates[]={
    {0x56e0f8,2}, // 0056e0f7 1 4 b9680efb06 B20
    {0x56e114,1}, // 0056e113 1 4 3dc8000000 H
    {0x56e124,0}, // 0056e122 2 4 81f9c8000000 W
    {0x56e12c,0}, // 0056e12a 2 4 69c0c8000000 W
    {0x56e138,2}, // 0056e135 3 4 8b048d680efb06 B20
    {0x56e13f,2}, // 0056e13c 3 4 8d0c8d680efb06 B20
    {0x56e165,3}, // 0056e163 2 4 81c188b03607 UNITARR
    {0x56e192,3}, // 0056e190 2 4 81c188b03607 UNITARR
};
bool verifyZocNeighborGuard(uint32_t address,const char* original,int size,bool second){
    auto at=(const uint8_t*)(g_strategyCodeBase+address-0x400000);
    if(!memcmp(at,original,size))return true;
    if(at[0]!=0xe9)return false;
    for(int n=5;n<size;++n)if(at[n]!=0x90)return false;
    auto cave=(const uint8_t*)((uintptr_t)at+5+*(const int32_t*)(at+1));
    size_t length=second?23:24;MEMORY_BASIC_INFORMATION memory={};
    if(!VirtualQuery(cave,&memory,sizeof memory) || memory.State!=MEM_COMMIT ||
       (memory.Protect&(PAGE_NOACCESS|PAGE_GUARD)) || memory.RegionSize<length ||
       (uintptr_t)cave-(uintptr_t)memory.BaseAddress>memory.RegionSize-length)return false;
    int branch=second?12:13,jump=second?18:19;
    return !memcmp(cave,"\x85\xc0\x0f\x84",4) &&
        !memcmp(cave+8,second?"\x83\x78\x24\x01":"\x8b\x48\x14\x85\xc9",second?4:5) &&
        !memcmp(cave+branch,"\x0f\x84",2) && cave[jump]==0xe9 &&
        (uintptr_t)cave+8+*(const int32_t*)(cave+4)==g_strategyCodeBase+0x16e19f &&
        (uintptr_t)cave+branch+6+*(const int32_t*)(cave+branch+2)==g_strategyCodeBase+0x16e19f &&
        (uintptr_t)cave+jump+5+*(const int32_t*)(cave+jump+1)==(uintptr_t)at+size;
}
bool verifyZocLayout(){
    for(const auto& s:g_zocLayouts){auto p=(const uint8_t*)(g_strategyCodeBase+s.start-0x400000);
        for(int n=0;n<s.size;++n){bool operand=false;
            if((s.start+n>=0x56e151 && s.start+n<0x56e158) ||
               (s.start+n>=0x56e17f && s.start+n<0x56e185))continue;
            for(const auto& m:g_zocImmediates)if(s.start+n>=m.at && s.start+n<m.at+4)operand=true;
            if(!operand && p[n]!=(uint8_t)s.bytes[n])return false;
        }
    }
    if(!verifyZocNeighborGuard(0x56e151,"\x8b\x48\x14\x85\xc9\x74\x47",7,false) ||
       !verifyZocNeighborGuard(0x56e17f,"\x83\x78\x24\x01\x74\x1a",6,true)){
        if(g_log)g_log("skills: unexpected movement ZOC neighbor guard; rejected");return false;
    }
    uint32_t width=*(const uint32_t*)(g_strategyCodeBase+0x5a078f-0x400000),height=*(const uint32_t*)(g_strategyCodeBase+0x5a0783-0x400000);
    uint32_t base=*(const uint32_t*)(g_strategyCodeBase+0x5a07aa-0x400000),units=*(const uint32_t*)(g_strategyCodeBase+0x56dbab-0x400000);
    if(!units)return false;
    for(const auto& m:g_zocImmediates)if(*(const uint32_t*)(g_strategyCodeBase+m.at-0x400000)!=(m.kind==0?width:m.kind==1?height:m.kind==2?base:units))return false;
    return true;
}
