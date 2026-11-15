// Exercise production step costs, range caps and route-time calculation without a game.
#include "../src/movement.cpp"
static int failures;
static void check(bool pass,const char* name) {printf("%s %s\n",pass?"PASS":"FAIL",name);failures+=!pass;}
static const uint8_t* caravan;
static bool __cdecl isTrade(const uint8_t* u) {return u==caravan;}
static bool __cdecl blocked(const uint8_t*) {return false;}
static bool __cdecl onlyWater(const uint8_t* u,const uint8_t* h) {
    int t=*(const uint32_t*)(h+4)&31;return u!=caravan || t==7 || t==8;
}
static int __cdecl tailwind(const uint8_t*,const uint8_t*,uint32_t,int c) {return (c+1)/2;}
static int __cdecl headwind(const uint8_t*,const uint8_t*,uint32_t,int c) {return (c*140+50)/100;}
int main() {
    uint8_t units[2*0xf4]={},table[12*32],alt[2]={36,36},afloat[2]={0,0},hex[1200*20]={};
    memset(table,6,sizeof table);table[10]=table[12]=3;table[9*32+7]=6;table[9*32+8]=5;
    g_table=(uint32_t)(uintptr_t)table;g_unit0=units;g_units=2;g_alt=alt;g_afloat=afloat;
    for(int n=0;n<2;++n) {units[n*0xf4+0xcc]=25;*(int*)(units+n*0xf4+8)=1;}
    caravan=units;movementSetTradeUnits(isTrade);
    auto step=[&](const uint8_t* u,int t,uint32_t flags=0) {return stepCost(u,t==7||t==8?9:0,t,flags,hex,nullptr);};
    check(step(units,10)==1 && step(units,0)==2 && step(units,2)==2,"caravan road and plain costs are reduced");
    check(step(units+0xf4,10)==3 && step(units+0xf4,0)==6,"ordinary transport is unchanged");
    check(step(units,5)==6 && step(units,3)==6 && step(units,13)==6,"forest, wetland and ford costs are unchanged");
    check(step(units,10,0x800000)==4,"fire still multiplies the caravan step cost");
    table[0]=255;check(step(units,0)==255,"impassable terrain never becomes passable");table[0]=6;
    check(rangeCap(units,false)==24 && rangeCap(units,true)==36 && rangeCap(units+0xf4,false)==10 && rangeCap(units+0xf4,true)==16,"only registered caravans receive extended land and sea ranges");
    movementSetAllowed(blocked);check(step(units,10)==255,"docked pause wins over acceleration");movementSetAllowed(nullptr);
    std::vector<uint32_t> path;for(int n=0;n<=60;++n) {path.push_back(n);*(uint32_t*)(hex+n*20+4)=10;}
    auto fast=movementRouteTime(units,path,hex,1,80),normal=movementRouteTime(units+0xf4,path,hex,1,80);
    check(fast.turns==3 && fast.steps==60 && fast.cost==60 && normal.turns==8,"route ETA packs actual road costs into turns and respects movement ranges");
    for(int n=0;n<=60;++n) *(uint32_t*)(hex+n*20+4)=0;
    fast=movementRouteTime(units,path,hex,1,80);normal=movementRouteTime(units+0xf4,path,hex,1,80);
    check(fast.turns==5 && normal.turns==15 && fast.land==60 && !fast.water,"plain route is three times faster with unchanged movement budget");
    path.resize(21);for(int n=0;n<=20;++n) *(uint32_t*)(hex+n*20+4)=8;
    units[0xcc]=36;alt[0]=25;afloat[0]=1;
    auto sea=movementRouteTime(units,path,hex,1,80);
    movementSetSeaCost(tailwind,50);auto wind=movementRouteTime(units,path,hex,1,80);
    check(sea.turns==1 && wind.turns==1 && wind.water==20,"merchant shipping discount applies to actual sea ETA");
    movementSetSeaCost(headwind,100);auto adverse=movementRouteTime(units,path,hex,1,80);
    check(adverse.turns==2 && adverse.cost==40 && sea.cost==20,"headwind remains slower after integer rounding");movementSetSeaCost(nullptr,100);
    check(step(units,8)==1 && step(units+0xf4,8)==4 && step(units,7)==6,"ordinary sea transport and merchant river costs retain their original shore scaling");
    check(step(units,8,0x800000)==4,"sea acceleration preserves fire penalty");
    path.clear();for(int n=0;n<=72;++n) {path.push_back(n);*(uint32_t*)(hex+n*20+4)=8;}
    auto longSea=movementRouteTime(units,path,hex,1,80);
    units[0xcc]=44;auto skilledSea=movementRouteTime(units,path,hex,1,80);units[0xcc]=36;
    check(longSea.turns==2 && longSea.water==72 && skilledSea.turns==2,"sea ETA crosses the old 24-hex cap and limits high-bonus ships to the reserved radius");
    path.resize(21);
    *(uint32_t*)(hex+4)=10;units[0xcc]=25;alt[0]=36;afloat[0]=0;
    auto mixed=movementRouteTime(units,path,hex,1,80);
    check(mixed.turns==1 && mixed.water==20,"boarding is charged using the same shore scaling as actual movement");
    table[9*32+8]=255;check(movementRouteTime(units,path,hex,1,80).turns==-1,"blocked route has unknown ETA rather than a fabricated arrival time");
    check(movementRouteTime(units,{0},hex,1,80).turns==0 && movementRouteTime(units,{0,99},hex,1,80).turns==-1,"arrival and out-of-map path boundaries");
    check(1+3*(TRADE_SEA_CAP+1)*(TRADE_SEA_CAP+2)<=RANGE_CAP && sizeof(g_pool)/0x14>=RANGE_CAP && sizeof(g_visited)/4>=RANGE_CAP,"extended sea movement plus the visited frontier fits both replacement buffers");
    bool bounded=true;
    for(int c=1;c<255;++c) {table[0]=(uint8_t)c;int fastCost=step(units,0);bounded &= fastCost>=1 && fastCost<=c && step(units+0xf4,0)==c;}
    check(bounded,"every passable configured terrain cost stays positive and ordinary units retain the original value");
    units[0xcc]=units[0xf4+0xcc]=36;afloat[0]=afloat[1]=1;alt[0]=alt[1]=25;
    bounded=true;
    for(int c=1;c<255;++c) {table[9*32+8]=(uint8_t)c;int fastCost=step(units,8);bounded &= fastCost>=1 && fastCost<=c && step(units+0xf4,8)==c && step(units,8,0x800000)>0;}
    check(bounded,"sea discount and fire never create zero-cost steps and every ordinary ship cost remains unchanged");
    units[0xcc]=units[0xf4+0xcc]=25;afloat[0]=afloat[1]=0;alt[0]=alt[1]=36;
    path.clear();table[0]=5;table[9*32+8]=5;
    for(int n=0;n<=1112;++n) {path.push_back(n);*(uint32_t*)(hex+n*20+4)=n%3==0?10:0;}
    fast=movementRouteTime(units,path,hex,1,1200);normal=movementRouteTime(units+0xf4,path,hex,1,1200);
    check(fast.turns>0 && fast.turns<normal.turns && fast.steps==1112 && fast.land==1112 && !fast.water,
          "long mixed road/plain corridor accounts for every step without truncating at the old range cap");
    printf("SYNTHETIC corridor (not the Rome map): 1112 steps, normal %d turns, caravan %d turns, land %d water %d\n",normal.turns,fast.turns,fast.land,fast.water);
    movementSetStepAllowed(onlyWater);
    check(!movementCanStep(units,hex) && movementCanStep(units+0xf4,hex),
          "A* hard exclusion uses the same step permission without treating blocked land as cost 255");
    check(stepCost(units,0,0,0,hex,nullptr)==255 && stepCost(units+0xf4,0,0,0,hex,nullptr)!=255,
          "water-only restriction reaches native step costs without restricting other transports");
    path={0,1,2};for(int n=0;n<3;++n)*(uint32_t*)(hex+n*20+4)=8;
    check(movementRouteTime(units,path,hex,1,1200).turns>0,"water-only ship retains a valid sea ETA");
    *(uint32_t*)(hex+20+4)=0;
    check(movementRouteTime(units,path,hex,1,1200).turns==-1,
          "ETA rejects a land shortcut using the same restriction as execution and search");
    movementSetStepAllowed(nullptr);
    printf("movement_trade_test: %d failures\n",failures);return failures?1:0;
}
