"""Generate the reviewed portrait-capacity patches independently of map patches.

python tools/patchgen/gen_portraits.py
FCOUNT is the even capacity, FMETA the metadata allocation, FMAN the archive manager.
"""
import json
import re
import struct
from pathlib import Path

import capstone
import detour

PROJ = Path(__file__).resolve().parents[2]
IMAGE = 0x400000
META, MANAGER = 0x6FAE8B8, 0x6ED84C8


def main():
    data = (PROJ / 'backup/san11pk.exe.orig').read_bytes()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    insns = list(md.disasm_lite(data[0x1000:0x34df1a], 0x401000))
    byva = {a: (s, m, o) for a, s, m, o in insns}
    lines, audit = [], []

    def field(va, old, expr, note, raw=None):
        if raw is None:
            raw = data[va - IMAGE:va - IMAGE + byva[va][0]]
        needle = struct.pack('<I', old & 0xffffffff)
        assert raw.count(needle) == 1, (hex(va), raw.hex(), hex(old))
        offset = raw.index(needle)
        lines.append(f'{va:08x} {offset} 4 {raw.hex()} {expr} ; {note}')
        audit.append(dict(va=va, offset=offset, old=old, expr=expr, note=note))

    for va, size, mnemonic, operands in insns:
        values = {int(v, 16) for v in re.findall(r'0x([0-9a-f]+)', operands)}
        for old in values:
            if META <= old < META + 2400 * 4:
                expr = 'FMETA+4*FCOUNT-1' if old == META + 2400 * 4 - 1 else f'FMETA+{old - META}'
                field(va, old, expr, 'portrait metadata address / range endpoint')
            if old == MANAGER:
                field(va, old, 'FMAN', 'portrait archive manager singleton')
        if mnemonic == 'cmp' and operands.endswith(', 0x95f'):
            field(va, 2399, 'FCOUNT-1', 'portrait id upper bound')
        if 0x46dd60 <= va < 0x46e990:
            for old in values:
                if 0xe100 <= old <= 0xe2e4:
                    field(va, old, f'24*FCOUNT+{old - 0xe100}', 'file descriptors after 3*FCOUNT image records')
                elif old == 0x1c20:
                    field(va, old, '3*FCOUNT', 'archive image count / descriptor indexing')
                elif old == 0x3840:
                    field(va, old, '6*FCOUNT', 'zero image records, DWORD count')
                elif old == 0x4b0:
                    field(va, old, 'FCOUNT/2', 'small-image records start after large-image records')
                elif old == 0xfffff6a0:
                    field(va, old, '0-FCOUNT', 'small image index to face id')
                elif old == 2400:
                    field(va, old, 'FCOUNT', 'large / small image index boundary')
    field(0x73be21, 2400, 'FCOUNT', 'metadata static initializer')
    # Resource group 0: UI micro thumbnails; group 1: paired 64x80 images; group 2: 240x240 images.
    for va, old, expr in [(0x77b4a0, 4800, '2*FCOUNT'), (0x77b4bc, 2400, 'FCOUNT'),
                          (0x77b4c0, 4800, '2*FCOUNT'), (0x77b4e0, 2400, 'FCOUNT')]:
        field(va, old, expr, 'portrait texture-cache resource definition', data[va - IMAGE:va - IMAGE + 4])
    fields = [(x['va'] + x['offset']) for x in audit]
    assert len(fields) == len(set(fields)), 'duplicate field patches'

    build = PROJ / 'build'
    build.mkdir(exist_ok=True)
    listing = build / 'portraits.asm'
    listing.write_text('\n'.join(f'{a:08x} {data[a-IMAGE:a-IMAGE+s].hex()} {m} {o}'
                                 for a, s, m, o in insns), encoding='utf-8')
    blank = build / 'portraits_empty_patches.txt'
    blank.write_text('')
    detour.LISTING = str(listing)
    builder = detour.Builder(patch_files=[str(blank)])
    # Native young/old portrait pairs occupy 0..1999. Custom faces have no paired age variant.
    # Preserve EDX as the result because some callers consume it directly after IDIV.
    modulo = [0x52a5c3, 0x546d7a, 0x5484a9, 0x548717, 0x548973, 0x549411]
    for va in modulo:
        block = [va, va + 5, va + 6, va + 11]
        assert [byva[a][1] for a in block] == ['add', 'cdq', 'mov', 'idiv']
        replacement = ('cmp eax, 2000; jl legacy; mov edx, eax; xor eax, eax; mov ecx, 2000; '
                       'jmp done; legacy: add eax, 1000; cdq; mov ecx, 2000; idiv ecx; done:')
        builder.detour(va, {block[0]: replacement, **{a: '' for a in block[1:]}},
                       'custom portraits retain identity when toggling young / old', min_len=13)
    va = 0x63bf56
    builder.detour(va, {va: ('cmp edi, 2000; jl legacy; mov edx, edi; xor eax, eax; mov ecx, 1000; '
                            'jmp done; legacy: mov eax, edi; cdq; mov ecx, 1000; idiv ecx; done:'),
                        va + 2: '', va + 3: '', va + 8: ''},
                   'custom portraits retain identity in historical officer detail', min_len=10)
    builder.detour(0x59c9be, {0x59c9be: 'cmp eax, 2000; jge done; add eax, 1000; done:'},
                   'aging does not change a custom portrait id')
    (PROJ / 'worldmod/worldmod_portraits.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    builder.write(str(PROJ / 'worldmod/worldmod_portraits_caves.txt'))
    (build / 'portrait_capacity_fields.json').write_text(json.dumps(audit, indent=1), encoding='utf-8')
    print(f'{len(lines)} field patches; {len(builder.records)} portrait caves')


if __name__ == '__main__':
    main()
