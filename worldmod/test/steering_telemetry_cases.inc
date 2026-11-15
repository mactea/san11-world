// The real 0x584acc CALL, complete 0x56f120 frame and 0x496370 position
// setter execute. World/person lookup and controller spatial-list work are adapters.
static uint8_t steeringUnit[0xf4],steeringPeople[3][400];
static int steeringCalls=0,steeringFault=0,steeringForce=2;
static int steeringSeaCalls=0;
static int __cdecl steeringSea(const uint8_t*,const uint8_t*,uint32_t,int cost){++steeringSeaCalls;return cost;}
static uint32_t* steeringCommand=nullptr;
static uint8_t* __fastcall steeringPerson(void*,void*,int id) {
    return id==600?steeringPeople[0]:id==14162?steeringPeople[1]:id==1000?steeringPeople[2]:nullptr;
}
static uint8_t* __fastcall steeringPersonM12(uint32_t,void*,int id) {return steeringPerson(nullptr,nullptr,id);}
static void* __fastcall steeringUnitLookup(void*,void*,int id){return id==17 && steeringFault!=4?steeringUnit:nullptr;}
static int __fastcall steeringUnitId(void*,void*,void* unit){return unit==steeringUnit?17:-1;}
static int __fastcall steeringOwner(void*,void*){return steeringForce;}
static bool steeringActor(const void* p,skilltelemetry::Actor& out){
    for(int n=0;n<3;++n)if(p==steeringPeople[n]){out.person=n==0?600:n==1?14162:1000;out.force=2;out.ruler=635;out.city=12;
        out.year=194;out.month=1;out.day=1;strcpy_s(out.name,"Steer");strcpy_s(out.rulerName,"Ruler");return true;}return false;
}
static bool steeringDescriptor(const uint8_t* unit,int terrain,SkillSteerCosts& out){
    return steeringDescribeWithPersons(unit,terrain,out,(PersonFn)steeringPersonM12);
}
static int __cdecl steeringNativeValid(void* p){return p==steeringUnit && steeringFault!=1;}
static void __fastcall steeringRemove(void*,void*,int id){assert(id==17);++steeringCalls;
    if(steeringFault==2)skilltelemetry::invalidateContext();
    if(steeringFault==3)steeringCommand[3]^=1;
    if(steeringFault==5)*(int*)(steeringPeople[0]+0xe8)=-1;
    if(steeringFault==6)*(int*)(steeringUnit+0xc)=-1;
    if(steeringFault==7)++steeringForce;
}
static void __fastcall steeringInsert(void*,void*,int id){assert(id==17);}
static uintptr_t steeringCall;
__declspec(naked) static void __cdecl runSteering(void*,uint32_t*,int,uint32_t,int){__asm {
    push esi
    mov ecx,[esp+8]
    mov esi,[esp+12]
    push [esp+24]
    push [esp+24]
    push [esp+24]
    jmp dword ptr [steeringCall]}}
__declspec(naked) static void steeringReturned(){__asm {pop esi
    ret}}
