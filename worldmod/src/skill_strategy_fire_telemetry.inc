struct StrategyCommandScope;
thread_local StrategyCommandScope* g_strategyCommand=nullptr;
struct StrategyCommandScope {
    const uint32_t* command;
    int mode;
    StrategyFireRecord fire;
    StrategyStatusReceipt status;
    StrategyCommandScope* previous;
    StrategyCommandScope(const uint32_t* c,int m):command(c),mode(m),previous(g_strategyCommand){g_strategyCommand=this;}
    ~StrategyCommandScope(){g_strategyCommand=previous;}
};
typedef int(__thiscall* StrategyCommand)(void*,const uint32_t*,int,void*);
uint32_t g_strategyCommandResume=0x5933a6;
__declspec(naked) void strategyCommandNative(){__asm {sub esp,0x0c
    push ebx
    push ebp
    push esi
    jmp dword ptr [g_strategyCommandResume]}}
StrategyCommand g_strategyCommandOriginal=(StrategyCommand)strategyCommandNative;
int __fastcall strategyCommand(void* self,void*,const uint32_t* command,int mode,void* person){
    if(!skilltelemetry::enabled())return g_strategyCommandOriginal(self,command,mode,person);
    ReachCommand reach(command,mode);StrategyCommandScope scope(command,mode);
    int result=g_strategyCommandOriginal(self,command,mode,person);reach.finish(result);return result;
}
void strategyFireCritical(const StrategyCriticalScope& scope,int plan,uint32_t to){
    auto command=g_strategyCommand;
    if(!command || plan!=0 || command->command[0]!=0 || command->command[2]!=to ||
       command->command[1]!=(uint32_t)g_telemetryUnitId((void*)0x7201958,scope.source))return;
    command->fire={scope.source,scope.target,scope.actor,scope.actorId,scope.owned,scope.effect,command->mode,to,scope.generation};
}
struct StrategyFirePending {
    StrategyFireRecord record;
    uint32_t snapshot[4]={};
    const void* owner=nullptr;
};
SRWLOCK g_strategyFireLock=SRWLOCK_INIT;
std::map<uintptr_t,StrategyFirePending> g_strategyFirePending;
uint64_t g_strategyFireGeneration=0;
void strategyFireSyncLocked(){
    auto generation=skilltelemetry::contextGeneration();
    if(g_strategyFireGeneration!=generation){g_strategyFirePending.clear();g_strategyFireGeneration=generation;}
}
struct StrategyFireInvocation;
thread_local StrategyFireInvocation* g_strategyFireInvocation=nullptr;
struct StrategyFireInvocation {
    StrategyFireRecord record;
    int turns,mode;
    const uint32_t* packet=nullptr;
    StrategyFireInvocation* previous;
    StrategyFireInvocation(const StrategyFireRecord& r,int t,int m):record(r),turns(t),mode(m),previous(g_strategyFireInvocation){g_strategyFireInvocation=this;}
    ~StrategyFireInvocation(){
        AcquireSRWLockExclusive(&g_strategyFireLock);strategyFireSyncLocked();
        auto it=g_strategyFirePending.find((uintptr_t)packet);
        if(it!=g_strategyFirePending.end() && it->second.owner==this)g_strategyFirePending.erase(it);
        ReleaseSRWLockExclusive(&g_strategyFireLock);g_strategyFireInvocation=previous;
    }
};
typedef void(__thiscall* StrategyFireApply)(void*,uint32_t,int,void*,int);
StrategyFireApply g_strategyFireApplyOriginal=(StrategyFireApply)0x5887e0;
void __fastcall strategyFireApply(void* self,void*,uint32_t position,int turns,void* source,int mode){
    auto command=g_strategyCommand;
    if(!command || !command->fire.actor || command->fire.source!=source || command->fire.position!=position || turns<2 || turns>4){
        g_strategyFireApplyOriginal(self,position,turns,source,mode);return;
    }
    StrategyFireInvocation scope(command->fire,turns,mode);
    g_strategyFireApplyOriginal(self,position,turns,source,mode);
}
void __cdecl strategyFirePacket(const uint32_t* packet){
    auto invocation=g_strategyFireInvocation;if(!invocation || !invocation->record.actor)return;
    const auto& r=invocation->record;
    if(r.generation!=skilltelemetry::contextGeneration() || packet[0]!=r.position ||
       packet[1]!=(uintptr_t)r.source || packet[2]!=(uint32_t)invocation->turns || packet[3]!=(uint32_t)invocation->mode)return;
    StrategyFirePending pending;pending.record=r;memcpy(pending.snapshot,packet,16);pending.owner=invocation;
    bool failed=false;AcquireSRWLockExclusive(&g_strategyFireLock);strategyFireSyncLocked();
    try{if(r.generation==g_strategyFireGeneration){g_strategyFirePending.insert_or_assign((uintptr_t)packet,pending);invocation->packet=packet;}}
    catch(...){failed=true;}
    ReleaseSRWLockExclusive(&g_strategyFireLock);
    if(failed){skilltelemetry::close();if(g_log)g_log("skill telemetry: fire provenance allocation failed; logging disabled");}
}
uint32_t g_strategyFirePacketResume=0x58880d,g_strategyFireDispatchCheck=0x57dd80;
__declspec(naked) void strategyFirePacketBridge(){__asm {pushfd
    pushad
    lea eax,[esp+40]
    push eax
    call strategyFirePacket
    add esp,4
    popad
    popfd
    call dword ptr [g_strategyFireDispatchCheck]
    jmp dword ptr [g_strategyFirePacketResume]}}
