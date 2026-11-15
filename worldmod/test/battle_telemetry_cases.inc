// Patched native call sites execute the original energy/troop clamp and byte/word setters.
// World lookups, force technology, unit membership and leader capacity are controlled fixtures.
static uintptr_t g_unitEntry,g_restEntry,g_restGrantEntry;
static uint8_t* g_battleUnit;
static bool g_advancedEnergy=false;
static int g_battleLookups=0;
static int g_battleFirstId=600,g_battleSecondId=601;
static uint8_t g_battleForce[300]={};
static int g_battleBoundaryChange=0;
static int g_battleBoundaryOwned=-1;
static bool g_battleInvalidateDiscovery=false;
static int g_battleActorResolutions=0;
static int __fastcall battleBoundaryEnergy(void* unit,void*,int requested) {
    int result=((UnitEnergy)site(0x4964f0))(unit,requested);
    if(g_battleBoundaryChange==1)skillsTelemetryStream(1,0);
    if(g_battleBoundaryChange==2)*(int*)(g_upkeepActor+0xe8)=g_battleBoundaryOwned;
    return result;
}
static int __fastcall battleBoundaryTroops(void* self,void*,void* unit,int requested) {
    int result=((IncomeGrant)site(0x4ae4a0))(self,unit,requested);
    if(g_battleBoundaryChange==1)skillsTelemetryStream(1,0);
    if(g_battleBoundaryChange==2)*(int*)(g_upkeepActor+0xe8)=g_battleBoundaryOwned;
    return result;
}
static void assertBattleAppliedRows(const wchar_t* path,int expected) {
    FILE* file=nullptr;assert(!_wfopen_s(&file,path,L"rb"));
    char line[2048];int count=0;
    while(fgets(line,sizeof line,file))if(strstr(line,",\"applied\","))++count;
    fclose(file);assert(count==expected);
}
__declspec(naked) static uintptr_t __cdecl runUnit1(void* self,int value) {
    __asm {
        mov ecx,[esp+4]
        mov eax,[esp+8]
        push eax
        mov edx,g_unitEntry
        jmp edx
    }
}
__declspec(naked) static int __cdecl runRestQuery(void* unit,int queried) {
    __asm {
        push esi
        sub esp,0x28
        mov esi,[esp+0x30]
        mov eax,[esp+0x34]
        mov ecx,esi
        mov edx,[esi]
        push eax
        mov eax,g_restEntry
        jmp eax
    }
}
__declspec(naked) static int __cdecl runRestGrant(void* unit,int holder) {
    __asm {
        push esi
        sub esp,0x28
        mov esi,[esp+0x30]
        mov eax,[esp+0x34]
        mov [esp+0x18],eax
        mov [esp+0x24],eax
        mov edx,g_restGrantEntry
        jmp edx
    }
}
__declspec(naked) static void restGrantReturned() {
    __asm {
        add esp,0x28
        pop esi
        ret
    }
}
__declspec(naked) static void restGrantRejected() {
    __asm {
        xor eax,eax
        add esp,0x28
        pop esi
        ret
    }
}
__declspec(naked) static void musicFindReturned() {
    __asm {
        mov eax,[esp+0x18]
        add esp,0x28
        pop esi
        ret
    }
}
__declspec(naked) static void poetryFindReturned() {
    __asm {
        mov eax,[esp+0x24]
        add esp,0x28
        pop esi
        ret
    }
}
static uint8_t* __fastcall battlePerson(void*,void*,int id) {
    ++g_battleLookups;
    if(g_battleInvalidateDiscovery){g_battleInvalidateDiscovery=false;skillsTelemetryStream(1,0);}
    return id==g_battleFirstId?g_upkeepActor:id==g_battleSecondId?g_secondIncomeActor:nullptr;
}
static void* __fastcall battleForce(void*,void*,int id) {assert(id==2);return g_battleForce;}
static int __fastcall battleTech(void*,void*,int id) {return id==16 && g_advancedEnergy;}
static uint16_t __fastcall battleCapacity(void*,void*) {return 10000;}
static int __fastcall battleEnergy(uint8_t* unit,void*) {return unit[0x1a];}
static int __fastcall battleTroops(uint8_t* unit,void*) {return *(uint16_t*)(unit+0x18);}
static int __fastcall battleOwner(void*,void*) {return 2;}
static int __fastcall battleUnitId(void*,void*,void* unit) {assert(unit==g_battleUnit);return 17;}
static int __fastcall battleFind(void*,void*,int effect) {
    if(personHas(g_upkeepActor,nullptr,effect))return g_battleFirstId;
    if(personHas(g_secondIncomeActor,nullptr,effect))return g_battleSecondId;
    return -1;
}
static bool battleActor(const void* p,skilltelemetry::Actor& actor) {
    ++g_battleActorResolutions;
    if(g_battleInvalidateDiscovery){g_battleInvalidateDiscovery=false;skillsTelemetryStream(1,0);}
    economyActor(p,actor);actor.person=p==g_upkeepActor?g_battleFirstId:g_battleSecondId;actor.force=2;return true;
}
static void battleTelemetryTests() {
    uint8_t person[400]={},second[400]={},unit[0xf4]={};
    g_upkeepActor=person;g_secondIncomeActor=second;g_battleUnit=unit;
    *(int*)(person+0xe8)=249;*(int*)(second+0xe8)=-1;*(int*)(unit+0xc)=g_battleFirstId;
    int effects[]={11,22,30,75,76};bool saved[5];
    for(int i=0;i<5;++i){saved[i]=g_skills[249].effects[effects[i]];g_skills[249].effects[effects[i]]=true;}
    void* vt[24]={};vt[0x40/4]=battleOwner;vt[0x50/4]=battleTroops;
    vt[0x54/4]=battleEnergy;vt[0x5c/4]=battleFind;*(void**)unit=vt;
    auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryUnitId;
    auto oldEnergy=g_unitEnergyOriginal;auto oldTroops=g_unitTroopsOriginal;
    auto oldMusic=g_musicFindResume;auto oldPoetry=g_poetryFindResume;
    g_telemetryPerson=(PersonById)battlePerson;g_telemetryUnitId=(BuildingId)battleUnitId;
    g_unitEnergyOriginal=(UnitEnergy)site(0x4964f0);g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    domesticStub(0x47a630,domesticValid);domesticStub(0x490b00,battlePerson);
    domesticStub(0x490aa0,battleForce);domesticStub(0x4811e0,battleTech);
    domesticStub(0x48a4f0,battleCapacity);
    for(uint32_t at:{0x584631u,0x58465au,0x584721u,0x5847ceu,0x584737u,0x5847e4u,0x4bec08u,0x59a3c8u,0x59a410u})
        *(uint8_t*)site(at+5)=0xc3;
    g_musicFindResume=(uint32_t)site(0x59a2bb);g_poetryFindResume=(uint32_t)site(0x59a2c8);
    domesticStub(0x59a2bb,musicFindReturned);domesticStub(0x59a2cc,poetryFindReturned);
    domesticStub(0x59a3cd,restGrantReturned);domesticStub(0x59a415,restGrantReturned);
    domesticStub(0x59a489,restGrantRejected);
    g_restEntry=site(0x59a2b4);
    int lookups=g_battleLookups;
    assert(runRestQuery(unit,75)==g_battleFirstId && g_battleLookups==lookups && !g_musicActor.person && !g_poetryActor.person);
    assert(skilltelemetry::open(L"build/battle_telemetry_test.csv",battleActor,nullptr));
    for(bool advanced:{false,true}) {
        g_advancedEnergy=advanced;int cap=advanced?120:100;
        for(int path=0;path<5;++path)for(int before:{10,cap-2,cap}) {
            unit[0x1a]=(uint8_t)before;
            int requested=path==2 || path==4?10:5;
            if(path<2) {
                g_unitEntry=site(path==0?0x584721:0x5847ce);
                assert(runUnit1(nullptr,g_battleFirstId)==(uintptr_t)person);
                g_unitEntry=site(path==0?0x584737:0x5847e4);
            } else if(path==2) {
                // Model the verified native object-query origin used by the defeat branch.
                QueryOrigin origin(0x4bebfc);assert(personHas(person,nullptr,11));
                g_unitEntry=site(0x4bec08);
            } else {
                g_restEntry=site(0x59a2b4);assert(runRestQuery(unit,75)==g_battleFirstId);
                g_restEntry=site(0x59a2c1);assert(runRestQuery(unit,76)==g_battleFirstId);
                assert(g_musicActor.person==person && g_poetryActor.person==person);
                g_restGrantEntry=site(path==3?0x59a3f9:0x59a3ad);
            }
            int actual=path<3?(int)runUnit1(unit,requested):runRestGrant(unit,g_battleFirstId);
            assert(actual==(before==10?requested:cap-before) && unit[0x1a]==before+actual);
        }
    }
    for(int before:{1000,9990,10000}) {
        *(uint16_t*)(unit+0x18)=(uint16_t)before;
        g_unitEntry=site(0x584631);assert(runUnit1(nullptr,g_battleFirstId)==(uintptr_t)person);
        g_incomeBonusEntry=site(0x58465a);
        assert(runIncomeBonus(unit,100)==(before==1000?100:10000-before));
        assert(*(uint16_t*)(unit+0x18)==(before==1000?1100:10000) && !g_heartActor.person);
    }
    // Preserve the original rest-only holder bound while still attributing expanded battle members.
    g_battleFirstId=1500;unit[0x1a]=10;
    g_restEntry=site(0x59a2b4);assert(runRestQuery(unit,75)==1500);
    g_restEntry=site(0x59a2c1);assert(runRestQuery(unit,76)==1500);
    g_restGrantEntry=site(0x59a3f9);assert(!runRestGrant(unit,1500) && unit[0x1a]==10);
    g_restGrantEntry=site(0x59a3ad);assert(!runRestGrant(unit,1500) && unit[0x1a]==10);
    g_unitEntry=site(0x584721);assert(runUnit1(nullptr,1500)==(uintptr_t)person);
    g_unitEntry=site(0x584737);assert(runUnit1(unit,5)==5 && unit[0x1a]==15);
    g_battleFirstId=600;
    // Calls without a fresh matching source preserve the native grant and cannot borrow old attribution.
    unit[0x1a]=10;g_unitEntry=site(0x584737);assert(runUnit1(unit,5)==5 && !g_rageActor.person);
    g_unitEntry=site(0x584721);assert(!runUnit1(nullptr,-1));
    g_unitEntry=site(0x584737);assert(runUnit1(unit,5)==5);
    assert(personHas(person,nullptr,11) && !g_elationActor.person); // unrelated predicate origin
    g_unitEntry=site(0x4bec08);assert(runUnit1(unit,10)==10);
    *(int*)(person+0xe8)=-1;g_poetryActor=recoveryActor(person);
    g_restEntry=site(0x59a2b4);assert(runRestQuery(unit,75)==-1 && !g_musicActor.person && !g_poetryActor.person);
    g_restGrantEntry=site(0x59a3f9);assert(runRestGrant(unit,600)==5);
    skilltelemetry::close();
    auto prepareRecovery=[&](int path) {
        *(int*)(person+0xe8)=249;unit[0x1a]=10;*(uint16_t*)(unit+0x18)=1000;
        if(path==0 || path==2) {
            g_unitEntry=site(path==0?0x584721:0x584631);
            assert(runUnit1(nullptr,600)==(uintptr_t)person);
        } else if(path==1) {
            QueryOrigin origin(0x4bebfc);assert(personHas(person,nullptr,11));
        } else {
            g_restEntry=site(path==3?0x59a2b4:0x59a2c1);assert(runRestQuery(unit,path==3?75:76)==600);
        }
    };
    auto commitRecovery=[&](int path) {
        if(path==2) {
            g_incomeBonusEntry=site(0x58465a);assert(runIncomeBonus(unit,100)==100);
            assert(*(uint16_t*)(unit+0x18)==1100);
        } else if(path<2) {
            g_unitEntry=site(path==0?0x584737:0x4bec08);
            int amount=path==0?5:10;assert((int)runUnit1(unit,amount)==amount && unit[0x1a]==10+amount);
        } else {
            g_restGrantEntry=site(path==3?0x59a3f9:0x59a3ad);
            int amount=path==3?5:10;assert(runRestGrant(unit,600)==amount && unit[0x1a]==10+amount);
        }
    };
    g_advancedEnergy=false;
    assert(skilltelemetry::open(L"build/battle_session_source_test.csv",battleActor,nullptr));
    for(int path=0;path<5;++path)prepareRecovery(path);
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/battle_session_after_test.csv",battleActor,nullptr));
    for(int path=0;path<5;++path) {
        unit[0x1a]=10;*(uint16_t*)(unit+0x18)=1000;
        int resolved=g_battleActorResolutions;commitRecovery(path);assert(g_battleActorResolutions==resolved);
    }
    for(int path=0;path<5;++path){prepareRecovery(path);int resolved=g_battleActorResolutions;commitRecovery(path);assert(g_battleActorResolutions==resolved+1);}
    skilltelemetry::close();
    assertBattleAppliedRows(L"build/battle_session_after_test.csv",5);
    assert(skilltelemetry::open(L"build/battle_context_test.csv",battleActor,nullptr));
    int recoveryEffects[]={30,11,22,75,76};
    for(int path=0;path<5;++path)for(int mutation=0;mutation<7;++mutation) {
        g_battleInvalidateDiscovery=mutation==6;
        prepareRecovery(path);
        assert(!g_battleInvalidateDiscovery);
        if(mutation==1)skillsTelemetryStream(1,0);
        if(mutation==2)*(int*)(person+0xe8)=recoveryEffects[path]; // still has the effect, but the captured owned skill changed
        if(mutation==3 || mutation==4) {
            g_battleBoundaryChange=mutation==3?1:2;
            g_battleBoundaryOwned=recoveryEffects[path];
            g_unitEnergyOriginal=(UnitEnergy)battleBoundaryEnergy;g_unitTroopsOriginal=(IncomeGrant)battleBoundaryTroops;
        }
        if(mutation==5)*(int*)(person+0xe8)=-1;
        int resolved=g_battleActorResolutions;commitRecovery(path);
        assert(g_battleActorResolutions==resolved+(mutation==0?1:0));
        g_unitEnergyOriginal=(UnitEnergy)site(0x4964f0);g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
        g_battleBoundaryChange=0;
    }
    skilltelemetry::close();assertBattleAppliedRows(L"build/battle_context_test.csv",5);
    puts("PASS recovery provenance: five reopened-session exclusions,35 native grants across generation/owned-skill/discovery boundaries and ten fresh controls");
    for(int i=0;i<5;++i)g_skills[249].effects[effects[i]]=saved[i];
    g_telemetryPerson=oldPerson;g_telemetryUnitId=oldId;g_unitEnergyOriginal=oldEnergy;g_unitTroopsOriginal=oldTroops;
    g_musicFindResume=oldMusic;g_poetryFindResume=oldPoetry;
    puts("PASS battle telemetry: five unit effects, two rage branches, native energy 100/120 caps, native troop clamp/stores, rest-query bridges and stale-source rejection");
}
