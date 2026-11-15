// Native provider loop,10-percent roll,100-item owner scan, candidate-index
// calculation and actual item setter execute. Parent frame, list allocation,
// text/UI and world lookups are adapters; no live-world claim is made.
static uint8_t g_plunderItems[100][0x54];
static bool g_plunderItemRegistered=true,g_plunderForceChanged=false;
static int g_plunderFixtureScan=1,g_plunderFixtureGate=1,g_plunderMutation=0;
static uintptr_t g_plunderFixtureBegin;
static int __fastcall plunderFixtureItemId(void*,void*,void* item){
    for(int n=0;n<100;++n)if(item==g_plunderItems[n])return g_plunderItemRegistered?n:-1;return -1;
}
static void* __fastcall plunderFixtureItem(void*,void*,int id){return id>=0 && id<100?g_plunderItems[id]:nullptr;}
static int __fastcall plunderFixtureOwner(void* object,void*){
    if(g_plunderForceChanged && object==g_criticalSource)return 3;
    return object==g_criticalSource || object==g_personGuide || object==g_personOther?2:3;
}
static void __fastcall plunderFixtureMerge(void* list,void*,uint8_t* from){
    for(auto node=*(CaptureFixtureNode**)(from+4);node;node=node->next)captureFixtureAppend((uint8_t*)list,nullptr,node->person);
}
static const char* __fastcall plunderFixtureFormat(void*,void*,int,void*,void*,void*){
    auto s=g_defeat;if(!s)return "fixture";assert(s->plunder);auto actor=(uint8_t*)s->plunder;
    if(g_plunderMutation==1)*(int*)(actor+0xe8)=-1;
    if(g_plunderMutation==2)g_criticalSourceRegistered=false;
    if(g_plunderMutation==3)skillsTelemetryStream(1,0);
    if(g_plunderMutation==4)g_criticalActorRegistered=false;
    if(g_plunderMutation==5)*(int*)(g_criticalSource+0xc)=-1;
    if(g_plunderMutation==6)g_plunderForceChanged=true;
    if(g_plunderMutation==9)g_plunderItemRegistered=false;
    if(g_plunderMutation==10)*(int*)(g_plunderItems[0]+0x40)=600;
    if(g_plunderMutation==12)*(int*)(actor+0xe8)=249;
    return "fixture";
}
static void __cdecl plunderFixtureMessage(const char*,void*,int){}
static bool plunderFixtureActor(const void* person,skilltelemetry::Actor& actor){
    afterFixtureActor(person,actor);actor.ruler=*(int*)(g_captureFixtureForce[actor.force==2?0:1]+4);return true;
}
static int __fastcall plunderFixtureNoTransfer(void*,void*,void*,int,int){return 0x1357;}
static int __fastcall plunderFixtureTransfer(void* self,void*,void* item,int owner,int treasury){
    int result=((PlunderTransfer)site(0x4a0a40))(self,item,owner,treasury);
    if(g_plunderMutation==11)skillsTelemetryStream(1,0);
    if(g_plunderMutation==13)g_plunderItemRegistered=false;
    return result;
}
__declspec(naked) static int __fastcall plunderFixtureDefeat(void*,void*,void*,void*,int,int,int){__asm {
    push ebp
    mov ebp,esp
    sub esp,0x4ac
    push ebx
    push esi
    push edi
    xor ebx,ebx
    mov [esp+0x1c],ecx
    mov [esp+0x4a4],ebp
    mov eax,g_plunderFixtureScan
    mov [esp+0x10],eax
    mov eax,g_plunderFixtureGate
    mov [esp+0x498],eax
    mov [esp+0x49c],ebx
    mov esi,g_criticalTarget
    mov edi,g_criticalSource
    jmp dword ptr [g_plunderFixtureBegin]}}
__declspec(naked) static void plunderFixtureDone(){__asm {
    mov ebp,[esp+0x4a4]
    pop edi
    pop esi
    pop ebx
    mov esp,ebp
    pop ebp
    mov eax,0x21aabbcc
    ret 20}}
