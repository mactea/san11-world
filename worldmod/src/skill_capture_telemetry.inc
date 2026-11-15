struct CaptureVictim {
    void* person=nullptr;
    int id=-1,status=-1,place=-1;
    const void* provider=nullptr;
    int providerId=-1,owned=-1;
    bool guaranteed=false,rolled=false,written=false;
};
struct DefeatScope;
thread_local DefeatScope* g_defeat=nullptr;
struct DefeatScope {
    void* defeated;
    void* attacker;
    CaptureVictim victims[3];
    skilltelemetry::Target target;
    int attackingUnit=-1,attackingBase=-1,attackingForce=-1;
    const void* bloodroad=nullptr;
    int bloodroadId=-1,bloodroadOwned=-1;
    const void* plunder=nullptr;
    int plunderId=-1,plunderOwned=-1;
    bool plunderRolled=false;
    RecoveryActor precision;
    int precisionId=-1,precisionSource=-1,precisionForce=-1,precisionProviderForce=-1;
    bool precisionBase=false;
    bool precisionConsumed=false;
    bool protectedBranch=false,cleanupActive=false,unitReset=false;
    uint64_t generation;
    DefeatScope* previous;
    DefeatScope(void* d,void* a):defeated(d),attacker(a),generation(skilltelemetry::contextGeneration()),previous(g_defeat){
        g_defeat=this;
        int id=d?g_telemetryUnitId((void*)0x7201958,d):-1;
        if(id<0 || g_movementUnitById((void*)0x7201958,id)!=d)return;
        target=unitTarget(d);target.equipment=*(const int*)((const uint8_t*)d+0xa8);
        typedef const uint32_t*(__thiscall* Position)(void*);
        auto position=*(*(Position*)(*(uint8_t**)d+0x3c))(d);target.x=(int16_t)position;target.y=(int16_t)(position>>16);
        attackingUnit=a?g_telemetryUnitId((void*)0x7201958,a):-1;
        if(attackingUnit>=0 && g_movementUnitById((void*)0x7201958,attackingUnit)==a)attackingForce=unitTarget(a).force;
        else attackingUnit=-1;
        for(int n=0;n<3;++n){
            auto& v=victims[n];int person=*(const int*)((const uint8_t*)d+0xc+4*n);
            auto p=person>=0?g_telemetryPerson((void*)0x7201958,person):nullptr;
            bool duplicate=false;for(int j=0;j<n;++j)if(victims[j].person==p)duplicate=true;if(duplicate)continue;
            if(p && growthPersonId(p)==person){v.person=p;v.id=person;v.status=*(int*)(p+0xa0);v.place=*(int*)(p+0x9c);}
        }
    }
    ~DefeatScope(){g_defeat=previous;}
};
bool captureProvider(const DefeatScope& s,const void* actor,int id,int owned,int effect,void* unit,bool member=true){
    return skilltelemetry::enabled() && s.generation==skilltelemetry::contextGeneration() && actor && id>=0 &&
        growthPersonId((void*)actor)==id && *(const int*)((const uint8_t*)actor+0xe8)==owned && contains(owned,effect) &&
        (!member || afterAttackMember(unit,actor));
}
CaptureVictim* captureVictim(DefeatScope& s,void* person){for(auto& v:s.victims)if(v.person==person && v.id>=0 && growthPersonId(person)==v.id)return &v;return nullptr;}
bool captureAttackerLive(const DefeatScope& s){
    if(s.attackingUnit>=0)return g_movementUnitById((void*)0x7201958,s.attackingUnit)==s.attacker;
    return s.attackingBase>=0 && g_baseById((void*)0x7201958,s.attackingBase)==s.attacker &&
        (*(BuildingValue*)(*(uint8_t**)s.attacker+0x40))(s.attacker)==s.attackingForce;
}
bool captureActorLive(const DefeatScope& s,const void* actor,int id,int owned){
    bool base=s.attackingBase>=0;
    if(!captureProvider(s,actor,id,owned,19,s.attacker,!base) || !captureAttackerLive(s))return false;
    if(!base)return true;
    int status=*(const int*)((const uint8_t*)actor+0xa0);
    return status>=0 && status<4 && *(const int*)((const uint8_t*)actor+0x9c)==s.attackingBase &&
        growthTarget((uint8_t*)actor,id).force==s.attackingForce;
}
skilltelemetry::Target captureTarget(const DefeatScope& s,int person){
    auto target=s.target;target.person=person;target.actorUnit=s.attackingUnit;
    target.equipment=s.attackingUnit>=0?*(const int*)((const uint8_t*)s.attacker+0xa8):-1;
    return target;
}
typedef int(__thiscall* Defeat)(void*,void*,void*,int,int,int);
uint32_t g_defeatResume=0x4be797;
__declspec(naked) void defeatNative(){__asm {push -1
    push 0x72fd42
    jmp dword ptr [g_defeatResume]}}
