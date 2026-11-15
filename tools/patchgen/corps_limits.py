"""Sixteen corps per force, including the ruler's first corps.

Corps IDs still use the existing global pool and save format. The AI's two
per-force scratch arrays grow from nine entries (including index zero) to
seventeen; their caller's city array moves past the enlarged statistics.
"""

LIMIT = 16
FIELD = []


def field(site, old, new, comment):
    FIELD.append((site, old, 1, str(new), '16 corps: ' + comment))


for site, comment in [
    (0x47e019, 'number validator'),
    (0x47e16d, 'number setter'),
    (0x47e5eb, 'corps consistency check'),
    (0x47ebe0, 'leader and corps refresh'),
    (0x48124f, 'force-local number lookup'),
    (0x4b5e21, 'force-wide corps refresh'),
    (0x4be48e, 'force-wide corps transfer'),
    (0x5b43a4, 'force-wide delegation reset'),
    (0x5c0953, 'regroup availability'),
    (0x5c18b7, 'regroup selected number'),
    (0x5c1ad8, 'new corps selected number'),
    (0x5c1db2, 'new corps availability'),
    (0x5d6214, 'officer reassignment checks'),
    (0x5debb1, 'AI desired corps cap'),
    (0x5debe3, 'AI desired corps cap value'),
    (0x5f46a5, 'AI existing delegated corps loop'),
    (0x5f46d3, 'AI creation cap'),
    (0x5f5b2b, 'AI all-corps actions'),
    (0x5f5c88, 'AI delegated corps actions'),
    (0x603928, 'native corps dialog validation'),
    (0x618d2e, 'delegation details dialog validation'),
    (0x675f64, 'force-wide corps merge'),
]:
    field(site, 8, LIMIT, comment)

for site, comment in [
    (0x5dcc2c, 'AI strongest corps search'),
    (0x5debd9, 'AI desired corps scan'),
    (0x5f4700, 'AI creation candidate scan'),
]:
    field(site, 9, LIMIT + 1, comment)

# Second statistics array: +8 + 17*4 instead of +8 + 9*4.
for site in (0x5dcb71, 0x5dcb80, 0x5dcc1d):
    field(site, 0x2c, 0x4c, 'AI corps weight array')
field(0x5dcc1a, 0x24, 0x44, 'AI weight relative to count pointer')

# 0x5d6090 counts officers by corps number when choosing cities/officers.
# Keep the count-array base; move its iterator and incoming arguments up 32.
for site in (0x5d6090, 0x5d60c4, 0x5d621f):
    field(site, 0x28, 0x48, 'officer count stack frame')
for site, old in [(0x5d6094, 0x30), (0x5d6165, 0x40),
                  (0x5d616e, 0x3c), (0x5d6174, 0x3c), (0x5d61cd, 0x3c)]:
    field(site, old, old + 0x20, 'officer count iterator or argument')

# These frames already grow with N in M7. Add our space to that expression.
FRAME_EXTRA = {site: 0x40 for site in
               (0x5f4580, 0x5f45c2, 0x5f470e, 0x5f471b, 0x5f47b8)}

NUMBER_TEXT = (
    'cmp ecx, 10; jl O; cmp ecx, 16; jg O; '
    # Big5 十, followed by the existing digit character for 11..16.
    'mov word ptr [esp+0x10], 0x51a4; mov word ptr [esp+0x12], 0; '
    'sub ecx, 10; jz D; mov edx, dword ptr [ecx*4+0x8ae388]; '
    'mov dx, word ptr [edx]; mov word ptr [esp+0x12], dx; '
    'mov byte ptr [esp+0x14], 0; '
    'D: lea edx, [esp+0x10]; jmp E; '
    'O: mov edx, dword ptr [ecx*4+0x8ae388]; E: nop')

