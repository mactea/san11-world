static int __fastcall safetyEnemy(uint32_t,void*,int a,int b){return a!=b;}
// Invoked inside the existing native transaction fixture, with real reservation
// and rollback logic and fixture-backed native resources.
static void safetyCases(Caravan& car) {
    DangerCells edges;edges.insert(pack(-1,2));edges.insert(pack(g_s.H,0));edges.insert(pack(0,g_s.W));
    edges.insert(pack(g_s.H-1,g_s.W-1));
    check(edges.count(pack(g_s.H-1,g_s.W-1)) && !edges.count(pack(0,0)),"danger bit grid bounds and final partial word are safe");
    auto oldCfg=g_cfg;auto oldDanger=g_routeDanger;auto oldOn=g_on;auto oldHostile=routeHostile;
    auto u=liveUnit(car);auto home=merchantTestBases;
    check(provisionFood(500,100,3000)==6450 && provisionFood(500,10,3000)==3000,
          "long trips carry round-trip allowance and short trips retain minimum provisions");
    check(provisionFood(60000,100,3000)<0 && provisionFood(500,-1,3000)<0,
          "over-capacity or unknown voyages cannot silently truncate provisions");
    std::vector<uint32_t> path(1000,pack(2,2));
    *(int*)(u+0x20)=50;car.port=0;car.state=DOCKED;
    int cityBefore=baseFood(home);auto result=supplyForRoute(car,u,path,1);
    check(result.empty() && *(int*)(u+0x20)>3000 && cityBefore-baseFood(home)==*(int*)(u+0x20)-50,
          "continuation loads actual long-route deficit from owned city exactly once");
    cityBefore=baseFood(home);int loaded=*(int*)(u+0x20);
    check(supplyForRoute(car,u,path,1).empty() && baseFood(home)==cityBefore && *(int*)(u+0x20)==loaded,
          "repeated continuation preflight never duplicates supplies");
    auto plans=g_autoPlans;auto automatic=car.automatic;
    tradeauto::Plan delegated;delegated.force=car.force;delegated.origin=car.origin;delegated.delegated=true;
    g_autoPlans={delegated};car.automatic.caravan=car.id;*(int*)(u+0x20)=50;*(int*)(home+0xac)=9000;
    check(!supplyForRoute(car,u,path,1).empty() && baseFood(home)==9000,
          "delegated resupply preserves the city's existing 9000-food military reserve");
    g_autoPlans=plans;car.automatic=automatic;*(int*)(home+0xac)=cityBefore;
    car.port=1;*(int*)(merchantTestBases+0x200+0x90)=1;*(int*)(u+0x20)=50;
    int foreignBefore=baseFood(merchantTestBases+0x200);
    check(!supplyForRoute(car,u,path,0).empty() && *(int*)(u+0x20)==50 && baseFood(merchantTestBases+0x200)==foreignBefore,
          "foreign shortage blocks return without stealing or teleporting city provisions");
    car.port=0;*(int*)(home+0xac)=0;
    check(!supplyForRoute(car,u,path,1).empty() && *(int*)(u+0x20)==50 && baseFood(home)==0,
          "owned market with insufficient food blocks without partial resource changes");
    *(int*)(home+0xac)=cityBefore;*(int*)(u+0x20)=1000;
    std::vector<uint32_t> shortRoute={pack(2,2),pack(2,3)};car.port=1;
    check(supplyForRoute(car,u,shortRoute,car.origin).empty(),
          "short return accepts sufficient remaining food below the formation minimum");
    g_routeDanger.resize(g_s.U);g_on=true;routeHostile=(HostileFn)safetyEnemy;
    auto enemy=merchantTestUnits+10*0xf4;memcpy(enemy,u,0xf4);
    *(int*)(enemy+8)=0;*(int*)(enemy+0x90)=1;*(int*)(enemy+0xb0)=5000;
    *(uint32_t*)(enemy+0xe0)=pack(10,10);
    refreshRouteDanger(u);
    check(!safeTradeStep(u,cell(10,11)) && !safeTradeStep(u,cell(11,10)) && safeTradeStep(u,cell(19,19)),
          "enemy formation blocks nearby cells while safe detours remain reachable");
    check(!safeTradeStep(u,cell(10,10)) && safeTradeStep(enemy,cell(10,10)),
          "enemy base/unit zones constrain registered caravans only");
    *(int*)(enemy+0x90)=0;*(int*)(merchantTestBases+0x200+0x90)=0;
    refreshRouteDanger(u);
    check(safeTradeStep(u,cell(10,10)),"diplomacy or enemy movement refresh removes obsolete danger cells");
    g_cfg=oldCfg;g_routeDanger=oldDanger;g_on=oldOn;routeHostile=oldHostile;
}

