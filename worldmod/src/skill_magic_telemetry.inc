struct MagicRecord {const uint8_t* packet=nullptr;void* target=nullptr;uint8_t bytes[0x80]={};};
struct MagicScope;
thread_local MagicScope* g_magic=nullptr;
struct MagicScope {
    int effect,sourceId=-1,force=-1,actorId=-1,owned=-1,success,critical;
    void* source=nullptr;const void* actor=nullptr;uint32_t position;
    uint64_t generation;MagicRecord records[8];MagicScope* previous;
    MagicScope(int e,const uint32_t* command,int ok,int crit):effect(e),success(ok),critical(crit),position(command[2]),
        generation(skilltelemetry::contextGeneration()),previous(g_magic){
        sourceId=(int)command[1];source=g_movementUnitById((void*)0x7201958,sourceId);
        if(source && g_telemetryUnitId((void*)0x7201958,source)==sourceId)force=unitTarget(source).force;
        g_magic=this;
    }
    ~MagicScope(){g_magic=previous;}
};
bool magicValid(const MagicScope& s){
    return skilltelemetry::enabled() && s.generation==skilltelemetry::contextGeneration() && s.actor && s.source &&
        s.sourceId>=0 && g_movementUnitById((void*)0x7201958,s.sourceId)==s.source && unitTarget(s.source).force==s.force &&
        growthPersonId((void*)s.actor)==s.actorId && *(const int*)((const uint8_t*)s.actor+0xe8)==s.owned &&
        contains(s.owned,s.effect) && afterAttackMember(s.source,s.actor);
}
typedef int(__thiscall* MagicCommand)(void*,const uint32_t*,int,int,int,void*,int);
uint32_t g_magicConfuseResume=0x592057,g_magicThunderResume=0x592ed6;
__declspec(naked) void magicConfuseNative(){__asm {sub esp,0x40c
    push ebx
    jmp dword ptr [g_magicConfuseResume]}}
__declspec(naked) void magicThunderNative(){__asm {push ebp
    mov ebp,esp
    and esp,-8
    jmp dword ptr [g_magicThunderResume]}}
MagicCommand g_magicConfuseOriginal=(MagicCommand)magicConfuseNative,g_magicThunderOriginal=(MagicCommand)magicThunderNative;
int magicCommand(int effect,MagicCommand native,void* self,const uint32_t* command,int success,int critical,int mode,void* extra,int flag){
    if(!skilltelemetry::enabled())return native(self,command,success,critical,mode,extra,flag);
    MagicScope scope(effect,command,success,critical);int result=native(self,command,success,critical,mode,extra,flag);
    if(magicValid(scope)){
        skilltelemetry::Target target;target.actorUnit=scope.sourceId;target.equipment=*(const int*)((const uint8_t*)scope.source+0xa8);
        target.x=(int16_t)scope.position;target.y=(int16_t)(scope.position>>16);char context[192];
        sprintf_s(context,"{\"native_success_argument\":%d,\"critical_argument\":%d,\"native_return\":%d,\"result_scope\":\"command_return_not_per_target_effect\",\"counterfactual_evaluated\":false}",success,critical,result);
        target.context=context;skilltelemetry::appliedEvent(scope.actor,scope.owned,effect,"strategy","magic_command_resolved",effect==70?0x592050:0x592ed0,target);
    }
    return result;
}
int __fastcall magicConfuse(void* self,void*,const uint32_t* command,int a,int b,int c,void* d,int e){return magicCommand(70,g_magicConfuseOriginal,self,command,a,b,c,d,e);}
int __fastcall magicThunder(void* self,void*,const uint32_t* command,int a,int b,int c,void* d,int e){return magicCommand(71,g_magicThunderOriginal,self,command,a,b,c,d,e);}
int __fastcall magicFind(void* unit,void*,int effect){
    typedef int(__thiscall* Find)(void*,int);auto native=*(Find*)(*(uint8_t**)unit+0x5c);auto s=g_magic;
    QueryOrigin origin(effect==70?0x5920c4:0x592f1e);
    if(!s || s->source!=unit || s->effect!=effect)return native(unit,effect);
    ProgressionProbe probe(effect);probe.scene="strategy";probe.event="magic_eligibility";int result=native(unit,effect);
    auto person=result>=0?g_telemetryPerson((void*)0x7201958,result):nullptr;
    if(person && person==probe.actor && growthPersonId(person)==result){s->actor=person;s->actorId=result;s->owned=*(const int*)(person+0xe8);}
    return result;
}
uint32_t g_magicConfuseFindResume=0x5920ca,g_magicThunderFindResume=0x592f24;
__declspec(naked) void magicConfuseFindBridge(){__asm {call magicFind
    push eax
    mov ecx,0x7201958
    jmp dword ptr [g_magicConfuseFindResume]}}
