// M13 trade, phase 2: offline test of the midmap trade panel (worldmod/src/trade_panel.cpp) with a fake market
// built on trade.bin. Checks the hit testing, the numbers of a planned trip against the formulas trade.cpp uses
// when the caravan is created and sold (trade_model.h purchase / saleQuote / capacity / taxPct), the quantity
// limits and the form's validation; renders every tab and the map overlay to BMP files to look at.
//
//   trade_panel_test.exe <trade.bin> <out dir>        exit code 0 = every check passed
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "../src/trade_panel.h"

using namespace trade;
using namespace tradeui;

static int g_bad = 0;
static void check(bool ok, const char* what) { printf("  %-74s %s\n", what, ok ? "ok" : "FAILED"); g_bad += !ok; }

static std::vector<uint8_t> readFile(const char* p) {
    std::vector<uint8_t> v; FILE* f = nullptr;
    if (fopen_s(&f, p, "rb") || !f) return v;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    v.resize(n); fread(v.data(), 1, n, f); fclose(f); return v;
}
static void writeBmp(const std::string& path, const uint32_t* px, int w, int h) {
    BITMAPFILEHEADER fh = {}; BITMAPINFOHEADER ih = {};
    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 32; ih.biCompression = BI_RGB;
    fh.bfType = 0x4d42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + w * h * 4;
    FILE* f = nullptr; if (fopen_s(&f, path.c_str(), "wb") || !f) return;
    fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f); fwrite(px, 4, (size_t)w * h, f); fclose(f);
}
static int findCity(const Data& d, const char* utf8) { for (int c = 0; c < d.C; c++) if (d.cityName[c] == utf8) return c; return -1; }
static int findGood(const Data& d, const char* utf8) { for (int g = 0; g < d.G; g++) if (d.goodName[g] == utf8) return g; return -1; }

