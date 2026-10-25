"""Rewrite the images of already-installed faces inside extension FACE banks.

python tools/roster/replace_portrait_payload.py <manifest.json>

The manifest lists people who are already installed (name, birth, face, folder).
Face ids, catalog entries and scenario bindings stay as they are; only the three
WFTX payloads of each face are rewritten in place.  Payload size is fixed per
resolution, so offsets and table sizes never move.  Original payload bytes are
saved under portrait_backups/replace-<stamp>/ before writing.
"""
import configparser
import hashlib
import json
import struct
import sys
from datetime import datetime
from pathlib import Path

from install_portraits import GAME, ROOT, texture

VARIANTS = ('240x240', '64x80', '64x80')


def main():
    manifest_path = Path(sys.argv[1])
    batch = json.loads(manifest_path.read_text(encoding='utf-8'))
    cfg = configparser.ConfigParser()
    cfg.read(GAME / 'worldmod.ini', encoding='utf-8-sig')
    section = cfg['san11pk_world']
    capacity = int(section.get('faces', '2400'))
    bank_root = GAME / section['save_dir'] / 'Koei/San11 Tc/FaceData'
    catalog = json.loads((ROOT / 'catalog.json').read_text(encoding='utf-8'))
    installed = {(c['name'], c['birth']): c for c in catalog}

    backups = {}
    for item in batch['people']:
        key = (item['name'], item['birth'])
        assert key in installed, ('not installed', item)
        assert installed[key]['face'] == item['face'], ('face mismatch', item)
        face = item['face']
        assert 2400 <= face < capacity
        number = (face - 2400) // 1000
        first = 2400 + number * 1000
        count = min(1000, capacity - first)
        path = bank_root / f'San11Face{10 + number:02d}.fce'
        if path not in backups:
            data = path.read_bytes()
            assert data[:4] == b'FACE' and struct.unpack_from('<4I', data, 4) == (64, first, count, count * 3)
            backups[path] = dict(original=data, data=bytearray(data), first=first, count=count,
                                 deltas=[], expected=[])
        bank = backups[path]
        local = face - first
        for variant, size in enumerate(VARIANTS):
            index = local if variant == 0 else count + local * 2 + variant - 1
            field = 20 + bank['count'] * 4 + index * 8
            offset, length = struct.unpack_from('<II', bank['data'], field)
            payload = texture(ROOT / item['folder'] / f'portrait_{size}.bmp')
            assert length == len(payload), ('size changed', item['name'], size, length, len(payload))
            old = bytes(bank['data'][offset:offset + length])
            bank['data'][offset:offset + length] = payload
            bank['deltas'].append(dict(face=face, name=item['name'], size=size, offset=offset,
                                       length=length, old=old,
                                       old_sha256=hashlib.sha256(old).hexdigest(),
                                       new_sha256=hashlib.sha256(payload).hexdigest()))
            bank['expected'].append((offset, payload))

    assert all(path.read_bytes() == bank['original'] for path, bank in backups.items())
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    backup = GAME / 'portrait_backups' / f'replace-{stamp}'
    backup.mkdir(parents=True)
    report = {}
    for path, bank in backups.items():
        old_dir = backup / path.stem
        old_dir.mkdir()
        for d in bank['deltas']:
            name = f"{d['face']:04d}_{d['size']}.wftx"
            (old_dir / name).write_bytes(d.pop('old'))
            d['old_file'] = f'{old_dir.name}/{name}'
        report[path.name] = dict(original_sha256=hashlib.sha256(bank['original']).hexdigest(),
                                 deltas=bank['deltas'])
    (backup / 'replace.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

    for path, bank in backups.items():
        path.write_bytes(bank['data'])
        actual = path.read_bytes()
        for offset, expected in bank['expected']:
            assert actual[offset:offset + len(expected)] == expected
    print(json.dumps({'replaced': len(batch['people']), 'banks': [str(p) for p in backups],
                      'backup': str(backup)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
