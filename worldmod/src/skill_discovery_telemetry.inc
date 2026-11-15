// Only the execution call sites participate; prediction and AI estimates stay queries.
typedef int(__cdecl* ExplorationOutcome)(uint8_t*,void*,void*);
typedef int(__cdecl* DiscoveryHandler)(uint8_t*,uint8_t*,void*);
typedef void(__thiscall* DiscoveryStore)(void*,uint8_t*,int);
ExplorationOutcome g_explorationOutcomeOriginal=(ExplorationOutcome)0x5d1ea0;
DiscoveryHandler g_discoveryHandlerOriginal=(DiscoveryHandler)0x5d56a0;
DiscoveryStore g_discoveryStoreOriginal=(DiscoveryStore)0x4a5b20;
struct DiscoveryPending {const void* actor=nullptr;int candidates=0;};
thread_local DiscoveryPending g_discoveryPending;
struct DiscoveryCommit;
thread_local DiscoveryCommit* g_discoveryCommit=nullptr;
struct DiscoveryCommit {
    uint8_t* person;
    const void* actor;
    void* base;
    int candidates;
    DiscoveryCommit* previous;
    DiscoveryCommit(uint8_t* p,const void* a,void* b,int n):person(p),actor(a),base(b),candidates(n),previous(g_discoveryCommit){g_discoveryCommit=this;}
    ~DiscoveryCommit(){g_discoveryCommit=previous;}
};
int __cdecl explorationOutcome(uint8_t* actor,void* city,void* candidates) {
    g_discoveryPending=DiscoveryPending{};
    if(!skilltelemetry::enabled())return g_explorationOutcomeOriginal(actor,city,candidates);
    ProgressionProbe probe(85);
    // Includes the native selection/random test exactly once. Never replay it for a baseline.
    int result=g_explorationOutcomeOriginal(actor,city,candidates);
    if(result && probe.actor==actor)g_discoveryPending={actor,*(const int*)((const uint8_t*)candidates+0xc)};
    return result;
}
int __cdecl discoveryHandler(uint8_t* person,uint8_t* actor,void* base) {
    auto pending=g_discoveryPending;g_discoveryPending=DiscoveryPending{};
    if(!skilltelemetry::enabled() || pending.actor!=actor || pending.candidates<=0 ||
       !contains(*(int*)(actor+0xe8),85))return g_discoveryHandlerOriginal(person,actor,base);
    DiscoveryCommit commit(person,actor,base,pending.candidates);
    return g_discoveryHandlerOriginal(person,actor,base);
}
void __fastcall discoveryStore(void* self,void*,uint8_t* person,int city) {
    auto commit=g_discoveryCommit;
    bool observe=skilltelemetry::enabled() && commit && commit->person==person && g_growthValid(person);
    int id=observe?growthPersonId(person):-1;
    int before=id>=0?*(int*)(person+0xa0):-1;
    int location=id>=0?*(int*)(person+0x98):-1;
    g_discoveryStoreOriginal(self,person,city);
    if(id<0 || before!=7 || *(int*)(person+0xa0)!=4)return;
    auto target=incomeTarget(commit->base);target.person=id;
    target.force=(*(BuildingValue*)(*(uint8_t**)person+0x40))(person);
    char context[256];
    sprintf_s(context,"{\"status_before\":7,\"status_after\":4,\"location_before\":%d,\"location_after\":%d,\"requested_city\":%d,\"candidate_count\":%d,\"discovery_chance\":100,\"counterfactual_evaluated\":false}",
        location,*(int*)(person+0x98),city,commit->candidates);
    target.context=context;
    // Discovery precedes the separate recruitment routine; do not report automatic recruitment.
    skilltelemetry::appliedEvent(commit->actor,*(const int*)((const uint8_t*)commit->actor+0xe8),85,
        "exploration","person_discovered",0x5d5708,target);
}
