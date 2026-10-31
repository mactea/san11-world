// trade: M13 trade system, phase 1 (M13_TRADE.md).
//
// Caravans are real transport units (部隊 +8 == 1) of the force that sends them. The game's own pieces:
// - unit creation: the sortie record (0x80 bytes) given to 0x5883f0 (command queue 0x96a59d8): called from the logic
//   thread it is posted to the main thread and waited for, from the main thread it runs 0x5db430 directly (the
//   player's sortie / transport command and the event script's create_unit 0x6772c0 build the same record). 0x5db430
//   does not take troops / gold / food from the base; the trade code takes the purchase price itself.
// - leader without an officer: a temporary special officer (PS+150..PT-1) like the game's bandit / barbarian leaders
//   (0x4af0e0): 0x4aa610 copies a template into the first free special slot, home base +0x98, corps 0x4a32f0,
//   status 0x4a5af0(3); renamed 「商」 so the unit shows as 商隊. 0x4bccd0 (unit removal) releases special officers
//   (0x489d10 / 0x4acae0) by itself.
// - movement: unit order 1 (移動) to a hex next to the destination city (0x5a8d20 on the AI order executor
//   0x96a9350, coordinate target), executed by 0x5ad980 (AI turn: 0x5ddcf0; the player: 一括実行 or by hand).
// - gold: 0x4ae2a0(base, delta) (clamped to the base's 0..max), 0x486c80 reads it.
// Hooks (trade=1 only):
//   0x58051e  call 0x4a1180 (logic thread, after every force has moved: the 旬 counter world+0x5c += 1) -> also the
//             trade turn: arrivals, transit tax / robbery, orders, losses; monthly: price reversion + AI caravans
//   0x5def90  AI unit planning (thiscall(AI force object, unit) ret 4): a caravan gets its order, planning skipped
//   0x4bfa30  unit annihilated (wrapper of 0x4be790: unit, attacker, a3, a4): the cargo's value goes to the victor;
//             a temporary leader is never captured: the unit is removed (0x4bccd0) and its officer recycled
//   0x57ff80  main loop's command pump (main thread): queued automation commands run here while the game waits
//             for the player (0x57dcd0)
// Phase 2 (trade_ui=1, needs the world 中地圖 of worldmod.cpp): the MapUI vtable slots +0x90 / +0x94 / +0xa8 (left
// button down / up, wheel; already worldmod's midmap handlers) are chained: the 「貿易」 button, the panel and the
// city picks are ours, everything else goes on to the midmap. The panel and the map overlay are drawn into the
// midmap texture by midmap.cpp's overlay callback (each compose) or, for panel-only changes, by locking the
// texture's panel rectangle (trade_panel.cpp renders). 「出發」 queues "trade send" like the automation command.
// Phase 3: monsoon (sea step cost of caravans through movement.cpp's sea-cost callback, storms, AI route times),
// market events (price factors), information delay (what each force knows of other markets); TRAD block v2.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "trade.h"
#include "trade_model.h"
#include "trade_panel.h"
#include "midmap.h"
#include "movement.h"

using namespace trade;

