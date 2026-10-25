"""M10 (M10_PERSONS.md §4.4, §4.5, §4.7; m10_review/flows.md §3.4, §5, §6): code patches for the officer-id ranges.

Layout (symbols as in m10_sites.py): scenario officers 0..PS-1 (Koei's 0..849 keep their ids: 0..699 historical,
700..799 event, 800..849 古武将; the EXTENDED officers 850..PS-1 are historical too), new officers PS..PS+149, special
officers PS+150..PT-1, PT = PS+250. The plain bounds (850/999/1000/1099/1100) come from the review (m10_review); the
places below cannot be fixed by a constant, because the extended officers sit between Koei's sub-ranges:

1. "historical" predicates (0..699) also accept 850..PS-1;
2. Koei's 0..699 loops run 0..699 then 850..PS-1 (and the inline 0..699 checks inside them accept 850..PS-1);
3. the 200-entry start-setup table (struct +0x428 / obj +0x24ac / obj +0x28d8, 200 x 0x10, index id-800 for 800..999)
   keeps 200 entries: 古武将 800..849 -> 0..49, new officers PS..PS+149 -> 50..199, through the virtual id
   v = id < 850 ? id : id-(PS-850) (id -> index) and id = v < 850 ? v : v+(PS-850) (index -> id);
4. the "古武将 or new officer" checks and loops (800..999 in Koei's exe, 800..PS+149 after the constant review) are split
   into 800..849 and PS..PS+149.

With PS = 850 every row behaves exactly like Koei's code (the inserted second range / remap is empty). The only visible
difference: a 0..699 loop counter leaves its loop as 850 instead of 700; every such counter is dead after its loop
(checked per site below).

Same row formats as m9_sites.py / m10_sites.py:
FIELD rows: (va, old value, size in bytes, new expression, comment); REWRITE rows: (start, length, asm, comment);
CAVE_TEXT rows: (va, original instruction, replacement asm) re-encoded in a cave (the builder steals >= 5 bytes around
the instruction and relocates the others; branches in a replacement target local labels or absolute exe addresses).
SKIP: review rows (m10_review, verdict PERSON) at instructions replaced here; gen_m7.py must drop them.
"""

FIELD = []
REWRITE = []
CAVE_TEXT = []
SKIP = set()

HIST_LAST = 0x2bb            # 699: last Koei historical officer
EXT0 = 0x352                 # 850: first extended officer (Koei: first new officer)
OLD_LAST = 0x351             # 849: last 古武将


# ---- 1. historical predicates. Both are `test eax,eax; jl F; cmp eax,0x2bb; jg F; T: mov eax,1; ret; F: xor eax,eax;
# ret`; the cmp becomes "0..699 -> T, 850..PS-1 -> fall into the original jg with flags of cmp eax,PS-1".
# 0x489c60 isHistorical(person) (11 callers), 0x488230 isHistorical(id) (0x48d692).
# (0x495110 validOfficer(id): its 849 at 0x495126 is already S-1 in the review, so 800..849 becomes 800..PS-1.)
def hist_check(reg, accept, reject):
    """replaces `cmp reg, 0x2bb` that is followed by `jg reject` (reg >= 0 already): accept 0..699 and 850..PS-1"""
    return (f'cmp {reg}, {HIST_LAST:#x}; jle {accept:#x}; cmp {reg}, {EXT0:#x}; jl {reject:#x}; '
            f'cmp {reg}, {{PS-1}}')


CAVE_TEXT += [
    (0x489c6f, 'cmp eax, 0x2bb', hist_check('eax', 0x489c76, 0x489c7c)),     # isHistorical(person)
    (0x488238, 'cmp eax, 0x2bb', hist_check('eax', 0x48823f, 0x488245)),     # isHistorical(id)
]

