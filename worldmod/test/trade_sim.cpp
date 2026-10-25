// M13 trade: offline 120-month simulation of the economy model (worldmod/src/trade_model.cpp, the same code the
// DLL runs). No game: a synthetic world on trade.bin's 530 cities (forces = Voronoi cells around R capitals,
// random relations, flat city income), AI caravans as in trade.cpp (at most 6 new caravans a month, 2 per
// force, 30 at a time, travel time from the hex distance). Checks that prices stay bounded and settle, that trade
// money stays small against the normal income, and that the AI's trips earn a sane return.
//
//   trade_sim.exe <trade.bin> [months=120] [seed] [temp caravans=30]
// Exit code 0 = every check passed.
#include "../src/trade_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <vector>

using namespace trade;

static std::vector<uint8_t> readFile(const char* p) {
    std::vector<uint8_t> v; FILE* f = nullptr;
    if (fopen_s(&f, p, "rb") || !f) return v;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    v.resize(n); fread(v.data(), 1, n, f); fclose(f); return v;
}

struct Caravan { int force, origin, dest, good, qty, arrive; int64_t cost; };

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: trade_sim <trade.bin> [months] [seed]\n"); return 2; }
    std::vector<uint8_t> bin = readFile(argv[1]);
    Data d; std::string err;
    if (!d.load(bin.data(), bin.size(), &err)) { printf("trade.bin: %s\n", err.c_str()); return 2; }
    int months = argc > 2 ? atoi(argv[2]) : 120;
    Rng rng; if (argc > 3) rng.s = (uint32_t)strtoul(argv[3], nullptr, 10) | 1;

    const int TEMP_MAX = argc > 4 ? atoi(argv[4]) : 30, AI_MONTH = TEMP_MAX / 5;   // trade_temp_max, trade_ai_month
    const int R = 200, RANGE = 80, PER_FORCE = 2, MIN_GOLD = 6000, INVEST = 4000, RESERVE = 2000;
    const int MARGIN = 25, TAX = 10, SPEED = 18 /* hexes a month: 6 a turn */, LOSS = 5 /* % trips intercepted */;
    // forces: R capitals spread over the city list, every city joins the nearest
    std::vector<int> cap(R), owner(d.C);
    for (int f = 0; f < R; f++) cap[f] = (int)((int64_t)f * d.C / R);
    for (int c = 0; c < d.C; c++) {
        int best = 0, bd = 1 << 30;
        for (int f = 0; f < R; f++) { int dd = hexDist(d.lo[c], d.hi[c], d.lo[cap[f]], d.hi[cap[f]]); if (dd < bd) { bd = dd; best = f; } }
        owner[c] = best;
    }
    std::vector<int> rel((size_t)R * R);
    for (int a = 0; a < R; a++) for (int b = a; b < R; b++) { int v = a == b ? 100 : (int)(rng.next() % 101); rel[(size_t)a * R + b] = rel[(size_t)b * R + a] = v; }
    std::vector<int64_t> gold(d.C, 5000);
    auto income = [&](int c) { return 600 + d.size[c] * 2; };

    Market m; m.d = &d; m.reset();
    auto inRange = citiesInRange(d, RANGE);
    std::vector<Caravan> live;
    int64_t totalIncome = 0, totalTradeNet = 0, trips = 0, lost = 0;
    std::vector<double> roi;
    std::vector<double> dev(months + 37);
    int start = 0, idxLo = 1 << 30, idxHi = 0;
    bool stopTrade = false;
    for (int mo = 0; mo < months + 36; mo++) {
        stopTrade = mo >= months;               // the last 3 years: no trade, prices must settle back to 1000
        for (int c = 0; c < d.C; c++) { gold[c] += income(c); totalIncome += mo < months ? income(c) : 0; if (gold[c] > 15000) gold[c] = 15000; }   // the rest is spent (troops, development)
        // arrivals
        for (size_t i = 0; i < live.size();) {
            Caravan& k = live[i];
            if (k.arrive > mo) { i++; continue; }
            int64_t net;
            if ((int)(rng.next() % 100) < LOSS) { net = -k.cost; lost++; }
            else {
                int64_t rev = m.sell(k.dest, k.good, k.qty);
                bool foreign = owner[k.dest] != k.force;
                int64_t tax = foreign ? rev * TAX / 100 : 0;
                gold[k.dest] += tax;                    // import duty to the market's owner
                gold[k.origin] += rev - tax;
                net = rev - tax - k.cost;
                roi.push_back((double)net / k.cost);
            }
            totalTradeNet += net; trips++;
            live[i] = live.back(); live.pop_back();
        }
        // AI plans (trade.cpp aiMonth)
        if (!stopTrade) {
            int made = 0;
            for (int n = 0; n < R && (int)live.size() < TEMP_MAX && made < AI_MONTH; n++) {
                int f = (start + n) % R;
                int mine = 0; for (auto& k : live) mine += k.force == f;
                if (mine >= PER_FORCE) continue;
                Route best; bool have = false;
                for (int c = 0; c < d.C; c++) {
                    if (owner[c] != f || gold[c] < MIN_GOLD) continue;
                    std::vector<int> land, sea;      // sea capacity when both ends are ports
                    for (int t : inRange[c]) { int o = owner[t]; if (o == f || rel[(size_t)f * R + o] >= 40) (d.port(c) && d.port(t) ? sea : land).push_back(t); }
                    int64_t budget = std::min<int64_t>(INVEST, gold[c] - RESERVE);
                    Route r;
                    if (bestRoute(m, c, land, budget, m.p.capLand, TAX, MARGIN, &r) && (!have || r.profit > best.profit)) { best = r; have = true; }
                    if (bestRoute(m, c, sea, budget, m.p.capSea, TAX, MARGIN, &r) && (!have || r.profit > best.profit)) { best = r; have = true; }
                }
                if (!have) continue;
                int64_t paid = m.buy(best.origin, best.good, best.qty) + m.p.fee;
                gold[best.origin] -= paid;
                live.push_back({ f, best.origin, best.dest, best.good, best.qty, mo + 1 + best.dist / SPEED, paid });
                made++;
            }
            start = (start + 7) % R;
        }
        m.month();
        int64_t sum = 0; for (int32_t v : m.idx) { sum += abs(v - 1000); idxLo = std::min(idxLo, (int)v); idxHi = std::max(idxHi, (int)v); }
        dev[mo] = (double)sum / m.idx.size();
    }
    std::sort(roi.begin(), roi.end());
    double med = roi.empty() ? 0 : roi[roi.size() / 2], p10 = roi.empty() ? 0 : roi[roi.size() / 10], p90 = roi.empty() ? 0 : roi[roi.size() * 9 / 10];
    double share = totalIncome ? (double)totalTradeNet / totalIncome : 0;
    int64_t goldSum = 0; for (auto g : gold) goldSum += g;
    printf("trade_sim: %d cities, %d goods, %d months (+36 without trade), %d forces\n", d.C, d.G, months, R);
    printf("  trips %lld (lost %lld), trade net %lld gold = %.2f%% of the normal income %lld\n", trips, lost, totalTradeNet, share * 100, totalIncome);
    printf("  return per trip: p10 %.0f%%  median %.0f%%  p90 %.0f%%\n", p10 * 100, med * 100, p90 * 100);
    printf("  price index range seen %d..%d; mean |idx-1000|: m12 %.2f  m60 %.2f  m%d %.2f  after 36 months without trade %.2f\n",
           idxLo, idxHi, dev[11], dev[59], months, dev[months - 1], dev[months + 35]);
    printf("  gold in all cities at the end %lld\n", goldSum);
    // checks
    int bad = 0;
    auto check = [&](bool ok, const char* what) { printf("  %-60s %s\n", what, ok ? "ok" : "FAILED"); bad += !ok; };
    check(idxLo >= m.p.idxMin && idxHi <= m.p.idxMax, "price index stays inside idx_min..idx_max");
    check(dev[months + 35] <= 1.0, "prices settle back to the base (mean |idx-1000| <= 1) without trade");
    check(fabs(dev[months - 1] - dev[months * 2 / 3]) <= std::max(2.0, dev[months * 2 / 3] * 0.5), "no drift: deviation at the end ~ deviation at 2/3");
    check(share > 0 && share < 0.10, "trade money is under 10% of the normal income");
    check(trips >= months, "the AI trades (at least one trip a month)");
    check(med > 0.05 && med < 2.0, "median AI return per trip between 5% and 200%");
    return bad ? 1 : 0;
}
