static uintptr_t g_strategyFireDurationEntry;
static bool g_fireInvalidate=false,g_fireMutate=false,g_fireCancel=false,g_fireChangeSkill=false,g_fireMoveTarget=false;
static int g_fireRepeats=1;
static int g_fireLastCritical=0;
typedef void(__thiscall* FireConsume)(void*,const uint32_t*);
static DWORD WINAPI strategyFireFixtureWorker(void* world){
    auto packet=*(const uint32_t**)((uint8_t*)world+0x48);
    for(int n=0;n<g_fireRepeats;++n)((FireConsume)site(0x592440))(world,packet);
    return 0;
}
static void __fastcall strategyFireFixtureDispatch(uint8_t* world,void*){
    assert(*(int*)(world+0x4c)==0x16);
    auto packet=*(uint32_t**)(world+0x48);
    if(g_fireInvalidate)skillsTelemetryStream(1,0);
    if(g_fireMutate)++packet[2];
    if(g_fireCancel)return;
    HANDLE thread=CreateThread(nullptr,0,strategyFireFixtureWorker,world,0,nullptr);assert(thread);
    assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);DWORD result=1;
    assert(GetExitCodeThread(thread,&result) && result==0);CloseHandle(thread);
}
static void __fastcall strategyFireFixtureNoop(void*,void*){}
static void* __fastcall strategyFireFixtureUnit(void*,void*,int id){return id==4095?g_afterSource:id==4094?g_afterTarget:nullptr;}
static void* __fastcall strategyFireFixtureObject(void*,void*){return g_afterTarget;}
static int __cdecl strategyFireFixtureValid(void* person){
    // Isolate the later immediate-damage phase after the real map write.
    if((uintptr_t)_ReturnAddress()==site(0x5924d9))return 0;
    return person!=nullptr;
}
static int __cdecl strategyFireFixtureNotExplosive(void*){return 0;}
static void __cdecl strategyFireFixtureRefresh(uint32_t){}
static DWORD(WINAPI* g_strategyFireThreadId)()=GetCurrentThreadId;
__declspec(naked) static void __cdecl runStrategyFireDuration(void* world,void* source,void* command,int critical){__asm {push ebp
    push esi
    mov eax,[esp+12]
    mov esi,[esp+16]
    mov ebp,[esp+20]
    mov edx,[esp+24]
    sub esp,0x420
    mov [esp+0x1c],eax
    mov [esp+0x400],edx
    jmp dword ptr [g_strategyFireDurationEntry]}}
__declspec(naked) static void strategyFireFixtureDurationDone(){__asm {add esp,0x420
    pop esi
    pop ebp
    ret}}
