// M13 trade: economy model, see trade_model.h and M13_TRADE.md.
#include "trade_model.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

namespace trade {

int Data::militaryWeapon(int g) const {
    if(unitsPerLot(g)!=100 || g>=(int)goodName.size())return -1;
    const char* names[]={"槍","戟","弩","馬"};
    for(int w=0;w<4;++w)if(goodName[g]==names[w])return w+1;
    return -1;
}

int Data::loadUnits(int g) const {
    if(g<0 || g>=(int)goodName.size()) return 10;
    if(g<(int)cargoUnits.size() && cargoUnits[g])return cargoUnits[g];
    const auto& n=goodName[g];
    if(n=="鹽") return 2;
    if(n=="棉布") return 4;
    if(n=="絲" || n=="皮毛" || n=="鐵器") return 5;
    return 10;
}
int64_t Cargo::loadUnits(const Data& d) const {
    int64_t total=0;
    for(const auto& l:lots) total+=(int64_t)l.qty*d.loadUnits(l.good);
    return total;
}
bool quoteOrder(const Market& m,int city,const std::vector<OrderLine>& order,
                int slots,int pp,int64_t budget,int fee,Cargo* cargo,int64_t* total) {
    if(city<0 || city>=m.d->C || order.empty() || order.size()>(size_t)m.d->G || slots<1 || fee<0) return false;
    Cargo result; int64_t cost=fee;
    std::vector<bool> seen(m.d->G,false);
    for(const auto& l:order) {
        if(l.good<0 || l.good>=m.d->G || l.qty<1 || seen[l.good]) return false;
        seen[l.good]=true;
        if((int64_t)l.qty*m.d->loadUnits(l.good)+result.loadUnits(*m.d)>(int64_t)slots*10) return false;
        int64_t value=m.quote(city,l.good,l.qty,true,pp);
        if(value<0 || cost+value>budget) return false;
        result.add(l.good,city,l.qty,value);cost+=value;
    }
    *cargo=std::move(result);*total=cost;return true;
}
int64_t transitCharge(int64_t value,int rate,int merchantPct,int64_t limit,int64_t paid) {
    return std::max<int64_t>(0,std::min(taxOf(value,rate,merchantPct),limit-paid));
}

namespace {
template <class T> bool rd(const uint8_t*& p, const uint8_t* end, T* v) {
    if ((size_t)(end - p) < sizeof(T)) return false;
    memcpy(v, p, sizeof(T)); p += sizeof(T); return true;
}
std::string name16(const uint8_t* p) { size_t n = 0; while (n < 16 && p[n]) n++; return std::string((const char*)p, n); }
}  // namespace

bool Data::load(const uint8_t* p, size_t n, std::string* err) {
    const uint8_t* end = p + n;
    uint32_t magic, ver, c, g, res;
    if (!rd(p, end, &magic) || magic != 0x42445254u /* "TRDB" */) { *err = "not a trade.bin (magic)"; return false; }
    if (!rd(p, end, &ver) || !rd(p, end, &c) || !rd(p, end, &g) || !rd(p, end, &res)) { *err = "short header"; return false; }
    if (ver < 1 || ver > 6) { *err = "unsupported version"; return false; }
    if (c < 1 || c > 4096 || g < 1 || g > MAX_GOODS) { *err = "bad city / good count"; return false; }
    size_t need = (size_t)g * 24 + (size_t)c * 24 + (size_t)c * g * 5;
    if (ver == 1 ? (size_t)(end - p) != need : (size_t)(end - p) < need) { *err = "size does not match the counts"; return false; }
    C = (int)c; G = (int)g; version = (int)ver;
    stockCap = 200; producerStock = 100; hubStock = 10; productionDiv = 30; consumptionDiv = 50;
    goodName.resize(G);
    cargoUnits.assign(G,0);
    nativeUnits.assign(G,0);minValue.assign(G,0);
    for (int i = 0; i < G; i++) {
        goodName[i] = name16(p); p += 20;
        uint32_t units;rd(p,end,&units);
        if(ver>=5) {
            if(units<1 || units>100) {*err="invalid cargo units";return false;}
            cargoUnits[i]=(uint8_t)units;
        }
    }
    lo.resize(C); hi.resize(C); flags.resize(C); group.resize(C); size.resize(C); cityName.resize(C);
    for (int i = 0; i < C; i++) {
        rd(p, end, &lo[i]); rd(p, end, &hi[i]); rd(p, end, &flags[i]); rd(p, end, &group[i]); rd(p, end, &size[i]);
        if (!size[i]) size[i] = 100;
        cityName[i] = name16(p); p += 16;
    }
    price.resize((size_t)C * G); supply.resize((size_t)C * G);
    memcpy(price.data(), p, price.size() * 4); p += price.size() * 4;
    memcpy(supply.data(), p, supply.size()); p += supply.size();
    demand.assign((size_t)C * G, 1000);
    for (uint32_t& v : price) if (!v) v = 1;
    groupName.clear(); seas.clear(); events.clear(); merchants.clear();
    if (ver == 1) return true;
    // version 2: groups, monsoon seas, events
    uint32_t ng, nr, ne;
    if (!rd(p, end, &ng) || ng > 250 || (size_t)(end - p) < (size_t)ng * 16) { *err = "v2: bad group table"; return false; }
    for (uint32_t i = 0; i < ng; i++) { groupName.push_back(name16(p)); p += 16; }
    if (!rd(p, end, &nr) || nr > 64 || (size_t)(end - p) < (size_t)nr * 36) { *err = "v2: bad monsoon table"; return false; }
    for (uint32_t i = 0; i < nr; i++) {
        Sea s; rd(p, end, &s.lo0); rd(p, end, &s.lo1); rd(p, end, &s.hi0); rd(p, end, &s.hi1);
        memcpy(s.months, p, 12); p += 12; s.name = name16(p); p += 16;
        seas.push_back(s);
    }
    if (!rd(p, end, &ne) || ne > 256 || (size_t)(end - p) < (size_t)ne * 28) { *err = "v2: bad event table"; return false; }
    for (uint32_t i = 0; i < ne; i++) {
        EventDef e; e.name = name16(p); p += 16;
        uint8_t pad8; uint16_t pad16;
        rd(p, end, &e.apply); rd(p, end, &e.good); rd(p, end, &e.group); rd(p, end, &pad8); rd(p, end, &e.mult);
        rd(p, end, &e.mMin); rd(p, end, &e.mMax); rd(p, end, &e.weight); rd(p, end, &pad16);
        if (e.apply > 2 || (e.good >= G && e.good < EV_GOOD_ANY) || e.group > ng || !e.mult || !e.mMin || e.mMax < e.mMin) {
            *err = "v2: bad event " + std::to_string(i); return false;
        }
        events.push_back(e);
    }
    if (ver == 2) { if (p != end) { *err = "v2: bytes after the event table"; return false; } return true; }
    // version 3: known merchants
    uint32_t nm;
    if (!rd(p, end, &nm) || nm > 4096 || (size_t)(end - p) < (size_t)nm * 32) { *err = "v3: bad merchant table"; return false; }
    for (uint32_t i = 0; i < nm; i++) {
        MerchantName m;                                  // char name[27] utf-8, i32 id, u8 talent
        size_t n = 0; while (n < 27 && p[n]) n++;
        m.name.assign((const char*)p, n);
        memcpy(&m.id, p + 27, 4);
        m.talent = p[31] ? p[31] : 1;
        p += 32;
        merchants.push_back(m);
    }
    if (ver >= 4) {
        uint32_t cfg[5];
        for (auto& v : cfg) if (!rd(p, end, &v) || v < 1 || v > 1000) { *err = "v4: invalid inventory settings"; return false; }
        stockCap = cfg[0]; producerStock = cfg[1]; hubStock = cfg[2]; productionDiv = cfg[3]; consumptionDiv = cfg[4];
        if (producerStock > stockCap || hubStock > stockCap || (size_t)(end - p) != demand.size() * 2 + (ver>=6 ? (size_t)G*8 : 0)) { *err = "v4: invalid inventory table"; return false; }
        memcpy(demand.data(), p, demand.size() * 2); p += demand.size() * 2;
        for (auto v : demand) if (v > 10000) { *err = "v4: invalid demand"; return false; }
    }
    if(ver>=6) for(int g=0;g<G;++g) {
        if(!rd(p,end,&nativeUnits[g]) || !rd(p,end,&minValue[g]) || nativeUnits[g]>100000 || minValue[g]>1000000 ||
           ((nativeUnits[g]==0)!=(minValue[g]==0))) {*err="v6: invalid pricing lot or floor";return false;}
    }
    if (p != end) { *err = "bytes after trade data"; return false; }
    return true;
}

int Data::seaOf(int lo, int hi) const {
    for (size_t i = 0; i < seas.size(); i++)
        if (lo >= seas[i].lo0 && lo < seas[i].lo1 && hi >= seas[i].hi0 && hi < seas[i].hi1) return (int)i;
    return -1;
}

int hexDist(int lo1, int hi1, int lo2, int hi2) {
    // odd-r: row = lo, odd rows shifted towards +hi (NOTES.md 1.1)
    int q1 = hi1 - (lo1 - (lo1 & 1)) / 2, q2 = hi2 - (lo2 - (lo2 & 1)) / 2;
    int dq = q1 - q2, dr = lo1 - lo2;
    return (abs(dq) + abs(dr) + abs(dq + dr)) / 2;
}

int64_t Cargo::saleBonus(int good,int qty,int dest,int64_t net,bool merchant) const {
    if(!merchant || qty<=0 || count(good)<qty || dest<0)return 0;
    int left=qty,eligible=0;int64_t basis=0,eligibleBasis=0;
    for(auto& l:lots) {
        if(l.good!=good || !left)continue;
        int n=std::min(left,l.qty);int64_t cost=l.cost*n/l.qty;
        basis+=cost;left-=n;
        if(l.origin!=dest) {eligible+=n;eligibleBasis+=cost;}
    }
    // Match take()'s FIFO basis. Same-city cargo earns nothing, and a losing sale cannot earn a bonus.
    return std::min(merchantProfitBonus(net,basis,true),
                    merchantProfitBonus(net*eligible/qty,eligibleBasis,true));
}
int Cargo::count(int good) const { int n = 0; for (const auto& l : lots) if (good < 0 || l.good == good) n += l.qty; return n; }
int64_t Cargo::cost() const { int64_t n = 0; for (const auto& l : lots) n += l.cost; return n; }
void Cargo::add(int good, int origin, int qty, int64_t value) {
    if (qty <= 0 || value < 0) return;
    for (auto& l : lots) if (l.good == good && l.origin == origin) { l.qty += qty; l.initial += qty; l.cost += value; return; }
    lots.push_back({ good, origin, qty, qty, value });
}
bool Cargo::take(int good, int qty, int dest, int tripXp, int64_t* basis, int* xp) {
    if (qty <= 0 || count(good) < qty) return false;
    *basis = 0; *xp = 0;
    for (auto it = lots.begin(); it != lots.end() && qty;) {
        auto& l = *it;
        if (l.good != good) { ++it; continue; }
        int n = std::min(qty, l.qty);
        int64_t cost = l.cost * n / l.qty;
        if (l.origin != dest && dest >= 0)
            *xp += (int)((int64_t)tripXp * (l.initial - l.qty + n) / l.initial - (int64_t)tripXp * (l.initial - l.qty) / l.initial);
        *basis += cost; l.cost -= cost; l.qty -= n; qty -= n;
        if (!l.qty) it = lots.erase(it); else ++it;
    }
    return true;
}

void Market::reset() {
    idx.assign((size_t)d->C * d->G, 1000);
    stock.resize(idx.size());
    for (int c = 0; c < d->C; c++) for (int g = 0; g < d->G; g++)
        stock[(size_t)c * d->G + g] = d->size[c] * (d->produces(c, g) ? (d->militaryWeapon(g)>=0 ? 20 : d->producerStock) : d->hub(c) ? (d->militaryWeapon(g)>=0 ? 2 : d->hubStock) : 0) / 100;
}

int64_t Market::valueAt(int c,int g,int index) const {
    int64_t floor=g<(int)d->minValue.size() ? (int64_t)d->minValue[g]*1000 : 0;
    return std::max(floor,base(c,g)*index);
}
int Market::buyPrice(int c, int g) const {
    return (int)(valueAt(c,g,at(c,g)) * (200 + p.spread) / 200000);
}
int Market::sellPrice(int c, int g) const {
    return (int)(valueAt(c,g,at(c,g)) * (200 - p.spread) / 200000);
}

int64_t Market::walk(int c, int g, int q, bool buying, int32_t* index, int pp) const {
    // Identical unit edges in either direction: splitting an order cannot improve its price.
    int half = std::max(5, p.spread * 5 - pp / 2);
    half = std::min(999, half);
    int step = p.impact ? std::max(1, p.impact * 10 / d->size[c]) : 0;
    int64_t total = 0;
    for (int n = 0; n < q; n++) {
        int next = std::max(p.idxMin, std::min(p.idxMax, *index + (buying ? step : -step)));
        int64_t value = (valueAt(c,g,*index) + valueAt(c,g,next)) * (buying ? 1000 + half : 1000 - half);
        total += buying ? (value + 1999999) / 2000000 : value / 2000000;
        *index = next;
    }
    return total;
}

int64_t Market::quote(int c, int g, int q, bool buying, int pp) const {
    if (c < 0 || c >= d->C || g < 0 || g >= d->G || q <= 0 || q > (buying ? available(c, g) : room(c, g))) return -1;
    int32_t i = at(c, g);
    return walk(c, g, q, buying, &i, pp);
}
int64_t Market::buy(int c, int g, int q, int pp) {
    int64_t value = quote(c, g, q, true, pp);
    if (value < 0) return value;
    walk(c, g, q, true, &at(c, g), pp); stock[(size_t)c * d->G + g] -= q; return value;
}
int64_t Market::sell(int c, int g, int q, int pp) {
    int64_t value = quote(c, g, q, false, pp);
    if (value < 0) return value;
    walk(c, g, q, false, &at(c, g), pp); stock[(size_t)c * d->G + g] += q; return value;
}

void Market::month(int phase) {
    for (int c = 0; c < d->C; c++) {
        int r = d->hub(c) ? p.revertHub : p.revert;
        for (int g = 0; g < d->G; g++) {
            size_t k = (size_t)c * d->G + g;
            int consume = d->size[c] * (d->demand.empty() ? 1000 : d->demand[k]) / (d->consumptionDiv * 1000);
            if(d->militaryWeapon(g)>=0) {
                auto flow=[&](int64_t n,int den) {int64_t t=std::max(0,phase);return (int)((t+1)*n/den-t*n/den);};
                consume=flow((int64_t)d->size[c]*(d->demand.empty()?1000:d->demand[k]),250000);
                int old=stock[k],cap=d->stockLimit(c,g);
                stock[k]=std::max(0,old-consume);
                if(old<=cap)stock[k]=std::min(cap,stock[k]+flow((int64_t)d->size[c]*d->supply[k],150));
            } else {
                stock[k] = std::max(0, stock[k] - consume);
                stock[k] = std::min(d->size[c] * d->stockCap / 100, stock[k] + d->size[c] * d->supply[k] / d->productionDiv);
            }
            int32_t& i = at(c, g);
            int gap = 1000 - i;
            int step = gap * r / 100;
            if (r && !step && gap) step = gap > 0 ? 1 : -1;
            i += step;
        }
    }
}

bool bestRoute(const Market& m, int origin, const std::vector<int>& dests, int64_t budget, int cap, int taxPct,
               int marginPct, Route* out, const std::vector<int>* timePermille, int pp, const std::vector<int>* duties, int merchantTaxPct, int departureFee, bool merchant) {
    const Data& d = *m.d;
    Route best; best.profit = 0;
    int64_t bestScore = 0;
    int fee = departureFee < 0 ? m.p.fee : departureFee;
    for (int g = 0; g < d.G; g++) {
        if (m.available(origin, g) <= 0) continue;
        std::vector<int64_t> costs(1, fee);
        int32_t buyIndex = m.at(origin, g);
        for (int q = 1; q <= std::min(cap*10/d.loadUnits(g), m.available(origin, g)); q++) {
            int64_t cost = costs.back() + m.walk(origin, g, 1, true, &buyIndex, pp);
            if (cost > budget) break;
            costs.push_back(cost);
        }
        if (costs.size() == 1) continue;
        for (size_t di = 0; di < dests.size(); di++) {
            int c = dests[di];
            if (c == origin) continue;
            int32_t sellIndex = m.at(c, g);
            int64_t rev = 0;
            int maxQty = std::min((int)costs.size() - 1, m.room(c, g));
            for (int q = 1; q <= maxQty; q++) {
                int64_t cost = costs[q];
                rev += m.walk(c, g, 1, false, &sellIndex, pp);
                int64_t profit = rev - cost - taxOf(rev, duties ? (*duties)[di] : taxPct, merchantTaxPct);
                profit += merchantProfitBonus(profit+cost,cost-fee,merchant);
                if (profit * 100 < cost * marginPct) continue;
                int dist = hexDist(d.lo[origin], d.hi[origin], d.lo[c], d.hi[c]);
                // gold per hex travelled: a far market has to pay for the time on the road
                // (a monsoon makes a sea route slower or faster: timePermille)
                int64_t tp = timePermille ? (*timePermille)[di] : 1000;
                int64_t score = profit * 1000000 / ((dist + 20) * tp);
                if (best.good < 0 || score > bestScore) {
                    best.origin = origin; best.dest = c; best.good = g; best.qty = q;
                    best.cost = cost; best.revenue = rev; best.profit = profit; best.dist = dist;
                    bestScore = score;
                }
            }
        }
    }
    if (best.good < 0) return false;
    *out = best;
    return true;
}

std::vector<std::vector<int>> citiesInRange(const Data& d, int range) {
    std::vector<std::vector<int>> r(d.C);
    for (int a = 0; a < d.C; a++)
        for (int b = 0; b < d.C; b++)
            if (a != b && hexDist(d.lo[a], d.hi[a], d.lo[b], d.hi[b]) <= range) r[a].push_back(b);
    return r;
}

// ---------------------------------------------------------------- phase 3: monsoon
char season(const Data& d, int sea, int month0) {
    if (sea < 0 || sea >= (int)d.seas.size()) return '-';
    return d.seas[sea].months[((month0 % 12) + 12) % 12];
}
int seaCostPct(const Data& d, const Params& p, int sea, int month0, int dir) {
    if (!p.monsoon) return 100;
    switch (season(d, sea, month0)) {
    case 'E': return dir > 0 ? p.monWith : dir < 0 ? p.monAgainst : 100;
    case 'W': return dir < 0 ? p.monWith : dir > 0 ? p.monAgainst : 100;
    case 'T': return p.monStorm;
    default: return 100;
    }
}
int seaStepCost(const Data& d, const Params& p, int sea, int month0, int dir, int cost) {
    int pct = seaCostPct(d, p, sea, month0, dir);
    int c = (cost * pct + 50) / 100, floor2 = cost < 2 ? cost : 2;
    if (c < floor2) c = floor2;
    return c < 1 ? 1 : c > 254 ? 254 : c;
}
namespace {
// the seas a route between two cities touches: at both ends and half-way (distinct, at most 3)
int routeSeas(const Data& d, int o, int t, int out[3]) {
    int n = 0;
    int pts[3][2] = { { d.lo[o], d.hi[o] }, { (d.lo[o] + d.lo[t]) / 2, (d.hi[o] + d.hi[t]) / 2 }, { d.lo[t], d.hi[t] } };
    for (auto& pt : pts) {
        int s = d.seaOf(pt[0], pt[1]);
        if (s < 0) continue;
        bool dup = false; for (int i = 0; i < n; i++) dup |= out[i] == s;
        if (!dup) out[n++] = s;
    }
    return n;
}
}  // namespace
int routeTimePermille(const Data& d, const Params& p, int origin, int dest, int month0) {
    if (!p.monsoon || !d.port(origin) || !d.port(dest)) return 1000;
    int seas[3], n = routeSeas(d, origin, dest, seas);
    if (!n) return 1000;
    int dir = d.lo[dest] > d.lo[origin] ? 1 : d.lo[dest] < d.lo[origin] ? -1 : 0;
    int sum = 0; for (int i = 0; i < n; i++) sum += seaCostPct(d, p, seas[i], month0, dir);
    return sum * 10 / n;
}
bool routeStormy(const Data& d, const Params& p, int origin, int dest, int month0) {
    if (!p.monsoon || !d.port(origin) || !d.port(dest)) return false;
    int seas[3], n = routeSeas(d, origin, dest, seas);
    for (int i = 0; i < n; i++) if (season(d, seas[i], month0) == 'T') return true;
    return false;
}

// ---------------------------------------------------------------- phase 3: events
bool Events::affects(const Data& d, const ActiveEvent& e, int c, int g) const {
    if (e.def >= d.events.size() || d.group[c] != e.group) return false;
    if (e.good != EV_GOOD_ALL && e.good != g) return false;
    switch (d.events[e.def].apply) {
    case EV_CONSUMERS: return !d.produces(c, g);
    case EV_PRODUCERS: return d.produces(c, g);
    default: return true;
    }
}
void Events::apply(const Data& d, std::vector<uint16_t>* mod) const {
    if (live.empty()) { mod->clear(); return; }
    mod->assign((size_t)d.C * d.G, 1000);
    for (auto& e : live)
        for (int c = 0; c < d.C; c++) {
            if (d.group[c] != e.group) continue;
            for (int g = 0; g < d.G; g++) {
                if (!affects(d, e, c, g)) continue;
                uint16_t& m = (*mod)[(size_t)c * d.G + g];
                int v = (int)m * d.events[e.def].mult / 1000;
                m = (uint16_t)(v < 200 ? 200 : v > 5000 ? 5000 : v);
            }
        }
}
void Events::month(const Data& d, const Params& p, Rng& rng, int month, std::vector<ActiveEvent>* started,
                   std::vector<ActiveEvent>* ended) {
    for (size_t i = 0; i < live.size();) {
        if (live[i].end <= month) { if (ended) ended->push_back(live[i]); live.erase(live.begin() + i); }
        else i++;
    }
    if (!p.events || d.events.empty() || d.groupName.empty() || (int)live.size() >= p.eventMax) return;
    if (rng.pct() >= p.eventChance) return;
    int total = 0; for (auto& e : d.events) total += e.weight;
    if (total <= 0) return;
    int pick = (int)(rng.next() % (uint32_t)total), def = 0;
    while (pick >= d.events[def].weight) { pick -= d.events[def].weight; def++; }
    const EventDef& ed = d.events[def];
    int NG = (int)d.groupName.size();
    for (int attempt = 0; attempt < 8; attempt++) {         // a target that the event changes something in
        ActiveEvent e = {};
        e.def = (uint8_t)def;
        e.good = ed.good == EV_GOOD_ANY ? (uint8_t)(rng.next() % (uint32_t)d.G) : ed.good;
        e.group = ed.group ? ed.group : (uint8_t)(1 + rng.next() % (uint32_t)NG);
        bool hit = false, dup = false;
        for (auto& o : live) dup |= o.def == e.def && o.group == e.group && o.good == e.good;
        for (int c = 0; c < d.C && !hit && !dup; c++)
            for (int g = 0; g < d.G && !hit; g++) hit = affects(d, e, c, g);
        if (!hit || dup) continue;
        e.start = month;
        e.end = month + ed.mMin + (int)(rng.next() % (uint32_t)(ed.mMax - ed.mMin + 1));
        live.push_back(e);
        if (started) started->push_back(e);
        return;
    }
}
std::string describe(const Data& d, const ActiveEvent& e) {
    if (e.def >= d.events.size()) return "?";
    const EventDef& ed = d.events[e.def];
    std::string s = ed.name + "\xEF\xBC\x9A" + d.groupLabel(e.group) + " ";            // 「：」
    s += e.good == EV_GOOD_ALL ? std::string("\xE5\x90\x84\xE8\xB2\xA8") : (e.good < d.G ? d.goodName[e.good] : "?");   // 各貨
    char buf[32]; snprintf(buf, sizeof buf, " \xC3\x97%d.%02d", ed.mult / 1000, ed.mult % 1000 / 10);   // ×1.60
    return s + buf;
}

// ---------------------------------------------------------------- phase 3: information delay
void Info::reset(int r, const Data& d, int h) {
    R = r; C = d.C; G = d.G; H = h < 1 ? 1 : h > 60 ? 60 : h;
    age.assign((size_t)R * C, 255);
    hist.assign((size_t)H * C * G, 0);
    observed.assign((size_t)R * C * G, Observation());
    histMonth = -1;
}
void Info::see(int f, int c, const Market* m) {
    if (f < 0 || f >= R || c < 0 || c >= C) return;
    age[(size_t)f * C + c] = 0;
    if (m) for (int g = 0; g < G; g++) {
        size_t k = (size_t)c * G + g;
        observed[((size_t)f * C + c) * G + g] = { (int16_t)m->idx[k], m->mod.empty() ? (uint16_t)1000 : m->mod[k], m->stock[k] };
    }
}
void Info::endMonth(const Market& m, int month) {
    if (!C) return;
    int16_t* slot = &hist[(size_t)(((month % H) + H) % H) * C * G];
    for (size_t i = 0; i < (size_t)C * G; i++) slot[i] = (int16_t)m.idx[i];
    histMonth = month;
    for (uint8_t& a : age) if (a < 255) a++;
}
bool Info::believed(const Market& m, int f, int c, int g, int32_t* idx) const {
    int a = ageOf(f, c);
    if (a == 0) { *idx = m.at(c, g); return true; }
    if (a > H || histMonth < 0) return false;
    int month = histMonth + 1 - a;
    if (month < 0 || month <= histMonth - H) return false;
    *idx = hist[((size_t)(month % H) * C + c) * G + g];
    return true;
}
void Info::view(const Market& m, int f, Market* out, std::vector<char>* known) const {
    *out = m;
    out->mod.assign((size_t)C * G, 1000);
    known->assign(m.d->C, 1);
    for (int c = 0; c < m.d->C; c++) {
        for (int g = 0; g < m.d->G; g++) {
            size_t k = (size_t)c * G + g;
            const Observation* o = f >= 0 && f < R ? &observed[((size_t)f * C + c) * G + g] : nullptr;
            if (o && o->idx > 0 && ageOf(f, c) <= H) {
                out->at(c, g) = o->idx; out->mod[k] = o->mod; out->stock[k] = o->stock; continue;
            }
            if (ageOf(f, c) == 0) { out->mod[k] = m.mod.empty() ? 1000 : m.mod[k]; continue; }
            int32_t v;
            if (believed(m, f, c, g, &v)) out->at(c, g) = v;
            else { out->at(c, g) = 1000; (*known)[c] = 0; }          // never seen: the base price (hearsay)
            // No old event/inventory record: unknown, rather than mixing current state into history.
            (*known)[c] = 0; out->stock[k] = 0;
        }
    }
}

// ---------------------------------------------------------------- phase 4: merchant officers
int merchantLevel(const Params& p, int exp) {
    int l = 0;
    while (l < 5 && exp >= p.merchExp[l]) l++;
    return l;
}
int merchantEffective(const Params& p, int exp, int talent) {
    int l = merchantLevel(p, exp) + (talent < 0 ? 0 : talent);
    return l > p.merchMaxLevel ? p.merchMaxLevel : l;
}
int merchantTalent(const Params& p, int known, int politics) {
    if (known > 0) return known > 2 ? 2 : known;
    return politics >= p.merchPol ? 1 : 0;
}
int merchantExpGain(const Params& p, int64_t profit) {
    int64_t g = p.merchExpTrip + (profit > 0 && p.merchExpDiv > 0 ? profit / p.merchExpDiv : 0);
    return (int)(g > 100000 ? 100000 : g);
}
MerchantBonus merchantBonus(const Params& p, int level) {
    MerchantBonus b;
    if (level <= 0) return b;
    b.pp = p.merchPP * level;
    b.capPct = p.merchCap * level;
    b.taxPct = 100 - p.merchTax * level; if (b.taxPct < 0) b.taxPct = 0;
    b.robPct = 100 - p.merchRob * level; if (b.robPct < 0) b.robPct = 0;
    return b;
}

}  // namespace trade
