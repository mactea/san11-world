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
// Phase 4: the player's caravans march by themselves at the end of the 旬 (default; per caravan 自動 / 手動), the
// monsoon by the direction of each step for every ship, keys in the panel (MapUI / midmap dialog vt +0xb8), merchant
// officers (experience, levels, 商才; the AI sends its merchants); TRAD block v3.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <compressapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <set>
#include <sstream>
#include "trade.h"
#include "persons_layout.h"
#include "skills.h"
#include "skill_telemetry.h"
#include "trade_model.h"
#include "trade_procurement.h"
#include "trade_panel.h"
#include "trade_auto.h"
#include "midmap.h"
#include "movement.h"
#include "pathfind.h"
#include "trade_native_ui.h"
#include "trade_ship_visual.h"
#include "trade_ship_ui.h"

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

#ifdef TRADE_UI_TEST
#define TRADE_CALL_CONST
#else
#define TRADE_CALL_CONST const
#endif
TRADE_CALL_CONST GetFn unitRec = (GetFn)0x490e70, personRec = (GetFn)0x490b00, forceRec = (GetFn)0x490aa0;
GetFn baseRec = (GetFn)0x490d00;
const GetFn cityRec = (GetFn)0x490a10;
TRADE_CALL_CONST GetFn corpsRec = (GetFn)0x490ad0;
ValidFn gameValid = (ValidFn)0x47a630;
// 0x47a630 calls the object's vfunc +8: guard against records never constructed (no vtable)
int objValid(const void* o) { return o && *(void* const*)o && gameValid(o); }
const IdOfFn unitIdOf = (IdOfFn)0x4917c0, personIdOf = (IdOfFn)0x491310;
TRADE_CALL_CONST AddGoldFn addGold = (AddGoldFn)0x4ae2a0;
TRADE_CALL_CONST AddGoldFn addFood = (AddGoldFn)0x4ae300, addTroops = (AddGoldFn)0x4ae3c0;
TRADE_CALL_CONST GoldFn baseGold = (GoldFn)0x486c80;
TRADE_CALL_CONST GoldFn baseFood = (GoldFn)0x486df0;
typedef int (__thiscall* WeaponFn)(void*,int);
typedef int (__thiscall* AddWeaponFn)(uint32_t,void*,int,int);
const WeaponFn baseWeapon=(WeaponFn)0x486950, weaponCapacity=(WeaponFn)0x486a30;
const AddWeaponFn addWeapon=(AddWeaponFn)0x4ae430;
bool g_militaryReady=false;
TRADE_CALL_CONST SetIntFn unitFoodSet = (SetIntFn)0x496280, unitTroopsSet = (SetIntFn)0x4961f0;
TRADE_CALL_CONST SetIntFn unitGoldSet = (SetIntFn)0x496250;   // unit +0x1c, clamped natively (10000 / transport 100000)
const TempNewFn tempNew = (TempNewFn)0x4aa610;
const PersonFn tempFree = (PersonFn)0x4acae0;
const PersonIntFn setCorps = (PersonIntFn)0x4a32f0, setStatus = (PersonIntFn)0x4a5af0;
const CountFn freeSpecials = (CountFn)0x4910e0;
TRADE_CALL_CONST SortieFn sortie = (SortieFn)0x5883f0;
TRADE_CALL_CONST RemoveFn removeUnit = (RemoveFn)0x4bccd0;
TRADE_CALL_CONST OrderFn setOrder = (OrderFn)0x5a8d20;
TRADE_CALL_CONST ExecFn execOrder = (ExecFn)0x5ad980;
const IdleFn waitingForPlayer = (IdleFn)0x57dcd0;
const SetAbilFn setAbility = (SetAbilFn)0x48a7d0;
TRADE_CALL_CONST AbilFn ability = (AbilFn)0x489030;
#undef TRADE_CALL_CONST
typedef int (__thiscall* HasSkillFn)(void*,int);
HasSkillFn personHasSkill = (HasSkillFn)0x4890f0;
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
    int ai = 1, aiHook = 1, aiMonth = 6, aiRange = 80, aiMinGold = 6000, aiInvest = 4000, aiReserve = 2000, aiMargin = 25;
    int tax = 10, taxHub = 5, relTax = 40, rob = 15, robLoss = 50, loot = 60, spoilMin = 5, spoilMax = 49;
    int escort = 500, food = 3000, arrive = 3, maxTurns = 90, playerAuto = 1;
    int ui = 1, uiAi = 1, monsoonAll = 1, PT = 1100, specials = 100;        // phase 2 panel, AI caravans on the map; monsoon for all ships; officer records
    int keys = 1, aiOfficers = 1, aiOfficerMin = 4, aiShips = 1, aiShipRange = 200;         // phase 4: panel keys; the AI's merchant officers
} g_cfg;

TraceLogFn g_log = nullptr;
TradeSetup g_s;
bool g_on = false;
Data g_data;
Market g_mk;
Rng g_rng;
std::vector<std::vector<int>> g_inRange, g_shipRange;
CRITICAL_SECTION g_cs;
struct Lock { Lock() { EnterCriticalSection(&g_cs); } ~Lock() { LeaveCriticalSection(&g_cs); } };

enum : uint32_t { F_TEMP = 1, F_PLAYER = 2, F_LOOTED = 4, F_MANUAL = 8 };   // F_MANUAL: the player moves it (no 自動行軍)
struct LegacyCaravan {
    int32_t unit, leader, force, corps, origin, dest, good, qty, cost, taxDue, born, lastCity;
    uint32_t flags;
};
static_assert(sizeof(LegacyCaravan) == 52, "v1-v3 caravan wire format");
enum { TRAVELLING, DOCKED, WAITING, FINISHED };
struct Caravan : LegacyCaravan {
    uint32_t id = 0;
    int state = TRAVELLING, port = -1, lastProgress = 0, maxLoad = 20;
    int32_t shipType = CARAVAN;
    int legOrigin=-1, embarkBase=-1, arrivalBase=-1;
    int transferStage=0, transferUntil=0; // 1 city -> port, 2 port -> city; days in the game calendar
    uint32_t lastPos = 0xffffffff;
    uint32_t dockHex = 0xffffffff;    // recomputed after load: native paths depend on current map/season
    int routeCost = 0;
    std::vector<uint32_t> route;
    Cargo cargo;
    int64_t transitLimit = 0, transitPaid = 0;
    Ledger ledger;
    std::vector<int32_t> taxed;
    std::string reason;
    bool tempReleased = false;
    tradeauto::Voyage automatic;
};
std::vector<Caravan> g_car;
std::vector<tradeauto::Plan> g_autoPlans;
void autoTick(int city=-1,int force=-1);
void autoRefund(Caravan& k);
bool autoPay(Caravan& k,int64_t amount);
std::vector<ClosedVoyage> g_closed;
uint32_t g_nextCar = 1;
struct Credit { int32_t force, origin; int64_t gold, troops, food; };
std::vector<Credit> g_credit;
struct TaxRemainder { int32_t payer, receiver, remainder; };
std::vector<TaxRemainder> g_saleTax;
struct PurchaseRec { uint32_t id; int32_t day,force,city,good,qty; int64_t cost; };
std::vector<PurchaseRec> g_purchases;
uint32_t g_nextPurchase=1;
const size_t PURCHASE_MAX=512;
std::vector<uint8_t> g_rawBlock;
bool g_readOnly = false;
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
std::vector<LONG> g_pausedLeader;             // leader + 1, zero allows movement; fixed storage after install
std::vector<LONG> g_shipLeader;               // water-only merchant ships, guarded by leader identity
std::vector<LONG> g_shipQuay;                 // live origin; two land hexes for initial quay access only
std::vector<LONG> g_tradeLeader;              // identity guard: reused native unit slots must not inherit acceleration
// A compact bit per map hex keeps many simultaneous caravans bounded in x86.
struct DangerCells {
    int width=g_s.W,height=g_s.H;
    std::vector<uint64_t> bits=std::vector<uint64_t>(((size_t)width*height+63)/64);
    void insert(uint32_t pos) {
        int x=(int16_t)pos,y=(int16_t)(pos>>16);
        if(x<0 || y<0 || x>=height || y>=width)return;
        size_t i=(size_t)x*width+y;bits[i/64]|=uint64_t(1)<<(i%64);
    }
    void erase(uint32_t pos) {
        int x=(int16_t)pos,y=(int16_t)(pos>>16);
        if(x<0 || y<0 || x>=height || y>=width)return;
        size_t i=(size_t)x*width+y;bits[i/64]&=~(uint64_t(1)<<(i%64));
    }
    bool count(uint32_t pos) const {
        int x=(int16_t)pos,y=(int16_t)(pos>>16);
        if(x<0 || y<0 || x>=height || y>=width)return false;
        size_t i=(size_t)x*width+y;return (bits[i/64]>>(i%64))&1;
    }
};
std::vector<std::shared_ptr<const DangerCells>> g_routeDanger;

