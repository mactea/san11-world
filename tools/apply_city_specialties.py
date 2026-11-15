"""Apply the reviewed city plan to installed scenarios, saves, and a running game.

Backups and a manifest are created before writing. Only the six specialty flags
of new cities are changed; save worlds are decompressed and recompressed.
"""
import argparse
import ctypes as ct
import hashlib
import json
import struct
import zlib
from datetime import datetime
from pathlib import Path

from convert_scen import scen_layout

ROOT = Path(__file__).resolve().parents[1]
KINDS = ['枪', '戟', '弩', '军马', '兵器', '舰船']


def marks(data):
    mark = data[44:48]
    assert mark == b'WIDE' or mark[:2] == b'WF', mark
    regular = struct.unpack('<H', mark[2:])[0] if mark[:2] == b'WF' else 42
    persons = struct.unpack_from('<H', data, 34)[0] if data[33] == 80 else 850
    assert data[8:18] == b'KOEI%SAN11'
    assert struct.unpack_from('<I', data, 36)[0] == 0
    return regular, persons


def scenario_offset(data, count):
    regular, persons = marks(data)
    layout, end = scen_layout(count, count + 45, wide=True, R=regular)
    shift = (persons - 850) * 152
    assert len(data) == end + shift, 'scenario layout mismatch'
    return layout['city'][0] + shift


def patch_flags(data, offset, stride, flag_offset, plan):
    result = bytearray(data)
    for city in plan:
        start = offset + stride * city['city_id'] + flag_offset
        before = data[start:start + 6]
        assert len(before) == 6 and all(v in (0, 1) for v in before)
        kind = city['specialty']
        result[start:start + 6] = bytes(int(k == kind) for k in KINDS)
    # Restoring just the allowed ranges must reproduce the entire input.
    restored = bytearray(result)
    for city in plan:
        start = offset + stride * city['city_id'] + flag_offset
        restored[start:start + 6] = data[start:start + 6]
    assert restored == data, 'unrelated data changed'
    return bytes(result)


