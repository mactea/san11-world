// M13 trade, phase 2: the 「貿易」 panel of the 中地圖 (see trade_panel.h, M13_TRADE.md "第二期").
// Layout, hit testing, the numbers of a planned trip (the formulas of trade.cpp) and GDI rendering into a DIB.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include "trade_panel.h"

using namespace trade;

namespace tradeui {

// ---------------------------------------------------------------- text
std::wstring wide(const std::string& s) {
    if (s.empty()) return L"";
    wchar_t buf[256];
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), buf, 255);
    return std::wstring(buf, n > 0 ? n : 0);
}
std::wstring fmtGold(int64_t v, bool sign) {
    wchar_t b[32];
    swprintf_s(b, sign && v > 0 ? L"+%lld" : L"%lld", (long long)v);
    return b;
}
static std::wstring cityW(const Model& m, int c) { return c >= 0 && c < m.d->C ? wide(m.d->cityName[c]) : L"—"; }
static std::wstring goodW(const Model& m, int g) { return g >= 0 && g < m.d->G ? wide(m.d->goodName[g]) : L"—"; }

// ---------------------------------------------------------------- layout (panel-local pixels)
struct Rc { int x, y, w, h; bool in(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };
static const int TAB_W = 44, TAB_H = 20, LINE = 16;
static Rc tabRect(int i) { return { 2 + i * (TAB_W + 2), 1, TAB_W, TAB_H }; }
static const Rc CLOSE = { PANEL_W - 24, 1, 22, TAB_H };
static const int INFO_Y = 26;                         // three lines: origin, destination, information / monsoon
static const int TABLE_Y = 76, ROW_Y = 92;            // market table: header, first row
static const int DETAIL_Y = 92 + 14 * LINE + 8;
static Rc goodBtn(int i) { return { 2 + (i % 3) * 76, 92 + (i / 3) * 20, 74, 18 }; }
static const int QTY_LABEL_Y = 198, QTY_Y = 214;
static const int QTY_STEP[6] = { -100, -10, -1, 1, 10, 100 };
static Rc qtyBtn(int i) { return { i < 3 ? 2 + i * 32 : 134 + (i - 3) * 32, QTY_Y, 30, 20 }; }
static const Rc QTY_BOX = { 98, QTY_Y, 34, 20 };
static const Rc QTY_ROW = { 0, QTY_Y - 2, PANEL_W, 24 };
static const int LEAD_LABEL_Y = 242, LEAD_Y = 258, LEAD_ROWS = 6, LEAD_H = 18;
static Rc leadRow(int i) { return { 2, LEAD_Y + i * LEAD_H, PANEL_W - 4, LEAD_H - 1 }; }
static const Rc LEAD_LIST = { 0, LEAD_Y, PANEL_W, LEAD_ROWS * LEAD_H };
static const int EST_Y = 372;
static const Rc SEND = { 65, 446, 100, 24 };
static const int MSG_Y = 476;
static const int CAR_Y = 26, CAR_H = 50, CAR_ROWS = 7, DONE_Y = 394;
static const Rc CAR_LIST = { 0, CAR_Y, PANEL_W, CAR_ROWS * CAR_H };
static Rc autoBtn(int row) { return { PANEL_W - 44, CAR_Y + row * CAR_H + 17, 40, 15 }; }   // 自動 / 手動 of a caravan row

bool inPanel(const State& s, int x, int y) { return s.on && x >= PANEL_X && x < MAP_W && y >= 0 && y < MAP_H; }
bool onToggle(const State& s, int x, int y) {
    return !s.on && x >= TOGGLE_X && y >= TOGGLE_Y && x < TOGGLE_X + TOGGLE_W && y < TOGGLE_Y + TOGGLE_H;
}

// goods the origin sells (the form's buttons), in data order
static std::vector<int> originGoods(const Model& m, int origin) {
    std::vector<int> v;
    if (origin < 0) return v;
    for (int g = 0; g < m.d->G; g++) if (m.d->canBuy(origin, g)) v.push_back(g);
    return v;
}
// leader rows: -1 (無) then the officers
static int leaderRows(const Model& m) { return 1 + (int)m.officers.size(); }
static int leaderAt(const Model& m, int row) { return row <= 0 ? -1 : row - 1 < (int)m.officers.size() ? m.officers[row - 1].id : -2; }
static const Officer* officer(const Model& m, int id) {
    for (auto& o : m.officers) if (o.id == id) return &o;
    return nullptr;
}
static std::vector<const CaravanView*> playerCaravans(const Model& m) {
    std::vector<const CaravanView*> v;
    for (auto& k : m.caravans) if (k.player) v.push_back(&k);
    return v;
}

// ---------------------------------------------------------------- numbers
static int nearestCity(const Data& d, int lo, int hi, int maxDist) {
    int best = -1, bd = maxDist + 1;
    for (int c = 0; c < d.C; c++) {
        int x = hexDist(lo, hi, d.lo[c], d.hi[c]);
        if (x < bd) { bd = x; best = c; }
    }
    return best;
}

Plan plan(const Model& m, int origin, int dest, int good, int qty, int leader, bool withTransit) {
    Plan p;
    const Data& d = *m.d;
    if (origin < 0 || origin >= d.C) { p.error = L"先在地圖上點選起點城市"; return p; }
    if (dest < 0 || dest >= d.C) { p.error = L"再點選終點城市"; return p; }
    if (origin == dest) { p.error = L"起點與終點相同"; return p; }
    if (good < 0 || good >= d.G) { p.error = L"選擇商品"; return p; }
    int pol = TEMP_POL, intel = TEMP_INT, level = 0;
    if (leader >= 0) {
        const Officer* o = officer(m, leader);
        if (o) { pol = o->pol; intel = o->intel; level = o->level; }
    }
    int capPct;
    leaderFactors(pol, intel, &p.pricePermille, &capPct);
    MerchantBonus mb = merchantBonus(m.live.p, level);           // phase 4 (trade.cpp applies the same)
    p.pricePermille += mb.pp; capPct += mb.capPct; p.level = level;
    p.cap = capacity(d, m.live.p, origin, dest, capPct);
    p.qty = std::max(0, std::min(qty, p.cap));
    p.dist = hexDist(d.lo[origin], d.hi[origin], d.lo[dest], d.hi[dest]);
    p.sea = d.port(origin) && d.port(dest);
    p.timePermille = routeTimePermille(d, m.live.p, origin, dest, m.month % 12);
    p.stormy = routeStormy(d, m.live.p, origin, dest, m.month % 12);
    int speed = p.sea ? m.cfg.speedSea : m.cfg.speedLand;
    p.eta = (int)((int64_t)p.dist * p.timePermille / 1000 + speed - 1) / std::max(1, speed);
    if (p.qty > 0) {
        p.goods = purchase(m.live, origin, good, p.qty, p.pricePermille);
        p.cost = p.goods + m.live.p.fee;
    }
    p.known = !m.known.empty() ? m.known[dest] != 0 : true;
    if (p.qty > 0 && p.known) {
        p.revenue = saleQuote(m.seen, dest, good, p.qty, p.pricePermille);
        bool free = m.stance[dest] == 2;
        p.duty = free ? 0 : taxOf(p.revenue, taxPct(d, dest, m.cfg.tax, m.cfg.taxHub), mb.taxPct);
        if (withTransit) {
            // the cities a caravan passes (trade.cpp: the nearest city within 30 hexes of its position, each once)
            int steps = std::max(1, p.dist / 3), last = origin;
            for (int i = 1; i < steps; i++) {
                int lo = d.lo[origin] + (d.lo[dest] - d.lo[origin]) * i / steps;
                int hi = d.hi[origin] + (d.hi[dest] - d.hi[origin]) * i / steps;
                int c = nearestCity(d, lo, hi, 30);
                if (c < 0 || c == last) continue;
                last = c;
                if (c == origin || c == dest || m.owner[c] < 0 || m.owner[c] == m.force) continue;
                if (m.stance[c] == 1) {
                    int64_t value = (int64_t)p.qty * m.seen.sellPrice(c, good);
                    p.transit += (int)taxOf(value, taxPct(d, c, m.cfg.tax, m.cfg.taxHub), mb.taxPct);
                    p.transitCities++;
                } else if (m.stance[c] == 0) p.hostileCities++;
            }
        }
        p.net = std::max<int64_t>(0, p.revenue - p.duty - p.transit);
        p.profit = p.net - p.cost;
    }
    // what 「出發」 would say (trade.cpp createCaravan checks the same in the game)
    if (m.owner[origin] != m.force || m.force < 0) p.error = L"起點不是我方城市";
    else if (!d.canBuy(origin, good)) p.error = cityW(m, origin) + L"不賣" + goodW(m, good);
    else if (qty <= 0 || p.qty <= 0) p.error = L"數量為零";
    else if (leader >= 0 && !officer(m, leader)) p.error = L"統領不在起點城市";
    else if (leader < 0 && m.tempLive >= m.cfg.tempMax) p.error = L"臨時商隊已達上限";
    else if (leader < 0 && m.freeSpecials - 1 < m.cfg.tempKeep) p.error = L"特殊武將空位不足";
    else if (m.gold[origin] >= 0 && p.cost > m.gold[origin]) p.error = L"金不足（需 " + fmtGold(p.cost) + L"，城中 " + fmtGold(m.gold[origin]) + L"）";
    else p.ok = true;
    return p;
}

bool cityProfit(const Model& m, const State& s, int city, int64_t* profit, int64_t* cost) {
    if (s.origin < 0 || s.good < 0 || city == s.origin || m.known.empty() || !m.known[city]) return false;
    Plan p = plan(m, s.origin, city, s.good, s.qty, s.leader, false);
    if (p.qty <= 0) return false;
    *profit = p.profit; *cost = p.cost;
    return true;
}

int cityAt(const Model& m, const Projector& proj, int x, int y, int radius) {
    int best = -1; float bd = (float)radius * radius + 0.01f;
    for (int c = 0; c < m.d->C; c++) {
        float px, py;
        if (!proj(m.d->lo[c], m.d->hi[c], &px, &py)) continue;
        float dd = (px - x) * (px - x) + (py - y) * (py - y);
        if (dd < bd) { bd = dd; best = c; }
    }
    return best;
}

// ---------------------------------------------------------------- input
static int clampQty(const Model& m, const State& s, int q) {
    int cap = 1;
    if (s.origin >= 0 && s.dest >= 0 && s.origin != s.dest && s.good >= 0) cap = std::max(1, plan(m, s.origin, s.dest, s.good, 1, s.leader, false).cap);
    else cap = std::max(m.live.p.capLand, m.live.p.capSea) * 2;          // no route yet: the largest load
    return std::max(1, std::min(q, cap));
}

void validate(Model& m, State& s) {
    if (s.origin >= m.d->C) s.origin = -1;
    if (s.dest >= m.d->C || s.dest == s.origin) s.dest = -1;
    if (s.good >= m.d->G) s.good = -1;
    if (s.leader >= 0 && !officer(m, s.leader)) s.leader = -1;
    s.leaderScroll = std::max(0, std::min(s.leaderScroll, leaderRows(m) - LEAD_ROWS));
    s.carScroll = std::max(0, std::min(s.carScroll, (int)playerCaravans(m).size() - CAR_ROWS));
    s.qty = clampQty(m, s, s.qty);
    if (s.sendPending && !m.lastResult.empty()) {
        s.sendPending = false;
        s.msg = m.lastResult; s.msgError = m.lastResult.compare(0, 2, L"成功") != 0;
    }
}

void selectCity(Model& m, State& s, int city) {
    if (city < 0) return;
    if (s.origin < 0 || s.dest >= 0) { s.origin = city; s.dest = -1; s.leader = -1; s.leaderScroll = 0; }
    else if (city == s.origin) s.origin = -1;
    else s.dest = city;
    if (s.origin >= 0 && s.good >= 0 && s.tab == 1 && !m.d->canBuy(s.origin, s.good)) s.good = -1;
    s.msg.clear();
}

Result click(Model& m, State& s, const Projector& proj, int x, int y) {
    Result r;
    if (onToggle(s, x, y)) { s.on = true; r.consumed = true; r.repaint = REPAINT_MAP | REPAINT_LAYOUT; return r; }
    if (!s.on) return r;
    if (!inPanel(s, x, y)) {                                    // on the map: a city near the click
        int c = cityAt(m, proj, x, y, 9);
        if (c < 0) return r;                                    // elsewhere: Koei's click (camera) and dragging
        selectCity(m, s, c);
        r.consumed = true; r.repaint = REPAINT_MAP;
        return r;
    }
    r.consumed = true;
    int px = x - PANEL_X, py = y;
    if (CLOSE.in(px, py)) { s.on = false; r.repaint = REPAINT_MAP | REPAINT_LAYOUT; return r; }
    for (int i = 0; i < 4; i++) if (tabRect(i).in(px, py)) { if (s.tab != i) { s.tab = i; r.repaint = REPAINT_PANEL; } return r; }
    if (s.tab == 0) {
        for (int g = 0; g < m.d->G; g++)
            if (py >= ROW_Y + g * LINE && py < ROW_Y + (g + 1) * LINE) { s.good = s.good == g ? -1 : g; r.repaint = REPAINT_MAP; return r; }
    } else if (s.tab == 1) {
        auto goods = originGoods(m, s.origin);
        for (size_t i = 0; i < goods.size() && i < 15; i++)
            if (goodBtn((int)i).in(px, py)) { s.good = goods[i]; s.qty = clampQty(m, s, s.qty); r.repaint = REPAINT_MAP; return r; }
        for (int i = 0; i < 6; i++)
            if (qtyBtn(i).in(px, py)) { s.qty = clampQty(m, s, s.qty + QTY_STEP[i]); r.repaint = REPAINT_MAP; return r; }
        for (int i = 0; i < LEAD_ROWS; i++)
            if (leadRow(i).in(px, py)) {
                int id = leaderAt(m, s.leaderScroll + i);
                if (id != -2) { s.leader = id; s.qty = clampQty(m, s, s.qty); r.repaint = REPAINT_PANEL; }
                return r;
            }
        if (SEND.in(px, py)) {
            if (s.sendPending) { s.msg = L"上一支商隊的命令還在等待執行"; s.msgError = true; r.repaint = REPAINT_PANEL; return r; }
            Plan p = plan(m, s.origin, s.dest, s.good, s.qty, s.leader);
            if (!p.ok) { s.msg = p.error; s.msgError = true; r.repaint = REPAINT_PANEL; return r; }
            r.send = true;
            r.req.origin = s.origin; r.req.dest = s.dest; r.req.good = s.good; r.req.qty = p.qty; r.req.leader = s.leader;
            s.sendPending = true; m.lastResult.clear();
            s.msg = L"已下令：遊戲等待指令時出發（關閉中地圖後）"; s.msgError = false;
            r.repaint = REPAINT_PANEL;
            return r;
        }
    } else if (s.tab == 2) {
        auto cars = playerCaravans(m);
        for (int i = 0; i < CAR_ROWS; i++) {
            int k = s.carScroll + i;
            if (k >= (int)cars.size()) break;
            if (autoBtn(i).in(px, py)) {                          // phase 4: 自動行軍 on / off for this caravan
                s.carSel = k;
                r.toggleUnit = cars[k]->unit; r.toggleAuto = !cars[k]->autoMarch;
                r.repaint = REPAINT_PANEL; return r;
            }
            if (py >= CAR_Y + i * CAR_H && py < CAR_Y + (i + 1) * CAR_H) {
                s.carSel = k;
                s.origin = cars[k]->origin; s.dest = cars[k]->dest; s.good = cars[k]->good;   // show its route on the map
                r.repaint = REPAINT_MAP; return r;
            }
        }
    }
    return r;
}

// ---------------------------------------------------------------- keyboard (phase 4)
static void showLeaderRow(const Model& m, State& s, int row) {
    if (row < s.leaderScroll) s.leaderScroll = row;
    if (row >= s.leaderScroll + LEAD_ROWS) s.leaderScroll = row - LEAD_ROWS + 1;
    s.leaderScroll = std::max(0, std::min(s.leaderScroll, leaderRows(m) - LEAD_ROWS));
}
Result key(Model& m, State& s, int vk, bool shift) {
    Result r;
    if (!s.on || !m.d) return r;                              // panel closed: Koei's keys
    r.consumed = true;
    const bool digit = (vk >= '0' && vk <= '9') || (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9);
    const int dval = vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9 ? vk - VK_NUMPAD0 : vk - '0';
    if (!(digit && s.tab == 1)) s.qtyTyping = false;
    if (vk != VK_RETURN && vk != VK_ESCAPE && s.confirm) { s.confirm = false; s.msg.clear(); }
    auto changeQty = [&](int q) { q = clampQty(m, s, q); if (q != s.qty) { s.qty = q; r.repaint = REPAINT_MAP; } else r.repaint = REPAINT_PANEL; };
    switch (vk) {
    case VK_ESCAPE:
        if (s.confirm) { s.confirm = false; s.msg = L"已取消"; s.msgError = false; r.repaint = REPAINT_PANEL; }
        else { s.on = false; r.repaint = REPAINT_MAP | REPAINT_LAYOUT; }
        return r;
    case VK_TAB:
        s.tab = (s.tab + (shift ? 3 : 1)) % 4; r.repaint = REPAINT_PANEL; return r;
    case VK_ADD: case VK_OEM_PLUS:
        if (s.tab <= 1) changeQty(s.qty + (shift ? 10 : 1));
        return r;
    case VK_SUBTRACT: case VK_OEM_MINUS:
        if (s.tab <= 1) changeQty(s.qty - (shift ? 10 : 1));
        return r;
    case VK_BACK:
        if (s.tab == 1) { s.qtyTyping = true; changeQty(std::max(1, s.qty / 10)); }
        return r;
    case VK_RETURN:
        if (s.tab == 0) { s.tab = 1; r.repaint = REPAINT_PANEL; return r; }
        if (s.tab != 1) return r;
        if (s.sendPending) { s.msg = L"上一支商隊的命令還在等待執行"; s.msgError = true; r.repaint = REPAINT_PANEL; return r; }
        {
            Plan p = plan(m, s.origin, s.dest, s.good, s.qty, s.leader);
            if (!p.ok) { s.confirm = false; s.msg = p.error; s.msgError = true; r.repaint = REPAINT_PANEL; return r; }
            if (!s.confirm) {
                s.confirm = true; s.msgError = false;
                s.msg = L"再按 Enter 出發（Esc 取消）：" + cityW(m, s.origin) + L"→" + cityW(m, s.dest) + L" " + goodW(m, s.good) + L" " +
                        fmtGold(p.qty) + L"擔，成本 " + fmtGold(p.cost);
                r.repaint = REPAINT_PANEL; return r;
            }
            s.confirm = false;
            r.send = true;
            r.req.origin = s.origin; r.req.dest = s.dest; r.req.good = s.good; r.req.qty = p.qty; r.req.leader = s.leader;
            s.sendPending = true; m.lastResult.clear();
            s.msg = L"已下令：遊戲等待指令時出發（關閉中地圖後）"; s.msgError = false;
            r.repaint = REPAINT_PANEL;
        }
        return r;
    }
    if (digit) {
        if (s.tab == 1) {                                          // type the quantity
            int q = s.qtyTyping ? s.qty * 10 + dval : dval;
            s.qtyTyping = true;
            changeQty(q > 9999 ? 9999 : q);
        } else if (dval >= 1 && dval <= 4) { s.tab = dval - 1; r.repaint = REPAINT_PANEL; }
        return r;
    }
    int step = 0;
    if (vk == VK_UP) step = -1; else if (vk == VK_DOWN) step = 1;
    else if (vk == VK_PRIOR) step = -7; else if (vk == VK_NEXT) step = 7;
    if (s.tab == 1 && (vk == VK_LEFT || vk == VK_RIGHT)) {         // the goods of the origin
        auto goods = originGoods(m, s.origin);
        if (goods.empty()) return r;
        int i = (int)(std::find(goods.begin(), goods.end(), s.good) - goods.begin());
        i = i >= (int)goods.size() ? 0 : (i + (vk == VK_RIGHT ? 1 : -1) + (int)goods.size()) % (int)goods.size();
        s.good = goods[i]; s.qty = clampQty(m, s, s.qty); r.repaint = REPAINT_MAP;
        return r;
    }
    if (step) {
        if (s.tab == 0) {
            int g = s.good < 0 ? 0 : std::max(0, std::min(m.d->G - 1, s.good + step));
            if (g != s.good) { s.good = g; r.repaint = REPAINT_MAP; }
        } else if (s.tab == 1) {
            int rows = leaderRows(m), row = 0;
            for (int i = 0; i < rows; i++) if (leaderAt(m, i) == s.leader) row = i;
            row = std::max(0, std::min(rows - 1, row + (step == 7 || step == -7 ? step * LEAD_ROWS / 7 : step)));
            s.leader = leaderAt(m, row); s.qty = clampQty(m, s, s.qty);
            showLeaderRow(m, s, row);
            r.repaint = REPAINT_MAP;
        } else if (s.tab == 2) {
            auto cars = playerCaravans(m);
            if (cars.empty()) return r;
            s.carSel = std::max(0, std::min((int)cars.size() - 1, s.carSel + step));
            if (s.carSel < s.carScroll) s.carScroll = s.carSel;
            if (s.carSel >= s.carScroll + CAR_ROWS) s.carScroll = s.carSel - CAR_ROWS + 1;
            const CaravanView& c = *cars[s.carSel];
            s.origin = c.origin; s.dest = c.dest; s.good = c.good;
            r.repaint = REPAINT_MAP;
        }
        return r;
    }
    if (vk == 'A' && s.tab == 2) {
        auto cars = playerCaravans(m);
        if (s.carSel >= 0 && s.carSel < (int)cars.size()) { r.toggleUnit = cars[s.carSel]->unit; r.toggleAuto = !cars[s.carSel]->autoMarch; r.repaint = REPAINT_PANEL; }
        return r;
    }
    r.consumed = false;                                            // other keys: Koei's
    return r;
}

Result wheel(Model& m, State& s, int x, int y, int delta, bool shift) {
    Result r;
    if (!inPanel(s, x, y)) return r;                          // the map zooms (midmap)
    r.consumed = true;
    int px = x - PANEL_X, py = y, notches = delta / 120;
    if (!notches) notches = delta > 0 ? 1 : -1;
    if (s.tab == 1 && QTY_ROW.in(px, py)) {
        int q = clampQty(m, s, s.qty + notches * (shift ? 10 : 1));
        if (q != s.qty) { s.qty = q; r.repaint = REPAINT_MAP; }
    } else if (s.tab == 1 && LEAD_LIST.in(px, py)) {
        int v = std::max(0, std::min(s.leaderScroll - notches, leaderRows(m) - LEAD_ROWS));
        if (v != s.leaderScroll) { s.leaderScroll = v; r.repaint = REPAINT_PANEL; }
    } else if (s.tab == 2 && CAR_LIST.in(px, py)) {
        int v = std::max(0, std::min(s.carScroll - notches, (int)playerCaravans(m).size() - CAR_ROWS));
        if (v != s.carScroll) { s.carScroll = v; r.repaint = REPAINT_PANEL; }
    } else if (s.tab == 0 && py >= ROW_Y && py < ROW_Y + m.d->G * LINE) {
        int g = s.good < 0 ? 0 : std::max(0, std::min(m.d->G - 1, s.good - notches));   // wheel walks the goods
        if (g != s.good) { s.good = g; r.repaint = REPAINT_MAP; }
    }
    return r;
}

// ---------------------------------------------------------------- drawing
namespace {
const uint32_t BG = 0xff1d1912, HEAD = 0xff3b3124, TAB_ON = 0xff6b5530, ROW_SEL = 0xff4f4228, ROW_ALT = 0xff242019,
               TEXT = 0xffeee4cc, DIM = 0xff9c9280, GOLD = 0xffe8c060, GREEN = 0xff86d886, RED = 0xffec6a60,
               BLUE = 0xff86b4ec, BTN = 0xff4a3e2a, BTN_HI = 0xff8a6a2c, FRAME = 0xff8a7650;

const wchar_t* pickFace() {
    static const wchar_t* face = nullptr;
    if (face) return face;
    static const wchar_t* const faces[] = { L"Microsoft JhengHei", L"MingLiU", L"PMingLiU", L"Microsoft YaHei", L"SimSun" };
    HDC dc = GetDC(nullptr);
    for (const wchar_t* f : faces) {
        LOGFONTW lf = {}; lf.lfCharSet = DEFAULT_CHARSET; wcscpy_s(lf.lfFaceName, f);
        bool found = false;
        EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) -> int { *(bool*)p = true; return 0; }, (LPARAM)&found, 0);
        if (found) { face = f; break; }
    }
    ReleaseDC(nullptr, dc);
    if (!face) face = L"MingLiU";
    return face;
}