CAVE_TEXT = [
    # Replace the eight-slot stack allocator with the native lookup, retaining
    # thiscall's ret 4 and all nonvolatile registers. Zero means no free number.
    (0x4a2a40, 'mov eax, dword ptr fs:[0]',
     'push ebx; push esi; mov esi, dword ptr [esp+12]; push esi; '
     'call 0x47a630; add esp, 4; test eax, eax; jz N; mov ebx, 1; '
     'L: push ebx; mov ecx, esi; call 0x481240; cmp eax, -1; je F; '
     'inc ebx; cmp ebx, 16; jle L; '
     'N: xor eax, eax; jmp D; F: mov eax, ebx; '
     'D: pop esi; pop ebx; ret 4'),
    (0x48f427, 'mov edx, dword ptr [ecx*4 + 0x8ae388]', NUMBER_TEXT),
    (0x48f4d7, 'mov edx, dword ptr [ecx*4 + 0x8ae388]', NUMBER_TEXT),
    (0x5d60c8, 'xor ecx, ecx',
     'push edi; lea edi, [esp+0x14]; xor eax, eax; mov ecx, 16; '
     'rep stosd; pop edi; xor ecx, ecx'),
    # Skip the two unrolled nine-entry initializers; clear the enlarged object.
    # At 0x5dcb20 four nonvolatile registers have been pushed.
    (0x5dcad5, 'xor ecx, ecx',
     'xor ebx, ebx; push edi; mov edi, esi; xor eax, eax; mov ecx, 0x24; '
     'rep stosd; mov ebp, dword ptr [esp+0x18]; jmp 0x5dcb20'),
    (0x5dcb8a, 'mov eax, dword ptr [esi + 8]',
     'xor eax, eax; lea edx, [esi+8]; mov ecx, 17; '
     'L: cmp dword ptr [edx], eax; jle S; inc dword ptr [esi+4]; '
     'S: add edx, 4; dec ecx; jnz L; jmp 0x5dcbe4'),
    (0x5f45e8, 'mov dword ptr [esp + eax*4 + 0x64], edx',
     'mov dword ptr [esp + eax*4 + 0xa4], edx'),
    (0x5f45f5, 'lea eax, [esp + 0x64]', 'lea eax, [esp + 0xa4]'),
    (0x5f46aa, 'lea edx, [esp + 0x68]', 'lea edx, [esp + 0xa8]'),
    (0x5f4794, 'lea eax, [esp + 0x68]', 'lea eax, [esp + 0xa8]'),
]


def build_records(exe):
    """Generate and validate this patch independently for regression fixtures."""
    import re
    import capstone
    import pefile
    from detour import Builder
    pe = pefile.PE(str(exe), fast_load=True)
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    builder = Builder.__new__(Builder)
    builder.insns, builder.order, builder.targets = {}, [], []
    builder.patched, builder.records = {}, []
    fields = []
    for site, old, size, expression, comment in FIELD:
        instruction = next(decoder.disasm(pe.get_data(site - pe.OPTIONAL_HEADER.ImageBase, 16), site))
        data = bytes(instruction.bytes)
        positions = [i for i in range(1, len(data)) if data[i] == old]
        assert len(positions) == 1, (hex(site), positions)
        fields.append(f'{site:08x} {positions[0]} 1 {data.hex()} {expression} ; {comment}')
    for site, original, replacement in CAVE_TEXT:
        length = 0
        for instruction in decoder.disasm(pe.get_data(site - pe.OPTIONAL_HEADER.ImageBase, 24), site):
            if not length:
                assert f'{instruction.mnemonic} {instruction.op_str}' == original, hex(site)
            builder.insns[instruction.address] = (bytes(instruction.bytes), instruction.mnemonic, instruction.op_str)
            builder.order.append(instruction.address)
            length += instruction.size
            if length >= 5:
                break
        builder.detour(site, {site: replacement}, '16 corps per force')
    # Include all native branches when checking that no detour steals a target.
    section = next(s for s in pe.sections if s.Name.startswith(b'.text'))
    decoder.skipdata = True
    targets = set()
    for address, size, mnemonic, operands in decoder.disasm_lite(
            section.get_data(), pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress):
        if mnemonic.startswith('j') and re.fullmatch(r'0x[0-9a-f]+', operands):
            targets.add(int(operands, 16))
    for record in builder.records:
        pieces = record.split()
        site, length = int(pieces[1], 16), int(pieces[2])
        assert not any(site < target < site + length for target in targets), hex(site)
    return fields, builder.records