Defeat g_defeatOriginal=(Defeat)defeatNative;
int __fastcall defeat(void* self,void*,void* unit,void* attacker,int a,int b,int c){
    if(!skilltelemetry::enabled())return g_defeatOriginal(self,unit,attacker,a,b,c);
    DefeatScope scope(unit,attacker);int result=g_defeatOriginal(self,unit,attacker,a,b,c);
    if(scope.generation==skilltelemetry::contextGeneration() && captureAttackerLive(scope))for(const auto& v:scope.victims){
        if(!v.written || growthPersonId(v.person)!=v.id || *(const int*)((const uint8_t*)v.person+0xa0)!=5 ||
           !captureActorLive(scope,v.provider,v.providerId,v.owned))continue;
        auto target=captureTarget(scope,v.id);char context[256];
        target.context="{\"native_capture_chance_percent\":100,\"person_status_after\":5,\"resolution_scope\":\"after_defeat_return\",\"counterfactual_evaluated\":false}";
        if(scope.attackingBase>=0){sprintf_s(context,"{\"source_base\":%d,\"native_capture_chance_percent\":100,\"person_status_after\":5,\"resolution_scope\":\"after_defeat_return\",\"counterfactual_evaluated\":false}",scope.attackingBase);target.context=context;}
        skilltelemetry::appliedEvent(v.provider,v.owned,19,"battle","capture_prisoner_after_defeat_resolved",0x4be790,target);
    }
    return result;
}
struct CaptureAttemptScope;
thread_local CaptureAttemptScope* g_captureAttempt=nullptr;
struct CaptureAttemptScope {
    DefeatScope* parent;
    const void* actor=nullptr;
    int id=-1,owned=-1;
    RecoveryActor ironwall;
    int ironwallId=-1,ironwallCount=0;
    RecoveryActor lucky;
    int luckyId=-1;
    CaptureAttemptScope* previous;
    explicit CaptureAttemptScope(DefeatScope* p):parent(p),previous(g_captureAttempt){g_captureAttempt=this;}
    ~CaptureAttemptScope(){g_captureAttempt=previous;}
};
typedef int(__stdcall* CaptureAttempt)(void*,void*,void*,void*,int,int,int,int);
CaptureAttempt g_captureAttemptOriginal=(CaptureAttempt)0x4b1280;
int __stdcall captureAttempt(void* captured,void* escaped,void* defeated,void* attacker,int a,int b,int c,int d){
    auto s=g_defeat;
    if(!s || s->defeated!=defeated || s->attacker!=attacker || s->generation!=skilltelemetry::contextGeneration())
        return g_captureAttemptOriginal(captured,escaped,defeated,attacker,a,b,c,d);
    CaptureAttemptScope scope(s);return g_captureAttemptOriginal(captured,escaped,defeated,attacker,a,b,c,d);
}
int __fastcall captureHas(void* unit,void*,int effect){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);auto a=g_captureAttempt;
    if(!a || effect!=19 || a->parent!=g_defeat || unit!=a->parent->attacker)return native(unit,effect);
    ProgressionProbe probe(19);probe.scene="battle";probe.event="capture_eligibility";int result=native(unit,effect);
    if(result && probe.actor){a->actor=probe.actor;a->id=growthPersonId((void*)probe.actor);a->owned=*(const int*)((const uint8_t*)probe.actor+0xe8);}
    return result;
}
int __fastcall captureBaseHas(uint8_t* person,void*,int effect){
    auto a=g_captureAttempt;auto s=a?a->parent:nullptr;
    if(!s || s!=g_defeat || effect!=19 || s->attackingUnit>=0 || !s->attacker || s->generation!=skilltelemetry::contextGeneration())return personHas(person,nullptr,effect);
    int base=g_telemetryBaseId((void*)0x7201958,s->attacker);
    if(base<0 || g_baseById((void*)0x7201958,base)!=s->attacker)return personHas(person,nullptr,effect);
    s->attackingBase=base;s->attackingForce=(*(BuildingValue*)(*(uint8_t**)s->attacker+0x40))(s->attacker);
    QueryOrigin origin(0x4b1577);ProgressionProbe probe(19);probe.scene="battle";probe.event="capture_eligibility";
    int result=personHas(person,nullptr,effect);
    if(result && probe.actor){a->actor=probe.actor;a->id=growthPersonId((void*)probe.actor);a->owned=*(const int*)((const uint8_t*)probe.actor+0xe8);}
    return result;
}
int __fastcall luckyCaptureHas(uint8_t* person,void*,int effect){
    auto a=g_captureAttempt;
    if(!a || a->parent!=g_defeat || effect!=32)return personHas(person,nullptr,effect);
    a->lucky={};auto snapshot=recoveryActor(person);
    QueryOrigin origin(0x4b17b9);ProgressionProbe probe(32);probe.scene="battle";probe.event="capture_luck_eligibility";
    int result=personHas(person,nullptr,effect);auto s=a->parent;auto v=captureVictim(*s,person);
    if(result && v && recoveryActorValid(snapshot,32) && captureProvider(*s,person,v->id,snapshot.owned,32,s->defeated)){
        a->lucky=snapshot;a->luckyId=v->id;
    }
    return result;
}
void __cdecl luckyCaptureTaken(void* person,int result){
    auto a=g_captureAttempt;if(!a || a->parent!=g_defeat)return;
    auto actor=a->lucky;int id=a->luckyId;a->lucky={};
    if(!result || actor.person!=person || !recoveryActorValid(actor,32))return;
    auto s=a->parent;
    if(s->target.unit<0 || g_movementUnitById((void*)0x7201958,s->target.unit)!=s->defeated ||
       !g_growthValid(s->defeated) || *(const int*)((const uint8_t*)s->defeated+8)<0 ||
       unitTarget(s->defeated).force!=s->target.force || !captureAttackerLive(*s) ||
       (s->attackingUnit>=0 && unitTarget(s->attacker).force!=s->attackingForce) ||
       !captureProvider(*s,person,id,actor.owned,32,s->defeated))return;
    auto target=s->target;target.person=id;target.actorUnit=s->target.unit;char context[320];
    sprintf_s(context,"{\"attacking_unit\":%d,\"attacking_base\":%d,\"attacking_force\":%d,\"capture_random_sample_skipped\":true,\"result_scope\":\"native_capture_candidate_roll_skipped\",\"counterfactual_capture_evaluated\":false}",
        s->attackingUnit,s->attackingBase,s->attackingForce);target.context=context;
    skilltelemetry::appliedEvent(person,actor.owned,32,"battle","lucky_capture_attempt_skipped",0x4b17bb,target);
}
uint32_t g_luckyCaptureSkip=0x4b18cb,g_luckyCaptureContinue=0x4b17c1;
__declspec(naked) void luckyCaptureTakenBridge(){__asm {
    pushfd
    pushad
    push eax
    push ebp
    call luckyCaptureTaken
    add esp,8
    popad
    popfd
    jnz skipped
    jmp dword ptr [g_luckyCaptureContinue]
 skipped:
    jmp dword ptr [g_luckyCaptureSkip]}}
