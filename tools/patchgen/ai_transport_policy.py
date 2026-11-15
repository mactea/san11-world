"""Rear-city transport policy; frontline calculations remain native.

Runtime base records precede the expanded AIRT tail. All accesses to that tail
must include (N-87)*0x34. Hooks are restricted to transport callers, not the
shared combat formation, pathfinding or domestic-readiness helpers.
"""

CAVE_TEXT = [
    # Native safe-rear demand is 3, not zero. Keep the shared demand record
    # intact: only ordinary transport may use its officers and 3,000 troop floor.
    # Unsigned comparisons leave invalid negative values on the native path.
    (0x5f0d62, 'cmp cx, word ptr [edi + 0x4a]',
     'cmp word ptr [edi+0x4a], 3; ja F; cmp cx, 0; jmp D; '
     'F: cmp cx, word ptr [edi+0x4a]; D: nop'),
    (0x5f0d7b, 'movsx edx, word ptr [edi + 0x4a]',
     'xor edx, edx; cmp word ptr [edi+0x4a], 3; jbe D; '
     'movsx edx, word ptr [edi+0x4a]; D: nop'),

    # Give safe rear redistribution the 3,000 floor without changing frontline N.
    (0x5f0de5, 'imul edi, edi, 0x1d4c',
     'cmp edi, 3; ja F; mov edi, 3000; jmp D; '
     'F: imul edi, edi, 0x1d4c; D: nop'),

    # City -> affiliated port transfers carry 3,000. Rear sources therefore
    # need 6,000 before using that earlier branch of the dispatcher.
    (0x5f4c23, 'cmp eax, 0xbb8',
     'push ecx; push edx; push eax; push edi; mov ecx, 0x7201958; call 0x491770; '
     'imul eax, eax, 0x34; cmp word ptr [eax+esi+0x4a], 3; ja F; '
     'pop eax; pop edx; pop ecx; cmp eax, 6000; jmp D; '
     'F: pop eax; pop edx; pop ecx; cmp eax, 3000; D: nop'),

    # Respect the player's permission before relief / port / ordinary dispatch,
    # rather than only in the last of those branches. Helper returns 0 for AI.
    (0x5f4e8e, 'mov eax, dword ptr [esi + 0x32a0]',
     'pushad; push 10; push edi; mov ecx, 0x9a916c4; call 0x5f65d0; '
     'test eax, eax; popad; jnz 0x5f4f0a; '
     'mov eax, dword ptr [esi+{0x32a0+(N-87)*0x34}]'),

    # For rear candidates, follow a valid same-corps configured destination.
    # Each local relay must strictly reduce distance, so it cannot bounce back.
    # Without a valid configured city, retain the native frontline gradient.
    (0x5e4e1e, 'mov al, byte ptr [ebx + 0x20]',
     'pushad; sub esp, 16; cmp dword ptr [esp+0x4c], 0x5f0b33; jne O; '
     'cmp word ptr [ebx+0x4a], 3; ja O; '
     'push dword ptr [esp+0x54]; mov ecx, 0x7201958; call 0x490ad0; '
     'test eax, eax; jz O; mov eax, dword ptr [eax+0x20]; '
     'test eax, eax; js O; cmp eax, {C}; jge O; mov dword ptr [esp+4], eax; '
     'push eax; mov ecx, 0x7201958; call 0x490d00; test eax, eax; jz O; '
     'mov ecx, eax; mov edx, dword ptr [eax]; call dword ptr [edx+0x44]; '
     'cmp eax, dword ptr [esp+0x54]; jne O; '
     'mov esi, dword ptr [esp+0x40]; mov ecx, dword ptr [esi+8]; push 0x11; '
     'call 0x4811e0; test eax, eax; mov eax, 0x4795c0; jz M; '
     'mov eax, 0x4795f0; M: mov dword ptr [esp+8], eax; '
     'mov eax, ebx; sub eax, esi; xor edx, edx; mov ecx, 0x34; div ecx; '
     'push dword ptr [esp+4]; push eax; call dword ptr [esp+16]; add esp, 8; '
     'test eax, eax; js O; cmp eax, 255; jge O; mov dword ptr [esp], eax; '
     'push dword ptr [esp+4]; push edi; call dword ptr [esp+16]; add esp, 8; '
     'test eax, eax; js R; cmp eax, dword ptr [esp]; jge R; '
     'add esp, 16; popad; jmp 0x5e4e2e; '
     'R: add esp, 16; popad; jmp 0x5e4e38; '
     'O: add esp, 16; popad; mov al, byte ptr [ebx+0x20]'),

    # A rear relay can receive soldiers even without the native supply flag.
    # Earlier filters still enforce reachability, corps and safe target state.
    (0x5f0bc1, 'mov ecx, ebp',
     'pushad; mov eax, dword ptr [esp+0x3c]; imul eax, eax, 0x34; '
     'cmp word ptr [eax+esi+0x4a], 3; ja O; '
     'test ebp, ebp; js O; cmp ebp, {C}; jge O; '
     'mov ecx, edi; call 0x486bc0; imul eax, eax, 9; xor edx, edx; '
     'mov ecx, 10; div ecx; mov ebp, eax; mov ecx, edi; '
     'mov edx, dword ptr [edi]; call dword ptr [edx+0x50]; '
     'sub ebp, eax; cmp ebp, 1000; jl R; '
     'popad; push edi; mov ecx, 0x99c7848; call 0x491f50; jmp 0x5f0c1c; '
     'R: popad; jmp 0x5f0c1c; O: popad; mov ecx, ebp'),

    # Rear shipments may carry whatever gold is available, including zero.
    (0x5f0e29, 'imul eax, eax, 0x190',
     'cmp dword ptr [esp+0x50], 3000; jne F; xor eax, eax; jmp D; '
     'F: imul eax, eax, 0x190; D: nop'),
    (0x5f0fb0, 'cmp ebp, 0x190',
     'cmp dword ptr [esp+0x50], 3000; jne F; cmp ebp, 0; jmp D; '
     'F: cmp ebp, 0x190; D: nop'),

    # Dedicated rear transport template: first currently eligible officer,
    # no required weapon cargo, >= 1,000 surplus, <= 60,000 per shipment.
    # Clamp before native post-checks to reserve, food and target headroom.
    # At this call site the two arguments are already on the stack, but no
    # return address has been pushed. Native and rear paths both consume 8.
    (0x5f0ee0, 'call 0x5e0760',
     'cmp dword ptr [esp+0x58], 3000; jne F; '
     'push ebp; push ebx; push edi; mov edi, dword ptr [esp+12]; '
     'mov ecx, dword ptr [esp+16]; mov eax, dword ptr [ecx+4]; '
     'test eax, eax; jz R; push eax; call 0x49f0d0; mov ebx, dword ptr [eax]; '
     'test ebx, ebx; jz R; '
     'mov ecx, dword ptr [esp+0x110]; mov edx, dword ptr [ecx]; '
     'call dword ptr [edx+0x50]; sub eax, 3000; mov ebp, eax; '
     'cmp ebp, 60000; jle A; mov ebp, 60000; A: '
     'mov eax, dword ptr [esi+{0x3290+(N-87)*0x34}]; '
     'sub eax, dword ptr [esp+0x6c]; sar eax, 1; '
     'cmp ebp, eax; jle B; mov ebp, eax; B: '
     'mov ecx, dword ptr [esp+0x24]; call 0x486bc0; imul eax, eax, 9; '
     'xor edx, edx; mov ecx, 10; div ecx; push eax; '
     'mov ecx, dword ptr [esp+0x28]; mov edx, dword ptr [ecx]; '
     'call dword ptr [edx+0x50]; pop edx; sub edx, eax; sub edx, dword ptr [esp+0x28]; '
     'cmp ebp, edx; jle C; mov ebp, edx; C: cmp ebp, 1000; jl R; '
     'mov eax, dword ptr [esp+0x110]; mov dword ptr [edi], eax; '
     'push ebx; mov ecx, 0x7201958; call 0x491310; '
     'mov dword ptr [edi+8], eax; mov dword ptr [edi+0xc], -1; '
     'mov dword ptr [edi+0x10], -1; mov dword ptr [edi+0x1c], ebp; '
     'mov dword ptr [edi+0x24], 9; mov dword ptr [edi+0x28], -1; '
     'mov ebx, 1; W: push ebx; mov ecx, dword ptr [esp+0x114]; call 0x486950; '
     'cmp eax, ebp; jle X; mov eax, ebp; X: cmp eax, dword ptr [edi+0x50]; jle Y; '
     'mov dword ptr [edi+0x20], ebx; mov dword ptr [edi+0x50], eax; '
     'Y: inc ebx; cmp ebx, 4; jle W; '
     'mov eax, 1; jmp D; R: xor eax, eax; '
     'D: pop edi; pop ebx; pop ebp; add esp, 8; jmp E; '
     'F: call 0x5e0760; E: nop'),
]

