//588cb0 publishes controller command3 on the worker thread, then its virtual
//57daf0/439020 waits until5892e0 consumes and acknowledges the command. Keep
// provenance only for that live invocation; no stack pointer crosses threads.
struct BloodroadPending {
    void* unit=nullptr;
    uint8_t snapshot[0xf4]={};
    uint64_t token=0,generation=0;
    bool claimed=false,reset=false;
};
SRWLOCK g_bloodroadLock=SRWLOCK_INIT;
std::map<uintptr_t,BloodroadPending> g_bloodroadPending;
uint64_t g_bloodroadGeneration=0,g_bloodroadToken=0;
void bloodroadSyncLocked(){
    auto generation=skilltelemetry::contextGeneration();
    if(g_bloodroadGeneration!=generation){g_bloodroadPending.clear();g_bloodroadGeneration=generation;}
}
struct BloodroadTransfer;
thread_local BloodroadTransfer* g_bloodroadTransfer=nullptr;
struct BloodroadTransfer {
    void* controller;
    void* unit;
    DefeatScope* parent;
    uint64_t generation,token=0;
    bool completed=false;
    BloodroadTransfer* previous;
    BloodroadTransfer(void* c,void* u,DefeatScope* p):controller(c),unit(u),parent(p),generation(p->generation),previous(g_bloodroadTransfer){g_bloodroadTransfer=this;}
    ~BloodroadTransfer(){
        bool reset=false;AcquireSRWLockExclusive(&g_bloodroadLock);bloodroadSyncLocked();
        auto it=g_bloodroadPending.find((uintptr_t)controller);
        if(it!=g_bloodroadPending.end() && it->second.token==token){reset=it->second.reset;g_bloodroadPending.erase(it);}
        ReleaseSRWLockExclusive(&g_bloodroadLock);
        if(completed && reset && parent==g_defeat && generation==skilltelemetry::contextGeneration())parent->unitReset=true;
        g_bloodroadTransfer=previous;
    }
};
typedef int(__thiscall* BloodroadVisual)(void*,void*);
BloodroadVisual g_bloodroadVisualOriginal=(BloodroadVisual)0x588cb0;
int __fastcall bloodroadVisual(void* self,void*,void* unit){
    auto s=g_defeat;
    if(!s || !s->cleanupActive || !s->protectedBranch || unit!=s->defeated || s->generation!=skilltelemetry::contextGeneration())
        return g_bloodroadVisualOriginal(self,unit);
    BloodroadTransfer transfer(self,unit,s);int result=g_bloodroadVisualOriginal(self,unit);transfer.completed=true;return result;
}
typedef void(__thiscall* BloodroadUnlock)(void*);
BloodroadUnlock g_bloodroadUnlockOriginal=(BloodroadUnlock)0x57dbd0;
void __fastcall bloodroadUnlock(void* self,void*){
    auto scope=g_bloodroadTransfer;bool failed=false;
    if(scope && self==scope->controller && scope->parent==g_defeat && scope->generation==skilltelemetry::contextGeneration() &&
       *(const int*)((const uint8_t*)self+0x4c)==3 && *(void**)((uint8_t*)self+0x48)==scope->unit){
        BloodroadPending pending;pending.unit=scope->unit;pending.generation=scope->generation;
        memcpy(pending.snapshot,scope->unit,sizeof pending.snapshot);
        AcquireSRWLockExclusive(&g_bloodroadLock);bloodroadSyncLocked();
        try {
            if(pending.generation==g_bloodroadGeneration){
                // An overlapping owner must not silently replace an active
                // command. Native controller publication is serialised.
                if(g_bloodroadPending.count((uintptr_t)self))failed=true;
                else {pending.token=++g_bloodroadToken;g_bloodroadPending.emplace((uintptr_t)self,pending);scope->token=pending.token;}
            }
        }catch(...){failed=true;}
        ReleaseSRWLockExclusive(&g_bloodroadLock);
    }
    // Never retain our lock while the native critical section is released or
    // while the producer waits for the UI. Failure disables only the logger.
    if(failed){skilltelemetry::close();if(g_log)g_log("skill telemetry: bloodroad handoff publication failed; logging disabled");}
    g_bloodroadUnlockOriginal(self);
}
struct BloodroadConsumer;
thread_local BloodroadConsumer* g_bloodroadConsumer=nullptr;
struct BloodroadConsumer {
    BloodroadResetTicket ticket;
    void* unit;
    BloodroadConsumer* previous;
    explicit BloodroadConsumer(void* u):unit(u),previous(g_bloodroadConsumer){g_bloodroadConsumer=this;}
    ~BloodroadConsumer(){g_bloodroadConsumer=previous;}
};
BloodroadVisual g_bloodroadConsumeOriginal=(BloodroadVisual)0x588460;
int __fastcall bloodroadConsume(void* self,void*,void* unit){
    BloodroadConsumer scope(unit);
    if(skilltelemetry::enabled() && *(const int*)((const uint8_t*)self+0x4c)==3 && *(void**)((uint8_t*)self+0x48)==unit){
        AcquireSRWLockExclusive(&g_bloodroadLock);bloodroadSyncLocked();auto it=g_bloodroadPending.find((uintptr_t)self);
        if(it!=g_bloodroadPending.end() && !it->second.claimed && it->second.unit==unit &&
           !memcmp(it->second.snapshot,unit,sizeof it->second.snapshot)){
            it->second.claimed=true;scope.ticket={(uintptr_t)self,it->second.token,it->second.generation};
        }
        ReleaseSRWLockExclusive(&g_bloodroadLock);
    }
    return g_bloodroadConsumeOriginal(self,unit);
}
BloodroadResetTicket bloodroadResetBegin(void* unit){
    BloodroadResetTicket ticket;if(!skilltelemetry::enabled())return ticket;
    auto scope=g_bloodroadConsumer;AcquireSRWLockExclusive(&g_bloodroadLock);bloodroadSyncLocked();
    for(auto it=g_bloodroadPending.begin();it!=g_bloodroadPending.end();){
        if(it->second.unit!=unit){++it;continue;}
        if(scope && scope->unit==unit && scope->ticket.controller==it->first && scope->ticket.token==it->second.token &&
           scope->ticket.generation==g_bloodroadGeneration && it->second.claimed && !it->second.reset){ticket=scope->ticket;++it;}
        else it=g_bloodroadPending.erase(it); // unrelated reset invalidates even identical-byte slot reuse
    }
    ReleaseSRWLockExclusive(&g_bloodroadLock);return ticket;
}
void bloodroadResetEnd(void* unit,const BloodroadResetTicket& ticket){
    if(!ticket.token || !defeatUnitCleared(unit))return;
    AcquireSRWLockExclusive(&g_bloodroadLock);bloodroadSyncLocked();auto it=g_bloodroadPending.find(ticket.controller);
    if(it!=g_bloodroadPending.end() && it->second.unit==unit && it->second.token==ticket.token &&
       it->second.generation==ticket.generation && ticket.generation==g_bloodroadGeneration)it->second.reset=true;
    ReleaseSRWLockExclusive(&g_bloodroadLock);
}