# ---- 2a. inline 0..699 checks (`cmp r,0x2bb; jg X`, r >= 0 tested before)
CAVE_TEXT += [
    # officer death 0x4a2600: sworn-brother group id ebx (+0x60) -> the clearing loop 0x4a26d3 (new / 古 groups are
    # handled before it at 0x4a265b / 0x4a266f)
    (0x4a268b, 'cmp ebx, 0x2bb', hist_check('ebx', 0x4a2697, 0x4a2750)),
    # new game 0x4a42d0, first pass over the historical officers: group +0x60, group +0x64, 5 liked (0x488890),
    # 5 disliked (0x488950) pointing at a dead historical officer are cleared
    (0x4a4638, 'cmp eax, 0x2bb', hist_check('eax', 0x4a463f, 0x4a465e)),
    (0x4a4669, 'cmp ebx, 0x2bb', hist_check('ebx', 0x4a4675, 0x4a478d)),
    (0x4a479c, 'cmp eax, 0x2bb', hist_check('eax', 0x4a47a3, 0x4a47c3)),
    (0x4a47dc, 'cmp eax, 0x2bb', hist_check('eax', 0x4a47e3, 0x4a4803)),
]


# ---- 2b. 0..699 loops -> 0..699, 850..PS-1. Each tail is `inc r; cmp r, 699; jle top` (0x642c41: `cmp r, 700; jl`);
# the cmp becomes `cmp r,700; jne X; mov r,850; X: cmp r,<same bound in PS>` and the original jcc stays. The counter is
# dead after every loop: 0x4a2706 ebp reloaded at 0x4a275f; 0x4a480e / 0x4a4b46 store the counter to [esp+0x60]
# right after the cmp (the cave changes it first), [esp+0x60] reset at 0x4a4875 / not read again in 0x4a42d0;
# 0x4a4869 esi reloaded at 0x4a488c; 0x4a4b46 edi reloaded before use (0x4a4b96 / 0x4a4c40); 0x642c41 ebp popped.
def hist_loop(reg, last):
    return f'cmp {reg}, 0x2bc; jne X; mov {reg}, {EXT0:#x}; X: cmp {reg}, {{{last}}}'


CAVE_TEXT += [
    (0x4a2706, 'cmp ebp, 0x2bb', hist_loop('ebp', 'PS-1')),   # officer death: clear the dead's sworn-brother group
    (0x4a480e, 'cmp eax, 0x2bb', hist_loop('eax', 'PS-1')),   # new game: drop relations to dead officers
    (0x4a4869, 'cmp esi, 0x2bb', hist_loop('esi', 'PS-1')),   # new game option: no female officers
    (0x4a4b46, 'cmp edi, 0x2bb', hist_loop('edi', 'PS-1')),   # new game "fictional" options (relations, stats, ...)
    (0x642c41, 'cmp ebp, 0x2bc', hist_loop('ebp', 'PS')),     # captives (status 5) at a unit's location: count
]
# left as Koei wrote it: 0x6902f0(0, 699) (3 random living officers for a title / demo screen), and the editor /
# face-picker lists 0x52a9b8, 0x549441, 0x549c80 (flows.md §4: extended officers are simply not listed)


# ---- 3. the 200-entry start-setup table (id - 800). id -> index: normalise the id (only in the register that becomes
# the index; at 0x54e154, 0x55641c, 0x556460 the real id stays live in eax / ebp). The flags after each site are dead
# (the next flag reader is preceded by a flag writer: shl / test / sub / cmp, or the code only pushes and calls).
REMAP = '{PS-850}'


def norm(reg, orig):
    """v = reg < 850 ? reg : reg - (PS-850), then the original instruction"""
    return f'cmp {reg}, {EXT0:#x}; jl X; sub {reg}, {REMAP}; X: {orig}'


def norm_after(orig, reg):
    """the original instruction (which loads reg = id), then v = norm(reg)"""
    return f'{orig}; cmp {reg}, {EXT0:#x}; jl X; sub {reg}, {REMAP}; X:'