REWRITE = [
    # A valid transport requires soldiers; weapon cargo is optional. Native
    # frontline groups already have both, so their behavior is unchanged.
    (0x5f0eed, 4, 'mov eax, dword ptr [esp+0x78]',
     'AI transport: validate troop count instead of required weapon type'),
]


def build_records(exe):
    """Compile these hooks independently for testing / incremental deployment.

    Full builds consume the same definitions through m8_sites / gen_m7.
    """
    import capstone
    import pefile
    from detour import Builder, asm

    pe = pefile.PE(str(exe))
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    builder = Builder.__new__(Builder)
    builder.insns, builder.order, builder.targets = {}, [], []
    builder.patched, builder.records = {}, []
    for site, original, replacement in CAVE_TEXT:
        size = 0
        for insn in decoder.disasm(pe.get_data(site - pe.OPTIONAL_HEADER.ImageBase, 32), site):
            if not size:
                assert f'{insn.mnemonic} {insn.op_str}' == original, hex(site)
            builder.insns[insn.address] = (bytes(insn.bytes), insn.mnemonic, insn.op_str)
            builder.order.append(insn.address)
            size += len(insn.bytes)
            if size >= 5:
                break
        builder.detour(site, {site: replacement}, 'AI rear transport policy')
    for site, size, replacement, comment in REWRITE:
        old = pe.get_data(site - pe.OPTIONAL_HEADER.ImageBase, size)
        new = asm(replacement, site)
        assert len(new) == size
        builder.records.append(f'REWRITE {site:08x} {size} {old.hex()} {new.hex()} ; {comment}')
    return builder.records


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', help='Original san11pk.exe, read only')
    print('\n'.join(build_records(parser.parse_args().exe)))
