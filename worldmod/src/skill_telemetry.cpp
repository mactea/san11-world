#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <share.h>
#include <string>
#include <sstream>
#include <atomic>
#include "skill_telemetry.h"

namespace skilltelemetry {
namespace {
SRWLOCK lock=SRWLOCK_INIT;
FILE* file=nullptr;
std::atomic<bool> active{false};
std::atomic<uint64_t> generation{0};
Resolve resolver=nullptr;
TraceLogFn logger=nullptr;
uint64_t sequence=0;
DWORD lastFlush=0;
thread_local bool inside=false;
struct ReentryReset { ~ReentryReset(){inside=false;} };
struct Exclusive {
    Exclusive(){AcquireSRWLockExclusive(&lock);}
    ~Exclusive(){ReleaseSRWLockExclusive(&lock);}
};

std::string csv(const std::string& value) {
    std::string out="\"";
    for(char c:value) { if(c=='\"')out+='\"'; out+=c; }
    return out+'\"';
}
std::string utf8(const char* text) {
    wchar_t wide[64]={}; char out[256]={};
    if(!MultiByteToWideChar(950,0,text,-1,wide,64))return "";
    if(!WideCharToMultiByte(CP_UTF8,0,wide,-1,out,256,nullptr,nullptr))return "";
    return out;
}
void emit(const void* person,int owned,int effect,const char* stage,const char* scene,
          const char* event,int targetPerson,int targetForce,int targetCity,
          const char* metric,int64_t before,int64_t after,uintptr_t caller,
          bool hasBaseline=false,int64_t baselineAfter=0,int targetBase=-1,int targetUnit=-1,int equipment=-1,int actorUnit=-1,
          const Target* context=nullptr) {
    if(inside || !enabled())return;
    inside=true;
    ReentryReset reentry;
    Resolve eventResolver;
    uint64_t eventGeneration;
    {
        Exclusive guard;
        if(!file || !enabled())return;
        eventResolver=resolver;
        eventGeneration=generation.load(std::memory_order_relaxed);
    }
    Actor actor;
    if(eventResolver && person)eventResolver(person,actor);
    SYSTEMTIME now;GetLocalTime(&now);
    char timestamp[40];sprintf_s(timestamp,"%04u-%02u-%02uT%02u:%02u:%02u.%03u",
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
    // Formatting uses only this call's snapshot. Keep conversion/allocation out
    // of the writer lock so one game's worker cannot block every other worker.
    std::ostringstream row;
    row<<GetCurrentProcessId()<<','<<GetCurrentThreadId()<<','<<csv(timestamp)<<','
           <<actor.year<<','<<actor.month<<','<<actor.day<<','<<csv(stage)<<','<<csv(scene)<<','<<csv(event)<<','
           <<owned<<','<<effect<<','<<actor.person<<','<<csv(utf8(actor.name))<<','
           <<actor.force<<','<<actor.ruler<<','<<csv(utf8(actor.rulerName))<<','<<actor.city<<','
           <<targetPerson<<','<<targetForce<<','<<targetCity<<','<<csv(metric)<<',';
        if(metric && *metric)row<<before<<','<<after<<','<<(after-before);
        else row<<",,"; // unavailable effect values stay empty, not fabricated zeroes
        row<<','<<caller<<',';
        if(hasBaseline)row<<baselineAfter<<','<<(after-baselineAfter);
        else row<<',';
        row<<','<<targetBase<<','<<targetUnit<<','<<equipment<<','<<actorUnit<<','
           <<(context?context->x:-1)<<','<<(context?context->y:-1)<<','<<(context?context->terrain:-1)<<','
           <<csv(context && context->context?context->context:"")<<"\r\n";
    std::string bytes=row.str();
    Exclusive guard;
    if(file && enabled() && generation.load(std::memory_order_relaxed)==eventGeneration) {
        char prefix[32];int prefixSize=sprintf_s(prefix,"%llu,",(unsigned long long)++sequence);
        bool failed=fwrite(prefix,1,prefixSize,file)!=(size_t)prefixSize;
        failed=fwrite(bytes.data(),1,bytes.size(),file)!=bytes.size() || failed;
        DWORD tick=GetTickCount();
        if(tick-lastFlush>=1000 || *stage=='a') {failed=fflush(file)!=0 || failed;lastFlush=tick;}
        if(failed) {active.store(false);fclose(file);file=nullptr;if(logger)logger("skill telemetry: CSV write failed; logging disabled");}
    }
}
}
bool open(const wchar_t* path,Resolve resolve,TraceLogFn log) {
    close();AcquireSRWLockExclusive(&lock);
    resolver=resolve;logger=log;sequence=0;
    file=_wfsopen(path,L"wb",_SH_DENYWR);
    if(file) {
        const char* header="\xef\xbb\xbfseq,pid,thread,wall_time,game_year,game_month,game_day,stage,scene,event,owned_skill,effect_skill,person_id,person_name,force_id,ruler_id,ruler_name,actor_city,target_person,target_force,target_city,metric,before,after,delta,caller,baseline_after,bonus_delta,target_base,target_unit,equipment_id,actor_unit,target_x,target_y,terrain_id,context_json\r\n";
        if(fputs(header,file)<0 || fflush(file)) {fclose(file);file=nullptr;}
    }
    bool ok=file!=nullptr;
    active.store(ok);
    ReleaseSRWLockExclusive(&lock);
    return ok;
}
void close() {active.store(false);AcquireSRWLockExclusive(&lock);++generation;if(file){fflush(file);fclose(file);file=nullptr;}ReleaseSRWLockExclusive(&lock);}
bool enabled() {return active.load(std::memory_order_relaxed);}
uint64_t contextGeneration() {return generation.load(std::memory_order_relaxed);}
void invalidateContext() {++generation;}
void matched(const void* p,int owned,int effect,uintptr_t caller,const char* scene,const char* event) {
    try {emit(p,owned,effect,"check_matched",scene,event,-1,-1,-1,"",0,0,caller);}
    catch(...) {close();if(logger)logger("skill telemetry: exception; logging disabled, gameplay continues");}
}
void matched(const void* p,int owned,int effect,uintptr_t caller,const char* scene,const char* event,const Target& target) {
    try {emit(p,owned,effect,"check_matched",scene,event,target.person,target.force,target.city,"",0,0,caller,false,0,target.base,target.unit,target.equipment,target.actorUnit,&target);}
    catch(...) {close();if(logger)logger("skill telemetry: exception; logging disabled, gameplay continues");}
}
void applied(const void* p,int owned,int effect,const char* scene,const char* event,
             int targetPerson,int targetForce,int targetCity,const char* metric,
             int64_t before,int64_t after,uintptr_t caller,int targetBase,int targetUnit,int equipment,int actorUnit) {
    try {emit(p,owned,effect,"applied",scene,event,targetPerson,targetForce,targetCity,metric,before,after,caller,false,0,targetBase,targetUnit,equipment,actorUnit);}
    catch(...) {close();if(logger)logger("skill telemetry: exception; logging disabled, gameplay continues");}
}
void appliedBonus(const void* p,int owned,int effect,const char* scene,const char* event,
                  const char* metric,int64_t before,int64_t after,int64_t baselineAfter,uintptr_t caller,
                  const Target& target) {
    try {emit(p,owned,effect,"applied",scene,event,target.person,target.force,target.city,metric,before,after,caller,true,baselineAfter,target.base,target.unit,target.equipment,target.actorUnit,&target);}
    catch(...) {close();if(logger)logger("skill telemetry: exception; logging disabled, gameplay continues");}
}
void appliedEvent(const void* p,int owned,int effect,const char* scene,const char* event,
                  uintptr_t caller,const Target& target) {
    try {emit(p,owned,effect,"applied",scene,event,target.person,target.force,target.city,"",0,0,caller,false,0,target.base,target.unit,target.equipment,target.actorUnit,&target);}
    catch(...) {close();if(logger)logger("skill telemetry: exception; logging disabled, gameplay continues");}
}
void applied(const void* p,int owned,int effect,const char* scene,const char* event,
             const char* metric,int64_t before,int64_t after,uintptr_t caller,const Target& target) {
    try {emit(p,owned,effect,"applied",scene,event,target.person,target.force,target.city,metric,before,after,caller,false,0,target.base,target.unit,target.equipment,target.actorUnit,&target);}
    catch(...) {close();if(logger)logger("skill telemetry: exception; logging disabled, gameplay continues");}
}
}
