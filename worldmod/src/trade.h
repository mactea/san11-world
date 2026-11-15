#pragma once
// M13 trade (trade.cpp, M13_TRADE.md): caravans as real transport units, a price index per city and good,
// transit tax / robbery / interception, AI caravans, a 'TRAD' block in saves, automation "trade ..." commands.
#include <stdint.h>
#include <stdio.h>
#include "d3dtrace.h"

struct TradeSetup {
    const wchar_t* dir;          // game directory (worldmod.ini)
    const wchar_t* section;      // ini section
    const wchar_t* dataDir;      // world data directory (trade.bin)
    int C, R, U;                 // cities, regular forces, units
    int W, H;                    // map width (hi) / height (lo) in hexes
    uint8_t* hex20;              // HEX20 (20 bytes a hex, index lo * W + hi)
    bool write;                  // false = verify_only: check the code, install nothing
};

// Reads trade / trade_* from the ini. With trade=1 and a valid trade.bin installs the hooks (end of a turn 0x58051e,
// AI unit planning 0x5def90, unit annihilation 0x4bfa30, the main loop's command pump 0x57ff80). trade=0 (default):
// nothing. Returns true when the hooks are in place.
bool tradeInstall(const TradeSetup& s, TraceLogFn log);

// 0x493BCE (world serializer, after every object): resets the trade state when a scenario or a save is read and
// reads / writes the TRAD block (opaque preservation when disabled). False aborts serialization.
bool tradeStream(uint8_t* stream);

// automation command line "trade ..." (command thread): dump / send / ai / tick
void tradeCommand(const char* line, FILE* out);
