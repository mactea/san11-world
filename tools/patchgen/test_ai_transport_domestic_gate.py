"""Ordinary transport may proceed before the city's domestic plan is complete."""
import struct
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP,
)

import m8_sites
from detour import asm


ROOT = Path(__file__).resolve().parents[2]
SITE, CONTINUE, REJECT, DISABLED = 0x5f0aeb, 0x5f0aed, 0x5f0b47, 0x5f0ac3
OLD = bytes.fromhex('745a')


def run(domestic_ready, player=True, transport_enabled=True, replacement=None):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    for page in (0x5f0000, 0x5f6000, 0x5f7000, 0x20000000):
        cpu.mem_map(page, 4096)
    # Native permission check and city validation immediately before the gate.
    cpu.mem_write(0x5f0aae, asm(
        'test eax, eax; je 0x5f0acd; push 0xa; push ebp; '
        'mov ecx, 0x9a916c4; call 0x5f65d0; test eax, eax; je 0x5f0acd; '
        'mov eax, 1; jmp 0x5f1189; '
        'push edi; or edi, -1; test ebx, ebx; jl 0x5f0adc; '
        'cmp ebx, 0x29; mov eax, ebx; jle 0x5f0ade; mov eax, edi; '
        'push eax; mov ecx, 0x9a916c4; call 0x5f7070; '
        'test eax, eax; je 0x5f0b47', 0x5f0aae))
    assert bytes(cpu.mem_read(SITE, 2)) == OLD
    cpu.mem_write(0x5f65d0, asm(f'mov eax, {int(not transport_enabled)}; ret 8', 0x5f65d0))
    cpu.mem_write(0x5f7070, asm(f'mov eax, {int(domestic_ready)}; ret 4', 0x5f7070))
    if replacement is not None:
        assert len(replacement) == 2
        cpu.mem_write(SITE, replacement)
    stack = 0x20000800
    initial = {UC_X86_REG_EAX: int(player), UC_X86_REG_EBP: 0x12345678,
               UC_X86_REG_EBX: 14, UC_X86_REG_ECX: 0x9a916c4,
               UC_X86_REG_EDI: 0x87654321, UC_X86_REG_ESI: 0x23456789,
               UC_X86_REG_ESP: stack}
    for register, value in initial.items():
        cpu.reg_write(register, value)
    calls = []

    def observe(uc, address, size, data):
        if address == 0x5f7070:
            calls.append(struct.unpack('<I', bytes(uc.mem_read(uc.reg_read(UC_X86_REG_ESP) + 4, 4)))[0])
        if address in (CONTINUE, REJECT, DISABLED):
            uc.emu_stop()

    cpu.hook_add(UC_HOOK_CODE, observe)
    cpu.emu_start(0x5f0aae, 0x5f0b48, count=50)
    result = cpu.reg_read(UC_X86_REG_EIP)
    assert result in (CONTINUE, REJECT, DISABLED)
    assert cpu.reg_read(UC_X86_REG_ESP) == stack - (0 if result == DISABLED else 4)
    for register in (UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ESI):
        assert cpu.reg_read(register) == initial[register]
    assert calls == ([] if result == DISABLED else [14])
    return result


class AITransportDomesticGateTest(unittest.TestCase):
    def test_original_blocks_incomplete_city(self):
        self.assertEqual(run(False), REJECT)
        self.assertEqual(run(True), CONTINUE)

    def check_patch(self, replacement):
        for player in (False, True):
            for ready in (False, True):
                with self.subTest(player=player, domestic_ready=ready):
                    self.assertEqual(run(ready, player=player, replacement=replacement), CONTINUE)
        for ready in (False, True):
            with self.subTest(transport_enabled=False, domestic_ready=ready):
                self.assertEqual(run(ready, transport_enabled=False, replacement=replacement), DISABLED)

    def test_generator(self):
        rows = [r for r in m8_sites.REWRITE if r[0] == SITE]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][1], 2)
        self.check_patch(asm(rows[0][2], SITE))

    def test_shipped_patch(self):
        rows = [line.split(';')[0].split() for line in
                (ROOT / 'worldmod/worldmod_bases_caves.txt').read_text(encoding='utf-8').splitlines()
                if line.startswith('REWRITE 005f0aeb ')]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][2:4], ['2', OLD.hex()])
        self.check_patch(bytes.fromhex(rows[0][4]))


if __name__ == '__main__':
    unittest.main()
