#pragma once
// M13 trade: the economy model (no game code; trade.cpp drives it in the game, tools/trade_sim runs it offline).
// M13_TRADE.md has the rules and the trade.bin format.
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

namespace trade {

enum : uint8_t { CITY_HUB = 1, CITY_PORT = 2, CITY_CHINA = 4 };

// trade.bin (tools/trade_data.py)
struct Data {
    int C = 0, G = 0;
    std::vector<std::string> goodName, cityName;   // utf-8
    std::vector<int16_t> lo, hi;                   // city hex (world coordinates)
    std::vector<uint8_t> flags, group;
    std::vector<uint16_t> size;                    // market size in loads
    std::vector<uint32_t> price;                   // C x G: gold per load at price index 1000
    std::vector<uint8_t> supply;                   // C x G: producer level 0..3
    bool load(const uint8_t* p, size_t n, std::string* err);
    bool canBuy(int c, int g) const { return supply[(size_t)c * G + g] || (flags[c] & CITY_HUB); }
    bool hub(int c) const { return (flags[c] & CITY_HUB) != 0; }
    bool port(int c) const { return (flags[c] & CITY_PORT) != 0; }
};

// worldmod.ini trade_* values (percent unless noted)
struct Params {
    int impact = 50;            // price index change when a market of `size` loads takes / gives `size` loads
    int spread = 10;            // buy price above, sell price below the index price (half each side)
    int revert = 10;            // share of the gap to 1000 a city's index recovers per month
    int revertHub = 25;         // the same at hubs
    int idxMin = 300, idxMax = 4000;
    int capLand = 20, capSea = 60;   // loads per caravan (both ends ports: sea)
    int fee = 300;              // gold per caravan (porters, animals) on top of the goods
};

int hexDist(int lo1, int hi1, int lo2, int hi2);

struct Market {
    const Data* d = nullptr;
    Params p;
    std::vector<int32_t> idx;   // C x G price index, 1000 = the base price of trade.bin

    void reset();               // every index to 1000
    int32_t& at(int c, int g) { return idx[(size_t)c * d->G + g]; }
    int32_t at(int c, int g) const { return idx[(size_t)c * d->G + g]; }
    // per load at the current index (no quantity effect)
    int buyPrice(int c, int g) const;
    int sellPrice(int c, int g) const;
    // total gold for q loads with the price moving along (quote: index unchanged; buy / sell: index moved)
    int64_t quoteBuy(int c, int g, int q) const { int32_t i = at(c, g); return walk(c, g, q, true, &i); }
    int64_t quoteSell(int c, int g, int q) const { int32_t i = at(c, g); return walk(c, g, q, false, &i); }
    int64_t buy(int c, int g, int q) { return walk(c, g, q, true, &at(c, g)); }
    int64_t sell(int c, int g, int q) { return walk(c, g, q, false, &at(c, g)); }
    void month();               // reversion towards 1000

    int64_t walk(int c, int g, int q, bool buying, int32_t* index) const;
};

// The most profitable cargo from `origin` to one of `dests` (indices into Data cities) for at most `budget` gold
// (fee included) and `cap` loads. Profit = sale - purchase - fee - tax% of the sale. Returns false if none gains
// at least margin% of the outlay.
struct Route { int origin = -1, dest = -1, good = -1, qty = 0; int64_t cost = 0, revenue = 0, profit = 0; int dist = 0; };
bool bestRoute(const Market& m, int origin, const std::vector<int>& dests, int64_t budget, int cap, int taxPct,
               int marginPct, Route* out);

// Cities within `range` hexes of each city (precomputed once; range in hexes).
std::vector<std::vector<int>> citiesInRange(const Data& d, int range);

// Deterministic rng (saved with the game so a reload repeats the same draws)
struct Rng {
    uint32_t s = 0x13572468;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    int pct() { return (int)(next() % 100); }
};

}  // namespace trade
