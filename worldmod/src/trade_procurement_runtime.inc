// Included in trade.cpp after native base helpers; no officer or unit creation.
std::string procureMilitary(int force,int city,int good,int qty,int64_t expected,uint32_t token) {
    Lock lock;
    if(g_readOnly || !g_militaryReady)return "military read-only";
    if(!token || token!=g_nextPurchase || token==UINT32_MAX)return "military duplicate order";
    if(city<0 || city>=g_data.C || force<0 || force>=g_s.R || baseForce(city)!=force)return "military ownership changed";
    int w=g_data.militaryWeapon(good);if(w<0)return "invalid military good";
    uint8_t* b=base(city);if(!b)return "military ownership changed";
    int have=baseWeapon(b,w),cap=weaponCapacity(b,w);
    g_purchases.reserve(PURCHASE_MAX+1); // allocate before changing any resources
    const char* error=commitMilitary(g_mk,city,good,qty,baseGold(b),std::max(0,cap-have),expected,
        [&](bool weapon,int delta){return weapon ? addWeapon(PROC,b,w,delta) : addGold(PROC,b,delta);});
    if(*error) {
        if(!strcmp(error,"military rollback failed")) {g_readOnly=true;g_log("trade: ERROR: military native rollback failed");}
        return error;
    }
    g_purchases.push_back({g_nextPurchase++,today(),force,city,good,qty,expected});
    if(g_purchases.size()>PURCHASE_MAX)g_purchases.erase(g_purchases.begin());
    g_info.see(force,city,&g_mk);
    g_log("trade: military purchase city %d force %d good %d lots %d gold %lld",city,force,good,qty,expected);
    return "";
}
void aiMilitaryMonth() {
    if(!g_militaryReady || g_readOnly)return;
    for(int c=0;c<g_data.C;++c) {
        int force=baseForce(c);if(force<0 || force>=g_s.R || isPlayerForce(force))continue;
        uint8_t* b=base(c);if(!b)continue;
        int cash=baseGold(b),budget=std::min(2000,std::min(cash/10,cash-2000));
        int target=std::min(10000,vcall(b,0x50)),have=0;
        for(int w=1;w<=4;++w)have+=baseWeapon(b,w);
        int gap=target-have;if(budget<=0 || gap<100)continue;
        int chosen=-1;int64_t best=INT64_MAX;
        for(int g=0;g<g_data.G;++g) {
            int w=g_data.militaryWeapon(g);if(w<0)continue;
            auto price=militaryQuote(g_mk,c,g,1,budget,std::max(0,weaponCapacity(b,w)-baseWeapon(b,w)));
            if(price>=0 && price<best){best=price;chosen=g;}
        }
        if(chosen<0)continue;
        int w=g_data.militaryWeapon(chosen),room=std::max(0,weaponCapacity(b,w)-baseWeapon(b,w));
        int qty=std::min(gap/100,std::min(20,std::min(room/100,g_mk.available(c,chosen))));
        while(qty>0 && militaryQuote(g_mk,c,chosen,qty,budget,room)<0)--qty;
        if(qty>0)procureMilitary(force,c,chosen,qty,g_mk.quote(c,chosen,qty,true,0),g_nextPurchase);
    }
}
