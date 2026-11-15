static uint8_t g_rawPriorityRecords[250][0x68];
static int g_rawPriorityFault=-1;
static void* __fastcall rawPriorityFixtureRecord(void*,void*,int owned){return owned>=0 && owned<250?g_rawPriorityRecords[owned]:nullptr;}
static uint8_t __fastcall rawPriorityFixtureAttribute(uint8_t* person,void*,int kind){
    assert(kind==0 || kind==4);
    if(person==g_personOther && g_rawPriorityFault>=0){
        int fault=g_rawPriorityFault;g_rawPriorityFault=-1;
        if(fault==0)*(int*)(g_personGuide+0xe8)=249;
        if(fault==1)skillsTelemetryStream(1,0);
    }
    return person[0xc8+kind];
}
static void rawPriorityTelemetryTests(){
    uint8_t people[2][400]={};auto first=people[0],second=people[1];
    auto oldGuide=g_personGuide,oldOther=g_personOther,oldTarget=g_personTarget,oldSpouse=g_personSpouse;
    g_personGuide=first;g_personOther=second;g_personTarget=g_personSpouse=nullptr;
    auto oldPerson=g_telemetryPerson;auto oldId=g_telemetryPersonId;auto oldResume=g_rawPriorityResume;
    g_telemetryPerson=(PersonById)personFixtureLookup;g_telemetryPersonId=(BuildingId)personFixtureId;
    g_rawPriorityResume=(uint32_t)site(0x5dea35);
    bool old84=g_skills[249].effects[84];g_skills[249].effects[84]=true;
    uint32_t sites[]={0x490c50,0x47a630,0x489030};uint8_t saved[3][5];
    const void* replacements[]={rawPriorityFixtureRecord,domesticValid,rawPriorityFixtureAttribute};
    for(int n=0;n<3;++n){memcpy(saved[n],(void*)site(sites[n]),5);domesticStub(sites[n],replacements[n]);}
    auto compare=[&](int ownA,int ownB,int mode,int statA,int statB,int kind){
        *(int*)(first+0xe8)=ownA;*(int*)(second+0xe8)=ownB;first[0xc8+kind]=(uint8_t)statA;second[0xc8+kind]=(uint8_t)statB;
        memset(g_rawPriorityRecords,0,sizeof g_rawPriorityRecords);
        *(int*)(g_rawPriorityRecords[77]+0x64)=mode!=0;
        *(int*)(g_rawPriorityRecords[84]+0x64)=mode==1;
        *(int*)(g_rawPriorityRecords[249]+0x64)=mode!=1;
        bool rawA=ownA==84,rawB=ownB==84;
        bool categoryA=ownA>=0 && *(int*)(g_rawPriorityRecords[ownA]+0x64)==1;
        bool categoryB=ownB>=0 && *(int*)(g_rawPriorityRecords[ownB]+0x64)==1;
        bool expected=rawA!=rawB?rawA:categoryA!=categoryB?categoryA:statA!=statB?statA>statB:first<second;
        assert(((RawPriorityCompare)site(0x5dea30))(first,second,kind)==int(expected));
    };
    assert(skilltelemetry::open(L"build/raw_priority_test.csv",personFixtureActor,nullptr));
    for(int ownA:{-1,77,84,249})for(int ownB:{-1,77,84,249})for(int mode=0;mode<3;++mode)
        for(int a:{0,50,100})for(int b:{0,50,100})for(int kind:{0,4})compare(ownA,ownB,mode,a,b,kind);
    for(int fault=0;fault<2;++fault){g_rawPriorityFault=fault;compare(84,84,1,0,100,0);assert(g_rawPriorityFault==-1);}
    skilltelemetry::close();for(int ownA:{84,249})for(int ownB:{-1,84})compare(ownA,ownB,0,0,100,4);
    for(int n=0;n<3;++n)memcpy((void*)site(sites[n]),saved[n],5);
    g_skills[249].effects[84]=old84;g_telemetryPerson=oldPerson;g_telemetryPersonId=oldId;g_rawPriorityResume=oldResume;
    g_personGuide=oldGuide;g_personOther=oldOther;g_personTarget=oldTarget;g_personSpouse=oldSpouse;
    puts("PASS raw84 ranking:864 native comparisons, raw/composed/category/stat/pointer priority, two stale controls and four disabled controls; query records only");
}