__declspec(naked) void magicThunderFindBridge(){__asm {call magicFind
    push eax
    mov ecx,0x7201958
    jmp dword ptr [g_magicThunderFindResume]}}
uintptr_t g_magicDecisionEntry=0x591fd0;
struct MagicDecisionUnit {
    void* unit=nullptr;int id=-1,force=-1,type=-1,weapon=-1,intelligence=-1,ids[3]={};uint32_t position=0;
    explicit MagicDecisionUnit(void* u):unit(u){
        if(!unit || !g_growthValid(unit))return;
        id=g_telemetryUnitId((void*)0x7201958,unit);
        if(id<0 || g_movementUnitById((void*)0x7201958,id)!=unit){id=-1;return;}
        force=unitTarget(unit).force;type=*(int*)((uint8_t*)unit+8);weapon=*(int*)((uint8_t*)unit+0xa8);
        intelligence=((uint8_t*)unit)[0xc6];memcpy(ids,(uint8_t*)unit+0xc,sizeof ids);
        typedef const uint32_t*(__thiscall* Position)(void*);position=*(*(Position*)(*(uint8_t**)unit+0x3c))(unit);
    }
    bool live()const{
        if(id<0 || g_movementUnitById((void*)0x7201958,id)!=unit || !g_growthValid(unit) ||
           unitTarget(unit).force!=force || *(int*)((uint8_t*)unit+8)!=type || *(int*)((uint8_t*)unit+0xa8)!=weapon ||
           ((uint8_t*)unit)[0xc6]!=intelligence || memcmp(ids,(uint8_t*)unit+0xc,sizeof ids))return false;
        typedef const uint32_t*(__thiscall* Position)(void*);return *(*(Position*)(*(uint8_t**)unit+0x3c))(unit)==position;
    }
};
struct MagicDecisionProbe {
    MagicDecisionUnit source,target;RecoveryActor actor;int actorId=-1,effect=-1,affiliation[5]={};
    uintptr_t caller=0;bool defensive=false;uint64_t generation;MagicDecisionProbe* previous;
    MagicDecisionProbe(void* s,void* t):source(s),target(t),generation(skilltelemetry::contextGeneration()),previous(g_magicDecisionProbe){g_magicDecisionProbe=this;}
    ~MagicDecisionProbe(){g_magicDecisionProbe=previous;}
};
void magicDecisionMatched(void* unit,int effect,int holder,uintptr_t caller){
    auto p=g_magicDecisionProbe;if(!p || p->actor.person || p->generation!=skilltelemetry::contextGeneration())return;
    uintptr_t nativeCaller=caller-g_strategyCodeBase+0x400000;bool defensive;
    if(nativeCaller==0x591fd9 && effect==61)defensive=true;
    else if((nativeCaller==0x591ffa && effect==57) || (nativeCaller==0x592007 && effect==63))defensive=false;
    else if((nativeCaller==0x59201c && effect==60) || (nativeCaller==0x592029 && effect==63))defensive=true;
    else return;
    if(unit!=(defensive?p->target.unit:p->source.unit))return;
    auto person=g_telemetryPerson((void*)0x7201958,holder);
    if(!person || growthPersonId(person)!=holder || !afterAttackMember(unit,person))return;
    auto actor=recoveryActor(person);if(!recoveryActorValid(actor,effect))return;
    p->actor=actor;p->actorId=holder;p->effect=effect;p->caller=nativeCaller;p->defensive=defensive;
    memcpy(p->affiliation,person+0x94,sizeof p->affiliation);
}
__declspec(naked) int __cdecl magicDecisionNative(void*,void*){__asm {push esi
    push edi
    mov edi,[esp+12]
    mov esi,[esp+16]
    call dword ptr [g_magicDecisionEntry]
    pop edi
    pop esi
    ret}}
