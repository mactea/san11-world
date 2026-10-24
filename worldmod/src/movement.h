#pragma once
// Per-weapon movement and terrain costs (movement.cpp, M12_MOVE.md).
#include <stdint.h>
#include "d3dtrace.h"

// Reads move / move_w0..move_w11 / move_steer_w9..w11 / move_transport_water / move_cap_ship from [section] of
// <dir>\worldmod.ini. With move=1 hooks the map entry (0x57ecb7, before 0x57e910 fills the cost table 0x9549830 from
// the weapon records): the weapon records get the configured absolute values and every unit's stats are rebuilt
// (0x496dd0). Also: 操舵 ships use their own 河 / 海 costs instead of +4 movement (0x56dd42, 0x496b81), a transport's
// base movement on water (0x496a76), and the range search's depth cap for ships with a bigger node pool / visited
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
