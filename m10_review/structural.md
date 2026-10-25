# M10 second sweep: structural sites (officer table 1100 -> P)

Static analysis of san11pk.exe (raw == VA - 0x400000), 2026-10-22. Scripts in the session scratchpad (`m10/s2/*.py`).
Symbols as in M10_PERSONS.md: S = scenario officers (850), P = S + 250 (1100); new officers S..S+149, special S+150..P-1.
"Field" rows give the instruction VA and the old value (same convention as `m9_sites.py` FIELD rows).

## 0. Method and coverage

| Sweep | How | Result |
|---|---|---|
| world+0xC0BC+i*0x190+f, i = 1..1100 | every hex operand in pk.asm in [0xC24C, 0x7790C); memory operands kept when the function is a world method (called after `mov ecx, 0x7201958`) or mentions 0x7201958 | 2385 raw hits, 75 in world functions; only the known `lea edi,[ecx+0x6db3c]` (0x491097, 0x4910ea) is officer-array. The i = 1100 hits (`+0x7777c`) are the treasure table start. Everything else is other objects (esi/ebx = local object) |
| absolute 0x720DA14..0x72790D4+0x190 in data | every aligned dword of .rdata / .data (all 0x93c47e0 bytes) / .rsrc | **none** (5 aligned hits in .rdata are byte/u16 pairs, not pointers). Code hits were already in the collector |
| biased bases T - n*stride (n = 670/700/800/850/1000/1100) for the officer table, new-officer edit data (world+0x1a5050, 0x247), new-officer records (world+0x1f9dc4, 0x20), as disp and as absolute | exact match over all operands | **2 new** (§1) |
| negative displacements with a scaled officer index | `[r + idx*s - D]` with D + n*s a plausible offset | the -800 table (§3): 3 sites not in M10 §4.4 |
| sizes n*s and n*s/4 (n = 1100/850/1000/150, s in 1..0x247), bitsets 0x8c/0x23/0x6c/0x1b | rep stos/movs counts, `push`+call, `sub esp`, `__chkstk` (0x707d40) sizes, `lea/add` offsets | no new officer-indexed array (§2) |
| stack frames >= 0x1100 in functions that call the getter 0x490b00 or use 1100/850/1000 | index use `[esp + r*k + X]`, rep counts | only the frames already in §4.2 + the save header list (§2.2) |
| array accesses right after a reviewed officer bound check (`cmp r,0x44b/0x44c/0x352`) | register tracked 25 insns | only known arrays (0x4b2380 object, 0x4b2820 frame, snapshot class, 0x5563f7) |

Limits: arrays reached only through a pointer computed elsewhere (`lea r,[esp+X]` passed to a callee) are not caught by the
index scan; the stack-frame list in §2.3 is the residue checked by hand.

## 1. New field patches (world-relative biased bases)

The two world getters for the new-officer tables take an officer id and subtract 850 by folding it into the displacement:

| Address | Instruction | What | Patch |
|---|---|---|---|
| 0x490eb8 | `lea eax,[eax+ecx+0x12c092]` (getter 0x490ea0, after `imul eax,eax,0x247`) | new-officer edit data world+0x1a5050, index id-850: 0x12c092 = 0x1a5050 - 850*0x247 | `0x1a5050-S*0x247` (disp32, may go negative: fine, wraps) |
| 0x490f15 | `lea eax,[eax+ecx+0x1f3384]` (getter 0x490f00, after `shl eax,5`) | new-officer records world+0x1f9dc4 (150 x 0x20), index id-850: 0x1f3384 = 0x1f9dc4 - 850*0x20 | `0x1f9dc4-S*0x20` |

(The bounds of both getters, 0x490ea4/0x490f04 = 850 and 0x490eab/0x490f0b = 999, are in the review.)
Already known and confirmed: 0x491097, 0x4910ea `lea edi,[ecx+0x6db3c]` -> `PERSONARR-0x7201958+(S+150)*0x190`.

Data constant (harmless, optional): **0x85ef94** dword 0x44c = officer entry (type 9) of the object-type descriptor table
0x85ef18 (30 x {func, count, nprops}, read by 0x675b60). The count is loaded but never used there; set it to `P` for consistency.

## 2. Sizes / stack frames

