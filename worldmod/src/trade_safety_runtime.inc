// Immutable snapshots are published per unit; the movement callback never
// walks game objects or takes the trade lock while A* is running.
using HostileFn=int(__thiscall*)(uint32_t,int,int);
HostileFn routeHostile=(HostileFn)0x4b5cc0;
void addDanger(DangerCells& cells,int x,int y,int radius) {
    for(int lo=x-radius;lo<=x+radius;++lo)for(int hi=y-radius;hi<=y+radius;++hi)
        if(lo>=0 && hi>=0 && lo<g_s.H && hi<g_s.W && hexDist(x,y,lo,hi)<=radius)cells.insert(pack(lo,hi));
}
// relax: up to two {lo,hi,radius} areas (the unit's own position and the destination's berths) cleared of
// danger. Used only after the fully safe search failed, so a caravan next to its market is not held forever.
void refreshRouteDanger(uint8_t* unit,const int* relax=nullptr) {
    if(!g_on || !tradeMovingUnit(unit) || g_routeDanger.size()!=(size_t)g_s.U)return;
    int force=vcall(unit,0x40);if(force<0 || force>=g_s.R)return;
    std::vector<uint8_t> hostile(g_s.R+5,0);
    for(int f=0;f<(int)hostile.size();++f)if(f!=force)hostile[f]=routeHostile(PROC,force,f)!=0;
    auto cells=std::make_shared<DangerCells>();
    for(int i=0;i<g_s.U;++i) {
        auto enemy=unitRec(WORLD,i);if(!objValid(enemy) || *(int*)(enemy+8)==1)continue;
        int f=vcall(enemy,0x40);if(f<0 || f>=(int)hostile.size() || !hostile[f] || vcall(enemy,0x50)<=0)continue;
        auto p=posOf(enemy);addDanger(*cells,p[0],p[1],4);
    }
    // Cities, gates and ports have native defensive fire; include expanded bases.
    for(int i=0;i<0x4000;++i) {
        auto enemy=baseRec(WORLD,i);if(!objValid(enemy) || *(int*)(enemy+8)>2)continue;
        int f=vcall(enemy,0x40);if(f<0 || f>=(int)hostile.size() || !hostile[f])continue;
        auto p=posOf(enemy);addDanger(*cells,p[0],p[1],3);
    }
    if(relax)for(int i=0;i<2;++i) {
        int x=relax[i*3],y=relax[i*3+1],radius=relax[i*3+2];
        for(int lo=x-radius;lo<=x+radius;++lo)for(int hi=y-radius;hi<=y+radius;++hi)
            if(hexDist(x,y,lo,hi)<=radius)cells->erase(pack(lo,hi));
    }
    size_t id=((uintptr_t)unit-(uintptr_t)g_unit0)/0xf4;
    std::shared_ptr<const DangerCells> snapshot=cells;
    std::atomic_store(&g_routeDanger[id],snapshot);
}
bool safeTradeStep(const uint8_t* unit,const uint8_t* hex) {
    if(!tradeMovingUnit(unit) || !hex || !g_s.hex20 || hex<g_s.hex20 ||
       hex>=g_s.hex20+(size_t)g_s.W*g_s.H*20)return true;
    size_t id=((uintptr_t)unit-(uintptr_t)g_unit0)/0xf4;
    if(id>=g_routeDanger.size())return true;
    auto snapshot=std::atomic_load(&g_routeDanger[id]);if(!snapshot)return true;
    size_t at=(hex-g_s.hex20)/20;
    return !snapshot->count(pack((int)(at/g_s.W),(int)(at%g_s.W)));
}
int geographicFood(int origin,int dest,int ship,int troops) {
    int speed=ship==MERCHANT_SHIP?28:15;
    int turns=(hexDist(g_data.lo[origin],g_data.hi[origin],g_data.lo[dest],g_data.hi[dest])+speed-1)/speed;
    if(ship==MERCHANT_SHIP)turns+=(shipAccess(origin).days()+9)/10+(shipAccess(dest).days()+9)/10;
    return provisionFood(troops,turns*2,g_cfg.food);
}
int pathFood(uint8_t* unit,const std::vector<uint32_t>& path,bool roundTrip) {
    auto time=movementRouteTime(unit,path,g_s.hex20,g_s.W,g_s.H);
    int turns=time.turns>=0?time.turns:(int)((path.size()+4)/5);
    if(roundTrip) {
        std::vector<uint32_t> back(path.rbegin(),path.rend());
        auto reverse=movementRouteTime(unit,back,g_s.hex20,g_s.W,g_s.H);
        turns+=reverse.turns>=0?reverse.turns:(int)((path.size()+4)/5);
    }
    return provisionFood(vcall(unit,0x50),turns,0);
}
std::string supplyForRoute(Caravan& k,uint8_t* unit,const std::vector<uint32_t>& path,int dest,int extraTurns=0) {
    int required=pathFood(unit,path,dest!=k.origin);
    if(required<0)return "voyage provisions exceed transport capacity";
    int64_t total=(int64_t)required+((vcall(unit,0x50)+9)/10)*(int64_t)extraTurns;
    required=total>100000?-1:(int)total;
    if(required<0)return "voyage provisions exceed transport capacity";
    int food=*(int*)(unit+0x20);if(food>=required)return "";
    auto city=k.port>=0?base(k.port):nullptr;
    if(!city || baseForce(k.port)!=k.force)return "insufficient voyage provisions; resupply at an owned market before continuing";
    int reserve=0;
    if(k.automatic.caravan)for(auto& plan:g_autoPlans)
        if(plan.force==k.force && plan.origin==k.origin && plan.delegated){reserve=9000;break;}
    int need=required-food;if((int64_t)baseFood(city)<need+(int64_t)reserve)return "city has insufficient round-trip provisions";
    int taken=-addFood(PROC,city,-need);
    if(taken!=need){credit(k.force,k.port,0,0,taken);return "provision reservation failed";}
    unitFoodSet(unit,required);
    int loaded=*(int*)(unit+0x20)-food;
    if(loaded<need)credit(k.force,k.port,0,0,need-std::max(0,loaded));
    if(loaded!=need)return "transport could not load all voyage provisions";
    return "";
}
