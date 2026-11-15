// Real city membership and caravan-count rules; native skill matching is stubbed here
// and exercised independently through the installed x86 hooks in skills_test.cpp.
static int __fastcall cityLimitValid(void* p,void*) {return *(int*)((uint8_t*)p+0x84);}
static int __fastcall cityLimitHasSkill(void* p,void*,int skill) {
    int owned=*(int*)((uint8_t*)p+0xe8);
    return skill==SKILL_MERCHANT && (owned==SKILL_MERCHANT || owned==249);
}
static void cityLimitCases() {
    auto oldPersons=g_person0;auto oldHas=personHasSkill;auto oldCars=g_car;
    auto oldCfg=g_cfg;
    g_cfg.PT=14613;g_cfg.specials=300;
    std::vector<uint8_t> records((size_t)g_cfg.PT*0x190);
    g_person0=records.data();personHasSkill=(HasSkillFn)cityLimitHasSkill;g_car.clear();
    void* vt[17]={};vt[2]=(void*)cityLimitValid;vt[16]=(void*)fixturePersonForce;
    auto p=g_person0+14162*0x190;*(void***)p=vt;
    *(int*)(p+0x84)=1;*(int*)(p+0x90)=0;*(int*)(p+0x9c)=0;*(int*)(p+0xa0)=3;
    *(int*)(p+0xe8)=-1;
    check(cityAutoLimit(0,0)==2 && cityAutoLimit(0,0,true)==2,
          "without merchant skill player/delegated and AI cities each allow two");
    *(int*)(p+0xe8)=SKILL_MERCHANT;
    check(cityAutoLimit(0,0)==4 && cityAutoLimit(0,0,true)==4 && cityAutoLimit(0,1)==2,
          "merchant skill doubles only the officer's physical city, including expanded historical IDs");
    auto second=p-0x190;memcpy(second,p,0x190);
    check(cityAutoLimit(0,0)==4 && cityAutoLimit(0,0,true)==4,"multiple merchants do not stack city slots");
    memset(second,0,0x190);
    for(int status:{4,5,6,7,8,-1}) {
        *(int*)(p+0xa0)=status;
        check(cityAutoLimit(0,0)==2,"free, captive, undiscovered, unborn and dead officers do not grant city slots");
    }
    for(int status:{0,1,2,3}) {
        *(int*)(p+0xa0)=status;
        check(cityAutoLimit(0,0)==4,"serving ruler, commander, governor and officer can organize trade");
    }
    *(int*)(p+0x9c)=g_data.C+45+1;
    check(cityAutoLimit(0,0)==2,"officer away with a unit grants no home-city bonus");
    *(int*)(p+0x9c)=1;
    check(cityAutoLimit(0,0)==2 && cityAutoLimit(0,1)==4,"relocation moves the bonus to the current city");
    *(int*)(p+0x90)=1;
    check(cityAutoLimit(0,1)==2,"foreign officer cannot grant the owner's city slots");
    *(int*)(p+0x90)=0;*(int*)(p+0x84)=0;
    check(cityAutoLimit(0,1)==2,"invalid native record cannot grant city slots");
    *(int*)(p+0x84)=1;*(int*)(p+0xe8)=249;
    check(cityAutoLimit(0,1)==4,"composite skill containing merchant effect grants the bonus");
    memcpy(g_person0+(size_t)(g_cfg.PT-g_cfg.specials)*0x190,p,0x190);memset(p,0,0x190);
    check(cityAutoLimit(0,1)==2,"temporary special officers never grant city slots");
    Caravan k={};k.force=0;k.origin=0;k.flags=F_PLAYER;k.automatic.caravan=1;
    for(auto state:{TRAVELLING,DOCKED,WAITING}) {k.state=state;g_car.push_back(k);}
    k.state=FINISHED;g_car.push_back(k);
    k.state=TRAVELLING;k.automatic.caravan=0;g_car.push_back(k); // manual player caravan
    k.flags=0;g_car.push_back(k); // AI caravan
    k.origin=1;g_car.push_back(k);k.force=1;g_car.push_back(k);
    check(cityAutoCount(0,0)==3 && cityAutoCount(0,0,true)==1 && cityAutoCount(0,1,true)==1,
          "count by origin and force; include waiting/returning, exclude finished and manual player caravans");
    g_car.pop_back();g_car.pop_back();
    check(cityAutoCount(0,0,true)==1 && cityAutoCount(0,1,true)==0 &&
          cityAutoCount(0,1,true)<cityAutoLimit(0,1,true),
          "an AI city's caravans never count against another city in the same force");
    check(cityAutoCount(0,0)>cityAutoLimit(0,0) && g_car.size()==6,
          "existing over-cap voyages remain intact after losing a skill or changing rules");
    g_person0=oldPersons;personHasSkill=oldHas;g_car=oldCars;g_cfg=oldCfg;
}