typedef int(__cdecl* FireTurns)(uint32_t,int);
FireTurns g_strategyFireTurnsOriginal=(FireTurns)0x5a0770;
const uint32_t* g_strategyFireMapOperand=(const uint32_t*)0x5a07aa;
int __cdecl strategyFireTurns(uint32_t position,int turns,const uint32_t* packet){
    StrategyFirePending pending;AcquireSRWLockExclusive(&g_strategyFireLock);strategyFireSyncLocked();
    auto it=g_strategyFirePending.find((uintptr_t)packet);
    if(it!=g_strategyFirePending.end()){
        if(!memcmp(it->second.snapshot,packet,16))pending=it->second;
        else g_strategyFirePending.erase(it);
    }
    ReleaseSRWLockExclusive(&g_strategyFireLock);
    auto r=pending.record;int x=(int16_t)position,y=(int16_t)(position>>16);
    const uint8_t* mapCode=(const uint8_t*)g_strategyFireMapOperand-0x3a;
    int height=*(const int*)(mapCode+0x13),width=*(const int*)(mapCode+0x1f),stride=*(const int*)(mapCode+0x2a);
    bool observe=skilltelemetry::enabled() && r.actor && r.generation==skilltelemetry::contextGeneration() &&
        position==r.position && turns==(int)pending.snapshot[2] && x>=0 && x<height && y>=0 && y<width && stride==width &&
        g_growthValid(r.source) && g_growthValid(r.target) && growthPersonId((void*)r.actor)==r.actorId &&
        *(const int*)((const uint8_t*)r.actor+0xe8)==r.owned && contains(r.owned,r.effect) && afterAttackMember(r.source,r.actor);
    skilltelemetry::Target target;int sourceId=-1;
    if(observe){
        sourceId=g_telemetryUnitId((void*)0x7201958,r.source);target=unitTarget(r.target);
        typedef const uint32_t*(__thiscall* Position)(void*);
        observe=sourceId>=0 && target.unit>=0 && g_movementUnitById((void*)0x7201958,sourceId)==r.source &&
            g_movementUnitById((void*)0x7201958,target.unit)==r.target &&
            *(*(Position*)(*(uint8_t**)r.target+0x3c))(r.target)==position;
    }
    auto cell=observe?(const uint32_t*)(*(const uint8_t* const*)g_strategyFireMapOperand+20*(x*stride+y)):nullptr;
    int before=observe?(*cell>>23)&7:0;
    int result=g_strategyFireTurnsOriginal(position,turns);
    if(observe && result==1 && r.generation==skilltelemetry::contextGeneration()){
        int after=(*cell>>23)&7;target.actorUnit=sourceId;target.equipment=*(const int*)((const uint8_t*)r.source+0xa8);
        target.x=x;target.y=y;target.terrain=cell[1]&31;char context[320];
        sprintf_s(context,"{\"strategy_id\":0,\"critical_result\":true,\"requested_fire_turns\":%d,\"command_mode\":%d,\"fire_packet_mode\":%u,\"map_width\":%d,\"map_height\":%d,\"native_stride\":%d,\"counterfactual_evaluated\":false}",
            turns,r.commandMode,pending.snapshot[3],width,height,stride);target.context=context;
        skilltelemetry::applied(r.actor,r.owned,r.effect,"strategy","strategy_critical_fire_turns_written","fire_turns",before,after,0x5924cb,target);
    }
    return result;
}
__declspec(naked) void strategyFireTurnsBridge(){__asm {push ebx
    push [esp+12]
    push [esp+12]
    call strategyFireTurns
    add esp,12
    ret}}

