"""Execute patched portrait functions and FACE loading in Unicorn before deployment."""
import json
import struct
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP

PROJ = Path(__file__).resolve().parents[2]
IMAGE, META, MANAGER = 0x400000, 0x20000000, 0x21000000
STACK, STOP, CAVES, RECORD = 0x30000000, 0x300ff000, 0x22000000, 0x23000000


def expression(text, symbols):
    return int(eval(text.replace('/', '//'), {'__builtins__': {}}, symbols))


def main():
    data = (PROJ / 'backup/san11pk.exe.orig').read_bytes()
    fields = json.loads((PROJ / 'build/portrait_capacity_fields.json').read_text())
    baseline = dict(FCOUNT=2400, FMETA=0x6fae8b8, FMAN=0x6ed84c8)
    assert all(expression(f['expr'], baseline) & 0xffffffff == f['old'] & 0xffffffff for f in fields)
    cases = 0
    results = []
    for count in (2400, 16000, 32760):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(IMAGE, 0x500000)
        uc.mem_write(IMAGE, data[:0x500000])
        for base, size in [(META, 0x40000), (MANAGER, 0x100000), (STACK, 0x100000),
                           (CAVES, 0x100000), (RECORD, 0x10000)]:
            uc.mem_map(base, size)
        symbols = dict(FCOUNT=count, FMETA=META, FMAN=MANAGER)
        for f in fields:
            value = expression(f['expr'], symbols) & 0xffffffff
            uc.mem_write(f['va'] + f['offset'], struct.pack('<I', value))
        cave_cursor = CAVES
        for line in (PROJ / 'worldmod/worldmod_portraits_caves.txt').read_text().splitlines():
            tokens = line.split(';')[0].split()
            _, site_s, stolen_s, old_s, code_s, *fixups = tokens
            site, stolen = int(site_s, 16), int(stolen_s)
            assert data[site - IMAGE:site - IMAGE + stolen].hex() == old_s
            code = bytearray.fromhex(code_s)
            for fx in fixups:
                off_s, target = fx[1:].split('=rel:')
                off = int(off_s)
                struct.pack_into('<I', code, off, (int(target, 16) - cave_cursor - off - 4) & 0xffffffff)
            uc.mem_write(cave_cursor, bytes(code))
            uc.mem_write(site, b'\xe9' + struct.pack('<I', (cave_cursor - site - 5) & 0xffffffff) + b'\x90' * (stolen - 5))
            cave_cursor += (len(code) + 15) & ~15

        def invoke(va, args=(), this=MANAGER, end=STOP, eax=0, edi=0):
            nonlocal cases
            sp = STACK + 0x80000
            uc.mem_write(sp, struct.pack('<' + 'I' * (len(args) + 1), STOP, *args))
            for reg, value in [(UC_X86_REG_ESP, sp), (UC_X86_REG_ECX, this), (UC_X86_REG_EAX, eax), (UC_X86_REG_EDI, edi)]:
                uc.reg_write(reg, value)
            uc.emu_start(va, end, count=1000000)
            cases += 1
            return uc.reg_read(UC_X86_REG_EAX)

        # Constructors really clear the enlarged regions, without touching guard bytes.
        for base, size in [(META, count * 4), (MANAGER, count * 24 + 0x1e8)]:
            uc.mem_write(base, b'\x7f' * (size + 16))
        invoke(0x73be20)
        invoke(0x46dd60)
        assert bytes(uc.mem_read(META, count * 4)) == bytes(count * 4)
        assert bytes(uc.mem_read(META + count * 4, 16)) == b'\x7f' * 16
        assert bytes(uc.mem_read(MANAGER + count * 24 + 0x1e8, 16)) == b'\x7f' * 16

        for face in [0, 1999, 2196, 2399, count - 1]:
            uc.mem_write(META + face * 4, bytes([30, 20, 60, 2]))
            uc.mem_write(RECORD + 0x3c, struct.pack('<I', face))
            assert invoke(0x48a450, this=RECORD) == face
            assert invoke(0x4802a0, (face,)) == 1
            invoke(0x4892d0, (face,), this=RECORD)
            assert struct.unpack('<I', uc.mem_read(RECORD + 0x3c, 4))[0] == face
        assert invoke(0x4802a0, (count,)) == 0
        invoke(0x4892d0, (count,), this=RECORD)
        assert struct.unpack('<I', uc.mem_read(RECORD + 0x3c, 4))[0] == count - 1

        for site in [0x52a5c3, 0x546d7a, 0x5484a9, 0x548717, 0x548973, 0x549411]:
            for face in [0, 999, 1000, 1999, 2196, count - 1]:
                invoke(site, end=site + 13, eax=face)
                assert uc.reg_read(UC_X86_REG_EDX) == ((face + 1000) % 2000 if face < 2000 else face)
        for face in [0, 1000, 1999, 2196, count - 1]:
            invoke(0x63bf56, end=0x63bf60, edi=face)
            assert uc.reg_read(UC_X86_REG_EDX) == (face % 1000 if face < 2000 else face)
            expected = face + 1000 if face < 2000 else face
            assert invoke(0x59c9be, end=0x59c9c3, eax=face) == expected

        # Execute the actual FACE loader against an in-memory file. Only CRT file IO is stubbed.
        first = count - 1
        textures = [b'LARGE-TEST', b'SMALL-A', b'SMALL-B']
        offset = 48
        entries = []
        for blob in textures:
            entries.append(struct.pack('<II', offset, len(blob)))
            offset += len(blob)
        file_data = b'FACE' + struct.pack('<4I', 64, first, 1, 3) + bytes([30, 20, 60, 2]) + b''.join(entries) + b''.join(textures)
        cursor = 0

        def ret(value, cleanup=0):
            sp = uc.reg_read(UC_X86_REG_ESP)
            pc = struct.unpack('<I', uc.mem_read(sp, 4))[0]
            uc.reg_write(UC_X86_REG_EAX, value)
            uc.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
            uc.reg_write(UC_X86_REG_EIP, pc)

        def io_hook(uc_, address, size, user):
            nonlocal cursor
            if address not in (0x70891d, 0x708462, 0x708649, 0x46ddf0):
                return
            sp = uc.reg_read(UC_X86_REG_ESP)
            args = struct.unpack('<4I', uc.mem_read(sp + 4, 16))
            if address == 0x70891d:
                cursor = 0
                ret(0x12345678)
            elif address == 0x708462:
                assert args[2] == 0
                cursor = args[1]
                ret(0)
            elif address == 0x708649:
                dst, width, n, handle = args
                blob = file_data[cursor:cursor + width * n]
                uc.mem_write(dst, blob)
                cursor += len(blob)
                ret(len(blob) // width)
            else:
                bank, off, n, dst = args
                blob = file_data[off:off + n]
                assert len(blob) == n
                uc.mem_write(dst, blob)
                ret(1, 16)

        hook = uc.hook_add(UC_HOOK_CODE, io_hook)
        assert invoke(0x46e360, (0, RECORD + 0x100)) == 1
        assert bytes(uc.mem_read(META + first * 4, 4)) == bytes([30, 20, 60, 2])
        indices = [first, count + first * 2, count + first * 2 + 1]
        for index, expected in zip(indices, entries):
            assert bytes(uc.mem_read(MANAGER + index * 8, 8)) == expected
        uc.mem_write(MANAGER + count * 24 + 0x1e0, struct.pack('<I', 1))
        for index, blob in zip(indices, textures):
            assert invoke(0x46de40, (index,)) == 0
            assert invoke(0x46df30, (index, RECORD + 0x200, len(blob), 0)) == 1
            assert bytes(uc.mem_read(RECORD + 0x200, len(blob))) == blob
        deployed = PROJ / 'data/persons/portraits/batches/0001_installed.json'
        if count == 16000 and deployed.exists():
            actual_bank = Path(json.loads(deployed.read_text(encoding='utf-8'))['banks'][0])
            file_data = actual_bank.read_bytes()
            invoke(0x46dd60)
            assert invoke(0x46e360, (0, RECORD + 0x100)) == 1
            uc.mem_write(MANAGER + count * 24 + 0x1e0, struct.pack('<I', 1))
            catalog = json.loads((PROJ / 'data/persons/portraits/catalog.json').read_text(encoding='utf-8'))
            bank_first, bank_count = struct.unpack_from('<II', file_data, 8)
            faces = sorted(p['face'] for p in catalog if bank_first <= p['face'] < bank_first + bank_count)
            for face in faces:
                assert uc.mem_read(META + face * 4 + 3, 1)[0] & 2
                for index, dimensions in [(face, (240, 240)), (count + face * 2, (64, 80)),
                                          (count + face * 2 + 1, (64, 80))]:
                    assert invoke(0x46de40, (index,)) == 0
                    assert invoke(0x46df30, (index, RECORD + 0x200, 24, 0)) == 1
                    header = bytes(uc.mem_read(RECORD + 0x200, 24))
                    assert header[:8] == b'WFTX0010'
                    assert struct.unpack_from('<HH', header, 16) == dimensions
        uc.hook_del(hook)
        results.append(dict(capacity=count, face_header_and_three_images='pass',
                            metadata_get_set_and_bounds='pass', custom_age_variants='pass', allocation_guards='pass'))
    report = dict(cases=cases, native_capacity_fields_match_original=len(fields), results=results)
    (PROJ / 'build/portrait_capacity_verification.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report))


if __name__ == '__main__':
    main()
