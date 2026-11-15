"""Expose the native sortie command in player delegated bases' military menu."""

# The native first corps already receives items 13..15 (training/sortie/transport).
# The fallback at this site receives only item 15. Expand it to 14..15 for a
# player-owned delegated corps; item 14 keeps its native availability and handler.
CAVE_TEXT = [(0x61791b, 'mov dword ptr [ebx], 0xf',
    'mov dword ptr [ebx], 15; pushfd; pushad; '
    'push esi; call 0x47a630; add esp, 4; test eax, eax; jz E; '
    'cmp dword ptr [esi+8], 2; jl E; '
    'cmp dword ptr [esi+8], 16; jg E; '
    'mov ecx, esi; mov eax, [ecx]; call dword ptr [eax+0x40]; '
    'push eax; mov ecx, 0x7201958; call 0x490aa0; '
    'mov esi, eax; push eax; call 0x47a630; add esp, 4; '
    'test eax, eax; jz E; mov ecx, esi; mov eax, [ecx]; '
    'call dword ptr [eax+0x48]; test eax, eax; jz E; '
    'mov dword ptr [ebx], 14; E: popad; popfd')]


def build_records(exe):
    import capstone
    import pefile
    from detour import Builder

    pe = pefile.PE(str(exe), fast_load=True)
    builder = Builder.__new__(Builder)
    builder.insns, builder.order, builder.targets = {}, [], []
    builder.patched, builder.records = {}, []
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for site, original, replacement in CAVE_TEXT:
        raw = pe.get_data(site - pe.OPTIONAL_HEADER.ImageBase, 16)
        size = 0
        for insn in decoder.disasm(raw, site):
            if not size:
                assert f'{insn.mnemonic} {insn.op_str}' == original
            builder.insns[insn.address] = (bytes(insn.bytes), insn.mnemonic, insn.op_str)
            builder.order.append(insn.address)
            size += len(insn.bytes)
            if size >= 5:
                break
        builder.detour(site, {site: replacement}, 'Player delegated military menu: native sortie')
    return builder.records
