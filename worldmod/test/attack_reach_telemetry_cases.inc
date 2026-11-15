// Original bounds and complete ordinary permission functions execute. The
// damage producer is an explicit packet adapter; actual resource CALLs and
// troop clamps/setters execute for direct and queued settlement paths.
static bool attackReachTechnology=false,attackReachAllowed=true;
static bool attackReachSourceChanged=false,attackReachTargetChanged=false;
static int __fastcall attackReachOwner(void* unit,void*){return unit==g_criticalSource?(attackReachSourceChanged?4:2):(attackReachTargetChanged?4:3);}
static int attackReachMutation=0,attackReachProducerCalls=0;
static uint8_t attackReachForce[0x1000];
static const uint8_t* attackReachCopySource=nullptr;
static uint8_t attackReachExpiredCopy[0x80];
static int __cdecl attackReachCopied(uint8_t* destination){
    assert(g_criticalCopyScope && g_criticalCopyScope->destination==destination);
    auto record=criticalLookup(destination);
    assert(!memcmp(destination,attackReachCopySource,0x604) && (record.attackReach.actor || record.forestReach.actor));
    memcpy(attackReachExpiredCopy,destination,0x80);
    assert(runCriticalQueued(g_criticalTarget,g_criticalSource,destination)==-250);return 0x5678;
}
__declspec(naked) static void attackReachCopiedOne(){__asm {lea eax,[esp+0x178]
    push eax
    call attackReachCopied
    add esp,4
    pop edi
    pop esi
    mov esp,ebp
    pop ebp
    ret 4}}
__declspec(naked) static void attackReachCopiedTwo(){__asm {lea eax,[esp+0x4d8]
    push eax
    call attackReachCopied
    add esp,4
    pop edi
    pop esi
    mov esp,ebp
    pop ebp
    ret 8}}
__declspec(naked) static void attackReachHandlerTwo(){__asm {sub esp,0xc00
    push esi
    push edi
    mov esi,[ebp+8]
    jmp dword ptr [g_criticalCopyTwoEntry]}}