thread_local const uint8_t* g_routeProbe = nullptr; // query a paused unit without enabling actual movement
uint8_t* g_unit0 = nullptr;                    // unit 0 (the sea-cost callback's pointer -> index)
int g_month0 = 0;                              // month of the year (0 = January) at the last turn
// phase 4: merchant officers' experience (saved)
struct MerchRec { int32_t person, exp, trips; };
std::vector<MerchRec> g_merch;
int merchExpOf(int person) { for (auto& m : g_merch) if (m.person == person) return m.exp; return 0; }
void merchGain(int person, int gain) {
    for (auto& m : g_merch) if (m.person == person) { m.exp += gain; m.trips++; return; }
    g_merch.push_back({ person, gain, 1 });
}

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
// a free hex 1..2 from (lo, hi), nearest to (flo, fhi); false if none. skip: packed hexes not to use (tried already)
bool hexNear(int lo, int hi, int flo, int fhi, int* olo, int* ohi, const std::vector<uint32_t>* skip = nullptr) {
    int best = 1 << 30;
    for (int r = lo - 3; r <= lo + 3; r++)
        for (int c = hi - 3; c <= hi + 3; c++) {
            int d = hexDist(lo, hi, r, c);
            if (d < 1 || d > 2 || !freeHex(r, c)) continue;
            if (skip) { uint32_t pk = (uint16_t)r | (uint32_t)(uint16_t)c << 16; bool used = false; for (uint32_t x : *skip) used |= x == pk; if (used) continue; }
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
    if (k.tempReleased || k.state == FINISHED || (k.flags & F_LOOTED) || k.unit < 0 || k.unit >= g_s.U) return nullptr;
    uint8_t* u = unitRec(WORLD, k.unit);
    return u && objValid(u) && *(int*)(u + 0xc) == k.leader && vcall(u, 0x40) == k.force ? u : nullptr;
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
int tempLive() { int n = 0; for (auto& k : g_car) n += k.state != FINISHED && !k.tempReleased && (k.flags & F_TEMP) != 0; return n; }
bool tempLeader(int leader) {
    for(auto& k:g_car) if(k.state!=FINISHED && !k.tempReleased && (k.flags&F_TEMP) && k.leader==leader)return true;
    return false;
}
// UI readers must not wait for g_cs: sortie can hold it while waiting for the UI
// thread. Only the configured special slots are eligible; normal/custom officers never are.
uint8_t* g_person0=nullptr;
std::vector<LONG> g_hiddenTempForces(100); // force+1; native district reassignment may change corps
volatile LONG g_hiddenRows=0;
void markHiddenTemp(int leader,int force) {
    int slot=leader-(g_cfg.PT-g_cfg.specials);
    if(slot>=0 && slot<g_cfg.specials)InterlockedExchange(&g_hiddenTempForces[slot],force+1);
}
bool tempPersonMatches(uint8_t* p,int force) {
    return !memcmp(p+4,"\xb0\xd3\0\0\0",5) && p[9]==0 && vcall(p,0x40)==force;
}
// Old saves can retain a generated merchant after its caravan record is gone.
// Only called inside the special-slot bounds below. Match the complete generated
// name, base abilities and dates, not merely an officer called 商. This is a UI
// exclusion only: do not free a person whose old unit/cargo ownership is unknown.
bool legacyTradePerson(const uint8_t* p) {
    static const uint8_t name[40]={0xb0,0xd3};
    static const uint8_t stats[5]={40,30,50,60,50};
    if(memcmp(p+4,name,sizeof name) || memcmp(p+0xc8,stats,sizeof stats))return false;
    int birth=*(const int*)(p+0x48),status=*(const int*)(p+0xa0);
    return birth>=130 && birth<=323 && *(const int*)(p+0x44)==birth+20 &&
           *(const int*)(p+0x4c)==std::min(birth+90,353) &&
           status>=0 && status<=4 && *(const int*)(p+0x94)>=0;
}
bool hiddenTradePerson(const uint8_t* p) {
    uintptr_t first=(uintptr_t)g_person0+(size_t)(g_cfg.PT-g_cfg.specials)*0x190,addr=(uintptr_t)p;
    if(!g_on || !g_person0 || addr<first || addr-first>=(size_t)g_cfg.specials*0x190 || (addr-first)%0x190)return false;
    LONG tag=InterlockedCompareExchange(&g_hiddenTempForces[(addr-first)/0x190],0,0);
    return tag ? tempPersonMatches((uint8_t*)p,tag-1) : legacyTradePerson(p);
}
void rebuildHiddenTemps() {
    std::vector<LONG> forces(g_cfg.specials);
    for(const auto& k:g_car) if((k.flags&F_TEMP) && !(k.flags&F_LOOTED) && !k.tempReleased && k.state!=FINISHED) {
        int slot=k.leader-(g_cfg.PT-g_cfg.specials);
        if(slot>=0 && slot<g_cfg.specials)forces[slot]=k.force+1;
    }
    for(int i=0;i<g_cfg.specials;i++)InterlockedExchange(&g_hiddenTempForces[i],forces[i]);
}
void freeTradeTemp(uint8_t* p,int leader) {
    if(leader<g_cfg.PT-g_cfg.specials || leader>=g_cfg.PT)return;
    tempFree(PROC,p);
    // Native death removes district/relationship references but leaves a valid
    // dead record. Reset our reserved slot so the native allocator can reuse it.
    ((void(__thiscall*)(void*,int))(*(void***)p)[0x20/4])(p,0);
    markHiddenTemp(leader,-1);
}
const char* goodName(int g) { return g >= 0 && g < g_data.G ? g_data.goodName[g].c_str() : "?"; }
const char* cityName(int c) { return c >= 0 && c < g_data.C ? g_data.cityName[c].c_str() : "?"; }

// a temporary leader still holding the slot we gave it (name 商, our force) -> release it
// (0x4bccd0 releases it when the unit is removed; this catches a leader taken away from its unit, e.g. captured).
void releaseTempIfOurs(const Caravan& k) {
    if(k.tempReleased || !(k.flags & F_TEMP)) return;
    // Historical and custom officers are never candidates for tempFree.
    if(k.leader<g_cfg.PT-g_cfg.specials || k.leader>=g_cfg.PT)return;
    uint8_t* p = personRec(WORLD, k.leader);
    if (!p || !objValid(p)) return;
    if (!tempPersonMatches(p,k.force)) return;
    int users = 0;
    for (auto& o : g_car) users += o.state != FINISHED && !o.tempReleased && o.leader == k.leader;
    if (users > 1) return;                                   // the slot already leads another caravan
    if (*(int*)(p + 0x9c) >= g_s.C + 45) return;             // in a unit (locations N.. are units, N = C + 10 gates + 35 ports)
    freeTradeTemp(p,k.leader);
}

// leader bonus: politics +-5% on prices, intelligence +-25% on the load
std::wstring big5(const uint8_t* p, int n) {
    char b[16] = {}; memcpy(b, p, n);
    wchar_t w[16] = {};
    int len = MultiByteToWideChar(950, 0, b, -1, w, 16);
    return len > 1 ? std::wstring(w, len - 1) : L"";
}
std::wstring personName(uint8_t* p) { return big5(p + 4, 4) + big5(p + 9, 4); }
// phase 4: 商才 of an officer (known merchant by id / Koei name, or politics)
int talentOf(int leader, uint8_t* p) {
    std::wstring n = personName(p);
    char u8[64] = {};
    WideCharToMultiByte(CP_UTF8, 0, n.c_str(), -1, u8, sizeof u8, nullptr, nullptr);
    return merchantTalent(g_mk.p, g_data.merchantTalent(leader, u8), ability(p, 3));
}
// Count only serving officers physically in this city, never the special-person pool.
// Native skill matching also recognizes 147 inside a configured composite skill.
int cityMerchantOfficer(int force,int city) {
    if(!g_person0 || force<0 || force>=g_s.R || city<0 || city>=g_data.C)return -1;
    for(int id=0;id<g_cfg.PT-g_cfg.specials;++id) {
        auto p=g_person0+(size_t)id*0x190;
        int status=*(int*)(p+0xa0);
        if(*(int*)(p+0x9c)!=city || status<0 || status>3 || !*(void**)p)continue;
        if(vcall(p,8) && vcall(p,0x40)==force && personHasSkill(p,SKILL_MERCHANT))return id;
    }
    return -1;
}
bool cityMerchantSkill(int force,int city) {return cityMerchantOfficer(force,city)>=0;}
int cityAutoLimit(int force,int city,bool ai=false,int* provider=nullptr) {
    int merchant=cityMerchantOfficer(force,city);if(provider)*provider=merchant;
    return (ai?tradeauto::AI_CITY_LIMIT:tradeauto::PLAYER_CITY_LIMIT)*(merchant>=0?2:1);
}
int cityTradeFee(int force,int origin,int shipType,bool nextLeg=false,int* provider=nullptr) {
    int merchant=baseForce(origin)==force?cityMerchantOfficer(force,origin):-1;if(provider)*provider=merchant;
    return merchantFee(transportFee(g_mk.p,shipType,nextLeg),merchant>=0);
}
int cityAutoCount(int force,int city,bool ai=false) {
    int count=0;
    for(auto& k:g_car)if(k.state!=FINISHED && k.force==force && k.origin==city &&
        (ai ? !(k.flags&F_PLAYER) : k.automatic.caravan!=0))++count;
    return count;
}
#include "trade_skill_telemetry.inc"
int leaderLevel(int leader) {
    uint8_t* p = leader >= 0 ? personRec(WORLD, leader) : nullptr;
    if (!p || !objValid(p)) return 0;
    return merchantEffective(g_mk.p, merchExpOf(leader), talentOf(leader, p));
}
// price permille and load of a leader, merchant level included; *mb: the level's tax / robbery factors
void leaderBonus(int leader, int* pricePermille, int* capPct, MerchantBonus* mb = nullptr) {
    uint8_t* p = personRec(WORLD, leader);
    int pol = p ? ability(p, 3) : 50, intel = p ? ability(p, 2) : 50;
    leaderFactors(pol, intel, pricePermille, capPct);   // + on sales, - on purchases (permille); load (trade_model.h)
    MerchantBonus b = merchantBonus(g_mk.p, leaderLevel(leader));
    *pricePermille += b.pp; *capPct += b.capPct;
    if (mb) *mb = b;
}
void leaderOrTemp(const Caravan& k, int* pp, int* capPct, MerchantBonus* mb = nullptr) {
    if (k.flags & F_TEMP) { leaderFactors(TEMP_POL, TEMP_INT, pp, capPct); if (mb) *mb = MerchantBonus(); }
    else leaderBonus(k.leader, pp, capPct, mb);
}
int8_t headingOf(const Caravan& k) {
    int d = g_data.lo[k.dest] - g_data.lo[k.origin];
    return d > 0 ? 1 : d < 0 ? -1 : 2;
}
uint8_t* nativePort(int id);
void rebuildDirs() {                                     // the sea-cost callback's per-unit table (phase 3); updated in
    rebuildHiddenTemps();
    if (g_unitDir.size() != (size_t)g_s.U) return;       // place (allocated once at install: the callback may run on another thread)
    std::vector<int8_t> dirs(g_s.U, 0);
    for (auto& k : g_car) if (k.state != FINISHED && !k.tempReleased && !(k.flags & F_LOOTED) && k.unit >= 0 && k.unit < g_s.U) dirs[k.unit] = headingOf(k);
    for (int i = 0; i < g_s.U; i++) if (g_unitDir[i] != dirs[i]) g_unitDir[i] = dirs[i];
    if (g_pausedLeader.size() == (size_t)g_s.U) {
        std::vector<LONG> paused(g_s.U, 0);
        for (auto& k : g_car) if (!k.tempReleased && !(k.flags & F_LOOTED) && k.unit >= 0 && k.unit < g_s.U && (k.state == DOCKED || k.state == WAITING || k.transferStage)) paused[k.unit] = k.leader + 1;
        for (int i = 0; i < g_s.U; i++) InterlockedExchange(&g_pausedLeader[i], paused[i]);
    }
    if(g_shipLeader.size()==(size_t)g_s.U && g_shipQuay.size()==(size_t)g_s.U) {
        std::vector<LONG> leaders(g_s.U,0);
        for(auto& k:g_car) if(k.shipType==MERCHANT_SHIP && k.state!=FINISHED && !k.tempReleased && !(k.flags&F_LOOTED) && k.unit>=0 && k.unit<g_s.U)
            {int lo,hi;cityPos(k.legOrigin>=0?k.legOrigin:k.origin,&lo,&hi);if(auto b=nativePort(k.embarkBase)){auto p=posOf(b);lo=p[0];hi=p[1];}
             leaders[k.unit]=k.leader+1;InterlockedExchange(&g_shipQuay[k.unit],pack(lo,hi));}
        for(int i=0;i<g_s.U;++i) InterlockedExchange(&g_shipLeader[i],leaders[i]);
    }
    if(g_tradeLeader.size()==(size_t)g_s.U) {
        std::vector<LONG> leaders(g_s.U,0);
        for(auto& k:g_car) if(k.state!=FINISHED && !k.tempReleased && !(k.flags&F_LOOTED) && k.unit>=0 && k.unit<g_s.U)
            leaders[k.unit]=k.leader+1;
        for(int i=0;i<g_s.U;++i) {
            InterlockedExchange(&g_tradeLeader[i],leaders[i]);
            if(!leaders[i] && i<(int)g_routeDanger.size())std::atomic_store(&g_routeDanger[i],std::shared_ptr<const DangerCells>{});
        }
    }
}
bool __cdecl tradeMovingUnit(const uint8_t* unit) {
    uintptr_t addr=(uintptr_t)unit,first=(uintptr_t)g_unit0;
    if(!first || addr<first || (addr-first)%0xf4) return false;
    size_t i=(addr-first)/0xf4;
    if(i>=g_tradeLeader.size() || *(const int*)(unit+8)!=1) return false;
    LONG leader=InterlockedCompareExchange(&g_tradeLeader[i],0,0);
    return leader && leader==*(const int*)(unit+0xc)+1;
}
bool merchantShip(const uint8_t* unit) {
    if(!tradeMovingUnit(unit))return false;
    size_t i=((uintptr_t)unit-(uintptr_t)g_unit0)/0xf4;
    return i<g_shipLeader.size() && InterlockedCompareExchange(&g_shipLeader[i],0,0)==*(const int*)(unit+0xc)+1;
}
bool waterHex(const uint8_t* h) {
    if(!h)return false;
    int t=*(const uint32_t*)(h+4)&31;return t==7 || t==8;
}
#include "trade_ports.inc"
int continuationFee(const Caravan& k,int dest) {
    int fee=cityTradeFee(k.force,k.origin,k.shipType,true);
    if(k.shipType==MERCHANT_SHIP) {
        auto from=k.arrivalBase<0 ? shipAccess(k.port) : shipAccessAt(k.port,k.arrivalBase);
        fee+=from.fee()+shipAccess(dest).fee();
    }
    return fee;
}
void credit(int,int,int64_t,int64_t,int64_t);
#include "trade_safety_runtime.inc"
bool __cdecl shipTerrainStepAllowed(const uint8_t* unit,const uint8_t* h) {
    if(!merchantShip(unit) || waterHex(h))return true;
    const int16_t* p=posOf((void*)unit);
    // Water may be three hexes from the city, so permit two initial land hexes.
    // Once afloat, no landing.
    if(waterHex(cell(p[0],p[1])) || !h || h<g_s.hex20 || h>=g_s.hex20+(size_t)g_s.W*g_s.H*20)return false;
    size_t i=((uintptr_t)unit-(uintptr_t)g_unit0)/0xf4, tile=(h-g_s.hex20)/20;
    if(i>=g_shipQuay.size())return false;
    uint32_t quay=InterlockedCompareExchange(&g_shipQuay[i],0,0);int lo=(int16_t)quay,hi=(int16_t)(quay>>16);
    return hexDist(lo,hi,p[0],p[1])<=2 && hexDist(lo,hi,(int)(tile/g_s.W),(int)(tile%g_s.W))<=2;
}
bool __cdecl shipStepAllowed(const uint8_t* unit,const uint8_t* hex) {
    return shipTerrainStepAllowed(unit,hex) && safeTradeStep(unit,hex);
}
bool __cdecl movementAllowed(const uint8_t* unit) {
    if (unit == g_routeProbe) return true;
    uintptr_t addr = (uintptr_t)unit, first = (uintptr_t)g_unit0;
    if (!first || addr < first || (addr - first) % 0xf4) return true;
    size_t i = (addr - first) / 0xf4;
    if (i >= g_pausedLeader.size()) return true;
    LONG leader = InterlockedCompareExchange(&g_pausedLeader[i], 0, 0);
    return !leader || leader != *(const int*)(unit + 0xc) + 1;
}
void recordDone(const Caravan& k, int64_t net, uint32_t flags) {
    if (!(k.flags & F_PLAYER)) return;
    DoneRec r = { today(), k.force, k.origin, k.dest, k.good, k.qty, k.cost, (int32_t)net, flags };
    Lock l;
    if(flags && !k.cargo.lots.empty()) {
        for(auto& lot:k.cargo.lots) {
            r.good=lot.good;r.origin=lot.origin;r.qty=lot.qty;r.cost=(int32_t)lot.cost;
            g_done.push_back(r);
        }
    } else g_done.push_back(r);
    if (g_done.size() > DONE_MAX) g_done.erase(g_done.begin(),g_done.end()-DONE_MAX);
}
void syncCargo(Caravan& k);
void closeLedger(Caravan& k) {
    if (k.state == FINISHED) return;
    autoRefund(k);
    k.ledger.losses += k.cargo.cost();
    g_closed.push_back({k.id, k.force, k.origin, k.dest, today(), k.ledger});
    if (g_closed.size() > DONE_MAX) g_closed.erase(g_closed.begin());
    k.cargo.lots.clear(); syncCargo(k); k.state = FINISHED;
}

// ---------------------------------------------------------------- caravans
uint8_t* payee(int f, int c);
void settleCredits();
void credit(int f, int c, int64_t gold, int64_t troops = 0, int64_t food = 0) {
    for (auto& v : g_credit) if (v.force == f && v.origin == c) {
        v.gold += gold; v.troops += troops; v.food += food; settleCredits(); return;
    }
    g_credit.push_back({ f, c, gold, troops, food }); settleCredits();
}
void syncCargo(Caravan& k) {
    k.qty = k.cargo.count(); k.cost = (int32_t)std::min<int64_t>(INT_MAX, k.cargo.cost());
    k.good = k.cargo.lots.empty() ? 0 : k.cargo.lots.front().good;
}
void refundUnit(Caravan& k, uint8_t* u) {
    if (!u) return;
    int troops = vcall(u, 0x50), food = *(int*)(u + 0x20);
    unitTroopsSet(u, 0); unitFoodSet(u, 0);
    credit(k.force, k.origin, 0, troops, food);
}
bool queryRoute(uint8_t* u, uint32_t start, const std::vector<uint32_t>& targets, std::vector<uint32_t>* path, int* cost, const int* relax = nullptr) {
    refreshRouteDanger(u, relax);
    const uint8_t* previous = g_routeProbe;
    g_routeProbe = u;
    bool ok = false;
    __try { ok = pathfindRoute(u, start, targets, path, cost); }
    __finally { g_routeProbe = previous; }
    return ok;
}
bool shipWaterPath(const std::vector<uint32_t>& path,uint32_t quay) {
    if(path.empty() || !waterHex(cell((int16_t)path.back(),(int16_t)(path.back()>>16))))return false;
    bool afloat=false;
    for(auto p:path) {
        int lo=(int16_t)p,hi=(int16_t)(p>>16);
        if(waterHex(cell(lo,hi)))afloat=true;
        else if(afloat || !cell(lo,hi) || hexDist((int16_t)quay,(int16_t)(quay>>16),lo,hi)>2)return false;
    }
    return true;
}
bool marketRoute(uint8_t* u, int dest, std::vector<uint32_t>* path, int* cost, const std::vector<uint32_t>* skip = nullptr, int anchor=-1) {
    bool ship=merchantShip(u);
    int lo, hi; cityPos(dest, &lo, &hi);
    if(ship) {
        auto a=anchor<0 ? shipAccess(dest) : shipAccessAt(dest,anchor);
        if(a.base<0)return false;
        lo=a.lo;hi=a.hi;
    }
    const int16_t* pos = posOf(u); uint32_t start = pack(pos[0], pos[1]);
    std::vector<uint32_t> targets;
    int radius=ship?3:2;
    for (int r = lo - radius; r <= lo + radius; r++) for (int c = hi - radius; c <= hi + radius; c++) {
        int dist = hexDist(lo, hi, r, c); uint32_t target = pack(r, c);
        if (dist < 1 || dist > radius || (!freeHex(r, c) && target != start)) continue;
        if (skip && std::find(skip->begin(), skip->end(), target) != skip->end()) continue;
        if(ship && !waterHex(cell(r,c)))continue;
        targets.push_back(target);
    }
    if(!queryRoute(u,start,targets,path,cost) || path->empty()) {
        // Hostile fire around the berths or the unit itself would otherwise strand it for good:
        // keep avoiding danger on the way, accept it at both ends.
        const int relax[6]={pos[0],pos[1],4,lo,hi,radius+4};
        path->clear();*cost=0;
        if(!queryRoute(u,start,targets,path,cost,relax) || path->empty())return false;
    }
    // Defense in depth: no large-hold overland travel even if a native search
    // ever bypasses the step callback. Arrival always remains outside the city.
    if(ship && !shipWaterPath(*path,InterlockedCompareExchange(&g_shipQuay[((uintptr_t)u-(uintptr_t)g_unit0)/0xf4],0,0))) {path->clear();*cost=0;return false;}
    return true;
}
uint32_t cacheRoute(Caravan& k, uint8_t* u, const std::vector<uint32_t>* skip = nullptr) {
    if (!marketRoute(u, k.dest, &k.route, &k.routeCost, skip,k.arrivalBase)) {
        k.dockHex = 0xffffffff; k.state = WAITING; k.reason = "no safe reachable market berth"; rebuildDirs();
        return 0xffffffff;
    }
    k.dockHex = k.route.back();
    return k.dockHex;
}
uint32_t issueOrder(Caravan& k, uint8_t* u, const std::vector<uint32_t>* skip = nullptr) {
    uint32_t target=cacheRoute(k,u,skip);
    if (target!=0xffffffff) setOrder(EXEC,u,1,target,-1);
    return target;
}

// creates a caravan; leader < 0: temporary 商 officer. Returns "" or an error.
std::string createCaravanOrder(int origin, int dest, const std::vector<OrderLine>& order, int leader, bool ai, Caravan* out, const tradeauto::Voyage* automatic=nullptr, int shipType=CARAVAN) {
    if (origin < 0 || origin >= g_data.C || dest < 0 || dest >= g_data.C || origin == dest) return "bad cities";
    if(!validTransport(shipType))return "invalid transport type";
    ShipAccess from,to;
    if(shipType==MERCHANT_SHIP) {from=shipAccess(origin);to=shipAccess(dest);}
    if(shipType==MERCHANT_SHIP && (!movementReady() || !pathfindReady() || from.base<0 || to.base<0))
        return "merchant ship requires water berths at both ports";
    if(order.empty()) return "nothing to carry";
    int good=order.front().good;
    uint8_t* b = base(origin);
    if (!b) return "origin is not a live base";
    int force = vcall(b, 0x40), corps = vcall(b, 0x44);
    if (force < 0 || force >= g_s.R) return "origin has no regular force";
    int capacityMerchant=-1,capacityLimit=0;
    if(ai || automatic){int count=cityAutoCount(force,origin,ai);capacityLimit=cityAutoLimit(force,origin,ai,&capacityMerchant);
        if(count>=capacityLimit)return "city automatic caravan limit reached";}
    MerchantTelemetry capacitySource(capacityMerchant,force,origin);
    if (g_readOnly) return "trade save is read-only";
    int df = baseForce(dest);
    if (df < 0 || stance(force, df) == 0) return "destination market is closed to this force";
    bool temp = leader < 0;
    if (!temp) {
        uint8_t* p = personRec(WORLD, leader);
        if (!p || !objValid(p)) return "no such officer";
        if (vcall(p, 0x40) != force) return "the officer is not of the origin's force";
        if (*(int*)(p + 0x9c) != origin) return "the officer is not in the origin city";
        if (*(int*)(p + 0xa0) < 0 || *(int*)(p + 0xa0) > 3) return "officer is not available";
        for (auto& car : g_car) if (car.state != FINISHED && !car.tempReleased && car.leader == leader) return "officer already leads a caravan";
    } else {
        if (tempLive() >= g_cfg.tempMax) return "temporary caravan limit (trade_temp_max) reached";
        if (freeSpecials(WORLD) - 1 < g_cfg.tempKeep) return "too few free special officer slots (trade_temp_keep)";
    }
    int pp = 0, capPct = 100;
    if (!temp) leaderBonus(leader, &pp, &capPct);           // politics / intelligence + merchant level (phase 4)
    else leaderFactors(TEMP_POL, TEMP_INT, &pp, &capPct);   // 商: politics 60, intelligence 50
    int cap = capacity(g_data, g_mk.p, origin, dest, capPct,shipType);
    Cargo prepared; int64_t cost=0;
    int feeMerchant=-1,chargedFee=cityTradeFee(force,origin,shipType,false,&feeMerchant);
    MerchantTelemetry feeSource(feeMerchant,force,origin);
    int ordinaryFee=transportFee(g_mk.p,shipType,false),fee=chargedFee+from.fee()+to.fee();
    if(!quoteOrder(g_mk,origin,order,cap,pp,baseGold(b),fee,&prepared,&cost))
        return "invalid cargo combination: stock, capacity or gold";
    int qty=prepared.count(); int64_t goods=prepared.cost();
    if (cost > INT_MAX) return "purchase exceeds the game gold range";
    int food=geographicFood(origin,dest,shipType,g_cfg.escort);
    if(food<0)return "voyage provisions exceed transport capacity";
    if (baseFood(b) < food || vcall(b, 0x50) < g_cfg.escort) return "not enough troops or food";
    int gold = baseGold(b);
    if (cost > gold) return "not enough gold (" + std::to_string(cost) + " needed, " + std::to_string(gold) + " in the city)";
    int sx, sy; cityPos(origin, &sx, &sy);
    uint8_t* launch=b;
    if(shipType==MERCHANT_SHIP && from.base!=origin) {launch=nativePort(from.base);sx=from.lo;sy=from.hi;}
    int tlo, thi;
    if (!hexNear(sx, sy, sx, sy, &tlo, &thi)) return "no free hex next to the origin city";
    // the leader
    uint8_t* p;
    if (temp) {
        uint8_t* tmpl = personRec(WORLD, g_cfg.tmpl);
        if (!tmpl || !objValid(tmpl)) return "template officer (trade_template) missing";
        p = tempNew(PROC, tmpl, -1, -1);
        if (!p || !objValid(p)) return "no free special officer slot";
        leader = personIdOf(WORLD, p);
        *(int*)(p + 0x98) = origin;                          // home base (as 0x4af0e0 does)
        setCorps(PROC, p, corps);
        memset(p + 4, 0, 0x13 - 4 + 25);                     // 姓 名 字 and the reading
        p[4] = 0xb0; p[5] = 0xd3;                            // 「商」 (Big5), no given name: the unit shows as 商隊
        markHiddenTemp(leader,force);                       // hide before publishing the ordinary officer status
        setStatus(PROC, p, 3);
        static const int abil[5] = { 40, 30, 50, 60, 50 };   // 統率 武力 智力 政治 魅力
        for (int i = 0; i < 5; i++) setAbility(p, i, abil[i]);
        int year = today() / 360 + 1;
        setBirth(p, year - 30); setDeath(p, year + 60 > 0x161 ? 0x161 : year + 60); setAppear(p, year - 10);
        ((void(__thiscall*)(void*))(*(void***)p)[0x34 / 4])(p);   // recompute the current abilities (as 0x4af2fb)
    } else p = personRec(WORLD, leader);
    // the transport unit (sortie record, 0x80 bytes)
    uint8_t rec[0x80] = {};
    *(void**)(rec + 0) = launch;
    *(int*)(rec + 4) = 1;                                    // transport
    *(int*)(rec + 8) = leader; *(int*)(rec + 0xc) = -1; *(int*)(rec + 0x10) = -1;
    *(int*)(rec + 0x14) = 0;                                 // gold carried: none (the cargo is the trade code's)
    *(int*)(rec + 0x18) = food;
    *(int*)(rec + 0x1c) = g_cfg.escort;
    for (int i = 0; i < 12; i++) *(int*)(rec + 0x20 + 4 * i) = -1;
    std::vector<std::pair<int,int>> oldMarket;
    for(auto& l:order) oldMarket.push_back({g_mk.at(origin,l.good),g_mk.available(origin,l.good)});
    auto restoreMarket=[&]() { for(size_t i=0;i<order.size();++i) { int g=order[i].good; g_mk.at(origin,g)=oldMarket[i].first;g_mk.stock[(size_t)origin*g_data.G+g]=oldMarket[i].second; } };
    int paidGold = -addGold(PROC, b, -(int)cost);
    int paidFood = -addFood(PROC, b, -food);
    int paidTroops = -addTroops(PROC, b, -g_cfg.escort);
    if (paidGold != cost || paidFood != food || paidTroops != g_cfg.escort) {
        credit(force, origin, paidGold, paidTroops, paidFood);
        if (temp) freeTradeTemp(p,leader);
        return "resource reservation failed";
    }
    for(auto& l:order) g_mk.buy(origin,l.good,l.qty,pp);
    int uid = sortie(CMDQ, rec);
    uint8_t* u = uid >= 0 && uid < g_s.U ? unitRec(WORLD, uid) : nullptr;
    if (!u || !objValid(u) || *(int*)(u + 0xc) != leader) {
        if (temp) freeTradeTemp(p,leader);
        restoreMarket();
        credit(force, origin, paidGold, paidTroops, paidFood);
        return "the game did not create the unit";
    }
    Caravan k = {};
    k.unit = uid; k.leader = leader; k.force = force; k.corps = corps; k.origin = origin; k.dest = dest; k.good = good;
    k.qty = qty; k.cost = (int32_t)goods; k.taxDue = 0; k.born = today(); k.lastCity = origin;
    k.flags = (temp ? F_TEMP : 0) | (isPlayerForce(force) ? F_PLAYER : 0);
    k.id = g_nextCar++;
    if(automatic) {k.automatic=*automatic;k.automatic.caravan=k.id;}
    k.lastProgress = today(); k.maxLoad = cap; k.shipType=shipType;
    if(shipType==MERCHANT_SHIP) {
        k.legOrigin=origin;k.embarkBase=from.base;k.arrivalBase=to.base;
        if(from.days()) {k.transferStage=1;k.transferUntil=today()+from.days();k.reason="city-to-port freight transfer";}
    }
    k.cargo=std::move(prepared);
    k.transitLimit=goods*g_mk.p.transitCap/100;
    k.ledger.purchases = goods; k.ledger.fees = fee;
    int capacityBefore=capacitySource.person?cityAutoCount(force,origin,ai):-1;
    { Lock l; g_car.push_back(k); rebuildDirs(); }
    // Register before executing movement: destruction callbacks must recognize the first leg.
    Caravan& live = g_car.back();
    bool reachable = issueOrder(live, u) != 0xffffffff;
    bool supplied=false;std::string supplyError;
    if(reachable) {
        int before=*(int*)(u+0x20);live.port=origin;
        supplyError=supplyForRoute(live,u,live.route,dest,2*((from.days()+to.days()+9)/10));
        supplied=supplyError.empty();
        live.port=-1;paidFood+=std::max(0,*(int*)(u+0x20)-before);
    }
    bool moved = reachable && supplied && (live.transferStage || execOrder(EXEC, u) != 0);
    if (!moved && liveUnit(live) && !(live.flags & F_LOOTED)) {
        unitFoodSet(u, 0); unitTroopsSet(u, 0); removeUnit(PROC, u, 0);
        restoreMarket();
        credit(force, origin, paidGold, paidTroops, paidFood);
        { Lock l; g_car.pop_back(); rebuildDirs(); }
        return !reachable ? "no safe reachable market berth" : !supplyError.empty() ? supplyError : "the caravan could not leave the city";
    }
    k = live;
    merchantFeeCommitted(feeSource,k,dest,ordinaryFee,chargedFee,fee,false);
    if(capacityBefore>=0)merchantCapacityCommitted(capacitySource,k,ai,capacityBefore,cityAutoCount(force,origin,ai),capacityLimit);
    g_log("trade: caravan %s -> %s: %d x %s for %lld gold (force %d, unit %d, leader %d%s)%s", cityName(origin), cityName(dest),
          qty, order.size()>1 ? "mixed cargo" : goodName(good), (long long)cost, force, uid, leader, temp ? " 商" : "", ai ? " [AI]" : "");
    if (out) *out = k;
    return "";
}
std::string createCaravan(int origin,int dest,int good,int qty,int leader,bool ai,Caravan* out) {
    return createCaravanOrder(origin,dest,{{good,qty}},leader,ai,out);
}

// money to force f near city c: c itself if f still owns it, else f's city nearest to c; returns the base
uint8_t* payee(int f, int c) {
    if (baseForce(c) == f) return base(c);
    int best = -1, bd = 1 << 30;
    for (int i = 0; i < g_data.C; i++)
        if (baseForce(i) == f) { int d = hexDist(g_data.lo[i], g_data.hi[i], g_data.lo[c], g_data.hi[c]); if (d < bd) { bd = d; best = i; } }
    return best >= 0 ? base(best) : nullptr;
}

void settleCredits() {
    for (auto it = g_credit.begin(); it != g_credit.end();) {
        uint8_t* b = payee(it->force, it->origin);
        if (b) {
            // The native function adds before clamping to city capacity (signed int).
            it->gold -= addGold(PROC, b, (int)std::min<int64_t>(INT_MAX - baseGold(b), it->gold));
            it->troops -= addTroops(PROC, b, (int)std::min<int64_t>(60000, it->troops));
            it->food -= addFood(PROC, b, (int)std::min<int64_t>(500000, it->food));
        }
        if (!it->gold && !it->troops && !it->food) it = g_credit.erase(it); else ++it;
    }
}
bool marketOpen(const Caravan& k, int c) {
    int f=baseForce(c);
    if(f<0 || stance(k.force,f)<=0)return false;
    return k.shipType!=MERCHANT_SHIP || k.port!=c || k.arrivalBase<0 || shipAccessAt(c,k.arrivalBase).base>=0;
}
int64_t salesTax(int payer, int receiver, int64_t revenue, int pct, int merchantPct) {
    auto it = std::find_if(g_saleTax.begin(), g_saleTax.end(), [&](const TaxRemainder& r) { return r.payer == payer && r.receiver == receiver; });
    int remainder = it == g_saleTax.end() ? 0 : it->remainder;
    int64_t due = taxWithCarry(revenue, pct, merchantPct, &remainder);
    if (it != g_saleTax.end()) { if (remainder) it->remainder = remainder; else g_saleTax.erase(it); }
    else if (remainder) g_saleTax.push_back({payer, receiver, remainder});
    return due;
}
std::string tradeCargo(Caravan& k, int good, int qty, bool buying, std::vector<int>* deferredExperience=nullptr) {
    if (k.state != DOCKED || !marketOpen(k, k.port)) return "caravan must be docked at an accessible market";
    if (qty <= 0 || good < 0 || good >= g_data.G) return "invalid cargo quantity or good";
    uint8_t* home = payee(k.force, k.origin);
    int pp, cp; MerchantBonus mb; leaderOrTemp(k, &pp, &cp, &mb);
    int c = k.port;
    if (buying) {
        if (!liveUnit(k)) return "unit entered a city; sell remaining cargo and finish this voyage";
        if (!home) return "no funding city";
        if (k.cargo.loadUnits(g_data) + (int64_t)qty*g_data.loadUnits(good) > (int64_t)k.maxLoad*10) return "cargo exceeds the prepared capacity";
        int64_t cost = purchase(g_mk, c, good, qty, pp);
        if (cost < 0) return "market has insufficient stock";
        if (!k.automatic.caravan && cost > baseGold(home)) return "funding city has insufficient gold";
        if(!autoPay(k,cost)) return "payment failed";
        g_mk.buy(c, good, qty, pp); k.cargo.add(good, c, qty, cost);
        k.ledger.purchases += cost;
    } else {
        if (k.cargo.count(good) < qty) return "not enough cargo";
        int64_t revenue = saleQuote(g_mk, c, good, qty, pp);
        if (revenue < 0) return "market warehouse is full";
        int rem=0;for(auto& r:g_saleTax)if(r.payer==k.force && r.receiver==baseForce(c))rem=r.remainder;
        int64_t quotedDuty=stance(k.force,baseForce(c))==2 ? 0 : taxWithCarry(revenue,taxPct(g_data,c,g_cfg.tax,g_cfg.taxHub),mb.taxPct,&rem);
        if(k.automatic.caravan && !autoPay(k,quotedDuty)) return "insufficient automatic tax reserve";
        int64_t duty = stance(k.force, baseForce(c)) == 2 ? 0 : salesTax(k.force, baseForce(c), revenue, taxPct(g_data, c, g_cfg.tax, g_cfg.taxHub), mb.taxPct);
        int64_t debt = std::min<int64_t>(k.taxDue, revenue - duty);
        int merchant=baseForce(k.origin)==k.force?cityMerchantOfficer(k.force,k.origin):-1;
        MerchantTelemetry saleSource(merchant,k.force,k.origin);int marketForce=baseForce(c);
        int64_t bonus=k.cargo.saleBonus(good,qty,c,revenue-duty-debt,merchant>=0);
        int64_t net = revenue + bonus - (k.automatic.caravan ? 0 : duty) - debt, basis; int xp;
        k.cargo.take(good, qty, c, g_mk.p.merchExpTrip, &basis, &xp);
        g_mk.sell(c, good, qty, pp); k.taxDue -= (int)debt;
        k.ledger.receipts += revenue+bonus; k.ledger.taxes += duty + debt;
        credit(k.force, k.origin, net);
        merchantSaleCommitted(saleSource,k,c,marketForce,good,qty,basis,duty,debt,bonus,net);
        if (duty) credit(baseForce(c), c, duty);
        Caravan record = k; record.dest = c; record.good = good; record.qty = qty; record.cost = (int32_t)basis;
        recordDone(record, revenue+bonus-duty-debt, 0); // Includes bonus, after tax even when escrow paid the tax.
        if (!(k.flags & F_TEMP) && xp > 0) {
            int gain=xp+(int)(std::max<int64_t>(0,net-basis)/g_mk.p.merchExpDiv);
            if(deferredExperience)deferredExperience->push_back(gain);else merchGain(k.leader,gain);
        }
    }
    syncCargo(k); g_info.see(k.force, c, &g_mk);
    return "";
}
struct BatchSaleLine {
    OrderLine order;int64_t revenue=0,duty=0,debt=0,bonus=0,basis=0;
    int64_t net() const {return revenue+bonus-duty-debt;}
};
struct BatchSale {std::vector<BatchSaleLine> lines;int64_t net=0;std::string error;};
BatchSale quoteBatchSale(const Caravan& k,const std::vector<OrderLine>& order) {
    BatchSale out;
    if(k.state!=DOCKED || !marketOpen(k,k.port)) {out.error="market not accessible";return out;}
    if(order.empty()) {out.error="not enough cargo";return out;}
    int pp,cp;MerchantBonus mb;leaderOrTemp(k,&pp,&cp,&mb);
    int receiver=baseForce(k.port),rem=0;int64_t debt=k.taxDue;std::set<int> goods;Cargo cargo=k.cargo;
    for(auto& r:g_saleTax)if(r.payer==k.force && r.receiver==receiver)rem=r.remainder;
    bool merchant=baseForce(k.origin)==k.force && cityMerchantSkill(k.force,k.origin);
    for(auto l:order) {
        if(l.good<0 || l.good>=g_data.G || l.qty<=0 || !goods.insert(l.good).second || cargo.count(l.good)<l.qty) {
            out.error="not enough cargo or duplicate good";return out;
        }
        BatchSaleLine q;q.order=l;q.revenue=saleQuote(g_mk,k.port,l.good,l.qty,pp);
        if(q.revenue<0) {out.error="market warehouse is full";return out;}
        q.duty=stance(k.force,receiver)==2 ? 0 : taxWithCarry(q.revenue,taxPct(g_data,k.port,g_cfg.tax,g_cfg.taxHub),mb.taxPct,&rem);
        q.debt=std::min<int64_t>(debt,q.revenue-q.duty);debt-=q.debt;
        q.bonus=cargo.saleBonus(l.good,l.qty,k.port,q.revenue-q.duty-q.debt,merchant);
        int xp;cargo.take(l.good,l.qty,k.port,0,&q.basis,&xp);
        out.net+=q.net();out.lines.push_back(q);
    }
    return out;
}
std::string sellCargoBatch(Caravan& k,const std::vector<OrderLine>& order,int64_t expectedNet) {
    if(k.automatic.caravan)return "automatic voyage: take manual control first";
    auto quote=quoteBatchSale(k,order);if(!quote.error.empty())return quote.error;
    if(quote.net!=expectedNet)return "batch quote changed";
    // The game-thread lock covers preflight and every sale; unique goods cannot consume one another's stock.
    std::vector<int> experience;
    for(auto l:order) {auto error=tradeCargo(k,l.good,l.qty,false,&experience);if(!error.empty())return error;}
    for(int gain:experience)merchGain(k.leader,gain);
    return "";
}
void dock(Caravan& k, int city) {
    k.transferStage=0;k.transferUntil=0;
    k.port = city; k.state = marketOpen(k, city) ? DOCKED : WAITING;
    k.reason = k.state == DOCKED ? "" : "market closed by diplomacy";
    rebuildDirs();
    g_info.see(k.force, city, &g_mk);
    uint8_t* u = liveUnit(k);
    if (u) { const int16_t* pos = posOf(u); setOrder(EXEC, u, 1, pack(pos[0], pos[1]), -1); }
}
void arriveAtMarket(Caravan& k,int day) {
    if(k.shipType==MERCHANT_SHIP && k.arrivalBase>=0) {
        auto a=shipAccessAt(k.dest,k.arrivalBase);
        if(a.base<0) {k.state=WAITING;k.reason="port transfer unavailable";rebuildDirs();return;}
        if(a.days()) {
            k.transferStage=2;k.transferUntil=day+a.days();k.reason="port-to-city freight transfer";
            rebuildDirs();return;
        }
    }
    dock(k,k.dest);
}
// Return true while the unit must wait. Rechecking ownership also covers a lost
// port after save/load; cargo stays aboard and cannot be sold before completion.
bool waitForTransfer(Caravan& k,int day) {
    if(!k.transferStage)return false;
    int city=k.transferStage==1 ? k.legOrigin : k.dest;
    int anchor=k.transferStage==1 ? k.embarkBase : k.arrivalBase;
    auto a=shipAccessAt(city,anchor);
    if(a.base<0 || !marketOpen(k,city) || (k.transferStage==1 && city==k.origin && baseForce(city)!=k.force)) {
        k.state=WAITING;k.reason="port transfer unavailable";rebuildDirs();return true;
    }
    k.state=TRAVELLING;
    if(day<k.transferUntil) {
        k.reason=k.transferStage==1 ? "city-to-port freight transfer" : "port-to-city freight transfer";
        rebuildDirs();return true;
    }
    bool arrival=k.transferStage==2;
    k.transferStage=0;k.transferUntil=0;k.reason.clear();k.lastProgress=day;rebuildDirs();
    if(arrival) {dock(k,k.dest);return true;}
    return false;
}
void loseCargo(Caravan& k, int percent) {
    int64_t before = k.cargo.cost();
    auto lots = k.cargo.lots;
    for (auto& l : lots) {
        int n = std::min(l.qty, (l.qty * percent + 99) / 100); int64_t basis; int xp;
        if (n) k.cargo.take(l.good, n, -1, 0, &basis, &xp);
    }
    syncCargo(k);
    k.ledger.losses += before - k.cargo.cost();
}
int64_t cargoValue(const Caravan& k, int city) {
    int64_t value = 0;
    for (auto& l : k.cargo.lots) value += (int64_t)l.qty * g_mk.sellPrice(city, l.good);
    return value;
}
std::string finishCaravan(Caravan& k, bool discard) {
    if (k.state != DOCKED) return "finish requires an accessible market";
    if (k.qty && !discard) return "sell cargo first, or confirm discard";
    if (k.taxDue) {
        uint8_t* home = payee(k.force, k.origin);
        if (!home || baseGold(home) < k.taxDue) return "legacy transit debt is unpaid";
        int paid = -addGold(PROC, home, -k.taxDue);
        k.taxDue -= paid; k.ledger.taxes += paid;
        if (k.taxDue) return "legacy transit debt is unpaid";
    }
    uint8_t* u = liveUnit(k);
    if (u) { refundUnit(k, u); removeUnit(PROC, u, 0); }
    if (k.flags & F_TEMP) { releaseTempIfOurs(k); k.tempReleased=true; }
    closeLedger(k);
    rebuildDirs();
    return "";
}
// the unit is gone without being destroyed: did it walk into a city of its force? (the leader is in that city)
int enteredCity(const Caravan& k) {
    if(k.tempReleased) return k.port>=0 && baseForce(k.port)==k.force ? k.port : -1;
    if (k.flags & F_LOOTED) return -1;
    uint8_t* p = personRec(WORLD, k.leader);
    if (!p || !objValid(p) || vcall(p, 0x40) != k.force) return -1;
    int loc = *(int*)(p + 0x9c);
    if (loc < 0 || loc >= g_data.C || baseForce(loc) != k.force) return -1;
    return loc;
}
bool autoMarch(const Caravan& k) { return (k.flags & F_PLAYER) && (g_cfg.playerAuto || (k.automatic.caravan && k.automatic.managed)) && !(k.flags & F_MANUAL); }
typedef int (__thiscall* UnitFlagFn)(void* unit);
const UnitFlagFn unitActed = (UnitFlagFn)0x4952c0;          // unit +0x40 bit 0: 行動済 (0x5ad980 checks it too)

// one caravan at the end of a turn; false = it is over (remove from the list)
bool caravanTurn(Caravan& k, int day) {
    if (k.state == FINISHED) return false;
    uint8_t* u = liveUnit(k);
    if (!u) {
        // Only a confirmed same-force officer at a city may keep the cargo after entering it.
        int c = enteredCity(k);
        if (c >= 0) {
            dock(k, c);
            if(k.flags & F_TEMP) {releaseTempIfOurs(k);k.tempReleased=true;rebuildDirs();}
            return true;
        }
        recordDone(k, 0, k.flags & F_LOOTED ? tradeui::DONE_INTERCEPTED : tradeui::DONE_LOST);
        if (k.flags & F_TEMP) releaseTempIfOurs(k);
        closeLedger(k);
        return false;
    }
    if (k.flags & F_LOOTED) { closeLedger(k); return false; }
    if(waitForTransfer(k,day))return true;
    if (k.port >= 0 && (k.state == DOCKED || k.state == WAITING)) {
        bool open = marketOpen(k, k.port);
        k.state = open ? DOCKED : WAITING;
        k.reason = open ? "" : "market closed by diplomacy";
        rebuildDirs(); return true;
    }
    if (k.state != TRAVELLING) return true;
    if(k.shipType==MERCHANT_SHIP && k.arrivalBase>=0 && shipAccessAt(k.dest,k.arrivalBase).base<0) {
        k.state=WAITING;k.reason="port transfer unavailable";rebuildDirs();return true;
    }
    const int16_t* up = posOf(u); int lo = up[0], hi = up[1];
    uint32_t pos = pack(lo, hi);
    if (pos != k.lastPos) { k.lastPos = pos; k.lastProgress = day; }
    if (k.dockHex == 0xffffffff && cacheRoute(k, u) == 0xffffffff) return true;
    if (pos == k.dockHex) { arriveAtMarket(k,day); return true; }
    if (day - k.lastProgress >= 90) { k.state = WAITING; k.reason = "no progress for nine turns"; rebuildDirs(); return true; }
    MerchantBonus mb; int pp, cp; leaderOrTemp(k, &pp, &cp, &mb);
    uint8_t* h = cell(lo, hi);
    int sea = h && (*(uint32_t*)(h + 4) & 0x1f) == 8 ? g_data.seaOf(lo, hi) : -1;
    if (g_mk.p.monsoon && sea >= 0 && season(g_data, sea, g_month0) == 'T' && g_rng.pct() < g_mk.p.stormRisk)
        loseCargo(k, g_mk.p.stormLoss);
    int c = nearestCity(lo, hi, 30);
    if (c >= 0) g_info.see(k.force, c, &g_mk);
    if (c >= 0 && c != k.origin && c != k.dest && std::find(k.taxed.begin(), k.taxed.end(), c) == k.taxed.end()) {
        int cf = baseForce(c), st = stance(k.force, cf);
        if (cf >= 0 && st == 1) {
            int64_t tax = transitCharge(cargoValue(k,c),g_data.hub(c) ? g_mk.p.transitHub : g_mk.p.transitTax,mb.taxPct,k.transitLimit,k.transitPaid);
            uint8_t* home = payee(k.force, k.origin);
            if (!k.automatic.caravan && (!home || tax > baseGold(home))) { k.state = WAITING; k.reason = "insufficient transit funds"; rebuildDirs(); return true; }
            if (tax) {
                if (!autoPay(k,tax)) { k.state = WAITING; k.reason = "transit payment failed"; rebuildDirs(); return true; }
                credit(cf, c, tax); k.ledger.taxes += tax; k.transitPaid += tax;
            }
        } else if (cf >= 0 && st == 0 && g_rng.pct() * 100 < g_cfg.rob * mb.robPct) {
            int64_t before = cargoValue(k, c);
            loseCargo(k, g_cfg.robLoss * mb.robPct / 100);
            credit(cf, c, (before - cargoValue(k, c)) * g_cfg.loot / 100);
        }
        k.taxed.push_back(c);
    }
    // Manual orders are never overwritten.
    if (autoMarch(k) && !unitActed(u) && !*(int*)(u + 0x24)) {
        std::vector<uint32_t> tried;
        for (int attempt = 0; attempt < 3 && liveUnit(k) && !unitActed(u); attempt++) {
            uint32_t target = issueOrder(k, u, &tried);
            if (target == 0xffffffff) break;
            tried.push_back(target);
            if (execOrder(EXEC, u)) break;
        }
        u = liveUnit(k);
        if (u && !(k.flags & F_LOOTED)) {
            const int16_t* np = posOf(u);
            if (pack(np[0], np[1]) == k.dockHex) arriveAtMarket(k,day);
        }
    }
    return true;
}

// phase 4: the AI's merchant for a caravan from city c: an ordinary officer (status 3) of force f in c, not the only
// ones (the city keeps at least trade_ai_officer_min - 1), with merchant level >= 1; the best level, then politics.
int aiMerchant(int f, int c) {
    int best = -1, bestScore = 0, inCity = 0;
    for (int i = 0; i < g_cfg.PT; i++) {
        uint8_t* p = personRec(WORLD, i);
        if (!p || !objValid(p) || *(int*)(p + 0x9c) != c || vcall(p, 0x40) != f) continue;
        inCity++;
        if (*(int*)(p + 0xa0) != 3) continue;                // 一般 officers only (not the ruler / governors)
        if (tempLeader(i)) continue;
        bool busy = false; for (auto& k : g_car) busy |= k.state != FINISHED && !k.tempReleased && k.leader == i;
        if (busy) continue;
        int lv = merchantEffective(g_mk.p, merchExpOf(i), talentOf(i, p));
        if (lv < 1) continue;
        int score = lv * 1000 + ability(p, 3);
        if (score > bestScore) { bestScore = score; best = i; }
    }
    return inCity >= g_cfg.aiOfficerMin ? best : -1;
}

// One AI month evaluates many origins against the same ports; ownership is stable within it.
std::vector<int8_t> g_aiShipCity;
bool aiShipCity(int c) {
    if(g_aiShipCity.size()!=(size_t)g_data.C)g_aiShipCity.assign(g_data.C,-1);
    if(g_aiShipCity[c]<0)g_aiShipCity[c]=shipAccess(c).base>=0;
    return g_aiShipCity[c]!=0;
}
// Origin/destination pairs whose native route failed at departure. Geography rarely changes, so the AI
// stops proposing them; cleared yearly because a berth may only have been occupied.
std::set<uint32_t> g_aiDeadRoute;
int g_aiDeadYear=-1;
uint32_t aiRouteKey(int type,int origin,int dest) {return (uint32_t)type<<30 | (uint32_t)origin<<15 | (uint32_t)dest;}
bool findRoute(int force, int city, int leader, int64_t budget, Route* route, int departureFee = -1, const Caravan* existing = nullptr, int shipType = CARAVAN) {
    uint8_t* unit=existing?liveUnit(*existing):nullptr;
    if (existing && !unit) return false;
    if(existing)shipType=existing->shipType;
    bool ship=shipType==MERCHANT_SHIP;
    ShipAccess from;
    if(ship) {
        if(!movementReady() || !pathfindReady())return false;
        from=existing && existing->arrivalBase>=0 ? shipAccessAt(city,existing->arrivalBase) : shipAccess(city);
        if(from.base<0)return false;
    }
    // A caravan at this market (or its own funding city) can observe today's stock and prices.
    if (g_mk.p.info) g_info.see(force, city, &g_mk);
    Market view; std::vector<char> known;
    if (g_mk.p.info) g_info.view(g_mk, force, &view, &known); else view = g_mk;
    std::vector<int> dests, duties;
    for (int t : ship ? g_shipRange[city] : g_inRange[city]) {
        int f = baseForce(t);
        if (f < 0 || stance(force, f) == 0) continue;
        if (ship && !aiShipCity(t)) continue;
        if (!existing && g_aiDeadRoute.count(aiRouteKey(shipType,city,t))) continue;
        dests.push_back(t);
        duties.push_back(stance(force, f) == 2 ? 0 : taxPct(g_data, t, g_cfg.tax, g_cfg.taxHub));
    }
    int pp, cp; MerchantBonus mb;
    if (leader >= 0) leaderBonus(leader, &pp, &cp, &mb); else leaderFactors(TEMP_POL, TEMP_INT, &pp, &cp);
    int cap = existing ? existing->maxLoad : capacity(g_data, g_mk.p, city, city, cp, shipType);
    if(departureFee<0)departureFee=cityTradeFee(force,city,shipType)+from.fee();
    int funding=existing ? existing->origin : city;
    bool merchant=baseForce(funding)==force && cityMerchantSkill(force,funding);
    while (bestRoute(view, city, dests, budget, cap, g_cfg.tax, g_cfg.aiMargin, route, nullptr, pp, &duties, mb.taxPct, departureFee, merchant)) {
        if (!existing) return true; // A newly formed caravan is checked once its native unit exists.
        std::vector<uint32_t> path; int cost;
        if (marketRoute(unit, route->dest, &path, &cost)) return true;
        auto it=std::find(dests.begin(),dests.end(),route->dest);
        duties.erase(duties.begin()+(it-dests.begin())); dests.erase(it);
    }
    return false;
}
std::string depart(Caravan& k, int dest) {
    if(k.transferStage)return "port transfer in progress";
    if (dest < 0 || dest >= g_data.C || !marketOpen(k, dest)) return "destination market is unavailable";
    if (k.state != DOCKED && k.state != WAITING) return "caravan is already travelling";
    uint8_t* u = liveUnit(k);
    if (!u) return "unit entered a city; finish this voyage before creating another";
    if (k.state == DOCKED && dest == k.port) return "already at this market";
    ShipAccess from,to;
    bool resume = k.state == WAITING && k.port < 0 && dest == k.dest;
    if(k.shipType==MERCHANT_SHIP) {
        to=resume && k.arrivalBase>=0 ? shipAccessAt(dest,k.arrivalBase) : shipAccess(dest);
        if(k.port>=0)from=k.arrivalBase<0 ? shipAccess(k.port) : shipAccessAt(k.port,k.arrivalBase);
        if(to.base<0 || (k.port>=0 && from.base<0))return "port transfer unavailable";
    }
    std::vector<uint32_t> route; int routeCost;
    if (!marketRoute(u, dest, &route, &routeCost,nullptr,to.base)) return "no reachable market berth";
    auto supplyError=supplyForRoute(k,u,route,dest,(from.days()+to.days()+9)/10);
    if(!supplyError.empty())return supplyError;
    uint8_t* home = payee(k.force, k.origin);
    if (!resume) {
        int feeMerchant=-1,chargedFee=cityTradeFee(k.force,k.origin,k.shipType,true,&feeMerchant);
        MerchantTelemetry feeSource(feeMerchant,k.force,k.origin);
        int ordinaryFee=transportFee(g_mk.p,k.shipType,true),paid=chargedFee+from.fee()+to.fee();
        if (!autoPay(k,paid)) return "insufficient departure fee";
        k.ledger.fees += paid; k.taxed.clear();
        merchantFeeCommitted(feeSource,k,dest,ordinaryFee,chargedFee,paid,true);
        k.transitLimit=k.cargo.cost()*g_mk.p.transitCap/100;k.transitPaid=0;
    }
    if(k.port>=0)k.legOrigin=k.port;
    if(k.shipType==MERCHANT_SHIP) {
        if(from.base>=0)k.embarkBase=from.base;
        k.arrivalBase=to.base;
        if(from.days()) {k.transferStage=1;k.transferUntil=today()+from.days();}
    }
    k.dest = dest; k.port = -1; k.state = TRAVELLING; k.reason=k.transferStage ? "city-to-port freight transfer" : ""; k.lastProgress = today();
    rebuildDirs();
    k.route = std::move(route); k.routeCost = routeCost; k.dockHex = k.route.back();
    setOrder(EXEC, u, 1, k.dockHex, -1);
    return "";
}
void aiMonth() {
    // Docked AI follows exactly the player's market operations, including partial fills.
    for (auto& k : g_car) {
        if (!(k.flags & F_PLAYER) && k.state == WAITING && k.port < 0) { depart(k, k.dest); continue; }
        if (k.flags & F_PLAYER || k.state != DOCKED) continue;
        auto lots = k.cargo.lots;
        for (auto& lot : lots) {
            int q = std::min(k.cargo.count(lot.good), g_mk.room(k.port, lot.good));
            if (q > 0) tradeCargo(k, lot.good, q, false);
        }
        if (k.qty) continue;
        uint8_t* home = payee(k.force, k.origin); Route r;
        int budget = home ? std::min(g_cfg.aiInvest, baseGold(home) - g_cfg.aiReserve) : 0;
        int legFee=cityTradeFee(k.force,k.origin,k.shipType,true);
        if (budget > legFee && findRoute(k.force, k.port, k.flags & F_TEMP ? -1 : k.leader, budget, &r, legFee, &k)) {
            if (tradeCargo(k, r.good, r.qty, true).empty()) depart(k, r.dest);
        } else finishCaravan(k, false);
    }
    int made = 0, ships = 0;
    g_aiShipCity.clear();
    if(today()/360!=g_aiDeadYear) {g_aiDeadYear=today()/360;g_aiDeadRoute.clear();}
    for (int n = 0; n < g_s.R && made < g_cfg.aiMonth; n++) {
        int f = (g_nextAiForce + n) % g_s.R;
        uint8_t* fo = forceRec(WORLD, f);
        if (!fo || !objValid(fo) || isPlayerForce(f)) continue;
        // [0] ordinary caravan, [1] merchant ship: the better one leaves, the other is the fallback.
        Route best[2]; int bestLeader[2] = { -1, -1 }; int64_t score[2] = { -1, -1 };
        for (int c = 0; c < g_data.C; c++) {
            if (baseForce(c) != f) continue;
            if(cityAutoCount(f,c,true)>=cityAutoLimit(f,c,true))continue;
            uint8_t* b = base(c);
            if (!b || baseGold(b) < g_cfg.aiMinGold || baseFood(b) < g_cfg.food || vcall(b, 0x50) < g_cfg.escort) continue;
            int leader = g_cfg.aiOfficers ? aiMerchant(f, c) : -1;
            if (leader < 0 && (tempLive() >= g_cfg.tempMax || freeSpecials(WORLD) - 1 < g_cfg.tempKeep)) continue;
            for (int type = CARAVAN; type <= MERCHANT_SHIP; type++) {
                if (type == MERCHANT_SHIP && (!g_cfg.aiShips || !aiShipCity(c))) continue;
                // A ship has three times the hold: allow it the capital to fill it.
                int invest = std::min(g_cfg.aiInvest * (type == MERCHANT_SHIP ? 2 : 1), baseGold(b) - g_cfg.aiReserve);
                Route r;
                if (!findRoute(f, c, leader, invest, &r, -1, nullptr, type)) continue;
                int64_t value = r.profit * 1000 / (r.dist + 20);
                if (value > score[type]) { score[type] = value; best[type] = r; bestLeader[type] = leader; }
            }
        }
        int first = score[MERCHANT_SHIP] >= score[CARAVAN] ? MERCHANT_SHIP : CARAVAN;
        for (int pass = 0; pass < 2; pass++) {
            int type = pass ? 1 - first : first;
            if (score[type] < 0) continue;
            std::string err; Route r = best[type];
            // An unreachable berth is remembered and the same city's next best market is tried at once.
            for (int attempt = 0; attempt < 4; attempt++) {
                err = createCaravanOrder(r.origin, r.dest, {{r.good, r.qty}}, bestLeader[type], true, nullptr, nullptr, type);
                if (err.empty() || err.find("reachable market berth") == std::string::npos) break;
                g_aiDeadRoute.insert(aiRouteKey(type, r.origin, r.dest));
                uint8_t* b = base(r.origin);
                int invest = b ? std::min(g_cfg.aiInvest * (type == MERCHANT_SHIP ? 2 : 1), baseGold(b) - g_cfg.aiReserve) : 0;
                if (!b || !findRoute(f, r.origin, bestLeader[type], invest, &r, -1, nullptr, type)) break;
            }
            if (err.empty()) { made++; ships += type == MERCHANT_SHIP; break; }
            g_log("trade: AI %s departure failed: %s", type == MERCHANT_SHIP ? "ship" : "caravan", err.c_str());
        }
    }
    g_nextAiForce = (g_nextAiForce + 7) % g_s.R;
    if (made) g_log("trade: AI sent %d caravans this month (%d merchant ships)", made, ships);
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
            g_info.see(f, c, &g_mk);
            for (int t : g_infoNear[c]) g_info.see(f, t, &g_mk);
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
        if (c >= 0) g_info.see(f, c, &g_mk);
    }
}
#include "trade_procurement_runtime.inc"

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
    if (!mapReady() || g_readOnly) return;
    settleCredits();
    int day = today(), month = day / 30;
    g_month0 = month % 12;
    g_unit0 = unitRec(WORLD, 0);
    for (size_t i = 0; i < g_car.size();) {
        bool keep;
        keep = caravanTurn(g_car[i], day);
        if (keep) i++;
        else { Lock l; g_car.erase(g_car.begin() + i); }
    }
    bool first = g_lastMonth < 0;
    if (first) g_lastMonth = month;
    while (g_lastMonth < month) {                            // a new month (several after a long skip: catch up)
        g_lastMonth++;
        { Lock l; if (g_mk.p.info) g_info.endMonth(g_mk, g_lastMonth - 1); g_mk.month(g_lastMonth); }
        monthEvents(g_lastMonth);
        if (g_lastMonth == month) {
            observeTerritory(-1);
            if (g_cfg.ai) { aiMilitaryMonth(); aiMonth(); }
        }
    }
    if (first) observeTerritory(-1);
    observeUnits();
    autoTick();
    { Lock l; rebuildDirs(); }
}

