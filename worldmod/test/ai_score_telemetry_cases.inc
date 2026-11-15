#include "ai_score_mapping.inc"
struct AIScoreFixtureNode {AIScoreFixtureNode* next;AIScoreFixtureNode* previous;void* person;};
static uintptr_t g_aiScoreOwnerEntry=0;
static int g_aiScoreOwnerValue=0,g_aiScoreCleanupCalls=0;
static int __cdecl aiScoreFixturePresent(void* p){return p!=nullptr;}
static void* __fastcall aiScoreFixtureNext(void*,void*,AIScoreFixtureNode** next){
    auto node=*next;assert(node);*next=node->next;return &node->person;
}
static void __fastcall aiScoreFixtureCleanup(void*,void*){++g_aiScoreCleanupCalls;}
__declspec(naked) static int __cdecl runAIScoreOwner(void*,void*,int,int,int){__asm {push ebx
    push ebp
    push esi
    push edi
    sub esp,0x70
    mov ebp,[esp+0x84]
    mov eax,[esp+0x88]
    mov eax,[eax+4]
    mov [esp+0x1c],eax
    mov eax,[esp+0x8c]
    mov [esp+0x5c],eax
    mov eax,[esp+0x90]
    mov [esp+0x60],eax
    mov eax,[esp+0x94]
    mov [esp+0x64],eax
    mov dword ptr [esp+0x10],0
    jmp dword ptr [g_aiScoreOwnerEntry]}}