#include "zoc_telemetry_cases.inc"
#include "steering_entry_telemetry_cases.inc"
static void steeringTelemetryTests(uint8_t* code){
    auto at=[&](uint32_t a){return code+a-0x400000;};
    auto stub=[&](uint32_t a,const void* fn){auto p=at(a);p[0]=0xe9;*(int32_t*)(p+1)=(int32_t)((const uint8_t*)fn-p-5);};
    auto oldLookup=g_movementUnitById;auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryUnitId;
    auto oldMove=g_steeringMoveOriginal;auto oldMap=g_movementMapOperand;
    auto oldSea=g_seaCost;g_seaCost=steeringSea;
    g_movementUnitById=(MovementUnitById)steeringUnitLookup;g_telemetryPerson=(PersonById)steeringPerson;
    g_telemetryUnitId=(BuildingId)steeringUnitId;g_steeringMoveOriginal=(SteeringMove)at(0x56f120);
    skillsSetSteeringDescribe(steeringDescriptor);g_movementMapOperand=(uint32_t*)at(0x5a07aa);
    stub(0x47a630,steeringNativeValid);stub(0x56e4f0,steeringRemove);stub(0x56e450,steeringInsert);
    stub(0x584ad1,steeringReturned);steeringCall=(uintptr_t)at(0x584acc);
    *(uint32_t*)at(0x56f13c)=*(uint32_t*)at(0x56f16e)=(uint32_t)steeringUnit;
    uint32_t sentinel=0xffffffff;
    *(uint32_t*)at(0x496372)=(uint32_t)&sentinel;*(uint32_t*)at(0x496383)=(uint32_t)((uint8_t*)&sentinel+2);
    void* vt[17]={};vt[16]=(void*)steeringOwner;*(void**)steeringUnit=vt;
    std::vector<uint8_t> controller(18*16);*(uint16_t*)(controller.data()+17*16+8)=0;
    // Apply the actual M9 wrapper-array cave to the complete native move body.
    auto cave=(uint8_t*)VirtualAlloc(nullptr,17,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(cave);
    memcpy(cave,"\x81\xc6\x00\x00\x00\x00\x69\xc0\xf4\x00\x00\x00\xe9",13);
    *(uint32_t*)(cave+2)=(uint32_t)controller.data();*(int32_t*)(cave+13)=(int32_t)(at(0x56f13b)-cave-17);
    at(0x56f133)[0]=0xe9;*(int32_t*)at(0x56f134)=(int32_t)(cave-at(0x56f133)-5);memset(at(0x56f138),0x90,3);
    uint8_t ordinary[384];memset(ordinary,8,sizeof ordinary);memset(g_steer,7,sizeof g_steer);
    g_table=(uint32_t)ordinary;g_steerReady=true;
    uint32_t command[44]={};steeringCommand=command;
    g_skills[249].defined=true;g_skills[249].effects[5]=true;
    int inputs=0;
    assert(skilltelemetry::open(L"../build/steering_test.csv",steeringActor,nullptr));
    for(int layout=0;layout<3;++layout){int width=layout==0?200:layout==1?249:530,height=layout==0?200:layout==1?530:249;
        std::vector<uint8_t> map((size_t)width*height*20);
        *(int*)at(0x5a0783)=*(int*)at(0x4963a0)=height;
        *(int*)at(0x5a078f)=*(int*)at(0x5a079a)=*(int*)at(0x4963ac)=width;
        *(uint32_t*)at(0x5a07aa)=(uint32_t)map.data();
        int x=height-3,y=width-5;uint32_t from=x|(y<<16),to=x|((y+3)<<16);
        for(int provider=0;provider<6;++provider)for(int transport=0;transport<2;++transport)for(int weapon=9;weapon<=11;++weapon)
        for(int table=0;table<5;++table){++inputs;
            steeringFault=0;steeringForce=2;memset(command,0,sizeof command);command[0]=17;command[1]=from;command[2]=to;command[43]=3;
            for(int n=0;n<3;++n){command[3+n]=x|((y+n+1)<<16);*(uint32_t*)(map.data()+20*((size_t)x*width+y+n+1)+4)=n==0?7:n==1?0:8;}
            *(uint32_t*)(steeringUnit+0x3c)=from;*(int*)(steeringUnit+8)=transport;*(int*)(steeringUnit+0x50)=weapon;
            *(int*)(steeringUnit+0xc)=600;*(int*)(steeringUnit+0x10)=14162;*(int*)(steeringUnit+0x14)=1000;
            for(int n=0;n<3;++n)*(int*)(steeringPeople[n]+0xe8)=-1;
            if(provider==0 || provider==3)*(int*)(steeringPeople[0]+0xe8)=5;
            if(provider==1 || provider==3)*(int*)(steeringPeople[1]+0xe8)=5;
            if(provider==2)*(int*)(steeringPeople[2]+0xe8)=5;
            if(provider==4)*(int*)(steeringPeople[0]+0xe8)=249; // M12 does not use composed5
            for(int w=9;w<=11;++w)for(int t:{7,8}){int cost=8+2*(w-9)+(t==8);
                ordinary[w*32+t]=table==3?255:cost;g_steer[w*32+t]=table==1?cost:table==2?255:table==4?cost+2:cost-1;}
            // The observer must not run a gameplay cost callback. Preview input
            // description alone must not log, then a real move must store to.
            SkillSteerCosts costs;steeringDescriptor(steeringUnit,7,costs);
            typedef int(__thiscall* Ship)(void*);int ship=((Ship)at(0x495490))(steeringUnit);
            if(costs.actor)assert(costs.weapon==ship);
            uint8_t expected[0xf4];memcpy(expected,steeringUnit,sizeof expected);*(uint32_t*)(expected+0x3c)=to;
            int calls=steeringCalls;runSteering(controller.data(),command,17,to,1);
            assert(steeringCalls==calls+1 && !memcmp(expected,steeringUnit,sizeof expected));
            // Same command after already arriving is not a second movement.
            calls=steeringCalls;runSteering(controller.data(),command,17,to,1);assert(steeringCalls==calls+1);
        }
        assert(movementTerrain(0xffffffff)==-1 && movementTerrain((uint32_t)height)==-1 && movementTerrain((uint32_t)width<<16)==-1);
        ++*(int*)at(0x5a079a);assert(movementTerrain(from)==-1);--*(int*)at(0x5a079a);
    }
    skilltelemetry::close();
    puts("PASS steering: 540 native commits across raw first/deputy/high/duplicate/composed/absent providers, ships/transports, five tables and three maps");
    // Use a fresh map for stale/no-write/invalid-route exclusions.
    std::vector<uint8_t> map(200*200*20);*(int*)at(0x5a0783)=*(int*)at(0x5a078f)=*(int*)at(0x5a079a)=200;
    *(int*)at(0x4963a0)=*(int*)at(0x4963ac)=200;*(uint32_t*)at(0x5a07aa)=(uint32_t)map.data();
    assert(skilltelemetry::open(L"../build/steering_edges_test.csv",steeringActor,nullptr));
    for(int edge=0;edge<17;++edge){steeringFault=edge<=7?edge:0;steeringForce=2;
        memset(command,0,sizeof command);command[0]=17;command[1]=0x10002;command[2]=0x20002;command[3]=command[2];command[43]=1;
        *(uint32_t*)(steeringUnit+0x3c)=command[1];*(int*)(steeringUnit+8)=0;*(int*)(steeringUnit+0x50)=9;
        *(int*)(steeringUnit+0xc)=600;*(int*)(steeringUnit+0x10)=*(int*)(steeringUnit+0x14)=-1;*(int*)(steeringPeople[0]+0xe8)=5;
        *(uint32_t*)(map.data()+20*402+4)=7;ordinary[9*32+7]=8;g_steer[9*32+7]=7;
        if(edge==8)command[43]=0;if(edge==9)command[43]=41;if(edge==10)command[3]=0x30002;
        if(edge==11)command[1]=0x00020001;if(edge==12)skillsSetSteeringDescribe(nullptr);
        if(edge==13)g_steerReady=false;
        if(edge==14){command[43]=40;command[2]=0x290002;
            for(int n=0;n<40;++n){command[3+n]=2|((n+2)<<16);*(uint32_t*)(map.data()+20*(402+n)+4)=7;}}
        if(edge==15)*(int*)(steeringUnit+0x50)=12;
        if(edge==16){command[43]=2;command[3]=0x40002;command[4]=command[2];}
        runSteering(controller.data(),command,17,command[2],1);
        skillsSetSteeringDescribe(steeringDescriptor);g_steerReady=true;
    }
    skilltelemetry::close();steeringFault=0;
    *(uint32_t*)(steeringUnit+0x3c)=command[1];
    uint8_t expected[0xf4];memcpy(expected,steeringUnit,sizeof expected);*(uint32_t*)(expected+0x3c)=command[2];
    int calls=steeringCalls;runSteering(controller.data(),command,17,command[2],1);
    assert(steeringCalls==calls+1 && !memcmp(expected,steeringUnit,sizeof expected));
    assert(inputs==540);
    puts("PASS steering: 15 exclusions, one short and one 40-cell control, disabled state parity and expanded-map bounds");
    zocTelemetryTests(code,controller.data());
    steeringEntryTelemetryTests(code,controller.data());
    assert(steeringSeaCalls==0);VirtualFree(cave,0,MEM_RELEASE);g_seaCost=oldSea;
    skillsSetSteeringDescribe(nullptr);g_movementUnitById=oldLookup;g_telemetryPerson=oldPerson;g_telemetryUnitId=oldId;
    g_steeringMoveOriginal=oldMove;g_movementMapOperand=oldMap;g_steerReady=false;g_table=0;
}