static int __fastcall strategyFireFixtureLeaf(void* world,void*,void* command,int success,int critical,int,void*,int){
    g_fireLastCritical=critical;
    if(g_fireChangeSkill)*(int*)(g_personGuide+0xe8)=-1;
    if(g_fireMoveTarget)*(uint32_t*)(g_afterTarget+4)=0x10003;
    if(success)runStrategyFireDuration(world,strategyFireFixtureUnit(nullptr,nullptr,((uint32_t*)command)[1]),command,critical);
    return 1;
}
#include "strategy_followup_telemetry_cases.inc"
#include "strategy_charm_telemetry_cases.inc"
#include "strategy_status_telemetry_cases.inc"
static void strategyFireTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,uint8_t* person,uint8_t* spouse,uint8_t* tiles){
    auto oldSkill=g_skills[249];auto oldCommand=g_strategyCommandResume;
    auto oldCritical=g_strategyCriticalOriginal,oldCriticalChance=g_strategyCriticalChanceOriginal;
    auto oldCriticalMaximum=g_strategyCriticalMaximumOriginal;auto oldMax=g_criticalMaximumResume;auto oldAttribute=g_criticalAttributeOriginal;
    auto oldGod=g_strategyCriticalGodResume,oldDeep=g_strategyDeepResume,oldAmbush=g_strategyAmbushResume;
    auto oldPacket=g_strategyFirePacketResume,oldDispatch=g_strategyFireDispatchCheck;
    auto oldFire=g_strategyFireApplyOriginal;auto oldTurns=g_strategyFireTurnsOriginal;auto oldMap=g_strategyFireMapOperand;
    auto oldConsumerResume=g_trapFireResume;g_trapFireResume=(uint32_t)site(0x592447);
    auto oldCost=g_strategyCostOriginal;auto oldEnergy=g_unitEnergyOriginal;
    g_strategyCommandResume=(uint32_t)site(0x5933a6);g_strategyCriticalOriginal=(StrategyCritical)site(0x5af820);
    g_strategyCriticalChanceOriginal=(StrategyCritical)site(0x5af520);g_strategyCriticalMaximumOriginal=(CriticalMaximum)site(0x495680);
    g_criticalMaximumResume=(uint32_t)site(0x495687);g_criticalAttributeOriginal=(CriticalAttribute)site(0x489030);
    g_strategyCriticalGodResume=(uint32_t)site(0x5af5f8);g_strategyDeepResume=(uint32_t)site(0x5af605);g_strategyAmbushResume=(uint32_t)site(0x5af71f);
    g_strategyFirePacketResume=(uint32_t)site(0x58880d);g_strategyFireDispatchCheck=(uint32_t)site(0x57dd80);
    g_strategyFireApplyOriginal=(StrategyFireApply)site(0x5887e0);g_strategyFireTurnsOriginal=(FireTurns)site(0x5a0770);
    g_strategyFireMapOperand=(const uint32_t*)site(0x5a07aa);g_strategyFireDurationEntry=site(0x591409);
    g_strategyCostOriginal=(StrategyCost)site(0x495170);g_unitEnergyOriginal=(UnitEnergy)site(0x4964f0);
    // Earlier fixtures terminated at these points; restore actual command flow.
    memcpy((void*)site(0x59340c),"\x33\xdb\x85\xed\x74\x1c",6);
    memcpy((void*)site(0x59342e),"\x8b\x44\x24\x24\x85\xc0\x75\x18",8);
    memcpy((void*)site(0x59344a),"\x8b\x44\x24\x24\x8b\x0e",6);
    domesticStub(0x591270,strategyFireFixtureLeaf);domesticStub(0x591438,strategyFireFixtureDurationDone);
    domesticStub(0x483b40,strategyFireFixtureObject);domesticStub(0x490e70,strategyFireFixtureUnit);
    domesticStub(0x47a630,strategyFireFixtureValid);domesticStub(0x5afbe0,strategyFireFixtureNotExplosive);
    domesticStub(0x59fea0,strategyFireFixtureRefresh);domesticStub(0x57e5d0,strategyFireFixtureNoop);
    domesticStub(0x57dbc0,strategyFireFixtureNoop);domesticStub(0x57dbd0,strategyFireFixtureNoop);
    domesticStub(0x495680,criticalMaximum);domesticStub(0x489030,strategyFixtureIntelligence);domesticStub(0x490b00,personFixtureLookup);
    domesticStub(0x490aa0,strategyFixtureForce);domesticStub(0x4811e0,strategyFixtureTech);g_strategyFixtureAdvanced=false;
    *(uint32_t*)site(0x57dd85)=(uint32_t)&g_strategyFireThreadId;
    uint32_t dispatches[9];for(int n=0;n<9;++n)dispatches[n]=(uint32_t)site(*(uint32_t*)site(0x5935fc+4*n));
    *(uint32_t*)site(0x59345c)=(uint32_t)dispatches;
    uint32_t criticalJumps[9];for(int n=0;n<9;++n)criticalJumps[n]=(uint32_t)site(*(uint32_t*)site(0x5af7e8+4*n));
    *(uint32_t*)site(0x5af63f)=(uint32_t)criticalJumps;
    *(uint32_t*)site(0x592486)=(uint32_t)tiles;*(uint32_t*)site(0x5a07aa)=(uint32_t)tiles;
    int terrainChance[32];for(auto& v:terrainChance)v=80;*(uint32_t*)site(0x5aea88)=(uint32_t)terrainChance;
    uint8_t costs[9]={10,10,15,15,10,10,20,50,50};*(uint32_t*)site(0x49518a)=(uint32_t)costs;
    const uint8_t sampler[]={0x8b,0x4c,0x24,0x04,0x83,0xf9,0x02,0x7c,0x27,0x66,0x85,0xc9,0x74,0x22,0xa1,0x44,0x5d,0x8a,0x00,0x69,0xc0,0x65,0x89,0x07,0x6c,0x05,0x39,0x30,0x00,0x00,0xa3,0x44,0x5d,0x8a,0x00,0xc1,0xe8,0x10,0x0f,0xb7,0xc9,0x99,0xf7,0xf9,0x66,0x8b,0xc2,0xc3,0x66,0x33,0xc0,0xc3};
    memcpy((void*)site(0x472150),sampler,sizeof sampler);*(uint32_t*)site(0x47215f)=(uint32_t)&g_strategySeed;
    *(uint32_t*)site(0x47216f)=(uint32_t)&g_strategySeed;
    uint8_t world[0x600]={};void* vt[5]={};vt[4]=strategyFireFixtureDispatch;*(void**)world=vt;
    (*(void***)source)[0x54/4]=battleEnergy;
    auto cell=(uint32_t*)(tiles+20*401);int command[3]={0,4095,0x10002};
    auto setup=[&](int effect,int provider,int before,int seed,bool dispatch){
        memset(g_skills[249].effects,0,sizeof g_skills[249].effects);g_skills[249].effects[effect]=true;
        for(auto p:{actor,other,person,spouse}){*(int*)(p+0xe8)=-1;p[0xc9]=110;}
        auto selected=(provider&1)?other:actor,helper=(provider&1)?actor:other;
        *(int*)(selected+0xe8)=provider<2?effect:249;*(int*)(helper+0xe8)=53;selected[0xc9]=effect==59?40:110;
        source[0x1a]=100;source[0xc6]=100;target[0xc6]=70;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        *cell=0x50000001u|((uint32_t)before<<23);cell[1]=0;cell[2]=4094;
        *(uint32_t*)(target+4)=command[2];g_strategySeed=seed;*(DWORD*)(world+0x5e0)=GetCurrentThreadId()+(dispatch?0:1);
        g_fireInvalidate=g_fireMutate=g_fireCancel=g_fireChangeSkill=g_fireMoveTarget=false;g_fireRepeats=1;
        g_afterSourceRegistered=g_afterTargetRegistered=g_afterActorRegistered=true;
    };
    auto apply=[&](int mode){int result=((StrategyCommand)site(0x5933a0))(world,(uint32_t*)command,mode,nullptr);
        assert(result==1 && !g_strategyCommand && !g_strategyCritical && !g_strategyFireInvocation && g_strategyFirePending.empty());return result;};
    assert(skilltelemetry::open(L"build/strategy_fire_test.csv",strategyMaximumActor,nullptr));
    int seeds[]={0,4,1},beforeValues[]={0,4,7};
    for(int effect:{63,67,58,59})for(int provider=0;provider<4;++provider)for(int mode:{0,1,2})for(bool dispatch:{false,true})for(int sample=0;sample<3;++sample){
        setup(effect,provider,beforeValues[sample],seeds[sample],dispatch);apply(mode);
        uint32_t expected=seeds[sample];for(int n=0;n<3;++n)expected=expected*0x6c078965u+0x3039u;
        assert(g_strategySeed==expected && ((*cell>>23)&7)==(unsigned)(sample+2) && (*cell&~0x03800000u)==0x50000001u);
        assert(source[0x1a]==(mode==0?90:100));
    }
    // Real decisions can remain valid even when later provenance expires.
    // These still execute the original map write, without a fire-effect row.
    for(int reason=0;reason<8;++reason){
        setup(63,0,7,0,true);
        if(reason==0)g_fireInvalidate=true;
        if(reason==1)g_fireMutate=true;
        if(reason==2)g_fireCancel=true;
        if(reason==3)g_fireChangeSkill=true;
        if(reason==4)g_afterSourceRegistered=false;
        if(reason==5)g_afterTargetRegistered=false;
        if(reason==6)g_afterActorRegistered=false;
        if(reason==7)g_fireMoveTarget=true;
        apply(1);
        assert(((*cell>>23)&7)==(unsigned)(reason==1?3:reason==2?7:2));
    }
    setup(63,0,7,0,true);g_fireRepeats=2;apply(1);assert(((*cell>>23)&7)==2); // two genuine writes, second delta0
    setup(63,0,7,0,false); // same real fire helper outside a command must not reuse expired provenance
    strategyFireApply(world,nullptr,0x10002,3,source,1);assert(((*cell>>23)&7)==3 && g_strategyFirePending.empty());
    // A genuine natural critical has no forced-skill provenance to transfer.
    setup(63,0,7,9,false);*(int*)(actor+0xe8)=-1;apply(1);assert(g_fireLastCritical==1 && ((*cell>>23)&7)==2);
    // M7-style non-square expanded map: source/consumer/native setter all use
    // the installed B20/W/H immediates, with a position beyond stock200 bounds.
    std::vector<uint8_t> expanded(20*249*530);command[2]=(230u<<16)|250;
    cell=(uint32_t*)(expanded.data()+20*(250*249+230));
    for(uint32_t at:{0x5ae9c9u,0x5ae9d0u,0x5af5a6u,0x5af5adu,0x592486u,0x5a07aau})*(uint32_t*)site(at)=(uint32_t)expanded.data();
    for(uint32_t at:{0x5ae9bdu,0x5af59au,0x5933cfu,0x59246eu,0x5a079au,0x5a078fu,0x5af55du,0x5af58eu})*(int*)site(at)=249;
    for(uint32_t at:{0x5a0783u,0x5af549u,0x5af57au})*(int*)site(at)=530;
    for(int mode:{0,1,2})for(bool dispatch:{false,true}){
        setup(63,0,7,1,dispatch);apply(mode);assert(((*cell>>23)&7)==4 && source[0x1a]==(mode==0?90:100));
    }
    skilltelemetry::close();
    setup(63,0,7,0,true);apply(1);assert(((*cell>>23)&7)==2 && g_strategyFirePending.empty());
    strategyFollowupTelemetryTests(world,source,target,actor,other,person,spouse,expanded.data(),249);
    strategyCharmTelemetryTests(source,target,actor,other,person,spouse,expanded.data(),249);
    strategyStatusTelemetryTests(setup,world,source,target,actor,other,command);
    g_skills[249]=oldSkill;g_strategyCommandResume=oldCommand;g_strategyCriticalOriginal=oldCritical;g_strategyCriticalChanceOriginal=oldCriticalChance;
    g_strategyCriticalMaximumOriginal=oldCriticalMaximum;g_criticalMaximumResume=oldMax;g_criticalAttributeOriginal=oldAttribute;
    g_strategyCriticalGodResume=oldGod;g_strategyDeepResume=oldDeep;g_strategyAmbushResume=oldAmbush;
    g_strategyFirePacketResume=oldPacket;g_strategyFireDispatchCheck=oldDispatch;g_strategyFireApplyOriginal=oldFire;
    g_strategyFireTurnsOriginal=oldTurns;g_strategyFireMapOperand=oldMap;g_strategyCostOriginal=oldCost;g_unitEnergyOriginal=oldEnergy;
    g_trapFireResume=oldConsumerResume;
    puts("PASS native fire: whole command decision/critical/cost dispatch, original duration arithmetic and RNG, native packet/consumer/tile write, direct and other-thread commit, source lifetime cleanup");
}
