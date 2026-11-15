#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "../src/bases.h"
#include "../src/forces.h"
#include "../src/persons.h"
#include "../src/skill_telemetry.cpp"
static Persons* g_persons=nullptr;
static Forces* g_forces=nullptr;
static Bases* g_bases=nullptr;
#include "../src/skill_actor_telemetry.inc"

static void name(uint8_t* person,const wchar_t* first,const wchar_t* last){
    memset(person+4,0,10);
    assert(WideCharToMultiByte(950,0,first,-1,(char*)person+4,5,nullptr,nullptr));
    assert(WideCharToMultiByte(950,0,last,-1,(char*)person+9,5,nullptr,nullptr));
}
int main(){
    void* world=VirtualAlloc((void*)0x7200000,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(world==(void*)0x7200000);auto date=(int*)0x7201958;
    Persons people;people.PT=14613;std::vector<uint8_t> records(people.PT*0x190);
    people.PERSONARR=records.data();g_persons=&people;
    Forces forces;forces.F=254;std::vector<uint8_t> corps(forces.F*0x50),owners(forces.F*0x12c);
    forces.CORPSARR=corps.data();forces.FORCEARR=owners.data();g_forces=&forces;
    Bases bases;bases.C=530;bases.N=575;g_bases=&bases;
    auto setup=[&](int id,int location){
        auto p=records.data()+id*0x190;
        name(p,L"夏",L"育");name(records.data()+635*0x190,L"劉",L"備");
        *(int*)(p+0x94)=forces.F-1;*(int*)(p+0x98)=location;*(int*)(p+0x9c)=23;
        *(int*)(corps.data()+(forces.F-1)*0x50+4)=forces.F-2;
        *(int*)(owners.data()+(forces.F-2)*0x12c+4)=635;
        date[2]=194;date[3]=6;date[4]=1;date[0x5c/4]=1;
        return p;
    };
    auto emit=[&](const void* person,const char* context){
        skilltelemetry::Target target;target.unit=17;target.context=context;
        skilltelemetry::applied(person,81,81,"metadata_test","actor_readback","gold",10,11,0,target);
    };
    assert(skilltelemetry::open(L"build/actor_metadata_test.csv",telemetryActor,nullptr));int inputs=0;
    for(int layout=0;layout<2;++layout){
        people.PT=layout?14613:1100;forces.F=layout?254:47;bases.C=layout?530:42;bases.N=bases.C+45;
        for(int id:layout?std::vector<int>{0,635,850,14162,14612}:std::vector<int>{0,635,1099}){
            for(int location:{0,bases.C-1,bases.C,bases.N-1,bases.N+17,-1,5000}){
                auto person=setup(id,location);skilltelemetry::Actor actor;
                auto beforePeople=records,beforeCorps=corps,beforeOwners=owners;
                assert(telemetryActor(person,actor));
                assert(actor.person==id && actor.force==forces.F-2 && actor.ruler==635);
                assert(actor.city==(location>=0 && location<bases.C?location:-1));
                assert(actor.year==194 && actor.month==6 && actor.day==11);
                char context[160];sprintf_s(context,"{\"layout\":%d,\"location\":%d,\"home_base\":23,\"city_limit\":%d}",layout,location,bases.C);
                emit(person,context);
                assert(records==beforePeople && corps==beforeCorps && owners==beforeOwners);++inputs;
            }
        }
    }
    skilltelemetry::close();
    assert(skilltelemetry::open(L"build/actor_metadata_edges_test.csv",telemetryActor,nullptr));
    for(int fault=0;fault<20;++fault){
        auto person=setup(14162,12);const void* argument=person;
        if(fault==1)*(int*)(person+0x94)=-1;
        if(fault==2)*(int*)(person+0x94)=forces.F;
        if(fault==3)*(int*)(corps.data()+253*0x50+4)=-1;
        if(fault==4)*(int*)(corps.data()+253*0x50+4)=forces.F;
        if(fault==5)*(int*)(owners.data()+252*0x12c+4)=-1;
        if(fault==6)*(int*)(owners.data()+252*0x12c+4)=people.PT;
        if(fault==7)forces.CORPSARR=nullptr;
        if(fault==8)forces.FORCEARR=nullptr;
        if(fault==9)*(int*)(person+0x98)=bases.N+17;
        if(fault==10)date[2]=INT_MAX;
        if(fault==11)date[0x5c/4]=INT_MAX;
        if(fault==12)date[3]=0;
        if(fault==13)date[4]=0;
        if(fault==14){g_bases=nullptr;*(int*)(person+0x98)=529;}
        if(fault==15)*(int*)(person+0x98)=529;
        if(fault==16)argument=person+1;
        if(fault==17)argument=records.data()+people.PT*0x190;
        if(fault==18)g_persons=nullptr;
        if(fault==19)people.PERSONARR=nullptr;
        skilltelemetry::Actor actor;actor.person=99;strcpy_s(actor.name,"stale");
        bool valid=telemetryActor(argument,actor);assert(valid==(fault<16));
        if(!valid)assert(actor.person==-1 && !actor.name[0] && actor.force==-1 && actor.city==-1);
        else {
            assert(actor.person==14162);
            assert(actor.force==((fault>=1 && fault<=4) || fault==7 || fault==8?-1:252));
            assert(actor.ruler==((fault>=1 && fault<=8)?-1:635));
            assert(actor.city==(fault==9 || fault==14?-1:fault==15?529:12));
            assert(actor.year==(fault>=10 && fault<=13?0:194));
        }
        auto beforePeople=records,beforeCorps=corps,beforeOwners=owners;
        char context[64];sprintf_s(context,"{\"metadata_case\":%d}",fault);emit(argument,context);
        assert(records==beforePeople && corps==beforeCorps && owners==beforeOwners);
        forces.CORPSARR=corps.data();forces.FORCEARR=owners.data();g_bases=&bases;g_persons=&people;people.PERSONARR=records.data();
    }
    skilltelemetry::close();
    assert(inputs==56);
    puts("PASS production actor resolver:56 stock/expanded city/unit/gate/port locations, Chinese names, high IDs and20 invalid/missing/date boundaries; source world unchanged");
    VirtualFree(world,0,MEM_RELEASE);
}
