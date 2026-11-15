#include "../src/skill_telemetry.cpp"
#include <assert.h>
#include <thread>
#include <vector>
static bool actor(const void*,skilltelemetry::Actor& a) {
    a.person=14162;a.force=1;a.ruler=635;a.city=8;a.year=194;a.month=6;a.day=11;
    strcpy_s(a.name,"A,\"B");
    WideCharToMultiByte(950,0,L"劉備",-1,a.rulerName,sizeof a.rulerName,nullptr,nullptr);return true;
}
static HANDLE resolverEntered, resolverRelease;
static bool pausedActor(const void* person,skilltelemetry::Actor& a) {
    actor(person,a);
    if(person==(void*)2) {
        assert(SetEvent(resolverEntered));
        assert(WaitForSingleObject(resolverRelease,5000)==WAIT_OBJECT_0);
    }
    return true;
}
static void assertOneControlRow(const wchar_t* path) {
    FILE* stream=nullptr;assert(!_wfopen_s(&stream,path,L"rb"));
    int lines=0,c;while((c=fgetc(stream))!=EOF)if(c=='\n')++lines;
    fclose(stream);assert(lines==2); // header plus the current-session control
}
static void sessionRaceTests() {
    resolverEntered=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    resolverRelease=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    assert(resolverEntered && resolverRelease);
    for(bool reopen:{false,true}) {
        const wchar_t* before=reopen?L"build/telemetry_session_old_test.csv":L"build/telemetry_context_test.csv";
        const wchar_t* after=L"build/telemetry_session_new_test.csv";
        ResetEvent(resolverEntered);ResetEvent(resolverRelease);
        assert(skilltelemetry::open(before,pausedActor,nullptr));
        if(reopen)skilltelemetry::applied((void*)1,61,61,"test","control",-1,-1,-1,"gold",10,11);
        std::thread pending([]{skilltelemetry::matched((void*)2,99,99,0);});
        assert(WaitForSingleObject(resolverEntered,5000)==WAIT_OBJECT_0);
        if(reopen)assert(skilltelemetry::open(after,pausedActor,nullptr));
        else skilltelemetry::invalidateContext();
        assert(SetEvent(resolverRelease));pending.join();
        skilltelemetry::applied((void*)1,61,61,"test","control",-1,-1,-1,"gold",10,11);
        skilltelemetry::close();
        assertOneControlRow(before);
        if(reopen)assertOneControlRow(after);
    }
    CloseHandle(resolverEntered);CloseHandle(resolverRelease);
}
static void liveReadTests() {
    const wchar_t* path=L"build/telemetry_live_read_test.csv";
    assert(skilltelemetry::open(path,actor,nullptr));
    HANDLE reader=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(reader!=INVALID_HANDLE_VALUE);
    auto lines=[&](){
        assert(SetFilePointer(reader,0,nullptr,FILE_BEGIN)!=INVALID_SET_FILE_POINTER);
        char bytes[4096];DWORD size=0;assert(ReadFile(reader,bytes,sizeof bytes,&size,nullptr));
        int count=0;for(DWORD n=0;n<size;++n)if(bytes[n]=='\n')++count;return count;
    };
    assert(lines()==1);
    skilltelemetry::applied((void*)1,61,61,"test","control",-1,-1,-1,"gold",10,11);
    assert(lines()==2 && skilltelemetry::enabled());
    HANDLE writer=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(writer==INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION);
    skilltelemetry::applied((void*)1,61,61,"test","control",-1,-1,-1,"gold",20,25);
    assert(lines()==3 && skilltelemetry::enabled());
    assert(CloseHandle(reader));skilltelemetry::close();
    puts("PASS live CSV reader: header and flushed effects readable while writer remains active; second writer rejected without disrupting logging");
}
int wmain(int argc,wchar_t** argv) {
    assert(argc==2);
    assert(!skilltelemetry::enabled());
    uint64_t closedSession=skilltelemetry::contextGeneration();
    skilltelemetry::matched(nullptr,1,1,0); // disabled must be safe with no game mapped
    assert(skilltelemetry::open(argv[1],actor,nullptr));
    uint64_t openedSession=skilltelemetry::contextGeneration();assert(openedSession!=closedSession);
    std::vector<std::thread> threads;
    for(int t=0;t<4;++t)threads.emplace_back([]{
        for(int i=0;i<100;++i)skilltelemetry::matched((void*)1,101,61,0x4890f6);
    });
    for(auto& t:threads)t.join();
    skilltelemetry::applied((void*)1,147,147,"trade","sale_bonus",-1,2,20,"gold",100,120,0x1234);
    skilltelemetry::close();
    assert(!skilltelemetry::enabled());
    assert(skilltelemetry::contextGeneration()!=openedSession);
    sessionRaceTests();
    liveReadTests();
    puts("PASS disabled path, concurrent query attribution, actual effect row, CSV close and session invalidation");
}
