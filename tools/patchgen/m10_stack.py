"""M10 (M10_PERSONS.md §4.2/§4.3, m10_review/structural.md §2.3): stack frames that hold officer-indexed arrays.

Koei sized these locals for 1100 officers; they grow to PT entries (int[PT] = 4*PT bytes, byte[PT] = A4(PT) bytes) by
enlarging the frame in place: the frame size, every esp-relative offset at or above the end of the array (SEH slots,
cookie, arguments read through esp, the `add esp` epilogue) and the member offsets of an object whose second array
follows the first. Frames above a page are probed (__chkstk 0x707d40, eax = size). With PT = 1100 every expression
equals Koei's value. The rep counts / loop bounds (`mov ecx, 0x44c`, `cmp r, 0x44c`, 0x4b0162 `[esi - 0x1130]`) are in
the review (m10_review/result_*.jsonl, verdict PERSON) and are not repeated here.

Stack use at PT = 15250 (1 MB main-thread reserve): 0x4b2820 ~127 KB, 0x5c7dc0 / 0x5c8bb0 ~119 KB, 0x5c97a0 and
0x590490 ~60 KB, 0x4b7e70 ~15 KB; none of them calls another of them.

Imported by gen_m7.py, same row formats as m9_sites.py:
FIELD rows: (va, old value, size in bytes, new expression, comment); REWRITE rows: (start, length, asm, comment);
CAVE_TEXT rows: (va, original instruction, replacement asm) re-encoded in a cave.
M7_GROW: {va: suffix} for rows of m7_struct_sites.jsonl whose expression must grow too (0x590490 is already a
M7 frame; add_field would reject a second expression for the same field), applied like M9's MapUI row:
`r['expr'] += M7_GROW[int(r['va'], 16)]`.
"""

FIELD = []
REWRITE = []
CAVE_TEXT = []
M7_GROW = {}


def f(va, old, expr, comment, size=4):
    FIELD.append((va, old, size, expr, comment))


# ---- 0x5c7dc0 (ebp frame, and esp,-8; chkstk 0x226c; push ebx/esi/edi): int[1100] at esp+0x18 (cleared 0x5c7de7)
# and int[1100] at esp+0x1148 (cleared 0x5c7ee8), the second one ends at the top of the frame. Arguments via ebp.
D1 = '(PT-1100)*8'
f(0x5c7dc6, 0x226c, f'0x226c+{D1}', '0x5c7dc0 frame: two int[PT]')
for va in (0x5c7eed, 0x5c8008, 0x5c8052):
    f(va, 0x1148, '0x18+PT*4', '0x5c7dc0: second int[PT] (after the first at esp+0x18)')

# ---- 0x5c8bb0 (same shape, chkstk 0x2274): int[1100] at esp+0x20 (0x5c8bd7), int[1100] at esp+0x1150 (0x5c8cd7)
f(0x5c8bb6, 0x2274, f'0x2274+{D1}', '0x5c8bb0 frame: two int[PT]')
for va in (0x5c8cdc, 0x5c8d75, 0x5c8d7c, 0x5c8dd1):
    f(va, 0x1150, '0x20+PT*4', '0x5c8bb0: second int[PT] (after the first at esp+0x20)')

# ---- 0x5c97a0 (ebp frame, and esp,-8, SEH; chkstk 0x1164 at 0x5c97bc): int[1100] at esp+0x40 (0x5c97ed) up to the
# pushed ecx; the SEH record above it (+0x1174 next, +0x117c state) moves. A8 keeps esp 8-aligned for odd PT.
# (0x5c92a0's own 0x102c chkstk at 0x5c92bc holds no officer array.)
D3 = 'A8((PT-1100)*4)'
f(0x5c97bc, 0x1164, f'0x1164+{D3}', '0x5c97a0 frame: int[PT]')
f(0x5c9847, 0x1180, f'0x1180+{D3}', '0x5c97a0: SEH state (one push)')
f(0x5c9992, 0x117c, f'0x117c+{D3}', '0x5c97a0: SEH state')
f(0x5c99a6, 0x117c, f'0x117c+{D3}', '0x5c97a0: SEH state')
f(0x5c99be, 0x1174, f'0x1174+{D3}', '0x5c97a0: SEH next (fs:[0] restore)')

