#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <vector>
#include "garrison.h"
#include "garrison_model.h"
#include "pathfind.h"
#include "movement.h"

namespace {
const uint32_t WORLD=0x7201958,PROC=0x799895c;
GarrisonSetup g_setup={};
TraceLogFn g_log=nullptr;
bool g_on=false;
int g_radius=51;
int g_foodPeriods=10;
SRWLOCK g_lock=SRWLOCK_INIT;
struct Assessment {
    uint64_t signature=0;
    int turn=-1,owner=-1,hops=255,paths=0,unknown=0,oldReserve=0,target=0;
    int64_t enemies=0;
    int commander=-1,foodPeriods=10;
    garrison::Commander profile;
    garrison::History history;
};
std::vector<Assessment> g_cities;
typedef uint8_t* (__thiscall* GetFn)(uint32_t,int);
GetFn baseRec=(GetFn)0x490d00,corpsRec=(GetFn)0x490ad0,personRec=(GetFn)0x490b00;
auto validFn=(int(__cdecl*)(void*))0x47a630;
auto baseId=(int(__thiscall*)(uint32_t,void*))0x491770;
auto hostileFn=(int(__thiscall*)(uint32_t,int,int))0x4b5cc0;
auto abilityFn=(uint8_t(__thiscall*)(void*,int))0x489030;
bool valid(void* p) { return p && validFn(p); }
int value(void* p,int slot) { return ((int(__thiscall*)(void*))(*(void***)p)[slot/4])(p); }
const int16_t* position(void* p) { return ((const int16_t*(__thiscall*)(void*))(*(void***)p)[0x3c/4])(p); }
int distance(int x,int y,int u,int v) {
    int a=x-u,b=(y-x/2)-(v-u/2);
    return std::max({abs(a),abs(b),abs(a+b)});
}
uint32_t pack(int x,int y) {return (uint16_t)x | (uint32_t)(uint16_t)y<<16;}
int turn() {
    const int* w=(const int*)WORLD;
    return ((w[2]-1)*12+w[3]-1)*3+w[0x5c/4]+(w[4]-1)/10;
}
void hash(uint64_t& h,int n) {h^=(uint32_t)n;h*=1099511628211ULL;}
struct Enemy {uint8_t* unit;uint32_t pos;int troops,dist;};
garrison::Commander officer(int id,int force) {
    auto p=id>=0 ? personRec(WORLD,id) : nullptr;
    if(!valid(p) || value(p,0x40)!=force)return {};
    // Scenario byte 129 (personality) serializes to person +0xfc. +0xf4 is ambition.
    return {*(int*)(p+0xfc),abilityFn(p,0),abilityFn(p,2),abilityFn(p,1),abilityFn(p,3)};
}

int calculate(uint8_t* city,int original) {
    g_foodPeriods=10;
    if(!g_on || !valid(city))return original;
    int id=baseId(WORLD,city);
    if(id<0 || id>=g_setup.bases->C)return original;
    int owner=value(city,0x40),now=turn();
    if(owner<0 || owner>=g_setup.forces)return original;
    int corps=value(city,0x44),commander=-1;
    auto cr=corps>=0 && corps<g_setup.forces+5 ? corpsRec(WORLD,corps) : nullptr;
    if(valid(cr) && *(int*)(cr+4)==owner)commander=*(int*)(cr+0xc);
    auto profile=officer(commander,owner);
    g_foodPeriods=garrison::foodPeriods(profile);
    const int16_t* cp=position(city);int x=cp[0],y=cp[1];
    std::vector<uint8_t> hostile(g_setup.forces+5,0);
    for(int f=0;f<(int)hostile.size();++f) if(f!=owner)
        hostile[f]=hostileFn(PROC,owner,f)!=0;
    int hops=255;uint64_t sig=1469598103934665603ULL;
    for(int b=0;b<g_setup.bases->N;++b) {
        auto p=baseRec(WORLD,b);if(!valid(p))continue;
        int f=value(p,0x40);
        if(f>=0 && f<(int)hostile.size() && hostile[f]) {
            int d=g_setup.bases->MAT87B[(size_t)b*g_setup.bases->N+id];
            hops=std::min(hops,d);
        }
    }
    hash(sig,hops);hash(sig,x);hash(sig,y);
    std::vector<Enemy> enemies;
    for(int i=0;i<g_setup.units->U;++i) {
        auto u=g_setup.units->UNITARR+(size_t)i*0xf4;
        if(!valid(u) || *(int*)(u+8)==1)continue; // transports do not attack cities
        int f=value(u,0x40);
        if(f<0 || f>=(int)hostile.size() || !hostile[f])continue;
        const int16_t* pos=position(u);int d=distance(x,y,pos[0],pos[1]);
        if(d>g_radius)continue; // maximum three-turn step cap, plus firing range
        int troops=value(u,0x50);if(troops<=0)continue;
        int weight=garrison::attackWeight(officer(*(int*)(u+0xc),f),*(int*)(u+0x48));
        troops=(int)(((int64_t)troops*weight+99)/100);
        uint32_t pk=pack(pos[0],pos[1]);
        hash(sig,i);hash(sig,*(int*)(u+0xc));hash(sig,pk);hash(sig,troops);
        hash(sig,*(int*)(u+0xa8));hash(sig,u[0xcc]);hash(sig,*(int*)(u+0x24));
        enemies.push_back({u,pk,troops,d});
    }
    AcquireSRWLockExclusive(&g_lock);
    auto& a=g_cities[id];
    if(a.turn!=now || a.owner!=owner || a.signature!=sig) {
        a.hops=hops;a.enemies=0;a.paths=0;a.unknown=0;
        std::sort(enemies.begin(),enemies.end(),[](const Enemy& a,const Enemy& b){return a.dist<b.dist;});
        std::vector<uint32_t> targets;
        for(int r=x-3;r<=x+3;++r)for(int c=y-3;c<=y+3;++c) {
            int d=distance(x,y,r,c);
            if(d>=1 && d<=3 && r>=0 && r<g_setup.height && c>=0 && c<g_setup.width)
                targets.push_back(pack(r,c));
        }
        ULONGLONG started=GetTickCount64();
        for(auto& e:enemies) {
            if(e.dist<=3) {a.enemies+=e.troops;continue;}
            if(!pathfindReady() || !movementReady() || a.paths>=8 || GetTickCount64()-started>=80) {
                ++a.unknown;a.enemies+=e.troops;continue; // untested threats never make a city safe
            }
            std::vector<uint32_t> path;int cost=0;++a.paths;
            if(!pathfindRoute(e.unit,e.pos,targets,&path,&cost))continue;
            int eta=movementRouteTime(e.unit,path,g_setup.hex20,g_setup.width,g_setup.height).turns;
            if(eta<0) {++a.unknown;a.enemies+=e.troops;}
            else if(eta<=3)a.enemies+=e.troops;
        }
        a.turn=now;a.owner=owner;a.signature=sig;
    }
    a.oldReserve=original;a.commander=commander;a.profile=profile;a.foodPeriods=g_foodPeriods;
    a.target=garrison::reserve(a.hops,a.enemies,a.unknown!=0,profile);
    if(a.unknown)a.target=std::max(a.target,original);
    int before=a.history.held;
    int result=a.history.update(owner,now,a.target,garrison::safeTurns(profile));
    if(result!=before && g_log)
        g_log("garrison: city=%d force=%d turn=%d native=%d reserve=%d target=%d enemy_hops=%d approaching=%lld paths=%d unknown=%d",
            id,owner,now,original,result,a.target,a.hops,a.enemies,a.paths,a.unknown);
    ReleaseSRWLockExclusive(&g_lock);
    return result;
}

// Enter before native food-reserve computation. [esp+e0] is the source base;
// EBP and [esp+28] both hold reserved troops. All other registers/flags survive.
uint32_t g_back=0x5ef71b;
__declspec(naked) void reserveHook() {
    __asm {
        pushfd
        pushad
        mov eax,[esp+104h]
        push ebp
        push eax
        call calculate
        add esp,8
        mov [esp+8],eax
        popad
        popfd
        mov [esp+28h],ebp
        push g_foodPeriods
        push ebp
        mov ecx,09a916c4h
        jmp g_back
    }
}
}