template<class Setup>
static void plunderTelemetryTests(Defeat run,Setup setup,uint8_t* source,uint8_t* target){
    auto oldDefeat=g_defeatOriginal;auto oldHas=g_plunderHasOriginal;auto oldRoll=g_plunderRollOriginal;
    auto oldTransfer=g_plunderTransferOriginal;auto oldItem=g_plunderItemById;auto oldId=g_plunderItemId;auto oldCombination=g_skills[249];
    auto vt=*(void***)source;auto oldOwner=vt[0x40/4];vt[0x40/4]=(void*)plunderFixtureOwner;
    auto guideVt=*(void**)g_personGuide,otherVt=*(void**)g_personOther;
    *(void**)g_personGuide=*(void**)g_personOther=vt;
    int oldRulers[2]={*(int*)(g_captureFixtureForce[0]+4),*(int*)(g_captureFixtureForce[1]+4)};
    *(int*)(g_captureFixtureForce[0]+4)=600;*(int*)(g_captureFixtureForce[1]+4)=1000;
    g_defeatOriginal=(Defeat)plunderFixtureDefeat;g_plunderHasOriginal=(PlunderHas)site(0x4890f0);
    g_plunderRollOriginal=(PlunderRoll)site(0x4721d0);g_plunderTransferOriginal=(PlunderTransfer)plunderFixtureTransfer;
    g_plunderItemById=(MovementUnitById)plunderFixtureItem;g_plunderItemId=(BuildingId)plunderFixtureItemId;
    g_plunderFixtureBegin=site(0x4be969);domesticStub(0x4bebdb,plunderFixtureDone);
    uint8_t rulerGetter[5];memcpy(rulerGetter,(void*)site(0x489d40),5);
    memcpy((void*)site(0x489d40),g_plunderLayouts[8].bytes,5); // undo earlier injury relative adapter
    domesticStub(0x4a6d50,captureFixtureVoid2);domesticStub(0x40b5c0,captureFixtureClear);domesticStub(0x40cf90,captureFixtureVoid);
    domesticStub(0x4a67b0,captureFixtureAppend);domesticStub(0x4bb8d0,plunderFixtureMerge);domesticStub(0x40b440,captureFixtureNext);
    domesticStub(0x490b30,plunderFixtureItem);domesticStub(0x4b9350,plunderFixtureFormat);domesticStub(0x4f6710,plunderFixtureMessage);
    uint32_t sampleAddresses[]={0x47215f,0x47216f},savedSamples[2];
    for(int n=0;n<2;++n){savedSamples[n]=*(uint32_t*)site(sampleAddresses[n]);*(uint32_t*)site(sampleAddresses[n])=(uint32_t)&g_defenseSeed;}
    g_skills[249].effects[21]=true;
    auto prepare=[&](int provider,int count,int items,int gate,int scan,int seed,int mutation){
        setup(4,0,count,0,0,0,1,seed);g_plunderFixtureGate=gate;g_plunderFixtureScan=scan;g_plunderMutation=mutation;
        g_plunderItemRegistered=true;g_plunderForceChanged=false;g_plunderTransferOriginal=(PlunderTransfer)plunderFixtureTransfer;
        *(int*)(g_personGuide+0xe8)=*(int*)(g_personOther+0xe8)=-1;
        if(provider==0 || provider==2 || provider>=4 && provider!=6)*(int*)(g_personGuide+0xe8)=provider==2 || provider==7?249:21;
        if(provider==1 || provider==3 || provider==4 || provider==5 || provider==7)*(int*)(g_personOther+0xe8)=provider==3 || provider==5?249:21;
        if(provider==7)*(int*)(source+0x14)=600;
        for(int n=0;n<100;++n){memset(g_plunderItems[n],0,0x54);*(int*)(g_plunderItems[n]+0x40)=n<items?(n%2?601:1000):-1;
            *(int*)(g_plunderItems[n]+0x44)=-1;}
        *(int*)(g_plunderItems[99]+0x40)=600; // outside defeated members: never a candidate
        if(mutation==7)g_plunderTransferOriginal=(PlunderTransfer)plunderFixtureNoTransfer;
    };
    assert(skilltelemetry::open(L"build/plunder_test.csv",plunderFixtureActor,nullptr));int commands=0;
    for(int provider=0;provider<8;++provider)for(int count:{1,2})for(int items=0;items<=4;++items)
        for(int gate:{0,1})for(int scan:{0,1})for(int seed:{0,1,2,3,4,9,11,17}){
            prepare(provider,count,items,gate,scan,seed,0);uint32_t state=seed;int selected=-1;
            if(provider!=6 && gate && scan){state=state*0x6c078965u+0x3039u;
                if((state>>16)%100<10){int candidates[4],size=0;
                    for(int slot=0;slot<count;++slot)for(int n=0;n<items;++n)if(n%2==slot)candidates[size++]=n;
                    if(size){if(size>=2)state=state*0x6c078965u+0x3039u;selected=candidates[size>=2?(state>>16)%size:0];}
                }
            }
            assert(run(nullptr,target,source,0,0,0)==(int)0x21aabbcc);
            assert(g_defenseSeed==state && !g_defeat);
            for(int n=0;n<items;++n)assert(*(int*)(g_plunderItems[n]+0x40)==(n==selected?600:n%2?601:1000));
            assert(*(int*)(g_plunderItems[99]+0x40)==600);++commands;
        }
    skilltelemetry::close();assert(skilltelemetry::open(L"build/plunder_edges_test.csv",plunderFixtureActor,nullptr));
    for(int mutation=1;mutation<=13;++mutation){
        prepare(0,2,1,1,1,0,mutation);int ruler=mutation==8?14613:600;*(int*)(g_captureFixtureForce[0]+4)=ruler;
        run(nullptr,target,source,0,0,0);assert(g_defenseSeed==0x3039 && !g_defeat);
        assert(*(int*)(g_plunderItems[0]+0x40)==(mutation==7?1000:mutation==8?-1:ruler));
    }
    // Actual expanded recipient uses the installed M10 person bound, not1099.
    prepare(0,2,1,1,1,0,0);*(int*)(g_captureFixtureForce[0]+4)=14162;
    auto oldBound=*(int*)site(0x484df2);*(int*)site(0x484df2)=14612;
    run(nullptr,target,source,0,0,0);assert(*(int*)(g_plunderItems[0]+0x40)==14162);*(int*)site(0x484df2)=oldBound;
    prepare(0,2,1,1,1,0,0);*(int*)(g_captureFixtureForce[0]+4)=600;
    plunderFixtureDefeat(nullptr,nullptr,target,source,0,0,0); // no owned defeat: query only
    assert(*(int*)(g_plunderItems[0]+0x40)==600 && g_defenseSeed==0x3039 && !g_defeat);
    skilltelemetry::close();prepare(4,2,4,1,1,0,0);*(int*)(g_captureFixtureForce[0]+4)=600;
    run(nullptr,target,source,0,0,0);assert(!g_defeat);
    uint32_t expected=0x3039u*0x6c078965u+0x3039u;assert(g_defenseSeed==expected);
    for(int n=0;n<2;++n){*(uint32_t*)site(sampleAddresses[n])=savedSamples[n];*(int*)(g_captureFixtureForce[n]+4)=oldRulers[n];}
    vt[0x40/4]=oldOwner;g_defeatOriginal=oldDefeat;g_plunderHasOriginal=oldHas;g_plunderRollOriginal=oldRoll;
    memcpy((void*)site(0x489d40),rulerGetter,5);
    *(void**)g_personGuide=guideVt;*(void**)g_personOther=otherVt;
    g_plunderTransferOriginal=oldTransfer;g_plunderItemById=oldItem;g_plunderItemId=oldId;g_skills[249]=oldCombination;
    printf("PASS native plunder21: %d commands, last matching provider/10-percent LCG/native item scan/index/actual owner store, M10 recipient and stale/no-write/disabled exclusions\n",commands);
}