struct Canvas {
    int w = 0, h = 0;
    uint32_t* px = nullptr;
    HDC dc = nullptr; HBITMAP bm = nullptr; HGDIOBJ oldBm = nullptr;
    HFONT font = nullptr, bold = nullptr; HGDIOBJ oldFont = nullptr;
    Canvas(int w_, int h_) : w(w_), h(h_) {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        dc = CreateCompatibleDC(nullptr);
        void* bits = nullptr;
        bm = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
        if (!bm) return;
        px = (uint32_t*)bits;
        oldBm = SelectObject(dc, bm);
        font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, pickFace());
        bold = CreateFontW(-13, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, pickFace());
        oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
    }
    ~Canvas() {
        if (oldFont) SelectObject(dc, oldFont);
        if (font) DeleteObject(font);
        if (bold) DeleteObject(bold);
        if (oldBm) SelectObject(dc, oldBm);
        if (bm) DeleteObject(bm);
        if (dc) DeleteDC(dc);
    }
    bool ok() const { return px != nullptr; }
    void fill(int x, int y, int ww, int hh, uint32_t c) {
        GdiFlush();
        for (int j = std::max(0, y); j < std::min(h, y + hh); j++)
            for (int i = std::max(0, x); i < std::min(w, x + ww); i++) px[(size_t)j * w + i] = c;
    }
    void frame(int x, int y, int ww, int hh, uint32_t c) { fill(x, y, ww, 1, c); fill(x, y + hh - 1, ww, 1, c); fill(x, y, 1, hh, c); fill(x + ww - 1, y, 1, hh, c); }
    int textW(const std::wstring& s) { SIZE sz = {}; GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz); return sz.cx; }
    // align: 0 left at x, 1 right ending at x, 2 centred on x
    void text(int x, int y, const std::wstring& s, uint32_t c, int align = 0, bool b = false) {
        if (s.empty()) return;
        if (b) SelectObject(dc, bold);
        int tw = textW(s);
        int tx = align == 1 ? x - tw : align == 2 ? x - tw / 2 : x;
        SetTextColor(dc, RGB((c >> 16) & 255, (c >> 8) & 255, c & 255));
        TextOutW(dc, tx, y, s.c_str(), (int)s.size());
        if (b) SelectObject(dc, font);
    }
    // wraps at the width; returns lines used
    int wrap(int x, int y, int width, const std::wstring& s, uint32_t c, int maxLines) {
        int lines = 0; size_t i = 0;
        while (i < s.size() && lines < maxLines) {
            size_t n = 1;
            while (i + n < s.size() && textW(s.substr(i, n + 1)) <= width) n++;
            text(x, y + lines * LINE, s.substr(i, n), c);
            i += n; lines++;
        }
        return lines;
    }
    void button(const Rc& r, const std::wstring& s, bool on, bool enabled = true) {
        fill(r.x, r.y, r.w, r.h, on ? BTN_HI : BTN);
        frame(r.x, r.y, r.w, r.h, FRAME);
        text(r.x + r.w / 2, r.y + (r.h - 14) / 2, s, enabled ? TEXT : DIM, 2);
    }
    void copyTo(uint32_t* dst, int pitch) {
        GdiFlush();
        for (int y = 0; y < h; y++) {
            const uint32_t* s = px + (size_t)y * w;
            uint32_t* d = dst + (size_t)y * pitch;
            for (int x = 0; x < w; x++) d[x] = s[x] | 0xff000000;
        }
    }
};

