#pragma once
#include <algorithm>
#include <stdint.h>

namespace garrison {
struct Commander {
    int personality=1,leadership=60,intelligence=60,martial=60,politics=50;
};
inline int stat(int n) {return (std::max)(0,(std::min)(100,n));}
inline int character(const Commander& c) {return c.personality>=0 && c.personality<=3 ? c.personality : 1;}
inline int military(const Commander& c) {return (3*stat(c.leadership)+2*stat(c.intelligence)+stat(c.martial))/6;}
inline int foodPeriods(const Commander& c) {
    return (std::max)(8,12-stat(c.politics)/20)+(character(c)==0 ? 1 : 0);
}
inline int safeTurns(const Commander& c) {return (character(c)==0 ? 4 : 3)+(stat(c.intelligence)<40 ? 1 : 0);}
inline int attackWeight(const Commander& c,int weapon) {
    int weight=100+(military(c)-60)/2;
    if(weapon>=5 && weapon<=8)weight+=20; // siege weapons pose extra risk to cities
    return (std::max)(80,(std::min)(140,weight));
}
// Distances are directed native base-graph hops; 255 means unreachable.
inline int reserve(int enemyHops, int64_t approachingTroops, bool uncertain,const Commander& c={}) {
    int floor = enemyHops <= 1 ? 18000 : enemyHops == 2 ? 10000 : 5000;
    const int caution[]={115,100,90,85};
    int factor=(std::max)(80,(std::min)(130,caution[character(c)]+(60-military(c))/4));
    floor=floor*factor/100;
    floor=(std::max)(floor,enemyHops<=1 ? 15000 : enemyHops==2 ? 8000 : 3000);
    if (uncertain) floor = (std::max)(floor, 18000);
    const int risk[]={20,0,-5,-10};
    int margin=(std::max)(115,(std::min)(165,125+risk[character(c)]+(60-military(c))/5));
    if(uncertain)margin=(std::max)(150,margin);
    if (approachingTroops > 0)
        floor = (int)(std::max<int64_t>)(floor, (std::min<int64_t>)(200000, 5000 + (approachingTroops * margin + 99) / 100));
    return floor;
}
struct History {
    int owner=-1, turn=-1, held=0, lowerSince=-1, lowerTarget=-1;
    int update(int force,int now,int wanted,int safe=3) {
        // On load/ownership change, require three observed safe turns before
        // releasing the initial frontier reserve. Reload cannot bypass cooldown.
        if(owner!=force || turn<0 || now<turn) {
            owner=force;turn=now;held=(std::max)(18000,wanted);
            lowerSince=wanted<held ? now : -1;lowerTarget=wanted;
            return held;
        }
        if(wanted>=held) { held=wanted;lowerSince=-1;lowerTarget=wanted; }
        else {
            if(lowerSince<0 || lowerTarget!=wanted || now>turn+1) lowerSince=now;
            lowerTarget=wanted;
            if(now-lowerSince>=(std::max)(3,safe)) {held=wanted;lowerSince=-1;}
        }
        turn=now;return held;
    }
};
}
