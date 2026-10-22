"""M10 (M10_PERSONS.md): sites that move the officer (武将) storage out of Koei's fixed 1100 records.

The world holds 1100 officers of 0x190 bytes at world+0xC0BC (0x720DA14); the treasures follow right after them, so the
array moves to a DLL allocation of PT records (worldmod/src/persons.cpp, symbol PERSONARR). Layout (symbols, because
P and S are taken by the M7 port count and the terrain stage): scenario officers 0..PS-1 (Koei's 850 = PS), new
officers PS..PS+149, special officers PS+150..PT-1, PT = PS+250 (Koei's 1100). With PS = 850 every expression equals
Koei's value. The officer-count constants come from the review in m10_review/ (result_*.jsonl, symbols S and P renamed
by gen_m7.py); this file holds the structural part. Imported by gen_m7.py, same row formats as m9_sites.py:

FIELD rows: (va, old value, size in bytes, new expression, comment); REWRITE rows: (start, length, asm, comment);
CAVE_TEXT rows: (va, original instruction, replacement asm) re-encoded in a cave.
"""

WORLD = 0x7201958
PBASE, PSIZE = 0x720DA14, 0x190

FIELD = []
REWRITE = []
CAVE_TEXT = []
SKIP = set()                     # review sites handled here


def f(va, old, expr, comment, size=4):
    FIELD.append((va, old, size, expr, comment))


# ---- officer array -> PERSONARR. The world-relative displacements (getter 0x490b15, ptr -> id 0x49131a, world dtor
# 0x4926b5 / ctor 0x492e8f, post-load 0x493422, serializer 0x493814, ctor EH funclet 0x72ef2e) and the debug name
# dump's absolute loop are in the review; these two point at the first special officer (find / count free specials)
for va in (0x491097, 0x4910ea):
    f(va, 0x6db3c, f'PERSONARR-{WORLD:#x}+(PS+150)*{PSIZE:#x}', 'first special officer -> PERSONARR')

# ---- new-officer tables indexed by id - 850, folded into the displacement (getters 0x490ea0 / 0x490f00)
f(0x490eb8, 0x12c092, '0x1a5050-PS*0x247', 'new-officer edit data (world+0x1a5050): index id-PS')
f(0x490f15, 0x1f3384, '0x1f9dc4-PS*0x20', 'new-officer records (world+0x1f9dc4): index id-PS')

# ---- object-type descriptor table 0x85ef18 (30 x {func, count, nprops}): officer count (read, not used)
f(0x85ef94, 0x44c, 'PT', 'object-type table: officer count (data)')

# ---- virtual fathers: father / mother +0x58 / +0x5c also hold 2000..2060, a token shared by siblings whose parent
# is not an officer (only compared for equality). They move to VF..VF+60 (persons.cpp: 2000 while PT <= 2000, else
# 32000; files are converted by tools/convert_scen.py), so they cannot collide with officer ids.
for lo, hi in ((0x48ac38, 0x48ac44), (0x48bb4f, 0x48bb56), (0x48bc4f, 0x48bc56), (0x48bcaf, 0x48bcb6),
               (0x48bdb8, 0x48bdbf), (0x48bdff, 0x48be06), (0x48be6d, 0x48be74), (0x48bebd, 0x48bec4),
               (0x48c07b, 0x48c082), (0x48c0fb, 0x48c102), (0x48d02c, 0x48d034)):
    f(lo, 0x7d0, 'VF', 'virtual father: first')
    f(hi, 0x80c, 'VF+60', 'virtual father: last')

