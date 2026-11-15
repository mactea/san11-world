#include "../src/garrison_model.h"
#include <stdio.h>
static int failures=0;
static void check(bool ok,const char* text) {printf("%s %s\n",ok?"PASS":"FAIL",text);failures+=!ok;}
int main() {
    using namespace garrison;
    check(reserve(255,0,false)==5000 && reserve(3,0,false)==5000,"safe and disconnected rear keep 5000");
    check(reserve(2,0,false)==10000,"one intervening base keeps buffer reserve");
    check(reserve(1,0,false)==18000,"direct enemy connection keeps frontline reserve");
    check(reserve(255,20000,false)==30000,"field army raises reserve even without an enemy city nearby");
    check(reserve(1,1000,false)==18000,"small raiders cannot lower a frontline reserve");
    check(reserve(255,0,true)==18000,"incomplete routing never classifies a city as safe rear");
    check(reserve(1,1000000000LL,true)==200000,"large attacks saturate without integer overflow");
    History h;
    check(h.update(1,100,5000)==18000,"load starts conservatively");
    for(int i=0;i<10;++i)h.update(1,100,5000);
    check(h.held==18000,"repeated evaluations in one turn do not consume cooldown");
    check(h.update(1,101,5000)==18000 && h.update(1,102,5000)==18000 && h.update(1,103,5000)==5000,"three continuously observed safe turns release rear reserve");
    check(h.update(1,103,30000)==30000,"new army raises reserve in the same turn");
    check(h.update(1,104,10000)==30000 && h.update(1,105,18000)==30000,"changed threat resets pending reduction");
    check(h.update(1,108,10000)==30000,"unobserved turns cannot prove safety");
    h.update(1,109,10000);h.update(1,110,10000);
    check(h.update(1,111,10000)==10000,"buffer settles after three safe turns");
    check(h.update(2,111,5000)==18000,"ownership changes discard the old city's cooldown");
    check(h.update(2,50,5000)==18000,"date rewind resets cooldown");
    h=History();check(h.update(2,50,5000)==18000,"same-date reload cannot reuse prior safe state");
    Commander timid={0,60,60,60,50},bold={2,60,60,60,50},rash={3,60,60,60,50},calm;
    check(reserve(1,0,false,timid)>reserve(1,0,false,calm) && reserve(1,0,false,calm)>reserve(1,0,false,bold) &&
          reserve(1,0,false,bold)>reserve(1,0,false,rash),"personality changes reserve preference in expected order");
    Commander skilled={3,100,100,100,100},weak={3,20,20,20,20};
    check(reserve(1,0,false,skilled)==15000 && reserve(1,0,false,weak)>15000,"military ability improves efficiency without removing the frontline floor");
    check(reserve(255,40000,false,skilled)>=51000,"even reckless talented commanders respect an invading army");
    check(foodPeriods(skilled)==8 && foodPeriods(weak)==11 && foodPeriods(timid)==11,"politics changes supply reserve, personality adds caution");
    check(safeTurns(weak)==4 && safeTurns(timid)==4 && safeTurns(skilled)==3,"low intelligence and cautious personality extend safety observation");
    check(attackWeight(skilled,5)>attackWeight(calm,1) && attackWeight(weak,1)<100,"enemy military skill and siege weapons increase effective threat");
    for(int personality=0;personality<4;++personality)for(int ability=0;ability<=100;ability+=10){
        Commander c={personality,ability,ability,ability,ability};
        if(reserve(1,0,false,c)<15000 || reserve(2,0,false,c)<8000 || reserve(255,0,false,c)<3000 || safeTurns(c)<3)++failures;
    }
    check(failures==0,"all personality and ability combinations retain safety floors");
    printf("garrison_test: %d failures\n",failures);return failures?1:0;
}
