//65 changes target permission, not chance/damage. Correlate the original
// read-only permission calculation with a paid, returned native command.
struct ReachCommand;
thread_local ReachCommand* g_reachCommand=nullptr;
struct ReachPermissionProbe;
thread_local ReachPermissionProbe* g_reachPermissionProbe=nullptr;
struct ReachPermissionProbe {
    int normalMax;
    ReachPermissionProbe* previous;
    explicit ReachPermissionProbe(int plan):normalMax(plan==0 || plan==5 || plan==6?1:2),previous(g_reachPermissionProbe){g_reachPermissionProbe=this;}
    ~ReachPermissionProbe(){g_reachPermissionProbe=previous;}
};
typedef int(__thiscall* ReachTechnology)(void*,int);
ReachTechnology g_reachTechnologyOriginal=(ReachTechnology)0x4811e0;
int __fastcall reachTechnology(void* force,void*,int tech){
    int result=g_reachTechnologyOriginal(force,tech);
    if(g_reachPermissionProbe && tech==29)g_reachPermissionProbe->normalMax=result?3:1;
    return result;
}
uintptr_t g_reachPermissionEntry=0x5a3cd0;
__declspec(naked) int __cdecl reachPermission(void*,uint32_t,void*,uint32_t,int,int){__asm {
    push ebx
    mov ebx,[esp+8]
    push [esp+28]
    push [esp+28]
    push [esp+28]
    push [esp+28]
    push [esp+28]
    call dword ptr [g_reachPermissionEntry]
    add esp,20
    pop ebx
    ret}}
typedef void*(__thiscall* ReachObject)(void*);
ReachObject g_reachObjectOriginal=(ReachObject)0x483b40;
int reachDistance(uint32_t from,uint32_t to){
    int x1=(int16_t)from,y1=(int16_t)(from>>16),x2=(int16_t)to,y2=(int16_t)(to>>16);
    int a=y1-(x1-(x1&1))/2,b=y2-(x2-(x2&1))/2;
    return (abs(a-b)+abs(x1-x2)+abs(a-b+x1-x2))/2;
}
struct ReachCommand {
    const uint32_t* command;uint32_t snapshot[3],from=0;
    int mode,sourceId=-1,force=-1,actorId=-1,owned=-1,normalMax=0,distance=0,mask=0;
    int energyBefore=0,energyAfter=0,requested=0;
    void* source=nullptr;const void* actor=nullptr;
    PersonReceipt provider;
    uintptr_t costFrame=0;
    bool eligible=false,paid=false;uint64_t generation;
    skilltelemetry::Target target;ReachCommand* previous;
    ReachCommand(const uint32_t* c,int m):command(c),mode(m),generation(skilltelemetry::contextGeneration()),previous(g_reachCommand){
        g_reachCommand=this;memcpy(snapshot,c,sizeof snapshot);
        if(mode!=0 || c[0]>8)return;
        sourceId=(int)c[1];source=g_movementUnitById((void*)0x7201958,sourceId);
        if(!source || g_telemetryUnitId((void*)0x7201958,source)!=sourceId)return;
        bool has=false;for(int n=0;n<3;++n){int id=*(const int*)((const uint8_t*)source+0xc+4*n);
            auto p=id>=0?g_telemetryPerson((void*)0x7201958,id):nullptr;
            if(p && contains(*(const int*)(p+0xe8),65)){has=true;break;}}
        if(!has)return;
        typedef const uint32_t*(__thiscall* Position)(void*);from=*(*(Position*)(*(uint8_t**)source+0x3c))(source);
        distance=reachDistance(from,c[2]);int x=(int16_t)c[2],y=(int16_t)(c[2]>>16);
        int width=*(const int*)(g_strategyCodeBase+0x5a078f-0x400000),height=*(const int*)(g_strategyCodeBase+0x5a0783-0x400000);
        if(x<0 || x>=height || y<0 || y>=width)return;
        auto cells=*(uint8_t* const*)(g_strategyCodeBase+0x5a07aa-0x400000);auto cell=cells+20*(x*width+y);
        void* object=g_reachObjectOriginal(cell);int kind=*(const uint32_t*)cell&3;
        if(kind==1 && object)target=unitTarget(object);else if(kind==2 && object)target=incomeTarget(object);
        force=unitTarget(source).force;
        ProgressionProbe eligibility(65,&provider);eligibility.scene="strategy";eligibility.event="extended_range_permission_checked";
        ReachPermissionProbe permission((int)c[0]);mask=reachPermission(source,from,object,c[2],distance,1u<<c[0]);normalMax=permission.normalMax;
        if(!provider.valid(65))return;
        actor=provider.person;actorId=provider.id;owned=provider.owned;
        eligible=(mask&(1u<<c[0])) && distance>normalMax && distance<=normalMax+1 && valid();
        target.actorUnit=sourceId;target.equipment=*(const int*)((const uint8_t*)source+0xa8);target.x=x;target.y=y;
        target.terrain=*(const uint32_t*)(cell+4)&31;
    }
    bool valid()const{
        return skilltelemetry::enabled() && generation==skilltelemetry::contextGeneration() && provider.valid(65) && source && actor && actorId>=0 &&
            !memcmp(command,snapshot,sizeof snapshot) && g_movementUnitById((void*)0x7201958,sourceId)==source &&
            unitTarget(source).force==force && growthPersonId((void*)actor)==actorId &&
            *(const int*)((const uint8_t*)actor+0xe8)==owned && contains(owned,65) && afterAttackMember(source,actor);
    }
    void finish(int result){
        if(!eligible || !paid || (result!=0 && result!=1) || !valid())return;
        char context[384];sprintf_s(context,"{\"strategy_id\":%u,\"distance\":%d,\"normal_range_max\":%d,\"skill_range_max\":%d,\"native_permission_mask\":%d,\"energy_before\":%d,\"energy_after\":%d,\"requested_energy_delta\":%d,\"native_command_return\":%d,\"result_scope\":\"paid_extended_range_command_not_target_outcome\",\"counterfactual_evaluated\":false}",
            snapshot[0],distance,normalMax,normalMax+1,mask,energyBefore,energyAfter,requested,result);target.context=context;
        skilltelemetry::appliedEvent(actor,owned,65,"strategy","extended_strategy_command_resolved",0x5933a0,target);
    }
    ~ReachCommand(){g_reachCommand=previous;}
};
void reachCostOwner(void* unit,int plan,uintptr_t frame){
    auto s=g_reachCommand;if(s && s->eligible && !s->costFrame && s->source==unit && (int)s->snapshot[0]==plan && s->valid())s->costFrame=frame;
}
bool reachObservesCost(void* unit,uintptr_t caller,uintptr_t frame){auto s=g_reachCommand;return s && s->eligible && !s->paid &&
    s->source==unit && caller==g_strategyCodeBase+0x19344a && s->costFrame==frame && s->valid();}