int __fastcall ironwallCaptureHas(void* person,void*,int effect,int count){
    auto a=g_captureAttempt;
    if(!a || a->parent!=g_defeat || effect!=29)return personHas((uint8_t*)person,nullptr,effect);
    a->ironwall={};
    QueryOrigin origin(0x4b17f6);ProgressionProbe probe(29);probe.scene="battle";probe.event="ironwall_capture_eligibility";
    auto snapshot=recoveryActor(person);int result=personHas((uint8_t*)person,nullptr,effect);
    auto s=a->parent;auto v=captureVictim(*s,person);
    if(result && count>1 && v && captureProvider(*s,person,v->id,snapshot.owned,29,s->defeated) && recoveryActorValid(snapshot,29)){
        a->ironwall=snapshot;a->ironwallId=v->id;a->ironwallCount=count;
    }
    return result;
}
__declspec(naked) void ironwallCaptureHasBridge(){__asm {
    push ebx
    push [esp+8]
    call ironwallCaptureHas
    ret 4}}
void ironwallCaptureRolled(CaptureAttemptScope* attempt,const RecoveryActor& actor,int id,int count,void* person,int chance,int result){
    if(!actor.person || actor.person!=person || count<=1 || chance<0 || chance>100 || attempt!=g_captureAttempt ||
       !attempt || attempt->parent!=g_defeat || !recoveryActorValid(actor,29))return;
    auto s=attempt->parent;
    if(s->target.unit<0 || g_movementUnitById((void*)0x7201958,s->target.unit)!=s->defeated ||
       !g_growthValid(s->defeated) || *(const int*)((const uint8_t*)s->defeated+8)<0 ||
       unitTarget(s->defeated).force!=s->target.force || !captureAttackerLive(*s) ||
       (s->attackingUnit>=0 && unitTarget(s->attacker).force!=s->attackingForce) ||
       !captureProvider(*s,person,id,actor.owned,29,s->defeated))return;
    auto target=s->target;target.person=id;target.actorUnit=s->target.unit;char context[384];
    sprintf_s(context,"{\"native_surround_count\":%d,\"used_capture_multiplier\":100,\"native_capture_chance_percent\":%d,\"native_result\":%d,\"attacking_unit\":%d,\"attacking_base\":%d,\"attacking_force\":%d,\"result_scope\":\"capture_multiplier_not_final_escape\",\"counterfactual_capture_chance_evaluated\":false}",
        count,chance,result,s->attackingUnit,s->attackingBase,s->attackingForce);target.context=context;
    skilltelemetry::appliedEvent(person,actor.owned,29,"battle","ironwall_capture_multiplier_reduced",0x4b18ac,target);
}
uint32_t g_captureHasResume=0x4b15c3,g_captureGuaranteeResume=0x4b1853;
__declspec(naked) void captureHasBridge(){__asm {call captureHas
    mov [esp+0x20],eax
    jmp dword ptr [g_captureHasResume]}}
