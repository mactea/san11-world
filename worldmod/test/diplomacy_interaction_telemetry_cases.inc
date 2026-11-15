template<typename Select> static void diplomacyInteractionTelemetryTests(void** command,uint8_t* actor,uint8_t* other,uint8_t* force,Select select){
    auto setup=[&](int owned,int final,int result){
        g_personOther=other;command[1]=actor;command[2]=force;*(int*)(force+4)=14162;
        memset(actor+0x98,0,16);*(int*)(actor+0xe8)=owned;g_skills[249].effects[86]=true;
        g_diplomacyActorRegistered=g_diplomacyForceRegistered=true;g_diplomacyFixtureForceId=249;
        g_diplomacyFixtureFault=g_diplomacyPreModalFault=0;g_diplomacyDifficulty=g_diplomacyPlayer=g_diplomacyScore=0;
        g_diplomacySample=1;g_diplomacyInteractive=true;g_diplomacyUIResult=final;g_diplomacyCommandReturn=result;
    };
    uint8_t saved[5];memcpy(saved,(void*)site(0x5bd7bf),5);
    *(uint8_t*)site(0x5bd7bf)=0xe8;*(int32_t*)site(0x5bd7c0)=(int32_t)(site(0x47be50)-site(0x5bd7c4));
    assert(skilltelemetry::open(L"build/diplomacy_interaction_before_test.csv",personFixtureActor,nullptr));
    for(int kind:{16,18,20})for(int owned:{86,249}){
        setup(owned,2,0);int ui=g_diplomacyUICalls;
        assert(select(kind)((uint8_t*)command)==0 && g_diplomacyConsumed==2 && g_diplomacyUICalls==ui+1);
    }
    skilltelemetry::close();memcpy((void*)site(0x5bd7bf),saved,5);
    assert(skilltelemetry::open(L"build/diplomacy_interaction_test.csv",personFixtureActor,nullptr));
    for(int kind:{16,18,20})for(int owned:{86,249})for(int result:{0,1})for(int final:{0,1,2}){
        setup(owned,final,result);int ui=g_diplomacyUICalls;
        assert(select(kind)((uint8_t*)command)==result && g_diplomacyConsumed==final && g_diplomacyUICalls==ui+1);
        assert(*(int*)g_diplomacyUIState==2 && *(void**)(g_diplomacyUIState+0x24)==actor &&
            *(int*)(g_diplomacyUIState+0x28)==1 && *(int*)(g_diplomacyUIState+0x2c)==final && !g_diplomacyInteractionScope);
    }
    for(int kind:{16,18,20})for(int owned:{86,249})for(int fault=1;fault<=9;++fault){
        if(fault==9 && owned==86)continue;
        setup(owned,2,1);g_diplomacyPreModalFault=fault;int ui=g_diplomacyUICalls;
        assert(select(kind)((uint8_t*)command)==1 && g_diplomacyConsumed==2 && g_diplomacyUICalls==ui+1 && !g_diplomacyInteractionScope);
    }
    for(int kind:{16,18,20})for(int owned:{86,249}){
        setup(owned,2,0);g_diplomacyInteractive=false;int ui=g_diplomacyUICalls;
        assert(select(kind)((uint8_t*)command)==0 && g_diplomacyConsumed==1 && g_diplomacyUICalls==ui);
    }
    // Controlled state immediately before the real native CALL: the observer
    // must reject mismatched mode/actor/flag/result/list/command independently.
    for(int fault=1;fault<=6;++fault){
        setup(249,2,0);select(16);DiplomacyExecution execution((uint8_t*)command,16);
        assert(runDiplomacyDecision(command)==1 && execution.valid());
        DiplomacyInteractionScope scope(&execution,(uint8_t*)command,1);
        *(int*)g_diplomacyUIState=2;*(void**)(g_diplomacyUIState+0x24)=actor;
        *(int*)(g_diplomacyUIState+0x28)=*(int*)(g_diplomacyUIState+0x2c)=1;
        void* list=g_diplomacyUIState+4;
        if(fault==1)*(int*)g_diplomacyUIState=0;
        if(fault==2)*(void**)(g_diplomacyUIState+0x24)=other;
        if(fault==3)*(int*)(g_diplomacyUIState+0x28)=0;
        if(fault==4)*(int*)(g_diplomacyUIState+0x2c)=2;
        if(fault==5)list=g_diplomacyUIState;
        if(fault==6)scope.command=nullptr;
        assert(((int(__thiscall*)(void*))site(0x5bd7bf))(list)==2);
    }
    for(int kind:{16,18,20})for(int owned:{86,249}){
        setup(owned,2,0);select(kind);assert(!g_diplomacyExecution && !g_diplomacyInteractionScope);
        assert(g_diplomacyInteractionOriginal((uint8_t*)command,1)==2);
    }
    skilltelemetry::close();
    for(int kind:{16,18,20})for(int owned:{86,249}){
        setup(owned,2,owned==249);int ui=g_diplomacyUICalls;
        assert(select(kind)((uint8_t*)command)==(owned==249) && g_diplomacyConsumed==2 && g_diplomacyUICalls==ui+1);
    }
    setup(249,2,1);
    puts("PASS diplomacy interaction: actual native modal stores survive cancelled commands; three command kinds, six old-gap cases,105 scenario inputs, stale/state/unowned exclusions and six disabled controls");
}
