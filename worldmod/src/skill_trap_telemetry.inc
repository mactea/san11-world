// Trap packets live only for their native command owner. No global packet
// cache: a preview, reused stack address or late consumer cannot inherit them.
struct TrapObject {
    void* pointer=nullptr;int unit=-1,base=-1,force=-1,type=-1,weapon=-1,ids[3]={};uint32_t position=0;
    TrapObject()=default;
    explicit TrapObject(void* p):pointer(p){
        if(!p)return;unit=g_telemetryUnitId((void*)0x7201958,p);
        if(unit>=0){if(g_movementUnitById((void*)0x7201958,unit)!=p){unit=-1;pointer=nullptr;return;}}
        else {base=g_telemetryBaseId((void*)0x7201958,p);if(base<0 || g_baseById((void*)0x7201958,base)!=p){pointer=nullptr;return;}}
        type=*(const int*)((uint8_t*)p+8);force=(*(BuildingValue*)(*(uint8_t**)p+0x40))(p);
        typedef const uint32_t*(__thiscall* Position)(void*);position=*(*(Position*)(*(uint8_t**)p+0x3c))(p);
        if(unit>=0){weapon=*(const int*)((uint8_t*)p+0xa8);memcpy(ids,(uint8_t*)p+0xc,sizeof ids);}
    }
    bool live() const {
        if(!pointer || type<0 || (unit>=0?g_movementUnitById((void*)0x7201958,unit):g_baseById((void*)0x7201958,base))!=pointer ||
           *(const int*)((uint8_t*)pointer+8)!=type || (*(BuildingValue*)(*(uint8_t**)pointer+0x40))(pointer)!=force)return false;
        typedef const uint32_t*(__thiscall* Position)(void*);
        return *(*(Position*)(*(uint8_t**)pointer+0x3c))(pointer)==position &&
            (unit<0 || (*(const int*)((uint8_t*)pointer+0xa8)==weapon && !memcmp(ids,(uint8_t*)pointer+0xc,sizeof ids)));
    }
};
struct TrapStep {RecoveryActor actor;int actorId=-1,effect=-1,offset=0,before=0,after=0,affiliation[4]={};uintptr_t caller=0;bool offensive=false;};
struct TrapPacket {
    uint8_t* packet=nullptr;uint8_t snapshot[0x80]={};TrapObject source,target;TrapStep steps[3];
    int count=0,kind=-1;bool auxiliary=false,ready=false,consumed[2]={};
};
struct TrapOwner;
thread_local TrapOwner* g_trapOwner=nullptr;
struct TrapOwner {
    TrapPacket packets[2];int count=0,kind;uint64_t generation=skilltelemetry::contextGeneration();TrapOwner* previous;
    explicit TrapOwner(int k):kind(k),previous(g_trapOwner){g_trapOwner=this;}
    ~TrapOwner(){g_trapOwner=previous;}
    TrapPacket* allocate(uint8_t* packet){
        for(int n=0;n<count;++n)if(packets[n].packet==packet){packets[n]=TrapPacket{};packets[n].packet=packet;return &packets[n];}
        if(count==2)return nullptr;auto result=&packets[count++];result->packet=packet;return result;
    }
};
bool trapFireCalculation(){return g_trapCalculation && g_trapOwner && g_trapOwner->kind==3;}
struct TrapCalculation {
    TrapPacket* record;TrapStep pending;const void* candidate=nullptr;int candidateId=-1;
    bool matched=false,active=false;TrapCalculation* previous;
    explicit TrapCalculation(TrapPacket* r):record(r),previous(g_trapCalculation){g_trapCalculation=this;}
    ~TrapCalculation(){g_trapCalculation=previous;}
    void finish(){
        if(active && matched && pending.actor.person && record->count<3){
            pending.after=*(const int*)(record->packet+pending.offset);record->steps[record->count++]=pending;
        }
        active=matched=false;candidate=nullptr;
    }
};
uintptr_t trapOrigin(uintptr_t caller){
    return caller>=g_strategyCodeBase && caller-g_strategyCodeBase<0x4a0000?caller-g_strategyCodeBase+0x400000:caller;
}
void trapBeforeSkill(void* object,int effect,uintptr_t caller){
    auto c=g_trapCalculation;caller=trapOrigin(caller);bool offensive=caller==0x5b168c || caller==0x5b1271;
    if(!c || !c->record || !((effect==62 && (offensive || caller==0x5b1791 || caller==0x5b12a6)) ||
       (effect==31 && (caller==0x5b16a1 || caller==0x5b17a7 || caller==0x5b1289)) || (effect==6 && caller==0x5b17c6)))return;
    c->finish();auto r=c->record;
    if(object!=(offensive?r->source.pointer:r->target.pointer))return;
    c->pending=TrapStep{};c->pending.effect=effect;c->pending.caller=caller;c->pending.offensive=offensive;
    c->pending.offset=r->auxiliary || r->target.unit>=0?0xc:0x10;
    c->pending.before=*(const int*)(r->packet+c->pending.offset);c->active=true;
}
void trapPersonMatched(const void* person,int owned,int effect,uintptr_t caller){
    auto c=g_trapCalculation;if(!c || !c->active || c->candidate || c->pending.effect!=effect || c->pending.caller!=trapOrigin(caller))return;
    c->candidate=person;c->candidateId=growthPersonId((void*)person);
    c->pending.actor={person,owned,g_trapOwner->generation};
    memcpy(c->pending.affiliation,(const uint8_t*)person+0x98,sizeof c->pending.affiliation);
}
void trapAfterSkill(void*,int effect,int holder,uintptr_t caller){
    auto c=g_trapCalculation;if(!c || !c->active || c->pending.effect!=effect || c->pending.caller!=trapOrigin(caller))return;
    c->matched=holder>=0 && holder==c->candidateId;c->pending.actorId=c->candidateId;
}
void __cdecl trapStepStored(){if(g_trapCalculation)g_trapCalculation->finish();}
uint32_t g_trapStepResume=0x5b17e2;
__declspec(naked) void trapStepBridge(){__asm {mov [ebp],eax
    mov dword ptr [esi+0x64],6
    pushfd
    pushad
    call trapStepStored
    popad
    popfd
    jmp dword ptr [g_trapStepResume]}}
