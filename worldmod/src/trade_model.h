#pragma once
// M13 trade: the economy model (no game code; trade.cpp drives it in the game, tools/trade_sim runs it offline).
// M13_TRADE.md has the rules and the trade.bin format.
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

namespace trade {

constexpr int MAX_GOODS = 128; // Event IDs 254/255 remain reserved.

// Nominal cash movements; remittances can remain pending when a city is full.
// Losses are shown at purchase cost and are already included in purchases (never subtract twice).
struct Ledger {
    int64_t purchases = 0, receipts = 0, fees = 0, taxes = 0, losses = 0;
    bool complete = true;
    int64_t cashNet() const { return receipts - purchases - fees - taxes; }
    int64_t profit(int64_t remainingCost) const { return cashNet() + remainingCost; }
};
struct ClosedVoyage {
    uint32_t id = 0;
    int force = -1, origin = -1, dest = -1, day = 0;
    Ledger ledger;
};

enum : uint8_t { CITY_HUB = 1, CITY_PORT = 2, CITY_CHINA = 4 };

// monsoon sea (trade.bin v2): a rectangle of world hexes and the season of each month
struct Sea {
    int16_t lo0 = 0, lo1 = 0, hi0 = 0, hi1 = 0;
    char months[12] = {};       // 'E' south-west monsoon (eastbound favoured), 'W' north-east (westbound), 'T' change (storms), '-'
    std::string name;
};
// market event template (trade.bin v2)
enum : uint8_t { EV_CONSUMERS = 0, EV_PRODUCERS = 1, EV_ALL = 2, EV_GOOD_ANY = 254, EV_GOOD_ALL = 255 };
struct EventDef {
    std::string name;
    uint8_t apply = 0, good = EV_GOOD_ANY, group = 0;   // group 1..NG, 0 = any
    uint16_t mult = 1000;                               // permille
    uint8_t mMin = 1, mMax = 1;
    uint16_t weight = 1;
};

// known merchant (trade.bin v3): the officer id (roster id = game id), or id -1: Koei's officer (0..849) of that name
// (utf-8, 姓+名), has the merchant talent 商才 1..2
struct MerchantName { std::string name; int32_t id = -1; uint8_t talent = 1; };

// trade.bin (tools/trade_data.py)
struct Data {
    int C = 0, G = 0, version = 0;
    std::vector<std::string> goodName, cityName;   // utf-8
    std::vector<int16_t> lo, hi;                   // city hex (world coordinates)
    std::vector<uint8_t> flags, group;
    std::vector<uint16_t> size;                    // market size in loads
    std::vector<uint32_t> price;                   // C x G: gold per load at price index 1000
    std::vector<uint8_t> supply;                   // C x G: producer level 0..3
    std::vector<uint16_t> demand;                 // v4: C x G consumption multiplier (permille)
    std::vector<uint8_t> cargoUnits;              // v5: tenths of a cargo slot per lot
    std::vector<uint32_t> nativeUnits, minValue;  // v6: pricing lot size and minimum pre-spread value
    int stockCap = 200, producerStock = 100, hubStock = 10; // % market size
    int productionDiv = 30, consumptionDiv = 50;
    std::vector<std::string> groupName;            // v2: market group g (1..NG) is groupName[g - 1]
    std::vector<Sea> seas;                         // v2: monsoon seas
    std::vector<EventDef> events;                  // v2: market events
    std::vector<MerchantName> merchants;           // v3: historical merchants
    int merchantTalent(int person, const std::string& utf8Name) const {
        for (auto& m : merchants)
            if (m.id >= 0 ? m.id == person : person < 850 && m.name == utf8Name) return m.talent;
        return 0;
    }
    bool load(const uint8_t* p, size_t n, std::string* err);
    bool produces(int c, int g) const { return supply[(size_t)c * G + g] != 0; }
    int seaOf(int lo, int hi) const;               // monsoon sea containing the hex, -1 none
    const char* groupLabel(int g) const { return g >= 1 && g <= (int)groupName.size() ? groupName[g - 1].c_str() : "?"; }
    bool canBuy(int c, int g) const { return supply[(size_t)c * G + g] || (flags[c] & CITY_HUB); }
    bool hub(int c) const { return (flags[c] & CITY_HUB) != 0; }
    bool port(int c) const { return (flags[c] & CITY_PORT) != 0; }
    // Tenths of a cargo slot per trade lot; bulk lots share packing space.
    int loadUnits(int g) const;
    uint32_t unitsPerLot(int g) const { return g>=0 && g<(int)nativeUnits.size() ? nativeUnits[g] : 0; }
    int militaryWeapon(int g) const;
    int stockLimit(int c,int g) const { return size[c]*(militaryWeapon(g)>=0 ? 40 : stockCap)/100; }
};

// worldmod.ini trade_* values (percent unless noted)
struct Params {
    int impact = 50;            // price index change when a market of `size` loads takes / gives `size` loads
    int spread = 10;            // buy price above, sell price below the index price (half each side)
    int revert = 10;            // share of the gap to 1000 a city's index recovers per month
    int revertHub = 25;         // the same at hubs
    int idxMin = 300, idxMax = 4000;
    int capLand = 20, capSea = 60;   // ordinary caravan / dedicated merchant ship base slots
    int fee = 100;              // gold per caravan (porters, animals) on top of the goods
    int legFee = 30;            // loading/dispatch charge for an existing caravan's next leg
    int transitTax = 2, transitHub = 1, transitCap = 5; // cap: % departure cargo purchase cost per leg
    // phase 3
    int monsoon = 1;            // seasons on (0: every factor 100)
    int monWith = 70, monAgainst = 140, monStorm = 125;   // percent of the normal sea step cost
    int stormRisk = 10, stormLoss = 30;                   // per turn at sea in a T month: chance %, cargo lost %
    int events = 1, eventChance = 15, eventMax = 4;       // per month: chance % that one starts, at most eventMax running
    int info = 1, infoRange = 40, infoMonths = 12;        // information delay: seen within infoRange hexes; history kept
    // phase 4: merchant officers. Experience per finished trip: merchExpTrip + profit / merchExpDiv; level 1..5 at
    // merchExp[0..4]; effective level = experience level + talent (商才: known merchants 1..2, politics >= merchPol: 1),
    // at most merchMaxLevel. Per level: +merchPP permille on prices, +merchCap % load, -merchTax % of the taxes,
    // -merchRob % of the robbery chance and of the loss.
    int merchExp[5] = { 500, 1500, 3500, 7000, 12000 };
    int merchExpTrip = 100, merchExpDiv = 20, merchPol = 80, merchMaxLevel = 7;
    int merchPP = 8, merchCap = 5, merchTax = 8, merchRob = 12;
};

// phase 4: merchant level and its bonuses (trade.cpp, the panel and the sim use these)
int merchantLevel(const Params& p, int exp);                  // 0..5 from experience
int merchantEffective(const Params& p, int exp, int talent);  // + talent, capped
int merchantTalent(const Params& p, int known, int politics); // known merchant talent, or 1 for politics >= merchPol
int merchantExpGain(const Params& p, int64_t profit);         // one finished trip
struct MerchantBonus { int pp = 0, capPct = 0, taxPct = 100, robPct = 100; };   // pp permille, capPct added %, tax / rob in % of normal
MerchantBonus merchantBonus(const Params& p, int level);

int hexDist(int lo1, int hi1, int lo2, int hi2);

struct Market {
    const Data* d = nullptr;
    Params p;
    std::vector<int32_t> idx;   // C x G price index, 1000 = the base price of trade.bin
    std::vector<uint16_t> mod;  // C x G permille factor of the active events (empty: none)
    std::vector<int32_t> stock;
    int available(int c, int g) const { return stock[(size_t)c * d->G + g]; }
    int room(int c, int g) const { int n=d->stockLimit(c,g)-available(c,g); return n>0 ? n : 0; }

