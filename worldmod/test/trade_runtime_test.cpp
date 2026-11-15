// Exercises the real serializer and command ownership without invoking the game executable.
#define TRADE_UI_TEST
#include "../src/trade.cpp"
bool midmapReady() { return false; }
bool midmapHexToPixel(int,int,float,float*,float*) { return false; }
void midmapSetClip(int) {}
void midmapSetView(float,float,float) {}
void midmapSetOverlay(MidmapOverlayFn) {}
void movementSetSeaCost(MovementSeaCostFn,int) {}
void movementSetAllowed(MovementAllowedFn) {}
void movementSetTradeUnits(MovementTradeFn) {}
void movementSetStepAllowed(MovementStepAllowedFn) {}
MovementRouteTime movementRouteTime(const uint8_t*,const std::vector<uint32_t>&,const uint8_t*,int,int) { return {}; }
bool movementReady() { return true; }
bool pathfindReady() { return true; }
bool pathfindRoute(void* u, uint32_t start, const std::vector<uint32_t>& targets, std::vector<uint32_t>* path, int* cost) {
    path->clear(); *cost=0;
    if (targets.empty() || !movementAllowed((uint8_t*)u)) return false;
    *path={start,targets[0]}; *cost=17; return true;
}
const MovementStepStats* movementStepStats() { static MovementStepStats s = {}; return &s; }
static int failures;
static void check(bool ok, const char* name) { printf("%s %s\n", ok ? "PASS" : "FAIL", name); failures += !ok; }
static void quiet(const char*, ...) {}
#include "trade_ship_cases.inc"
#include "trade_person_ui_cases.inc"
#include "trade_city_limit_cases.inc"
#include "trade_skill_telemetry_cases.inc"
#include "trade_delegated_cases.inc"
#include "trade_interaction_cases.inc"
static void autoOfferCases() {
    auto prices=g_data.price;g_data.price={10,20,500,600};
    tradeui::Model m;m.d=&g_data;m.live=g_mk;m.live.stock={100,100,0,0};m.seen=m.live;
    m.force=0;m.owner={0,1};m.stance={2,1};m.known={1,1};m.age={0,0};
    m.gold={10000,0};m.troops={5000,0};m.food={30000,0};m.grainRate={50,50};
    tradeauto::Plan p;p.force=0;p.origin=0;p.preferredDestination=1;p.budget.limit=4000;p.budget.advanceMonth(today()/30);
    auto before=m.live.stock;auto offers=autoOffersModel(p,0,m,10000);
    check(!offers.empty() && offers[0].rank.eligible() && offers[0].quote.cost+offers[0].reserve<=4000,
          "automatic planner produces a profitable basket within combined goods and fee allowance");
    check(m.live.stock==before && p.budget.occupied==0,
          "automatic preview does not consume inventory or monthly allowance");
    g_data.price={100,100,220,220};
    auto normal=autoOffersModel(p,0,m,10000);
    check(!normal.empty(),"ordinary-margin route is not rejected by the former 3000-gold provision placeholder");
    if(!normal.empty()) {
        auto rank=normal[0].rank;int64_t food=tradeauto::provisionsValue(g_cfg.food,50,TEMP_POL);
        check(food==585 && rank.roundTripNet==normal[0].quote.revenue-normal[0].quote.cost-normal[0].reserve-food,
              "automatic profit deducts native-valued provisions exactly once without charging cash budget");
        rank.roundTripNet-=g_cfg.food-food;rank.expenditure+=g_cfg.food-food;
        check(!rank.eligible(),"same ordinary route reproduces rejection under the old provision estimate");
    }
    std::wstring why;m.grainRate[0]=0;
    check(autoOffersModel(p,0,m,10000,-1,&why).empty() && why.find(L"糧價")!=std::wstring::npos,
          "unknown grain rate explains deferral instead of assuming free provisions");
    m.grainRate[0]=50;m.troops[0]=0;
    check(autoOffersModel(p,0,m,10000,-1,&why).empty() && why.find(L"兵力")!=std::wstring::npos,
          "insufficient escort is reported before scanning market routes");
    m.troops[0]=5000;g_data.price={10,20,500,600};
    m.known[1]=0;
    check(autoOffersModel(p,0,m,10000).empty(),"automatic planner cannot select unknown destination quotes");
    m.known[1]=1;
    check(autoOffersModel(p,0,m,tradeauto::CITY_RESERVE).empty(),"automatic planner preserves funding city's minimum cash");
    m.owner={1,0};p.origin=1;p.preferredDestination=0;m.tempLive=m.cfg.tempMax;m.freeSpecials=0;
    check(!autoOffersModel(p,0,m,10000,1).empty(),
          "existing caravan can buy return cargo at foreign port without owning it or allocating a new merchant");
    p.budget.occupied=p.budget.limit;
    check(autoOffersModel(p,0,m,10000,1).empty(),"return cargo cannot bypass an exhausted monthly allowance");
    p.budget.occupied=0;p.budget.limit=100;m.stance[1]=2;
    check(!autoOffersModel(p,0,m,2100,1).empty(),
          "small return budget pays continuation fee without requiring another formation fee");
    Caravan prepaid;prepaid.automatic.caravan=1;prepaid.automatic.fees.balance=50;
    check(autoPay(prepaid,30) && prepaid.automatic.fees.balance==20 &&
          !autoPay(prepaid,21) && !autoPay(prepaid,-1) && prepaid.automatic.fees.balance==20,
          "runtime automatic payment spends only escrow and rejects insufficient or negative charges atomically");
    prepaid.flags=F_TEMP;prepaid.leader=20;
    releaseTempIfOurs(prepaid);
    check(prepaid.automatic.fees.balance==20,
          "historical officer slots are excluded before invoking any native temporary-officer release");
    g_data.price=prices;
}
static void nativeViewCases() {
    g_data.cityName={"甲城","乙城"};g_data.goodName={"絲","鹽"};
    g_model=tradeui::Model();g_model.d=&g_data;g_model.live=g_mk;g_model.seen=g_mk;
    g_model.known={1,0};g_model.age={0,255};
    g_model.live.at(1,0)=3000;g_model.seen.at(1,0)=500;
    g_model.live.stock[2]=99;g_model.seen.stock[2]=7;
    auto unknown=nativeGoods(1);
    check(unknown.rows[0].cells[2]==L"未知" && unknown.rows[0].cells[3]==L"未知",
        "native market list does not reveal unknown prices or stock");
    g_model.known[1]=1;g_model.age[1]=3;
    auto stale=nativeGoods(1);
    check(stale.rows[0].cells[2]==nativeNum(g_model.seen.sellPrice(1,0)) && stale.rows[0].cells[3]==L"7" && nativeAge(1)==L"3個月前",
        "native market list uses dated observations rather than live foreign prices");
    g_model.live.stock[1]=0;
    auto buy=nativeGoods(0,true);
    tradeui::CaravanView car={};car.cargo.add(1,0,3,60);
    auto sell=nativeGoods(0,true,&car,true);
    check(!buy.rows[1].enabled && !sell.rows[0].enabled && sell.rows[1].enabled && sell.rows[1].cells[4]==L"3",
        "native goods choices distinguish market stock from the selected caravan cargo");
    check(buy.rows[0].picture==L"絲" && buy.rows[1].picture==L"鹽" && sell.rows[1].picture==L"鹽",
        "goods pictures use names and remain available for disabled stock rows");
    buy.picture=L"胡椒";
    check(tradenative::pictureForRow(buy,1)==L"鹽" && tradenative::pictureForRow(buy,0)==L"絲",
        "picture lookup uses the native stable key rather than the visual sorted position");
    check(tradenative::pictureForRow(buy,-1)==L"胡椒" && tradenative::pictureForRow(buy,INT_MAX)==L"胡椒",
        "invalid native focus falls back to the current operation's commodity picture");
    tradenative::Table sorted={L"",L"",{L"值"},{{42,{L"100"}},{7,{L"20"}},{99,{L"-5"}}}};
    tradenative::Context ctx={};ctx.table=&sorted;tradenative::current=&ctx;
    check(tradenative::sortValue(nullptr,nullptr,0,0,0,0)==100 && tradenative::sortValue(nullptr,nullptr,2,0,0,0)==-5,
        "native numeric columns sort by signed values, not displayed text");
    tradenative::current=nullptr;g_model=tradeui::Model();
}
static int __fastcall nativeMenuRange(uint8_t*,void*,int,int* first,int* last) { *first=6; *last=12; return 7; }
static const char* __fastcall nativeMenuText(uint8_t*,void*,int) { return "native"; }
static void citySubmenuCases() {
    g_cityRangePrev=nativeMenuRange; g_cityTextPrev=nativeMenuText; g_cityHelpPrev=nativeMenuText;
    int first=0,last=0;
    check(cityMenuRangeHook(nullptr,nullptr,CITY_TRADE,&first,&last)==1 && first==CITY_TRADE+1 && last==CITY_TRADE+8,
        "trade parent exposes eight native submenu actions");
    check(cityMenuRangeHook(nullptr,nullptr,0,&first,&last)==7 && first==6 && last==12,
        "original city menu range delegates unchanged");
    check(!strcmp(cityMenuTextHook(nullptr,nullptr,0),"native") && !strcmp(cityMenuHelpHook(nullptr,nullptr,0),"native"),
        "original menu labels and help delegate unchanged");
    check(!cityTradeEntry(INT_MIN) && !cityTradeEntry(CITY_TRADE) && !cityTradeEntry(CITY_TRADE+9) && !cityTradeEntry(INT_MAX),
        "parent, native sentinel and out-of-range commands are not trade actions");
    const int tabs[]={0,1,2,2,3,0,0,0};
    bool pages=true, leaves=true, text=true, reset=true;
    for(int i=0;i<CITY_TRADE_COUNT;i++) {
        int id=CITY_TRADE+i+1;
        cityMenuRangeHook(nullptr,nullptr,id,&first,&last);
        leaves &= first==INT_MIN && last==INT_MIN;
        text &= cityMenuTextHook(nullptr,nullptr,id)[0]!=0 && cityMenuHelpHook(nullptr,nullptr,id)[0]!=0;
        g_ui.dest=12;g_ui.leader=42;g_ui.confirm=true;g_ui.sendPending=true;g_ui.accounts=true;
        prepareCityTrade(9,*cityTradeEntry(id));
        pages &= g_ui.on && g_ui.origin==9 && g_ui.tab==tabs[i] && g_ui.accounts==(i==3);
        reset &= g_ui.dest==-1 && g_ui.leader==-1 && !g_ui.confirm && !g_ui.sendPending;
    }
    check(leaves && text,"trade leaves have labels/help and no further submenu");
    check(pages,"each submenu selects its intended page; accounts opens the ledger");
    check(reset,"opening a city action does not reuse an old destination or send confirmation");
    g_ui=tradeui::State(); g_cityRangePrev=nullptr;g_cityTextPrev=nullptr;g_cityHelpPrev=nullptr;
}
static std::vector<uint8_t> v5Bytes() {
    auto pl = payloadBytes(false); std::vector<uint8_t> v;
    put(v,TRAD); put(v,5u); put(v,(uint32_t)pl.size());
    v.insert(v.end(),pl.begin(),pl.end()); return v;
}
static std::vector<uint8_t> readFixture(const char* path) {
    FILE* f=nullptr;fopen_s(&f,path,"rb");if(!f)return {};
    fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    std::vector<uint8_t> bytes(size);fread(bytes.data(),1,bytes.size(),f);fclose(f);return bytes;
}
#include "trade_port_cases.inc"
static bool expandedGoodsCases(const char* beforePath,const char* afterPath) {
    auto before=readFixture(beforePath),after=readFixture(afterPath);std::string error;
    Data oldData,newData;
    bool loaded=!before.empty() && !after.empty() && oldData.load(before.data(),before.size(),&error) &&
        newData.load(after.data(),after.size(),&error) && (oldData.G==14 || oldData.G==69) && newData.G==72;
    check(loaded,"published catalogue and current pricing catalogue both load");
    if(!loaded){printf("catalogue error: %s\n",error.c_str());return false;}
    const int oldG=oldData.G, newG=newData.G;
    printf("catalogue migration %d -> %d goods\n",oldG,newG);
    bool unchanged=oldData.C==newData.C;
    for(int c=0;c<oldData.C;++c)for(int g=0;g<oldG;++g) {
        size_t a=(size_t)c*oldG+g,b=(size_t)c*newG+g;
        // This release deliberately reprices horses; all other published goods stay unchanged.
        unchanged &= (g==4 ? newData.price[b]>=oldData.price[a] : oldData.price[a]==newData.price[b]) &&
                     oldData.supply[a]==newData.supply[b] && oldData.demand[a]==newData.demand[b];
    }
    for(int g=0;g<oldG;++g)unchanged &= oldData.goodName[g]==newData.goodName[g] && oldData.loadUnits(g)==newData.loadUnits(g);
    check(unchanged,"only approved horse prices change in the old prefix; supplies, demands, IDs and cargo weights stay identical");
    for(int g:{4,69,70,71})check(newData.unitsPerLot(g)==100 && newData.minValue[g]==150,"military goods define 100 units per pricing lot with a 150 gold pre-spread floor");
    {
        auto invalid=after;uint32_t missingUnits=0;
        memcpy(invalid.data()+invalid.size()-72*8+69*8,&missingUnits,4);Data rejected;
        check(!rejected.load(invalid.data(),invalid.size(),&error),"v6 rejects a price floor without a defined lot size");
        check(!rejected.load(after.data(),after.size()-1,&error),"v6 rejects a truncated pricing table");
    }
    check(newData.loadUnits(14)==2 && newData.loadUnits(68)>0,"v5 loads explicit new bulk and final commodity weights");
    for(uint32_t units:{0u,101u}) {
        auto invalid=after;memcpy(invalid.data()+20+14*24+20,&units,4);Data rejected;
        check(!rejected.load(invalid.data(),invalid.size(),&error),"invalid v5 cargo weight rejects catalogue");
    }
    for(int g=69;g<72;++g) {
        int origin=-1;for(int c=0;c<newData.C;++c)if(newData.produces(c,g)){origin=c;break;}
        Market market;market.d=&newData;market.reset();Cargo basket;int64_t total=0;
        auto stock=market.stock;
        check(origin>=0 && quoteOrder(market,origin,{{g,2}},20,0,10000,100,&basket,&total) &&
              basket.lots[0].good==g && basket.loadUnits(newData)==20 && market.stock==stock,
              "new weapon has a producing city and can be quoted as two full cargo lots");
    }
    g_s.C=oldData.C;g_s.R=2;g_data=oldData;resetState();
    for(size_t i=0;i<g_mk.idx.size();++i){g_mk.idx[i]=800+(int)(i%401);g_mk.stock[i]=(int)(i%37);}
    for(int f=0;f<2;++f)for(int c=0;c<g_data.C;++c)g_info.see(f,c,&g_mk);
    g_info.endMonth(g_mk,today()/30-1);
    auto oldIdx=g_mk.idx,oldStock=g_mk.stock;auto oldInfo=g_info;
    Caravan k={};k.unit=1;k.leader=20;k.force=0;k.origin=0;k.dest=1;k.id=1;k.maxLoad=20;
    k.port=1;k.state=DOCKED;k.flags=F_PLAYER|F_TEMP;k.tempReleased=true;
    k.cargo.add(0,0,7,73);k.cargo.add(13,1,3,65);syncCargo(k);k.ledger.purchases=138;k.ledger.fees=300;
    k.transitLimit=50;k.transitPaid=9;k.automatic.caravan=1;k.automatic.origin=0;k.automatic.force=0;
    k.automatic.fees.month=today()/30;k.automatic.fees.balance=350;k.automatic.stage=tradeauto::Stage::Returning;
    g_car={k};g_nextCar=2;g_credit={{0,0,123456,500,3000}};g_saleTax={{0,1,7500}};g_merch={{20,700,2}};
    tradeauto::Plan plan;plan.origin=0;plan.force=0;plan.preferredDestination=1;plan.enabled=true;
    plan.budget.month=today()/30;plan.budget.limit=4000;plan.budget.occupied=850;plan.preferredGoods={0,13};g_autoPlans={plan};
    auto legacy=blockBytes(),v8=compressBlock(payloadBytes(true,true,false),8);
    g_data=newData;resetState();auto initialStock=g_mk.stock;auto stable=payloadBytes();int cars=0;
    check(!parseBlock(legacy.data()+12,legacy.size()-13,TRAD_VERSION,&cars) && payloadBytes()==stable,
          "truncated old catalogue save leaves expanded state unchanged");
    std::swap(g_data.goodName[0],g_data.goodName[1]);
    check(!parseBlock(legacy.data()+12,legacy.size()-12,TRAD_VERSION,&cars) && payloadBytes()==stable,
          "reordered legacy catalogue rejects migration atomically");
    std::swap(g_data.goodName[0],g_data.goodName[1]);
    if(oldG==69) {
        std::swap(g_data.goodName[67],g_data.goodName[68]);
        check(!parseBlock(legacy.data()+12,legacy.size()-12,TRAD_VERSION,&cars) && payloadBytes()==stable,
              "reordered IDs beyond the original fourteen reject 69-good migration atomically");
        std::swap(g_data.goodName[67],g_data.goodName[68]);
    }
    bool migrated=parseBlock(legacy.data()+12,legacy.size()-12,TRAD_VERSION,&cars);
    check(migrated && cars==1 && g_info.G==newG,"v9 published legacy save migrates to expanded goods");
    if(!migrated)return false;
    bool rows=true,observations=true,history=true,appended=true;
    for(int c=0;c<g_data.C;++c)for(int g=0;g<newG;++g) {
        size_t b=(size_t)c*newG+g,a=(size_t)c*oldG+g;
        if(g<oldG)rows &= g_mk.idx[b]==oldIdx[a] && g_mk.stock[b]==oldStock[a];
        else appended &= g_mk.idx[b]==1000 && g_mk.stock[b]==initialStock[b];
    }
    for(size_t r=0;r<(size_t)oldInfo.R*oldInfo.C;++r)for(int g=0;g<newG;++g) {
        const auto& b=g_info.observed[r*newG+g];
        if(g<oldG) {const auto& a=oldInfo.observed[r*oldG+g];observations &= a.idx==b.idx && a.mod==b.mod && a.stock==b.stock;}
        else observations &= b.idx==0 && b.stock==0;
    }
    for(size_t r=0;r<(size_t)oldInfo.H*oldInfo.C;++r)for(int g=0;g<newG;++g)
        history &= g_info.hist[r*newG+g]==(g<oldG ? oldInfo.hist[r*oldG+g] : 0);
    check(rows && appended,"migration preserves all old market rows and initializes appended markets");
    check(observations && history && g_info.age==oldInfo.age && g_info.histMonth==oldInfo.histMonth,
          "migration preserves all old observations, ages and history without inventing new observations");
    check(g_car[0].cargo.count()==10 && g_car[0].cargo.cost()==138 && g_car[0].cargo.lots[1].good==13 &&
          g_car[0].ledger.fees==300 && g_car[0].transitPaid==9 && g_credit[0].gold==123456 &&
          g_saleTax[0].remainder==7500 && g_merch[0].exp==700,
          "migration preserves mixed cargo, accounts, transit, credits, tax carry and merchant experience");
    check(g_autoPlans.size()==1 && g_autoPlans[0].preferredGoods==plan.preferredGoods &&
          g_autoPlans[0].budget.occupied==850 && g_car[0].automatic.fees.balance==350 && g_car[0].tempReleased &&
          !tempLeader(20),"migration preserves automatic budget, preferences, escrow and released merchant exclusion");
    Market seen;std::vector<char> known;g_info.view(g_mk,0,&seen,&known);
    check(!known[1] && seen.stock[newG+oldG]==0,"new foreign goods remain unknown until information refresh");
    auto full=payloadBytes(),packed=blockBytes();
    check(parseBlock(packed.data()+12,packed.size()-12,TRAD_VERSION,&cars) && payloadBytes()==full,
          "expanded migrated save round trips exactly");
    g_car[0].cargo.add(71,0,1,60);syncCargo(g_car[0]);g_car[0].ledger.purchases+=60;g_autoPlans[0].preferredGoods={69,70,71};
    packed=blockBytes();full=payloadBytes();
    check(parseBlock(packed.data()+12,packed.size()-12,TRAD_VERSION,&cars) && payloadBytes()==full && g_car[0].cargo.lots.back().good==71,
          "goods beyond ID 63 work in mixed cargo and automatic preferences across saves");
    g_model=tradeui::Model();g_model.d=&g_data;g_model.live=g_mk;g_model.seen=g_mk;
    g_model.known.assign(g_data.C,1);g_model.age.assign(g_data.C,0);
    auto table=nativeGoods(0);
    check(table.rows[4].cells.back()==L"100匹" && table.rows[69].cells.back()==L"100件",
          "native market explains horse and weapon pricing lot sizes");
    check(table.rows.size()==72 && table.rows.back().id==71 && tradenative::pictureForRow(table,69)==L"槍" &&
          tradenative::pictureForRow(table,70)==L"戟" && tradenative::pictureForRow(table,71)==L"弩",
          "native market table exposes final commodity and its stable picture key beyond 64 rows");
    g_purchases={{1,today(),0,0,69,2,400}};g_nextPurchase=2;
    auto recorded=payloadBytes(),recordBlock=blockBytes();g_purchases.clear();g_nextPurchase=1;
    check(parseBlock(recordBlock.data()+12,recordBlock.size()-12,TRAD_VERSION,&cars) && payloadBytes()==recorded &&
          g_purchases.size()==1 && g_purchases[0].cost==400 && g_nextPurchase==2 && g_car[0].tempReleased,
          "v10 preserves separate military expense, replay sequence and released merchant identity");
    auto damagedPurchase=recorded;int64_t negativeCost=-1;memcpy(damagedPurchase.data()+payloadBytes(true,true,true,true,false).size()-8,&negativeCost,8);
    check(!parseBlock(damagedPurchase.data(),damagedPurchase.size(),TRAD_VERSION,&cars,true) && payloadBytes()==recorded,
          "invalid military expense rejects entire save without changing any state");
    check(!parseBlock(recorded.data(),recorded.size()-1,TRAD_VERSION,&cars,true) && payloadBytes()==recorded,
          "truncated military record rejects atomically");
    bool ready=g_militaryReady;g_militaryReady=true;
    check(procureMilitary(0,0,69,1,100,1)=="military duplicate order" && payloadBytes()==recorded,
          "replayed procurement token is rejected before native resource calls");
    check(procureMilitary(g_s.R,0,69,1,100,2)=="military ownership changed" && payloadBytes()==recorded,
          "invalid purchasing force cannot mutate city or market");
    g_militaryReady=ready;
    auto oldV9=payloadBytes(true,true,true,false);
    check(parseBlock(oldV9.data(),oldV9.size(),9,&cars,true) && g_purchases.empty() && g_nextPurchase==1 && g_car[0].tempReleased,
          "v9 migration starts an empty procurement book without resurrecting landed merchants");
    g_model=tradeui::Model();
    check(parseBlock(v8.data()+12,v8.size()-12,8,&cars) && g_car[0].cargo.count()==10 && g_autoPlans.empty(),
          "v8 legacy save also expands without fabricating automatic plans");
    g_s.R=249;resetState();
    return true;
}
static HANDLE pumpOwnerReady, pumpNativeDone;
static int pumpNativeCalls, pumpResult;
static int __fastcall testNativePump(void* self, void*, int arg) {
    if(self==(void*)0x1234 && arg==17)++pumpNativeCalls;
    SetEvent(pumpNativeDone);
    return 123;
}
static DWORD WINAPI holdTradeLock(void*) {
    Lock l;
    SetEvent(pumpOwnerReady);
    WaitForSingleObject(pumpNativeDone,5000);
    return 0;
}
static DWORD WINAPI callTradePump(void*) {
    pumpResult=pumpHook((void*)0x1234,nullptr,17);
    return 0;
}
static void pumpLockCases() {
    auto original=g_pumpOrig;g_pumpOrig=testNativePump;
    auto job=std::make_shared<Job>();g_jobs.push_back(job);
    pumpOwnerReady=CreateEventA(nullptr,TRUE,FALSE,nullptr);
    pumpNativeDone=CreateEventA(nullptr,TRUE,FALSE,nullptr);
    HANDLE owner=CreateThread(nullptr,0,holdTradeLock,nullptr,0,nullptr);
    check(WaitForSingleObject(pumpOwnerReady,2000)==WAIT_OBJECT_0,"logic thread holds trade lock while awaiting native command");
    HANDLE pump=CreateThread(nullptr,0,callTradePump,nullptr,0,nullptr);
    bool responsive=WaitForSingleObject(pump,500)==WAIT_OBJECT_0;
    check(responsive,"busy trade lock cannot block the native main-thread command pump");
    // Stop this isolated test process on the reproduced deadlock; releasing
    // the lock would enter fixed native game code absent from this fixture.
    if(!responsive) {fflush(stdout);ExitProcess(1);}
    SetEvent(pumpNativeDone);
    WaitForSingleObject(owner,2000);WaitForSingleObject(pump,2000);
    check(pumpNativeCalls==1 && pumpResult==123,"native pump keeps its arguments and return value under contention");
    check(g_jobs.size()==1 && g_jobs.front()==job,"busy trade pump defers queued trade work without losing it");
    g_jobs.clear();g_pumpOrig=original;
    CloseHandle(owner);CloseHandle(pump);CloseHandle(pumpOwnerReady);CloseHandle(pumpNativeDone);
}
static void unitTradeDetailsCases() {
    auto oldCars=g_car;auto oldUnits=g_unit0;bool oldOn=g_on;
    auto oldNames=g_data.goodName,oldCities=g_data.cityName;
    auto oldLeaders=g_tradeLeader;
    uint8_t units[0xf4*2]={};g_unit0=units;g_on=true;
    *(int*)(units+8)=1;*(int*)(units+0xc)=21;
    g_tradeLeader.assign(2,0);g_tradeLeader[0]=22;
    g_data.goodName={"絲","鹽"};g_data.cityName={"番禺","羅馬"};
    Caravan k={};k.unit=0;k.leader=21;k.id=17;k.origin=0;k.dest=1;k.maxLoad=60;k.shipType=MERCHANT_SHIP;
    k.cargo.add(0,0,3,250);k.cargo.add(1,1,2,100);g_car={k};
    TradeShipUiDetails out;Table table;
    check(readUnitTradeDetails(units,out,&table) && out.summary.find(L"大型海船")!=std::wstring::npos &&
        out.cargo.find(L"絲 3批")!=std::wstring::npos && out.cargo.find(L"鹽 2批")!=std::wstring::npos,
        "native unit details copy mixed cargo, weighted load and ship type");
    check(table.rows.size()==9 && table.rows[7].cells[1]==L"番禺" && table.rows[7].cells[3]==L"250" &&
        table.rows[8].cells[1]==L"羅馬" && table.rows[6].cells[2]==L"5",
        "full unit cargo preserves each origin, quantity and cost");
    g_car[0].cargo.lots.clear();g_car[0].shipType=CARAVAN;
    readUnitTradeDetails(units,out,&table);
    check(out.cargo==L"載貨：空載" && table.rows[2].cells[1]==L"無（陸路運輸）" && table.rows.size()==8,
        "empty land caravan does not retain a sea vessel or former cargo");
    g_car[0].cargo.add(0,0,1,90);
    readUnitTradeDetails(units,out,&table);
    check(out.cargo==L"載貨：絲 1批" && table.rows[7].cells[3]==L"90","unit cargo refreshes after buying and selling");
    g_car[0].tempReleased=true;check(!readUnitTradeDetails(units,out),"released merchant has no unit details");g_car[0].tempReleased=false;
    *(int*)(units+0xc)=22;check(!readUnitTradeDetails(units,out),"reused unit identity cannot expose a former caravan's cargo");*(int*)(units+0xc)=21;
    check(!readUnitTradeDetails(units+0xf4,out),"ordinary transport has no trade details");
    pumpOwnerReady=CreateEventA(nullptr,TRUE,FALSE,nullptr);pumpNativeDone=CreateEventA(nullptr,TRUE,FALSE,nullptr);
    HANDLE owner=CreateThread(nullptr,0,holdTradeLock,nullptr,0,nullptr);WaitForSingleObject(pumpOwnerReady,2000);
    ULONGLONG start=GetTickCount64();bool read=readUnitTradeDetails(units,out);
    check(!read && GetTickCount64()-start<500,"properties reader returns promptly while dispatch owns the trade lock");
    SetEvent(pumpNativeDone);WaitForSingleObject(owner,2000);
    CloseHandle(owner);CloseHandle(pumpOwnerReady);CloseHandle(pumpNativeDone);
    g_car=oldCars;g_unit0=oldUnits;g_on=oldOn;g_data.goodName=oldNames;g_data.cityName=oldCities;g_tradeLeader=oldLeaders;
}
int main(int argc,char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    baseRec=(GetFn)shipTestBase; // Isolated tests use directory positions instead of native city objects.
    citySubmenuCases();
    void* world = VirtualAlloc((void*)0x7200000, 0x20000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (world != (void*)0x7200000) { puts("cannot allocate isolated world date fixture"); return 2; }
    int* w = (int*)(uintptr_t)WORLD; w[2] = 184; w[3] = 6; w[4] = 1;
    InitializeCriticalSection(&g_cs); g_log = quiet;
    pumpLockCases();
    g_s.C = 2; g_s.R = 2; g_s.U = 20; g_cfg.PT = 1100;
    g_data.C = 2; g_data.G = 2; g_data.size = { 100,100 }; g_data.flags = { CITY_HUB,0 };
    g_data.supply = { 3,0,0,0 }; g_data.price = { 10,20,30,40 }; g_data.lo = { 0,20 }; g_data.hi = { 0,20 };
    g_data.group = { 1,1 }; g_data.demand.assign(4,1000);
    g_mk.d = &g_data; g_unitDir.assign(20,0); g_pausedLeader.assign(20,0); g_tradeLeader.assign(20,0);g_shipLeader.assign(20,0);g_shipQuay.assign(20,0xffffffffu); resetState();
    autoOfferCases();
    cityLimitCases();
    merchantTelemetryCases();
    shipOfferCases();
    merchantProfitCases();
    nativeViewCases();
    interactionCases();
    unitTradeDetailsCases();
    check(g_month0 == 5, "reset initializes June before any movement");
    Caravan k = {}; k.unit=1; k.leader=20; k.force=0; k.corps=0; k.origin=0; k.dest=1; k.good=0;
    k.id=1; k.maxLoad=20; k.port=1; k.state=DOCKED; k.flags=F_PLAYER; k.taxed={0};
    k.cargo.add(0,0,7,73); k.cargo.add(1,1,3,65); syncCargo(k); k.ledger.purchases=138; k.ledger.fees=300; g_car.push_back(k); g_nextCar=2;
    g_credit.push_back({0,0,123456,500,3000}); g_info.see(0,0,&g_mk); g_merch.push_back({20,700,2});
    shipSaveCases();
    personUiCases();
    g_cfg.specials=300;g_cfg.PT+=200;g_hiddenTempForces.assign(300,0);
    personUiCases();
    {
        auto prior=payloadBytes();int oldU=g_s.U,loaded=0;
        g_s.U=200;Caravan sample=g_car[0];g_car.clear();
        for(int i=0;i<200;++i) {
            auto car=sample;car.id=i+1;car.unit=i;car.leader=g_cfg.PT-g_cfg.specials+i;
            car.flags|=F_TEMP;car.tempReleased=false;g_car.push_back(car);
        }
        g_nextCar=201;rebuildDirs();auto bytes200=blockBytes();
        check(tempLive()==200 && parseBlock(bytes200.data()+12,bytes200.size()-12,TRAD_VERSION,&loaded) && loaded==200 && tempLive()==200,
              "200 simultaneous caravan records and expanded merchant IDs survive save round trip");
        for(auto& car:g_car)car.tempReleased=true;
        rebuildDirs();check(tempLive()==0,"released merchants no longer consume the 200-caravan allowance");
        g_s.U=oldU;check(parseBlock(prior.data(),prior.size(),TRAD_VERSION,&loaded,true),"large caravan fixture restores original runtime state");
    }
    g_cfg.specials=100;g_cfg.PT-=200;g_hiddenTempForces.assign(100,0);rebuildDirs();
    auto bytes = v5Bytes(); int cars=0;
    check(tradSpan(bytes.data(), bytes.size()) == bytes.size(), "complete TRAD span");
    check(tradSpan(bytes.data(), 7) == 7 && tradSpan(bytes.data(), bytes.size()-1) == bytes.size()-1, "truncated header and payload remain opaque");
    uint32_t unrelated=123;
    check(!tradSpan((uint8_t*)&unrelated,4), "unrelated following block is not consumed");
    uint8_t units[20*0xf4] = {}; g_unit0=units; *(int*)(units+0xf4+0xc)=20;
    rebuildDirs(); check(!movementAllowed(units+0xf4), "docked caravan cannot move");
    *(int*)(units+0xf4+8)=1;
    check(tradeMovingUnit(units+0xf4) && !tradeMovingUnit(units),"acceleration identifies only a registered transport and leader");
    shipIdentityCases(units);
    portAccessCases(units);
    portQuoteCases();
    portSaveCases();
    *(int*)(units+0xf4+8)=0;
    check(!tradeMovingUnit(units+0xf4),"combat unit cannot inherit caravan acceleration");
    *(int*)(units+0xf4+8)=1;
    g_car[0].flags|=F_LOOTED; rebuildDirs();
    check(!tradeMovingUnit(units+0xf4),"destroyed caravan immediately loses acceleration");
    check(movementAllowed(units+0xf4) && !g_unitDir[1] && !liveUnit(g_car[0]),
          "legacy destroyed voyage cannot freeze or reclaim a reused unit and leader");
    g_car[0].flags&=~F_LOOTED; rebuildDirs();
    g_car[0].flags|=F_TEMP;g_car[0].tempReleased=true;rebuildDirs();
    check(!tempLive() && !tempLeader(20) && !liveUnit(g_car[0]) && movementAllowed(units+0xf4) &&
          !tradeMovingUnit(units+0xf4) && !g_unitDir[1],
          "released merchant leaves cargo intact and cannot own, pause or accelerate a reused slot");
    check(g_car[0].cargo.count()==10 && g_car[0].ledger.purchases==138,
          "releasing a landed merchant does not destroy its goods or accounts");
    g_car[0].tempReleased=false;
    check(tempLive()==1 && tempLeader(20) && !tempLeader(21),
          "temporary officer filtering uses registered identity, not an ordinary officer's name");
    g_car[0].flags&=~F_TEMP;rebuildDirs();
    std::vector<uint32_t> path; int routeCost=0;
    check(queryRoute(units+0xf4,123,{456},&path,&routeCost) && path.back()==456 && routeCost==17 && !movementAllowed(units+0xf4), "route query temporarily allows planning but leaves docked unit frozen");
    check(!queryRoute(units+0xf4,123,{},&path,&routeCost) && !movementAllowed(units+0xf4) && path.empty(), "failed route query also restores pause and leaves no stale path");
    *(int*)(units+0xf4+0xc)=22; check(movementAllowed(units+0xf4), "reused slot with another leader is not frozen");
    check(!tradeMovingUnit(units+0xf4),"reused slot with a different leader is not accelerated");
    *(int*)(units+0xf4+0xc)=20; g_car[0].state=TRAVELLING; rebuildDirs();
    check(movementAllowed(units+0xf4), "departing caravan can move");
    g_car[0].state=FINISHED; rebuildDirs();
    check(!tradeMovingUnit(units+0xf4),"finished voyage loses acceleration");
    check(movementAllowed(units+0xf4) && !g_unitDir[1], "finished voyage cannot leave movement or seasonal direction on a reused unit slot");
    g_car[0].state=DOCKED; g_unit0=nullptr;
    check(parseBlock(bytes.data()+12,bytes.size()-12,5,&cars) && cars==1, "v5 reads multi-good voyage and ledger");
    check(v5Bytes()==bytes, "v5 round trip preserves inventory, observations, cargo, credits, IDs and ledger");
    g_car[0].cost+=300;
    auto oldSummary=v5Bytes();
    check(parseBlock(oldSummary.data()+12,oldSummary.size()-12,5,&cars) && g_car[0].cost==138 && v5Bytes()==bytes,
          "old cargo summary excludes departure fees after load without changing cargo or ledger");
    auto v6=compressBlock(payloadBytes(false),6);
    check(parseBlock(v6.data()+12,v6.size()-12,6,&cars) && g_saleTax.empty() && v5Bytes()==bytes,"v6 legacy compression migrates with zero fractional tax");
    check(salesTax(0,1,95,5,100)==4,"first partial sale retains fractional tax");
    auto taxSave=blockBytes(); g_saleTax.clear();
    check(parseBlock(taxSave.data()+12,taxSave.size()-12,TRAD_VERSION,&cars) && salesTax(0,1,95,5,100)==5,"fractional tax survives save-load between partial sales");
    check(salesTax(1,0,95,5,100)==4,"reverse trading direction has an independent tax balance");
    auto goodTax=payloadBytes(true,false); auto badTax=goodTax; int invalidRemainder=10000; memcpy(badTax.data()+badTax.size()-4,&invalidRemainder,4);
    check(!parseBlock(badTax.data(),badTax.size(),7,&cars,true) && payloadBytes(true,false)==goodTax,"invalid tax remainder is rejected without changing state");
    check(!parseBlock(goodTax.data(),goodTax.size()-1,7,&cars,true) && payloadBytes(true,false)==goodTax,"truncated tax balances are rejected atomically");
    check(parseBlock(v6.data()+12,v6.size()-12,6,&cars) && g_saleTax.empty(),"loading an older save clears fractional balances from the previous game");
    {
        auto original=g_car[0];
        g_car[0].cargo.lots.clear();g_car[0].cargo.add(0,0,10,100);g_car[0].cargo.add(1,0,50,200);syncCargo(g_car[0]);
        g_car[0].ledger.purchases=300;g_car[0].transitLimit=15;g_car[0].transitPaid=9;
        auto oldDone=g_done;auto loss=g_car[0];loss.flags|=F_PLAYER;
        recordDone(loss,0,tradeui::DONE_INTERCEPTED);
        check(g_done.size()==oldDone.size()+2 && g_done[g_done.size()-2].good==0 &&
              g_done[g_done.size()-2].qty==10 && g_done.back().good==1 && g_done.back().qty==50 && g_done.back().cost==200,
              "mixed cargo loss history retains each commodity quantity and cost");
        g_done=oldDone;
        auto mixed=blockBytes();g_car[0].transitPaid=0;
        check(parseBlock(mixed.data()+12,mixed.size()-12,TRAD_VERSION,&cars) && g_car[0].cargo.count()==60 &&
              g_car[0].cargo.loadUnits(g_data)==150 && g_car[0].transitPaid==9 && g_car[0].transitLimit==15,
              "v8 saves mixed bulk cargo beyond legacy count capacity and retains transit already paid");
        auto raw=payloadBytes(true,true,false);auto damaged=raw;int64_t excess=16;memcpy(damaged.data()+damaged.size()-8,&excess,8);
        check(!parseBlock(damaged.data(),damaged.size(),8,&cars,true) && payloadBytes(true,true,false)==raw,
              "transit paid beyond cap rejects the whole save without mutation");
        check(!parseBlock(raw.data(),raw.size()-1,8,&cars,true) && payloadBytes(true,true,false)==raw,"truncated transit cap data is rejected atomically");
        auto legacy=payloadBytes(true,false);
        check(parseBlock(legacy.data(),legacy.size(),7,&cars,true) && g_car[0].transitLimit==0 && g_car[0].transitPaid==0,
              "v7 migration adds no retroactive transit charges on the current leg");
        g_car[0]=original;
    }
    {
        auto legacy=payloadBytes(true,true,false);auto original=g_car[0];
        tradeauto::Plan plan;plan.origin=0;plan.force=0;plan.preferredDestination=1;plan.enabled=true;
        plan.budget.month=today()/30;plan.budget.limit=4000;plan.budget.occupied=850;
        plan.dispatchedMonth=plan.budget.month;plan.preferredGoods={0,1};g_autoPlans={plan};
        auto& a=g_car[0].automatic;a.caravan=1;a.origin=0;a.force=0;
        a.stage=tradeauto::Stage::Returning;a.issue=tradeauto::Issue::Route;
        a.retryMonth=plan.budget.month+1;a.fees.month=plan.budget.month;a.fees.balance=350;
        auto full=payloadBytes();auto save=blockBytes();g_autoPlans.clear();g_car[0].automatic={};
        check(parseBlock(save.data()+12,save.size()-12,TRAD_VERSION,&cars) && payloadBytes()==full &&
              g_autoPlans[0].budget.occupied==850 && g_car[0].automatic.fees.balance==350,
              "v9 round trip preserves plan, basket preferences, occupied budget and prepaid cash");
        check(g_car[0].automatic.stage==tradeauto::Stage::Returning &&
              g_car[0].automatic.issue==tradeauto::Issue::Route &&
              g_car[0].automatic.retryMonth==plan.budget.month+1,
              "v9 preserves committed return purchases and next-month retry without repeating purchases");
        auto damaged=full;int64_t negative=-1;memcpy(damaged.data()+payloadBytes(true,true,true,true,false).size()-16,&negative,8);
        check(!parseBlock(damaged.data(),damaged.size(),TRAD_VERSION,&cars,true) && payloadBytes()==full,
              "negative automatic advance rejects the entire save atomically");
        damaged=full;int badIssue=7;memcpy(damaged.data()+payloadBytes(true,true,true,true,false).size()-32,&badIssue,4);
        check(!parseBlock(damaged.data(),damaged.size(),TRAD_VERSION,&cars,true) && payloadBytes()==full,
              "undefined automatic exception rejects the entire save atomically");
        check(!parseBlock(full.data(),full.size()-1,TRAD_VERSION,&cars,true) && payloadBytes()==full,
              "truncated automatic plan data cannot partly overwrite current state");
        g_car[0].flags|=F_TEMP;g_car[0].tempReleased=true;
        auto reused=g_car[0];reused.id=2;reused.tempReleased=false;reused.automatic={};
        g_car.push_back(reused);g_nextCar=3;auto recycled=payloadBytes();
        check(parseBlock(recycled.data(),recycled.size(),TRAD_VERSION,&cars,true) && cars==2 &&
              g_car[0].tempReleased && !g_car[1].tempReleased,
              "v9 accepts a recycled officer/unit pair without reconnecting the landed old merchant");
        g_car[0].tempReleased=false;auto collision=payloadBytes();
        check(!parseBlock(collision.data(),collision.size(),TRAD_VERSION,&cars,true),
              "v9 still rejects two live caravans sharing an officer or unit");
        g_car={original};g_nextCar=2;g_autoPlans.clear();
        g_autoPlans.push_back(plan);
        check(parseBlock(legacy.data(),legacy.size(),8,&cars,true) && g_autoPlans.empty() &&
              !g_car[0].automatic.caravan && !g_car[0].tempReleased,
              "v8 load removes previous-session automation and starts with no automatic spending");
    }
    auto packed=blockBytes();
    check(packed.size()>16 && parseBlock(packed.data()+12,packed.size()-12,TRAD_VERSION,&cars) && v5Bytes()==bytes,
          "v8 compressed round trip preserves the complete v5 payload");
    auto corrupt=packed; corrupt[16]^=0xff;
    check(!parseBlock(corrupt.data()+12,corrupt.size()-12,TRAD_VERSION,&cars) && v5Bytes()==bytes, "corrupt compression header is rejected atomically");
    check(!parseBlock(packed.data()+12,packed.size()-13,TRAD_VERSION,&cars) && v5Bytes()==bytes, "truncated compressed block is rejected atomically");
    corrupt=packed; uint32_t huge=0xffffffff; memcpy(corrupt.data()+12,&huge,4);
    check(!parseBlock(corrupt.data()+12,corrupt.size()-12,TRAD_VERSION,&cars), "oversized decompression rejected before allocation");
    corrupt=packed; uint32_t wrong=(uint32_t)bytes.size(); memcpy(corrupt.data()+12,&wrong,4);
    check(!parseBlock(corrupt.data()+12,corrupt.size()-12,TRAD_VERSION,&cars), "mismatched decompressed length rejected");
    auto stable = blockBytes();
    check(!parseBlock(bytes.data()+12,bytes.size()-13,5,&cars) && blockBytes()==stable, "truncated v5 is rejected without mutation");
    auto bad=bytes; int invalid=-3; memcpy(bad.data()+12+8+4*4+12+4+4,&invalid,4);
    check(!parseBlock(bad.data()+12,bad.size()-12,5,&cars) && blockBytes()==stable, "invalid officer rejects entire block");
    bad=bytes; int64_t negative=-1; memcpy(bad.data()+bad.size()-48,&negative,8);
    check(!parseBlock(bad.data()+12,bad.size()-12,5,&cars) && blockBytes()==stable, "invalid ledger rejects save atomically");
    Job destroyed; destroyed.line="trade sell 1 0 1"; g_car[0].flags |= F_LOOTED;
    runJob(&destroyed);
    check(destroyed.result=="unknown, destroyed or no longer owned voyage" && g_car[0].cargo.count()==10, "destroyed caravan cannot sell cargo before end-of-turn cleanup");
    g_car[0].flags &= ~F_LOOTED;
    auto v4=bytes; v4.resize(v4.size()-48); // one ledger (44) and empty closed-account count (4)
    check(parseBlock(v4.data()+12,v4.size()-12,4,&cars) && !g_car[0].ledger.complete && g_car[0].ledger.purchases==138, "v4 migration labels missing accounting history instead of inventing it");
    closeLedger(g_car[0]); closeLedger(g_car[0]);
    auto closedBytes=v5Bytes();
    check(g_closed.size()==1 && g_closed[0].ledger.losses==138 && parseBlock(closedBytes.data()+12,closedBytes.size()-12,5,&cars) && g_closed.size()==1, "closing is idempotent and its loss/account survives save-load");
    std::vector<uint8_t> old; put(old,(uint32_t)2);put(old,(uint32_t)2);
    for(int i=0;i<4;i++)put(old,(int32_t)1000);
    put(old,(int32_t)-1);put(old,(uint32_t)123);put(old,(uint32_t)0);put(old,(uint32_t)1);
    LegacyCaravan legacy={};legacy.unit=2;legacy.leader=21;legacy.force=0;legacy.origin=0;legacy.dest=1;legacy.good=0;legacy.qty=8;legacy.cost=150;legacy.taxDue=33;
    put(old,legacy);
    check(parseBlock(old.data(),old.size(),1,&cars) && g_car[0].cargo.count()==8 && g_car[0].taxDue==33, "v1 migrates cargo and legacy tax debt without charging again");
    check(g_info.ageOf(0,0)==255, "legacy incomplete observations are invalidated");
    for(uint32_t version : {2u,3u}) {
        auto v=old;put(v,(uint32_t)0);put(v,(uint32_t)0);
        put(v,(uint32_t)0);put(v,(uint32_t)0);put(v,(uint32_t)0);put(v,(uint32_t)0);put(v,(int32_t)-1);
        if(version==3)put(v,(uint32_t)0);
        check(parseBlock(v.data(),v.size(),version,&cars) && g_car[0].cargo.count()==8, "v2/v3 migration remains readable");
    }
    auto command=std::make_shared<Job>();command->done=CreateEventA(nullptr,TRUE,FALSE,nullptr);
    std::weak_ptr<Job> weak=command; g_jobs.push_back(command);
    std::vector<std::shared_ptr<Job>> running;running.swap(g_jobs);command.reset();
    check(!weak.expired() && SetEvent(running[0]->done), "timed-out caller cannot destroy running job or event");
    running.clear();check(weak.expired(), "last owner releases completed command");
    auto queued=std::make_shared<Job>();queued->done=CreateEventA(nullptr,TRUE,FALSE,nullptr);g_jobs.push_back(queued);
    resetState();check(g_jobs.empty() && WaitForSingleObject(queued->done,0)==WAIT_OBJECT_0 && queued->result=="cancelled by game load", "loading cancels queued commands and wakes callers");
    const char* oldCatalogue=argc>1 ? argv[1] : "worldmod/test/fixtures/trade-v4-14.bin";
    const char* newCatalogue=argc>2 ? argv[2] : "worldmod/build/trade.bin";
    if(!expandedGoodsCases(argc>6 ? "worldmod/test/fixtures/trade-v4-14.bin" : oldCatalogue,newCatalogue))return 1;
    if(argc==1 && !expandedGoodsCases("worldmod/test/fixtures/trade-v5-69.bin",newCatalogue))return 1;
    // Actual 530-city, 249-force, 72-good catalogue exceeds the former 64 MiB decoder limit.
    for(int f=0;f<249;f++) g_info.see(f,f%530,&g_mk);
    auto full=payloadBytes(); packed=blockBytes();
    printf("full-world trade payload %zu -> %zu bytes\n",full.size(),packed.size());
    check(full.size()>64u*1024*1024 && !packed.empty() && packed.size()<1024*1024 &&
          parseBlock(packed.data()+12,packed.size()-12,TRAD_VERSION,&cars) && payloadBytes()==full,
          "full-world save fits native heap and round trips every observation");
    {
        Data current=g_data,previous;std::string error;
        auto bytes=readFixture("worldmod/test/fixtures/trade-v5-72.bin");
        bool loaded=!bytes.empty() && previous.load(bytes.data(),bytes.size(),&error);
        check(loaded,"previous 72-good pricing catalogue remains readable");
        if(!loaded)return 1;
        g_data=previous;resetState();
        Caravan old=k;old.cargo.lots.clear();old.cargo.add(69,0,2,64);old.cargo.add(70,0,2,80);old.cargo.add(71,0,2,100);
        syncCargo(old);old.ledger.purchases=244;g_car={old};g_nextCar=2;
        auto before=payloadBytes(),save=blockBytes();int oldQuote=g_mk.buyPrice(0,69);
        g_data=current;
        check(parseBlock(save.data()+12,save.size()-12,TRAD_VERSION,&cars) && payloadBytes()==before &&
              g_car[0].cargo.cost()==244 && g_car[0].ledger.purchases==244 && g_mk.buyPrice(0,69)>oldQuote,
              "repricing an existing 72-good save updates quotes without rewriting original cargo costs or any saved field");
    }
    if(argc>6) {
        // Optional extracted TRAD block from a real save. Never opens/writes the original save.
        auto block=readFixture(argv[3]);auto oldBytes=readFixture(oldCatalogue);std::string error;
        Data expanded=g_data,oldData;oldData.load(oldBytes.data(),oldBytes.size(),&error);
        w[2]=atoi(argv[4]);w[3]=atoi(argv[5]);w[4]=atoi(argv[6]);
        g_cfg.PT=14413;g_s.U=1000;g_unitDir.assign(1000,0);g_pausedLeader.assign(1000,0);g_tradeLeader.assign(1000,0);g_shipLeader.assign(1000,0);g_shipQuay.assign(1000,0xffffffffu);
        uint32_t version=0;if(block.size()>=12)memcpy(&version,block.data()+4,4);
        g_data=oldData;resetState();
        bool valid=block.size()>12 && parseBlock(block.data()+12,block.size()-12,version,&cars);
        check(valid,"actual extracted save parses with its original catalogue and date");
        if(valid) {
            if(version>=9 && version<=11) {
                uint32_t rawSize=0;memcpy(&rawSize,block.data()+12,4);
                std::vector<uint8_t> originalRaw(rawSize);TradeCodec codec(true);SIZE_T written=0;
                bool decoded=codec.h && Decompress(codec.h,block.data()+16,block.size()-16,originalRaw.data(),originalRaw.size(),&written);
                check(decoded && written==originalRaw.size() && originalRaw==payloadBytes(true,true,true,version>=10,version>=11),
                      "original real-save payload round trips byte-for-byte without normalizing any existing field");
            }
            auto expected=payloadBytes();g_data=expanded;resetState();
            bool migrated=parseBlock(block.data()+12,block.size()-12,version,&cars);
            check(migrated,"actual extracted save migrates to expanded goods");
            if(migrated) {
                auto upgraded=blockBytes(),expandedPayload=payloadBytes();
                check(parseBlock(upgraded.data()+12,upgraded.size()-12,TRAD_VERSION,&cars) && payloadBytes()==expandedPayload,
                      "upgraded real-save block round trips before export");
                auto trim=[&](auto& v,size_t rows) {
                    auto small=v;small.resize(rows*oldData.G);
                    for(size_t r=0;r<rows;++r)std::copy_n(v.begin()+r*expanded.G,oldData.G,small.begin()+r*oldData.G);
                    v=std::move(small);
                };
                trim(g_mk.idx,g_data.C);trim(g_mk.stock,g_data.C);
                trim(g_info.hist,(size_t)g_info.H*g_info.C);trim(g_info.observed,(size_t)g_info.R*g_info.C);
                g_info.G=oldData.G;g_data=oldData;
                check(payloadBytes()==expected,"every original serialized byte survives real-save migration when appended columns are removed");
                if(argc>7 && !failures) {
                    FILE* out=nullptr;fopen_s(&out,argv[7],"wb");
                    bool written=out && fwrite(upgraded.data(),1,upgraded.size(),out)==upgraded.size();
                    if(out)written=fclose(out)==0 && written;
                    check(written,"exports validated upgraded TRAD block to caller staging path only");
                }
                // Optional read-only city snapshot: force, count; then each city's
                // owner, stance, gold, troops, food, native grain rate. No native calls.
                if(argc>8) {
                    FILE* in=nullptr;fopen_s(&in,argv[8],"r");int force=-1,count=0;
                    bool valid=in && fscanf_s(in,"%d %d",&force,&count)==2 && count==g_data.C;
                    tradeui::Model model;model.d=&g_data;model.live=g_mk;model.force=force;
                    model.owner.resize(count);model.stance.resize(count);model.gold.resize(count);
                    model.troops.resize(count);model.food.resize(count);model.grainRate.resize(count);
                    for(int c=0;valid && c<count;++c) {
                        int s=0;valid=fscanf_s(in,"%d %d %d %d %d %d",&model.owner[c],&s,&model.gold[c],&model.troops[c],&model.food[c],&model.grainRate[c])==6;
                        model.stance[c]=(int8_t)s;
                    }
                    if(in)fclose(in);
                    check(valid,"actual city resource snapshot parses for offline automatic-route replay");
                    if(valid) {
                        auto info=g_info;for(int c=0;c<count;++c)if(model.owner[c]==force)info.see(force,c,&g_mk);
                        info.view(g_mk,force,&model.seen,&model.known);model.tempLive=tempLive();
                        for(auto p:g_autoPlans)if(p.force==force) {
                            p.budget.advanceMonth(today()/30);std::wstring why;
                            auto offers=autoOffersModel(p,p.origin,model,model.gold[p.origin],-1,&why);
                            int oldEligible=0;auto food=tradeauto::provisionsValue(g_cfg.food,model.grainRate[p.origin],TEMP_POL);
                            for(auto offer:offers) {
                                offer.rank.roundTripNet-=g_cfg.food-food;offer.rank.expenditure+=g_cfg.food-food;
                                oldEligible+=offer.rank.eligible();
                            }
                            printf("REPLAY city=%d cash=%d budget=%lld grain=%d foodValue=%lld retry=%d candidates=%zu oldEstimateEligible=%d\n",
                                   p.origin,model.gold[p.origin],(long long)p.budget.available(),model.grainRate[p.origin],(long long)food,p.mayDispatch(today()/30),offers.size(),oldEligible);
                            int best=autoBest(offers,p.preferredDestination);
                            if(best>=0) {auto& o=offers[best];printf("REPLAY best=%d cashCost=%lld estimatedNet=%lld\n",o.rank.destination,(long long)(o.quote.cost+o.reserve),(long long)o.rank.roundTripNet);}
                        }
                    }
                }
            }
        }
    }
    delegatedPlanCases();
    DeleteCriticalSection(&g_cs);VirtualFree(world,0,MEM_RELEASE);
    printf("trade_runtime_test: %d failures\n",failures);return failures?1:0;
}
