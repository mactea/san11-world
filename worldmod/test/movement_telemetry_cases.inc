// Execute the original hazard calculator through the installed settlement call.
// Cell storage, the RNG and native independent cliff technology are controlled fixtures.
static uintptr_t g_hazardEntry;
static int g_hazardRandomCalls=0;
static bool g_cliffTechnology=false;
__declspec(naked) static int __cdecl runHazard(int* out,void* unit,uint32_t hex) {
    __asm {
        push dword ptr [esp+12]
        push dword ptr [esp+12]
        push dword ptr [esp+12]
        mov eax,g_hazardEntry
        jmp eax
    }
}
__declspec(naked) static void hazardReturned() {
    __asm {add esp,12
           ret}
}
static int __cdecl hazardRandom(int range) {
    assert(range==50 || range==200);++g_hazardRandomCalls;return 17;
}
static int __fastcall hazardTechnology(void*,void*,int id) {assert(id==17);return g_cliffTechnology;}
static void movementTelemetryTests() {
    uint8_t first[400]={},second[400]={},unit[0xf4]={},cells[203*20]={};
    auto oldFirst=g_upkeepActor,oldSecond=g_secondIncomeActor,oldUnit=g_battleUnit;
    int oldFirstId=g_battleFirstId,oldSecondId=g_battleSecondId;
    auto oldId=g_telemetryUnitId;auto oldHazard=g_movementHazardOriginal;auto oldMap=g_movementMapOperand;
    uint8_t mapCode[0x40]={};g_movementMapOperand=(const uint32_t*)(mapCode+0x3a);
    *(int*)(mapCode+0x13)=*(int*)(mapCode+0x1f)=*(int*)(mapCode+0x2a)=200;
    *(uint32_t*)(mapCode+0x3a)=(uint32_t)cells;
    g_upkeepActor=first;g_secondIncomeActor=second;g_battleUnit=unit;
    g_battleFirstId=600;g_battleSecondId=14162;
    g_telemetryUnitId=(BuildingId)battleUnitId;
    g_movementHazardOriginal=(MovementHazard)site(0x5ae500);
    // Relocate the native LEA's absolute grid base into this fixture.
    auto cellOperand=(uint32_t*)site(0x5ae51f);uint32_t oldOperand=*cellOperand;
    *cellOperand=(uint32_t)cells;
    domesticStub(0x472150,hazardRandom);domesticStub(0x5848a9,hazardReturned);
    g_hazardEntry=site(0x5848a4);
    void* vt[25]={};vt[0x40/4]=battleOwner;vt[0x58/4]=(void*)site(0x47a8a0);
    vt[0x5c/4]=battleFind;vt[0x60/4]=hazardTechnology;*(void**)unit=vt;
    bool oldVine=g_skills[249].effects[31];g_skills[249].effects[31]=true;
    bool saved[3];int effects[]={6,8,62};
    for(int i=0;i<3;++i){saved[i]=g_skills[249].effects[effects[i]];g_skills[249].effects[effects[i]]=true;}
    const uint32_t hex=0x00020001; // x=1, y=2; native index x*200+y
    auto setCell=[&](int terrain,bool fire) {
        *(uint32_t*)(cells+202*20)=fire?0x00800000:0;
        *(uint32_t*)(cells+202*20+4)=terrain;
    };
    int damage[2]={-1,-1};
    *(int*)(first+0xe8)=6;*(int*)(second+0xe8)=-1;setCell(11,false);
    int calls=g_hazardRandomCalls;
    assert(runHazard(damage,unit,hex)==0 && damage[0]==0 && damage[1]==0);
    assert(calls==g_hazardRandomCalls && !g_terrainProbe);
    assert(skilltelemetry::open(L"build/movement_telemetry_test.csv",battleActor,nullptr));
    for(int effect:effects) {
        setCell(effect==6?11:effect==8?4:0,effect==62);
        for(int mode=0;mode<4;++mode) {
            *(int*)(first+0xe8)=mode==2?-1:mode==1?249:effect;
            *(int*)(second+0xe8)=mode>=2?effect:-1;
            damage[0]=damage[1]=-1;calls=g_hazardRandomCalls;
            assert(runHazard(damage,unit,hex)==(effect==62?1:0));
            assert(damage[0]==0 && damage[1]==0 && calls==g_hazardRandomCalls && !g_terrainProbe);
        }
        // Direct UI/AI-style use of the same calculator: eligibility only, never applied.
        calls=g_hazardRandomCalls;
        assert(g_movementHazardOriginal(damage,unit,hex)==(effect==62?1:0));
        assert(calls==g_hazardRandomCalls && !g_terrainProbe);
        *(int*)(first+0xe8)=*(int*)(second+0xe8)=-1;
        assert(runHazard(damage,unit,hex)==1 && damage[1]==(effect==6?117:effect==8?217:67));
        assert(g_hazardRandomCalls==calls+1 && !g_terrainProbe);
    }
    // Native technology 17 independently prevents cliff loss without a skill provider.
    setCell(11,false);g_cliffTechnology=true;calls=g_hazardRandomCalls;
    assert(runHazard(damage,unit,hex)==0 && damage[1]==0 && calls==g_hazardRandomCalls);
    g_cliffTechnology=false;
    // Fire is checked first, even on a poisonous tile. Attribute only to the fire provider.
    setCell(4,true);*(int*)(first+0xe8)=8;*(int*)(second+0xe8)=62;
    calls=g_hazardRandomCalls;
    assert(runHazard(damage,unit,hex)==1 && damage[1]==0 && calls==g_hazardRandomCalls);
    // Repeating a real settlement is two events; a prior preview must not leave stale attribution.
    setCell(11,false);*(int*)(first+0xe8)=6;*(int*)(second+0xe8)=-1;
    for(int i=0;i<2;++i)assert(runHazard(damage,unit,hex)==0 && damage[1]==0);
    setCell(0,false);
    assert(runHazard(damage,unit,hex)==0 && damage[1]==0);
    // Vines double one existing fire RNG sample; fire immunity takes priority over vines.
    setCell(0,true);g_skills[249].effects[62]=false;
    for(int mode=0;mode<4;++mode) {
        *(int*)(first+0xe8)=mode==2?-1:mode==1?249:31;
        *(int*)(second+0xe8)=mode>=2?31:-1;calls=g_hazardRandomCalls;
        assert(runHazard(damage,unit,hex)==1 && damage[0]==3 && damage[1]==134);
        assert(g_hazardRandomCalls==calls+1 && !g_terrainProbe);
    }
    g_skills[249].effects[62]=true;
    *(int*)(first+0xe8)=62;*(int*)(second+0xe8)=31;calls=g_hazardRandomCalls;
    assert(runHazard(damage,unit,hex)==1 && damage[1]==0 && calls==g_hazardRandomCalls);
    skilltelemetry::close();
    // M7 relocates HEX20 and uses a non-square stride. Execute the same native
    // calculator at coordinates beyond the original 200x200 grid as well.
    uint32_t oldWidth=*(uint32_t*)site(0x5ae50b);
    assert(skilltelemetry::open(L"build/movement_map_test.csv",battleActor,nullptr));
    for(int layout=0;layout<2;++layout){int width=layout?530:249,height=layout?249:530;
        std::vector<uint8_t> expanded((size_t)width*height*20);
        *(int*)(mapCode+0x13)=height;*(int*)(mapCode+0x1f)=*(int*)(mapCode+0x2a)=width;
        *(uint32_t*)(mapCode+0x3a)=*cellOperand=(uint32_t)expanded.data();*(int*)site(0x5ae50b)=width;
        uint32_t p=(height-2)|((width-2)<<16);auto cell=expanded.data()+20*((size_t)(height-2)*width+width-2);
        for(int effect:{6,8,62,31}){*(int*)(first+0xe8)=effect;*(int*)(second+0xe8)=-1;
            *(uint32_t*)cell=(effect==62 || effect==31)?0x00800000:0;
            *(uint32_t*)(cell+4)=effect==6?11:effect==8?4:0;
            calls=g_hazardRandomCalls;
            assert(runHazard(damage,unit,p)==(effect==62 || effect==31?1:0));
            assert(damage[1]==(effect==31?134:0) && g_hazardRandomCalls==calls+(effect==31));
            assert(movementTerrain(p)==(effect==6?11:effect==8?4:0));
        }
        assert(movementTerrain(0xffffffff)==-1 && movementTerrain((uint32_t)height)==-1 && movementTerrain((uint32_t)width<<16)==-1);
        ++*(int*)(mapCode+0x2a);assert(movementTerrain(p)==-1);--*(int*)(mapCode+0x2a);
    }
    skilltelemetry::close();*(uint32_t*)site(0x5ae50b)=oldWidth;
    puts("PASS M7 movement hazards: native 249x530 and 530x249 grids, relocated cells, four effects and preserved RNG");
    assert(!g_terrainProbe);
    for(int i=0;i<3;++i)g_skills[249].effects[effects[i]]=saved[i];
    g_skills[249].effects[31]=oldVine;
    *cellOperand=oldOperand;g_movementHazardOriginal=oldHazard;g_movementMapOperand=oldMap;
    g_upkeepActor=oldFirst;g_secondIncomeActor=oldSecond;g_battleUnit=oldUnit;
    g_battleFirstId=oldFirstId;g_battleSecondId=oldSecondId;g_telemetryUnitId=oldId;
    puts("PASS native movement: poison/cliff/fire immunity, coordinates, composed/high-ID providers, preview isolation and unchanged RNG");
}
