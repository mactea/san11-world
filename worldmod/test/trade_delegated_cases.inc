// Real plan lifecycle/serializer, without touching native game resources.
static void delegatedPlanCases() {
    auto original=payloadBytes();int cars=0;
    g_autoPlans.clear();g_car.clear();
    syncDelegatedPlan(0,0,false);
    check(g_autoPlans.empty(),"direct cities do not acquire autonomous trade plans");
    syncDelegatedPlan(0,0,true);
    auto p=autoPlan(0,0);
    check(p && p->enabled && p->delegated && p->budget.limit==4000 &&
          p->budget.occupied==0 && p->preferredDestination==-1,
          "delegated city receives one bounded budget and automatic market selection");
    p->budget.charge(700);p->dispatchedMonth=today()/30;
    syncDelegatedPlan(0,0,true);
    check(g_autoPlans.size()==1 && p->budget.occupied==700 && !p->mayDispatch(today()/30),
          "repeated delegation refresh cannot duplicate plans or reset monthly expenditure");
    syncDelegatedPlan(0,0,false);
    check(!p->enabled && p->budget.occupied==700,"recalling a district stops new purchases without resetting accounting");
    syncDelegatedPlan(0,0,true);
    check(p->enabled && !p->mayDispatch(today()/30),"redelegation preserves the same month's dispatch limit");
    auto full=payloadBytes();auto packed=blockBytes();g_autoPlans.clear();
    check(parseBlock(packed.data()+12,packed.size()-12,TRAD_VERSION,&cars) && payloadBytes()==full &&
          g_autoPlans[0].delegated && g_autoPlans[0].preferredDestination==-1,
          "v12 preserves delegated origin, automatic destination and monthly accounts across reload");
    auto corrupt=full;uint32_t bad=2;memcpy(corrupt.data()+corrupt.size()-4,&bad,4);
    check(!parseBlock(corrupt.data(),corrupt.size(),TRAD_VERSION,&cars,true) && payloadBytes()==full,
          "invalid delegated flag rejects the whole save atomically");
    check(!parseBlock(full.data(),full.size()-1,TRAD_VERSION,&cars,true) && payloadBytes()==full,
          "truncated delegated metadata cannot partially overwrite trade state");
    p=autoPlan(0,0);p->delegated=false;p->enabled=false;p->budget.limit=1700;
    syncDelegatedPlan(0,0,true);
    check(!p->enabled && !p->delegated && p->budget.limit==1700,
          "explicit player pause and budget override autonomous district defaults");
    p->enabled=true;syncDelegatedPlan(0,0,false);
    check(p->enabled,"explicit player plans continue when their city returns to direct control");
    p->preferredDestination=1;
    auto old=payloadBytes(true,true,true,true,true,false);
    check(parseBlock(old.data(),old.size(),11,&cars,true) && !g_autoPlans[0].delegated &&
          g_autoPlans[0].enabled && g_autoPlans[0].budget.limit==1700,
          "v11 plans migrate as explicit player choices rather than delegated defaults");
    Caravan trip={};trip.id=1;trip.force=0;trip.origin=0;trip.automatic.caravan=1;
    autoBlock(trip,tradeauto::Issue::Budget,"fixture");
    check(trip.automatic.retryMonth==today()/30+1,"player voyages also retry transient failures next month");
    g_autoPlans[0].delegated=true;trip.automatic.issue=tradeauto::Issue::None;
    autoBlock(trip,tradeauto::Issue::Budget,"fixture");
    check(trip.automatic.retryMonth==today()/30+1,"delegated voyages retry transient failures next month");
    autoBlock(trip,tradeauto::Issue::Ownership,"fixture");
    check(trip.automatic.retryMonth==-1,"lost ownership never authorizes an automatic retry");
    check(parseBlock(original.data(),original.size(),TRAD_VERSION,&cars,true),"delegated tests restore the existing trade fixture");
}
