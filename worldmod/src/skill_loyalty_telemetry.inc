struct LoyaltyProvider {void* city=nullptr;const void* actor=nullptr;PersonReceipt receipt;int cityId=-1,cityForce=-1;};
thread_local LoyaltyProvider g_loyaltyProvider;
int __fastcall loyaltyFind(void* city,void*,int effect) {
    typedef int(__thiscall* Find)(void*,int);
    auto original=(*(Find*)(*(uint8_t**)city+0x5c));
    g_loyaltyProvider=LoyaltyProvider{};
    if(!skilltelemetry::enabled())return original(city,effect);
    PersonReceipt receipt;ProgressionProbe probe(96,&receipt);
    int result=original(city,effect);
    // The original lower/upper-ID branches decide whether the aura becomes active.
    if(result>=0 && effect==96) {
        auto person=g_telemetryPerson((void*)0x7201958,result);
        if(person && probe.actor==person && receipt.valid(96)){
            auto target=incomeTarget(city);g_loyaltyProvider={city,person,receipt,target.base,target.force};
        }
    }
    return result;
}
uint32_t g_loyaltyFindResume=0x58e5d1,g_loyaltyDecay=0x58e626,g_loyaltySkip=0x58e7bb;
__declspec(naked) void loyaltyFindBridge() {
    __asm {call loyaltyFind
           cmp eax,edi
           jmp dword ptr [g_loyaltyFindResume]}
}
void __cdecl recordLoyaltySkip(void* city,uint8_t* person) {
    auto provider=g_loyaltyProvider;
    if(!provider.actor || provider.city!=city || !provider.receipt.valid(96))return;
    int id=growthPersonId(person);if(id<0)return;
    auto target=incomeTarget(city);if(target.base!=provider.cityId || target.force!=provider.cityForce)return;target.person=id;
    target.force=(*(BuildingValue*)(*(uint8_t**)person+0x40))(person);
    char context[112];sprintf_s(context,"{\"loyalty_before\":%d,\"loyalty_after\":%d,\"decay_check_skipped\":true}",person[0xac],person[0xac]);
    target.context=context;
    // The decay code has later relationship/date/technology/RNG gates. Do not invent a loss.
    if(!provider.receipt.valid(96))return;
    skilltelemetry::appliedEvent(provider.actor,provider.receipt.owned,96,
        "loyalty","loyalty_decay_check_skipped",0x58e626,target);
}
__declspec(naked) void loyaltySkipBridge() {
    __asm {
        cmp [esp+0x10],edi
        je decay
        pushfd
        pushad
        push esi
        push ebx
        call recordLoyaltySkip
        add esp,8
        popad
        popfd
        jmp dword ptr [g_loyaltySkip]
    decay:
        jmp dword ptr [g_loyaltyDecay]
    }
}