# ---- officer snapshot (vt 0x862d28, ctor 0x69a410 -> init 0x699fb0, object 0x59294 bytes on the stack of 0x52de50,
# 0x543f50, 0x61c350 and 0x622bb0): 1100 x 0xa4 records at +4, a backup copy at +0x2c0b4, 1100 x 4 flags at +0x58164.
# As M9's unit snapshot, every data method starts by swapping this for a per-instance DLL buffer of the same layout
# sized for PT (persons.cpp personSnapShadow, which also accepts a buffer it handed out), so the frames keep their size.
# The 1100 counts inside these methods are in the review.
SNAP_ENTRY = [(0x699fb0, 'push ecx'), (0x69a0c0, 'mov eax, dword ptr [esp + 4]'), (0x69a280, 'xor eax, eax'),
              (0x69a2b0, 'mov eax, dword ptr [esp + 4]'), (0x69a2d0, 'mov edx, dword ptr [esp + 4]'),
              (0x69a450, 'push ecx'), (0x69a4a0, 'push ecx'), (0x69abb0, 'push ebx'), (0x69ac30, 'push ebx'),
              (0x69af50, 'push ebp'), (0x69b210, 'push ebp')]
CAVE_TEXT += [(va, old, 'push edx; push ecx; mov eax, {PSNAPSHADOW}; call eax; add esp, 4; pop edx; mov ecx, eax; ' + old)
              for va, old in SNAP_ENTRY]
SNAP_FLAGS, SNAP_BACKUP = '4+2*PT*0xa4', '4+PT*0xa4'
f(0x699fba, 0x58164, SNAP_FLAGS, 'officer snapshot: flags (after 2 x PT records)')
f(0x69a09e, 0xb02c, 'PT*0xa4/4', 'officer snapshot: backup copy (dwords)')
for va in (0x69a0a6, 0x69a0eb, 0x69a4d0):
    f(va, 0x2c0b4, SNAP_BACKUP, 'officer snapshot: backup records (after PT records)')
for va in (0x69a282, 0x69a2bf, 0x69a2d6, 0x69a6a9):
    f(va, 0x58164, SNAP_FLAGS, 'officer snapshot: flags (after 2 x PT records)')

# ---- the generic object-pointer list (CList vtbl 0x79bf44, NewNode 0x47c090) takes its 10-node blocks from a static
# pool at 0x6ee7950 whose bitmap allows 0x10000 blocks, but only about 6500 blocks of storage lie before the next data
# (0x6fae414): with thousands of officers in lists it would run over them. The pool moves to a DLL allocation of the
# full size (persons.cpp PPOOL: 0x2000-byte bitmap + 0x10000 x 0x7c).
for va in (0x47c098, 0x47c0bf):
    f(va, 0x6ee7950, 'PPOOL', 'generic list node pool -> PPOOL')

# ---- officer record serializer (vtable+0x30, 0x48b760): a scenario stream stores the officers below 850 (the
# record is reset first when reading). The bound becomes the stream's scenario-officer count (persons.cpp PSTREAM,
# from the header mark: 850 for Koei's files), so older scenarios load with officers PSTREAM..PS-1 left empty.
CAVE_TEXT.append((0x48b787, 'cmp eax, 0x352', 'cmp eax, dword ptr [{PSTREAM}]'))
SKIP.add(0x48b787)

# ---- biography (0x48e850, cdecl (string, officer ptr)): ids 0..849 take a message of S11MSG02.S11, new officers
# their edit data (0x488250 / 0x490ea0). Extended officers fall through to 0x48e91e (esi = id, edi = string): the DLL
# gives their text (persons.cpp personBio, persons_bio.bin), appended with 0x471ac0 like the others.
CAVE_TEXT.append((0x48e91e, 'push esi',
                  'push esi; mov eax, {PBIO}; call eax; add esp, 4; test eax, eax; je koei; '
                  'mov ecx, edi; push eax; mov eax, 0x471ac0; call eax; mov eax, 0x48e942; jmp eax; koei: push esi'))

# allocations made by persons.cpp (bytes), for the record
ALLOCS = {'PERSONARR': f'PT*{PSIZE:#x}', 'PPOOL': '0x2000+0x10000*0x7c', 'snapshot shadows': '4+PT*0x14c each'}
