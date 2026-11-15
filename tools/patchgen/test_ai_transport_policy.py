"""Run native transport instructions with generated and shipped x86 hooks.

The native executable is a read-only fixture from project.json's game_dir.
Tests skip when it is absent; no game, saves or installed patch files change.
"""
import json
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

import ai_transport_policy as policy
from detour import asm


ROOT = Path(__file__).resolve().parents[2]
STACK, SOURCE, TARGET, VTABLE = 0x20008000, 0x21000000, 0x21001000, 0x21002000
AI, NODE, PERSON, CORPS, FORCE = 0x22000000, 0x24000000, 0x24001000, 0x25000000, 0x25001000
CAVE = 0x10000000


class NativeFixture:
    def __init__(self, pe, records, cities=530):
        self.cpu = cpu = Uc(UC_ARCH_X86, UC_MODE_32)
        self.values = dict(C=cities, N=cities + 45)
        self.tail = (self.values['N'] - 87) * 0x34
        cpu.mem_map(0x400000, 0x400000)
        for address, size in [(0x5f0000, 0x7000), (0x5e4000, 0x1000)]:
            cpu.mem_write(address, pe.get_data(address - pe.OPTIONAL_HEADER.ImageBase, size))
        for address, size in [(CAVE, 0x10000), (0x20000000, 0x10000),
                              (SOURCE, 0x10000), (AI, 0x20000),
                              (NODE, 0x2000), (CORPS, 0x3000)]:
            cpu.mem_map(address, size)
        # Apply existing AIRT-tail relocations before the new caller hooks.
        for line in (ROOT / 'worldmod/worldmod_bases.txt').read_text().splitlines():
            row = line.split(';')[0].split()
            if len(row) < 5:
                continue
            site = int(row[0], 16)
            if 0x5f0d4c <= site < 0x5f1066 or 0x5f4e8e <= site < 0x5f4ecd:
                value = self.expression(row[4])
                cpu.mem_write(site + int(row[1]), value.to_bytes(int(row[2]), 'little'))
        self.sites = set()
        for index, record in enumerate(records):
            row = record.split(';')[0].split()
            kind, site, length = row[0], int(row[1], 16), int(row[2])
            self.sites.add(site)
            code, base = bytearray.fromhex(row[4]), site if kind == 'REWRITE' else CAVE + index * 0x1000
            for fix in row[5:]:
                offset, value = fix[1:].split('=')
                typ, expr = value.split(':')
                offset = int(offset)
                value = int(expr, 16) - (base + offset + 4) if typ == 'rel' else self.expression(expr)
                struct.pack_into('<I', code, offset, value & 0xffffffff)
            cpu.mem_write(base, bytes(code))
            if kind == 'CAVE':
                cpu.mem_write(site, asm(f'jmp {base}', site) + b'\x90' * (length - 5))
        self.stub(0x700000, 'mov eax, [ecx+4]; ret')  # virtual troop getter
        self.stub(0x700010, 'mov eax, [ecx+0x1c]; ret')  # virtual corps getter
        self.stub(0x700020, 'mov eax, 0x21003000; ret')  # target coordinates
        self.stub(0x486bc0, 'mov eax, [ecx+0x18]; ret')
        self.stub(0x486df0, 'mov eax, [ecx+8]; ret')
        self.stub(0x486ea0, 'mov eax, [ecx+0xc]; ret')
        self.stub(0x486c80, 'mov eax, [ecx+0x10]; ret')
        self.stub(0x486d30, 'mov eax, [ecx+0x14]; ret')
        self.stub(0x491770, 'mov eax, [esp+4]; mov eax, [eax+0x20]; ret 4')
        self.stub(0x491310, 'mov eax, [esp+4]; mov eax, [eax]; ret 4')
        self.stub(0x49f0d0, 'mov eax, [esp+4]; add eax, 8; ret 4')
        self.stub(0x5db840, 'mov eax, 10; ret')
        self.stub(0x5e84e0, 'xor eax, eax; ret 8')
        self.stub(0x486950, 'xor eax, eax; ret 4')
        self.stub(0x5e0760,
                  f'mov edx, [esp+4]; mov dword ptr [edx], {SOURCE}; '
                  'mov dword ptr [edx+8], 77; mov dword ptr [edx+0xc], -1; '
                  'mov dword ptr [edx+0x10], -1; mov dword ptr [edx+0x1c], 5000; '
                  'mov dword ptr [edx+0x20], 1; mov dword ptr [edx+0x24], 9; '
                  'mov dword ptr [edx+0x50], 5000; mov eax, 1; ret 8')
        self.write(VTABLE + 0x50, 0x700000)
        self.write(VTABLE + 0x44, 0x700010)
        self.write(VTABLE + 0x3c, 0x700020)
        for base, index in [(SOURCE, 410), (TARGET, 476)]:
            self.write(base, VTABLE)
            for off, value in [(4, 10000), (8, 10000), (0xc, 500000),
                               (0x10, 0), (0x14, 100000), (0x18, 100000),
                               (0x1c, 14), (0x20, index)]:
                self.write(base + off, value)
        self.write(NODE + 8, PERSON)
        self.write(PERSON, 77)
        self.write(AI + 0x3298 + self.tail, NODE)
        self.write(AI + 0x32a0 + self.tail, 1)
        self.write(AI + 0x3288 + self.tail, 10)
        self.write(AI + 0x328c + self.tail, 0)
        self.write(AI + 0x3290 + self.tail, 10000)
        self.cpu.reg_write(UC_X86_REG_ESP, STACK)
        self.cpu.reg_write(UC_X86_REG_ESI, AI)
        self.calls = []

    def expression(self, text):
        return eval(text, {'__builtins__': {}}, self.values)

    def stub(self, address, code):
        self.cpu.mem_write(address, asm(code, address))

    def write(self, address, value, size=4):
        self.cpu.mem_write(address, (value & ((1 << (size * 8)) - 1)).to_bytes(size, 'little'))

    def read(self, address):
        return struct.unpack('<I', self.cpu.mem_read(address, 4))[0]

    def run(self, start, stops):
        def observe(cpu, address, size, data):
            if address in (0x5e0760, 0x49f0d0, 0x4795c0, 0x4795f0):
                self.calls.append(address)
            if address in stops:
                cpu.emu_stop()
        hook = self.cpu.hook_add(UC_HOOK_CODE, observe)
        self.cpu.emu_start(start, 0x800000, count=2000)
        self.cpu.hook_del(hook)
        stopped = self.cpu.reg_read(UC_X86_REG_EIP)
        assert stopped in stops, hex(stopped)
        return stopped

    def shipment(self, troops=10000, food=10000, gold=0, n=3,
                 target_troops=0, target_capacity=100000, officers=1, ap=10):
        self.write(SOURCE + 4, troops)
        self.write(TARGET + 4, target_troops)
        self.write(TARGET + 0x18, target_capacity)
        self.write(AI + 0x3290 + self.tail, food)
        self.write(AI + 0x328c + self.tail, gold)
        self.write(AI + 0x3288 + self.tail, ap)
        self.write(AI + 0x32a0 + self.tail, officers)
        self.write(AI + 410 * 0x34 + 0x22, officers, 2)
        self.write(AI + 410 * 0x34 + 0x4a, n, 2)
        for offset, value in [(0xfc, SOURCE), (0x18, 476), (0x1c, 410)]:
            self.write(STACK + offset, value)
        self.cpu.reg_write(UC_X86_REG_EAX, TARGET)
        self.cpu.reg_write(UC_X86_REG_EBX, 410)
        stopped = self.run(0x5f0d4c, {0x5f0b47, 0x5f1183, 0x5f1066})
        if stopped != 0x5f1066:
            return None
        command = struct.unpack('<36I', self.cpu.mem_read(STACK + 0x5c, 0x90))
        return dict(troops=command[7], gold=command[5], food=command[6],
                    weapon=command[8], weapon_amount=command[20],
                    leader=command[2], source=command[0], transport=command[32],
                    reserve=self.read(STACK + 0x50), reserve_food=self.read(STACK + 0x58))


class AITransportPolicyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        game = Path(json.loads((ROOT / 'project.json').read_text())['game_dir'])
        exe = game / 'san11pk.exe'
        if not exe.exists():
            raise unittest.SkipTest('Native read-only fixture san11pk.exe is not installed')
        cls.pe = pefile.PE(str(exe))
        cls.generated = policy.build_records(exe)

    def variants(self):
        yield 'generator', self.generated
        sites = {int(r.split()[1], 16) for r in self.generated}
        rows = [r for r in (ROOT / 'worldmod/worldmod_bases_caves.txt').read_text().splitlines()
                if r.startswith(('CAVE ', 'REWRITE ')) and int(r.split()[1], 16) in sites]
        self.assertEqual(len(rows), len(sites), 'Shipped table must contain every policy hook exactly once')
        yield 'shipped', rows

    def test_original_rejects_poor_rear_city(self):
        self.assertIsNone(NativeFixture(self.pe, []).shipment(troops=10000, gold=0))

    def test_native_rear_demand_three_can_ship_with_one_officer(self):
        self.assertIsNone(NativeFixture(self.pe, []).shipment(
            troops=15000, food=100000, gold=1000, n=3, officers=1))
        for name, records in self.variants():
            for n in (0, 1, 2, 3):
                with self.subTest(variant=name, n=n):
                    fixture = NativeFixture(self.pe, records)
                    result = fixture.shipment(troops=15000, food=100000, n=n)
                    self.assertIsNotNone(result)
                    self.assertEqual((result['reserve'], result['troops']), (3000, 12000))
                    self.assertEqual(fixture.read(AI + 410 * 0x34 + 0x4a) & 0xffff, n)

    def test_rear_priority_supply_also_uses_available_officer(self):
        for name, records in self.variants():
            with self.subTest(variant=name):
                fixture = NativeFixture(self.pe, records)
                fixture.write(STACK + 0x14, 1)
                result = fixture.shipment(troops=15000, food=100000, n=3, officers=1)
                self.assertIsNotNone(result)
                self.assertEqual((result['reserve'], result['troops']), (3000, 12000))

    def test_rear_surplus_and_no_required_gold_or_weapons(self):
        for name, records in self.variants():
            for cities in (530, 1000):
                for troops, food, target, expected in [
                    (3000, 10000, 0, None), (3999, 10000, 0, None),
                    (4000, 10000, 0, 1000), (5001, 10000, 0, 2001),
                    (100000, 300000, 0, 60000), (10000, 4999, 0, None),
                    (10000, 5000, 0, 1000), (10000, 100000, 89000, 1000),
                    (10000, 100000, 89500, None), (10000, 100000, 90000, None),
                ]:
                    with self.subTest(variant=name, cities=cities, troops=troops, food=food, target=target):
                        fixture = NativeFixture(self.pe, records, cities)
                        result = fixture.shipment(troops=troops, food=food, target_troops=target)
                        if expected is None:
                            self.assertIsNone(result)
                            continue
                        self.assertEqual(result['troops'], expected)
                        self.assertEqual((result['gold'], result['weapon'], result['weapon_amount']), (0, 0, 0))
                        self.assertEqual((result['leader'], result['source'], result['transport']), (77, SOURCE, 1))
                        self.assertEqual(result['reserve'], 3000)
                        self.assertGreaterEqual(troops - result['troops'], 3000)
                        self.assertGreaterEqual(food - result['food'], result['reserve_food'])
                        self.assertLessEqual(target + result['troops'], 90000)
                        self.assertNotIn(0x5e0760, fixture.calls)

    def test_frontline_reserve_and_native_formation_are_preserved(self):
        for name, records in self.variants():
            for n in (5, 7, 9):
                with self.subTest(variant=name, n=n):
                    fixture = NativeFixture(self.pe, records)
                    result = fixture.shipment(troops=n * 7500 + 5000, food=300000,
                                              gold=400, n=n, officers=n + 1)
                    self.assertIsNotNone(result)
                    self.assertEqual(result['reserve'], n * 7500)
                    self.assertEqual((result['troops'], result['weapon'], result['weapon_amount']), (5000, 1, 5000))
                    self.assertIn(0x5e0760, fixture.calls)
            self.assertIsNone(NativeFixture(self.pe, records).shipment(
                troops=50000, food=300000, gold=399, n=5, officers=6))

    def test_idle_officer_and_action_budget_are_still_required(self):
        for name, records in self.variants():
            for officers, ap in [(0, 10), (1, 0), (1, 9)]:
                with self.subTest(variant=name, officers=officers, ap=ap):
                    self.assertIsNone(NativeFixture(self.pe, records).shipment(officers=officers, ap=ap))

    def test_second_group_counts_first_group_against_target_capacity(self):
        for name, records in self.variants():
            with self.subTest(variant=name):
                fixture = NativeFixture(self.pe, records)
                first = fixture.shipment(troops=100000, food=300000)
                self.assertEqual(first['troops'], 60000)
                # Resume the native group loop after accounting for that queue.
                fixture.write(SOURCE + 4, 100000 - first['troops'])
                fixture.write(AI + 0x3290 + fixture.tail, 300000 - first['food'])
                fixture.write(STACK + 0x14, first['troops'])
                result = fixture.run(0x5f0ec5, {0x5f1183, 0x5f1066})
                self.assertEqual(result, 0x5f1066)
                second = fixture.read(STACK + 0x78)
                self.assertEqual(second, 30000)
                self.assertEqual(first['troops'] + second, 90000)

    def test_empty_eligible_list_cannot_create_a_shipment(self):
        for name, records in self.variants():
            with self.subTest(variant=name):
                fixture = NativeFixture(self.pe, records)
                fixture.write(AI + 0x3298 + fixture.tail, 0)
                self.assertIsNone(fixture.shipment())

    def test_available_weapon_cargo_is_optional_and_can_be_partial(self):
        for name, records in self.variants():
            for stock in (0, 100, 5000, 10000):
                with self.subTest(variant=name, stock=stock):
                    fixture = NativeFixture(self.pe, records)
                    fixture.stub(0x486950, 'cmp dword ptr [esp+4], 2; '
                                 f'mov eax, {stock}; je D; xor eax, eax; D: ret 4')
                    result = fixture.shipment(troops=10000, food=100000)
                    self.assertEqual(result['troops'], 7000)
                    self.assertEqual(result['weapon'], 2 if stock else 0)
                    self.assertEqual(result['weapon_amount'], min(stock, 7000))

    def test_port_supply_preserves_rear_floor(self):
        for name, records in self.variants():
            for n in (0, 3, 5):
                for troops in (2999, 3000, 5999, 6000):
                    with self.subTest(variant=name, n=n, troops=troops):
                        fixture = NativeFixture(self.pe, records)
                        fixture.write(AI + 410 * 0x34 + 0x4a, n, 2)
                        fixture.cpu.reg_write(UC_X86_REG_EDI, SOURCE)
                        fixture.cpu.reg_write(UC_X86_REG_EAX, troops)
                        result = fixture.run(0x5f4c23, {0x5f4c2e, 0x5f4e0a})
                        self.assertEqual(result == 0x5f4c2e, troops >= (6000 if n <= 3 else 3000))
                        self.assertEqual(fixture.cpu.reg_read(UC_X86_REG_ESP), STACK)

    def test_permission_safety_and_food_before_dispatch(self):
        for name, records in self.variants():
            for enabled, state, food in [(False, 0, 10000), (True, 0, 10000),
                                         (True, 3, 10000), (True, 0, 0)]:
                with self.subTest(variant=name, enabled=enabled, state=state, food=food):
                    fixture = NativeFixture(self.pe, records)
                    fixture.stub(0x5f65d0, f'mov eax, {int(not enabled)}; ret 8')
                    fixture.write(AI + 410 * 0x34 + 0x40, state, 1)
                    fixture.write(SOURCE + 8, food)
                    fixture.cpu.reg_write(UC_X86_REG_EDI, SOURCE)
                    fixture.cpu.reg_write(UC_X86_REG_EBX, 410)
                    result = fixture.run(0x5f4e8e, {0x5f4ecd, 0x5f4f0a})
                    self.assertEqual(result == 0x5f4ecd, enabled and state != 3 and food > 0)
                    self.assertEqual(fixture.cpu.reg_read(UC_X86_REG_ESP), STACK)

    def test_relay_strictly_approaches_valid_same_corps_destination(self):
        for name, records in self.variants():
            for n, dest, same_corps, distance, expected in [
                (3, 500, True, (4, 3), True), (3, 500, True, (4, 4), False),
                (3, 500, True, (4, 5), False), (3, 410, True, (0, 1), False),
                (0, 500, True, (4, 3), True), (0, 500, True, (4, 4), False),
                (0, 500, True, (4, 5), False), (0, 410, True, (0, 1), False),
                (0, 500, True, (4, -1), False),
                # Invalid/unset destinations and frontline sources use native depth.
                (0, -1, True, (4, 3), False), (0, 500, False, (4, 3), False),
                (5, 500, True, (4, 3), False),
            ]:
                for technology in (False, True):
                    with self.subTest(variant=name, n=n, dest=dest, same_corps=same_corps,
                                      distance=distance, technology=technology):
                        fixture = NativeFixture(self.pe, records)
                        source_record, target_record = AI + 410 * 0x34, AI + 476 * 0x34
                        fixture.write(source_record + 0x4a, n, 2)
                        fixture.write(source_record + 0x20, 4, 1)
                        fixture.write(target_record + 0x20, 4, 1)
                        fixture.write(STACK + 0x10, AI)
                        fixture.write(STACK + 0x1c, 0x5f0b33)
                        fixture.write(STACK + 0x24, 14)
                        fixture.write(AI + 8, FORCE)
                        fixture.write(CORPS + 0x20, dest)
                        fixture.write(TARGET + 0x1c, 14 if same_corps else 18)
                        fixture.stub(0x490ad0, f'mov eax, {CORPS}; ret 4')
                        fixture.stub(0x490d00, f'mov eax, {TARGET}; ret 4')
                        fixture.stub(0x4811e0, f'mov eax, {int(technology)}; ret 4')
                        for func in (0x4795c0, 0x4795f0):
                            fixture.stub(func, 'mov eax, [esp+4]; cmp eax, 410; '
                                         f'mov eax, {distance[0]}; je D; mov eax, {distance[1]}; D: ret')
                        fixture.cpu.reg_write(UC_X86_REG_EBX, source_record)
                        fixture.cpu.reg_write(UC_X86_REG_EBP, target_record)
                        fixture.cpu.reg_write(UC_X86_REG_EDI, 476)
                        result = fixture.run(0x5e4e1e, {0x5e4e2e, 0x5e4e38})
                        self.assertEqual(result == 0x5e4e2e, expected)
                        self.assertEqual(fixture.cpu.reg_read(UC_X86_REG_ESP), STACK)
                        if n <= 3 and dest >= 0 and same_corps:
                            self.assertIn(0x4795f0 if technology else 0x4795c0, fixture.calls)

    def test_rear_relay_can_receive_without_native_supply_flag(self):
        for name, records in self.variants():
            for troops, n, expected in [(0, 3, True), (89000, 3, True),
                                        (89500, 3, False), (0, 0, True), (89000, 0, True),
                                        (89500, 0, False), (0, 5, False)]:
                with self.subTest(variant=name, troops=troops, n=n):
                    fixture = NativeFixture(self.pe, records)
                    fixture.write(STACK + 0x1c, 410)
                    fixture.write(AI + 410 * 0x34 + 0x4a, n, 2)
                    fixture.write(TARGET + 4, troops)
                    fixture.stub(0x491f50, 'ret 4')
                    fixture.cpu.reg_write(UC_X86_REG_EDI, TARGET)
                    fixture.cpu.reg_write(UC_X86_REG_EBP, 476)
                    fixture.cpu.reg_write(UC_X86_REG_EBX, 0x12345678)
                    # Detect insertion without depending on the C++ list allocator.
                    result = fixture.run(0x5f0bc1, {0x491f50, 0x5f0c1c})
                    self.assertEqual(result == 0x491f50, expected)
                    if n <= 3:
                        self.assertEqual(fixture.cpu.reg_read(UC_X86_REG_EBX), 0x12345678)

    def test_shared_target_helper_keeps_other_callers_native(self):
        for name, records in self.variants():
            with self.subTest(variant=name):
                fixture = NativeFixture(self.pe, records)
                source_record, target_record = AI + 410 * 0x34, AI + 476 * 0x34
                fixture.write(source_record + 0x20, 4, 1)
                fixture.write(target_record + 0x20, 4, 1)
                fixture.write(STACK + 0x1c, 0x5f052d)
                fixture.cpu.reg_write(UC_X86_REG_EBX, source_record)
                fixture.cpu.reg_write(UC_X86_REG_EBP, target_record)
                result = fixture.run(0x5e4e1e, {0x5e4e2e, 0x5e4e38})
                self.assertEqual(result, 0x5e4e38)
                self.assertEqual(fixture.cpu.reg_read(UC_X86_REG_ESP), STACK)


if __name__ == '__main__':
    unittest.main()
