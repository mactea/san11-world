"""Read back every catalogued portrait and its bindings in all deployed scenarios."""
import configparser
import json
import struct
from pathlib import Path

from install_portraits import GAME, PROJ, ROOT, person_name, texture
from import_roster import parse


def main():
    catalog = json.loads((ROOT / 'catalog.json').read_text(encoding='utf-8'))
    roster = json.loads((PROJ / 'data/persons/roster.json').read_text(encoding='utf-8'))
    roster_by_identity = {}
    for person in roster:
        roster_by_identity.setdefault((person['name_full'], person['birth']), []).append(person)
    cfg = configparser.ConfigParser()
    cfg.read(GAME / 'worldmod.ini', encoding='utf-8-sig')
    section = cfg['san11pk_world']
    capacity = int(section.get('faces', '2400'))
    paths = [GAME / 'Media/face/San11Face00.fce']
    paths += sorted((GAME / section['save_dir'] / 'Koei/San11 Tc/FaceData').glob('San11Face??.fce'))
    banks = []
    for path in paths:
        data = path.read_bytes()
        assert data[:4] == b'FACE'
        version, first, count, images = struct.unpack_from('<4I', data, 4)
        assert version == 64 and images == count * 3 and first + count <= capacity
        banks.append((first, count, data, path))
    scenarios = []
    for path in sorted((GAME / section['data_dir'] / 'scenario').glob('*')):
        if path.name.lower().startswith('scen') and path.name.lower() != 'scenario.s11':
            data = path.read_bytes()
            layout = parse(data)
            off, n, stride = layout['L']['person']
            identities = {}
            for i in range(n):
                base = off + i * stride
                key = (person_name(data, base), struct.unpack_from('<h', data, base + 58)[0])
                identities.setdefault(key, []).append(i)
            scenarios.append((path, data, layout, identities))
    assert len(scenarios) == 15
    evidence = []
    for item in catalog:
        face = item['face']
        hits = roster_by_identity.get((item['name'], item['birth']), [])
        assert len(hits) == 1 and hits[0]['face'] == face
        person = hits[0]
        matches = [b for b in banks if b[0] <= face < b[0] + b[1]]
        assert len(matches) == 1
        first, count, data, bank = matches[0]
        local = face - first
        assert data[20 + local * 4 + 3] & 2
        for variant, index in enumerate([local, count + local * 2, count + local * 2 + 1]):
            dimensions = '240x240' if variant == 0 else '64x80'
            files = list((ROOT / item['folder']).glob(f'*_{dimensions}.bmp'))
            assert len(files) == 1
            expected = texture(files[0])
            offset, size = struct.unpack_from('<II', data, 20 + count * 4 + index * 8)
            assert data[offset:offset + size] == expected, (item['name'], variant)
        game_ids = {}
        for path, data, layout, identities in scenarios:
            off, n, stride = layout['L']['person']
            ids = identities.get((person['sei'] + person['mei'], person['birth']), [])
            assert len(ids) == 1
            game_ids[path.name] = ids[0]
            assert struct.unpack_from('<h', data, off + ids[0] * stride + 53)[0] == face
        evidence.append(dict(name=item['name'], birth=item['birth'], face=face, roster_id=person['id'],
                             bank=str(bank), image_variants_verified=3, scenario_game_ids=game_ids))
    result = dict(catalogued=len(catalog), variants_verified=len(catalog) * 3,
                  scenario_bindings_verified=len(catalog) * len(scenarios),
                  runtime_rendering_verified=False, people=evidence)
    (ROOT / 'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'people'}))


if __name__ == '__main__':
    main()
