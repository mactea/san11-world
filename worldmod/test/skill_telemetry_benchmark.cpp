// Exercises the production CSV writer. The actor resolver is synthetic; this is
// not a native-game hook, gameplay or frame-time benchmark.
#include "../src/skill_telemetry.cpp"
#include <algorithm>
#include <assert.h>
#include <psapi.h>
#include <thread>
#include <vector>

static std::atomic<unsigned> resolved{0};
static bool benchmarkActor(const void* person,skilltelemetry::Actor& a) {
    ++resolved;
    a.person=14158+(int)(uintptr_t)person;a.force=249;a.ruler=14162;a.city=529;
    a.year=194;a.month=6;a.day=11;
    strcpy_s(a.name,"CSV,\"actor");
    strcpy_s(a.rulerName,"CSV ruler");
    return true;
}
static int64_t ticks() {
    LARGE_INTEGER value;assert(QueryPerformanceCounter(&value));return value.QuadPart;
}
static SIZE_T peakMemory() {
    PROCESS_MEMORY_COUNTERS info={};info.cb=sizeof info;
    assert(GetProcessMemoryInfo(GetCurrentProcess(),&info,sizeof info));return info.PeakWorkingSetSize;
}
static void runCase(FILE* report,const wchar_t* folder,const char* name,int workers,int each,int appliedEvery,bool enabled) {
    wchar_t path[MAX_PATH];swprintf_s(path,L"%s\\%hs.csv",folder,name);
    resolved.store(0);
    if(enabled)assert(skilltelemetry::open(path,benchmarkActor,nullptr));
    else {skilltelemetry::close();assert(!skilltelemetry::enabled());}
    HANDLE start=CreateEventW(nullptr,TRUE,FALSE,nullptr);assert(start);
    std::atomic<int> ready{0};
    std::vector<std::vector<int64_t>> samples(workers,std::vector<int64_t>(each));
    std::vector<std::thread> threads;
    for(int worker=0;worker<workers;++worker)threads.emplace_back([&,worker]{
        skilltelemetry::Target target;target.force=1;target.city=8;target.base=8;
        target.unit=999;target.actorUnit=998;target.equipment=3;
        target.x=900;target.y=1700;target.terrain=6;
        target.context="{\"performance_fixture\":true,\"whole_change_not_marginal_skill_bonus\":true}";
        ++ready;assert(WaitForSingleObject(start,10000)==WAIT_OBJECT_0);
        for(int i=0;i<each;++i) {
            int64_t begin=ticks();
            if(appliedEvery && i%appliedEvery==0)
                skilltelemetry::applied((void*)(uintptr_t)(worker+1),249,24,"benchmark","resource_committed",
                    "troops",1000,900,0x584fdf,target);
            else skilltelemetry::matched((void*)(uintptr_t)(worker+1),249,24,0x4890f6,"benchmark","predicate");
            samples[worker][i]=ticks()-begin;
        }
    });
    while(ready.load()!=workers)SwitchToThread();
    LARGE_INTEGER frequency;assert(QueryPerformanceFrequency(&frequency));
    int64_t begin=ticks();assert(SetEvent(start));
    for(auto& thread:threads)thread.join();
    skilltelemetry::close();
    int64_t elapsed=ticks()-begin;
    assert(resolved.load()==(enabled?(unsigned)(workers*each):0));
    CloseHandle(start);
    std::vector<int64_t> sorted;sorted.reserve(workers*each);
    for(auto& sample:samples)sorted.insert(sorted.end(),sample.begin(),sample.end());
    std::sort(sorted.begin(),sorted.end());
    double us=1000000.0/frequency.QuadPart;
    unsigned applied=appliedEvery?workers*((each+appliedEvery-1)/appliedEvery):0;
    fprintf(report,"{\"name\":\"%s\",\"workers\":%d,\"calls\":%d,\"enabled\":%s,"
        "\"expected_rows\":%d,\"expected_applied\":%u,\"resolver_calls\":%u,"
        "\"elapsed_ms_including_close\":%.3f,\"calls_per_second\":%.1f,"
        "\"call_latency_us\":{\"p50\":%.3f,\"p95\":%.3f,\"p99\":%.3f,\"max\":%.3f},"
        "\"process_peak_working_set_bytes\":%llu}",name,workers,workers*each,enabled?"true":"false",
        enabled?workers*each:0,enabled?applied:0,resolved.load(),elapsed*us/1000,
        workers*each*1000000.0/(elapsed*us),sorted[sorted.size()/2]*us,
        sorted[sorted.size()*95/100]*us,sorted[sorted.size()*99/100]*us,sorted.back()*us,
        (unsigned long long)peakMemory());
    printf("%s: %d calls, %.1f ms, p99 %.2f us\n",name,workers*each,elapsed*us/1000,sorted[sorted.size()*99/100]*us);
}
int wmain(int argc,wchar_t** argv) {
    assert(argc==2);CreateDirectoryW(argv[1],nullptr);
    wchar_t path[MAX_PATH];swprintf_s(path,L"%s\\measurements.json",argv[1]);
    FILE* report=nullptr;assert(!_wfopen_s(&report,path,L"wb"));
    fputs("{\"writer_only\":true,\"synthetic_resolver\":true,\"live_gameplay_validated\":false,\"cases\":[",report);
    runCase(report,argv[1],"disabled",1,1000000,10,false);fputs(",",report);
    runCase(report,argv[1],"queries",1,20000,0,true);fputs(",",report);
    runCase(report,argv[1],"applied",1,20000,1,true);fputs(",",report);
    runCase(report,argv[1],"mixed_single",1,100000,10,true);fputs(",",report);
    runCase(report,argv[1],"mixed_four",4,25000,10,true);
    fputs("]}\n",report);assert(!fclose(report));
    puts("PASS sustained writer workload and disabled resolver exclusion");
}
