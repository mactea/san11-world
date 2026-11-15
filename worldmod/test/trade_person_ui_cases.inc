static std::vector<uint32_t> personRows;
static int __fastcall acceptPersonRow(void*,void*,int,uint32_t low,uint32_t) {
    personRows.push_back(low);return 1;
}
static int __fastcall fixturePersonForce(void* p,void*) { return *(int*)((uint8_t*)p+0x90); }
struct RankTestNode {RankTestNode* next;RankTestNode* prev;uint8_t* person;};
struct RankTestList {void* vt;RankTestNode* head;RankTestNode* tail;int count;};
static int rankSourceCalls;
static int __fastcall rankSource(void* owner,void*,void* list,void* force,int mask) {
    check(owner==(void*)1 && force==(void*)2 && mask==0x3f,"appointment hook forwards native force and eligibility mask");
    ++rankSourceCalls;return ((RankTestList*)list)->count;
}
static void __fastcall rankErase(void* list,void*,void* item) {
    auto l=(RankTestList*)list;auto n=(RankTestNode*)item;
    if(n->prev)n->prev->next=n->next;else l->head=n->next;
    if(n->next)n->next->prev=n->prev;else l->tail=n->prev;
    --l->count;n->next=n->prev=nullptr;
}
static void rankCandidateCases(uint8_t* merchant,uint8_t* ordinary) {
    auto source=g_rankPeopleOrig;auto erase=g_rankListErase;
    g_rankPeopleOrig=(RankPeopleFn)rankSource;g_rankListErase=(ListEraseFn)rankErase;
    LONG removed=g_hiddenRankCandidates;int calls=rankSourceCalls;
    for(auto people:{std::vector<uint8_t*>{merchant,merchant,ordinary},
                    std::vector<uint8_t*>{ordinary,merchant},
                    std::vector<uint8_t*>{merchant},std::vector<uint8_t*>{}}) {
        std::vector<RankTestNode> nodes(people.size());
        for(size_t i=0;i<nodes.size();++i)nodes[i]={i+1<nodes.size()?&nodes[i+1]:nullptr,i?&nodes[i-1]:nullptr,people[i]};
        RankTestList list={nullptr,nodes.empty()?nullptr:&nodes.front(),nodes.empty()?nullptr:&nodes.back(),(int)nodes.size()};
        int expected=(int)std::count(people.begin(),people.end(),ordinary);
        int result=rankPeopleHook((void*)1,nullptr,&list,(void*)2,0x3f);
        bool valid=result==expected && list.count==expected;
        if(expected)valid &= list.head && list.head==list.tail && list.head->person==ordinary && !list.head->next && !list.head->prev;
        else valid &= !list.head && !list.tail;
        check(valid,"appointment and automatic-assignment source excludes merchants with correct links and count");
    }
    check(rankSourceCalls==calls+4 && g_hiddenRankCandidates==removed+4,"candidate diagnostics count removed merchants rather than displayed rows");
    RankTestNode node={nullptr,nullptr,merchant};RankTestList list={nullptr,&node,&node,1};
    bool enabled=g_on;g_on=false;
    check(rankPeopleHook((void*)1,nullptr,&list,(void*)2,0x3f)==1 && list.head==&node,"disabled trade preserves native appointment candidates");
    g_on=enabled;g_rankPeopleOrig=source;g_rankListErase=erase;
}
static void personUiCases() {
    auto before=payloadBytes();bool enabled=g_on;auto oldPersons=g_person0;auto oldInsert=g_listRowOrig;
    std::vector<uint8_t> records((size_t)g_cfg.PT*0x190);
    g_person0=records.data();g_on=true;g_listRowOrig=(ListRowFn)acceptPersonRow;
    int leader=g_cfg.PT-g_cfg.specials+1;auto p=g_person0+(size_t)leader*0x190;
    void* personVtable[17]={};personVtable[16]=(void*)fixturePersonForce;*(void***)p=personVtable;
    p[4]=0xb0;p[5]=0xd3;*(int*)(p+0x94)=0;
    g_car[0].leader=leader;g_car[0].flags|=F_TEMP;g_car[0].tempReleased=false;
    for(auto state:{TRAVELLING,DOCKED,WAITING}) {
        g_car[0].state=state;rebuildDirs();
        check(hiddenTradePerson(p),"temporary merchant hidden while travelling, docked or waiting");
    }
    // Heading/automatic stage does not change the temporary identity.
    g_car[0].automatic.stage=tradeauto::Stage::Returning;rebuildDirs();
    check(hiddenTradePerson(p),"return voyage keeps temporary officer hidden");
    g_car[0].automatic={};
    auto registered=g_car;g_car.clear();rebuildDirs();
    check(!hiddenTradePerson(p),"unregistered special officer is not hidden by name alone");
    auto orphan=g_person0+(size_t)(leader+1)*0x190;
    memcpy(orphan,p,0x190);
    const uint8_t stats[5]={40,30,50,60,50};memcpy(orphan+0xc8,stats,5);
    *(int*)(orphan+0x44)=186;*(int*)(orphan+0x48)=166;*(int*)(orphan+0x4c)=256;
    *(int*)(orphan+0xa0)=3;
    check(hiddenTradePerson(orphan),"legacy generated merchant remains hidden without a caravan record");
    auto normal=g_person0+20*0x190;memcpy(normal,orphan,0x190);
    check(!hiddenTradePerson(normal),"complete legacy fingerprint never hides a historical officer");
    rankCandidateCases(orphan,normal);
    ++orphan[0xc8];check(!hiddenTradePerson(orphan),"same name with different base abilities is not a legacy merchant");--orphan[0xc8];
    ++orphan[9];check(!hiddenTradePerson(orphan),"legacy fingerprint requires the complete generated name");--orphan[9];
    ++*(int*)(orphan+0x44);check(!hiddenTradePerson(orphan),"legacy fingerprint requires matching generated dates");--*(int*)(orphan+0x44);
    *(int*)(orphan+0xa0)=8;check(!hiddenTradePerson(orphan),"freed legacy special slot is not hidden");*(int*)(orphan+0xa0)=3;
    *(int*)(orphan+0x94)=-1;check(!hiddenTradePerson(orphan),"unowned special NPC is not hidden");*(int*)(orphan+0x94)=0;
    *(int*)(orphan+0x48)=300;*(int*)(orphan+0x44)=320;*(int*)(orphan+0x4c)=353;
    check(hiddenTradePerson(orphan),"legacy fingerprint supports the native death-year cap");
    memset(orphan,0,0x190);
    markHiddenTemp(leader,0);
    check(hiddenTradePerson(p),"new merchant is hidden before sortie publishes its caravan record");
    markHiddenTemp(leader,-1);
    check(!hiddenTradePerson(p),"failed creation releases the pending UI identity");
    g_car=registered;rebuildDirs();
    auto historical=g_person0+20*0x190,custom=g_person0+(size_t)(g_cfg.PT-g_cfg.specials-1)*0x190;
    memcpy(historical,p,0x190);memcpy(custom,p,0x190);
    check(!hiddenTradePerson(historical) && !hiddenTradePerson(custom),"same-name historical and custom officers remain visible");
    for(int id=g_cfg.PT-g_cfg.specials;id<g_cfg.PT;++id) {
        auto slot=g_person0+(size_t)id*0x190;
        if(id==leader)continue;
        memcpy(slot,p,0x190);markHiddenTemp(id,0);
        check(hiddenTradePerson(slot),"every configured special slot supports merchant hiding");
        markHiddenTemp(id,-1);memset(slot,0,0x190);
    }
    *(int*)(p+0x94)=1;check(hiddenTradePerson(p) && tempPersonMatches(p,g_car[0].force),"native district reassignment cannot expose a merchant or prevent its arrival cleanup");
    *(int*)(p+0x90)=1;check(!hiddenTradePerson(p),"reused special slot with another force remains visible");*(int*)(p+0x90)=0;
    *(int*)(p+0x94)=0;p[4]='X';check(!hiddenTradePerson(p),"reused special slot with another name remains visible");p[4]=0xb0;
    check(!hiddenTradePerson(nullptr) && !hiddenTradePerson(p+1) &&
          !hiddenTradePerson(g_person0+(size_t)g_cfg.PT*0x190),"null, misaligned and out-of-range rows are safe");
    auto low=(uint32_t)(uintptr_t)p;auto ordinary=(uint32_t)(uintptr_t)historical;
    personRows.clear();LONG filtered=g_hiddenRows;
    check(!listRowHook(nullptr,nullptr,INT_MAX,low,0) &&
          !listRowHook(nullptr,nullptr,INT_MAX,low,0xffffffff),"pointer rows are suppressed before native insertion, including sign extension");
    check(listRowHook(nullptr,nullptr,INT_MAX,ordinary,0)==1 &&
          listRowHook(nullptr,nullptr,INT_MAX,42,0)==1 &&
          listRowHook(nullptr,nullptr,INT_MAX,low,7)==1 && personRows.size()==3 &&
          personRows[0]==ordinary && g_hiddenRows==filtered+2,"ordinary rows and composite data keep insertion order/count without blank rows");
    rankCandidateCases(p,historical);
    // A UI read must complete even while a logic-thread trade operation owns g_cs.
    EnterCriticalSection(&g_cs);
    HANDLE thread=CreateThread(nullptr,0,[](void* arg)->DWORD{return hiddenTradePerson((uint8_t*)arg)?1:0;},p,0,nullptr);
    DWORD waited=thread?WaitForSingleObject(thread,2000):WAIT_FAILED;
    LeaveCriticalSection(&g_cs);
    if(thread){WaitForSingleObject(thread,2000);CloseHandle(thread);}
    check(waited==WAIT_OBJECT_0,"UI identity lookup never waits for the trade lock");
    g_car[0].state=DOCKED;g_car[0].port=1;
    auto saved=blockBytes();int cars=0;
    resetState();check(!hiddenTradePerson(p),"scenario/save reset clears hidden identities");
    check(parseBlock(saved.data()+12,saved.size()-12,TRAD_VERSION,&cars) && hiddenTradePerson(p),
          "save reload rebuilds the merchant filter from persisted caravan identity");
    g_car[0].tempReleased=true;rebuildDirs();
    auto released=blockBytes();
    check(!hiddenTradePerson(p) && parseBlock(released.data()+12,released.size()-12,TRAD_VERSION,&cars) &&
          !hiddenTradePerson(p),"landed/recycled merchant stays released after save reload");
    g_car[0].tempReleased=false;g_car[0].state=FINISHED;rebuildDirs();
    check(!hiddenTradePerson(p),"completed caravan drops its UI lease");
    g_car[0].state=TRAVELLING;g_car[0].flags|=F_LOOTED;rebuildDirs();
    check(!hiddenTradePerson(p),"destroyed caravan drops its UI lease");
    g_car[0].flags&=~F_LOOTED;rebuildDirs();g_on=false;
    check(!hiddenTradePerson(p),"disabled trade never filters native lists");
    check(parseBlock(before.data(),before.size(),TRAD_VERSION,&cars,true),"person UI fixture restores prior runtime state");
    g_on=enabled;g_person0=oldPersons;g_listRowOrig=oldInsert;rebuildDirs();
}
