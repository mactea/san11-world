// Offline, independent route benchmark using the production market and basket planner.
// Usage: trade_routes_sim.exe trade.bin market-snapshot.txt output.json [seed] [origin:dest,...]
#include "../src/trade_panel.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <set>
#include <climits>
using namespace trade;
using namespace tradeui;
static void require(bool b,const char* why) { if(!b) throw std::runtime_error(why); }
static std::string js(const std::string& s) {
    std::string r="\"";for(char c:s) {if(c=='"'||c=='\\') r+='\\';r+=c;}return r+'"';
}
static std::vector<OrderLine> singleOrder(const Model& m,int a,int b) {
    std::vector<OrderLine> best;int64_t profit=LLONG_MIN;
    for(int g=0;g<m.d->G;++g) {
        int maxq=std::min(maxOrderQuantity(m,a,b,{},g,-1),m.seen.room(b,g));
        int chosen=0;int64_t gain=LLONG_MIN;
        for(int q=1;q<=maxq;++q) {
            auto p=plan(m,a,b,g,q,-1,false);
            if(p.ok && p.quoted && p.profit>gain) {gain=p.profit;chosen=q;}
        }
        if(!chosen) continue;
        std::vector<OrderLine> order={{g,chosen}};auto p=basketPlan(m,a,b,order,-1);
        if(p.ok && p.quoted && p.profit>profit) {best=order;profit=p.profit;}
    }
    return best;
}
int main(int argc,char** argv) {try {
    require(argc>=4,"usage: trade_routes_sim trade.bin snapshot.txt output.json [seed]");
    std::ifstream bin(argv[1],std::ios::binary);require(bool(bin),"missing trade.bin");
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(bin)),{});
    Data d;std::string err;require(d.load(bytes.data(),bytes.size(),&err),err.c_str());
    Model m;m.d=&d;m.live.d=&d;m.live.reset();m.owner.assign(d.C,-1);
    m.gold.assign(d.C,4000);m.troops.assign(d.C,500);m.food.assign(d.C,3000);
    m.known.assign(d.C,1);m.age.assign(d.C,0);m.stance.assign(d.C,1);
    std::vector<int> savedGold(d.C),savedTroops(d.C),savedFood(d.C);std::set<int> loaded;
    std::ifstream snapshot(argv[2]);require(bool(snapshot),"missing snapshot");
    std::string line;while(std::getline(snapshot,line)) {
        std::istringstream s(line);int c;std::string name,flags;
        if(!(s>>c>>name) || c<0 || c>=d.C || name!=d.cityName[c]) continue;
        require(loaded.insert(c).second,"duplicate city");s>>m.owner[c]>>flags;
        for(int g=0;g<d.G;++g) {
            std::string v;s>>v;int price,index,stock;
            require(sscanf_s(v.c_str(),"%d/%d/%d",&price,&index,&stock)==3,"bad market cell");
            m.live.at(c,g)=index;m.live.stock[c*d.G+g]=stock;
            require(m.live.buyPrice(c,g)==price,"snapshot/config price mismatch");
        }
        std::string gold,troops,food;s>>gold>>troops>>food;
        savedGold[c]=std::stoi(gold.substr(5));savedTroops[c]=std::stoi(troops.substr(7));savedFood[c]=std::stoi(food.substr(5));
    }
    require(loaded.size()==size_t(d.C),"incomplete snapshot");m.seen=m.live;
    const int upper[]={30,80,150,300};
    std::vector<std::pair<int,int>> pools[4];
    for(int a=0;a<d.C;++a) {
        bool available=false;for(int g=0;g<d.G;++g) available|=m.live.available(a,g)>0;
        if(m.owner[a]<0 || !available) continue;
        for(int b=0;b<d.C;++b) if(a!=b && m.owner[b]>=0) {
            int dist=hexDist(d.lo[a],d.hi[a],d.lo[b],d.hi[b]);
            for(int k=0;k<4;++k) if(dist<=upper[k]) {pools[k].push_back({a,b});break;}
        }
    }
    bool explicitRoutes=argc>5;int expected=100;
    if(explicitRoutes) {
        for(auto& pool:pools) pool.clear();
        std::istringstream pairs(argv[5]);std::string pair;std::set<std::pair<int,int>> unique;
        while(std::getline(pairs,pair,',')) {
            int a,b;require(sscanf_s(pair.c_str(),"%d:%d",&a,&b)==2,"bad route pair");
            require(a>=0 && a<d.C && b>=0 && b<d.C && a!=b,"invalid route cities");
            require(unique.insert({a,b}).second,"duplicate explicit route");pools[0].push_back({a,b});
        }
        expected=int(pools[0].size());require(expected>0,"empty route list");
    }
    uint32_t seed=argc>4?uint32_t(std::stoul(argv[4])):20261101;Rng rng;rng.s=seed;
    std::ofstream out(argv[3],std::ios::binary);require(bool(out),"cannot write output");
    out<<"{\"seed\":"<<seed<<",\"cities\":"<<d.C<<",\"goods\":"<<d.G<<",\"budget\":4000,\"phase\":1,\"pool_sizes\":[";
    for(int k=0;k<4;++k) {if(k)out<<',';out<<pools[k].size();}out<<"],\"routes\":[\n";
    int id=0;
    for(int k=0;k<4;++k) {
        auto& pool=pools[k];int count=explicitRoutes?int(pool.size()):25;
        require(pool.size()>=size_t(count),"distance stratum too small");
        if(!explicitRoutes) for(size_t n=pool.size();n>1;--n) std::swap(pool[n-1],pool[rng.next()%n]);
        for(int n=0;n<count;++n) {
            int a=pool[n].first,b=pool[n].second;m.force=m.owner[a];
            for(int c=0;c<d.C;++c) m.stance[c]=m.owner[c]<0?-1:m.owner[c]==m.force?2:1;
            auto single=singleOrder(m,a,b),order=recommendOrder(m,a,b,-1);bool recommended=!order.empty();
            if(order.empty()) order=single;
            require(!order.empty(),"sampled route has no saleable goods");
            auto p=basketPlan(m,a,b,order,-1);require(p.ok && p.quoted,"invalid basket");
            auto sp=basketPlan(m,a,b,single,-1);
            Market actual=m.live;int64_t bought=0;
            std::vector<int64_t> costs;for(auto l:order) {auto v=actual.buy(a,l.good,l.qty,p.pricePermille);require(v>=0,"buy failed");bought+=v;costs.push_back(v);}
            require(bought==p.goods,"quote differs from purchase");
            int months=(1+p.eta)/3;for(int j=0;j<months;++j) actual.month(j);
            int64_t revenue=0,duty=0,remainingCost=0;int remainder=0,unsold=0;
            std::ostringstream cargo;cargo<<'[';
            for(size_t j=0;j<order.size();++j) {
                auto l=order[j];int q=std::min(l.qty,actual.room(b,l.good));
                int64_t rev=q?actual.sell(b,l.good,q,p.pricePermille):0;require(rev>=0,"sale failed");
                int64_t tax=m.stance[b]==2?0:taxWithCarry(rev,taxPct(d,b,m.cfg.tax,m.cfg.taxHub),100,&remainder);
                revenue+=rev;duty+=tax;unsold+=l.qty-q;remainingCost+=costs[j]-costs[j]*q/l.qty;
                if(j)cargo<<',';cargo<<"{\"good\":"<<js(d.goodName[l.good])<<",\"quantity\":"<<l.qty<<",\"sold\":"<<q<<",\"cost\":"<<costs[j]<<",\"revenue\":"<<rev<<",\"tax\":"<<tax<<'}';
            }cargo<<']';
            auto cash=revenue-duty-p.transit-p.cost;
            require(p.used<=200 && p.cost<=4000 && p.transit<=p.transitLimit,"budget/capacity/tax invariant");
            if(id)out<<",\n";
            out<<"{\"id\":"<<++id<<",\"stratum\":"<<k<<",\"origin_id\":"<<a<<",\"destination_id\":"<<b<<",\"origin\":"<<js(d.cityName[a])<<",\"destination\":"<<js(d.cityName[b])
               <<",\"distance\":"<<p.dist<<",\"turns\":"<<p.eta<<",\"months\":"<<months<<",\"recommended\":"<<(recommended?"true":"false")
               <<",\"saved_gold\":"<<savedGold[a]<<",\"saved_troops\":"<<savedTroops[a]<<",\"saved_food\":"<<savedFood[a]
               <<",\"snapshot_resources_sufficient\":"<<((savedGold[a]>=p.cost && savedTroops[a]>=500 && savedFood[a]>=3000)?"true":"false")
               <<",\"cargo\":"<<cargo.str()<<",\"load_tenths\":"<<p.used<<",\"purchase\":"<<p.goods<<",\"fee\":"<<m.live.p.fee
               <<",\"departure_cost\":"<<p.cost<<",\"quoted_revenue\":"<<p.revenue<<",\"quoted_sales_tax\":"<<p.duty<<",\"estimated_transit\":"<<p.transit
               <<",\"transit_cap\":"<<p.transitLimit<<",\"quoted_profit\":"<<p.profit<<",\"single_quoted_profit\":"<<sp.profit
               <<",\"sales_tax_pct\":"<<(m.stance[b]==2?0:taxPct(d,b,m.cfg.tax,m.cfg.taxHub))
               <<",\"arrival_revenue\":"<<revenue<<",\"arrival_sales_tax\":"<<duty<<",\"cash_profit\":"<<cash
               <<",\"remaining_cost\":"<<remainingCost<<",\"realized_profit\":"<<cash+remainingCost<<",\"unsold_quantity\":"<<unsold
               <<",\"cap_tax_cash_profit\":"<<revenue-duty-p.transitLimit-p.cost;
            if(explicitRoutes) {
                out<<",\"single_options_at_departure\":[";bool comma=false;
                for(int g=0;g<d.G;++g) {
                    int q=std::min(maxOrderQuantity(m,a,b,{},g,-1),m.seen.room(b,g));
                    if(q<1) continue;auto alt=basketPlan(m,a,b,{{g,q}},-1);
                    require(alt.ok && alt.quoted,"invalid single commodity comparison");
                    if(comma)out<<',';comma=true;
                    out<<"{\"good\":"<<js(d.goodName[g])<<",\"quantity\":"<<q
                       <<",\"produced_at_origin\":"<<(d.produces(a,g)?"true":"false")
                       <<",\"stock\":"<<m.live.available(a,g)<<",\"purchase\":"<<alt.goods
                       <<",\"revenue\":"<<alt.revenue<<",\"sales_tax\":"<<alt.duty
                       <<",\"transit\":"<<alt.transit<<",\"profit\":"<<alt.profit<<'}';
                }
                out<<']';
            }
            out<<'}';
        }
    }
    out<<"\n]}\n";require(id==expected,"route count");require(bool(out),"output write failed");
    std::cout<<"PASS: "<<id<<" independent routes; snapshot prices, purchase quotes, budgets, cargo capacity and transit caps verified.\n";
    return 0;
}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
