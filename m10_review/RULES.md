# Review rules: officer (武将) count constants in san11pk.exe (M10)

Context: Romance of the Three Kingdoms XI PK (san11pk.exe, 32-bit x86 MSVC, raw == VA - 0x400000). The user owns
the game and is modding it (worldmod: a d3d9.dll proxy that patches the exe in memory). Earlier milestones grew the
cities (M7, BUILDINGS.md / M7_LAYOUT.md), forces (M8_FORCES.md) and units (M9_UNITS.md); read M9_UNITS.md first, it is
the closest precedent. Now the officer table grows from 1100 to P records.

## The officer table today
* World object 0x7201958 (static singleton). Officers: world+0xC0BC = 0x720DA14, 1100 records of 0x190 bytes, ending at
  0x72790D4 where the treasure (宝物) array starts. Getter 0x490b00 (`cmp eax,0x44b; imul eax,0x190; lea [eax+ecx+0xc0bc]`,
  1053 callers), id check 0x412390 (0..1099), object-limit table 0x490d30 (by object type 0..29).
* Ids: 0..849 scenario officers (inside: 670..699 Koei's empty "史實01~30" slots, 700..799 event/barbarian leaders,
  800..849 "古武将"), 850..999 user-created new officers (新武将, 150; edit data 150 x 0x247 at world+0x1A5050),
  1000..1099 special/temporary officers (敵将 etc.).
* Files: scenario files store officers 0..849 (850 x 152 bytes); saves store all 1100 (1100 x 203 bytes).
* Officer ids inside records are 4-byte ints in memory (father, mother, spouse, sworn brother, liked/disliked x5, ...).

## The new id layout (expressions use S and P)
S = number of scenario officers (850 today), P = S + 250 = total officers (1100 today).
* scenario officers 0..S-1 (Koei's 0..849 keep their ids; the new extended officers are 850..S-1),
* new officers (新武将) S..S+149 (still 150),
* special officers S+150..S+249 = P-100..P-1,
* total P.
So: 850 -> S, 849 -> S-1, 999 -> S+149 (= P-101), 1000 -> S+150 (= P-100), 1099 -> P-1, 1100 -> P, 150 stays 150.
Koei's sub-ranges inside the scenario block (670..699, 700..799, 800..849) keep their numbers: KEEP.

## For EVERY site in your batch decide
- verdict:
  * PERSON  - the value is an officer count, officer id bound, officer-range boundary, or a byte size of an array
    indexed by officer id, and must change;
  * KEEP    - officer-related but must NOT change (a field offset inside the officer record, a sub-range inside
    0..849 such as 700/800, the 150 new-officer count, a face/portrait id, a biography/列传 id, ...);
  * NOT     - unrelated (units 1000/999, item ids, money/food/troop values, percent/permille, time, UI coordinates,
    struct offsets of other objects, string/resource ids, CRT/library code, other tables' counts);
  * UNSURE.
- role (for PERSON): PERSON_COUNT (1100 -> P), PERSON_MAX (1099 -> P-1), SCEN_COUNT (850 -> S, also "first new officer"),
  SCEN_MAX (849 -> S-1), NEW_MAX (999 -> S+149), SPECIAL_FIRST (1000 -> S+150), ARRAY_SIZE (byte size of an
  officer-indexed array: give the formula, e.g. "P*4"), LOOP_PTR (a loop over the records by pointer, e.g. end
  address), OTHER (explain).
- patch: the new value as an expression in S and P ("P", "P-1", "S", "S-1", "S+149", "S+150", "P*4", ...); "" for KEEP/NOT.
- encoding: "imm8" or "imm32" (instruction bytes: 83 /x ib, 6a ib, 6b /r ib are imm8; 3d/81/68/69/b8+r are imm32);
  "disp32"/"disp8" when the value is a memory displacement.
- note: one short sentence of evidence: what the compared/stored register holds (an officer id passed to the getter
  0x490b00 or the check 0x412390, a loop counter that indexes officers, an officer pointer, a unit id, money, ...).

Key traps:
* 1000/999 are ALSO the unit count/max (M9 reviewed those as UNIT): a unit id (getter 0x490e70, unit array 0x736b088,
  locations N..N+999) is NOT. The field `m9_note` in your batch is what the M9 reviewer wrote about the same site.
* 150 is also used for many unrelated things (UI widths, percentages). PERSON only when it bounds the new officers in
  a way that must move (normally the count 150 itself stays: KEEP; a bound like `id < 1000` for "not special" moves).
* "Usable officers" loops `for id in 0..999` (they skip the special 1000..1099): bound 1000 -> SPECIAL_FIRST (S+150).
* Loops over all 1100 officers: PERSON_COUNT. Checks `id <= 1099` / `id < 1100`: PERSON_MAX / PERSON_COUNT.
* Checks `id >= 850` (new or special) / `id < 850` (scenario officer): SCEN_COUNT. `id <= 849`: SCEN_MAX.
* 0x190 multiplications and the pointer -> id division (magic 0x51eb851f, then sar 7 = /0x190 after a subtraction of
  the array base) stay: KEEP (the array moves, the record size does not). The same magic with other shifts is /100: NOT.
* Displacements 0xC0BC (world+0xC0BC) and absolute addresses in 0x720DA14..0x72790D4: verdict PERSON, role OTHER,
  patch "PERSONARR-0x7201958" (displacement from the world) or "PERSONARR+<offset>" (absolute), the array moves.

Tools (run from the scratchpad m10 directory, shown in your prompt):
`python show.py <hexaddr> [before] [after]` prints the disassembly around an address; `python callers.py <hexaddr>`
lists call sites of a function. pk.asm is the full disassembly (grep it). Be skeptical; read enough context to know
what the register holds (trace back to a getter call, a loop counter, a struct field).

## Output
Write JSON Lines to m10_review/result_<batch>.jsonl in the repository C:\Users\garib\Documents\Work\CMCode\san11world-persons,
one object per site:
{"addr":"0x...","value":"...","verdict":"PERSON|KEEP|NOT|UNSURE","role":"...","patch":"...","encoding":"...","note":"..."}
Every site of your batch exactly once. Do not modify any other files; do not run the game.
Reply with a short summary: counts per verdict and role, UNSURE addresses, and surprising findings (officer-indexed
arrays inside other objects, bitsets over officers, lists with a fixed number of officer ids, file formats).
