#pragma once
#include <algorithm>
#include <stdint.h>

namespace trade {
// Conservative planning allowance: one tenth of escort strength per turn,
// 25% travel contingency and four turns for berthing/trading. Not free supplies.
inline int provisionFood(int troops,int turns,int minimum) {
    if(troops<0 || turns<0)return -1;
    int64_t periods=(int64_t)turns+(turns+3LL)/4+4;
    int64_t food=std::max<int64_t>(minimum,((int64_t)troops+9)/10*periods);
    return food>100000 ? -1 : (int)food; // Native transport food capacity.
}
}