namespace {

// ---------------------------------------------------------------- game objects and functions
const uint32_t WORLD = 0x7201958, PROC = 0x799895c, EXEC = 0x96a9350, CMDQ = 0x96a59d8;

typedef uint8_t* (__thiscall* GetFn)(uint32_t world, int id);
typedef int (__cdecl* ValidFn)(const void* obj);
typedef int (__thiscall* IdOfFn)(uint32_t world, const void* obj);
typedef int (__thiscall* AddGoldFn)(uint32_t proc, void* base, int delta);
typedef int (__thiscall* GoldFn)(void* base);
typedef uint8_t* (__thiscall* TempNewFn)(uint32_t proc, void* tmpl, int base, int loc);
typedef void (__thiscall* PersonFn)(uint32_t proc, void* person);
typedef void (__thiscall* PersonIntFn)(uint32_t proc, void* person, int v);
typedef int (__thiscall* CountFn)(uint32_t world);
typedef int (__thiscall* SortieFn)(uint32_t cmdq, void* rec);
typedef void (__thiscall* RemoveFn)(uint32_t proc, void* unit, int flag);
typedef int (__thiscall* OrderFn)(uint32_t exec, void* unit, int type, uint32_t pos, int extra);
typedef int (__thiscall* ExecFn)(uint32_t exec, void* unit);
typedef int (__thiscall* IdleFn)(uint32_t cmdq);
typedef void (__thiscall* SetIntFn)(void* obj, int v);
typedef void (__thiscall* SetAbilFn)(void* person, int i, int v);
typedef uint8_t (__thiscall* AbilFn)(void* person, int i);
typedef uint8_t (__thiscall* RelFn)(void* force, int other);
typedef int (__thiscall* MaskFn)(void* force, int other);
typedef int (__thiscall* VIntFn)(void* obj);
typedef int (__thiscall* HomeFn)(void* unit);
typedef int (__thiscall* StreamIoFn)(void* stream, void* buf, int n);
typedef int (__thiscall* StreamKindFn)(void* stream);

const GetFn unitRec = (GetFn)0x490e70, personRec = (GetFn)0x490b00, baseRec = (GetFn)0x490d00, forceRec = (GetFn)0x490aa0;
const ValidFn gameValid = (ValidFn)0x47a630;
// 0x47a630 calls the object's vfunc +8: guard against records never constructed (no vtable)
int objValid(const void* o) { return o && *(void* const*)o && gameValid(o); }
const IdOfFn unitIdOf = (IdOfFn)0x4917c0, personIdOf = (IdOfFn)0x491310;
const AddGoldFn addGold = (AddGoldFn)0x4ae2a0;
const GoldFn baseGold = (GoldFn)0x486c80;
const TempNewFn tempNew = (TempNewFn)0x4aa610;
const PersonFn tempFree = (PersonFn)0x4acae0;
const PersonIntFn setCorps = (PersonIntFn)0x4a32f0, setStatus = (PersonIntFn)0x4a5af0;
const CountFn freeSpecials = (CountFn)0x4910e0;
const SortieFn sortie = (SortieFn)0x5883f0;
const RemoveFn removeUnit = (RemoveFn)0x4bccd0;
const OrderFn setOrder = (OrderFn)0x5a8d20;
const ExecFn execOrder = (ExecFn)0x5ad980;
const IdleFn waitingForPlayer = (IdleFn)0x57dcd0;
const SetAbilFn setAbility = (SetAbilFn)0x48a7d0;
const AbilFn ability = (AbilFn)0x489030;
const SetIntFn setBirth = (SetIntFn)0x489330, setDeath = (SetIntFn)0x4898d0, setAppear = (SetIntFn)0x489310;
const RelFn relation = (RelFn)0x4814e0;
const MaskFn allied = (MaskFn)0x4811b0;
const HomeFn unitHome = (HomeFn)0x495520;
const StreamIoFn streamRead = (StreamIoFn)0x46ff20, streamWrite = (StreamIoFn)0x470180;
const StreamKindFn isSaveKind = (StreamKindFn)0x43a8f0, isScenarioKind = (StreamKindFn)0x43a8a0;

int vcall(void* obj, int off) { return ((VIntFn)(*(void***)obj)[off / 4])(obj); }
const int16_t* posOf(void* obj) { return (const int16_t*)((uint8_t* (__thiscall*)(void*))(*(void***)obj)[0x3c / 4])(obj); }
uint32_t pack(int lo, int hi) { return (uint16_t)lo | (uint32_t)(uint16_t)hi << 16; }

// ---------------------------------------------------------------- settings (worldmod.ini)
struct Cfg {
    int on = 0;
    int tempMax = 30, tempKeep = 30, tmpl = 745;
    int ai = 1, aiHook = 1, aiMax = 2, aiMonth = 6, aiRange = 80, aiMinGold = 6000, aiInvest = 4000, aiReserve = 2000, aiMargin = 25;
    int tax = 10, taxHub = 5, relTax = 40, rob = 15, robLoss = 50, loot = 60;
    int escort = 500, food = 3000, arrive = 3, maxTurns = 90, playerAuto = 0;
    int ui = 1, uiAi = 1, monsoonAll = 0, PT = 1100;        // phase 2 panel, AI caravans on the map; phase 3; officer records
} g_cfg;

TraceLogFn g_log = nullptr;
TradeSetup g_s;
bool g_on = false;
Data g_data;
Market g_mk;
Rng g_rng;
std::vector<std::vector<int>> g_inRange;
CRITICAL_SECTION g_cs;
struct Lock { Lock() { EnterCriticalSection(&g_cs); } ~Lock() { LeaveCriticalSection(&g_cs); } };

enum : uint32_t { F_TEMP = 1, F_PLAYER = 2, F_LOOTED = 4 };
struct Caravan {
    int32_t unit, leader, force, corps, origin, dest, good, qty, cost, taxDue, born, lastCity;
    uint32_t flags;
};
std::vector<Caravan> g_car;
int32_t g_lastMonth = -1;
uint32_t g_nextAiForce = 0;
// phase 3
Events g_ev;
Info g_info;
std::vector<std::vector<int>> g_infoNear;      // cities within trade_info_range of each city
struct DoneRec { int32_t day, force, origin, dest, good, qty, cost, net; uint32_t flags; };   // a finished trip (tradeui::DONE_*)
std::vector<DoneRec> g_done;                   // the players' last trips, newest last (at most DONE_MAX)
const size_t DONE_MAX = 24;
std::vector<int8_t> g_unitDir;                 // per unit: caravan heading (1 east, -1 west, 2 neither), 0 = not a caravan
uint8_t* g_unit0 = nullptr;                    // unit 0 (the sea-cost callback's pointer -> index)
int g_month0 = 0;                              // month of the year (0 = January) at the last turn

int today() {      // days since 1/1/1: the start date world +8 / +0xc / +0x10 plus 10 days a 旬 (+0x5c)
    const int* w = (const int*)(uintptr_t)WORLD;
    return (((w[2] - 1) * 12 + w[3] - 1) * 3 + w[0x5c / 4]) * 10 + w[4] - 1;
}

// ---------------------------------------------------------------- map helpers
uint8_t* cell(int lo, int hi) {
    if (lo < 0 || hi < 0 || lo >= g_s.H || hi >= g_s.W) return nullptr;
    return g_s.hex20 + ((size_t)lo * g_s.W + hi) * 20;
}
bool freeHex(int lo, int hi) {
    uint8_t* c = cell(lo, hi);
    if (!c) return false;
    int terr = *(uint32_t*)(c + 4) & 0x1f;
    if ((*(uint32_t*)c & 3) != 0) return false;                        // a building or a unit there
    return terr <= 14 || terr == 19;                                    // not 崖 (15), 城 港 関 (16..18)
}
// a free hex 1..2 from (lo, hi), nearest to (flo, fhi); false if none
bool hexNear(int lo, int hi, int flo, int fhi, int* olo, int* ohi) {
    int best = 1 << 30;
    for (int r = lo - 3; r <= lo + 3; r++)
        for (int c = hi - 3; c <= hi + 3; c++) {
            int d = hexDist(lo, hi, r, c);
            if (d < 1 || d > 2 || !freeHex(r, c)) continue;
            int e = hexDist(r, c, flo, fhi) * 4 + d;
            if (e < best) { best = e; *olo = r; *ohi = c; }
        }
    return best < (1 << 30);
}

uint8_t* base(int c) { uint8_t* b = c >= 0 && c < g_s.C ? baseRec(WORLD, c) : nullptr; return b && objValid(b) ? b : nullptr; }
int baseForce(int c) { uint8_t* b = base(c); return b ? vcall(b, 0x40) : -1; }
bool cityPos(int c, int* lo, int* hi) {
    uint8_t* b = base(c);
    if (!b) { *lo = g_data.lo[c]; *hi = g_data.hi[c]; return false; }
    const int16_t* p = posOf(b); *lo = p[0]; *hi = p[1]; return true;
}
int nearestCity(int lo, int hi, int maxDist) {
    int best = -1, bd = maxDist + 1;
    for (int c = 0; c < g_data.C; c++) {
        int d = hexDist(lo, hi, g_data.lo[c], g_data.hi[c]);
        if (d < bd) { bd = d; best = c; }
    }
    return best;
}
uint8_t* liveUnit(const Caravan& k) {
    if (k.unit < 0 || k.unit >= g_s.U) return nullptr;
    uint8_t* u = unitRec(WORLD, k.unit);
    return u && objValid(u) && *(int*)(u + 0xc) == k.leader ? u : nullptr;
}
bool isUnitPtr(const uint8_t* p) {
    uint8_t* u0 = unitRec(WORLD, 0);
    return u0 && p >= u0 && p < u0 + (size_t)g_s.U * 0xf4 && (p - u0) % 0xf4 == 0;
}
bool isBasePtr(const uint8_t* p) {
    uint8_t* b0 = baseRec(WORLD, 0);
    return b0 && p >= b0 && p < b0 + (size_t)0x4000 * 0x38 && (p - b0) % 0x38 == 0;
}
// relation of force a to force b: 2 same / allied, 1 normal (tax), 0 hostile (robbery)
int stance(int a, int b) {
    if (a == b) return 2;
    if (a < 0 || b < 0) return 1;
    uint8_t* fa = forceRec(WORLD, a);
    if (!fa || !objValid(fa)) return 1;
    if (allied(fa, b)) return 2;
    return relation(fa, b) >= g_cfg.relTax ? 1 : 0;
}
bool isPlayerForce(int f) {
    uint8_t* fo = f >= 0 ? forceRec(WORLD, f) : nullptr;
    return fo && objValid(fo) && vcall(fo, 0x48) != 0;
}
int tempLive() { int n = 0; for (auto& k : g_car) n += (k.flags & F_TEMP) != 0; return n; }
const char* goodName(int g) { return g >= 0 && g < g_data.G ? g_data.goodName[g].c_str() : "?"; }
const char* cityName(int c) { return c >= 0 && c < g_data.C ? g_data.cityName[c].c_str() : "?"; }

// a temporary leader still holding the slot we gave it (name 商, our corps) -> release it
// (0x4bccd0 releases it when the unit is removed; this catches a leader taken away from its unit, e.g. captured).
void releaseTempIfOurs(const Caravan& k) {
    uint8_t* p = personRec(WORLD, k.leader);
    if (!p || !objValid(p)) return;
    if (p[4] != 0xb0 || p[5] != 0xd3 || p[6] != 0 || *(int*)(p + 0x94) != k.corps) return;
    int users = 0;
    for (auto& o : g_car) users += o.leader == k.leader;
    if (users > 1) return;                                   // the slot already leads another caravan
    if (*(int*)(p + 0x9c) >= g_s.C + 45) return;             // in a unit (locations N.. are units, N = C + 10 gates + 35 ports)
    tempFree(PROC, p);
}

// leader bonus: politics +-5% on prices, intelligence +-25% on the load
void leaderBonus(int leader, int* pricePermille, int* capPct) {
    uint8_t* p = personRec(WORLD, leader);
    int pol = p ? ability(p, 3) : 50, intel = p ? ability(p, 2) : 50;
    leaderFactors(pol, intel, pricePermille, capPct);   // + on sales, - on purchases (permille); load (trade_model.h)
}
void leaderOrTemp(const Caravan& k, int* pp, int* capPct) {
    if (k.flags & F_TEMP) leaderFactors(TEMP_POL, TEMP_INT, pp, capPct);
    else leaderBonus(k.leader, pp, capPct);
}
int8_t headingOf(const Caravan& k) {
    int d = g_data.lo[k.dest] - g_data.lo[k.origin];
    return d > 0 ? 1 : d < 0 ? -1 : 2;
}
void rebuildDirs() {                                     // the sea-cost callback's per-unit table (phase 3); updated in
    if (g_unitDir.size() != (size_t)g_s.U) return;       // place (allocated once at install: the callback may run on another thread)
    std::vector<int8_t> dirs(g_s.U, 0);
    for (auto& k : g_car) if (k.unit >= 0 && k.unit < g_s.U) dirs[k.unit] = headingOf(k);
    for (int i = 0; i < g_s.U; i++) if (g_unitDir[i] != dirs[i]) g_unitDir[i] = dirs[i];
}
void recordDone(const Caravan& k, int64_t net, uint32_t flags) {
    if (!(k.flags & F_PLAYER)) return;
    DoneRec r = { today(), k.force, k.origin, k.dest, k.good, k.qty, k.cost, (int32_t)net, flags };
    Lock l;
    g_done.push_back(r);
    if (g_done.size() > DONE_MAX) g_done.erase(g_done.begin());
}

// ---------------------------------------------------------------- caravans
void issueOrder(const Caravan& k, uint8_t* u) {
    int dlo, dhi, tlo, thi;
    cityPos(k.dest, &dlo, &dhi);
    const int16_t* up = posOf(u);
    if (!hexNear(dlo, dhi, up[0], up[1], &tlo, &thi)) { tlo = dlo; thi = dhi; }
    setOrder(EXEC, u, 1, pack(tlo, thi), -1);
}

// creates a caravan; leader < 0: temporary 商 officer. Returns "" or an error.
std::string createCaravan(int origin, int dest, int good, int qty, int leader, bool ai, Caravan* out) {
    if (origin < 0 || origin >= g_data.C || dest < 0 || dest >= g_data.C || origin == dest) return "bad cities";
    if (good < 0 || good >= g_data.G) return "bad good";
    if (!g_data.canBuy(origin, good)) return std::string(cityName(origin)) + " does not sell " + goodName(good);
    uint8_t* b = base(origin);
    if (!b) return "origin is not a live base";
    int force = vcall(b, 0x40), corps = vcall(b, 0x44);
    if (force < 0 || force >= g_s.R) return "origin has no regular force";
    bool temp = leader < 0;
    if (!temp) {
        uint8_t* p = personRec(WORLD, leader);
        if (!p || !objValid(p)) return "no such officer";
        if (vcall(p, 0x40) != force) return "the officer is not of the origin's force";
        if (*(int*)(p + 0x9c) != origin) return "the officer is not in the origin city";
    } else {
        if (tempLive() >= g_cfg.tempMax) return "temporary caravan limit (trade_temp_max) reached";
        if (freeSpecials(WORLD) - 1 < g_cfg.tempKeep) return "too few free special officer slots (trade_temp_keep)";
    }
    int pp = 0, capPct = 100;
    if (!temp) leaderBonus(leader, &pp, &capPct);
    else leaderFactors(TEMP_POL, TEMP_INT, &pp, &capPct);   // 商: politics 60, intelligence 50
    int cap = capacity(g_data, g_mk.p, origin, dest, capPct);
    if (qty > cap) qty = cap;
    if (qty <= 0) return "nothing to carry";
    int64_t goods = purchase(g_mk, origin, good, qty, pp);
    int64_t cost = goods + g_mk.p.fee;
    int gold = baseGold(b);
    if (cost > gold) return "not enough gold (" + std::to_string(cost) + " needed, " + std::to_string(gold) + " in the city)";
    int sx, sy; cityPos(origin, &sx, &sy);
    int tlo, thi;
    if (!hexNear(sx, sy, sx, sy, &tlo, &thi)) return "no free hex next to the origin city";
    // the leader
    uint8_t* p;
    if (temp) {
        uint8_t* tmpl = personRec(WORLD, g_cfg.tmpl);
        if (!tmpl || !objValid(tmpl)) return "template officer (trade_template) missing";
        p = tempNew(PROC, tmpl, -1, -1);
        if (!p || !objValid(p)) return "no free special officer slot";
        *(int*)(p + 0x98) = origin;                          // home base (as 0x4af0e0 does)
        setCorps(PROC, p, corps);
        setStatus(PROC, p, 3);
        memset(p + 4, 0, 0x13 - 4 + 25);                     // 姓 名 字 and the reading
        p[4] = 0xb0; p[5] = 0xd3;                            // 「商」 (Big5), no given name: the unit shows as 商隊
        static const int abil[5] = { 40, 30, 50, 60, 50 };   // 統率 武力 智力 政治 魅力
        for (int i = 0; i < 5; i++) setAbility(p, i, abil[i]);
        int year = today() / 360 + 1;
        setBirth(p, year - 30); setDeath(p, year + 60 > 0x161 ? 0x161 : year + 60); setAppear(p, year - 10);
        ((void(__thiscall*)(void*))(*(void***)p)[0x34 / 4])(p);   // recompute the current abilities (as 0x4af2fb)
        leader = personIdOf(WORLD, p);
    } else p = personRec(WORLD, leader);
    // the transport unit (sortie record, 0x80 bytes)
    uint8_t rec[0x80] = {};
    *(void**)(rec + 0) = b;
    *(int*)(rec + 4) = 1;                                    // transport
    *(int*)(rec + 8) = leader; *(int*)(rec + 0xc) = -1; *(int*)(rec + 0x10) = -1;
    *(int*)(rec + 0x14) = 0;                                 // gold carried: none (the cargo is the trade code's)
    *(int*)(rec + 0x18) = g_cfg.food;
    *(int*)(rec + 0x1c) = g_cfg.escort;
    for (int i = 0; i < 12; i++) *(int*)(rec + 0x20 + 4 * i) = -1;
    int uid = sortie(CMDQ, rec);
    uint8_t* u = uid >= 0 && uid < g_s.U ? unitRec(WORLD, uid) : nullptr;
    if (!u || !objValid(u) || *(int*)(u + 0xc) != leader) {
        if (temp) tempFree(PROC, p);
        return "the game did not create the unit";
    }
    Caravan k = {};
    k.unit = uid; k.leader = leader; k.force = force; k.corps = corps; k.origin = origin; k.dest = dest; k.good = good;
    k.qty = qty; k.cost = (int32_t)cost; k.taxDue = 0; k.born = today(); k.lastCity = origin;
    k.flags = (temp ? F_TEMP : 0) | (isPlayerForce(force) ? F_PLAYER : 0);
    // out of the city: move to the target (the game removes a sortied unit that cannot leave, 0x4af3c7)
    issueOrder(k, u);
    if (!execOrder(EXEC, u)) {
        removeUnit(PROC, u, 0);                              // releases a temporary leader
        return "the caravan could not leave the city";
    }
    addGold(PROC, b, -(int)cost);
    g_mk.buy(origin, good, qty);
    {
        Lock l;
        g_car.push_back(k);
        rebuildDirs();
    }
    g_log("trade: caravan %s -> %s: %d x %s for %lld gold (force %d, unit %d, leader %d%s)%s", cityName(origin), cityName(dest),
          qty, goodName(good), (long long)cost, force, uid, leader, temp ? " 商" : "", ai ? " [AI]" : "");
    if (out) *out = k;
    return "";
}

// money to force f near city c: c itself if f still owns it, else f's city nearest to c; returns the base
uint8_t* payee(int f, int c) {
    if (baseForce(c) == f) return base(c);
    int best = -1, bd = 1 << 30;
    for (int i = 0; i < g_data.C; i++)
        if (baseForce(i) == f) { int d = hexDist(g_data.lo[i], g_data.hi[i], g_data.lo[c], g_data.hi[c]); if (d < bd) { bd = d; best = i; } }
    return best >= 0 ? base(best) : nullptr;
}

// one caravan at the end of a turn; false = it is over (remove from the list)
bool caravanTurn(Caravan& k, int day) {
    uint8_t* u = liveUnit(k);
    if (!u) {
        g_log("trade: caravan %s -> %s (%d x %s) is gone%s", cityName(k.origin), cityName(k.dest), k.qty, goodName(k.good),
              k.flags & F_LOOTED ? " (intercepted)" : " (lost)");
        if (k.flags & F_TEMP) releaseTempIfOurs(k);
        recordDone(k, 0, k.flags & F_LOOTED ? tradeui::DONE_INTERCEPTED : tradeui::DONE_LOST);
        return false;
    }
    const int16_t* up = posOf(u);
    int lo = up[0], hi = up[1];
    int dlo, dhi; cityPos(k.dest, &dlo, &dhi);
    if (hexDist(lo, hi, dlo, dhi) <= g_cfg.arrive) {
        int pp = 0, capPct = 100;
        leaderOrTemp(k, &pp, &capPct);
        int64_t rev = g_mk.sell(k.dest, k.good, k.qty) * (1000 + pp) / 1000;   // = saleQuote (trade_model.h) before the sale
        g_info.see(k.force, k.dest);
        int df = baseForce(k.dest), st = stance(k.force, df);
        int64_t duty = st == 2 ? 0 : rev * (g_data.hub(k.dest) ? g_cfg.taxHub : g_cfg.tax) / 100;
        if (duty && base(k.dest)) addGold(PROC, base(k.dest), (int)duty);
        int64_t net = rev - duty - k.taxDue;
        if (net < 0) net = 0;
        uint8_t* home = payee(k.force, k.origin);
        int paid = home ? addGold(PROC, home, (int)net) : 0;
        g_log("trade: caravan arrived at %s: %d x %s sold for %lld, duty %lld, transit tax %d, %d gold home (cost %d, profit %lld)",
              cityName(k.dest), k.qty, goodName(k.good), (long long)rev, (long long)duty, k.taxDue, paid, k.cost,
              (long long)(net - k.cost));
        recordDone(k, net, 0);
        removeUnit(PROC, u, 0);                              // officers go home, a temporary one is released
        return false;
    }
    if (day - k.born > g_cfg.maxTurns * 10) {
        g_log("trade: caravan %s -> %s gave up after %d turns, cargo lost", cityName(k.origin), cityName(k.dest), g_cfg.maxTurns);
        recordDone(k, 0, tradeui::DONE_GAVE_UP);
        removeUnit(PROC, u, 0);
        return false;
    }
    // phase 3: a storm at sea in a month of the monsoon change
    if (g_mk.p.monsoon) {
        uint8_t* h = cell(lo, hi);
        int sea = h && (*(uint32_t*)(h + 4) & 0x1f) == 8 ? g_data.seaOf(lo, hi) : -1;
        if (sea >= 0 && season(g_data, sea, g_month0) == 'T' && g_rng.pct() < g_mk.p.stormRisk) {
            int lost = (k.qty * g_mk.p.stormLoss + 99) / 100;
            if (lost > k.qty) lost = k.qty;
            k.qty -= lost;
            g_log("trade: caravan %s -> %s caught by a storm (%s): %d x %s lost", cityName(k.origin), cityName(k.dest),
                  g_data.seas[sea].name.c_str(), lost, goodName(k.good));
        }
    }
    // transit: the first turn in a city's surroundings
    int c = nearestCity(lo, hi, 30);
    if (c >= 0) g_info.see(k.force, c);                      // phase 3: the caravan sees the market it passes
    if (c >= 0 && c != k.lastCity) {
        k.lastCity = c;
        int cf = baseForce(c);
        if (cf >= 0 && cf != k.force && c != k.dest && base(c)) {
            int st = stance(k.force, cf);
            int64_t value = (int64_t)k.qty * g_mk.sellPrice(c, k.good);
            if (st == 1) {
                int t = (int)(value * (g_data.hub(c) ? g_cfg.taxHub : g_cfg.tax) / 100);
                addGold(PROC, base(c), t);
                k.taxDue += t;
                g_log("trade: caravan %s -> %s pays %d transit tax at %s", cityName(k.origin), cityName(k.dest), t, cityName(c));
            } else if (st == 0 && g_rng.pct() < g_cfg.rob) {
                int lost = (k.qty * g_cfg.robLoss + 99) / 100;
                if (lost > k.qty) lost = k.qty;
                k.qty -= lost;
                int got = (int)((int64_t)lost * g_mk.sellPrice(c, k.good) * g_cfg.loot / 100);
                addGold(PROC, base(c), got);
                g_log("trade: caravan %s -> %s robbed near %s: %d x %s lost", cityName(k.origin), cityName(k.dest), cityName(c), lost, goodName(k.good));
            }
        }
    }
    issueOrder(k, u);                                        // re-path every turn (blocked target, moved city)
    if ((k.flags & F_PLAYER) && g_cfg.playerAuto) execOrder(EXEC, u);
    return true;
}

// AI: at most aiMonth new caravans a month, aiMax per force, temporary leaders only
void aiMonth() {
    std::vector<int> owner(g_data.C);
    for (int c = 0; c < g_data.C; c++) owner[c] = baseForce(c);
    int made = 0;
    for (int n = 0; n < g_s.R && made < g_cfg.aiMonth; n++) {
        int f = (int)((g_nextAiForce + n) % g_s.R);
        if (tempLive() >= g_cfg.tempMax || freeSpecials(WORLD) - 1 < g_cfg.tempKeep) break;
        uint8_t* fo = forceRec(WORLD, f);
        if (!fo || !objValid(fo) || vcall(fo, 0x48)) continue;          // gone or the player's
        int mine = 0; for (auto& k : g_car) mine += k.force == f;
        if (mine >= g_cfg.aiMax) continue;
        Route best; bool have = false;
        // phase 3: the force plans with the prices it knows (information delay), unseen markets at the base price
        Market view; std::vector<char> known; bool haveView = false;         // built for the first city with the gold
        auto eff = [&](const Route& x) { return x.profit * 1000 / routeTimePermille(g_data, g_mk.p, x.origin, x.dest, g_month0); };
        for (int c = 0; c < g_data.C; c++) {
            if (owner[c] != f) continue;
            uint8_t* b = base(c);
            int gold = b ? baseGold(b) : 0;
            if (gold < g_cfg.aiMinGold) continue;
            if (!haveView) { if (g_mk.p.info) g_info.view(g_mk, f, &view, &known); else view = g_mk; haveView = true; }
            std::vector<int> land, sea, seaTime;
            for (int t : g_inRange[c]) {
                int o = owner[t];
                if (o >= 0 && stance(f, o) == 0) continue;               // robbers there
                if (g_data.port(c) && g_data.port(t)) { sea.push_back(t); seaTime.push_back(routeTimePermille(g_data, g_mk.p, c, t, g_month0)); }
                else land.push_back(t);
            }
            int64_t budget = gold - g_cfg.aiReserve < g_cfg.aiInvest ? gold - g_cfg.aiReserve : g_cfg.aiInvest;
            Route r;
            // routes compare by profit per travel time (monsoon; land routes 1000: phase 1's comparison)
            if (bestRoute(view, c, land, budget, g_mk.p.capLand, g_cfg.tax, g_cfg.aiMargin, &r) && (!have || eff(r) > eff(best))) { best = r; have = true; }
            if (bestRoute(view, c, sea, budget, g_mk.p.capSea, g_cfg.tax, g_cfg.aiMargin, &r, &seaTime) && (!have || eff(r) > eff(best))) { best = r; have = true; }
        }
        if (!have) continue;
        std::string err = createCaravan(best.origin, best.dest, best.good, best.qty, -1, true, nullptr);
        if (err.empty()) made++;
        else g_log("trade: AI force %d: %s -> %s %s: %s", f, cityName(best.origin), cityName(best.dest), goodName(best.good), err.c_str());
    }
    g_nextAiForce = (g_nextAiForce + 7) % (uint32_t)g_s.R;
    if (made) g_log("trade: AI sent %d caravans this month", made);
}

bool mapReady() { uint8_t* b = baseRec(WORLD, 0); return b && objValid(b); }

// phase 3: what the forces see. Own and allied cities and every city within trade_info_range of them: now.
void observeTerritory(int onlyForce) {
    if (!g_mk.p.info || !g_info.C) return;
    std::vector<int> owner(g_data.C);
    for (int c = 0; c < g_data.C; c++) owner[c] = baseForce(c);
    std::vector<int8_t> al((size_t)g_s.R * g_s.R, -1);         // stance cache (f, o)
    for (int c = 0; c < g_data.C; c++) {
        int o = owner[c];
        if (o < 0 || o >= g_s.R) continue;
        for (int f = onlyForce < 0 ? 0 : onlyForce; f < (onlyForce < 0 ? g_s.R : onlyForce + 1); f++) {
            int8_t& a = al[(size_t)f * g_s.R + o];
            if (a < 0) a = (int8_t)(stance(f, o) == 2);
            if (!a) continue;
            g_info.see(f, c);
            for (int t : g_infoNear[c]) g_info.see(f, t);
        }
    }
}
// units of a force see the market of the city they stand near (10 hexes)
void observeUnits() {
    if (!g_mk.p.info || !g_info.C) return;
    for (int i = 0; i < g_s.U; i++) {
        uint8_t* u = unitRec(WORLD, i);
        if (!u || !objValid(u)) continue;
        int h = unitHome(u), f = h >= 0 ? baseForce(h) : -1;
        if (f < 0 || f >= g_s.R) continue;
        const int16_t* p = posOf(u);
        int c = nearestCity(p[0], p[1], 10);
        if (c >= 0) g_info.see(f, c);
    }
}
void monthEvents(int month) {
    std::vector<ActiveEvent> st, en;
    {
        Lock l;
        g_ev.month(g_data, g_mk.p, g_rng, month, &st, &en);
        g_ev.apply(g_data, &g_mk.mod);
    }
    for (auto& e : st) g_log("trade: event: %s for %d months", describe(g_data, e).c_str(), e.end - e.start);
    for (auto& e : en) g_log("trade: event over: %s", describe(g_data, e).c_str());
}

void turn() {
    if (!mapReady()) return;
    int day = today(), month = day / 30;
    g_month0 = month % 12;
    g_unit0 = unitRec(WORLD, 0);
    for (size_t i = 0; i < g_car.size();) {
        bool keep;
        { Caravan k = g_car[i]; keep = caravanTurn(k, day); Lock l; if (i < g_car.size()) g_car[i] = k; }
        if (keep) i++;
        else { Lock l; g_car.erase(g_car.begin() + i); }
    }
    bool first = g_lastMonth < 0;
    if (first) g_lastMonth = month;
    while (g_lastMonth < month) {                            // a new month (several after a long skip: catch up)
        g_lastMonth++;
        { Lock l; if (g_mk.p.info) g_info.endMonth(g_mk, g_lastMonth - 1); g_mk.month(); }
        monthEvents(g_lastMonth);
        if (g_lastMonth == month) {
            observeTerritory(-1);
            if (g_cfg.ai) aiMonth();
        }
    }
    if (first) observeTerritory(-1);
    observeUnits();
    { Lock l; rebuildDirs(); }
}

// phase 3: monsoon on sea hexes (movement.cpp's step cost, any thread: reads only g_unitDir / g_month0)
int __cdecl seaCostHook(const uint8_t* unit, const uint8_t* rec, int cost) {
    uint8_t* u0 = g_unit0;
    if (!u0 || !rec || unit < u0) return cost;
    size_t ui = (size_t)(unit - u0) / 0xf4;
    int dir = ui < g_unitDir.size() ? g_unitDir[ui] : 0;
    if (!dir && !g_cfg.monsoonAll) return cost;
    size_t h = (size_t)(rec - g_s.hex20) / 20;
    if (rec < g_s.hex20 || h >= (size_t)g_s.W * g_s.H) return cost;
    int sea = g_data.seaOf((int)(h / g_s.W), (int)(h % g_s.W));
    if (sea < 0) return cost;
    int pct = seaCostPct(g_data, g_mk.p, sea, g_month0, dir == 2 ? 0 : dir);
    int c = (cost * pct + 50) / 100;
    return c < 1 ? 1 : c > 254 ? 254 : c;
}

// ---------------------------------------------------------------- hooks
typedef void (__thiscall* VoidThisFn)(void*);
const VoidThisFn turnOrig = (VoidThisFn)0x4a1180;
void __fastcall turnHook(void* self) {
    turnOrig(self);
    __try { turn(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (g_log) g_log("trade: ERROR: exception %08lx in the turn", GetExceptionCode()); }
}

typedef int (__fastcall* PlanFn)(void* ai, void* edx, uint8_t* unit);
PlanFn g_planOrig = nullptr;
int __fastcall planHook(void* ai, void* edx, uint8_t* unit) {
    if (unit && !g_car.empty()) {
        for (auto& k : g_car)
            if (unitRec(WORLD, k.unit) == unit && *(int*)(unit + 0xc) == k.leader) { issueOrder(k, unit); return 1; }
    }
    return g_planOrig(ai, edx, unit);
}

typedef int (__fastcall* AnnihilateFn)(void* proc, void* edx, uint8_t* unit, uint8_t* attacker, int a3, int a4);
AnnihilateFn g_annOrig = nullptr;
int __fastcall annihilateHook(void* proc, void* edx, uint8_t* unit, uint8_t* attacker, int a3, int a4) {
    bool temp = false;
    if (unit && !g_car.empty()) {
        Lock l;
        for (auto& k : g_car) {
            if (unitRec(WORLD, k.unit) != unit || *(int*)(unit + 0xc) != k.leader || (k.flags & F_LOOTED)) continue;
            k.flags |= F_LOOTED;
            temp = (k.flags & F_TEMP) != 0;
            const int16_t* up = posOf(unit);
            int c = nearestCity(up[0], up[1], 1 << 20);
            int value = (int)((int64_t)k.qty * g_mk.sellPrice(c, k.good) * g_cfg.loot / 100);
            uint8_t* to = nullptr;
            if (attacker && isUnitPtr(attacker)) { int h = unitHome(attacker); to = h >= 0 ? base(h) : nullptr; }
            else if (attacker && isBasePtr(attacker)) to = attacker;
            int got = to ? addGold(PROC, to, value) : 0;
            g_log("trade: caravan %s -> %s (%d x %s) destroyed; %d gold to the victor (force %d)%s", cityName(k.origin),
                  cityName(k.dest), k.qty, goodName(k.good), got, to ? vcall(to, 0x40) : -1,
                  temp ? ", 商 leader released" : "");
            break;
        }
    }
    if (temp) { removeUnit(PROC, unit, 0); return 0; }       // never captured: vanish, the slot is recycled
    return g_annOrig(proc, edx, unit, attacker, a3, a4);
}

int findCityExt(const char* s);
int findGoodExt(const char* s);

// main-thread queue (automation "trade send / ai", the panel's 「出發」)
struct Job { std::string line, result; HANDLE done; };   // done == nullptr: queued by the panel (result -> g_uiResult)
std::wstring g_uiResult;                                   // the last panel send's result (shown by the panel)
std::wstring resultText(const std::string& r) {
    if (!r.compare(0, 2, "ok")) return L"成功：商隊出發（" + tradeui::wide(r.substr(3)) + L"）";
    return L"失敗：" + tradeui::wide(r);
}
std::vector<Job*> g_jobs;
void runJob(Job* j);
typedef int (__fastcall* PumpFn)(void* self, void* edx, int arg);   // 0x57ff80: thiscall, 1 argument (ret 4)
PumpFn g_pumpOrig = nullptr;
bool runJobSafe(Job* j) {
    __try { runJob(j); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void pumpJobs() {
    std::vector<Job*> jobs;
    { Lock l; jobs.swap(g_jobs); }
    for (Job* j : jobs) {
        if (!runJobSafe(j)) j->result = "exception";
        if (j->done) SetEvent(j->done);
        else { { Lock l; g_uiResult = resultText(j->result); } g_log("trade: panel send: %s", j->result.c_str()); delete j; }
    }
}
int __fastcall pumpHook(void* self, void* edx, int arg) {
    if (!g_jobs.empty() && waitingForPlayer((uint32_t)(uintptr_t)self)) pumpJobs();
    return g_pumpOrig(self, edx, arg);
}

void* trampoline(uint32_t from, const uint8_t* expect, size_t len) {
    if (memcmp((void*)(uintptr_t)from, expect, len)) return nullptr;
    uint8_t* t = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!t) return nullptr;
    memcpy(t, (void*)(uintptr_t)from, len);
    t[len] = 0xE9; *(int32_t*)(t + len + 1) = (int32_t)((from + len) - ((uintptr_t)t + len + 5));
    return t;
}
void jmpTo(uint32_t from, const void* to) {
    DWORD old; VirtualProtect((void*)(uintptr_t)from, 5, PAGE_EXECUTE_READWRITE, &old);
    uint8_t* p = (uint8_t*)(uintptr_t)from;
    p[0] = 0xE9; *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (from + 5));
    VirtualProtect((void*)(uintptr_t)from, 5, old, &old);
}

// ---------------------------------------------------------------- phase 2: the 中地圖 panel (trade_panel.cpp)
// research/midmap_re.md: while a MapUI is open its device callback is installed ([0x89e020] == 0x640340, [0x89e024] = the MapUI);
// MapUI +0x7c = the viewing force, +0x8c = 1 when opened from the main map (mode 4, the camera-jump click).
// Mouse points are panel pixels (the map area starts at +14). The midmap is modal: the game does not advance while
// it is open, but every read of the trade state still takes the lock.
const uint32_t MAPUI_VT = 0x859670;
const int MM_BORDER = 14;
typedef int (__fastcall* UiMouseFn)(void* ui, void* edx, void* target, uint32_t wp, POINT* pt);
typedef int (__fastcall* UiWheelFn)(void* ui, void* edx, void* target, uint32_t keys, int delta, POINT* pt);
typedef int (__thiscall* UiComposeFn)(void* ui);
typedef void (__thiscall* UiLayoutFn)(void* ui);
UiMouseFn g_uiDownPrev = nullptr, g_uiUpPrev = nullptr;
UiWheelFn g_uiWheelPrev = nullptr;
bool g_uiOn = false;                 // the panel hooks are installed
tradeui::State g_ui;
tradeui::Model g_model;
bool g_uiOwnDown = false;            // the last button down was ours: swallow its button up
uint8_t* g_uiLast = nullptr;         // the MapUI the state was last used with

uint8_t* openMapUi() { return *(uint32_t*)(uintptr_t)0x89e020 == 0x640340 ? *(uint8_t**)(uintptr_t)0x89e024 : nullptr; }
// the player's force when this MapUI is the main map's 中地圖 and the player has a city; else -1
int panelForce(uint8_t* ui) {
    if (!ui || ui[0x8c] != 1 || !mapReady()) return -1;
    int f = *(int*)(ui + 0x7c);
    if (f < 0 || f >= g_s.R || !isPlayerForce(f)) return -1;
    for (int c = 0; c < g_data.C; c++) if (baseForce(c) == f) return f;
    return -1;
}
std::wstring big5(const uint8_t* p, int n) {
    char b[16] = {}; memcpy(b, p, n);
    wchar_t w[16] = {};
    int len = MultiByteToWideChar(950, 0, b, -1, w, 16);
    return len > 1 ? std::wstring(w, len - 1) : L"";
}
void buildModel(int force) {
    observeTerritory(force);                                 // what the player sees now
    tradeui::Model& m = g_model;
    std::vector<int> owner(g_data.C);
    for (int c = 0; c < g_data.C; c++) owner[c] = baseForce(c);
    Lock l;
    m.d = &g_data;
    m.live = g_mk;
    if (g_mk.p.info) g_info.view(g_mk, force, &m.seen, &m.known);
    else { m.seen = g_mk; m.known.assign(g_data.C, 1); }
    m.age.resize(g_data.C);
    for (int c = 0; c < g_data.C; c++) m.age[c] = g_mk.p.info ? (uint8_t)g_info.ageOf(force, c) : 0;
    m.force = force;
    m.owner = owner;
    m.stance.assign(g_data.C, -1); m.gold.assign(g_data.C, -1);
    for (int c = 0; c < g_data.C; c++) {
        if (owner[c] < 0) continue;
        m.stance[c] = (int8_t)stance(force, owner[c]);
        if (owner[c] == force) { uint8_t* b = base(c); m.gold[c] = b ? baseGold(b) : 0; }
    }
    m.officers.clear();
    if (g_ui.origin >= 0 && g_ui.origin < g_data.C && owner[g_ui.origin] == force)
        for (int i = 0; i < g_cfg.PT; i++) {
            uint8_t* p = personRec(WORLD, i);
            if (!p || !objValid(p) || vcall(p, 0x40) != force || *(int*)(p + 0x9c) != g_ui.origin) continue;
            if (p[4] == 0xb0 && p[5] == 0xd3 && !p[6] && !p[9]) continue;     // a temporary 商
            m.officers.push_back({ i, big5(p + 4, 4) + big5(p + 9, 4), (int)ability(p, 3), (int)ability(p, 2) });
        }
    m.caravans.clear();
    for (auto& k : g_car) {
        uint8_t* u = liveUnit(k);
        if (!u) continue;
        const int16_t* up = posOf(u);
        bool player = k.force == force;
        if (!player) {
            if (!g_cfg.uiAi) continue;
            int c = nearestCity(up[0], up[1], 30);
            if (c < 0 || (g_mk.p.info && g_info.ageOf(force, c) != 0)) continue;   // AI caravans where the player can see
        }
        uint8_t* h = cell(up[0], up[1]);
        int terr = h ? *(uint32_t*)(h + 4) & 0x1f : 0;
        m.caravans.push_back({ k.origin, k.dest, k.good, k.qty, k.cost, up[0], up[1], player, (k.flags & F_TEMP) != 0, terr == 7 || terr == 8 });
    }
    m.done.clear();
    for (auto it = g_done.rbegin(); it != g_done.rend(); ++it)
        if (it->force == force) m.done.push_back({ it->day, it->origin, it->dest, it->good, it->qty, it->cost, it->net, it->flags });
    m.events = g_ev.live;
    m.month = today() / 30;
    m.tempLive = tempLive();
    m.freeSpecials = freeSpecials(WORLD);
    m.cfg.tax = g_cfg.tax; m.cfg.taxHub = g_cfg.taxHub; m.cfg.tempMax = g_cfg.tempMax; m.cfg.tempKeep = g_cfg.tempKeep;
    m.lastResult = g_uiResult;
}
bool projectHex(int lo, int hi, float* x, float* y) { return midmapHexToPixel(lo, hi, 0.0f, x, y); }

// midmap.cpp's overlay: every compose of the world 中地圖 (open, zoom, pan, background mode, our repaints)
void overlayImpl(uint32_t* dst, int pitch) {
    uint8_t* ui = openMapUi();
    int f = panelForce(ui);
    if (f < 0) { midmapSetClip(600); return; }
    if (ui != g_uiLast) { g_uiLast = ui; g_uiOwnDown = false; g_ui.msg.clear(); }
    midmapSetClip(g_ui.on ? tradeui::PANEL_X : 600);
    buildModel(f);
    tradeui::validate(g_model, g_ui);
    tradeui::renderMap(g_model, g_ui, projectHex, dst, pitch);
    if (g_ui.on) tradeui::renderPanel(g_model, g_ui, dst + tradeui::PANEL_X, pitch);
}
void __cdecl overlayHook(uint32_t* dst, int pitch) {
    __try { overlayImpl(dst, pitch); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_log("trade: ERROR: exception %08lx in the midmap overlay", GetExceptionCode()); }
}

void repaint(uint8_t* ui, int what) {
    if (what & (tradeui::REPAINT_MAP | tradeui::REPAINT_LAYOUT)) {
        midmapSetClip(g_ui.on ? tradeui::PANEL_X : 600);
        ((UiComposeFn)0x640260)(ui);                         // worldmod's compose: Koei's frame, the world map, the overlay
        if (what & tradeui::REPAINT_LAYOUT) ((UiLayoutFn)0x63fd00)(ui);   // markers under the panel hidden / shown again
        return;
    }
    if (!(what & tradeui::REPAINT_PANEL) || !g_ui.on) return;
    void* tex = *(void**)(uintptr_t)0x89e01c;                // the shared texture (IDirect3DTexture9: LockRect +0x4c, UnlockRect +0x50)
    if (!tex) return;
    struct { int Pitch; void* pBits; } lr = {};
    RECT rc = { MM_BORDER + tradeui::PANEL_X, MM_BORDER, MM_BORDER + tradeui::MAP_W, MM_BORDER + tradeui::MAP_H };
    typedef long (__stdcall* LockFn)(void* t, unsigned level, void* lr, const RECT* r, unsigned long flags);
    typedef long (__stdcall* UnlockFn)(void* t, unsigned level);
    void** vt = *(void***)tex;
    if (((LockFn)vt[0x4c / 4])(tex, 0, &lr, &rc, 0) >= 0) {
        tradeui::renderPanel(g_model, g_ui, (uint32_t*)lr.pBits, lr.Pitch / 4);
        ((UnlockFn)vt[0x50 / 4])(tex, 0);
    }
}
void queueSend(const tradeui::SendRequest& r) {
    char line[128];
    if (r.leader >= 0) sprintf_s(line, "trade send %d %d %d %d %d", r.origin, r.dest, r.good, r.qty, r.leader);
    else sprintf_s(line, "trade send %d %d %d %d", r.origin, r.dest, r.good, r.qty);
    Job* j = new Job; j->line = line; j->done = nullptr;
    { Lock l; g_uiResult.clear(); g_jobs.push_back(j); }
    g_log("trade: panel queued \"%s\"", line);
}
// 0: not ours; 1: handled
int downImpl(uint8_t* ui, POINT* pt) {
    int f = panelForce(ui);
    if (f < 0 || !pt) return 0;
    if (ui != g_uiLast) { g_uiLast = ui; g_ui.msg.clear(); }
    buildModel(f);
    tradeui::validate(g_model, g_ui);
    tradeui::Result r = tradeui::click(g_model, g_ui, projectHex, pt->x - MM_BORDER, pt->y - MM_BORDER);
    if (!r.consumed) return 0;
    if (r.send) queueSend(r.req);
    if (r.repaint & (tradeui::REPAINT_MAP | tradeui::REPAINT_LAYOUT)) {
        if (g_ui.origin >= 0 && g_ui.origin < g_data.C) buildModel(f);   // officers of a new origin
        tradeui::validate(g_model, g_ui);
    }
    repaint(ui, r.repaint);
    return 1;
}
int wheelImpl(uint8_t* ui, uint32_t keys, int delta, POINT* pt) {
    int f = panelForce(ui);
    if (f < 0 || !pt || !g_ui.on) return 0;
    tradeui::Result r = tradeui::wheel(g_model, g_ui, pt->x - MM_BORDER, pt->y - MM_BORDER, (int16_t)delta, (keys & MK_SHIFT) != 0);
    if (!r.consumed) return 0;
    repaint(ui, r.repaint);
    return 1;
}
int downSafe(uint8_t* ui, POINT* pt) {
    __try { return downImpl(ui, pt); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_log("trade: ERROR: exception %08lx in the panel click", GetExceptionCode()); return 0; }
}
int wheelSafe(uint8_t* ui, uint32_t keys, int delta, POINT* pt) {
    __try { return wheelImpl(ui, keys, delta, pt); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_log("trade: ERROR: exception %08lx in the panel wheel", GetExceptionCode()); return 0; }
}
int __fastcall uiDownHook(uint8_t* ui, void* edx, void* target, uint32_t wp, POINT* pt) {
    if (downSafe(ui, pt)) { g_uiOwnDown = true; return 1; }
    g_uiOwnDown = false;
    return g_uiDownPrev(ui, edx, target, wp, pt);
}
int __fastcall uiUpHook(uint8_t* ui, void* edx, void* target, uint32_t wp, POINT* pt) {
    if (g_uiOwnDown) { g_uiOwnDown = false; return 1; }
    return g_uiUpPrev(ui, edx, target, wp, pt);
}
int __fastcall uiWheelHook(uint8_t* ui, void* edx, void* target, uint32_t keys, int delta, POINT* pt) {
    if (wheelSafe(ui, keys, delta, pt)) return 1;
    return g_uiWheelPrev(ui, edx, target, keys, delta, pt);
}
void* patchSlot(uint32_t slot, const void* fn) {
    void* old = *(void**)(uintptr_t)slot;
    DWORD o; VirtualProtect((void*)(uintptr_t)slot, 4, PAGE_READWRITE, &o);
    *(const void**)(uintptr_t)slot = fn;
    VirtualProtect((void*)(uintptr_t)slot, 4, o, &o);
    return old;
}
// the world 中地圖 (worldmod.cpp installMidmapHooks) is in place: its handlers in the slots, compose jumps to it
bool worldMidmapInstalled() {
    auto slot = [](uint32_t off) { return *(uint32_t*)(uintptr_t)(MAPUI_VT + off); };
    return midmapReady() && *(uint8_t*)(uintptr_t)0x640260 == 0xE9 && slot(0x90) != 0x63d790 && slot(0x94) != 0x63d800 && slot(0xa8) != 0x4754d0;
}

// automation: "trade panel <from> <to> <good> <file.bmp>": the panel and its map overlay as the player sees them now,
// rendered off screen (a fixed view around the origin, 2 pixels a hex)
std::string panelShot(const char* line) {
    char a[64] = {}, b[64] = {}, c[64] = {}, path[MAX_PATH] = {};
    if (sscanf_s(line, "%*s %*s %63s %63s %63s %259[^\r\n]", a, (unsigned)sizeof a, b, (unsigned)sizeof b, c, (unsigned)sizeof c, path, (unsigned)sizeof path) != 4)
        return "usage: trade panel <from> <to> <good> <file.bmp>";
    int o = findCityExt(a), d = findCityExt(b), g = findGoodExt(c);
    if (o < 0 || d < 0 || g < 0) return "unknown city or good";
    int f = -1;
    for (int i = 0; i < g_s.R && f < 0; i++) if (isPlayerForce(i)) f = i;
    if (f < 0) return "no player force";
    tradeui::State keep = g_ui;
    g_ui = tradeui::State(); g_ui.on = true; g_ui.origin = o; g_ui.dest = d; g_ui.good = g;
    buildModel(f);
    tradeui::validate(g_model, g_ui);
    const int W = tradeui::MAP_W, H = tradeui::MAP_H;
    std::vector<uint32_t> px((size_t)W * H * 4, 0xffcac4b7);
    float cl = (float)g_data.lo[o], ch = (float)g_data.hi[o];
    tradeui::Projector proj = [&](int lo, int hi, float* x, float* y) {
        *x = (lo - cl) * 2.0f + tradeui::PANEL_X / 2.0f; *y = (hi - ch) * 2.0f + H / 2.0f;
        return *x >= 0 && *y >= 0 && *x < tradeui::PANEL_X && *y < H;
    };
    std::string out;
    FILE* fp = nullptr;
    if (fopen_s(&fp, path, "wb") || !fp) out = "cannot write the file";
    else {
        int shot = 0;
        for (int tab = 0; tab < 4; tab++) {                  // the four tabs side by side
            g_ui.tab = tab;
            std::vector<uint32_t> one((size_t)W * H, 0xffcac4b7);
            tradeui::renderMap(g_model, g_ui, proj, one.data(), W);
            tradeui::renderPanel(g_model, g_ui, one.data() + tradeui::PANEL_X, W);
            for (int y = 0; y < H; y++) memcpy(&px[(size_t)y * W * 4 + (size_t)tab * W], &one[(size_t)y * W], W * 4);
            shot++;
        }
        BITMAPFILEHEADER fh = {}; BITMAPINFOHEADER ih = {};
        ih.biSize = sizeof ih; ih.biWidth = W * 4; ih.biHeight = -H; ih.biPlanes = 1; ih.biBitCount = 32; ih.biCompression = BI_RGB;
        fh.bfType = 0x4d42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + W * 4 * H * 4;
        fwrite(&fh, sizeof fh, 1, fp); fwrite(&ih, sizeof ih, 1, fp); fwrite(px.data(), 4, px.size(), fp); fclose(fp);
        out = "ok " + std::to_string(shot) + " tabs, " + std::to_string(g_model.officers.size()) + " officers, " +
              std::to_string(g_model.caravans.size()) + " caravans on the map";
    }
    g_ui = keep;
    return out;
}

// ---------------------------------------------------------------- save block
const uint32_t TRAD = 0x44415254;      // "TRAD"
const uint32_t TRAD_VERSION = 2;           // 1: phase 1; 2: + events, the players' last trips, information (phase 3)
void resetPhase3() {
    g_ev.live.clear(); g_mk.mod.clear(); g_done.clear();
    g_info.reset(g_s.R, g_data, g_mk.p.infoMonths);
}
void resetState() {
    Lock l;
    g_car.clear(); g_mk.reset(); g_lastMonth = -1; g_rng = Rng(); g_nextAiForce = 0;
    resetPhase3();
    rebuildDirs();
}
template <class T> void put(std::vector<uint8_t>& v, const T& x) { const uint8_t* p = (const uint8_t*)&x; v.insert(v.end(), p, p + sizeof x); }
std::vector<uint8_t> blockBytes() {
    std::vector<uint8_t> pl;
    put(pl, (uint32_t)g_data.C); put(pl, (uint32_t)g_data.G);
    for (int32_t v : g_mk.idx) put(pl, v);
    put(pl, g_lastMonth); put(pl, g_rng.s); put(pl, g_nextAiForce);
    put(pl, (uint32_t)g_car.size());
    for (auto& k : g_car) put(pl, k);
    // v2
    put(pl, (uint32_t)g_ev.live.size());
    for (auto& e : g_ev.live) put(pl, e);
    put(pl, (uint32_t)g_done.size());
    for (auto& r : g_done) put(pl, r);
    put(pl, (uint32_t)g_info.R); put(pl, (uint32_t)g_info.C); put(pl, (uint32_t)g_info.G); put(pl, (uint32_t)g_info.H);
    put(pl, g_info.histMonth);
    pl.insert(pl.end(), g_info.age.begin(), g_info.age.end());
    const uint8_t* hp = (const uint8_t*)g_info.hist.data();
    pl.insert(pl.end(), hp, hp + g_info.hist.size() * 2);
    std::vector<uint8_t> out;
    put(out, TRAD); put(out, TRAD_VERSION); put(out, (uint32_t)pl.size());
    out.insert(out.end(), pl.begin(), pl.end());
    return out;
}
// parses a payload; false (state unchanged) if malformed
bool parseBlock(const uint8_t* p, size_t n, uint32_t version, int* cars) {
    const uint8_t* e = p + n;
    auto get = [&](void* v, size_t s) { if ((size_t)(e - p) < s) return false; memcpy(v, p, s); p += s; return true; };
    uint32_t C, G, nc, rng, nextAi; int32_t lastMonth;
    if (!get(&C, 4) || !get(&G, 4) || C > 4096 || G > 64) return false;
    std::vector<int32_t> idx((size_t)C * G);
    if (!idx.empty() && !get(idx.data(), idx.size() * 4)) return false;
    if (!get(&lastMonth, 4) || !get(&rng, 4) || !get(&nextAi, 4) || !get(&nc, 4) || nc > 100000) return false;
    std::vector<Caravan> cars_((size_t)nc);
    if (nc && !get(cars_.data(), nc * sizeof(Caravan))) return false;
    // v2: events, last trips, information
    std::vector<ActiveEvent> evs; std::vector<DoneRec> done;
    uint32_t iR = 0, iC = 0, iG = 0, iH = 0; int32_t histMonth = -1;
    std::vector<uint8_t> age; std::vector<int16_t> hist;
    if (version >= 2) {
        uint32_t ne, nd;
        if (!get(&ne, 4) || ne > 256) return false;
        evs.resize(ne); if (ne && !get(evs.data(), ne * sizeof(ActiveEvent))) return false;
        if (!get(&nd, 4) || nd > 1024) return false;
        done.resize(nd); if (nd && !get(done.data(), nd * sizeof(DoneRec))) return false;
        if (!get(&iR, 4) || !get(&iC, 4) || !get(&iG, 4) || !get(&iH, 4) || !get(&histMonth, 4)) return false;
        if (iR > 4096 || iC > 4096 || iG > 64 || iH > 60) return false;
        age.resize((size_t)iR * iC); hist.resize((size_t)iH * iC * iG);
        if (!age.empty() && !get(age.data(), age.size())) return false;
        if (!hist.empty() && !get(hist.data(), hist.size() * 2)) return false;
    }
    Lock l;
    if ((int)C == g_data.C && (int)G == g_data.G) {
        g_mk.idx = idx;
        for (int32_t& v : g_mk.idx) v = v < g_mk.p.idxMin ? g_mk.p.idxMin : v > g_mk.p.idxMax ? g_mk.p.idxMax : v;
    } else if (g_log) g_log("trade: save block for %u cities / %u goods, trade.bin has %d / %d: prices reset", C, G, g_data.C, g_data.G);
    g_lastMonth = lastMonth; g_rng.s = rng ? rng : 1; g_nextAiForce = nextAi;
    g_car.clear();
    for (auto& k : cars_) if (k.good >= 0 && k.good < g_data.G && k.origin >= 0 && k.origin < g_data.C && k.dest >= 0 && k.dest < g_data.C) g_car.push_back(k);
    *cars = (int)g_car.size();
    resetPhase3();
    if (version >= 2) {
        for (auto& e : evs) if (e.def < g_data.events.size() && (e.good < g_data.G || e.good == EV_GOOD_ALL)) g_ev.live.push_back(e);
        g_ev.apply(g_data, &g_mk.mod);
        g_done = done;
        if ((int)iR == g_info.R && (int)iC == g_info.C && (int)iG == g_info.G && (int)iH == g_info.H) {
            g_info.age = age; g_info.hist = hist; g_info.histMonth = histMonth;
        } else if (g_log) g_log("trade: save block information for %u forces / %u cities / %u goods / %u months, now %d / %d / %d / %d: information reset",
                                iR, iC, iG, iH, g_info.R, g_info.C, g_info.G, g_info.H);
    }
    rebuildDirs();
    return true;
}

// ---------------------------------------------------------------- commands
bool dumpState(const char* path) {
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") || !f) return false;
    Lock l;
    fprintf(f, "trade: %d cities, %d goods, month %d, %zu caravans (%d with 商), free special officers %d\n", g_data.C, g_data.G,
            g_lastMonth, g_car.size(), tempLive(), mapReady() ? freeSpecials(WORLD) : -1);
    fprintf(f, "month of year %d; events %zu; info: %d forces x %d cities, history to month %d\n", g_month0 + 1, g_ev.live.size(),
            g_info.R, g_info.C, g_info.histMonth);
    for (auto& e : g_ev.live) fprintf(f, "event %s: months %d..%d\n", describe(g_data, e).c_str(), e.start, e.end);
    for (size_t i = 0; i < g_data.seas.size(); i++) fprintf(f, "monsoon %s: %c\n", g_data.seas[i].name.c_str(), season(g_data, (int)i, g_month0));
    for (auto& r : g_done) fprintf(f, "done day %d force %d: %s -> %s %d x %s cost %d net %d flags %u\n", r.day, r.force, cityName(r.origin),
                                   cityName(r.dest), r.qty, goodName(r.good), r.cost, r.net, r.flags);
    for (auto& k : g_car) {
        uint8_t* u = mapReady() ? liveUnit(k) : nullptr;
        const int16_t* up = u ? posOf(u) : nullptr;
        fprintf(f, "caravan unit %d leader %d force %d%s%s: %s -> %s, %d x %s, cost %d, tax %d, at %d,%d\n", k.unit, k.leader, k.force,
                k.flags & F_TEMP ? " 商" : "", k.flags & F_PLAYER ? " player" : "", cityName(k.origin), cityName(k.dest), k.qty,
                goodName(k.good), k.cost, k.taxDue, up ? up[0] : -1, up ? up[1] : -1);
    }
    fprintf(f, "city id name force flags");
    for (int g = 0; g < g_data.G; g++) fprintf(f, " %s", g_data.goodName[g].c_str());
    fprintf(f, "   (buy price / index permille; * = sells it)\n");
    for (int c = 0; c < g_data.C; c++) {
        fprintf(f, "%d %s %d %s%s", c, cityName(c), mapReady() ? baseForce(c) : -1, g_data.hub(c) ? "H" : "-", g_data.port(c) ? "P" : "-");
        for (int g = 0; g < g_data.G; g++) fprintf(f, " %d/%d%s", g_mk.buyPrice(c, g), g_mk.at(c, g), g_data.canBuy(c, g) ? "*" : "");
        fprintf(f, "\n");
    }
    fclose(f);
    return true;
}

int findCity(const char* s) {
    char* end; long v = strtol(s, &end, 10);
    if (*s && !*end) return v >= 0 && v < g_data.C ? (int)v : -1;
    for (int c = 0; c < g_data.C; c++) if (g_data.cityName[c] == s) return c;
    return -1;
}
int findGood(const char* s) {
    char* end; long v = strtol(s, &end, 10);
    if (*s && !*end) return v >= 0 && v < g_data.G ? (int)v : -1;
    for (int g = 0; g < g_data.G; g++) if (g_data.goodName[g] == s) return g;
    return -1;
}

int findCityExt(const char* s) { return findCity(s); }
int findGoodExt(const char* s) { return findGood(s); }

void runJob(Job* j) {
    char cmd[32] = {}, a[64] = {}, b[64] = {}, c[64] = {};
    int qty = 0, officer = -1;
    sscanf_s(j->line.c_str(), "%*s %31s", cmd, (unsigned)sizeof cmd);
    if (!strcmp(cmd, "send")) {
        int n = sscanf_s(j->line.c_str(), "%*s %*s %63s %63s %63s %d %d", a, (unsigned)sizeof a, b, (unsigned)sizeof b, c, (unsigned)sizeof c, &qty, &officer);
        if (n < 4) { j->result = "usage: trade send <from> <to> <good> <qty> [officer]"; return; }
        int o = findCity(a), d = findCity(b), g = findGood(c);
        if (o < 0 || d < 0 || g < 0) { j->result = "unknown city or good"; return; }
        Caravan k;
        std::string err = createCaravan(o, d, g, qty, n >= 5 ? officer : -1, false, &k);
        j->result = err.empty() ? "ok unit " + std::to_string(k.unit) + " leader " + std::to_string(k.leader) + " qty " + std::to_string(k.qty) +
                                  " cost " + std::to_string(k.cost) : err;
    } else if (!strcmp(cmd, "panel")) {
        j->result = panelShot(j->line.c_str());
    } else if (!strcmp(cmd, "ai")) {
        size_t before = g_car.size();
        aiMonth();
        j->result = "ok " + std::to_string(g_car.size() - before) + " AI caravans";
    } else j->result = "unknown trade command";
}

}  // namespace

// ---------------------------------------------------------------- public
namespace {
void streamImpl(uint8_t* stream) {
    {
        bool reading = *(int*)(stream + 8) == 1;
        bool save = isSaveKind(stream) != 0, scen = isScenarioKind(stream) != 0;
        if (!save && !scen) return;                         // master data, TS stages...
        if (*(int*)(stream + 0x68)) { g_log("trade: stream obfuscation on at the end of the world data, TRAD block skipped"); return; }
        if (reading) {
            resetState();
            if (!save) { g_log("trade: new game (scenario stream kind %#x): trade state reset", *(int*)(stream + 0x54)); return; }
            uint8_t* cur = *(uint8_t**)(stream + 0x14), *end = *(uint8_t**)(stream + 0x18);
            size_t left = cur && end > cur ? (size_t)(end - cur) : 0;
            uint32_t hdr[3];
            if (left < 12) { g_log("trade: save without a TRAD block (%zu bytes after the world): fresh trade state", left); return; }
            memcpy(hdr, cur, 12);
            if (hdr[0] != TRAD || 12 + (size_t)hdr[2] > left) { g_log("trade: save without a TRAD block: fresh trade state"); return; }
            int cars = 0;
            bool ok = (hdr[1] == 1 || hdr[1] == TRAD_VERSION) && parseBlock(cur + 12, hdr[2], hdr[1], &cars);
            std::vector<uint8_t> sink(12 + (size_t)hdr[2]);
            streamRead(stream, sink.data(), (int)sink.size());   // consume the block (bounded: it fits)
            g_log("trade: TRAD block v%u (%u bytes) %s, %d caravans", hdr[1], hdr[2], ok ? "loaded" : "NOT understood (fresh state)", cars);
            if (!ok) resetState();
        } else {
            if (!save) return;
            std::vector<uint8_t> v;
            { Lock l; v = blockBytes(); }
            int r = streamWrite(stream, v.data(), (int)v.size());
            g_log("trade: TRAD block written (%zu bytes, %zu caravans)%s", v.size(), g_car.size(), r ? "" : " - write FAILED");
        }
    }
}
bool dumpSafe(const char* path) {
    __try { return dumpState(path); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}  // namespace

void tradeStream(uint8_t* stream) {
    if (!g_on) return;
    __try { streamImpl(stream); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_log("trade: ERROR: exception %08lx in the save hook", GetExceptionCode()); }
}

void tradeCommand(const char* line, FILE* out) {
    if (!g_on) { fprintf(out, "trade: disabled (worldmod.ini trade=1)\n"); return; }
    char cmd[32] = {}, arg[MAX_PATH] = {};
    sscanf_s(line, "%*s %31s", cmd, (unsigned)sizeof cmd);
    if (!strcmp(cmd, "dump")) {
        char path[MAX_PATH];
        if (sscanf_s(line, "%*s %*s %259[^\r\n]", arg, (unsigned)sizeof arg) == 1) strcpy_s(path, arg);
        else { char d[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, g_s.dir, -1, d, MAX_PATH, nullptr, nullptr); sprintf_s(path, "%s\\worldmod_trade_dump.txt", d); }
        fprintf(out, "trade dump: %s (%s)\n", dumpSafe(path) ? "ok" : "FAILED", path);
    } else if (!strcmp(cmd, "tick")) {                      // advance only the price model n months (no units)
        int n = 1; sscanf_s(line, "%*s %*s %d", &n);
        { Lock l; for (int i = 0; i < n && i < 1200; i++) g_mk.month(); }
        fprintf(out, "trade tick: %d months of price reversion\n", n);
    } else if (!strcmp(cmd, "target")) {                    // target <city> <lo> <hi>: the hex a caravan at lo,hi heads for
        char cs[64] = {}; int lo = 0, hi = 0, tlo = -1, thi = -1;
        int n = sscanf_s(line, "%*s %*s %63s %d %d", cs, (unsigned)sizeof cs, &lo, &hi);
        int c = n == 3 ? findCity(cs) : -1, dlo = 0, dhi = 0;
        if (c >= 0) cityPos(c, &dlo, &dhi);
        bool ok = c >= 0 && hexNear(dlo, dhi, lo, hi, &tlo, &thi);
        fprintf(out, "trade target: city %d at %d,%d -> %s %d,%d\n", c, dlo, dhi, ok ? "hex" : "none", tlo, thi);
    } else if (!strcmp(cmd, "send") || !strcmp(cmd, "ai") || !strcmp(cmd, "panel")) {
        Job j; j.line = line; j.done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        { Lock l; g_jobs.push_back(&j); }
        DWORD w = WaitForSingleObject(j.done, 60000);
        if (w != WAIT_OBJECT_0) {
            Lock l;
            for (size_t i = 0; i < g_jobs.size(); i++) if (g_jobs[i] == &j) { g_jobs.erase(g_jobs.begin() + i); break; }
            j.result = "timeout: the game was not waiting for the player's input";
        }
        CloseHandle(j.done);
        fprintf(out, "trade %s: %s\n", cmd, j.result.c_str());
    } else fprintf(out, "trade: commands: dump [file] | send <from> <to> <good> <qty> [officer] | ai | tick [months] | target <city> <lo> <hi> | panel <from> <to> <good> <file.bmp>\n");
}

bool tradeInstall(const TradeSetup& s, TraceLogFn log) {
    g_log = log; g_s = s;
    wchar_t ini[MAX_PATH]; swprintf_s(ini, L"%s\\worldmod.ini", s.dir);
    auto I = [&](const wchar_t* k, int d) { return (int)GetPrivateProfileIntW(s.section, k, d, ini); };
    g_cfg.on = I(L"trade", 0);
    if (!g_cfg.on) return false;
    g_cfg.tempMax = I(L"trade_temp_max", 30); g_cfg.tempKeep = I(L"trade_temp_keep", 30); g_cfg.tmpl = I(L"trade_template", 745);
    g_cfg.ai = I(L"trade_ai", 1); g_cfg.aiHook = I(L"trade_ai_hook", 1); g_cfg.aiMax = I(L"trade_ai_max", 2);
    g_cfg.aiMonth = I(L"trade_ai_month", 6); g_cfg.aiRange = I(L"trade_ai_range", 80); g_cfg.aiMinGold = I(L"trade_ai_min_gold", 6000);
    g_cfg.aiInvest = I(L"trade_ai_invest", 4000); g_cfg.aiReserve = I(L"trade_ai_reserve", 2000); g_cfg.aiMargin = I(L"trade_ai_margin", 25);
    g_cfg.tax = I(L"trade_tax", 10); g_cfg.taxHub = I(L"trade_tax_hub", 5); g_cfg.relTax = I(L"trade_rel_tax", 40);
    g_cfg.rob = I(L"trade_rob", 15); g_cfg.robLoss = I(L"trade_rob_loss", 50); g_cfg.loot = I(L"trade_loot", 60);
    g_cfg.escort = I(L"trade_escort", 500); g_cfg.food = I(L"trade_food", 3000); g_cfg.arrive = I(L"trade_arrive", 3);
    g_cfg.maxTurns = I(L"trade_max_turns", 90); g_cfg.playerAuto = I(L"trade_player_auto", 0);
    g_cfg.ui = I(L"trade_ui", 1); g_cfg.uiAi = I(L"trade_ui_ai_caravans", 1); g_cfg.monsoonAll = I(L"trade_monsoon_all", 0);
    g_cfg.PT = I(L"persons", 850) + 250;
    Params p;
    p.impact = I(L"trade_impact", 50); p.spread = I(L"trade_spread", 10); p.revert = I(L"trade_revert", 10);
    p.revertHub = I(L"trade_revert_hub", 25); p.capLand = I(L"trade_cap_land", 20); p.capSea = I(L"trade_cap_sea", 60);
    p.fee = I(L"trade_fee", 300);
    p.monsoon = I(L"trade_monsoon", 1); p.monWith = I(L"trade_monsoon_with", 70); p.monAgainst = I(L"trade_monsoon_against", 140);
    p.monStorm = I(L"trade_monsoon_storm", 125); p.stormRisk = I(L"trade_storm_risk", 10); p.stormLoss = I(L"trade_storm_loss", 30);
    p.events = I(L"trade_events", 1); p.eventChance = I(L"trade_event_chance", 15); p.eventMax = I(L"trade_event_max", 4);
    p.info = I(L"trade_info", 1); p.infoRange = I(L"trade_info_range", 40); p.infoMonths = I(L"trade_info_months", 12);
    if (g_cfg.tempMax < 0 || g_cfg.tempMax > 100 || g_cfg.tempKeep < 0 || g_cfg.tempKeep > 100 || g_cfg.escort < 1 || g_cfg.escort > 60000 ||
        p.capLand < 1 || p.capSea < 1 || p.impact < 0 || p.impact > 1000 || p.spread < 0 || p.spread > 100 || p.revert < 0 || p.revert > 100 ||
        p.revertHub < 0 || p.revertHub > 100 || g_cfg.aiRange < 1 || g_cfg.aiRange > 2000 || g_cfg.arrive < 1 || g_cfg.arrive > 10 ||
        p.monWith < 20 || p.monWith > 500 || p.monAgainst < 20 || p.monAgainst > 500 || p.monStorm < 20 || p.monStorm > 500 ||
        p.stormRisk < 0 || p.stormRisk > 100 || p.stormLoss < 0 || p.stormLoss > 100 || p.eventChance < 0 || p.eventChance > 100 ||
        p.eventMax < 0 || p.eventMax > 32 || p.infoRange < 0 || p.infoRange > 2000 || p.infoMonths < 1 || p.infoMonths > 60 ||
        g_cfg.PT < 1100 || g_cfg.PT > 40000) {
        log("trade: ERROR: a trade_* value is out of range, trade not installed");
        return false;
    }
    wchar_t tb[MAX_PATH]; wchar_t name[MAX_PATH];
    GetPrivateProfileStringW(s.section, L"trade_data", L"trade.bin", name, MAX_PATH, ini);
    swprintf_s(tb, L"%s\\%s", s.dataDir && *s.dataDir ? s.dataDir : s.dir, name);
    std::vector<uint8_t> bytes;
    {
        FILE* f = nullptr;
        if (_wfopen_s(&f, tb, L"rb") || !f) { log("trade: ERROR: %ls not found, trade not installed", tb); return false; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        bytes.resize(n > 0 ? n : 0); if (n > 0) fread(bytes.data(), 1, n, f); fclose(f);
    }
    std::string err;
    if (!g_data.load(bytes.data(), bytes.size(), &err)) { log("trade: ERROR: %ls: %s, trade not installed", tb, err.c_str()); return false; }
    if (g_data.C != s.C) { log("trade: ERROR: trade.bin has %d cities, the game %d (cities=), trade not installed", g_data.C, s.C); return false; }
    if (!s.hex20 || s.R < 1 || s.U < 1) { log("trade: ERROR: no map / forces / units, trade not installed"); return false; }
    // expected code at the hook sites (after worldmod's own patches)
    static const uint8_t turnCall[] = { 0xe8, 0x5d, 0x0c, 0xf2, 0xff };                   // 0x58051e call 0x4a1180
    static const uint8_t planHead[] = { 0x83, 0xec, 0x20, 0x53, 0x55 };                   // 0x5def90 sub esp,20h; push ebx; push ebp
    static const uint8_t annHead[] = { 0x8b, 0x44, 0x24, 0x10, 0x8b, 0x54, 0x24, 0x0c };  // 0x4bfa30 mov eax,[esp+10h]; mov edx,[esp+0Ch]
    static const uint8_t pumpHead[] = { 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04 };            // 0x57ff80 push esi; mov esi,ecx; mov eax,[esi+4]
    struct Site { uint32_t at; const uint8_t* b; size_t n; const char* what; } sites[] = {
        { 0x58051e, turnCall, sizeof turnCall, "turn end" }, { 0x5def90, planHead, sizeof planHead, "AI unit planning" },
        { 0x4bfa30, annHead, sizeof annHead, "unit annihilated" }, { 0x57ff80, pumpHead, sizeof pumpHead, "command pump" } };
    int bad = 0;
    for (auto& t : sites)
        if (memcmp((void*)(uintptr_t)t.at, t.b, t.n)) { log("trade: ERROR: unexpected code at %08x (%s)", t.at, t.what); bad++; }
    if (bad) { log("trade: not installed"); return false; }
    if (!s.write) {
        log("trade: verify_only: hook sites ok, %d cities, %d goods (trade.bin v%d: %zu monsoon seas, %zu events); panel: %s", g_data.C, g_data.G,
            g_data.version, g_data.seas.size(), g_data.events.size(), !g_cfg.ui ? "off" : worldMidmapInstalled() ? "world 中地圖 present" : "needs the world 中地圖 (not installed in verify_only)");
        return true;
    }

    InitializeCriticalSection(&g_cs);
    g_mk.d = &g_data; g_mk.p = p; g_mk.reset();
    g_inRange = citiesInRange(g_data, g_cfg.aiRange);
    if (g_data.version < 2) { g_mk.p.monsoon = 0; g_mk.p.events = 0; }      // a phase 1 trade.bin: no seas, no events
    g_infoNear = citiesInRange(g_data, g_mk.p.infoRange);
    resetPhase3();
    g_unitDir.assign(g_s.U, 0);
    g_planOrig = g_cfg.aiHook ? (PlanFn)trampoline(0x5def90, planHead, sizeof planHead) : nullptr;
    g_annOrig = (AnnihilateFn)trampoline(0x4bfa30, annHead, sizeof annHead);
    g_pumpOrig = (PumpFn)trampoline(0x57ff80, pumpHead, sizeof pumpHead);
    if ((g_cfg.aiHook && !g_planOrig) || !g_annOrig || !g_pumpOrig) { log("trade: ERROR: trampolines failed, not installed"); return false; }
    {
        DWORD old; VirtualProtect((void*)0x58051f, 4, PAGE_EXECUTE_READWRITE, &old);
        *(int32_t*)0x58051f = (int32_t)((uintptr_t)turnHook - (0x58051e + 5));
        VirtualProtect((void*)0x58051f, 4, old, &old);
    }
    if (g_planOrig) jmpTo(0x5def90, (void*)planHook);
    jmpTo(0x4bfa30, (void*)annihilateHook);
    jmpTo(0x57ff80, (void*)pumpHook);
    // phase 3: monsoon on caravans' sea steps (movement.cpp, move=1)
    if (g_mk.p.monsoon && !g_data.seas.empty()) movementSetSeaCost(seaCostHook);
    // phase 2: the panel on the world 中地圖
    if (g_cfg.ui) {
        if (worldMidmapInstalled()) {
            g_uiDownPrev = (UiMouseFn)patchSlot(MAPUI_VT + 0x90, (void*)uiDownHook);
            g_uiUpPrev = (UiMouseFn)patchSlot(MAPUI_VT + 0x94, (void*)uiUpHook);
            g_uiWheelPrev = (UiWheelFn)patchSlot(MAPUI_VT + 0xa8, (void*)uiWheelHook);
            midmapSetOverlay(overlayHook);
            g_uiOn = true;
        } else log("trade: midmap panel NOT installed: the world 中地圖 is not active (world_terrain / bases)");
    }
    g_on = true;
    int hubs = 0; for (int c = 0; c < g_data.C; c++) hubs += g_data.hub(c);
    log("trade: installed: %d cities (%d hubs), %d goods; temp caravans <= %d keeping %d special slots; AI %s (planning hook %s), range %d",
        g_data.C, hubs, g_data.G, g_cfg.tempMax, g_cfg.tempKeep, g_cfg.ai ? "on" : "off", g_planOrig ? "on" : "off", g_cfg.aiRange);
    log("trade: phase 2 panel %s; phase 3: monsoon %s (%zu seas, caravans%s, %d/%d/%d%%), events %s (%zu kinds, %d%%/month, <= %d), information delay %s (range %d, %d months)",
        g_uiOn ? "on (MapUI slots +0x90 +0x94 +0xa8 chained, overlay)" : "off", g_mk.p.monsoon ? "on" : "off", g_data.seas.size(),
        g_cfg.monsoonAll ? " and all ships" : " only", g_mk.p.monWith, g_mk.p.monAgainst, g_mk.p.monStorm, g_mk.p.events ? "on" : "off",
        g_data.events.size(), g_mk.p.eventChance, g_mk.p.eventMax, g_mk.p.info ? "on" : "off", g_mk.p.infoRange, g_mk.p.infoMonths);
    return true;
}
