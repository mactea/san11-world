// Complete native70/71 command frames,70 gate/status path and71 constructors
// execute. World/UI primitives and record-apply outer body are adapters; real
// shared resource CALLs/clamps/setters execute within the native command owner.
static uint8_t* g_magicFixtureSecond;
static bool g_magicFixtureBase=false,g_magicFixtureFootprint=false;
static int g_magicFixtureMutation=0,g_magicFixtureApplyCalls=0,g_magicFixtureStatusCalls=0;
static uint8_t g_magicFixtureSavedPacket[0x80];
static void* __fastcall magicFixtureUnit(void*,void*,int id){
    return id==4093 && g_criticalTargetRegistered?g_magicFixtureSecond:criticalFixtureUnit(nullptr,nullptr,id);
}
static int __fastcall magicFixtureUnitId(void*,void*,void* unit){return unit==g_magicFixtureSecond?4093:criticalFixtureUnitId(nullptr,nullptr,unit);}
static int __fastcall magicFixtureNeighbors(void*,void*,uint32_t* positions,int radius,uint32_t){
    assert(radius==1);positions[0]=0x20002;positions[1]=g_magicFixtureFootprint?0x30002:0x10001;
    if(g_magicFixtureFootprint){positions[2]=0x10001;return 3;}return 2;
}
static int __fastcall magicFixtureEnemy(void* unit,void*,void* other){return criticalFixtureOwner(unit,nullptr)!=criticalFixtureOwner(other,nullptr);}
static void* __fastcall magicFixtureMapObject(void* cell,void*){
    if(cell==g_criticalCells+20*201)return g_criticalSource;
    if(cell==g_criticalCells+20*401)return g_criticalTarget;
    if(g_magicFixtureFootprint && (cell==g_criticalCells+20*402 || cell==g_criticalCells+20*403))return g_criticalTarget;
    if(cell==g_criticalCells+20*402)return g_magicFixtureSecond;
    return nullptr;
}
static void* __fastcall magicFixtureLeader(uint8_t* unit,void*){return personFixtureLookup(nullptr,nullptr,*(int*)(unit+0xc));}
static const void* __cdecl magicFixtureCommandName(const uint32_t*){return g_personTarget;}
static void __fastcall magicFixtureTextInit(void*,void*,int){}
static void __fastcall magicFixtureTextAppend(void*,void*,int,const void*){}
static const char* __cdecl magicFixtureText(const void*,const void*,int){return "fixture";}
static void __cdecl magicFixtureCommandEvent(void*,void*,uint32_t,int,int){}
static int __cdecl magicFixtureVisible(uint32_t){return 1;}
static int __cdecl magicFixtureSoundRandom(int){return 0;}
static int __fastcall magicFixtureComplete(void*,void*,void*,const void*,int,int,int){return 0;}
static int __cdecl magicFixtureNotExplosive(void*){return 0;}
static void __fastcall magicFixtureVoid(void*,void*){}
static void __fastcall magicFixturePacket(void*,void*,const void*){}
static void __cdecl magicFixtureClock(int){}
static void magicFixtureMutate(){
    if(g_magicFixtureMutation==1)*(int*)(g_personGuide+0xe8)=-1;
    if(g_magicFixtureMutation==2)g_criticalSourceRegistered=false;
    if(g_magicFixtureMutation==3)g_criticalActorRegistered=false;
    if(g_magicFixtureMutation==4)skillsTelemetryStream(1,0);
    if(g_magicFixtureMutation==5)*(int*)(g_criticalSource+0xc)=-1;
}
static void __fastcall magicFixtureStatus(void* self,void*,uint8_t* unit,void* source,int status,int turns,int display){
    ++g_magicFixtureStatusCalls;magicFixtureMutate();
    if(g_magicFixtureMutation!=6)((StatusGrant)site(0x4aea70))(self,unit,source,status,turns,display);
    if(g_magicFixtureMutation==7)skillsTelemetryStream(1,0);
}
static int __cdecl magicFixtureApply(uint8_t* packet){
    ++g_magicFixtureApplyCalls;magicFixtureMutate();
    auto target=*(uint32_t*)(packet+0x5c)==0x10002?g_criticalTarget:g_magicFixtureSecond;
    memcpy(g_magicFixtureSavedPacket,packet,0x80);
    uint8_t copy[0x80];if(g_magicFixtureMutation==8){memcpy(copy,packet,0x80);packet=copy;}
    if(g_magicFixtureMutation==9)++packet[0xc];
    if(g_magicFixtureMutation!=6){
        g_criticalQueuedHp=false;g_criticalQueuedEntry=site(g_magicFixtureBase && target==g_criticalTarget?0x5b1a74:0x5b1c6c);
        runCriticalQueued(target,g_criticalSource,packet);
        if(g_magicFixtureBase && target==g_criticalTarget){g_criticalQueuedHp=true;g_criticalQueuedEntry=site(0x5b1aaf);runCriticalQueued(target,g_criticalSource,packet);}
    }
    return 1;
}
#include "strategy_ambush_telemetry_cases.inc"
static void magicTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},second[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_criticalSource=source;g_criticalTarget=target;g_magicFixtureSecond=second;
    g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x2c/4]=criticalFixtureObject;vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=criticalFixtureOwner;
    vt[0x50/4]=criticalFixtureTargetTroops;vt[0x54/4]=battleEnergy;vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=criticalFixtureFind;
    for(auto unit:{source,target,second}){*(void**)unit=vt;*(int*)(unit+0x10)=*(int*)(unit+0x14)=-1;*(int*)(unit+0xa8)=2;}
    *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(target+0xc)=1000;*(int*)(second+0xc)=601;
    *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;*(uint32_t*)(second+4)=0x20002;*(int*)(source+0xa8)=4;
    auto oldSkill=g_skills[249];for(int i=0;i<LIMIT;++i)g_skills[249].effects[i]=false;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldUnit=g_movementUnitById;auto oldUnitId=g_telemetryUnitId;
    auto oldValid=g_growthValid;auto oldTroops=g_unitTroopsOriginal;auto oldStatus=g_magicStatusOriginal;auto oldTurns=g_statusTurnsOriginal;
    auto oldConfuse=g_magicConfuseResume,oldThunder=g_magicThunderResume,oldFind=g_magicConfuseFindResume,oldThunderFind=g_magicThunderFindResume,oldAdjust=g_magicThunderAdjustedResume;
    auto oldDecision=g_magicDecisionEntry;auto oldBuild=g_magicThunderBuildOriginal;
    auto oldQueued=g_criticalQueuedTroopsResume,oldBaseTroopsResume=g_criticalBaseTroopsResume,oldHpResume=g_criticalBaseHpResume;
    auto oldBaseId=g_telemetryBaseId;auto oldBase=g_baseById;auto oldBaseTroops=g_criticalBaseTroopsOriginal,oldHp=g_criticalBaseHpOriginal;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_movementUnitById=(MovementUnitById)magicFixtureUnit;g_telemetryUnitId=(BuildingId)magicFixtureUnitId;g_growthValid=domesticValid;
    g_magicConfuseResume=(uint32_t)site(0x592057);g_magicThunderResume=(uint32_t)site(0x592ed6);
    domesticStub(0x592050,magicConfuse);domesticStub(0x592ed0,magicThunder); // undo followup leaf adapters
    g_magicConfuseFindResume=(uint32_t)site(0x5920ca);g_magicThunderFindResume=(uint32_t)site(0x592f24);g_magicThunderAdjustedResume=(uint32_t)site(0x593202);
    g_magicDecisionEntry=site(0x591fd0);g_magicStatusOriginal=(StatusGrant)magicFixtureStatus;g_statusTurnsOriginal=(StatusTurnsSet)site(0x496350);
    g_magicThunderBuildOriginal=(MagicBuild)site(0x5b1330);g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);g_criticalBaseTroopsResume=(uint32_t)site(0x5b1a79);g_criticalBaseHpResume=(uint32_t)site(0x5b1ab4);
    g_criticalBaseTroopsOriginal=(IncomeGrant)site(0x4ae3c0);g_criticalBaseHpOriginal=(IncomeGrant)site(0x4ae360);
    g_telemetryBaseId=(BuildingId)criticalFixtureBaseId;g_baseById=(BaseById)criticalFixtureBase;
    domesticStub(0x47a630,domesticValid);domesticStub(0x490e70,magicFixtureUnit);domesticStub(0x490b00,personFixtureLookup);
    domesticStub(0x495a40,magicFixtureLeader);domesticStub(0x490aa0,criticalFixtureNoForce);domesticStub(0x495c50,magicFixtureEnemy);
    domesticStub(0x483c80,magicFixtureNeighbors);domesticStub(0x483b40,magicFixtureMapObject);domesticStub(0x483b20,magicFixtureMapObject);
    domesticStub(0x590fc0,magicFixtureCommandName);domesticStub(0x591060,magicFixtureCommandEvent);
    domesticStub(0x49cb60,magicFixtureTextInit);domesticStub(0x49cb80,magicFixtureTextAppend);domesticStub(0x49b390,magicFixtureText);domesticStub(0x4f6690,afterFixtureMessage);
    domesticStub(0x5a0700,magicFixtureVisible);domesticStub(0x5a06c0,magicFixtureVisible);
    memcpy((void*)site(0x576b10),"\x0f\xbf\x44\x24\x04",5); // real native city-footprint classification
    domesticStub(0x402310,magicFixtureSoundRandom);domesticStub(0x5873a0,magicFixtureComplete);domesticStub(0x5afbe0,magicFixtureNotExplosive);
    domesticStub(0x47a6d0,magicFixtureNotExplosive);
    domesticStub(0x570b40,magicFixturePacket);domesticStub(0x590f50,magicFixtureClock);domesticStub(0x56de40,magicFixtureVoid);domesticStub(0x5940b0,magicFixtureNotExplosive);
    domesticStub(0x5b1870,magicFixtureApply);domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x4954c0,afterFixtureRecalc);
    domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);domesticStub(0x5b1a79,criticalFixtureQueuedCommitted);domesticStub(0x5b1ab4,criticalFixtureQueuedCommitted);
    domesticStub(0x486bc0,criticalFixtureBaseMax);domesticStub(0x487230,criticalFixtureSetBaseTroops);domesticStub(0x487150,criticalFixtureHpMax);domesticStub(0x4c0c30,criticalFixtureCityTech);domesticStub(0x4875c0,criticalFixtureEmptyGarrison);
    uint32_t addresses[32],saved[32];int addressCount=0;
    auto patch=[&](uint32_t at,uint32_t value){addresses[addressCount]=at;saved[addressCount++]=*(uint32_t*)site(at);*(uint32_t*)site(at)=value;};
    for(const auto& m:g_magicMapImmediates)if(m.kind)patch(m.at,(uint32_t)g_criticalCells+(m.kind==2?8:0));
    for(uint32_t at:{0x591f80u,0x591f87u,0x592d83u})patch(at,(uint32_t)g_criticalCells);
    patch(0x5b135c,(uint32_t)site(0x79c1d0));patch(0x5b1365,(uint32_t)site(0x79c1d0));
    patch(0x4721dc,(uint32_t)&g_defenseSeed);patch(0x4721ec,(uint32_t)&g_defenseSeed);patch(0x47215f,(uint32_t)&g_defenseSeed);patch(0x47216f,(uint32_t)&g_defenseSeed);
    patch(0x5931e3,(uint32_t)site(0x79cc88));
    auto prepare=[&](int effect,int provider,int defense,int intelligence,int before,int seed,bool base){
        g_skills[249].effects[70]=effect==70;g_skills[249].effects[71]=effect==71;
        *(int*)(actor+0xe8)=provider==0?effect:provider==2 || provider==4?249:-1;
        *(int*)(other+0xe8)=provider==1 || provider==4?effect:provider==3?249:-1;
        *(int*)(person+0xe8)=defense==1?61:defense==2?60:defense==3?63:-1;*(int*)(spouse+0xe8)=-1;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;
        source[0xc6]=intelligence==0?100:intelligence==1?60:80;target[0xc6]=80;second[0xc6]=100;
        for(auto unit:{target,second}){*(int*)(unit+0x24)=0;unit[0x28]=(uint8_t)before;*(uint16_t*)(unit+0x18)=(uint16_t)before;}
        *(int*)(target+8)=base?2:0;*(int16_t*)(target+0x10)=(int16_t)before;
        *(int*)(g_criticalBaseResource+0x24)=before;*(void**)g_criticalBaseResource=vt;
        memset(g_criticalCells,0,sizeof g_criticalCells);
        for(int i:{201,401,402})*(uint32_t*)(g_criticalCells+20*i)=1|(1<<23);
        if(base)*(uint32_t*)(g_criticalCells+20*401)=2|(1<<23);
        *(uint16_t*)(g_criticalCells+20*401+8)=4094;*(uint16_t*)(g_criticalCells+20*402+8)=4093;*(uint16_t*)(g_criticalCells+20*201+8)=4095;
        g_magicFixtureFootprint=false;g_magicFixtureBase=g_criticalBuildingTarget=base;g_criticalBaseRegistered=true;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        g_magicFixtureMutation=g_magicFixtureApplyCalls=g_magicFixtureStatusCalls=0;g_defenseSeed=seed;
    };
    uint32_t command[3]={7,4095,0x10002};auto confuse=(MagicCommand)site(0x592050),thunder=(MagicCommand)site(0x592ed0);
    assert(skilltelemetry::open(L"build/magic_confuse_test.csv",afterFixtureActor,nullptr));int commands=0;
    for(int provider=0;provider<6;++provider)for(int success:{0,1})for(int critical:{0,1})for(int defense=0;defense<4;++defense)
        for(int intelligence=0;intelligence<3;++intelligence)for(int before:{0,4,5})for(int seed:{0,3,4,9}){
            prepare(70,provider,defense,intelligence,before,seed,false);uint32_t state=seed;int calls=0;
            int states[2]={0,0},turns[2]={before,before};
            if(success)for(int n=0;n<2;++n){bool result=false;int stat=n?100:80;
                if(!n && (defense==1 || source[0xc6]<stat && (defense==2 || defense==3)))result=false;
                else{state=state*0x6c078965u+0x3039u;result=(state>>16)%100<(unsigned)(100-stat/2);}
                if(result){state=state*0x6c078965u+0x3039u;states[n]=(state>>16)%100<50?2:1;
                    turns[n]=before+2+(critical?1:0);if(turns[n]>5)turns[n]=5;++calls;}}
            assert(confuse(nullptr,command,success,critical,0,nullptr,1)==1);
            assert(g_defenseSeed==state && g_magicFixtureStatusCalls==calls && !g_magic && !g_statusWriteProbe);
            assert(*(int*)(target+0x24)==states[0] && target[0x28]==turns[0]);assert(*(int*)(second+0x24)==states[1] && second[0x28]==turns[1]);++commands;
        }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/magic_thunder_test.csv",afterFixtureActor,nullptr));command[0]=8;
    for(int provider=0;provider<6;++provider)for(int success:{0,1})for(int critical:{0,1})for(bool base:{false,true})
        for(int before:{0,100,1000,5000})for(int seed:{0,4}){
            prepare(71,provider,0,0,before,seed,base);uint32_t state=seed;int first=0,last=0;
            if(success){state=state*0x6c078965u+0x3039u;first=(base?700:1500)+(state>>16)%(base?300:1000);
                state=state*0x6c078965u+0x3039u;last=1500+(state>>16)%1000;}
            assert(thunder(nullptr,command,success,critical,0,nullptr,1)==1);
            assert(g_defenseSeed==state && !g_magic && g_magicFixtureApplyCalls==(success?2:0));
            if(base){int expected=before>first?before-first:0;if(success && expected>1000)expected=1000;assert(*(int16_t*)(target+0x10)==expected);}
            else assert(*(uint16_t*)(target+0x18)==(before>first?before-first:0));
            assert(*(uint16_t*)(second+0x18)==(before>last?before-last:0));++commands;
        }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/magic_footprint_test.csv",afterFixtureActor,nullptr));
    for(int provider:{0,2,3})for(int critical:{0,1})for(int before:{0,100,1000,5000})for(int seed:{0,4}){
        prepare(71,provider,0,0,before,seed,true);g_magicFixtureFootprint=true;*(int*)(target+8)=0;*(int*)(g_criticalBaseResource+0x40)=before;
        for(int i:{402,403})*(uint32_t*)(g_criticalCells+20*i)=2|(1<<23);
        uint32_t state=seed;int damage[3];for(int n=0;n<3;++n){state=state*0x6c078965u+0x3039u;damage[n]=700+(state>>16)%300;}
        int expected=before-damage[0]-damage[1]/10-damage[2]/10;if(expected<0)expected=0;if(expected>1000)expected=1000;
        int result=thunder(nullptr,command,1,critical,0,nullptr,1);
        assert(result==1 && !g_magic && g_defenseSeed==state && g_magicFixtureApplyCalls==1);
        assert(*(int16_t*)(target+0x10)==expected);++commands;
    }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/magic_edges_test.csv",afterFixtureActor,nullptr));
    for(int effect:{70,71})for(int mutation=1;mutation<=9;++mutation){
        prepare(effect,0,0,0,4,0,false);g_magicFixtureMutation=mutation;command[0]=effect==70?7:8;
        assert((effect==70?confuse:thunder)(nullptr,command,1,0,0,nullptr,1)==1 && !g_magic);
    }
    skilltelemetry::close();
    // Native per-target gates run once with logging and once without it. The
    // command owner/member lookup is controlled;591fd0's comparisons, OR order,
    // early returns and original RNG execute unchanged.
    std::vector<std::pair<int,uint32_t>> coeffectResults;
    for(int pass=0;pass<2;++pass){
        if(!pass)assert(skilltelemetry::open(L"build/magic_coeffect_test.csv",afterFixtureActor,nullptr));
        int index=0;
        for(int branch=0;branch<5;++branch)for(int provider=0;provider<6;++provider)for(int stat:{60,80,100}){
            prepare(70,0,0,0,0,4,false);command[0]=7;
            for(int e:{57,60,61,63})g_skills[249].effects[e]=false;
            int effect=branch==0?61:branch==1?57:branch==3?60:63;
            bool defensive=branch==0 || branch>=3;
            auto first=defensive?person:actor;auto deputy=defensive?spouse:other;
            *(int*)(target+0x10)=601;source[0xc6]=(uint8_t)stat;target[0xc6]=80;
            *(int*)(first+0xe8)=provider==0 || provider==4?effect:provider==2?249:defensive?-1:70;
            *(int*)(deputy+0xe8)=provider==1 || provider==4?effect:provider==3?249:defensive?-1:70;
            if(!defensive && provider==4)*(int*)(first+0xe8)=249; // retain70 beside the duplicated attack trait
            g_skills[249].effects[effect]=true;
            uint32_t expectedSeed=4;int expected;
            bool forced=provider!=5 && (effect==61 || (defensive?stat<80:stat>80));
            if(forced)expected=defensive?0:1;
            else{expectedSeed=expectedSeed*0x6c078965u+0x3039u;expected=(expectedSeed>>16)%100<60;}
            {MagicScope owner(70,command,1,0);assert(magicFind(source,nullptr,70)>=0);
                int result=magicDecision(source,target);
                assert(result==expected && g_defenseSeed==expectedSeed && !g_magicDecisionProbe);
                if(!pass)coeffectResults.push_back({result,g_defenseSeed});
                else assert(coeffectResults[index]==std::make_pair(result,(uint32_t)g_defenseSeed));
            }
            ++index;assert(!g_magic && !g_magicDecisionProbe);
        }
        assert(index==90);
        for(int priority=0;priority<3;++priority){
            prepare(70,0,0,0,0,4,false);command[0]=7;
            for(int e:{57,60,63})g_skills[249].effects[e]=true;
            g_skills[249].effects[61]=priority==0;
            *(int*)(actor+0xe8)=249;*(int*)(other+0xe8)=-1;
            *(int*)(person+0xe8)=priority==1?-1:249;*(int*)(spouse+0xe8)=-1;
            source[0xc6]=priority==2?60:100;target[0xc6]=80;
            {MagicScope owner(70,command,1,0);assert(magicFind(source,nullptr,70)==600);
                int result=magicDecision(source,target);assert(result==(priority==1?1:0) && g_defenseSeed==4 && !g_magicDecisionProbe);
                if(!pass)coeffectResults.push_back({result,g_defenseSeed});
                else assert(coeffectResults[index]==std::make_pair(result,(uint32_t)g_defenseSeed));
            }
            ++index;
        }
        assert(index==93);skilltelemetry::close();
    }
    // A direct native preview with matching skills has no command owner and
    // cannot turn a predicate into an applied event.
    assert(skilltelemetry::open(L"build/magic_coeffect_preview_test.csv",afterFixtureActor,nullptr));
    prepare(70,0,1,0,0,4,false);assert(magicDecision(source,target)==0 && g_defenseSeed==4);
    skilltelemetry::close();*(int*)(target+0x10)=-1;
    for(int e:{57,60,61,63})g_skills[249].effects[e]=false;
    for(int effect:{70,71}){
        prepare(effect,0,0,0,4,0,false);assert((effect==70?confuse:thunder)(nullptr,command,1,0,0,nullptr,1)==1 && !g_magic);
        uint32_t state=0;
        for(int n=0;n<2;++n){state=state*0x6c078965u+0x3039u;
            auto unit=n?second:target;
            if(effect==70){assert((state>>16)%100<(unsigned)(n?50:60));state=state*0x6c078965u+0x3039u;
                assert(*(int*)(unit+0x24)==((state>>16)%100<50?2:1) && unit[0x28]==5);}
            else assert(*(uint16_t*)(unit+0x18)==0);
        }
        assert(g_defenseSeed==state && !g_statusWriteProbe);
    }
    strategyAmbushTelemetryTests(source,target,actor,other,person,spouse);
    for(int n=0;n<addressCount;++n)*(uint32_t*)site(addresses[n])=saved[n];g_skills[249]=oldSkill;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_movementUnitById=oldUnit;g_telemetryUnitId=oldUnitId;g_growthValid=oldValid;
    g_unitTroopsOriginal=oldTroops;g_magicStatusOriginal=oldStatus;g_statusTurnsOriginal=oldTurns;
    g_magicConfuseResume=oldConfuse;g_magicThunderResume=oldThunder;g_magicConfuseFindResume=oldFind;g_magicThunderFindResume=oldThunderFind;g_magicThunderAdjustedResume=oldAdjust;
    g_magicDecisionEntry=oldDecision;g_magicThunderBuildOriginal=oldBuild;g_criticalQueuedTroopsResume=oldQueued;g_criticalBaseTroopsResume=oldBaseTroopsResume;g_criticalBaseHpResume=oldHpResume;
    g_telemetryBaseId=oldBaseId;g_baseById=oldBase;g_criticalBaseTroopsOriginal=oldBaseTroops;g_criticalBaseHpOriginal=oldHp;
    printf("PASS magic70/71: %d native command/provider/gate/RNG/status/packet inputs, real status and unit/base resource writes, bounded command-owned provenance\n",commands);
}
