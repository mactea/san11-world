// Included only by a /DTRADE_NATIVE_TEST build, inside trade.cpp's namespace.
// Run on an isolated game copy through the normal game-thread command queue.
std::string nativeInteractionProbe() {
    Lock lock;int city=-1,port=-1,force=-1;
    for(int c=0;c<g_data.C;++c)if(isPlayerForce(baseForce(c))) {city=c;force=baseForce(c);break;}
    for(int c=0;c<g_data.C;++c)if(c!=city && baseForce(c)>=0 && baseForce(c)!=force && stance(force,baseForce(c))>0) {port=c;break;}
    if(port<0)return "FAIL no accessible market";
    auto before=payloadBytes();std::vector<int> gold(g_data.C);
    for(int c=0;c<g_data.C;++c)if(base(c))gold[c]=baseGold(base(c));
    bool ok=true;std::string report;
    auto test=[&](bool pass,const char* name){ok&=pass;report+=(pass?"PASS ":"FAIL ");report+=name;report+="\n";};
    Caravan car={};car.force=force;car.origin=city;car.dest=car.port=port;car.state=DOCKED;car.flags=F_TEMP;
    std::vector<OrderLine> order;
    for(int g=0;g<g_data.G && order.size()<2;++g)if(g_mk.room(port,g)>2) {
        car.cargo.add(g,city,2,10);order.push_back({g,2});
    }
    if(order.size()!=2)return "FAIL no batch goods";
    auto q=quoteBatchSale(car,order);test(q.error.empty() && q.lines.size()==2,"batch quote contains both goods");
    auto untouched=payloadBytes();auto cargo=car.cargo.count();
    auto invalid=order;invalid[1].qty=3;
    test(!sellCargoBatch(car,invalid,q.net).empty() && payloadBytes()==untouched && car.cargo.count()==cargo,"invalid second item commits neither item");
    test(!sellCargoBatch(car,{order[0],order[0]},q.net).empty() && payloadBytes()==untouched,"duplicate good rejects before sale");
    test(!sellCargoBatch(car,order,q.net+1).empty() && payloadBytes()==untouched,"changed quote rejects without any writes");
    car.automatic.caravan=1;car.automatic.managed=true;
    test(!sellCargoBatch(car,order,q.net).empty() && payloadBytes()==untouched,"automatic caravan requires takeover");
    car.automatic=tradeauto::Voyage();
    auto error=sellCargoBatch(car,order,q.net);
    test(error.empty() && car.cargo.count()==0 && car.ledger.receipts-car.ledger.taxes==q.net,"batch receipt agrees with combined tax and reward quote");
    auto after=payloadBytes();
    test(!sellCargoBatch(car,order,q.net).empty() && payloadBytes()==after,"repeated sale cannot credit twice");
    int leader=-1;
    for(int i=0;i<g_cfg.PT-g_cfg.specials;++i) {
        auto p=personRec(WORLD,i);if(p && objValid(p) && vcall(p,0x40)==force) {leader=i;break;}
    }
    test(leader>=0,"historical merchant fixture is available");
    if(leader>=0) {
        car.flags=0;car.leader=leader;car.ledger=Ledger();
        for(auto l:order)car.cargo.add(l.good,city,l.qty,10);
        g_merch.erase(std::remove_if(g_merch.begin(),g_merch.end(),[&](const MerchRec& r){return r.person==leader;}),g_merch.end());
        g_merch.push_back({leader,g_mk.p.merchExp[0]-1,0});
        int level=leaderLevel(leader);auto priced=quoteBatchSale(car,order);
        auto sold=sellCargoBatch(car,order,priced.net);
        test(priced.error.empty() && sold.empty() && leaderLevel(leader)>level && car.ledger.receipts-car.ledger.taxes==priced.net,
             "merchant leveling after the batch cannot change confirmed item prices");
    }
    int cars=0;bool restored=parseBlock(before.data(),before.size(),TRAD_VERSION,&cars,true);
    for(int c=0;c<g_data.C;++c)if(base(c))addGold(PROC,base(c),gold[c]-baseGold(base(c)));
    test(restored && payloadBytes()==before,"probe restores original trade state");
    for(int c=0;c<g_data.C;++c)if(base(c))ok&=gold[c]==baseGold(base(c));
    return std::string(ok?"PASS":"FAIL")+" trade interaction native probe\n"+report;
}
bool nativeMerchantSalesProbe(int city,int force) {
    int port=-1,good=-1,pp,cp;leaderFactors(TEMP_POL,TEMP_INT,&pp,&cp);
    for(int c=0;c<g_data.C && port<0;++c)if(baseForce(c)>=0 && baseForce(c)!=force && stance(force,baseForce(c))>0)
        for(int g=0;g<g_data.G;++g)if(saleQuote(g_mk,c,g,1,pp)>=200) {port=c;good=g;break;}
    if(port<0) {g_log("merchant sales probe: no accessible foreign market fixture");return false;}
    auto before=payloadBytes();auto source=base(city),target=base(port);
    int sourceGold=baseGold(source),targetGold=baseGold(target);bool ok=true;
    auto pending=[&](int f) {int64_t n=0;for(auto& c:g_credit)if(c.force==f)n+=c.gold;return n;};
    for(int mode=0;mode<5;++mode) {
        bool automatic=mode==1,full=mode==2,loss=mode==3,sameCity=mode==4;
        if(full) {addGold(PROC,source,1000000);addGold(PROC,target,1000000);}
        int sg=baseGold(source),tg=baseGold(target),foreign=baseForce(port);
        auto sourcePending=pending(force),targetPending=pending(foreign);
        int64_t revenue=saleQuote(g_mk,port,good,1,pp),cost=loss?revenue+100:revenue/2;
        int rem=0;for(auto& r:g_saleTax)if(r.payer==force && r.receiver==foreign)rem=r.remainder;
        int64_t tax=stance(force,foreign)==2 ? 0 : taxWithCarry(revenue,taxPct(g_data,port,g_cfg.tax,g_cfg.taxHub),100,&rem);
        int64_t bonus=loss || sameCity ? 0 : (revenue-tax-cost)/5;
        Caravan car={};car.force=force;car.origin=city;car.dest=car.port=port;car.state=DOCKED;car.flags=F_TEMP;
        car.cargo.add(good,sameCity?port:city,1,cost);car.ledger.purchases=cost;
        car.automatic.caravan=automatic;car.automatic.fees.balance=automatic?tax:0;
        auto error=tradeCargo(car,good,1,false);
        g_log("merchant sales probe mode %d: error=%s revenue=%lld bonus=%lld receipts=%lld tax=%lld/%lld seller=%lld/%lld receiver=%lld/%lld advance=%lld",
            mode,error.c_str(),revenue,bonus,car.ledger.receipts,car.ledger.taxes,tax,
            (int64_t)baseGold(source)-sg+pending(force)-sourcePending,revenue+bonus-(automatic?0:tax),
            (int64_t)baseGold(target)-tg+pending(foreign)-targetPending,tax,car.automatic.fees.balance);
        ok &= error.empty() && car.cargo.count()==0 && car.ledger.receipts==revenue+bonus && car.ledger.taxes==tax &&
              baseGold(source)-sg+pending(force)-sourcePending==revenue+bonus-(automatic?0:tax) &&
              baseGold(target)-tg+pending(foreign)-targetPending==tax && car.automatic.fees.balance==0;
        auto settled=payloadBytes();auto receipts=car.ledger.receipts;
        ok &= !tradeCargo(car,good,1,false).empty() && car.ledger.receipts==receipts && settled==payloadBytes();
        addGold(PROC,source,sourceGold-baseGold(source));addGold(PROC,target,targetGold-baseGold(target));
        int cars=0;bool restored=parseBlock(before.data(),before.size(),TRAD_VERSION,&cars,true) && payloadBytes()==before;
        g_log("merchant sales probe mode %d: restore=%d cumulative=%d",mode,restored,ok);
        ok &= restored;
    }
    return ok && baseGold(source)==sourceGold && baseGold(target)==targetGold;
}
std::string nativeCityLimitProbe() {
    uint8_t* officer=nullptr;int city=-1,force=-1;
    for(int id=0;id<g_cfg.PT-g_cfg.specials;++id) {
        auto p=personRec(WORLD,id);if(!objValid(p) || *(int*)(p+0xa0)!=3)continue;
        int c=*(int*)(p+0x9c),f=vcall(p,0x40);
        if(c<0 || c>=g_data.C || baseForce(c)!=f || !isPlayerForce(f) || cityMerchantSkill(f,c))continue;
        officer=p;city=c;force=f;break;
    }
    if(!officer)return "FAIL requires a serving player officer in a city without merchant skill";
    auto before=payloadBytes();int savedSkill=*(int*)(officer+0xe8),savedLoc=*(int*)(officer+0x9c);
    typedef void(__thiscall* SetSkill)(void*,int);
    auto setSkill=(SetSkill)0x4899d0;
    bool ok=cityAutoLimit(force,city)==2 && cityAutoLimit(force,city,true)==1;
    int ordinaryFee=cityTradeFee(force,city,CARAVAN),ordinaryLeg=cityTradeFee(force,city,CARAVAN,true);
    setSkill(officer,SKILL_MERCHANT);
    ok &= *(int*)(officer+0xe8)==SKILL_MERCHANT && cityAutoLimit(force,city)==4 && cityAutoLimit(force,city,true)==2;
    ok &= cityTradeFee(force,city,CARAVAN)==merchantFee(ordinaryFee,true) &&
          cityTradeFee(force,city,CARAVAN,true)==merchantFee(ordinaryLeg,true) &&
          cityTradeFee(force,city,MERCHANT_SHIP)==240 && cityTradeFee(force,city,MERCHANT_SHIP,true)==48;
    buildModel(force);g_model.shipType=MERCHANT_SHIP;
    ok &= tradeui::quoteFee(g_model,city)==240;
    ok &= nativeMerchantSalesProbe(city,force);
    *(int*)(officer+0x9c)=g_data.C+45;
    ok &= cityAutoLimit(force,city)==2 && cityAutoLimit(force,city,true)==1;
    ok &= cityTradeFee(force,city,CARAVAN)==ordinaryFee && cityTradeFee(force,city,CARAVAN,true)==ordinaryLeg;
    *(int*)(officer+0x9c)=savedLoc;setSkill(officer,savedSkill);
    ok &= *(int*)(officer+0xe8)==savedSkill && before==payloadBytes();
    return std::string(ok?"PASS":"FAIL")+" native merchant skill: player 2->4->2, AI 1->2->1; city="+
        std::to_string(city)+" force="+std::to_string(force)+"; fees 80%, ship 240/48; profit bonus: manual, automatic, full treasury, loss, same-city and repeat-sale checks; original officer, gold and trade data restored";
}