### 2.1 Hits ruled out (not officer-indexed)
| Address | Hit | Why not |
|---|---|---|
| 0x44961b, 0x44971c, 0x44978f | `mov ecx,0x113; rep movsd` | copy of a 0x470-byte vector element {dword, char[0x44c], ...} in COM/file-enumeration code (0x449890: FindFirst-like vcalls, attribute test 0x10). Path buffer |
| 0x48fd3a, 0x57515f, 0x64b482, 0x64b495 and all `sub esp,0x8c` frames | 0x23 dwords / 0x8c bytes | by-value copies of the 64-char string class (init 0x4719b0(buf,0x3f)), not bitsets. No officer bitset exists |
| `sub esp,0x6c` frames (26) | bitset850 size | ordinary locals, no officer index |
| 0x631000 frame 0x10010, `rep stosd 0x4000` | int[0x4000] | indexed by building id (+0x98 / getter 0x490d00), loop over 1100 officers only counts into it |
| 0x598144 `mov ecx,0x2710` | 10000 dwords | 200x200 map byte grid |
| 0x4f3701, 0x5448de `mov ecx,0x1388`; 0x46dda7 0x3840; 0x73be21 0x960 | 5000/14400/2400 dwords | UI / scenario-setup objects, no officer index |
| allocs 0x44c (0x4dea8a, 0x61dcc9, 0x64537e) | 1100 bytes | text object (ctor 0x437d10), already NOT in the review |
| allocs 0x2150/0x2bb4/0x1284/0x52ac/0x1344/0x265f0/0x201d0 | | ctors 0x61d420/0x4efd60/0x4f5ad0/0x513e60/0x527050/0x51bab0/0x673e50 have no officer count or loop |
| 0x157c offsets (0x619e3c.. 9 sites) | = 1100*0x14/4 | member offset of a dialog object |
| 0x48d88b, 0x48da4e `0x61a80`, 0x567541 `0x445c0` | = 1000*0x190, 700*0x190 | 400000 / 280000 value caps |
| frames 0x6a28e0 0x7d0, 0x5b41db/0x5c9ae5 0x7d0, 0x53bed0 0x22e4, 0x53fdb0 0x267c, 0x59a0e0 0x4024 (`rep stosd 0x1000`), 0x4a42d0 0x3fd4, 0x4b40c0 0x192c, 0x5c84e0 0x2ef4, 0x6844f0 0x57bc, 0x602550 0x120c | | no officer-indexed array: string buffers, small fixed id lists (e.g. 0x53cd00 2-entry list), map grids |

### 2.2 Save header officer list (M10 §4.7, now characterised)
World+0x184 (0x7201adc) is a CList of object pointers (vtbl 0x79bf44, the generic list of §5) owned by the world's base
part (ctor 0x4830b0, vtbl 0x79c134). Its serializer **0x483120** has a frame `sub esp,0xfa4` (0x483120) holding
`int ids[1000]` at esp+0x14:
* save: 0x48331b..0x483365 walks at most 1000 list entries (`cmp ebp,0x3e8` 0x48335f), stores `0x491310(ptr)` (officer id)
  or -1; 0x483367..0x48337c writes exactly 1000 dwords (`mov ebp,0x3e8` 0x48336b).
* load: 0x483380 reads 1000 dwords (0x483080), re-adds ids 0..1099 (`cmp eax,0x44b` 0x483398) via getter + AddTail 0x47c1b0.

The list is iterated by many systems (0x49f470, 0x4a18b5, 0x4c51db, 0x5ce11d, 0x5f443a, 0x5f83da, 0x5fe29b, 0x60220b,
0x60644a, 0x60d4fa; filter on officer +0x13c status). Nothing in the exe bounds it to 1000, so with P > 1000 the save
silently drops entries 1000+. Patch if it can grow past 1000: frame `0xfa4` -> `0xfa4+(L-1000)*4` (and every esp offset
above esp+0x14+4000 in 0x483120..0x48343d: 0xfb4 0x483129, the `add esp,0xfa4` 0x483437) with L = the new capacity
(e.g. P), the three 1000 constants -> L, and the save format (header field + converter) -> L dwords. Needs a runtime check
of the list's real content first (its add site was not found statically: no direct AddTail on 0x7201adc).

### 2.3 Confirmed known frames (M10 §4.2) — nothing to add
0x5c7dc0 0x226c, 0x5c8bb0 0x2274, 0x5c92a0 0x1164 (second chkstk 0x5c97bc; 0x102c chkstk 0x5c92bc has no officer array),
0x590490 0x1740, 0x4b2820 0x398c, 0x4b7e70 0x480, the snapshot frames (§4).

