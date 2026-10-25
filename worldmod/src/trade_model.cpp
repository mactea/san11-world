// M13 trade: economy model, see trade_model.h and M13_TRADE.md.
#include "trade_model.h"
#include <string.h>
#include <stdlib.h>

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
    if (ver != 1) { *err = "unsupported version"; return false; }
    if (c < 1 || c > 4096 || g < 1 || g > 64) { *err = "bad city / good count"; return false; }
    size_t need = (size_t)g * 24 + (size_t)c * 24 + (size_t)c * g * 5;
    if ((size_t)(end - p) != need) { *err = "size does not match the counts"; return false; }
    C = (int)c; G = (int)g;
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
    memcpy(supply.data(), p, supply.size());
    for (uint32_t& v : price) if (!v) v = 1;
    return true;
}

int hexDist(int lo1, int hi1, int lo2, int hi2) {
    // odd-r: row = lo, odd rows shifted towards +hi (NOTES.md 1.1)
    int q1 = hi1 - (lo1 - (lo1 & 1)) / 2, q2 = hi2 - (lo2 - (lo2 & 1)) / 2;
    int dq = q1 - q2, dr = lo1 - lo2;
    return (abs(dq) + abs(dr) + abs(dq + dr)) / 2;
}

void Market::reset() { idx.assign((size_t)d->C * d->G, 1000); }

int Market::buyPrice(int c, int g) const {
    return (int)((int64_t)d->price[(size_t)c * d->G + g] * at(c, g) * (200 + p.spread) / 200000);
}
int Market::sellPrice(int c, int g) const {
    return (int)((int64_t)d->price[(size_t)c * d->G + g] * at(c, g) * (200 - p.spread) / 200000);
}

int64_t Market::walk(int c, int g, int q, bool buying, int32_t* index) const {
    if (q <= 0) return 0;
    double base = d->price[(size_t)c * d->G + g];
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
               int marginPct, Route* out) {
    const Data& d = *m.d;
    Route best; best.profit = 0;
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
        for (int c : dests) {
            if (c == origin) continue;
            int64_t rev = m.quoteSell(c, g, q);
            int64_t profit = rev - cost - rev * taxPct / 100;
            if (profit * 100 < cost * marginPct) continue;
            int dist = hexDist(d.lo[origin], d.hi[origin], d.lo[c], d.hi[c]);
            // gold per hex travelled: a far market has to pay for the time on the road
            int64_t score = profit * 1000 / (dist + 20), bestScore = best.profit * 1000 / (best.dist + 20);
            if (best.good < 0 || score > bestScore) {
                best.origin = origin; best.dest = c; best.good = g; best.qty = q;
                best.cost = cost; best.revenue = rev; best.profit = profit; best.dist = dist;
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

}  // namespace trade
