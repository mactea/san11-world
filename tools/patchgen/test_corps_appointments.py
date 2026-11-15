"""Execute the companion loader and governor AP patch as x86 code."""
import struct
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
    UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
)
from corps_appointments import build_records


class LoaderTest(unittest.TestCase):
    def run_loader(self, library=True, export=True):
        exe = Path(__file__).resolve().parents[2] / 'backup/san11pk.exe.orig'
        fields = build_records(exe)[0].split(';')[0].split()
        code = bytearray.fromhex(fields[4])
        base = 0x10000000
        for fixup in fields[5:]:
            offset, target = fixup[1:].split('=rel:')
            offset, target = int(offset), int(target, 16)
            struct.pack_into('<I', code, offset, (target - base - offset - 4) & 0xffffffff)
        machine = Uc(UC_ARCH_X86, UC_MODE_32)
        for start, size in [(0x400000, 0x400000), (base, 0x1000), (0x20000000, 0x10000)]:
            machine.mem_map(start, size)
        machine.mem_write(base, bytes(code))
        machine.mem_write(0x74e2f8, struct.pack('<II', 0x401000, 0x402000))
        registers = [UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                     UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP]
        for n, reg in enumerate(registers):
            machine.reg_write(reg, 0x12340000 + n)
        machine.reg_write(UC_X86_REG_ESP, 0x2000f000)
        machine.reg_write(UC_X86_REG_EFLAGS, 0x246)
        calls = []

        def word(address):
            return struct.unpack('<I', machine.mem_read(address, 4))[0]

        def string(address):
            return bytes(machine.mem_read(address, 32)).split(b'\0')[0]

        def on_code(uc, address, size, data):
            if address == 0x6034a7:
                uc.emu_stop()
                return
            if address not in (0x401000, 0x402000, 0x403000):
                return
            esp = uc.reg_read(UC_X86_REG_ESP)
            if address == 0x401000:
                self.assertEqual(string(word(esp + 4)), b'worldmod_corps.dll')
                result, arguments = (0x1234 if library else 0), 1
            elif address == 0x402000:
                self.assertEqual(word(esp + 4), 0x1234)
                self.assertEqual(string(word(esp + 8)), b'WorldmodCorpsInit')
                result, arguments = (0x403000 if export else 0), 2
            else:
                self.assertEqual(word(esp + 4), 0)
                result, arguments = 0, 1
            calls.append(address)
            uc.reg_write(UC_X86_REG_EAX, result)
            uc.reg_write(UC_X86_REG_ECX, 0xdeadbeef)
            uc.reg_write(UC_X86_REG_EDX, 0xdeadbeef)
            uc.reg_write(UC_X86_REG_ESP, esp + 4 + 4 * arguments)
            uc.reg_write(UC_X86_REG_EIP, word(esp))

        machine.hook_add(UC_HOOK_CODE, on_code)
        machine.emu_start(base, 0x6034a7 + 1, count=500)
        self.assertEqual(machine.reg_read(UC_X86_REG_EIP), 0x6034a7)
        for n, reg in enumerate(registers):
            self.assertEqual(machine.reg_read(reg), 0x12340000 + n)
        self.assertEqual(machine.reg_read(UC_X86_REG_EFLAGS), 0x246)
        self.assertEqual(machine.reg_read(UC_X86_REG_ESP), 0x2000eff8)
        self.assertEqual(word(0x2000effc), 0xffffffff)
        self.assertEqual(word(0x2000eff8), 0x735c14)
        return calls

    def test_loaded(self):
        self.assertEqual(len(self.run_loader()), 3)

    def test_missing_library(self):
        self.assertEqual(len(self.run_loader(library=False)), 1)

    def test_missing_export(self):
        self.assertEqual(len(self.run_loader(export=False)), 2)


class GovernorAPTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import pefile
        exe = Path(__file__).resolve().parents[2] / 'backup/san11pk.exe.orig'
        cls.record = build_records(exe)[1]
        pe = pefile.PE(str(exe), fast_load=True)
        cls.native_ap = pe.get_data(0x5986a0 - pe.OPTIONAL_HEADER.ImageBase, 0x1e0)

    def contribution(self, leadership, politics, charm, number=2):
        fields = self.record.split(';')[0].split()
        code = bytearray.fromhex(fields[4])
        base, person, corps, stack = 0x10000000, 0x20001000, 0x20002000, 0x2000f000
        for fixup in fields[5:]:
            offset, target = fixup[1:].split('=rel:')
            offset = int(offset)
            struct.pack_into('<I', code, offset, (int(target, 16) - base - offset - 4) & 0xffffffff)
        machine = Uc(UC_ARCH_X86, UC_MODE_32)
        machine.mem_map(0x400000, 0x400000)
        machine.mem_map(base, 0x1000)
        machine.mem_map(0x20000000, 0x10000)
        machine.mem_write(base, bytes(code))
        for offset, value in [(0x170, leadership), (0x173, politics), (0x174, charm)]:
            machine.mem_write(person + offset, bytes([value]))
        machine.mem_write(corps + 8, struct.pack('<I', number))
        for reg, value in [(UC_X86_REG_ESI, person), (UC_X86_REG_ECX, person),
                           (UC_X86_REG_EBP, corps), (UC_X86_REG_EDI, 0x20003000),
                           (UC_X86_REG_EBX, 0x44556677), (UC_X86_REG_ESP, stack)]:
            machine.reg_write(reg, value)
        calls = []

        def on_code(uc, address, size, data):
            if address in (0x598774, 0x598705):
                uc.emu_stop()
            elif address == 0x4890b0:
                calls.append(address)
                self.assertEqual(uc.reg_read(UC_X86_REG_ECX), person)
                esp = uc.reg_read(UC_X86_REG_ESP)
                ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
                uc.reg_write(UC_X86_REG_EAX, charm)
                uc.reg_write(UC_X86_REG_ESP, esp + 4)
                uc.reg_write(UC_X86_REG_EIP, ret)

        machine.hook_add(UC_HOOK_CODE, on_code)
        machine.emu_start(base, 0x80000000, count=100)
        for reg, value in [(UC_X86_REG_EBP, corps), (UC_X86_REG_EDI, 0x20003000),
                           (UC_X86_REG_EBX, 0x44556677), (UC_X86_REG_ESP, stack)]:
            self.assertEqual(machine.reg_read(reg), value)
        if number == 1:
            self.assertEqual(calls, [0x4890b0])
            self.assertEqual(machine.reg_read(UC_X86_REG_EIP), 0x598705)
            self.assertEqual(machine.reg_read(UC_X86_REG_ESI), person)
            self.assertEqual(machine.reg_read(UC_X86_REG_EAX), charm)
            return None
        self.assertEqual(calls, [])
        self.assertEqual(machine.reg_read(UC_X86_REG_EIP), 0x598774)
        return machine.reg_read(UC_X86_REG_ESI)

    def test_literal_weights_are_not_normalized(self):
        self.assertEqual(self.contribution(100, 100, 100), 28)
        self.assertEqual(self.contribution(80, 80, 80), 22)

    def test_politics_is_included_and_leadership_has_more_weight(self):
        self.assertEqual(self.contribution(100, 0, 0), 12)
        self.assertEqual(self.contribution(0, 100, 0), 8)
        self.assertEqual(self.contribution(0, 0, 100), 8)
        self.assertEqual(self.contribution(83, 64, 73), 20)

    def test_rounding_and_floor(self):
        for stats, expected in [((0, 0, 0), 6), ((44, 10, 11), 6), ((45, 10, 10), 7)]:
            with self.subTest(stats=stats):
                self.assertEqual(self.contribution(*stats), expected)

    def test_expanded_abilities_and_last_corps(self):
        for number in (8, 9, 16):
            self.assertEqual(self.contribution(255, 255, 255, number=number), 71)

    def test_main_corps_retains_native_formula(self):
        self.contribution(40, 100, 95, number=1)

    def test_native_total_keeps_advisor_multiplier_and_255_cap(self):
        for number, advisor_iq, existing, expected in [(2, None, 0, 30), (2, 100, 0, 36),
                                                       (2, 100, 240, 255), (1, None, 0, 26)]:
            with self.subTest(corps=number, advisor=advisor_iq, existing=existing):
                machine = Uc(UC_ARCH_X86, UC_MODE_32)
                machine.mem_map(0x400000, 0x400000)
                machine.mem_map(0x10000000, 0x1000)
                machine.mem_map(0x20000000, 0x10000)
                machine.mem_write(0x5986a0, self.native_ap)
                fields = self.record.split(';')[0].split()
                code = bytearray.fromhex(fields[4])
                for fixup in fields[5:]:
                    offset, target = fixup[1:].split('=rel:')
                    offset = int(offset)
                    struct.pack_into('<I', code, offset, (int(target, 16) - 0x10000000 - offset - 4) & 0xffffffff)
                machine.mem_write(0x10000000, bytes(code))
                machine.mem_write(0x598700, b'\xe9' + struct.pack('<I', 0x10000000 - 0x598705))
                person, corps, force, advisor, city, vtable = [0x20001000 + i * 0x1000 for i in range(6)]

                def put(address, value):
                    machine.mem_write(address, struct.pack('<I', value & 0xffffffff))

                put(corps, vtable)
                put(corps + 8, number)
                put(corps + 12, 92)
                machine.mem_write(corps + 0x2c, bytes([existing]))
                put(vtable + 0x40, 0x401000)
                put(city, vtable)
                put(vtable + 0x44, 0x402000)
                put(force + 8, -1 if advisor_iq is None else 999)
                machine.mem_write(person + 0x170, bytes([83, 78, 72, 64, 73]))
                machine.mem_write(advisor + 0x172, bytes([advisor_iq or 0]))
                stack = 0x2000f000
                put(stack, 0x403000)
                put(stack + 4, corps)
                machine.reg_write(UC_X86_REG_ESP, stack)
                written = []

                def on_code(uc, address, size, data):
                    if address == 0x403000:
                        uc.emu_stop()
                        return
                    esp = uc.reg_read(UC_X86_REG_ESP)
                    arg = struct.unpack('<I', uc.mem_read(esp + 4, 4))[0]
                    cleanup = 0
                    if address == 0x401000:
                        value = 1
                    elif address == 0x402000:
                        value = 0xffffffff
                    elif address == 0x490aa0:
                        value, cleanup = force, 4
                    elif address == 0x490b00:
                        value, cleanup = {92: person, 999: advisor}.get(arg, 0), 4
                    elif address == 0x47a630:
                        value = int(arg != 0)
                    elif address in (0x489070, 0x4890b0, 0x489090):
                        value = {0x489070: 83, 0x4890b0: 73, 0x489090: advisor_iq}[address]
                    elif address == 0x47e9b0:
                        uc.mem_write(arg, b'\0' * 4)
                        value, cleanup = 0, 4
                    elif address == 0x490a10:
                        value, cleanup = city, 4
                    elif address == 0x490ad0:
                        value, cleanup = 0, 4
                    elif address == 0x47e3e0:
                        self.assertEqual(uc.reg_read(UC_X86_REG_ECX), corps)
                        written.append(arg)
                        value, cleanup = arg, 4
                    else:
                        return
                    ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
                    uc.reg_write(UC_X86_REG_EAX, value)
                    uc.reg_write(UC_X86_REG_ESP, esp + 4 + cleanup)
                    uc.reg_write(UC_X86_REG_EIP, ret)

                machine.hook_add(UC_HOOK_CODE, on_code)
                machine.emu_start(0x5986a0, 0x403001, count=10000)
                self.assertEqual(written, [expected])
                self.assertEqual(machine.reg_read(UC_X86_REG_EIP), 0x403000)
                self.assertEqual(machine.reg_read(UC_X86_REG_ESP), stack + 8)


if __name__ == '__main__':
    unittest.main()
