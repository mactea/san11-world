// Observe skill86's eligibility bypass and forced result during actual command execution.
// Native result codes stay categorical: neither a debate start nor a signed pact
// can be inferred from a positive predicate or from result1 alone.
typedef int(__cdecl* DiplomacyExecute)(uint8_t*);
typedef int(__cdecl* DiplomacyInteraction)(uint8_t*,int);
BuildingId g_diplomacyForceId=(BuildingId)0x491270;
PersonById g_diplomacyForce=(PersonById)0x490aa0;
DiplomacyInteraction g_diplomacyInteractionOriginal=(DiplomacyInteraction)0x5bd790;
struct DiplomacyExecution;
thread_local DiplomacyExecution* g_diplomacyExecution=nullptr;
struct DiplomacyExecution {
    uint8_t* command;
    int kind,owned=-1,initial=-1,final=-1;
    int actorId=-1,affiliation[4]={},ruler=-1;
    const void* actor=nullptr;
    const void* expectedActor;void* force;const void* rulerActor=nullptr;
    uint64_t generation=skilltelemetry::contextGeneration();
    bool interactive=false;
    skilltelemetry::Target target;
    DiplomacyExecution* previous;
    DiplomacyExecution(uint8_t* c,int k):command(c),kind(k),expectedActor(*(void**)(c+4)),force(*(void**)(c+8)),previous(g_diplomacyExecution){g_diplomacyExecution=this;}
    bool valid()const{
        if(!skilltelemetry::enabled() || generation!=skilltelemetry::contextGeneration() || !actor || actorId<0 ||
           *(void**)(command+4)!=expectedActor || *(void**)(command+8)!=force || actor!=expectedActor ||
           g_telemetryPerson((void*)0x7201958,actorId)!=actor || growthPersonId((void*)actor)!=actorId ||
           *(const int*)((const uint8_t*)actor+0xe8)!=owned || !contains(owned,86) ||
           memcmp(affiliation,(const uint8_t*)actor+0x98,sizeof affiliation))return false;
        if(target.force>=0 && (g_diplomacyForce((void*)0x7201958,target.force)!=force ||
           g_diplomacyForceId((void*)0x7201958,force)!=target.force || *(int*)((uint8_t*)force+4)!=ruler))return false;
        return target.person<0 || (g_telemetryPerson((void*)0x7201958,target.person)==rulerActor &&
            growthPersonId((void*)rulerActor)==target.person);
    }
    ~DiplomacyExecution(){g_diplomacyExecution=previous;}
};
struct DiplomacyDecision;
struct DiplomacyInteractionScope;
void diplomacyContestEnd(uint64_t);
void diplomacyContestPublish(DiplomacyInteractionScope*);
thread_local DiplomacyInteractionScope* g_diplomacyInteractionScope=nullptr;
struct DiplomacyInteractionScope {
    DiplomacyExecution* execution;uint8_t* command;int initial;
    uint64_t contestToken=0;
    DiplomacyInteractionScope* previous;
    DiplomacyInteractionScope(DiplomacyExecution* e,uint8_t* c,int i):execution(e),command(c),initial(i),previous(g_diplomacyInteractionScope){g_diplomacyInteractionScope=this;}
    ~DiplomacyInteractionScope(){diplomacyContestEnd(contestToken);g_diplomacyInteractionScope=previous;}
};
thread_local DiplomacyDecision* g_diplomacyDecision=nullptr;
struct DiplomacyDecision {
    bool forced=false;
    bool eligibilityRecorded=false;
    uint8_t* command;PersonReceipt provider;void* force;int forceId=-1,ruler=-1;
    DiplomacyDecision* previous;
    explicit DiplomacyDecision(uint8_t* c):command(c),provider(*(void**)(c+4)),force(*(void**)(c+8)),previous(g_diplomacyDecision){
        if(force){forceId=g_diplomacyForceId((void*)0x7201958,force);ruler=*(int*)((uint8_t*)force+4);}
        g_diplomacyDecision=this;
    }
    ~DiplomacyDecision(){g_diplomacyDecision=previous;}
};
// At this native successor EAX is nonzero only when the86 predicate skipped
//5bc650; an ordinary successful gate reaches it with EAX=0. This records the
// bypass itself, independently of the later score/result/debate outcome.
void __cdecl diplomacyEligibilityBypassed(uint8_t* command,int matched){
    auto decision=g_diplomacyDecision;auto execution=g_diplomacyExecution;
    if(!matched || !decision || !execution || decision->eligibilityRecorded || execution->kind!=16 ||
       execution->command!=command || decision->command!=command || execution->expectedActor!=decision->provider.person ||
       *(void**)(command+4)!=decision->provider.person || *(void**)(command+8)!=decision->force ||
       execution->force!=decision->force || !decision->provider.valid(86) || decision->forceId<0 ||
       g_diplomacyForce((void*)0x7201958,decision->forceId)!=decision->force ||
       g_diplomacyForceId((void*)0x7201958,decision->force)!=decision->forceId ||
       *(int*)((uint8_t*)decision->force+4)!=decision->ruler)return;
    skilltelemetry::Target target;target.force=decision->forceId;
    auto ruler=g_telemetryPerson((void*)0x7201958,decision->ruler);
    if(ruler && growthPersonId(ruler)==decision->ruler)target.person=decision->ruler;
    if(!decision->provider.valid(86))return;
    decision->eligibilityRecorded=true;
    target.context="{\"command_type\":16,\"gate\":\"0x5bc650\",\"counterfactual_evaluated\":false}";
    skilltelemetry::appliedEvent(decision->provider.person,decision->provider.owned,86,"diplomacy",
        "negotiation_eligibility_check_bypassed",0x5b1fdf,target);
}
uint32_t g_diplomacyEligibilityResume=0x5b1fe5;
__declspec(naked) void diplomacyEligibilityBridge(){__asm {pushfd
    pushad
    push eax
    push esi
    call diplomacyEligibilityBypassed
    add esp,8
    popad
    popfd
    mov ecx,[esi+8]
    mov eax,[ecx+4]
    jmp dword ptr [g_diplomacyEligibilityResume]}}
