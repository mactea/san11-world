// Two AI owners compare raw91..94 with a native three-base mapping. This is
// a selection predicate, separate from the tribal-camp replacement effect.
typedef int(__thiscall* TribalAffinityMatch)(void*,int,int);
TribalAffinityMatch g_tribalAffinityOriginal=(TribalAffinityMatch)0x5f6610;
int __fastcall tribalAffinityMatch(void* self,void*,int baseId,int raw,void* person,uintptr_t caller){
    uintptr_t origin=trapOrigin(caller);
    if(!skilltelemetry::enabled() || raw<91 || raw>94 ||
       (origin!=0x5e9cbe && origin!=0x5ed9d2))return g_tribalAffinityOriginal(self,baseId,raw);
    PersonReceipt actor(person);
    TrapObject base(baseId>=0?g_baseById((void*)0x7201958,baseId):nullptr);
    int result=g_tribalAffinityOriginal(self,baseId,raw);
    if(result && actor.owned==raw && actor.valid(raw) && base.base==baseId && base.live() &&
       g_telemetryBaseId((void*)0x7201958,base.pointer)==baseId){
        auto target=incomeTarget(base.pointer);target.x=(int16_t)base.position;target.y=(int16_t)(base.position>>16);
        char context[192];sprintf_s(context,"{\"native_owner\":%u,\"raw_id_only\":true,\"selection_stage\":\"%s\"}",
            (unsigned)(origin-5),origin==0x5e9cbe?"candidate_list_filter":"member_selection_gate");target.context=context;
        skilltelemetry::matched(actor.person,raw,raw,origin,"ai_evaluation","tribal_base_mapping_checked",target);
    }
    return result;
}
__declspec(naked) void tribalAffinityBridge(){__asm {push [esp]
    push esi
    push [esp+16]
    push [esp+16]
    call tribalAffinityMatch
    ret 8}}
const struct TribalAffinityLayout {uint32_t start;int size;const char* bytes;} g_tribalAffinityLayouts[]={
    {0x5e9cac,59,"\x8b\x86\xe8\x00\x00\x00\x50\x55\xb9\xc4\x16\xa9\x09\xe8\x52\xc9\x00\x00\x85\xc0\x74\x25\x6a\x00\x56\xb9\x88\x78\x9c\x09\xe8\x21\x8e\xe9\xff\x85\xc0\x0f\x84\xd2\x00\x00\x00\x50\xb9\x88\x78\x9c\x09\xe8\xde\x92\xe9\xff\xe9\xc2\x00\x00\x00"},
    {0x5ed9b7,44,"\x8b\x86\xe8\x00\x00\x00\x83\xc4\x08\x02\xda\x8b\x54\x24\x14\x50\x52\xb9\xc4\x16\xa9\x09\xe8\x3e\x8c\x00\x00\x85\xc0\x75\x6f\x6a\x5f\x8b\xce\xe8\x11\xb7\xe9\xff\x85\xc0\x75\x62"},
    {0x5f6610,79,"\x8b\x44\x24\x08\x83\xf8\x5b\x56\x7c\x36\x83\xf8\x5e\x7f\x31\x8b\x74\x24\x08\x8d\x94\x40\xef\xfe\xff\xff\x33\xc9\x8d\x14\x95\x44\xcf\x84\x00\x85\xf6\x7c\x09\x83\xfe\x29\x7f\x04\x8b\xc6\xeb\x03\x83\xc8\xff\x3b\x02\x74\x0f\x41\x83\xc2\x04\x83\xf9\x03\x7c\xe3\x33\xc0\x5e\xc2\x08\x00\xb8\x01\x00\x00\x00\x5e\xc2\x08\x00"},
};
bool verifyTribalAffinityLayout(){
    for(const auto& s:g_tribalAffinityLayouts){auto p=(const uint8_t*)(g_strategyCodeBase+s.start-0x400000);
        for(int n=0;n<s.size;++n){
            if(s.start+n>=0x5f6637 && s.start+n<0x5f663c)continue;
            if(p[n]!=(uint8_t)s.bytes[n])return false;
        }
    }
    const auto at=(const uint8_t*)(g_strategyCodeBase+0x5f6637-0x400000);
    if(!memcmp(at,"\x83\xfe\x29\x7f\x04",5))return true;
    if(at[0]!=0xe9)return false;
    auto cave=(const uint8_t*)((uintptr_t)at+5+*(const int32_t*)(at+1));MEMORY_BASIC_INFORMATION memory={};
    if(!VirtualQuery(cave,&memory,sizeof memory) || memory.State!=MEM_COMMIT ||
       (memory.Protect&(PAGE_NOACCESS|PAGE_GUARD)) || memory.RegionSize<17 ||
       (uintptr_t)cave-(uintptr_t)memory.BaseAddress>memory.RegionSize-17)return false;
    int bound=*(const int*)(cave+2);
    return !memcmp(cave,"\x81\xfe",2) && bound>=41 && bound<65535 && !memcmp(cave+6,"\x0f\x8f",2) && cave[12]==0xe9 &&
        (uintptr_t)cave+12+*(const int32_t*)(cave+8)==(uintptr_t)at+9 &&
        (uintptr_t)cave+17+*(const int32_t*)(cave+13)==(uintptr_t)at+5;
}
