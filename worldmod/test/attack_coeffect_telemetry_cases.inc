// Custom crossbow models can expose both the24 range gate and the50 forest
// gate for tactic12/15. Compare each attribution with an independent native
// execution omitting just that effect; do not add their whole-hit losses.
static uintptr_t attackCoAggregateCall;
__declspec(naked) static void __cdecl attackCoAggregate(uint8_t*){__asm {
    push ebx
    push esi
    push edi
    mov eax,[esp+16]
    lea ecx,[eax-0x488]
    push eax
    jmp dword ptr [attackCoAggregateCall]}}
__declspec(naked) static void attackCoAggregated(){__asm {pop edi
    pop esi
    pop ebx
    ret}}
static void attackCoeffectTelemetryTests(CriticalProducer producer,uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other){
    const int firstSkills[]={24,50,249,24,50,-1,249,24,-1};
    const int otherSkills[]={50,24,-1,-1,-1,-1,249,249,249};
    uint8_t packet[0x80];int inputs=0,expectedEvents=0,expectedPairs=0;
    assert(skilltelemetry::open(L"build/attack_coeffect_test.csv",personFixtureActor,nullptr));
    for(int layout=0;layout<2;++layout){int width=layout?249:200,height=layout?530:200,x=height-10,y=width-10;
        std::vector<uint8_t> map((size_t)width*height*20);
        *(int*)site(0x5a078f)=*(int*)site(0x5a079a)=*(int*)site(0x49616b)=*(int*)site(0x5a3ffd)=width;
        *(int*)site(0x5a0783)=height;*(uint32_t*)site(0x5a07aa)=(uint32_t)map.data();
        *(uint32_t*)site(0x49617a)=(uint32_t)map.data()+4;*(uint32_t*)site(0x5a4009)=(uint32_t)map.data();
        for(int tactic:{12,15})for(int provider=0;provider<9;++provider)for(int natural=0;natural<2;++natural)
        for(int allow=0;allow<2;++allow)for(int distance=1;distance<=4;++distance)for(int route=0;route<2;++route)
        for(int before:{0,100,1000,10000}){
            ++inputs;attackReachMutation=0;attackReachTechnology=false;attackReachAllowed=allow!=0;
            attackReachSourceChanged=attackReachTargetChanged=false;
            g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;g_criticalBuildingTarget=false;
            *(int*)(actor+0xe8)=firstSkills[provider];*(int*)(other+0xe8)=otherSkills[provider];
            *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
            *(int*)(source+8)=0;*(int*)(source+0x48)=*(int*)(source+0xa8)=3;
            *(uint32_t*)(source+4)=x|(y<<16);*(uint32_t*)(target+4)=x|((y+distance)<<16);*(uint16_t*)(target+0x18)=(uint16_t)before;
            memset(attackTacticWeapon,0,sizeof attackTacticWeapon);attackTacticWeapon[0x78+tactic]=1;
            *(uint32_t*)(attackTacticModels[tactic]+0x38)=1u|(natural?1u<<5:0u);
            auto src=map.data()+20*((size_t)x*width+y),dst=map.data()+20*((size_t)x*width+y+distance);
            *(uint32_t*)src=*(uint32_t*)dst=1;*(uint16_t*)(src+8)=4095;*(uint16_t*)(dst+8)=4094;*(uint32_t*)(dst+4)=5;
            uint8_t sourceBefore[0x110],targetBefore[0x110],peopleBefore[800];
            memcpy(sourceBefore,source,sizeof sourceBefore);memcpy(targetBefore,target,sizeof targetBefore);
            memcpy(peopleBefore,actor,400);memcpy(peopleBefore+400,other,400);
            int masks[3];int omitted[]={-99,24,50};
            for(int n=0;n<3;++n){DomesticProbe quiet(omitted[n]);quiet.baseline=true;
                uint8_t choices[72]={};g_attackReachTacticBounds(choices,source,*(uint32_t*)(source+4));
                masks[n]=g_attackReachTacticPermission(source,*(uint32_t*)(source+4),target,*(uint32_t*)(target+4),distance,choices);
            }
            bool range=((uint32_t)masks[0]&(1u<<tactic)) && !((uint32_t)masks[1]&(1u<<tactic));
            bool forest=((uint32_t)masks[0]&(1u<<tactic)) && !((uint32_t)masks[2]&(1u<<tactic));
            expectedEvents+=(int)range+(int)forest;expectedPairs+=range && forest;
            int calls=attackReachProducerCalls;
            assert(producer(packet,source,tactic,*(uint32_t*)(target+4),0,0,1)==0x1234 && attackReachProducerCalls==calls+1);
            auto record=criticalLookup(packet);
            assert(attackReachGrantMatches(record,source,packet,target,-250)==(range || forest));
            assert(!memcmp(sourceBefore,source,sizeof sourceBefore) && !memcmp(targetBefore,target,sizeof targetBefore) &&
                !memcmp(peopleBefore,actor,400) && !memcmp(peopleBefore+400,other,400) && g_criticalRandomCalls==0);
            assert((route?runCriticalQueued(target,source,packet):runCriticalCommit(target,source,packet))==-min(before,250));
            assert(*(uint16_t*)(target+0x18)==before-min(before,250));criticalTake(packet);
        }
    }
    skilltelemetry::close();assert(inputs==4608 && expectedEvents==1056 && expectedPairs==192 && g_criticalRecords.empty());
    std::vector<uint8_t> map(200*200*20);
    *(int*)site(0x5a078f)=*(int*)site(0x5a079a)=*(int*)site(0x49616b)=*(int*)site(0x5a3ffd)=200;
    *(int*)site(0x5a0783)=200;*(uint32_t*)site(0x5a07aa)=(uint32_t)map.data();
    *(uint32_t*)site(0x49617a)=(uint32_t)map.data()+4;*(uint32_t*)site(0x5a4009)=(uint32_t)map.data();
    assert(skilltelemetry::open(L"build/attack_coeffect_copy_test.csv",personFixtureActor,nullptr));
    for(int tactic:{12,15})for(int provider=0;provider<4;++provider)for(int route=0;route<2;++route){
        attackReachMutation=0;attackReachAllowed=true;attackReachTechnology=false;
        attackReachSourceChanged=attackReachTargetChanged=false;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        *(int*)(actor+0xe8)=provider==0?24:provider==1?50:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==0?50:provider==3?249:-1;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        *(int*)(source+0x48)=*(int*)(source+0xa8)=3;*(uint32_t*)(source+4)=0x10001;
        int distance=provider==0 || provider==2?3:2;
        *(uint32_t*)(target+4)=1|((1+distance)<<16);*(uint16_t*)(target+0x18)=1000;
        memset(attackTacticWeapon,0,sizeof attackTacticWeapon);attackTacticWeapon[0x78+tactic]=1;
        *(uint32_t*)(attackTacticModels[tactic]+0x38)=1;
        auto dst=map.data()+20*(201+distance);*(uint32_t*)dst=1;*(uint16_t*)(dst+8)=4094;*(uint32_t*)(dst+4)=5;
        uint8_t group[0x604]={};attackReachCopySource=group;
        {CriticalCopyScope owner;owner.destination=group;
            producer(group,source,tactic,*(uint32_t*)(target+4),0,0,1);*(int*)(group+0x600)=1;
            int result=route?((CriticalHandlerTwo)site(0x5860a0))(nullptr,group,7):((CriticalHandlerOne)site(0x585780))(nullptr,group);
            assert(result==0x5678 && *(uint16_t*)(target+0x18)==750);
        }
        assert(g_criticalRecords.empty());*(uint16_t*)(target+0x18)=1000;
        assert(runCriticalQueued(target,source,attackReachExpiredCopy)==-250 && g_criticalRecords.empty());
    }
    skilltelemetry::close();assert(g_criticalRandomCalls==0);
    assert(skilltelemetry::open(L"build/attack_coeffect_stale_test.csv",personFixtureActor,nullptr));
    for(int tactic:{12,15})for(int route=0;route<2;++route)for(int mutation=0;mutation<11;++mutation){
        attackReachMutation=0;attackReachAllowed=true;attackReachTechnology=false;
        attackReachSourceChanged=attackReachTargetChanged=false;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;g_personOther=other;
        *(int*)(actor+0xe8)=24;*(int*)(other+0xe8)=50;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        *(int*)(source+0x48)=*(int*)(source+0xa8)=3;*(uint32_t*)(source+4)=0x10001;
        *(uint32_t*)(target+4)=0x40001;*(uint16_t*)(target+0x18)=1000;
        memset(attackTacticWeapon,0,sizeof attackTacticWeapon);attackTacticWeapon[0x78+tactic]=1;
        *(uint32_t*)(attackTacticModels[tactic]+0x38)=1;
        auto dst=map.data()+20*204;*(uint32_t*)dst=1;*(uint16_t*)(dst+8)=4094;*(uint32_t*)(dst+4)=5;
        producer(packet,source,tactic,*(uint32_t*)(target+4),0,0,1);
        if(mutation==1)*(int*)(actor+0xe8)=-1;
        if(mutation==2)*(int*)(other+0xe8)=-1;
        if(mutation==3)*(int*)(source+0xc)=-1;
        if(mutation==4)*(int*)(source+0x10)=-1;
        if(mutation==5)g_criticalActorRegistered=false;
        if(mutation==6)g_personOther=other+1;
        if(mutation==7)skilltelemetry::invalidateContext();
        if(mutation==8)packet[0x78]^=1;
        if(mutation==9)attackReachTargetChanged=true;
        if(mutation==10)++*(int*)(source+0xa8);
        assert((route?runCriticalQueued(target,source,packet):runCriticalCommit(target,source,packet))==-250 && *(uint16_t*)(target+0x18)==750);
        criticalTake(packet);g_personOther=other;
    }
    skilltelemetry::close();assert(g_criticalRandomCalls==0 && g_criticalRecords.empty());
    attackCoAggregateCall=site(0x586641);domesticStub(0x586646,attackCoAggregated);
    assert(skilltelemetry::open(L"build/attack_coeffect_aggregate_test.csv",personFixtureActor,nullptr));
    for(int tactic:{12,15})for(int provider=0;provider<4;++provider)for(int count:{0,1,2}){
        attackReachMutation=0;attackReachAllowed=true;attackReachTechnology=false;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        *(int*)(actor+0xe8)=provider==0?24:provider==1?50:provider==2?249:-1;
        *(int*)(other+0xe8)=provider==0?50:provider==3?249:-1;
        *(int*)(source+0xc)=600;*(int*)(source+0x10)=14162;*(int*)(source+0x14)=-1;
        *(int*)(source+0x48)=*(int*)(source+0xa8)=3;*(uint32_t*)(source+4)=0x10001;
        int distance=provider==0 || provider==2?3:2;
        *(uint32_t*)(target+4)=1|((1+distance)<<16);*(uint16_t*)(target+0x18)=1000;
        memset(attackTacticWeapon,0,sizeof attackTacticWeapon);attackTacticWeapon[0x78+tactic]=1;
        *(uint32_t*)(attackTacticModels[tactic]+0x38)=1;
        auto dst=map.data()+20*(201+distance);*(uint32_t*)dst=1;*(uint16_t*)(dst+8)=4094;*(uint32_t*)(dst+4)=5;
        uint8_t frame[0xa90]={};auto group=frame+0x488;
        {CriticalCopyScope owner;owner.destination=group;
            producer(group,source,tactic,*(uint32_t*)(target+4),0,0,1);*(int*)(group+0x600)=count;
            attackCoAggregate(group);
            assert(attackReachGrantMatches(criticalLookup(frame),source,frame,target,-250)==(count==1));
            assert(runCriticalQueued(target,source,frame)==-250 && *(uint16_t*)(target+0x18)==750);
        }
        assert(g_criticalRecords.empty());*(uint16_t*)(target+0x18)=1000;
        assert(runCriticalQueued(target,source,frame)==-250 && g_criticalRecords.empty());
    }
    skilltelemetry::close();assert(g_criticalRandomCalls==0);
    puts("PASS attack24/50 coeffects:4608 original-native three-way permissions,1056 attributed effects/192 shared writes, first/second/composed/high/duplicate providers, native caps and unchanged RNG/state");
    puts("PASS attack24/50 coeffects:16 native REP MOVSD copies retain dual and forest-only metadata; owner cleanup rejects identical expired packets");
    puts("PASS attack24/50 coeffects: actual586641 CALL and584510 aggregation retain eight single records;16 ambiguous/empty aggregates and24 expired frames excluded");
    puts("PASS attack24/50 coeffects:44 selective provider/member/identity/world/packet/owner/weapon cases retain only still-valid attributions");
}