void reachCostPaid(void* unit,int before,int after,int requested,int result,uintptr_t frame){
    auto s=g_reachCommand;if(!s || s->source!=unit || s->costFrame!=frame || !s->eligible || s->paid || !s->valid() ||
        requested>=0 || after>=before || result!=after-before)return;
    s->paid=true;s->energyBefore=before;s->energyAfter=after;s->requested=requested;
}

struct ReachLayout {uint32_t start;int size;const char* bytes;};
const ReachLayout g_reachLayouts[]={
    {0x5a3cd0,703,"\x51\x8b\x44\x24\x18\x85\xc0\x75\x04\x33\xc0\x59\xc3\x8b\x03\x55\x56\x57\x8b\xcb\xc7\x44\x24\x0c\x00\x00\x00\x00\xff\x50\x40\x50\xb9\x58\x19\x20\x07\xe8\xa6\xcd\xee\xff\x0f\xbf\x4c\x24\x1e\x8b\x13\x8b\xf8\x0f\xbf\x44\x24\x1c\x69\xc0\xc8\x00\x00\x00\x03\xc1\x8d\x34\x80\x6a\x41\x8b\xcb\x8d\x34\xb5\x68\x0e\xfb\x06\xff\x52\x58\x8b\xe8\x8a\x44\x24\x24\xf7\xdd\x1b\xed\xf7\xdd\xa8\x01\x74\x6c\x6a\x1d\x8b\xcf\xe8\xa6\xd4\xed\xff\x8b\x4c\x24\x20\x8b\x7c\x24\x18\xf7\xd8\x1b\xc0\x83\xe0\x02\x40\x03\xc5\x3b\xc8\x7f\x51\x57\xe8\xda\x68\xed\xff\x83\xc4\x04\x85\xc0\x74\x23\x57\x8b\xcb\xe8\x5b\x2f\xef\xff\x85\xc0\x74\x38\x57\xe8\x71\xbe\x00\x00\x83\xc4\x04\x85\xc0\x75\x2b\xc7\x44\x24\x0c\x01\x00\x00\x00\xeb\x21\x8b\x46\x04\x83\xe0\x1f\x50\xe8\x54\xfc\xed\xff\x83\xc4\x04\x85\xc0\x74\x0e\xc7\x44\x24\x0c\x01\x00\x00\x00\xeb\x04\x8b\x7c\x24\x18\xf6\x44\x24\x24\x02\x74\x31\x8b\x44\x24\x20\x8d\x4d\x02\x3b\xc1\x7f\x26\xf7\x06\x00\x00\x80\x03\x76\x1e\x57\xe8\x6f\x68\xed\xff\x83\xc4\x04\x85\xc0\x74\x0c\x57\x8b\xcb\xe8\x80\x2e\xef\xff\x85\xc0\x75\x05\x83\x4c\x24\x0c\x02\xf6\x44\x24\x24\x04\x74\x1f\x8b\x44\x24\x20\x8d\x55\x02\x3b\xc2\x7f\x14\x53\x8b\xf7\xe8\x2d\xfe\xff\xff\x83\xc4\x04\x85\xc0\x74\x05\x83\x4c\x24\x0c\x04\xf6\x44\x24\x24\x08\x74\x1f\x8b\x4c\x24\x20\x8d\x45\x02\x3b\xc8\x7f\x14\x53\x8b\xf7\xe8\x07\xfe\xff\xff\x83\xc4\x04\x85\xc0\x74\x05\x83\x4c\x24\x0c\x08\xf6\x44\x24\x24\x10\x74\x4f\x8b\x44\x24\x20\x8d\x4d\x02\x3b\xc1\x7f\x44\x57\xe8\xf3\x67\xed\xff\x83\xc4\x04\x85\xc0\x74\x37\x8b\x17\x6a\x0b\x8b\xcf\xff\x52\x2c\x85\xc0\x74\x2a\x57\xe8\x89\x21\xef\xff\x83\xc4\x04\x50\x8b\xcb\xe8\xbe\x1d\xef\xff\x85\xc0\x74\x15\x57\xe8\x74\x21\xef\xff\x8b\x48\x24\x83\xc4\x04\x85\xc9\x74\x05\x83\x4c\x24\x0c\x10\xf6\x44\x24\x24\x40\x74\x67\x8b\x4c\x24\x20\x8d\x45\x01\x3b\xc8\x7f\x5c\x53\xe8\xdd\xfd\xff\xff\x83\xc4\x04\x85\xc0\x74\x4b\x8b\x4c\x24\x18\x51\xe8\x3c\x21\xef\xff\x53\x8b\xf8\xe8\x84\xd0\x07\x00\x8b\x17\x83\xc4\x08\x8b\xcf\x8b\xf0\xff\x52\x3c\x6a\x00\x56\xb9\x58\x19\x20\x07\x8b\xf8\xe8\xf9\xd8\xee\xff\x50\x8b\x07\x68\xf0\x31\x5a\x00\x6a\x01\x50\xb9\x68\x0e\xfb\x06\xe8\xc4\x04\xee\xff\x85\xc0\x74\x05\x83\x4c\x24\x0c\x40\x8b\x7c\x24\x18\xf6\x44\x24\x24\x20\x74\x43\x8b\x44\x24\x20\x8d\x4d\x01\x3b\xc1\x7f\x38\x0f\xbf\x44\x24\x14\x0f\xbf\x54\x24\x16\x69\xc0\xc8\x00\x00\x00\x03\xc2\x8d\x04\x80\x8b\x0c\x85\x6c\x0e\xfb\x06\x83\xe1\x1f\x80\xf9\x05\x75\x14\x53\x8b\xf7\xe8\xf9\xfc\xff\xff\x83\xc4\x04\x85\xc0\x74\x05\x83\x4c\x24\x0c\x20\x8a\x44\x24\x24\x84\xc0\x79\x22\x8b\x44\x24\x20\x8d\x55\x02\x3b\xc2\x7f\x17\x53\x8b\xf7\xe8\xd2\xfc\xff\xff\x83\xc4\x04\x85\xc0\x74\x08\x81\x4c\x24\x0c\x80\x00\x00\x00\x8b\x44\x24\x24\xbe\x00\x01\x00\x00\x85\xc6\x74\x1c\x8b\x44\x24\x20\x83\xc5\x02\x3b\xc5\x7f\x11\x57\xe8\x65\xbc\x00\x00\x83\xc4\x04\x85\xc0\x75\x04\x09\x74\x24\x0c\x8b\x44\x24\x0c\x5f\x5e\x5d\x59\xc3"},
    {0x59342e,32,"\x8b\x44\x24\x24\x85\xc0\x75\x18\x8b\x0e\x51\x8b\xcf\xe8\x30\x1d\xf0\xff\xf7\xd8\x50\x8b\xcf\xe8\xa6\x30\xf0\xff\x8b\x44\x24\x24"},
    {0x593548,177,"\x8b\x44\x24\x18\x85\xc0\x0f\x84\x8e\x00\x00\x00\x8b\x74\x24\x20\x56\xe8\xd2\x70\xee\xff\x83\xc4\x04\x85\xc0\x74\x7d\x8b\x5c\x24\x10\x6a\x00\x56\x6a\x54\x8b\xcb\xe8\x4b\x64\xff\xff\x56\x6a\x5f\x8b\xcb\xe8\x01\x64\xff\xff\x85\xf6\x74\x11\x8b\x3e\xe8\xd6\x58\xed\xff\x50\x8b\xce\xff\x57\x2c\x85\xc0\x75\x02\x33\xf6\x56\xe8\x94\x70\xee\xff\x83\xc4\x04\x85\xc0\x74\x43\x8b\xce\xe8\x96\x24\xf0\xff\x56\x8b\xf8\xe8\x4e\xd1\x00\x00\x83\xc4\x04\x85\xc0\x75\x2d\x85\xff\x74\x29\x68\x00\x00\x80\xbf\x57\x6a\x45\xb9\x10\x98\x11\x09\xe8\x01\xe4\xf3\xff\x8b\xcb\xe8\xfa\xaf\xfe\xff\x5f\x5e\x8b\xc5\x5d\x5b\x83\xc4\x0c\xc2\x0c\x00\x8b\x5c\x24\x10\x8b\xcb\xe8\xe3\xaf\xfe\xff\x5f\x5e\x8b\xc5\x5d\x5b\x83\xc4\x0c\xc2\x0c\x00"},
};
struct ReachMapImmediate {uint32_t at;int kind;};
const ReachMapImmediate g_reachMapImmediates[]={
    {0x5a3d0a,0}, // 005a3d08 2 4 69c0c8000000 W
    {0x5a3d1a,1}, // 005a3d17 3 4 8d34b5680efb06 B20
    {0x5a3ed3,1}, // 005a3ed2 1 4 b9680efb06 B20
    {0x5a3f07,0}, // 005a3f05 2 4 69c0c8000000 W
    {0x5a3f13,2}, // 005a3f10 3 4 8b0c856c0efb06 B20+4
};
bool verifyReachLayout(){
    for(const auto& s:g_reachLayouts){auto p=(const uint8_t*)(g_strategyCodeBase+s.start-0x400000);
        for(int n=0;n<s.size;++n){bool operand=false;
            for(const auto& m:g_reachMapImmediates)if(s.start+n>=m.at && s.start+n<m.at+4)operand=true;
            if(!operand && p[n]!=(uint8_t)s.bytes[n]){if(g_log)g_log("skills: unexpected reach layout at %08x; rejected",s.start+n);return false;}
        }
    }
    uint32_t width=*(const uint32_t*)(g_strategyCodeBase+0x5a078f-0x400000),base=*(const uint32_t*)(g_strategyCodeBase+0x5a07aa-0x400000);
    for(const auto& m:g_reachMapImmediates)if(*(const uint32_t*)(g_strategyCodeBase+m.at-0x400000)!=(m.kind==0?width:base+(m.kind==2?4:0)))return false;
    return true;
}
