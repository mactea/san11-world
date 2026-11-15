"""Add 3600 x 2200 to San11PK's in-game windowed resolution list.

Uses the supported executable's zero padding; keeps SizeOfImage unchanged so
the existing world mod build check and every other patch remain valid.
Run against a stopped game. A timestamped backup is made before replacement.
"""
import argparse
from datetime import datetime
from pathlib import Path
import shutil
import struct

from keystone import Ks, KS_ARCH_X86, KS_MODE_32
import pefile

STUB = 0x9c82600
MODE = STUB + 0xc0
# Title-screen and in-game environment settings have separate list builders.
# At each site ECX is the display-mode index, EAX is the hardware mode count.
SITES = [
    (0x4f4633, bytes.fromhex('0f8320020000'), 0x4f4639, 0x4f4643, 0x4f4859),
    (0x545daf, bytes.fromhex('0f833e020000'), 0x545db5, 0x545dbf, 0x545ff3),
]


def hook(index):
    _, _, original, initialize, end = SITES[index]
    # The existing two-pass outer loop is fullscreen=0, windowed=1.
    # Supply one extra D3DDISPLAYMODE to the windowed pass only. All existing
    # format, depth, desktop-size, duplicate and device checks still run.
    source = f'''
        jb {original}
        jne {end}
        cmp dword ptr [esp+0x20], 1
        jne {end}
        mov esi, {MODE}
        jmp {initialize}
    '''
    return bytes(Ks(KS_ARCH_X86, KS_MODE_32).asm(source, STUB+index*0x60)[0])


def stub():
    first, second = hook(0), hook(1)
    assert len(first) <= 0x60 and len(second) <= 0x60
    return first.ljust(0x60, b'\0') + second.ljust(0x60, b'\0') + struct.pack('<4I', 3600, 2200, 0, 22)


def patched(data):
    pe = pefile.PE(data=data, fast_load=True)
    assert pe.FILE_HEADER.TimeDateStamp == 1168334429
    assert pe.OPTIONAL_HEADER.ImageBase == 0x400000
    assert pe.OPTIONAL_HEADER.SizeOfImage == 0x9883000
    section = pe.sections[-1]
    offset = section.PointerToRawData + STUB-0x400000-section.VirtualAddress
    payload = stub()
    changes = []
    for index, (address, old, _, _, _) in enumerate(SITES):
        site = pe.get_offset_from_rva(address-0x400000)
        jump = b'\xe9' + struct.pack('<i', STUB+index*0x60-address-5) + b'\x90'
        changes.append((site, old, jump))
    if all(data[site:site+len(old)] == jump for site, old, jump in changes):
        assert data[offset:offset+len(payload)] == payload
        return data
    assert all(data[site:site+len(old)] == old for site, old, _ in changes), 'Unknown display-list code'
    end = section.PointerToRawData + section.SizeOfRawData
    assert end <= offset and offset+len(payload) <= len(data)
    assert not any(data[end:]), 'Executable padding is in use'
    extent = offset+len(payload)-section.PointerToRawData
    extent = (extent+pe.OPTIONAL_HEADER.FileAlignment-1) & -pe.OPTIONAL_HEADER.FileAlignment
    assert section.VirtualAddress+extent <= pe.OPTIONAL_HEADER.SizeOfImage
    assert section.Characteristics & 0x20000000
    out = bytearray(data)
    for site, old, jump in changes:
        out[site:site+len(old)] = jump
    out[offset:offset+len(payload)] = payload
    struct.pack_into('<I', out, section.get_file_offset()+8, extent)
    struct.pack_into('<I', out, section.get_file_offset()+16, extent)
    return bytes(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    original = args.exe.read_bytes()
    result = patched(original)
    if result == original:
        print('Already installed')
        return
    backup = args.exe.with_name(args.exe.name+'.before-window-resolution-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    shutil.copy2(args.exe, backup)
    temporary = args.exe.with_name(args.exe.name+'.window-resolution.tmp')
    temporary.write_bytes(result)
    temporary.replace(args.exe)
    assert args.exe.read_bytes() == result
    print(f'Installed windowed 3600 x 2200; backup: {backup}')


if __name__ == '__main__':
    main()