std::wstring ageText(int a) {
    if (a == 0) return L"即時";
    if (a >= 255) return L"未知";
    wchar_t b[24]; swprintf_s(b, L"%d月前", a); return b;
}
std::wstring seasonText(char c) {
    switch (c) {
    case 'E': return L"西南季風（東行順、西行逆）";
    case 'W': return L"東北季風（西行順、東行逆）";
    case 'T': return L"轉季（風暴，海路慢）";
    default: return L"無季風";
    }
}
uint32_t profitColour(int64_t profit, int64_t cost) {
    if (cost <= 0) return DIM;
    double r = (double)profit / cost;                       // margin
    if (r >= 0.5) return 0xff36d04a;
    if (r >= 0.0) {                                         // yellow -> green
        int t = (int)(r / 0.5 * 255);
        return 0xff000000 | ((uint32_t)(0xf0 - (0xf0 - 0x36) * t / 255) << 16) | ((uint32_t)(0xd0) << 8) | (uint32_t)(0x40 + (0x4a - 0x40) * t / 255);
    }
    if (r > -0.5) {                                         // yellow -> red
        int t = (int)(-r / 0.5 * 255);
        return 0xff000000 | (0xf0u << 16) | ((uint32_t)(0xd0 - (0xd0 - 0x40) * t / 255) << 8) | 0x40u;
    }
    return 0xffe03a30;
}

