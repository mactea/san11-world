// Included after buildModel: all mutations run on the game's existing trade thread.
tradeauto::Plan* autoPlan(int force,int origin) {
    for(auto& p:g_autoPlans)if(p.force==force && p.origin==origin)return &p;
    return nullptr;
}
bool delegatedCity(int city,int force) {
    auto b=base(city);if(!b || vcall(b,0x40)!=force || !isPlayerForce(force))return false;
    int id=vcall(b,0x44);if(id<0 || id>=g_s.R+5)return false;
    auto cr=corpsRec(WORLD,id);
    // Native player-control predicate 0x47a6d0 tests corps +8 == 1.
    // This is the force-local corps number, not the global corps slot ID.
    return objValid(cr) && *(int*)(cr+4)==force && *(int*)(cr+8)>=2 && *(int*)(cr+8)<=8;
}
void syncDelegatedPlan(int city,int force,bool delegated) {
    auto p=autoPlan(force,city);
    if(!p && delegated) {
        tradeauto::Plan plan;plan.origin=city;plan.force=force;plan.delegated=true;
        plan.budget.limit=4000;plan.budget.advanceMonth(today()/30);
        g_autoPlans.push_back(plan);p=&g_autoPlans.back();
    }
    if(!p || !p->delegated)return; // Explicit plans, including pauses, always win.
    p->enabled=delegated;
    if(!delegated)p->status=L"已收回直轄，停止委任新派遣";
}
void refreshDelegatedPlans() {
    for(int city=0;city<g_data.C;++city) {
        int force=baseForce(city);
        if(force>=0 && isPlayerForce(force))syncDelegatedPlan(city,force,delegatedCity(city,force));
    }
}
void autoRefund(Caravan& k) {
    auto& a=k.automatic;if(!a.caravan)return;
    auto p=autoPlan(k.force,k.origin);
    tradeauto::Budget detached;
    if(p)p->budget.advanceMonth(today()/30);
    int64_t refund=a.fees.refund(p?p->budget:detached);
    if(refund)credit(k.force,k.origin,refund);
}
bool autoPay(Caravan& k,int64_t amount) {
    if(amount<0 || amount>INT_MAX)return false;
    if(k.automatic.caravan)return k.automatic.fees.spend(amount);
    auto home=payee(k.force,k.origin);
    if(!home || baseGold(home)<amount)return false;
    int paid=-addGold(PROC,home,-(int)amount);
    if(paid!=amount) {credit(k.force,k.origin,paid);return false;}
    return true;
}
void autoBlock(Caravan& k,tradeauto::Issue issue,const char* reason) {
    if(k.automatic.block(issue))g_log("trade: automatic voyage %u needs decision: %s",k.id,reason);
    k.reason=reason;
    // Transient obstacles are re-evaluated next month for every plan; a blocked
    // voyage must not hold its city's automatic slot until the player notices.
    if(issue!=tradeauto::Issue::Ownership)
        k.automatic.retryMonth=today()/30+1;
}
struct AutoOffer {
    std::vector<OrderLine> order;
    tradeui::BasketPlan quote;
    tradeauto::Candidate rank;
    int64_t reserve=0;
    int shipType=0;
};
std::vector<AutoOffer> autoOffersModel(const tradeauto::Plan& plan,int source,tradeui::Model model,int64_t cash,int fixed=-1,std::wstring* reason=nullptr) {
    model.shipType=plan.shipType;
    model.fundingCity=plan.origin;
    bool merchant=plan.origin>=0 && plan.origin<(int)model.merchantCities.size() && model.merchantCities[plan.origin];
    model.departureFee=merchantFee(transportFee(g_mk.p,plan.shipType,fixed>=0),merchant);
    // Reserve the full future return fee: the organizer may leave before arrival.
    int legFee=transportFee(g_mk.p,plan.shipType,true);
    int64_t available=std::min<int64_t>(plan.budget.available(),std::max<int64_t>(0,cash-tradeauto::CITY_RESERVE));
    std::vector<AutoOffer> offers;
    auto reject=[&](const wchar_t* why) {if(reason)*reason=why;return offers;};
    if(reason)*reason=L"預算內沒有達到淨收益10%門檻的已知開放商路";
    if(!validTransport(plan.shipType) || (plan.shipType==MERCHANT_SHIP && !tradeui::shipMarket(model,source)))return reject(L"海運需近岸水面或20格陸路內的同勢力港口");
    if(cash<=tradeauto::CITY_RESERVE)return reject(L"城市須保留2000金，目前無可用資金");
    if(available<=model.departureFee+(fixed<0?legFee:0))return reject(L"本月剩餘預算或城市可用金不足以支付費用");
    int64_t foodCost=0;
    if(fixed<0) {
        if(model.owner[source]!=plan.force)return reject(L"出資城已非我方所有");
        if(model.troops[source]<model.escortNeeded || model.food[source]<model.foodNeeded)return reject(L"護衛兵力或出發兵糧不足");
        if(model.tempLive>=model.cfg.tempMax)return reject(L"臨時商隊數量已達上限");
        if(model.freeSpecials-1<model.cfg.tempKeep)return reject(L"特殊武將空位不足，暫不能建立商人");
        foodCost=tradeauto::provisionsValue(g_cfg.food,source<(int)model.grainRate.size()?model.grainRate[source]:0,TEMP_POL);
        if(foodCost<0)return reject(L"尚無有效的本城糧價，暫不能估算兵糧成本");
    }
    model.gold[source]=(int)std::min<int64_t>(available,INT_MAX);
    if(fixed>=0) {
        // The existing caravan buys at a foreign port without forming a new
        // native unit there. Only the local quote's formation checks are bypassed.
        model.owner[source]=plan.force;
        model.troops[source]=g_cfg.escort;model.food[source]=100000;
        model.tempLive=0;model.freeSpecials=model.cfg.tempKeep+1;
        // Keep the selected transport and use only its next-leg fee.
    }
    for(int dest=0;dest<g_data.C;++dest) {
        if(dest==source || (fixed>=0 && dest!=fixed) || !model.known[dest] || model.stance[dest]<=0)continue;
        if(!tradeui::transportRoute(model,source,dest))continue;
        for(int preferred=0;preferred<2;++preferred) {
            if(preferred && plan.preferredGoods.empty())continue;
            auto limited=model;
            if(preferred)for(int g=0;g<g_data.G;++g)
                if(std::find(plan.preferredGoods.begin(),plan.preferredGoods.end(),g)==plan.preferredGoods.end())limited.live.stock[source*g_data.G+g]=0;
            auto order=tradeui::recommendOrder(limited,source,dest,-1);
            while(!order.empty()) {
                auto q=tradeui::basketPlan(model,source,dest,order,-1);
                int64_t reserve=q.duty+q.transitLimit+(fixed<0?legFee+tradeui::transferFee(model,source,dest):0);
                if(q.ok && q.quoted && q.cost+reserve<=available) {
                    int turns=std::max(1,q.eta)*(fixed<0?2:1);
                    // Value existing provisions at the native replacement price;
                    // this is an economic cost, not an additional gold withdrawal.
                    int required=provisionFood(g_cfg.escort,turns,g_cfg.food);
                    if(required<0 || (fixed<0 && (int64_t)model.food[source]<required+(plan.delegated?9000LL:0LL)))break;
                    if(fixed<0)foodCost=tradeauto::provisionsValue(required,model.grainRate[source],TEMP_POL);
                    int64_t net=q.revenue+q.merchantBonus-q.cost-reserve-foodCost;
                    tradeauto::Candidate rank{dest,turns,q.cost+reserve+foodCost,q.profit,net,true,true,preferred!=0};
                    if(rank.eligible())offers.push_back({order,q,rank,reserve,plan.shipType});
                    break;
                }
                if(--order.back().qty==0)order.pop_back();
            }
        }
    }
    return offers;
}
std::vector<AutoOffer> autoOffers(const tradeauto::Plan& plan,int source,int fixed=-1,std::wstring* reason=nullptr,const Caravan* existing=nullptr) {
    buildModel(plan.force);auto home=base(plan.origin);
    if(existing && existing->shipType==MERCHANT_SHIP && existing->arrivalBase>=0) {
        auto a=shipAccessAt(source,existing->arrivalBase);
        g_model.waterAccess[source]=a.base>=0;g_model.portBase[source]=a.base;g_model.portSteps[source]=a.steps;
    }
    return autoOffersModel(plan,source,g_model,home?baseGold(home):0,fixed,reason);
}
int autoBest(const std::vector<AutoOffer>& offers,int preferred) {
    std::vector<tradeauto::Candidate> candidates;for(auto& o:offers)candidates.push_back(o.rank);
    return tradeauto::choose(candidates,preferred);
}
bool autoFund(tradeauto::Plan& p,tradeauto::Advance& advance,int64_t amount) {
    if(baseForce(p.origin)!=p.force || !advance.fund(p.budget,amount))return false;
    auto home=base(p.origin);
    if(!home || baseGold(home)-tradeauto::CITY_RESERVE<amount) {advance.refund(p.budget);return false;}
    int paid=-addGold(PROC,home,-(int)amount);
    if(paid!=amount) {advance.refund(p.budget);credit(p.force,p.origin,paid);return false;}
    return true;
}
bool autoReserve(Caravan& k,int64_t amount) {
    if(k.automatic.fees.balance>=amount)return true;
    auto p=autoPlan(k.force,k.origin);if(!p)return false;
    p->budget.advanceMonth(today()/30);
    autoRefund(k);settleCredits();
    return autoFund(*p,k.automatic.fees,amount);
}
// Another open market that takes the whole cargo above cost and that the unit can actually reach.
int autoAlternateMarket(Caravan& k,uint8_t* unit) {
    int pp,cp;MerchantBonus mb;leaderOrTemp(k,&pp,&cp,&mb);
    const int16_t* at=posOf(unit);
    std::vector<std::pair<int64_t,int>> ranked;
    for(int c=0;c<g_data.C;++c) {
        if(c==k.port || c==k.dest || c==k.origin)continue;
        int owner=baseForce(c);if(owner<0 || stance(k.force,owner)==0)continue;
        int lo,hi;cityPos(c,&lo,&hi);int dist=hexDist(at[0],at[1],lo,hi);
        if(dist>150 || (k.shipType==MERCHANT_SHIP && shipAccess(c).base<0))continue;
        int64_t net=0;bool ok=true;
        for(auto& lot:k.cargo.lots) {
            if(g_mk.room(c,lot.good)<lot.qty) {ok=false;break;}
            int64_t quote=saleQuote(g_mk,c,lot.good,lot.qty,pp);
            int64_t duty=stance(k.force,owner)==2?0:(quote*taxPct(g_data,c,g_cfg.tax,g_cfg.taxHub)+99)/100;
            if(quote-duty<lot.cost) {ok=false;break;}
            net+=quote-duty-lot.cost;
        }
        if(ok)ranked.push_back({net*1000/(dist+20),c});
    }
    std::sort(ranked.rbegin(),ranked.rend());
    for(size_t i=0;i<ranked.size() && i<8;++i) {           // native route queries are the expensive part
        std::vector<uint32_t> path;int cost=0;
        if(marketRoute(unit,ranked[i].second,&path,&cost))return ranked[i].second;
    }
    return -1;
}
void autoVoyageStep(Caravan& k);
void autoVoyage(Caravan& k) {
    using namespace tradeauto;
    auto& a=k.automatic;
    if(!a.caravan || !a.managed || k.state==FINISHED)return;
    // Saves from before automatic retries hold blocked voyages with no retry date.
    if(a.issue!=Issue::None && a.issue!=Issue::Ownership && a.retryMonth<0)a.retryMonth=today()/30;
    bool due=a.retryMonth>=0 && today()/30>=a.retryMonth;
    autoVoyageStep(k);
    // Still blocked on a due retry after nine turns without progress: carry the cargo home.
    if(!due || !a.managed || a.issue==Issue::None || a.issue==Issue::Ownership || k.transferStage ||
       (k.state!=DOCKED && k.state!=WAITING) || k.port==k.origin || today()-k.lastProgress<90 ||
       !liveUnit(k) || baseForce(k.origin)!=k.force)return;
    if(k.qty>0) {
        // Prefer selling the cargo at another market over hauling it home.
        int other=autoAlternateMarket(k,liveUnit(k));
        if(other>=0 && autoReserve(k,continuationFee(k,other)+k.cargo.cost()*g_mk.p.transitCap/100) && depart(k,other).empty()) {
            a.stage=Stage::Outbound;a.issue=Issue::None;a.retryMonth=-1;
            g_log("trade: automatic voyage %u diverts its cargo to %s after repeated obstacles",k.id,cityName(other));
            return;
        }
    } else if(k.state==DOCKED && a.issue==Issue::Route) {
        // Nothing aboard and no way home: settle here instead of holding the slot.
        if(finishCaravan(k,false).empty())g_log("trade: automatic voyage %u settled at %s: empty with no route home",k.id,cityName(k.port));
        return;
    }
    if(!autoReserve(k,continuationFee(k,k.origin)))return;
    auto error=depart(k,k.origin);
    if(!error.empty())return;
    a.stage=Stage::Returning;a.issue=Issue::None;a.retryMonth=-1;
    g_log("trade: automatic voyage %u returns home with its cargo after repeated obstacles",k.id);
}
void autoVoyageStep(Caravan& k) {
    using namespace tradeauto;
    auto& a=k.automatic;
    if(!a.caravan || !a.managed || k.state==FINISHED)return;
    auto p=autoPlan(k.force,k.origin);
    if(!p || baseForce(k.origin)!=k.force) {if(p)p->enabled=false;autoBlock(k,Issue::Ownership,"origin ownership changed; choose a new funding city");return;}
    p->budget.advanceMonth(today()/30);bool retry=a.retry(today()/30);
    if(a.issue!=Issue::None)return;
    if(retry && k.state==WAITING && k.port<0) {
        if(!autoReserve(k,k.transitLimit-k.transitPaid)) {autoBlock(k,Issue::Budget,"monthly transit budget exhausted");return;}
        auto error=depart(k,k.dest);
        if(!error.empty())autoBlock(k,Issue::Route,error.c_str());
        return;
    }
    if(k.state==WAITING) {autoBlock(k,Issue::Route,"route or market is blocked");return;}
    if(k.state!=DOCKED)return;
    if(a.stage==Stage::Complete) {finishCaravan(k,false);return;}
    // Purchases already committed on the return leg must never run again after
    // a failed departure, a retry or a save/load at this port.
    if(a.stage==Stage::Returning && k.port!=k.origin) {
        auto error=depart(k,k.origin);
        if(!error.empty())autoBlock(k,Issue::Route,error.c_str());
        return;
    }
    a.arrive(k.port==k.origin);
    if(a.stage==Stage::Selling || a.stage==Stage::HomeSelling) {
        int64_t needed=a.stage==Stage::Selling?continuationFee(k,k.origin):0;
        for(auto lot:k.cargo.lots) {
            if(g_mk.room(k.port,lot.good)<lot.qty) {autoBlock(k,Issue::Warehouse,"warehouse cannot accept the remaining cargo");return;}
            int pp,cp;MerchantBonus mb;leaderOrTemp(k,&pp,&cp,&mb);
            int64_t quote=saleQuote(g_mk,k.port,lot.good,lot.qty,pp);
            int64_t duty=stance(k.force,baseForce(k.port))==2?0:(quote*taxPct(g_data,k.port,g_cfg.tax,g_cfg.taxHub)+99)/100;
            needed+=duty;
            if(quote-duty<lot.cost) {autoBlock(k,Issue::Unprofitable,"arrival quote is below purchase cost");return;}
        }
        if(!autoReserve(k,needed)) {autoBlock(k,Issue::Budget,"monthly sales tax budget exhausted");return;}
        auto lots=k.cargo.lots;
        for(auto lot:lots) {auto error=tradeCargo(k,lot.good,lot.qty,false);if(!error.empty()) {autoBlock(k,Issue::Budget,error.c_str());return;}}
        a.soldAll();
        if(a.stage==Stage::Complete) {finishCaravan(k,false);return;}
    }
    if(a.stage!=Stage::ReturnBuying)return;
    if(!liveUnit(k)) {autoBlock(k,Issue::Resources,"merchant was absorbed into the city; manual settlement required");return;}
    std::vector<uint32_t> supplyRoute;int supplyCost=0;
    if(!marketRoute(liveUnit(k),k.origin,&supplyRoute,&supplyCost)) {autoBlock(k,Issue::Route,"no safe return route");return;}
    auto supplyError=supplyForRoute(k,liveUnit(k),supplyRoute,k.origin);
    if(!supplyError.empty()) {autoBlock(k,Issue::Resources,supplyError.c_str());return;}
    auto returning=*p;returning.shipType=k.shipType; // A changed plan never converts an existing ship.
    // A recalled district finishes the voyage without committing to another purchase.
    auto offers=p->delegated && !p->enabled ? std::vector<AutoOffer>() : autoOffers(returning,k.port,k.origin,nullptr,&k);
    int pick=autoBest(offers,k.origin);
    if(pick<0 && p->delegated) {
        a.stage=Stage::Returning;
        auto error=depart(k,k.origin); // The outbound budget already reserved the return departure fee.
        if(!error.empty())autoBlock(k,Issue::Route,error.c_str());
        return;
    }
    if(pick<0) {autoBlock(k,Issue::Unprofitable,"no profitable return cargo within the monthly budget");return;}
    auto offer=offers[pick];std::vector<uint32_t> route;int cost=0;
    if(!marketRoute(liveUnit(k),k.origin,&route,&cost)) {autoBlock(k,Issue::Route,"no reachable return market");return;}
    autoRefund(k);settleCredits();
    Caravan before=k;auto oldIndex=g_mk.idx;auto oldStock=g_mk.stock;
    if(!autoFund(*p,a.fees,offer.quote.cost+offer.reserve)) {autoBlock(k,Issue::Budget,"monthly return budget exhausted");return;}
    for(auto line:offer.order) {
        auto error=tradeCargo(k,line.good,line.qty,true);
        if(!error.empty()) {
            // Buying uses only the prepaid account. Restore the whole basket,
            // including the spent portion, before refunding that account once.
            a.fees.balance=offer.quote.cost+offer.reserve;
            autoRefund(k);k=before;g_mk.idx=oldIndex;g_mk.stock=oldStock;
            g_info.see(k.force,k.port,&g_mk);
            autoBlock(k,Issue::Resources,error.c_str());return;
        }
    }
    a.returnPurchased();
    auto error=depart(k,k.origin);
    if(!error.empty()) {autoBlock(k,Issue::Route,error.c_str());return;}
}
void autoTick(int city,int force) {
    if(g_readOnly)return;
    Lock l;
    if(city<0) {
        refreshDelegatedPlans();
        for(auto& k:g_car)autoVoyage(k);
    }
    for(auto& p:g_autoPlans) {
        if(city>=0 && (p.origin!=city || p.force!=force))continue;
        p.budget.advanceMonth(today()/30);
        if(baseForce(p.origin)!=p.force) {p.enabled=false;continue;}
        if(!isPlayerForce(p.force))continue;
        if(!p.enabled) {p.status=L"新派遣已暫停";continue;}
        if(!p.mayDispatch(today()/30)) {p.status=L"本月已派遣，下月再評估";continue;}
        int active=cityAutoCount(p.force,p.origin),limit=cityAutoLimit(p.force,p.origin),launched=0;
        if(active>=limit)p.status=L"已有"+std::to_wstring(active)+L"支自動商隊，上限"+std::to_wstring(limit)+L"支，等待返程";
        while(active<limit) {
            if(p.delegated) {
                auto home=base(p.origin);
                if(!home || baseGold(home)<6000 || baseFood(home)<g_cfg.food+9000 || vcall(home,0x50)<g_cfg.escort+5000) {
                    p.status=L"委任城需6000金，並在出發後保留5000兵、9000糧";break;
                }
            }
            auto offers=autoOffers(p,p.origin,-1,&p.status);bool sent=false;
            if(p.delegated) {
                // A delegated district has no player-chosen vessel: compare both and let the better voyage leave.
                auto other=p;other.shipType=p.shipType==MERCHANT_SHIP?CARAVAN:MERCHANT_SHIP;
                auto more=autoOffers(other,p.origin);
                offers.insert(offers.end(),more.begin(),more.end());
            }
            while(!offers.empty()) {
                int best=autoBest(offers,p.preferredDestination);if(best<0)break;
                auto offer=offers[best];tradeauto::Voyage a;a.origin=p.origin;a.force=p.force;
                if(!p.budget.charge(offer.quote.cost)) {p.status=L"本月預算不足";break;}
                if(!autoFund(p,a.fees,offer.reserve)) {p.budget.rollback(p.budget.month,offer.quote.cost);p.status=L"本月預算或城市資金不足以預留費用";break;}
                auto error=createCaravanOrder(p.origin,offer.rank.destination,offer.order,-1,false,nullptr,&a,offer.shipType);
                if(error.empty()) {
                    sent=true;++active;++launched;p.dispatchedMonth=today()/30;
                    p.status=L"已有"+std::to_wstring(active)+L"支自動商隊，上限"+std::to_wstring(limit)+L"支";
                    g_log("trade: auto plan city %d dispatched to %d by %s, occupied %lld",p.origin,offer.rank.destination,offer.shipType==MERCHANT_SHIP?"ship":"caravan",(long long)p.budget.occupied);break;
                }
                p.status=L"出發失敗："+tradeui::wide(error);
                g_log("trade: auto plan city %d to %d failed: %s",p.origin,offer.rank.destination,error.c_str());
                credit(p.force,p.origin,a.fees.refund(p.budget));p.budget.rollback(p.budget.month,offer.quote.cost);settleCredits();
                offers.erase(offers.begin()+best);
            }
            if(!sent)break;
        }
        if(launched)p.status=L"已派出"+std::to_wstring(launched)+L"隊；"+p.status;
    }
}