struct BloodroadHandoffLayout {uint32_t start;int size;const char* bytes;};
const BloodroadHandoffLayout g_bloodroadHandoffLayouts[]={
    {0x588cb0,108,"\x56\x8b\xf1\xe8\xc8\x50\xff\xff\x85\xc0\x74\x27\x8b\xce\xe8\xfd\x4e\xff\xff\x8b\x44\x24\x08\x8b\xce\xc7\x46\x4c\x03\x00\x00\x00\x89\x46\x48\xe8\xf8\x4e\xff\xff\x8b\x16\x8b\xce\xff\x52\x10\x5e\xc2\x04\x00\x57\x8b\x7c\x24\x0c\x57\xb9\x50\x93\x6a\x09\xe8\x3d\x2b\x02\x00\x57\xb9\x58\x19\x20\x07\xe8\xc2\x8a\xf0\xff\x50\xe8\x9c\x73\x01\x00\x83\xc4\x04\x57\xb9\x5c\x89\x99\x07\xe8\xbe\xa3\xf1\xff\x8b\xce\xe8\xb7\x58\xff\xff\x5f\x5e\xc2"},
    {0x588460,62,"\x56\x8b\x74\x24\x08\x57\x8b\xf9\x56\xb9\x50\x93\x6a\x09\xe8\xbd\x33\x02\x00\x56\xb9\x58\x19\x20\x07\xe8\x42\x93\xf0\xff\x50\xe8\x1c\x7c\x01\x00\x83\xc4\x04\x56\xb9\x5c\x89\x99\x07\xe8\x3e\xac\xf1\xff\x8b\xcf\xe8\x37\x61\xff\xff\x5f\x5e\xc2\x04\x00"},
    {0x5892fe,35,"\x8b\x06\xff\x50\x14\x85\xc0\x0f\x84\x75\x02\x00\x00\x8b\x46\x4c\x57\x33\xff\x83\xf8\x1c\x0f\x87\x65\x02\x00\x00\xff\x24\x85\x98\x95\x58\x00"},
    {0x58937a,11,"\x8b\x4e\x48\x51\x8b\xce\xe8\xdb\xf0\xff\xff"},
    {0x589559,38,"\x8b\xce\xe8\x60\x46\xff\xff\x8b\xce\x89\x7e\x44\xc7\x46\x48\x00\x00\x00\x00\xc7\x46\x4c\x1d\x00\x00\x00\xe8\x58\x46\xff\xff\x8b\x06\x8b\xce\xff\x50\x0c"},
    {0x57dd80,22,"\x56\x8b\xf1\xff\x15\x44\xe2\x74\x00\x2b\x86\xe0\x05\x00\x00\xf7\xd8\x1b\xc0\x40\x5e\xc3"},
    {0x57daf0,22,"\x56\x8b\xf1\xe8\xb8\xb5\xeb\xff\x85\xc0\x75\x08\x8b\xce\x5e\xe9\x1c\xb5\xeb\xff\x5e\xc3"},
    {0x439020,50,"\x56\x8b\xf1\x8b\x46\x08\x50\xff\x15\x50\xe3\x74\x00\x8b\x16\x8b\xce\xff\x52\x14\x85\xc0\x74\x18\x57\x8b\x3d\x54\xe3\x74\x00\x90\x6a\x0a\xff\xd7\x8b\x06\x8b\xce\xff\x50\x14\x85\xc0\x75\xf1\x5f\x5e\xc3"},
    {0x439060,23,"\x8b\x41\x08\x6a\x00\x50\xff\x15\x4c\xe3\x74\x00\x2d\x02\x01\x00\x00\xf7\xd8\x1b\xc0\x40\xc3"},
    {0x439000,24,"\x56\x8b\xf1\x8b\x06\xff\x50\x14\x85\xc0\x74\x0a\x8b\x4e\x08\x51\xff\x15\x58\xe3\x74\x00\x5e\xc3"},
    {0x5895a4,4,"\x7a\x93\x58\x00"}, // command3 dispatch table entry
};
bool verifyBloodroadHandoffLayout(){
    for(const auto& s:g_bloodroadHandoffLayouts){
        const auto p=(const uint8_t*)(g_strategyCodeBase+s.start-0x400000);
        // M7 relocates AREAHEX in both the direct and UI-thread consumers.
        // Accept only their paired operand, retaining every opcode/branch.
        int operand=s.start==0x588cb0?0x3a:(s.start==0x588460?0xa:-1);
        bool match=true;
        for(int n=0;n<s.size;++n){
            if(operand>=0 && n>=operand && n<operand+4)continue;
            if(p[n]!=(uint8_t)s.bytes[n])match=false;
        }
        if(operand>=0){
            uint32_t area=*(const uint32_t*)(p+operand);
            match=match && area && area==*(const uint32_t*)(g_strategyCodeBase+0x58846a-0x400000) &&
                area==*(const uint32_t*)(g_strategyCodeBase+0x588cea-0x400000);
        }
        if(!match){if(g_log)g_log("skills: unexpected bloodroad handoff at %08x; rejected",s.start);return false;}
    }
    return true;
}