std::string nativeSpecialPoolProbe(bool keep) {
    int before=freeSpecials(WORLD),origin=-1;
    for(int c=0;c<g_data.C;++c)if(baseForce(c)==1){origin=c;break;}
    if(g_cfg.specials!=300 || before<250 || origin<0)return "FAIL expanded pool with 250 free slots and player city required";
    int force=baseForce(origin),corps=vcall(base(origin),0x44),last=-1;bool ok=true;
    std::vector<std::pair<int,uint8_t*>> made;
    for(int i=0;i<200;++i) {
        auto p=tempNew(PROC,personRec(WORLD,g_cfg.tmpl),-1,-1);
        if(!p || !objValid(p)){ok=false;break;}
        int id=personIdOf(WORLD,p);made.push_back({id,p});last=id;
        setCorps(PROC,p,corps);*(int*)(p+0x98)=origin;*(int*)(p+0x9c)=origin;
        memset(p+4,0,40);p[4]=0xb0;p[5]=0xd3;markHiddenTemp(id,force);setStatus(PROC,p,3);
        const int abilities[5]={40,30,50,60,50};for(int j=0;j<5;++j)setAbility(p,j,abilities[j]);
        int year=today()/360+1;setBirth(p,year-30);setAppear(p,year-10);setDeath(p,std::min(year+60,353));
        ok &= hiddenTradePerson(p) && id>=g_cfg.PT-300 && id<g_cfg.PT;
    }
    ok &= made.size()==200 && last>=g_cfg.PT-200 && freeSpecials(WORLD)==before-200;
    if(!keep || !ok)for(auto& v:made)freeTradeTemp(v.second,v.first);
    if(!keep)ok &= freeSpecials(WORLD)==before;
    return std::string(ok?"PASS":"FAIL")+" native special allocation/hiding count="+std::to_string(made.size())+
        " last="+std::to_string(last)+" free before="+std::to_string(before)+" after="+std::to_string(freeSpecials(WORLD))+(keep?" kept for isolated save round trip":" released");
}

