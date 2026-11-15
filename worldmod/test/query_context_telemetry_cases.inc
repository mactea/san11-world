static uintptr_t g_marriageQueryEntry=0;
static void* g_marriageQuerySelected=nullptr,*g_marriageQueryPartner=nullptr;
static int __fastcall queryContextAttribute(uint8_t* person,void*){return person[0xc8];}
__declspec(naked) static void __cdecl runMarriageQueries(void*,void*){__asm {push ebp
    push ebx
    push esi
    push edi
    mov edi,[esp+20]
    mov esi,[esp+24]
    push edi
    jmp dword ptr [g_marriageQueryEntry]}}
__declspec(naked) static void marriageQueryDone(){__asm {mov g_marriageQuerySelected,ebx
    mov g_marriageQueryPartner,ebp
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret}}
static void queryContextTelemetryTests(){
    uint8_t people[2][400]={};auto first=people[0],second=people[1];
    auto oldGuide=g_personGuide,oldOther=g_personOther,oldTarget=g_personTarget,oldSpouse=g_personSpouse;
    g_personGuide=first;g_personOther=second;g_personTarget=g_personSpouse=nullptr;
    auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryPersonId;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)personFixtureId;
    bool old77=g_skills[249].effects[77],old99=g_skills[249].effects[99];g_skills[249].effects[77]=g_skills[249].effects[99]=true;
    uint32_t sites[]={0x4890a0,0x47a630,0x5c7cf0,0x5c7d60};uint8_t saved[4][5];
    const void* replacements[]={queryContextAttribute,domesticValid,marriageQueryDone,marriageQueryDone};
    for(int n=0;n<4;++n){memcpy(saved[n],(void*)site(sites[n]),5);domesticStub(sites[n],replacements[n]);}
    g_marriageQueryEntry=site(0x5c7cba);
    auto compare=[&](int ownFirst,int ownSecond,int statFirst,int statSecond){
        *(int*)(first+0xe8)=ownFirst;*(int*)(second+0xe8)=ownSecond;first[0xc8]=(uint8_t)statFirst;second[0xc8]=(uint8_t)statSecond;
        int a=50+2*statFirst/3,b=50+2*statSecond/3;
        if(ownFirst>=0)a*=2;if(ownSecond>=0)b*=2;
        assert(((int(__cdecl*)(void*,void*))site(0x5db780))(first,second)==(a>=b));
    };
    auto marry=[&](int ownFirst,int ownSecond){
        *(int*)(first+0xe8)=ownFirst;*(int*)(second+0xe8)=ownSecond;runMarriageQueries(first,second);
        assert(g_marriageQuerySelected==(ownFirst>=0?first:ownSecond>=0?second:nullptr));
        assert(g_marriageQueryPartner==(ownFirst>=0?second:ownSecond>=0?first:nullptr));
    };
    assert(skilltelemetry::open(L"build/query_context_test.csv",personFixtureActor,nullptr));
    for(int ownFirst:{-1,77,249})for(int ownSecond:{-1,77,249})for(int a:{0,50,100})for(int b:{0,50,100})compare(ownFirst,ownSecond,a,b);
    for(int ownFirst:{-1,99,249})for(int ownSecond:{-1,99,249})marry(ownFirst,ownSecond);
    for(int effect:{77,99})for(int owned:{effect,249}){*(int*)(first+0xe8)=owned;assert(personHas(first,nullptr,effect));}
    {ProgressionProbe unrelated(84);unrelated.scene="growth";unrelated.event="guidance_eligibility";
        compare(77,-1,0,100);assert(!unrelated.actor);}
    {ProgressionProbe unrelated(99);unrelated.scene="person_stats";unrelated.event="spousal_bonus_eligibility";
        marry(249,99);assert(unrelated.actor==first);}
    skilltelemetry::close();
    for(int owned:{77,249})compare(owned,-1,0,100);
    for(int owned:{99,249})marry(owned,-1);
    for(int n=0;n<4;++n)memcpy((void*)site(sites[n]),saved[n],5);
    g_skills[249].effects[77]=old77;g_skills[249].effects[99]=old99;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldId;
    g_personGuide=oldGuide;g_personOther=oldOther;g_personTarget=oldTarget;g_personSpouse=oldSpouse;
    puts("PASS native query contexts:96 input cases; complete77 comparator and99 first-holder presentation selection, raw/composed/high actors, unowned and ambient-probe controls; no applied effects");
}
