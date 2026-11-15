"""Native corps-menu companion loader and delegated governor AP contribution."""


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
     'N: call 0x4890b0')]


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
        comment = ('Native corps governor appointment menu companion' if site == 0x6034a0 else
                   'Delegated governor AP: leadership 60% + politics 40% + charm 40%, minimum 6')
        builder.detour(site, {site: replacement}, comment)
    return builder.records


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe')
    print('\n'.join(build_records(parser.parse_args().exe)))