typedef void(__cdecl* TrapBuild)(uint8_t*,int,void*,void*,void*);
typedef void(__cdecl* TrapAuxBuild)(uint8_t*,void*,void*,void*);
uint32_t g_trapBuildResume=0x5b15a7,g_trapAuxResume=0x5b1197;
__declspec(naked) void trapBuildNative(){__asm {push ebx
    push ebp
    push esi
    mov esi,[esp+0x10]
    jmp dword ptr [g_trapBuildResume]}}
__declspec(naked) void trapAuxNative(){__asm {push ebx
    push ebp
    push esi
    mov esi,[esp+0x10]
    jmp dword ptr [g_trapAuxResume]}}
TrapBuild g_trapBuildOriginal=(TrapBuild)trapBuildNative;
TrapAuxBuild g_trapAuxOriginal=(TrapAuxBuild)trapAuxNative;
struct TrapPreviewScope {
    TrapCalculation* previous=g_trapCalculation;
    TrapPreviewScope(){g_trapCalculation=nullptr;}
    ~TrapPreviewScope(){g_trapCalculation=previous;}
};
void __cdecl trapBuild(uint8_t* packet,int kind,void* source,void* target,void* option){
    const uintptr_t callers[]={0x596277,0x597a1b,0x597b17};auto owner=g_trapOwner;
    auto r=skilltelemetry::enabled() && owner && owner->kind>=0 && owner->kind<3 && trapOrigin((uintptr_t)_ReturnAddress())==callers[owner->kind]?owner->allocate(packet):nullptr;
    if(!r){TrapPreviewScope preview;g_trapBuildOriginal(packet,kind,source,target,option);return;}
    r->kind=kind;r->source=TrapObject(source);r->target=TrapObject(target);TrapCalculation c(r);
    g_trapBuildOriginal(packet,kind,source,target,option);c.finish();memcpy(r->snapshot,packet,sizeof r->snapshot);r->ready=true;
}
void __cdecl trapAuxBuild(uint8_t* packet,void* source,void* target,void* option){
    auto owner=g_trapOwner;uintptr_t caller=trapOrigin((uintptr_t)_ReturnAddress());
    auto r=skilltelemetry::enabled() && owner && ((owner->kind==1 && caller==0x597a28) || (owner->kind==2 && caller==0x597b2e) ||
        (owner->kind==3 && caller==0x5924f8))?owner->allocate(packet):nullptr;
    if(!r){TrapPreviewScope preview;g_trapAuxOriginal(packet,source,target,option);return;}
    r->auxiliary=true;r->source=TrapObject(source);r->target=TrapObject(target);TrapCalculation c(r);
    g_trapAuxOriginal(packet,source,target,option);c.finish();memcpy(r->snapshot,packet,sizeof r->snapshot);r->ready=true;
}
typedef int(__thiscall* TrapRootFour)(void*,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
typedef int(__thiscall* TrapRootFive)(void*,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
uint32_t g_trapLineResume=0x5960d6,g_trapDirectResume=0x5979d6,g_trapCellResume=0x597a67;
__declspec(naked) void trapLineNative(){__asm {sub esp,0xa0
    jmp dword ptr [g_trapLineResume]}}
__declspec(naked) void trapDirectNative(){__asm {sub esp,0x104
    jmp dword ptr [g_trapDirectResume]}}
__declspec(naked) void trapCellNative(){__asm {push -1
    push 0x733a5b
    jmp dword ptr [g_trapCellResume]}}
TrapRootFour g_trapLineOriginal=(TrapRootFour)trapLineNative;
TrapRootFive g_trapDirectOriginal=(TrapRootFive)trapDirectNative,g_trapCellOriginal=(TrapRootFive)trapCellNative;
int __fastcall trapLine(void* self,void*,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){
    if(!skilltelemetry::enabled())return g_trapLineOriginal(self,a,b,c,d);
    TrapOwner owner(0);return g_trapLineOriginal(self,a,b,c,d);
}
int __fastcall trapDirect(void* self,void*,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e){
    if(!skilltelemetry::enabled())return g_trapDirectOriginal(self,a,b,c,d,e);
    TrapOwner owner(1);return g_trapDirectOriginal(self,a,b,c,d,e);
}
int __fastcall trapCell(void* self,void*,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e){
    if(!skilltelemetry::enabled())return g_trapCellOriginal(self,a,b,c,d,e);
    TrapOwner owner(2);return g_trapCellOriginal(self,a,b,c,d,e);
}
typedef void(__thiscall* TrapFire)(void*,const uint32_t*);
uint32_t g_trapFireResume=0x592447;
__declspec(naked) void trapFireNative(){__asm {push -1
    push 0x7339bb
    jmp dword ptr [g_trapFireResume]}}
TrapFire g_trapFireOriginal=(TrapFire)trapFireNative;
void __fastcall trapFire(void* self,void*,const uint32_t* packet){
    if(!skilltelemetry::enabled()){g_trapFireOriginal(self,packet);return;}
    TrapOwner owner(3);g_trapFireOriginal(self,packet);
}
bool trapPacketMatches(const TrapPacket& r,const uint8_t* packet,void* affected,int offset,bool common){
    if(!r.ready || r.packet!=packet || r.target.pointer!=affected || !r.target.live() || r.consumed[offset==0x10])return false;
    uint8_t expected[0x80];memcpy(expected,r.snapshot,sizeof expected);
    if(common && !r.auxiliary){*(uint32_t*)(expected+0x58)=r.source.position;*(uint32_t*)(expected+0x5c)=r.target.position;}
    return !memcmp(expected,packet,sizeof expected);
}
void trapEmit(const TrapPacket& r,int offset,int requested,int before,int after,uintptr_t caller){
    auto owner=g_trapOwner;if(!owner || owner->generation!=skilltelemetry::contextGeneration() || !r.target.live())return;
    for(int n=0;n<r.count;++n){const auto& s=r.steps[n];auto& actorObject=s.offensive?r.source:r.target;
        if(owner!=g_trapOwner || !skilltelemetry::enabled() || owner->generation!=skilltelemetry::contextGeneration())return;
        if(s.offset!=offset || !actorObject.live() || !recoveryActorValid(s.actor,s.effect) || growthPersonId((void*)s.actor.person)!=s.actorId ||
           memcmp(s.affiliation,(const uint8_t*)s.actor.person+0x98,sizeof s.affiliation))continue;
        if(actorObject.unit>=0 && !afterAttackMember(actorObject.pointer,s.actor.person))continue;
        auto target=r.target.unit>=0?unitTarget(r.target.pointer):incomeTarget(r.target.pointer);
        target.actorUnit=actorObject.unit;target.equipment=actorObject.weapon;target.x=(int16_t)r.target.position;target.y=(int16_t)(r.target.position>>16);
        bool fire=owner->kind==3;
        char scope[80];if(fire)strcpy_s(scope,"\"fire_consumer\":true");
        else sprintf_s(scope,"\"trap_kind\":%d,\"auxiliary_fire\":%s",r.kind,r.auxiliary?"true":"false");
        char context[480];sprintf_s(context,"{%s,\"native_modifier_caller\":%u,\"provider_role\":\"%s\",\"source_unit\":%d,\"source_base\":%d,\"source_force\":%d,\"actor_base\":%d,\"component_final_damage\":%d,\"requested_total_loss\":%d,\"resource_before\":%d,\"resource_after\":%d,\"damage_attribution\":\"native_calculation_step_with_whole_commit_context\",\"counterfactual_evaluated\":false}",
            scope,(unsigned)s.caller,s.offensive?"attacker":"defender",r.source.unit,r.source.base,r.source.force,actorObject.base,*(const int*)(r.snapshot+offset),-requested,before,after);target.context=context;
        skilltelemetry::applied(s.actor.person,s.actor.owned,s.effect,"battle",fire?"fire_damage_modifier_settled":"trap_damage_modifier_settled",
            fire?(offset==0x10?"calculated_fire_hp_damage":"calculated_fire_troop_damage"):
                (offset==0x10?"calculated_trap_hp_damage":"calculated_trap_troop_damage"),s.before,s.after,caller,target);
    }
}
int trapGrantObserved(void* self,void* affected,int requested,uintptr_t caller,const uint8_t* first,const uint8_t* second,IncomeGrant grant){
    auto owner=g_trapOwner;bool hp=caller==0x597de5 || caller==0x5b1aaf || caller==0x592636;int offset=hp?0x10:0xc;
    bool common=caller==0x5b1aaf || caller==0x5b1a74 || caller==0x5b1c6c;
    if(!owner || !skilltelemetry::enabled() || owner->generation!=skilltelemetry::contextGeneration())return grant(self,affected,requested);
    TrapPacket* records[2]={};for(int n=0;n<owner->count;++n){auto& r=owner->packets[n];
        if(r.packet==first && trapPacketMatches(r,first,affected,offset,common))records[0]=&r;
        if(second && r.packet==second && trapPacketMatches(r,second,affected,offset,false))records[1]=&r;
    }
    bool observe=records[0] && (!second || records[1]) && first!=second &&
        requested==-((int64_t)*(const int*)(first+offset)+(second?*(const int*)(second+offset):0));
    if(!observe)return grant(self,affected,requested);
    TrapPacket copies[2];for(int n=0;n<2;++n)if(records[n]){copies[n]=*records[n];records[n]->consumed[hp]=true;}
    auto value=hp?nullptr:*(BuildingValue*)(*(uint8_t**)affected+0x50);
    int before=hp?*(const int16_t*)((uint8_t*)affected+0x10):value(affected),result=grant(self,affected,requested);
    if(owner!=g_trapOwner || owner->generation!=skilltelemetry::contextGeneration() || !copies[0].target.live())return result;
    int after=hp?*(const int16_t*)((uint8_t*)affected+0x10):value(affected);
    if(owner->kind==3 && after!=std::max<int64_t>(0,(int64_t)before+requested))return result;
    for(int n=0;n<2;++n)if(records[n] && trapPacketMatches(copies[n],copies[n].packet,affected,offset,common && n==0))
        trapEmit(copies[n],offset,requested,before,after,caller);
    return result;
}
int __fastcall trapDirectGrant(void* self,void*,const uint8_t* first,const uint8_t* second,uintptr_t caller,void* target,int requested){
    auto grant=(caller==0x597de5 || caller==0x592636)?g_criticalBaseHpOriginal:caller==0x597e02?g_criticalBaseTroopsOriginal:g_unitTroopsOriginal;
    return trapGrantObserved(self,target,requested,caller,first,second,grant);
}
uint32_t g_trapUnitGrantResume=0x59787f,g_trapHpGrantResume=0x597dea,g_trapBaseGrantResume=0x597e07;
__declspec(naked) void trapUnitGrantBridge(){__asm {push 0x59787a
    push edx
    push ebp
    call trapDirectGrant
    jmp dword ptr [g_trapUnitGrantResume]}}
__declspec(naked) void trapHpGrantBridge(){__asm {lea eax,[esp+0xd8]
    lea edx,[esp+0x58]
    push 0x597de5
    push eax
    push edx
    call trapDirectGrant
    jmp dword ptr [g_trapHpGrantResume]}}
__declspec(naked) void trapBaseGrantBridge(){__asm {lea eax,[esp+0xd8]
    lea edx,[esp+0x58]
    push 0x597e02
    push eax
    push edx
    call trapDirectGrant
    jmp dword ptr [g_trapBaseGrantResume]}}
uint32_t g_trapFireUnitResume=0x59253d,g_trapFireHpResume=0x59263b;
__declspec(naked) void trapFireUnitGrantBridge(){__asm {lea eax,[esp+0x24]
    push 0x592538
    push 0
    push eax
    call trapDirectGrant
    jmp dword ptr [g_trapFireUnitResume]}}
__declspec(naked) void trapFireHpGrantBridge(){__asm {lea eax,[esp+0x24]
    push 0x592636
    push 0
    push eax
    call trapDirectGrant
    jmp dword ptr [g_trapFireHpResume]}}

const struct TrapLayout {uint32_t at;int size;const char* bytes;} g_trapLayouts[]={
    {0x5924f3,5,"\xe8\x98\xec\x01\x00"},
    {0x592525,19,"\x8b\x44\x24\x28\xf7\xde\x1b\xf6\xf7\xd8\x50\x23\xf7\x56\xb9\x5c\x89\x99\x07"},
    {0x592629,13,"\x8b\x44\x24\x2c\xf7\xd8\x50\x56\xb9\x5c\x89\x99\x07"},
    {0x596272,5,"\xe8\x29\xb3\x01\x00"},
    {0x597a16,5,"\xe8\x85\x9b\x01\x00"},
    {0x597b12,5,"\xe8\x89\x9a\x01\x00"},
    {0x597a23,5,"\xe8\x68\x97\x01\x00"},
    {0x597b29,5,"\xe8\x62\x96\x01\x00"},
    {0x5b168c,12,"\x85\xc0\x74\x08\x8b\x45\x00\xd1\xe0\x89\x45\x00"},
    {0x5b16a1,18,"\x85\xc0\x0f\x84\xdf\x00\x00\x00\x8b\x45\x00\xd1\xe0\xe9\x00\x01\x00\x00"},
    {0x5b1791,13,"\x85\xc0\x74\x28\xc7\x45\x00\x00\x00\x00\x00\xeb\x1f"},
    {0x5b17a7,22,"\x85\xc0\x74\x12\x8b\x45\x00\x99\x2b\xc2\xd1\xf8\x89\x45\x00\xc7\x46\x64\x1f\x00\x00\x00"},
    {0x5b17c6,18,"\x85\xc0\x74\x18\xdb\x45\x00\xd8\x0d\x88\xcc\x79\x00\xe8\x9c\x62\x15\x00"},
    {0x5b1271,15,"\x85\xc0\x74\x0b\x8b\x4e\x0c\xd1\xe1\x89\x4e\x0c\x89\x6e\x60"},
    {0x5b1289,43,"\x85\xc0\x74\x11\x8b\x46\x0c\xd1\xe0\x89\x46\x0c\xc7\x46\x64\x1f\x00\x00\x00\xeb\x16\x8b\x17\x55\x8b\xcf\xff\x52\x58\x85\xc0\x74\x0a\xc7\x46\x0c\x00\x00\x00\x00\x89\x6e\x64"},
    {0x59786a,16,"\x8b\x4d\x0c\x57\x03\x4a\x0c\xf7\xd9\x51\x56\xb9\x5c\x89\x99\x07"},
    {0x597dcf,22,"\x8b\x54\x24\x60\x8b\x84\x24\xe0\x00\x00\x00\x03\xc2\xf7\xd8\x50\x56\xb9\x5c\x89\x99\x07"},
    {0x597dea,24,"\x8b\x4c\x24\x5c\x8b\x94\x24\xdc\x00\x00\x00\x03\xd1\xf7\xda\x52\x56\xb9\x5c\x89\x99\x07\x8b\xf8"},
};
bool verifyTrapLayout(){
    for(auto& g:g_trapLayouts)if(memcmp((const void*)(g_strategyCodeBase+g.at-0x400000),g.bytes,g.size))return false;
    return true;
}
