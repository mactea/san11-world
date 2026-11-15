void nativeMilitaryLedger() {
    Table t={L"軍需採購明細",L"獨立記錄城市軍需支出，不計作商隊虧損；保留最近512筆全世界採購中的我方記錄。",
             {L"日期",L"城市",L"商品",L"批數",L"軍需數量",L"支出"},{}};
    for(auto it=g_purchases.rbegin();it!=g_purchases.rend();++it)if(it->force==g_model.force) {
        auto& r=*it;
        t.rows.push_back({(int)r.id,{nativeNum(r.day/360+1)+L"年"+nativeNum(r.day%360/30+1)+L"月"+nativeNum(r.day%30+1)+L"日",
            nativeCity(r.city),nativeGood(r.good),nativeNum(r.qty),nativeNum((int64_t)r.qty*g_data.unitsPerLot(r.good)),nativeNum(r.cost)},true,nativeGood(r.good)});
    }
    if(t.rows.empty())t.rows.push_back({0,{L"尚無採購",L"—",L"—",L"—",L"—",L"—"},false});
    tradenative::show(t,false);
}
void nativeMilitaryGoods(int city,bool horses) {
    int force=g_model.force;
    for(;;) {
        if(baseForce(city)!=force)return;
        uint8_t* b=base(city);if(!b)return;
        if(!g_militaryReady || g_readOnly) {nativeInfo(L"軍需採購",{{0,{L"狀態",L"目前無法執行軍需採購。"}}});return;}
        Table t={nativeCity(city)+(horses?L"　採購軍馬":L"　採購兵裝"),L"每批100件／匹，使用本城資金；市場與原生兵裝庫存分開。",
                 {L"商品",L"市場/批",L"買1批/金",L"城市庫存",L"城市上限"},{}};
        for(int g=0;g<g_data.G;++g) {
            int w=g_data.militaryWeapon(g);if(w<0 || (w==4)!=horses)continue;
            int have=baseWeapon(b,w),cap=weaponCapacity(b,w);
            auto cost=g_mk.quote(city,g,1,true,0);
            bool enabled=militaryQuote(g_mk,city,g,1,baseGold(b),std::max(0,cap-have))>=0;
            t.rows.push_back({g,{nativeGood(g),nativeNum(g_mk.available(city,g)),cost>=0?nativeNum(cost):L"—",nativeNum(have),nativeNum(cap)},enabled,nativeGood(g)});
        }
        int good=tradenative::show(t);if(good<0)return;
        int w=g_data.militaryWeapon(good);if(w<0 || baseForce(city)!=force)continue;
        int have=baseWeapon(b,w),cap=weaponCapacity(b,w),cash=baseGold(b),lo=0;
        int hi=std::min(g_mk.available(city,good),std::max(0,cap-have)/100);
        while(lo<hi) {int mid=lo+(hi-lo+1)/2;if(militaryQuote(g_mk,city,good,mid,cash,cap-have)>=0)lo=mid;else hi=mid-1;}
        int qty=tradenative::number(L"採購批數（每批100件／匹）",1,lo,nativeGood(good));if(qty<0)continue;
        auto cost=militaryQuote(g_mk,city,good,qty,cash,cap-have);if(cost<0)continue;
        uint32_t token=g_nextPurchase;
        if(!nativeConfirm(L"確認軍需採購",{{0,{L"付款城市",nativeCity(city)}},{1,{L"商品",nativeGood(good)}},
            {2,{L"採購批數",nativeNum(qty)}},{3,{L"增加軍需",nativeNum(qty*100)}},{4,{L"總費用",nativeNum(cost)}},
            {5,{L"採購後城市庫存",nativeNum(have+qty*100)+L" / "+nativeNum(cap)}},{6,{L"採購後資金",nativeNum(cash-cost)}}},nativeGood(good)))continue;
        nativeExecute("trade procure "+std::to_string(city)+" "+std::to_string(good)+" "+std::to_string(qty)+" "+std::to_string(cost)+" "+std::to_string(token),
                      L"已扣除本城資金及市場現貨，軍需已入城市兵裝庫。" );
    }
}
void nativeMilitary(int city) {
    for(;;) {
        Table t={nativeCity(city)+L"　軍需採購",L"採購市場現貨補充本城軍備。軍糧兌換尚未開放。",{L"項目",L"說明"},
                 {{0,{L"採購兵裝",L"槍、戟、弩"}},{1,{L"採購軍馬",L"馬"}},{2,{L"採購明細",L"我方城市軍需支出"}}}};
        int action=tradenative::show(t);if(action<0)return;
        if(action==2)nativeMilitaryLedger();else nativeMilitaryGoods(city,action==1);
    }
}
