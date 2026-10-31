// M13 trade: economy model, see trade_model.h and M13_TRADE.md.
#include "trade_model.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace trade {

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
    if (ver != 1 && ver != 2) { *err = "unsupported version"; return false; }
    if (c < 1 || c > 4096 || g < 1 || g > 64) { *err = "bad city / good count"; return false; }
    size_t need = (size_t)g * 24 + (size_t)c * 24 + (size_t)c * g * 5;
    if (ver == 1 ? (size_t)(end - p) != need : (size_t)(end - p) < need) { *err = "size does not match the counts"; return false; }
    C = (int)c; G = (int)g; version = (int)ver;
    goodName.resize(G);
    for (int i = 0; i < G; i++) { goodName[i] = name16(p); p += 16; p += 8; }
    lo.resize(C); hi.resize(C); flags.resize(C); group.resize(C); size.resize(C); cityName.resize(C);
    for (int i = 0; i < C; i++) {
        rd(p, end, &lo[i]); rd(p, end, &hi[i]); rd(p, end, &flags[i]); rd(p, end, &group[i]); rd(p, end, &size[i]);
        if (!size[i]) size[i] = 100;
        cityName[i] = name16(p); p += 16;
    }
    price.resize((size_t)C * G); supply.resize((size_t)C * G);
    memcpy(price.data(), p, price.size() * 4); p += price.size() * 4;
    memcpy(supply.data(), p, supply.size()); p += supply.size();
    for (uint32_t& v : price) if (!v) v = 1;
    groupName.clear(); seas.clear(); events.clear();
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
    if (!rd(p, end, &ne) || ne > 256 || (size_t)(end - p) != (size_t)ne * 28) { *err = "v2: bad event table"; return false; }
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

void Market::reset() { idx.assign((size_t)d->C * d->G, 1000); }

int Market::buyPrice(int c, int g) const {
    return (int)(base(c, g) * at(c, g) * (200 + p.spread) / 200000);
}
int Market::sellPrice(int c, int g) const {
    return (int)(base(c, g) * at(c, g) * (200 - p.spread) / 200000);
}

int64_t Market::walk(int c, int g, int q, bool buying, int32_t* index) const {
    if (q <= 0) return 0;
    double base = (double)this->base(c, g);
    double f = 1.0 + p.impact / 100.0 * q / d->size[c];
    double i0 = *index, i1 = buying ? i0 * f : i0 / f;
    if (i1 < p.idxMin) i1 = p.idxMin;
    if (i1 > p.idxMax) i1 = p.idxMax;
    double side = buying ? (200 + p.spread) / 200.0 : (200 - p.spread) / 200.0;
    double total = base * q * (i0 + i1) / 2000.0 * side;
    *index = (int32_t)(i1 + 0.5);
    return (int64_t)(total + 0.5);
}

void Market::month() {
    for (int c = 0; c < d->C; c++) {
        int r = d->hub(c) ? p.revertHub : p.revert;
        for (int g = 0; g < d->G; g++) {
            int32_t& i = at(c, g);
            int gap = 1000 - i;
            int step = gap * r / 100;
            if (!step && gap) step = gap > 0 ? 1 : -1;      // always closes in on 1000
            i += step;
        }
    }
}

bool bestRoute(const Market& m, int origin, const std::vector<int>& dests, int64_t budget, int cap, int taxPct,
               int marginPct, Route* out, const std::vector<int>* timePermille) {
    const Data& d = *m.d;
    Route best; best.profit = 0;
    int64_t bestScore = 0;
    for (int g = 0; g < d.G; g++) {
        if (!d.canBuy(origin, g)) continue;
        int unit = m.buyPrice(origin, g);
        if (unit <= 0) continue;
        int q = (int)((budget - m.p.fee) / unit);
        if (q > cap) q = cap;
        if (q <= 0) continue;
        int64_t cost = m.quoteBuy(origin, g, q) + m.p.fee;
        while (q > 1 && cost > budget) { q--; cost = m.quoteBuy(origin, g, q) + m.p.fee; }   // the price walks up
        if (cost > budget) continue;
        for (size_t di = 0; di < dests.size(); di++) {
            int c = dests[di];
            if (c == origin) continue;
            int64_t rev = m.quoteSell(c, g, q);
            int64_t profit = rev - cost - rev * taxPct / 100;
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
    histMonth = -1;
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
    known->assign(m.d->C, 1);
    for (int c = 0; c < m.d->C; c++) {
        if (ageOf(f, c) == 0) continue;
        for (int g = 0; g < m.d->G; g++) {
            int32_t v;
            if (believed(m, f, c, g, &v)) out->at(c, g) = v;
            else { out->at(c, g) = 1000; (*known)[c] = 0; }          // never seen: the base price (hearsay)
        }
    }
}

}  // namespace trade