void __cdecl captureGuaranteed(void* person,int contribution){
    auto a=g_captureAttempt;
    if(!a || a->parent!=g_defeat || contribution!=100)return;
    auto s=a->parent;auto v=captureVictim(*s,person);
    if(!v || !afterAttackMember(s->defeated,person) || !captureActorLive(*s,a->actor,a->id,a->owned))return;
    v->provider=a->actor;v->providerId=a->id;v->owned=a->owned;v->guaranteed=true;v->rolled=false;
}
__declspec(naked) void captureGuaranteeBridge(){__asm {pushfd
    pushad
    push eax
    push ebp
    call captureGuaranteed
    add esp,8
    popad
    popfd
    mov [esp+0x18],eax
    mov eax,0x55555556
    jmp dword ptr [g_captureGuaranteeResume]}}
typedef int(__cdecl* CaptureRoll)(int);
CaptureRoll g_captureRollOriginal=(CaptureRoll)0x4721d0;
int __cdecl captureRoll(void* person,int chance){
    auto attempt=g_captureAttempt;RecoveryActor ironwall=attempt?attempt->ironwall:RecoveryActor{};
    int ironwallId=attempt?attempt->ironwallId:-1,count=attempt?attempt->ironwallCount:0;
    if(attempt)attempt->ironwall={};
    int result=g_captureRollOriginal(chance);
    ironwallCaptureRolled(attempt,ironwall,ironwallId,count,person,chance,result);auto a=g_captureAttempt;
    if(!a || a->parent!=g_defeat || chance!=100)return result;auto s=a->parent;auto v=captureVictim(*s,person);
    if(!v || !v->guaranteed || !captureActorLive(*s,v->provider,v->providerId,v->owned))return result;
    v->rolled=result==1;auto target=captureTarget(*s,v->id);char context[256];
    if(s->attackingBase>=0)sprintf_s(context,"{\"source_base\":%d,\"native_capture_chance_percent\":100,\"native_result\":%d,\"result_scope\":\"candidate_roll_not_final_disposition\",\"counterfactual_evaluated\":false}",s->attackingBase,result);
    else sprintf_s(context,"{\"native_capture_chance_percent\":100,\"native_result\":%d,\"result_scope\":\"candidate_roll_not_final_disposition\",\"counterfactual_evaluated\":false}",result);target.context=context;
    skilltelemetry::appliedEvent(v->provider,v->owned,19,"battle","capture_guarantee_roll_resolved",0x4b18ac,target);return result;
}
__declspec(naked) void captureRollBridge(){__asm {push [esp+4]
    push ebp
    call captureRoll
    add esp,8
    ret}}