void drawHeader(Canvas& cv, const State& s) {
    static const wchar_t* const names[4] = { L"市場", L"組隊", L"商隊", L"消息" };
    cv.fill(0, 0, PANEL_W, TAB_H + 2, HEAD);
    for (int i = 0; i < 4; i++) {
        Rc r = tabRect(i);
        cv.fill(r.x, r.y, r.w, r.h, s.tab == i ? TAB_ON : HEAD);
        if (s.tab == i) cv.frame(r.x, r.y, r.w, r.h, FRAME);
        cv.text(r.x + r.w / 2, r.y + 3, names[i], s.tab == i ? GOLD : TEXT, 2, s.tab == i);
    }
    cv.button(CLOSE, L"×", false);
}

void drawInfo(Canvas& cv, const Model& m, const State& s) {
    int y = INFO_Y;
    std::wstring o = L"起：" + cityW(m, s.origin);
    if (s.origin >= 0) {
        if (m.owner[s.origin] == m.force) o += L"（我方，金 " + fmtGold(m.gold[s.origin]) + L"）";
        else o += m.owner[s.origin] < 0 ? L"（無主）" : L"（他國）";
    } else o = L"起：點地圖上的城市";
    cv.text(4, y, o, s.origin >= 0 ? TEXT : DIM);
    std::wstring d = L"終：" + cityW(m, s.dest);
    if (s.origin >= 0 && s.dest >= 0) {
        Plan p = plan(m, s.origin, s.dest, s.good < 0 ? 0 : s.good, 1, s.leader, false);
        wchar_t b[64]; swprintf_s(b, L"　%d格 約%d旬%s", p.dist, p.eta, p.sea ? L" 海路" : L"");
        d += b;
    } else if (s.origin >= 0) d = L"終：再點一座城（比較價格）";
    cv.text(4, y + LINE, d, s.dest >= 0 ? TEXT : DIM);
    std::wstring i;
    uint32_t ic = DIM;
    if (s.dest >= 0) {
        int a = m.age.empty() ? 0 : m.age[s.dest];
        i = L"終點行情：" + ageText(a);
        ic = a == 0 ? GREEN : a >= 255 ? RED : GOLD;
        if (m.d->port(s.origin) && m.d->port(s.dest)) {
            int tp = routeTimePermille(*m.d, m.live.p, s.origin, s.dest, m.month % 12);
            if (tp != 1000) {
                wchar_t b[48]; swprintf_s(b, L"　季風 航程×%d.%02d", tp / 1000, tp % 1000 / 10);
                i += b;
            }
            if (routeStormy(*m.d, m.live.p, s.origin, s.dest, m.month % 12)) i += L" 風暴";
        }
    }
    cv.text(4, y + 2 * LINE, i, ic);
    cv.fill(2, y + 3 * LINE + 2, PANEL_W - 4, 1, FRAME);
}

