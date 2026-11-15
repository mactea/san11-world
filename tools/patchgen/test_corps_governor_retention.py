"""Execute native governor selection with a lower-ranked appointed incumbent."""
import struct
import unittest
from pathlib import Path

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
    UC_X86_REG_EIP,
)
from corps_appointments import build_records


class GovernorRetentionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        exe = Path(__file__).resolve().parents[2] / 'backup/san11pk.exe.orig'
        cls.record = next(row for row in build_records(exe)
                          if row.startswith('CAVE 004be5b0 '))
        pe = pefile.PE(str(exe), fast_load=True)
        cls.native = pe.get_data(0x4be5b0 - pe.OPTIONAL_HEADER.ImageBase, 8)

    def select(self, number=3, player=True, status=1, same_corps=True,
               valid_incumbent=True, valid_force=True, patch=True, location=522,
               corps_id=14):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        cave, memory = 0x10000000, 0x20000000
        for address, size in [(0x400000, 0x400000), (cave, 0x1000), (memory, 0x10000)]:
            uc.mem_map(address, size)
        uc.mem_write(0x4be5b0, self.native)
        if patch:
            fields = self.record.split(';')[0].split()
            code = bytearray.fromhex(fields[4])
            for fixup in fields[5:]:
                offset, target = fixup[1:].split('=rel:')
                offset = int(offset)
                struct.pack_into('<I', code, offset,
                                 (int(target, 16) - cave - offset - 4) & 0xffffffff)
            uc.mem_write(cave, bytes(code))
            uc.mem_write(0x4be5b0, b'\xe9' + struct.pack('<I', cave - 0x4be5b5) + b'\x90' * 3)
        corps, incumbent, native_best, force, vtable, slot = [memory + i * 0x1000 for i in range(6)]
        stack = memory + 0xf000

        def put(address, value):
            uc.mem_write(address, struct.pack('<I', value & 0xffffffff))

        def word(address):
            return struct.unpack('<I', uc.mem_read(address, 4))[0]

        put(corps, vtable)
        put(corps + 8, number)
        put(corps + 12, 422)
        put(incumbent + 0x94, corps_id if same_corps else corps_id + 1)
        put(incumbent + 0x98, 522)
        put(incumbent + 0x9c, location)
        put(incumbent + 0xa0, status)
        put(force, vtable)
        put(vtable + 0x40, 0x401000)
        put(vtable + 0x48, 0x402000)
        put(slot, native_best)
        saved = {UC_X86_REG_EAX: slot, UC_X86_REG_EBX: incumbent,
                 UC_X86_REG_ECX: 0x12345678, UC_X86_REG_EDX: 0x87654321,
                 UC_X86_REG_ESI: corps, UC_X86_REG_EDI: 0x34567812,
                 UC_X86_REG_EBP: 0x45678123, UC_X86_REG_ESP: stack}
        for reg, value in saved.items():
            uc.reg_write(reg, value)

        def on_code(machine, address, size, data):
            if address == 0x4be5b8:
                machine.emu_stop()
                return
            sp = machine.reg_read(UC_X86_REG_ESP)
            cleanup = 0
            if address == 0x47a630:
                pointer = word(sp + 4)
                self.assertIn(pointer, (incumbent, force))
                value = int(valid_incumbent if pointer == incumbent else valid_force)
            elif address == 0x4912c0:
                self.assertEqual(word(sp + 4), corps)
                value, cleanup = corps_id, 4
            elif address == 0x401000:
                self.assertEqual(machine.reg_read(UC_X86_REG_ECX), corps)
                value = 1
            elif address == 0x490aa0:
                self.assertEqual(word(sp + 4), 1)
                value, cleanup = force, 4
            elif address == 0x402000:
                self.assertEqual(machine.reg_read(UC_X86_REG_ECX), force)
                value = int(player)
            else:
                return
            machine.reg_write(UC_X86_REG_EAX, value)
            machine.reg_write(UC_X86_REG_ECX, 0xdeadbeef)
            machine.reg_write(UC_X86_REG_EDX, 0xdeadbeef)
            machine.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
            machine.reg_write(UC_X86_REG_EIP, word(sp))

        uc.hook_add(UC_HOOK_CODE, on_code)
        uc.emu_start(0x4be5b0, 0x80000000, count=500)
        self.assertEqual(uc.reg_read(UC_X86_REG_EIP), 0x4be5b8)
        for reg in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI):
            self.assertEqual(uc.reg_read(reg), saved[reg])
        self.assertEqual(uc.reg_read(UC_X86_REG_ESP), stack - 4)
        self.assertEqual(word(stack - 4), incumbent)
        return uc.reg_read(UC_X86_REG_EBP) == incumbent

    def test_reproduces_native_overwrite_then_retains_appointed_governor(self):
        self.assertFalse(self.select(patch=False))
        self.assertTrue(self.select())

    def test_every_player_delegated_number_is_retained(self):
        for number in range(2, 17):
            with self.subTest(number=number):
                self.assertTrue(self.select(number=number))

    def test_active_statuses_and_absence_do_not_remove_incumbent(self):
        for status in (1, 2, 3):
            self.assertTrue(self.select(status=status))
        for location in (522, 404, 641, 2000):
            self.assertTrue(self.select(location=location))

    def test_captured_dead_unavailable_or_transferred_use_native_successor(self):
        for status in (-1, 0, 4, 5, 6, 7):
            with self.subTest(status=status):
                self.assertFalse(self.select(status=status))
        self.assertFalse(self.select(same_corps=False))
        self.assertFalse(self.select(valid_incumbent=False))

    def test_ai_main_corps_and_invalid_force_are_unchanged(self):
        self.assertFalse(self.select(player=False))
        self.assertFalse(self.select(valid_force=False))
        for number in (-1, 0, 1, 17):
            self.assertFalse(self.select(number=number))

    def test_global_corps_ids_are_not_force_local_numbers(self):
        for corps_id in (14, 66, 248):
            self.assertTrue(self.select(number=16, corps_id=corps_id))


if __name__ == '__main__':
    unittest.main()
