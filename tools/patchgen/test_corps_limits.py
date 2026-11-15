"""Run the native corps lookup, allocation, names and AI arrays as x86 code."""
import re
import struct
import unittest
from pathlib import Path

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP)
from corps_limits import build_records, FRAME_EXTRA

ROOT = Path(__file__).resolve().parents[2]
FORCE, CORPS, CITY, PERSON = 0x20001000, 0x22000000, 0x23000000, 0x24000000
STACK, STOP, VTABLE, AI = 0x2100f000, 0x401000, 0x20002000, 0x26000000
OWNER, AFFILIATION = 0x401020, 0x401030


class Native:
    pe = pefile.PE(str(ROOT / 'backup/san11pk.exe.orig'), fast_load=True)
    fields, caves = build_records(ROOT / 'backup/san11pk.exe.orig')

    def __init__(self, numbers=range(1, 17)):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        for address, size in [(0x400000, 0x500000), (0x7200000, 0x20000), (0x73f5000, 0x1000),
                              (0x20000000, 0x10000), (0x21000000, 0x20000),
                              (CORPS, 0x10000), (CITY, 0x100000),
                              (PERSON, 0x100000), (AI, 0x10000)]:
            self.uc.mem_map(address, size)
        for section in self.pe.sections:
            address = self.pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
            if address < 0x900000:
                data = section.get_data()[:0x900000-address]
                if data:
                    self.uc.mem_write(address, data)
        self.next_cave = 0x10000000
        self.uc.mem_map(self.next_cave, 0x100000)
        # Keep the existing expanded-city/global-corps patches in these native
        # functions so the new limit is checked with C=530, R=249 as installed.
        ranges = [(0x481240, 0x4812b2), (0x5d6090, 0x5d6223),
                  (0x5dcad0, 0x5dcbeb), (0x5f4580, 0x5f47bf)]
        for name in ('worldmod_bases.txt', 'worldmod_bases_caves.txt'):
            for line in (ROOT / 'worldmod' / name).read_text(encoding='utf-8').splitlines():
                p = line.split(';')[0].split()
                if not p:
                    continue
                address = int(p[1] if p[0] in ('CAVE', 'REWRITE') else p[0], 16)
                if any(lo <= address < hi for lo, hi in ranges):
                    self.apply(line)
        for line in self.fields + self.caves:
            self.apply(line)
        for site in FRAME_EXTRA:
            self.put(site + 2, 0x1f4 + (575-87)*4)
        self.corps = {}
        for number in numbers:
            # Deliberately make the global ID differ from the force-local one.
            global_id = 200 - number
            address = CORPS + global_id * 0x50
            self.corps[global_id] = address
            self.put(address, VTABLE)
            self.put(address + 4, 1)
            self.put(address + 8, number)
        self.put(VTABLE + 0x40, OWNER)
        self.put(VTABLE + 0x44, AFFILIATION)
        self.people, self.cities, self.callbacks, self.formatted = {}, {}, {}, []
        self.uc.hook_add(UC_HOOK_CODE, self.on_code)

    def put(self, address, value):
        self.uc.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def word(self, address):
        return struct.unpack('<I', self.uc.mem_read(address, 4))[0]

    @staticmethod
    def expression(value):
        value = re.sub(r'\bR\b', '249', value)
        value = re.sub(r'\bC\b', '530', value)
        value = re.sub(r'\bN\b', '575', value)
        return eval(value, {'__builtins__': {}}, {})

    def apply(self, line):
        p = line.split(';')[0].split()
        if p[0] == 'CAVE':
            address, length, code = int(p[1], 16), int(p[2]), bytearray.fromhex(p[4])
            base = self.next_cave
            self.next_cave += 0x1000
            for fixup in p[5:]:
                offset, kind, value = re.fullmatch(r'@(\d+)=(\w+):(.+)', fixup).groups()
                offset = int(offset)
                result = int(value, 16) - base - offset - 4 if kind == 'rel' else self.expression(value)
                size = {'rel': 4, 'abs': 4, 'abs8': 1, 'abs16': 2}[kind]
                code[offset:offset+size] = (result & ((1 << (8*size))-1)).to_bytes(size, 'little')
            self.uc.mem_write(base, bytes(code))
            self.uc.mem_write(address, b'\xe9' + struct.pack('<I', base-address-5) + b'\x90'*(length-5))
        else:
            address, offset, size = int(p[0], 16), int(p[1]), int(p[2])
            result = self.expression(''.join(p[4:]))
            self.uc.mem_write(address + offset, result.to_bytes(size, 'little'))

    def return_value(self, value, cleanup=0):
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        self.uc.reg_write(UC_X86_REG_EAX, value & 0xffffffff)
        self.uc.reg_write(UC_X86_REG_EIP, self.word(esp))
        self.uc.reg_write(UC_X86_REG_ESP, esp + 4 + cleanup)

    def on_code(self, uc, address, size, data):
        esp, this = uc.reg_read(UC_X86_REG_ESP), uc.reg_read(UC_X86_REG_ECX)
        argument = self.word(esp + 4)
        if address == STOP:
            uc.emu_stop()
        elif address in self.callbacks:
            self.callbacks[address](self, argument)
        elif address == 0x47a630:
            self.return_value(int(argument != 0))
        elif address == 0x490ad0:
            self.return_value(self.corps.get(argument, 0), 4)
        elif address == 0x491270:
            self.return_value(1, 4)
        elif address == 0x490aa0:
            self.return_value(FORCE, 4)
        elif address == 0x490b00:
            self.return_value(self.people.get(argument, 0), 4)
        elif address == 0x490a10:
            self.return_value(self.cities.get(argument, 0), 4)
        elif address == OWNER:
            self.return_value(self.word(this + 4))
        elif address == AFFILIATION:
            self.return_value(self.word(this + (0x94 if this >= PERSON else 0x38)))
        elif address == 0x471d10:
            pointer = self.word(esp + 12)
            value = bytes(uc.mem_read(pointer, 32)).split(b'\0')[0].decode('cp950')
            self.formatted.append(value)
            self.return_value(0)
        elif address == 0x707b1a:
            self.return_value(0)

    def call(self, address, *arguments, this=0, max_count=150000):
        self.put(STACK, STOP)
        for i, value in enumerate(arguments):
            self.put(STACK + 4 + i*4, value)
        for register, value in [(UC_X86_REG_ESP, STACK), (UC_X86_REG_ECX, this),
                (UC_X86_REG_EBX, 0x11111111), (UC_X86_REG_EBP, 0x22222222),
                (UC_X86_REG_ESI, 0x33333333), (UC_X86_REG_EDI, 0x44444444)]:
            self.uc.reg_write(register, value)
        self.uc.emu_start(address, STOP + 1, count=max_count)
        assert self.uc.reg_read(UC_X86_REG_EIP) == STOP, hex(self.uc.reg_read(UC_X86_REG_EIP))
        for register, value in [(UC_X86_REG_EBX, 0x11111111), (UC_X86_REG_EBP, 0x22222222),
                               (UC_X86_REG_ESI, 0x33333333), (UC_X86_REG_EDI, 0x44444444)]:
            assert self.uc.reg_read(register) == value
        return self.uc.reg_read(UC_X86_REG_EAX)

    def city(self, index, number):
        pointer = CITY + index * 0x248
        self.cities[index] = pointer
        self.put(pointer, VTABLE)
        self.put(pointer + 0x38, 200 - number)

    def person(self, index, number):
        pointer = PERSON + index * 0x190
        self.people[index] = pointer
        self.put(pointer, VTABLE)
        self.put(pointer + 4, 1)
        self.put(pointer + 0x94, 200 - number)
        self.put(pointer + 0x98, 0)


