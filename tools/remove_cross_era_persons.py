"""Disable the identified cross-era guests in installed scenarios, saves and RAM.

Preserves every slot and unrelated byte. Default is preflight; --apply backs up
all changed files and live writes, briefly suspends the game, then verifies.
"""
import argparse
import ctypes as ct
import hashlib
import json
import re
import struct
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PERSONS = ROOT.parent / 'san11world-persons'
sys.path.insert(0, str(PERSONS / 'tools'))
sys.path.insert(0, str(PERSONS / 'tools/roster'))
from import_roster import parse, OTHER_FIELDS
from install_portraits import person_name
from patch_save_faces import unpack, pack, find_base, scenario_persons
from apply_city_specialties import scenario_offset, save_world


def targets(data, base, count, stride, memory=False):
    offset = 4 if memory else 0
    birth, death = (72, 76) if memory else (58, 60)
    fmt = '<i' if memory else '<h'
    koei = json.loads((PERSONS / 'data/persons/src/koei.json').read_text('utf-8'))
    original_names = {p['koei_id']: p['sei'] + p['mei'] for p in koei}
    found = []
    for i in range(count):
        pos = base + i * stride
        name = person_name(data, pos + offset)
        b = struct.unpack_from(fmt, data, pos + birth)[0]
        d = struct.unpack_from(fmt, data, pos + death)[0]
        ancient = 800 <= i <= 831
        if ancient:
            assert name == original_names[i], (i, name, original_names[i])
        if ancient or (i >= 850 and name == '李績' and (b, d) == (183, 269)):
            status = struct.unpack_from('<i', data, pos + 160)[0] if memory else data[pos + 100]
            found.append(dict(id=i, name=name, birth=b, death=d, status=status))
    assert len(found) in (32, 33), found
    return found


def patch_people(data, base, count, stride, found, memory=False):
    out = bytearray(data)
    edits = []
    removed = {p['id'] for p in found}
    fmt = '<i' if memory else '<h'

    def put(at, value, form=fmt):
        raw = struct.pack(form, value)
        if out[at:at + len(raw)] != raw:
            edits.append((at, bytes(out[at:at + len(raw)]), raw))
            out[at:at + len(raw)] = raw

    relations = [84, 88, 92, 96, 100] if memory else [63, 65, 67, 70, 72]
    relations += list(range(108, 148, 4)) if memory else list(range(75, 95, 2))
    for i in range(count):
        pos = base + stride * i
        for field in relations:
            value = struct.unpack_from(fmt, data, pos + field)[0]
            if value in removed and i not in removed:
                put(pos + field, i if field == relations[0] else -1)
    for p in found:
        pos = base + p['id'] * stride
        if p['status'] in (-1, 255):
            continue
        assert p['status'] in (3, 4, 6, 7, 8), ('leadership needs separate resolution', p)
        if memory:
            assert struct.unpack_from('<i', data, pos + 316)[0] == -1, 'officer has pending command'
            for field in (148, 152, 156, 160, 168, 352):
                put(pos + field, -1)
        else:
            if stride == 203:
                assert data[pos + 169] == 255, 'officer has pending command'
            put(pos + 95, -1, '<b')
            put(pos + 96, -1)
            put(pos + 98, -1)
            put(pos + 100, 255, '<B')
            put(pos + 102, -1)
    return bytes(out), edits


def patch_units(data, base, count, stride, removed, memory=False):
    out = bytearray(data)
    edits, units = [], []
    fmt, field = ('<3i', 12) if memory else ('<3h', 1)
    for i in range(count):
        at = base + i * stride + field
        old = struct.unpack_from(fmt, data, at)
        if not removed.intersection(old):
            continue
        remaining = [x for x in old if x >= 0 and x not in removed]
        assert remaining, ('unit would have no leader', i, old)
        new = tuple((remaining + [-1] * 3)[:3])
        raw = struct.pack(fmt, *new)
        edits.append((at, data[at:at + len(raw)], raw))
        out[at:at + len(raw)] = raw
        units.append(dict(unit=i, before=old, after=new))
    return bytes(out), edits, units


