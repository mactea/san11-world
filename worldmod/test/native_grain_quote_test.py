"""Read-only x86 execution of the installed game's native grain quote.

Requires pefile and unicorn. Only object lookup/validity calls are stubbed;
the quote, signed 64-bit multiply/divide and clamp execute original bytes.
This does not execute the trade command, UI, action cost or runtime patches.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP


def quote(rate, politics, food):
    if not 1 <= rate <= 255 or not 0 <= politics <= 255:
        raise ValueError('invalid native grain rate or politics')
    factor = 450 - politics
    value = (food * factor * 10 // (rate * 400) if food >= 0
             else -((-food) * 3200 // (rate * factor)))
    return min(2147483647, max(-2147483648, value))


class NativeQuote:
    def __init__(self, exe):
        self.data = Path(exe).read_bytes()
        self.pe = pefile.PE(data=self.data, fast_load=True)
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        # Quote + clamp, arithmetic helpers, object accessors.
        for page in (0x5ca000, 0x70a000, 0x47a000, 0x486000,
                     0x491000, 0x490000, 0x489000, 0x4ae000, 0x487000):
            self.uc.mem_map(page, 4096)
            self.uc.mem_write(page, self.bytes(page, 4096))
        for page in (0x10000000, 0x20000000, 0x30000000):
            self.uc.mem_map(page, 4096)
        self.uc.hook_add(UC_HOOK_CODE, self.stub)
        self.politics = 0
        self.weapons, self.weapon_cap = 0, 1000

    def bytes(self, va, size):
        offset = self.pe.get_offset_from_rva(va - self.pe.OPTIONAL_HEADER.ImageBase)
        return self.data[offset:offset + size]

    def stub(self, uc, address, size, context):
        calls = {
            0x47a630: (1, 0),           # valid object, cdecl
            0x486660: (1, 4),           # eligible base
            0x491770: (0, 4),           # base -> city id
            0x490a10: (0x10000000, 4),  # city record
            0x4890a0: (self.politics, 0),
            0x486950: (self.weapons, 4),
            0x486a30: (self.weapon_cap, 4),
            0x4874d0: (0, 8),
        }
        if address not in calls:
            return
        value, popped = calls[address]
        sp = uc.reg_read(UC_X86_REG_ESP)
        if address == 0x4874d0:
            weapon, self.weapons = struct.unpack('<ii', uc.mem_read(sp + 4, 8))
            if weapon != 1:
                raise AssertionError('incorrect native weapon argument order')
        ret = struct.unpack('<I', uc.mem_read(sp, 4))[0]
        uc.reg_write(UC_X86_REG_EAX, value)
        uc.reg_write(UC_X86_REG_ESP, sp + 4 + popped)
        uc.reg_write(UC_X86_REG_EIP, ret)

    def run(self, rate, politics, food):
        self.politics = politics
        self.uc.mem_write(0x1000007c, bytes([rate]))
        self.uc.mem_write(0x20000800, struct.pack('<IIIi',
                          0x30000000, 0x10000000, 0x10000100, food))
        self.uc.reg_write(UC_X86_REG_ESP, 0x20000800)
        self.uc.emu_start(0x5ca620, 0x30000000, count=2000)
        if self.uc.reg_read(UC_X86_REG_EIP) != 0x30000000:
            raise AssertionError('native function did not return')
        return struct.unpack('<i', struct.pack('<I', self.uc.reg_read(UC_X86_REG_EAX)))[0]

    def change_weapons(self, current, capacity, delta):
        self.weapons, self.weapon_cap = current, capacity
        self.uc.mem_write(0x20000800, struct.pack('<IIIi', 0x30000000, 0x10000000, 1, delta))
        self.uc.reg_write(UC_X86_REG_ESP, 0x20000800)
        self.uc.emu_start(0x4ae430, 0x30000000, count=2000)
        actual = struct.unpack('<i', struct.pack('<I', self.uc.reg_read(UC_X86_REG_EAX)))[0]
        if self.uc.reg_read(UC_X86_REG_EIP) != 0x30000000:
            raise AssertionError('native weapon function did not return')
        return actual, self.weapons


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    native = NativeQuote(args.exe)
    cases = {(r, p, q) for r in (1, 10, 30, 50, 80, 100, 255)
             for p in (0, 50, 80, 93, 100, 120, 255)
             for q in (0, 1, -1, 99, -99, 1000, -1000, 1000000, -1000000,
                       2147483647, -2147483647)}
    cases.update((r, 100, q) for r in range(1, 256) for q in (12345, -12345))
    cases.update((50, p, q) for p in range(256) for q in (12345, -12345))
    for rate, politics, food in sorted(cases):
        actual, expected = native.run(rate, politics, food), quote(rate, politics, food)
        if actual != expected:
            raise AssertionError((rate, politics, food, actual, expected))
    weapon_cases = 0
    for current in (0, 150, 950, 1000):
        for delta in (-2000, -200, -1, 0, 1, 100, 500, 2000):
            final = max(0, min(1000, current + delta))
            if native.change_weapons(current, 1000, delta) != (final-current, final):
                raise AssertionError(('native weapon clamp', current, delta))
            weapon_cases += 1
    rows = [{'rate': r, 'politics': p, 'food': 1000,
             'buy_gold': quote(r, p, 1000), 'sell_gold': -quote(r, p, -1000)}
            for r in (30, 50, 80) for p in (50, 80, 100, 120)]
    report = {
        'exe': str(args.exe.resolve()), 'exe_sha256': hashlib.sha256(native.data).hexdigest(),
        'quote_address': '0x5ca620',
        'quote_sha256': hashlib.sha256(native.bytes(0x5ca620, 0x14b)).hexdigest(),
        'status': 'PASS', 'emulated_cases': len(cases), 'examples': rows,
        'weapon_api_address': '0x4ae430', 'weapon_clamp_cases': weapon_cases,
        'scope': 'Original quote arithmetic only; object lookup stubbed. No live game or save writes.',
        'native_roundtrip_note': 'High politics can quote sell above buy; action/UI limits not tested. Do not treat native buy price as a no-arbitrage floor.',
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'native_grain_quote_test: PASS ({len(cases)} original-code executions)')
    print(f'native weapon API: PASS ({weapon_cases} original clamp/return executions; stock accessors stubbed)')
    for row in rows:
        print(row)


if __name__ == '__main__':
    main()
