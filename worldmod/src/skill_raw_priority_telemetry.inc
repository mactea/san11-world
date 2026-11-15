// The native comparator checks raw84, then skill category/stat/pointer order.
// A composed skill containing84 does not satisfy this raw-ID priority check.
uint32_t g_rawPriorityResume=0x5dea35;
__declspec(naked) void rawPriorityNative(){__asm {mov eax,[esp+8]
    push ebp
    jmp dword ptr [g_rawPriorityResume]}}
typedef int(__cdecl* RawPriorityCompare)(void*,void*,int);
RawPriorityCompare g_rawPriorityOriginal=(RawPriorityCompare)rawPriorityNative;
int __cdecl rawPriorityCompare(void* first,void* second,int kind){
    if(!skilltelemetry::enabled())return g_rawPriorityOriginal(first,second,kind);
    PersonReceipt a(first),b(second);
    int result=g_rawPriorityOriginal(first,second,kind);
    if(a.owned==84 && a.valid(84))skilltelemetry::matched(a.person,84,84,0x5dea48,
        "officer_selection","guidance_worker_priority_checked");
    if(b.owned==84 && b.valid(84))skilltelemetry::matched(b.person,84,84,0x5dea50,
        "officer_selection","guidance_worker_priority_checked");
    return result;
}
const struct RawPriorityLayout {uint32_t start;int size;const char* bytes;} g_rawPriorityLayouts[]={
    {0x5dea30,207,"\x8b\x44\x24\x08\x55\x8b\x6c\x24\x08\x8b\x8d\xe8\x00\x00\x00\x56\x8b\xb0\xe8\x00\x00\x00\x33\xc0\x83\xf9\x54\x0f\x94\xc0\x33\xd2\x83\xfe\x54\x0f\x94\xc2\x57\x85\xc0\x8b\xfa\x75\x09\x33\xd2\x85\xff\x0f\x94\xc2\x8b\xfa\x85\xff\x0f\x84\x8d\x00\x00\x00\x53\x51\xb9\x58\x19\x20\x07\xe8\xd6\x21\xeb\xff\x56\xb9\x58\x19\x20\x07\x8b\xf8\xe8\xc9\x21\xeb\xff\x57\x8b\xf0\xe8\xa1\xbb\xe9\xff\x83\xc4\x04\x85\xc0\xbb\x01\x00\x00\x00\x74\x09\x39\x5f\x64\x75\x04\x8b\xfb\xeb\x02\x33\xff\x56\xe8\x84\xbb\xe9\xff\x83\xc4\x04\x85\xc0\x74\x09\x39\x5e\x64\x75\x04\x8b\xc3\xeb\x02\x33\xc0\x85\xff\x75\x09\x33\xc9\x85\xc0\x0f\x94\xc1\x8b\xc1\x85\xc0\x75\x07\x5b\x8b\xc7\x5f\x5e\x5d\xc3\x8b\x74\x24\x1c\x56\x8b\xcd\xe8\x4e\xa5\xea\xff\x56\x8b\x74\x24\x1c\x8b\xce\x8a\xd8\xe8\x40\xa5\xea\xff\x3a\xc3\x75\x02\x3b\xee\x1b\xc0\xf7\xd8\x5b\x5f\x5e\x5d\xc3"},
};
bool verifyRawPriorityLayout(){
    for(const auto& s:g_rawPriorityLayouts)if(memcmp((const void*)(g_strategyCodeBase+s.start-0x400000),s.bytes,s.size))return false;
    return true;
}