void drawMarket(Canvas& cv, const Model& m, const State& s) {
    const Data& d = *m.d;
    int y = TABLE_Y;
    bool route = s.origin >= 0 && s.dest >= 0;
    // columns: good, buy, sell, index, producer | destination sale, profit per load
    const int X_BUY = 74, X_SELL = 104, X_IDX = 132, X_PROD = 140, X_DSELL = 188, X_PROF = 227;
    cv.text(4, y, L"商品", DIM);
    cv.text(X_BUY, y, L"買", DIM, 1); cv.text(X_SELL, y, L"賣", DIM, 1); cv.text(X_IDX, y, L"行情", DIM, 1);
    cv.text(X_PROD + 6, y, L"產", DIM, 2);
    if (route) { cv.text(X_DSELL, y, L"終賣", DIM, 1); cv.text(X_PROF, y, L"利/擔", DIM, 1); }
    for (int g = 0; g < d.G; g++) {
        int ry = ROW_Y + g * LINE;
        if (g == s.good) cv.fill(1, ry, PANEL_W - 2, LINE, ROW_SEL);
        else if (g & 1) cv.fill(1, ry, PANEL_W - 2, LINE, ROW_ALT);
        cv.text(4, ry + 1, goodW(m, g), g == s.good ? GOLD : TEXT);
        if (s.origin < 0) continue;
        int c = s.origin;
        bool sells = d.canBuy(c, g);
        int a = m.age.empty() ? 0 : m.age[c];
        bool know = m.known.empty() || m.known[c];
        const Market& mk = a == 0 ? m.live : m.seen;
        if (!know) { cv.text(X_BUY, ry + 1, L"?", DIM, 1); cv.text(X_SELL, ry + 1, L"?", DIM, 1); }
        else {
            cv.text(X_BUY, ry + 1, sells ? fmtGold(mk.buyPrice(c, g)) : L"—", sells ? TEXT : DIM, 1);
            cv.text(X_SELL, ry + 1, fmtGold(mk.sellPrice(c, g)), TEXT, 1);
            int idx = mk.at(c, g);
            int64_t ev = mk.mod.empty() ? 1000 : mk.mod[(size_t)c * d.G + g];
            int eff = (int)(idx * ev / 1000);               // the index with the events: what the market feels like
            std::wstring t = eff >= 1100 ? L"高" : eff <= 900 ? L"低" : L"平";
            cv.text(X_IDX, ry + 1, t, eff >= 1100 ? RED : eff <= 900 ? GREEN : DIM, 1);
        }
        if (d.produces(c, g)) cv.text(X_PROD + 6, ry + 1, L"●", GOLD, 2);
        else if (d.hub(c)) cv.text(X_PROD + 6, ry + 1, L"◇", DIM, 2);
        if (route) {
            if (!m.known.empty() && !m.known[s.dest]) { cv.text(X_DSELL, ry + 1, L"?", DIM, 1); continue; }
            cv.text(X_DSELL, ry + 1, fmtGold(m.seen.sellPrice(s.dest, g)), m.age.empty() || m.age[s.dest] == 0 ? TEXT : GOLD, 1);
            if (sells) {
                Plan p = plan(m, s.origin, s.dest, g, s.qty, s.leader, false);
                if (p.qty > 0) {
                    int64_t per = p.profit / p.qty;
                    cv.text(X_PROF, ry + 1, fmtGold(per, true), per > 0 ? GREEN : RED, 1);
                }
            }
        }
    }
    // the selected trip
    int y2 = DETAIL_Y;
    cv.fill(2, y2 - 4, PANEL_W - 4, 1, FRAME);
    if (route && s.good >= 0) {
        Plan p = plan(m, s.origin, s.dest, s.good, s.qty, s.leader);
        wchar_t b[128];
        swprintf_s(b, L"%ls %d擔（上限 %d）　%ls", goodW(m, s.good).c_str(), p.qty, p.cap, s.leader < 0 ? L"商隊" : L"武將");
        cv.text(4, y2, b, GOLD); y2 += LINE;
        if (p.qty > 0) {
            cv.text(4, y2, L"購入 " + fmtGold(p.goods) + L" ＋手續費 " + fmtGold(m.live.p.fee), TEXT); y2 += LINE;
            if (!p.known) { cv.text(4, y2, L"終點行情不明（無情報）", RED); y2 += LINE; }
            else {
                cv.text(4, y2, L"售出 " + fmtGold(p.revenue) + L"　關稅 " + fmtGold(p.duty), TEXT); y2 += LINE;
                swprintf_s(b, L"過境稅 約 %lld（%d城）", (long long)p.transit, p.transitCities);
                cv.text(4, y2, b, TEXT);
                if (p.hostileCities) { swprintf_s(b, L"敵對 %d城：恐遭搶劫", p.hostileCities); cv.text(PANEL_W - 4, y2, b, RED, 1); }
                y2 += LINE;
                double pct = p.cost ? 100.0 * p.profit / p.cost : 0;
                swprintf_s(b, L"淨利 約 %ls（%+.0f%%）", fmtGold(p.profit, true).c_str(), pct);
                cv.text(4, y2, b, p.profit > 0 ? GREEN : RED, 0, true); y2 += LINE;
                if (!m.age.empty() && m.age[s.dest] > 0) { cv.text(4, y2, L"（按 " + ageText(m.age[s.dest]) + L"的行情估算）", GOLD); y2 += LINE; }
            }
            if (p.stormy) { cv.text(4, y2, L"轉季風暴：海上每旬有失貨風險", RED); y2 += LINE; }
        }
    } else if (s.origin >= 0) cv.text(4, y2, route ? L"點一行商品看這條路線的估算" : L"點地圖上第二座城比較價格", DIM);
    // legend
    int ly = MAP_H - 3 * LINE - 4;
    cv.fill(2, ly - 4, PANEL_W - 4, 1, FRAME);
    cv.text(4, ly, L"● 產地　◇ 樞紐（各貨都賣）", DIM);
    cv.text(4, ly + LINE, L"地圖：綠＝有利　黃＝持平　紅＝虧", DIM);
    cv.text(4, ly + 2 * LINE, L"灰＝無情報；點城市：起點→終點", DIM);
}