int __cdecl magicDecision(void* source,void* target){
    auto s=g_magic;
    if(!s || s->effect!=70 || !s->success || s->source!=source || !magicValid(*s))return magicDecisionNative(source,target);
    MagicDecisionProbe probe(source,target);
    int result=magicDecisionNative(source,target);
    if(probe.actor.person && probe.generation==skilltelemetry::contextGeneration() && magicValid(*s) &&
       probe.source.live() && probe.target.live() && growthPersonId((void*)probe.actor.person)==probe.actorId &&
       recoveryActorValid(probe.actor,probe.effect) && !memcmp(probe.affiliation,(const uint8_t*)probe.actor.person+0x94,sizeof probe.affiliation) &&
       result==(probe.defensive?0:1)){
        auto to=unitTarget(target);to.actorUnit=probe.defensive?probe.target.id:probe.source.id;
        to.equipment=probe.defensive?probe.target.weapon:probe.source.weapon;
        to.x=(int16_t)probe.target.position;to.y=(int16_t)(probe.target.position>>16);char context[384];
        sprintf_s(context,"{\"strategy_id\":7,\"attacking_unit\":%d,\"attacking_force\":%d,\"source_intelligence\":%d,\"target_intelligence\":%d,\"native_result\":%d,\"defensive\":%s,\"result_scope\":\"per_target_decision_not_status_store\",\"counterfactual_evaluated\":false}",
            probe.source.id,probe.source.force,probe.source.intelligence,probe.target.intelligence,result,probe.defensive?"true":"false");to.context=context;
        skilltelemetry::appliedEvent(probe.actor.person,probe.actor.owned,probe.effect,"strategy",
            probe.defensive?"magic_target_immunity_forced_failure":"magic_target_forced_success",probe.caller,to);
    }
    if(s && s->effect==70 && s->source==source && magicValid(*s)){
        auto to=unitTarget(target);
        if(to.unit>=0 && g_movementUnitById((void*)0x7201958,to.unit)==target){
            to.actorUnit=s->sourceId;to.equipment=*(const int*)((const uint8_t*)source+0xa8);
            typedef const uint32_t*(__thiscall* Position)(void*);uint32_t position=*(*(Position*)(*(uint8_t**)target+0x3c))(target);
            to.x=(int16_t)position;to.y=(int16_t)(position>>16);char context[192];
            sprintf_s(context,"{\"native_result\":%d,\"source_intelligence\":%u,\"target_intelligence\":%u,\"result_scope\":\"per_target_decision_not_status_store\",\"counterfactual_evaluated\":false}",
                result,((uint8_t*)source)[0xc6],((uint8_t*)target)[0xc6]);to.context=context;
            skilltelemetry::appliedEvent(s->actor,s->owned,70,"strategy","magic_target_decision",0x592246,to);
        }
    }
    return result;
}
__declspec(naked) void magicDecisionBridge(){__asm {push esi
    push edi
    call magicDecision
    add esp,8
    ret}}