# ---- 0x590490 (ebp frame, and esp,-8, SEH; chkstk 0x1740 at 0x5904ac): class A (vt 0x84b09c, its only instance)
# at esp+0x4a0, int[1100] at +0x180 (cleared 0x5907ad; indexed by 0x58d1d0 / 0x58d430) is the last member and ends at
# the pushed ecx. M7 already grows this frame and moves these slots (m7_struct_sites.jsonl: 0x1740+3*A8((N-87)*4),
# 0x175c+3*A8(...), 0x1754+3*A8(...)); the officer array adds its own growth on top.
D4 = '+A8((PT-1100)*4)'
for va in (0x5904ac,                                                    # chkstk frame size
           0x590834, 0x5908b5, 0x5909e4, 0x5909f5, 0x590bdb, 0x590bf2,  # SEH state [esp+0x175c]
           0x590c0d):                                                   # SEH next [esp+0x1754]
    M7_GROW[va] = D4

# ---- 0x4b2820 (ebp frame, and esp,-8, SEH; chkstk 0x398c at 0x4b283c): object of 0x4b27f0 / 0x4b2380 / 0x4b0140 /
# 0x4b01b0 / 0x4aef90 at esp+0x1728 ({4 dwords}, int[1100] at +0x10, int[1100] at +0x1140), built only here (all
# its methods are reached from 0x4b29d4); it ends at the pushed ecx, the SEH record above it moves.
D5 = '(PT-1100)*8'
f(0x4b283c, 0x398c, f'0x398c+{D5}', '0x4b2820 frame: object with two int[PT]')
f(0x4b28eb, 0x39a8, f'0x39a8+{D5}', '0x4b2820: SEH state (one push)')
f(0x4b2c5c, 0x39a4, f'0x39a4+{D5}', '0x4b2820: SEH state')
f(0x4b2c6c, 0x399c, f'0x399c+{D5}', '0x4b2820: SEH next (fs:[0] restore)')
for va in (0x4b015c, 0x4aefc9, 0x4b01ed, 0x4b2415, 0x4b24e1, 0x4b2702, 0x4b2717):
    f(va, 0x1140, '0x10+PT*4', 'officer-pair object: second int[PT] at +0x10+PT*4')

# ---- 0x4b7e70 (SEH, cookie, `sub esp, 0x480` without probe; push ebx/ebp/esi): byte[1100] at esp+0x3c built by
# 0x4b5430 (ranks of the 17 officers of table 0x8a69a8; getter bound 0x4b5418 in the review), cookie at esp+0x488 right
# after it, then the SEH record and the argument at esp+0x49c. The frame becomes a probed one (eax is dead there: it
# held fs:[0], already pushed, and is reloaded with the cookie next; __chkstk keeps ecx/edx/ebx/esi/edi).
D6 = 'A4(PT)-1100'
CAVE_TEXT.append((0x4b7e85, 'sub esp, 0x480', f'mov eax, {{0x480+{D6}}}; call 0x707d40'))
f(0x4b7e94, 0x48c, f'0x48c+{D6}', '0x4b7e70: cookie store (one push)')
f(0x4b7ee4, 0x4a0, f'0x4a0+{D6}', '0x4b7e70: SEH state (three pushes)')
f(0x4b7ef0, 0x49c, f'0x49c+{D6}', '0x4b7e70: argument 1')
f(0x4b801e, 0x494, f'0x494+{D6}', '0x4b7e70: SEH state')
f(0x4b8032, 0x49c, f'0x49c+{D6}', '0x4b7e70: argument 1')
f(0x4b8067, 0x494, f'0x494+{D6}', '0x4b7e70: SEH state')
f(0x4b8079, 0x48c, f'0x48c+{D6}', '0x4b7e70: SEH next (fs:[0] restore)')
f(0x4b8089, 0x480, f'0x480+{D6}', '0x4b7e70: cookie check (after two pops)')
f(0x4b8096, 0x48c, f'0x48c+{D6}', '0x4b7e70: epilogue add esp')
f(0x4b5434, 0x113, 'A4(PT)/4', '0x4b5430: clear byte[PT] (dwords)')
