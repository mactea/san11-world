#pragma once
// M13 trade, phase 2: the 「貿易」 panel of the 中地圖 (trade_panel.cpp, M13_TRADE.md "第二期").
//
// No game code here: trade.cpp fills a Model from the game (under its lock) and calls click / wheel / the two
// renderers; worldmod/test/trade_panel_test.cpp does the same with a fake model. Coordinates are pixels of the
// midmap's 600x600 map area (the panel's 14-pixel frame removed). With the panel open the right PANEL_W pixels
// are the panel and the map shows in the left PANEL_X.
#include <stdint.h>
#include <string>
#include <vector>
#include <functional>
#include "trade_model.h"

namespace tradeui {

const int MAP_W = 600, MAP_H = 600;
const int PANEL_W = 230, PANEL_X = MAP_W - PANEL_W;              // panel: x PANEL_X..MAP_W-1, y 0..MAP_H-1
const int TOGGLE_X = MAP_W - 52, TOGGLE_Y = 4, TOGGLE_W = 48, TOGGLE_H = 20;   // the 「貿易」 button (panel closed)

struct Officer { int id; std::wstring name; int pol, intel; };
struct CaravanView { int origin, dest, good, qty, cost, lo, hi; bool player, temp, atSea; };
enum : uint32_t { DONE_LOST = 1, DONE_INTERCEPTED = 2, DONE_GAVE_UP = 4 };
struct DoneView { int day, origin, dest, good, qty, cost, net; uint32_t flags; };
struct Config { int tax = 10, taxHub = 5, tempMax = 30, tempKeep = 30, speedLand = 5, speedSea = 7; };

struct Model {
    const trade::Data* d = nullptr;
    trade::Market live;              // the real market (prices where the player is)
    trade::Market seen;              // the market as the player's force knows it (information delay)
    std::vector<char> known;         // per city: the player knows its prices
    std::vector<uint8_t> age;        // per city: months since the player saw it (0 now, 255 never)
    int force = -1;                  // the player's force
    std::vector<int> owner;          // per city: owning force, -1 none
    std::vector<int8_t> stance;      // per city: 2 own / allied (no tax), 1 tax, 0 hostile (robbery), -1 no owner
    std::vector<int> gold;           // per city: gold of the player's cities, -1 elsewhere
    std::vector<Officer> officers;   // the player's officers in the selected origin city
    std::vector<CaravanView> caravans;   // the player's caravans and the AI caravans the player can see
    std::vector<DoneView> done;      // recent finished trips of the player, newest first
    std::vector<trade::ActiveEvent> events;
    int month = 0;                   // months since 1/1/1 (game calendar)
    int tempLive = 0, freeSpecials = 100;
    Config cfg;
    std::wstring lastResult;         // result of the last 「出發」 (runs queued, see trade.cpp)
};

struct State {
    bool on = false;
    int tab = 0;                     // 0 市場 1 組隊 2 商隊 3 消息
    int origin = -1, dest = -1, good = -1, qty = 10;
    int leader = -1;                 // officer id; -1 = 無（商隊）: a temporary 商
    int leaderScroll = 0, carScroll = 0;
    std::wstring msg; bool msgError = false;
    bool sendPending = false;        // a send is queued and its result not seen yet
};

struct SendRequest { int origin = -1, dest = -1, good = -1, qty = 0, leader = -1; };

// a planned trip with the formulas of trade.cpp (createCaravan / caravanTurn)
struct Plan {
    bool ok = false; std::wstring error;  // error: why 「出發」 would fail
    int cap = 0, qty = 0, pricePermille = 0;
    int64_t goods = 0, cost = 0, revenue = 0, duty = 0, transit = 0, net = 0, profit = 0;
    int transitCities = 0, hostileCities = 0, dist = 0, eta = 0, timePermille = 1000;
    bool known = true, sea = false, stormy = false;
};
// withTransit = false: no transit estimate (the map colours: 530 plans per repaint)
Plan plan(const Model& m, int origin, int dest, int good, int qty, int leader, bool withTransit = true);
// gold per load and margin for the map colours; false = unknown market (grey)
bool cityProfit(const Model& m, const State& s, int city, int64_t* profit, int64_t* cost);

// hex -> map-area pixel (x, y set even when off the map); false: off the map (or under the panel)
typedef std::function<bool(int lo, int hi, float* x, float* y)> Projector;
int cityAt(const Model& m, const Projector& proj, int x, int y, int radius);  // nearest city to (x, y), -1 none

// mouse (map-area pixels). Repaint: what changed.
enum : int { REPAINT_NONE = 0, REPAINT_PANEL = 1, REPAINT_MAP = 2, REPAINT_LAYOUT = 4 };   // MAP includes the panel; LAYOUT: marker clip changed
struct Result { bool consumed = false; int repaint = REPAINT_NONE; bool send = false; SendRequest req; };
Result click(Model& m, State& s, const Projector& proj, int x, int y);
Result wheel(Model& m, State& s, int x, int y, int delta, bool shift);
void selectCity(Model& m, State& s, int city);   // first click: origin; second: destination; then a new origin
void validate(Model& m, State& s);               // after the model is rebuilt: drop selections that became invalid
bool inPanel(const State& s, int x, int y);
bool onToggle(const State& s, int x, int y);

// rendering into ARGB pixels: the panel (PANEL_W x MAP_H at dst) and the map overlay (the map area at dst:
// city colours, routes, caravans; with the panel closed only the 「貿易」 button)
void renderPanel(const Model& m, const State& s, uint32_t* dst, int pitch);
void renderMap(const Model& m, const State& s, const Projector& proj, uint32_t* dst, int pitch);

// text helpers (exposed for the test)
std::wstring wide(const std::string& utf8);
std::wstring fmtGold(int64_t v, bool sign = false);

}  // namespace tradeui
