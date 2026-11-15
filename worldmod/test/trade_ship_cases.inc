// Production model, movement identity and serialization; no native game calls.
static void shipOfferCases() {
    auto prices=g_data.price;auto flags=g_data.flags;
    g_data.flags={CITY_PORT,CITY_PORT};g_data.price={10,20,500,600};
    tradeui::Model m;m.d=&g_data;m.live=g_mk;m.live.stock={100,100,0,0};m.seen=m.live;
    m.force=0;m.owner={0,1};m.stance={2,1};m.known={1,1};m.age={0,0};m.waterAccess={1,1};
    m.gold={20000,0};m.troops={5000,0};m.food={30000,0};m.grainRate={50,50};m.shipType=MERCHANT_SHIP;
    auto ship=tradeui::basketPlan(m,0,1,{{0,40},{1,20}},-1);
    check(ship.ok && ship.slots==60 && ship.used==600 && ship.cost==ship.goods+300,
          "merchant ship accepts a full mixed 60-slot cargo and charges one 300-gold formation fee");
    check(!tradeui::basketPlan(m,0,1,{{0,41},{1,20}},-1).ok,"ship rejects mixed cargo above its hold");
    m.merchantCities={1,0};
    auto discount=tradeui::basketPlan(m,0,1,{{0,40},{1,20}},-1);
    check(discount.ok && discount.goods==ship.goods && discount.cost==ship.cost-60 &&
          discount.revenue==ship.revenue && discount.merchantBonus>0 && discount.profit==ship.profit+60+discount.merchantBonus && discount.slots==ship.slots,
          "merchant city discounts fees and adds separately quoted profit bonus without changing prices, taxes or capacity");
    int64_t expectedBonus=0;int taxRemainder=0;
    for(auto line:std::vector<OrderLine>{{0,40},{1,20}}) {
        auto revenue=saleQuote(m.seen,1,line.good,line.qty,discount.pricePermille);
        auto duty=taxWithCarry(revenue,taxPct(g_data,1,m.cfg.tax,m.cfg.taxHub),100,&taxRemainder);
        expectedBonus+=merchantProfitBonus(revenue-duty,purchase(m.live,0,line.good,line.qty,discount.pricePermille),true);
    }
    check(discount.merchantBonus==expectedBonus && discount.duty==ship.duty,
          "mixed-cargo quote sums each commodity's after-tax bonus once without taxing the bonus");
    auto single=tradeui::plan(m,0,1,0,10,-1,false);
    auto singleBasket=tradeui::basketPlan(m,0,1,{{0,10}},-1);
    check(single.merchantBonus==singleBasket.merchantBonus && single.merchantBonus>0,
          "single-good and basket quote agree on after-tax rewards");
    Route ai;std::vector<int> rates={taxPct(g_data,1,m.cfg.tax,m.cfg.taxHub)};
    bool aiFound=bestRoute(m.live,0,{1},20000,20,m.cfg.tax,0,&ai,nullptr,discount.pricePermille,&rates,100,80,true);
    auto aiTax=taxOf(ai.revenue,rates[0],100);
    check(aiFound && ai.profit==ai.revenue-aiTax-ai.cost+merchantProfitBonus(ai.revenue-aiTax,ai.cost-80,true),
          "AI route ranking includes the same positive goods-margin reward and discounted fee");
    m.departureFee=48;
    check(tradeui::quoteFee(m,0)==48,"explicit continuation quote is never discounted twice");
    m.departureFee=-1;m.merchantCities.clear();
    m.shipType=CARAVAN;
    check(!tradeui::basketPlan(m,0,1,{{0,40},{1,20}},-1).ok && capacity(g_data,g_mk.p,0,1,100)==20,
          "ordinary caravans remain 20 slots even between ports");
    tradeauto::Plan p;p.force=0;p.origin=0;p.preferredDestination=1;p.shipType=MERCHANT_SHIP;
    p.budget.limit=20000;p.budget.advanceMonth(today()/30);
    auto ships=autoOffersModel(p,0,m,22000);
    check(!ships.empty() && ships[0].quote.slots==60 && ships[0].quote.cost==ships[0].quote.goods+300 &&
          ships[0].reserve==ships[0].quote.duty+ships[0].quote.transitLimit+60,
          "automatic ship quote reserves outbound costs and exactly one 60-gold return departure");
    m.merchantCities={1,0};
    auto reduced=autoOffersModel(p,0,m,22000);
    check(!reduced.empty() && reduced[0].quote.cost==reduced[0].quote.goods+240 &&
          reduced[0].reserve==reduced[0].quote.duty+reduced[0].quote.transitLimit+60,
          "automatic merchant formation costs 240 and reserves full future return fee against losing the organizer");
    m.merchantCities.clear();
    m.owner={1,0};p.origin=1;p.preferredDestination=0;
    auto returning=autoOffersModel(p,0,m,22000,1);
    check(!returning.empty() && returning[0].quote.slots==60 && returning[0].quote.cost==returning[0].quote.goods+60 &&
          returning[0].reserve==returning[0].quote.duty+returning[0].quote.transitLimit,
          "return ship cargo keeps 60 slots and pays continuation fee without another formation fee");
    m.merchantCities={0,1};
    auto returnDiscount=autoOffersModel(p,0,m,22000,1);
    check(!returnDiscount.empty() && returnDiscount[0].quote.cost==returnDiscount[0].quote.goods+48 && returnDiscount[0].quote.merchantBonus>0,
          "return cargo uses the original funding city's organizer, not the foreign port's officers");
    m.merchantCities={1,0};
    auto foreignOnly=autoOffersModel(p,0,m,22000,1);
    check(!foreignOnly.empty() && foreignOnly[0].quote.cost==foreignOnly[0].quote.goods+60 && foreignOnly[0].quote.merchantBonus==0,
          "foreign-market merchant cannot discount the funded voyage");
    m.merchantCities.clear();
    m.waterAccess[1]=0;
    check(autoOffersModel(p,0,m,22000,1).empty(),"ship auto planner rejects a port without a water berth");
    m.waterAccess[1]=1;g_data.flags[1]=0;
    check(!autoOffersModel(p,0,m,22000,1).empty(),"ship auto planner accepts a water berth without a port flag");
    m.shipType=MERCHANT_SHIP;
    m.owner={0,1};
    check(tradeui::basketPlan(m,0,1,{{0,1}},-1).ok,"manual ship formation does not require a port flag");
    m.owner={1,0};
    auto access=m.waterAccess;m.waterAccess.clear();
    check(!tradeui::shipMarket(m,0) && autoOffersModel(p,0,m,22000,1).empty(),
          "missing live water evidence cannot substitute an old port flag for ship access");
    m.waterAccess=access;
    p.shipType=2;
    check(autoOffersModel(p,0,m,22000,1).empty(),"undefined transport cannot enter automatic planning");
    check(transportFee(g_mk.p,CARAVAN)==100 && transportFee(g_mk.p,CARAVAN,true)==30 &&
          capacity(g_data,g_mk.p,0,1,125,MERCHANT_SHIP)==75,"ordinary fees and leader capacity bonus are preserved");
    check(merchantFee(100,true)==80 && merchantFee(30,true)==24 && merchantFee(60,true)==48 &&
          merchantFee(1,true)==1 && merchantFee(0,true)==0 && merchantFee(101,true)==81 && merchantFee(100,false)==100,
          "merchant fee rounding preserves zero and positive fees and ordinary fees without the skill");
    g_data.price=prices;g_data.flags=flags;
}
static void merchantProfitCases() {
    check(merchantProfitBonus(2000,1000,true)==200 && merchantProfitBonus(999,1000,true)==0 &&
          merchantProfitBonus(1004,1000,true)==0 && merchantProfitBonus(1005,1000,true)==1 &&
          merchantProfitBonus(2000,1000,false)==0,"profit reward is twenty percent, rounds down and never rewards losses or missing skill");
    Cargo cargo;cargo.add(0,0,10,1000);cargo.add(0,1,10,2000);
    check(cargo.saleBonus(0,10,1,2000,true)==200 && cargo.saleBonus(0,20,1,6000,true)==400 &&
          cargo.saleBonus(0,20,1,2999,true)==0 && cargo.saleBonus(0,21,1,6000,true)==0,
          "FIFO sale reward excludes same-city lots, caps by total positive margin and rejects overselling");
    int64_t basis;int xp;cargo.take(0,10,1,0,&basis,&xp);
    check(basis==1000 && cargo.saleBonus(0,10,1,6000,true)==0,"same-city remaining cargo cannot farm rewards");
    Cargo full;full.add(0,0,10,1003);auto whole=full.saleBonus(0,10,1,2000,true);auto split=full;
    int64_t rewards=0;
    for(int i=0;i<10;++i) {rewards+=split.saleBonus(0,1,1,200,true);split.take(0,1,1,0,&basis,&xp);}
    check(rewards<=whole && split.saleBonus(0,1,1,200,true)==0,"splitting a constant-price sale cannot increase reward or repeat it after cargo is gone");
    auto market=g_mk;int pp,cp;leaderFactors(TEMP_POL,TEMP_INT,&pp,&cp);
    market.p.impact=0;
    for(int q:{1,10,20}) {
        auto paid=purchase(market,0,0,q,pp);market.buy(0,0,q,pp);
        auto sale=saleQuote(market,0,0,q,pp);Cargo local;local.add(0,0,q,paid);
        check(sale<=paid && local.saleBonus(0,q,0,sale,true)==0,"same-market round trip stays unprofitable with merchant skill");
    }
}
static const int16_t* __fastcall shipTestPosition(void* u,void*) { return (const int16_t*)((uint8_t*)u+0xe0); }
static uint8_t* __fastcall shipTestBase(uint32_t,void*,int) { return nullptr; }
static void shipIdentityCases(uint8_t* units) {
    auto savedSetup=g_s;auto saved=g_car[0];
    uint8_t hex[5*5*20]={};g_s.W=5;g_s.H=5;g_s.hex20=hex;
    *(uint32_t*)(cell(1,0)+4)=8;*(uint32_t*)(cell(2,0)+4)=7;
    void* vt[16]={};vt[15]=(void*)shipTestPosition;*(void**)(units+0xf4)=vt;
    *(uint32_t*)(units+0xf4+0xe0)=pack(0,0);
    g_car[0].shipType=MERCHANT_SHIP;rebuildDirs();
    check(merchantShip(units+0xf4) && !merchantShip(units),"merchant identity applies only to the registered unit");
    check(shipStepAllowed(units+0xf4,cell(0,0)) && shipStepAllowed(units+0xf4,cell(1,0)) &&
          shipStepAllowed(units+0xf4,cell(2,0)) && !shipStepAllowed(units+0xf4,cell(3,0)),
          "ship can leave its initial quay onto sea or river, but cannot walk beyond the port apron");
    check(shipStepAllowed(units+0xf4,cell(0,1)),"initial departure permits the adjacent land apron used by two-hex coastal ports");
    *(uint32_t*)(units+0xf4+0xe0)=pack(1,0);
    check(!shipStepAllowed(units+0xf4,cell(0,0)),"once afloat the original land quay is no longer a landing exception");
    check(shipWaterPath({pack(0,0),pack(1,0),pack(2,0)},0) &&
          !shipWaterPath({pack(0,0),pack(3,0),pack(2,0)},0) && !shipWaterPath({pack(1,0),pack(3,0)},0) &&
          !shipWaterPath({},0) && !shipWaterPath({pack(0,0)},0) && shipWaterPath({pack(1,0)},0),
          "full path check rejects land shortcuts and land arrival, including zero-step boundary cases");
    check(shipWaterPath({pack(0,0),pack(0,1),pack(1,0)},0) &&
          !shipWaterPath({pack(0,0),pack(1,0),pack(0,1),pack(2,0)},0),
          "port apron is allowed only before first entering water, never as a later land shortcut");
    *(uint32_t*)(cell(1,0)+4)=0;*(uint32_t*)(cell(2,0)+4)=0;*(uint32_t*)(cell(3,0)+4)=8;
    *(uint32_t*)(units+0xf4+0xe0)=pack(0,0);
    check(!(g_data.flags[0]&CITY_PORT) && shipPort(0),"unmarked city can form ships with water exactly three hexes away");
    check(!shipPort(-1) && !shipPort(g_data.C),"invalid cities have no ship access");
    check(shipStepAllowed(units+0xf4,cell(2,0)) && !shipStepAllowed(units+0xf4,cell(3,1)),
          "initial land apron reaches two hexes but cannot extend to three");
    check(shipWaterPath({pack(0,0),pack(1,0),pack(2,0),pack(3,0)},0) &&
          !shipWaterPath({pack(0,0),pack(1,0),pack(2,0),pack(3,1),pack(3,0)},0),
          "three-hex embarkation accepts two land steps and rejects a longer land prefix");
    *(uint32_t*)(units+0xf4+0xe0)=pack(3,0);
    check(!shipStepAllowed(units+0xf4,cell(2,0)),"three-hex access never lets an afloat ship return to land");
    *(uint32_t*)(cell(3,0)+4)=0;*(uint32_t*)(cell(4,0)+4)=7;
    check(!shipPort(0),"water four hexes away does not qualify for ship formation");
    *(uint32_t*)(cell(3,0)+4)=7;
    check(shipPort(0),"river water within three hexes also qualifies");
    std::vector<uint32_t> path;int cost=0;
    check(marketRoute(units+0xf4,0,&path,&cost) && path.back()==pack(3,0),
          "ship destination search includes a water berth exactly three hexes from the city");
    *(uint32_t*)(cell(3,0)+4)=0;
    check(!marketRoute(units+0xf4,0,&path,&cost),"ship destination search excludes a water berth four hexes away");
    *(int*)(units+0xf4+0xc)=21;
    check(!merchantShip(units+0xf4) && shipStepAllowed(units+0xf4,cell(3,0)),"reused unit with a new leader cannot inherit ship constraints");
    *(int*)(units+0xf4+0xc)=20;
    g_car[0].tempReleased=true;rebuildDirs();
    check(!merchantShip(units+0xf4),"released merchant cannot constrain a reused native unit slot");
    g_car[0]=saved;g_s=savedSetup;*(void**)(units+0xf4)=nullptr;rebuildDirs();
}
static void shipSaveCases() {
    auto before=payloadBytes();int cars=0;
    tradeauto::Plan p;p.origin=0;p.force=0;p.preferredDestination=1;p.budget.month=today()/30;
    p.shipType=MERCHANT_SHIP;g_autoPlans={p};g_car[0].shipType=MERCHANT_SHIP;g_car[0].maxLoad=60;
    auto full=payloadBytes(true,true,true,true,true,false),block=compressBlock(full,11);g_car[0].shipType=CARAVAN;g_autoPlans[0].shipType=CARAVAN;
    check(parseBlock(block.data()+12,block.size()-12,11,&cars) && payloadBytes(true,true,true,true,true,false)==full &&
          g_car[0].shipType==MERCHANT_SHIP && g_car[0].maxLoad==60 && g_autoPlans[0].shipType==MERCHANT_SHIP,
          "v11 round trip retains both voyage and plan ship type and hold capacity");
    for(size_t offset:{full.size()-4,full.size()-8}) {
        auto damaged=full;int invalid=2;memcpy(damaged.data()+offset,&invalid,4);
        check(!parseBlock(damaged.data(),damaged.size(),11,&cars,true) && payloadBytes(true,true,true,true,true,false)==full,
              "invalid ship type rejects the entire save atomically");
    }
    check(!parseBlock(full.data(),full.size()-1,11,&cars,true) && payloadBytes(true,true,true,true,true,false)==full,
          "truncated ship extension cannot partially overwrite current state");
    // A real v10 save had no dedicated ships; serialize a normal-sized legacy caravan.
    g_car[0].shipType=CARAVAN;g_car[0].maxLoad=20;g_autoPlans[0].shipType=CARAVAN;
    auto old=payloadBytes(true,true,true,true,false);
    g_car[0].shipType=MERCHANT_SHIP;g_autoPlans[0].shipType=MERCHANT_SHIP;
    check(parseBlock(old.data(),old.size(),10,&cars,true) && g_car[0].shipType==CARAVAN &&
          g_autoPlans[0].shipType==CARAVAN && g_car[0].maxLoad==20,
          "v10 migration defaults old caravans and plans to ordinary transport without enlarging cargo");
    check(parseBlock(before.data(),before.size(),TRAD_VERSION,&cars,true),"ship fixture restores prior runtime state");
}
