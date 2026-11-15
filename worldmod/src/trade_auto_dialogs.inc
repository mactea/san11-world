void nativeAuto(int city) {
    int force=baseForce(city);auto saved=autoPlan(force,city);
    tradeauto::Plan edit=saved?*saved:tradeauto::Plan();
    edit.origin=city;edit.force=force;edit.budget.advanceMonth(today()/30);
    if(!saved)edit.budget.limit=4000;
    auto initial=edit;
    for(auto& d:g_autoDrafts)if(d.origin==city && d.force==force) {
        edit.preferredDestination=d.preferredDestination;edit.preferredGoods=d.preferredGoods;
        edit.budget.limit=d.budget.limit;edit.enabled=d.enabled;edit.shipType=d.shipType;
    }
    std::wstring notice;tradenative::ViewState view;
    for(;;) {
        if(baseForce(city)!=force)return;
        buildModel(force);g_model.shipType=edit.shipType;
        std::wstring goods;
        for(int g:edit.preferredGoods) {if(!goods.empty())goods+=L"、";goods+=nativeGood(g);}
        auto current=autoPlan(force,city);
        int64_t occupied=current && current->budget.month==today()/30 ? current->budget.occupied : 0;
        bool dirty=!sameAutoSettings(edit,initial);
        Table t={L"自動往返貿易",dirty?L"尚未保存修改；試算使用目前設定。返回可保留草稿。":notice.empty()?L"配置 → 試算 → 保存並啟用本城；僅保存不立即評估。":notice,{L"項目",L"內容"},{
            {9,{L"運輸方式／船型",transportName(edit.shipType)}},
            {1,{L"每月預算",nativeNum(edit.budget.limit)+L"金（採購與費用預留）"}},
            {0,{L"優先目標",edit.preferredDestination<0?L"不限，系統擇優":nativeCity(edit.preferredDestination)+L"；不合適時自動擇優"}},
            {2,{L"偏好商品（多選）",goods.empty()?L"不限定，系統配貨":goods}},
            {3,{L"保存後新派遣",edit.enabled?L"啟用":L"暫停；已有商隊繼續往返"}},
            {5,{L"試算目前設定",L"不扣款，查看商品、投入、預計淨利及未派遣原因"}},
            {6,{L"保存並啟用本城",L"立即評估本城新派遣，不推動其他城市或在途交易"},!g_readOnly},
            {13,{L"僅保存設定",L"按上方啟用／暫停狀態保存，等待正常旬調度"},!g_readOnly},
            {14,{L"查看本城商隊",L"管理、接管或處理已到港商隊"}},
            {4,{L"本月已占用／剩餘",nativeNum(occupied)+L" / "+nativeNum(std::max<int64_t>(0,edit.budget.limit-occupied))},false},
            {7,{L"商隊數／上限",nativeNum(cityAutoCount(force,city))+L" / "+nativeNum(cityAutoLimit(force,city))},false},
            {12,{L"本城商才",cityMerchantSkill(force,city)?L"已生效：上限加倍，費用八折，正毛利加兩成":L"尚無：本城任職武將具備商才時生效"},false},
            {10,{L"計畫來源",edit.delegated?L"軍團委任；保存後改為玩家設定":L"玩家設定"},false},
            {8,{L"執行狀態",current?current->status:L"尚未保存"},false}}};
        int action=tradenative::show(t,true,false,&view);
        if(action<0) {
            if(!dirty)return;
            int leave=nativeLeaveDraft();if(leave<0 || leave==2)continue;
            g_autoDrafts.erase(std::remove_if(g_autoDrafts.begin(),g_autoDrafts.end(),[&](const tradeauto::Plan& p){return p.force==force && p.origin==city;}),g_autoDrafts.end());
            if(leave==0)g_autoDrafts.push_back(edit);
            return;
        }
        if(action==9) {int type=nativeChooseTransport(city);if(type>=0)edit.shipType=type;}
        if(action==0) {int c=nativeChooseCity(city,-1,true,true);if(c>=0 || c==-2)edit.preferredDestination=c==-2?-1:c;}
        if(action==1) {int n=tradenative::number(L"每月預算（0表示不投入）",(int)edit.budget.limit,100000,L"",0);if(n>=0)edit.budget.limit=n;}
        if(action==2) {
            Table gt={L"偏好商品",L"連續勾選後按決定；全不選表示系統配貨。返回取消本次多選。",{L"商品",L"本城庫存／批"},{}};
            for(int g=0;g<g_data.G;++g)gt.rows.push_back({g,{nativeGood(g),nativeNum(g_model.live.available(city,g))},true,nativeGood(g)});
            std::vector<int> chosen(edit.preferredGoods.begin(),edit.preferredGoods.end());
            if(tradenative::show(gt,true,false,nullptr,&chosen)>=0)edit.preferredGoods.assign(chosen.begin(),chosen.end());
        }
        if(action==3)edit.enabled=!edit.enabled;
        if(action==14)nativeCaravans(city);
        if(action==5) {
            auto preview=edit;preview.budget.advanceMonth(today()/30);preview.budget.occupied=occupied;
            std::wstring reason;auto offers=autoOffers(preview,city,-1,&reason);int best=autoBest(offers,edit.preferredDestination);
            if(best<0)nativeInfo(L"商路試算",{{0,{L"尚不能派遣",reason}},{1,{L"下一步",L"可調整預算、船型或目的地，亦可等待行情恢復"}}});
            else {auto& o=offers[best];std::wstring cargo;for(auto l:o.order)cargo+=nativeGood(l.good)+L" "+nativeNum(l.qty)+L"批 ";
                nativeInfo(L"商路試算",{{0,{L"銷售城市",nativeCity(o.rank.destination)}},{1,{L"商品組合",cargo}},
                    {2,{L"貨款及費用預留",nativeNum(o.quote.cost+o.reserve)}},{3,{L"預計淨利",nativeNum(o.rank.roundTripNet)}},
                    {4,{L"往返地理粗估",nativeNum(o.rank.turns)+L"旬；實際出發仍須尋路成功"}},
                    {6,{L"商才收益加成",nativeNum(o.quote.merchantBonus)+L"金（已含於預計淨利）"}},
                    {5,{L"說明",L"依目前設定與已知行情估算，修改設定後請重新試算"}}});}
        }
        if((action==6 || action==13) && !g_readOnly) {
            auto proposed=edit;if(action==6)proposed.enabled=true;
            if(!nativeConfirm(action==6?L"保存並啟用本城":L"僅保存設定",{
                {0,{L"出資城",nativeCity(city)}},{1,{L"每月預算",nativeNum(proposed.budget.limit)}},
                {2,{L"狀態",proposed.enabled?L"啟用新派遣":L"暫停新派遣，已有商隊繼續"}},
                {3,{L"本次影響",action==6?L"只立即評估本城；符合條件時扣款並派出商隊":L"現在不派遣，等待正常旬調度"}},
                {4,{L"運輸方式",transportName(proposed.shipType)}}}))continue;
            {
                Lock l;if(baseForce(city)!=force)return;
                auto live=autoPlan(force,city);proposed.delegated=false;
                if(live) {
                    live->budget.advanceMonth(today()/30);proposed.budget.month=live->budget.month;
                    proposed.budget.occupied=live->budget.occupied;proposed.dispatchedMonth=live->dispatchedMonth;*live=proposed;
                } else {proposed.budget.advanceMonth(today()/30);proposed.budget.occupied=0;g_autoPlans.push_back(proposed);}
                if(action==6)autoTick(city,force);
                else {auto p=autoPlan(force,city);p->status=p->enabled?L"已保存，等待下旬評估":L"新派遣已暫停；已有商隊繼續";}
            }
            auto result=autoPlan(force,city);edit=*result;initial=edit;notice=L"已保存："+result->status;
            g_autoDrafts.erase(std::remove_if(g_autoDrafts.begin(),g_autoDrafts.end(),[&](const tradeauto::Plan& p){return p.force==force && p.origin==city;}),g_autoDrafts.end());
        }
    }
}
std::wstring nativeAutoIssue(const Caravan& k) {
    if(k.tempReleased)return L"已入城並回收商人，請接管後結算餘貨";
    switch(k.automatic.issue) {
    case tradeauto::Issue::None:return L"正常執行，可接管";
    case tradeauto::Issue::Unprofitable:return L"目前無合適盈利交易";
    case tradeauto::Issue::Budget:return L"預算或預留費用不足";
    case tradeauto::Issue::Warehouse:return L"目的市場庫容不足";
    case tradeauto::Issue::Route:return L"道路或市場不可用";
    case tradeauto::Issue::Ownership:return L"出資城已易主，請接管後選擇我方城市";
    case tradeauto::Issue::Resources:return L"商隊或採購資源不足";
    }
    return L"需要處理";
}
void nativeAutoPending() {
    tradenative::ViewState view;
    for(;;) {
        Table t={L"自動貿易待處理",L"停靠期間仍可能消耗兵糧；沒有確認不會額外採購或棄貨。",{L"商隊",L"出資城",L"原因"},{}};
        for(auto& k:g_car)if(k.force==g_model.force && nativeNeedsDecision(k))
            t.rows.push_back({(int)k.id,{L"#"+nativeNum(k.id),nativeCity(k.origin),nativeAutoIssue(k)}});
        if(t.rows.empty()) {nativeInfo(L"沒有待處理商隊",{{0,{L"狀態",L"正常商隊可在商隊管理查看或接管。"}}});return;}
        int id=tradenative::show(t,true,false,&view);if(id<0)return;
        Caravan* k=nativeCar(id);if(!k)continue;
        bool canReturn=k->port>=0 && k->port!=k->origin && liveUnit(*k) && baseForce(k->origin)==k->force;
        Table options={L"處理商隊",L"建議有貨時保留貨物返程，無貨時等待下月重新評估。",{L"操作",L"說明"},{
            {0,{L"等下月重評",L"不立即花費"},k->automatic.issue!=tradeauto::Issue::None},
            {1,{L"空載返程",L"需有效商隊，出資城仍屬我方"},k->qty==0 && canReturn},
            {2,{L"帶貨返程",L"需有效商隊，保留現有貨物"},k->qty>0 && canReturn},
            {3,{L"手動接管",L"退回未用預留款，之後由商隊管理操作"}}}};
        int action=tradenative::show(options,true);if(action<0 || g_readOnly)continue;
        if(action==3) {if(nativeTakeover(id))nativeManage(id);continue;}
        if(!nativeConfirm(L"確認處理",{{0,{L"商隊",L"#"+nativeNum(id)}},{1,{L"操作",options.rows[action].cells[0]}}}))continue;
        k=nativeCar(id);if(!k)continue;
        if(action==0) {Lock l;k->automatic.decide(tradeauto::Decision::RetryNextMonth,today()/30,false,false);continue;}
        if(baseForce(k->origin)!=k->force) {nativeInfo(L"無法返程",{{0,{L"原因",L"出資城已易主，請先手動接管並選擇我方城市。"}}});continue;}
        std::string error;
        {Lock l;error=depart(*k,k->origin);
            if(error.empty()) {k->automatic.stage=tradeauto::Stage::Returning;k->automatic.issue=tradeauto::Issue::None;k->automatic.retryMonth=-1;}}
        if(!error.empty())nativeInfo(L"無法返程",{{0,{L"原因",nativeError(error)}}});
    }
}