static int __fastcall attackReachTech(void*,void*,int tech){return tech==10 && attackReachTechnology;}
static void* __fastcall attackReachForceLookup(void*,void*,int id){assert(id==2);return attackReachForce;}
static int __fastcall attackReachAllowedTarget(void*,void*,void*){return attackReachAllowed;}
static int __cdecl attackReachNotExplosive(void*){return 0;}
static int __cdecl attackReachRandom(int){++g_criticalRandomCalls;return 0;}
static void* __fastcall attackReachObject(uint8_t* cell,void*){return *(uint16_t*)(cell+8)==4094?g_criticalTarget:g_criticalSource;}
static int __cdecl attackReachFixtureProducer(uint8_t* packet,void* source,int tactic,uint32_t to,int mode,int,int){
    ++attackReachProducerCalls;memset(packet,0,0x80);*(int*)(packet+8)=tactic;*(int*)(packet+0xc)=250;
    *(uint32_t*)(packet+0x58)=*(uint32_t*)((uint8_t*)source+4);*(uint32_t*)(packet+0x5c)=to;
    if(attackReachMutation==1)skilltelemetry::invalidateContext();
    if(attackReachMutation==2)*(int*)(g_personGuide+0xe8)=-1;
    if(attackReachMutation==3)*(int*)((uint8_t*)source+0xc)=-1;
    if(attackReachMutation==4)g_criticalSourceRegistered=false;
    if(attackReachMutation==5)g_criticalTargetRegistered=false;
    if(attackReachMutation==6)++*(uint32_t*)((uint8_t*)source+4);
    if(attackReachMutation==7)++*(uint32_t*)(g_criticalTarget+4);
    if(attackReachMutation==8)*(int*)(packet+0x70)=1;
    if(attackReachMutation==9)++*(int*)((uint8_t*)source+0xa8);
    if(attackReachMutation==10)g_criticalActorRegistered=false;
    if(attackReachMutation==15)attackReachSourceChanged=true;
    if(attackReachMutation==16)attackReachTargetChanged=true;
    return 0x1234;
}
#include "attack_tactic_telemetry_cases.inc"
static void attackReachTelemetryTests(){
    uint8_t source[0x110]={},target[0x110]={},actor[400]={},other[400]={},person[400]={},spouse[400]={},packet[0x80]={};
    g_criticalSource=source;g_criticalTarget=target;g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    void* vt[25]={};vt[0x2c/4]=criticalFixtureObject;vt[0x3c/4]=criticalFixturePosition;vt[0x40/4]=attackReachOwner;
    vt[0x50/4]=battleTroops;vt[0x58/4]=(void*)site(0x47a8a0);vt[0x5c/4]=criticalFixtureFind;
    *(void**)source=*(void**)target=vt;
    *(int*)(target+0xc)=1000;*(int*)(person+0xe8)=-1;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;auto oldUnit=g_movementUnitById;auto oldId=g_telemetryUnitId;
    auto oldProducer=g_criticalProducerOriginal;auto oldTroops=g_unitTroopsOriginal;auto oldMap=g_movementMapOperand;auto oldObject=g_reachObjectOriginal;
    auto oldBounds=g_attackReachBoundsEntry,oldPermission=g_attackReachPermissionEntry;auto oldHandle=g_attackReachHandleUnit;
    auto oldValid=g_growthValid;g_growthValid=domesticValid;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)criticalFixturePersonId;
    g_movementUnitById=(MovementUnitById)criticalFixtureUnit;g_telemetryUnitId=(BuildingId)criticalFixtureUnitId;
    g_criticalProducerOriginal=attackReachFixtureProducer;g_unitTroopsOriginal=(IncomeGrant)site(0x4ae4a0);
    g_movementMapOperand=(uint32_t*)site(0x5a07aa);g_reachObjectOriginal=(ReachObject)attackReachObject;
    g_attackReachBoundsEntry=site(0x5a3870);g_attackReachPermissionEntry=site(0x5a4990);g_attackReachHandleUnit=(AttackReachHandleUnit)site(0x56dba0);
    // Restore helper bodies after earlier fixtures' adapters, before relocating.
    for(const auto& s:g_attackReachLayouts)memcpy((void*)site(s.start),s.bytes,s.size);
    memcpy((void*)site(0x56dba0),"\x0f\xb7\x41\x08\x69\xc0\xf4\x00\x00\x00\x05\x88\xb0\x36\x07\xc3",16);
    memcpy((void*)site(0x496160),"\x0f\xbf\x44\x24\x04",5);
    *(uint32_t*)site(0x56dbab)=(uint32_t)source-4095*0xf4;
    uint32_t jumps[9];for(int n=0;n<9;++n)jumps[n]=(uint32_t)site(*(uint32_t*)site(0x5a3958+4*n));
    *(uint32_t*)site(0x5a38b5)=(uint32_t)jumps;
    domesticStub(0x490aa0,attackReachForceLookup);domesticStub(0x4811e0,attackReachTech);
    domesticStub(0x496cc0,attackReachAllowedTarget);domesticStub(0x5afbe0,attackReachNotExplosive);
    domesticStub(0x47a630,domesticValid);domesticStub(0x490b00,personFixtureLookup);
    domesticStub(0x4721d0,attackReachRandom);
    domesticStub(0x472150,attackReachRandom);
    domesticStub(0x4957b0,criticalFixtureCapacity);domesticStub(0x48a4f0,battleCapacity);
    domesticStub(0x584fe4,criticalFixtureCommitted);g_criticalCommitEntry=site(0x584fdf);g_criticalTroopsResume=(uint32_t)site(0x584fe4);
    domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);
    auto producer=(CriticalProducer)site(0x5afc70);
    bool old24=g_skills[249].effects[24],old50=g_skills[249].effects[50];g_skills[249].effects[24]=g_skills[249].effects[50]=true;
    int inputs=0;g_criticalRandomCalls=0;
    assert(skilltelemetry::open(L"build/attack_reach_test.csv",personFixtureActor,nullptr));
    for(int layout=0;layout<2;++layout){int width=layout?249:200,height=layout?530:200,x=height-10,y=width-10;
        std::vector<uint8_t> map((size_t)width*height*20);
        *(int*)site(0x5a078f)=*(int*)site(0x5a079a)=*(int*)site(0x49616b)=*(int*)site(0x5a4c12)=width;
        *(int*)site(0x5a0783)=height;*(uint32_t*)site(0x5a07aa)=(uint32_t)map.data();
        *(uint32_t*)site(0x49617a)=*(uint32_t*)site(0x5a4c1e)=(uint32_t)map.data()+4;
        for(int weapon:{3,6,7})for(int provider=0;provider<6;++provider)for(int tech=0;tech<2;++tech)
        for(int distance=1;distance<=5;++distance)for(int terrain:{0,5})for(int allow=0;allow<2;++allow)
        for(int before:{0,100,1000,10000})for(int route=0;route<2;++route){++inputs;
            attackReachMutation=0;attackReachTechnology=tech!=0;attackReachAllowed=allow!=0;
            g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;g_criticalBuildingTarget=false;
            int effect=weapon==3?50:24;*(int*)(actor+0xe8)=provider==0?effect:provider==2 || provider==4?249:-1;
            *(int*)(other+0xe8)=provider==1 || provider==4?effect:provider==3?249:-1;
            *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
            *(int*)(source+8)=0;*(int*)(source+0x48)=*(int*)(source+0xa8)=weapon;
            *(uint32_t*)(source+4)=x|(y<<16);*(uint32_t*)(target+4)=x|((y+distance)<<16);
            *(uint16_t*)(target+0x18)=(uint16_t)before;
            auto src=map.data()+20*((size_t)x*width+y),dst=map.data()+20*((size_t)x*width+y+distance);
            *(uint32_t*)src=*(uint32_t*)dst=1;*(uint32_t*)(src+4)=0;*(uint32_t*)(dst+4)=terrain;
            *(uint16_t*)(src+8)=4095;*(uint16_t*)(dst+8)=4094;
            uint8_t sourceBefore[sizeof source],targetBefore[sizeof target],peopleBefore[800];memcpy(sourceBefore,source,sizeof source);
            memcpy(targetBefore,target,sizeof target);
            memcpy(peopleBefore,actor,400);memcpy(peopleBefore+400,other,400);
            int calls=attackReachProducerCalls;
            assert(producer(packet,source,-1,*(uint32_t*)(target+4),0,0,1)==0x1234);
            assert(attackReachProducerCalls==calls+1 && !memcmp(sourceBefore,source,sizeof source) &&
                !memcmp(targetBefore,target,sizeof target) && !memcmp(peopleBefore,actor,400) && !memcmp(peopleBefore+400,other,400) && g_criticalRandomCalls==0);
            bool expectedReach=provider!=5 && allow && (weapon==3?terrain==5 && distance>1 && distance<=2+tech:distance==(weapon==6?3:4));
            assert((criticalLookup(packet).attackReach.actor!=nullptr)==expectedReach);
            int expected=before<250?before:250;
            assert((route?runCriticalQueued(target,source,packet):runCriticalCommit(target,source,packet))==-expected);
            assert(*(uint16_t*)(target+0x18)==before-expected);
            criticalTake(packet);
        }
    }
    assert(inputs==11520 && g_criticalRecords.empty());skilltelemetry::close();
    puts("PASS attack reach24/50: 11520 original native bounds/permission and actual direct/queued troop stores across two maps, providers, technology, distance, terrain, permission and caps");
    // Same valid attacks with stale provenance between construction/settlement.
    std::vector<uint8_t> map(200*200*20);*(int*)site(0x5a078f)=*(int*)site(0x5a079a)=*(int*)site(0x49616b)=*(int*)site(0x5a4c12)=200;
    *(int*)site(0x5a0783)=200;*(uint32_t*)site(0x5a07aa)=(uint32_t)map.data();
    *(uint32_t*)site(0x49617a)=*(uint32_t*)site(0x5a4c1e)=(uint32_t)map.data()+4;
    assert(skilltelemetry::open(L"build/attack_reach_edges_test.csv",personFixtureActor,nullptr));
    for(int effect:{24,50})for(int mutation=0;mutation<17;++mutation){attackReachMutation=mutation<=10 || mutation>=15?mutation:0;
        attackReachSourceChanged=attackReachTargetChanged=false;
        attackReachAllowed=true;attackReachTechnology=false;g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        *(int*)(actor+0xe8)=effect;*(int*)(other+0xe8)=-1;*(int*)(source+0xc)=600;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
        *(int*)(source+0x48)=*(int*)(source+0xa8)=effect==24?6:3;*(uint32_t*)(source+4)=0x10001;
        int distance=effect==24?3:2;*(uint32_t*)(target+4)=1|((1+distance)<<16);*(uint16_t*)(target+0x18)=1000;
        auto dst=map.data()+20*(201+distance);*(uint32_t*)dst=1;*(uint16_t*)(dst+8)=4094;*(uint32_t*)(dst+4)=effect==50?5:0;
        *(uint32_t*)(map.data()+20*201+4)=0;
        producer(packet,source,mutation==11?32:-1,*(uint32_t*)(target+4),mutation==12?1:0,0,1);
        if(mutation==13)packet[0xc]^=1;if(mutation==14)criticalReset(packet);
        runCriticalCommit(target,source,packet);criticalTake(packet);
    }
    skilltelemetry::close();attackReachMutation=0;
    puts("PASS attack reach: 32 stale/ownership/nonordinary/mode/packet/reset exclusions, two controls and native RNG preservation");
    g_criticalHandlerOneOriginal=(CriticalHandlerOne)criticalHandlerOneNative;g_criticalHandlerTwoOriginal=(CriticalHandlerTwo)criticalHandlerTwoNative;
    g_criticalHandlerOneResume=(uint32_t)site(0x585786);g_criticalHandlerTwoResume=(uint32_t)site(0x5860a6);
    g_criticalCopyOneResume=(uint32_t)site(0x5857bd);g_criticalCopyTwoResume=(uint32_t)site(0x5860d5);
    g_criticalCopyOneOffset=(const int*)site(0x5857c0);g_criticalCopyTwoOffset=(const int*)site(0x5860d8);
    g_criticalCopyOneEntry=site(0x5857b8);g_criticalCopyTwoEntry=site(0x5860d0);
    domesticStub(0x585786,criticalFixtureHandlerOne);domesticStub(0x5860a6,attackReachHandlerTwo);
    domesticStub(0x5857c6,attackReachCopiedOne);domesticStub(0x5860de,attackReachCopiedTwo);
    assert(skilltelemetry::open(L"build/attack_reach_copy_test.csv",personFixtureActor,nullptr));
    for(int effect:{24,50})for(int route=0;route<2;++route){
        attackReachSourceChanged=attackReachTargetChanged=false;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;attackReachAllowed=true;attackReachTechnology=false;
        *(int*)(actor+0xe8)=effect;*(int*)(source+0xc)=600;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
        *(int*)(source+0x48)=*(int*)(source+0xa8)=effect==24?6:3;*(uint32_t*)(source+4)=0x10001;
        int distance=effect==24?3:2;*(uint32_t*)(target+4)=1|((1+distance)<<16);*(uint16_t*)(target+0x18)=1000;
        auto dst=map.data()+20*(201+distance);*(uint32_t*)dst=1;*(uint16_t*)(dst+8)=4094;*(uint32_t*)(dst+4)=effect==50?5:0;
        uint8_t group[0x604]={};attackReachCopySource=group;
        {CriticalCopyScope owner;owner.destination=group;
            producer(group,source,-1,*(uint32_t*)(target+4),0,0,1);*(int*)(group+0x600)=1;
            int result=route?((CriticalHandlerTwo)site(0x5860a0))(nullptr,group,7):((CriticalHandlerOne)site(0x585780))(nullptr,group);
            assert(result==0x5678 && *(uint16_t*)(target+0x18)==750);
        }
        assert(g_criticalRecords.empty());*(uint16_t*)(target+0x18)=1000;
        assert(runCriticalQueued(target,source,attackReachExpiredCopy)==-250); // unowned identical bytes cannot revive provenance
    }
    skilltelemetry::close();assert(g_criticalRandomCalls==0);
    puts("PASS attack reach: both native REP MOVSD transports, owner cleanup and unowned identical-byte exclusion");
    *(uint16_t*)(target+0x18)=1000;int calls=attackReachProducerCalls;
    uint8_t sourceBefore[sizeof source];memcpy(sourceBefore,source,sizeof source);
    assert(producer(packet,source,-1,*(uint32_t*)(target+4),0,0,1)==0x1234 && attackReachProducerCalls==calls+1);
    assert(runCriticalCommit(target,source,packet)==-250 && *(uint16_t*)(target+0x18)==750 &&
        !memcmp(sourceBefore,source,sizeof source) && g_criticalRecords.empty() && g_criticalRandomCalls==0);
    attackTacticTelemetryTests(producer,source,target,actor,other);
    g_skills[249].effects[24]=old24;g_skills[249].effects[50]=old50;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;g_movementUnitById=oldUnit;g_telemetryUnitId=oldId;
    g_criticalProducerOriginal=oldProducer;g_unitTroopsOriginal=oldTroops;g_movementMapOperand=oldMap;g_reachObjectOriginal=oldObject;
    g_attackReachBoundsEntry=oldBounds;g_attackReachPermissionEntry=oldPermission;g_attackReachHandleUnit=oldHandle;
    g_growthValid=oldValid;
}
