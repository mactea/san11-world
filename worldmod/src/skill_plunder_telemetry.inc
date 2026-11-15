// Native defeat scans all three attackers and keeps the LAST matching21.
// A successful10-percent roll is not proof that an item was transferred.
typedef int(__thiscall* PlunderHas)(void*,int);
PlunderHas g_plunderHasOriginal=(PlunderHas)0x4890f0;
int __fastcall plunderHas(void* person,void*,int effect){
    QueryOrigin origin(0x4be9b5);
    auto s=g_defeat;
    if(!s || effect!=21 || !afterAttackMember(s->attacker,person))return g_plunderHasOriginal(person,effect);
    ProgressionProbe probe(21);probe.scene="battle";probe.event="plunder_eligibility";
    int result=g_plunderHasOriginal(person,effect);
    if(result && probe.actor==person){s->plunder=person;s->plunderId=growthPersonId(person);s->plunderOwned=*(const int*)((const uint8_t*)person+0xe8);}
    return result;
}
bool plunderProvider(const DefeatScope& s){
    return captureProvider(s,s.plunder,s.plunderId,s.plunderOwned,21,s.attacker) && s.attackingUnit>=0 &&
        g_movementUnitById((void*)0x7201958,s.attackingUnit)==s.attacker && unitTarget(s.attacker).force==s.attackingForce;
}
typedef int(__cdecl* PlunderRoll)(int);
PlunderRoll g_plunderRollOriginal=(PlunderRoll)0x4721d0;
int __cdecl plunderRoll(void* selected,int chance){
    int result=g_plunderRollOriginal(chance);auto s=g_defeat;
    if(!s || selected!=s->plunder || chance!=10 || !plunderProvider(*s))return result;
    s->plunderRolled=result==1;auto target=s->target;target.actorUnit=s->attackingUnit;
    target.equipment=*(const int*)((const uint8_t*)s->attacker+0xa8);char context[192];
    sprintf_s(context,"{\"native_chance_percent\":10,\"native_result\":%d,\"result_scope\":\"attempt_roll_not_item_transfer\",\"provider_selection\":\"last_matching_member\",\"counterfactual_evaluated\":false}",result);
    target.context=context;skilltelemetry::appliedEvent(s->plunder,s->plunderOwned,21,"battle","plunder_roll_resolved",0x4be9f6,target);return result;
}
__declspec(naked) void plunderRollBridge(){__asm {push [esp+4]
    push [esp+0x20]
    call plunderRoll
    add esp,8
    ret}}
BuildingId g_plunderItemId=(BuildingId)0x491360;
MovementUnitById g_plunderItemById=(MovementUnitById)0x490b30;
typedef int(__thiscall* PlunderTransfer)(void*,void*,int,int);
PlunderTransfer g_plunderTransferOriginal=(PlunderTransfer)0x4a0a40;
int __fastcall plunderTransfer(void* self,void*,void* item,int owner,int treasury){
    auto s=g_defeat;bool observe=s && s->plunderRolled && plunderProvider(*s) && item;
    int id=observe?g_plunderItemId((void*)0x7201958,item):-1;
    observe=observe && id>=0 && id<100 && g_plunderItemById((void*)0x7201958,id)==item;
    int before=observe?*(int*)((uint8_t*)item+0x40):-1,oldTreasury=observe?*(int*)((uint8_t*)item+0x44):-1;
    auto victim=observe?captureVictim(*s,g_telemetryPerson((void*)0x7201958,before)):nullptr;
    observe=observe && victim && afterAttackMember(s->defeated,victim->person) && owner>=0 && treasury==-1;
    int result=g_plunderTransferOriginal(self,item,owner,treasury);
    if(!observe || !plunderProvider(*s) || g_plunderItemId((void*)0x7201958,item)!=id ||
       g_plunderItemById((void*)0x7201958,id)!=item || growthPersonId(victim->person)!=victim->id)return result;
    int after=*(int*)((uint8_t*)item+0x40),newTreasury=*(int*)((uint8_t*)item+0x44);
    if(after!=owner || newTreasury!=-1 || (before==after && oldTreasury==newTreasury))return result;
    auto target=s->target;target.person=victim->id;target.actorUnit=s->attackingUnit;
    target.equipment=*(const int*)((const uint8_t*)s->attacker+0xa8);char context[384];
    sprintf_s(context,"{\"item_id\":%d,\"owner_before\":%d,\"owner_after\":%d,\"treasury_force_before\":%d,\"treasury_force_after\":%d,\"native_chance_percent\":10,\"recipient_scope\":\"attacking_ruler\",\"provider_selection\":\"last_matching_member\",\"counterfactual_evaluated\":false}",
        id,before,after,oldTreasury,newTreasury);target.context=context;
    skilltelemetry::appliedEvent(s->plunder,s->plunderOwned,21,"battle","plunder_item_transferred",0x4bebbb,target);return result;
}

