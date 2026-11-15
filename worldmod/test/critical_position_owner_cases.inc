// Execute complete579e40/57ad70 native source owners. Only map/rule data and
// UI/world primitives are adapted. Producer arithmetic and child world apply
// remain fixtures; native critical decisions and real troop grants execute.
static uint8_t g_criticalRootRules[20*256]={};
static uint32_t g_criticalRootSelected=4095;
static bool g_criticalRootSelectionValid=true,g_criticalRootNested=false;
static int g_criticalRootDepth=0,g_criticalRootCandidates=2,g_criticalRootConsumed=0;
static void __fastcall criticalRootUiSelect(void*,void*,int,int) {}
static void* __cdecl criticalRootUiObject(){return g_criticalSource;}
static int __cdecl criticalRootUiId(){return 0;}
static void __cdecl criticalRootUiUpdate(int,int) {}
static void* __fastcall criticalRootUiSelection(void*,void*) {return g_criticalRootSelectionValid?g_criticalSource:nullptr;}
static void* __fastcall criticalRootUnit(void*,void*,int id) {assert(id==4095);return g_criticalSource;}
static void* __fastcall criticalRootCellObject(void*,void*) {return g_criticalTarget;}
static int __fastcall criticalRootCandidates(void*,void*,uint32_t* output,void*,void*) {
    // Duplicate positions deliberately exercise two distinct packet slots; no
    // claim is made about the native world candidate-selection algorithm.
    for(int n=0;n<g_criticalRootCandidates;++n)output[n]=0x10002;
    return g_criticalRootCandidates;
}
static const uint32_t* __fastcall criticalRootAttackPosition(void*,void*,uint32_t* output,uint32_t,uint32_t) {
    *output=0x10002;return output;
}
static int __fastcall criticalRootConsume(void*,void*,const uint8_t* group) {
    assert(g_criticalCopyScope && g_criticalCopyScope->destination==group);
    int count=*(int*)(group+0x600);assert(count>=0 && count<=2);
    for(int n=0;n<count;++n)assert(criticalLookup(group+n*0x80).actor);
    ++g_criticalRootDepth;++g_criticalRootConsumed;
    if(g_criticalRootNested && g_criticalRootDepth==1 && count==1 && !g_criticalBuildingTarget) {
        uint8_t child[0x20]={};
        assert(((CriticalPositionOwner)site(0x57ad70))(child,0x10002)==1);
        assert(g_criticalCopyScope->destination==group && criticalLookup(group).actor);
    }
    if(!g_criticalBuildingTarget)for(int n=0;n<count;++n)
        assert(runCriticalCommit(g_criticalTarget,g_criticalSource,(void*)(group+n*0x80))==-250);
    --g_criticalRootDepth;return 7;
}
static void criticalPositionOwnerTelemetryTests(uint8_t* source,uint8_t* target,uint8_t* actor,uint8_t* other,const int* effects) {
    auto oldOne=g_criticalPositionOneResume,oldTwo=g_criticalPositionTwoResume;
    g_criticalPositionOneResume=(uint32_t)site(0x579e46);g_criticalPositionTwoResume=(uint32_t)site(0x57ad76);
    struct Operand {uint32_t at,original,replacement;};
    Operand operands[]={
        {0x579e65,0x6fb0e68,(uint32_t)g_criticalCells},{0x579e6c,0x6fb0e68,(uint32_t)g_criticalCells},
        {0x579e8d,0x96a85d0,(uint32_t)g_criticalRootRules},{0x579ec2,0x96a59d0,(uint32_t)&g_criticalRootSelected},
        {0x579ef4,0x8b9608,(uint32_t)&g_criticalRootSelected},{0x579f83,0x96a59d0,(uint32_t)&g_criticalRootSelected},
        {0x579ff1,0x6fb0e70,(uint32_t)g_criticalCells+8},{0x57ad98,0x6fb0e68,(uint32_t)g_criticalCells},
        {0x57ad9f,0x6fb0e68,(uint32_t)g_criticalCells},{0x57adc1,0x96a85dc,(uint32_t)g_criticalRootRules+12},
        {0x57adea,0x96a59d0,(uint32_t)&g_criticalRootSelected},{0x57ae27,0x8b9608,(uint32_t)&g_criticalRootSelected},
        {0x57ae7c,0x8b9608,(uint32_t)&g_criticalRootSelected}};
    for(auto& operand:operands){assert(*(uint32_t*)site(operand.at)==operand.original);*(uint32_t*)site(operand.at)=operand.replacement;}
    domesticStub(0x574940,criticalRootUiSelect);domesticStub(0x632a10,criticalRootUiObject);
    domesticStub(0x573120,criticalRootUiId);domesticStub(0x5789e0,criticalRootUiUpdate);
    domesticStub(0x56dba0,criticalRootUiSelection);domesticStub(0x490e70,criticalRootUnit);
    domesticStub(0x483b40,criticalRootCellObject);domesticStub(0x5a4340,criticalRootCandidates);
    domesticStub(0x5a4210,criticalRootAttackPosition);
    domesticStub(0x588580,criticalRootConsume);domesticStub(0x5885d0,criticalRootConsume);
    uint8_t action[0x20]={};uint8_t* cell=g_criticalCells+20*401; // native x*200+y, position(2,1)
    auto prepare=[&](bool building) {
        for(int n=0;n<19;++n)g_skills[249].effects[effects[n]]=false;g_skills[249].effects[17]=false;
        g_skills[249].effects[building?17:23]=true;if(!building)g_skills[249].effects[42]=true;
        *(int*)(actor+0xe8)=249;*(int*)(other+0xe8)=-1;actor[0xc9]=100;
        *(int*)(source+0xa8)=1;source[0xc5]=0;*(uint32_t*)(source+4)=0x10001;*(uint32_t*)(target+4)=0x10002;
        *(uint16_t*)(target+0x18)=1000;*(uint32_t*)cell=0x200+(building?2:1);*(uint16_t*)(cell+8)=4095;
        *(uint32_t*)(g_criticalRootRules+20+4)=0x21;*(uint32_t*)(g_criticalRootRules+20+12)=0xffffffff;
        g_criticalBuildingTarget=building;g_criticalRootSelectionValid=true;*(int*)(action+0x1c)=0;
        g_criticalRootNested=false;g_criticalRootCandidates=2;
    };
    g_criticalRootDepth=g_criticalRootConsumed=0;
    assert(skilltelemetry::open(L"build/critical_position_owners_test.csv",personFixtureActor,nullptr));
    for(uint32_t entry:{0x579e40u,0x57ad70u})for(bool building:{false,true})for(int n=0;n<16;++n) {
        prepare(building);g_criticalRootNested=entry==0x579e40 && !building;
        int before=g_criticalRandomCalls;assert(((CriticalPositionOwner)site(entry))(action,0x10002)==1);
        assert(g_criticalRecords.empty() && !g_criticalCopyScope && !g_criticalRootDepth && g_criticalRandomCalls==before);
    }
    for(int n=0;n<8;++n) {
        prepare(false);*(int*)(action+0x1c)=1;
        assert(((CriticalPositionOwner)site(0x579e40))(action,0x10002)==1);
        assert(g_criticalRecords.empty() && !g_criticalCopyScope);
    }
    prepare(false);*(int*)(action+0x1c)=1;g_criticalRootCandidates=0;
    assert(((CriticalPositionOwner)site(0x579e40))(action,0x10002)==1 && g_criticalRecords.empty());
    for(uint32_t entry:{0x579e40u,0x57ad70u}) {
        prepare(false);*(uint32_t*)(g_criticalRootRules+20+4)=*(uint32_t*)(g_criticalRootRules+20+12)=0;
        assert(((CriticalPositionOwner)site(entry))(action,0x10002)==0 && g_criticalRecords.empty() && !g_criticalCopyScope);
        prepare(false);g_criticalRootSelectionValid=false;
        assert(((CriticalPositionOwner)site(entry))(action,0x10002)==1 && g_criticalRecords.empty() && !g_criticalCopyScope);
    }
    assert(g_criticalRootConsumed==89 && !g_criticalRootDepth);
    skilltelemetry::close();g_criticalBuildingTarget=false;
    g_criticalPositionOneResume=oldOne;g_criticalPositionTwoResume=oldTwo;
    for(const auto& operand:operands)*(uint32_t*)site(operand.at)=operand.original;
    puts("PASS native source owners579e40/57ad70: complete native bodies/returns, unit/base and multi-record branches, nested genuine troop commits, early exits and empty provenance after every owner return");
}
