"""Append approved portraits to bounded FACE banks and bind them to deployed scenarios.

python tools/roster/install_portrait_batch.py data/persons/portraits/batches/0001.json
Each manifest identifies people by name and birth; roster ids may change between revisions.
"""
import configparser
import hashlib
import json
import struct
import sys
from datetime import datetime
from pathlib import Path

from install_portraits import GAME, PROJ, ROOT, person_name, texture
sys.path.insert(0, str(PROJ / 'tools'))
from import_roster import parse


def digest(d):
    return hashlib.sha256(d).hexdigest()


def main():
    manifest_path = Path(sys.argv[1])
    batch = json.loads(manifest_path.read_text(encoding='utf-8'))
    cfg = configparser.ConfigParser()
    cfg.read(GAME / 'worldmod.ini', encoding='utf-8-sig')
    section = cfg['san11pk_world']
    capacity = int(section.get('faces', '2400'))
    user_root = GAME / section['save_dir'] / 'Koei/San11 Tc'
    bank_root = user_root / 'FaceData'
    catalog_path = ROOT / 'catalog.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8')) if catalog_path.exists() else []
    roster_path = PROJ / 'data/persons/roster.json'
    roster_before = roster_path.read_bytes()
    roster = json.loads(roster_before)
    people = []
    for item in batch['people']:
        hits = [p for p in roster if p['name_full'] == item['name'] and p['birth'] == item['birth']]
        assert len(hits) == 1, ('roster identity', item, len(hits))
        p = hits[0]
        face = item['face']
        assert 2400 <= face < capacity
        assert p.get('face', -1) in (-1, face)
        conflicts = [x for x in catalog if x['face'] == face and (x['name'], x['birth']) != (item['name'], item['birth'])]
        assert not conflicts, conflicts
        people.append((item, p))

    scenarios = []
    for path in sorted((GAME / section['data_dir'] / 'scenario').glob('*')):
        if not path.name.lower().startswith('scen') or path.name.lower() == 'scenario.s11':
            continue
        original = path.read_bytes()
        layout = parse(original)
        off, n, stride = layout['L']['person']
        out = bytearray(original)
        deltas = []
        for item, p in people:
            name = p['sei'] + p['mei']
            hits = [i for i in range(n) if person_name(original, off + i * stride) == name
                    and struct.unpack_from('<h', original, off + i * stride + 58)[0] == p['birth']]
            assert len(hits) == 1, ('game identity', path.name, name, hits)
            field = off + hits[0] * stride + 53
            old = bytes(original[field:field + 2])
            new = struct.pack('<h', item['face'])
            out[field:field + 2] = new
            deltas.append(dict(offset=field, old=old.hex(), new=new.hex(), game_id=hits[0], name=item['name']))
        mask = bytearray(out)
        for d in deltas:
            mask[d['offset']:d['offset'] + 2] = bytes.fromhex(d['old'])
        assert mask == original
        scenarios.append((path, original, bytes(out), deltas))
    assert len(scenarios) == 15

    banks = {}
    base = (GAME / 'Media/face/San11Face00.fce').read_bytes()
    for item, p in people:
        face = item['face']
        number = (face - 2400) // 1000
        first = 2400 + number * 1000
        count = min(1000, capacity - first)
        path = bank_root / f'San11Face{10 + number:02d}.fce'
        if path not in banks:
            old = path.read_bytes() if path.exists() else b''
            if old:
                assert old[:4] == b'FACE' and struct.unpack_from('<4I', old, 4) == (64, first, count, count * 3)
                data = bytearray(old)
            else:
                data = bytearray(b'FACE' + struct.pack('<4I', 64, first, count, count * 3) + bytes(count * 28))
            banks[path] = dict(original=old, data=data, deltas=[], expected=[])
        bank = banks[path]
        local = face - first
        metadata = 20 + local * 4
        template = 2100 if p['sex'] else 2000
        new_meta = base[20 + template * 4:24 + template * 4]
        bank['deltas'].append(dict(offset=metadata, old=bank['data'][metadata:metadata + 4].hex(), new=new_meta.hex()))
        bank['data'][metadata:metadata + 4] = new_meta
        for variant, index in enumerate([local, count + local * 2, count + local * 2 + 1]):
            size = '240x240' if variant == 0 else '64x80'
            payload = texture(ROOT / item['folder'] / f'portrait_{size}.bmp')
            field = 20 + count * 4 + index * 8
            previous = bytes(bank['data'][field:field + 8])
            offset, length = struct.unpack('<II', previous)
            if length:
                assert bank['data'][offset:offset + length] == payload
            else:
                offset = len(bank['data'])
                bank['data'].extend(payload)
                new = struct.pack('<II', offset, len(payload))
                bank['data'][field:field + 8] = new
                bank['deltas'].append(dict(offset=field, old=previous.hex(), new=new.hex()))
            bank['expected'].append((field, payload))
    # Guard against concurrent scenario, archive or roster edits before any writes.
    assert roster_path.read_bytes() == roster_before
    assert all(p.read_bytes() == old for p, old, _, _ in scenarios)
    assert all((p.read_bytes() if p.exists() else b'') == b['original'] for p, b in banks.items())
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    backup = GAME / 'portrait_backups' / f'batch-{stamp}'
    backup.mkdir(parents=True)
    rollback = dict(banks=[dict(file=str(p), original_size=len(b['original']), original_sha256=digest(b['original']),
                               deltas=b['deltas']) for p, b in banks.items()],
                    scenarios=[dict(file=str(p), original_sha256=digest(old), deltas=d)
                               for p, old, out, d in scenarios])
    (backup / 'rollback.json').write_text(json.dumps(rollback, ensure_ascii=False, indent=2), encoding='utf-8')
    bank_root.mkdir(parents=True, exist_ok=True)
    for path, bank in banks.items():
        path.write_bytes(bank['data'])
        actual = path.read_bytes()
        for field, expected in bank['expected']:
            offset, length = struct.unpack_from('<II', actual, field)
            assert actual[offset:offset + length] == expected
    for path, old, out, deltas in scenarios:
        path.write_bytes(out)
        assert path.read_bytes() == out
    for item, p in people:
        p['face'] = item['face']
        if not any(x['face'] == item['face'] for x in catalog):
            catalog.append({**item, 'roster_id': p['id'], 'status': 'installed', 'batch': manifest_path.name})
    assert roster_path.read_bytes() == roster_before, 'Roster changed during file installation; rerun to finish sync'
    roster_path.write_text(json.dumps(roster, ensure_ascii=False) + '\n', encoding='utf-8')
    catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2), encoding='utf-8')
    report = dict(people=[dict(name=i['name'], face=i['face'], roster_id=p['id']) for i, p in people],
                  banks=[str(p) for p in banks], scenarios=len(scenarios), backup=str(backup),
                  verification='All encoded payloads and scenario bytes read back; runtime scene pending')
    (ROOT / 'batches' / f'{manifest_path.stem}_installed.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