struct StrategyFireLayout {uint32_t start;int size;const char* bytes;};
const StrategyFireLayout g_strategyFireLayouts[]={
    {0x5a0770,96,"\x8b\x54\x24\x04\x8b\x4c\x24\x04\x0f\xbf\xc2\xc1\xf9\x10\x85\xc0\x7c\x13\x3d\xc8\x00\x00\x00\x7d\x0c\x85\xc9\x7c\x08\x81\xf9\xc8\x00\x00\x00\x7c\x03\x33\xc0\xc3\x69\xc0\xc8\x00\x00\x00\x03\xc1\x8b\x4c\x24\x08\x8d\x04\x80\x8d\x04\x85\x68\x0e\xfb\x06\x56\x8b\x30\xc1\xe1\x17\x33\xce\x81\xe1\x00\x00\x80\x03\x33\xf1\x52\x89\x30\xe8\xda\xf6\xff\xff\x83\xc4\x04\xb8\x01\x00\x00\x00\x5e\xc3"},
    {0x5887e3,37,"\x8b\x44\x24\x14\x8b\x54\x24\x18\x56\x8b\xf1\x8b\x4c\x24\x20\x89\x44\x24\x04\x8b\x44\x24\x24\x89\x4c\x24\x08\x8b\xce\x89\x54\x24\x0c\x89\x44\x24\x10"},
    {0x591409,47,"\x8b\x8c\x24\x00\x04\x00\x00\x51\x56\x6a\x03\xe8\x37\x0d\xee\xff\x0f\xb7\xd0\x8b\x84\x24\x0c\x04\x00\x00\x8d\x4c\x02\x01\x8b\x55\x08\x83\xc4\x04\x51\x8b\x4c\x24\x28\x52\xe8\xa8\x73\xff\xff"},
    {0x5924c4,15,"\x8b\x53\x08\x8b\x03\x52\x50\xe8\xa0\xe2\x00\x00\x83\xc4\x08"},
    {0x5924ae,9,"\x8b\x43\x08\x8b\x0b\x50\x51\xeb\x14"},
    {0x5933a0,38,"\x83\xec\x0c\x53\x55\x56\x8b\x74\x24\x1c\x8b\x46\x04\x57\x89\x4c\x24\x10\x50\xb9\x58\x19\x20\x07\xe8\xb3\xda\xef\xff\x8b\xf8\x8b\x46\x08\x89\x44\x24\x20"},
    {0x5935d6,12,"\x5f\x5e\x8b\xc5\x5d\x5b\x83\xc4\x0c\xc2\x0c\x00"},
    {0x5935ed,12,"\x5f\x5e\x8b\xc5\x5d\x5b\x83\xc4\x0c\xc2\x0c\x00"},
    {0x588834,24,"\x5e\x83\xc4\x10\xc2\x10\x00\x8d\x44\x24\x04\x50\xe8\xfb\x9b\x00\x00\x5e\x83\xc4\x10\xc2\x10\x00"},
};
bool verifyStrategyFireLayout(){
    for(const auto& s:g_strategyFireLayouts){
        const uint8_t* p=(const uint8_t*)(g_strategyCodeBase+s.start-0x400000);
        bool match=true;
        for(int n=0;n<s.size;++n){
            // M7 patches only these four immediates. Keep every opcode and
            // branch exact, and require coherent bounds/stride and paired B20.
            if(s.start==0x5a0770 && ((n>=0x13 && n<0x17) || (n>=0x1f && n<0x23) ||
               (n>=0x2a && n<0x2e) || (n>=0x3a && n<0x3e)))continue;
            if(p[n]!=(uint8_t)s.bytes[n])match=false;
        }
        if(s.start==0x5a0770){
            int h=*(const int*)(p+0x13),w=*(const int*)(p+0x1f),stride=*(const int*)(p+0x2a);
            uint32_t base=*(const uint32_t*)(p+0x3a);
            match=match && h>0 && h<=32767 && w>0 && w<=32767 && stride==w && base &&
                (uint64_t)base+(uint64_t)h*w*20<=0x100000000ull &&
                base==*(const uint32_t*)(g_strategyCodeBase+0x5ae9d0-0x400000) &&
                base==*(const uint32_t*)(g_strategyCodeBase+0x5af5ad-0x400000) &&
                base==*(const uint32_t*)(g_strategyCodeBase+0x592486-0x400000);
        }
        if(!match){if(g_log)g_log("skills: unexpected strategy fire layout at %08x; rejected",s.start);return false;}
    }
    return true;
}