## 3. The id-800 table (古武将 + 新武将, 200 x 0x10) — sites missing from M10 §4.4

All use `lea r,[r + id*0x10 + obj - 0x928]` = obj+0x28d8+(id-800)*0x10 (0x928 = 800*0x10 - 0x28d8).

| Address | Instruction | id source | Verdict |
|---|---|---|---|
| 0x5540b5 | `lea ecx,[ecx+edi-0x928]` | officer vcall +0x28, gated by 0x49d6e0 (800..849 only) | KEEP (古武将 slots 0..49 do not move) |
| 0x556120 | `lea eax,[ecx+edx-0x928]` | `[rec+4]` (record id, unchecked) | needs the 800-table remap (code) |
| 0x556427 | `lea ecx,[ecx+ebp-0x928]` | officer vcall +0x28 (0x5563e7), unchecked | needs remap (code) |
| 0x556467 | `lea eax,[eax+ebx-0x928]` | same ebp | needs remap (code) |
| 0x5563ff / 0x556408 | `add ecx,-0x320` / `lea edx,[ecx+ebx+0x28d8]` | known (0x5563f7) | needs remap (code) |
| 0x54fc3d | `lea eax,[eax+ecx-0x2dd8]` (setup struct +0x428) | known | needs remap (code) |

These cannot be field-patched: the index map is 800..849 -> 0..49, S..S+149 -> 50..199 (a cave with a compare).

## 4. Officer snapshot class (vtbl 0x862d28)

### 4.1 Layout (object size 0x59294)
| Offset | Content | New (shadow) |
|---|---|---|
| +0 | vtbl 0x862d28 (single slot: deleting dtor 0x69a430) | +0 |
| +4 | 1100 x 0xa4 current records | +4, P records |
| +0x2c0b4 | 1100 x 0xa4 backup (= 4+1100*0xa4) | `4+P*0xa4` |
| +0x58164 | 1100 x 4 flags (= 4+2*1100*0xa4) | `4+2*P*0xa4` |
| end 0x59294 | | shadow size `4+P*0x14c` |

### 4.2 Methods
`this`-methods (ecx = snapshot):

| Method | First insn | Role | Touches data |
|---|---|---|---|
| 0x69a410 | `push esi` | ctor: vtbl + init 0x699fb0 | via 0x699fb0 |
| 0x699fb0 | `push ecx` | init 1100 records (loop `mov edi,0x44c` 0x699fc7), flags from +0x58164 (0x699fba), backup copy `rep movsd` ecx=0xb02c (0x69a09e) to +0x2c0b4 (0x69a0a6) | yes |
| 0x699fa0 | `mov dword ptr [ecx],0x862d28` | dtor (vtbl only); free the shadow here | no |
| 0x69a430 | | vtbl deleting dtor (never called directly) | no |
| 0x69a0c0 | `mov eax,dword ptr [esp+4]` | fill record id (current, or backup +0x2c0b4 at 0x69a0eb when arg3) from officer | yes |
| 0x69a280 | `xor eax,eax` | any flag set? (+0x58164 at 0x69a282) | yes |
| 0x69a2b0 | `mov eax,dword ptr [esp+4]` | set flag[id] (0x69a2bf) | yes |
| 0x69a2d0 | `mov edx,dword ptr [esp+4]` | set all flags (0x69a2d6) | yes |
| 0x69a450 | `push ecx` | fill all (loop 0..1099 -> 0x69a0c0) | via 0x69a0c0 (and saves ecx on stack: swap at entry) |
| 0x69a4a0 | `push ecx` | apply record id (current or backup +0x2c0b4 at 0x69a4d0) to the officer, clear flag (0x69a6a9) | yes |
| 0x69abb0 | `push ebx` | apply all, walking `lea ebx,[ebp+4]` / `add ebx,0xa4` | yes |
| 0x69ac30 | `push ebx` | apply all (other filter), same walk | yes |
| 0x69af50 | `push ebp` | save 1100 records to a file (0x470da0 at 0x69b18c, `add ebx,4`) | yes (+ file format) |
| 0x69b210 | `push ebp` | load 1100 records from a file (0x470d60 at 0x69b470, `lea esi,[ebx+4]`), then 0x69ac30 | yes (+ file format) |
| 0x69ad10 | `push -1` | dialog runner (0x543f50, 0x61c350 sites): calls 0x69a450 and passes this as an argument to the dialog 0x6a00f0; 0x69aca0/0x69a7c0/0x69a9b0 get ecx=this but ignore it | no direct access |
| 0x69ae90 | `push -1` | dialog runner (0x622bb0 site): 0x69a450, 0x69abb0, dialog 0x6a00f0 | no direct access |

