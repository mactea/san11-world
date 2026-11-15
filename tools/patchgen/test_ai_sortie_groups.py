"""Exercise the native sortie quota branch with the generated and shipped patch."""
import struct
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS

import m8_sites
from detour import asm

SITE, PASS, REJECT = 0x5efa34, 0x5efa46, 0x5f0097
OLD = bytes.fromhex('8d541201')
ROOT = Path(__file__).resolve().parents[2]


def run(quota, reduced=False, existing=False, replacement=None):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    for page in [0x5ef000, 0x5f0000, 0x20000000]:
        cpu.mem_map(page, 4096)
    cpu.mem_write(0x5efa23, asm(
        'mov ecx, [esp+0x3c]; mov eax, [esp+0x20]; xor edx, edx; '
        'test ecx, ecx; sete dl; test eax, eax; lea edx, [edx+edx+1]; '
        f'mov esi, edx; jne {PASS}; cmp [esp+0x24], esi; jl {REJECT}', 0x5efa23))
    assert bytes(cpu.mem_read(SITE, 4)) == OLD
    if replacement is not None:
        assert len(replacement) == 4
        cpu.mem_write(SITE, replacement)
    stack = 0x20000800
    for offset, value in [(0x3c, int(reduced)), (0x20, int(existing)), (0x24, quota)]:
        cpu.mem_write(stack+offset, struct.pack('<I', value))
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.reg_write(UC_X86_REG_EFLAGS, 0x202)
    flags = []

    def observe(uc, address, size, data):
        if address in [SITE, SITE+4]:
            flags.append(uc.reg_read(UC_X86_REG_EFLAGS))
        if address in [PASS, REJECT]:
            uc.emu_stop()

    cpu.hook_add(UC_HOOK_CODE, observe)
    cpu.emu_start(0x5efa23, REJECT+1, count=30)
    assert len(flags) == 2 and flags[0] == flags[1], flags
    assert cpu.reg_read(UC_X86_REG_ESP) == stack
    assert cpu.reg_read(UC_X86_REG_EAX) == int(existing)
    assert cpu.reg_read(UC_X86_REG_ECX) == int(reduced)
    return cpu.reg_read(UC_X86_REG_EIP) == PASS, cpu.reg_read(UC_X86_REG_ESI)


class AISortieGroupsTest(unittest.TestCase):
    def test_original_rejects_observed_two_group_quota(self):
        self.assertEqual(run(2), (False, 3))

    def check_cases(self, replacement):
        for reduced in [False, True]:
            minimum = 1 if reduced else 2
            for existing in [False, True]:
                for quota in range(5):
                    with self.subTest(reduced=reduced, existing=existing, quota=quota):
                        self.assertEqual(run(quota, reduced, existing, replacement),
                                         (existing or quota >= minimum, minimum))

    def test_generator(self):
        rows = [row for row in m8_sites.REWRITE if row[0] == SITE]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][1], 4)
        self.check_cases(asm(rows[0][2], SITE))

    def test_shipped_patch(self):
        rows = [row.split(';')[0].split() for row in
                (ROOT/'worldmod/worldmod_bases_caves.txt').read_text(encoding='utf-8').splitlines()
                if row.startswith('REWRITE 005efa34 ')]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][2:4], ['4', OLD.hex()])
        self.check_cases(bytes.fromhex(rows[0][4]))


if __name__ == '__main__':
    unittest.main()
