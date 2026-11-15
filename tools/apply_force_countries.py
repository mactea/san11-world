"""Apply the approved ruler-to-country plan without changing ranks or other state.

Matches rulers by their stored names, so historical scenarios and older saves
can have different force/officer IDs. Backs up every changed file and live array.
"""
import argparse
import ctypes as ct
import hashlib
import json
import struct
import zlib
from datetime import datetime
from pathlib import Path

from apply_city_specialties import marks, save_world, scenario_offset
from convert_scen import scen_layout

ROOT = Path(__file__).resolve().parents[1]


def name(data):
    return data.split(b'\0')[0].decode('cp950', 'replace')


def apply(data, layout, plan):
    fo, fn, fs, ruler_size, country_field, country_size = layout['force']
    po, pn, ps, person_name = layout['person']
    co, cs, country_name = layout['country']
    result = bytearray(data)
    names = [name(data[co + i * cs + country_name:co + i * cs + country_name + 5]) for i in range(84)]
    rulers = []
    used = {i for i, value in enumerate(names) if value in set(plan.values())}
    for i in range(fn):
        pos = fo + i * fs
        ruler_at = pos + (4 if ruler_size == 4 else 0)
        ruler = int.from_bytes(data[ruler_at:ruler_at + ruler_size], 'little', signed=True)
        country = int.from_bytes(data[pos + country_field:pos + country_field + country_size], 'little', signed=True)
        if not 0 <= ruler < pn:
            continue
        if 0 <= country < 84:
            used.add(country)
        p = po + ruler * ps + person_name
        rulers.append((i, name(data[p:p + 5]) + name(data[p + 5:p + 10])))
    allowed = []
    changes = []
    for fid, ruler in rulers:
        if ruler not in plan:
            continue
        desired = plan[ruler]
        if desired in names:
            cid = names.index(desired)
        else:
            cid = next(i for i in range(42, 84) if i not in used)
            value = desired.encode('cp950').ljust(5, b'\0')
            assert len(value) == 5
            at = co + cid * cs + country_name
            result[at:at + 5] = value
            names[cid] = desired
            allowed.append((at, 5))
        used.add(cid)
        at = fo + fid * fs + country_field
        result[at:at + country_size] = cid.to_bytes(country_size, 'little', signed=True)
        allowed.append((at, country_size))
        changes.append({'force': fid, 'ruler': ruler, 'country': desired, 'country_id': cid})
    restored = bytearray(result)
    for at, size in allowed:
        restored[at:at + size] = data[at:at + size]
    assert restored == data, 'unrelated bytes changed'
    for row in changes:
        at = fo + row['force'] * fs + country_field
        cid = int.from_bytes(result[at:at + country_size], 'little', signed=True)
        at = co + cid * cs + country_name
        assert name(result[at:at + 5]) == row['country']
    return bytes(result), changes, allowed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    args = parser.parse_args()
    cfg = json.loads((ROOT / 'project.json').read_text(encoding='utf-8'))
    game = Path(cfg['game_dir'])
    count = cfg['cities']
    data_dir = game / cfg['data_name']
    rows = json.loads((ROOT / 'data/force_country_plan.json').read_text(encoding='utf-8'))['forces']
    plan = {row['leader']: row['country'] for row in rows}
    assert len(plan) == len(rows) == 44
    reference = (data_dir / 'scenario/Scen000.s11').read_bytes()
    start = scenario_offset(reference, count)
    originals = [reference[start + i * 81 + 75:start + i * 81 + 81] for i in range(42)]
    pending = []
    for folder in ['scenario', 'scenario_base']:
        for path in sorted((data_dir / folder).glob('*')):
            if path.suffix.lower() != '.s11':
                continue
            before = path.read_bytes()
            if struct.unpack_from('<I', before, 4)[0] != 0x16:
                continue
            regular, persons = marks(before)
            sections, end = scen_layout(count, count + 45, wide=True, R=regular)
            shift = (persons - 850) * 152
            assert len(before) == end + shift
            f, fn, fs = sections['force']
            layout = {'force': (f + shift, fn, fs, 2, 5 + fn, 1),
                      'person': (sections['person'][0], persons, 152, 0),
                      'country': (sections['type14'][0] + shift, 89, 0)}
            after, changes, _ = apply(before, layout, plan)
            pending.append((path, before, after, changes))
    for path in sorted((game / cfg['save_dir']).rglob('Save[0-9][0-9][0-9].s11')):
        before = path.read_bytes()
        world, city = save_world(before, count, originals)
        regular, persons = marks(before)
        fn = regular + 5
        fs = 214 + 2 * (fn - 47) + 4 * ((fn + 31) // 32 - 2)
        po = 2212 + regular - 42 + 16384 * 18
        fo = city - fn * (fs + 17)
        assert fo == po + (persons + 250) * 203 + 100 * 58
        layout = {'force': (fo, fn, fs, 2, 5 + fn, 1),
                  'person': (po, persons + 250, 203, 0),
                  'country': (city + count * 108 + 45 * 70, 89, 0)}
        changed, changes, _ = apply(world, layout, plan)
        compressed = zlib.compress(changed, 9)
        after = before[:439] + struct.pack('<II', len(changed), len(compressed)) + compressed
        reread, check_city = save_world(after, count, originals)
        assert reread == changed and city == check_city and before[:435] == after[:435]
        pending.append((path, before, after, changes))
    api = ct.WinDLL('kernel32', use_last_error=True)
    ptr, size = ct.c_void_p, ct.c_size_t
    api.OpenProcess.restype = ptr
    api.ReadProcessMemory.argtypes = [ptr, ptr, ptr, size, ct.POINTER(size)]
    api.WriteProcessMemory.argtypes = api.ReadProcessMemory.argtypes
    api.CloseHandle.argtypes = [ptr]
    handle = api.OpenProcess(0x1f0fff, False, args.pid)
    assert handle, ct.get_last_error()
    def read(address, length):
        buf, got = ct.create_string_buffer(length), size()
        assert api.ReadProcessMemory(handle, address, buf, length, ct.byref(got))
        assert got.value == length
        return buf.raw
    def write(address, value):
        got = size()
        assert api.WriteProcessMemory(handle, address, value, len(value), ct.byref(got))
        assert got.value == len(value)
    try:
        world_base = 0x7201958
        assert read(0x490fc4, 2) == b'\x8d\xb9'
        force_base = (struct.unpack('<I', read(0x490fc6, 4))[0] + world_base) & 0xffffffff
        # The running executable logs PERSONARR, and its installed getter also
        # exposes the relocated array through the M10 patch displacement.
        import re
        log = (game / 'worldmod_san11pk_world.log').read_text(errors='replace')
        person_base = int(re.findall(r'PERSONARR ([0-9A-Fa-f]+)', log)[-1], 16)
        fn = cfg['forces'] + 5
        pn = int(re.findall(r'persons: PS=\d+ PT=(\d+)', log)[-1])
        force_data = read(force_base, fn * 0x12c)
        person_data = read(person_base, pn * 0x190)
        country_base = world_base + 0x7ecd8
        country_data = read(country_base, 84 * 0x68)
        joined = force_data + person_data + country_data
        co = len(force_data) + len(person_data)
        layout = {'force': (0, fn, 0x12c, 4, 0x40, 4),
                  'person': (len(force_data), pn, 0x190, 4),
                  'country': (co, 0x68, 4)}
        changed, live_changes, allowed = apply(joined, layout, plan)
        assert len(live_changes) == 44
        backup = game / ('country-backups/' + datetime.now().strftime('%Y%m%d-%H%M%S'))
        backup.mkdir(parents=True, exist_ok=False)
        manifest = {'live': live_changes, 'files': []}
        (backup / 'live_forces_before.bin').write_bytes(force_data)
        (backup / 'live_countries_before.bin').write_bytes(country_data)
        for path, before, after, changes in pending:
            dest = backup / path.relative_to(game)
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(before)
            assert dest.read_bytes() == before
            manifest['files'].append({'path': str(path), 'changes': changes,
                                      'before_sha256': hashlib.sha256(before).hexdigest(),
                                      'after_sha256': hashlib.sha256(after).hexdigest()})
        (backup / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
        native = ct.WinDLL('ntdll')
        native.NtSuspendProcess.argtypes = [ptr]
        native.NtResumeProcess.argtypes = [ptr]
        assert native.NtSuspendProcess(handle) == 0
        try:
            assert read(force_base, len(force_data)) == force_data
            assert read(country_base, len(country_data)) == country_data
            assert all(path.read_bytes() == before for path, before, _, _ in pending)
            for at, length in allowed:
                address = force_base + at if at < len(force_data) else country_base + at - co
                assert at < len(force_data) or at >= co
                write(address, changed[at:at + length])
            assert read(force_base, len(force_data)) == changed[:len(force_data)]
            assert read(country_base, len(country_data)) == changed[co:]
            for path, before, after, changes in pending:
                temp = path.with_suffix('.countries.tmp')
                temp.write_bytes(after)
                assert temp.read_bytes() == after
                temp.replace(path)
                assert path.read_bytes() == after
        finally:
            assert native.NtResumeProcess(handle) == 0
        print(json.dumps({'live_forces': len(live_changes), 'files': len(pending),
                          'backup': str(backup), 'file_matches': {p.name: len(c) for p, _, _, c in pending}}, ensure_ascii=False))
    finally:
        api.CloseHandle(handle)


if __name__ == '__main__':
    main()