typedef void(__thiscall* CaptureStatus)(void*,int);
CaptureStatus g_captureStatusOriginal=(CaptureStatus)0x4898f0;
void __fastcall captureStatus(void* person,void*,int status){
    auto s=g_defeat;auto v=s?captureVictim(*s,person):nullptr;
    bool observe=v && v->guaranteed && v->rolled && status==5 && captureActorLive(*s,v->provider,v->providerId,v->owned);
    int before=observe?*(const int*)((const uint8_t*)person+0xa0):-1;g_captureStatusOriginal(person,status);
    if(!observe || before==5 || *(const int*)((const uint8_t*)person+0xa0)!=5 ||
       !captureActorLive(*s,v->provider,v->providerId,v->owned) || growthPersonId(person)!=v->id)return;
    auto target=captureTarget(*s,v->id);char context[256];
    target.context="{\"native_capture_chance_percent\":100,\"final_disposition\":\"prisoner_status_written\",\"counterfactual_evaluated\":false}";
    if(s->attackingBase>=0){sprintf_s(context,"{\"source_base\":%d,\"native_capture_chance_percent\":100,\"final_disposition\":\"prisoner_status_written\",\"counterfactual_evaluated\":false}",s->attackingBase);target.context=context;}
    v->written=true;
    skilltelemetry::applied(v->provider,v->owned,19,"battle","capture_prisoner_status_written","person_status",before,5,0x4a9724,target);
}
int __fastcall bloodroadHas(void* unit,void*,int effect){
    typedef int(__thiscall* Has)(void*,int);auto native=*(Has*)(*(uint8_t**)unit+0x58);auto s=g_defeat;
    if(!s || effect!=33 || s->defeated!=unit)return native(unit,effect);
    ProgressionProbe probe(33);probe.scene="battle";probe.event="bloodroad_eligibility";int result=native(unit,effect);
    if(result && probe.actor){s->bloodroad=probe.actor;s->bloodroadId=growthPersonId((void*)probe.actor);s->bloodroadOwned=*(const int*)((const uint8_t*)probe.actor+0xe8);}
    return result;
}
uint32_t g_bloodroadHasResume=0x4beeb4,g_bloodroadSkip=0x4bf02d,g_bloodroadContinue=0x4beeba;
__declspec(naked) void bloodroadHasBridge(){__asm {call bloodroadHas
    test eax,eax
    jmp dword ptr [g_bloodroadHasResume]}}
