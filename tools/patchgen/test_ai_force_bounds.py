"""AI target selection must bound force ids by R, independently of city count C.

Run: python tools/patchgen/test_ai_force_bounds.py
Requires the patch compiler's keystone-engine and the Unicorn x86 emulator.
"""
import struct
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP

import m8_sites
from detour import Builder, asm


SITE = 0x5e77f7
CAVE = 0x10000000
ROOT = Path(__file__).resolve().parents[2]


def generated_record():
    replacements = {va: (old, new) for va, old, new in m8_sites.CAVE_TEXT}
    old, replacement = replacements[SITE]
    assert old == 'call 0x4882b0'
    builder = Builder.__new__(Builder)
    builder.insns = {SITE: (asm(old, SITE), 'call', '0x4882b0')}
    builder.order = [SITE]
    builder.targets = []
    builder.patched = {}
    builder.records = []
    builder.detour(SITE, {SITE: replacement}, 'AI target force bound')
    return builder.records[0]


def run_target(city_count, force_count, force_id, record=None):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    for page in (0x488000, 0x5e7000, CAVE, 0x20000000):
        cpu.mem_map(page, 0x1000)
    # The shared is_city helper remains bounded by C for its other callers.
    cpu.mem_write(0x4882b0, asm(
        f'mov eax, [esp+4]; cmp eax, {city_count}; setb al; movzx eax, al; ret', 0x4882b0))
    cpu.mem_write(SITE - 1, asm(
        'push esi; call 0x4882b0; add esp, 4; test eax, eax; je 0x5e7a1a', SITE - 1))
    if record:
        parts = record.split(';')[0].split()
        assert parts[:4] == ['CAVE', '005e77f7', '5', 'e8b40aeaff']
        code = bytearray.fromhex(parts[4])
        for fix in parts[5:]:
            offset, value = fix[1:].split('=')
            kind, expression = value.split(':')
            offset = int(offset)
            if kind == 'abs':
                assert expression == 'R'
                value = force_count
            else:
                assert kind == 'rel'
                value = int(expression, 16) - (CAVE + offset + 4)
            struct.pack_into('<I', code, offset, value & 0xffffffff)
        cpu.mem_write(CAVE, bytes(code))
        cpu.mem_write(SITE, asm(f'jmp {CAVE}', SITE))
    cpu.reg_write(UC_X86_REG_ESI, force_id & 0xffffffff)
    cpu.reg_write(UC_X86_REG_ESP, 0x20000800)
    # Both paths stop before the target's regular-force AI record could be read.
    cpu.hook_add(UC_HOOK_CODE, lambda uc, address, size, data: uc.emu_stop()
                 if address == 0x5e7a1a else None)
    cpu.emu_start(SITE - 1, 0x5e7807, count=40)
    assert cpu.reg_read(UC_X86_REG_EIP) in (0x5e7807, 0x5e7a1a)
    accepted = cpu.reg_read(UC_X86_REG_EIP) == 0x5e7807
    assert cpu.reg_read(UC_X86_REG_ESP) == 0x20000800
    assert cpu.reg_read(UC_X86_REG_ESI) == force_id & 0xffffffff
    return accepted


class AITargetForceBoundsTest(unittest.TestCase):
    def test_reproduces_logged_bandit_overflow(self):
        # ESI=0xd184 in the crash is force 253 * the 0xd4-byte record size.
        self.assertEqual(253 * 0xd4, 0xd184)
        self.assertTrue(run_target(530, 249, 253))

    def check_bounds(self, record):
        for cities, forces in ((42, 42), (530, 42), (530, 249), (1000, 249), (100, 249)):
            ids = (-1, 0, forces - 1, *range(forces, forces + 5), 255, 0x7fffffff, -0x80000000)
            for force_id in ids:
                with self.subTest(cities=cities, forces=forces, force_id=force_id):
                    self.assertEqual(run_target(cities, forces, force_id, record), 0 <= force_id < forces)

    def test_compiled_source(self):
        self.check_bounds(generated_record())

    def test_shipped_patch(self):
        rows = (ROOT / 'worldmod/worldmod_bases_caves.txt').read_text(encoding='utf-8').splitlines()
        records = [row for row in rows if row.startswith('CAVE 005e77f7 ')]
        self.assertEqual(len(records), 1)
        self.check_bounds(records[0])


if __name__ == '__main__':
    unittest.main()
