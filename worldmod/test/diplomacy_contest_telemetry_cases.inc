#include <thread>
static uintptr_t g_contestFactoryEntry=0,g_contestLeafEntry=0;
static uint8_t* g_contestController=nullptr,*g_contestOtherController=nullptr,*g_contestHolder=nullptr;
static void* g_contestActor=nullptr,*g_contestOpponent=nullptr,*g_contestVtable=nullptr;
static int g_contestKind=16,g_contestMode=0,g_contestFault=0,g_contestEngineCalls=0,g_contestResult=-1;
static bool g_contestParallel=false,g_contestNested=false;
static uint8_t g_contestEngineState[0x40]={};
__declspec(naked) static int __cdecl runContestFactory(void*){__asm {sub esp,8
    push ebx
    push esi
    push edi
    mov esi,[esp+24]
    xor ebx,ebx
    jmp dword ptr [g_contestFactoryEntry]}}
__declspec(naked) static int __cdecl runContestLeaf(void*,void*,void*){__asm {push ebp
    push ebx
    push esi
    push edi
    mov esi,[esp+20]
    mov eax,[esp+24]
    mov edx,[esp+28]
    sub esp,0x990
    mov [esp+0x1c],eax
    mov ebx,edx
    mov edi,edx
    jmp dword ptr [g_contestLeafEntry]}}
