// Real map paths, production movement costs and month-by-month monsoon calibration.
// No native movement or port transfer is executed. Arguments: trade.bin paths.txt results.json
#include "../src/movement.cpp"
#include "../src/trade_model.h"
#include <fstream>
#include <algorithm>
static trade::Data data;
static trade::Params params;
static const uint8_t* seaHex;
static int width, month0;
static bool __cdecl registered(const uint8_t*) {return true;}
static int __cdecl seasonal(const uint8_t*,const uint8_t* rec,uint32_t from,int cost) {
    size_t h=(rec-seaHex)/20;int x=int(h/width),y=int(h%width),fx=int16_t(from);
    return trade::seaStepCost(data,params,data.seaOf(x,y),month0,(x>fx)-(x<fx),cost);
}
int main(int argc,char** argv) {
    if(argc!=4) {printf("usage: sea_route_test trade.bin paths.txt results.json\n");return 2;}
    std::ifstream bin(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(bin)),{});
    std::string error;if(!data.load(bytes.data(),bytes.size(),&error)) {printf("%s\n",error.c_str());return 2;}
    std::ifstream input(argv[2]);int height,legs;
    if(!(input>>width>>height>>legs) || width!=1000 || height!=1800 || legs!=6) return 2;
    std::vector<uint8_t> hex(size_t(width)*height*20);
    std::vector<std::vector<uint32_t>> paths(legs);int total=0;
    for(auto& path:paths) {
        int count;if(!(input>>count) || count<2 || count>10000) return 2;
        for(int i=0;i<count;++i) {
            int x,y;if(!(input>>x>>y) || x<0 || x>=height || y<0 || y>=width) return 2;
            if(!path.empty() && trade::hexDist(int16_t(path.back()),int16_t(path.back()>>16),x,y)!=1) return 2;
            path.push_back(uint32_t(x)|(uint32_t(y)<<16));
            *(uint32_t*)(&hex[(size_t(x)*width+y)*20+4])=8;
        }
        total+=count-1;
    }
    uint8_t unit[0xf4]={},table[12*32],alt[1]={25},afloat[1]={1};
    memset(table,5,sizeof table);unit[0xcc]=36;*(int*)(unit+8)=1;
    g_table=(uint32_t)(uintptr_t)table;g_unit0=unit;g_units=1;g_alt=alt;g_afloat=afloat;
    seaHex=hex.data();movementSetTradeUnits(registered);movementSetSeaCost(seasonal,70);
    std::ofstream out(argv[3]);if(!out) return 2;
    // Five turns (50 days) is a scenario allowance for Egypt's transfer and handling,
    // not an implemented port rule or a measured land path. Also test longer waits.
    const int transferTurns=5;int failures=0;
    out<<"{\"native_validated\":false,\"sea_steps\":"<<total<<",\"transfer_allowance_turns\":"<<transferTurns<<",\"runs\":[";
    for(int reverse=0;reverse<2;++reverse) {
        auto voyage=paths;if(reverse) {std::reverse(voyage.begin(),voyage.end());for(auto& p:voyage)std::reverse(p.begin(),p.end());}
        for(int start=0;start<12;++start) {
            int turns=0,sailed=0;
            for(int li=0;li<legs;++li) {
                auto& path=voyage[li];size_t i=1;
                while(i<path.size()) {
                    month0=(start+turns/3)%12;int used=0,steps=0;
                    while(i<path.size() && steps<rangeCap(unit,true)) {
                        uint32_t p=path[i];const uint8_t* rec=seaHex+(size_t(uint16_t(p))*width+uint16_t(p>>16))*20;
                        int c=costForStep(unit,9,8,0,rec,path[i-1],36,25,1);
                        if(c<1 || c>36) return 3;
                        if(used+c>36) break;
                        used+=c;++steps;++i;++sailed;
                    }
                    ++turns;
                }
                if(li==(reverse?0:4)) turns+=transferTurns;
            }
            bool pass=sailed==total && turns>=54 && turns<=72;
            failures+=!pass;
            printf("%s %s departure %02d: %d turns (%.1f months), including %d assumed transfer turns\n",pass?"PASS":"FAIL",reverse?"eastbound":"westbound",start+1,turns,turns/3.0,transferTurns);
            if(reverse || start)out<<',';
            out<<"{\"direction\":\""<<(reverse?"eastbound":"westbound")<<"\",\"departure_month\":"<<start+1<<",\"turns\":"<<turns<<",\"with_6_month_delay_turns\":"<<turns+18<<",\"with_12_month_delay_turns\":"<<turns+36<<'}';
        }
    }
    out<<"],\"failures\":"<<failures<<'}';
    printf("sea_route_test: %d failures\n",failures);return failures?1:0;
}
