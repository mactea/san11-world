"""Long false reports must count down and validate without changing other statuses."""
import struct
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EFLAGS

import m9_sites
from detour import Builder, asm

ROOT = Path(__file__).resolve().parents[2]
CAVE, UNIT, STACK, DONE = 0x10000000, 0x20000000, 0x20001800, 0x50000000
SPANS = {
    0x496350: [(bytes.fromhex('8a442404'), 'mov', 'al, byte ptr [esp + 4]'),
               (bytes.fromhex('84c0'), 'test', 'al, al')],
    0x497211: [(bytes.fromhex('b105'), 'mov', 'cl, 5'),
               (bytes.fromhex('3aca'), 'cmp', 'cl, dl'),
               (bytes.fromhex('8b15a4ca3f07'), 'mov', 'edx, dword ptr [0x73fcaa4]')],
}


def generated_records():
    replacements = {va: text for va, old, text in m9_sites.CAVE_TEXT}
    records = []
    for site, rows in SPANS.items():
        b = Builder.__new__(Builder)
        b.insns, b.order, b.targets, b.patched, b.records = {}, [], [], {}, []
        address = site
        for raw, mnemonic, operands in rows:
            b.insns[address] = (raw, mnemonic, operands)
            b.order.append(address)
            address += len(raw)
        b.detour(site, {site: replacements[site]}, 'long false report support')
        records.append(b.records[0])
    return records


def cpu_for(site, record):
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    for page, size in [(0x496000, 0x2000), (CAVE, 0x1000), (UNIT, 0x2000), (DONE, 0x1000), (0x73fc000, 0x1000)]:
        u.mem_map(page, size)
    u.mem_write(0x496350, asm(
        'mov al, [esp+4]; test al, al; ja high; xor al, al; mov [ecx+0x28], al; ret 4; '
        'high: cmp al, 5; jb store; mov al, 5; store: mov [ecx+0x28], al; ret 4', 0x496350))
    u.mem_write(0x497211, bytes.fromhex('b1053aca8b15a4ca3f07'))
    u.mem_write(0x73fcaa4, struct.pack('<I', 0x1234beef))
    parts = record.split(';')[0].split()
    assert parts[0] == 'CAVE' and int(parts[1], 16) == site
    length = int(parts[2])
    assert bytes(u.mem_read(site, length)).hex() == parts[3]
    code = bytearray.fromhex(parts[4])
    for fix in parts[5:]:
        off, target = fix[1:].split('=rel:')
        off = int(off)
        struct.pack_into('<I', code, off, (int(target, 16)-CAVE-off-4) & 0xffffffff)
    u.mem_write(CAVE, bytes(code))
    u.mem_write(site, asm(f'jmp {CAVE}', site)+b'\x90'*(length-5))
    return u


def set_duration(cpu, status, duration):
    cpu.mem_write(UNIT+0x24, struct.pack('<I', status))
    cpu.mem_write(STACK, struct.pack('<II', DONE, duration))
    cpu.reg_write(UC_X86_REG_ECX, UNIT)
    cpu.reg_write(UC_X86_REG_EDX, 0x12345678)
    cpu.reg_write(UC_X86_REG_ESP, STACK)
    cpu.emu_start(0x496350, DONE, count=100)
    assert cpu.reg_read(UC_X86_REG_ESP) == STACK+8
    assert cpu.reg_read(UC_X86_REG_EDX) == 0x12345678
    return bytes(cpu.mem_read(UNIT+0x28, 1))[0]


class LongFalseReportTest(unittest.TestCase):
    def check_records(self, records):
        setter, validator = [cpu_for(site, record) for site, record in zip(SPANS, records)]
        for status in range(7):
            limit = 30 if status == 2 else 5
            for duration in [0, 1, 4, 5, 6, 29, 30, 31, 255]:
                with self.subTest(status=status, duration=duration):
                    self.assertEqual(set_duration(setter, status, duration), min(duration, limit))
                    validator.mem_write(UNIT+0x24, struct.pack('<I', status))
                    validator.reg_write(UC_X86_REG_EBX, UNIT)
                    validator.reg_write(UC_X86_REG_ECX, 0xaabbcc00)
                    validator.reg_write(UC_X86_REG_EDX, duration)
                    validator.reg_write(UC_X86_REG_ESP, STACK)
                    validator.emu_start(0x497211, 0x49721b, count=100)
                    self.assertEqual(not (validator.reg_read(UC_X86_REG_EFLAGS) & 1), duration <= limit)
                    self.assertEqual(validator.reg_read(UC_X86_REG_EDX), 0x1234beef)
                    self.assertEqual(validator.reg_read(UC_X86_REG_ECX) >> 8, 0xaabbcc)
        for remaining in range(30, 0, -1):
            self.assertEqual(set_duration(setter, 2, remaining-1), remaining-1)

    def test_generator(self):
        self.check_records(generated_records())

    def test_shipped(self):
        lines = (ROOT/'worldmod/worldmod_bases_caves.txt').read_text(encoding='utf-8').splitlines()
        rows = []
        for site in SPANS:
            matches = [x for x in lines if x.startswith(f'CAVE {site:08x} ')]
            self.assertEqual(len(matches), 1)
            rows.append(matches[0])
        self.check_records(rows)


if __name__ == '__main__':
    unittest.main()
