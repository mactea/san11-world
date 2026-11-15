#pragma once
#include <stdint.h>

namespace personlayout {
constexpr int NEW_COUNT=150, LEGACY_SPECIALS=100, EXPANDED_SPECIALS=300;
inline bool validSpecials(int n) { return n==LEGACY_SPECIALS || n==EXPANDED_SPECIALS; }
inline int total(int scenario,int specials) { return scenario+NEW_COUNT+specials; }
// P is the existing 100-slot format; Q appends 200 slots without moving any ID.
inline int streamSpecials(uint32_t mark) { return ((mark>>8)&255)=='Q'?EXPANDED_SPECIALS:LEGACY_SPECIALS; }
inline bool marked(uint32_t mark) { return ((mark>>8)&255)=='P' || ((mark>>8)&255)=='Q'; }
inline int streamScenario(uint32_t mark) { return marked(mark)?int(mark>>16):850; }
inline uint32_t mark(uint32_t value,int scenario,int specials) {
    if(scenario==850 && specials==LEGACY_SPECIALS)return value;
    return value | uint32_t(specials==EXPANDED_SPECIALS?'Q':'P')<<8 | uint32_t(scenario)<<16;
}
}