void drawForm(Canvas& cv, const Model& m, const State& s) {
    const Data& d = *m.d;
    cv.text(4, TABLE_Y, L"商品", DIM);
    if (s.origin < 0) { cv.text(4, 92, L"先在地圖上點選我方城市", DIM); return; }
    if (m.owner[s.origin] != m.force) { cv.text(4, 92, cityW(m, s.origin) + L"不是我方城市", RED); return; }
    auto goods = originGoods(m, s.origin);
    if (goods.empty()) cv.text(4, 92, cityW(m, s.origin) + L"沒有可買的商品", RED);
    for (size_t i = 0; i < goods.size() && i < 15; i++) {
        int g = goods[i];
        Rc r = goodBtn((int)i);
        cv.button(r, goodW(m, g) + L" " + fmtGold(m.live.buyPrice(s.origin, g)), g == s.good);
    }
    Plan p = plan(m, s.origin, s.dest, s.good, s.qty, s.leader);
    wchar_t b[128];
    swprintf_s(b, L"數量（擔）　上限 %d　金 %ls", p.cap ? p.cap : 0, fmtGold(m.gold[s.origin]).c_str());
    cv.text(4, QTY_LABEL_Y, b, DIM);
    static const wchar_t* const lab[6] = { L"-100", L"-10", L"-1", L"+1", L"+10", L"+100" };
    for (int i = 0; i < 6; i++) cv.button(qtyBtn(i), lab[i], false);
    cv.fill(QTY_BOX.x, QTY_BOX.y, QTY_BOX.w, QTY_BOX.h, 0xff0e0c08);
    cv.frame(QTY_BOX.x, QTY_BOX.y, QTY_BOX.w, QTY_BOX.h, FRAME);
    cv.text(QTY_BOX.x + QTY_BOX.w / 2, QTY_BOX.y + 3, fmtGold(s.qty), GOLD, 2, true);
    cv.text(4, LEAD_LABEL_Y, L"統領　Lv 商人等級 ★商才", DIM);
    for (int i = 0; i < LEAD_ROWS; i++) {
        int row = s.leaderScroll + i, id = leaderAt(m, row);
        if (id == -2) break;
        Rc r = leadRow(i);
        bool sel = id == s.leader;
        cv.fill(r.x, r.y, r.w, r.h, sel ? ROW_SEL : (i & 1) ? ROW_ALT : BG);
        if (id < 0) { cv.text(r.x + 4, r.y + 2, L"無（商隊）　臨時「商」", sel ? GOLD : TEXT); continue; }
        const Officer* o = officer(m, id);
        swprintf_s(b, L"政%d 智%d", o->pol, o->intel);
        cv.text(r.x + 4, r.y + 2, o->name, sel ? GOLD : TEXT);
        cv.text(r.x + r.w - 4, r.y + 2, b, DIM, 1);
        if (o->level > 0 || o->talent > 0) {                       // phase 4: merchant level, ★ = 商才
            wchar_t lv[24]; swprintf_s(lv, L"%lsLv%d", o->talent ? L"★" : L"", o->level);
            cv.text(r.x + 64, r.y + 2, lv, o->level >= 3 ? GREEN : GOLD);
        }
    }
    if (leaderRows(m) > LEAD_ROWS) {
        swprintf_s(b, L"%d/%d", s.leaderScroll + 1, leaderRows(m) - LEAD_ROWS + 1);
        cv.text(PANEL_W - 4, LEAD_LABEL_Y, b, DIM, 1);
    }
    int y = EST_Y;
    if (p.qty > 0 && s.dest >= 0) {
        cv.text(4, y, L"成本 " + fmtGold(p.cost) + L"（含手續費）", TEXT); y += LINE;
        if (p.known) {
            cv.text(4, y, L"售出 " + fmtGold(p.revenue) + L"　稅 " + fmtGold(p.duty + p.transit), TEXT); y += LINE;
            swprintf_s(b, L"淨利 約 %ls　約%d旬", fmtGold(p.profit, true).c_str(), p.eta);
            cv.text(4, y, b, p.profit > 0 ? GREEN : RED, 0, true); y += LINE;
        } else { cv.text(4, y, L"終點行情不明", RED); y += LINE; }
        if (p.hostileCities) { cv.text(4, y, L"沿途有敵對城市，恐遭搶劫", RED); y += LINE; }
    } else if (s.dest < 0) cv.text(4, y, L"在地圖上點終點城市", DIM);
    cv.button(SEND, s.sendPending ? L"等待執行…" : L"出發", false, p.ok && !s.sendPending);
    if (!s.msg.empty()) cv.wrap(4, MSG_Y, PANEL_W - 8, s.msg, s.msgError ? RED : GREEN, 5);
    else if (!p.ok && s.origin >= 0 && s.dest >= 0) cv.wrap(4, MSG_Y, PANEL_W - 8, p.error, DIM, 3);
}

