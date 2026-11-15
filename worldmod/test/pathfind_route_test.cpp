// Exercise the actual A* adapter; replace only its native function pointer.
#include "../src/pathfind.cpp"
#include "../src/persons.h"
int movementSeaMinPct() { return 100; }
static const uint8_t* tradeUnit=nullptr;
bool movementIsTradeUnit(const uint8_t* unit) { return unit==tradeUnit && unit; }
bool movementCanStep(const uint8_t*,const uint8_t*) { return true; }
const uint8_t* movementSteerCosts() { return nullptr; }
void movementSetStepFrom(uint32_t) {}
uint8_t* personPtr(int) { return nullptr; }
static Node nodes[2];
static void* expectedView;
static Node* __cdecl nativeAstar(void* view, uint32_t start, const uint32_t* targets, int count) {
    if (view != expectedView || *(uint16_t*)((uint8_t*)view+8) != 45 || count != 1) return nullptr;
    nodes[0] = {}; nodes[1] = {};
    nodes[0].coord=targets[0]; nodes[1].coord=start; nodes[1].parent=&nodes[0]; nodes[1].g=27;
    return &nodes[1];
}
static int failures;
static void check(bool ok, const char* name) { printf("%s %s\n",ok?"PASS":"FAIL",name); failures+=!ok; }
int main() {
    Units units;
    if (!unitsSetup(1000,units,nullptr)) return 2;
    uint8_t* unit=units.UNITARR+45*0xf4;
    tradeUnit=unit;
    check(unitHeuristic(unit,4)==1 && unitHeuristic(unit,2)==1 && unitHeuristic(unit,0)==0,
          "one-point caravan steps have an admissible heuristic in original and optimized modes");
    check(unitHeuristic(units.UNITARR,4)==4,"ordinary units keep their existing search heuristic");
    *(int*)(unit+8)=1; // transport type, emphatically NOT unit id 45
    expectedView=units.UWRAP+45*16; *(uint16_t*)((uint8_t*)expectedView+8)=45;
    g_routeAstar=nativeAstar;
    g_cfg.hex28=(uint8_t*)nodes; g_cfg.hex28Size=sizeof nodes;
    std::vector<uint32_t> path; int cost=0;
    check(pathfindRoute(unit,100,{200},&path,&cost) && path==std::vector<uint32_t>({100,200}) && cost==27,
          "transport record is converted to its own native view, preserving path and cost");
    check(!pathfindRoute(unit+1,100,{200},&path,&cost) && path.empty() && cost==0,
          "misaligned record is rejected before entering native A*");
    check(!unitsView(units.UNITARR+1000*0xf4),"out-of-range unit is rejected");
    *(uint16_t*)((uint8_t*)expectedView+8)=1;
    check(!pathfindRoute(unit,100,{200},&path,&cost),"uninitialized or mismatched view is rejected");
    printf("pathfind_route_test: %d failures\n",failures); return failures?1:0;
}
