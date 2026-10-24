#pragma once
// City shapes (M11_MAP.md): new cities may cover 3 hexes (a triangle) or 1 hex instead of Koei's 7.
#include <stdint.h>
#include "d3dtrace.h"

struct CityShapeSetup {
    const wchar_t* dataDir;      // city_shapes.bin
    int C, N, W, H;              // cities, bases, world columns (hi) and rows (lo)
    const uint8_t* area20;       // u16 area of every hex at its HEX20 byte offset + 4 (bases=1)
    const int32_t* area2base;    // base id of every area (N + 6 entries)
    float scaleMid, scaleSmall;  // model scale of tier 2 (3-hex) and tier 3 (1-hex) cities
    bool write;                  // false (verify_only): load the file and check the code sites, change nothing
};

// Loads <dataDir>\city_shapes.bin; without it nothing is hooked and every city keeps Koei's 7 hexes.
// Otherwise hooks the object footprint (0x41bce0 callers), the object matrix (0x41c2e0) and the
// building-radius walks / rings listed in cityshape.cpp. Returns true when the hooks are in place.
bool cityShapesInstall(const CityShapeSetup& s, TraceLogFn log);
