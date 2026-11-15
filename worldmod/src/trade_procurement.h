#pragma once
#include "trade_model.h"
#include <functional>
#include <limits.h>

namespace trade {
// Only the four explicitly mapped native weapon goods are eligible. No grain,
// reverse exchange, caravan allocation, merchant discount or formation fee.
inline int64_t militaryQuote(const Market& m,int c,int g,int qty,int gold,int room) {
    if(c<0 || c>=m.d->C || m.d->militaryWeapon(g)<0 || qty<1 || room<0 ||
       qty>room/(int)m.d->unitsPerLot(g))return -1;
    auto cost=m.quote(c,g,qty,true,0);
    return cost>=0 && cost<=gold && cost<=INT_MAX ? cost : -1;
}
// Native change returns the amount actually applied. Roll back a short native
// write before touching the trade market; called on the game's command thread.
inline const char* commitMilitary(Market& m,int c,int g,int qty,int gold,int room,int64_t expected,
                                 const std::function<int(bool,int)>& change) {
    auto cost=militaryQuote(m,c,g,qty,gold,room);
    if(cost<0)return "military stock, gold or city capacity insufficient";
    if(cost!=expected)return "military quote changed";
    int paid=-change(false,-(int)cost);
    if(paid!=cost) {
        if(paid && change(false,paid)!=paid)return "military rollback failed";
        return "military payment failed";
    }
    int units=qty*(int)m.d->unitsPerLot(g),added=change(true,units);
    if(added!=units) {
        bool restored=!added || change(true,-added)==-added;
        restored=change(false,paid)==paid && restored;
        return restored ? "military capacity changed" : "military rollback failed";
    }
    m.buy(c,g,qty,0);
    return "";
}
}