void __cdecl bloodroadTaken(int result){
    auto s=g_defeat;if(!s || !result || s->target.unit<0 ||
        g_movementUnitById((void*)0x7201958,s->target.unit)!=s->defeated ||
        !captureProvider(*s,s->bloodroad,s->bloodroadId,s->bloodroadOwned,33,s->defeated))return;
    s->protectedBranch=true;auto target=s->target;char context[192];
    sprintf_s(context,"{\"attacking_unit\":%d,\"attacking_force\":%d,\"effect_scope\":\"native_capture_attempt_skipped\",\"counterfactual_evaluated\":false}",s->attackingUnit,s->attackingForce);target.context=context;
    skilltelemetry::appliedEvent(s->bloodroad,s->bloodroadOwned,33,"battle","bloodroad_capture_attempt_blocked",0x4beeb4,target);
}
__declspec(naked) void bloodroadTakenBridge(){__asm {pushfd
    pushad
    push eax
    call bloodroadTaken
    add esp,4
    popad
    popfd
    jnz skipped
    jmp dword ptr [g_bloodroadContinue]
 skipped:
    jmp dword ptr [g_bloodroadSkip]}}
typedef int(__thiscall* DefeatCleanup)(void*,void*,int);
typedef int(__thiscall* DefeatUnitReset)(void*,int);
uint32_t g_defeatUnitResetResume=0x4960a7;
__declspec(naked) void defeatUnitResetNative(){__asm {push ebx
    push esi
    xor ebx,ebx
    push ebx
    mov esi,ecx
    jmp dword ptr [g_defeatUnitResetResume]}}
DefeatUnitReset g_defeatUnitResetOriginal=(DefeatUnitReset)defeatUnitResetNative;
bool defeatUnitCleared(void* unit){
    auto p=(const uint8_t*)unit;return *(const int*)(p+8)==-1 && *(const int*)(p+0xc)==-1 &&
        *(const int*)(p+0x10)==-1 && *(const int*)(p+0x14)==-1;
}
struct BloodroadResetTicket {uintptr_t controller=0;uint64_t token=0,generation=0;};
BloodroadResetTicket bloodroadResetBegin(void*);
void bloodroadResetEnd(void*,const BloodroadResetTicket&);
int __fastcall defeatUnitReset(void* unit,void*,int flag){
    auto ticket=bloodroadResetBegin(unit);int result=g_defeatUnitResetOriginal(unit,flag);auto s=g_defeat;
    if(s && s->cleanupActive && unit==s->defeated && s->generation==skilltelemetry::contextGeneration() && defeatUnitCleared(unit))s->unitReset=true;
    bloodroadResetEnd(unit,ticket);
    return result;
}
DefeatCleanup g_defeatCleanupOriginal=(DefeatCleanup)0x4bccd0;
int __fastcall defeatCleanup(void* self,void*,void* unit,int flag){
    auto s=g_defeat;
    bool observe=s && unit==s->defeated && s->protectedBranch && s->target.unit>=0 &&
        g_movementUnitById((void*)0x7201958,s->target.unit)==unit && captureProvider(*s,s->bloodroad,s->bloodroadId,s->bloodroadOwned,33,unit);
    if(observe)s->cleanupActive=true;int result=g_defeatCleanupOriginal(self,unit,flag);if(observe)s->cleanupActive=false;
    // Unit getters return fixed slots even after native destruction. Require
    // the real reset plus its cleared members, not a fabricated null getter.
    if(observe && s->unitReset && g_movementUnitById((void*)0x7201958,s->target.unit)==unit && defeatUnitCleared(unit) &&
       captureProvider(*s,s->bloodroad,s->bloodroadId,s->bloodroadOwned,33,unit,false)){
        for(const auto& v:s->victims){
            if(!v.person || growthPersonId(v.person)!=v.id || v.status<0 || v.status>4)continue;
            int status=*(const int*)((const uint8_t*)v.person+0xa0),place=*(const int*)((const uint8_t*)v.person+0x9c);
            if(status<0 || status>4)continue;auto target=s->target;target.person=v.id;char context[256];
            sprintf_s(context,"{\"attacking_unit\":%d,\"attacking_force\":%d,\"place_before\":%d,\"place_after\":%d,\"person_status_after\":%d,\"protected_unit_removed\":true,\"counterfactual_evaluated\":false}",
                s->attackingUnit,s->attackingForce,v.place,place,status);target.context=context;
            skilltelemetry::appliedEvent(s->bloodroad,s->bloodroadOwned,33,"battle","bloodroad_member_escape_resolved",0x4bf186,target);
        }
    }
    return result;
}

