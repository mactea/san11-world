#pragma once
#include "trade_safety.h"
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

// level: effective merchant level (experience level + talent 商才, trade_model.h merchantEffective); exp: experience
struct Officer { int id; std::wstring name; int pol, intel; int level = 0, exp = 0, talent = 0; };
struct CaravanView {
    int origin, dest, good, qty, cost, lo, hi; bool player, temp, atSea;
    int unit = -1; std::wstring leader; int level = 0; bool autoMarch = false;   // phase 4
    uint32_t id = 0;
    int state = 0, port = -1, maxLoad = 20, payer = -1;
    int shipType = trade::CARAVAN;
    int transferStage = 0;
    trade::Cargo cargo;
    trade::Ledger ledger;
    int routeCost = 0, routeSteps = 0;
    int routeTurns = -1, routeLand = 0, routeWater = 0;
    std::vector<uint32_t> route;
    std::wstring reason;
};
enum : uint32_t { DONE_LOST = 1, DONE_INTERCEPTED = 2, DONE_GAVE_UP = 4 };
struct DoneView { int day, origin, dest, good, qty, cost, net; uint32_t flags; };
struct Config { int tax = 10, taxHub = 5, tempMax = 30, tempKeep = 30, speedLand = 15, speedSea = 28; };

struct Model {
    int shipType = trade::CARAVAN;
    int departureFee = -1; // Explicit next-leg quote; -1 uses formation fee for this type.
    int fundingCity = -1; // Automatic return cargo still belongs to the original funding city.
    std::vector<char> waterAccess; // Direct berth or a connected port owned by this market's force.
    std::vector<int> portBase,portSteps; // Selected native port and bounded land transfer distance (0 for direct access).
    std::vector<char> merchantCities; // Same-force serving merchant skill, physically in each city.
    const trade::Data* d = nullptr;
    trade::Market live;              // the real market (prices where the player is)
    trade::Market seen;              // the market as the player's force knows it (information delay)
    std::vector<char> known;         // per city: the player knows its prices
    std::vector<uint8_t> age;        // per city: months since the player saw it (0 now, 255 never)
    int force = -1;                  // the player's force
    std::vector<int> owner;          // per city: owning force, -1 none
    std::vector<int8_t> stance;      // per city: 2 own / allied (no tax), 1 tax, 0 hostile (robbery), -1 no owner
    std::vector<int> saleTaxRemainder; // player's fractional sales tax by the city's owning force
    std::vector<int> gold;           // per city: gold of the player's cities, -1 elsewhere
    std::vector<int> troops, food;
    int escortNeeded = 500, foodNeeded = 3000;
    std::vector<int> grainRate; // Native city +0x7c, own cities only; 0 = unknown.
    std::vector<Officer> officers;   // the player's officers in the selected origin city
    std::vector<CaravanView> caravans;   // the player's caravans and the AI caravans the player can see
    std::vector<DoneView> done;      // recent finished trips of the player, newest first
    std::vector<trade::ClosedVoyage> closed;
    int64_t pendingGold = 0;
    std::vector<trade::ActiveEvent> events;
    int month = 0;                   // months since 1/1/1 (game calendar)
    int tempLive = 0, freeSpecials = 100;
    Config cfg;
    std::wstring lastResult;         // result of the last 「出發」 (runs queued, see trade.cpp)
    bool playerAuto = true;          // trade_player_auto: the player's caravans march by themselves (per caravan opt-out)
};

inline bool quoteMerchant(const Model& m,int origin) {
    int city=m.fundingCity>=0 ? m.fundingCity : origin;
    return city>=0 && city<(int)m.merchantCities.size() && m.merchantCities[city];
}
inline int transferSteps(const Model& m,int city) {
    return m.shipType==trade::MERCHANT_SHIP && city>=0 && city<(int)m.portSteps.size() ? m.portSteps[city] : 0;
}
inline int transferFee(const Model& m,int origin,int dest) {return 2*(transferSteps(m,origin)+transferSteps(m,dest));}
inline int quoteFee(const Model& m,int origin,int dest=-1) {
    int fee=m.departureFee>=0 ? m.departureFee : trade::merchantFee(trade::transportFee(m.live.p,m.shipType),quoteMerchant(m,origin));
    return fee+transferFee(m,origin,dest); // Handoff fees are per leg, separate from the organizer discount.
}
inline bool shipMarket(const Model& m,int city) {
    return m.d && city>=0 && city<m.d->C &&
        city<(int)m.waterAccess.size() && m.waterAccess[city];
}
inline bool transportRoute(const Model& m,int origin,int dest) {
    return trade::validTransport(m.shipType) && (m.shipType==trade::CARAVAN || (shipMarket(m,origin) && shipMarket(m,dest)));
}

