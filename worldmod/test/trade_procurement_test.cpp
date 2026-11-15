#include "../src/trade_procurement.h"
#include <algorithm>
#include <stdio.h>
using namespace trade;
static int failures;
static void check(bool ok,const char* label) {printf("%s %s\n",ok?"PASS":"FAIL",label);failures+=!ok;}
int main() {
    Data d;d.C=2;d.G=3;d.goodName={"槍","馬","小麥"};d.nativeUnits={100,100,0};d.minValue={150,150,0};
    d.size={500,100};d.flags={CITY_HUB,0};d.price={180,220,6,200,240,8};d.supply={3,0,3,0,1,0};d.demand.assign(6,1000);
    Market m;m.d=&d;m.reset();int gold=10000,weapon=150,cap=1000,calls=0;
    auto adjust=[&](bool equipment,int delta) {++calls;int& value=equipment?weapon:gold;int old=value;value=std::max(0,std::min(equipment?cap:100000,value+delta));return value-old;};
    auto beforeStock=m.stock,beforePrice=m.idx;auto cost=militaryQuote(m,0,0,5,gold,cap-weapon);
    check(cost>0 && !*commitMilitary(m,0,0,5,gold,cap-weapon,cost,adjust) && weapon==650 && gold==10000-cost && m.available(0,0)==beforeStock[0]-5,
          "successful purchase spends city gold, removes market lots and adds native weapons exactly once");
    check(m.at(0,0)>beforePrice[0],"procurement uses the ordinary market quantity price impact");
    beforeStock=m.stock;beforePrice=m.idx;int beforeGold=gold,beforeWeapon=weapon,beforeCalls=calls;
    for(int q:{0,-1,4,INT_MAX})check(*commitMilitary(m,0,0,q,gold,cap-weapon,1,adjust),"invalid quantity or native capacity rejects order");
    check(*commitMilitary(m,0,2,1,gold,cap-weapon,6,adjust),"food cannot be converted by the weapon procurement path");
    check(*commitMilitary(m,0,0,1,0,cap-weapon,100,adjust),"insufficient city gold rejects procurement");
    check(*commitMilitary(m,0,0,1,gold,cap-weapon,1,adjust),"changed confirmation quote cannot silently charge a different price");
    check(m.stock==beforeStock && m.idx==beforePrice && gold==beforeGold && weapon==beforeWeapon && calls==beforeCalls,"rejections have no native or market side effects");
    weapon=0;cap=1000;gold=10000;cost=militaryQuote(m,0,0,5,gold,cap);
    auto shortWeapon=[&](bool equipment,int delta){return adjust(equipment,equipment && delta>0?std::min(delta,200):delta);};
    check(*commitMilitary(m,0,0,5,gold,cap,cost,shortWeapon) && gold==10000 && weapon==0 && m.stock==beforeStock && m.idx==beforePrice,
          "partial native weapon addition rolls back weapons and payment without moving market prices");
    auto shortGold=[&](bool equipment,int delta){return adjust(equipment,!equipment && delta<0?std::max(delta,-100):delta);};
    check(*commitMilitary(m,0,0,5,gold,cap,cost,shortGold) && gold==10000 && weapon==0 && m.stock==beforeStock && m.idx==beforePrice,
          "partial native payment is refunded before adding weapons");
    check(militaryQuote(m,0,0,3,gold,350)>0 && militaryQuote(m,0,0,4,gold,350)<0,"350 spare units accept only three complete 100-unit lots");
    m.stock[0]=1000;beforeStock=m.stock;m.month(0);
    check(m.available(0,0)==998 && m.room(0,0)==0 && m.quote(0,0,1,false)<0 && m.quote(0,0,1,true)>0,
          "legacy over-cap stock remains purchasable, consumes gradually and cannot receive new goods");
    // New supply has no effect on unrelated goods; an imported military good has no production.
    check(m.available(0,2)==540 && m.available(1,0)==0,"ordinary food production and empty imported weapons keep their existing semantics");
    printf("trade_procurement_test: %d failures\n",failures);return failures?1:0;
}