Static helpers living in the class range (no `this`): 0x69a300, 0x69a6c0 (loops 800..848, KEEP), 0x69a340 (loop 0..1099,
reviewed), 0x69a710 (file exists), 0x69a7c0, 0x69a9b0 (UI), 0x69aca0 (loop 0..849, reviewed SCEN_MAX).

The sub-dialog (ctor 0x69b500, dtor 0x69ba80, methods 0x69b830.. 0x6a2f20) stores the snapshot pointer at dialog+0x19c
(0x6a013f) and **only calls snapshot methods** with it: 0x69a280 (0x69b85f, 0x69ce69), 0x69a2b0 (0x69f07a, 0x6a2a89,
0x6a2c73), 0x69a2d0 (0x6a29b6), 0x69a4a0 (0x69b90c, 0x6a2a3a), 0x69abb0 (0x69b931, 0x6a2a75). No direct dereference of
the snapshot outside the methods was found. (0x6a2880 has no caller other than itself: dead or reached through an
unresolved pointer; it only calls methods too.)

### 4.3 SNAPSHADOW plan (as M9)
Swap `this` at the entry of: 0x699fb0, 0x69a0c0, 0x69a280, 0x69a2b0, 0x69a2d0, 0x69a450, 0x69a4a0, 0x69abb0, 0x69ac30,
0x69af50, 0x69b210 (first instructions in the table above; the swap must accept an already-shadow pointer since 0x69a450,
0x69abb0, 0x69ac30, 0x69b210 call other swapped methods with the swapped pointer). Free in 0x699fa0.

Fields (instruction VA, old value -> expression):

| VA | Instruction | Old | New |
|---|---|---|---|
| 0x699fba | `lea esi,[eax+0x58164]` | 0x58164 | `4+2*P*0xa4` |
| 0x69a09e | `mov ecx,0xb02c` | 0xb02c | `P*0xa4/4` (= P*41) |
| 0x69a0a6 | `lea edi,[eax+0x2c0b4]` | 0x2c0b4 | `4+P*0xa4` |
| 0x69a0eb | `lea edi,[eax+0x2c0b4]` | 0x2c0b4 | `4+P*0xa4` |
| 0x69a282 | `add ecx,0x58164` | 0x58164 | `4+2*P*0xa4` |
| 0x69a2bf | `mov [ecx+eax*4+0x58164],edx` | 0x58164 | `4+2*P*0xa4` |
| 0x69a2d6 | `add ecx,0x58164` | 0x58164 | `4+2*P*0xa4` |
| 0x69a4d0 | `lea ebx,[eax+0x2c0b4]` | 0x2c0b4 | `4+P*0xa4` |
| 0x69a6a9 | `mov dword ptr [edx+eax*4+0x58164],0` | 0x58164 | `4+2*P*0xa4` |

The 1100 counts in these methods (0x699fc7, 0x69a0cc, 0x69a288, 0x69a298, 0x69a2b4, 0x69a2dc, 0x69a2e9, 0x69a48b,
0x69a4b0, 0x69a69d, 0x69ac1a, 0x69ac92, 0x69b17b, 0x69b45f) are already PERSON_COUNT in the review. The 9 fields above are
**not** in the review (no 1100 constant in them).

Snapshot file (0x69af50 / 0x69b210, a 4-byte header then N x 0xa4): N = P makes old files unreadable; give the
load loop a stream count like M9's USTREAM (records beyond the file: keep the init values).

### 4.4 Stack construction sites (frames unchanged under SNAPSHADOW)
| Call site | Function | Frame (`__chkstk` mov eax) | Object at |
|---|---|---|---|
| 0x52df9f | 0x52de50 | 0x592d8 (0x52defe) | esp+0x50 |
| 0x543fb0 | 0x543f50 | 0x59298 (0x543f5e) | esp |
| 0x61c8ad | 0x61c350 (unit dialog of M9) | 0x790e0 (0x61c35e) | esp+0x1fe58 (the M9 unit snapshot shares this frame) |
| 0x622c02 | 0x622bb0 | 0x59298 (0x622bbe) | esp (lea ecx,[esp+4] after a push) |

If the frames were grown instead: frame + (P-1100)*0x14c and every esp offset above the object in those four functions
(EH state 0x592f4/0x592a0/0x790fc/0x592b0, cookie, args) would move; SNAPSHADOW avoids all of that.

