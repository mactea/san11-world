// Execute the whole native calculator and construction/repair routine, including
// native seed save/restore, real HP clamp/word setter and completion flag write.
static uint8_t* g_constructionFixtureUnit;
static uint8_t* g_constructionFixtureBase;
static int g_constructionFixtureSeed=17,g_constructionSamples=0,g_constructionTroops=1000;
static uint16_t g_constructionFixtureMax=1000;
static bool g_constructionUnitRegistered=true,g_constructionBaseRegistered=true;
static int g_constructionFault=0,g_constructionUnitForce=2,g_constructionBaseForce=3;
static uint8_t g_constructionFacilityDefinition[0xc0]={};
static void* __fastcall constructionFixtureFacility(void*,void*,int) {return g_constructionFacilityDefinition;}
static uintptr_t g_constructionRandomResume;
__declspec(naked) static void constructionFixtureNativeRandom() {__asm {mov ecx,[esp+4]
    cmp ecx,2
    jmp dword ptr [g_constructionRandomResume]}}
static uint16_t __cdecl constructionFixtureRandom(int bound) {
    assert(bound==10);++g_constructionSamples;
    return ((uint16_t(__cdecl*)(int))constructionFixtureNativeRandom)(bound);
}
static int __fastcall constructionFixtureTroops(void*,void*) {return g_constructionTroops;}
static const uint32_t* __fastcall constructionFixturePosition(uint8_t* unit,void*) {return (const uint32_t*)(unit+4);}
static int __fastcall constructionFixtureUnitOwner(void*,void*) {return g_constructionUnitForce;}
static int __fastcall constructionFixtureBaseOwner(void*,void*) {return g_constructionBaseForce;}
static uint16_t __fastcall constructionFixtureMax(void*,void*) {return g_constructionFixtureMax;}
static int __fastcall constructionFixtureUnitId(void*,void*,void* unit) {return unit==g_constructionFixtureUnit?4095:-1;}
static void* __fastcall constructionFixtureUnit(void*,void*,int id) {return id==4095 && g_constructionUnitRegistered?g_constructionFixtureUnit:nullptr;}
static int __fastcall constructionFixtureBaseId(void*,void*,void* base) {return base==g_constructionFixtureBase?9000:-1;}
static void* __fastcall constructionFixtureBase(void*,void*,int id) {return id==9000 && g_constructionBaseRegistered?g_constructionFixtureBase:nullptr;}
static int __fastcall constructionFixtureFind(uint8_t* unit,void*,int effect) {
    for(int i=0;i<3;++i){int id=*(int*)(unit+0xc+4*i);auto p=personFixtureLookup(nullptr,nullptr,id);
        if(p && personHas(p,nullptr,effect))return id;}
    return -1;
}
static void __cdecl constructionFixtureRefresh(void*) {}
static int __fastcall constructionFixtureWorldChange(void* self,void*,void* base,int requested){
    if(g_constructionFault==14)skilltelemetry::invalidateContext();
    int result=((IncomeGrant)site(0x4ae360))(self,base,requested);
    if(g_constructionFault==1)skilltelemetry::invalidateContext();
    if(g_constructionFault==2)*(int*)(g_personGuide+0xe8)=-1;
    if(g_constructionFault==3)*(int*)(g_constructionFixtureUnit+0xc)=-1;
    if(g_constructionFault==4)g_constructionUnitRegistered=false;
    if(g_constructionFault==5)g_constructionBaseRegistered=false;
    if(g_constructionFault==6)++*(int*)(g_personGuide+0x94);
    if(g_constructionFault==7)++*(int*)(g_personGuide+0xa4);
    if(g_constructionFault==8)++*(int*)(g_constructionFixtureUnit+0xa8);
    if(g_constructionFault==9)++*(int*)(g_constructionFixtureUnit+8);
    if(g_constructionFault==10)++*(uint32_t*)(g_constructionFixtureUnit+4);
    if(g_constructionFault==11)g_constructionUnitForce=4;
    if(g_constructionFault==12)++*(int*)((uint8_t*)base+8);
    if(g_constructionFault==13)g_constructionBaseForce=4;
    if(g_constructionFault==15)*(int*)(g_personGuide+0xe8)=249;
    if(g_constructionFault==18)g_personGuide=g_personSpouse;
    return result;
}
static int __cdecl constructionFixtureCalcWorldChange(void* unit){
    int result=((ConstructionAmount)site(0x5a3520))(unit);skilltelemetry::invalidateContext();return result;
}
static void constructionTelemetryTests() {
    uint8_t unit[0x110]={},base[0x40]={},actor[400]={},other[400]={},person[400]={},spouse[400]={};
    g_personGuide=actor;g_personOther=other;g_personTarget=person;g_personSpouse=spouse;
    g_constructionFixtureUnit=unit;g_constructionFixtureBase=base;
    void* unitVt[24]={};unitVt[0x3c/4]=constructionFixturePosition;unitVt[0x40/4]=constructionFixtureUnitOwner;
    unitVt[0x50/4]=constructionFixtureTroops;unitVt[0x58/4]=(void*)site(0x47a8a0);unitVt[0x5c/4]=constructionFixtureFind;
    void* baseVt[24]={};baseVt[0x40/4]=constructionFixtureBaseOwner;*(void**)unit=unitVt;*(void**)base=baseVt;
    *(int*)(unit+0xc)=600;*(int*)(unit+0x10)=14162;*(int*)(unit+0x14)=-1;*(int*)(base+8)=49;
    auto oldAmount=g_constructionAmountOriginal;auto oldGrant=g_constructionGrantOriginal;auto oldMax=g_constructionMax;
    auto oldPerson=g_telemetryPerson;auto oldPersonId=g_telemetryPersonId;
    auto oldUnitId=g_telemetryUnitId;auto oldUnit=g_movementUnitById;auto oldBaseId=g_telemetryBaseId;auto oldBase=g_baseById;auto oldValid=g_growthValid;
    auto oldHas=g_constructionHasResume;g_constructionHasResume=(uint32_t)site(0x5a35dc);
    g_constructionAmountOriginal=(ConstructionAmount)site(0x5a3520);g_constructionGrantOriginal=(IncomeGrant)site(0x4ae360);
    g_constructionMax=(ConstructionMax)constructionFixtureMax;g_telemetryUnitId=(BuildingId)constructionFixtureUnitId;
    g_movementUnitById=(MovementUnitById)constructionFixtureUnit;g_telemetryBaseId=(BuildingId)constructionFixtureBaseId;
    g_baseById=(BaseById)constructionFixtureBase;g_growthValid=domesticValid;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)personFixtureId;
    uint8_t savedRandomCode[3][5];uint32_t randomEntries[]={0x472130,0x472140,0x472150};
    for(int i=0;i<3;++i)memcpy(savedRandomCode[i],(void*)site(randomEntries[i]),5);
    memcpy((void*)site(0x472130),"\x8b\x44\x24\x04\xa3",5);
    memcpy((void*)site(0x472140),"\xa1\x44\x5d\x8a\x00",5);
    uint32_t randomOperands[]={0x472135,0x472141,0x47215f,0x47216f},savedRandomOperands[4];
    for(int i=0;i<4;++i){auto p=(uint32_t*)site(randomOperands[i]);savedRandomOperands[i]=*p;*p=(uint32_t)&g_constructionFixtureSeed;}
    g_constructionRandomResume=site(0x472157);
    domesticStub(0x472150,constructionFixtureRandom);domesticStub(0x707a74,domesticTruncate);
    domesticStub(0x47a630,domesticValid);domesticStub(0x487150,constructionFixtureMax);domesticStub(0x59fdd0,constructionFixtureRefresh);
    domesticStub(0x490b90,constructionFixtureFacility);
    uint32_t operands[]={0x5a354d,0x5a3572,0x5a359b,0x5a35a7},saved[4];
    for(int i=0;i<4;++i){auto p=(uint32_t*)site(operands[i]);saved[i]=*p;*p=(uint32_t)site(*p);}
    bool old77=g_skills[249].effects[77];g_skills[249].effects[77]=true;
    typedef int(__cdecl* Build)(void*,void*,int);auto build=(Build)site(0x5850a0);
    struct AmountCase {int troops,politics,normal,samples;};AmountCase amounts[]={{1,1,1,1},{1000,50,88,1},{10000,100,405,2}};
    *(int*)(actor+0xe8)=*(int*)(other+0xe8)=-1;
    for(auto a:amounts)for(int before:{0,900,990,1000})for(int completed:{0,1}) {
        g_constructionTroops=a.troops;unit[0xcb]=(uint8_t)a.politics;
        *(int16_t*)(base+0x10)=(int16_t)before;*(int*)(base+0x14)=completed;int calls=g_constructionSamples;
        int after=before+a.normal;if(after>1000)after=1000;
        assert(build(base,unit,0)==(!completed && after==1000));
        assert(*(int16_t*)(base+0x10)==after && g_constructionSamples==calls+a.samples && g_constructionFixtureSeed==17);
    }
    assert(skilltelemetry::open(L"build/construction_telemetry_test.csv",personFixtureActor,nullptr));
    for(int mode=0;mode<4;++mode)for(auto a:amounts)for(int completed:{0,1})for(int before:{0,900,990,1000}) {
        *(int*)(actor+0xe8)=mode==2?-1:mode==1?249:77;*(int*)(other+0xe8)=mode>=2?77:-1;
        g_constructionTroops=a.troops;unit[0xcb]=(uint8_t)a.politics;
        *(int16_t*)(base+0x10)=(int16_t)before;*(int*)(base+0x14)=completed;int calls=g_constructionSamples;
        int after=before+2*a.normal;if(after>1000)after=1000;
        assert(build(base,unit,0)==(!completed && after==1000));
        assert(*(int16_t*)(base+0x10)==after && *(int*)(base+0x14)==(completed || after==1000));
        assert(g_constructionSamples==calls+a.samples && g_constructionFixtureSeed==17);
        assert(!g_constructionProbe && !g_constructionPending.actor.person && !g_progressionProbe);
    }
    struct WordCase {uint16_t maximum;int before,normal,after;};
    WordCase words[]={{0,0,0,0},{32768,0,-32768,-32768},{65535,0,-1,-1},{32768,-32768,0,0},{1000,-1,87,175},{65535,32760,0,0}};
    g_constructionTroops=1000;unit[0xcb]=50;
    for(auto c:words) {
        g_constructionFixtureMax=c.maximum;*(int*)(actor+0xe8)=*(int*)(other+0xe8)=-1;
        *(int16_t*)(base+0x10)=(int16_t)c.before;*(int*)(base+0x14)=1;
        build(base,unit,0);assert(*(int16_t*)(base+0x10)==c.normal); // independent native no-skill baseline
        *(int*)(actor+0xe8)=77;*(int16_t*)(base+0x10)=(int16_t)c.before;
        build(base,unit,0);assert(*(int16_t*)(base+0x10)==c.after);
    }
    g_constructionFixtureMax=1000;*(int16_t*)(base+0x10)=0;*(int*)(base+0x14)=0;
    int calls=g_constructionSamples;assert(g_constructionAmountOriginal(unit)==176 && g_constructionSamples==calls+1);
    assert(!g_constructionPending.actor.person); // preview cannot authorize a following unscoped grant
    constructionGrant(nullptr,nullptr,base,176);
    constructionAmount(unit);constructionGrant(nullptr,nullptr,base,175); // mismatched request
    g_constructionUnitRegistered=false;build(base,unit,0);g_constructionUnitRegistered=true;
    g_constructionBaseRegistered=false;build(base,unit,0);g_constructionBaseRegistered=true;
    skilltelemetry::close();assert(!g_constructionProbe && !g_constructionPending.actor.person && !g_progressionProbe);
    // Reproduce an actual native build whose grant crosses a world generation.
    // The real HP write remains176, but old-world provenance must not be emitted.
    assert(skilltelemetry::open(L"build/construction_world_test.csv",personFixtureActor,nullptr));
    *(int*)(actor+0xe8)=77;*(int*)(other+0xe8)=-1;*(int16_t*)(base+0x10)=0;*(int*)(base+0x14)=0;
    g_constructionGrantOriginal=(IncomeGrant)constructionFixtureWorldChange;
    g_constructionFault=1;
    assert(build(base,unit,0)==0 && *(int16_t*)(base+0x10)==176);skilltelemetry::close();
    FILE* worldLog=nullptr;assert(!_wfopen_s(&worldLog,L"build/construction_world_test.csv",L"rb"));
    int lines=0,byte;while((byte=fgetc(worldLog))!=EOF)if(byte=='\n')++lines;fclose(worldLog);
    assert(lines==2); // header and eligibility only
    assert(skilltelemetry::open(L"build/construction_owner_test.csv",personFixtureActor,nullptr));
    auto prepare=[&](int provider){
        g_personGuide=actor;*(int*)(actor+0xe8)=provider==1?-1:provider==2?249:77;*(int*)(other+0xe8)=provider==1?77:-1;
        memset(actor+0x94,0,20);memset(other+0x94,0,20);
        *(int*)(unit+0xc)=600;*(int*)(unit+0x10)=14162;*(int*)(unit+8)=0;*(int*)(unit+0xa8)=0;*(uint32_t*)(unit+4)=0x10001;
        *(int*)(base+8)=49;*(int16_t*)(base+0x10)=0;*(int*)(base+0x14)=0;
        g_constructionUnitRegistered=g_constructionBaseRegistered=true;g_constructionUnitForce=2;g_constructionBaseForce=3;
        g_constructionFault=0;g_constructionAmountOriginal=(ConstructionAmount)site(0x5a3520);
    };
    for(int provider:{0,1,2}){prepare(provider);assert(build(base,unit,0)==0 && *(int16_t*)(base+0x10)==176);}
    for(int fault=1;fault<=18;++fault){prepare(0);g_constructionFault=fault;
        if(fault==16){assert(constructionAmount(unit)==176);assert(constructionGrant(nullptr,nullptr,base,176)==176);}
        else{
            if(fault==17)g_constructionAmountOriginal=constructionFixtureCalcWorldChange;
            assert(build(base,unit,0)==0);
        }
        assert(*(int16_t*)(base+0x10)==176 && !g_constructionPending.actor.person && g_constructionFixtureSeed==17);
    }
    skilltelemetry::close();g_personGuide=actor;g_constructionFault=0;g_constructionUnitRegistered=g_constructionBaseRegistered=true;
    g_constructionUnitForce=2;g_constructionBaseForce=3;
    g_skills[249].effects[77]=old77;g_constructionAmountOriginal=oldAmount;g_constructionGrantOriginal=oldGrant;g_constructionMax=oldMax;
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldPersonId;
    g_telemetryUnitId=oldUnitId;g_movementUnitById=oldUnit;g_telemetryBaseId=oldBaseId;g_baseById=oldBase;g_growthValid=oldValid;g_constructionHasResume=oldHas;
    for(int i=0;i<4;++i)*(uint32_t*)site(operands[i])=saved[i];
    for(int i=0;i<4;++i)*(uint32_t*)site(randomOperands[i])=savedRandomOperands[i];
    for(int i=0;i<3;++i)memcpy((void*)site(randomEntries[i]),savedRandomCode[i],5);
    puts("PASS native construction: whole build/repair routine and calculator,77 doubled same sample, HP clamp/word setter, completion flag, native seed restore and preview/temporary exclusions");
}