struct PlunderLayout {uint32_t start;int size;const char* bytes;};
const PlunderLayout g_plunderLayouts[]={
    {0x4be969,157,"\x39\x5c\x24\x10\x89\x5c\x24\x14\x74\x58\x89\x5c\x24\x10\x8b\x4c\x24\x10\x51\x8b\xcf\xe8\x0d\x69\xfd\xff\x50\xb9\x58\x19\x20\x07\xe8\x72\x21\xfd\xff\x8b\xe8\x55\xe8\x9a\xbc\xfb\xff\x83\xc4\x04\x85\xc0\x74\x20\x8b\x4c\x24\x1c\x68\x2c\x01\x00\x00\x55\xe8\xa4\x83\xfe\xff\x6a\x15\x8b\xcd\xe8\x3b\xa7\xfc\xff\x85\xc0\x74\x04\x89\x6c\x24\x14\x8b\x44\x24\x10\x40\x83\xf8\x03\x89\x44\x24\x10\x7c\xac\x8b\x54\x24\x14\x8b\xac\x24\x9c\x04\x00\x00\x52\xe8\x54\xbc\xfb\xff\x83\xc4\x04\x85\xc0\x0f\x84\xf4\x01\x00\x00\x39\x9c\x24\x98\x04\x00\x00\x0f\x84\xe7\x01\x00\x00\x6a\x0a\xe8\xd5\x37\xfb\xff\x83\xc4\x04\x85\xc0\x0f\x84\xd5\x01\x00\x00"},
    {0x4bea42,159,"\x8b\x44\x24\x10\x50\x8b\xce\xe8\x42\x68\xfd\xff\x50\xb9\x58\x19\x20\x07\xe8\xa7\x20\xfd\xff\x8b\xe8\x55\xe8\xcf\xbb\xfb\xff\x83\xc4\x04\x85\xc0\x74\x67\x89\x5c\x24\x58\x89\x5c\x24\x5c\x89\x5c\x24\x54\x89\x5c\x24\x50\x89\x5c\x24\x60\xc7\x44\x24\x64\x0a\x00\x00\x00\x89\x5c\x24\x68\xc7\x44\x24\x4c\xf4\xaa\x75\x00\x8d\x4c\x24\x4c\xc6\x84\x24\x88\x04\x00\x00\x01\xe8\x1f\xcb\xf4\xff\x55\x8d\x4c\x24\x50\x51\xb9\x08\x98\x99\x07\xe8\x8f\xf4\x00\x00\x8d\x54\x24\x4c\x52\x8d\x4c\x24\x30\xe8\x11\xce\xff\xff\x8d\x4c\x24\x4c\x88\x9c\x24\x88\x04\x00\x00\xe8\xc1\xe4\xf4\xff\x8b\x44\x24\x10\x40\x83\xf8\x03\x89\x44\x24\x10\x0f\x8c\x61\xff\xff\xff"},
    {0x4beae1,64,"\x8b\x44\x24\x38\x3b\xc3\x0f\x84\xd3\x00\x00\x00\x50\xe8\x5d\x36\xfb\xff\x0f\xb7\xc0\x83\xc4\x04\x50\x8d\x4c\x24\x30\xe8\x4d\xce\xff\xff\x8b\xe8\x55\xe8\x25\xbb\xfb\xff\x83\xc4\x04\x85\xc0\x0f\x84\xaa\x00\x00\x00\x8b\x4c\x24\x14\x8b\x45\x40\x53\x51\x55\x50"},
    {0x4bebaa,22,"\x8b\x4c\x24\x14\x6a\xff\xe8\x8b\xb1\xfc\xff\x8b\x4c\x24\x20\x50\x55\xe8\x80\x1e\xfe\xff"},
    {0x4cdf40,103,"\x55\x8b\x6c\x24\x08\x56\x8b\xcd\xe8\x73\xd6\xf3\xff\x8b\x74\x24\x10\x56\xe8\xa9\xc6\xfa\xff\x83\xc4\x04\x85\xc0\x74\x41\x53\x57\x56\xb9\x58\x19\x20\x07\xe8\xa5\x33\xfc\xff\x8b\xd8\x33\xff\x90\x57\xb9\x58\x19\x20\x07\xe8\xb5\x2b\xfc\xff\x8b\xf0\x56\xe8\xad\xc6\xfa\xff\x83\xc4\x04\x85\xc0\x74\x0d\x3b\x5e\x40\x75\x08\x56\x8b\xcd\xe8\x19\x88\xfd\xff\x47\x83\xff\x64\x7c\xd3\x5f\x5b\x8b\x45\x0c\x5e\x5d\xc2\x08\x00"},
    {0x4bb950,56,"\x51\x56\x8b\xf1\x8b\x46\x04\x85\xc0\x57\x89\x44\x24\x08\x74\x1f\x8b\x7c\x24\x10\x8d\x44\x24\x08\x50\x8b\xce\xe8\xd0\xfa\xf4\xff\x85\xff\x8b\x00\x7e\x0c\x8b\x44\x24\x08\x4f\x85\xc0\x75\xe5\x8b\x46\x1c\x5f\x5e\x59\xc2\x04\x00"},
    {0x4a0a40,39,"\x56\x8b\x74\x24\x08\x56\xe8\xe5\x9b\xfd\xff\x83\xc4\x04\x85\xc0\x74\x11\x8b\x44\x24\x10\x8b\x4c\x24\x0c\x50\x51\x8b\xce\xe8\x7d\x43\xfe\xff\x5e\xc2\x0c\x00"},
    {0x484de0,49,"\x83\xc8\xff\x89\x41\x40\x89\x41\x44\x8b\x44\x24\x04\x85\xc0\x7c\x0d\x3d\x4b\x04\x00\x00\x7f\x06\x89\x41\x40\xc2\x08\x00\x8b\x44\x24\x08\x85\xc0\x7c\x08\x83\xf8\x29\x7f\x03\x89\x41\x44\xc2\x08\x00"},
    {0x489d40,42,"\x8b\x01\x56\xff\x50\x40\x50\xb9\x58\x19\x20\x07\xe8\x4f\x6d\x00\x00\x8b\xf0\x56\xe8\xd7\x08\xff\xff\x83\xc4\x04\x85\xc0\x74\x05\x8b\x46\x04\x5e\xc3\x83\xc8\xff\x5e\xc3"},
};
bool plunderTreasuryBranch(){
    const auto at=(const uint8_t*)(g_strategyCodeBase+0x484e06-0x400000);
    if(!memcmp(at,"\x83\xf8\x29\x7f\x03",5))return true;
    // The shipped expansion replaces this imm8 comparison with an imm32
    // cave. Check its whole payload, readable allocation and both returns.
    if(at[0]!=0xe9)return false;
    auto cave=(const uint8_t*)((uintptr_t)at+5+*(const int32_t*)(at+1));MEMORY_BASIC_INFORMATION memory={};
    if(!VirtualQuery(cave,&memory,sizeof memory) || memory.State!=MEM_COMMIT ||
       (memory.Protect&(PAGE_NOACCESS|PAGE_GUARD)) || memory.RegionSize<16 ||
       (uintptr_t)cave-(uintptr_t)memory.BaseAddress>memory.RegionSize-16)return false;
    int bound=*(const int*)(cave+1);
    return cave[0]==0x3d && bound>=41 && bound<65535 && !memcmp(cave+5,"\x0f\x8f",2) && cave[11]==0xe9 &&
        (uintptr_t)cave+11+*(const int32_t*)(cave+7)==(uintptr_t)at+8 &&
        (uintptr_t)cave+16+*(const int32_t*)(cave+12)==(uintptr_t)at+5;
}
bool verifyPlunderLayout(){
    for(const auto& s:g_plunderLayouts){
        const auto p=(const uint8_t*)(g_strategyCodeBase+s.start-0x400000);bool match=true;
        for(int n=0;n<s.size;++n){
            if(s.start==0x484de0 && n>=0x12 && n<0x16)continue; // M10 person bound
            if(s.start==0x484de0 && n>=0x26 && n<0x2b)continue; // expanded treasury cave
            if(p[n]!=(uint8_t)s.bytes[n])match=false;
        }
        if(s.start==0x484de0){int bound=*(const int*)(p+0x12);
            match=match && bound>=1099 && bound<65535 && bound==*(const int*)(g_strategyCodeBase+0x490b09-0x400000) && plunderTreasuryBranch();}
        if(!match){if(g_log)g_log("skills: unexpected plunder layout at %08x; rejected",s.start);return false;}
    }
    return true;
}