StatusGrant g_magicStatusOriginal=(StatusGrant)0x4aea70;
void __fastcall magicStatus(void* self,void*,uint8_t* unit,void* source,int status,int turns,int display){
    auto s=g_magic;bool observe=s && s->effect==70 && s->success && s->source==source && magicValid(*s);
    auto target=observe?unitTarget(unit):skilltelemetry::Target{};
    observe=observe && target.unit>=0 && g_movementUnitById((void*)0x7201958,target.unit)==unit;
    int before=observe?unit[0x28]:0,oldStatus=observe?*(int*)(unit+0x24):0;
    StatusWriteProbe write(observe?unit:nullptr);g_magicStatusOriginal(self,unit,source,status,turns,display);
    if(observe && write.written && magicValid(*s) && *(int*)(unit+0x24)==status){
        target.actorUnit=s->sourceId;target.equipment=*(const int*)((const uint8_t*)source+0xa8);
        typedef const uint32_t*(__thiscall* Position)(void*);auto position=*(*(Position*)(*(uint8_t**)unit+0x3c))(unit);
        target.x=(int16_t)position;target.y=(int16_t)(position>>16);char context[192];
        sprintf_s(context,"{\"status_before\":%d,\"status_after\":%d,\"requested_turns\":%d,\"critical_argument\":%d,\"counterfactual_evaluated\":false}",oldStatus,status,(int)(uint8_t)turns,s->critical);target.context=context;
        skilltelemetry::applied(s->actor,s->owned,70,"strategy","magic_status_written","status_turns",before,unit[0x28],0x5922b4,target);
    }
}
typedef int(__cdecl* MagicBuild)(uint8_t*,void*,void*,int);
MagicBuild g_magicThunderBuildOriginal=(MagicBuild)0x5b1330;
MagicRecord* magicRecord(const uint8_t* packet){if(g_magic)for(auto& r:g_magic->records)if(r.packet==packet)return &r;return nullptr;}
int __cdecl magicThunderBuild(uint8_t* packet,void* source,void* target,int critical){
    int result=g_magicThunderBuildOriginal(packet,source,target,critical);auto s=g_magic;
    if(!s || s->effect!=71 || !s->success || s->source!=source || !magicValid(*s))return result;
    auto r=magicRecord(packet);if(!r)for(auto& slot:s->records)if(!slot.packet){r=&slot;break;}
    if(!r){skilltelemetry::close();if(g_log)g_log("skill telemetry: unexpected lightning record lifetime; logging disabled");return result;}
    r->packet=packet;r->target=target;memcpy(r->bytes,packet,sizeof r->bytes);return result;
}
void __cdecl magicThunderAdjusted(const uint8_t* damage,int value){
    auto s=g_magic;auto r=magicRecord(damage-0x10);
    if(!s || s->effect!=71 || !r || !magicValid(*s))return;
    if(memcmp(r->packet,r->bytes,sizeof r->bytes)){r->packet=nullptr;return;}
    *(int*)(r->bytes+0x10)=value;
}
uint32_t g_magicThunderAdjustedResume=0x593202;
__declspec(naked) void magicThunderAdjustedBridge(){__asm {pushfd
    pushad
    push ecx
    push edi
    call magicThunderAdjusted
    add esp,8
    popad
    popfd
    mov [edi],ecx
    mov [esp+0x18],eax
    jmp dword ptr [g_magicThunderAdjustedResume]}}
bool magicGrantMatches(void* source,const uint8_t* packet,void* affected,int requested,uintptr_t caller){
    auto s=g_magic;if(!s || s->effect!=71 || !s->success || s->source!=source || !magicValid(*s))return false;
    auto r=magicRecord(packet);return r && r->target==affected && !memcmp(r->bytes,packet,sizeof r->bytes) &&
        (caller==0x5b1c6c || caller==0x5b1a74 || caller==0x5b1aaf) &&
        requested==-*(const int*)(packet+(caller==0x5b1aaf?0x10:0xc));
}
void magicGrantCommitted(const uint8_t* packet,int requested,int before,int after,uintptr_t caller,skilltelemetry::Target target){
    auto s=g_magic;uint32_t position=*(const uint32_t*)(packet+0x5c);target.x=(int16_t)position;target.y=(int16_t)(position>>16);
    char context[192];sprintf_s(context,"{\"requested_loss\":%d,\"critical_argument\":%d,\"damage_attribution\":\"whole_lightning_effect\",\"counterfactual_evaluated\":false}",-requested,s->critical);target.context=context;
    skilltelemetry::applied(s->actor,s->owned,71,"strategy","magic_lightning_resource_written",caller==0x5b1aaf?"facility_hp":"troops",before,after,caller,target);
}