class CorpsLimitTest(unittest.TestCase):
    def test_shipped_tables_include_all_limit_and_frame_changes(self):
        fields = {}
        for line in (ROOT / 'worldmod/worldmod_bases.txt').read_text(encoding='utf-8').splitlines():
            p = line.split(';')[0].split()
            if p:
                fields[int(p[0], 16), int(p[1])] = p
        for line in Native.fields:
            p = line.split(';')[0].split()
            self.assertEqual(fields[int(p[0], 16), int(p[1])][2:], p[2:])
        for site in FRAME_EXTRA:
            self.assertEqual(Native.expression(''.join(fields[site, 2][4:])), 0x1f4+(575-87)*4)
        shipped = {}
        for line in (ROOT / 'worldmod/worldmod_bases_caves.txt').read_text(encoding='utf-8').splitlines():
            p = line.split(';')[0].split()
            if p and p[0] == 'CAVE':
                shipped[int(p[1], 16)] = p
        for line in Native.caves:
            p = line.split(';')[0].split()
            self.assertEqual(shipped[int(p[1], 16)], p)

    def test_native_number_validator_and_setter(self):
        game = Native()
        for number in (0, 1, 8, 9, 15, 16, 17):
            self.assertEqual(game.call(0x47e010, number), int(1 <= number <= 16))
            game.put(FORCE + 8, 7)
            game.call(0x47e160, number, this=FORCE)
            self.assertEqual(game.word(FORCE + 8), number if 0 <= number <= 16 else 7)

    def test_lookup_includes_last_corps_and_keeps_global_id(self):
        game = Native()
        for number in (1, 8, 9, 16):
            self.assertEqual(game.call(0x481240, number, this=FORCE), 200-number)
        self.assertEqual(game.call(0x481240, 17, this=FORCE), 0xffffffff)

    def test_allocation_fills_holes_and_stops_at_sixteen(self):
        for numbers, expected in [(range(1, 9), 9), (range(1, 16), 16),
                (range(1, 17), 0), ([1, 2, 4, 9, 16], 3)]:
            game = Native(numbers)
            self.assertEqual(game.call(0x4a2a40, FORCE), expected)
            self.assertEqual(game.uc.reg_read(UC_X86_REG_ESP), STACK + 8)

    def test_chinese_names_ten_through_sixteen(self):
        game = Native()
        for function in (0x48f3b0, 0x48f460):
            for number, expected in [(8, '八'), (9, '九'), (10, '十'), (11, '十一'), (16, '十六')]:
                game.call(function, FORCE, number, 0)
                self.assertEqual(game.formatted[-1], expected)

    def test_ai_statistics_do_not_overlap_and_include_high_numbers(self):
        game = Native()
        weights, statistics = 0x20003000, 0x20004000
        game.uc.mem_write(statistics - 4, b'\xa5' * 0x98)
        for index, number in enumerate([1, 8, 9, 16, 16]):
            game.city(index, number)
            game.put(weights + index*4, index + 10)
        game.call(0x5dcad0, 1, weights, this=statistics)
        self.assertEqual(game.word(statistics), 5)
        self.assertEqual(game.word(statistics + 4), 4)
        for number, count, weight in [(1, 1, 10), (8, 1, 11), (9, 1, 12), (16, 2, 27)]:
            self.assertEqual(game.word(statistics + 8 + number*4), count)
            self.assertEqual(game.word(statistics + 0x4c + number*4), weight)
        self.assertEqual(game.word(statistics - 4), 0xa5a5a5a5)
        self.assertEqual(game.word(statistics + 0x90), 0xa5a5a5a5)
        self.assertEqual(game.call(0x5dcbf0, this=statistics), 16)
        game.put(statistics + 8 + 9*4, 2)
        game.put(statistics + 0x4c + 9*4, 100)
        self.assertEqual(game.call(0x5dcbf0, this=statistics), 9)

    def test_officer_reassignment_counts_corps_sixteen_without_stack_damage(self):
        game = Native()
        city, empty_list = CITY, 0x20003000
        game.put(city, VTABLE)
        game.put(city + 4, 1)
        for number in range(1, 17):
            game.person(number, number)
        self.assertEqual(game.call(0x5d6090, city, empty_list), 0)
        game.people.pop(16)
        self.assertEqual(game.call(0x5d6090, city, empty_list), 1)
        self.assertEqual(game.uc.reg_read(UC_X86_REG_ESP), STACK + 4)

    def test_delegated_ai_visits_all_sixteen_corps(self):
        game = Native()
        game.put(AI + 8, FORCE)
        game.put(0x72019b4, 1)
        visits = []
        for address in (0x57e460, 0x57e470, 0x5e61e0, 0x5f44d0, 0x5f5640, 0x5eb740):
            game.callbacks[address] = lambda fixture, argument: fixture.return_value(0)
        for address in (0x5ec370, 0x5f5820):
            def record(fixture, argument):
                visits.append(argument)
                fixture.return_value(1, 4)
            game.callbacks[address] = record
        game.callbacks[0x4123b0] = lambda fixture, argument: fixture.return_value(1)
        self.assertEqual(game.call(0x5f5a00, this=AI), 1)
        self.assertEqual(visits, [200-number for number in range(1, 17) for _ in range(2)])

    def test_native_ai_creation_frame_keeps_city_weights_separate(self):
        game = Native()
        game.put(AI + 8, FORCE)
        game.put(FORCE + 4, 92)
        game.person(92, 1)
        for index, number in [(0, 1), (529, 16)]:
            game.city(index, number)
            game.uc.mem_write(AI + index*0x34 + 0x1f, bytes([index % 100 + 1]))
        game.callbacks[0x482570] = lambda fixture, argument: fixture.return_value(1)
        checked = []

        def check_statistics(fixture, argument):
            statistics = fixture.uc.reg_read(UC_X86_REG_ECX)
            self.assertEqual(fixture.word(statistics), 2)
            self.assertEqual(fixture.word(statistics + 8 + 16*4), 1)
            self.assertEqual(fixture.word(statistics + 0x4c + 16*4), 30)
            self.assertEqual(fixture.word(statistics + 0x90 + 529*4), 30)
            checked.append(True)
            fixture.return_value(1)

        game.callbacks[0x5deba0] = check_statistics

        def stop_after_first_corps(fixture, argument):
            fixture.put(AI, 1)
            fixture.return_value(1, 4)

        game.callbacks[0x5dcc40] = stop_after_first_corps
        for address in (0x57e460, 0x57e470):
            game.callbacks[address] = lambda fixture, argument: fixture.return_value(0)
        self.assertEqual(game.call(0x5f4580, this=AI), 0)
        self.assertEqual(checked, [True])
        self.assertEqual(game.uc.reg_read(UC_X86_REG_ESP), STACK + 4)


if __name__ == '__main__':
    unittest.main()
