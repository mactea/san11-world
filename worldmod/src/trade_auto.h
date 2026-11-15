#pragma once
// Player automatic-trade policy. No game addresses or native resource mutations.
#include <stdint.h>
#include <algorithm>
#include <vector>
#include <string>

namespace tradeauto {
constexpr int PLAYER_CITY_LIMIT = 2, AI_CITY_LIMIT = 2;
constexpr int CITY_RESERVE = 2000;
enum class Stage : int32_t { Outbound, Selling, ReturnBuying, Returning, HomeSelling, Complete };
enum class Issue : int32_t { None, Unprofitable, Budget, Warehouse, Route, Ownership, Resources };
enum class Decision { RetryNextMonth, ReturnEmpty, ReturnWithCargo, Manual };

// occupied is expenditure plus advances charged to this month. Revenue is deliberately
// not an operation on this account. Outstanding advances survive monthly rollover.
struct Budget {
    int32_t month = -1;
    int64_t limit = 0, occupied = 0;
    bool advanceMonth(int32_t now) {
        if(now<0 || now<month) return false;
        if(now>month) {month=now;occupied=0;}
        return true;
    }
    int64_t available() const {return std::max<int64_t>(0,limit-occupied);}
    bool charge(int64_t amount) {
        if(month<0 || amount<0 || amount>available()) return false;
        occupied+=amount;return true;
    }
    bool rollback(int32_t chargedMonth,int64_t amount) {
        if(chargedMonth!=month || amount<0 || amount>occupied) return false;
        occupied-=amount;return true;
    }
};
struct Advance {
    int32_t month = -1;
    int64_t balance = 0;
    bool fund(Budget& budget,int64_t amount) {
        if(balance || amount<0 || !budget.charge(amount)) return false;
        month=budget.month;balance=amount;return true;
    }
    bool spend(int64_t amount) {
        if(amount<0 || amount>balance) return false;
        balance-=amount;return true;
    }
    // Caller credits the returned amount to the funding city exactly once.
    int64_t refund(Budget& budget) {
        int64_t amount=balance;
        if(month==budget.month) budget.rollback(month,amount);
        balance=0;return amount;
    }
};
struct Plan {
    int32_t origin=-1,force=-1,preferredDestination=-1;
    int32_t shipType=0; // trade::TransportType; legacy plans remain ordinary caravans.
    bool enabled=false, delegated=false; // delegated plans follow the city's current control mode.
    Budget budget;
    std::vector<int32_t> preferredGoods;
    int32_t dispatchedMonth=-1;
    std::wstring status; // Runtime explanation only; not part of the save format.
    bool mayDispatch(int month) const {
        // Older builds marked failed attempts as dispatched. Zero occupied funds
        // proves no launch was paid for this month, so those saves may retry too.
        return enabled && (dispatchedMonth!=month || budget.occupied==0);
    }
};
// Native grain purchase quote, valued at the temporary merchant's politics.
// -1 means the city grain rate is unavailable; never treat that as free supplies.
inline int64_t provisionsValue(int food,int rate,int politics) {
    if(food<0 || rate<1 || rate>255 || politics<0 || politics>255)return -1;
    return (int64_t)food*(450-politics)*10/(rate*400);
}
struct Voyage {
    uint32_t caravan=0;
    int32_t origin=-1,force=-1;
    Stage stage=Stage::Outbound;
    Issue issue=Issue::None;
    int32_t retryMonth=-1;
    bool managed=true;
    Advance fees;
    // Only fresh arrival transitions are accepted: duplicate turn/load processing
    // must not sell twice or manufacture a second return voyage.
    bool arrive(bool atHome) {
        if(!managed || issue!=Issue::None) return false;
        if(stage==Stage::Outbound && !atHome) {stage=Stage::Selling;return true;}
        if(stage==Stage::Returning && atHome) {stage=Stage::HomeSelling;return true;}
        return false;
    }
    bool soldAll() {
        if(!managed || issue!=Issue::None) return false;
        if(stage==Stage::Selling) {stage=Stage::ReturnBuying;return true;}
        if(stage==Stage::HomeSelling) {stage=Stage::Complete;return true;}
        return false;
    }
    bool returnPurchased() {
        if(!managed || issue!=Issue::None || stage!=Stage::ReturnBuying) return false;
        stage=Stage::Returning;return true;
    }
    bool block(Issue why) {
        if(!managed || why==Issue::None || stage==Stage::Complete) return false;
        bool notify=issue==Issue::None;
        if(issue!=why)retryMonth=-1;
        issue=why;return notify;
    }
    bool decide(Decision decision,int32_t month,bool reachable,bool empty) {
        if(!managed || issue==Issue::None || month<0) return false;
        if(decision==Decision::Manual) {managed=false;issue=Issue::None;retryMonth=-1;return true;}
        if(decision==Decision::RetryNextMonth) {retryMonth=month+1;return true;}
        if(!reachable || issue==Issue::Ownership || (decision==Decision::ReturnEmpty && !empty)) return false;
        stage=Stage::Returning;issue=Issue::None;retryMonth=-1;return true;
    }
    bool retry(int32_t month) {
        if(!managed || retryMonth<0 || month<retryMonth) return false;
        retryMonth=-1;issue=Issue::None;return true;
    }
};
// The caller supplies conservative round-trip costs (including estimated food)
// and known-market quotes. Estimated return cargo profit is never required.
struct Candidate {
    int32_t destination=-1,turns=0;
    int64_t expenditure=0,outboundNet=0,roundTripNet=0;
    bool known=false,reachable=false,preferredGoods=false;
    bool eligible() const {
        return destination>=0 && known && reachable && turns>0 && expenditure>0 &&
            outboundNet>0 && roundTripNet>0 && roundTripNet>=expenditure/10+(expenditure%10!=0);
    }
};
inline long double score(const Candidate& c) {return (long double)c.roundTripNet/c.turns;}
inline int choose(const std::vector<Candidate>& candidates,int preferredDestination) {
    std::vector<int> routes;
    for(size_t i=0;i<candidates.size();++i) {
        const auto& c=candidates[i];if(!c.eligible())continue;
        auto at=std::find_if(routes.begin(),routes.end(),[&](int r){return candidates[r].destination==c.destination;});
        if(at==routes.end())routes.push_back((int)i);
        else if(score(c)>score(candidates[*at]))*at=(int)i;
    }
    // Goods preference applies within each destination before comparing routes.
    for(int& r:routes) {
        int goods=-1;
        for(size_t i=0;i<candidates.size();++i) {
            const auto& c=candidates[i];
            if(c.eligible() && c.destination==candidates[r].destination && c.preferredGoods &&
               score(c)>=score(candidates[r])*0.95L && (goods<0 || score(c)>score(candidates[goods])))goods=(int)i;
        }
        if(goods>=0)r=goods;
    }
    int best=-1,preferred=-1;
    for(int r:routes) {
        if(best<0 || score(candidates[r])>score(candidates[best]))best=r;
        if(candidates[r].destination==preferredDestination)preferred=r;
    }
    if(best<0)return -1;
    if(preferred>=0 && score(candidates[best])<score(candidates[preferred])*1.20L)best=preferred;
    return best;
}
} // namespace tradeauto
