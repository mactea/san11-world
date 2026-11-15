"""Native corps appointments, governor retention and delegated AP contribution."""


def _stack_string(text, offset):
    data = text.encode('ascii') + b'\0'
    data = data.ljust(20, b'\0')
    assert len(data) == 20
    return '; '.join(
        f'mov dword ptr [esp+{offset + n}], {int.from_bytes(data[n:n + 4], "little")}'
        for n in range(0, 20, 4))


CAVE_TEXT = [(0x6034a0, 'push -1',
    'pushfd; pushad; sub esp, 40; '
    + _stack_string('worldmod_corps.dll', 0) + '; '
    + _stack_string('WorldmodCorpsInit', 20) + '; '
    'mov eax, esp; push eax; call dword ptr [0x74e2f8]; '
    'test eax, eax; jz E; lea ecx, [esp+20]; push ecx; push eax; '
    'call dword ptr [0x74e2fc]; test eax, eax; jz E; '
    'push 0; call eax; '
    'E: add esp, 40; popad; popfd; push -1'),
    # ESI is the validated governor, EBP the corps, and EDI the force's advisor.
    # Main corps retain the native ruler formula. Weighted integer arithmetic
    # implements floor((0.6 * leadership + 0.4 * politics + 0.4 * charm) / 5)
    # without truncating the composite value before division.
    (0x598700, 'call 0x4890b0',
     'cmp dword ptr [ebp+8], 1; je N; '
     'movzx eax, byte ptr [esi+0x170]; imul eax, eax, 6; '
     'movzx ecx, byte ptr [esi+0x173]; lea eax, [eax+ecx*4]; '
     'movzx ecx, byte ptr [esi+0x174]; lea eax, [eax+ecx*4]; '
     'xor edx, edx; mov ecx, 50; div ecx; cmp eax, 6; jge A; '
     'mov eax, 6; A: mov esi, eax; jmp 0x598774; '
     'N: call 0x4890b0'),
    # Native personnel refresh always sorts the corps and replaces its governor.
    # Keep a player delegated corps' incumbent while still serving in that corps.
    # Read the existing saved governor ID, so this also works before opening the
    # appointment dialog and requires no new save fields or per-session registry.
    # ESI = corps, EBX = incumbent, [EAX] = native replacement candidate.
    (0x4be5b0, 'mov ebp, dword ptr [eax]',
     'mov ebp, [eax]; pushfd; pushad; '
     'cmp dword ptr [esi+8], 2; jl E; '
     'cmp dword ptr [esi+8], 16; jg E; '
     'push ebx; call 0x47a630; add esp, 4; test eax, eax; jz E; '
     'cmp dword ptr [ebx+0xa0], 1; jl E; '
     'cmp dword ptr [ebx+0xa0], 3; jg E; '
     'push esi; mov ecx, 0x7201958; call 0x4912c0; '
     'cmp dword ptr [ebx+0x94], eax; jne E; '
     'mov ecx, esi; mov eax, [ecx]; call dword ptr [eax+0x40]; '
     'push eax; mov ecx, 0x7201958; call 0x490aa0; '
     'mov edi, eax; push eax; call 0x47a630; add esp, 4; '
     'test eax, eax; jz E; mov ecx, edi; mov eax, [ecx]; '
     'call dword ptr [eax+0x48]; test eax, eax; jz E; '
     'mov dword ptr [esp+8], ebx; E: popad; popfd')]


def build_records(exe):
    import capstone
    import pefile
    from detour import Builder
    pe = pefile.PE(str(exe))
    builder = Builder.__new__(Builder)
    builder.insns, builder.order, builder.targets = {}, [], []
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    builder.patched, builder.records = {}, []
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
        comment = {
            0x6034a0: 'Native corps governor appointment menu companion',
            0x598700: 'Delegated governor AP: leadership 60% + politics 40% + charm 40%, minimum 6',
            0x4be5b0: 'Player delegated governor: retain eligible incumbent during personnel refresh',
        }[site]
        builder.detour(site, {site: replacement}, comment)
    return builder.records


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe')
    print('\n'.join(build_records(parser.parse_args().exe)))
