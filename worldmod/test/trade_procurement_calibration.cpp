// Offline proposal only. Does not change the catalogue, DLL, or game saves.
#include "../src/trade_model.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdio.h>
using namespace trade;

static int failures;
static void check(bool ok,const char* label) {
    printf("%s %s\n",ok ? "PASS" : "FAIL",label);failures+=!ok;
}
static int nativeFood(int g) { return g==17 ? 150 : 100; }
static int64_t nativeSale(int food,int rate,int politics) {
    return (int64_t)food*3200/(rate*(450-politics));
}
// Conservative upper bound: any value of the effective-politics byte (0..255).
// A live implementation would need a validated nonzero current city rate.
static uint32_t foodFloor(int units,int rate) {
    int denominator=rate*(450-255);
    return (units*3200+denominator-1)/denominator;
}
static int flow(int month,int numerator,int denominator) {
    return month*numerator/denominator-(month-1)*numerator/denominator;
}
static int initial(const Data& d,int c,int g) {
    return d.size[c]*(d.produces(c,g) ? 20 : d.hub(c) ? 2 : 0)/100;
}
static int proposedMonth(const Data& d,int c,int g,int stock,int month) {
    int n=c*d.G+g;
    int consumed=flow(month,d.size[c]*d.demand[n],250000);
    int produced=flow(month,d.size[c]*d.supply[n],150);
    int remaining=std::max(0,stock-consumed),cap=d.size[c]*40/100;
    // Old saves may exceed the new soft cap. Consume normally, suspend new
    // production, and never erase legacy inventory merely because of an upgrade.
    return stock>cap ? remaining : std::min(cap,remaining+produced);
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    Data d;std::ifstream input(argv[1],std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    std::string error;if(bytes.empty()||!d.load(bytes.data(),bytes.size(),&error)||d.G!=72||d.C!=530)return 2;
    Market market;market.d=&d;market.reset();
    printf("PROPOSAL: military initial 20%% producer / 2%% hub; cap 40%%; production /150; consumption /250. Fractional flows use cumulative month rounding.\n");
    bool stocks=true,fractions=true;
    for(int c=0;c<d.C;++c)for(int g:{4,69,70,71}) {
        int stock=initial(d,c,g),produced=0,consumed=0;
        for(int month=1;month<=120;++month) {
            int before=stock;stock=proposedMonth(d,c,g,stock,month);
            stocks &= stock>=0 && stock<=d.size[c]*40/100;
            if(!d.produces(c,g))stocks &= stock<=before;
            produced+=flow(month,d.size[c]*d.supply[c*d.G+g],150);
            consumed+=flow(month,d.size[c]*d.demand[c*d.G+g],250000);
        }
        fractions &= produced==120*d.size[c]*d.supply[c*d.G+g]/150;
        fractions &= consumed==120*d.size[c]*d.demand[c*d.G+g]/250000;
    }
    check(stocks,"530 cities x 4 military goods x 120 months: bounded stock; imports do not regenerate");
    check(fractions,"fractional production/consumption survives small markets without cumulative rounding loss");
    bool legacy=true;
    for(int c=0;c<d.C;++c)for(int g:{4,69,70,71}) {
        int stock=d.size[c]*2;
        for(int month=1;month<=120;++month) {
            int before=stock;stock=proposedMonth(d,c,g,stock,month);
            if(before>d.size[c]*40/100)
                legacy &= stock==std::max(0,before-flow(month,d.size[c]*d.demand[c*d.G+g],250000));
            else legacy &= stock<=d.size[c]*40/100;
        }
    }
    check(legacy,"legacy over-cap stock is consumed gradually without confiscation or extra production");
    bool implemented=true;
    for(bool oldSave:{false,true}) {
        Market actual;actual.d=&d;actual.reset();auto expected=actual.stock;
        for(int c=0;c<d.C;++c)for(int g:{4,69,70,71}) {
            implemented &= actual.available(c,g)==initial(d,c,g);
            if(oldSave)actual.stock[c*d.G+g]=expected[c*d.G+g]=d.size[c]*2;
        }
        for(int month=1;month<=120;++month) {
            actual.month(month-1);
            for(int c=0;c<d.C;++c)for(int g:{4,69,70,71}) {
                int n=c*d.G+g;expected[n]=proposedMonth(d,c,g,expected[n],month);
                implemented &= expected[n]==actual.stock[n] && actual.room(c,g)==std::max(0,d.size[c]*40/100-expected[n]);
            }
        }
    }
    check(implemented,"live Market implementation matches calibrated new-game and legacy inventory over 120 months");
    for(int c:{15,39,110})for(int g:{69,71,4})if(d.produces(c,g)) {
        int stock=initial(d,c,g);for(int month=1;month<=24;++month)stock=proposedMonth(d,c,g,stock,month);
        printf("MILITARY %s %s: initial %d -> %d units; cap %d -> %d; gross production/year %d; consumption/year %d; stock after24 months %d\n",
            d.cityName[c].c_str(),d.goodName[g].c_str(),d.size[c]*100,initial(d,c,g)*100,
            d.size[c]*2*100,d.size[c]*40,12*d.size[c]*d.supply[c*d.G+g]/150*100,
            12*d.size[c]*d.demand[c*d.G+g]/250000*100,stock*100);
    }
    // Baseline counterexample. Purchase at the current local market, convert,
    // then sell the resulting food through the native quote at rate50/politics100.
    int unsafe=0;
    for(int g:{14,15,16,17}) {
        int city=-1;int64_t best=INT64_MAX;
        for(int c=0;c<d.C;++c)if(d.produces(c,g)) {
            auto price=market.quote(c,g,20,true);
            if(price>=0 && price<best){best=price;city=c;}
        }
        auto receipt=nativeSale(20*nativeFood(g),50,100);
        unsafe+=receipt>best;
        printf("UNSAFE_BASELINE %s at %s: 20 lots cost %lld; proposed food %d; native sale %lld; difference %+lld (native action/UI limits excluded)\n",
            d.goodName[g].c_str(),d.cityName[city].c_str(),best,20*nativeFood(g),receipt,receipt-best);
    }
    check(unsafe==4,"detects unsafe direct grain conversions for all four proposed food goods");
    Data guarded=d;Market candidate;candidate.d=&guarded;candidate.reset();candidate.mod.assign(d.C*d.G,1000);
    bool safe=true,split=true;int cases=0;
    // Shared market value floor, not an extra fee on the city procurement screen.
    // It is recalculated for each hypothetical city's rate in this offline audit.
    for(int c=0;c<d.C;++c)for(int g:{14,15,16,17})for(int rate:{30,50,80}) {
        guarded.minValue[g]=foodFloor(nativeFood(g),rate);
        int n=c*d.G+g;candidate.stock[n]=d.size[c];
        for(int index:{300,1000,4000})for(int event:{200,1000,5000})for(int pp:{0,106,2000})for(int q:{1,10,20}) {
            candidate.idx[n]=index;candidate.mod[n]=(uint16_t)event;
            auto cost=candidate.quote(c,g,q,true,pp);
            safe &= cost>=nativeSale(q*nativeFood(g),rate,255) && cost>0;++cases;
        }
    }
    // Sweep the full nonzero rate-byte range and every politics value, proving
    // the floor covers higher-politics sellers than the purchaser.
    for(int rate=1;rate<=255;++rate)for(int p=0;p<=255;++p)for(int g:{14,15,16,17}) {
        guarded.minValue[g]=foodFloor(nativeFood(g),rate);
        candidate.idx[g]=300;candidate.mod[g]=200;candidate.stock[g]=d.size[0];
        auto cost=candidate.quote(0,g,20,true,2000);
        safe &= cost>=nativeSale(20*nativeFood(g),rate,p);++cases;
    }
    for(int g:{14,15,16,17}) {
        guarded.minValue[g]=foodFloor(nativeFood(g),50);candidate.idx[g]=300;candidate.mod[g]=200;candidate.stock[g]=d.size[0];
        auto bulk=candidate.quote(0,g,20,true,106);int64_t parts=0;
        for(int j=0;j<20;++j)parts+=candidate.buy(0,g,1,106);
        split &= bulk==parts;
        printf("SAFE_FLOOR_CANDIDATE %s: units/lot=%d; pre-spread floor at rate50=%u; 20-lot maximum-discount/slump quote=%lld\n",
            d.goodName[g].c_str(),nativeFood(g),guarded.minValue[g],bulk);
    }
    check(safe,"hypothetical shared native-linked floor prevents immediate native resale profit");
    check(split,"shared-floor procurement is invariant under order splitting");
    printf("grain quote cases=%d; NOTE: guard is a proposal, not active in the game; native buy channel and future rate changes remain separate design gates.\n",cases);
    printf("trade_procurement_calibration: %s (%d failures)\n",failures ? "FAIL" : "PASS",failures);
    return failures ? 1 : 0;
}
