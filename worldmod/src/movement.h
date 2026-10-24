#pragma once
// Per-weapon movement and terrain costs (movement.cpp, M12_MOVE.md).
#include <stdint.h>
#include "d3dtrace.h"

// Reads move / move_w0..move_w11 from [section] of <dir>\worldmod.ini. With move=1 hooks the map entry (0x57ecb7,
// before 0x57e910 fills the cost table 0x9549830 from the weapon records): the weapon records get the configured
// absolute values and every unit's stats are rebuilt (0x496dd0). write = false (verify_only): only checks the code.
// Returns true when the hook is in place.
bool movementInstall(const wchar_t* dir, const wchar_t* section, bool write, TraceLogFn log);

// Weapon specs as the ini gives them: "<movement or ->[;<terrain>:<cost>,...]". Parses one into
// move (-1 = keep) and cost[32] (-1 = keep); false on a syntax error or a value out of range.
bool movementParseSpec(const char* spec, int* move, int cost[32]);
// Built-in default spec of weapon w (0..11).
const char* movementDefaultSpec(int w);
// Applies the specs to 12 weapon records (0xa0 bytes each, movement +0x57, costs +0x58); returns changed bytes.
int movementApplyRecords(uint8_t* records, const int move[12], const int cost[12][32]);