std::string nativeSpeedProbe(bool move) {
    Caravan* car=nullptr;
    for(auto& k:g_car) if(k.origin==7 && liveUnit(k)) {car=&k;break;}
    if(!car) return "FAIL requires a live caravan originating in Beihai";
    auto u=liveUnit(*car);auto before=payloadBytes();uint8_t unitBefore[0xf4];memcpy(unitBefore,u,sizeof unitBefore);
    std::string report;bool ok=true;
    for(int dest:{8,124,42}) {
        std::vector<uint32_t> slowPath,fastPath;int slowCost=0,fastCost=0;
        bool fast=marketRoute(u,dest,&fastPath,&fastCost);
        auto time=movementRouteTime(u,fastPath,g_s.hex20,g_s.W,g_s.H);
        // Use the same set of berth candidates for the baseline comparison.
        movementSetTradeUnits(nullptr);bool slow=marketRoute(u,dest,&slowPath,&slowCost);
        auto oldTime=movementRouteTime(u,slowPath,g_s.hex20,g_s.W,g_s.H);movementSetTradeUnits(tradeMovingUnit);
        bool pass=slow && fast && time.turns>=0 && oldTime.turns>=0 && time.turns<=oldTime.turns;
        ok &= pass;
        report+=std::string(pass?"PASS ":"FAIL ")+"destination "+std::to_string(dest)+" normal steps "+std::to_string(slowPath.size())+
            " cost "+std::to_string(slowCost)+" turns "+std::to_string(oldTime.turns)+"; trade steps "+std::to_string(fastPath.size())+
            " cost "+std::to_string(fastCost)+" turns "+std::to_string(time.turns)+" land "+std::to_string(time.land)+" water "+std::to_string(time.water)+"\n";
    }
    bool unchanged=before==payloadBytes() && !memcmp(unitBefore,u,sizeof unitBefore);
    ok &= unchanged;report+=std::string(unchanged?"PASS":"FAIL")+" route queries do not alter trade state or native unit\n";
    if(move) {
        car->dest=124;car->port=-1;car->state=TRAVELLING;car->flags&=~F_MANUAL;rebuildDirs();
        uint32_t start=pack(posOf(u)[0],posOf(u)[1]);u[0x40]&=~1;
        bool routed=issueOrder(*car,u)!=0xffffffff;auto path=car->route;
        bool moved=routed && execOrder(EXEC,u)!=0;
        uint32_t end=pack(posOf(u)[0],posOf(u)[1]);
        auto it=std::find(path.begin(),path.end(),end);int steps=it==path.end()?-1:int(it-path.begin());
        bool pass=moved && start!=end && steps>10;ok &= pass;
        report+=std::string(pass?"PASS":"FAIL")+" one native movement action advances "+std::to_string(steps)+" cached path steps; no save written\n";
    }
    return std::string(ok?"PASS":"FAIL")+" native caravan speed\n"+report;
}
std::string nativeRoutingProbe() {
    Caravan* target=nullptr;
    for (auto& k:g_car) if (k.state==DOCKED && k.origin==7 && k.port==8 && liveUnit(k)) { target=&k; break; }
    if (!target || g_car.size()!=1 || baseForce(7)!=target->force || baseForce(9)!=target->force)
        return "FAIL requires the docked Beihai/Xiapi checkpoint and owned Xiaopei";
    auto before=payloadBytes(); auto oldCars=g_car; auto oldPrices=g_data.price;
    auto oldRange=g_inRange[8]; auto oldParams=g_mk.p; auto oldCfg=g_cfg;
    Caravan& k=*target; uint8_t* u=liveUnit(k); uint8_t* home=base(7);
    uint8_t unitBefore[0xf4]; memcpy(unitBefore,u,sizeof unitBefore);
    int gold=baseGold(home); bool ok=true; std::string report;
    auto check=[&](bool pass,const char* message) { report+=std::string(pass?"PASS ":"FAIL ")+message+"\n"; ok &= pass; };
    const int good=13, source=8*g_data.G+good;
    g_inRange[8]={7,9}; g_mk.p.impact=0; g_mk.p.legFee=30; g_mk.p.info=1;
    if (g_mk.mod.empty()) g_mk.mod.assign(g_data.C*g_data.G,1000);
    for (int g=0;g<g_data.G;g++) g_mk.stock[8*g_data.G+g]=0;
    for (int c:{7,8,9}) {
        int i=c*g_data.G+good; g_mk.idx[i]=1000; g_mk.mod[i]=1000; g_mk.stock[i]=c==8?40:0;
        g_data.price[i]=c==8?10:c==7?100:500; g_info.see(k.force,c,&g_mk);
    }
    k.maxLoad=3; k.cargo.lots.clear(); syncCargo(k); k.ledger=Ledger();
    Route r; int leader=k.flags&F_TEMP?-1:k.leader;
    auto choose=[&](int budget) { return findRoute(k.force,k.port,leader,budget,&r,g_mk.p.legFee,&k); };
    check(choose(4000) && r.qty==3,"continuation uses the caravan's prepared load rather than a newly calculated capacity");
    g_mk.stock[source]=0; g_mk.idx[source]=300; g_info.see(k.force,8,&g_mk);
    g_mk.stock[source]=40; g_mk.idx[source]=1000;
    auto remoteBefore=g_info.observed[((size_t)k.force*g_data.C+9)*g_data.G+good];
    Market remoteView=g_mk; g_mk.idx[9*g_data.G+good]=2000; g_mk.stock[9*g_data.G+good]=g_data.size[9]*g_data.stockCap/100;
    int pp,cp; leaderOrTemp(k,&pp,&cp);
    bool fresh=choose(4000);
    check(fresh && r.qty==3 && r.cost==purchase(g_mk,8,good,r.qty,pp)+30,"current dock stock and price are refreshed before planning a purchase");
    auto remoteAfter=g_info.observed[((size_t)k.force*g_data.C+9)*g_data.G+good];
    check(fresh && r.dest==9 && r.revenue==saleQuote(remoteView,9,good,r.qty,pp) && !memcmp(&remoteBefore,&remoteAfter,sizeof remoteBefore),
        "refreshing the current dock does not reveal remote market changes");
    g_mk.idx[9*g_data.G+good]=1000; g_mk.stock[9*g_data.G+good]=0;
    g_info.see(k.force,8,&g_mk); // Keep subsequent path and budget tests independent of the refresh assertion.
    // An out-of-map market position produces no berth candidates, without entering native A* for it.
    int16_t* blocked=const_cast<int16_t*>(posOf(base(9))); int16_t savedBlocked[2]={blocked[0],blocked[1]};
    blocked[0]=blocked[1]=-100;
    LARGE_INTEGER started,ended,freq; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&started);
    bool found=choose(4000); QueryPerformanceCounter(&ended);
    check(found && r.dest==7,"unreachable best market falls back to a reachable profitable market");
    report+="fallback native planning ms "+std::to_string(1000.0*(ended.QuadPart-started.QuadPart)/freq.QuadPart)+"\n";
    int16_t* other=const_cast<int16_t*>(posOf(base(7))); int16_t savedOther[2]={other[0],other[1]};
    other[0]=other[1]=-100;
    check(!choose(4000),"no reachable market returns no plan");
    other[0]=savedOther[0]; other[1]=savedOther[1]; blocked[0]=savedBlocked[0]; blocked[1]=savedBlocked[1];
    int budget=(int)purchase(g_mk,8,good,2,pp)+30;
    check(!choose(30) && choose(budget) && r.qty==2 && r.cost==budget,"purchase planning reserves continuation fee within the available budget");
    check(baseGold(home)==gold && k.qty==0 && !memcmp(unitBefore,u,sizeof unitBefore),"route selection does not spend money, load cargo or change native orders");
    bool executable=choose(budget); int64_t quoted=executable?r.cost:0;
    k.flags&=~F_PLAYER; g_cfg.aiInvest=budget; g_cfg.aiReserve=gold-budget; g_cfg.aiMonth=0;
    if (executable) aiMonth();
    check(executable && k.state==TRAVELLING && k.qty==2 && baseGold(home)==gold-quoted && k.ledger.fees==30,
        "AI monthly continuation buys and departs at the quoted total without using the reserve");
    report+="AI purchase and fee "+std::to_string(gold-baseGold(home))+" budget "+std::to_string(budget)+" reserve "+std::to_string(g_cfg.aiReserve)+"\n";
    memcpy(u,unitBefore,sizeof unitBefore); addGold(PROC,home,gold-baseGold(home));
    g_data.price=oldPrices; g_inRange[8]=oldRange; g_mk.p=oldParams; g_cfg=oldCfg;
    int cars=0; bool restored=parseBlock(before.data(),before.size(),TRAD_VERSION,&cars,true);
    g_car=std::move(oldCars); rebuildDirs();
    check(restored && payloadBytes()==before && baseGold(home)==gold && !memcmp(unitBefore,u,sizeof unitBefore),
        "routing probe restores markets, cargo, money, native orders and trade state");
    return std::string(ok?"PASS":"FAIL")+" native AI routing\n"+report;
}
std::string nativeManualProbe() {
    for (auto& k:g_car) {
        uint8_t* u=liveUnit(k);
        if (!u || k.state!=TRAVELLING) continue;
        const int16_t* p=posOf(u);
        if (nearestCity(p[0],p[1],30)!=k.origin) continue;
        uint8_t* h=cell(p[0],p[1]);
        if (!h || (*(uint32_t*)(h+4)&0x1f)==8) continue;
        Caravan probe=k; probe.flags|=F_MANUAL; probe.dockHex=0xffffffff;
        probe.lastProgress=today();
        setOrder(EXEC,u,1,pack(p[0],p[1]),-1);
        uint8_t before[0xf4]; memcpy(before,u,sizeof before);
        caravanTurn(probe,today());
        bool preserved=!memcmp(before,u,sizeof before);
        bool cached=probe.dockHex!=0xffffffff && !probe.route.empty();
        setOrder(EXEC,u,1,pack(p[0],p[1]),-1);
        memcpy(before,u,sizeof before);
        uint32_t flags=k.flags; k.flags|=F_MANUAL;
        planHook(nullptr,nullptr,u);
        bool planPreserved=!memcmp(before,u,sizeof before);
        k.flags=flags;
        issueOrder(k,u);
        return std::string(preserved && cached && planPreserved?"PASS":"FAIL")+" manual order preserved after cold route cache; preserved="+
            std::to_string(preserved)+" cached="+std::to_string(cached)+" planner="+std::to_string(planPreserved);
    }
    return "FAIL requires a travelling caravan near its origin on land";
}
bool nativeEconomyCases(int origin,int port,uint8_t* u,int leader,std::string& report,bool checkpoint=false) {
    bool ok=true;
    auto check=[&](bool pass,const char* message) { report+=std::string(pass?"PASS ":"FAIL ")+message+"\n"; ok &= pass; };
    uint8_t* source=base(origin), *market=base(port);
    int force=baseForce(origin), foreign=baseForce(port), good=13;
    Caravan car={}; car.id=g_nextCar++; car.unit=unitIdOf(WORLD,u); car.leader=leader;
    car.force=force; car.corps=vcall(source,0x44); car.origin=origin; car.dest=port; car.port=port;
    car.state=DOCKED; car.flags=F_TEMP|F_PLAYER; car.maxLoad=30; car.good=good;
    car.born=car.lastProgress=today(); g_car.push_back(car); rebuildDirs();
    Caravan& k=g_car.back();
    int pp,cp; MerchantBonus mb; leaderOrTemp(k,&pp,&cp,&mb);
    int stock=g_mk.available(port,good), sourceGold=baseGold(source), foreignGold=baseGold(market);
    int64_t cost=purchase(g_mk,port,good,10,pp);
    std::string err=tradeCargo(k,good,10,true);
    check(err.empty() && cost>0 && baseGold(source)==sourceGold-cost && baseGold(market)==foreignGold &&
        g_mk.available(port,good)==stock-10 && k.cargo.count()==10 && k.ledger.purchases==cost,"foreign purchase: payer, stock, cargo and ledger");
    if (!err.empty()) return false;
    auto pending=[&](int f) { int64_t n=0; for(auto& c:g_credit) if(c.force==f) n+=c.gold; return n; };
    int fractional=0;
    for (auto& r:g_saleTax) if(r.payer==force && r.receiver==foreign) fractional=r.remainder;
    for (int capped=0;capped<2;capped++) {
        if (capped) { addGold(PROC,source,1000000); addGold(PROC,market,1000000); }
        int sg=baseGold(source), fg=baseGold(market);
        int64_t revenue=saleQuote(g_mk,port,good,5,pp);
        int64_t tax=taxWithCarry(revenue,taxPct(g_data,port,g_cfg.tax,g_cfg.taxHub),mb.taxPct,&fractional), net=revenue-tax;
        int64_t receipts=k.ledger.receipts, taxes=k.ledger.taxes;
        err=tradeCargo(k,good,5,false);
        report+="sale gross "+std::to_string(revenue)+" tax "+std::to_string(tax)+" capped "+std::to_string(capped)+"\n";
        check(err.empty() && tax>0 && k.cargo.count()==5*(1-capped) &&
            k.ledger.receipts==receipts+revenue && k.ledger.taxes==taxes+tax &&
            baseGold(source)-sg+pending(force)==net && baseGold(market)-fg+pending(foreign)==tax,
            capped?"full treasuries retain seller proceeds and foreign tax":"foreign sale pays seller and taxing force exactly");
        if (checkpoint && !capped) {
            report+="checkpoint voyage "+std::to_string(k.id)+" remaining "+std::to_string(k.qty)+" tax fraction "+std::to_string(fractional)+"\n";
            return ok;
        }
        if (capped) {
            check(pending(force)==net && pending(foreign)==tax,"both overflow credits retained");
            addGold(PROC,source,-(int)net); addGold(PROC,market,-(int)tax); settleCredits();
            check(baseGold(source)==sg && baseGold(market)==fg && g_credit.empty(),"credits settle after capacity is available");
            settleCredits(); check(baseGold(source)==sg && baseGold(market)==fg,"repeated settlement does not duplicate money");
        }
    }
    check(g_mk.available(port,good)==stock,"foreign market stock restored after selling purchased goods");
    err=tradeCargo(k,good,5,true);
    if (!err.empty()) { check(false,"prepare interception cargo"); return false; }
    const int16_t* pos=posOf(u); int nearby=nearestCity(pos[0],pos[1],1<<20);
    int64_t reward=cargoValue(k,nearby)*g_cfg.loot/100, basis=k.cargo.cost();
    int fg=baseGold(market);
    annihilateHook((void*)(uintptr_t)PROC,nullptr,u,market,0,0);
    report+="loot expected "+std::to_string(reward)+" credited "+std::to_string(pending(foreign))+"\n";
    check(reward>0 && baseGold(market)==fg && pending(foreign)==reward,"interception reward survives a full treasury");
    check(!objValid(u) && (k.flags&F_LOOTED),"intercepted temporary caravan removed immediately");
    check(k.state==FINISHED && InterlockedCompareExchange(&g_pausedLeader[k.unit],0,0)==0 && !g_unitDir[k.unit],"interception closes voyage and releases its movement lock immediately");
    caravanTurn(k,today());
    check(k.state==FINISHED && k.cargo.count()==0 && k.ledger.losses==basis,"intercepted cargo becomes a loss exactly once");
    if (pending(foreign)>0) { addGold(PROC,market,-(int)reward); settleCredits(); }
    check(baseGold(market)==fg && pending(foreign)==0,"interception credit settles once");
    int sg=baseGold(source);
    credit(force,origin,INT_MAX);
    check(baseGold(source)>=sg && pending(force)+baseGold(source)-sg==INT_MAX,"large pending payment does not overflow native treasury addition");
    int64_t owed=pending(force); auto saved=blockBytes(); int cars=0;
    check(!saved.empty() && parseBlock(saved.data()+12,saved.size()-12,TRAD_VERSION,&cars) && pending(force)==owed,
        "large unpaid balance survives the production save codec");
    bool taxRestored=false;
    for (auto& r:g_saleTax) if(r.payer==force && r.receiver==foreign) taxRestored=r.remainder==fractional;
    check(fractional==0 || taxRestored,"sales-tax fraction persists after the voyage ends and the save reloads");
    return ok;
}
std::string nativePortProbe(int origin, int port, int count, bool economy=false, bool checkpoint=false) {
    if (economy) {
        for (int c=0;c<g_data.C;c++) if (g_data.port(c) && base(c) && stance(baseForce(origin),baseForce(c))==1 && g_mk.available(c,13)>=10) { port=c; break; }
        if (port<0) return "FAIL no neutral foreign port with salt stock";
    }
    uint8_t* source=base(origin), *anchor=base(port);
    if (!source || !anchor || !g_data.port(port) || count<1 || count>3) return "FAIL invalid port probe arguments";
    int force=baseForce(origin), foreign=baseForce(port), corps=vcall(source,0x44);
    if (force<0 || foreign<0 || force==foreign) return "FAIL requires different regular forces";
    auto activeUnits=[]() { int n=0; for(int id=0;id<g_s.U;id++) n+=objValid(unitRec(WORLD,id))!=0; return n; };
    int unitsBefore=activeUnits(), goldBefore=baseGold(source);
    int freeBefore=freeSpecials(WORLD), troopsBefore=vcall(source,0x50), foodBefore=baseFood(source);
    int targetGold=baseGold(anchor), targetTroops=vcall(anchor,0x50), targetFood=baseFood(anchor);
    if (freeBefore<count+g_cfg.tempKeep || troopsBefore<count*500 || foodBefore<count*3000 || !g_credit.empty())
        return "FAIL insufficient test resources or pending credits";
    uint8_t* tmpl=personRec(WORLD,g_cfg.tmpl);
    if (!tmpl || !objValid(tmpl)) return "FAIL no template";
    auto before=payloadBytes();
    struct Created { uint8_t* person; uint8_t* unit; int troops,food; };
    std::vector<Created> created;
    bool ok=true; std::string report;
    for (int i=0;i<count;i++) {
        uint8_t* p=tempNew(PROC,tmpl,-1,-1);
        if (!p || !objValid(p)) { ok=false; break; }
        int leader=personIdOf(WORLD,p);
        *(int*)(p+0x98)=origin; setCorps(PROC,p,corps); setStatus(PROC,p,3);
        // Match createCaravan: the bandit template is immortal until setDeath clears that flag.
        memset(p+4,0,0x13-4+25); p[4]=0xb0; p[5]=0xd3;
        static const int abil[5]={40,30,50,60,50};
        for (int a=0;a<5;a++) setAbility(p,a,abil[a]);
        int year=today()/360+1;
        setBirth(p,year-30); setDeath(p,year+60>0x161?0x161:year+60); setAppear(p,year-10);
        ((void(__thiscall*)(void*))(*(void***)p)[0x34/4])(p);
        int paidTroops=-addTroops(PROC,source,-500), paidFood=-addFood(PROC,source,-3000);
        created.push_back({p,nullptr,paidTroops,paidFood});
        if (paidTroops!=500 || paidFood!=3000) { ok=false; break; }
        uint8_t rec[0x80]={}; *(void**)rec=anchor; *(int*)(rec+4)=1;
        *(int*)(rec+8)=leader; *(int*)(rec+0xc)=*(int*)(rec+0x10)=-1;
        *(int*)(rec+0x18)=3000; *(int*)(rec+0x1c)=500;
        for (int w=0;w<12;w++) *(int*)(rec+0x20+4*w)=-1;
        int id=sortie(CMDQ,rec);
        uint8_t* u=id>=0 && id<g_s.U ? unitRec(WORLD,id) : nullptr;
        if (!u || !objValid(u)) { ok=false; break; }
        created.back().unit=u;
        bool owned=vcall(u,0x40)==force && *(int*)(u+0xc)==leader && vcall(p,0x40)==force;
        bool funded=vcall(u,0x50)==500 && *(int*)(u+0x20)==3000;
        char line[256]; sprintf_s(line,"unit %d leader %d force %d expected %d troops %d food %d\n",
            id,leader,vcall(u,0x40),force,vcall(u,0x50),*(int*)(u+0x20)); report+=line;
        ok &= owned && funded;
        if (!ok) break;
        if (economy) { report+="economy origin "+std::to_string(origin)+" port "+std::to_string(port)+"\n"; ok &= nativeEconomyCases(origin,port,u,leader,report,checkpoint); }
        if (checkpoint) return std::string(ok?"PASS":"FAIL")+" tax checkpoint intentionally retained for save/reload\n"+report;
    }
    ok &= created.size()==(size_t)count;
    // Reclaim every created unit before returning; source reservations are returned exactly once.
    for (auto& c:created) {
        if (c.unit) { if (objValid(c.unit)) { unitTroopsSet(c.unit,0); unitFoodSet(c.unit,0); removeUnit(PROC,c.unit,0); } }
        else if (objValid(c.person)) tempFree(PROC,c.person);
        addTroops(PROC,source,c.troops); addFood(PROC,source,c.food);
    }
    if (economy) {
        addGold(PROC,source,goldBefore-baseGold(source)); addGold(PROC,anchor,targetGold-baseGold(anchor));
        int cars=0;
        ok &= parseBlock(before.data(),before.size(),TRAD_VERSION,&cars,true);
    }
    bool restored=activeUnits()==unitsBefore && baseGold(source)==goldBefore && freeSpecials(WORLD)==freeBefore && vcall(source,0x50)==troopsBefore && baseFood(source)==foodBefore &&
        baseGold(anchor)==targetGold && vcall(anchor,0x50)==targetTroops && baseFood(anchor)==targetFood && payloadBytes()==before;
    char detail[512]; sprintf_s(detail,"before/after units %d/%d free %d/%d source gold %d/%d troops %d/%d food %d/%d foreign gold %d/%d troops %d/%d food %d/%d trade %d\n",
        unitsBefore,activeUnits(),freeBefore,freeSpecials(WORLD),goldBefore,baseGold(source),troopsBefore,vcall(source,0x50),foodBefore,baseFood(source),
        targetGold,baseGold(anchor),targetTroops,vcall(anchor,0x50),targetFood,baseFood(anchor),payloadBytes()==before); report+=detail;
    for (auto& c:created) { sprintf_s(detail,"reclaimed person valid %d status %d location %d force %d\n",objValid(c.person),*(int*)(c.person+0xa0),*(int*)(c.person+0x9c),vcall(c.person,0x40)); report+=detail; }
    report+=(restored?"PASS":"FAIL"); report+=" source/foreign resources, temporary slots and trade state restored\n";
    return std::string(ok && restored?"PASS":"FAIL")+" foreign-port native sortie count "+std::to_string(count)+"\n"+report;
}
