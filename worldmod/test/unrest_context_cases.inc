// Collection boundaries use controlled callbacks; roll/region/flag selection
// and the native camp constructor call/return path remain the existing fixture.
static void* g_unrestContextOuter;
static void* g_unrestContextInner;
static int g_unrestContextMode=0;
static void __fastcall unrestContextCollect(int* flags,void*,void* city,int) {
    memset(flags,0,BASE*sizeof(int));
    if(city==g_unrestContextInner) {
        runIncome(flags,95,g_secondIncomeActor);
        if(g_unrestContextMode==5)skillsTelemetryStream(1,0);
        if(g_unrestContextMode==3 || g_unrestContextMode==6)throw 9;
        return;
    }
    assert(city==g_unrestContextOuter);
    runIncome(flags,249,g_upkeepActor);
    if(g_unrestContextMode==4)skillsTelemetryStream(1,0);
    if(g_unrestContextMode==7)throw 7;
    int inner[BASE]={};
    if(g_unrestContextMode==1)runIncome(inner,95,g_secondIncomeActor);
    if(g_unrestContextMode==2 || g_unrestContextMode==3 || g_unrestContextMode==5)
        unrestCityFlags(inner,nullptr,g_unrestContextInner,0);
    if(g_unrestContextMode==6) {
        try {unrestCityFlags(inner,nullptr,g_unrestContextInner,0);}
        catch(int value){assert(value==9);}
    }
    if(g_unrestContextMode==1 || g_unrestContextMode==2 || g_unrestContextMode==5 || g_unrestContextMode==6)
        runIncome(flags,249,g_upkeepActor);
}
static void unrestCollectionContextTests(UpkeepBuilding& city,UpkeepBuilding& inner,uint8_t* first,uint8_t* second,uint8_t* record) {
    auto oldFlags=g_cityIncomeFlagsOriginal;
    g_cityIncomeFlagsOriginal=(CityIncomeFlags)unrestContextCollect;
    g_unrestContextOuter=&city;g_unrestContextInner=&inner;
    memset(g_skills[249].effects,0,sizeof g_skills[249].effects);
    for(int effect=91;effect<=95;++effect)g_skills[249].effects[effect]=true;
    *(int*)(first+0xe8)=249;*(int*)(second+0xe8)=95;record[0x85]=70;
    g_unrestExpectedForce=*g_unrestForceBase+4;
    assert(skilltelemetry::open(L"build/unrest_collection_context_test.csv",unrestFixtureActor,nullptr));
    bool collectionsCorrect=true;int checks=0;
    for(int cityId:{0,19,23,38})for(int mode=0;mode<8;++mode) {
        city.id=cityId;g_unrestContextMode=mode;int flags[BASE]={},enclosingFlags[BASE]={};
        IncomeProviders enclosing;enclosing.flags=enclosingFlags;
        auto previous=g_collectIncomeProviders;g_collectIncomeProviders=&enclosing;
        bool thrown=false;
        try {unrestCityFlags(flags,nullptr,&city,0);}
        catch(int value){assert(value==(mode==7?7:9));thrown=true;}
        assert(thrown==(mode==3 || mode==7));
        bool active=mode==0 || mode==1 || mode==2 || mode==6;
        bool correct=g_collectIncomeProviders==&enclosing;
        if(active) {
            correct=correct && g_unrestProviders.city==&city && g_unrestFlags==flags;
            for(int effect=91;effect<=95;++effect)
                correct=correct && g_unrestProviders.holders[effect].person==first;
        }else correct=correct && !g_unrestProviders.city && !g_unrestFlags;
        collectionsCorrect=collectionsCorrect && correct;
        // Repair only the test's enclosing pointer after observing a bad scope,
        // so the pre-fix run can retain all of its CSV evidence before failing.
        g_collectIncomeProviders=previous;
        int chance=g_unrestChanceCalls,created=g_unrestWorldCreates;
        assert(runUnrestRoll(flags,record)==0 && g_unrestChanceCalls==chance && !g_unrestGateProbe);
        assert(runUnrestCamp(&city,flags,cityId)==g_unrestCamp && g_unrestWorldCreates==created+1);
        assert(g_unrestTribe==-1);++checks;
    }
    skilltelemetry::close();
    printf("unrest collection boundaries: %d cases, contexts correct=%d\n",checks,(int)collectionsCorrect);
    assert(collectionsCorrect);
    assertBattleAppliedRows(L"build/unrest_collection_context_test.csv",32);
    g_cityIncomeFlagsOriginal=oldFlags;
    puts("PASS unrest collection: native threshold/camp controls, nested queries/collections, caught/uncaught exceptions and world-generation changes");
}
