"""Install approved portrait sets into FACE and existing world scenarios.

Only selected, unused archive slots and the matched officers' face fields change.
Run from this worktree: python tools/roster/install_portraits.py
"""
import hashlib
import json
import shutil
import struct
import sys
from datetime import datetime
from pathlib import Path

PROJ = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJ / "tools"))
from import_roster import parse

GAME = Path(json.loads((PROJ / "project.json").read_text())['game_dir'])
ROOT = PROJ / "data/persons/portraits"
SETS = [
    (9649, "王猛", 325, 2196, "wang_meng", "wang_meng_9649"),
    (10370, "奧勒留", 121, 2197, "marcus_aurelius", "marcus_aurelius"),
    (9879, "龍樹", 150, 2198, "nagarjuna", "nagarjuna"),
    (9387, "卑彌呼", 158, 2199, "himiko", "himiko"),
]


def bmp_pixels(path):
    """Read our uncompressed 24-bit BMP exports as top-down packed BGR."""
    d = path.read_bytes()
    assert d[:2] == b"BM"
    start = struct.unpack_from('<I', d, 10)[0]
    w, h, planes, bits, compression = struct.unpack_from('<iiHHI', d, 18)
    assert w > 0 and h != 0 and planes == 1 and bits == 24 and compression == 0
    stride = (w * 3 + 3) & ~3
    rows = [d[start + y * stride:start + y * stride + w * 3] for y in range(abs(h))]
    if h > 0:
        rows.reverse()
    return w, abs(h), b''.join(rows)


def texture(path):
    w, h, pixels = bmp_pixels(path)
    return b'WFTX0010' + struct.pack('<IIHHHH', 24 + len(pixels), 1, w, h, 24, 0) + pixels


def person_name(d, off):
    return ''.join(d[off + j:off + j + 5].split(b'\0')[0].decode('cp950', errors='replace')
                   for j in (0, 5))


def main():
    archive = GAME / 'Media/face/San11Face00.fce'
    current = archive.read_bytes()
    previous_report = ROOT / 'installed.json'
    if previous_report.exists():
        previous = json.loads(previous_report.read_text(encoding='utf-8'))
        assert hashlib.sha256(current).hexdigest() == previous['archive_sha256'], 'Archive changed since installation'
        initial = Path(previous.get('original_backup', previous['backup'])) / archive.name
        original = initial.read_bytes()
    else:
        original = current
    assert original[:4] == b'FACE'
    _, first, count, images = struct.unpack_from('<IIII', original, 4)
    assert first == 0 and count == 2200 and images == count * 3
    # FACE header is 20 bytes. Each image-table entry is OFFSET, SIZE.
    # The field at +4 is a format version, not a header-size offset.
    table = 20 + count * 4
    out = bytearray(original)
    expected = {}
    for rid, name, birth, slot, folder, stem in SETS:
        for block in range(3):
            index = slot if block == 0 else count + 2 * slot + block - 1
            oldoff, oldsize = struct.unpack_from('<II', original, table + index * 8)
            size = '240x240' if block == 0 else '64x80'
            payload = texture(ROOT / folder / f'{stem}_{size}.bmp')
            if oldsize:
                assert original[oldoff:oldoff + oldsize] == payload, f'portrait slot {slot} is occupied'
            else:
                struct.pack_into('<II', out, table + index * 8, len(out), len(payload))
                out.extend(payload)
            expected[index] = payload
        template = 2100 if folder == 'himiko' else 2000
        out[20 + slot * 4:24 + slot * 4] = original[20 + template * 4:24 + template * 4]

    changes = []
    for path in sorted((GAME / 'world_custom/scenario').glob('*')):
        if not path.name.lower().startswith('scen') or path.name.lower() == 'scenario.s11':
            continue
        d = path.read_bytes()
        layout = parse(d)
        off, n, stride = layout['L']['person']
        e = bytearray(d)
        matches = []
        for rid, name, birth, slot, folder, stem in SETS:
            hits = [i for i in range(n) if person_name(d, off + i * stride) == name
                    and struct.unpack_from('<h', d, off + i * stride + 58)[0] == birth]
            assert len(hits) == 1, (path.name, name, hits)
            i = hits[0]
            field = off + i * stride + 53
            previous = struct.unpack_from('<h', d, field)[0]
            struct.pack_into('<h', e, field, slot)
            matches.append(dict(roster_id=rid, game_id=i, name=name, old_face=previous, face=slot))
        allowed = {off + m['game_id'] * stride + 53 + k for m in matches for k in range(2)}
        assert all(a == b or i in allowed for i, (a, b) in enumerate(zip(d, e)))
        assert len(e) == len(d)
        changes.append((path, bytes(e), matches))
    assert len(changes) == 15

    backup = GAME / 'portrait_backups' / datetime.now().strftime('%Y%m%d-%H%M%S')
    backup.mkdir(parents=True)
    shutil.copy2(archive, backup / archive.name)
    for path, _, _ in changes:
        shutil.copy2(path, backup / path.name)
    archive.write_bytes(out)
    for path, d, _ in changes:
        path.write_bytes(d)

    actual = archive.read_bytes()
    for index, payload in expected.items():
        p, size = struct.unpack_from('<II', actual, table + index * 8)
        assert actual[p:p + size] == payload
    # Existing image records and payloads remain byte-for-byte unchanged.
    changed_header = {x for _, _, _, slot, _, _ in SETS for x in range(20 + slot * 4, 24 + slot * 4)}
    changed_header |= {x for index in expected for x in range(table + index * 8, table + index * 8 + 8)}
    for start in sorted(changed_header):
        assert start < len(original)
    masked = bytearray(actual[:len(original)])
    for i in changed_header:
        masked[i] = original[i]
    assert masked == original
    for path, d, _ in changes:
        assert path.read_bytes() == d

    roster_path = PROJ / 'data/persons/roster.json'
    shutil.copy2(roster_path, backup / 'roster.json')
    roster_before = roster_path.read_bytes()
    roster = json.loads(roster_before.decode('utf-8'))
    original_names = {'wang_meng': 'Wang Meng (Former Qin)', 'marcus_aurelius': 'Marcus Aurelius',
                      'nagarjuna': 'Nagarjuna', 'himiko': 'Himiko'}
    roster_matches = []
    for rid, name, birth, slot, folder, stem in SETS:
        hits = [x for x in roster if x.get('name_orig') == original_names[folder] and x['birth'] == birth]
        assert len(hits) == 1
        hits[0]['face'] = slot
        roster_matches.append(dict(id=hits[0]['id'], name=hits[0]['name_full'], face=slot))
    assert roster_path.read_bytes() == roster_before, 'Roster changed during installation; rerun'
    roster_path.write_text(json.dumps(roster, ensure_ascii=False) + '\n', encoding='utf-8')
    report = dict(backup=str(backup), original_backup=str(initial.parent) if previous_report.exists() else str(backup),
                  small_image_layout='interleaved: faceCount + faceId*2 + variant', archive=str(archive),
                  archive_sha256=hashlib.sha256(actual).hexdigest(),
                  image_entries_verified=len(expected), roster_people=roster_matches,
                  scenarios=[dict(file=str(p), people=m) for p, _, m in changes],
                  note='Binary round-trip verified; runtime display not yet checked. Existing saves are unchanged.')
    (ROOT / 'installed.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(backup=str(backup), image_entries=12, scenarios=len(changes),
                         people=changes[0][2]), ensure_ascii=False))


if __name__ == '__main__':
    main()
