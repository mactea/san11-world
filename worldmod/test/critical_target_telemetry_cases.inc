// Actual unit/base troop and HP CALL instructions in the common record apply
// routine. World dispatch is controlled; outer native clamps and final native
// unit/city/gate/port/HP storage execute.
static uintptr_t g_criticalQueuedEntry;
static bool g_criticalQueuedHp=false,g_criticalBaseRegistered=true;
static uint8_t g_criticalBaseResource[0x248]={};
static int criticalFixtureResourceOffset(){return *(int*)(g_criticalTarget+8)==0?0x40:0x24;}
static int __fastcall criticalFixtureTargetTroops(void* target,void*) {
    return g_criticalBuildingTarget && target==g_criticalTarget?
        *(int*)(g_criticalBaseResource+criticalFixtureResourceOffset()):battleTroops((uint8_t*)target,nullptr);
}
static int __fastcall criticalFixtureBaseId(void*,void*,void* base) {
    if(base!=g_criticalTarget)return -1;
    int kind=*(int*)(g_criticalTarget+8);return kind==0?8:kind==1?45:kind==2?60:9000;
}
static void* __fastcall criticalFixtureBase(void*,void*,int id) {
    return g_criticalBaseRegistered && id==criticalFixtureBaseId(nullptr,nullptr,g_criticalTarget)?g_criticalTarget:nullptr;
}
static int __fastcall criticalFixtureBaseMax(void*,void*) {return *(int*)(g_criticalTarget+8)==0?100000:30000;}
static uint16_t __fastcall criticalFixtureHpMax(void*,void*) {return 1000;}
static int __cdecl criticalFixtureCityTech(void*,int kind) {assert(kind==0x26);return 0;}
static void* __fastcall criticalFixtureNoForce(void*,void*,int) {return nullptr;}
static void __fastcall criticalFixtureEmptyGarrison(void*,void*,int) {}
static void __fastcall criticalFixtureSetBaseTroops(void* base,void*,int value) {
    assert(base==g_criticalTarget);
    typedef void(__thiscall* Setter)(void*,int);
    ((Setter)site(*(int*)(g_criticalTarget+8)==0?0x47bcb0:0x483600))(g_criticalBaseResource,value);
}
__declspec(naked) static int __cdecl runCriticalQueued(void* target,void* source,void* packet) {
    __asm {push ebx
        push ebp
        push esi
        push edi
        mov esi,[esp+20]
        mov ebp,[esp+24]
        mov edi,[esp+28]
        mov eax,[edi+0xc]
        cmp byte ptr [g_criticalQueuedHp],0
        je troops
        mov eax,[edi+0x10]
troops: neg eax
        push eax
        push esi
        xor ecx,ecx
        jmp dword ptr [g_criticalQueuedEntry]}
}
__declspec(naked) static void criticalFixtureQueuedCommitted(){__asm {pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
static void criticalTargetTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,
                                         CriticalProducer producer,const int* effects) {
    auto oldQueued=g_criticalQueuedTroopsResume,oldTroopsResume=g_criticalBaseTroopsResume,oldHpResume=g_criticalBaseHpResume;
    auto oldTroops=g_criticalBaseTroopsOriginal,oldHp=g_criticalBaseHpOriginal;
    auto oldBaseId=g_telemetryBaseId;auto oldBase=g_baseById;
    auto vt=*(void***)target;void* oldValue=vt[0x50/4];vt[0x50/4]=criticalFixtureTargetTroops;
    *(void**)g_criticalBaseResource=vt;
    g_criticalQueuedTroopsResume=(uint32_t)site(0x5b1c71);g_criticalBaseTroopsResume=(uint32_t)site(0x5b1a79);
    g_criticalBaseHpResume=(uint32_t)site(0x5b1ab4);
    g_criticalBaseTroopsOriginal=(IncomeGrant)site(0x4ae3c0);g_criticalBaseHpOriginal=(IncomeGrant)site(0x4ae360);
    g_telemetryBaseId=(BuildingId)criticalFixtureBaseId;g_baseById=(BaseById)criticalFixtureBase;
    domesticStub(0x5b1c71,criticalFixtureQueuedCommitted);domesticStub(0x5b1a79,criticalFixtureQueuedCommitted);
    domesticStub(0x5b1ab4,criticalFixtureQueuedCommitted);domesticStub(0x486bc0,criticalFixtureBaseMax);
    domesticStub(0x487230,criticalFixtureSetBaseTroops);domesticStub(0x487150,criticalFixtureHpMax);
    domesticStub(0x4c0c30,criticalFixtureCityTech);domesticStub(0x490aa0,criticalFixtureNoForce);
    domesticStub(0x4875c0,criticalFixtureEmptyGarrison);
    assert(skilltelemetry::open(L"build/critical_targets_test.csv",personFixtureActor,nullptr));
    uint8_t packet[0x80]={};
    auto prepare=[&](int effect,int owned,bool building,int kind=0) {
        for(int n=0;n<19;++n)g_skills[249].effects[effects[n]]=false;g_skills[249].effects[17]=false;
        if(owned==249)g_skills[249].effects[effect]=true;
        *(int*)(actor+0xe8)=owned;*(int*)(other+0xe8)=-1;actor[0xc9]=other[0xc9]=building?150:100;
        g_criticalBuildingTarget=building;g_criticalBaseRegistered=true;
        *(int*)(target+8)=kind;*(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
        source[0xc5]=0;g_criticalRandomResult=0;
        *(int*)(source+0xa8)=effect==35 || effect==43?2:effect==36 || effect==44?3:
            effect==25 || effect==37 || effect==45?4:effect==46?8:effect==38 || effect==47?10:1;
        *(uint32_t*)(g_criticalCells+20*201+4)=effect==15?5:0;
        g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=true;
        int tactic=effect==15 || effect==17 || effect==23 || effect==25 || effect==40?-1:effect==46?14:effect==38 || effect==47?16:0;
        int calls=g_criticalRandomCalls;
        assert(producer(packet,source,tactic,*(uint32_t*)(target+4),0,1,0)==0x1234);
        assert(*(int*)packet==1 && g_criticalRandomCalls==calls && criticalLookup(packet).effect==effect);
    };
    g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;
    for(int n=0;n<19;++n)for(int owned:{effects[n],249})for(int before:{0,100,1000}) {
        prepare(effects[n],owned,false);*(uint16_t*)(target+0x18)=(uint16_t)before;
        assert(runCriticalQueued(target,source,packet)==-(before<250?before:250));
        assert(*(uint16_t*)(target+0x18)==(before<250?0:before-250));
    }
    int buildingEffects[]={0,17,23,25,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48};
    for(int kind:{0,1,2})for(int effect:buildingEffects)for(int owned:{effect,249})for(int before:{0,100,1000}) {
        prepare(effect,owned,true,kind);
        *(int*)(g_criticalBaseResource+criticalFixtureResourceOffset())=before;*(int16_t*)(target+0x10)=(int16_t)before;
        g_criticalQueuedEntry=site(0x5b1a74);g_criticalQueuedHp=false;
        assert(runCriticalQueued(target,source,packet)==-(before<250?before:250));
        assert(*(int*)(g_criticalBaseResource+criticalFixtureResourceOffset())==(before<250?0:before-250));
        g_criticalQueuedEntry=site(0x5b1aaf);g_criticalQueuedHp=true;
        assert(runCriticalQueued(target,source,packet)==-(before<125?before:125));
        assert(*(int16_t*)(target+0x10)==(before<125?0:before-125));
    }
    // Real zero-loss requests at over-cap native state still expose the actual
    // clamp/store, rather than inventing a no-critical marginal damage amount.
    for(int kind:{0,1,2}) {
        g_criticalFixtureTroopLoss=g_criticalFixtureHpLoss=0;prepare(17,17,true,kind);
        g_criticalFixtureTroopLoss=250;g_criticalFixtureHpLoss=125;
        *(int*)(g_criticalBaseResource+criticalFixtureResourceOffset())=criticalFixtureBaseMax(nullptr,nullptr)+1;
        *(int16_t*)(target+0x10)=1001;
        g_criticalQueuedEntry=site(0x5b1a74);g_criticalQueuedHp=false;assert(runCriticalQueued(target,source,packet)==-1);
        g_criticalQueuedEntry=site(0x5b1aaf);g_criticalQueuedHp=true;assert(runCriticalQueued(target,source,packet)==-1);
    }
    for(int effect:buildingEffects)for(int owned:{effect,249})for(int before:{0,100,1000}) {
        prepare(effect,owned,true,49);*(int16_t*)(target+0x10)=(int16_t)before;
        g_criticalQueuedEntry=site(0x5b1aaf);g_criticalQueuedHp=true;
        assert(runCriticalQueued(target,source,packet)==-(before<125?before:125));
    }
    // Mutations, unregistered bases/source/actor, ownership changes, wrong
    // coordinates and kind1 overrides do not authorize a new applied row.
    for(int test=0;test<8;++test) {
        prepare(17,17,true);*(int16_t*)(target+0x10)=1000;g_criticalQueuedEntry=site(0x5b1aaf);g_criticalQueuedHp=true;
        if(test==0)packet[0x10]++;
        if(test==1)g_criticalBaseRegistered=false;
        if(test==2)g_criticalSourceRegistered=false;
        if(test==3)g_criticalActorRegistered=false;
        if(test==4)*(int*)(actor+0xe8)=-1;
        if(test==5)++*(uint32_t*)(source+4);
        if(test==6)++*(uint32_t*)(target+4);
        if(test==7)*(int*)(packet+0x70)=1;
        runCriticalQueued(target,source,packet);
    }
    prepare(17,17,true);g_criticalBuildingTarget=false;
    producer(packet,source,-1,*(uint32_t*)(target+4),0,1,0);assert(!*(int*)packet && !criticalLookup(packet).actor);
    g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;*(uint16_t*)(target+0x18)=1000;
    assert(runCriticalQueued(target,source,packet)==-250); //17 requires a building target
    prepare(15,15,false);g_criticalBuildingTarget=true;
    producer(packet,source,-1,*(uint32_t*)(target+4),0,1,0);assert(!*(int*)packet && !criticalLookup(packet).actor);
    g_criticalQueuedEntry=site(0x5b1aaf);g_criticalQueuedHp=true;*(int16_t*)(target+0x10)=1000;
    assert(runCriticalQueued(target,source,packet)==-125); //15 forest fallback requires a unit target
    uint8_t replacement[0x110];
    for(bool building:{false,true}) {
        prepare(building?17:42,building?17:42,building);memcpy(replacement,target,sizeof replacement);
        g_criticalTarget=replacement; // same ID, position and registration, different object pointer
        if(building) {
            *(int16_t*)(replacement+0x10)=1000;g_criticalQueuedEntry=site(0x5b1aaf);g_criticalQueuedHp=true;
            assert(runCriticalQueued(replacement,source,packet)==-125 && *(int16_t*)(replacement+0x10)==875);
        } else {
            *(uint16_t*)(replacement+0x18)=1000;g_criticalQueuedEntry=site(0x5b1c6c);g_criticalQueuedHp=false;
            assert(runCriticalQueued(replacement,source,packet)==-250 && *(uint16_t*)(replacement+0x18)==750);
        }
        g_criticalTarget=target; // generation-time target identity must match the settled object
    }
    g_criticalSourceRegistered=g_criticalTargetRegistered=g_criticalActorRegistered=g_criticalBaseRegistered=true;
    g_criticalBuildingTarget=false;g_criticalQueuedHp=false;
    g_criticalQueuedTroopsResume=oldQueued;g_criticalBaseTroopsResume=oldTroopsResume;g_criticalBaseHpResume=oldHpResume;
    g_criticalBaseTroopsOriginal=oldTroops;g_criticalBaseHpOriginal=oldHp;g_telemetryBaseId=oldBaseId;g_baseById=oldBase;
    vt[0x50/4]=oldValue;skilltelemetry::close();g_criticalRecords.clear();
    puts("PASS critical targets: common queued unit troop grant;19 building critical effects including17; city/gate/port troop and HP native clamps/stores; stale target/provider exclusions");
}
static const uint8_t* g_criticalOwnedSource;
static int g_criticalOwnedCalls=0;
static int __fastcall criticalFixtureOwnedConsume(void* self,void*,const uint8_t* group,int mode) {
    assert(self==(void*)0x1234 && mode==0 && *(int*)(group+0x600)==1 && *(int*)(group+0x70)==2);
    assert(g_criticalCopyScope && g_criticalCopyScope->previous && g_criticalCopyScope->previous->destination==group);
    for(int n=0x80;n<0x600;++n)assert(!group[n]); // whole native0x181-dword zero, not a fixture memset
    auto i=g_criticalRecords.find((uintptr_t)group);
    assert(i!=g_criticalRecords.end() && i->second.actor && *(int*)(i->second.snapshot+0x70)==0);
    g_criticalOwnedSource=group;++g_criticalOwnedCalls;return 7;
}
static void criticalOwnerTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,const int* effects) {
    auto oldOwner=g_criticalOwnerResume,oldZero=g_criticalOwnerZeroResume;
    auto oldConsumer=g_criticalHandlerTwoOriginal;
    g_criticalOwnerResume=(uint32_t)site(0x592a16);g_criticalOwnerZeroResume=(uint32_t)site(0x592a2a);
    g_criticalHandlerTwoOriginal=(CriticalHandlerTwo)criticalFixtureOwnedConsume;
    for(int n=0;n<19;++n)g_skills[249].effects[effects[n]]=false;g_skills[249].effects[17]=false;
    *(int*)(actor+0xe8)=23;*(int*)(other+0xe8)=-1;*(int*)(source+0xa8)=1;actor[0xc9]=100;
    *(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
    g_criticalBuildingTarget=false;g_criticalOwnedCalls=0;
    assert(skilltelemetry::open(L"build/critical_owner_test.csv",personFixtureActor,nullptr));
    for(int n=0;n<32;++n) {
        assert(((CriticalOwner)site(0x592a10))((void*)0x1234,source,0x10002)==1);
        assert(!g_criticalCopyScope && g_criticalRecords.empty() && !criticalLookup(g_criticalOwnedSource).actor);
    }
    assert(g_criticalOwnedCalls==32);skilltelemetry::close();
    g_criticalOwnerResume=oldOwner;g_criticalOwnerZeroResume=oldZero;g_criticalHandlerTwoOriginal=oldConsumer;
    puts("PASS native critical source owner592a10: full frame/zero/count/kind2/return path;32 repeated calls retain pending source until child returns and reclaim all provenance on owner return");
}
