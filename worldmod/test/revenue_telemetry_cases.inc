// Execute monthly halving/facility/port arithmetic, original resource clamp/setters,
// native calendar predicates and the complete between-month tax routine.
static uintptr_t g_revenueSeedEntry,g_revenuePortEntry;
static uint8_t g_revenueDate[0x60]={};
static UpkeepBuilding* g_revenueCity;
static UpkeepBuilding* g_revenuePort;
static bool g_revenueFacility=false;
static int g_revenueBoundaryMutation=0,g_revenueBoundaryEffect=87,g_revenueResolutions=0;
static void __fastcall revenueBoundaryFlags(int* flags,void* unused,void* building,int mode) {
    collectIncome(flags,unused,building,mode);if(g_revenueBoundaryMutation==5)skillsTelemetryStream(1,0);
}
static int revenueBoundaryGrant(void* self,void* building,int requested,bool gold) {
    int result=((IncomeGrant)site(gold?0x4ae2a0:0x4ae300))(self,building,requested);
    if(g_revenueBoundaryMutation==3)skillsTelemetryStream(1,0);
    if(g_revenueBoundaryMutation==4)*(int*)(g_upkeepActor+0xe8)=g_revenueBoundaryEffect;
    return result;
}
static int __fastcall revenueBoundaryGold(void* self,void*,void* building,int requested){return revenueBoundaryGrant(self,building,requested,true);}
static int __fastcall revenueBoundaryFood(void* self,void*,void* building,int requested){return revenueBoundaryGrant(self,building,requested,false);}
static int __fastcall revenueGoldMax(UpkeepBuilding* b,void*) {return b->goldMax;}
static int __fastcall revenueFoodMax(UpkeepBuilding* b,void*) {return b->foodMax;}
static void __fastcall revenueSetGold(UpkeepBuilding* b,void*,int n) {b->gold=n;}
static void __fastcall revenueSetFood(UpkeepBuilding* b,void*,int n) {b->food=n;}
static int __fastcall revenueFacility(void*,void*,int id) {assert(id==2);return g_revenueFacility;}
static int __fastcall revenueHarvest(void*,void*) {return ((BuildingValue)site(0x4829d0))(g_revenueDate);}
static int __fastcall revenueRaw(void*,void*,void* city) {assert(city==g_revenueCity);return 103;}
static void* __fastcall revenueCity(void*,void*) {return g_revenueCity;}
static void* __fastcall revenueBase(void*,void*,int id) {return id==0?g_revenueCity:id==52?g_revenuePort:nullptr;}
static int __fastcall revenuePortId(void*,void*,int slot) {assert(slot>=0 && slot<5);return slot==0?52:-1;}
static int __fastcall revenueFind(void*,void*,int effect) {
    if(g_revenueBoundaryMutation==8)skillsTelemetryStream(1,0);
    if(personHas(g_upkeepActor,nullptr,effect))return 600;
    if(personHas(g_secondIncomeActor,nullptr,effect))return 14162;
    return -1;
}
static bool revenueActor(const void* p,skilltelemetry::Actor& a) {
    ++g_revenueResolutions;
    economyActor(p,a);a.person=p==g_upkeepActor?600:14162;a.force=2;a.city=0;
    a.year=*(int*)(g_revenueDate+8);a.month=*(int*)(g_revenueDate+12);a.day=*(int*)(g_revenueDate+16);
    return true;
}
__declspec(naked) static int __cdecl runRevenueSeed(int effect,int raw,int flag) {
    __asm {
        push ebp
        push esi
        push edi
        sub esp,0x1c0
        mov ecx,[esp+0x1d0]
        mov eax,[esp+0x1d8]
        mov [esp+0x1b4],eax
        mov [esp+0x1b8],eax
        mov ebp,[esp+0x1d4]
        mov edi,ebp
        mov esi,g_revenueCity
        mov edx,g_revenueSeedEntry
        mov eax,ebp
        jmp edx
    }
}
__declspec(naked) static void revenueGoldSeeded() {
    __asm {mov eax,ebp
           add esp,0x1c0
           pop edi
           pop esi
           pop ebp
           ret}
}
__declspec(naked) static void revenueFoodSeeded() {
    __asm {mov eax,edi
           add esp,0x1c0
           pop edi
           pop esi
           pop ebp
           ret}
}
__declspec(naked) static void revenueFoodSkipped() {
    __asm {xor eax,eax
           add esp,0x1c0
           pop edi
           pop esi
           pop ebp
           ret}
}
__declspec(naked) static uint64_t __cdecl runRevenuePorts(int gold,int food) {
    __asm {
        push ebp
        push esi
        push edi
        mov ebp,[esp+16]
        mov edi,[esp+20]
        mov eax,g_revenuePortEntry
        jmp eax
    }
}
__declspec(naked) static void revenuePortsDivided() {
    __asm {mov eax,ebp
           mov edx,esi
           pop edi
           pop esi
           pop ebp
           ret}
}
static void revenueTelemetryTests() {
    uint8_t first[400]={},second[400]={};g_upkeepActor=first;g_secondIncomeActor=second;
    void* vt[24]={};vt[0x40/4]=upkeepForce;vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=revenueFind;
    UpkeepBuilding city={vt,0,0,1000,0,2,0,1000,5000,5000};
    UpkeepBuilding port={vt,0,2,1000,0,2,52,1000,5000,5000};
    g_revenueCity=&city;g_revenuePort=&port;
    auto oldFlags=g_cityIncomeFlagsOriginal;auto oldGold=g_telemetryGold;auto oldFood=g_telemetryFood;
    auto oldId=g_telemetryBaseId;auto oldGoldGrant=g_goldIncomeGrantOriginal;auto oldFoodGrant=g_foodIncomeGrantOriginal;
    auto oldGoldMax=g_revenueGoldMax;auto oldFoodMax=g_revenueFoodMax;auto oldHarvest=g_revenueHarvest;
    auto oldGoldSeedResume=g_goldRevenueSeedResume;auto oldFoodSeedResume=g_foodRevenueSeedResume;
    auto oldOffResume=g_offMonthTaxResume;
    g_cityIncomeFlagsOriginal=(CityIncomeFlags)collectIncome;g_telemetryGold=(BuildingValue)bonusGold;
    g_telemetryFood=(BuildingValue)upkeepFood;g_telemetryBaseId=(BuildingId)upkeepBaseId;
    g_goldIncomeGrantOriginal=(IncomeGrant)site(0x4ae2a0);g_foodIncomeGrantOriginal=(IncomeGrant)site(0x4ae300);
    g_revenueGoldMax=(BuildingValue)revenueGoldMax;g_revenueFoodMax=(BuildingValue)revenueFoodMax;
    g_revenueHarvest=(BuildingValue)revenueHarvest;
    g_goldRevenueSeedResume=(uint32_t)site(0x590544);g_foodRevenueSeedResume=(uint32_t)site(0x5905f5);
    g_offMonthTaxResume=(uint32_t)site(0x59964e);
    domesticStub(0x47a630,domesticValid);domesticStub(0x486c80,bonusGold);domesticStub(0x486df0,upkeepFood);
    domesticStub(0x486d30,revenueGoldMax);domesticStub(0x486ea0,revenueFoodMax);
    domesticStub(0x487310,revenueSetGold);domesticStub(0x4873f0,revenueSetFood);
    domesticStub(0x47b3c0,revenueFacility);domesticStub(0x4866f0,revenueCity);
    domesticStub(0x49e590,revenueRaw);domesticStub(0x49e810,revenueRaw);
    domesticStub(0x490d00,revenueBase);domesticStub(0x47bc50,revenuePortId);
    domesticStub(0x590551,revenueGoldSeeded);domesticStub(0x590602,revenueFoodSeeded);
    domesticStub(0x590648,revenueFoodSkipped);
    domesticStub(0x590677,revenuePortsDivided);g_revenuePortEntry=site(0x590657);
    for(uint32_t at:{0x590558u,0x590609u,0x5906d1u,0x590726u})*(uint8_t*)site(at+5)=0xc3;
    // Only this routine's calendar-this operand is relocated; the original date arithmetic runs.
    auto dateOperand=(uint32_t*)site(0x599604);uint32_t oldDateOperand=*dateOperand;
    *dateOperand=(uint32_t)g_revenueDate;
    auto foodDateOperand=(uint32_t*)site(0x5905ac);uint32_t oldFoodDateOperand=*foodDateOperand;
    *foodDateOperand=(uint32_t)g_revenueDate;
    bool saved[250];memcpy(saved,g_skills[249].effects,sizeof saved);
    memset(g_skills[249].effects,0,sizeof saved);
    for(int effect:{87,88,89,90})g_skills[249].effects[effect]=true;
    int flags[BASE]={};g_cityFlagsEntry=site(0x590527);
    *(int*)(g_revenueDate+8)=194;*(int*)(g_revenueDate+12)=1;*(int*)(g_revenueDate+16)=1;
    for(int month=1;month<=12;++month)for(int day:{1,11,21}) {
        *(int*)(g_revenueDate+12)=month;*(int*)(g_revenueDate+16)=day;
        assert(revenueHarvest(nullptr,nullptr)==((month-1)%3==0));
        assert(((BuildingValue)site(0x482680))(g_revenueDate)==(day==1));
    }
    *(int*)(g_revenueDate+12)=1;*(int*)(g_revenueDate+16)=1;
    *(int*)(first+0xe8)=249;*(int*)(second+0xe8)=-1;
    runCityFlags(flags,&city,0);g_revenueSeedEntry=site(0x59053d);
    assert(runRevenueSeed(89,103,1)==51 && !g_goldRevenue.actor.person);
    // Execute the actual ordinary-harvest/skill gating, not a test-side replacement.
    *(int*)(g_revenueDate+12)=2;g_revenueSeedEntry=site(0x59059e);
    assert(runRevenueSeed(90,103,0)==0);
    *(int*)(g_revenueDate+12)=1;assert(runRevenueSeed(90,103,0)==103);
    assert(skilltelemetry::open(L"build/revenue_telemetry_test.csv",revenueActor,nullptr));
    auto grant=[&](uint32_t at,UpkeepBuilding& b,int n) {
        g_incomeBonusEntry=site(at);int old=(at==0x590609 || at==0x590635 || at==0x590726 || at==0x590744)?b.food:b.gold;
        int result=runIncomeBonus(&b,n);
        assert(result==(old+n>5000?5000-old:n));
    };
    for(int month:{1,2})for(int who=0;who<2;++who)for(bool facility:{false,true})for(int before:{1000,4990,5000}) {
        *(int*)(g_revenueDate+12)=month;g_revenueFacility=facility;
        assert(revenueHarvest(nullptr,nullptr)==(month==1));
        assert(((BuildingValue)site(0x482680))(g_revenueDate)==1);
        *(int*)(first+0xe8)=who?-1:249;*(int*)(second+0xe8)=who?249:-1;
        city.gold=city.food=port.gold=port.food=before;runCityFlags(flags,&city,0);
        g_revenueSeedEntry=site(0x59053d);int gold=runRevenueSeed(89,103,flags[89]);assert(gold==51);
        // Run harvest/skill gating, the 3/2 facility branch, the food seed and halving.
        g_revenueSeedEntry=site(0x59059e);int food=runRevenueSeed(90,103,flags[90]);assert(food==(facility?77:51));
        auto ports=runRevenuePorts(gold,food);assert((int)ports==10 && (int)(ports>>32)==food/5);
        grant(0x590558,city,gold);grant(0x590576,city,gold/2);
        grant(0x590609,city,food);if(month==1)grant(0x590635,city,food/2);
        grant(0x5906d1,port,(int)ports);grant(0x5906ef,port,(int)ports/2);
        grant(0x590726,port,(int)(ports>>32));if(month==1)grant(0x590744,port,(int)(ports>>32)/2);
    }
    *(int*)(g_revenueDate+12)=1;*(int*)(first+0xe8)=*(int*)(second+0xe8)=89;
    city.gold=1000;runCityFlags(flags,&city,0);g_revenueSeedEntry=site(0x59053d);
    assert(runRevenueSeed(89,103,1)==51);grant(0x590558,city,51);
    // Wrong request, wrong city, an untaken flag and a changed provider are not attributed.
    grant(0x590558,city,49);grant(0x590558,port,51);
    assert(runRevenueSeed(89,103,0)==103);grant(0x590558,city,103);
    assert(runRevenueSeed(89,0,0)==0);grant(0x590558,city,0);
    *(int*)(second+0xe8)=-1;runCityFlags(flags,&city,0);
    assert(runRevenueSeed(89,103,1)==51);*(int*)(first+0xe8)=-1;grant(0x590558,city,51);
    // Tax/collection and wealth/rice can belong to different people at the same grant.
    *(int*)(first+0xe8)=89;*(int*)(second+0xe8)=87;city.gold=1000;
    runCityFlags(flags,&city,0);assert(runRevenueSeed(89,103,1)==51);
    grant(0x590558,city,51);grant(0x590576,city,25);
    *(int*)(first+0xe8)=90;*(int*)(second+0xe8)=88;city.food=1000;g_revenueFacility=true;
    runCityFlags(flags,&city,0);g_revenueSeedEntry=site(0x59059e);
    assert(runRevenueSeed(90,103,1)==77);grant(0x590609,city,77);grant(0x590635,city,38);
    auto off=(void(__cdecl*)())site(0x599600);
    for(int day:{11,21})for(int mode=0;mode<4;++mode)for(int before:{1000,4990,5000}) {
        *(int*)(g_revenueDate+16)=day;
        assert(((BuildingValue)site(0x482680))(g_revenueDate)==0);
        *(int*)(first+0xe8)=mode==2?-1:mode==1?249:89;*(int*)(second+0xe8)=mode>=2?89:-1;
        city.gold=port.gold=before;off();
        assert(city.gold==(before==1000?1051:5000) && port.gold==(before==1000?1010:5000));
    }
    *(int*)(g_revenueDate+16)=1;city.gold=port.gold=1000;off();
    assert(city.gold==1000 && port.gold==1000); // native month-start gate
    *(int*)(g_revenueDate+16)=11;port.force=3;off();
    assert(city.gold==1051 && port.gold==1000);port.force=2;
    *(int*)(first+0xe8)=89;*(int*)(second+0xe8)=-1;city.gold=port.gold=1000;
    off();off();assert(city.gold==1102 && port.gold==1020);
    unitHas(&city,nullptr,89); // preview-style eligibility leaves no committed provider
    *(int*)(first+0xe8)=-1;off();assert(!g_offMonthRevenue.actor.person && !g_offMonthTaxProbe);
    skilltelemetry::close();
    *(int*)(g_revenueDate+12)=1;*(int*)(g_revenueDate+16)=1;g_revenueFacility=false;
    g_cityIncomeFlagsOriginal=(CityIncomeFlags)revenueBoundaryFlags;
    g_goldIncomeGrantOriginal=(IncomeGrant)revenueBoundaryGold;g_foodIncomeGrantOriginal=(IncomeGrant)revenueBoundaryFood;
    assert(skilltelemetry::open(L"build/revenue_context_test.csv",revenueActor,nullptr));
    for(int effect:{87,88,89,90})for(int mutation=0;mutation<7;++mutation) {
        g_revenueBoundaryEffect=effect;g_revenueBoundaryMutation=mutation;
        *(int*)(first+0xe8)=249;*(int*)(second+0xe8)=-1;city.gold=city.food=1000;
        runCityFlags(flags,&city,0);
        if(effect==89 || effect==90){g_revenueSeedEntry=site(effect==89?0x59053d:0x59059e);assert(runRevenueSeed(effect,103,1)==51);}
        if(mutation==1)skillsTelemetryStream(1,0);
        if(mutation==2)*(int*)(first+0xe8)=effect;
        if(mutation==6)*(int*)(first+0xe8)=-1;
        int resolved=g_revenueResolutions;
        grant(effect==87?0x590576:effect==88?0x590635:effect==89?0x590558:0x590609,city,effect<89?100:51);
        assert(g_revenueResolutions==resolved+(mutation==0?1:0));
    }
    skilltelemetry::close();assertBattleAppliedRows(L"build/revenue_context_test.csv",4);
    assert(skilltelemetry::open(L"build/revenue_offmonth_context_test.csv",revenueActor,nullptr));
    *(int*)(g_revenueDate+16)=11;g_revenueBoundaryEffect=89;
    for(int mutation:{0,3,4,8}) {
        g_revenueBoundaryMutation=mutation;*(int*)(first+0xe8)=249;*(int*)(second+0xe8)=-1;
        city.gold=port.gold=1000;int resolved=g_revenueResolutions;
        off();assert(city.gold==1051 && port.gold==1010);
        assert(g_revenueResolutions==resolved+(mutation==0?3:1));
    }
    skilltelemetry::close();assertBattleAppliedRows(L"build/revenue_offmonth_context_test.csv",2);
    g_revenueBoundaryMutation=0;
    memcpy(g_skills[249].effects,saved,sizeof saved);*dateOperand=oldDateOperand;*foodDateOperand=oldFoodDateOperand;
    g_goldRevenueSeedResume=oldGoldSeedResume;g_foodRevenueSeedResume=oldFoodSeedResume;g_offMonthTaxResume=oldOffResume;
    g_cityIncomeFlagsOriginal=oldFlags;g_telemetryGold=oldGold;g_telemetryFood=oldFood;g_telemetryBaseId=oldId;
    g_goldIncomeGrantOriginal=oldGoldGrant;g_foodIncomeGrantOriginal=oldFoodGrant;
    g_revenueGoldMax=oldGoldMax;g_revenueFoodMax=oldFoodMax;g_revenueHarvest=oldHarvest;
    g_goldRevenue=g_foodRevenue=g_offMonthRevenue=RevenueCycle{};
    puts("PASS native revenue: monthly/harvest calendars, facility and port rounding, tax installments, real caps/setters, composed/high-ID providers and stale-context rejection");
}
