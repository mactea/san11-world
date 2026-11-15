"""Stage TRAD upgrades using the real C++ serializer; never writes original saves.

The caller deploys the validated staged files only after backing up originals.
Only expanded MWC saves with a terminal TRAD block are eligible. Legacy saves
without a trade block are left alone (the DLL initializes trade on load).
"""
import argparse
import ctypes as ct
import hashlib
import json
import struct
import subprocess
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def unpack_trad(block):
    magic, version, size = struct.unpack_from('<4sII', block)
    if magic != b'TRAD' or size != len(block)-12 or not 6 <= version <= 11:
        raise ValueError('unsupported or malformed TRAD block')
    raw_size = struct.unpack_from('<I', block, 12)[0]
    if not 0 < raw_size <= 128*1024*1024:
        raise ValueError('invalid TRAD raw size')
    api = ct.WinDLL('cabinet', use_last_error=True)
    api.CreateDecompressor.argtypes = [ct.c_uint32, ct.c_void_p, ct.POINTER(ct.c_void_p)]
    api.Decompress.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t)]
    api.CloseDecompressor.argtypes = [ct.c_void_p]
    handle = ct.c_void_p()
    if not api.CreateDecompressor(4, None, ct.byref(handle)):
        raise ct.WinError(ct.get_last_error())
    try:
        source = ct.create_string_buffer(block[16:])
        output = ct.create_string_buffer(raw_size)
        actual = ct.c_size_t()
        if not api.Decompress(handle, source, len(block)-16, output, raw_size, ct.byref(actual)) or actual.value != raw_size:
            raise ValueError('TRAD decompression failed')
        return version, output.raw
    finally:
        api.CloseDecompressor(handle)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    game, out = args.game.resolve(), args.out.resolve()
    if out == game or game in out.parents:
        raise ValueError('staging must be outside the game installation')
    out.mkdir(parents=True, exist_ok=True)
    manifest = []
    savedir = game/'UserData_kit/Koei/San11 Tc/SaveData'
    for path in sorted(savedir.glob('Save[0-9][0-9][0-9].s11')):
        original = path.read_bytes()
        record = {'save': path.name, 'source': str(path), 'before_sha256': sha(original)}
        if original[435:439] != b'MWC\x1a':
            record['status'] = 'unchanged: legacy container'
            manifest.append(record)
            continue
        raw_size, packed_size = struct.unpack_from('<II', original, 439)
        if len(original) != 447+packed_size:
            raise ValueError(f'{path.name}: unexpected container length')
        raw = zlib.decompress(original[447:])
        if len(raw) != raw_size:
            raise ValueError(f'{path.name}: bad raw length')
        at = raw.rfind(b'TRAD')
        while at >= 0 and (at+12 > len(raw) or at+12+struct.unpack_from('<I', raw, at+8)[0] != len(raw)):
            at = raw.rfind(b'TRAD', 0, at)
        if at < 0:
            record['status'] = 'unchanged: no terminal trade block'
            manifest.append(record)
            continue
        block = raw[at:]
        version, payload = unpack_trad(block)
        cities, goods = struct.unpack_from('<II', payload)
        if cities != 530 or goods not in (14, 69, 72):
            raise ValueError(f'{path.name}: unsupported world/catalogue')
        year = struct.unpack_from('<H', raw, 1)[0]
        days = year*360+(raw[3]-1)*30+raw[4]-1+struct.unpack_from('<I', raw, 60)[0]*10
        date = (days//360, days%360//30+1, days%30+1)
        old = ROOT/('worldmod/test/fixtures/trade-v4-14.bin' if goods == 14 else
                    'worldmod/test/fixtures/trade-v5-69.bin' if goods == 69 else 'data/world/trade.bin')
        source_block, upgraded_block = out/(path.stem+'-old-trad.bin'), out/(path.stem+'-new-trad.bin')
        source_block.write_bytes(block)
        cmd = [str(ROOT/'worldmod/build/trade_runtime_test.exe'), str(old), str(ROOT/'data/world/trade.bin'),
               str(source_block), *map(str, date), str(upgraded_block)]
        result = subprocess.run(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out/(path.stem+'-validation.txt')).write_bytes(result.stdout)
        if result.returncode:
            raise RuntimeError(f'{path.name}: actual-save validation failed; see staging log')
        upgraded = upgraded_block.read_bytes()
        new_version, new_payload = unpack_trad(upgraded)
        if new_version != 11 or struct.unpack_from('<II', new_payload) != (530, 72):
            raise ValueError('unexpected upgraded block')
        changed_raw = raw[:at]+upgraded
        packed = zlib.compress(changed_raw)
        changed = original[:439]+struct.pack('<II', len(changed_raw), len(packed))+packed
        reread = zlib.decompress(changed[447:])
        if changed[:435] != original[:435] or reread[:at] != raw[:at] or reread[at:] != upgraded:
            raise AssertionError('non-trade save data changed')
        target = out/path.name
        target.write_bytes(changed)
        if path.read_bytes() != original:
            raise RuntimeError('original save changed during staging')
        record.update(status='staged', staged=str(target), after_sha256=sha(changed),
                      version_before=version, version_after=new_version, goods_before=goods, goods_after=72,
                      date=date, native_world_sha256=sha(raw[:at]), native_world_unchanged=True,
                      original_trade_fields_preserved=True)
        manifest.append(record)
    (out/'save-upgrades.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    for row in manifest:
        print(row['save'], row['status'])


if __name__ == '__main__':
    main()
