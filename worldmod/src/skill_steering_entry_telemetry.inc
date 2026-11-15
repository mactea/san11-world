// Base entry bypasses the field-position commit. Follow only its owned friendly
// transfer to each contributing holder's changed current-location store.
typedef void(__cdecl* SteeringEntry)(int,uint32_t);
typedef int(__thiscall* SteeringTransfer)(void*,void*,void*);
typedef void(__thiscall* SteeringEnterPerson)(void*,void*,int);
SteeringEntry g_steeringEntryOriginal=(SteeringEntry)0x59f5f0;
SteeringTransfer g_steeringTransferOriginal=(SteeringTransfer)0x4bf1f0;
SteeringEnterPerson g_steeringEnterPersonOriginal=(SteeringEnterPerson)0x4a31e0;
struct SteeringEntryScope;
thread_local SteeringEntryScope* g_steeringEntry=nullptr;
struct SteeringEntryScope {
    uint64_t generation=skilltelemetry::contextGeneration();
    ZocRouteScope zoc;SteeringRoute route;bool recorded=false;
    SteeringEntryScope* previous;
    SteeringEntryScope(int id,uint32_t to,const uint32_t* command):zoc(id,to,1,command),route(id,to,1,command),previous(g_steeringEntry){g_steeringEntry=this;}
    ~SteeringEntryScope(){g_steeringEntry=previous;}
    void* unit() const{return zoc.count?zoc.unit:route.unit;}
    const uint32_t* snapshot() const{return zoc.count?zoc.snapshot:route.snapshot;}
    int force() const{return zoc.count?zoc.force:route.force;}
    bool live() const {
        return generation==skilltelemetry::contextGeneration() &&
            ((route.live() && *(const uint32_t*)(route.unit+0x3c)==route.snapshot[1]) || zoc.live(true));
    }
};
struct SteeringTransferScope;
thread_local SteeringTransferScope* g_steeringTransfer=nullptr;
struct SteeringTransferScope {
    SteeringEntryScope* root;void* unit;void* base;
    SteeringTransferScope* previous;
    SteeringTransferScope(SteeringEntryScope* r,void* u,void* b):root(r),unit(u),base(b),previous(g_steeringTransfer){g_steeringTransfer=this;}
    ~SteeringTransferScope(){g_steeringTransfer=previous;}
};
void __cdecl steeringBaseEntry(int id,uint32_t to,const uint32_t* command){
    SteeringEntryScope scope(id,to,command);g_steeringEntryOriginal(id,to);
}
__declspec(naked) void steeringBaseEntryBridge(){__asm {
    push esi
    push [esp+12]
    push [esp+12]
    call steeringBaseEntry
    add esp,12
    ret}}
int __fastcall steeringBaseTransfer(void* self,void*,void* unit,void* base){
    auto root=g_steeringEntry;
    bool observe=root && root->live() && root->unit()==unit;
    SteeringTransferScope scope(observe?root:nullptr,unit,base);
    return g_steeringTransferOriginal(self,unit,base);
}
bool steeringEntryDestination(const SteeringTransferScope* transfer,int baseId){
    if(!transfer || !transfer->root || transfer->root!=g_steeringEntry ||
       !transfer->root->live() || transfer->unit!=transfer->root->unit() || baseId<0 ||
       g_baseById((void*)0x7201958,baseId)!=transfer->base ||
       g_telemetryBaseId((void*)0x7201958,transfer->base)!=baseId || !g_growthValid(transfer->base))return false;
    auto root=transfer->root;auto cell=zocCell(root->snapshot()[2]);
    return *(const uint32_t*)((uint8_t*)root->unit()+0x3c)==root->snapshot()[1] && cell &&
        (*(const uint32_t*)cell&3)==2 && *(const uint16_t*)(cell+8)!=0xffff &&
        g_zocObjectFromCell((void*)cell)==transfer->base &&
        (*(BuildingValue*)(*(uint8_t**)transfer->base+0x40))(transfer->base)==root->force() && root->live();
}
void __fastcall steeringEnterPerson(void* self,void*,uint8_t* person,int baseId){
    auto transfer=g_steeringTransfer;
    auto root=transfer?transfer->root:nullptr;
    bool observe=root && ((!root->recorded && person==root->route.actor) || root->zoc.pending(person)) && steeringEntryDestination(transfer,baseId);
    int before=observe?*(int*)(person+0x98):-1;
    g_steeringEnterPersonOriginal(self,person,baseId);
    if(!observe || !steeringEntryDestination(transfer,baseId) || before==baseId || *(int*)(person+0x98)!=baseId)return;
    auto target=unitTarget(root->unit());
    target.base=baseId;target.city=*(int*)((uint8_t*)transfer->base+8)==0?baseId:-1;target.person=root->route.actorId;
    if(!root->recorded && person==root->route.actor && root->route.live()){
        root->recorded=true;root->route.record(0x4bf61f,&target);
    }
    root->zoc.finish(&target,person);
}
void __cdecl steeringTransferCommitted(void* unit,void* base){
    auto transfer=g_steeringTransfer;
    if(!transfer || !transfer->root || transfer->unit!=unit || transfer->base!=base)return;
    int id=g_telemetryBaseId((void*)0x7201958,base);
    if(!steeringEntryDestination(transfer,id))return;
    auto root=transfer->root;auto target=unitTarget(unit);
    target.base=id;target.city=*(int*)((uint8_t*)base+8)==0?id:-1;target.person=root->route.actorId;
    if(!root->recorded && root->route.live()){
        root->recorded=true;root->route.record(0x4bf636,&target,true);
    }
    root->zoc.finish(&target,nullptr,true);
}
uint32_t g_steeringTransferCommittedResume=0x4bf63c;
__declspec(naked) void steeringTransferCommittedBridge(){__asm {
    pushfd
    pushad
    push esi
    push edi
    call steeringTransferCommitted
    add esp,8
    popad
    popfd
    push edi
    mov ecx,0x7201958
    jmp dword ptr [g_steeringTransferCommittedResume]}}