## 5. List node pools (0x472e10 / 0x472e80)

0x472e80(pHead, nBlockSize, cbElem, pool, nbits) is MFC's `CPlex::Create` with an optional static pool: with a pool it
takes one 0x7c-byte block (4-byte link + 10 nodes of 0xc, the hard-coded 0x7c at 0x472e68 assumes nBlockSize = 10) from
a bitmap allocator 0x472e10(pool, nbits) laid out as [bitmap ceil(nbits/32)*4][blocks nbits*0x7c]; when the bitmap is
full it falls back to `new(0x7c)` (0x43f140). 0x472ee0(pool, nbits) is FreeDataChain (clears the bit or deletes).

Each CList instantiation has its own NewNode with its own static pool; **all pass nbits = 0x10000**:

| NewNode | vtbl | pool | |
|---|---|---|---|
| 0x47c090 | 0x79bf34 / 0x79bf44 | **0x6ee7950** | the generic object-pointer list (420 vtbl refs). Officer lists use it: AddTail 0x47c1b0 is the call in 46 officer loops; world+0x184 (§2.2) is one |
| 0x491ea0 / 0x492170 | 0x79c8cc.. / 0x79c8dc.. | 0x70743c8 / 0x713ae90 | world lists (e.g. 0x491ff0/0x491f50 list of buildings) |
| 0x40cf20, 0x49f680, 0x49f770, 0x4a2140, 0x4a2220, 0x536350, 0x551810, 0x5519e0, 0x5b3fb0, 0x5d8680, 0x5e1340, 0x5e1410, 0x5f7ed0, 0x623270, 0x6233d0 | | 0x906358, 0x767de40, 0x7744908, 0x780b3d0, 0x78d1e98, 0x91bc600, 0x9283410, 0x9349ed8, 0x96aaa70, 0x97737c8, 0x983a298, 0x9900d60, 0x99cac00, 0x9a926e8, 0x9b591b0 | other types |
| 0x4db6a0, 0x54ace0, 0x5d7a70 | | none (heap only) | |

Capacity is **not** sized by the officer count (0x10000 blocks = 655360 nodes by the bitmap). The officer record itself
holds no list. But: the static storage between consecutive pools is only ~0xc6ac4 bytes (0x6ee7950 -> next referenced
data 0x6fae414; 0x70743c8 -> 0x713ae90 = 0xc6ac8; same for the others), i.e. 0x2000 bitmap + room for about
**6496 blocks (~65000 nodes)**, while the bitmap allows 65536 blocks. Block 6497+ of the generic pool would be written over
the data after 0x6fae414 (the table at 0x6fae8b8, map 0x6fb0e68). Every non-empty list takes at least one 10-node block,
so usage grows with the number of objects in lists; with P = 5000..30000 the generic pool is the one that can run out.
Not seen to overflow with 1100 officers. Cheap fix (field patches): point the generic list at a DLL pool of
0x2000+0x10000*0x7c bytes: `push 0x6ee7950` at 0x47c098 and 0x47c0bf -> `PERSONPOOL` (or lower nbits 0x10000 at 0x47c093
and 0x47c0ba to 6496 so the heap fallback takes over). Worth a runtime check of the bitmap fill (bytes 0x6ee7950..+0x32c)
at P = 5000 before deciding.

## 6. Counts

| Category | New | Known/confirmed | Ruled out |
|---|---|---|---|
| world-relative / absolute displacement into the officer array | 0 | 2 (0x491097, 0x4910ea) | 2381 operand hits, 5 .rdata dwords |
| biased world bases (new-officer tables, id-850) | 2 field patches (0x490eb8, 0x490f15) | | |
| data tables | 1 optional (0x85ef94) | | |
| id-800 table sites | 3 code sites (0x556120, 0x556427, 0x556467) + 1 KEEP (0x5540b5) | 0x54fc3d, 0x5563f7..0x556408 | |
| sizes / frames / rep counts | 0 officer arrays; save header list frame 0xfa4 (0x483120) characterised | 6 frames + 0x4b2380 object (§4.2/4.3 of M10) | ~60 hits (§2.1) |
| snapshot class | 9 field patches + 11 SNAPSHADOW entry swaps + file stream count | 14 count constants in review, 4 stack sites | |
| node pools | 1 risk (generic pool storage ~6496 blocks), 2-site fix | | 17 other pools not officer-specific |