// monsoon on sea hexes (movement.cpp's step cost, any thread: reads only g_unitDir / g_month0 / the map).
// Phase 4: by the direction of the step (east / west component of from -> to) for every ship (trade_monsoon_all) when
// movement.cpp knows where the step comes from (A*, range search, path cost sum); else a caravan's route heading
// (phase 3), and for other ships no direction (only the storm months count).
MovementStepStats g_dirStats = {};               // steps with a usable / unusable from-hex (harness check)
int __cdecl seaCostHook(const uint8_t* unit, const uint8_t* rec, uint32_t from, int cost) {
    if (!rec || rec < g_s.hex20) return cost;
    size_t h = (size_t)(rec - g_s.hex20) / 20;
    if (h >= (size_t)g_s.W * g_s.H) return cost;
    int tlo = (int)(h / g_s.W), thi = (int)(h % g_s.W);
    int sea = g_data.seaOf(tlo, thi);
    if (sea < 0) return cost;
    int carDir = 0;
    uint8_t* u0 = g_unit0;
    if (u0 && unit >= u0) { size_t ui = (size_t)(unit - u0) / 0xf4; if (ui < g_unitDir.size()) carDir = g_unitDir[ui]; }
    if (!carDir && !g_cfg.monsoonAll) return cost;
    int dir; bool known = false;
    if (from != 0xffffffffu) {
        int flo = (int16_t)(from & 0xffff), fhi = (int16_t)((from >> 16) & 0x7fff);
        if (hexDist(flo, fhi, tlo, thi) == 1) {
            dir = tlo > flo ? 1 : tlo < flo ? -1 : 0;
            if (from & 0x80000000u) dir = -dir;                 // a backwards search (0x567ed0): the unit moves to -> from
            known = true;
        }
    }
    if (known) g_dirStats.astar++; else g_dirStats.unknown++;
    if (!known) dir = carDir == 2 ? 0 : carDir;
    return seaStepCost(g_data, g_mk.p, sea, g_month0, dir, cost);
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
            if (k.state != FINISHED && !k.tempReleased && !(k.flags & F_LOOTED) && unitRec(WORLD, k.unit) == unit && *(int*)(unit + 0xc) == k.leader) { if (k.state == TRAVELLING && !k.transferStage && !(k.flags & F_MANUAL)) issueOrder(k, unit); return 1; }
    }
    return g_planOrig(ai, edx, unit);
}