def verify_diff(before, after, edits):
    restored = bytearray(after)
    for at, old, new in reversed(edits):
        assert after[at:at + len(new)] == new
        restored[at:at + len(old)] = old
    assert bytes(restored) == before, 'unrelated bytes changed'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--pid', type=int, required=True)
    ap.add_argument('--apply', action='store_true')
    args = ap.parse_args()
    cfg = json.loads((ROOT / 'project.json').read_text('utf-8'))
    game = Path(cfg['game_dir'])
    reference = game / cfg['data_name'] / 'scenario/SCEN001.S11'
    people = scenario_persons(reference)
    ref = reference.read_bytes()
    city = scenario_offset(ref, cfg['cities'])
    originals = [ref[city + i * 81 + 75:city + i * 81 + 81] for i in range(42)]
    pending, report = [], dict(files=[], live=None, applied=args.apply)
    for folder in ('scenario', 'scenario_base'):
        for path in sorted((game / cfg['data_name'] / folder).glob('*')):
            if path.suffix.lower() != '.s11' or path.name.lower() == 'scenario.s11':
                continue
            before = path.read_bytes()
            info = parse(before)
            base, count, stride = info['L']['person']
            found = targets(before, base, count, stride)
            removed = {p['id'] for p in found}
            for section, field, label in OTHER_FIELDS:
                off, n, size = info['L'][section]
                assert all(struct.unpack_from('<h', before, off + i * size + field)[0] not in removed
                           for i in range(n)), (path, label)
            after, edits = patch_people(before, base, count, stride, found)
            verify_diff(before, after, edits)
            pending.append((path, before, after))
            report['files'].append(dict(path=str(path), targets=found, units=[], changed_bytes=sum(a != b for a, b in zip(before, after))))
    for path in sorted((game / cfg['save_dir']).rglob('Save[0-9][0-9][0-9].s11')):
        before = path.read_bytes()
        _, world = unpack(before)
        base = find_base(world, people)
        ps = struct.unpack_from('<H', before, 34)[0] if before[33] in (80, 81) else 850
        count = ps + (450 if before[33] == 81 else 250)
        found = targets(world, base, count, 203)
        removed = {p['id'] for p in found}
        # Original saves have narrow item/city records; already disabled guests
        # require no mutation and their unrelated layout need not be rewritten.
        changed, edits = patch_people(world, base, count, 203, found)
        unit_changes = []
        if edits:
            check, city = save_world(before, cfg['cities'], originals)
            assert check == world
            regular = struct.unpack_from('<H', before, 46)[0] if before[44:46] == b'WF' else 42
            fn = regular + 5
            fs = 214 + 2 * (fn - 47) + 4 * ((fn + 31) // 32 - 2)
            force = base + count * 203 + 100 * 58
            assert force + fn * (fs + 17) == city
            advisors = bytearray(changed)
            for i in range(fn):
                ruler, advisor = struct.unpack_from('<2h', world, force + i * fs)
                assert ruler not in removed, 'force ruler reference'
                if advisor in removed:
                    at = force + i * fs + 2
                    edits.append((at, world[at:at + 2], b'\xff\xff'))
                    advisors[at:at + 2] = b'\xff\xff'
                assert struct.unpack_from('<h', world, force + fn * fs + i * 17 + 2)[0] not in removed, 'corps leadership reference'
            changed = bytes(advisors)
            for i in range(100):
                assert struct.unpack_from('<h', world, base + count * 203 + i * 58 + 53)[0] not in removed, 'item owner reference'
            units = city + cfg['cities'] * 108 + 45 * 70 + 84 * 89
            changed, unit_edits, unit_changes = patch_units(changed, units, cfg['units'], 98, removed)
            edits += unit_edits
        verify_diff(world, changed, edits)
        after = pack(before, changed) if edits else before
        assert unpack(after)[1] == changed
        pending.append((path, before, after))
        report['files'].append(dict(path=str(path), targets=found, units=unit_changes, changed_bytes=sum(a != b for a, b in zip(world, changed))))

    api = ct.WinDLL('kernel32', use_last_error=True)
    api.OpenProcess.restype = ct.c_void_p
    api.ReadProcessMemory.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t)]
    api.WriteProcessMemory.argtypes = api.ReadProcessMemory.argtypes
    api.CloseHandle.argtypes = [ct.c_void_p]
    handle = api.OpenProcess(0x1f0fff, False, args.pid)
    assert handle, ct.get_last_error()
    def read(at, size):
        buf, got = ct.create_string_buffer(size), ct.c_size_t()
        assert api.ReadProcessMemory(handle, at, buf, size, ct.byref(got)) and got.value == size
        return buf.raw
    def write(at, value):
        got = ct.c_size_t()
        assert api.WriteProcessMemory(handle, at, value, len(value), ct.byref(got)) and got.value == len(value)
    try:
        log = (game / 'worldmod_san11pk_world.log').read_text(errors='replace')
        base = int(re.findall(r'PERSONARR ([0-9A-Fa-f]+)', log)[-1], 16)
        count = int(re.findall(r'persons: PS=\d+ PT=(\d+)', log)[-1])
        unit_base = int(re.findall(r'UNITARR ([0-9A-Fa-f]+)', log)[-1], 16)
        live = read(base, count * 400)
        found = targets(live, 0, count, 400, True)
        removed = {p['id'] for p in found}
        changed, edits = patch_people(live, 0, count, 400, found, True)
        verify_diff(live, changed, edits)
        units = read(unit_base, cfg['units'] * 244)
        unit_after, unit_edits, unit_changes = patch_units(units, 0, cfg['units'], 244, removed, True)
        verify_diff(units, unit_after, unit_edits)
        live_edits = [(base + at, old, new) for at, old, new in edits]
        live_edits += [(unit_base + at, old, new) for at, old, new in unit_edits]
        force_base = int(re.findall(r'FORCEARR ([0-9A-Fa-f]+)', log)[-1], 16)
        forces = read(force_base, (cfg['forces'] + 5) * 300)
        advisor_changes = []
        for i in range(cfg['forces'] + 5):
            ruler, advisor = struct.unpack_from('<2i', forces, i * 300 + 4)
            assert ruler not in removed, 'live force ruler reference'
            if advisor in removed:
                at = i * 300 + 8
                live_edits.append((force_base + at, forces[at:at + 4], b'\xff' * 4))
                advisor_changes.append(dict(force=i, removed_advisor=advisor))
        report['live'] = dict(pid=args.pid, person_array=hex(base), targets=found, units=unit_changes,
                              advisors=advisor_changes,
                              edits=[dict(address=hex(at), old=old.hex(), new=new.hex()) for at, old, new in live_edits])
        for row, (path, before, after) in zip(report['files'], pending):
            row['before_sha256'] = hashlib.sha256(before).hexdigest()
            row['after_sha256'] = hashlib.sha256(after).hexdigest()
        if args.apply:
            backup = game / 'chronology-backups' / datetime.now().strftime('%Y%m%d-%H%M%S')
            backup.mkdir(parents=True)
            report['backup'] = str(backup)
            for path, before, after in pending:
                if before != after:
                    dest = backup / path.relative_to(game)
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(before)
                    assert dest.read_bytes() == before
            (backup / 'manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
            native = ct.WinDLL('ntdll')
            native.NtSuspendProcess.argtypes = [ct.c_void_p]
            native.NtResumeProcess.argtypes = [ct.c_void_p]
            assert native.NtSuspendProcess(handle) == 0
            written, memory_written = [], []
            try:
                assert all(path.read_bytes() == before for path, before, after in pending)
                assert all(read(at, len(old)) == old for at, old, new in live_edits)
                # Replace unit membership before disabling the old leader.
                for at, old, new in reversed(live_edits):
                    write(at, new)
                    memory_written.append((at, old))
                    assert read(at, len(new)) == new
                for path, before, after in pending:
                    if before == after:
                        continue
                    tmp = path.with_suffix('.chronology.tmp')
                    tmp.write_bytes(after)
                    tmp.replace(path)
                    written.append((path, before))
                    assert path.read_bytes() == after
                report['verified'] = True
            except BaseException:
                for path, before in reversed(written):
                    path.write_bytes(before)
                for at, old in reversed(memory_written):
                    write(at, old)
                raise
            finally:
                assert native.NtResumeProcess(handle) == 0
        dest = ROOT / 'out/person_chronology'
        dest.mkdir(parents=True, exist_ok=True)
        (dest / ('game_removal.json' if args.apply else 'game_removal_preview.json')).write_text(
            json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(dict(applied=args.apply, backup=report.get('backup'), files_checked=len(pending),
                              files_changed=sum(old != new for _, old, new in pending),
                              live_targets=[p for p in found if p['status'] not in (-1, 255)], units=unit_changes), ensure_ascii=False))
    finally:
        api.CloseHandle(handle)


if __name__ == '__main__':
    main()