bool garrisonInstall(const GarrisonSetup& s,TraceLogFn log) {
    wchar_t ini[MAX_PATH];swprintf_s(ini,L"%s\\worldmod.ini",s.dir);
    if(!GetPrivateProfileIntW(s.section,L"ai_garrison",1,ini))return true;
    if(!s.bases || !s.units || !s.hex20 || !s.bases->MAT87B)return false;
    const uint8_t expected[]={0x6a,0x0a,0x55,0xb9,0xc4,0x16,0xa9,0x09};
    auto site=(uint8_t*)0x5ef713;
    if(memcmp(site,expected,sizeof expected)) {log("garrison: unexpected reserve hook, not installed");return false;}
    if(!s.write) {log("garrison: reserve hook verified");return true;}
    g_setup=s;g_log=log;g_cities.resize(s.bases->C);
    int cap=GetPrivateProfileIntW(s.section,L"move_cap_ship",16,ini);
    g_radius=3*std::max(10,std::min(25,cap))+3;
    DWORD old;
    if(!VirtualProtect(site,sizeof expected,PAGE_EXECUTE_READWRITE,&old))return false;
    site[0]=0xe9;*(int32_t*)(site+1)=(int32_t)((uintptr_t)reserveHook-(uintptr_t)(site+5));
    memset(site+5,0x90,3);VirtualProtect(site,sizeof expected,old,&old);
    FlushInstructionCache(GetCurrentProcess(),site,sizeof expected);g_on=true;
    log("garrison: installed; commander-driven reserve, rear/buffer/frontier bases=5000/10000/18000, cooldown=3..5 turns");
    return true;
}
void garrisonReset() {
    AcquireSRWLockExclusive(&g_lock);
    for(auto& a:g_cities)a=Assessment();
    ReleaseSRWLockExclusive(&g_lock);
}
void garrisonDump(FILE* out) {
    if(!TryAcquireSRWLockShared(&g_lock)) {fprintf(out,"garrison: busy, retry\n");return;}
    fprintf(out,"garrison: %s; cached AI evaluations only\n",g_on?"installed":"disabled");
    for(size_t i=0;i<g_cities.size();++i) {
        const auto& a=g_cities[i];if(a.turn<0)continue;
        fprintf(out,"city=%zu force=%d turn=%d native=%d reserve=%d target=%d hops=%d enemy=%lld paths=%d unknown=%d lower_since=%d\n",
            i,a.owner,a.turn,a.oldReserve,a.history.held,a.target,a.hops,a.enemies,a.paths,a.unknown,a.history.lowerSince);
        fprintf(out,"  commander=%d character=%d lead=%d int=%d martial=%d politics=%d food_periods=%d safe_turns=%d\n",
            a.commander,a.profile.personality,a.profile.leadership,a.profile.intelligence,a.profile.martial,a.profile.politics,a.foodPeriods,garrison::safeTurns(a.profile));
    }
    ReleaseSRWLockShared(&g_lock);
}