void drawCaravans(Canvas& cv, const Model& m, const State& s) {
    auto cars = playerCaravans(m);
    wchar_t b[160];
    if (cars.empty()) cv.text(4, CAR_Y, L"沒有我方商隊（在「組隊」派出）", DIM);
    else cv.text(PANEL_W - 4, DONE_Y - 17, m.playerAuto ? L"自動＝回合末自動行軍（A 鍵切換）" : L"自動行軍已關（trade_player_auto=0）", DIM, 1);
    for (int i = 0; i < CAR_ROWS; i++) {
        int k = s.carScroll + i;
        if (k >= (int)cars.size()) break;
        const CaravanView& c = *cars[k];
        int y = CAR_Y + i * CAR_H;
        if (k == s.carSel) cv.fill(1, y, PANEL_W - 2, CAR_H, ROW_SEL);
        else if (i & 1) cv.fill(1, y, PANEL_W - 2, CAR_H, ROW_ALT);
        swprintf_s(b, L"%ls→%ls  %ls %d擔", cityW(m, c.origin).c_str(), cityW(m, c.dest).c_str(), goodW(m, c.good).c_str(), c.qty);
        cv.text(4, y + 1, b, c.temp ? TEXT : GOLD);
        int dist = hexDist(c.lo, c.hi, m.d->lo[c.dest], m.d->hi[c.dest]);
        int speed = c.atSea ? m.cfg.speedSea : m.cfg.speedLand;
        bool know = m.known.empty() || m.known[c.dest];
        int64_t value = know ? (int64_t)c.qty * m.seen.sellPrice(c.dest, c.good) : -1;
        swprintf_s(b, L"%d,%d 距%d格 約%d旬%ls", c.lo, c.hi, dist, (dist + speed - 1) / std::max(1, speed), c.atSea ? L" 海" : L"");
        cv.text(10, y + 17, b, DIM);
        Rc ab = autoBtn(i);
        bool autoOn = m.playerAuto && c.autoMarch;
        cv.button(ab, autoOn ? L"自動" : L"手動", autoOn, m.playerAuto);
        swprintf_s(b, L"值%ls 本%d", value >= 0 ? fmtGold(value).c_str() : L"?", c.cost);
        cv.text(10, y + 33, b, value >= 0 && value > c.cost ? GREEN : DIM);
        std::wstring who = c.temp ? L"商" : c.leader;
        if (!c.temp && c.level > 0) { wchar_t lv[16]; swprintf_s(lv, L" Lv%d", c.level); who += lv; }
        cv.text(PANEL_W - 4, y + 33, who, c.temp ? DIM : GOLD, 1);
    }
    if ((int)cars.size() > CAR_ROWS) { swprintf_s(b, L"%d–%d / %zu", s.carScroll + 1, s.carScroll + CAR_ROWS, cars.size()); cv.text(PANEL_W - 4, DONE_Y, b, DIM, 1); }
    cv.fill(2, DONE_Y - 4, PANEL_W - 4, 1, FRAME);
    cv.text(4, DONE_Y, L"最近交易", GOLD, 0, true);
    int y = DONE_Y + LINE + 2;
    if (m.done.empty()) cv.text(4, y, L"（無）", DIM);
    for (auto& t : m.done) {
        if (y > MAP_H - LINE) break;
        int mon = t.day / 30;
        swprintf_s(b, L"%d年%d月 %ls→%ls %ls%d", mon / 12 + 1, mon % 12 + 1, cityW(m, t.origin).c_str(), cityW(m, t.dest).c_str(),
                   goodW(m, t.good).c_str(), t.qty);
        cv.text(4, y, b, TEXT);
        std::wstring r = t.flags & DONE_INTERCEPTED ? L"被截" : t.flags & DONE_LOST ? L"失蹤" : t.flags & DONE_GAVE_UP ? L"放棄" : fmtGold(t.net - t.cost, true);
        cv.text(PANEL_W - 4, y, r, t.flags || t.net < t.cost ? RED : GREEN, 1);
        y += LINE;
    }
}

void drawNews(Canvas& cv, const Model& m, const State&) {
    const Data& d = *m.d;
    int y = INFO_Y;
    wchar_t b[160];
    swprintf_s(b, L"季風（%d月）", m.month % 12 + 1);
    cv.text(4, y, b, GOLD, 0, true); y += LINE + 2;
    if (d.seas.empty() || !m.live.p.monsoon) { cv.text(4, y, L"（未開啟）", DIM); y += LINE; }
    for (size_t i = 0; i < d.seas.size() && m.live.p.monsoon; i++) {
        char c = season(d, (int)i, m.month % 12);
        cv.text(4, y, wide(d.seas[i].name), TEXT);
        cv.text(PANEL_W - 4, y, seasonText(c), c == 'T' ? RED : c == '-' ? DIM : GREEN, 1);
        y += LINE;
    }
    // next change
    if (!d.seas.empty() && m.live.p.monsoon) {
        char now = season(d, 0, m.month % 12);
        for (int k = 1; k < 12; k++) if (season(d, 0, (m.month + k) % 12) != now) {
            swprintf_s(b, L"%d個月後轉為：%ls", k, seasonText(season(d, 0, (m.month + k) % 12)).c_str());
            cv.text(4, y, b, DIM); y += LINE; break;
        }
    }
    y += 6; cv.fill(2, y - 3, PANEL_W - 4, 1, FRAME);
    cv.text(4, y, L"市場消息", GOLD, 0, true); y += LINE + 2;
    if (m.events.empty()) { cv.text(4, y, L"（平靜）", DIM); y += LINE; }
    for (auto& e : m.events) {
        if (y > MAP_H - 6 * LINE) break;
        cv.text(4, y, wide(describe(d, e)), e.def < d.events.size() && d.events[e.def].mult >= 1000 ? GOLD : BLUE);
        swprintf_s(b, L"還有%d月", std::max(0, e.end - m.month));
        cv.text(PANEL_W - 4, y, b, DIM, 1);
        y += LINE;
    }
    y += 6; cv.fill(2, y - 3, PANEL_W - 4, 1, FRAME);
    cv.text(4, y, L"情報", GOLD, 0, true); y += LINE + 2;
    int now = 0, known = 0;
    for (int c = 0; c < d.C; c++) {
        if (!m.age.empty() && m.age[c] == 0) now++;
        if (m.known.empty() || m.known[c]) known++;
    }
    swprintf_s(b, L"即時行情 %d城，知道行情 %d / %d城", now, known, d.C);
    cv.text(4, y, b, TEXT); y += LINE;
    cv.wrap(4, y, PANEL_W - 8, L"我方與同盟城市附近的行情即時可見；商隊到過的城市記得當時的行情，越久越不準。", DIM, 4);
}
}  // namespace

