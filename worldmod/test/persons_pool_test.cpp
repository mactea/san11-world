#include <windows.h>
#include <stdio.h>
#include <initializer_list>
#include "../src/persons.h"
static int failures=0,resets=0;
static void check(bool ok,const char* name) {printf("%s %s\n",ok?"PASS":"FAIL",name);failures+=!ok;}
static void __fastcall reset(void* p,void*,int arg) {if(arg!=0)++failures;*(int*)((char*)p+0xa0)=8;++resets;}
int main() {
    using namespace personlayout;
    const int ps=14163;
    check(total(ps,100)==14413 && total(ps,300)==14613,"append 200 slots without shifting existing IDs");
    for(int count:{100,300})for(int scenario:{850,ps})for(uint32_t kind:{0u,1u}) {
        auto h=mark(kind,scenario,count);
        check(streamScenario(h)==scenario && streamSpecials(h)==count && (h&255)==kind,"header round trip preserves scenario count, special capacity and native kind");
    }
    Persons p;
    check(!personsSetup(ps,200,L"",p,nullptr),"unsupported pool sizes fail before allocation");
    if(!personsSetup(ps,300,L"",p,nullptr))return 2;
    check(p.PT==14613 && p.specials==300 && *p.PTSTREAM==p.PT,"runtime allocates expanded pool and serializer bound");
    check(personPtr(ps+150) && personPtr(p.PT-1) && !personPtr(p.PT),"old and new special IDs have bounded native storage");
    void* vt[9]={};vt[8]=(void*)reset;
    for(int i=0;i<p.PT;++i) {auto a=personPtr(i);*(void***)a=vt;*(int*)(a+0xa0)=3;}
    personsResetStreamTail(ps+250);
    check(resets==200 && *(int*)(personPtr(ps+249)+0xa0)==3 && *(int*)(personPtr(p.PT-1)+0xa0)==8,
          "loading a legacy save resets exactly the appended slots and preserves existing officers");
    *(int*)(personPtr(p.PT-1)+0xa0)=3;
    personsResetStreamTail(ps+250);
    check(resets==400 && *(int*)(personPtr(p.PT-1)+0xa0)==8,"new-save then old-save reload cannot retain ghost merchants");
    personsResetStreamTail(p.PT);personsResetStreamTail(-1);personsResetStreamTail(p.PT+1);
    check(resets==400,"full-length and invalid reset bounds do not touch records");
    printf("persons_pool_test: %d failures\n",failures);return failures?1:0;
}