// The game's own "captured gold / provisions" number over a unit or base, as its defeat routine shows it at
// 0x4bed14 / 0x4bed91: (amount, 3 gold | 4 food, recipient, packed hex). A hex outside the original 200x200
// map makes it use the recipient's own position.
typedef void (__thiscall* SpoilPopupFn)(uint32_t, int amount, int kind, uint8_t* recipient, uint32_t hex);
const SpoilPopupFn spoilPopup = (SpoilPopupFn)0x589210;
const uint32_t SPOIL_UI = 0x96a59d8;
void showSpoils(int amount, int kind, uint8_t* recipient) {
#ifndef TRADE_UI_TEST
    __try { spoilPopup(SPOIL_UI, amount, kind, recipient, 0xffffffffu); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (g_log) g_log("trade: ERROR: exception %08lx in the spoils animation", GetExceptionCode()); }
#endif
}
typedef int (__fastcall* AnnihilateFn)(void* proc, void* edx, uint8_t* unit, uint8_t* attacker, int a3, int a4);
AnnihilateFn g_annOrig = nullptr;
int __fastcall annihilateHook(void* proc, void* edx, uint8_t* unit, uint8_t* attacker, int a3, int a4) {
    bool temp = false;
    if (unit && !g_car.empty()) {
        Lock l;
        for (auto& k : g_car) {
            if (k.state == FINISHED || k.tempReleased || unitRec(WORLD, k.unit) != unit || *(int*)(unit + 0xc) != k.leader || (k.flags & F_LOOTED)) continue;
            k.flags |= F_LOOTED;
            temp = (k.flags & F_TEMP) != 0;
            const int16_t* up = posOf(unit);
            int c = nearestCity(up[0], up[1], 1 << 20);
            // The victor takes a random share below 50% of the cargo's local value, as gold or as provisions.
            int share = g_cfg.spoilMin + (int)(g_rng.next() % (uint32_t)(g_cfg.spoilMax - g_cfg.spoilMin + 1));
            int64_t value = cargoValue(k, c) * share / 100;
            int winner = -1, home = c;
            if (attacker && objValid(attacker) && (isUnitPtr(attacker) || isBasePtr(attacker))) {
                winner = vcall(attacker, 0x40);
                if (isUnitPtr(attacker)) { int h = unitHome(attacker); if (h >= 0 && h < g_data.C) home = h; }
            }
            // Resolve the recipient by the attacker's force even if its home was captured.
            // Barbarian and bandit forces (ids >= R) have no city account but their unit still carries the spoils.
            bool awarded = winner >= 0 && winner < g_s.R + 5 && value > 0;
            bool asFood = false;
            if (awarded) {
                uint8_t* city = c >= 0 && c < g_data.C ? cityRec(WORLD, c) : nullptr;
                int rate = objValid(city) ? city[0x7c] : 0;
                asFood = rate > 0 && (g_rng.next() & 1);
                // Same native grain quote the automatic planner values provisions with.
                if (asFood) value = value * rate * 400 / ((450 - TEMP_POL) * 10);
                int64_t left = value;
                if (isUnitPtr(attacker)) {
                    int before = *(int*)(attacker + (asFood ? 0x20 : 0x1c));
                    (asFood ? unitFoodSet : unitGoldSet)(attacker, (int)std::min<int64_t>(INT_MAX, before + value));
                    left -= *(int*)(attacker + (asFood ? 0x20 : 0x1c)) - before;
                }
                // A base that shot it down, or what the unit could not carry, goes to the victor's city.
                if (left > 0 && winner < g_s.R) credit(winner, home, asFood ? 0 : left, 0, asFood ? left : 0);
                showSpoils((int)std::min<int64_t>(INT_MAX, value), asFood ? 4 : 3, attacker);
            }
            g_log("trade: caravan %s -> %s (%d x %s) destroyed; %d%% of the cargo: %lld %s to the victor (force %d)%s", cityName(k.origin),
                  cityName(k.dest), k.qty, k.cargo.lots.size()>1 ? "mixed cargo" : goodName(k.good), awarded ? share : 0, (long long)(awarded ? value : 0),
                  asFood ? "food" : "gold", winner, temp ? ", 商 leader released" : "");
            recordDone(k, 0, tradeui::DONE_INTERCEPTED);
            closeLedger(k);
            rebuildDirs();
            break;
        }
    }
    if (temp) { removeUnit(PROC, unit, 0); return 0; }       // never captured: vanish, the slot is recycled
    return g_annOrig(proc, edx, unit, attacker, a3, a4);
}

int findCityExt(const char* s);
int findGoodExt(const char* s);

