"""Emulate the nearby-enemy callback, including its cdecl stack contract."""
import struct
import unittest
from pathlib import Path

from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_ESP

import m9_sites
from detour import Builder, asm

SITE, CAVE, STACK, DATA = 0x4b997e, 0x10000000, 0x20000000, 0x30000000
ROOT = Path(__file__).resolve().parents[2]


def generated_record():
    replacement = {va: text for va, old, text in m9_sites.CAVE_TEXT}[SITE]
    b = Builder.__new__(Builder)
    b.insns = {
        SITE: (bytes.fromhex('8b4c2410'), 'mov', 'ecx, dword ptr [esp + 0x10]'),
        SITE + 4: (bytes.fromhex('8b11'), 'mov', 'edx, dword ptr [ecx]'),
    }
    b.order = [SITE, SITE + 4]
    b.targets, b.patched, b.records = [], {}, []
    b.detour(SITE, {SITE: replacement}, 'AI nearby-unit null guard')
    return b.records[0]


def run_callback(unit_id, kind=1, hostile=1, record=None):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    for page in (0x4b9000, 0x4b5000, 0x490000, CAVE, STACK, DATA):
        cpu.mem_map(page, 0x1000)
    # Instructions of 0x4b9960, with the same addresses and calling conventions.
    text = ('mov eax, [esp+4]; mov ecx, [eax]; and cl, 3; cmp cl, 1; jne no; '
            'movzx eax, word ptr [eax+8]; push esi; push eax; mov ecx, 0x7201958; call 0x490e70; '
            'mov ecx, [esp+0x10]; mov edx, [ecx]; mov esi, eax; call [edx+0x40]; push eax; '
            'mov eax, [esi]; mov ecx, esi; call [eax+0x40]; push eax; mov ecx, 0x799895c; '
            'call 0x4b5cc0; test eax, eax; pop esi; je no; mov eax, 1; ret; no: xor eax, eax; ret')
    original = asm(text, 0x4b9960)
    assert original[SITE - 0x4b9960:SITE - 0x4b9960 + 6].hex() == '8b4c24108b11'
    cpu.mem_write(0x4b9960, original)
    cpu.mem_write(0x490e70, asm(
        f'mov eax, [esp+4]; cmp eax, 1000; jae no; mov eax, {DATA+0x200}; ret 4; no: xor eax, eax; ret 4', 0x490e70))
    cpu.mem_write(0x4b5cc0, asm(f'mov eax, {hostile}; ret 8', 0x4b5cc0))
    cpu.mem_write(DATA+0x400, asm('mov eax, [ecx+4]; ret', DATA+0x400))
    cpu.mem_write(DATA+0x40, struct.pack('<I', DATA+0x400))
    cpu.mem_write(DATA+0x100, struct.pack('<I4xH', kind, unit_id))
    cpu.mem_write(DATA+0x200, struct.pack('<II', DATA, 253))
    cpu.mem_write(DATA+0x300, struct.pack('<II', DATA, 11))
    if record:
        parts = record.split(';')[0].split()
        assert parts[:4] == ['CAVE', '004b997e', '6', '8b4c24108b11']
        code = bytearray.fromhex(parts[4])
        for fix in parts[5:]:
            off, expr = fix[1:].split('=rel:')
            off = int(off)
            struct.pack_into('<I', code, off, (int(expr, 16) - CAVE - off - 4) & 0xffffffff)
        cpu.mem_write(CAVE, bytes(code))
        cpu.mem_write(SITE, asm(f'jmp {CAVE}; nop', SITE))
    sp = STACK + 0x800
    cpu.mem_write(sp, struct.pack('<5I', STACK, DATA+0x100, 0, DATA+0x300, 0))
    cpu.reg_write(UC_X86_REG_ESP, sp)
    cpu.reg_write(UC_X86_REG_ESI, 0x12345678)
    cpu.emu_start(0x4b9960, STACK, count=100)
    assert cpu.reg_read(UC_X86_REG_ESP) == sp + 4
    assert cpu.reg_read(UC_X86_REG_ESI) == 0x12345678
    return cpu.reg_read(UC_X86_REG_EAX)


class NearbyUnitTest(unittest.TestCase):
    def test_original_null_dereference(self):
        with self.assertRaises(UcError):
            run_callback(0xffff)

    def check_callback(self, record):
        for kind in (0, 1, 2, 3, 5):
            for uid in (0, 85, 999, 1000, 0xffff):
                for hostile in (0, 1):
                    with self.subTest(kind=kind, unit=uid, hostile=hostile):
                        self.assertEqual(run_callback(uid, kind, hostile, record),
                                         int((kind & 3) == 1 and uid < 1000 and hostile))

    def test_generated_guard(self):
        self.check_callback(generated_record())

    def test_shipped_guard(self):
        rows = (ROOT/'worldmod/worldmod_bases_caves.txt').read_text(encoding='utf-8').splitlines()
        records = [r for r in rows if r.startswith('CAVE 004b997e ')]
        self.assertEqual(len(records), 1)
        self.check_callback(records[0])


if __name__ == '__main__':
    unittest.main()
