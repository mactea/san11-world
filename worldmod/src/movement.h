#pragma once
// Per-weapon movement and terrain costs (movement.cpp, M12_MOVE.md).
#include <stdint.h>
#include <vector>
#include "d3dtrace.h"

// Reads move / move_w0..move_w11 / move_steer_w9..w11 / move_transport_water / move_cap_ship / move_shore_scale from [section] of
// <dir>\worldmod.ini. With move=1 hooks the map entry (0x57ecb7, before 0x57e910 fills the cost table 0x9549830 from
// the weapon records): the weapon records get the configured absolute values and every unit's stats are rebuilt
// (0x496dd0). Also: 操舵 ships use their own 河 / 海 costs instead of +4 movement (0x56dd42, 0x496b81), a transport's
// base movement on water (0x496a76), landing / boarding cost scaling (0x496dd0 entry, move_shore_scale), and the range search's depth cap for ships with a bigger node pool / visited
// list (0x5a47e0, 0x5a4540..0x5a497a). write = false (verify_only): only checks the code.
// Returns true when the hooks are in place.
bool movementInstall(const wchar_t* dir, const wchar_t* section, bool write, TraceLogFn log);

// Weapon specs as the ini gives them: "<movement or ->[;<terrain>:<cost>,...]". Parses one into
// move (-1 = keep, else 1..moveMax) and cost[32] (-1 = keep); false on a syntax error or a value out of range.
bool movementParseSpec(const char* spec, int* move, int cost[32], int moveMax);
// Built-in default spec of weapon w (0..11).
const char* movementDefaultSpec(int w);
// Applies the specs to 12 weapon records (0xa0 bytes each, movement +0x57, costs +0x58); returns changed bytes.
int movementApplyRecords(uint8_t* records, const int move[12], const int cost[12][32]);
// The 操舵 step costs (12 x 32, laid out like 0x9549830) the step cost 0x56dd00 uses on 河 / 海 for a unit with 操舵;
// nullptr before the first map entry or with move=0.
const uint8_t* movementSteerCosts();
struct SkillSteerCosts;
bool movementDescribeSteering(const uint8_t* unit,int terrain,SkillSteerCosts& out);
// M13 trade (monsoon): called for every step onto a sea hex (terrain 8) with the unit, the HEX20 record stepped onto,
// the packed hex the step comes from (lo | hi << 16; 0xffffffff = unknown, see movement.cpp; bit 31 set: a backwards
// search, the unit really moves the other way) and the cost so far
// (move=1 only); returns the cost to use (1..254). minPct: the smallest percent of the cost it can return (A*'s
// heuristic, pathfind.cpp). nullptr (default): no change.
typedef int (__cdecl* MovementSeaCostFn)(const uint8_t* unit, const uint8_t* hexRec, uint32_t from, int cost);
void movementSetSeaCost(MovementSeaCostFn fn, int minPct);
// Trade pause constraints must affect range display, path finding and actual execution equally.
typedef bool (__cdecl* MovementAllowedFn)(const uint8_t* unit);
void movementSetAllowed(MovementAllowedFn fn);
bool movementReady();
// Applied to native movement, range search, A* and route-time quotes alike.
typedef bool (__cdecl* MovementStepAllowedFn)(const uint8_t* unit,const uint8_t* hexRec);
void movementSetStepAllowed(MovementStepAllowedFn fn);
bool movementCanStep(const uint8_t* unit,const uint8_t* hexRec);
// Only registered trade caravans receive the road/plain/sea discounts and extended range.
typedef bool (__cdecl* MovementTradeFn)(const uint8_t* unit);
void movementSetTradeUnits(MovementTradeFn fn);
bool movementIsTradeUnit(const uint8_t* unit);
struct MovementRouteTime { int turns = -1, steps = 0, land = 0, water = 0, cost = 0; };
// Uses the same step costs and per-turn range limits as native movement. Current season,
// no future blockers or rests; path must start at the unit's current position.
MovementRouteTime movementRouteTime(const uint8_t* unit, const std::vector<uint32_t>& path,
                                   const uint8_t* hex20, int width, int height);
int movementSeaMinPct();
// pathfind.cpp: the hex its next 0x56dd00 calls step from (0xffffffff: none; bit 31: backwards search); per thread
void movementSetStepFrom(uint32_t packed);
// how the step direction was found (counts of sea-hex steps)
struct MovementStepStats { uint32_t astar, range, pathSum, unknown; };
const MovementStepStats* movementStepStats();
