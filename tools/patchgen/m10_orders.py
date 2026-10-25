"""M10 (M10_PERSONS.md §4.7 "存档头 +0x184 列表", m10_review/orders.md): officers with an order in progress.

The world's base part (vtable 0x79c134, the world at 0x7201958 is its only instance) keeps at +0x184 (0x7201adc) a
generic object-pointer list (CList vtbl 0x79bf44: +4 head, +8 tail, +0xc count; node +0 next, +4 prev, +8 data) of the
officers that carry an order (officer +0x13c = 0..43, parameters +0x140..+0x150, setter 0x489bd0):
* added only by 0x482f80 (world thiscall, officer; valid + not yet present -> AddTail 0x47c1b0), right after
  0x489bd0 gives an order 0..43 (0x4a7403, 0x4a746b, 0x4a83dd, 0x4a8655, 0x4a8a8c);
* removed only by the purge 0x483440 (drops entries that are not valid objects or whose +0x13c is outside 0..43;
  called by 0x4a1880 / 0x4a1a20 after they process the list, and by the turn routine at 0x59a081), RemoveAll in the
  reset 0x482cb0 (vtable+0x20, run before a load) and the dtor 0x482c50;
* saved by the header serializer 0x483120 (vtable+0x30, the first thing 0x4937b0 serializes, i.e. BEFORE the officer
  records): at most 1000 entries (frame 0xfa4, `cmp ebp, 0x3e8` 0x48335f), 1000 x i16 in the file. Entries past 1000
  are dropped silently and, after loading, their officers keep their order (+0x13c is in the officer record of save
  streams, 0x48baaa) but are never processed again (a moving officer never arrives).

Fix (orders.md §5, option a): the file format stays (1000 entries); after a save stream has been read completely
(0x4937b0 at 0x493be5, the post-load call 0x493400 on the "not type 4 / 0x18" read path; officers are loaded by then),
the DLL appends every valid officer with +0x13c in 0..43 that the list does not hold yet (persons.cpp
personsRebuildOrders, symbol PORDERS, cdecl (list)). Only save streams carry the list (0x43a8f0(stream) != 0, the same
test 0x483120 uses at 0x48319f); the cave tests it, so scenario loads are untouched. With <= 1000 entries in the list
the DLL finds nothing to add and the game behaves exactly as Koei's.

The 1000s at 0x48335f / 0x48336b / 0x4833b7 stay (review verdict UNSURE, no patch); 0x483398 `cmp eax, 0x44b` is the
review's P-1 (PERSON_MAX) and stays in the review. Nothing here replaces a review row, so SKIP is empty.

Wiring (not done here): gen_m7.py adds m10_orders.FIELD / CAVE_TEXT / SKIP next to m10_sites; worldmod.cpp registers
{ "PORDERS", (void*)personsRebuildOrders } with the other person symbols.

Same row formats as m9_sites.py / m10_sites.py:
FIELD rows: (va, old value, size in bytes, new expression, comment); REWRITE rows: (start, length, asm, comment);
CAVE_TEXT rows: (va, original instruction, replacement asm) re-encoded in a cave.
"""

ORDERS_LIST = 0x7201adc          # world 0x7201958 + 0x184

FIELD = []
REWRITE = []
CAVE_TEXT = []
SKIP = set()

# ---- 0x4937b0 (world serializer, ebx = world, ebp = stream), read path for streams other than type 4 / 0x18:
#   0x493bce cmp [ebp+8], 1 (reading) ; 0x493bda 0x43a940(stream) == 0 ; 0x493be5 call 0x493400 (post-load vfunc+0x34
#   of every object) ; 0x493bea mov ecx, ebx ...
# After the post-load call: if 0x43a8f0(stream) (save stream: types 6, 0xc, 0x11, 0x1a, 0x1b) the DLL completes the
# list. eax / ecx / edx are free (0x493bea reloads ecx; eax is not read before the next call); ebx / ebp / esi / edi
# are preserved by the callees (cdecl / thiscall).
CAVE_TEXT.append((0x493be5, 'call 0x493400',
                  'mov eax, 0x493400; call eax; '
                  'mov ecx, ebp; mov eax, 0x43a8f0; call eax; test eax, eax; je skip; '
                  f'push {ORDERS_LIST:#x}; mov eax, {{PORDERS}}; call eax; add esp, 4; '
                  'skip:'))


if __name__ == '__main__':
    # validation: originals against pk.asm, replacements assemble with keystone ({...} -> dummy constant)
    import re
    import sys
    from keystone import Ks, KS_ARCH_X86, KS_MODE_32
    asm = sys.argv[1] if len(sys.argv) > 1 else 'pk.asm'
    text = {}
    for line in open(asm, encoding='utf-8'):
        parts = line.rstrip('\n').split(None, 2)
        if len(parts) == 3 and len(parts[0]) == 8:
            text[int(parts[0], 16)] = parts[2].strip()
    ks = Ks(KS_ARCH_X86, KS_MODE_32)
    for va, old, new in CAVE_TEXT:
        assert text.get(va) == old, (hex(va), text.get(va), old)
        enc, _ = ks.asm(re.sub(r'\{[^}]*\}', '0x12345678', new), 0x900000)
        print(f'{va:#x} {old!r}: {len(enc)} bytes')
    assert not FIELD and not REWRITE and not SKIP
    print('ok')