struct MagicLayout {uint32_t start;int size;const char* bytes;};
const MagicLayout g_magicLayouts[]={
    {0x576b10,87,"\x0f\xbf\x44\x24\x04\x8b\x4c\x24\x04\x69\xc0\xc8\x00\x00\x00\xc1\xf9\x10\x03\xc1\x8d\x0c\x80\x8b\x14\x8d\x68\x0e\xfb\x06\x8d\x0c\x8d\x68\x0e\xfb\x06\x80\xe2\x03\x80\xfa\x02\x74\x03\x33\xc0\xc3\x56\xe8\xda\xcf\xf0\xff\x8b\xf0\x56\xe8\xe2\x3a\xf0\xff\x83\xc4\x04\x85\xc0\x74\x0e\x8b\x46\x08\x85\xc0\x75\x07\xb8\x01\x00\x00\x00\x5e\xc3\x33\xc0\x5e\xc3"},
    {0x592050,141,"\x81\xec\x0c\x04\x00\x00\x53\x55\x8b\xac\x24\x18\x04\x00\x00\x8b\x45\x08\x89\x44\x24\x08\x0f\xbf\xc0\x69\xc0\xc8\x00\x00\x00\x89\x4c\x24\x10\x8b\x4c\x24\x08\xc1\xf9\x10\x03\xc1\x8d\x14\x80\x8d\x04\x95\x68\x0e\xfb\x06\x56\x89\x44\x24\x10\x8b\x45\x04\x57\x50\xb9\x58\x19\x20\x07\xe8\xd6\xed\xef\xff\x8b\xf8\x8b\x17\x8b\xcf\xff\x52\x40\x50\xb9\x58\x19\x20\x07\xe8\xf2\xe9\xef\xff\x8b\xcf\x89\x44\x24\x2c\xe8\x87\x39\xf0\xff\x8b\xd8\x8b\x07\x6a\x46\x8b\xcf\xff\x50\x5c\x50\xb9\x58\x19\x20\x07\xe8\x31\xea\xef\xff\x8b\xf0\x85\xf6\x75\x02\x8b\xf3\x55\xe8\xe3\xee\xff\xff"},
    {0x592178,365,"\x0f\x84\x63\x02\x00\x00\x8b\x45\x08\x57\x8b\xc8\x51\x8d\x54\x24\x3c\x52\x89\x44\x24\x3c\xe8\x9d\xfd\xff\xff\x8b\xd8\x33\xc0\x89\x44\x24\x1c\x89\x44\x24\x2c\x89\x44\x24\x28\x8b\x44\x24\x20\x0f\xb7\x40\x08\x83\xc4\x0c\x43\x50\xb9\x58\x19\x20\x07\x89\x5c\x24\x28\xe8\xb2\xec\xef\xff\x8b\xf0\x56\xe8\x6a\x84\xee\xff\x83\xc4\x04\x85\xc0\x74\x2a\x56\xe8\x2d\xe5\x00\x00\x83\xc4\x04\x85\xc0\x75\x1d\x8b\x4c\x24\x18\x56\x6a\x51\xe8\x9a\x77\xff\xff\x8b\x4c\x24\x18\x68\xd0\x07\x00\x00\x56\x6a\x3f\xe8\xc9\x77\xff\xff\x85\xdb\xc7\x44\x24\x14\x00\x00\x00\x00\x0f\x8e\x66\x01\x00\x00\xeb\x07\x8d\xa4\x24\x00\x00\x00\x00\x8b\x4c\x24\x14\x8b\x44\x8c\x30\x89\x44\x24\x28\x8b\x54\x24\x28\x0f\xbf\xc0\x69\xc0\xc8\x00\x00\x00\xc1\xfa\x10\x03\xc2\x8d\x04\x80\x0f\xb7\x04\x85\x70\x0e\xfb\x06\x50\xb9\x58\x19\x20\x07\xe8\x2c\xec\xef\xff\x8b\xf0\xe8\x85\xfd\xff\xff\x85\xc0\x74\x79\x8b\x44\x24\x20\x85\xc0\x75\x04\x89\x74\x24\x20\x56\xe8\x9f\xe4\x00\x00\x83\xc4\x04\x85\xc0\x75\x1d\x8b\x5c\x24\x18\x56\x6a\x54\x8b\xcb\xe8\x0a\x77\xff\xff\x68\xf4\x01\x00\x00\x56\x6a\x44\x8b\xcb\xe8\x3b\x77\xff\xff\x8b\x94\x24\x28\x04\x00\x00\x33\xdb\x85\xd2\x0f\x95\xc3\x6a\x32\x83\xc3\x02\xe8\x33\xff\xed\xff\x83\xc4\x04\x85\xc0\x6a\x01\xb9\x5c\x89\x99\x07\x53\x74\x04\x6a\x02\xeb\x02\x6a\x01\x57\x56\xe8\xb7\xc7\xf1\xff\x8b\x44\x24\x10\x8b\x5c\x24\x24\x40\x89\x44\x24\x10\xeb\x0c\x8b\x44\x24\x1c\x85\xc0\x75\x04\x89\x74\x24\x1c\x8b\x44\x24\x14\x40\x3b\xc3\x89\x44\x24\x14\x0f\x8c\x2b\xff\xff\xff"},
    {0x591fd0,122,"\x8b\x06\x6a\x3d\x8b\xce\xff\x50\x58\x85\xc0\x75\x50\x0f\xb6\x87\xc6\x00\x00\x00\x0f\xb6\x8e\xc6\x00\x00\x00\x2b\xc1\x85\xc0\x7e\x20\x8b\x17\x6a\x39\x8b\xcf\xff\x52\x58\x85\xc0\x75\x0d\x8b\x07\x6a\x3f\x8b\xcf\xff\x50\x58\x85\xc0\x74\x25\xb8\x01\x00\x00\x00\xc3\x7d\x1d\x8b\x16\x6a\x3c\x8b\xce\xff\x52\x58\x85\xc0\x75\x0d\x8b\x06\x6a\x3f\x8b\xce\xff\x50\x58\x85\xc0\x74\x03\x33\xc0\xc3\x0f\xb6\x8e\xc6\x00\x00\x00\xd1\xe9\xba\x64\x00\x00\x00\x2b\xd1\x52\xe8\x8a\x01\xee\xff\x83\xc4\x04\xc3"},
    {0x592ed0,106,"\x55\x8b\xec\x83\xe4\xf8\x81\xec\x54\x08\x00\x00\x8b\x45\x08\x8b\x40\x04\x53\x56\x57\x89\x4c\x24\x1c\x50\xb9\x58\x19\x20\x07\xe8\x7c\xdf\xef\xff\x8b\xf0\x8b\xce\xe8\x43\x2b\xf0\xff\x8b\x16\x8b\xce\x8b\xd8\xff\x52\x40\x50\xb9\x58\x19\x20\x07\xe8\x8f\xdb\xef\xff\x89\x44\x24\x24\x8b\x06\x6a\x47\x8b\xce\xff\x50\x5c\x50\xb9\x58\x19\x20\x07\xe8\xd7\xdb\xef\xff\x8b\xf8\x85\xff\x75\x02\x8b\xfb\x8b\x4d\x08\x51\xe8\x86\xe0\xff\xff"},
    {0x593067,496,"\x8b\x43\x08\x83\xc4\x0c\x50\xe8\x9d\x3a\xfe\xff\x89\x44\x24\x1c\x8b\x43\x08\x89\x44\x24\x14\x8b\x4c\x24\x14\x0f\xbf\xc0\x69\xc0\xc8\x00\x00\x00\xc1\xf9\x10\x03\xc1\x8d\x0c\x80\x83\xc4\x04\x8d\x0c\x8d\x68\x0e\xfb\x06\xe8\x9e\x0a\xef\xff\x8b\xd0\x33\xc0\xb9\xe0\x00\x00\x00\x8d\xbc\x24\xc0\x00\x00\x00\x52\x89\x54\x24\x14\xf3\xab\xe8\x72\x75\xee\xff\x83\xc4\x04\x85\xc0\x74\x1a\x8b\x55\x10\x8b\x44\x24\x10\x52\x50\x8d\x8c\x24\xc8\x00\x00\x00\x56\x51\xe8\x54\xe2\x01\x00\x83\xc4\x10\x8b\x53\x08\x52\x6a\x01\x8d\x44\x24\x30\x50\xb9\x68\x0e\xfb\x06\xe8\x8c\x0b\xef\xff\x8b\xf8\x33\xdb\x85\xff\x89\x7c\x24\x20\x0f\x8e\x55\x01\x00\x00\x89\x5c\x24\x10\xc7\x44\x24\x0c\x80\x00\x00\x00\x8b\x44\x9c\x28\x89\x44\x24\x14\x8b\x4c\x24\x14\x0f\xbf\xc0\x69\xc0\xc8\x00\x00\x00\xc1\xf9\x10\x03\xc1\x8d\x0c\x80\x8d\x0c\x8d\x68\x0e\xfb\x06\xe8\x07\x0a\xef\xff\x8b\xf8\x8b\x44\x9c\x28\x89\x44\x24\x14\x8b\x54\x24\x14\x0f\xbf\xc0\x69\xc0\xc8\x00\x00\x00\xc1\xfa\x10\x03\xc2\x8d\x0c\x80\x8b\x04\x8d\x68\x0e\xfb\x06\x8d\x0c\x8d\x68\x0e\xfb\x06\x24\x03\x3c\x02\x0f\x85\x94\x00\x00\x00\xe8\xab\x09\xef\xff\x50\x89\x44\x24\x18\xe8\xb1\x74\xee\xff\x83\xc4\x04\x85\xc0\x0f\x84\x7a\x00\x00\x00\x8b\x4c\x24\x14\x8b\x41\x08\x85\xc0\x75\x6f\x8b\x44\x24\x18\x85\xc0\x75\x29\x8b\x55\x10\x52\x57\x8b\x7c\x24\x14\x8d\x84\x3c\xc8\x00\x00\x00\x56\x50\xe8\x7c\xe1\x01\x00\x8b\x44\x24\x28\x83\xc4\x10\x40\x89\x7c\x24\x10\x89\x44\x24\x18\xeb\x76\x8b\x4d\x10\x51\x57\x8d\x54\x24\x48\x56\x52\xe8\x5a\xe1\x01\x00\xdb\x44\x24\x60\x8b\x7c\x24\x20\x83\xc4\x10\xd8\x0d\x88\xcc\x79\x00\x8d\xbc\x3c\xd0\x00\x00\x00\xe8\x81\x48\x17\x00\x8b\x0f\x03\xc8\x8b\x44\x24\x18\x40\x89\x0f\x89\x44\x24\x18\xeb\x38\x57\xe8\x26\x74\xee\xff\x83\xc4\x04\x85\xc0\x74\x2b\x57\xe8\xc9\xc9\x01\x00\x83\xc4\x04\x85\xc0\x75\x1e\x3b\xf7\x74\x1a\x8b\x45\x10\x8b\x4c\x24\x0c\x50\x57\x8d\x94\x0c\xc8\x00\x00\x00\x56\x52\xe8\xf7\xe0\x01\x00\x83\xc4\x10\x8b\x4c\x24\x0c\x8b\x44\x24\x20\x43\x81\xc1\x80\x00\x00\x00\x3b\xd8\x89\x4c\x24\x0c\x0f\x8c\xb9\xfe\xff\xff"},
    {0x593259,116,"\x8b\x4d\x10\x8b\x5d\x08\x8b\x53\x08\x8d\x84\x24\xc0\x00\x00\x00\x50\x51\x8b\x4c\x24\x24\x52\x56\xe8\xea\xfa\xff\xff\x85\xff\x89\x44\x24\x18\x7e\x4f\x8b\x44\x24\x20\x8d\x5c\x24\x28\x8d\xbc\x24\x50\x01\x00\x00\x89\x44\x24\x10\x8b\x47\xfc\x85\xc0\x75\x05\x83\x3f\x00\x74\x19\x8b\x55\x10\x8b\x03\x8d\x4f\xf0\x51\x8b\x4c\x24\x20\x52\x50\x56\xe8\xae\xfa\xff\xff\x01\x44\x24\x18\x8b\x44\x24\x10\x81\xc7\x80\x00\x00\x00\x83\xc3\x04\x48\x89\x44\x24\x10\x75\xc7\x8b\x5d\x08"},
    {0x5b1330,231,"\x53\x56\x8b\x74\x24\x0c\x57\x83\xcb\xff\x33\xc0\xb9\x20\x00\x00\x00\x8b\xfe\xf3\xab\x8b\x7c\x24\x18\x89\x5e\x08\x89\x5e\x50\x89\x5e\x60\x89\x5e\x68\x89\x5e\x64\x89\x5e\x6c\xa1\xd0\xc1\x79\x00\x89\x46\x74\x8b\x0d\xd0\xc1\x79\x00\x89\x4e\x78\xc7\x46\x7c\x01\x00\x00\x00\x8b\x17\x6a\x0b\x8b\xcf\xff\x52\x2c\x85\xc0\x74\x1a\x68\xe8\x03\x00\x00\xe8\xc6\x0d\xec\xff\x0f\xb7\xc0\x83\xc4\x04\x05\xdc\x05\x00\x00\x89\x46\x0c\xeb\x19\x68\x2c\x01\x00\x00\xe8\xac\x0d\xec\xff\x0f\xb7\xc8\x83\xc4\x04\x81\xc1\xbc\x02\x00\x00\x89\x4e\x10\x57\xe8\x77\x92\xec\xff\x83\xc4\x04\x85\xc0\x74\x36\x8b\x17\x6a\x05\x8b\xcf\xff\x52\x2c\x85\xc0\x74\x29\x55\x8b\x2f\xe8\x9b\x20\xfc\xff\x50\x8b\xcf\xff\x55\x2c\xf7\xd8\x1b\xc0\x23\xc7\x83\x78\x08\x18\x5d\x75\x0e\x8b\x48\x14\x85\xc9\x75\x07\xc7\x46\x10\x00\x00\x00\x00\x89\x5e\x50\x8b\x07\x8b\xcf\xff\x50\x3c\x8b\x08\x89\x4e\x5c\x8b\x4c\x24\x14\x8b\x11\xff\x52\x3c\x8b\x00\x5f\x89\x46\x58\x5e\x5b\xc3"},
};
struct MagicMapImmediate {uint32_t at;int kind;};
const MagicMapImmediate g_magicMapImmediates[]={
    {0x576b1b,0}, // 00576b19 2 4 69c0c8000000 W
    {0x576b2a,1}, // 00576b27 3 4 8b148d680efb06 B20
    {0x576b31,1}, // 00576b2e 3 4 8d0c8d680efb06 B20
    {0x592082,1}, // 0059207f 3 4 8d0495680efb06 B20
    {0x592235,2}, // 00592231 4 4 0fb70485700efb06 B20+8
    {0x593099,1}, // 00593096 3 4 8d0c8d680efb06 B20
    {0x5930eb,1}, // 005930ea 1 4 b9680efb06 B20
    {0x593130,1}, // 0059312d 3 4 8d0c8d680efb06 B20
    {0x59315b,1}, // 00593158 3 4 8b048d680efb06 B20
    {0x593162,1}, // 0059315f 3 4 8d0c8d680efb06 B20
    {0x59206b,0}, // 00592069 2 4 69c0c8000000 W
    {0x592225,0}, // 00592223 2 4 69c0c8000000 W
    {0x593087,0}, // 00593085 2 4 69c0c8000000 W
    {0x593121,0}, // 0059311f 2 4 69c0c8000000 W
    {0x59314c,0}, // 0059314a 2 4 69c0c8000000 W
};
bool verifyMagicLayout(){
    for(const auto& layout:g_magicLayouts){
        auto p=(const uint8_t*)(g_strategyCodeBase+layout.start-0x400000);
        for(int n=0;n<layout.size;++n){
            bool immediate=false;for(const auto& m:g_magicMapImmediates)if(layout.start+n>=m.at && layout.start+n<m.at+4)immediate=true;
            if(!immediate && p[n]!=(uint8_t)layout.bytes[n]){if(g_log)g_log("skills: unexpected magic layout at %08x; rejected",layout.start+n);return false;}
        }
    }
    uint32_t width=*(const uint32_t*)(g_strategyCodeBase+0x5a078f-0x400000),base=*(const uint32_t*)(g_strategyCodeBase+0x5a07aa-0x400000);
    for(const auto& m:g_magicMapImmediates)if(*(const uint32_t*)(g_strategyCodeBase+m.at-0x400000)!=(m.kind==0?width:base+(m.kind==2?8:0)))return false;
    return true;
}
