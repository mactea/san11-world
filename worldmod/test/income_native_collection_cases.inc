// Execute whole4ceae0 ->4cf360 ->489fe0/488b30 ->4cea00, including the
// installed income bridge. Personnel storage and list primitives are adapters.
struct IncomeNativeNode {IncomeNativeNode* next;void* person;};
static IncomeNativeNode g_incomeNativeInput[2],g_incomeNativeOutput[2];
static uint8_t g_incomeNativeList[16];
static UpkeepBuilding* g_incomeNativeBuilding;
static bool g_incomeNativeReady=true,g_incomeNativeAppendFailed=false;
static int g_incomeNativeUsed=0;
static int __fastcall incomeNativeReady(void* building,void*){assert(building==g_incomeNativeBuilding);return g_incomeNativeReady;}
static void* __fastcall incomeNativePeople(void* building,void*){assert(building==g_incomeNativeBuilding);return g_incomeNativeList;}
static void __fastcall incomeNativeClear(uint8_t* list,void*) {
    *(void**)(list+4)=*(void**)(list+8)=nullptr;*(int*)(list+12)=0;g_incomeNativeUsed=0;
}
static void* __fastcall incomeNativeAppend(uint8_t* list,void*,void* person) {
    if(g_incomeNativeAppendFailed)return nullptr;
    assert(g_incomeNativeUsed<2);auto node=&g_incomeNativeOutput[g_incomeNativeUsed++];node->next=nullptr;node->person=person;
    auto tail=*(IncomeNativeNode**)(list+8);if(tail)tail->next=node;else *(void**)(list+4)=node;
    *(void**)(list+8)=node;++*(int*)(list+12);return node;
}
static void** __fastcall incomeNativeNext(void*,void*,IncomeNativeNode** cursor) {
    auto node=*cursor;assert(node);*cursor=node->next;return &node->person;
}
static void incomeNativeCollectionTests(UpkeepBuilding& city,uint8_t* first,uint8_t* second) {
    auto oldFlags=g_cityIncomeFlagsOriginal;auto oldGold=g_goldIncomeGrantOriginal;auto oldFood=g_foodIncomeGrantOriginal;
    g_cityIncomeFlagsOriginal=(CityIncomeFlags)site(0x4ceae0);
    g_goldIncomeGrantOriginal=(IncomeGrant)site(0x4ae2a0);g_foodIncomeGrantOriginal=(IncomeGrant)site(0x4ae300);
    uint32_t addresses[]={0x487e10,0x487bc0,0x47a600,0x47a630,0x47be50,0x47c100,0x47c1b0,0x47bed0,
        0x486c80,0x486df0,0x486d30,0x486ea0,0x487310,0x4873f0};uint8_t saved[14][5];
    for(int n=0;n<14;++n)memcpy(saved[n],(void*)site(addresses[n]),5);
    domesticStub(0x487e10,incomeNativeReady);domesticStub(0x487bc0,incomeNativePeople);
    domesticStub(0x47a600,domesticValid);domesticStub(0x47a630,domesticValid);
    domesticStub(0x47be50,incomeNativeClear);domesticStub(0x47c100,incomeNativeClear);
    domesticStub(0x47c1b0,incomeNativeAppend);domesticStub(0x47bed0,incomeNativeNext);
    domesticStub(0x486c80,bonusGold);domesticStub(0x486df0,upkeepFood);
    domesticStub(0x486d30,revenueGoldMax);domesticStub(0x486ea0,revenueFoodMax);
    domesticStub(0x487310,revenueSetGold);domesticStub(0x4873f0,revenueSetFood);
    auto table=(uint32_t*)site(0x488b3d);uint32_t oldTable=*table,jumps[10];
    for(int n=0;n<10;++n)jumps[n]=(uint32_t)site(*(uint32_t*)site(0x488b9c+4*n));*table=(uint32_t)jumps;
    auto resume=(uint8_t*)g_incomeResume;uint8_t oldResume=*resume;
    assert(!memcmp(resume+1,"\x44\x24\x10\x85\xc0\x75",6));*resume=0x8b;
    g_incomeNativeBuilding=&city;g_upkeepActor=first;g_secondIncomeActor=second;
    int flags[BASE+2]={};flags[BASE]=0x1234;flags[BASE+1]=0x5678;
    assert(skilltelemetry::open(L"build/income_native_collection_test.csv",economyActor,nullptr));
    int applied=0;
    for(int profile=0;profile<10;++profile) {
        *(int*)(first+0xe8)=profile==0 || profile==3 || profile==9?-1:profile==2?87:249;
        *(int*)(second+0xe8)=profile==0?-1:profile==2?88:profile==3?249:87;
        *(int*)(first+0xa0)=profile==4?5:1;*(int*)(second+0xa0)=3;
        *(int*)(first+0x98)=profile==5?9999:8;
        g_incomeNativeReady=profile!=7;g_incomeNativeAppendFailed=profile==8;
        g_incomeNativeInput[0]={&g_incomeNativeInput[1],first};
        g_incomeNativeInput[1]={nullptr,profile==6?first:second};
        *(void**)(g_incomeNativeList+4)=g_incomeNativeInput;
        runCityFlags(flags,&city,0);
        bool gold=profile!=0 && profile!=7 && profile!=8;
        bool food=profile==1 || profile==2 || profile==3 || profile==5 || profile==6;
        assert((flags[87]!=0)==gold && (flags[88]!=0)==food && flags[BASE]==0x1234 && flags[BASE+1]==0x5678);
        assert(g_incomeProviders.flags==flags && g_incomeProviders.city==&city && !g_collectIncomeProviders);
        for(int effect:{87,88}) {
            bool present=effect==87?gold:food;
            const void* expected=profile==3 || profile==4 || profile==9 || profile==2 && effect==88?second:first;
            assert(g_incomeProviders.holders[effect].person==(present?expected:nullptr));
            g_incomeBonusEntry=site(effect==87?0x590576:0x590635);city.gold=city.food=1000;
            assert(runIncomeBonus(&city,100)==100 && (effect==87?city.gold:city.food)==1100);
            applied+=present;
        }
    }
    // Seven profiles supply gold and five supply food. The three empty/rejected
    // lists still receive native control grants, with no attributed skill event.
    skilltelemetry::close();assert(applied==12);assertBattleAppliedRows(L"build/income_native_collection_test.csv",12);
    *resume=oldResume;*table=oldTable;
    for(int n=0;n<14;++n)memcpy((void*)site(addresses[n]),saved[n],5);
    g_cityIncomeFlagsOriginal=oldFlags;g_goldIncomeGrantOriginal=oldGold;g_foodIncomeGrantOriginal=oldFood;
    puts("PASS native income collection: ten original city/list/status/flags inputs, first/second/composed/duplicate providers, absent base, failed append, original resource grants and flags-array sentinels");
}