__declspec(naked) static void aiScoreOwnerAccepted(){__asm {mov eax,[esp+0x10]
    mov g_aiScoreOwnerValue,eax
    mov eax,1
    add esp,0x70
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
__declspec(naked) static void aiScoreOwnerRejected(){__asm {mov eax,[esp+0x10]
    mov g_aiScoreOwnerValue,eax
    xor eax,eax
    add esp,0x70
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret}}
static bool aiScoreFixtureListed(uint32_t at,int raw){
    auto values=(const int*)site(at);for(int n=0;values[n]>=0 && values[n]<100;++n)if(values[n]==raw)return true;return false;
}
static int aiScoreRoleExpected(int raw,int role,int flagA,int flagB){
    if(raw<0 || raw>=100 || ((role<0 || role>11) && !flagA && !flagB))return 0;
    uint32_t good=0,bad=0;
    if(flagB){good=0x7e7830;bad=0x7e7a30;}
    else if(flagA || (role>=9 && role<=11)){good=0x7e7810;bad=0x7e79d8;}
    else if(role==1){good=0x7e7748;bad=0x7e7850;}
    else if(role==2){good=0x7e7768;bad=0x7e7890;}
    else if(role==3){good=0x7e7794;bad=0x7e78d0;}
    else if(role==4){good=0x7e77b8;bad=0x7e7914;}
    else if(role>=5 && role<=8){good=0x7e77e0;bad=0x7e7950;}
    return good && aiScoreFixtureListed(good,raw)?4000:bad && aiScoreFixtureListed(bad,raw)?-8000:0;
}
static int aiScorePairExpected(int raw,const std::vector<void*>& members){
    if(raw<0 || raw>=100)return 0;
    int score=0;auto incompatible=(const int*)site(0x7e7460);
    for(auto person:members){
        if(!person)continue;int other=*(int*)((uint8_t*)person+0xe8);if(other<0 || other>=100)continue;
        if(raw==other)return -9000;
        for(int n=0;n<186;n+=2)if((incompatible[n]==raw && incompatible[n+1]==other) || (incompatible[n]==other && incompatible[n+1]==raw))return -9000;
        for(int n=0;n<75;n+=3)if((g_aiScorePairWeights[n]==raw && g_aiScorePairWeights[n+1]==other) ||
            (g_aiScorePairWeights[n]==other && g_aiScorePairWeights[n+1]==raw))score=std::max<int>(score,4*g_aiScorePairWeights[n+2]);
    }
    return score;
}
static void aiScoreTelemetryTests(){
    uint8_t candidate[400]={},first[400]={},second[400]={},third[400]={};
    auto oldTarget=g_personTarget,oldGuide=g_personGuide,oldOther=g_personOther,oldSpouse=g_personSpouse;
    g_personTarget=candidate;g_personGuide=first;g_personOther=second;g_personSpouse=third;g_personRegistered=true;
    auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryPersonId;auto oldRole=g_aiRoleScoreEntry,oldPair=g_aiPairScoreEntry,oldMember=g_aiPairMemberResume;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)personFixtureId;
    g_aiRoleScoreEntry=site(0x49eed0);g_aiPairScoreEntry=site(0x49f570);g_aiPairMemberResume=site(0x49f5e9);g_aiScoreOwnerEntry=site(0x49fbf9);
    uint32_t stubs[]={0x47a630,0x47bed0,0x47c100,0x49fc5b,0x49fc7a};uint8_t savedStubs[5][5];
    const void* replacements[]={aiScoreFixturePresent,aiScoreFixtureNext,aiScoreFixtureCleanup,aiScoreOwnerRejected,aiScoreOwnerAccepted};
    for(int n=0;n<5;++n){memcpy(savedStubs[n],(void*)site(stubs[n]),5);domesticStub(stubs[n],replacements[n]);}
    uint32_t operands[]={0x49d786,0x49d816,0x49d772,0x49d78b,0x49d791,0x49d797,0x49d79d,0x49d7a3,0x49d7a9,
        0x49d802,0x49d81b,0x49d821,0x49d827,0x49d82d,0x49d833,0x49d839,0x49d98c,0x49d9a8,0x49d89f,0x49d94c};uint32_t savedOperands[20];
    for(int n=0;n<20;++n){auto p=(uint32_t*)site(operands[n]);savedOperands[n]=*p;
        *p=n<18?(uint32_t)site(*p):(uint32_t)g_aiScorePairWeights+(n==18?8:308);}
    uint32_t jumps[22];for(int n=0;n<22;++n){auto p=(uint32_t*)site(n<11?0x49d7b0+n*4:0x49d840+(n-11)*4);jumps[n]=*p;*p=(uint32_t)site(*p);}
    void* vt[24]={};vt[0x40/4]=personFixtureOwner;for(auto p:{candidate,first,second,third})*(void**)p=vt;
    uint8_t list[32]={};AIScoreFixtureNode nodes[3]={};std::vector<void*> members;
    auto setup=[&](uint8_t* person,int raw,const std::vector<int>& owned){
        for(auto p:{candidate,first,second,third}){*(int*)(p+0xe8)=-1;memset(p+0x98,0,16);}
        *(int*)(person+0xe8)=raw;memset(list,0,sizeof list);memset(nodes,0,sizeof nodes);members.clear();
        uint8_t* people[]={first,second,third};assert(owned.size()<=3);
        for(size_t n=0;n<owned.size();++n){*(int*)(people[n]+0xe8)=owned[n];members.push_back(people[n]);nodes[n].person=people[n];if(n)nodes[n-1].next=&nodes[n];}
        if(!owned.empty())*(void**)(list+4)=nodes;g_aiScoreCleanupCalls=0;
    };
    auto run=[&](uint8_t* person,int raw,int role,int flagA,int flagB){
        int pair=aiScorePairExpected(raw,members),roleScore=aiScoreRoleExpected(raw,role,flagA,flagB);
        bool eligible=raw>=0 && raw<100,accepted=!eligible || (pair>=0 && pair+roleScore>=0);
        int expected=!eligible?0:pair<0?pair:pair+roleScore;
        assert(runAIScoreOwner(person,list,role,flagA,flagB)==int(accepted));
        if(g_aiScoreOwnerValue!=expected || g_aiScoreCleanupCalls!=int(!accepted))fprintf(stderr,"AI score fixture raw=%d role=%d flags=%d,%d pair=%d roleScore=%d actual=%d expected=%d cleanups=%d accepted=%d\n",raw,role,flagA,flagB,pair,roleScore,g_aiScoreOwnerValue,expected,g_aiScoreCleanupCalls,int(accepted));
        assert(g_aiScoreOwnerValue==expected && g_aiScoreCleanupCalls==int(!accepted));assert(!g_aiPairMembers);
    };
    assert(*(int*)site(0x7e7748)==0 && *(int*)site(0x7e7460)==34);
    assert(skilltelemetry::open(L"build/ai_role_score_test.csv",personFixtureActor,nullptr));
    for(auto person:{candidate,second})for(int raw=0;raw<100;++raw)for(int role=-1;role<=12;++role)for(int flagA:{0,1})for(int flagB:{0,1}){
        setup(person,raw,{});run(person,raw,role,flagA,flagB);
    }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/ai_pair_score_test.csv",personFixtureActor,nullptr));
    for(int raw=0;raw<100;++raw)for(int other=0;other<100;++other){setup(candidate,raw,{other});run(candidate,raw,0,0,0);}
    for(int raw:{-1,100,249}){setup(candidate,raw,{49,45,37});run(candidate,raw,1,0,0);}
    struct Input {int raw,a,b,c;};
    for(auto input:{Input{49,37,45,40},Input{49,-1,249,45},Input{49,45,49,37},Input{0,34,-1,249},Input{50,-1,249,-1}}){
        setup(candidate,input.raw,{input.a,input.b,input.c});run(candidate,input.raw,1,0,0);
    }
    skilltelemetry::close();for(int raw:{0,49,-1,249}){setup(candidate,raw,{45});run(candidate,raw,1,1,1);}
    for(int n=0;n<5;++n)memcpy((void*)site(stubs[n]),savedStubs[n],5);
    for(int n=0;n<20;++n)*(uint32_t*)site(operands[n])=savedOperands[n];
    for(int n=0;n<22;++n)*(uint32_t*)site(n<11?0x49d7b0+n*4:0x49d840+(n-11)*4)=jumps[n];
    g_telemetryPerson=oldPerson;g_telemetryPersonId=oldId;g_aiRoleScoreEntry=oldRole;g_aiPairScoreEntry=oldPair;g_aiPairMemberResume=oldMember;
    g_personTarget=oldTarget;g_personGuide=oldGuide;g_personOther=oldOther;g_personSpouse=oldSpouse;
    puts("PASS AI skill scores:11200 role cases and10000 pair cases, native lookup/matching/score arithmetic and owner rejection/return branches; raw/composed and multi-member controls");
}
