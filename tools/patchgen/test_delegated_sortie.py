"""Run the patched native military-menu range function as x86 code."""
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
from delegated_sortie import build_records


class DelegatedSortieTest(unittest.TestCase):
    def menu_range(self, number, player=True, valid_corps=True, valid_force=True,
                   category=1):
        exe = Path(__file__).resolve().parents[2] / 'backup/san11pk.exe.orig'
        pe = pefile.PE(str(exe), fast_load=True)
        native = pe.get_data(0x617770 - pe.OPTIONAL_HEADER.ImageBase, 0x290)
        fields = build_records(exe)[0].split(';')[0].split()
        code = bytearray.fromhex(fields[4])
        cave = 0x10000000
        for fixup in fields[5:]:
            offset, target = fixup[1:].split('=rel:')
            offset = int(offset)
            struct.pack_into('<I', code, offset,
                             (int(target, 16) - cave - offset - 4) & 0xffffffff)
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        for start, size in [(0x400000, 0x400000), (cave, 0x1000),
                            (0x20000000, 0x10000)]:
            uc.mem_map(start, size)
        uc.mem_write(0x617770, native)
        uc.mem_write(cave, bytes(code))
        uc.mem_write(0x61791b, b'\xe9' + struct.pack('<I', cave - 0x617920) + b'\x90')
        menu, city, corps, force, vtable, output = [0x20001000 + i * 0x1000 for i in range(6)]

        def put(address, value):
            uc.mem_write(address, struct.pack('<I', value & 0xffffffff))

        def word(address):
            return struct.unpack('<I', uc.mem_read(address, 4))[0]

        put(menu + 0x354, city)
        put(city, vtable)
        put(vtable + 0x44, 0x401000)
        put(corps, vtable)
        put(corps + 8, number)
        put(vtable + 0x40, 0x402000)
        put(force, vtable)
        put(vtable + 0x48, 0x403000)
        stack, stop = 0x2000f000, 0x404000
        for offset, value in [(0, stop), (4, category), (8, output), (12, output + 4)]:
            put(stack + offset, value)
        saved = [UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP]
        for reg in saved:
            uc.reg_write(reg, 0x12345678)
        uc.reg_write(UC_X86_REG_ECX, menu)
        uc.reg_write(UC_X86_REG_ESP, stack)

        def on_code(machine, address, size, data):
            if address == stop:
                machine.emu_stop()
                return
            cleanup = 0
            sp = machine.reg_read(UC_X86_REG_ESP)
            if address == 0x401000:
                result = 14
            elif address == 0x402000:
                result = 1
            elif address == 0x403000:
                result = int(player)
            elif address == 0x490ad0:
                result, cleanup = corps, 4
            elif address == 0x490aa0:
                self.assertEqual(word(sp + 4), 1)
                result, cleanup = force, 4
            elif address == 0x47a630:
                pointer = word(sp + 4)
                self.assertIn(pointer, (corps, force))
                result = int(valid_corps if pointer == corps else valid_force)
            else:
                return
            machine.reg_write(UC_X86_REG_EAX, result)
            machine.reg_write(UC_X86_REG_ECX, 0xdeadbeef)
            machine.reg_write(UC_X86_REG_EDX, 0xdeadbeef)
            machine.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
            machine.reg_write(UC_X86_REG_EIP, word(sp))

        uc.hook_add(UC_HOOK_CODE, on_code)
        uc.emu_start(0x617890, stop + 1, count=500)
        self.assertEqual(uc.reg_read(UC_X86_REG_EIP), stop)
        self.assertEqual(uc.reg_read(UC_X86_REG_EAX), 1)
        self.assertEqual(uc.reg_read(UC_X86_REG_ESP), stack + 16)
        for reg in saved:
            self.assertEqual(uc.reg_read(reg), 0x12345678)
        return word(output), word(output + 4)

    def test_player_delegated_corps_receive_sortie_and_transport(self):
        for number in (2, 3, 8, 9, 16):
            with self.subTest(number=number):
                self.assertEqual(self.menu_range(number), (14, 15))

    def test_main_corps_retains_training_sortie_and_transport(self):
        self.assertEqual(self.menu_range(1), (13, 15))

    def test_ai_and_invalid_corps_keep_native_fallback(self):
        for kwargs in [dict(player=False), dict(valid_corps=False),
                       dict(valid_force=False)]:
            with self.subTest(kwargs=kwargs):
                self.assertEqual(self.menu_range(2, **kwargs), (15, 15))
        for number in (0, -1, 17):
            self.assertEqual(self.menu_range(number), (15, 15))

    def test_other_menu_categories_are_unchanged(self):
        for category, expected in [(0, (6, 12)), (2, (16, 21)),
                                   (3, (22, 28)), (4, (29, 31)), (5, (33, 37))]:
            with self.subTest(category=category):
                self.assertEqual(self.menu_range(2, category=category), expected)


if __name__ == '__main__':
    unittest.main()