def save_world(data, count, originals):
    regular, _ = marks(data)
    assert struct.unpack_from('<I', data, 4)[0] == 0x1a
    assert data[435:439] == b'MWC\x1a'
    raw_size, size = struct.unpack_from('<II', data, 439)
    assert len(data) == 447 + size
    decoder = zlib.decompressobj()
    world = decoder.decompress(data[447:])
    assert decoder.eof and not decoder.unused_data and len(world) == raw_size
    # M8 also expands the world's per-force turn-order byte array.
    building = 2212 + regular - 42
    assert all(world[building + 18 * i] == 0 for i in range(count))
    assert world[building + 18 * count] != 0, 'city count mismatch'
    # Recognize all 42 original specialty records, independently of officer,
    # force, and trade extensions that precede or follow the city block.
    hits = []
    at = -1
    while True:
        at = world.find(originals[0], at + 1)
        if at < 0:
            break
        if at >= 77 and all(world[at + 108 * i:at + 108 * i + 6] == v
                            for i, v in enumerate(originals)):
            hits.append(at - 77)
    assert len(hits) == 1, f'no unique city block: {hits}'
    assert hits[0] + count * 108 <= len(world)
    return world, hits[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--pid', type=int, help='also update the running world executable')
    args = ap.parse_args()
    cfg = json.loads((ROOT / 'project.json').read_text(encoding='utf-8'))
    game = Path(cfg['game_dir'])
    count = cfg['cities']
    plan = json.loads((ROOT / 'data/city_specialties_plan.json').read_text(encoding='utf-8'))['cities']
    assert sorted(c['city_id'] for c in plan) == list(range(42, count))
    deployed = json.loads((game / cfg['data_name'] / 'bases_cities.json').read_text(encoding='utf-8'))
    assert deployed['C'] == count
    assert {c['id']: c['name'] for c in deployed['new']} == {c['city_id']: c['name'] for c in plan}
    data_dir = game / cfg['data_name']
    reference = (data_dir / 'scenario/Scen000.s11').read_bytes()
    offset = scenario_offset(reference, count)
    originals = [reference[offset + i * 81 + 75:offset + i * 81 + 81] for i in range(42)]
    # Verify the flag mapping against six known stock cities.
    for city, kind in [(11, 0), (36, 1), (24, 2), (21, 3), (18, 4), (23, 5)]:
        assert originals[city] == bytes(int(i == kind) for i in range(6))
    pending = []
    for folder in ['scenario', 'scenario_base']:
        for path in sorted((data_dir / folder).glob('*')):
            if path.suffix.lower() != '.s11':
                continue
            before = path.read_bytes()
            if struct.unpack_from('<I', before, 4)[0] != 0x16:
                continue
            start = scenario_offset(before, count)
            after = patch_flags(before, start, 81, 75, plan)
            pending.append((path, before, after))
    for path in sorted((game / cfg['save_dir']).rglob('Save[0-9][0-9][0-9].s11')):
        before = path.read_bytes()
        world, start = save_world(before, count, originals)
        changed = patch_flags(world, start, 108, 77, plan)
        packed = zlib.compress(changed, 9)
        after = before[:439] + struct.pack('<II', len(changed), len(packed)) + packed
        reread, check_start = save_world(after, count, originals)
        assert reread == changed and check_start == start
        assert after[:435] == before[:435]
        pending.append((path, before, after))
    assert pending
    backup = game / ('specialty-backups/' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True, exist_ok=False)
    manifest = {'files': [], 'live': None}
    for path, before, after in pending:
        target = backup / path.relative_to(game)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(before)
        assert target.read_bytes() == before
        manifest['files'].append({'path': str(path), 'before_sha256': hashlib.sha256(before).hexdigest(),
                                  'after_sha256': hashlib.sha256(after).hexdigest()})
    if args.pid:
        api = ct.WinDLL('kernel32', use_last_error=True)
        api.OpenProcess.restype = ct.c_void_p
        api.ReadProcessMemory.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t)]
        api.WriteProcessMemory.argtypes = api.ReadProcessMemory.argtypes
        api.CloseHandle.argtypes = [ct.c_void_p]
        handle = api.OpenProcess(0x438, False, args.pid)
        assert handle, ct.get_last_error()
        def read(address, size):
            buf, actual = ct.create_string_buffer(size), ct.c_size_t()
            assert api.ReadProcessMemory(handle, address, buf, size, ct.byref(actual)), ct.get_last_error()
            assert actual.value == size
            return buf.raw
        try:
            # The installed M7 patch redirects this city-array LEA displacement.
            assert read(0x4938ab, 2) == b'\x8d\xb3'
            base = (struct.unpack('<I', read(0x4938ad, 4))[0] + 0x7201958) & 0xffffffff
            live = read(base, count * 0x248)
            assert all(live[i * 0x248 + 0x86:i * 0x248 + 0x8c] == v for i, v in enumerate(originals))
            changed = patch_flags(live, 0, 0x248, 0x86, plan)
            (backup / 'live_city_array_before.bin').write_bytes(live)
            manifest['live'] = {'pid': args.pid, 'city_array': hex(base), 'cities': len(plan)}
            # Recheck disk sources immediately before committing.
            assert all(path.read_bytes() == before for path, before, _ in pending)
            for city in plan:
                start = city['city_id'] * 0x248 + 0x86
                actual = ct.c_size_t()
                assert api.WriteProcessMemory(handle, base + start, changed[start:start + 6], 6, ct.byref(actual)), ct.get_last_error()
                assert actual.value == 6
            assert read(base, len(live)) == changed
        finally:
            api.CloseHandle(handle)
    for path, before, after in pending:
        assert path.read_bytes() == before, f'file changed concurrently: {path}'
        temp = path.with_suffix('.specialties.tmp')
        temp.write_bytes(after)
        assert temp.read_bytes() == after
        temp.replace(path)
        assert path.read_bytes() == after
    (backup / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'backup': str(backup), 'files': len(pending), 'live': manifest['live']}, ensure_ascii=False))


if __name__ == '__main__':
    main()
