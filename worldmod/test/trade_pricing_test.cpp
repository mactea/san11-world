// Price/cost calibration using the same market implementation as the game DLL.
#include "../src/trade_model.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdio.h>
using namespace trade;
static bool read(const char* path,Data& data) {
    std::ifstream file(path,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    std::string error;return !bytes.empty() && data.load(bytes.data(),bytes.size(),&error);
}
int main(int argc,char** argv) {
    if(argc<3)return 2;
    Data before,data;if(!read(argv[1],before)||!read(argv[2],data)||before.C!=data.C||before.G!=72||data.G!=72)return 2;
    bool unchanged=true,floor=true,ordinary=true,lot=true;
    for(int g=0;g<72;++g) {
        bool military=g==4||g>=69;
        lot &= data.unitsPerLot(g)==(military ? 100u : 0u) && data.minValue[g]==(military ? 150u : 0u);
        unchanged &= data.goodName[g]==before.goodName[g] && data.loadUnits(g)==before.loadUnits(g);
        for(int c=0;c<data.C;++c) {
            size_t n=(size_t)c*72+g;
            unchanged &= data.supply[n]==before.supply[n] && data.demand[n]==before.demand[n];
            if(!military)unchanged &= data.price[n]==before.price[n];
        }
    }
    Market market;market.d=&data;market.reset();
    for(int g:{69,70,71,4}) {
        int64_t best=INT64_MAX,worst=0;int origin=-1;
        for(int c=0;c<data.C;++c)if(data.produces(c,g)) {
            int64_t quote=market.quote(c,g,10,true,0);
            if(quote<best){best=quote;origin=c;}worst=std::max(worst,quote);
        }
        printf("%s: 1000 units (10 lots) producer purchase %lld..%lld gold; cheapest %s; ordinary production comparison 350 gold; %.2fx..%.2fx\n",
               data.goodName[g].c_str(),best,worst,origin>=0 ? data.cityName[origin].c_str() : "none",best/350.0,worst/350.0);
        ordinary &= origin>=0 && best>=1400;
    }
    market.mod.assign(data.C*data.G,1000);
    int64_t lowestBuy=INT64_MAX,lowestSell=INT64_MAX;int cases=0;
    // Both endpoints of the full configured index range, event drops and strongest
    // discounts. Stock is a test fixture so even normally unavailable cities quote.
    for(int c=0;c<data.C;++c)for(int g:{4,69,70,71}) {
        size_t n=(size_t)c*data.G+g;market.stock[n]=data.stockLimit(c,g)/2;
        for(int index:{300,833,1000,4000})for(int event:{100,700,1000,1700})for(int discount:{0,106,2000}) {
            market.idx[n]=index;market.mod[n]=(uint16_t)event;
            auto buy=market.quote(c,g,1,true,discount),sell=market.quote(c,g,1,false,discount);
            floor &= buy>=151 && sell>=142 && buy>sell;
            lowestBuy=std::min(lowestBuy,buy);lowestSell=std::min(lowestSell,sell);++cases;
        }
    }
    printf("floor stress %d quotes: minimum buy %lld, sell %lld gold per 100 units before taxes/fees\n",cases,lowestBuy,lowestSell);
    printf("production comparison only: 700 gold per run with assumed outputs 1000/2000/3000 -> 70/35/23.33 gold per 100 units; not a fixed native yield\n");
    printf("trade_pricing_test: %s (unrelated prices %d, lot metadata %d, production advantage %d, stress floor %d)\n",
           unchanged&&lot&&ordinary&&floor ? "PASS" : "FAIL",unchanged,lot,ordinary,floor);
    return unchanged&&lot&&ordinary&&floor ? 0 : 1;
}
