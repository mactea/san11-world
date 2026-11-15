"""Execute the generated x86 timer/UI caves, including relocated cave records."""
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from patch_guidance_research import SITES, cave, cave_records
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *


def run(index, months, kind, people, fid=1, address=0x31000000):
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    for at, size in [(0x400000, 0x600000), (0x10000, 0x1000),
                     (0x20000000, 0x10000), (0x21000000, 0x10000),
                     (0x22000000, 0x10000), (address, 0x1000)]:
        u.mem_map(at, size)
    def put(at, value): u.mem_write(at, struct.pack('<I', value & 0xffffffff))
    def get(at): return struct.unpack('<I', u.mem_read(at, 4))[0]
    force, research, base, ui = 0x20000000, 0x20001000, 0x20002000, 0x20003000
    put(force, 0x10000); put(0x10028, 0x10100)
    put(base, 0x10200); put(0x10240, 0x10120); put(ui+0x18c, base)
    put(research+0x48, kind); u.mem_write(research+0x60, bytes([months]))
    put(0x490b09, len(people))
    for i, (owner, status, skill) in enumerate(people):
        at = 0x21000000+i*400
        put(at, 0x10300); put(0x10340, 0x10110)
        put(at+0x90, owner); put(at+0xa0, status); put(at+0xe8, skill)
    # Reproduce the startup loader's rel32 fixup at a different cave base.
    parts = cave_records()[index].split(';')[0].split()
    code = bytearray.fromhex(parts[4])
    for fix in parts[5:]:
        offset, target = fix[1:].split('=rel:')
        struct.pack_into('<I', code, int(offset), (int(target, 16)-address-int(offset)-4)&0xffffffff)
    assert bytes(code) == cave(index, address)
    u.mem_write(address, bytes(code))
    initial = {UC_X86_REG_EAX:0x12345678, UC_X86_REG_EBX:0x543210,
               UC_X86_REG_ECX:0x87654321, UC_X86_REG_EDX:0x12341234,
               UC_X86_REG_ESI:research, UC_X86_REG_EDI:force if index==0 else ui,
               UC_X86_REG_EBP:0xabcdef, UC_X86_REG_ESP:0x22008000}
    for reg, value in initial.items(): u.reg_write(reg, value)
    result = {}
    def hook(uc, at, size, data):
        if at == SITES[index][2]:
            uc.emu_stop(); return
        if at not in (0x10100, 0x10110, 0x10120, 0x490b00, 0x490aa0, 0x4890f0, 0x471d10): return
        esp = uc.reg_read(UC_X86_REG_ESP); ecx = uc.reg_read(UC_X86_REG_ECX); cleanup = 0
        if at in (0x10100, 0x10120): value = fid
        elif at == 0x10110: value = get(ecx+0x90)
        elif at == 0x490aa0: value = force; cleanup = 4
        elif at == 0x490b00: value = 0x21000000+get(esp+4)*400; cleanup = 4
        elif at == 0x4890f0:
            assert get(esp+4) == 84
            value = int(get(ecx+0xe8) in (84, 249)); cleanup = 4
        else:
            fmt = bytes(uc.mem_read(get(esp+8), 8)).split(b'\0')[0].decode('cp950')
            result.update(days=get(esp+12), fmt=fmt); value = get(esp+4)
        uc.reg_write(UC_X86_REG_EAX, value & 0xffffffff)
        uc.reg_write(UC_X86_REG_ESP, esp+4+cleanup)
        uc.reg_write(UC_X86_REG_EIP, get(esp))
    u.hook_add(UC_HOOK_CODE, hook)
    u.emu_start(address, address+len(code), count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == SITES[index][2]
    for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
        assert u.reg_read(reg) == initial[reg]
    assert u.reg_read(UC_X86_REG_ESP) == initial[UC_X86_REG_ESP]-(12 if index else 0)
    if index == 0: return u.reg_read(UC_X86_REG_EAX)
    assert result['fmt'] == '%d天'
    return result['days']//10


def main():
    cases = 0
    for months in (3, 4, 5, 6):
        for kind in (0, 1, 2):
            for people, guidance in [([], False), ([(1, 0, 84)], True),
                ([(1, 3, 84)], True), ([(1, 2, 249)], True),
                ([(2, 2, 84)], False), ([(1, 4, 84)], False),
                ([(1, -1, 84)], False), ([(1, 8, 84)], False),
                ([(1, 2, 84), (1, 2, 84)], True),
                ([(2, 2, 84), (1, 2, 84)], True)]:
                expected = (months*3+1)//2 if kind==2 and guidance else months*3
                for index in (0, 1):
                    assert run(index, months, kind, people) == expected
                    cases += 1
    assert run(0, 6, 2, [(1, 2, 84)], fid=248) == 18
    assert run(1, 6, 2, [(248, 2, 84)], fid=248) == 9
    print(f'PASS {cases+2} native x86 timer/UI cases: owner, status, composites, no stacking, non-skill research, relocation, registers and stack')


if __name__ == '__main__': main()
