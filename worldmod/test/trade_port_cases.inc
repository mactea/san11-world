static std::vector<uint8_t> portBases;
static uint8_t* __fastcall portTestBase(uint32_t,void*,int id) {
    return id>=0 && id<(int)portBases.size()/56 ? portBases.data()+id*56 : nullptr;
}
static int __cdecl portTestValid(const void* p) {return p && *(void* const*)p;}
static int __fastcall portTestOwner(void* p,void*) {return *(int*)((uint8_t*)p+0xc);}
static const int16_t* __fastcall portTestPosition(void* p,void*) {return (const int16_t*)((uint8_t*)p+0x1e);}
static void portAccessCases(uint8_t* units) {
    auto setup=g_s;auto data=g_data;auto oldBase=baseRec;auto valid=gameValid;auto oldCars=g_car;auto priorInfo=g_info;
    auto bytes=readFixture("worldmod/test/fixtures/port-transfer-map.bin");
    check(bytes.size()==12620 && !memcmp(bytes.data(),"TPRT",4),"real Beihai/Jianye map fixture is present");
    if(bytes.size()!=12620)return;
    const int* header=(const int*)(bytes.data()+4);int lo0=header[0],hi0=header[1],height=header[2],width=header[3];
    const int* rows=header+6;
    std::vector<uint8_t> terrain(1800*1000*20);
    for(size_t i=4;i<terrain.size();i+=20)terrain[i]=15;
    const auto* tiles=(const uint8_t*)(rows+37*4);
    g_s.C=530;g_s.R=249;g_s.W=1000;g_s.H=1800;g_s.hex20=terrain.data();
    g_data.C=530;g_data.lo.resize(530);g_data.hi.resize(530);
    for(int x=0;x<height;++x)for(int y=0;y<width;++y)cell(lo0+x,hi0+y)[4]=tiles[x*width+y];
    portBases.assign(575*56,0);void* vt[20]={};vt[15]=(void*)portTestPosition;vt[16]=(void*)portTestOwner;
    for(int i=0;i<37;++i) {
        int id=rows[i*4];auto b=portBases.data()+id*56;*(void**)b=vt;
        *(int*)(b+8)=id<530?0:2;*(int*)(b+0xc)=rows[i*4+3];
        *(int16_t*)(b+0x1e)=(int16_t)rows[i*4+1];*(int16_t*)(b+0x20)=(int16_t)rows[i*4+2];
    }
    baseRec=(GetFn)portTestBase;gameValid=portTestValid;
    check(!waterNear(1394,487) && !waterNear(1396,540),"real Beihai and Jianye remain outside direct three-hex shore access");
    auto beihai=shipAccess(7),jianye=shipAccess(22);
    printf("real ports: Beihai base %d steps %d fee %d days %d; Jianye base %d steps %d fee %d days %d\n",
        beihai.base,beihai.steps,beihai.fee(),beihai.days(),jianye.base,jianye.steps,jianye.fee(),jianye.days());
    check(beihai.base==545 && beihai.steps==15 && beihai.fee()==30 && beihai.days()==20,"Beihai reaches a live same-force port over land");
    check(jianye.base==556 && jianye.steps==6 && jianye.fee()==12 && jianye.days()==10,"Jianye reaches its actual nearby native port");
    check(shipAccessAt(7,556).base<0 && !nativePort(530) && !nativePort(575),"foreign ports, gates and facility IDs cannot become embarkation ports");
    auto b=portBases.data()+jianye.base*56;
    *(int*)(b+0xc)=-1;
    check(shipAccessAt(22,jianye.base).base<0,"unowned port cannot be borrowed");
    *(int*)(b+0xc)=1;
    check(shipAccessAt(22,jianye.base).base<0,"captured port invalidates its former city's freight connection");
    *(int*)(b+0xc)=24;
    uint8_t original[6];int n=0;
    for(int x=1395;x<=1397;++x)for(int y=539;y<=541;++y)if(hexDist(1396,540,x,y)==1) {
        original[n++]=cell(x,y)[4];cell(x,y)[4]=7;
    }
    check(portLandSteps(1396,540,jianye.lo,jianye.hi)<0,"freight cannot cross water even when a same-force port is geographically near");
    n=0;for(int x=1395;x<=1397;++x)for(int y=539;y<=541;++y)if(hexDist(1396,540,x,y)==1)cell(x,y)[4]=original[n++];
    check(portLandSteps(1396,540,1420,540)<0,"ports beyond twenty land steps are rejected");
    auto k=oldCars[0];k.origin=k.legOrigin=22;k.dest=7;k.force=24;k.shipType=MERCHANT_SHIP;
    k.embarkBase=556;k.arrivalBase=beihai.base;k.port=-1;k.state=TRAVELLING;k.transferStage=1;k.transferUntil=today()+10;
    k.flags=0;k.unit=-1;k.tempReleased=false;
    auto cargo=k.cargo.count();auto fees=k.ledger.fees;
    check(waitForTransfer(k,today()) && k.transferStage==1 && k.state==TRAVELLING && k.port==-1,
          "outbound handoff holds cargo and never exposes a docked market early");
    check(depart(k,7)=="port transfer in progress" && k.ledger.fees==fees,"repeated departure during transfer cannot charge or skip time");
    *(int*)(b+0xc)=1;
    check(waitForTransfer(k,today()+10) && k.state==WAITING && k.transferStage==1 && k.cargo.count()==cargo,
          "losing the departure port pauses the handoff without deleting cargo");
    *(int*)(b+0xc)=24;
    check(!waitForTransfer(k,today()+10) && !k.transferStage && !k.transferUntil && k.state==TRAVELLING && k.ledger.fees==fees,
          "restored port completes elapsed handoff without charging twice");
    k.dest=22;k.arrivalBase=556;k.state=TRAVELLING;
    arriveAtMarket(k,today());int deadline=k.transferUntil;
    check(k.transferStage==2 && k.port==-1 && deadline>today() && waitForTransfer(k,deadline-1),
          "arrival at the quay starts port-to-city delivery instead of immediately selling cargo");
    check(tradeCargo(k,0,1,false)=="caravan must be docked at an accessible market",
          "sale command rejects cargo during inbound handoff");
    *(int*)(b+0xc)=1;
    check(waitForTransfer(k,deadline) && k.state==WAITING && k.port==-1,"lost arrival port cannot deliver into a city market");
    *(int*)(b+0xc)=24;
    check(waitForTransfer(k,deadline) && !k.transferStage && k.state==DOCKED && k.port==22 && k.cargo.count()==cargo && k.ledger.fees==fees,
          "inbound handoff completes exactly once and preserves cargo and ledger for manual/automatic trading");
    // The registered ship's berth candidates are around the port, never inland at Jianye.
    g_car[0]=k;g_car[0].unit=1;g_car[0].leader=20;g_car[0].embarkBase=556;
    void* unitVt[16]={};unitVt[15]=(void*)shipTestPosition;*(void**)(units+0xf4)=unitVt;
    uint32_t water=0;
    for(int x=jianye.lo-3;x<=jianye.lo+3 && !water;++x)for(int y=jianye.hi-3;y<=jianye.hi+3;++y)
        if(hexDist(jianye.lo,jianye.hi,x,y)<=3 && waterHex(cell(x,y))) {water=pack(x,y);break;}
    *(uint32_t*)(units+0xf4+0xe0)=water;rebuildDirs();std::vector<uint32_t> path;int cost=0;
    g_car[0].state=TRAVELLING;g_car[0].port=-1;g_car[0].transferStage=1;g_car[0].transferUntil=today()+10;rebuildDirs();
    check(!movementAllowed(units+0xf4),"native movement cannot sail away while outbound freight is pending");
    g_car[0].transferStage=0;g_car[0].transferUntil=0;rebuildDirs();
    check(movementAllowed(units+0xf4) && !shipStepAllowed(units+0xf4,cell(1396,540)),
          "released ship can sail but cannot use Jianye's inland city as a land shortcut");
    check(marketRoute(units+0xf4,22,&path,&cost,nullptr,556) && !path.empty() &&
          waterHex(cell((int16_t)path.back(),(int16_t)(path.back()>>16))) &&
          hexDist(jianye.lo,jianye.hi,(int16_t)path.back(),(int16_t)(path.back()>>16))<=3,
          "native route targets an actual water berth beside the selected port");
    *(int*)(b+0xc)=1;
    check(!marketRoute(units+0xf4,22,&path,&cost,nullptr,556),"cached destination port is revalidated before native routing");
    *(void**)(units+0xf4)=nullptr;
    g_s=setup;g_data=data;g_car=oldCars;g_info=priorInfo;baseRec=oldBase;gameValid=valid;rebuildDirs();
}
static void portQuoteCases() {
    auto prices=g_data.price;g_data.price={10,20,500,600};
    tradeui::Model m;m.d=&g_data;m.live=g_mk;m.live.stock={100,100,0,0};m.seen=m.live;
    m.force=0;m.owner={0,1};m.stance={2,1};m.known={1,1};m.age={0,0};m.waterAccess={1,1};
    m.gold={20000,0};m.troops={5000,0};m.food={30000,0};m.grainRate={50,50};m.shipType=MERCHANT_SHIP;
    auto direct=tradeui::basketPlan(m,0,1,{{0,40},{1,20}},-1);
    m.portSteps={15,6};auto via=tradeui::basketPlan(m,0,1,{{0,40},{1,20}},-1);
    check(via.ok && via.cost==direct.cost+42 && via.profit==direct.profit-42 && via.eta==direct.eta+3,
          "manual basket includes both ends' freight fee and rounds each transfer's travel time separately");
    m.merchantCities={1,0};check(tradeui::quoteFee(m,0,1)==240+42,"merchant discount applies to formation, not twice to freight");
    m.merchantCities.clear();tradeauto::Plan p;p.force=0;p.origin=0;p.preferredDestination=1;p.shipType=MERCHANT_SHIP;
    p.budget.limit=20000;p.budget.advanceMonth(today()/30);
    auto offers=autoOffersModel(p,0,m,22000);
    check(!offers.empty() && offers[0].quote.cost==offers[0].quote.goods+342 &&
          offers[0].reserve==offers[0].quote.duty+offers[0].quote.transitLimit+102,
          "automatic outbound budget includes freight and reserves both ends of the return handoff");
    p.origin=1;p.preferredDestination=0;m.owner={1,0};auto back=autoOffersModel(p,0,m,22000,1);
    check(!back.empty() && back[0].quote.cost==back[0].quote.goods+102 &&
          back[0].reserve==back[0].quote.duty+back[0].quote.transitLimit,
          "automatic return basket pays continuation and two handoffs once without a second formation fee");
    m.departureFee=48;check(tradeui::quoteFee(m,0,1)==90,"explicit discounted return fee still adds unduplicated freight");
    m.shipType=CARAVAN;m.departureFee=-1;
    check(tradeui::quoteFee(m,0,1)==100 && tradeui::transferSteps(m,0)==0,"ordinary caravans acquire no port costs or delays");
    g_data.price=prices;
}
static void portSaveCases() {
    auto before=payloadBytes();int cars=0;auto& k=g_car[0];
    k.shipType=MERCHANT_SHIP;k.legOrigin=k.origin;k.embarkBase=g_s.C+10;k.arrivalBase=g_s.C+11;
    k.port=-1;k.state=TRAVELLING;k.transferStage=1;k.transferUntil=today()+20;k.tempReleased=false;
    auto bytes=payloadBytes();auto packed=blockBytes();
    check(parseBlock(packed.data()+12,packed.size()-12,13,&cars) && payloadBytes()==bytes && g_car[0].transferUntil==today()+20,
          "v13 saves exact port choices, handoff phase and remaining deadline without charging again");
    for(int offset:{4,8,12,16,20}) {
        auto bad=bytes;int invalid=INT_MAX;memcpy(bad.data()+bad.size()-offset,&invalid,4);
        check(!parseBlock(bad.data(),bad.size(),13,&cars,true) && payloadBytes()==bytes,
              "invalid handoff fields reject the save atomically");
    }
    check(!parseBlock(bytes.data(),bytes.size()-1,13,&cars,true) && payloadBytes()==bytes,"truncated port extension is atomic");
    auto legacy=payloadBytes(true,true,true,true,true,true,false);
    check(parseBlock(legacy.data(),legacy.size(),12,&cars,true) && !g_car[0].transferStage && g_car[0].embarkBase==-1,
          "existing v12 voyages migrate without new fees, teleports or invented handoff delays");
    check(parseBlock(before.data(),before.size(),TRAD_VERSION,&cars,true),"port serializer tests restore original trade state");
}