    int64_t base(int c, int g) const {
        int64_t b = d->price[(size_t)c * d->G + g];
        return mod.empty() ? b : b * mod[(size_t)c * d->G + g] / 1000;
    }
    int64_t valueAt(int c, int g, int index) const; // gold x 1000, after event/index and before spread

    void reset();               // every index to 1000
    int32_t& at(int c, int g) { return idx[(size_t)c * d->G + g]; }
    int32_t at(int c, int g) const { return idx[(size_t)c * d->G + g]; }
    // per load at the current index (no quantity effect)
    int buyPrice(int c, int g) const;
    int sellPrice(int c, int g) const;
    // total gold for q loads with the price moving along (quote: index unchanged; buy / sell: index moved)
    int64_t quoteBuy(int c, int g, int q) const { return quote(c, g, q, true); }
    int64_t quoteSell(int c, int g, int q) const { return quote(c, g, q, false); }
    int64_t buy(int c, int g, int q, int pp = 0);
    int64_t sell(int c, int g, int q, int pp = 0);
    int64_t quote(int c, int g, int q, bool buying, int pp = 0) const;
    void month(int phase=0);    // absolute month supplies a stable fractional-production phase

    int64_t walk(int c, int g, int q, bool buying, int32_t* index, int pp = 0) const;
};

// Caravan prices, shared by trade.cpp and the panel (trade_panel.cpp) so the panel shows what a caravan gets.
// pp: the leader's price bonus in permille (politics - 50; + on sales, - on purchases); capPct: load (intelligence).
const int TEMP_POL = 60, TEMP_INT = 50;          // the temporary 商 officer
inline void leaderFactors(int pol, int intel, int* pp, int* capPct) { *pp = pol - 50; *capPct = 100 + (intel - 50) / 2; }
inline int64_t purchase(const Market& m, int c, int g, int q, int pp) { return m.quote(c, g, q, true, pp); }
inline int64_t saleQuote(const Market& m, int c, int g, int q, int pp) { return m.quote(c, g, q, false, pp); }
enum TransportType : int32_t { CARAVAN = 0, MERCHANT_SHIP = 1 };
inline bool validTransport(int type) { return type==CARAVAN || type==MERCHANT_SHIP; }
inline const wchar_t* transportName(int type) { return type==MERCHANT_SHIP ? L"海運商隊" : L"陸路商隊"; }
inline int transportFee(const Params& p,int type,bool nextLeg=false) {
    return type==MERCHANT_SHIP ? (nextLeg ? 60 : 300) : (nextLeg ? p.legFee : p.fee);
}
inline int merchantFee(int fee,bool merchant) {
    return merchant ? (int)(((int64_t)fee*80+99)/100) : fee; // 20% reduction, round up; no stacking.
}
inline int64_t merchantProfitBonus(int64_t net,int64_t cost,bool merchant) {
    return merchant && net>cost ? (net-cost)/5 : 0; // 20% of positive after-tax goods margin, round down.
}
inline int capacity(const Data& d, const Params& p, int origin, int dest, int capPct, int type=CARAVAN) {
    (void)d; (void)origin; (void)dest;
    return (type==MERCHANT_SHIP ? p.capSea : p.capLand) * capPct / 100;
}
inline int taxPct(const Data& d, int c, int tax, int taxHub) { return d.hub(c) ? taxHub : tax; }
// a tax of pct % on value, the merchant pays merchPct % of it (merchantBonus().taxPct; 100: phase 1's value * pct / 100)
inline int64_t taxOf(int64_t value, int pct, int merchPct) { return value * pct * merchPct / 10000; }
inline int64_t taxWithCarry(int64_t value, int pct, int merchPct, int* remainder) {
    int64_t scaled = value * pct * merchPct + *remainder;
    *remainder = (int)(scaled % 10000);
    return scaled / 10000;
}

// The most profitable cargo from `origin` to one of `dests` (indices into Data cities) for at most `budget` gold
// (fee included) and `cap` loads. Profit = sale - purchase - fee - tax% of the sale. Returns false if none gains
// at least margin% of the outlay.
// timePermille (optional, parallel to dests): travel time factor of each destination (monsoon); score = profit / time.
struct Route { int origin = -1, dest = -1, good = -1, qty = 0; int64_t cost = 0, revenue = 0, profit = 0; int dist = 0; };
struct CargoLot {
    int32_t good = 0, origin = 0, qty = 0, initial = 0;
    int64_t cost = 0;
};
struct Cargo {
    std::vector<CargoLot> lots;
    int count(int good = -1) const;
    int64_t cost() const;
    int64_t loadUnits(const Data& d) const;
    void add(int good, int origin, int qty, int64_t cost);
    // Removes exact quantities, returning their weighted cost and the eligible fraction of trip XP.
    bool take(int good, int qty, int dest, int tripXp, int64_t* basis, int* xp);
    int64_t saleBonus(int good,int qty,int dest,int64_t net,bool merchant) const;
};
struct OrderLine { int good = -1, qty = 0; };
// Validates the complete order before any money, stock or unit is changed.
bool quoteOrder(const Market& m, int city, const std::vector<OrderLine>& order,
                int slots, int pp, int64_t budget, int fee, Cargo* cargo, int64_t* total);
int64_t transitCharge(int64_t value, int rate, int merchantPct, int64_t limit, int64_t paid);
bool bestRoute(const Market& m, int origin, const std::vector<int>& dests, int64_t budget, int cap, int taxPct,
               int marginPct, Route* out, const std::vector<int>* timePermille = nullptr,
               int pp = 0, const std::vector<int>* duties = nullptr, int merchantTaxPct = 100, int departureFee = -1, bool merchant = false);

// Deterministic rng (saved with the game so a reload repeats the same draws)
struct Rng {
    uint32_t s = 0x13572468;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    int pct() { return (int)(next() % 100); }
};

// ---- phase 3: monsoon (month0 = 0..11 = January..December)
char season(const Data& d, int sea, int month0);
// percent of the normal step cost on a sea hex of `sea` for a ship heading dir (+1 east, -1 west, 0 unknown)
int seaCostPct(const Data& d, const Params& p, int sea, int month0, int dir);
// Integer movement cost used by the native sea callback and offline route calibration.
int seaStepCost(const Data& d, const Params& p, int sea, int month0, int dir, int cost);
// travel time factor (permille) of a route between two ports: the seas at both ends and half-way; 1000 off the seas
int routeTimePermille(const Data& d, const Params& p, int origin, int dest, int month0);
// a storm month in the sea of this route (risk for a caravan at sea)
bool routeStormy(const Data& d, const Params& p, int origin, int dest, int month0);

// ---- phase 3: market events
struct ActiveEvent { uint8_t def, good, group, pad; int32_t start, end; };   // good 255 = all; months [start, end)
struct Events {
    std::vector<ActiveEvent> live;
    // a new month: drop the ended, maybe start one (p.eventChance, p.eventMax); the changes go to started / ended
    void month(const Data& d, const Params& p, Rng& rng, int month, std::vector<ActiveEvent>* started, std::vector<ActiveEvent>* ended);
    void apply(const Data& d, std::vector<uint16_t>* mod) const;   // C x G permille (empty when none is running)
    bool affects(const Data& d, const ActiveEvent& e, int c, int g) const;
};
std::string describe(const Data& d, const ActiveEvent& e);       // "流行：羅馬 絲 ×1.6" (utf-8)

// ---- phase 3: information delay. A force knows a market as it was the month it last saw it.
struct Info {
    struct Observation { int16_t idx = 0; uint16_t mod = 1000; int32_t stock = 0; };
    int R = 0, C = 0, G = 0, H = 12;
    std::vector<uint8_t> age;       // R x C months since force f last saw city c (255: never)
    std::vector<int16_t> hist;      // H x C x G: the price index at the end of month m in slot m % H
    std::vector<Observation> observed; // R x C x G, frozen at the actual observation
    int32_t histMonth = -1;         // the last month stored
    void reset(int r, const Data& d, int h);
    void endMonth(const Market& m, int month);      // store the month that ended, every age + 1
    void see(int f, int c, const Market* market = nullptr);
    int ageOf(int f, int c) const { return f >= 0 && f < R && c >= 0 && c < C ? age[(size_t)f * C + c] : 255; }
    // the index force f believes for (c, g); false = unknown (never seen, or longer ago than H months)
    bool believed(const Market& m, int f, int c, int g, int32_t* idx) const;
    // m as force f sees it; known[c] = 0 for cities it knows nothing about (their index: 1000, the base price)
    void view(const Market& m, int f, Market* out, std::vector<char>* known) const;
};

// Cities within `range` hexes of each city (precomputed once; range in hexes).
std::vector<std::vector<int>> citiesInRange(const Data& d, int range);


}  // namespace trade