CAVE_TEXT += [
    # 0x54e0f0 (apply setup): city +0x54 officer eax (checked 古 / new before) -> ecx = (id-733)*16 = entry +8
    (0x54e154, 'lea ecx, [eax - 0x2dd]',
     f'lea ecx, [eax - 0x2dd]; cmp eax, {EXT0:#x}; jl X; sub ecx, {REMAP}; X:'),
    # 0x54e0f0: officer eax (0x491310) -> table[(id-800)*16] = ebx (eax dead afterwards)
    (0x54e33c, 'sub eax, 0x320', norm('eax', 'sub eax, 0x320')),
    # setup loop 0x54fbe6 800..999 (split below): officer id eax (vt+0x28) -> eax*16 + struct - 0x2dd8
    (0x54fc36, 'mov ecx, dword ptr [esp + 0x14]', norm('eax', 'mov ecx, dword ptr [esp + 0x14]')),
    # 0x5539f0 (obj +0x24ac): edi = id (vt+0x28) - 800, guarded by `cmp edi, 0xc8; jge skip`; an extended officer
    # (850..PS-1, not expected in this list) is sent to index 200 so the guard skips it
    (0x553abd, 'sub edi, 0x320',
     f'cmp edi, {EXT0:#x}; jl X; sub edi, {REMAP}; cmp edi, {EXT0:#x}; jge X; mov edi, 0x3e8; X: sub edi, 0x320'),
    # 0x553b40 (obj +0x24ac): eax = id (vt+0x28) - 800
    (0x553c04, 'sub eax, 0x320', norm('eax', 'sub eax, 0x320')),
    # 0x5540b0 (obj +0x28d8): list of 古 + new officers (0x4cf820); a new officer (0x489c80 false) reaches the index
    # directly, so this site is NOT 古-only (structural.md §3 said KEEP): ecx = id*16 - 0x928
    (0x5540b0, 'mov ecx, eax', norm_after('mov ecx, eax', 'ecx')),
    # 0x556114 (obj +0x28d8): slot record id [eax+4] -> ecx
    (0x556114, 'mov ecx, dword ptr [eax + 4]', norm_after('mov ecx, dword ptr [eax + 4]', 'ecx')),
    # 0x556380 (obj +0x28d8): slot id [esi+4] (checked 0..PT-1 only, Koei's latent bug) -> ecx - 800
    (0x5563ff, 'add ecx, 0xfffffce0', norm('ecx', 'add ecx, 0xfffffce0')),
    # same function: ebp = officer id (vt+0x28), shifted for the second entry; ebp is reloaded from [esp+0x28]
    # (0x556448) and stored back as the real id (0x55648d), so normalising it here is safe
    (0x55641c, 'shl ebp, 4', norm('ebp', 'shl ebp, 4')),
    # same function, other branch: eax = ebp (real id, ebp stays the real id)
    (0x556460, 'mov eax, ebp', norm_after('mov eax, ebp', 'eax')),
]


# index -> id: the four loops over the 200 entries (0..199) build the officer id 800+idx; v >= 850 -> + (PS-850)
def denorm(orig, reg):
    return f'{orig}; cmp {reg}, {EXT0:#x}; jl X; add {reg}, {REMAP}; X:'


CAVE_TEXT += [
    (0x54e050, 'lea eax, [ebx + 0x320]', denorm('lea eax, [ebx + 0x320]', 'eax')),   # 0x54e040 apply
    (0x54e192, 'add eax, 0x320', denorm('add eax, 0x320', 'eax')),                   # 0x54e0f0 apply
    (0x5516c0, 'lea eax, [edi + 0x320]', denorm('lea eax, [edi + 0x320]', 'eax')),   # 0x551460 build
    (0x557760, 'add eax, 0x320', denorm('add eax, 0x320', 'eax')),                   # 0x557460 copy
]
# not changed: 0x54fc3d / 0x556120 / 0x556408 / 0x556427 / 0x556467 / 0x553ad9 / 0x553c09 (they use the register
# normalised above); 0x5540b5 (same); the 200 bounds (0xc8) and the per-index loops (0x553a11.., 0x556151..).