int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: trade_panel_test <trade.bin> <out dir>\n"); return 2; }
    SetConsoleOutputCP(CP_UTF8);
    std::vector<uint8_t> bin = readFile(argv[1]);
    Data d; std::string err;
    if (!d.load(bin.data(), bin.size(), &err)) { printf("trade.bin: %s\n", err.c_str()); return 2; }
    std::string out = argv[2];
    CreateDirectoryA(out.c_str(), nullptr);

    // ---- a fake world: Koei's China split among forces 0..4 (the player is force 1), the rest force 6..
    const int PLAYER = 1;
    const int chengdu = findCity(d, "成都"), luoyang = findCity(d, "洛陽"), changan = findCity(d, "長安"), panyu = findCity(d, "番禺"),
              hepu = findCity(d, "合浦"), muziris = findCity(d, "穆濟"), rome = findCity(d, "羅馬"), jiangzhou = findCity(d, "江州");
    const int brocade = findGood(d, "蜀錦"), silk = findGood(d, "絲"), pearl = findGood(d, "珍珠");
    printf("cities 成都 %d 洛陽 %d 長安 %d 番禺 %d 合浦 %d 穆濟 %d 羅馬 %d; goods 蜀錦 %d 絲 %d 珍珠 %d\n", chengdu, luoyang, changan, panyu, hepu,
           muziris, rome, brocade, silk, pearl);
    if (chengdu < 0 || luoyang < 0 || brocade < 0 || panyu < 0 || hepu < 0) { printf("test cities / goods missing\n"); return 2; }
    Model m; m.d = &d;
    m.live.d = &d; m.live.reset();
    Rng rng; rng.s = 12345;
    for (auto& v : m.live.idx) v = 700 + (int)(rng.next() % 700);
    m.force = PLAYER;
    m.owner.assign(d.C, -1); m.stance.assign(d.C, -1); m.gold.assign(d.C, -1);
    for (int c = 0; c < d.C; c++) {
        int f = c < 42 ? c % 5 : (c % 7 == 0 ? -1 : 6 + c % 40);
        m.owner[c] = f;
        m.stance[c] = f < 0 ? -1 : f == PLAYER || f == 3 ? 2 : (f % 4 == 0 ? 0 : 1);   // force 3 allied, some hostile
    }
    m.owner[chengdu] = PLAYER; m.stance[chengdu] = 2; m.owner[panyu] = PLAYER; m.stance[panyu] = 2;
    m.owner[luoyang] = 2; m.stance[luoyang] = 1;
    for (int c = 0; c < d.C; c++) if (m.owner[c] == PLAYER) m.gold[c] = 8000;
    m.gold[panyu] = 1500;
    m.officers = { { 1001, L"諸葛亮", 95, 100 }, { 1002, L"張飛", 22, 30 }, { 1003, L"糜竺", 80, 70 }, { 1004, L"簡雍", 70, 72 },
                   { 1005, L"孫乾", 70, 74 }, { 1006, L"伊籍", 72, 71 }, { 1007, L"馬良", 82, 80 } };
    m.month = (184 - 1) * 12 + 5;                               // June 184: the south-west monsoon
    m.cfg.tempMax = 30; m.tempLive = 3; m.freeSpecials = 80;
    // information: the player knows China now, Rome never, 番禺's surroundings 2 months ago
    Info info; info.reset(60, d, 12);
    m.live.p.infoRange = 40;
    for (int mo = 0; mo < 4; mo++) info.endMonth(m.live, m.month - 4 + mo);
    for (int c = 0; c < 42; c++) info.see(PLAYER, c);
    info.age[(size_t)PLAYER * d.C + hepu] = 2;
    info.view(m.live, PLAYER, &m.seen, &m.known);
    m.age.resize(d.C); for (int c = 0; c < d.C; c++) m.age[c] = (uint8_t)info.ageOf(PLAYER, c);
    // events: force two to start
    Events ev; Params ep = m.live.p; ep.eventChance = 100;
    for (int i = 0; i < 3; i++) ev.month(d, ep, rng, m.month - 2 + i, nullptr, nullptr);
    m.events = ev.live;
    m.caravans = { { chengdu, luoyang, brocade, 18, 1700, d.lo[chengdu] + 6, d.hi[chengdu] - 3, true, true, false },
                   { panyu, muziris, silk, 60, 9000, d.lo[panyu] - 40, d.hi[panyu] + 60, true, false, true },
                   { luoyang, chengdu, silk, 20, 900, d.lo[luoyang] - 8, d.hi[luoyang] + 2, false, true, false } };
    m.done = { { m.month * 30 - 20, chengdu, luoyang, brocade, 20, 1800, 2950, 0 }, { m.month * 30 - 50, chengdu, changan, silk, 20, 1200, 0, DONE_INTERCEPTED },
               { m.month * 30 - 90, panyu, hepu, pearl, 40, 2600, 2400, 0 } };

    // projector: 2.4 pixels per hex around 成都 (map area pixels; the panel covers x >= PANEL_X)
    float scale = 2.4f, cLo = d.lo[chengdu] + 20.0f, cHi = d.hi[chengdu] - 10.0f;
    State s;
    Projector proj = [&](int lo, int hi, float* x, float* y) {
        *x = (lo - cLo) * scale + PANEL_X / 2.0f; *y = (hi - cHi) * scale + MAP_H / 2.0f;
        return *x >= 0 && *y >= 0 && *x < (s.on ? PANEL_X : MAP_W) && *y < MAP_H;
    };

    printf("hit testing\n");
    check(!inPanel(s, 500, 300) && onToggle(s, TOGGLE_X + 5, TOGGLE_Y + 5), "panel closed: only the 「貿易」 button is ours");
    Result r = click(m, s, proj, 100, 100);
    check(!r.consumed && !s.on, "panel closed: a map click is left to Koei (camera)");
    r = click(m, s, proj, TOGGLE_X + 10, TOGGLE_Y + 10);
    check(r.consumed && s.on && (r.repaint & REPAINT_LAYOUT), "「貿易」 opens the panel (map repaint + marker layout)");
    check(inPanel(s, PANEL_X, 0) && inPanel(s, MAP_W - 1, MAP_H - 1) && !inPanel(s, PANEL_X - 1, 10), "panel rectangle = x 370..599");
    float cx, cy; proj(d.lo[chengdu], d.hi[chengdu], &cx, &cy);
    r = click(m, s, proj, (int)cx + 3, (int)cy - 2);
    check(r.consumed && s.origin == chengdu && s.dest < 0, "click near 成都 on the map: origin");
    r = click(m, s, proj, 5, 5);
    check(!r.consumed, "click on empty map: not consumed (camera jump / drag stay Koei's)");
    float lx, ly; bool lyOn = proj(d.lo[luoyang], d.hi[luoyang], &lx, &ly);
    r = click(m, s, proj, (int)lx, (int)ly);
    check(lyOn && r.consumed && s.dest == luoyang, "second city: destination");
    r = click(m, s, proj, PANEL_X + 2 + 46 + 10, 8);
    check(s.tab == 1 && r.repaint == REPAINT_PANEL, "tab 組隊");
    r = click(m, s, proj, PANEL_X + 10, 8);
    check(s.tab == 0, "tab 市場");
    r = click(m, s, proj, PANEL_X + 50, 92 + brocade * 16 + 5);
    check(s.good == brocade && (r.repaint & REPAINT_MAP), "market row click selects the good (map colours change)");
    r = wheel(m, s, PANEL_X + 50, 92 + 5 * 16, -120, false);
    check(s.good == brocade + 1, "wheel over the table walks the goods");
    s.good = brocade;
    r = wheel(m, s, 200, 200, 120, false);
    check(!r.consumed, "wheel on the map: not consumed (zoom)");

    printf("numbers (the same formulas as trade.cpp createCaravan / caravanTurn)\n");
    {
        s.qty = 15;
        Plan p = plan(m, chengdu, luoyang, brocade, 15, -1);
        int pp, capPct; leaderFactors(TEMP_POL, TEMP_INT, &pp, &capPct);
        int64_t goods = m.live.quoteBuy(chengdu, brocade, 15) * (1000 - pp) / 1000;
        Market after = m.live; int64_t sold = after.sell(luoyang, brocade, 15) * (1000 + pp) / 1000;   // what caravanTurn gets (prices known now)
        int64_t duty = sold * 10 / 100;                                                                  // 洛陽: other force, not a hub? (hub: 5%)
        if (d.hub(luoyang)) duty = sold * 5 / 100;
        char b[200];
        sprintf_s(b, "temp 商: pp %d cap %d; goods %lld = %lld, cost = goods + fee %d", pp, p.cap, (long long)p.goods, (long long)goods, m.live.p.fee);
        check(p.pricePermille == 10 && p.cap == m.live.p.capLand && p.goods == goods && p.cost == goods + m.live.p.fee, b);
        sprintf_s(b, "sale %lld = Market::sell %lld, duty %lld = %lld (洛陽 hub %d)", (long long)p.revenue, (long long)sold, (long long)p.duty, (long long)duty, d.hub(luoyang));
        check(p.revenue == sold && p.duty == duty, b);
        check(p.net == p.revenue - p.duty - p.transit && p.profit == p.net - p.cost, "net = sale - duty - transit, profit = net - cost");
        Plan q = plan(m, chengdu, luoyang, brocade, 15, 1001);                 // 諸葛亮: politics 95, intelligence 100
        sprintf_s(b, "officer 諸葛亮: pp 45, cap 20 * 125%% = %d, cheaper purchase (%lld < %lld)", q.cap, (long long)q.goods, (long long)p.goods);
        check(q.pricePermille == 45 && q.cap == 25 && q.goods < p.goods && q.revenue > p.revenue, b);
        Plan sea = plan(m, panyu, hepu, pearl, 100, -1);
        check(sea.cap == m.live.p.capSea && sea.qty == m.live.p.capSea && sea.sea, "port to port: sea load (60), quantity clamped to the load");
        // transit tax: a caravan from 成都 to 洛陽 passes other cities; the estimate charges each tax city once
        check(p.transitCities + p.hostileCities >= 0 && p.transit >= 0, "transit estimate present");
        // allied destination: no duty
        Plan al = plan(m, chengdu, 3, brocade, 10, -1);
        check(m.stance[3] == 2 && al.duty == 0, "allied destination: no duty");
        // unknown destination
        Plan unk = plan(m, chengdu, rome, brocade, 10, -1);
        check(!unk.known && unk.revenue == 0, "destination never seen (羅馬): price unknown, no profit estimate");
        // stale destination: believed index from the history
        int32_t bi; info.believed(m.live, PLAYER, hepu, pearl, &bi);
        Plan st = plan(m, panyu, hepu, pearl, 10, -1);
        check(st.known && m.seen.at(hepu, pearl) == bi && st.revenue == saleQuote(m.seen, hepu, pearl, 10, 10), "stale destination (合浦, 2 months): the remembered index");
        // monsoon: 番禺 -> 穆濟 is westbound; June = south-west monsoon: against
        Plan mon = plan(m, panyu, muziris, silk, 10, -1);
        sprintf_s(b, "monsoon June 番禺->穆濟 (west): time x%d/1000 > 1000; 穆濟->番禺 (east) < 1000", mon.timePermille);
        Plan mon2 = plan(m, muziris, panyu, silk, 10, -1);
        check(mon.timePermille > 1000 && mon2.timePermille < 1000, b);
        // the sea step cost the movement callback uses (trade.cpp seaCostHook): 南海 by month and heading
        int nanhai = d.seaOf(d.lo[panyu], d.hi[panyu] + 20);
        const Params& pr = m.live.p;
        check(nanhai >= 0 && seaCostPct(d, pr, nanhai, 5, 1) == pr.monWith && seaCostPct(d, pr, nanhai, 5, -1) == pr.monAgainst &&
              seaCostPct(d, pr, nanhai, 0, -1) == pr.monWith && seaCostPct(d, pr, nanhai, 0, 1) == pr.monAgainst &&
              seaCostPct(d, pr, nanhai, 3, 1) == pr.monStorm && seaCostPct(d, pr, nanhai, 9, -1) == pr.monStorm && seaCostPct(d, pr, nanhai, 5, 0) == 100,
              "sea cost %: June east 70 / west 140, January west 70 / east 140, April / October 125, no heading 100");
        Params off = pr; off.monsoon = 0;
        check(seaCostPct(d, off, nanhai, 5, 1) == 100 && routeTimePermille(d, off, panyu, muziris, 5) == 1000, "trade_monsoon=0: factors 100 / 1000");
    }

    printf("quantity limits and the form\n");
    {
        s.tab = 1; s.origin = chengdu; s.dest = luoyang; s.good = brocade; s.qty = 10; s.leader = -1;
        // quantity buttons: -100 -10 -1 [box] +1 +10 +100 at y 214
        click(m, s, proj, PANEL_X + 134 + 64 + 5, 220);           // +100
        check(s.qty == 20, "+100 stops at the load (20)");
        click(m, s, proj, PANEL_X + 2 + 5, 220);                  // -100
        check(s.qty == 1, "-100 stops at 1");
        click(m, s, proj, PANEL_X + 166 + 5, 220);                // +10
        click(m, s, proj, PANEL_X + 134 + 5, 220);                // +1
        check(s.qty == 12, "+10 +1 = 12");
        wheel(m, s, PANEL_X + 100, 220, 240, false);
        check(s.qty == 14, "wheel over the quantity row: +2");
        wheel(m, s, PANEL_X + 100, 220, -120, true);
        check(s.qty == 4, "shift + wheel: -10");
        // leader list: row 0 無, rows 1.. officers, scrolling
        click(m, s, proj, PANEL_X + 50, 258 + 18 + 5);            // row 1 = 諸葛亮
        check(s.leader == 1001, "leader row 1 = the first officer");
        click(m, s, proj, PANEL_X + 134 + 64 + 5, 220);           // +100 with 諸葛亮: load 25
        check(s.qty == 25, "officer's load (intelligence 100: 25)");
        wheel(m, s, PANEL_X + 50, 300, -360, false);
        check(s.leaderScroll == 2, "leader list scrolls (8 rows, 6 visible: at most 2)");
        click(m, s, proj, PANEL_X + 50, 258 + 5);                 // first visible row is now officer index 1 (張飛)
        check(s.leader == 1002, "after scrolling, row 0 = 張飛");
        s.leaderScroll = 0;
        click(m, s, proj, PANEL_X + 50, 258 + 5);
        check(s.leader == -1, "row 0 = 無（商隊）");
        // send: ok
        s.qty = 10;
        r = click(m, s, proj, PANEL_X + 100, 455);
        check(r.send && r.req.origin == chengdu && r.req.dest == luoyang && r.req.good == brocade && r.req.qty == 10 && r.req.leader == -1 && s.sendPending,
              "「出發」: request (成都, 洛陽, 蜀錦, 10, 商)");
        r = click(m, s, proj, PANEL_X + 100, 455);
        check(!r.send && s.msgError, "a second 「出發」 while the first is queued: refused");
        m.lastResult = L"成功：商隊出發"; validate(m, s);
        check(!s.sendPending && !s.msgError, "the queued result arrives: shown, button free again");
        // errors
        s.origin = luoyang; s.dest = chengdu;
        r = click(m, s, proj, PANEL_X + 100, 455);
        check(!r.send && s.msg.find(L"不是我方") != std::wstring::npos, "origin not ours: error");
        s.origin = panyu; s.dest = hepu; s.good = pearl; s.qty = 60;
        Plan p = plan(m, panyu, hepu, pearl, 60, -1);
        r = click(m, s, proj, PANEL_X + 100, 455);
        char b[200]; sprintf_s(b, "not enough gold (cost %lld, gold 1500): error", (long long)p.cost);
        check(!r.send && s.msg.find(L"金不足") != std::wstring::npos, b);
        s.qty = 1; m.tempLive = 30;
        r = click(m, s, proj, PANEL_X + 100, 455);
        check(!r.send && s.msg.find(L"上限") != std::wstring::npos, "temporary caravans at trade_temp_max: error");
        m.tempLive = 3; m.freeSpecials = 30;
        r = click(m, s, proj, PANEL_X + 100, 455);
        check(!r.send && s.msg.find(L"空位") != std::wstring::npos, "too few free special officer slots: error");
        m.freeSpecials = 80;
        s.origin = chengdu; s.dest = luoyang; s.good = silk;
        r = click(m, s, proj, PANEL_X + 100, 455);
        check(!r.send && !d.canBuy(chengdu, silk) == (s.msg.find(L"不賣") != std::wstring::npos), "good the origin does not sell: error");
        s.leader = 1007; m.officers.pop_back(); validate(m, s);
        check(s.leader == -1, "leader gone from the city (model rebuilt): back to 無");
        m.officers.push_back({ 1007, L"馬良", 82, 80 });
        // selection cycle
        selectCity(m, s, changan);
        check(s.origin == changan && s.dest == -1, "third city click: a new origin");
        selectCity(m, s, changan);
        check(s.origin == -1, "clicking the origin again clears it");
        // close
        r = click(m, s, proj, MAP_W - 10, 8);
        check(!s.on && (r.repaint & REPAINT_LAYOUT), "× closes the panel");
    }

    // ---- renders
    printf("renders\n");
    std::vector<uint32_t> full((size_t)MAP_W * MAP_H);
    auto background = [&]() {                                   // something map-like: parchment, city dots
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) full[(size_t)y * MAP_W + x] = ((x / 40 + y / 40) & 1) ? 0xffcac4b7 : 0xffc2bba9;
        for (int c = 0; c < d.C; c++) {
            float x, y; if (!proj(d.lo[c], d.hi[c], &x, &y)) continue;
            for (int j = -3; j <= 3; j++) for (int i = -3; i <= 3; i++) {
                int px = (int)x + i, py = (int)y + j;
                if (px >= 0 && py >= 0 && px < MAP_W && py < MAP_H) full[(size_t)py * MAP_W + px] = 0xff705030;
            }
        }
    };
    s = State(); s.on = true; s.origin = chengdu; s.dest = luoyang; s.good = brocade; s.qty = 18;
    validate(m, s);
    const char* names[4] = { "market", "form", "caravans", "news" };
    for (int t = 0; t < 4; t++) {
        s.tab = t;
        if (t == 1) { s.leader = 1003; s.msg = L"已下令：遊戲等待指令時出發（關閉中地圖後）"; }
        background();
        renderMap(m, s, proj, full.data(), MAP_W);
        renderPanel(m, s, full.data() + PANEL_X, MAP_W);
        std::string path = out + "\\panel_" + std::to_string(t) + "_" + names[t] + ".bmp";
        writeBmp(path, full.data(), MAP_W, MAP_H);
        printf("  %s\n", path.c_str());
    }
    s.on = false; background(); renderMap(m, s, proj, full.data(), MAP_W);
    writeBmp(out + "\\panel_closed.bmp", full.data(), MAP_W, MAP_H);
    // pixel checks: the panel paints exactly its rectangle, the map overlay never under the panel
    {
        std::vector<uint32_t> buf((size_t)MAP_W * MAP_H, 0x12345678);
        s.on = true; s.tab = 0;
        renderPanel(m, s, buf.data() + PANEL_X, MAP_W);
        bool inside = true, outside = true;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            uint32_t v = buf[(size_t)y * MAP_W + x];
            if (x >= PANEL_X) inside &= (v >> 24) == 0xff; else outside &= v == 0x12345678;
        }
        check(inside && outside, "renderPanel writes exactly the panel rectangle (opaque)");
        std::fill(buf.begin(), buf.end(), 0x12345678);
        renderMap(m, s, proj, buf.data(), MAP_W);
        bool clean = true; int painted = 0;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            uint32_t v = buf[(size_t)y * MAP_W + x];
            if (x >= PANEL_X) clean &= v == 0x12345678; else painted += v != 0x12345678;
        }
        char b[96]; sprintf_s(b, "renderMap: nothing under the panel, %d overlay pixels on the map", painted);
        check(clean && painted > 200, b);
        // colours: a city with a known profitable price is green-ish, an unknown one grey
        int64_t pr, co; int good = 0, grey = 0;
        for (int c = 0; c < d.C; c++) { if (cityProfit(m, s, c, &pr, &co)) good += pr > 0; else grey++; }
        sprintf_s(b, "map colours: %d profitable cities for 蜀錦 from 成都, %d grey (unknown)", good, grey);
        check(good > 0 && grey > 0, b);
    }
    printf("trade_panel_test: %s (%d failed)\n", g_bad ? "FAILED" : "passed", g_bad);
    return g_bad ? 1 : 0;
}
