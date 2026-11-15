// Native calls are replaced; the production collector, cache, policy and x86
// hook execute unchanged. No game process or save is touched.
#include "../src/garrison.cpp"
static uint8_t cities[4][0xf4]={},units[10][0xf4]={};
static uint8_t corps[0xf4]={},person[0x190]={};
static bool hasCommander=false;
static bool war[8]={},reachable=true,ready=true;
static int eta=1,routeCalls=0,failures=0;
static void check(bool ok,const char* s){printf("%s %s\n",ok?"PASS":"FAIL",s);failures+=!ok;}
static int __cdecl isValid(void* p){return p && ((uint8_t*)p)[0x70]!=0;}
static uint8_t* __fastcall getBase(uint32_t,void*,int id){return id>=0 && id<4 ? cities[id] : nullptr;}
static int __fastcall idOf(uint32_t,void*,void* p){return (int)(((uint8_t*)p-cities[0])/0xf4);}
static int __fastcall enemyOf(uint32_t,void*,int,int f){return f>=0 && f<8 && war[f];}
static uint8_t* __fastcall getCorps(uint32_t,void*,int){return hasCommander ? corps : nullptr;}
static uint8_t* __fastcall getPerson(uint32_t,void*,int id){return hasCommander && id==1 ? person : nullptr;}
static int __fastcall corpsOf(void*,void*){return 1;}
static uint8_t __fastcall ability(void* p,void*,int i){return ((uint8_t*)p)[0x80+i];}
static int __fastcall forceOf(void* p,void*){return *(int*)((uint8_t*)p+0x60);}
static int __fastcall troopsOf(void* p,void*){return *(int*)((uint8_t*)p+0x64);}
static const int16_t* __fastcall posOf(void* p,void*){return (int16_t*)((uint8_t*)p+0x3c);}
bool pathfindReady(){return ready;}
bool movementReady(){return ready;}
bool pathfindRoute(void*,uint32_t start,const std::vector<uint32_t>& goals,std::vector<uint32_t>* path,int* cost){
    ++routeCalls;check(!goals.empty() && goals.size()<=64,"native route request stays within adapter bounds");
    if(!reachable)return false;*path={start,goals[0]};*cost=1;return true;
}
MovementRouteTime movementRouteTime(const uint8_t*,const std::vector<uint32_t>&,const uint8_t*,int,int){MovementRouteTime r;r.turns=eta;return r;}
static void* vt[21]={};
static void init(uint8_t* p,int force,int x,int y,int troops){
    memset(p,0,0xf4);*(void***)p=vt;p[0x70]=1;*(int*)(p+0x60)=force;*(int*)(p+0x64)=troops;
    *(uint32_t*)(p+0x3c)=pack(x,y);p[0xcc]=20;
}
static uint32_t savedFlags,afterFlags,hookEbp,hookSlot,hookEbx,hookEsi,hookEdi,hookEax,hookEdx,hookEcx,hookArg;
__declspec(naked) static void hookReturn(){__asm{
    pushfd
    pop afterFlags
    mov hookEbp,ebp
    mov hookEbx,ebx
    mov hookEsi,esi
    mov hookEdi,edi
    mov hookEax,eax
    mov hookEdx,edx
    mov hookEcx,ecx
    mov eax,[esp+4]
    mov hookArg,eax
    add esp,8
    mov eax,[esp+28h]
    mov hookSlot,eax
    add esp,0e8h
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
}}
__declspec(naked) static void invokeHook(void*){__asm{
    push ebp
    push ebx
    push esi
    push edi
    sub esp,0e8h
    mov eax,[esp+0fch]
    mov [esp+0e0h],eax
    mov ebp,17000
    mov ebx,11223344h
    mov esi,22334455h
    mov edi,33445566h
    mov eax,44556677h
    mov edx,55667788h
    stc
    pushfd
    pop savedFlags
    jmp reserveHook
}}
int main(){
    auto world=VirtualAlloc((void*)0x7200000,0x20000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(world!=(void*)0x7200000)return 2;
    validFn=isValid;baseRec=(GetFn)getBase;baseId=(decltype(baseId))idOf;hostileFn=(decltype(hostileFn))enemyOf;
    corpsRec=(GetFn)getCorps;personRec=(GetFn)getPerson;abilityFn=(decltype(abilityFn))ability;
    vt[0x3c/4]=(void*)posOf;vt[0x40/4]=(void*)forceOf;vt[0x50/4]=(void*)troopsOf;
    vt[0x44/4]=(void*)corpsOf;
    int* w=(int*)WORLD;w[2]=200;w[3]=1;w[4]=1;
    Bases b;b.C=b.N=4;uint8_t matrix[16];memset(matrix,255,sizeof matrix);b.MAT87B=matrix;
    Units u;u.U=10;u.UNITARR=units[0];uint8_t hex[20]={};
    g_setup={L"",L"",&b,&u,hex,100,100,3,true};g_on=true;g_cities.resize(4);
    for(int i=0;i<4;++i)init(cities[i],i==3?2:1,30+i*10,30,40000);
    war[2]=true;
    for(int t=0;t<=3;++t){w[0x5c/4]=t;calculate(cities[0],17000);}
    check(g_cities[0].history.held==5000,"real collector releases safe rear reserve after cooldown");
    matrix[12]=1;check(calculate(cities[0],17000)==18000,"new hostile base immediately raises reserve");
    matrix[12]=255;init(units[0],2,50,30,24000);
    check(calculate(cities[0],17000)==35000 && routeCalls==1,"reachable approaching field army raises reserve by strength");
    calculate(cities[0],17000);check(routeCalls==1,"unchanged same-turn snapshot reuses route result");
    *(int*)(units[0]+0x64)=32000;
    check(calculate(cities[0],17000)==45000 && routeCalls==2,"same-turn reinforcement invalidates cache");
    garrisonReset();war[2]=false;calculate(cities[0],17000);
    check(g_cities[0].enemies==0 && g_cities[0].hops==255,"allied or ceasefire force contributes no threat");
    war[2]=true;reachable=false;garrisonReset();calculate(cities[0],17000);
    check(g_cities[0].enemies==0,"unreachable army across terrain is not counted as approaching");
    reachable=true;eta=4;garrisonReset();calculate(cities[0],17000);
    check(g_cities[0].enemies==0,"route requiring more than three turns is not immediate pressure");
    eta=-1;garrisonReset();calculate(cities[0],17000);
    check(g_cities[0].unknown==1 && g_cities[0].history.held==53000,"unknown movement estimate cannot expose the city");
    eta=1;*(int*)(units[0]+8)=1;garrisonReset();calculate(cities[0],17000);
    check(g_cities[0].enemies==0,"transport and trade units do not become invading armies");
    for(int i=0;i<10;++i)init(units[i],2,50+i,30,1000);
    garrisonReset();calculate(cities[0],17000);
    check(g_cities[0].paths<=8 && g_cities[0].unknown>=2 && g_cities[0].enemies==10000,"route budget preserves untested threats conservatively");
    garrisonReset();memset(units,0,sizeof units);g_back=(uint32_t)(uintptr_t)hookReturn;invokeHook(cities[0]);
    check(hookEbp==18000 && hookSlot==18000 && hookArg==10 && hookEcx==0x9a916c4,"compiled native hook updates register, stack reserve and food-call arguments");
    check(hookEbx==0x11223344 && hookEsi==0x22334455 && hookEdi==0x33445566 && hookEax==0x44556677 && hookEdx==0x55667788 && savedFlags==afterFlags,"compiled native hook preserves all other registers and flags");
    hasCommander=true;init(corps,1,0,0,0);*(int*)(corps+4)=1;*(int*)(corps+0xc)=1;
    init(person,1,0,0,0);for(int i=0;i<5;++i)person[0x80+i]=90;*(int*)(person+0xfc)=3;
    garrisonReset();matrix[12]=1;invokeHook(cities[0]);
    check(g_cities[0].commander==1 && g_cities[0].profile.personality==3 && g_cities[0].target==15000 && hookArg==8,
          "real adapter reads corps commander personality and abilities, supplies native food horizon");
    *(int*)(person+0xfc)=0;person[0x83]=20;calculate(cities[0],17000);
    check(g_cities[0].target>18000 && g_foodPeriods==12,"commander change raises cautious reserve and supply buffer immediately");
    g_on=false;check(calculate(cities[0],12345)==12345,"disabled feature preserves native reserve");
    printf("garrison_runtime_test: %d failures\n",failures);return failures?1:0;
}
