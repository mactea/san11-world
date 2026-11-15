// Exercise production navigation with an injected native dialog, not copied UI logic.
static void interactionCases() {
    auto model=g_model;auto prices=g_data.price;auto plans=g_autoPlans;auto cars=g_car;
    g_data.price={10,20,500,600};
    g_model=tradeui::Model();g_model.d=&g_data;g_model.live=g_mk;g_model.live.stock={100,100,0,0};
    g_model.seen=g_model.live;g_model.force=0;g_model.owner={0,1};g_model.stance={2,1};
    g_model.known={1,1};g_model.age={0,0};g_model.gold={10000,0};
    g_model.troops={5000,0};g_model.food={30000,0};g_model.grainRate={50,50};
    std::vector<OrderLine> order={{0,2},{1,3}};
    int tables=0,numbers=0;
    tradenative::testShow=[&](const Table& t,bool,bool,tradenative::ViewState* view,std::vector<int>*) {
        check(t.title==L"調整貨單" && view && t.rows.size()==2,"cargo editor directly lists both goods with retained view state");
        return tables++==0?0:-1;
    };
    tradenative::testNumber=[&](const std::wstring&,int value,int maximum,int minimum) {
        ++numbers;check(value==2 && maximum>=2 && minimum==0,"quantity editor permits removing goods with zero");return 0;
    };
    nativeEditOrder(0,1,-1,order);
    check(tables==2 && numbers==1 && order.size()==1 && order[0].good==1 && order[0].qty==3,
          "one row and one quantity edit removes only the chosen good and returns to the list");
    tables=0;numbers=0;order={{0,2},{1,3}};
    tradenative::testNumber=[&](const std::wstring&,int,int,int){++numbers;return -1;};
    nativeEditOrder(0,1,-1,order);
    check(sameOrder(order,{{0,2},{1,3}}) && numbers==1,"cancel quantity and back preserve the complete cargo draft");
    tradenative::testShow=[&](const Table& t,bool,bool,tradenative::ViewState*,std::vector<int>*) {
        bool found=false;for(auto& r:t.rows)found|=r.id==-2 && r.enabled;
        check(found,"automatic destination selector offers an explicit unrestricted choice");return -2;
    };
    check(nativeChooseCity(0,-1,true,true)==-2,"unrestricted destination is distinct from Back");
    tradenative::testShow=nullptr;tradenative::testNumber=nullptr;
    Table table={L"",L"",{L""},{{7,{L"7"}},{4,{L"4"}},{9,{L"9"}}}};
    tradenative::ViewState view;view.order={9,7};view.scroll=1;view.focus=7;
    auto restored=tradenative::restoreView(table,&view);
    check(restored.rows[0].id==9 && restored.rows[1].id==7 && restored.rows[2].id==4 && table.rows[0].id==7,
          "view restoration retains stable IDs and appends new rows without modifying the source");
    Caravan k={};k.state=DOCKED;k.automatic.caravan=1;k.automatic.managed=true;
    check(nativeManaged(k) && !nativeNeedsDecision(k),"healthy automatic caravan is managed but is not a pending exception");
    k.automatic.issue=tradeauto::Issue::Budget;
    check(nativeNeedsDecision(k),"budget exception appears in pending decisions");
    k.automatic.issue=tradeauto::Issue::None;k.tempReleased=true;
    check(nativeNeedsDecision(k),"released merchant with cargo remains actionable");
    k.state=FINISHED;check(!nativeNeedsDecision(k),"finished caravan is excluded from pending decisions");
    tradeauto::Plan a;a.force=0;a.origin=0;a.budget.month=-1;a.enabled=true;
    auto b=a;b.origin=1;b.budget.occupied=99;b.status=L"unchanged";
    auto c=b;c.origin=0;c.force=1;
    g_autoPlans={a,b,c};g_car={k};autoTick(0,0);
    check(!g_autoPlans[0].enabled && g_autoPlans[0].budget.month==today()/30 &&
          g_autoPlans[1].budget.month==-1 && g_autoPlans[1].budget.occupied==99 && g_autoPlans[1].status==L"unchanged" &&
          g_autoPlans[2].budget.month==-1 && g_autoPlans[2].enabled && g_car[0].tempReleased,
          "city-scoped evaluation leaves other cities, other forces and existing voyages untouched");
    FormationDraft draft;draft.city=0;draft.force=0;draft.order=order;
    auto changed=draft;changed.order[0].qty++;
    check(!sameFormation(draft,changed) && sameFormation(draft,draft),"draft change detection includes cargo quantities");
    g_formationDrafts={draft};g_autoDrafts={a};g_nativeFeedback=L"old game";clearNativeDrafts();
    check(g_formationDrafts.empty() && g_autoDrafts.empty() && g_nativeFeedback.empty(),"loading a game clears all session drafts and stale feedback");
    g_model=model;g_data.price=prices;g_autoPlans=plans;g_car=cars;
}
