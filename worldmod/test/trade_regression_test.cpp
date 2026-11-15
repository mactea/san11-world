#include "../src/trade_model.h"
#include <stdio.h>
#include <vector>
using namespace trade;
static int failures;
static void check(bool ok, const char* label) { printf("%s %s\n", ok ? "PASS" : "FAIL", label); failures += !ok; }
int main() {
    {
        Data goods;goods.C=1;goods.G=2;goods.goodName={"槍","絲"};goods.size={100};goods.flags={0};
        goods.supply={3,3};goods.price={180,180};goods.nativeUnits={100,0};goods.minValue={150,0};
        bool floor=true,split=true,roundTrip=true,ordinary=true;
        for(int index:{300,830,834,1000,4000})for(int event:{100,700,1000,1700})for(int pp:{0,106,2000}) {
            Market market;market.d=&goods;market.reset();market.at(0,0)=index;market.at(0,1)=index;market.mod={uint16_t(event),uint16_t(event)};
            floor &= market.quote(0,0,1,true,pp)>=151 && market.quote(0,0,1,false,pp)>=142;
            ordinary &= market.valueAt(0,1,index)==market.base(0,1)*index;
            auto stock=market.stock,idx=market.idx;auto quote=market.quote(0,0,10,true,pp);
            int64_t paid=0;for(int n=0;n<10;++n)paid+=market.buy(0,0,1,pp);
            split &= paid==quote;
            auto revenue=market.sell(0,0,10,pp);
            roundTrip &= revenue<paid && market.stock==stock && (index==4000 || market.idx==idx);
        }
        check(floor,"military price floor survives slump, events and maximum merchant discounts");
        check(split && roundTrip,"crossing the floor preserves split-order equality and prevents same-market round-trip profit");
        check(ordinary,"goods without a military anchor retain their original price formula");
    }
    Data d; d.C = 2; d.G = 2; d.size = { 600, 100 }; d.flags = { CITY_HUB, 0 };
    d.supply = { 3, 0, 0, 0 }; d.price = { 1981, 100, 2000, 120 }; d.demand.assign(4, 1000);
    d.lo = { 0, 20 }; d.hi = { 0, 20 }; d.group = { 1, 1 };
    Market m; m.d = &d; m.reset();
    check(m.available(0,0) == 600 && m.available(0,1) == 60 && m.available(1,0) == 0, "initial producer, hub and ordinary inventories");
    auto old = m.stock; auto idx = m.idx;
    check(m.buy(0,1,61) == -1 && m.stock == old && m.idx == idx, "oversized purchase is atomic");
    check(m.sell(1,0,201) == -1 && m.stock == old && m.idx == idx, "warehouse capacity rejects the whole order");
    for (int pp : { -50, 0, 10, 58, 106, 2000 }) for (int start : { 300, 1000, 4000 }) {
        m.reset(); m.at(0,0) = start;
        int64_t paid = m.buy(0,0,26,pp), revenue = 0;
        for (int i = 0; i < 26; i++) revenue += m.sell(0,0,1,pp);
        check(paid >= revenue && m.available(0,0) == 600, "same-market bulk buy / split sell cannot create money or cargo");
        m.reset(); m.at(0,0) = start;
        Market split = m;
        int64_t bulk = m.buy(0,0,26,pp), parts = 0;
        for (int i = 0; i < 26; i++) parts += split.buy(0,0,1,pp);
        check(bulk == parts && split.idx == m.idx, "splitting purchases preserves exact price and index");
    }
    m.reset(); m.p.revert = m.p.revertHub = 0; m.at(0,0) = 1100; m.month();
    check(m.at(0,0) == 1100, "zero reversion really disables reversion");
    check(m.available(0,0) == 648 && m.available(0,1) == 48 && m.available(1,0) == 0, "production minus consumption; imported goods never regenerate");
    check(m.sell(1,1,10) >= 0 && m.buy(1,1,5) >= 0 && m.available(1,1) == 5, "ordinary markets resell delivered imports");
    Info info; info.reset(2,d,12); m.reset(); info.see(0,0,&m);
    m.mod.assign(4,1600); m.at(0,0) = 1900; m.stock[0] = 1;
    Market seen; std::vector<char> known; info.view(m,0,&seen,&known);
    check(known[0] && seen.base(0,0) == 1981 && seen.at(0,0) == 1000 && seen.available(0,0) == 600, "observation freezes event, index and stock even within a month");
    info.endMonth(m,100); info.view(m,0,&seen,&known);
    check(known[0] && seen.base(0,0) == 1981, "old observation does not acquire new events");
    check(!known[1] && seen.mod[2] == 1000 && seen.available(1,0) == 0, "unknown market does not leak current stock or event multiplier");
    for(int n=0;n<13;n++) info.endMonth(m,101+n);
    info.view(m,0,&seen,&known); check(!known[0], "expired observations become unknown");
    Cargo cargo; cargo.add(0,0,10,101); int64_t basis, sum=0; int xp, learned=0;
    for(int n=0;n<10;n++){ check(cargo.take(0,1,1,100,&basis,&xp), "partial sale accepted"); sum+=basis; learned+=xp; }
    check(sum == 101 && learned == 100 && cargo.count() == 0, "partial sales conserve cost and trip experience");
    cargo.add(0,0,10,100); cargo.take(0,5,0,100,&basis,&xp);
    check(xp == 0 && basis == 50 && cargo.count()==5, "returning cargo to origin earns no experience");
    auto before = cargo.cost(); check(!cargo.take(0,6,1,100,&basis,&xp) && cargo.cost()==before, "overselling cargo is atomic");
    Ledger a; a.purchases=1000; a.receipts=800; a.fees=30; a.taxes=80; a.losses=100;
    check(a.cashNet()==-310 && a.profit(400)==90, "partial-sale ledger separates cash flow from remaining stock; losses not deducted twice");
    a.losses+=400;
    check(a.profit(0)==-310, "discarding remaining cargo realizes its cost without inventing a second cash expense");
    for (int discount : {100,92,44}) {
        int bulkCarry=0,splitCarry=0; int64_t splitTax=0;
        int64_t bulkTax=taxWithCarry(190,5,discount,&bulkCarry);
        for (int n=0;n<10;n++) splitTax+=taxWithCarry(19,5,discount,&splitCarry);
        check(bulkTax==splitTax && bulkCarry==splitCarry,"tax and fractional balance are invariant under split sales and merchant discounts");
    }
    int carry=0; check(taxWithCarry(95,5,100,&carry)==4 && carry==7500 && taxWithCarry(95,5,100,&carry)==5 && carry==5000,
        "two 95-gold sales collect nine gold tax instead of eight");
    Data rd; rd.C=2; rd.G=1; rd.size={100,100}; rd.flags={0,0}; rd.supply={3,0}; rd.price={10,20}; rd.lo={0,20}; rd.hi={0,20};
    Market rm; rm.d=&rd; rm.reset(); rm.p.impact=0; rm.stock[1]=192;
    Route route;
    check(bestRoute(rm,0,{1},4000,20,0,0,&route,nullptr,0,nullptr,100,30) && route.qty==8 && route.profit>0,
        "destination with room for eight loads accepts a profitable reduced cargo");
    check(route.cost==purchase(rm,0,0,route.qty,0)+30 && route.revenue==saleQuote(rm,1,0,route.qty,0),"reduced route totals match executable market quotes");
    auto routeStock=rm.stock, routeIdx=rm.idx;
    rm.stock[1]=0;
    for (int fee : {300,100,50,30}) {
        bool found=bestRoute(rm,0,{1},4000,20,0,25,&route,nullptr,0,nullptr,100,fee);
        printf("fee comparison: %d gold, viable %d, profit %lld\n",fee,found,(long long)(found?route.profit:0));
        if (fee==300) check(!found,"one-time setup cost makes this small trade unsuitable for a new caravan");
        if (fee==30) check(found && route.qty==20,"existing caravan can profitably reuse its capacity at the continuation charge");
    }
    rm.stock=routeStock;
    check(!bestRoute(rm,0,{1},10,20,0,0,&route,nullptr,0,nullptr,100,30) && rm.stock==routeStock && rm.idx==routeIdx,
        "route search respects total cash budget and does not alter markets");
    rm.stock[1]=199;
    check(bestRoute(rm,0,{1},4000,20,0,0,&route,nullptr,0,nullptr,100,0) && route.qty==1,"one remaining warehouse slot is usable");
    {
        Data bulk;bulk.C=2;bulk.G=2;bulk.size={100,100};bulk.flags={0,0};
        bulk.goodName={"鹽","蜀錦"};bulk.supply={3,3,0,0};bulk.price={10,100,30,200};
        Market market;market.d=&bulk;market.reset();market.p.impact=0;
        Cargo basket;int64_t total=0;
        auto stock=market.stock,index=market.idx;
        check(quoteOrder(market,0,{{0,50},{1,10}},20,0,10000,100,&basket,&total) && basket.count()==60 && basket.loadUnits(bulk)==200,
            "mixed salt and brocade share 20 cargo slots while carrying 60 lots");
        check(total==purchase(market,0,0,50,0)+purchase(market,0,1,10,0)+100 && market.stock==stock && market.idx==index,
            "basket quote charges one setup fee and never mutates markets");
        auto savedCost=total;auto savedCargo=basket.cost();
        check(!quoteOrder(market,0,{{0,51},{1,10}},20,0,10000,100,&basket,&total) && total==savedCost && basket.cost()==savedCargo,
            "overweight second item rejects whole basket without partial output");
        check(!quoteOrder(market,0,{{0,50},{1,10}},20,0,savedCost-1,100,&basket,&total),"shared budget includes every good and exactly one fee");
        check(!quoteOrder(market,0,{{0,1},{0,1}},20,0,10000,100,&basket,&total) &&
              !quoteOrder(market,0,{{0,1},{1,-1}},20,0,10000,100,&basket,&total) &&
              !quoteOrder(market,0,{{0,1},{2,1}},20,0,10000,100,&basket,&total),"duplicate, negative and invalid goods are rejected atomically");
        market.stock[1]=0;
        check(!quoteOrder(market,0,{{0,1},{1,1}},20,0,10000,100,&basket,&total) && basket.cost()==savedCargo,"unavailable second good cannot partially buy the first");
        check(transitCharge(1000,2,100,50,0)==20 && transitCharge(1000,1,100,50,20)==10 &&
              transitCharge(10000,2,100,50,30)==20 && transitCharge(10000,2,100,50,50)==0,
            "2 percent transit and 1 percent hub tax stop at the per-leg cap");
        check(transitCharge(1000,2,50,50,0)==10,"merchant discount also reduces transit charges");
    }
    printf("trade_regression_test: %d failures\n", failures); return failures ? 1 : 0;
}
