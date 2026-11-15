// Execute both complete native tactic functions. Model/world getters and the
// damage producer remain explicit adapters; actual troop stores stay native.
static uint8_t attackTacticModels[32][0x50],attackTacticWeapon[0xb0];
static void* __fastcall attackTacticModel(void*,void*,int id){assert(id>=0 && id<32);return attackTacticModels[id];}
static void* __fastcall attackTacticWeaponModel(void*,void*,int){return attackTacticWeapon;}
static int __fastcall attackTacticAptitude(void*,void*,int){return 3;}
static int __fastcall attackTacticEnergy(void*,void*){return 50;}
static void* __cdecl attackTacticAsUnit(void* unit){return unit;}
static int __cdecl attackTacticWeaponKind(int){return 0;}
static int __fastcall attackTacticLine(void*,void*,uint32_t,uint32_t,int){return 0;}
#include "attack_coeffect_telemetry_cases.inc"
static void attackTacticTelemetryTests(CriticalProducer producer,uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other){
    auto oldBounds=g_attackReachTacticBounds;auto oldPermission=g_attackReachTacticPermission;
    g_attackReachTacticBounds=(AttackReachTacticBounds)site(0x5a3a60);
    g_attackReachTacticPermission=(AttackReachTacticPermission)site(0x5a3f90);
    // The native range builder has a three-entry jump table and byte dispatch.
    uint32_t jumps[3];for(int n=0;n<3;++n)jumps[n]=(uint32_t)site(*(uint32_t*)site(0x5a3bc8+4*n));
    *(uint32_t*)site(0x5a3b65)=(uint32_t)site(0x5a3bce);*(uint32_t*)site(0x5a3b6c)=(uint32_t)jumps;
    // Native bit testing probes its actual four-byte word through this IAT.
    static auto readProbe=&IsBadReadPtr;*(uint32_t*)site(0x4725b7)=(uint32_t)&readProbe;
    domesticStub(0x490c90,attackTacticModel);domesticStub(0x490bc0,attackTacticWeaponModel);
    domesticStub(0x496d50,attackTacticAptitude);domesticStub(0x495fe0,attackTacticAsUnit);
    domesticStub(0x4946e0,attackTacticWeaponKind);domesticStub(0x484040,attackTacticLine);
    auto vt=*(void***)source;void* oldEnergy=vt[0x54/4];vt[0x54/4]=attackTacticEnergy;
    memset(attackTacticModels,0,sizeof attackTacticModels);
    for(auto& data:attackTacticModels){data[0x30]=10;data[0x31]=1;data[0x32]=2;data[0x33]=1;*(uint32_t*)(data+0x3c)=2;}
    uint8_t packet[0x80];int inputs=0,expectedEvents=0;g_criticalRandomCalls=0;
    assert(skilltelemetry::open(L"build/attack_tactic_test.csv",personFixtureActor,nullptr));
    for(int layout=0;layout<2;++layout){int width=layout?249:200,height=layout?530:200,x=height-10,y=width-10;
        std::vector<uint8_t> map((size_t)width*height*20);
        *(int*)site(0x5a078f)=*(int*)site(0x5a079a)=*(int*)site(0x49616b)=*(int*)site(0x5a3ffd)=width;
        *(int*)site(0x5a0783)=height;*(uint32_t*)site(0x5a07aa)=(uint32_t)map.data();
        *(uint32_t*)site(0x49617a)=(uint32_t)map.data()+4;*(uint32_t*)site(0x5a4009)=(uint32_t)map.data();
        for(int tactic:{12,15,6,7,8,31})for(int provider=0;provider<6;++provider)for(int available=0;available<2;++available)
        for(int natural=0;natural<2;++natural)for(int allow=0;allow<2;++allow)for(int distance=1;distance<=4;++distance)
        for(int route=0;route<2;++route)for(int tech=0;tech<2;++tech){
            int effect=tactic==12 || tactic==15?24:50;if(effect==24 && (natural || tech))continue;
            ++inputs;attackReachMutation=0;attackReachTechnology=tech!=0;attackReachAllowed=allow!=0;
            attackReachSourceChanged=attackReachTargetChanged=false;
            g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;g_criticalBuildingTarget=false;
            *(int*)(actor+0xe8)=provider==0?effect:provider==2 || provider==4?249:-1;
            *(int*)(other+0xe8)=provider==1 || provider==4?effect:provider==3?249:-1;
            *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
            *(int*)(source+8)=0;*(int*)(source+0x48)=*(int*)(source+0xa8)=effect==50?3:tactic==12?6:7;
            *(uint32_t*)(source+4)=x|(y<<16);*(uint32_t*)(target+4)=x|((y+distance)<<16);*(uint16_t*)(target+0x18)=1000;
            memset(attackTacticWeapon,0,sizeof attackTacticWeapon);attackTacticWeapon[0x78+tactic]=(uint8_t)available;
            *(uint32_t*)(attackTacticModels[tactic]+0x38)=1u|(natural?1u<<5:0u);
            auto src=map.data()+20*((size_t)x*width+y),dst=map.data()+20*((size_t)x*width+y+distance);
            *(uint32_t*)src=*(uint32_t*)dst=1;*(uint16_t*)(src+8)=4095;*(uint16_t*)(dst+8)=4094;*(uint32_t*)(dst+4)=effect==50?5:0;
            uint8_t sourceBefore[0x110],targetBefore[0x110],peopleBefore[800];
            memcpy(sourceBefore,source,sizeof sourceBefore);memcpy(targetBefore,target,sizeof targetBefore);
            memcpy(peopleBefore,actor,400);memcpy(peopleBefore+400,other,400);
            // Independent original-native with/without-effect permissions. The
            // scoped predicate suppression emits no queries and changes no data.
            int masks[2];for(int baseline=0;baseline<2;++baseline){DomesticProbe quiet(baseline?effect:-99);quiet.baseline=true;
                uint8_t choices[72]={};g_attackReachTacticBounds(choices,source,*(uint32_t*)(source+4));
                masks[baseline]=g_attackReachTacticPermission(source,*(uint32_t*)(source+4),target,*(uint32_t*)(target+4),distance,choices);
            }
            bool expected=((uint32_t)masks[0]&(1u<<tactic)) && !((uint32_t)masks[1]&(1u<<tactic));
            int calls=attackReachProducerCalls;
            assert(producer(packet,source,tactic,*(uint32_t*)(target+4),0,0,1)==0x1234 && attackReachProducerCalls==calls+1);
            auto record=criticalLookup(packet);assert((record.attackReach.actor!=nullptr)==expected);
            if(expected){++expectedEvents;assert(record.attackReach.tactic==tactic && record.attackReach.effect==effect);}
            assert(!memcmp(sourceBefore,source,sizeof sourceBefore) && !memcmp(targetBefore,target,sizeof targetBefore) &&
                !memcmp(peopleBefore,actor,400) && !memcmp(peopleBefore+400,other,400) && g_criticalRandomCalls==0);
            assert((route?runCriticalQueued(target,source,packet):runCriticalCommit(target,source,packet))==-250 && *(uint16_t*)(target+0x18)==750);
            criticalTake(packet);
        }
    }
    assert(inputs==6912 && expectedEvents==420 && g_criticalRecords.empty());skilltelemetry::close();
    std::vector<uint8_t> edgeMap(200*200*20);
    *(int*)site(0x5a078f)=*(int*)site(0x5a079a)=*(int*)site(0x49616b)=*(int*)site(0x5a3ffd)=200;
    *(int*)site(0x5a0783)=200;*(uint32_t*)site(0x5a07aa)=(uint32_t)edgeMap.data();
    *(uint32_t*)site(0x49617a)=(uint32_t)edgeMap.data()+4;*(uint32_t*)site(0x5a4009)=(uint32_t)edgeMap.data();
    assert(skilltelemetry::open(L"build/attack_tactic_edges_test.csv",personFixtureActor,nullptr));
    for(int effect:{24,50})for(int route=0;route<2;++route)for(int mutation=0;mutation<14;++mutation){
        int tactic=effect==24?12:6,distance=effect==24?3:2;
        attackReachMutation=mutation<=10?mutation:0;attackReachTechnology=false;attackReachAllowed=true;
        attackReachSourceChanged=attackReachTargetChanged=false;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        *(int*)(actor+0xe8)=effect;*(int*)(other+0xe8)=-1;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=*(int*)(source+0x14)=-1;
        *(int*)(source+0x48)=*(int*)(source+0xa8)=effect==24?6:3;
        *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=1|((1+distance)<<16);*(uint16_t*)(target+0x18)=1000;
        memset(attackTacticWeapon,0,sizeof attackTacticWeapon);attackTacticWeapon[0x78+tactic]=1;
        *(uint32_t*)(attackTacticModels[tactic]+0x38)=1;
        auto dst=edgeMap.data()+20*(201+distance);*(uint32_t*)dst=1;*(uint16_t*)(dst+8)=4094;*(uint32_t*)(dst+4)=effect==50?5:0;
        producer(packet,source,tactic,*(uint32_t*)(target+4),0,0,1);
        if(mutation==11)++*(int*)(packet+8);
        if(mutation==12)criticalTake(packet);
        if(mutation==13)skilltelemetry::invalidateContext();
        assert((route?runCriticalQueued(target,source,packet):runCriticalCommit(target,source,packet))==-250 && *(uint16_t*)(target+0x18)==750);
        criticalTake(packet);
    }
    skilltelemetry::close();attackReachMutation=0;
    attackCoeffectTelemetryTests(producer,source,target,actor,other);
    assert(g_criticalRandomCalls==0);vt[0x54/4]=oldEnergy;
    g_attackReachTacticBounds=oldBounds;g_attackReachTacticPermission=oldPermission;
    puts("PASS tactic24/50: 6912 full-native bounds/permission paired baselines, 420 actual troop settlements, both maps, tactic31, forest/technology/availability/ownership and zero extra RNG");
    puts("PASS tactic24/50: 52 stale/provider/member/unit/position/weapon/kind/tactic/owner/world exclusions, four controls, direct and queued native troop stores");
}