# ---- 4. "古武将 or new officer" = 800..849 or PS..PS+149. Checks `cmp eax,0x320; jl N; cmp eax,0x3e7; jg N`
# (or `...; jle M` with N the fall-through): the 999 compare becomes "800..849 -> flags of cmp eax,PS+149 (in);
# 850..PS-1 -> N; else the original jcc decides on cmp eax,PS+149". The review's 999 -> S+149 rows at these
# instructions are dropped (SKIP).
def split_check(reg, nonmember):
    return f'cmp {reg}, {OLD_LAST:#x}; jle L; cmp {reg}, {{PS}}; jl {nonmember:#x}; L: cmp {reg}, {{PS+149}}'


CHECKS = [   # (va of `cmp eax, 0x3e7`, non-member address)        what
    (0x4c8e04, 0x4c8e25),   # script property "古 / new officer" (0x4c8df2)
    (0x5410fa, 0x541101),   # forces whose ruler is not 古 / new
    (0x54b023, 0x54b031),   # setup screen predicate (vt+0x28 id)
    (0x54b87a, 0x54b881),   # force ruler 古 / new
    (0x54da79, 0x54da87),   # setup screen predicate (vt+0x28 id)
    (0x54e324, 0x54e356),   # 0x54e0f0: before table[(id-800)*16] (0x54e33c)
    (0x5515c7, 0x5515ce),   # 0x551460 build: force loop
    (0x552428, 0x552430),   # count 古 / new in a list
    (0x552a5d, 0x552c28),
    (0x552b59, 0x552b8f),
    (0x552e28, 0x552e2f),
    (0x552f2c, 0x552f33),
    (0x55372c, 0x553734),
    (0x55384c, 0x553854),
    (0x5575ed, 0x5575f4),
    (0x557623, 0x557657),
    (0x557bfa, 0x557c01),   # force ruler [ebp+4] 古 / new
]
CAVE_TEXT += [(va, 'cmp eax, 0x3e7', split_check('eax', n)) for va, n in CHECKS]
# already right after the review (new range tested first, then 800..849): 0x4a0750 (0x4a0740), 0x54e131 / 0x54e13f,
# 0x4a2658; and every 850..999 (new officers only) check, which the review turns into PS..PS+149.


# loops 800..999 (`inc r; cmp r,0x3e7; jle top`): 800..849 then PS..PS+149. The counter is dead after each loop
# (0x54d276 / 0x5500b2 / 0x54fc4d / 0x54fcc3 popped, 0x55327d zeroed at 0x553285).
def split_loop(reg):
    return f'cmp {reg}, {EXT0:#x}; jne X; mov {reg}, {{PS}}; X: cmp {reg}, {{PS+149}}'


CAVE_TEXT += [
    (0x54d276, 'cmp edi, 0x3e7', split_loop('edi')),   # loop 0x54d229: any free 古 / new officer
    (0x54fc4d, 'cmp ebx, 0x3e7', split_loop('ebx')),   # loop 0x54fbe6: setup table +0xc (indexes the table, 0x54fc36)
    (0x54fcc3, 'cmp edi, 0x3e7', split_loop('edi')),   # 0x54fc60 (loop 0x54fc62): any 古 / new officer without a force
    (0x5500b2, 'cmp edi, 0x3e7', split_loop('edi')),   # loop 0x550037: list control of 古 / new officers
    (0x55327d, 'cmp edi, 0x3e7', split_loop('edi')),   # loop 0x553213: list of free 古 / new officers
]

# review rows (verdict PERSON, 999 -> S+149) at instructions replaced above
SKIP |= {va for va, _ in CHECKS} | {0x54d276, 0x54fc4d, 0x54fcc3, 0x5500b2, 0x55327d}