void renderPanel(const Model& m, const State& s, uint32_t* dst, int pitch) {
    if (!s.on || !m.d) return;
    Canvas cv(PANEL_W, MAP_H);
    if (!cv.ok()) return;
    cv.fill(0, 0, PANEL_W, MAP_H, BG);
    cv.fill(0, 0, 2, MAP_H, FRAME);
    drawHeader(cv, s);
    if (s.tab == 0 || s.tab == 1) drawInfo(cv, m, s);
    if (s.tab == 0) drawMarket(cv, m, s);
    else if (s.tab == 1) drawForm(cv, m, s);
    else if (s.tab == 2) drawCaravans(cv, m, s);
    else drawNews(cv, m, s);
    cv.copyTo(dst, pitch);
}

// ---------------------------------------------------------------- map overlay (direct pixels)
namespace {
inline void blend(uint32_t& d, uint32_t c, int a) {      // a/256 of c
    uint32_t o = 0xff000000;
    for (int sh = 0; sh < 24; sh += 8) {
        int x = (d >> sh) & 255, y = (c >> sh) & 255;
        o |= (uint32_t)((x * (256 - a) + y * a) >> 8) << sh;
    }
    d = o;
}
struct Pix {
    uint32_t* p; int pitch, w, h;
    void put(int x, int y, uint32_t c, int a = 256) { if (x >= 0 && y >= 0 && x < w && y < h) { if (a >= 256) p[(size_t)y * pitch + x] = c; else blend(p[(size_t)y * pitch + x], c, a); } }
    void disc(float cx, float cy, float r, uint32_t c, uint32_t edge) {
        int x0 = (int)floorf(cx - r - 1), x1 = (int)ceilf(cx + r + 1), y0 = (int)floorf(cy - r - 1), y1 = (int)ceilf(cy + r + 1);
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float dd = sqrtf((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
            if (dd <= r - 1) put(x, y, c);
            else if (dd <= r) put(x, y, edge);
        }
    }
    void ring(float cx, float cy, float r, uint32_t c) {
        int x0 = (int)floorf(cx - r - 2), x1 = (int)ceilf(cx + r + 2), y0 = (int)floorf(cy - r - 2), y1 = (int)ceilf(cy + r + 2);
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float dd = sqrtf((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
            if (fabsf(dd - r) <= 1.0f) put(x, y, c);
        }
    }
    void line(float x0, float y0, float x1, float y1, uint32_t c, int dash, int a = 256) {
        // clip to the area (Liang-Barsky): an end can be far off the map
        float t0 = 0, t1 = 1, dx = x1 - x0, dy = y1 - y0;
        const float pp[4] = { -dx, dx, -dy, dy }, qq[4] = { x0 + 1, (float)w - 2 - x0, y0 + 1, (float)h - 2 - y0 };
        for (int i = 0; i < 4; i++) {
            if (pp[i] == 0) { if (qq[i] < 0) return; continue; }
            float r = qq[i] / pp[i];
            if (pp[i] < 0) { if (r > t1) return; if (r > t0) t0 = r; }
            else { if (r < t0) return; if (r < t1) t1 = r; }
        }
        float ox = x0, oy = y0;
        x0 = ox + dx * t0; y0 = oy + dy * t0; x1 = ox + dx * t1; y1 = oy + dy * t1;
        float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
        int n = std::max(1, (int)len);
        for (int i = 0; i <= n; i++) {
            if (dash && (i / dash) & 1) continue;
            float x = x0 + (x1 - x0) * i / n, y = y0 + (y1 - y0) * i / n;
            put((int)x, (int)y, c, a); put((int)x + 1, (int)y, c, a / 2); put((int)x, (int)y + 1, c, a / 2);
        }
    }
    void diamond(float cx, float cy, int r, uint32_t c) {
        for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) {
            int dd = abs(x) + abs(y);
            if (dd <= r) put((int)cx + x, (int)cy + y, dd == r ? 0xff101010 : c);
        }
    }
};
}  // namespace

void renderMap(const Model& m, const State& s, const Projector& proj, uint32_t* dst, int pitch) {
    if (!m.d) return;
    if (!s.on) {                                                // the 「貿易」 button
        Canvas cv(TOGGLE_W, TOGGLE_H);
        if (!cv.ok()) return;
        cv.fill(0, 0, TOGGLE_W, TOGGLE_H, BTN);
        cv.frame(0, 0, TOGGLE_W, TOGGLE_H, GOLD);
        cv.text(TOGGLE_W / 2, 3, L"貿易", GOLD, 2, true);
        cv.copyTo(dst + (size_t)TOGGLE_Y * pitch + TOGGLE_X, pitch);
        return;
    }
    Pix px = { dst, pitch, PANEL_X, MAP_H };
    const Data& d = *m.d;
    auto at = [&](int c, float* x, float* y) { return proj(d.lo[c], d.hi[c], x, y); };
    // the selected route
    float ox, oy, tx, ty;
    bool haveO = s.origin >= 0 && at(s.origin, &ox, &oy), haveT = s.dest >= 0 && at(s.dest, &tx, &ty);
    if (s.origin >= 0 && s.dest >= 0) {
        float ax, ay, bx, by;                                   // the line even when an end is off the map (put() clips)
        proj(d.lo[s.origin], d.hi[s.origin], &ax, &ay); proj(d.lo[s.dest], d.hi[s.dest], &bx, &by);
        px.line(ax, ay, bx, by, 0xffffffff, 0, 200);
    }
    // cities: profit of the selected good from the origin
    if (s.origin >= 0 && s.good >= 0) {
        for (int c = 0; c < d.C; c++) {
            if (c == s.origin) continue;
            float x, y;
            if (!at(c, &x, &y)) continue;
            int64_t profit, cost;
            uint32_t col = cityProfit(m, s, c, &profit, &cost) ? profitColour(profit, cost) : 0xff8a8a8a;
            px.disc(x, y + 9, 3.5f, col, 0xff101010);        // under Koei's marker
        }
    }
    // caravans: the player's in gold, visible AI ones in grey, a dashed line to their destination
    for (auto& k : m.caravans) {
        float x, y, dx, dy;
        bool here = proj(k.lo, k.hi, &x, &y);
        proj(d.lo[k.dest], d.hi[k.dest], &dx, &dy);
        if (!here) continue;
        uint32_t c = k.player ? 0xffffc020 : 0xffb0b0b0;
        px.line(x, y, dx, dy, c, 4, k.player ? 220 : 120);
        px.diamond(x, y, k.player ? 5 : 4, c);
    }
    if (haveO) { px.ring(ox, oy, 11, 0xffffffff); px.ring(ox, oy, 12, 0xff000000); }
    if (haveT) { px.ring(tx, ty, 11, 0xff60a0ff); px.ring(tx, ty, 12, 0xff000000); }
}

}  // namespace tradeui