struct CaptureLayout {uint32_t start;int size;const char* bytes;};
const CaptureLayout g_captureLayouts[]={
    {0x4b17d7,93,"\x8b\x9c\x24\xa4\x00\x00\x00\x3b\xde\x0f\xb6\xf8\xc7\x44\x24\x1c\x01\x00\x00\x00\x7e\x28\x6a\x1d\x8b\xcd\xe8\xfa\x78\xfd\xff\x85\xc0\xb8\x01\x00\x00\x00\x75\x02\x8b\xc3\x6b\xc0\x64\x83\xf8\x01\x89\x44\x24\x1c\x7d\x08\xc7\x44\x24\x1c\x01\x00\x00\x00\x8b\x84\x24\xa8\x00\x00\x00\x83\xf8\x03\x74\x0d\x83\xf8\x04\x74\x08\xd9\x05\x18\xe7\x74\x00\xeb\x06\xd9\x05\x34\xb3\x77\x00"},
    {0x4b151a,133,"\x6a\x0f\x50\x8d\x4c\x24\x68\x51\xb9\x08\x98\x99\x07\xe8\xc4\xdd\x01\x00\x8b\x54\x24\x18\x56\x52\x6a\x4b\x8d\x4c\x24\x6c\xe8\xc3\xde\xff\xff\x39\x74\x24\x6c\x7f\x08\xc7\x44\x24\x10\x01\x00\x00\x00\x39\x74\x24\x24\x74\x3a\x8b\x44\x24\x64\x3b\xc6\x89\x44\x24\x24\x74\x2e\x8d\x49\x00\x8d\x44\x24\x24\x50\x8d\x4c\x24\x64\xe8\x62\xa9\xfc\xff\x8b\x08\x6a\x13\xe8\x79\x7b\xfd\xff\x85\xc0\x75\x08\x39\x74\x24\x24\x75\xdf\xeb\x08\xc7\x44\x24\x20\x01\x00\x00\x00\x8d\x4c\x24\x60\xc7\x84\x24\x88\x00\x00\x00\xff\xff\xff\xff\xe8\x61\xab\xfc\xff"},
    {0x4960ac,15,"\x83\xc9\xff\x89\x4e\x08\x89\x4e\x0c\x89\x4e\x10\x89\x4e\x14"},
    {0x4a0cd9,13,"\x8b\xce\x89\x87\x9c\x00\x00\x00\xe8\x9a\x87\x01\x00"},
    {0x4bce73,12,"\x8b\x54\x24\x18\x52\x8b\xcb\xe8\xe1\x36\xff\xff"},
    {0x4be790,56,"\x6a\xff\x68\x42\xfd\x72\x00\x64\xa1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x81\xec\x70\x04\x00\x00\x56\x8b\xb4\x24\x84\x04\x00\x00\x56\x89\x4c\x24\x14\xe8\x73\xbe\xfb\xff\x83\xc4\x04\x85\xc0\x0f\x84\x03\x0a\x00\x00"},
    {0x4beea9,17,"\x8b\x06\x6a\x21\x8b\xce\xff\x50\x58\x85\xc0\x0f\x85\x73\x01\x00\x00"},
    {0x4bef68,49,"\x8b\x94\x24\xa0\x04\x00\x00\x8b\x44\x24\x28\x52\x6a\xff\x6a\xff\x50\x55\x56\x8d\x8c\x24\xa8\x00\x00\x00\x51\x8b\x4c\x24\x38\x8d\x54\x24\x68\x52\xc6\x84\x24\xa8\x04\x00\x00\x04\xe8\xe7\x22\xff\xff"},
    {0x4b15a1,36,"\x55\xe8\x89\x90\xfc\xff\x83\xc4\x04\x85\xc0\x74\x15\x8b\x44\x24\x24\x85\xc0\x74\x0d\x8b\x16\x6a\x13\x8b\xce\xff\x52\x58\x89\x44\x24\x20\x33\xf6"},
    {0x4b1780,65,"\x39\x74\x24\x10\x0f\x85\x41\x01\x00\x00\x39\x74\x24\x38\x75\x0c\x8b\x8c\x24\x90\x00\x00\x00\xe9\x36\x01\x00\x00\x56\x55\xb9\xcc\xb3\x80\x07\xe8\xe8\xed\xfe\xff\x85\xc0\x0f\x85\x1b\x01\x00\x00\x6a\x20\x8b\xcd\xe8\x37\x79\xfd\xff\x85\xc0\x0f\x85\x0a\x01\x00\x00"},
    {0x4b1834,174,"\x8b\x44\x24\x20\x8b\x5c\x24\x14\xf7\xd8\x1b\xc0\x83\xe0\x64\x33\xc9\x3b\xde\x0f\x95\xc1\x89\x44\x24\x18\xb8\x56\x55\x55\x55\x41\x89\x4c\x24\x24\xb9\x78\x00\x00\x00\x2b\xcf\xf7\xe9\x8b\xc2\xc1\xe8\x1f\x03\xc2\x89\x44\x24\x2c\xdb\x44\x24\x2c\xda\x4c\x24\x1c\xd8\xc9\xd8\x0d\x88\xaa\x77\x00\xda\x74\x24\x24\xda\x44\x24\x18\xe8\xeb\x61\x25\x00\xdd\xd8\x83\xf8\x64\x7c\x05\xb8\x64\x00\x00\x00\x39\xb4\x24\xac\x00\x00\x00\x74\x0d\x83\xc0\x1e\x83\xf8\x64\x7c\x05\xb8\x64\x00\x00\x00\x50\xe8\x1f\x09\xfc\xff\x83\xc4\x04\x85\xc0\x55\x74\x09\x8b\x8c\x24\x94\x00\x00\x00\xeb\x11\x8b\x8c\x24\x98\x00\x00\x00\xeb\x08\x8b\x8c\x24\x94\x00\x00\x00\x55\xe8\xd8\xa8\xfc\xff\x39\x74\x24\x28\x0f\x85\x8e\xfe\xff\xff"},
    {0x4a9713,34,"\x56\xe8\xe7\x0e\xfd\xff\x83\xc4\x04\x85\xc0\x74\x09\x6a\x05\x8b\xce\xe8\xc7\x01\xfe\xff\x8b\x44\x24\x20\x50\x8b\xce\xe8\x7b\x04\xfe\xff"},
    {0x4898f0,27,"\x8b\x44\x24\x04\x83\xf8\xff\x74\x09\x85\xc0\x7c\x0b\x83\xf8\x08\x7f\x06\x89\x81\xa0\x00\x00\x00\xc2\x04\x00"},
    {0x4bf171,26,"\x8b\x7c\x24\x1c\x56\xe8\xb5\xb4\xfb\xff\x83\xc4\x04\x85\xc0\x74\x09\x53\x56\x8b\xcf\xe8\x45\xdb\xff\xff"},
};
bool verifyCaptureLayout(){
    for(const auto& s:g_captureLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size)){
        if(g_log)g_log("skills: unexpected capture branch at %08x; rejected",s.start);return false;
    }
    return true;
}
