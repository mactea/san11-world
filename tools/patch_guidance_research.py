"""Guidance halves skill research; native ten-day timer rounds upward."""
from keystone import Ks, KS_ARCH_X86, KS_MODE_32
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

SITES = [(0x5cecc4, bytes.fromhex('8a4660b103f6e9'), 0x5ceccb),
         (0x60b224, bytes.fromhex('0fbe5660528d84249000000068d4cb7e0050e8d56ae6ff'), 0x60b23b)]
CAVE = 0x30000000


def cave(index, address=CAVE):
    # Native methods preserve EBX/ESI/EDI/EBP. The officer limit is read from
    # the relocated native getter, so expanded and ordinary rosters both work.
    helper = '''
duration:
    push ebx
    push esi
    push edi
    push ebp
    mov esi, edx
    movzx ebp, byte ptr [esi+0x60]
    imul ebp, ebp, 3
    cmp dword ptr [esi+0x48], 2
    jne done
    test ecx, ecx
    jz done
    mov eax, [ecx]
    call dword ptr [eax+0x28]
    mov edi, eax
    test edi, edi
    js done
    xor ebx, ebx
next_person:
    cmp ebx, dword ptr [0x490b09]
    jge done
    push ebx
    mov ecx, 0x7201958
    mov eax, 0x490b00
    call eax
    test eax, eax
    jz advance
    mov esi, eax
    cmp dword ptr [esi+0xa0], 0
    jl advance
    cmp dword ptr [esi+0xa0], 3
    jg advance
    mov ecx, esi
    mov eax, [ecx]
    call dword ptr [eax+0x40]
    cmp eax, edi
    jne advance
    push 84
    mov ecx, esi
    mov eax, 0x4890f0
    call eax
    test eax, eax
    jnz halve
advance:
    inc ebx
    jmp next_person
halve:
    inc ebp
    shr ebp, 1
done:
    mov eax, ebp
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
'''
    if index == 0:
        body = '''
    pushfd
    pushad
    mov ecx, edi
    mov edx, esi
    call duration
    mov [esp+28], eax
    popad
    popfd
    jmp 0x5ceccb
'''
    else:
        # The ability tree owns a base pointer at +18c. Obtain its actual
        # force rather than assuming the player/current turn's force.
        body = '''
    pushfd
    pushad
    mov ecx, [edi+0x18c]
    mov eax, [ecx]
    call dword ptr [eax+0x40]
    push eax
    mov ecx, 0x7201958
    mov eax, 0x490aa0
    call eax
    mov ecx, eax
    mov edx, esi
    call duration
    imul eax, eax, 10
    mov [esp+20], eax
    popad
    popfd
    push edx
    lea eax, [esp+0x90]
    call format_pointer
format_pointer:
    pop ecx
    add ecx, FORMAT_DELTA
    push ecx
    push eax
    mov eax, 0x471d10
    call eax
    jmp 0x60b23b
'''
    ks = Ks(KS_ARCH_X86, KS_MODE_32)
    # Local format address is derived from EIP and survives cave relocation.
    asm = body.replace('FORMAT_DELTA', '0x11223344') + helper
    code = bytes(ks.asm(asm, address)[0])
    if index == 1:
        instructions = list(Cs(CS_ARCH_X86, CS_MODE_32).disasm(code, address))
        pop = next(x.address for x in instructions if x.mnemonic == 'pop' and x.op_str == 'ecx')
        import struct
        code = code.replace(struct.pack('<I', 0x11223344), struct.pack('<I', address + len(code) - pop))
        code += '%d天'.encode('cp950') + b'\0'
    return code


def cave_records():
    records = []
    for index, (site, old, resume) in enumerate(SITES):
        code = cave(index)
        jump = next(x for x in Cs(CS_ARCH_X86, CS_MODE_32).disasm(code, CAVE)
                    if x.mnemonic == 'jmp' and x.op_str == hex(resume))
        offset = jump.address - CAVE + 1
        records.append(f'CAVE {site:08x} {len(old)} {old.hex()} {code.hex()} '
                       f'@{offset}=rel:{resume:x} ; Guidance halves skill research, no stacking')
    return records