struct State {
    bool on = false;
    int tab = 0;                     // 0 市場 1 組隊 2 商隊 3 消息
    int origin = -1, dest = -1, good = -1, qty = 10;
    int leader = -1;                 // officer id; -1 = 無（商隊）: a temporary 商
    int leaderScroll = 0, carScroll = 0;
    std::wstring msg; bool msgError = false;
    bool sendPending = false;        // a send is queued and its result not seen yet
    int carSel = 0;                  // keyboard: the selected caravan in 商隊
    bool confirm = false;            // keyboard: Enter pressed once, a second Enter sends
    bool qtyTyping = false;          // keyboard: digits typed into the quantity
    bool accounts = false;           // caravan page: cargo controls / accounts and recent settlements
    int closedScroll = 0, doneScroll = 0;
};

struct SendRequest { int origin = -1, dest = -1, good = -1, qty = 0, leader = -1; };

// a planned trip with the formulas of trade.cpp (createCaravan / caravanTurn)
struct Plan {
    bool ok = false; std::wstring error;  // error: why 「出發」 would fail
    int cap = 0, qty = 0, pricePermille = 0, level = 0;   // pricePermille includes the merchant level's bonus
    int64_t goods = 0, cost = 0, revenue = 0, duty = 0, transit = 0, net = 0, profit = 0;
    int64_t merchantBonus = 0; // Extra receipts, excluded from the market's taxable sale price.
    int transitCities = 0, hostileCities = 0, dist = 0, eta = 0, provisions = 0;
    bool known = true, quoted = false;
};
// withTransit = false: no transit estimate (the map colours: 530 plans per repaint)
Plan plan(const Model& m, int origin, int dest, int good, int qty, int leader, bool withTransit = true);
struct BasketPlan : Plan { int slots=0; int64_t used=0, transitLimit=0; };
BasketPlan basketPlan(const Model& m,int origin,int dest,const std::vector<trade::OrderLine>& order,int leader);
std::vector<trade::OrderLine> recommendOrder(const Model& m,int origin,int dest,int leader);
int maxOrderQuantity(const Model& m,int origin,int dest,const std::vector<trade::OrderLine>& order,int good,int leader);
// gold per load and margin for the map colours; false = unknown market (grey)
bool cityProfit(const Model& m, const State& s, int city, int64_t* profit, int64_t* cost);

// hex -> map-area pixel (x, y set even when off the map); false: off the map (or under the panel)
typedef std::function<bool(int lo, int hi, float* x, float* y)> Projector;
int cityAt(const Model& m, const Projector& proj, int x, int y, int radius);  // nearest city to (x, y), -1 none

// mouse (map-area pixels). Repaint: what changed.
enum : int { REPAINT_NONE = 0, REPAINT_PANEL = 1, REPAINT_MAP = 2, REPAINT_LAYOUT = 4 };   // MAP includes the panel; LAYOUT: marker clip changed
struct Result {
    std::string command;
    bool consumed = false; int repaint = REPAINT_NONE; bool send = false; SendRequest req;
    int toggleUnit = -1; bool toggleAuto = false;   // phase 4: switch a caravan's 自動行軍 (unit id)
};
Result click(Model& m, State& s, const Projector& proj, int x, int y);
Result wheel(Model& m, State& s, int x, int y, int delta, bool shift);
// keyboard (WM_KEYDOWN virtual key). Panel closed: nothing is consumed. Open: Tab / Shift+Tab and 1-4 (not on 組隊)
// switch pages; arrows, PageUp / PageDown move the selection of the page's list (商品, 統領, 商隊; Left / Right the
// goods on 組隊); + / - quantity (Shift: 10); digits on 組隊 type the quantity; Enter on 組隊 asks, a second Enter
// sends (市場: go to 組隊); A on 商隊 switches 自動行軍; Esc cancels the question, else closes the panel.
Result key(Model& m, State& s, int vk, bool shift);
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
