static uint8_t* merchantTestBases;static uint8_t* merchantTestUnits;
static void** merchantTestVtable;static int merchantTestUid=1,merchantTestFault=0;
static int __cdecl merchantTestValid(const void* p){return p && *(int*)((const uint8_t*)p+0x84);}
static int __fastcall merchantTestGold(void* p,void*){return *(int*)((uint8_t*)p+0xa8);}
static int __fastcall merchantTestFood(void* p,void*){return *(int*)((uint8_t*)p+0xac);}
static int __fastcall merchantTestTroops(void* p,void*){return *(int*)((uint8_t*)p+0xb0);}
static int __fastcall merchantTestCorps(void*,void*){return 0;}
static int __fastcall merchantTestPlayer(void* p,void*){return *(int*)((uint8_t*)p+0x90)==0;}
static uint8_t* __fastcall merchantTestBase(uint32_t,void*,int id){return id>=0 && id<2?merchantTestBases+id*0x200:nullptr;}
static uint8_t* __fastcall merchantTestPerson(uint32_t,void*,int id){return id>=0 && id<g_cfg.PT?g_person0+(size_t)id*0x190:nullptr;}
static uint8_t* __fastcall merchantTestUnit(uint32_t,void*,int id){return id>=0 && id<20?merchantTestUnits+id*0xf4:nullptr;}
static uint8_t* __fastcall merchantTestForce(uint32_t,void*,int){return merchantTestBases;}
static uint8_t __fastcall merchantTestAbility(void*,void*,int kind){return kind==3?60:50;}
static int merchantTestAdd(void* p,int delta,int offset,int cap){int& value=*(int*)((uint8_t*)p+offset);int before=value;value=std::max(0,std::min(cap,value+delta));return value-before;}
static int __fastcall merchantTestAddGold(uint32_t,void*,void* p,int delta){
    int result=merchantTestAdd(p,delta,0xa8,100000);
    if(delta<0){auto actor=g_person0+14162*0x190;
        if(merchantTestFault==4)skilltelemetry::invalidateContext();
        if(merchantTestFault==5)*(int*)(actor+0xe8)=-1;
        if(merchantTestFault==6)++*(int*)(actor+0x9c);
    }
    if(delta>0 && merchantTestFault==9)skilltelemetry::invalidateContext();
    return result;
}
static int __fastcall merchantTestAddFood(uint32_t,void*,void* p,int delta){return merchantTestAdd(p,delta,0xac,500000);}
static int __fastcall merchantTestAddTroops(uint32_t,void*,void* p,int delta){return merchantTestFault==2 && delta<0?0:merchantTestAdd(p,delta,0xb0,100000);}
static int __fastcall merchantTestSortie(uint32_t,void*,uint8_t* rec){
    if(merchantTestFault==1)return -1;
    int id=merchantTestUid++;auto u=merchantTestUnits+id*0xf4;*(void***)u=merchantTestVtable;
    *(int*)(u+0x20)=*(int*)(rec+0x18);*(int*)(u+0xb0)=*(int*)(rec+0x1c);
    *(int*)(u+0x84)=1;*(int*)(u+8)=1;*(int*)(u+0xc)=*(int*)(rec+8);
    *(uint32_t*)(u+0xe0)=*(uint32_t*)(rec+4);*(int*)(u+0x90)=0;
    *(int*)(g_person0+(size_t)*(int*)(rec+8)*0x190+0x9c)=g_data.C+45+id;
    return id;
}
static int __fastcall merchantTestOrder(uint32_t,void*,void*,int,uint32_t,int){return 1;}
static int __fastcall merchantTestExecute(uint32_t,void*,void*){return merchantTestFault==3?0:1;}
static void __fastcall merchantTestSet(void*,void*,int){}
static void __fastcall merchantTestSetFood(void* unit,void*,int food){*(int*)((uint8_t*)unit+0x20)=std::min(100000,std::max(0,food));}
static void __fastcall merchantTestRemove(uint32_t,void*,uint8_t* p,int){*(int*)(p+0x84)=0;}
static bool merchantTestActor(const void* p,skilltelemetry::Actor& out){
    out.person=(int)(((const uint8_t*)p-g_person0)/0x190);out.force=*(int*)((const uint8_t*)p+0x90);out.ruler=635;
    out.city=*(int*)((const uint8_t*)p+0x9c);if(out.city>=g_data.C)out.city=-1;
    out.year=194;out.month=6;out.day=1;strcpy_s(out.name,"Merchant");return true;
}
#include "trade_safety_cases.inc"
static void merchantTelemetryCases(){
    auto oldData=g_data;auto oldSetup=g_s;auto oldCfg=g_cfg;auto oldMarket=g_mk;auto oldInfo=g_info;
    auto oldPersons=g_person0,oldUnits=g_unit0;auto oldBase=baseRec;auto oldValid=gameValid;auto oldHas=personHasSkill;
    auto oldCars=g_car;auto oldCredit=g_credit;auto oldTax=g_saleTax;auto oldNext=g_nextCar;
    auto oldHidden=g_hiddenTempForces;g_hiddenTempForces.assign(300,0);
    auto oldGold=baseGold;auto oldFood=baseFood;auto oldPersonRec=personRec;auto oldUnitRec=unitRec;auto oldForceRec=forceRec;auto oldCorpsRec=corpsRec;
    auto oldAbility=ability;auto oldAddGold=addGold;auto oldAddFood=addFood;auto oldAddTroops=addTroops;
    auto oldSortie=sortie;auto oldOrder=setOrder;auto oldExecute=execOrder;auto oldSetFood=unitFoodSet;auto oldSetTroops=unitTroopsSet;auto oldRemove=removeUnit;
    baseGold=(GoldFn)merchantTestGold;baseFood=(GoldFn)merchantTestFood;personRec=(GetFn)merchantTestPerson;
    unitRec=(GetFn)merchantTestUnit;forceRec=corpsRec=(GetFn)merchantTestForce;
    ability=(AbilFn)merchantTestAbility;addGold=(AddGoldFn)merchantTestAddGold;addFood=(AddGoldFn)merchantTestAddFood;addTroops=(AddGoldFn)merchantTestAddTroops;
    sortie=(SortieFn)merchantTestSortie;setOrder=(OrderFn)merchantTestOrder;execOrder=(ExecFn)merchantTestExecute;
    unitTroopsSet=(SetIntFn)merchantTestSet;unitFoodSet=(SetIntFn)merchantTestSetFood;removeUnit=(RemoveFn)merchantTestRemove;
    std::vector<uint8_t> persons(14613*0x190),units(20*0xf4),bases(2*0x200),map(20*20*20);
    g_person0=persons.data();g_unit0=units.data();merchantTestUnits=g_unit0;merchantTestBases=bases.data();
    g_cfg.PT=14613;g_cfg.specials=300;g_s.W=g_s.H=20;g_s.hex20=map.data();
    g_data.lo={2,10};g_data.hi={2,10};g_data.cityName={"Origin","Market"};g_data.goodName={"Silk","Salt"};
    g_data.price={100,100,300,300};baseRec=(GetFn)merchantTestBase;gameValid=merchantTestValid;personHasSkill=(HasSkillFn)cityLimitHasSkill;
    void* vt[21]={};vt[2]=cityLimitValid;vt[0x3c/4]=shipTestPosition;vt[0x40/4]=fixturePersonForce;
    vt[0x44/4]=merchantTestCorps;vt[0x48/4]=merchantTestPlayer;vt[0x50/4]=merchantTestTroops;merchantTestVtable=vt;
    auto prepare=[&](int owned,bool ownLeader){
        std::fill(persons.begin(),persons.end(),0);std::fill(units.begin(),units.end(),0);std::fill(bases.begin(),bases.end(),0);
        for(int c=0;c<2;++c){auto b=merchantTestBases+c*0x200;*(void***)b=vt;*(int*)(b+0x84)=1;
            *(int*)(b+0xa8)=10000;*(int*)(b+0xac)=100000;*(int*)(b+0xb0)=10000;*(uint32_t*)(b+0xe0)=pack(g_data.lo[c],g_data.hi[c]);}
        for(int id:{600,14162}){auto p=g_person0+(size_t)id*0x190;*(void***)p=vt;*(int*)(p+0x84)=1;*(int*)(p+0xa0)=3;*(int*)(p+0xe8)=-1;}
        *(int*)(g_person0+14162*0x190+0xe8)=owned;
        g_car.clear();g_credit.clear();g_saleTax.clear();g_nextCar=12;merchantTestUid=1;merchantTestFault=0;
        g_mk.d=&g_data;g_mk.reset();g_mk.stock={100,100,0,0};g_info.reset(2,g_data,12);
        for(int n=0;n<2;++n){Caravan k={};k.id=n+1;k.force=0;k.origin=0;k.leader=610+n;k.state=WAITING;k.flags=F_PLAYER;k.automatic.caravan=k.id;g_car.push_back(k);}
        return ownLeader?14162:600;
    };
    auto create=[&](int leader,bool ai=false){tradeauto::Voyage automatic={};automatic.fees.balance=1000;
        return createCaravanOrder(0,1,{{0,1}},leader,ai,nullptr,ai?nullptr:&automatic);};
    check(skilltelemetry::open(L"worldmod/build/merchant_effects_test.csv",merchantTestActor,nullptr),"merchant CSV opens");
    for(int owned:{147,249})for(bool ownLeader:{false,true}){
        int leader=prepare(owned,ownLeader);auto error=create(leader);check(error.empty(),"merchant full creation succeeds");
        if(!error.empty()){printf("merchant create error: %s\n",error.c_str());continue;}
        check(cityAutoCount(0,0)==3 && g_car.back().ledger.fees>0,"merchant third caravan commits beyond ordinary city limit");
        if(!ownLeader){auto& k=g_car.back();k.port=1;k.state=DOCKED;std::vector<int> experience;
            auto before=baseGold(merchantTestBases);auto receipts=k.ledger.receipts;
            check(tradeCargo(k,0,1,false,&experience).empty() && k.ledger.receipts>receipts && baseGold(merchantTestBases)>before,
                  "merchant actual sale credits bonus payout");
            check(depart(k,0).empty(),"merchant continuation pays discounted fee from automatic escrow");}
    }
    int cappedLeader=prepare(147,false);check(create(cappedLeader).empty(),"merchant capped-credit sale is created");
    {auto& car=g_car.back();car.port=1;car.state=DOCKED;*(int*)(merchantTestBases+0xa8)=100000;
        std::vector<int> experience;check(tradeCargo(car,0,1,false,&experience).empty() && !g_credit.empty() && g_credit[0].gold>0 &&
            baseGold(merchantTestBases)==100000,"merchant bonus survives a full native treasury as pending credit");
        check(!tradeCargo(car,0,1,false,&experience).empty(),"repeated sale cannot duplicate merchant bonus");}
    prepare(147,false);*(int*)(g_person0+600*0x190+0xe8)=249;
    check(create(600).empty(),"first composite merchant is credited before a later raw merchant");
    prepare(147,false);g_car.resize(1);g_car[0].flags=0;g_car[0].force=1;
    for(auto p:{merchantTestBases,merchantTestBases+0x200,g_person0+600*0x190,g_person0+14162*0x190})*(int*)(p+0x90)=1;
    check(create(600,true).empty() && cityAutoCount(1,0,true)==2,"AI merchant commits its second caravan beyond ordinary limit one");
    skilltelemetry::close();check(skilltelemetry::open(L"worldmod/build/merchant_effects_edges_test.csv",merchantTestActor,nullptr),"merchant edge CSV opens");
    for(int fault:{1,2,3,4,5,6}){int leader=prepare(147,false);merchantTestFault=fault;auto error=create(leader);
        check((fault<=3)==!error.empty(),"merchant creation failures/refunds and attribution invalidation preserve gameplay");
        if(fault<=3)check(g_car.size()==2 && baseGold(merchantTestBases)==10000 && baseFood(merchantTestBases)==100000 &&
            vcall(merchantTestBases,0x50)==10000,"failed merchant formation refunds all reserved resources without a committed voyage");}
    int leader=prepare(147,false);check(create(leader).empty(),"merchant stale-sale control is created");
    auto& k=g_car.back();k.port=1;k.state=DOCKED;merchantTestFault=9;std::vector<int> experience;
    check(tradeCargo(k,0,1,false,&experience).empty(),"merchant world change after native credit preserves actual sale");
    cityTradeFee(0,0,CARAVAN);cityAutoLimit(0,0); // previews add no committed rows
    skilltelemetry::close();prepare(147,false);check(create(600).empty(),"disabled merchant logging preserves successful formation");
    safetyCases(g_car.back());
    g_data=oldData;g_s=oldSetup;g_cfg=oldCfg;g_mk=oldMarket;g_mk.d=&g_data;g_info=oldInfo;
    g_person0=oldPersons;g_unit0=oldUnits;baseRec=oldBase;gameValid=oldValid;personHasSkill=oldHas;
    g_car=oldCars;g_credit=oldCredit;g_saleTax=oldTax;g_nextCar=oldNext;g_hiddenTempForces=oldHidden;rebuildDirs();
    baseGold=oldGold;baseFood=oldFood;personRec=oldPersonRec;unitRec=oldUnitRec;forceRec=oldForceRec;corpsRec=oldCorpsRec;
    ability=oldAbility;addGold=oldAddGold;addFood=oldAddFood;addTroops=oldAddTroops;sortie=oldSortie;setOrder=oldOrder;execOrder=oldExecute;
    unitFoodSet=oldSetFood;unitTroopsSet=oldSetTroops;removeUnit=oldRemove;
}
