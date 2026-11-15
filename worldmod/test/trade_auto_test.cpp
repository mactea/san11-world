#include "../src/trade_auto.h"
#include <stdio.h>
using namespace tradeauto;
static int failures;
static void check(bool ok,const char* message) {printf("%s %s\n",ok?"PASS":"FAIL",message);failures+=!ok;}
int main() {
    Plan plan;plan.enabled=true;plan.budget.advanceMonth(2361);plan.budget.limit=1000;plan.dispatchedMonth=2361;
    check(plan.mayDispatch(2361),"real-save failed attempt with zero spend can retry in the same month");
    plan.budget.charge(500);
    check(!plan.mayDispatch(2361),"paid launch cannot be repeated by saving settings again");
    plan.budget.advanceMonth(2362);
    check(plan.mayDispatch(2362),"next month restores launch eligibility");
    plan.enabled=false;check(!plan.mayDispatch(2362),"paused plan remains paused");
    check(provisionsValue(3000,50,60)==585 && provisionsValue(3000,100,60)==292 &&
          provisionsValue(3000,0,60)==-1,"provisions use native grain quote with integer rounding and reject missing rate");
    Budget b;b.limit=4000;b.advanceMonth(10);
    Advance fees;
    check(b.charge(2000) && fees.fund(b,500) && b.available()==1500,"purchase and fee advance share monthly allowance");
    check(!b.charge(1501) && b.available()==1500,"a second caravan cannot overspend");
    check(fees.spend(100) && b.occupied==2500,"spending reserved fees does not debit monthly budget twice");
    check(b.advanceMonth(10) && b.occupied==2500 && !b.advanceMonth(9),"same-month reload and backward dates cannot replenish allowance");
    b.limit=1000;check(b.available()==0 && !b.charge(1),"lower budget stops new spending without erasing commitments");
    b.limit=4000;b.advanceMonth(11);
    check(b.available()==4000 && fees.balance==400,"month rollover resets allowance but retains outstanding cash");
    check(fees.refund(b)==400 && fees.refund(b)==0 && b.available()==4000,"old advance refund is one-shot and does not inflate new-month allowance");
    check(fees.fund(b,300) && fees.spend(50) && fees.refund(b)==250 && b.occupied==50,"same-month unused advance releases only its own unspent amount");
    check(!b.charge(-1) && !fees.spend(-1) && !b.rollback(11,51),"invalid money changes are rejected without mutation");
    Voyage v;
    check(!v.arrive(true) && v.arrive(false) && !v.arrive(false),"arrival changes phase only once and at the right endpoint");
    check(v.soldAll() && v.stage==Stage::ReturnBuying && !v.soldAll(),"completed sales advance to return planning only once");
    check(v.block(Issue::Budget) && !v.block(Issue::Budget) && !v.returnPurchased(),"exception notifies once and prevents automatic return purchases");
    check(v.decide(Decision::RetryNextMonth,11,false,false) && !v.block(Issue::Budget) && !v.retry(11) && v.retry(12),"repeated exception checks preserve the player's next-month retry");
    check(v.returnPurchased() && v.arrive(true) && v.soldAll() && v.stage==Stage::Complete,"normal automatic round trip reaches completion");
    check(!v.block(Issue::Route),"completed voyage cannot be reopened by a late callback");
    v=Voyage();v.block(Issue::Warehouse);
    check(!v.decide(Decision::ReturnEmpty,12,true,false) && !v.decide(Decision::ReturnWithCargo,12,false,false),"no implicit cargo discard or unreachable return");
    check(v.decide(Decision::ReturnWithCargo,12,true,false) && v.stage==Stage::Returning,"player can explicitly return with remaining goods");
    v.block(Issue::Ownership);check(!v.decide(Decision::ReturnEmpty,12,true,true),"captured origin requires explicit reassignment before return");
    check(v.decide(Decision::Manual,12,false,false) && !v.arrive(true),"manual takeover stops automatic transitions");
    Candidate a{1,10,1000,200,200,true,true,false},c=a;c.destination=2;c.roundTripNet=239;
    check(choose({a,c},1)==0,"preferred destination retained below twenty percent advantage");
    c.roundTripNet=240;check(choose({a,c},1)==1,"twenty percent better alternative may replace preferred destination");
    c=a;c.preferredGoods=true;c.roundTripNet=190;
    check(choose({a,c},1)==1,"preferred goods selected within five percent of best score");
    c.roundTripNet=189;check(choose({a,c},1)==0,"goods preference cannot sacrifice more than five percent");
    a.known=false;check(choose({a},1)==-1,"unknown market excluded");a.known=true;a.reachable=false;
    check(choose({a},1)==-1,"unreachable market excluded");a.reachable=true;a.roundTripNet=99;
    check(choose({a},1)==-1,"ten percent minimum margin enforced");a.roundTripNet=100;a.outboundNet=0;
    check(choose({a},1)==-1,"speculative return cargo cannot rescue unprofitable outbound trade");
    printf("trade_auto_test: %d failures\n",failures);return failures?1:0;
}
