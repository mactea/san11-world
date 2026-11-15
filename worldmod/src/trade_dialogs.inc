// City trade workflows. Included in trade.cpp to use the same model, validation
// and commands as the map panel, without duplicating the economy.
using tradenative::Row;
using tradenative::Table;
struct FormationDraft {
    int force=-1,city=-1,dest=-1,leader=-1,ship=CARAVAN;
    std::vector<OrderLine> order;
};
std::vector<FormationDraft> g_formationDrafts;
std::vector<tradeauto::Plan> g_autoDrafts;
std::wstring g_nativeFeedback;
void clearNativeDrafts() {g_formationDrafts.clear();g_autoDrafts.clear();g_nativeFeedback.clear();}
bool sameOrder(const std::vector<OrderLine>& a,const std::vector<OrderLine>& b) {
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i)if(a[i].good!=b[i].good || a[i].qty!=b[i].qty)return false;
    return true;
}
bool sameFormation(const FormationDraft& a,const FormationDraft& b) {
    return a.dest==b.dest && a.leader==b.leader && a.ship==b.ship && sameOrder(a.order,b.order);
}
bool sameAutoSettings(const tradeauto::Plan& a,const tradeauto::Plan& b) {
    return a.preferredDestination==b.preferredDestination && a.preferredGoods==b.preferredGoods &&
        a.budget.limit==b.budget.limit && a.enabled==b.enabled && a.shipType==b.shipType;
}
int nativeLeaveDraft() {
    return tradenative::show({L"尚未提交的修改",L"草稿僅保留在本次遊戲；讀檔或關閉遊戲後清除。",{L"操作",L"結果"},
        {{0,{L"保留草稿並返回",L"不扣款、不派遣，下次繼續編輯"}},
         {1,{L"放棄修改並返回",L"清除本次草稿，不改已保存設定"}},
         {2,{L"繼續編輯",L"回到剛才的頁面"}}}});
}
void nativeFormation(int city,int seedDest=-1,int seedGood=-1);
std::wstring nativeCity(int c) { return c >= 0 && c < g_data.C ? tradeui::wide(g_data.cityName[c]) : L"未選擇"; }
std::wstring nativeGood(int g) { return g >= 0 && g < g_data.G ? tradeui::wide(g_data.goodName[g]) : L"未選擇"; }
std::wstring nativeNum(int64_t v) { return std::to_wstring(v); }
std::wstring nativeAge(int c) {
    return !g_model.known[c] ? L"未知" : g_model.age[c] == 0 ? L"即時" : nativeNum(g_model.age[c])+L"個月前";
}
void nativeInfo(const std::wstring& title, const std::vector<Row>& rows) {
    tradenative::show({title,L"按返回回到上一層。",{L"項目",L"內容"},rows},false);
}
bool nativeConfirm(const std::wstring& title, const std::vector<Row>& rows, const std::wstring& picture=L"") {
    return tradenative::show({title,L"確認內容後按決定；返回不執行操作。",{L"項目",L"內容"},rows,picture},true,true) == 0;
}
std::wstring nativeError(const std::string& error) {
    const std::pair<const char*,const wchar_t*> messages[] = {
        {"no safe",L"敵軍或敵方據點附近無安全商路，暫停出發；可更換目的地或等待局勢改善。"},
        {"voyage provisions exceed",L"往返所需兵糧超過運輸隊容量，請縮短商路。"},
        {"provisions",L"續航兵糧不足；請在本方市場補給，城市需有足夠庫存。"},
        {"provision reservation",L"城市調糧未完成，已退回未裝載部分；請重試。"},
        {"batch quote changed",L"行情已變動，未出售任何貨物；請重新確認報價。"},
        {"military duplicate",L"此採購已處理或已過期，請重新選擇。"},
        {"military quote changed",L"行情已變動，請重新確認採購金額。"},
        {"military stock, gold or city capacity",L"市場現貨、本城資金或兵裝庫剩餘容量不足。"},
        {"military capacity changed",L"兵裝庫容量已變動，採購已取消並退還資金。"},
        {"military payment failed",L"付款未完成，採購已取消。"},
        {"take manual control first",L"此商隊由自動貿易管理，請在商隊管理選擇手動接管。"},
        {"read-only",L"此存檔的貿易資料只能查看，不能操作。"},
        {"unsupported or damaged",L"此存檔的貿易資料只能查看，不能操作。"},
        {"officer",L"統領目前無法出任，請重新選擇。"},
        {"port transfer in progress",L"貨物正在城市與港口間接駁，完成後才能續航。"},
        {"port transfer unavailable",L"接駁港口失守或陸路中斷，貨物仍由商隊保管。"},
        {"merchant ship requires",L"海運兩端需近岸水面或20格陸路內的同勢力港口，並啟用移動及尋路模組。"},
        {"invalid transport type",L"船型無效，請重新選擇。"},
        {"invalid cargo combination",L"商品組合無效，或超過庫存、貨艙及預算；請重新調整貨單。"},
        {"stock",L"市場庫存不足，請減少數量。"},
        {"warehouse",L"市場庫容不足，請減少數量。"},
        {"capacity",L"超過商隊載量，請減少數量。"},
        {"gold",L"付款城市的金不足。"}, {"fee",L"付款城市的金不足以支付續航費。"},
        {"troops or food",L"城市兵力或兵糧不足。"},
        {"entered a city",L"商隊已入城，請先售完餘貨並結束行程，再重新組隊。"},
        {"no reachable",L"無法到達目的市場，請另選城市。"},
        {"could not leave",L"商隊無法離城，請檢查周圍通路。"},
        {"no free hex",L"城市周圍沒有可供商隊出發的空地。"},
        {"unpaid",L"尚有舊行程欠稅，請先準備足夠的金。"},
        {"funding city",L"沒有可支付費用的我方城市。"},
        {"already travelling",L"商隊正在行軍，抵達市場後才能續航。"},
        {"already at",L"商隊已在此市場。"},
        {"city automatic caravan limit",L"本城自動商隊已達上限，等待既有商隊返程。"},
        {"temporary caravan limit",L"臨時商隊已達上限。"},
        {"not enough cargo",L"商隊攜帶的貨物不足。"},
        {"market",L"市場目前無法交易，請檢查停靠狀態及外交關係。"},
        {"resupply requires",L"補給需要停靠我方城市，且商隊部隊仍在城外。"},
        {"ownership",L"城市所屬已變更，請重新選擇。"},
        {"no longer owned",L"商隊已不存在或不再屬於我方。"}
    };
    for (auto& p:messages) if (error.find(p.first)!=std::string::npos) return p.second;
    return L"操作未完成，請重新檢查商隊及城市狀態。";
}
bool nativeExecute(const std::string& command, const std::wstring& success,bool quietSuccess=false) {
    Job job; job.line=command; job.actor=g_model.force;
    if (!runJobSafe(&job)) job.result="exception";
    bool ok=job.result.compare(0,3,"ok ")==0;
    g_log("trade: native operation %s: %s",command.c_str(),job.result.c_str());
    g_nativeFeedback=ok ? success : nativeError(job.result);
    if(!quietSuccess)nativeInfo(ok ? L"執行完畢" : L"無法執行",{{0,{L"結果",g_nativeFeedback}}});
    buildModel(job.actor);
    return ok;
}
Table nativeGoods(int city, bool buyingOnly=false, const tradeui::CaravanView* car=nullptr, bool selling=false) {
    Table t={nativeCity(city)+L"　商品",L"價格及數量以批計；我方城市可經軍需採購取得兵裝。灰色項目目前不能交易。",{L"商品",L"買入/批",L"賣出/批",L"庫存/批",L"攜帶/批",L"每批數量"},{}};
    const Market& mk=g_model.age[city]==0 ? g_model.live : g_model.seen;
    bool known=g_model.known[city]!=0;
    for(int g=0;g<g_data.G;++g) {
        bool offered=g_data.canBuy(city,g) || mk.available(city,g)>0;
        int have=car ? car->cargo.count(g) : 0;
        bool enabled= !buyingOnly || (selling ? have>0 : g_model.live.available(city,g)>0);
        t.rows.push_back({g,{nativeGood(g),known && offered ? nativeNum(mk.buyPrice(city,g)) : L"—",
            known ? nativeNum(mk.sellPrice(city,g)) : L"未知",known ? nativeNum(mk.available(city,g)) : L"未知",
            car ? nativeNum(have) : L"—",g_data.unitsPerLot(g) ? nativeNum(g_data.unitsPerLot(g))+(g_data.goodName[g]=="馬" ? L"匹" : L"件") : L"—"},enabled,nativeGood(g)});
    }
    return t;
}
int nativeChooseCity(int from, int good, bool destination,bool automatic=false,bool fromMarket=false) {
    Table t={destination ? L"選擇目的市場" : L"各地行情",L"點擊表頭排序；返回保留原選擇。",
        {L"城市",L"關係",L"行情時間",L"賣出參考",L"距離"},{}};
    if(good>=0) t.picture=nativeGood(good);
    if(automatic)t.rows.push_back({-2,{L"不限，系統擇優",L"",L"",L"",L""}});
    if(fromMarket)t.help=L"選擇開放市場可直接組建商隊，帶入商品；返回只查看，不交易。";
    for(int c=0;c<g_data.C;++c) {
        if(destination && (c==from || g_model.stance[c]<=0 || !tradeui::transportRoute(g_model,from,c))) continue;
        const wchar_t* rel=g_model.owner[c]==g_model.force ? L"我方" : g_model.stance[c]==2 ? L"同盟" : g_model.stance[c]==1 ? L"通商" : L"關閉";
        t.rows.push_back({c,{nativeCity(c),rel,nativeAge(c),good>=0 && g_model.known[c] ? nativeNum(g_model.seen.sellPrice(c,good)) : L"未知",
            nativeNum(hexDist(g_data.lo[from],g_data.hi[from],g_data.lo[c],g_data.hi[c]))},!fromMarket || (c!=from && g_model.stance[c]>0)});
    }
    if(t.rows.empty()) { nativeInfo(L"目的市場",{{0,{L"狀態",L"目前沒有開放交易的其他城市。"}}}); return -1; }
    return tradenative::show(t,destination||fromMarket);
}
void nativeMarket(int city) {
    tradenative::ViewState view;
    for(;;) {
        buildModel(g_model.force);
        Table t=nativeGoods(city); t.title=nativeCity(city)+L"　市場行情";
        t.help=L"選擇商品查看各地行情。兵裝每批100件、馬100匹；城市採購請用軍需採購。成交額含數量影響，商隊另有統領加成。";
        int good=tradenative::show(t,true,false,&view); if(good<0) return;
        int dest=nativeChooseCity(city,good,false,false,true);
        if(dest>=0)nativeFormation(city,dest,good);
    }
}
std::wstring nativeLeader(int id) {
    for(auto& o:g_model.officers) if(o.id==id) return o.name;
    return id<0 ? L"臨時商人" : L"不可用";
}
int nativeChooseLeader() {
    Table t={L"選擇統領",L"政治影響價格，智力影響載量；經商天賦與經驗提供額外加成。",
        {L"統領",L"政治",L"智力",L"商人等級",L"經驗",L"經商天賦"},{}};
    t.rows.push_back({0,{L"臨時商人",nativeNum(TEMP_POL),nativeNum(TEMP_INT),L"0",L"0",L"0"},
        g_model.tempLive<g_model.cfg.tempMax && g_model.freeSpecials-1>=g_model.cfg.tempKeep});
    for(auto& o:g_model.officers) {
        uint8_t* p=personRec(WORLD,o.id);
        bool available=p && *(int*)(p+0xa0)>=0 && *(int*)(p+0xa0)<=3;
        for(auto& k:g_car) if(k.state!=FINISHED && !k.tempReleased && k.leader==o.id) available=false;
        t.rows.push_back({o.id+1,{o.name,nativeNum(o.pol),nativeNum(o.intel),nativeNum(o.level),nativeNum(o.exp),nativeNum(o.talent)},available});
    }
    return tradenative::show(t);
}
std::wstring nativeLoad(int64_t units) {
    return nativeNum(units/10)+L"."+nativeNum(units%10)+L" 格";
}
void setOrderQty(std::vector<OrderLine>& order,int good,int qty) {
    for(auto it=order.begin();it!=order.end();++it) if(it->good==good) {
        if(qty>0) it->qty=qty;else order.erase(it);return;
    }
    if(qty>0) order.push_back({good,qty});
}
int nativeRecommendedRoute(int city,int leader,std::vector<OrderLine>& order) {
    struct Choice {int city;tradeui::BasketPlan plan;std::vector<OrderLine> order;};
    std::vector<Choice> choices;
    for(int c=0;c<g_data.C;++c) {
        if(c==city || g_model.stance[c]<=0 || !g_model.known[c]) continue;
        auto goods=order.empty() ? tradeui::recommendOrder(g_model,city,c,leader) : order;
        if(goods.empty()) continue;
        auto p=tradeui::basketPlan(g_model,city,c,goods,leader);
        if(p.ok && p.quoted) choices.push_back({c,p,goods});
    }
    std::stable_sort(choices.begin(),choices.end(),[](const Choice& a,const Choice& b) {
        return a.plan.profit*std::max(1,b.plan.eta)>b.plan.profit*std::max(1,a.plan.eta);
    });
    Table t={L"商路推薦",L"按每旬淨收益排列，可點表頭排序。空貨單自動建議組合；旬數及過境稅為地理估算，未含糧耗貨損。",
        {L"目的地",L"淨收益",L"每旬收益",L"約需旬數",L"銷售／過境稅",L"行情／敵境風險"},{}};
    for(auto& c:choices) t.rows.push_back({c.city,{nativeCity(c.city),nativeNum(c.plan.profit),nativeNum(c.plan.profit/std::max(1,c.plan.eta)),nativeNum(c.plan.eta),
        nativeNum(c.plan.duty)+L" / "+nativeNum(c.plan.transit),nativeAge(c.city)+L" / "+nativeNum(c.plan.hostileCities)+L" 城"}});
    if(choices.empty()) {nativeInfo(L"商路推薦",{{0,{L"提示",L"已知行情中沒有可用方案；可手動選擇市場及配貨。"}}});return -1;}
    int c=tradenative::show(t);
    if(c>=0) for(auto& choice:choices) if(choice.city==c) {order=choice.order;break;}
    return c;
}
void nativeEditOrder(int city,int dest,int leader,std::vector<OrderLine>& order) {
    tradenative::ViewState view;
    for(;;) {
        auto p=tradeui::basketPlan(g_model,city,dest,order,leader);
        Table t={L"調整貨單",L"選商品直接輸入數量；0移除。返回保留貨單。貨艙 "+nativeLoad(p.used)+L" / "+nativeNum(p.slots)+
            L"；支付 "+nativeNum(p.cost)+L"；預計淨利 "+nativeNum(p.profit),
            {L"商品",L"數量",L"最多可買",L"每批佔艙",L"採購額",L"毛利含商才"},{}};
        for(int g=0;g<g_data.G;++g) {
            int qty=0;for(auto& l:order)if(l.good==g)qty=l.qty;
            int maximum=tradeui::maxOrderQuantity(g_model,city,dest,order,g,leader);
            if(!qty && !g_model.live.available(city,g))continue;
            auto one=tradeui::plan(g_model,city,dest,g,qty,leader,false);
            t.rows.push_back({g,{nativeGood(g),nativeNum(qty),nativeNum(maximum),nativeLoad(g_data.loadUnits(g)),
                nativeNum(one.goods),one.quoted ? nativeNum(one.revenue+one.merchantBonus-one.duty-one.goods) : L"未知"},qty>0||maximum>0,nativeGood(g)});
        }
        int good=tradenative::show(t,true,false,&view);if(good<0)return;
        int current=0;for(auto& l:order)if(l.good==good)current=l.qty;
        int maximum=tradeui::maxOrderQuantity(g_model,city,dest,order,good,leader),bestQty=0;int64_t bestProfit=INT64_MIN;
        for(int qty=1;qty<=maximum;++qty) {
            auto candidate=order;setOrderQty(candidate,good,qty);auto q=tradeui::basketPlan(g_model,city,dest,candidate,leader);
            if(q.ok && q.quoted && q.profit>bestProfit) {bestProfit=q.profit;bestQty=qty;}
        }
        int qty=tradenative::number(L"建議"+nativeNum(bestProfit>0?bestQty:0),
            current>0 ? std::min(current,maximum) : bestProfit>0 ? bestQty : 0,maximum,nativeGood(good),0);
        if(qty>=0)setOrderQty(order,good,qty);
    }
}
std::wstring nativePortLabel(int city,int anchor) {
    if(anchor==city)return L"城市近岸直航";
    auto b=nativePort(anchor);if(!b)return L"無可用港口";
    auto p=posOf(b);return L"港口（"+nativeNum(p[0])+L", "+nativeNum(p[1])+L"）";
}
std::wstring nativeAccess(int city) {
    if(city<0 || city>=(int)g_model.portBase.size())return L"待選城市";
    int steps=g_model.portSteps[city];
    auto text=nativePortLabel(city,g_model.portBase[city]);
    if(steps)text+=L"；接駁 "+nativeNum(steps)+L"格 / "+nativeNum((steps+9)/10)+L"旬 / "+nativeNum(steps*2)+L"金";
    return text;
}
int nativeChooseTransport(int city) {
    Table t={L"選擇運輸方式",L"海運可經20格陸路內的同勢力港口接駁；每10格1旬、每格2金。商船沿河海航行，不能跨陸。",
        {L"種類",L"基礎貨艙",L"組建費",L"續航費",L"航行範圍"},{}};
    for(int type:{CARAVAN,MERCHANT_SHIP})
        t.rows.push_back({type,{transportName(type),nativeNum(type==MERCHANT_SHIP?g_mk.p.capSea:g_mk.p.capLand),
            nativeNum(cityTradeFee(g_model.force,city,type)),nativeNum(cityTradeFee(g_model.force,city,type,true)),
            type==MERCHANT_SHIP?L"近岸直航／同勢力港口接駁":L"陸路與水路"},
            type==CARAVAN || (movementReady() && pathfindReady() && shipPort(city))});
    return tradenative::show(t);
}
// Editing is read-only. The complete basket is validated and committed in one command.
void nativeFormation(int city,int seedDest,int seedGood) {
    FormationDraft draft;draft.city=city;draft.force=g_model.force;
    auto initial=draft;
    for(auto& d:g_formationDrafts)if(d.city==city && d.force==draft.force)draft=d;
    if(seedDest>=0) {draft.dest=seedDest;if(seedGood>=0)setOrderQty(draft.order,seedGood,1);}
    auto& s=g_ui;s.origin=city;s.dest=draft.dest;s.leader=draft.leader;
    auto& order=draft.order;int& shipType=draft.ship;
    std::vector<OrderLine> previousOrder;bool canUndo=false;std::wstring notice;
    tradenative::ViewState view;
    for(;;) {
        buildModel(g_model.force);g_model.shipType=shipType;
        auto p=tradeui::basketPlan(g_model,city,s.dest,order,s.leader);
        bool ready=p.ok && !g_readOnly;
        Table t={nativeCity(city)+L"　組建商隊",notice.empty()?L"推薦商路或自選目的地 → 調整貨單 → 確認出發。返回可保留草稿。":notice,{L"項目",L"內容"},{
            {6,{L"推薦商路",L"選一條商路帶入目的地與貨單"}},
            {2,{L"自選目的市場",nativeCity(s.dest)}},
            {15,{L"運輸方式／船型",transportName(shipType)}},
            {0,{L"出發城市",nativeCity(city)},false},{1,{L"統領",nativeLeader(s.leader)}},
            {3,{L"調整貨單",s.dest>=0 ? nativeNum(order.size())+L" 種商品，直接修改數量" : L"先選目的市場"},s.dest>=0},
            {4,{L"重新推薦配貨",L"替換貨單；可恢復替換前內容"},s.dest>=0},
            {17,{L"恢復原貨單",L"撤回上次推薦配貨"},canUndo},
            {5,{L"出發",ready ? (p.quoted && p.profit<0 ? L"預計虧損，請檢查貨單" : L"確認組隊內容") : g_readOnly ? L"存檔只能查看" : p.error},ready},
            {7,{L"貨艙佔用",p.slots>0 ? nativeLoad(p.used)+L" / "+nativeNum(p.slots)+L" 格" : L"選定商路後計算"},false},
            {8,{L"貨款／組隊及接駁費",nativeNum(p.goods)+L" / "+nativeNum(tradeui::quoteFee(g_model,city,s.dest))},false},
            {9,{L"預估銷售／過境稅",p.quoted ? nativeNum(p.duty)+L" / "+nativeNum(p.transit) : L"行情不足"},false},
            {10,{L"單程過境稅上限",nativeNum(p.transitLimit)},false},
            {11,{L"預估淨收益",p.quoted ? tradeui::fmtGold(p.profit,true)+L"（未含糧耗貨損）" : L"行情不足"},false},
            {16,{L"商才收益加成",nativeNum(p.merchantBonus)+L"金（已含於預估淨收益）"},false},
            {12,{L"地理粗估旬數／每旬收益",p.quoted ? nativeNum(p.eta)+L" / "+tradeui::fmtGold(p.profit/std::max(1,p.eta),true) : L"—"},false},
            {13,{L"護衛／兵糧",nativeNum(g_model.escortNeeded)+L" / "+nativeNum(p.provisions)},false},
            {14,{L"城市可用金",nativeNum(g_model.gold[city])},false}
        }};
        if(shipType==MERCHANT_SHIP) {
            t.rows.push_back({20,{L"出發接駁",nativeAccess(city)},false});
            t.rows.push_back({21,{L"目的接駁",nativeAccess(s.dest)},false});
        }
        if(order.size()==1) t.picture=nativeGood(order.front().good);
        int action=tradenative::show(t,true,false,&view);
        if(action<0) {
            draft.dest=s.dest;draft.leader=s.leader;
            if(sameFormation(draft,initial))return;
            int leave=nativeLeaveDraft();if(leave<0 || leave==2)continue;
            g_formationDrafts.erase(std::remove_if(g_formationDrafts.begin(),g_formationDrafts.end(),[&](const FormationDraft& d){return d.city==city && d.force==draft.force;}),g_formationDrafts.end());
            if(leave==0)g_formationDrafts.push_back(draft);
            return;
        }
        if(action==15) {int type=nativeChooseTransport(city);if(type>=0)shipType=type;}
        if(action==1) {int n=nativeChooseLeader();if(n>=0) s.leader=n-1;}
        if(action==2) {int c=nativeChooseCity(city,order.empty() ? -1 : order.front().good,true);if(c>=0) s.dest=c;}
        if(action==3) {if(s.dest<0) nativeInfo(L"商品組合",{{0,{L"提示",L"請先選擇目的市場。"}}});else nativeEditOrder(city,s.dest,s.leader,order);}
        if(action==4) {
            if(s.dest<0) {nativeInfo(L"建議配貨",{{0,{L"提示",L"請先選擇目的市場，或使用商路推薦。"}}});continue;}
            auto proposed=tradeui::recommendOrder(g_model,city,s.dest,s.leader);
            if(proposed.empty()) nativeInfo(L"建議配貨",{{0,{L"提示",L"已知行情下未找到盈利組合，保留原貨單；可換市場或手動配貨。"}}});
            else {previousOrder=order;canUndo=true;order=std::move(proposed);notice=L"已更新建議貨單；可選恢復原貨單。";}
        }
        if(action==17 && canUndo) {order=previousOrder;canUndo=false;notice=L"已恢復原貨單。";}
        if(action==6) {auto proposed=order;int c=nativeRecommendedRoute(city,s.leader,proposed);if(c>=0) {previousOrder=order;canUndo=true;order=proposed;s.dest=c;}}
        if(action==5 && ready) {
            std::vector<Row> rows={{0,{L"行程",nativeCity(city)+L" → "+nativeCity(s.dest)}},{1,{L"統領",nativeLeader(s.leader)}},
                {2,{L"支付金",nativeNum(p.cost)+L"（組隊及接駁費 "+nativeNum(tradeui::quoteFee(g_model,city,s.dest))+L"）"}},
                {3,{L"預估淨收益",p.quoted ? tradeui::fmtGold(p.profit,true)+L"；未含糧耗貨損" : L"行情未知"}},
                {4,{L"行情／地理粗估旬數",nativeAge(s.dest)+L" / "+nativeNum(p.eta)}},
                {5,{L"過境稅／上限",nativeNum(p.transit)+L" / "+nativeNum(p.transitLimit)}},
                {6,{L"風險",L"出發前核對安全路線；敵軍動態與到港行情可能變動"}},
                {9,{L"往返備糧",nativeNum(p.provisions)+L"（地理預估；出發前按實際繞行及接駁復核）"}}};
            for(auto& l:order) rows.push_back({7,{nativeGood(l.good),nativeNum(l.qty)+L" 批"},true,nativeGood(l.good)});
            rows.push_back({8,{L"運輸方式",transportName(shipType)}});
            if(shipType==MERCHANT_SHIP) {
                rows.push_back({20,{L"出發接駁",nativeAccess(city)}});
                rows.push_back({21,{L"目的接駁",nativeAccess(s.dest)}});
            }
            if(!nativeConfirm(L"確認商品組合出發",rows)) continue;
            std::string cmd="trade sendship "+std::to_string(city)+" "+std::to_string(s.dest)+" "+std::to_string(s.leader)+" "+std::to_string(shipType);
            for(auto& l:order) cmd+=" "+std::to_string(l.good)+" "+std::to_string(l.qty);
            if(nativeExecute(cmd,L"商隊已出發；可在本城商隊管理查看行程。",true)) {
                g_formationDrafts.erase(std::remove_if(g_formationDrafts.begin(),g_formationDrafts.end(),[&](const FormationDraft& d){return d.city==city && d.force==draft.force;}),g_formationDrafts.end());
                nativeInfo(L"商隊已出發",{{0,{L"結果",g_nativeFeedback}}});return;
            }
            notice=g_nativeFeedback+L"；貨單已保留，可調整後重試。";
        }
    }
}
void nativeLedger(const Ledger& a, int64_t remaining) {
    nativeInfo(L"行程收支",{{0,{L"帳本範圍",a.complete ? L"完整行程" : L"舊存檔：僅遷移後收支"}},
        {1,{L"採購支出",nativeNum(a.purchases)}},{2,{L"收入（含商才獎勵）",nativeNum(a.receipts)}},
        {3,{L"組隊／續航費",nativeNum(a.fees)}},{4,{L"過境與銷售稅",nativeNum(a.taxes)}},
        {5,{L"貨損成本（已含採購）",nativeNum(a.losses)}},{6,{L"剩餘貨物成本",nativeNum(remaining)}},
        {7,{L"現金淨流量",tradeui::fmtGold(a.cashNet(),true)}},{8,{L"已實現損益",tradeui::fmtGold(a.profit(remaining),true)}}});
}
Caravan* nativeCar(uint32_t id) {
    for(auto& k:g_car) if(k.id==id && k.force==g_model.force && k.state!=FINISHED && !(k.flags&F_LOOTED)) return &k;
    return nullptr;
}
bool nativeManaged(const Caravan& k) {return k.automatic.caravan && k.automatic.managed;}
bool nativeNeedsDecision(const Caravan& k) {
    return k.state!=FINISHED && !(k.flags&F_LOOTED) && nativeManaged(k) && (k.automatic.issue!=tradeauto::Issue::None || k.tempReleased);
}
bool nativeTakeover(uint32_t id) {
    auto k=nativeCar(id);if(!k || !nativeManaged(*k) || g_readOnly)return false;
    if(!nativeConfirm(L"接管此商隊",{{0,{L"商隊",L"#"+nativeNum(id)}},{1,{L"退回未用預留款",nativeNum(k->automatic.fees.balance)}},
        {2,{L"結果",L"停止此隊自動買賣與行軍，保留貨物；本城計畫仍可派遣其他商隊"}}}))return false;
    Lock l;k=nativeCar(id);if(!k || !nativeManaged(*k))return false;
    autoRefund(*k);k->automatic=tradeauto::Voyage();k->flags|=F_MANUAL;return true;
}
void nativeSellBatch(uint32_t id) {
    tradenative::ViewState view;std::vector<int> selected;bool first=true;std::wstring notice;
    for(;;) {
        auto k=nativeCar(id);if(!k || nativeManaged(*k) || g_readOnly || k->state!=DOCKED)return;
        Table t={L"批量出售貨物",notice.empty()?L"勾選後決定；預選非虧損貨物，未選項保留。庫容不足時只列可售數量。":notice,
            {L"商品",L"可售／攜帶",L"稅後收入含獎勵",L"進貨成本",L"本次毛利"},{}};
        std::vector<int> quantities(g_data.G,0);
        for(int g=0;g<g_data.G;++g)if(k->cargo.count(g)>0) {
            int qty=std::min(k->cargo.count(g),g_mk.room(k->port,g));quantities[g]=qty;
            auto q=qty>0 ? quoteBatchSale(*k,{{g,qty}}) : BatchSale();
            bool ok=qty>0 && q.error.empty();int64_t basis=ok?q.lines[0].basis:0;
            t.rows.push_back({g,{nativeGood(g),nativeNum(qty)+L" / "+nativeNum(k->cargo.count(g)),ok?nativeNum(q.net):L"庫容不足",
                ok?nativeNum(basis):L"—",ok?tradeui::fmtGold(q.net-basis,true):L"—"},ok,nativeGood(g)});
            if(first && ok && q.net>=basis)selected.push_back(g);
        }
        first=false;if(t.rows.empty())return;
        if(tradenative::show(t,true,false,&view,&selected)<0)return;
        if(selected.empty()) {notice=L"未選擇貨物，沒有出售；可重新勾選或返回。";continue;}
        std::vector<OrderLine> order;for(int g:selected)order.push_back({g,quantities[g]});
        k=nativeCar(id);if(!k)return;auto quote=quoteBatchSale(*k,order);
        if(!quote.error.empty()) {notice=nativeError(quote.error);continue;}
        std::vector<Row> rows;bool loss=false;
        for(auto& q:quote.lines) {loss|=q.net()<q.basis;rows.push_back({q.order.good,{nativeGood(q.order.good),nativeNum(q.order.qty)+L" 批；毛利 "+tradeui::fmtGold(q.net()-q.basis,true)}});}
        rows.push_back({-2,{L"合計稅後收入含獎勵",nativeNum(quote.net)}});
        if(loss)rows.push_back({-3,{L"注意",L"包含虧損商品，決定後仍會出售"}});
        if(!nativeConfirm(loss?L"確認虧損出售":L"確認批量出售",rows))continue;
        std::string command="trade sellbatch "+std::to_string(id)+" "+std::to_string(quote.net);
        for(auto l:order)command+=" "+std::to_string(l.good)+" "+std::to_string(l.qty);
        nativeExecute(command,L"所選貨物已售出，收入已匯回；可繼續處理剩餘貨物。",true);
        notice=g_nativeFeedback;
    }
}
void nativeCargo(const tradeui::CaravanView& initial, bool buying) {
    tradenative::ViewState view;std::wstring notice;int lastGood=-1,lastQty=1;
    for(;;) {
    buildModel(g_model.force);tradeui::CaravanView car=initial;bool found=false;
    for(auto& c:g_model.caravans)if(c.id==initial.id && c.player) {car=c;found=true;break;}
    if(!found)return;
    auto choices=nativeGoods(car.port,true,&car,!buying);if(!notice.empty())choices.help=notice;
    int good=tradenative::show(choices,true,false,&view); if(good<0) return;
    Caravan* k=nativeCar(car.id); if(!k) return;
    if(nativeManaged(*k) || g_readOnly)return;
    int pp,cp; MerchantBonus mb; leaderOrTemp(*k,&pp,&cp,&mb);
    int maximum=buying ? std::min((int)std::max<int64_t>(0,((int64_t)car.maxLoad*10-car.cargo.loadUnits(g_data))/g_data.loadUnits(good)),g_model.live.available(car.port,good)) : car.cargo.count(good);
    uint8_t* payer=payee(k->force,k->origin);
    while(maximum>0) {
        int64_t amount=buying ? purchase(g_model.live,car.port,good,maximum,pp) : saleQuote(g_model.live,car.port,good,maximum,pp);
        if(amount>=0 && (!buying || (payer && amount<=baseGold(payer)))) break;
        --maximum;
    }
    if(maximum<1) { nativeInfo(L"無法交易",{{0,{L"原因",buying ? L"載量、庫存或付款城市的金不足。" : L"市場已無法接收此商品。"}}}); continue; }
    int qty=lastGood==good ? std::min(lastQty,maximum) : buying ? 1 : maximum;
    for(;;) {
    qty=tradenative::number(buying ? L"買入數量（批）" : L"賣出數量（批）",qty,maximum,nativeGood(good));
    if(qty<1 || qty>maximum) break;
    lastGood=good;lastQty=qty;
    int64_t amount=buying ? purchase(g_model.live,car.port,good,qty,pp) : saleQuote(g_model.live,car.port,good,qty,pp);
    int rem=g_model.saleTaxRemainder[car.port];
    int64_t duty=buying || g_model.stance[car.port]==2 ? 0 : taxWithCarry(amount,taxPct(g_data,car.port,g_cfg.tax,g_cfg.taxHub),mb.taxPct,&rem);
    int64_t debt=buying ? 0 : std::min<int64_t>(k->taxDue,amount-duty);
    int64_t bonus=buying ? 0 : car.cargo.saleBonus(good,qty,car.port,amount-duty-debt,
        baseForce(k->origin)==k->force && cityMerchantSkill(k->force,k->origin));
    if(nativeConfirm(buying ? L"確認買入" : L"確認賣出／匯款",{
        {0,{L"商隊",L"#"+nativeNum(car.id)+L"　"+nativeCity(car.port)}},
        {1,{L"商品",nativeGood(good)+L" "+nativeNum(qty)+L" 批"}},
        {2,{buying ? L"貨款" : L"稅前收入",nativeNum(amount)}},
        {3,{L"銷售稅／舊欠稅",nativeNum(duty)+L" / "+nativeNum(debt)}},
        {4,{buying ? L"付款城市" : L"收款城市",nativeCity(car.payer)}},
        {5,{buying ? L"支付金" : L"稅後收入（含獎勵）",nativeNum(amount+bonus-duty-debt)}},
        {6,{L"說明",buying ? L"決定後扣款並裝貨。" : L"城市金達上限時，餘額列為待入帳。"}},
        {7,{L"商才收益加成",nativeNum(bonus)+L"金"}}
    },nativeGood(good))) {
        nativeExecute(std::string(buying ? "trade buy " : "trade sell ")+std::to_string(car.id)+" "+std::to_string(good)+" "+std::to_string(qty),buying ? L"貨物已買入，可繼續選購；返回續航。" : L"貨物已售出並辦理匯款，可繼續出售。",true);
        notice=g_nativeFeedback;break;
    }
    }
    }
}
void nativeManage(uint32_t id) {
    tradenative::ViewState view;std::wstring notice;
    for(;;) {
        buildModel(g_model.force);
        tradeui::CaravanView car; bool found=false;
        for(auto& k:g_model.caravans) if(k.player && k.id==id) { car=k; found=true; break; }
        if(!found) return;
        Caravan* k=nativeCar(id); if(!k) return;
        g_model.shipType=car.shipType;
        bool managed=nativeManaged(*k),docked=car.state==DOCKED, live=liveUnit(*k)!=nullptr, write=!g_readOnly&&!managed;
        Table t={L"商隊 #"+nativeNum(id),notice.empty()?(managed?L"此隊自動管理；需要自行買賣或改道時，先在此接管。":L"到港後：出售貨物 → 採購回程貨物 → 返回出資城或續航。灰色操作暫不可用。"):notice,{L"操作",L"內容"},{
            {18,{L"手動接管",managed?L"退回預留款，停止此隊自動交易，保留貨物":L"目前已由玩家控制"},managed&&!g_readOnly},
            {17,{L"批量出售",managed?L"請先手動接管":L"勾選貨物，一次確認；虧損項不預選"},docked&&write&&car.cargo.count()>0},
            {0,{L"貨物一覽",nativeNum(car.cargo.count())+L" 批；"+nativeLoad(car.cargo.loadUnits(g_data))+L" / "+nativeNum(car.maxLoad)+L" 格"}},
            {1,{L"買入",docked && live ? L"選擇商品與數量" : live ? L"需停靠開放市場" : L"已入城，請先結束行程"},docked&&live&&write},
            {2,{L"賣出／匯款",docked ? L"選擇攜帶貨物與數量" : L"需停靠開放市場"},docked&&write&&car.cargo.count()>0},
            {3,{L"續航",k->transferStage ? L"接駁完成後可續航" : car.state!=TRAVELLING && live ? L"選擇下一個市場" : live ? L"正在行軍" : L"已入城，請先結束行程"},car.state!=TRAVELLING&&!k->transferStage&&live&&write},
            {4,{L"補充兵糧",docked && car.port>=0 && g_model.owner[car.port]==g_model.force ? L"從停靠城市補給" : L"需停靠我方城市"},docked&&live&&car.port>=0&&g_model.owner[car.port]==g_model.force&&write},
            {5,{L"行軍方式",g_model.playerAuto && car.autoMarch ? L"自動（點擊改為手動）" : L"手動（點擊改為自動）"},g_model.playerAuto&&live&&write},
            {6,{L"結束行程",docked ? L"收回商隊，處理剩餘貨物" : L"需停靠開放市場"},docked&&write},
            {7,{L"收支帳本",L"查看本行程的收支明細"}},
            {19,{L"返回出資城",car.port==car.origin?L"已在出資城":nativeCity(car.origin)},docked&&live&&write&&car.port!=car.origin&&baseForce(car.origin)==g_model.force},
            {16,{L"運輸方式／續航費",std::wstring(transportName(car.shipType))+L" / "+nativeNum(cityTradeFee(k->force,k->origin,car.shipType,true))+L"金"},false},
            {8,{L"行程",nativeCity(car.origin)+L" → "+nativeCity(car.dest)},false},
            {9,{L"停靠／狀態",car.port>=0 ? nativeCity(car.port) : car.transferStage ? L"港口接駁中" : car.state==TRAVELLING ? L"行軍中" : L"等待重新安排路線"},false},
            {10,{L"統領",car.temp ? L"臨時商人" : car.leader+L"　商人等級 "+nativeNum(car.level)},false},
            {11,{L"付款城市",nativeCity(car.payer)},false},
            {12,{L"所在座標",nativeNum(car.lo)+L", "+nativeNum(car.hi)},false},
            {13,{L"剩餘路線",car.route.empty() ? L"尚無行軍路線" : nativeNum(car.routeSteps)+L" 格，步耗 "+nativeNum(car.routeCost)},false},
            {14,{L"路徑預估剩餘旬數",car.routeTurns>=0 ? nativeNum(car.routeTurns)+L" 旬（含接駁，未含額外停留）" : L"待取得可用路徑"},false},
            {15,{L"剩餘陸路／水路",car.routeTurns>=0 ? nativeNum(car.routeLand)+L" / "+nativeNum(car.routeWater)+L" 格" : L"—"},false}
        }};
        if(car.shipType==MERCHANT_SHIP) {
            t.rows.push_back({20,{L"出航／到達港口",nativePortLabel(k->legOrigin>=0?k->legOrigin:k->origin,k->embarkBase>=0?k->embarkBase:k->origin)+L" → "+nativePortLabel(k->dest,k->arrivalBase>=0?k->arrivalBase:k->dest)},false});
            t.rows.push_back({21,{L"接駁／通路狀態",!car.reason.empty()?car.reason:L"可航行／交易"},false});
        }
        if(managed)for(auto& row:t.rows)if(row.id>=1 && row.id<=6)row.cells[1]=L"自動管理中，請先手動接管";
        int action=tradenative::show(t,true,false,&view); if(action<0) return;
        if(action==18) {if(nativeTakeover(id))notice=L"已接管；可直接買賣、返程或續航。";continue;}
        if(action==17) {g_nativeFeedback.clear();nativeSellBatch(id);notice=g_nativeFeedback;continue;}
        if(action==19) {
            k=nativeCar(id);if(!k)continue;
            if(nativeConfirm(L"返回出資城",{{0,{L"目的地",nativeCity(car.origin)}},{1,{L"續航及接駁費",nativeNum(continuationFee(*k,car.origin))}},
                {2,{L"貨物",L"保留現有貨物，到港後由玩家決定出售"}}})) {
                nativeExecute("trade go "+std::to_string(id)+" "+std::to_string(car.origin),L"已安排返回出資城。",true);notice=g_nativeFeedback;
            }
            continue;
        }
        if(action==0) {
            Table cargo={L"商隊貨物",L"按返回回到商隊操作。",{L"商品",L"產地",L"數量",L"成本"},{}};
            for(auto& lot:car.cargo.lots) cargo.rows.push_back({0,{nativeGood(lot.good),nativeCity(lot.origin),nativeNum(lot.qty),nativeNum(lot.cost)},true,nativeGood(lot.good)});
            if(cargo.rows.empty()) cargo.rows.push_back({0,{L"空載",L"—",L"0",L"0"}});
            tradenative::show(cargo,false);
        } else if(action==1 || action==2) nativeCargo(car,action==1);
        else if(action==3) {
            for(;;) {
            int dest=nativeChooseCity(car.port>=0 ? car.port : car.origin,car.good,true);
            k=nativeCar(id); if(dest<0 || !k) break;
            bool resume=k->state==WAITING && k->port<0 && dest==k->dest;
            if(nativeConfirm(L"確認續航",{{0,{L"目的市場",nativeCity(dest)}},
                {1,{L"續航及接駁費",nativeNum(resume ? 0 : continuationFee(*k,dest))}},
                {2,{L"付款城市",nativeCity(car.payer)}},{3,{L"通路",L"道路可達時才會扣費並出發。"}}})) {
                nativeExecute("trade go "+std::to_string(id)+" "+std::to_string(dest),L"商隊已繼續行程。",true);notice=g_nativeFeedback;
                break;
            }
            }
        } else if(action==4) {
            k=nativeCar(id); uint8_t* u=k ? liveUnit(*k) : nullptr; if(!u) continue;
            int troops=std::max(0,g_cfg.escort-vcall(u,0x50)),food=std::max(0,g_cfg.food-*(int*)(u+0x20));
            if(nativeConfirm(L"確認補給",{{0,{L"補給城市",nativeCity(car.port)}},
                {1,{L"調出兵力",nativeNum(troops)}},{2,{L"調出兵糧",nativeNum(food)}}}))
                nativeExecute("trade resupply "+std::to_string(id),L"已補充商隊護衛與兵糧。" );
        } else if(action==5) {
            tradeui::Result r; r.toggleUnit=car.unit;r.toggleAuto=!car.autoMarch;applyToggle(r);
            notice=car.autoMarch?L"已改為手動行軍，可再次切換。":L"已改為每旬自動行軍，可再次切換。";
        } else if(action==6) {
            if(nativeConfirm(car.cargo.count()>0 ? L"確認放棄餘貨並結束" : L"確認結束行程",{
                {0,{L"商隊",L"#"+nativeNum(id)}},{1,{L"剩餘貨物",nativeNum(car.cargo.count())+L" 批"}},
                {2,{L"將放棄貨物成本",nativeNum(car.cargo.cost())}},
                {3,{L"結果",L"解散商隊，歸還可回收的兵力、兵糧與金。"}}
            })) if(nativeExecute("trade finish "+std::to_string(id)+(car.cargo.count()>0 ? " discard" : ""),L"行程已結束，收支已歸入帳本。")) return;
        } else if(action==7) nativeLedger(car.ledger,car.cargo.cost());
    }
}
void nativeCaravans(int city=-1) {
    bool all=city<0;tradenative::ViewState view;
    for(;;) {
        buildModel(g_model.force);
        Table t={all?L"全部商隊":nativeCity(city)+L"　出資商隊",L"先列待處理及到港商隊；選擇一隊可直接交易或接管。",{L"行程",L"統領",L"起點",L"目的地",L"狀態",L"載量"},{}};
        for(auto& k:g_model.caravans) if(k.player && (all || k.origin==city)) t.rows.push_back({(int)k.id,{L"#"+nativeNum(k.id),k.temp ? L"臨時商人" : k.leader,
            nativeCity(k.origin),nativeCity(k.dest),k.state==DOCKED ? L"停靠" : k.state==WAITING ? L"等待" : k.transferStage ? L"接駁" : L"行軍",nativeLoad(k.cargo.loadUnits(g_data))+L"/"+nativeNum(k.maxLoad)+L" 格"}});
        std::stable_sort(t.rows.begin(),t.rows.end(),[](const Row& a,const Row& b) {
            auto rank=[](int id) {auto k=nativeCar(id);return !k?3:nativeNeedsDecision(*k)?0:k->state==DOCKED?1:2;};
            return rank(a.id)<rank(b.id);
        });
        if(t.rows.empty())t.rows.push_back({-3,{L"暫無商隊",L"可組建或切換全部",L"",L"",L"",L""},false});
        if(city>=0)t.rows.insert(t.rows.begin(),{-2,{all?L"只看本城":L"查看全部商隊",L"",L"",L"",L"",L""}});
        int id=tradenative::show(t,true,false,&view);if(id==-2){all=!all;view={};continue;}if(id<0)return;nativeManage((uint32_t)id);
    }
}
void nativeAccounts() {
    for(;;) {
        Table t={L"收支帳本",L"選擇行程查看明細；全勢力待入帳金："+nativeNum(g_model.pendingGold),
            {L"行程",L"狀態",L"起點",L"目的地",L"現金淨流量",L"帳本範圍"},{}};
        std::vector<std::pair<Ledger,int64_t>> accounts;
        for(auto& k:g_model.caravans) if(k.player) {
            t.rows.push_back({(int)accounts.size(),{L"#"+nativeNum(k.id),L"進行中",nativeCity(k.origin),nativeCity(k.dest),tradeui::fmtGold(k.ledger.cashNet(),true),k.ledger.complete ? L"完整" : L"遷移後"}});
            accounts.push_back({k.ledger,k.cargo.cost()});
        }
        for(auto& k:g_model.closed) {
            t.rows.push_back({(int)accounts.size(),{L"#"+nativeNum(k.id),L"已結束",nativeCity(k.origin),nativeCity(k.dest),tradeui::fmtGold(k.ledger.cashNet(),true),k.ledger.complete ? L"完整" : L"遷移後"}});
            accounts.push_back({k.ledger,0});
        }
        int recentId=(int)accounts.size();
        t.rows.push_back({recentId,{L"最近交易",L"稅後收入／成本",L"—",L"—",L"—",L"—"}});
        t.rows.push_back({-1,{L"待入帳",nativeNum(g_model.pendingGold),L"全勢力",L"—",L"—",L"—"},false});
        int id=tradenative::show(t); if(id<0) return;
        if(id==recentId) {
            Table recent={L"最近交易",L"收入為稅後金額；損失貨物已計入行程成本。",{L"日期",L"市場",L"商品",L"數量",L"收入",L"成本",L"結果"},{}};
            for(auto& d:g_model.done) recent.rows.push_back({0,{nativeNum(d.day/360+1)+L"年"+nativeNum(d.day%360/30+1)+L"月"+nativeNum(d.day%30+1)+L"日",nativeCity(d.dest),nativeGood(d.good),nativeNum(d.qty),nativeNum(d.net),nativeNum(d.cost),d.flags ? L"損失" : L"成交"},true,nativeGood(d.good)});
            tradenative::show(recent,false);
        } else if(id<recentId) nativeLedger(accounts[id].first,accounts[id].second);
    }
}
void nativeNews() {
    Table t={L"貿易情報",L"外地行情可能過期；市場報價以抵達後看到的行情為準。",{L"類別",L"地區／事件",L"內容",L"期限"},{}};
    for(size_t i=0;i<g_data.seas.size() && g_model.live.p.monsoon;++i) {
        char s=season(g_data,(int)i,g_model.month%12);
        std::wstring value=s=='E' ? L"西南季風：東行順風" : s=='W' ? L"東北季風：西行順風" : s=='T' ? L"轉季：留意風暴" : L"無季風";
        int months=1;while(months<12 && season(g_data,(int)i,(g_model.month+months)%12)==s) ++months;
        t.rows.push_back({0,{L"季風",tradeui::wide(g_data.seas[i].name),value,months<12 ? nativeNum(months)+L"個月後轉季" : L"全年"}});
    }
    for(auto& e:g_model.events) t.rows.push_back({0,{L"市場消息",e.def<g_data.events.size() ? tradeui::wide(g_data.events[e.def].name) : L"事件",tradeui::wide(describe(g_data,e)),nativeNum(std::max(0,e.end-g_model.month))+L"個月"}});
    if(g_model.events.empty()) t.rows.push_back({0,{L"市場消息",L"平靜",L"目前沒有市場事件",L"—"}});
    int fresh=0,known=0;for(int c=0;c<g_data.C;++c) {if(g_model.age[c]==0) ++fresh;if(g_model.known[c]) ++known;}
    t.rows.push_back({0,{L"行情情報",L"即時／已知／全部",nativeNum(fresh)+L" / "+nativeNum(known)+L" / "+nativeNum(g_data.C),L"城市"}});
    tradenative::show(t,false);
}
#include "trade_auto_dialogs.inc"
#include "trade_procurement_dialogs.inc"
void nativeCityTrade(int city,int action) {
    buildModel(baseForce(city));
    switch(action) {
    case 1:nativeMarket(city);break;
    case 2:nativeFormation(city);break;
    case 3:nativeCaravans(city);break;
    case 4:nativeAccounts();break;
    case 5:nativeNews();break;
    case 6:nativeAuto(city);break;
    case 7:nativeAutoPending();break;
    case 8:nativeMilitary(city);break;
    }
    g_model.shipType=CARAVAN;g_model.departureFee=-1;
}
