static uintptr_t g_strategyOwnerGrantEntry=0;
static uint8_t* g_strategyOwnerProvider=nullptr;
static int g_strategyOwnerFault=0,g_strategyOwnerStage=0,g_strategyOwnerForce=2;
static bool g_strategyOwnerForeign=false,g_strategyOwnerInterpose=false;
static void strategyOwnerMutate(){
    switch(g_strategyOwnerFault){
    case 1:skilltelemetry::invalidateContext();break;
    case 2:*(int*)(g_strategyOwnerProvider+0xe8)=-1;break;
    case 3:g_strategyFixtureActorRegistered=false;break;
    case 4:g_strategyFixtureUnitRegistered=false;break;
    case 5:case 6:case 7:case 8:++*(int*)(g_strategyOwnerProvider+0x98+4*(g_strategyOwnerFault-5));break;
    case 9:++*(uint32_t*)(g_strategyFixtureUnit+4);break;
    case 10:++*(int*)(g_strategyFixtureUnit+0xa8);break;
    case 11:++*(int*)(g_strategyFixtureUnit+8);break;
    case 12:*(int*)(g_strategyFixtureUnit+0xc)=-1;break;
    case 13:*(int*)(g_strategyFixtureUnit+0x10)=-1;break;
    case 14:g_strategyOwnerForce=3;break;
    case 15:g_skills[249].effects[64]=false;break;
    case 16:++g_strategyFixtureUnit[0x1a];break;
    }
}
static int __fastcall strategyOwnerCost(void* unit,void*,int plan){
    int result=((StrategyCost)site(0x495170))(unit,plan);if(!g_strategyOwnerStage)strategyOwnerMutate();return result;
}
static int __fastcall strategyOwnerGrant(void* unit,void*,int requested){
    int result=((UnitEnergy)site(0x4964f0))(unit,requested);if(g_strategyOwnerStage)strategyOwnerMutate();return result;
}
static int __fastcall strategyOwnerUnitForce(void*,void*){return g_strategyOwnerForce;}
static void* __fastcall strategyOwnerForceLookup(void*,void*,int id){assert(id==2 || id==3);return g_strategyFixtureForce;}
__declspec(naked) static int __cdecl strategyOwnerForeignGrant(void*){__asm {sub esp,0x40
    push edi
    mov edi,[esp+0x48]
    push -1
    mov ecx,edi
    jmp dword ptr [g_strategyOwnerGrantEntry]}}
static void strategyOwnerBetween(){
    if(g_strategyOwnerInterpose){g_strategyOwnerForeign=true;
        assert(strategyOwnerForeignGrant(g_strategyFixtureUnit)==-1);g_strategyOwnerForeign=false;}
}
__declspec(naked) static void strategyOwnerBetweenBridge(){__asm {pushfd
    pushad
    call strategyOwnerBetween
    popad
    popfd
    neg eax
    push eax
    mov ecx,edi
    jmp dword ptr [g_strategyOwnerGrantEntry]}}
__declspec(naked) static void strategyOwnerCommitted(){__asm {pop edi
    cmp g_strategyOwnerForeign,0
    je done
    add esp,0x40
done:
    ret}}
static void strategyOwnerTelemetryTests(uint8_t* unit,uint8_t* actor,uint8_t* other,uint8_t* costs){
    auto oldCost=g_strategyCostOriginal;auto oldEnergy=g_unitEnergyOriginal;
    auto forceSlot=(void**)(*(uint8_t**)unit+0x40);void* oldForce=*forceSlot;*forceSlot=strategyOwnerUnitForce;
    uint32_t sites[]={0x593440,0x59344a,0x490aa0};uint8_t saved[3][5];
    for(int n=0;n<3;++n)memcpy(saved[n],(void*)site(sites[n]),5);
    domesticStub(0x593440,strategyOwnerBetweenBridge);domesticStub(0x59344a,strategyOwnerCommitted);domesticStub(0x490aa0,strategyOwnerForceLookup);
    g_strategyOwnerGrantEntry=site(0x593445);g_strategyCostOriginal=(StrategyCost)strategyOwnerCost;g_unitEnergyOriginal=(UnitEnergy)strategyOwnerGrant;
    auto setup=[&](int provider,int advanced,int before){
        memset(actor+0x98,0,16);memset(other+0x98,0,16);
        *(int*)(actor+0xe8)=provider==0?64:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==1?64:provider==3?249:-1;
        g_strategyOwnerProvider=provider==1 || provider==3?other:actor;
        *(int*)(unit+0xc)=600;*(int*)(unit+0x10)=14162;*(int*)(unit+0x14)=-1;
        *(int*)(unit+8)=0;*(int*)(unit+0xa8)=4;*(uint32_t*)(unit+4)=0x10001;unit[0x1a]=(uint8_t)before;
        g_skills[249].effects[64]=true;g_strategyFixtureAdvanced=advanced!=0;
        g_strategyFixtureActorRegistered=g_strategyFixtureUnitRegistered=true;
        g_strategyOwnerFault=g_strategyOwnerStage=0;g_strategyOwnerForce=2;g_strategyOwnerForeign=g_strategyOwnerInterpose=false;
        g_strategyCostPending=StrategyCostPending{};
    };
    assert(skilltelemetry::open(L"build/strategy_owner_test.csv",personFixtureActor,nullptr));
    for(int provider=0;provider<4;++provider)for(int advanced:{0,1})for(int before:{10,120,255}){
        setup(provider,advanced,before);int after=before-1;if(after>(advanced?120:100))after=advanced?120:100;
        assert(runStrategyCost(unit,0)==after-before && unit[0x1a]==after);
    }
    for(int provider=0;provider<4;++provider){setup(provider,0,10);g_strategyOwnerInterpose=true;
        assert(runStrategyCost(unit,0)==-1 && unit[0x1a]==8 && !g_strategyCostPending.actor);}
    for(int owned:{64,249})for(int stage:{0,1})for(int fault=1;fault<=15;++fault){
        if(fault==15 && owned==64)continue;
        setup(owned==64?0:2,0,10);g_strategyOwnerStage=stage;g_strategyOwnerFault=fault;
        assert(runStrategyCost(unit,0)==-1 && unit[0x1a]==9 && !g_strategyCostPending.actor);
    }
    for(int owned:{64,249}){setup(owned==64?0:2,0,10);g_strategyOwnerStage=1;g_strategyOwnerFault=16;
        assert(runStrategyCost(unit,0)==-1 && unit[0x1a]==10 && !g_strategyCostPending.actor);}
    for(int provider=0;provider<4;++provider){setup(provider,0,10);
        assert(strategyCost(unit,nullptr,0)==1 && strategyCostGrant(unit,nullptr,-1)==-1 && unit[0x1a]==9);}
    for(int advanced:{0,1}){setup(4,advanced,10);costs[0]=1;
        assert(runStrategyCost(unit,0)==-1 && unit[0x1a]==9);costs[0]=10;}
    skilltelemetry::close();
    for(int provider=0;provider<4;++provider){setup(provider,0,10);assert(runStrategyCost(unit,0)==-1 && unit[0x1a]==9);}
    for(int n=0;n<3;++n)memcpy((void*)site(sites[n]),saved[n],5);
    *forceSlot=oldForce;g_strategyCostOriginal=oldCost;g_unitEnergyOriginal=oldEnergy;
    g_strategyOwnerForeign=g_strategyOwnerInterpose=false;
    puts("PASS strategy64 ownership:94 native pair/control inputs; four providers, before/after-cost identity snapshots, same-unit foreign-frame debit, unowned previews, ordinary1 and four disabled controls");
}