const struct DiplomacyEligibilityLayout {uint32_t start;int size;const char* bytes;} g_diplomacyEligibilityLayouts[]={
    {0x5b1fc2,29,"\x8b\x4e\x04\x6a\x56\xe8\x24\x71\xed\xff\x85\xc0\x75\x0f\x8b\xce\xe8\x79\xa6\x00\x00\x85\xc0\x0f\x85\x67\x01\x00\x00"},
};
bool verifyDiplomacyEligibilityLayout(){
    for(const auto& s:g_diplomacyEligibilityLayouts)
        if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
void __cdecl diplomacyForced() {if(g_diplomacyDecision)g_diplomacyDecision->forced=true;}
int __fastcall diplomacyDecision(uint8_t* command,void*,int* option) {
    typedef int(__thiscall* Decide)(void*,int*);
    auto original=(*(Decide*)(*(uint8_t**)command+0x14));
    auto execution=g_diplomacyExecution;
    if(!skilltelemetry::enabled() || !execution || execution->command!=command)return original(command,option);
    ProgressionProbe probe(86);probe.scene="diplomacy";probe.event="negotiation_skill_eligibility";
    DiplomacyDecision decision(command);
    int result=original(command,option); // retain the native difficulty/player gates and single random sample
    auto actor=*(uint8_t**)(command+4);
    if(result==1 && decision.forced && probe.actor==actor && actor==execution->expectedActor &&
       execution==g_diplomacyExecution && execution->generation==skilltelemetry::contextGeneration() && growthPersonId(actor)>=0) {
        execution->actor=actor;execution->actorId=growthPersonId(actor);execution->owned=*(int*)(actor+0xe8);execution->initial=execution->final=result;
        memcpy(execution->affiliation,actor+0x98,sizeof execution->affiliation);
        auto force=*(uint8_t**)(command+8);
        int id=g_diplomacyForceId((void*)0x7201958,force);
        if(id>=0 && g_diplomacyForce((void*)0x7201958,id)==force) {
            execution->target.force=id;
            int ruler=execution->ruler=*(int*)(force+4);auto person=g_telemetryPerson((void*)0x7201958,ruler);
            if(person && growthPersonId(person)==ruler){execution->target.person=ruler;execution->rulerActor=person;}
        }
    }
    return result;
}
int __cdecl diplomacyInteraction(uint8_t* command,int initial) {
    auto execution=g_diplomacyExecution;
    bool observe=execution && execution->command==command && execution->valid() && initial==execution->initial;
    uintptr_t caller=(uintptr_t)_ReturnAddress();
    bool owner=observe && caller==g_strategyCodeBase+(execution->kind==16?0x1b22cd:execution->kind==18?0x1b655f:0x1cc6de);
    DiplomacyInteractionScope scope(owner?execution:nullptr,command,initial);
    int result=g_diplomacyInteractionOriginal(command,initial);
    if(observe){execution->interactive=true;execution->final=result;}
    return result;
}
int diplomacyExecute(uint8_t* command,int kind,DiplomacyExecute original) {
    if(!skilltelemetry::enabled())return original(command);
    DiplomacyExecution execution(command,kind);
    int result=original(command);
    if(result && execution.valid()) {
        char context[224];
        sprintf_s(context,"{\"command_type\":%d,\"initial_result_code\":%d,\"final_result_code\":%d,\"interactive_resolution\":%s,\"target_role\":\"force_ruler\",\"counterfactual_evaluated\":false}",
            kind,execution.initial,execution.final,execution.interactive?"true":"false");execution.target.context=context;
        skilltelemetry::appliedEvent(execution.actor,execution.owned,86,"diplomacy","negotiation_skill_result_resolved",
            kind==16?0x5b2250:kind==18?0x5b6510:0x5cc650,execution.target);
    }
    return result;
}
uint32_t g_diplomacy16Resume=0x5b2256,g_diplomacy18Resume=0x5b6516,g_diplomacy20Resume=0x5cc656;
__declspec(naked) void diplomacy16Native(){__asm {sub esp,0x3d4
    jmp dword ptr [g_diplomacy16Resume]}}
__declspec(naked) void diplomacy18Native(){__asm {sub esp,0x3d8
    jmp dword ptr [g_diplomacy18Resume]}}
__declspec(naked) void diplomacy20Native(){__asm {sub esp,0x3d0
    jmp dword ptr [g_diplomacy20Resume]}}
DiplomacyExecute g_diplomacy16Original=(DiplomacyExecute)diplomacy16Native,g_diplomacy18Original=(DiplomacyExecute)diplomacy18Native,g_diplomacy20Original=(DiplomacyExecute)diplomacy20Native;
int __cdecl diplomacy16(uint8_t* c){return diplomacyExecute(c,16,g_diplomacy16Original);}
int __cdecl diplomacy18(uint8_t* c){return diplomacyExecute(c,18,g_diplomacy18Original);}
int __cdecl diplomacy20(uint8_t* c){return diplomacyExecute(c,20,g_diplomacy20Original);}
uint32_t g_diplomacy16DecisionResume=0x5b22ac,g_diplomacy18DecisionResume=0x5b653e,g_diplomacy20DecisionResume=0x5cc6c1;
__declspec(naked) void diplomacy16DecisionBridge(){__asm {call diplomacyDecision
    mov ebp,eax
    jmp dword ptr [g_diplomacy16DecisionResume]}}
__declspec(naked) void diplomacy18DecisionBridge(){__asm {call diplomacyDecision
    mov ebp,eax
    jmp dword ptr [g_diplomacy18DecisionResume]}}
__declspec(naked) void diplomacy20DecisionBridge(){__asm {call diplomacyDecision
    mov ecx,esi
    jmp dword ptr [g_diplomacy20DecisionResume]}}
uint32_t g_diplomacy16ForcedResume=0x5b212d,g_diplomacy18ForcedResume=0x5b63b3,g_diplomacy20ForcedResume=0x5cc516;
#define DIPLOMACY_FORCED_BRIDGE(name,resume) \
__declspec(naked) void name(){__asm {pushfd} __asm {pushad} __asm {call diplomacyForced} \
    __asm {popad} __asm {popfd} __asm {mov eax,1} __asm {jmp dword ptr [resume]}}
DIPLOMACY_FORCED_BRIDGE(diplomacy16ForcedBridge,g_diplomacy16ForcedResume)
DIPLOMACY_FORCED_BRIDGE(diplomacy18ForcedBridge,g_diplomacy18ForcedResume)
DIPLOMACY_FORCED_BRIDGE(diplomacy20ForcedBridge,g_diplomacy20ForcedResume)
#undef DIPLOMACY_FORCED_BRIDGE