// main-thread queue (automation "trade send / ai", the panel's 「出發」)
struct Job { std::string line, result; int actor = -1; HANDLE done = nullptr; ~Job() { if (done) CloseHandle(done); } };   // done == nullptr: queued by the panel (result -> g_uiResult)
std::wstring g_uiResult;                                   // the last panel send's result (shown by the panel)
std::wstring resultText(const std::string& r) {
    if (!r.compare(0, 2, "ok")) return L"成功：" + tradeui::wide(r.substr(3));
    return L"失敗：" + tradeui::wide(r);
}
std::vector<std::shared_ptr<Job>> g_jobs;
void runJob(Job* j);
typedef int (__fastcall* PumpFn)(void* self, void* edx, int arg);   // 0x57ff80: thiscall, 1 argument (ret 4)
PumpFn g_pumpOrig = nullptr;
bool runJobSafe(Job* j) {
    __try { runJob(j); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void cleanLandedMerchants() {
    if(!g_on || g_readOnly || g_car.empty() || !mapReady())return;
    Lock l;
    for(auto& k:g_car) if(k.state!=FINISHED && (k.flags & F_TEMP) && !k.tempReleased && !liveUnit(k)) {
        int c=enteredCity(k);
        if(c>=0) {dock(k,c);releaseTempIfOurs(k);k.tempReleased=true;rebuildDirs();}
    }
}
void cleanLandedSafe() {
    __try {cleanLandedMerchants();}
    __except(EXCEPTION_EXECUTE_HANDLER) {if(g_log)g_log("trade: ERROR: exception %08lx cleaning landed merchants",GetExceptionCode());}
}
void pumpJobs() {
    std::vector<std::shared_ptr<Job>> jobs;
    { Lock l; jobs.swap(g_jobs); }
    for (auto& owned : jobs) {
        Job* j = owned.get();
        if (!runJobSafe(j)) j->result = "exception";
        if (j->done) SetEvent(j->done);
        else { { Lock l; g_uiResult = resultText(j->result); } g_log("trade: panel send: %s", j->result.c_str()); }
    }
}
int __fastcall pumpHook(void* self, void* edx, int arg) {
    // A logic-thread auto dispatch can hold g_cs while sortie waits for this
    // thread's native command pump. Never block that pump on the trade lock.
    if (TryEnterCriticalSection(&g_cs)) {
        __try {
            bool pending=!g_jobs.empty();
            if (waitingForPlayer((uint32_t)(uintptr_t)self)) {
                cleanLandedSafe();
                if(pending) pumpJobs();
            }
        } __finally { LeaveCriticalSection(&g_cs); }
    }
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

// Standard native object rows (0x4dd0b0) and list-copy insertion (0x4e6a7f)
// converge here. Custom dialogs may also keep their own candidate collection.
typedef int (__thiscall* ListRowFn)(void*,int,uint32_t,uint32_t);
ListRowFn g_listRowOrig=nullptr;
int __fastcall listRowHook(void* list,void*,int index,uint32_t low,uint32_t high) {
    if((high==0 || high==0xffffffff) && hiddenTradePerson((const uint8_t*)(uintptr_t)low)) {
        InterlockedIncrement(&g_hiddenRows);
        return 0; // native insertion also returns 0 for a filtered/duplicate item
    }
    return g_listRowOrig(list,index,low,high);
}

// The rank dialog stores a private CList at +0x554, used by both its custom
// appointment grid and automatic assignment. Filter that source, not global
// simulation lists (which still need merchants for movement and save/load).
typedef int (__thiscall* RankPeopleFn)(void*,void*,void*,int);
typedef void (__thiscall* ListEraseFn)(void*,void*);
RankPeopleFn g_rankPeopleOrig=(RankPeopleFn)0x4cf480;
ListEraseFn g_rankListErase=(ListEraseFn)0x482fc0;
volatile LONG g_hiddenRankCandidates=0;
int __fastcall rankPeopleHook(void* owner,void*,void* list,void* force,int mask) {
    g_rankPeopleOrig(owner,list,force,mask);
    auto node=*(uint8_t**)((uint8_t*)list+4);
    while(node) {
        auto next=*(uint8_t**)node;
        auto person=*(uint8_t**)(node+8);
        if(hiddenTradePerson(person)) {
            g_rankListErase(list,node);
            InterlockedIncrement(&g_hiddenRankCandidates);
        }
        node=next;
    }
    return *(int*)((uint8_t*)list+0xc);
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
void buildModel(int force) {
    observeTerritory(force);                                 // what the player sees now
    tradeui::Model& m = g_model;
    std::vector<int> owner(g_data.C);
    for (int c = 0; c < g_data.C; c++) owner[c] = baseForce(c);
    Lock l;
    m.d = &g_data; m.shipType=CARAVAN; m.departureFee=-1; m.fundingCity=-1;
    m.merchantCities.assign(g_data.C,0);
    m.waterAccess.resize(g_data.C);
    m.portBase.resize(g_data.C);m.portSteps.resize(g_data.C);
    for(int c=0;c<g_data.C;++c) {
        auto a=shipAccess(c);m.waterAccess[c]=a.base>=0;m.portBase[c]=a.base;m.portSteps[c]=a.steps;
    }
    m.live = g_mk;
    if (g_mk.p.info) g_info.view(g_mk, force, &m.seen, &m.known);
    else { m.seen = g_mk; m.known.assign(g_data.C, 1); }
    m.age.resize(g_data.C);
    for (int c = 0; c < g_data.C; c++) m.age[c] = g_mk.p.info ? (uint8_t)g_info.ageOf(force, c) : 0;
    m.force = force;
    m.owner = owner;
    m.stance.assign(g_data.C, -1); m.gold.assign(g_data.C, -1);
    m.saleTaxRemainder.assign(g_data.C, 0);
    m.troops.assign(g_data.C, -1); m.food.assign(g_data.C, -1); m.escortNeeded = g_cfg.escort; m.foodNeeded = g_cfg.food;
    m.grainRate.assign(g_data.C,0);
    for (int c = 0; c < g_data.C; c++) {
        if (owner[c] < 0) continue;
        m.stance[c] = (int8_t)stance(force, owner[c]);
        for (auto& r : g_saleTax) if (r.payer == force && r.receiver == owner[c]) { m.saleTaxRemainder[c] = r.remainder; break; }
        if (owner[c] == force) {
            m.merchantCities[c]=cityMerchantSkill(force,c);
            uint8_t* b = base(c); m.gold[c] = b ? baseGold(b) : 0; m.troops[c] = b ? vcall(b, 0x50) : 0; m.food[c] = b ? baseFood(b) : 0;
            uint8_t* city=cityRec(WORLD,c);if(objValid(city))m.grainRate[c]=city[0x7c];
        }
    }
    m.officers.clear();
    if (g_ui.origin >= 0 && g_ui.origin < g_data.C && owner[g_ui.origin] == force)
        for (int i = 0; i < g_cfg.PT; i++) {
            uint8_t* p = personRec(WORLD, i);
            if (!p || !objValid(p) || vcall(p, 0x40) != force || *(int*)(p + 0x9c) != g_ui.origin) continue;
            if (tempLeader(i)) continue;
            tradeui::Officer o = { i, personName(p), (int)ability(p, 3), (int)ability(p, 2) };
            o.exp = merchExpOf(i); o.talent = talentOf(i, p); o.level = merchantEffective(g_mk.p, o.exp, o.talent);
            m.officers.push_back(o);
        }
    m.caravans.clear();
    for (auto& k : g_car) {
        uint8_t* u = liveUnit(k);
        if (k.state == FINISHED) continue;
        int16_t dockPos[2] = { g_data.lo[k.port >= 0 ? k.port : k.origin], g_data.hi[k.port >= 0 ? k.port : k.origin] };
        const int16_t* up = u ? posOf(u) : dockPos;
        bool player = k.force == force;
        if (!player) {
            if (!g_cfg.uiAi) continue;
            int c = nearestCity(up[0], up[1], 30);
            if (c < 0 || (g_mk.p.info && g_info.ageOf(force, c) != 0)) continue;   // AI caravans where the player can see
        }
        uint8_t* h = cell(up[0], up[1]);
        int terr = h ? *(uint32_t*)(h + 4) & 0x1f : 0;
        tradeui::CaravanView cv = { k.origin, k.dest, k.good, k.qty, k.cost, up[0], up[1], player, (k.flags & F_TEMP) != 0, terr == 7 || terr == 8 };
        cv.shipType=k.shipType;cv.transferStage=k.transferStage; cv.unit = k.unit; cv.autoMarch = !(k.flags & F_MANUAL);
        cv.ledger = k.ledger; cv.routeCost = k.routeCost; cv.routeSteps = k.route.empty() ? 0 : (int)k.route.size() - 1;
        if (player) {
            auto current = std::find(k.route.begin(), k.route.end(), pack(up[0], up[1]));
            if (current != k.route.end()) cv.route.assign(current, k.route.end());
        }
        if(u && !cv.route.empty()) {
            auto time=movementRouteTime(u,cv.route,g_s.hex20,g_s.W,g_s.H);
            cv.routeTurns=time.turns;cv.routeLand=time.land;cv.routeWater=time.water;
            cv.routeSteps=(int)cv.route.size()-1;
            if(time.turns>=0) cv.routeCost=time.cost;
        }
        if(k.shipType==MERCHANT_SHIP && cv.routeTurns>=0) {
            cv.routeTurns+=std::max(0,k.transferUntil-today()+9)/10;
            if(k.transferStage!=2)cv.routeTurns+=shipAccessAt(k.dest,k.arrivalBase).days()/10;
        }
        cv.id = k.id; cv.state = k.state; cv.port = k.port; cv.maxLoad = k.maxLoad; cv.cargo = k.cargo; cv.reason = tradeui::wide(k.reason);
        if(k.reason=="port transfer unavailable")cv.reason=L"港口失守或接駁陸路中斷，等待恢復";
        else if(k.reason=="no safe reachable market berth")cv.reason=L"敵軍或敵方據點阻斷安全商路，暫停等待";
        else if(k.reason.find("provision")!=std::string::npos)cv.reason=L"續航兵糧不足，請在本方市場補給後重試";
        else if(k.transferStage)cv.reason=std::wstring(k.transferStage==1?L"城市送貨至港口":L"港口交貨至城市")+L"，剩餘 "+std::to_wstring(std::max(0,k.transferUntil-today()+9)/10)+L" 旬";
        uint8_t* payer = payee(k.force, k.origin);
        cv.payer = payer ? ((IdOfFn)0x491770)(WORLD, payer) : -1;
        if (!(k.flags & F_TEMP)) { uint8_t* lp = personRec(WORLD, k.leader); if (lp && objValid(lp)) cv.leader = personName(lp); cv.level = leaderLevel(k.leader); }
        m.caravans.push_back(cv);
    }
    m.done.clear();
    m.closed.clear(); m.pendingGold = 0;
    for (auto it = g_closed.rbegin(); it != g_closed.rend(); ++it) if (it->force == force) m.closed.push_back(*it);
    for (auto& c : g_credit) if (c.force == force) m.pendingGold += c.gold;
    for (auto it = g_done.rbegin(); it != g_done.rend(); ++it)
        if (it->force == force) m.done.push_back({ it->day, it->origin, it->dest, it->good, it->qty, it->cost, it->net, it->flags });
    m.events = g_ev.live;
    m.month = today() / 30;
    m.tempLive = tempLive();
    m.freeSpecials = freeSpecials(WORLD);
    m.cfg.tax = g_cfg.tax; m.cfg.taxHub = g_cfg.taxHub; m.cfg.tempMax = g_cfg.tempMax; m.cfg.tempKeep = g_cfg.tempKeep;
    m.lastResult = g_uiResult;
    m.playerAuto = g_cfg.playerAuto != 0;
}
bool projectHex(int lo, int hi, float* x, float* y) { return midmapHexToPixel(lo, hi, 0.0f, x, y); }

// midmap.cpp's overlay: every compose of the world 中地圖 (open, zoom, pan, background mode, our repaints)
void overlayImpl(uint8_t* ui, uint32_t* dst, int pitch) {
    int f = panelForce(ui);
    if (f < 0) { midmapSetClip(600); return; }
    if (ui != g_uiLast) { g_uiLast = ui; g_uiOwnDown = false; g_ui.msg.clear(); }
    midmapSetClip(g_ui.on ? tradeui::PANEL_X : 600);
    buildModel(f);
    tradeui::validate(g_model, g_ui);
    tradeui::renderMap(g_model, g_ui, projectHex, dst, pitch);
    if (g_ui.on) tradeui::renderPanel(g_model, g_ui, dst + tradeui::PANEL_X, pitch);
}
void __cdecl overlayHook(void* ui, uint32_t* dst, int pitch) {
    __try { overlayImpl((uint8_t*)ui, dst, pitch); }
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
// the panel's 自動 / 手動 switch of a caravan (phase 4)
void applyToggle(const tradeui::Result& r) {
    if (r.toggleUnit < 0) return;
    Lock l;
    for (auto& k : g_car)
        if (k.state != FINISHED && !k.tempReleased && !k.automatic.caravan && k.unit == r.toggleUnit && (k.flags & F_PLAYER)) {
            if (r.toggleAuto) k.flags &= ~F_MANUAL; else k.flags |= F_MANUAL;
            g_log("trade: caravan %s -> %s: 自動行軍 %s", cityName(k.origin), cityName(k.dest), r.toggleAuto ? "on" : "off");
        }
}
void queueSend(const tradeui::SendRequest& r) {
    char line[128];
    if (r.leader >= 0) sprintf_s(line, "trade send %d %d %d %d %d", r.origin, r.dest, r.good, r.qty, r.leader);
    else sprintf_s(line, "trade send %d %d %d %d", r.origin, r.dest, r.good, r.qty);
    auto j = std::make_shared<Job>(); j->line = line; j->actor = g_model.force;
    { Lock l; g_uiResult.clear(); g_jobs.push_back(j); }
    g_log("trade: panel queued \"%s\"", line);
}
// 0: not ours; 1: handled
void queueAction(const std::string& command) {
    if (command.empty()) return;
    auto j = std::make_shared<Job>(); j->line = command; j->actor = g_model.force;
    Lock l; g_uiResult.clear(); g_jobs.push_back(j);
}
int downImpl(uint8_t* ui, POINT* pt) {
    int f = panelForce(ui);
    if (f < 0 || !pt) return 0;
    if (ui != g_uiLast) { g_uiLast = ui; g_ui.msg.clear(); }
    buildModel(f);
    tradeui::validate(g_model, g_ui);
    tradeui::Result r = tradeui::click(g_model, g_ui, projectHex, pt->x - MM_BORDER, pt->y - MM_BORDER);
    if (!r.consumed) return 0;
    if (r.send) queueSend(r.req);
    applyToggle(r);
    queueAction(r.command);
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
// keys (phase 4): the window system sends WM_KEYDOWN to the focused window's vt +0xb8 (target, vk, repeat, flags) and
// on to its parents while they return 0 (0x46f6b0 / 0x46fb9b). Koei's MapUI and the midmap dialog both have the
// default 0x4ed9d0 (handles nothing). Ours: the panel's keys when it is open (trade_panel.cpp key()), else nothing.
typedef int (__fastcall* UiKeyFn)(void* self, void* edx, void* target, uint32_t vk, uint32_t rep, uint32_t flags);
UiKeyFn g_keyMapPrev = nullptr, g_keyDlgPrev = nullptr;
int keyImpl(uint32_t vk) {
    uint8_t* ui = openMapUi();
    int f = panelForce(ui);
    if (f < 0 || !g_ui.on) return 0;
    buildModel(f);
    tradeui::validate(g_model, g_ui);
    tradeui::Result r = tradeui::key(g_model, g_ui, (int)vk, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
    if (!r.consumed) return 0;
    if (r.send) queueSend(r.req);
    applyToggle(r);
    queueAction(r.command);
    if (r.repaint & (tradeui::REPAINT_MAP | tradeui::REPAINT_LAYOUT)) { buildModel(f); tradeui::validate(g_model, g_ui); }
    else if (r.toggleUnit >= 0) { buildModel(f); tradeui::validate(g_model, g_ui); }
    repaint(ui, r.repaint);
    return 1;
}
int keySafe(uint32_t vk) {
    __try { return keyImpl(vk); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_log("trade: ERROR: exception %08lx in the panel key", GetExceptionCode()); return 0; }
}
int __fastcall keyMapHook(void* self, void* edx, void* target, uint32_t vk, uint32_t rep, uint32_t flags) {
    if (keySafe(vk)) return 1;
    return g_keyMapPrev(self, edx, target, vk, rep, flags);
}
int __fastcall keyDlgHook(void* self, void* edx, void* target, uint32_t vk, uint32_t rep, uint32_t flags) {
    if (keySafe(vk)) return 1;
    return g_keyDlgPrev(self, edx, target, vk, rep, flags);
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

#include "trade_auto_runtime.inc"
#include "trade_dialogs.inc"

// The native properties page runs on the UI thread. Never wait for a dispatch
// that may itself be waiting for this thread, and release the lock before a modal.
bool readUnitTradeDetails(const uint8_t* unit,TradeShipUiDetails& out,Table* table=nullptr) {
    if(!g_on || !tradeMovingUnit(unit) || !TryEnterCriticalSection(&g_cs))return false;
    struct Unlock { ~Unlock(){LeaveCriticalSection(&g_cs);} } unlock;
    int uid=(int)(((uintptr_t)unit-(uintptr_t)g_unit0)/0xf4);
    for(auto& k:g_car) if(k.unit==uid && k.leader==*(const int*)(unit+0xc) &&
        k.state!=FINISHED && !k.tempReleased && !(k.flags&F_LOOTED)) {
        std::wstring mode=k.shipType==MERCHANT_SHIP?L"海運":L"陸路";
        std::wstring vessel=k.shipType==MERCHANT_SHIP?L"大型海船（雙桅雙帆）":L"無（陸路運輸）";
        std::wstring load=nativeLoad(k.cargo.loadUnits(g_data))+L" / "+nativeNum(k.maxLoad)+L" 格";
        out.route=nativeCity(k.legOrigin>=0?k.legOrigin:k.origin)+L" → "+nativeCity(k.dest);
        out.load=load;
        out.summary=mode+(k.shipType==MERCHANT_SHIP?L" · 大型海船":L" · 陸路運輸")+L"　載量 "+load;
        out.cargo=L"載貨：";
        for(auto& lot:k.cargo.lots)if(lot.qty>0) {
            if(out.cargo!=L"載貨：")out.cargo+=L"、";
            out.cargo+=nativeGood(lot.good)+L" "+nativeNum(lot.qty)+L"批";
        }
        if(out.cargo==L"載貨：")out.cargo+=L"空載";
        if(table) {
            std::wstring state=k.transferStage?L"港口接駁中":k.state==DOCKED?L"停靠市場":k.state==WAITING?L"等待安排路線":L"行進中";
            *table={L"商隊詳情",L"貨物以批計數；可捲動查看全部商品，按返回回到部隊資料。",{L"項目／商品",L"內容／產地",L"數量（批）",L"成本（金）"},{
                {0,{L"商隊編號",L"#"+nativeNum(k.id)}},
                {1,{L"運輸方式",mode}},{2,{L"船型",vessel}},
                {3,{L"本程商路",nativeCity(k.legOrigin>=0?k.legOrigin:k.origin)+L" → "+nativeCity(k.dest)}},
                {4,{L"狀態",state+(k.port>=0?L" · "+nativeCity(k.port):L"")}},
                {5,{L"運作方式",k.automatic.caravan?L"自動貿易":k.flags&F_MANUAL?L"手動行軍":L"自動行軍"}},
                {6,{L"貨艙佔用",load,nativeNum(k.cargo.count()),nativeNum(k.cargo.cost())}}
            }};
            int row=7;
            for(auto& lot:k.cargo.lots)if(lot.qty>0)table->rows.push_back({row++,{
                nativeGood(lot.good),nativeCity(lot.origin),nativeNum(lot.qty),nativeNum(lot.cost)},true,nativeGood(lot.good)});
            if(row==7)table->rows.push_back({7,{L"貨物",L"空載"}});
        }
        return true;
    }
    return false;
}
void openUnitTradeDetails(const uint8_t* unit) {
    TradeShipUiDetails out;Table table;
    if(!readUnitTradeDetails(unit,out,&table))return;
    tradenative::show(table,false);
}

// Native city command menu (vt 0x851d00). AddItem uses Koei's own button skin,
// sizing, hit testing and hover states; no screen coordinates or replacement commands.
const int CITY_TRADE = 0x7000;
struct CityTradeEntry { const wchar_t* label; const wchar_t* help; int tab; bool accounts; };
const CityTradeEntry CITY_TRADE_ENTRIES[] = {
    { L"市場行情", L"查看本城商品、價格與各地行情。", 0, false },
    { L"組建商隊", L"以本城為起點，選擇貨物、統領與目的地。", 1, false },
    { L"商隊管理", L"查看商隊位置、貨物及行軍狀態。", 2, false },
    { L"收支帳本", L"查看商隊收支與已結算航次。", 2, true },
    { L"貿易情報", L"查看季風、事件與行情情報。", 3, false },
    { L"自動貿易", L"設定本城的每月預算、優先目標與商品。", 0, false },
    { L"待處理", L"查看需要決定的自動商隊。", 0, false },
    { L"軍需採購", L"以本城資金採購市場兵裝與軍馬；查看採購明細。", 0, false }
};
const int CITY_TRADE_COUNT = sizeof CITY_TRADE_ENTRIES / sizeof CITY_TRADE_ENTRIES[0];
const CityTradeEntry* cityTradeEntry(int id) {
    return id > CITY_TRADE && id <= CITY_TRADE + CITY_TRADE_COUNT ? &CITY_TRADE_ENTRIES[id - CITY_TRADE - 1] : nullptr;
}
void prepareCityTrade(int city, const CityTradeEntry& entry) {
    g_ui = tradeui::State();
    g_ui.on = true; g_ui.origin = city; g_ui.tab = entry.tab; g_ui.accounts = entry.accounts;
}
typedef int (__fastcall* CityMenuBuildFn)(uint8_t*, void*);
typedef void (__fastcall* CityMenuClickFn)(uint8_t*, void*, void*);
typedef int (__fastcall* CityMenuRangeFn)(uint8_t*, void*, int, int*, int*);
typedef const char* (__fastcall* CityMenuTextFn)(uint8_t*, void*, int);
typedef int (__fastcall* CityMenuValueFn)(uint8_t*, void*, int);
CityMenuBuildFn g_cityBuildPrev = nullptr;
CityMenuClickFn g_cityClickPrev = nullptr;
CityMenuRangeFn g_cityRangePrev = nullptr;
CityMenuTextFn g_cityTextPrev = nullptr, g_cityHelpPrev = nullptr;
CityMenuValueFn g_cityEnabledPrev = nullptr, g_cityParentPrev = nullptr;
int g_cityOpening = -1;
int tradeCity(uint8_t* menu) {
    if (!g_on || !mapReady() || !menu) return -1;
    uint8_t* b = *(uint8_t**)(menu + 0x354);
    if (!b || !objValid(b)) return -1;
    int c = ((IdOfFn)0x491770)(WORLD, b);
    if (c < 0 || c >= g_data.C || !isPlayerForce(baseForce(c))) return -1;
    return c;
}
int __fastcall cityMenuBuildHook(uint8_t* menu, void* edx) {
    int ok = g_cityBuildPrev(menu, edx);
    if (!ok || tradeCity(menu) < 0) return ok;
    typedef int (__thiscall* AddFn)(void*, const char*, int, int);
    static char label[16] = {};
    if (!label[0]) WideCharToMultiByte(950, 0, L"貿易", -1, label, sizeof label, nullptr, nullptr);
    return ((AddFn)0x59f4b0)(menu, label, CITY_TRADE, 1);
}
int __fastcall cityMenuRangeHook(uint8_t* menu, void* edx, int id, int* first, int* last) {
    if (id != CITY_TRADE && !cityTradeEntry(id)) return g_cityRangePrev(menu, edx, id, first, last);
    if (!first || !last) return 0;
    *first = id == CITY_TRADE ? CITY_TRADE + 1 : INT_MIN;
    *last = id == CITY_TRADE ? CITY_TRADE + CITY_TRADE_COUNT : INT_MIN;
    return 1;
}
const char* __fastcall cityMenuTextHook(uint8_t* menu, void* edx, int id) {
    const CityTradeEntry* entry = cityTradeEntry(id);
    if (!entry && id != CITY_TRADE) return g_cityTextPrev(menu, edx, id);
    static char text[CITY_TRADE_COUNT + 1][32] = {};
    char* label = text[id - CITY_TRADE];
    if (!label[0]) WideCharToMultiByte(950, 0, entry ? entry->label : L"貿易", -1, label, 32, nullptr, nullptr);
    return label;
}
const char* __fastcall cityMenuHelpHook(uint8_t* menu, void* edx, int id) {
    const CityTradeEntry* entry = cityTradeEntry(id);
    if (!entry && id != CITY_TRADE) return g_cityHelpPrev(menu, edx, id);
    static char text[CITY_TRADE_COUNT + 1][128] = {};
    char* help = text[id - CITY_TRADE];
    if (!help[0]) WideCharToMultiByte(950, 0, entry ? entry->help : L"展開行情、組隊、商隊、帳本與情報。", -1, help, 128, nullptr, nullptr);
    return help;
}
int __fastcall cityMenuEnabledHook(uint8_t* menu, void* edx, int id) {
    return id == CITY_TRADE || cityTradeEntry(id) ? tradeCity(menu) >= 0 : g_cityEnabledPrev(menu, edx, id);
}
int __fastcall cityMenuParentHook(uint8_t* menu, void* edx, int id) {
    return cityTradeEntry(id) ? CITY_TRADE : g_cityParentPrev(menu, edx, id);
}
void __fastcall cityMenuClickHook(uint8_t* menu, void* edx, void* button) {
    typedef int (__thiscall* ItemIdFn)(void*, void*);
    int id = ((ItemIdFn)0x4db7e0)(menu, button);
    if (id == INT_MIN) id = ((ItemIdFn)0x4db7e0)(menu + 0x1a8, button);
    const CityTradeEntry* entry = cityTradeEntry(id);
    if (!entry) {
        // The native handler expands/collapses the trade parent just like 軍事 / 人材.
        g_cityClickPrev(menu, edx, button);
        return;
    }
    int c = tradeCity(menu);
    uint8_t* stage = *(uint8_t**)(uintptr_t)0x91ba018;
    void* mainUi = stage ? *(void**)(stage + 8) : nullptr;
    if (c < 0 || !mainUi) return;
    prepareCityTrade(c, *entry);
    g_uiResult.clear();
    g_cityOpening = c;
    g_log("trade: city submenu action %d opened for city %d", id - CITY_TRADE, c);
    typedef void (__thiscall* ShowFn)(void*, int);
    ShowFn show = (ShowFn)(*(void***)menu)[0x4c / 4];
    uint8_t* sub = menu + 0x1a8;
    ShowFn showSub = (ShowFn)(*(void***)sub)[0x4c / 4];
    int subVisible = *(int*)(sub + 0x24);
    showSub(sub, 0);
    show(menu, 0);
    nativeCityTrade(c,id-CITY_TRADE);
    g_cityOpening = -1;
    show(menu, 1);
    showSub(sub, subVisible);
}
// Init composes before the caller sets main-map mode (+0x8c). Compose once more
// after that flag is set, so the first frame has the trade entry / city panel.
void __fastcall mapModeHook(uint8_t* ui, void*, int mainMap) {
    *(int*)(ui + 0x8c) = mainMap;
    if (mainMap) {
        if (g_cityOpening >= 0 && g_cityOpening < g_data.C)
            midmapSetView((float)g_data.lo[g_cityOpening], (float)g_data.hi[g_cityOpening], 2.0f);
        repaint(ui, tradeui::REPAINT_MAP | tradeui::REPAINT_LAYOUT);
    }
}
void installCityMenu() {
    static const uint8_t modeCode[] = { 0x8b, 0x44, 0x24, 0x04, 0x89, 0x81, 0x8c, 0, 0, 0, 0xc2, 4, 0 };
    if (*(uint32_t*)0x851ee8 != 0x617790 || *(uint32_t*)0x851e1c != 0x6186e0 ||
        *(uint32_t*)0x851ef0 != 0x617890 || *(uint32_t*)0x851efc != 0x617ab0 ||
        *(uint32_t*)0x851f04 != 0x617ae0 || *(uint32_t*)0x851f0c != 0x617b80 || *(uint32_t*)0x851f10 != 0x617a00 ||
        *(uint32_t*)(0x85b9e0+0x1d4) != 0x64e1d0 || *(uint32_t*)(0x85b9e0+0x1d8) != 0x707028 ||
        *(uint32_t*)(0x8344f8+0x1e8) != 0x4e3af0 || *(uint32_t*)(0x8344f8+0x1fc) != 0x4e1e40 ||
        memcmp((void*)0x63d500, modeCode, sizeof modeCode)) {
        g_log("trade: city menu NOT installed: unexpected native menu / list / map mode code");
        return;
    }
    g_cityBuildPrev = (CityMenuBuildFn)patchSlot(0x851ee8, (void*)cityMenuBuildHook);
    g_cityClickPrev = (CityMenuClickFn)patchSlot(0x851e1c, (void*)cityMenuClickHook);
    g_cityRangePrev = (CityMenuRangeFn)patchSlot(0x851ef0, (void*)cityMenuRangeHook);
    g_cityTextPrev = (CityMenuTextFn)patchSlot(0x851efc, (void*)cityMenuTextHook);
    g_cityEnabledPrev = (CityMenuValueFn)patchSlot(0x851f04, (void*)cityMenuEnabledHook);
    g_cityHelpPrev = (CityMenuTextFn)patchSlot(0x851f0c, (void*)cityMenuHelpHook);
    g_cityParentPrev = (CityMenuValueFn)patchSlot(0x851f10, (void*)cityMenuParentHook);
    jmpTo(0x63d500, (void*)mapModeHook);
    g_log("trade: native city menu installed (five submenu actions, selected city, first paint)");
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
const uint32_t TRAD_VERSION = 13;          // v13: port endpoints and timed freight transfers (append-only)
void resetPhase3() {
    g_ev.live.clear(); g_mk.mod.clear(); g_done.clear(); g_merch.clear();
    g_info.reset(g_s.R, g_data, g_mk.p.infoMonths);
}
void resetState() {
    for(auto& danger:g_routeDanger)std::atomic_store(&danger,std::shared_ptr<const DangerCells>{});
    Lock l;
    clearNativeDrafts();
    for (auto& j : g_jobs) { j->result = "cancelled by game load"; if (j->done) SetEvent(j->done); }
    g_jobs.clear(); g_uiResult.clear();
    g_autoPlans.clear();
    g_purchases.clear();g_nextPurchase=1;
    g_car.clear(); g_closed.clear(); g_credit.clear(); g_saleTax.clear(); g_nextCar = 1; g_mk.reset(); g_lastMonth = -1; g_rng = Rng(); g_nextAiForce = 0;
    g_month0 = (today() / 30) % 12; g_rawBlock.clear(); g_readOnly = false;
    resetPhase3();
    rebuildDirs();
}
template <class T> void put(std::vector<uint8_t>& v, const T& x) { const uint8_t* p = (const uint8_t*)&x; v.insert(v.end(), p, p + sizeof x); }
void putLedger(std::vector<uint8_t>& v, const Ledger& a) {
    put(v, a.purchases); put(v, a.receipts); put(v, a.fees); put(v, a.taxes); put(v, a.losses); put(v, (uint32_t)a.complete);
}
std::vector<uint8_t> payloadBytes(bool withTax = true, bool withTransit = true, bool withAuto = true, bool withPurchases = true, bool withShips = true, bool withDelegated = true, bool withPorts = true) {
    std::vector<uint8_t> pl;
    pl.reserve(g_info.observed.size()*8+g_info.hist.size()*2+g_info.age.size()+g_mk.idx.size()*8+4096);
    put(pl, (uint32_t)g_data.C); put(pl, (uint32_t)g_data.G);
    for (int32_t v : g_mk.idx) put(pl, v);
    put(pl, g_lastMonth); put(pl, g_rng.s); put(pl, g_nextAiForce);
    uint32_t active = 0; for (auto& k : g_car) active += k.state != FINISHED;
    put(pl, active);
    for (auto& k : g_car) if (k.state != FINISHED) put(pl, static_cast<const LegacyCaravan&>(k));
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
    // v3
    put(pl, (uint32_t)g_merch.size());
    for (auto& m : g_merch) put(pl, m);
    // v4: inventory, complete observations, multi-good voyages and outstanding credits.
    for (auto v : g_mk.stock) put(pl, v);
    for (auto& o : g_info.observed) { put(pl, o.idx); put(pl, o.mod); put(pl, o.stock); }
    put(pl, g_nextCar);
    for (auto& k : g_car) {
        if (k.state == FINISHED) continue;
        put(pl, k.id); put(pl, k.state); put(pl, k.port); put(pl, k.lastProgress); put(pl, k.lastPos); put(pl, k.maxLoad);
        put(pl, (uint32_t)k.cargo.lots.size());
        for (auto& l : k.cargo.lots) { put(pl, l.good); put(pl, l.origin); put(pl, l.qty); put(pl, l.initial); put(pl, l.cost); }
        put(pl, (uint32_t)k.taxed.size()); for (auto c : k.taxed) put(pl, c);
    }
    put(pl, (uint32_t)g_credit.size());
    for (auto& c : g_credit) { put(pl, c.force); put(pl, c.origin); put(pl, c.gold); put(pl, c.troops); put(pl, c.food); }
    // v5: cumulative voyage accounts; v4 prefix unchanged.
    for (auto& k : g_car) if (k.state != FINISHED) putLedger(pl, k.ledger);
    put(pl, (uint32_t)g_closed.size());
    for (auto& c : g_closed) {
        put(pl, c.id); put(pl, c.force); put(pl, c.origin); put(pl, c.dest); put(pl, c.day); putLedger(pl, c.ledger);
    }
    if (withTax) {
        put(pl, (uint32_t)g_saleTax.size());
        for (auto& r : g_saleTax) { put(pl, r.payer); put(pl, r.receiver); put(pl, r.remainder); }
    }
    if(withTax && withTransit) for(auto& k:g_car) if(k.state!=FINISHED) {put(pl,k.transitLimit);put(pl,k.transitPaid);}
    if(withTax && withTransit && withAuto) {
        put(pl,(uint32_t)g_autoPlans.size());
        for(auto& a:g_autoPlans) {
            put(pl,a.origin);put(pl,a.force);put(pl,a.preferredDestination);put(pl,(uint32_t)a.enabled);
            put(pl,a.budget.month);put(pl,a.budget.limit);put(pl,a.budget.occupied);put(pl,a.dispatchedMonth);
            put(pl,(uint32_t)a.preferredGoods.size());for(auto g:a.preferredGoods)put(pl,g);
        }
        for(auto& k:g_car)if(k.state!=FINISHED) {
            auto& a=k.automatic;put(pl,(uint32_t)k.tempReleased);put(pl,a.caravan);
            put(pl,(int32_t)a.stage);put(pl,(int32_t)a.issue);put(pl,a.retryMonth);put(pl,(uint32_t)a.managed);
            put(pl,a.fees.month);put(pl,a.fees.balance);
        }
    }
    if(withTax && withTransit && withAuto && withPurchases) {
        put(pl,g_nextPurchase);put(pl,(uint32_t)g_purchases.size());
        for(auto& r:g_purchases) {put(pl,r.id);put(pl,r.day);put(pl,r.force);put(pl,r.city);put(pl,r.good);put(pl,r.qty);put(pl,r.cost);}
    }
    if(withTax && withTransit && withAuto && withPurchases && withShips) {
        for(auto& k:g_car)if(k.state!=FINISHED)put(pl,k.shipType);
        for(auto& a:g_autoPlans)put(pl,a.shipType);
        if(withDelegated)for(auto& a:g_autoPlans)put(pl,(uint32_t)a.delegated);
        if(withDelegated && withPorts)for(auto& k:g_car)if(k.state!=FINISHED) {
            put(pl,k.legOrigin);put(pl,k.embarkBase);put(pl,k.arrivalBase);put(pl,k.transferStage);put(pl,k.transferUntil);
        }
    }
    return pl;
}
struct TradeCodec {
    COMPRESSOR_HANDLE h = nullptr;
    bool reading;
    explicit TradeCodec(bool read) : reading(read) {
        if (read) CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &h);
        else CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &h);
    }
    ~TradeCodec() { if (h) { if (reading) CloseDecompressor(h); else CloseCompressor(h); } }
};
// Koei's 32 MiB heap must hold both the chunked world and its contiguous copy.
// Compress before streamWrite; leaving this to the outer MWC compressor is too late.
std::vector<uint8_t> compressBlock(const std::vector<uint8_t>& pl, uint32_t version) {
    TradeCodec codec(false); SIZE_T size = 0;
    if (!codec.h) return {};
    Compress(codec.h, pl.data(), pl.size(), nullptr, 0, &size);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !size || size > UINT32_MAX - 4) return {};
    std::vector<uint8_t> out(16 + size);
    if (!Compress(codec.h, pl.data(), pl.size(), out.data() + 16, size, &size)) return {};
    out.resize(16 + size);
    uint32_t hdr[] = {TRAD, version, (uint32_t)size + 4, (uint32_t)pl.size()};
    memcpy(out.data(), hdr, sizeof hdr);
    return out;
}
std::vector<uint8_t> blockBytes() { return compressBlock(payloadBytes(), TRAD_VERSION); }
bool canExpandLegacyGoods(uint32_t savedGoods) {
    static const char* names[]={"絲","蜀錦","漆器","鐵器","馬","玉","胡椒","棉布","玻璃","珊瑚","乳香","皮毛","珍珠","鹽",
        "小麥","稻米","大豆","魚乾","牛肉","乳酪","葡萄乾","蜂蜜","蔗糖","橄欖油","椰子油","生薑","肉桂","丁香",
        "豆蔻","藏紅花","麝香","檀香","羊毛","棉花","麻","毛織品","麻布","絨毯","葡萄酒","茶","米酒","黃金",
        "白銀","琥珀","水晶","紅寶石","藍寶石","祖母綠","金剛石","陶器","象牙","象牙雕品","金工藝品","雕刻","繪畫","人參",
        "大黃","靛藍","鬱金","木材","蜂蠟","鐵礦石","銅礦石","錫礦石","煤炭","硫磺","硝鹽","刀劍","甲胄"};
    if((savedGoods!=14 && savedGoods!=69) || g_data.G<=(int)savedGoods || g_data.goodName.size()<(size_t)g_data.G)return false;
    for(uint32_t i=0;i<savedGoods;++i)if(g_data.goodName[i]!=names[i])return false;
    return true;
}
// parses a payload; false (state unchanged) if malformed
bool parseBlock(const uint8_t* p, size_t n, uint32_t version, int* cars, bool unpacked = false) {
    if (version < 1 || version > TRAD_VERSION) return false;
    if (version >= 6 && !unpacked) {
        uint32_t rawSize;
        if (n < 4) return false;
        memcpy(&rawSize, p, 4);
        if (!rawSize || rawSize > 128u * 1024 * 1024) return false;
        TradeCodec codec(true); SIZE_T size = 0;
        if (!codec.h) return false;
        Decompress(codec.h, p + 4, n - 4, nullptr, 0, &size);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size != rawSize) return false;
        std::vector<uint8_t> raw(rawSize);
        if (!Decompress(codec.h, p + 4, n - 4, raw.data(), raw.size(), &size) || size != rawSize) return false;
        return parseBlock(raw.data(), raw.size(), version == 6 ? 5 : version, cars, true);
    }
    const uint8_t* e = p + n;
    auto get = [&](void* v, size_t s) { if ((size_t)(e - p) < s) return false; memcpy(v, p, s); p += s; return true; };
    uint32_t C, G, nc, rng, nextAi; int32_t lastMonth;
    if (!get(&C, 4) || !get(&G, 4) || !C || C > 4096 || !G || G > MAX_GOODS) return false;
    bool expand=C==(uint32_t)g_data.C && canExpandLegacyGoods(G);
    if((size_t)C*G>(size_t)(e-p)/4)return false;
    std::vector<int32_t> idx((size_t)C * G);
    if (!idx.empty() && !get(idx.data(), idx.size() * 4)) return false;
    if (!get(&lastMonth, 4) || !get(&rng, 4) || !get(&nextAi, 4) || !get(&nc, 4) || nc > (uint32_t)g_s.U) return false;
    std::vector<Caravan> cars_((size_t)nc);
    std::set<int> unitIds, leaders;
    for (auto& k : cars_) {
        if (!get(static_cast<LegacyCaravan*>(&k), sizeof(LegacyCaravan))) return false;
        if (k.unit < 0 || k.unit >= g_s.U || k.leader < 0 || k.leader >= g_cfg.PT || k.force < 0 || k.force >= g_s.R ||
            k.origin < 0 || k.origin >= g_data.C || k.dest < 0 || k.dest >= g_data.C || k.good < 0 || k.good >= (int)G ||
            k.qty < 0 || k.qty > 100000 || k.cost < 0 || k.taxDue < 0 || (k.flags & ~15u)) return false;
    }
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
        if (iR > 4096 || iC > 4096 || iG > MAX_GOODS || iH > 60) return false;
        if((uint64_t)iR*iC+(uint64_t)iH*iC*iG*2>(uint64_t)(e-p))return false;
        age.resize((size_t)iR * iC); hist.resize((size_t)iH * iC * iG);
        if (!age.empty() && !get(age.data(), age.size())) return false;
        if (!hist.empty() && !get(hist.data(), hist.size() * 2)) return false;
    }
    std::vector<MerchRec> merch;                              // v3: merchant officers
    if (version >= 3) {
        uint32_t nm;
        if (!get(&nm, 4) || nm > 100000) return false;
        merch.resize(nm); if (nm && !get(merch.data(), nm * sizeof(MerchRec))) return false;
    }
    std::vector<int32_t> stock;
    std::vector<Info::Observation> observed;
    std::vector<Credit> credits;
    uint32_t nextCar = 1;
    if (version >= 4) {
        if (C != (uint32_t)g_data.C || (G != (uint32_t)g_data.G && !expand) || iR != (uint32_t)g_s.R || iC != C || iG != G) return false;
        if((uint64_t)C*G*4+(uint64_t)iR*C*G*8>(uint64_t)(e-p))return false;
        stock.resize((size_t)C * G);
        if (!get(stock.data(), stock.size() * 4)) return false;
        for (size_t i = 0; i < stock.size(); i++) if (stock[i] < 0 || stock[i] > g_data.size[i / G] * g_data.stockCap / 100) return false;
        observed.resize((size_t)iR * C * G);
        for (auto& o : observed) if (!get(&o.idx, 2) || !get(&o.mod, 2) || !get(&o.stock, 4) || o.idx < 0 || o.idx > 4000 || o.mod > 5000 || o.stock < 0) return false;
        if (!get(&nextCar, 4) || !nextCar) return false;
        std::set<uint32_t> ids;
        for (auto& k : cars_) {
            if (!get(&k.id, 4) || !get(&k.state, 4) || !get(&k.port, 4) || !get(&k.lastProgress, 4) || !get(&k.lastPos, 4) || !get(&k.maxLoad, 4)) return false;
            if (!k.id || k.id >= nextCar || !ids.insert(k.id).second || k.state < TRAVELLING || k.state > FINISHED || k.port < -1 || k.port >= g_data.C || k.maxLoad < 1 || k.maxLoad > 100000) return false;
            uint32_t count;
            if (!get(&count, 4) || count > C * G) return false;
            for (uint32_t i = 0; i < count; i++) {
                CargoLot l;
                if (!get(&l.good, 4) || !get(&l.origin, 4) || !get(&l.qty, 4) || !get(&l.initial, 4) || !get(&l.cost, 8) ||
                    l.good < 0 || l.good >= (int)G || l.origin < 0 || l.origin >= g_data.C || l.qty < 1 || l.qty > k.maxLoad*10/g_data.loadUnits(l.good) || l.initial < l.qty || l.cost < 0 || l.cost > INT_MAX) return false;
                k.cargo.lots.push_back(l);
            }
            if (k.cargo.loadUnits(g_data) > (int64_t)k.maxLoad*10 || k.cargo.count() != k.qty || k.cargo.cost() > INT_MAX) return false;
            syncCargo(k); // Older v4/v5 writers included departure fees in the legacy summary.
            if (!get(&count, 4) || count > C) return false;
            std::set<int> taxed;
            for (uint32_t i = 0; i < count; i++) { int32_t c; if (!get(&c, 4) || c < 0 || c >= g_data.C || !taxed.insert(c).second) return false; k.taxed.push_back(c); }
        }
        uint32_t count;
        if (!get(&count, 4) || count > (uint32_t)g_s.R * C) return false;
        for (uint32_t i = 0; i < count; i++) {
            Credit c;
            if (!get(&c.force, 4) || !get(&c.origin, 4) || !get(&c.gold, 8) || !get(&c.troops, 8) || !get(&c.food, 8) ||
                c.force < 0 || c.force >= g_s.R || c.origin < 0 || c.origin >= g_data.C || c.gold < 0 || c.troops < 0 || c.food < 0 ||
                c.gold > 1000000000000LL || c.troops > 1000000000LL || c.food > 1000000000000LL) return false;
            credits.push_back(c);
        }
    } else {
        for (auto& k : cars_) {
            k.id = nextCar++; k.lastProgress = today(); k.maxLoad = std::max(k.qty, g_mk.p.capLand);
            k.cargo.add(k.good, k.origin, k.qty, k.cost);
        }
    }
    for (auto& r : done) if (r.force < 0 || r.force >= g_s.R || r.origin < 0 || r.origin >= g_data.C || r.dest < 0 || r.dest >= g_data.C || r.good < 0 || r.good >= (int)G || r.qty < 0 || r.cost < 0 || r.net < 0) return false;
    for (auto& event : evs) if (event.def >= g_data.events.size() || (event.good >= G && event.good != EV_GOOD_ALL) || event.group > g_data.groupName.size() || event.end <= event.start) return false;
    std::set<int> merchantIds;
    for (auto& m : merch) if (m.person < 0 || m.person >= g_cfg.PT || m.exp < 0 || m.trips < 0 || !merchantIds.insert(m.person).second) return false;
    std::vector<ClosedVoyage> closed;
    if (version >= 5) {
        auto account = [&](Ledger& a) {
            uint32_t complete;
            if (!get(&a.purchases, 8) || !get(&a.receipts, 8) || !get(&a.fees, 8) || !get(&a.taxes, 8) || !get(&a.losses, 8) || !get(&complete, 4)) return false;
            for (auto v : {a.purchases, a.receipts, a.fees, a.taxes, a.losses}) if (v < 0 || v > 1000000000000LL) return false;
            if (complete > 1 || a.losses > a.purchases) return false;
            a.complete = complete != 0; return true;
        };
        std::set<uint32_t> ids;
        for (auto& k : cars_) {
            if (!account(k.ledger) || k.ledger.purchases < k.cargo.cost() + k.ledger.losses) return false;
            ids.insert(k.id);
        }
        uint32_t count;
        if (!get(&count, 4) || count > DONE_MAX) return false;
        for (uint32_t i = 0; i < count; i++) {
            ClosedVoyage c;
            if (!get(&c.id,4) || !get(&c.force,4) || !get(&c.origin,4) || !get(&c.dest,4) || !get(&c.day,4) || !account(c.ledger) ||
                !c.id || c.id >= nextCar || !ids.insert(c.id).second || c.force < 0 || c.force >= g_s.R ||
                c.origin < 0 || c.origin >= g_data.C || c.dest < 0 || c.dest >= g_data.C || c.day < 0 || c.day > today()) return false;
            closed.push_back(c);
        }
    } else {
        for (auto& k : cars_) { k.ledger.purchases = k.cargo.cost(); k.ledger.complete = false; }
    }
    std::vector<TaxRemainder> saleTax;
    if (version >= 7) {
        uint32_t count;
        if (!get(&count,4) || count > (uint32_t)g_s.R * g_s.R) return false;
        std::set<std::pair<int,int>> keys;
        for (uint32_t i=0;i<count;i++) {
            TaxRemainder r;
            if (!get(&r.payer,4) || !get(&r.receiver,4) || !get(&r.remainder,4) ||
                r.payer<0 || r.payer>=g_s.R || r.receiver<0 || r.receiver>=g_s.R || r.payer==r.receiver ||
                r.remainder<1 || r.remainder>=10000 || !keys.insert({r.payer,r.receiver}).second) return false;
            saleTax.push_back(r);
        }
    }
    if(version>=8) for(auto& k:cars_) {
        if(!get(&k.transitLimit,8) || !get(&k.transitPaid,8) || k.transitLimit<0 || k.transitLimit>INT_MAX || k.transitPaid<0 || k.transitPaid>k.transitLimit) return false;
    } // Older voyages get no additional transit charge until their next departure.
    std::vector<tradeauto::Plan> autoPlans;
    if(version>=9) {
        uint32_t count;if(!get(&count,4) || count>(uint32_t)g_data.C*g_s.R)return false;
        std::set<std::pair<int,int>> keys;
        for(uint32_t n=0;n<count;++n) {
            tradeauto::Plan a;uint32_t enabled,goods;
            if(!get(&a.origin,4)||!get(&a.force,4)||!get(&a.preferredDestination,4)||!get(&enabled,4)||
               !get(&a.budget.month,4)||!get(&a.budget.limit,8)||!get(&a.budget.occupied,8)||!get(&a.dispatchedMonth,4)||!get(&goods,4))return false;
            if(a.origin<0||a.origin>=g_data.C||a.force<0||a.force>=g_s.R||a.preferredDestination<(version>=12?-1:0)||a.preferredDestination>=g_data.C||a.preferredDestination==a.origin||enabled>1||goods>(uint32_t)g_data.G||
               a.budget.month<0||a.budget.month>today()/30||a.budget.limit<0||a.budget.limit>100000||a.budget.occupied<0||a.budget.occupied>100000||a.dispatchedMonth < -1||a.dispatchedMonth>today()/30||!keys.insert({a.force,a.origin}).second)return false;
            a.enabled=enabled!=0;std::set<int> ids;
            for(uint32_t j=0;j<goods;++j) {int g;if(!get(&g,4)||g<0||g>=(int)G||!ids.insert(g).second)return false;a.preferredGoods.push_back(g);}
            autoPlans.push_back(a);
        }
        for(auto& k:cars_) {
            auto& a=k.automatic;uint32_t released,managed;int stage,issue;
            if(!get(&released,4)||!get(&a.caravan,4)||!get(&stage,4)||!get(&issue,4)||!get(&a.retryMonth,4)||!get(&managed,4)||!get(&a.fees.month,4)||!get(&a.fees.balance,8))return false;
            if(released>1 || (released && (!(k.flags&F_TEMP) || k.port<0 || (k.state!=DOCKED && k.state!=WAITING))) || managed>1 || stage<0||stage>5||issue<0||issue>6||a.retryMonth < -1||a.retryMonth>today()/30+1||a.fees.month < -1||a.fees.month>today()/30||a.fees.balance<0||a.fees.balance>100000)return false;
            if(a.caravan && (a.caravan!=k.id || !keys.count({k.force,k.origin})))return false;
            if(!a.caravan && a.fees.balance)return false;
            k.tempReleased=released!=0;a.managed=managed!=0;a.stage=(tradeauto::Stage)stage;a.issue=(tradeauto::Issue)issue;a.origin=k.origin;a.force=k.force;
        }
    }
    uint32_t nextPurchase=1;std::vector<PurchaseRec> purchases;
    if(version>=10) {
        uint32_t count=0,previous=0;
        if(!get(&nextPurchase,4)||!nextPurchase||!get(&count,4)||count>PURCHASE_MAX)return false;
        for(uint32_t i=0;i<count;++i) {
            PurchaseRec r={};
            if(!get(&r.id,4)||!get(&r.day,4)||!get(&r.force,4)||!get(&r.city,4)||!get(&r.good,4)||!get(&r.qty,4)||!get(&r.cost,8))return false;
            if(r.id<=previous||r.id>=nextPurchase||r.day<0||r.day>today()||r.force<0||r.force>=g_s.R||r.city<0||r.city>=(int)C||
               r.good<0||r.good>=(int)G||g_data.militaryWeapon(r.good)<0||r.qty<1||r.qty>100000||r.cost<1||r.cost>INT_MAX)return false;
            previous=r.id;purchases.push_back(r);
        }
    }
    if(version>=11) {
        for(auto& k:cars_)if(!get(&k.shipType,4) || !validTransport(k.shipType))return false;
        for(auto& a:autoPlans)if(!get(&a.shipType,4) || !validTransport(a.shipType))return false;
    }
    if(version>=12)for(auto& a:autoPlans) {
        uint32_t delegated;if(!get(&delegated,4) || delegated>1)return false;
        a.delegated=delegated!=0;
    }
    if(version>=13)for(auto& k:cars_) {
        if(!get(&k.legOrigin,4)||!get(&k.embarkBase,4)||!get(&k.arrivalBase,4)||!get(&k.transferStage,4)||!get(&k.transferUntil,4))return false;
        auto anchorValid=[&](int b){return b==-1 || (b>=0 && b<(int)C) || (b>=(int)C+10 && b<(int)C+45);};
        if(k.legOrigin < -1 || k.legOrigin >= (int)C || !anchorValid(k.embarkBase) || !anchorValid(k.arrivalBase) ||
           k.transferStage<0 || k.transferStage>2 || k.transferUntil<0 || k.transferUntil>today()+20)return false;
        if(k.transferStage && (k.shipType!=MERCHANT_SHIP || k.port!=-1 || k.tempReleased ||
            (k.state!=TRAVELLING && k.state!=WAITING) || k.transferUntil==0 ||
            (k.transferStage==1 ? k.legOrigin<0 || k.embarkBase<(int)C+10 : k.arrivalBase<(int)C+10)))return false;
        if(!k.transferStage && k.transferUntil)return false;
        if((k.embarkBase>=0 && k.embarkBase<(int)C && k.embarkBase!=k.legOrigin) ||
           (k.arrivalBase>=0 && k.arrivalBase<(int)C && k.arrivalBase!=k.dest) ||
           (k.shipType!=MERCHANT_SHIP && (k.embarkBase!=-1 || k.arrivalBase!=-1)))return false;
    }
    for(auto& k:cars_)if(!k.tempReleased && (!unitIds.insert(k.unit).second || !leaders.insert(k.leader).second))return false;
    if (p != e || lastMonth < -1 || lastMonth > today() / 30 || histMonth > today() / 30) return false;
    if(expand) {
        // Only the published 14/69-good prefixes are migratable. Keep its IDs and rows;
        // initialize appended markets, but never invent old foreign observations.
        Market initial=g_mk;initial.reset();
        for(uint32_t c=0;c<C;++c) {
            std::copy_n(idx.begin()+(size_t)c*G,G,initial.idx.begin()+(size_t)c*g_data.G);
            if(version>=4)std::copy_n(stock.begin()+(size_t)c*G,G,initial.stock.begin()+(size_t)c*g_data.G);
        }
        idx=std::move(initial.idx);if(version>=4)stock=std::move(initial.stock);
        if(iC==C && iG==G) {
            std::vector<int16_t> expandedHist((size_t)iH*C*g_data.G,0);
            for(size_t row=0;row<(size_t)iH*C;++row)std::copy_n(hist.begin()+row*G,G,expandedHist.begin()+row*g_data.G);
            hist=std::move(expandedHist);
            if(version>=4) {
                std::vector<Info::Observation> expandedObserved((size_t)iR*C*g_data.G);
                for(size_t row=0;row<(size_t)iR*C;++row)std::copy_n(observed.begin()+row*G,G,expandedObserved.begin()+row*g_data.G);
                observed=std::move(expandedObserved);
            }
            iG=g_data.G;
        }
        if(g_log)g_log("trade: expanded legacy goods %u -> %d; existing cargo, budgets and market rows preserved",G,g_data.G);
        G=g_data.G;
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
    for (auto& m : merch) if (m.person >= 0 && m.person < g_cfg.PT && m.exp >= 0) g_merch.push_back(m);
    g_autoPlans=std::move(autoPlans);
    g_purchases=std::move(purchases);g_nextPurchase=nextPurchase;
    g_nextCar = nextCar; g_credit = credits; g_closed = std::move(closed); g_saleTax = std::move(saleTax);
    if (version >= 4) { g_mk.stock = std::move(stock); g_info.observed = std::move(observed); }
    else std::fill(g_info.age.begin(), g_info.age.end(), 255);
    g_month0 = (today() / 30) % 12;
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
    {
        const MovementStepStats* ms = movementStepStats();
        fprintf(f, "monsoon steps: with a direction %u, without %u (movement.cpp: A* %u, range search %u, path sum %u, unknown %u)\n",
                g_dirStats.astar, g_dirStats.unknown, ms->astar, ms->range, ms->pathSum, ms->unknown);
    }
    for (auto& m : g_merch) fprintf(f, "merchant officer %d: experience %d, %d trips, level %d\n", m.person, m.exp, m.trips, leaderLevel(m.person));
    for(auto& p:g_autoPlans) {
        char status[1024]={};WideCharToMultiByte(CP_UTF8,0,p.status.c_str(),-1,status,sizeof status,nullptr,nullptr);
        fprintf(f,"auto plan city %d force %d delegated %d enabled %d target %d budget %lld occupied %lld dispatched %d active %d limit %d status %s\n",
            p.origin,p.force,p.delegated,p.enabled,p.preferredDestination,p.budget.limit,p.budget.occupied,p.dispatchedMonth,
            cityAutoCount(p.force,p.origin),cityAutoLimit(p.force,p.origin),status);
    }
    for (auto& r : g_done) fprintf(f, "done day %d force %d: %s -> %s %d x %s cost %d net %d flags %u\n", r.day, r.force, cityName(r.origin),
                                   cityName(r.dest), r.qty, goodName(r.good), r.cost, r.net, r.flags);
    for (auto& k : g_car) {
        uint8_t* u = mapReady() ? liveUnit(k) : nullptr;
        const int16_t* up = u ? posOf(u) : nullptr;
        fprintf(f, "caravan unit %d leader %d force %d%s%s: %s -> %s, %d x %s, cost %d, tax %d, at %d,%d\n", k.unit, k.leader, k.force,
                k.flags & F_TEMP ? " 商" : "", k.flags & F_PLAYER ? " player" : "", cityName(k.origin), cityName(k.dest), k.qty,
                k.cargo.lots.size()>1 ? "mixed cargo" : goodName(k.good), k.cost, k.taxDue, up ? up[0] : -1, up ? up[1] : -1);
        fprintf(f, "  voyage %u state %d berth %d,%d route steps %zu cost %d; purchases %lld receipts %lld fees %lld taxes %lld losses %lld cash %lld profit %lld complete %d\n",
                k.id, k.state, (int16_t)k.dockHex, (int16_t)(k.dockHex >> 16), k.route.empty() ? 0 : k.route.size()-1, k.routeCost,
                k.ledger.purchases, k.ledger.receipts, k.ledger.fees, k.ledger.taxes, k.ledger.losses,
                k.ledger.cashNet(), k.ledger.profit(k.cargo.cost()), k.ledger.complete);
        fprintf(f,"  cargo slots %lld/10 of %d; transit %lld/%lld\n",k.cargo.loadUnits(g_data),k.maxLoad,k.transitPaid,k.transitLimit);
        if(k.shipType==MERCHANT_SHIP)fprintf(f,"  ports from city %d via %d to %d; transfer stage %d until %d\n",k.legOrigin,k.embarkBase,k.arrivalBase,k.transferStage,k.transferUntil);
        for(auto& lot:k.cargo.lots) fprintf(f,"    cargo good %d %s origin %d qty %d cost %lld\n",lot.good,goodName(lot.good),lot.origin,lot.qty,lot.cost);
    }
    for (auto& c : g_credit) fprintf(f, "pending force %d origin %d gold %lld troops %lld food %lld\n", c.force,c.origin,c.gold,c.troops,c.food);
    fprintf(f, "city id name force flags");
    for (int g = 0; g < g_data.G; g++) fprintf(f, " %s", g_data.goodName[g].c_str());
    fprintf(f, "   (buy price / index permille / stock; * = available)\n");
    for (int c = 0; c < g_data.C; c++) {
        fprintf(f, "%d %s %d %s%s", c, cityName(c), mapReady() ? baseForce(c) : -1, g_data.hub(c) ? "H" : "-", g_data.port(c) ? "P" : "-");
        for (int g = 0; g < g_data.G; g++) fprintf(f, " %d/%d/%d%s", g_mk.buyPrice(c, g), g_mk.at(c, g), g_mk.available(c,g), g_mk.available(c,g) > 0 ? "*" : "");
        uint8_t* b = mapReady() ? base(c) : nullptr;
        if (b) fprintf(f, " gold=%d troops=%d food=%d", baseGold(b),vcall(b,0x50),baseFood(b));
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

#ifdef TRADE_NATIVE_TEST
#include "../test/trade_native_cases.inc"
#endif

void runJob(Job* j) {
    if (g_readOnly) { j->result = "save block is unsupported or damaged; trade is read-only"; return; }
    char cmd[32] = {}, a[64] = {}, b[64] = {}, c[64] = {};
    int qty = 0, officer = -1;
    sscanf_s(j->line.c_str(), "%*s %31s", cmd, (unsigned)sizeof cmd);
#ifdef TRADE_NATIVE_TEST
    if (!strcmp(cmd,"native_test")) {
        if (j->line.find("native_test interaction")!=std::string::npos) { j->result=nativeInteractionProbe();return; }
        if (j->line.find("native_test citylimit")!=std::string::npos) { j->result=nativeCityLimitProbe();return; }
        if (j->line.find("native_test specials")!=std::string::npos) { j->result=nativeSpecialPoolProbe(j->line.find(" keep")!=std::string::npos);return; }
        if (j->line.find("native_test speed")!=std::string::npos) { j->result=nativeSpeedProbe(j->line.find(" move")!=std::string::npos); return; }
        if (j->line.find("native_test routing")!=std::string::npos) { j->result=nativeRoutingProbe(); return; }
        if (j->line.find("native_test taxsave")!=std::string::npos) { j->result=nativePortProbe(7,-1,1,true,true); return; }
        if (j->line.find("native_test economy")!=std::string::npos) { j->result=nativePortProbe(7,-1,1,true); return; }
        if (j->line.find("native_test manual")!=std::string::npos) { j->result=nativeManualProbe(); return; }
        int origin=-1,port=-1,count=1;
        sscanf_s(j->line.c_str(),"%*s %*s %d %d %d",&origin,&port,&count);
        j->result=nativePortProbe(origin,port,count); return;
    }
#endif
    if(!strcmp(cmd,"procure")) {
        std::istringstream in(j->line);std::string prefix,verb,extra;int city=-1,good=-1,amount=0;int64_t expected=-1;uint32_t token=0;
        if(!(in>>prefix>>verb>>city>>good>>amount>>expected>>token) || (in>>extra)) {j->result="invalid military order";return;}
        int actor=j->actor>=0 ? j->actor : (city>=0 && city<g_data.C ? baseForce(city) : -1);
        if(actor<0 || !isPlayerForce(actor)) {j->result="military ownership changed";return;}
        auto err=procureMilitary(actor,city,good,amount,expected,token);
        j->result=err.empty() ? "ok military purchase" : err;
    } else if(!strcmp(cmd,"sendmix") || !strcmp(cmd,"sendship")) {
        std::istringstream in(j->line);std::string prefix,verb;int o=-1,d=-1,leader=-1,g,q;
        if(!(in>>prefix>>verb>>o>>d>>leader)) {j->result="invalid cargo combination";return;}
        int shipType=CARAVAN;
        if(!strcmp(cmd,"sendship") && (!(in>>shipType) || !validTransport(shipType))) {j->result="invalid transport type";return;}
        std::vector<OrderLine> order;
        while(in>>std::ws && !in.eof()) {if(!(in>>g>>q) || order.size()>=(size_t)g_data.G) {j->result="invalid cargo combination";return;} order.push_back({g,q});}
        if(!in.eof() || o<0 || o>=g_data.C || (j->actor>=0 && baseForce(o)!=j->actor)) {j->result="origin ownership changed";return;}
        Caravan k;auto err=createCaravanOrder(o,d,order,leader,false,&k,nullptr,shipType);
        j->result=err.empty() ? "ok voyage "+std::to_string(k.id) : err;
    } else if (!strcmp(cmd, "send")) {
        int n = sscanf_s(j->line.c_str(), "%*s %*s %63s %63s %63s %d %d", a, (unsigned)sizeof a, b, (unsigned)sizeof b, c, (unsigned)sizeof c, &qty, &officer);
        if (n < 4) { j->result = "usage: trade send <from> <to> <good> <qty> [officer]"; return; }
        int o = findCity(a), d = findCity(b), g = findGood(c);
        if (o < 0 || d < 0 || g < 0) { j->result = "unknown city or good"; return; }
        if (j->actor >= 0 && baseForce(o) != j->actor) { j->result = "origin ownership changed"; return; }
        Caravan k;
        std::string err = createCaravan(o, d, g, qty, n >= 5 ? officer : -1, false, &k);
        j->result = err.empty() ? "ok voyage " + std::to_string(k.id) + " unit " + std::to_string(k.unit) + " leader " + std::to_string(k.leader) + " qty " + std::to_string(k.qty) +
                                  " cost " + std::to_string(k.cost) : err;
    } else if (!strcmp(cmd,"sellbatch")) {
        Lock l;
        std::istringstream in(j->line);std::string head,verb;uint32_t id;int64_t expected;
        if(!(in>>head>>verb>>id>>expected) || expected<0) {j->result="invalid batch quote";return;}
        std::vector<OrderLine> order;OrderLine line;
        while(in>>line.good) {if(!(in>>line.qty)) {j->result="invalid batch cargo";return;}order.push_back(line);}
        if(!in.eof() || order.empty() || order.size()>(size_t)g_data.G) {j->result="invalid batch cargo";return;}
        Caravan* k=nullptr;for(auto& car:g_car)if(car.id==id && car.state!=FINISHED && !(car.flags&F_LOOTED) && (j->actor<0 || car.force==j->actor)) {k=&car;break;}
        if(!k) {j->result="unknown, destroyed or no longer owned voyage";return;}
        auto error=sellCargoBatch(*k,order,expected);
        j->result=error.empty()?"ok batch sale":error;
    } else if (!strcmp(cmd, "buy") || !strcmp(cmd, "sell") || !strcmp(cmd, "go") || !strcmp(cmd, "finish") || !strcmp(cmd, "resupply") || !strcmp(cmd, "route")) {
        unsigned id = 0; char arg[64] = {}; int amount = 0;
        sscanf_s(j->line.c_str(), "%*s %*s %u %63s %d", &id, arg, (unsigned)sizeof arg, &amount);
        Caravan* k = nullptr; for (auto& car : g_car) if (car.id == id && car.state != FINISHED) { k = &car; break; }
        if (!k || (k->flags & F_LOOTED) || (j->actor >= 0 && k->force != j->actor)) { j->result = "unknown, destroyed or no longer owned voyage"; return; }
        if(k->automatic.caravan && k->automatic.managed && strcmp(cmd,"route")) {j->result="automatic voyage: take manual control first";return;}
        std::string err;
        if (!strcmp(cmd, "buy") || !strcmp(cmd, "sell")) err = tradeCargo(*k, findGood(arg), amount, !strcmp(cmd, "buy"));
        else if (!strcmp(cmd, "go")) err = depart(*k, findCity(arg));
        else if (!strcmp(cmd, "route")) {
            uint8_t* u = liveUnit(*k); int dest = *arg ? findCity(arg) : k->dest;
            std::vector<uint32_t> path; int cost = 0;
            if (!u || dest < 0 || !marketRoute(u, dest, &path, &cost)) err = "no reachable market berth";
            else {
                uint32_t target = path.back();
                j->result = "ok native route: steps " + std::to_string(path.size()-1) + " cost " + std::to_string(cost) +
                    " berth " + std::to_string((int16_t)target) + "," + std::to_string((int16_t)(target>>16));
                return;
            }
        }
        else if (!strcmp(cmd, "finish")) err = finishCaravan(*k, !strcmp(arg, "discard"));
        else {
            uint8_t* u = liveUnit(*k); uint8_t* b = base(k->port);
            if (k->state != DOCKED || !u || !b || baseForce(k->port) != k->force) err = "resupply requires a friendly owned city and a live unit";
            else {
                int troops = std::max(0, g_cfg.escort - vcall(u, 0x50)), food = std::max(0, g_cfg.food - *(int*)(u + 0x20));
                if (vcall(b, 0x50) < troops || baseFood(b) < food) err = "city has insufficient troops or food";
                else { addTroops(PROC, b, -troops); addFood(PROC, b, -food); unitTroopsSet(u, vcall(u, 0x50) + troops); unitFoodSet(u, *(int*)(u + 0x20) + food); }
            }
        }
        j->result = err.empty() ? "ok voyage " + std::to_string(id) : err;
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
// Preserve even a truncated TRAD header/payload. Never consume an unrelated following block.
size_t tradSpan(const uint8_t* cur, size_t left) {
    uint32_t hdr[3] = {};
    if (!cur || left < 4) return 0;
    memcpy(hdr, cur, std::min(left, sizeof hdr));
    if (hdr[0] != TRAD) return 0;
    if (left < sizeof hdr) return left;
    return std::min(left, sizeof hdr + (size_t)hdr[2]);
}
bool streamImpl(uint8_t* stream) {
    {
        bool reading = *(int*)(stream + 8) == 1;
        bool save = isSaveKind(stream) != 0, scen = isScenarioKind(stream) != 0;
        if (!save && !scen) return true;                         // master data, TS stages...
        if (*(int*)(stream + 0x68)) { g_log("trade: stream obfuscation on at the end of the world data, TRAD block skipped"); return true; }
        if (reading) {
            resetState();
            if (!save) { g_log("trade: new game (scenario stream kind %#x): trade state reset", *(int*)(stream + 0x54)); return true; }
            uint8_t* cur = *(uint8_t**)(stream + 0x14), *end = *(uint8_t**)(stream + 0x18);
            size_t left = cur && end > cur ? (size_t)(end - cur) : 0;
            size_t span = tradSpan(cur, left);
            uint32_t hdr[3];
            if (!span) { g_log("trade: save without a TRAD block (%zu bytes after the world): fresh trade state", left); return true; }
            if (span < 12) {
                g_rawBlock.resize(span); streamRead(stream, g_rawBlock.data(), (int)span); g_readOnly = true; return true;
            }
            memcpy(hdr, cur, 12);
            int cars = 0;
            bool ok = hdr[2] <= span - 12 && hdr[1] >= 1 && hdr[1] <= TRAD_VERSION && parseBlock(cur + 12, hdr[2], hdr[1], &cars);
            std::vector<uint8_t> sink(span);
            streamRead(stream, sink.data(), (int)sink.size());   // consume the block (bounded: it fits)
            g_log("trade: TRAD block v%u (%u bytes) %s, %d caravans", hdr[1], hdr[2], ok ? "loaded" : "preserved read-only", cars);
            if (!ok) { resetState(); g_rawBlock = std::move(sink); g_readOnly = true; }
        } else {
            if (!save) return true;
            std::vector<uint8_t> v;
            { Lock l; v = g_readOnly ? g_rawBlock : blockBytes(); }
            if (v.empty()) { g_log("trade: ERROR: cannot compress save payload"); return false; }
            int r = streamWrite(stream, v.data(), (int)v.size());
            g_log("trade: TRAD block written (%zu bytes, %zu caravans)%s", v.size(), g_car.size(), r ? "" : " - write FAILED");
            return r != 0;
        }
    }
    return true;
}
bool dumpSafe(const char* path) {
    __try { return dumpState(path); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool disabledStream(uint8_t* stream) {
    if (!isSaveKind(stream) && !isScenarioKind(stream)) return true;
    if (*(int*)(stream + 0x68)) return true;
    if (*(int*)(stream + 8) == 1) {
        g_rawBlock.clear();
        if (!isSaveKind(stream)) return true;
        uint8_t* cur = *(uint8_t**)(stream + 0x14), *end = *(uint8_t**)(stream + 0x18);
        size_t span = tradSpan(cur, cur && end > cur ? (size_t)(end - cur) : 0);
        if (span) { g_rawBlock.resize(span); streamRead(stream, g_rawBlock.data(), (int)span); }
    } else if (isSaveKind(stream) && !g_rawBlock.empty()) return streamWrite(stream, g_rawBlock.data(), (int)g_rawBlock.size()) != 0;
    return true;
}
}  // namespace

bool tradeStream(uint8_t* stream) {
    __try { return g_on ? streamImpl(stream) : disabledStream(stream); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_log("trade: ERROR: exception %08lx in the save hook", GetExceptionCode()); return false; }
}

void tradeCommand(const char* line, FILE* out) {
    if (!g_on) { fprintf(out, "trade: disabled (worldmod.ini trade=1)\n"); return; }
    char cmd[32] = {}, arg[MAX_PATH] = {};
    sscanf_s(line, "%*s %31s", cmd, (unsigned)sizeof cmd);
    if (!strcmp(cmd,"hidden")) {
        int count=0;
        for(int id=g_cfg.PT-g_cfg.specials;g_person0 && id<g_cfg.PT;id++)
            if(hiddenTradePerson(g_person0+(size_t)id*0x190)){fprintf(out,"hidden merchant %d\n",id);++count;}
        fprintf(out,"hidden merchants=%d, filtered UI rows=%ld, native filter=%s\n",count,
            InterlockedCompareExchange(&g_hiddenRows,0,0),g_listRowOrig?"installed":"absent");
        fprintf(out,"filtered appointment candidates=%ld\n",InterlockedCompareExchange(&g_hiddenRankCandidates,0,0));
    } else if (!strcmp(cmd, "dump")) {
        char path[MAX_PATH];
        if (sscanf_s(line, "%*s %*s %259[^\r\n]", arg, (unsigned)sizeof arg) == 1) strcpy_s(path, arg);
        else { char d[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, g_s.dir, -1, d, MAX_PATH, nullptr, nullptr); sprintf_s(path, "%s\\worldmod_trade_dump.txt", d); }
        fprintf(out, "trade dump: %s (%s)\n", dumpSafe(path) ? "ok" : "FAILED", path);
    } else if (!strcmp(cmd, "tick")) {                      // advance only the price model n months (no units)
        int n = 1; sscanf_s(line, "%*s %*s %d", &n);
        { Lock l; for (int i = 0; i < n && i < 1200; i++) g_mk.month(today()/30+i); }
        fprintf(out, "trade tick: %d months of price reversion\n", n);
    } else if (!strcmp(cmd, "target")) {                    // target <city> <lo> <hi>: the hex a caravan at lo,hi heads for
        char cs[64] = {}; int lo = 0, hi = 0, tlo = -1, thi = -1;
        int n = sscanf_s(line, "%*s %*s %63s %d %d", cs, (unsigned)sizeof cs, &lo, &hi);
        int c = n == 3 ? findCity(cs) : -1, dlo = 0, dhi = 0;
        if (c >= 0) cityPos(c, &dlo, &dhi);
        bool ok = c >= 0 && hexNear(dlo, dhi, lo, hi, &tlo, &thi);
        fprintf(out, "trade target: city %d at %d,%d -> %s %d,%d\n", c, dlo, dhi, ok ? "hex" : "none", tlo, thi);
    } else if (!strcmp(cmd, "procure") || !strcmp(cmd, "sendmix") || !strcmp(cmd, "sendship") || !strcmp(cmd, "send") || !strcmp(cmd, "ai") || !strcmp(cmd, "panel") || !strcmp(cmd, "buy") || !strcmp(cmd, "sell") || !strcmp(cmd, "go") || !strcmp(cmd, "finish") || !strcmp(cmd, "resupply") || !strcmp(cmd, "route")
#ifdef TRADE_NATIVE_TEST
               || !strcmp(cmd,"native_test")
#endif
    ) {
        auto j = std::make_shared<Job>(); j->line = line; j->done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!j->done) { fprintf(out, "trade: could not create command event\n"); return; }
        { Lock l; g_jobs.push_back(j); }
        DWORD w = WaitForSingleObject(j->done, 60000);
        if (w != WAIT_OBJECT_0) {
            Lock l;
            auto it = std::find(g_jobs.begin(), g_jobs.end(), j);
            if (it != g_jobs.end()) g_jobs.erase(it);
            fprintf(out, "trade %s: timeout; an already running command may still complete\n", cmd);
        } else fprintf(out, "trade %s: %s\n", cmd, j->result.c_str());
    } else fprintf(out, "trade: commands: dump [file] | sendmix <from-id> <to-id> <leader> <good-id qty>... | send <from> <to> <good> <qty> [officer] | buy/sell <voyage> <good> <qty> | go/route <voyage> <city> | finish <voyage> [discard] | resupply <voyage> | ai | tick [months] | target <city> <lo> <hi> | panel <from> <to> <good> <file.bmp>\n");
}

bool tradeInstall(const TradeSetup& s, TraceLogFn log) {
    g_log = log; g_s = s;
    wchar_t ini[MAX_PATH]; swprintf_s(ini, L"%s\\worldmod.ini", s.dir);
    auto I = [&](const wchar_t* k, int d) { return (int)GetPrivateProfileIntW(s.section, k, d, ini); };
    g_cfg.on = I(L"trade", 0);
    if (!g_cfg.on) return false;
    const uint8_t weaponRead[]={0x56,0x8b,0xf1,0x56,0xb9},weaponWrite[]={0x56,0x8b,0x74,0x24,0x08};
    g_militaryReady=!memcmp((void*)0x486950,weaponRead,sizeof weaponRead) && !memcmp((void*)0x486a30,weaponRead,sizeof weaponRead) &&
                    !memcmp((void*)0x4ae430,weaponWrite,sizeof weaponWrite);
    if(!g_militaryReady)log("trade: military procurement disabled: native weapon API signature mismatch");
    if (!I(L"move", 0) || (s.write && !movementReady())) {
        log("trade: ERROR: move=1 and installed movement hooks required for dock/wait constraints"); return false;
    }
    if(s.write && !pathfindReady()) { log("trade: ERROR: caravan movement requires the installed A* adapter"); return false; }
    g_cfg.tempMax = I(L"trade_temp_max", 30); g_cfg.tempKeep = I(L"trade_temp_keep", 30); g_cfg.tmpl = I(L"trade_template", 745);
    g_cfg.ai = I(L"trade_ai", 1); g_cfg.aiHook = I(L"trade_ai_hook", 1);
    g_cfg.aiShips = I(L"trade_ai_ships", 1); g_cfg.aiShipRange = I(L"trade_ai_ship_range", 200);
    g_cfg.aiMonth = I(L"trade_ai_month", 6); g_cfg.aiRange = I(L"trade_ai_range", 80); g_cfg.aiMinGold = I(L"trade_ai_min_gold", 6000);
    g_cfg.aiInvest = I(L"trade_ai_invest", 4000); g_cfg.aiReserve = I(L"trade_ai_reserve", 2000); g_cfg.aiMargin = I(L"trade_ai_margin", 25);
    g_cfg.tax = I(L"trade_tax", 10); g_cfg.taxHub = I(L"trade_tax_hub", 5); g_cfg.relTax = I(L"trade_rel_tax", 40);
    g_cfg.rob = I(L"trade_rob", 15); g_cfg.robLoss = I(L"trade_rob_loss", 50); g_cfg.loot = I(L"trade_loot", 60);
    g_cfg.spoilMin = I(L"trade_spoil_min", 5); g_cfg.spoilMax = I(L"trade_spoil_max", 49);
    g_cfg.escort = I(L"trade_escort", 500); g_cfg.food = I(L"trade_food", 3000); g_cfg.arrive = I(L"trade_arrive", 3);
    g_cfg.maxTurns = I(L"trade_max_turns", 90); g_cfg.playerAuto = I(L"trade_player_auto", 1);
    g_cfg.ui = I(L"trade_ui", 1); g_cfg.uiAi = I(L"trade_ui_ai_caravans", 1); g_cfg.monsoonAll = I(L"trade_monsoon_all", 1);
    g_cfg.keys = I(L"trade_keys", 1); g_cfg.aiOfficers = I(L"trade_ai_officers", 1); g_cfg.aiOfficerMin = I(L"trade_ai_officer_min", 4);
    g_cfg.specials=I(L"special_persons",100);
    g_cfg.PT = personlayout::total(I(L"persons",850),g_cfg.specials);
    Params p;
    p.impact = I(L"trade_impact", 50); p.spread = I(L"trade_spread", 10); p.revert = I(L"trade_revert", 10);
    p.revertHub = I(L"trade_revert_hub", 25); p.capLand = I(L"trade_cap_land", 20); p.capSea = I(L"trade_cap_sea", 60);
    p.fee = I(L"trade_fee", 100);
    p.transitTax=I(L"trade_transit_tax",2);p.transitHub=I(L"trade_transit_hub",1);p.transitCap=I(L"trade_transit_cap",5);
    p.legFee = I(L"trade_leg_fee", 30);
    p.monsoon = I(L"trade_monsoon", 1); p.monWith = I(L"trade_monsoon_with", 70); p.monAgainst = I(L"trade_monsoon_against", 140);
    p.monStorm = I(L"trade_monsoon_storm", 125); p.stormRisk = I(L"trade_storm_risk", 10); p.stormLoss = I(L"trade_storm_loss", 30);
    p.events = I(L"trade_events", 1); p.eventChance = I(L"trade_event_chance", 15); p.eventMax = I(L"trade_event_max", 4);
    p.info = I(L"trade_info", 1); p.infoRange = I(L"trade_info_range", 40); p.infoMonths = I(L"trade_info_months", 12);
    {   // phase 4: merchant levels "500,1500,3500,7000,12000"
        wchar_t lv[128]; GetPrivateProfileStringW(s.section, L"trade_merchant_exp", L"500,1500,3500,7000,12000", lv, 128, ini);
        int v[5], n = swscanf_s(lv, L"%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4]);
        bool ok = n == 5;
        for (int i = 0; ok && i < 5; i++) ok = v[i] > 0 && (i == 0 || v[i] > v[i - 1]);
        if (ok) for (int i = 0; i < 5; i++) p.merchExp[i] = v[i];
        else { log("trade: ERROR: trade_merchant_exp must be 5 rising numbers, trade not installed"); return false; }
    }
    p.merchExpTrip = I(L"trade_merchant_exp_trip", 100); p.merchExpDiv = I(L"trade_merchant_exp_div", 20);
    p.merchPol = I(L"trade_merchant_pol", 80); p.merchMaxLevel = I(L"trade_merchant_max_level", 7);
    p.merchPP = I(L"trade_merchant_price", 8); p.merchCap = I(L"trade_merchant_load", 5);
    p.merchTax = I(L"trade_merchant_tax", 8); p.merchRob = I(L"trade_merchant_rob", 12);
    if (p.transitTax<0 || p.transitTax>100 || p.transitHub<0 || p.transitHub>100 || p.transitCap<0 || p.transitCap>100 || g_cfg.food < 1 || g_cfg.food > 500000 || p.fee < 0 || p.fee > 100000 || p.legFee < 0 || p.legFee > 100000 || p.capLand > 10000 || p.capSea > 30000 ||
        g_cfg.tax < 0 || g_cfg.tax > 100 || g_cfg.taxHub < 0 || g_cfg.taxHub > 100 || g_cfg.loot < 0 || g_cfg.loot > 100 || g_cfg.spoilMin < 0 || g_cfg.spoilMax < g_cfg.spoilMin || g_cfg.spoilMax > 49 ||
        g_cfg.rob < 0 || g_cfg.rob > 100 || g_cfg.robLoss < 0 || g_cfg.robLoss > 100 ||
        g_cfg.tempMax < 0 || g_cfg.tempMax > g_cfg.specials || g_cfg.tempKeep < 0 || g_cfg.tempKeep > g_cfg.specials || g_cfg.escort < 1 || g_cfg.escort > 60000 ||
        p.capLand < 1 || p.capSea < 1 || p.impact < 0 || p.impact > 1000 || p.spread < 0 || p.spread > 100 || p.revert < 0 || p.revert > 100 ||
        p.revertHub < 0 || p.revertHub > 100 || g_cfg.aiRange < 1 || g_cfg.aiRange > 2000 || g_cfg.aiShipRange < 1 || g_cfg.aiShipRange > 2000 || g_cfg.arrive < 1 || g_cfg.arrive > 10 ||
        p.monWith < 20 || p.monWith > 500 || p.monAgainst < 20 || p.monAgainst > 500 || p.monStorm < 20 || p.monStorm > 500 ||
        p.stormRisk < 0 || p.stormRisk > 100 || p.stormLoss < 0 || p.stormLoss > 100 || p.eventChance < 0 || p.eventChance > 100 ||
        p.eventMax < 0 || p.eventMax > 32 || p.infoRange < 0 || p.infoRange > 2000 || p.infoMonths < 1 || p.infoMonths > 60 ||
        !personlayout::validSpecials(g_cfg.specials) || g_cfg.PT < 1100 || g_cfg.PT > 40000 || p.merchExpTrip < 0 || p.merchExpDiv < 1 || p.merchMaxLevel < 0 || p.merchMaxLevel > 20 ||
        p.merchPP < 0 || p.merchPP > 100 || p.merchCap < 0 || p.merchCap > 100 || p.merchTax < 0 || p.merchTax > 20 ||
        p.merchRob < 0 || p.merchRob > 20 || g_cfg.aiOfficerMin < 1 || g_cfg.aiOfficerMin > 50) {
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
    static const uint8_t listRowHead[] = { 0x55,0x56,0x8b,0xf1,0x8b,0x06 };              // 0x4e6340 native UI row insertion
    static const uint8_t rankPeopleCall[] = { 0xe8,0x6c,0x31,0xed,0xff };                // 0x5fc30f call 0x4cf480
    struct Site { uint32_t at; const uint8_t* b; size_t n; const char* what; } sites[] = {
        { 0x58051e, turnCall, sizeof turnCall, "turn end" }, { 0x5def90, planHead, sizeof planHead, "AI unit planning" },
        { 0x4bfa30, annHead, sizeof annHead, "unit annihilated" }, { 0x57ff80, pumpHead, sizeof pumpHead, "command pump" },
        { 0x4e6340,listRowHead,sizeof listRowHead,"officer list filter" },
        { 0x5fc30f,rankPeopleCall,sizeof rankPeopleCall,"appointment candidates" } };
    int bad = 0;
    for (auto& t : sites)
        if (memcmp((void*)(uintptr_t)t.at, t.b, t.n)) { log("trade: ERROR: unexpected code at %08x (%s)", t.at, t.what); bad++; }
    if (bad) { log("trade: not installed"); return false; }
    if (!s.write) {
        log("trade: verify_only: hook sites ok, %d cities, %d goods (trade.bin v%d: %zu monsoon seas, %zu events); panel: %s", g_data.C, g_data.G,
            g_data.version, g_data.seas.size(), g_data.events.size(), !g_cfg.ui ? "off" : worldMidmapInstalled() ? "world 中地圖 present" : "needs the world 中地圖 (not installed in verify_only)");
        return true;
    }

    g_hiddenTempForces.assign(g_cfg.specials,0);
    InitializeCriticalSection(&g_cs);
    g_mk.d = &g_data; g_mk.p = p; g_mk.reset();
    g_inRange = citiesInRange(g_data, g_cfg.aiRange);
    g_shipRange = citiesInRange(g_data, g_cfg.aiShipRange);
    if (g_data.version < 2) { g_mk.p.monsoon = 0; g_mk.p.events = 0; }      // a phase 1 trade.bin: no seas, no events
    g_infoNear = citiesInRange(g_data, g_mk.p.infoRange);
    resetPhase3();
    g_unitDir.assign(g_s.U, 0);
    g_pausedLeader.assign(g_s.U, 0);
    g_tradeLeader.assign(g_s.U, 0);
    g_shipLeader.assign(g_s.U,0);
    g_shipQuay.assign(g_s.U,-1);
    movementSetTradeUnits(tradeMovingUnit);
    movementSetStepAllowed(shipStepAllowed);
    g_planOrig = g_cfg.aiHook ? (PlanFn)trampoline(0x5def90, planHead, sizeof planHead) : nullptr;
    g_annOrig = (AnnihilateFn)trampoline(0x4bfa30, annHead, sizeof annHead);
    g_pumpOrig = (PumpFn)trampoline(0x57ff80, pumpHead, sizeof pumpHead);
    g_listRowOrig = (ListRowFn)trampoline(0x4e6340,listRowHead,sizeof listRowHead);
    if ((g_cfg.aiHook && !g_planOrig) || !g_annOrig || !g_pumpOrig || !g_listRowOrig) { log("trade: ERROR: trampolines failed, not installed"); return false; }
    {
        DWORD old; VirtualProtect((void*)0x58051f, 4, PAGE_EXECUTE_READWRITE, &old);
        *(int32_t*)0x58051f = (int32_t)((uintptr_t)turnHook - (0x58051e + 5));
        VirtualProtect((void*)0x58051f, 4, old, &old);
    }
    if (g_planOrig) jmpTo(0x5def90, (void*)planHook);
    jmpTo(0x4bfa30, (void*)annihilateHook);
    jmpTo(0x57ff80, (void*)pumpHook);
    g_person0=personRec(WORLD,0);
    jmpTo(0x4e6340,(void*)listRowHook);
    FlushInstructionCache(GetCurrentProcess(),(void*)0x4e6340,6);
    log("trade: temporary merchant filter installed at native UI row insertion");
    {
        DWORD old;VirtualProtect((void*)0x5fc310,4,PAGE_EXECUTE_READWRITE,&old);
        *(int32_t*)0x5fc310=(int32_t)((uintptr_t)rankPeopleHook-0x5fc314);
        VirtualProtect((void*)0x5fc310,4,old,&old);
        FlushInstructionCache(GetCurrentProcess(),(void*)0x5fc30f,5);
    }
    log("trade: temporary merchant filter installed at appointment candidate source");
    // monsoon on sea steps (movement.cpp, move=1): caravans (phase 3), every ship by step direction (phase 4)
    g_unit0 = unitRec(WORLD, 0);
    movementSetAllowed(movementAllowed);
    if (g_mk.p.monsoon && !g_data.seas.empty()) movementSetSeaCost(seaCostHook, g_mk.p.monWith);
    // phase 2: the panel on the world 中地圖
    if (g_cfg.ui) {
        tradeImagesInit(s.dataDir,log);
        if (worldMidmapInstalled()) {
            g_uiDownPrev = (UiMouseFn)patchSlot(MAPUI_VT + 0x90, (void*)uiDownHook);
            g_uiUpPrev = (UiMouseFn)patchSlot(MAPUI_VT + 0x94, (void*)uiUpHook);
            g_uiWheelPrev = (UiWheelFn)patchSlot(MAPUI_VT + 0xa8, (void*)uiWheelHook);
            midmapSetOverlay(overlayHook);
            g_uiOn = true;
            installCityMenu();
            // phase 4: keys (WM_KEYDOWN, vt +0xb8) of the MapUI and of the main map's midmap dialog (vt 0x858598)
            const uint32_t DLG_VT = 0x858598;
            if (g_cfg.keys && *(uint32_t*)(uintptr_t)(MAPUI_VT + 0xb8) == 0x4ed9d0 && *(uint32_t*)(uintptr_t)(DLG_VT + 0xb8) == 0x4ed9d0) {
                g_keyMapPrev = (UiKeyFn)patchSlot(MAPUI_VT + 0xb8, (void*)keyMapHook);
                g_keyDlgPrev = (UiKeyFn)patchSlot(DLG_VT + 0xb8, (void*)keyDlgHook);
            } else if (g_cfg.keys) log("trade: panel keys NOT installed: unexpected key handlers (vt +0xb8)");
        } else log("trade: midmap panel NOT installed: the world 中地圖 is not active (world_terrain / bases)");
    }
    g_routeDanger.resize(s.U);
    g_on = true;
    if(g_cfg.ui)tradeShipUiInstall(s.dataDir,[](const uint8_t* unit) {
        return g_on && tradeMovingUnit(unit) ? (merchantShip(unit)?2:1) : 0;
    },[](const uint8_t* unit,TradeShipUiDetails& out){return readUnitTradeDetails(unit,out);},openUnitTradeDetails,log);
    if(GetPrivateProfileIntW(s.section,L"trade_ship_visual",1,ini))
        tradeShipVisualInstall(s.dataDir,[](int id) {
            return g_on && g_unit0 && id>=0 && id<g_s.U && merchantShip(g_unit0+(size_t)id*0xf4);
        },log);
    int hubs = 0; for (int c = 0; c < g_data.C; c++) hubs += g_data.hub(c);
    log("trade: installed: %d cities (%d hubs), %d goods; temp caravans <= %d keeping %d special slots; AI %s (planning hook %s), range %d",
        g_data.C, hubs, g_data.G, g_cfg.tempMax, g_cfg.tempKeep, g_cfg.ai ? "on" : "off", g_planOrig ? "on" : "off", g_cfg.aiRange);
    log("trade: phase 2 panel %s; phase 3: monsoon %s (%zu seas, caravans%s, %d/%d/%d%%), events %s (%zu kinds, %d%%/month, <= %d), information delay %s (range %d, %d months)",
        g_uiOn ? "on (MapUI slots +0x90 +0x94 +0xa8 chained, overlay)" : "off", g_mk.p.monsoon ? "on" : "off", g_data.seas.size(),
        g_cfg.monsoonAll ? " and all ships by step direction" : " only", g_mk.p.monWith, g_mk.p.monAgainst, g_mk.p.monStorm, g_mk.p.events ? "on" : "off",
        g_data.events.size(), g_mk.p.eventChance, g_mk.p.eventMax, g_mk.p.info ? "on" : "off", g_mk.p.infoRange, g_mk.p.infoMonths);
    log("trade: phase 4: 自動行軍 %s; panel keys %s; merchants: %zu known (trade.bin v%d), levels at %d/%d/%d/%d/%d, AI merchant officers %s",
        g_cfg.playerAuto ? "on" : "off", g_keyMapPrev ? "on (vt +0xb8 of the MapUI and the midmap dialog)" : "off", g_data.merchants.size(),
        g_data.version, g_mk.p.merchExp[0], g_mk.p.merchExp[1], g_mk.p.merchExp[2], g_mk.p.merchExp[3], g_mk.p.merchExp[4],
        g_cfg.aiOfficers ? "on" : "off");
    return true;
}