__declspec(naked) static void contestLeafDone(){__asm {cmp g_contestKind,20
    je third
    mov eax,[esp+0x974]
    jmp done
third:
    mov eax,[esp+0x97c]
done:
    add esp,0x990
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret}}
static int __cdecl contestEngine(const uintptr_t* packet){
    assert(packet[0]==(uintptr_t)g_contestActor && packet[1]==1000 && packet[2]==1 &&
        packet[3]==(uintptr_t)g_contestOpponent && packet[4]==1000 && packet[5]==0 && packet[6]==0 && packet[7]==25);
    ++g_contestEngineCalls;*(int*)(g_contestEngineState+0x18)=0;*(int*)(g_contestEngineState+0x1c)=g_contestMode==2;
    if(g_contestFault==16)g_personRegistered=false;
    if(g_contestFault==22)++*(int*)((uint8_t*)g_contestOpponent+0x9c);
    if(g_contestFault==23)skilltelemetry::invalidateContext();
    if(g_contestFault==24)*(int*)((uint8_t*)g_contestActor+0xe8)=-1;
    return g_contestMode==1;
}
static void __fastcall contestObjectNoop(void*,void*){}
static int __fastcall contestUpdateReady(void*,void*){return 1;}
static int __fastcall contestNoScreen(void*,void*,int a,int b){assert(a==800 && b==0);return 0;}
static int __fastcall contestInitialize(uint8_t* self,void*,void* person){
    assert(self==g_contestController && person==g_contestActor);
    if(g_contestFault==25 && !g_contestNested){
        g_contestNested=true;assert(((DiplomacyContestFactory)site(0x689dd0))(person,1,1)==self);
    }
    *(void**)(self+0x1c90)=person;*(void**)(self+0x1c94)=g_diplomacyFixtureForce;
    *(void**)(self+0x1c9c)=g_personSpouse;*(int*)(self+0x1c88)=g_contestFault!=19;
    return g_contestFault!=19;
}
static int __fastcall contestCallback(uint8_t* self,void*,int,int){
    assert(self==g_contestController && *(int*)(self+0x1c8c)==1);
    if(g_contestFault==12)*(int*)(self+0x1c8c)=2;
    if(g_contestFault==15)*(void**)self=nullptr;
    return runContestLeaf(g_contestFault==21?g_contestOtherController:self,g_contestActor,g_contestOpponent);
}
static void contestUI(){
    auto execute=[](){
        if(g_contestParallel)assert(!g_diplomacyExecution && !g_diplomacyInteractionScope);
        int initialized=runContestFactory(g_contestHolder);
        assert(initialized==(g_contestFault!=19));
        assert(*(void**)(g_contestHolder+0x201e4)==(initialized?g_contestController:nullptr));
        switch(g_contestFault){
        case 1:skilltelemetry::invalidateContext();break;
        case 2:*(int*)((uint8_t*)g_contestActor+0xe8)=-1;break;
        case 3:g_diplomacyActorRegistered=false;break;
        case 4:++*(int*)((uint8_t*)g_contestActor+0x9c);break;
        case 5:g_diplomacyForceRegistered=false;break;
        case 6:*(int*)(g_diplomacyFixtureForce+4)=1000;break;
        case 7:++*(int*)((uint8_t*)g_contestActor+0x13c);break;
        case 8:*(void**)(g_contestController+0x1c90)=g_personTarget;break;
        case 9:*(void**)(g_contestController+0x1c94)=nullptr;break;
        case 10:*(void**)(g_contestController+0x1c9c)=g_personTarget;break;
        case 11:*(int*)(g_contestController+0x1c88)=0;break;
        case 13:*(int*)(g_contestController+4)=0;break;
        case 14:*(int*)(g_contestController+8)=2;break;
        case 17:g_skills[249].effects[86]=false;break;
        case 18:diplomacyContestEnd(diplomacyContestSnapshot().token);break;
        case 20:assert(((DiplomacyContestFactory)site(0x689dd0))(g_contestActor,1,1)==g_contestController);break;
        }
        int calls=g_contestEngineCalls;
        g_contestResult=((int(__thiscall*)(void*))site(0x689eb0))(g_contestController);
        assert(g_contestResult==((g_contestFault==11 || g_contestFault==19)?2:g_contestMode));
        assert(g_contestEngineCalls==calls+!(g_contestFault==11 || g_contestFault==19));
        assert(*(int*)(g_contestController+0x1c8c)==0);
    };
    if(g_contestParallel){std::thread consumer(execute);consumer.join();}else execute();
}
template<typename Select> static void diplomacyContestTelemetryTests(void** command,uint8_t* actor,uint8_t* other,uint8_t* opponent,uint8_t* ruler,uint8_t* force,Select select){
    uint8_t controllers[3][0x1ca4]={};void* vtables[3][4]={};std::vector<uint8_t> holder(0x20200);
    g_contestHolder=holder.data();g_contestOpponent=opponent;
    void* oldActorVt=*(void**)actor,*oldOtherVt=*(void**)other,*oldOpponentVt=*(void**)opponent,*oldRulerVt=*(void**)ruler;
    void* actorVt[24]={};void* foreignVt[24]={};actorVt[0x48/4]=diplomacyFixturePlayer;actorVt[0x40/4]=personFixtureOwner;
    foreignVt[0x40/4]=personFixtureForeignOwner;*(void**)actor=*(void**)other=actorVt;*(void**)opponent=*(void**)ruler=foreignVt;
    int oldActorOrder=*(int*)(actor+0x13c),oldOtherOrder=*(int*)(other+0x13c);bool oldRegistered=g_personRegistered;
    auto oldFactory=g_diplomacyContestFactoryOriginal;auto oldResume=g_diplomacyContestFactoryResume;auto oldContest=g_diplomacyContestOriginal;
    g_diplomacyContestFactoryResume=(uint32_t)site(0x689dda);g_diplomacyContestOriginal=(DiplomacyContest)site(0x689d00);
    g_contestFactoryEntry=site(0x690549);
    uint32_t stubSites[]={0x68ac29,0x68ae13,0x68b976,0x68b815,0x68c084,0x68bf23,0x520680,0x4dd640,0x4dd9c0,0x67aad0,0x67a910};
    const void* replacements[]={contestLeafDone,contestLeafDone,contestLeafDone,contestLeafDone,contestLeafDone,contestLeafDone,
        contestEngine,contestObjectNoop,contestObjectNoop,contestUpdateReady,contestNoScreen};
    uint8_t savedStubs[11][5];for(int n=0;n<11;++n){memcpy(savedStubs[n],(void*)site(stubSites[n]),5);domesticStub(stubSites[n],replacements[n]);}
    uint32_t operandSites[]={0x69054a,0x690550,0x690556,0x689d7f,0x689d90,0x689d9c,0x689da2,0x68a76a,0x68b43a,0x68baca,0x689d4b},savedOperands[11];
    for(int n=0;n<11;++n)savedOperands[n]=*(uint32_t*)site(operandSites[n]);
    uint32_t table[7];for(int n=0;n<7;++n)table[n]=(uint32_t)site(*(uint32_t*)site(0x689db0+4*n));
    uint32_t values[]={(uint32_t)(g_diplomacyUIState+0x2c),(uint32_t)(g_diplomacyUIState+0x24),(uint32_t)(g_diplomacyUIState+0x28),(uint32_t)table,
        (uint32_t)controllers[0],(uint32_t)controllers[1],(uint32_t)controllers[2],(uint32_t)vtables[0],(uint32_t)vtables[1],(uint32_t)vtables[2],(uint32_t)g_contestEngineState};
    for(int n=0;n<11;++n)*(uint32_t*)site(operandSites[n])=values[n];
    for(int n=0;n<3;++n){vtables[n][0]=(void*)site(0x689eb0);vtables[n][2]=contestInitialize;vtables[n][3]=contestCallback;}
    auto setup=[&](int kind,int who,int owned,int mode,bool parallel){
        int family=kind==16?0:kind==18?1:2;g_contestKind=kind;g_contestMode=mode;g_contestParallel=parallel;g_contestFault=0;g_contestNested=false;
        g_contestController=controllers[family];g_contestOtherController=controllers[(family+1)%3];g_contestVtable=vtables[family];
        memset(g_contestController,0,0x1ca4);*(void**)g_contestController=g_contestVtable;
        g_contestLeafEntry=site(kind==16?0x68ac0a:kind==18?0x68b957:0x68c065);g_contestResult=-1;
        g_contestActor=who?(void*)other:(void*)actor;command[1]=g_contestActor;command[2]=force;*(int*)(force+4)=601;
        for(auto p:{actor,other,opponent,ruler}){memset(p+0x98,0,16);*(int*)(p+0xe8)=-1;}
        *(int*)((uint8_t*)g_contestActor+0xe8)=owned;*(int*)((uint8_t*)g_contestActor+0x13c)=kind;
        g_diplomacyActorRegistered=g_diplomacyForceRegistered=g_personRegistered=true;g_diplomacyFixtureForceId=249;
        g_diplomacyFixtureFault=g_diplomacyPreModalFault=0;g_skills[249].effects[86]=true;
        g_diplomacyDifficulty=g_diplomacyPlayer=g_diplomacyScore=0;g_diplomacySample=1;
        g_diplomacyInteractive=true;g_diplomacyUIResult=2;g_diplomacyCommandReturn=0;g_diplomacyContestUIFixture=contestUI;
    };
    auto execute=[&](int kind){assert(select(kind)((uint8_t*)command)==0 && g_diplomacyConsumed==2 && !g_diplomacyInteractionScope && !diplomacyContestSnapshot().token);};
    uint32_t newSites[]={0x689dd0,0x68ac10,0x68b95d,0x68c06b};uint8_t newBytes[4][10];
    for(int n=0;n<4;++n)memcpy(newBytes[n],(void*)site(newSites[n]),n?5:10);
    memcpy((void*)site(0x689dd0),"\x8b\x4c\x24\x04\x8b\x81\x3c\x01\x00\x00",10);
    for(int n=1;n<4;++n){*(uint8_t*)site(newSites[n])=0xe8;*(int32_t*)site(newSites[n]+1)=(int32_t)(site(0x689d00)-site(newSites[n])-5);}
    assert(skilltelemetry::open(L"build/diplomacy_contest_before_test.csv",personFixtureActor,nullptr));
    for(int kind:{16,18,20})for(int owned:{86,249})for(int mode:{0,1,2}){setup(kind,0,owned,mode,false);execute(kind);}
    skilltelemetry::close();for(int n=0;n<4;++n)memcpy((void*)site(newSites[n]),newBytes[n],n?5:10);
    assert(skilltelemetry::open(L"build/diplomacy_contest_test.csv",personFixtureActor,nullptr));
    for(int kind:{16,18,20})for(int who:{0,1})for(int owned:{86,249})for(int mode:{0,1,2})for(bool parallel:{false,true}){
        setup(kind,who,owned,mode,parallel);execute(kind);
    }
    for(int owned:{86,249})for(int fault=1;fault<=25;++fault){
        if(fault==17 && owned==86)continue;
        setup(16,0,owned,0,true);g_contestFault=fault;execute(16);
    }
    for(int kind:{16,18,20})for(int ordinary:{0,1}){
        setup(kind,0,ordinary?86:-1,0,false);
        if(ordinary){g_diplomacyScore=1000;g_diplomacyDifficulty=2;g_diplomacyPlayer=1;g_diplomacySample=0;}
        execute(kind);
    }
    for(int kind:{16,18,20})for(int mode:{0,1}){
        setup(kind,0,86,mode,false);select(kind);
        assert(((DiplomacyContest)site(0x689d00))(g_contestActor,g_contestOpponent)==mode && !diplomacyContestSnapshot().token);
    }
    for(int kind:{16,18,20}){
        setup(kind,0,86,0,false);select(kind);contestInitialize(g_contestController,nullptr,g_contestActor);
        *(int*)(g_contestController+4)=*(int*)(g_contestController+8)=1;
        assert(((int(__thiscall*)(void*))site(0x689eb0))(g_contestController)==0 && !diplomacyContestSnapshot().token);
    }
    skilltelemetry::close();
    for(int kind:{16,18,20})for(int owned:{86,249}){setup(kind,0,owned,owned==249?1:2,true);execute(kind);}
    g_diplomacyContestUIFixture=nullptr;
    for(int n=0;n<11;++n){memcpy((void*)site(stubSites[n]),savedStubs[n],5);*(uint32_t*)site(operandSites[n])=savedOperands[n];}
    g_diplomacyContestFactoryOriginal=oldFactory;g_diplomacyContestFactoryResume=oldResume;g_diplomacyContestOriginal=oldContest;
    *(void**)actor=oldActorVt;*(void**)other=oldOtherVt;*(void**)opponent=oldOpponentVt;*(void**)ruler=oldRulerVt;
    *(int*)(actor+0x13c)=oldActorOrder;*(int*)(other+0x13c)=oldOtherOrder;g_personRegistered=oldRegistered;
    command[1]=actor;command[2]=force;*(int*)(force+4)=14162;*(int*)(actor+0xe8)=249;g_skills[249].effects[86]=true;
    g_diplomacyInteractive=true;g_diplomacyUIResult=2;g_diplomacyCommandReturn=1;g_diplomacyFixtureFault=g_diplomacyPreModalFault=0;
    g_diplomacyActorRegistered=g_diplomacyForceRegistered=true;
    puts("PASS diplomacy contest: native factory/virtual updater/three controller CALLs/result stores and32-byte engine packet;72 same/other-thread positives,49 stale cases,15 unowned controls,18 old-gap inputs and six disabled controls");
}
