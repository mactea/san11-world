#pragma once
// More officers (武将) than Koei's 1100 (persons.cpp, M10_PERSONS.md).
#include <stdint.h>
#include "d3dtrace.h"

struct Persons {
    int PS = 850;                    // scenario officers 0..PS-1; new officers PS..PS+149; special PS+150..PT-1
    int PT = 1100;                   // all officer records (PS + 250)
    int VF = 2000;                   // first virtual father id (VF..VF+60, see m10_sites.py)
    uint8_t* PERSONARR = nullptr;    // PT x 0x190 (world+0xC0BC in Koei's layout)
    uint8_t* PPOOL = nullptr;        // generic list node pool: 0x2000-byte bitmap + 0x10000 x 0x7c (0x6ee7950)
    int* PSTREAM = nullptr;          // scenario officers in the stream being read / written (850 for Koei's files)
};

// thiscall entry hook of the officer snapshot methods (vt 0x862d28): this -> per-instance buffer sized for PT
extern "C" uint8_t* __cdecl personSnapShadow(uint8_t* self);

// dataDir: persons_bio.bin (biographies of the extended officers, read on first use)
bool personsSetup(int PS, const wchar_t* dataDir, Persons& p, TraceLogFn log);

// cdecl: biography text (game encoding, NUL-terminated) of extended officer id (850..PS-1), or nullptr
extern "C" const char* __cdecl personBio(int id);

// officer record of id (nullptr outside 0..PT-1 or before personsSetup)
uint8_t* personPtr(int id);

// cdecl, after a save stream has been loaded (m10_orders.py): the officers with an order in progress (+0x13c 0..43)
// form the list at world+0x184, which saves keep only 1000 of; append the valid ones the list does not hold
extern "C" void __cdecl personsRebuildOrders(uint8_t* list);
