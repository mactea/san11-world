"""Rewrite portrait ids inside an in-progress save (SaveData/Save%03d.s11).

A save keeps its own copy of the world: scenarios only decide what a new game
starts with, so portraits installed after the save was made must be patched in
here as well.

Container: 'MWC' + u8 + u32 uncompressed size + u32 compressed size + zlib.
Unpacked person records are byte-compatible with scenario records (name at +0,
birth +58, face +53) but 203 bytes apart instead of 152.

usage: python tools/roster/patch_save_faces.py SAVE [SAVE...] [--dry-run]
"""
import json
import struct
import sys
import zlib
from datetime import datetime
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJ = HERE.parents[1]
sys.path.insert(0, str(PROJ / 'tools'))
sys.path.insert(0, str(HERE))
from import_roster import parse  # noqa: E402
from install_portraits import GAME, person_name  # noqa: E402

MAGIC = b'MWC'
SAVE_STRIDE = 203
NAME = slice(0, 10)
BIRTH = 58
FACE = 53


def unpack(save_bytes):
    at = save_bytes.find(MAGIC)
    assert at > 0, 'no MWC container'
    raw_size, packed_size = struct.unpack_from('<II', save_bytes, at + 4)
    world = zlib.decompress(save_bytes[at + 12:])
    assert len(world) == raw_size, (len(world), raw_size)
    assert len(save_bytes) - at - 12 == packed_size
    return at, world


def pack(save_bytes, world):
    at = save_bytes.find(MAGIC)
    body = zlib.compress(world, 6)
    head = MAGIC + save_bytes[at + 3:at + 4] + struct.pack('<II', len(world), len(body))
    return save_bytes[:at] + head + body


def scenario_persons(scen_path):
    data = scen_path.read_bytes()
    off, n, stride = parse(data)['L']['person']
    return [(bytes(data[off + i * stride: off + i * stride + 10]),
             person_name(data, off + i * stride),
             struct.unpack_from('<h', data, off + i * stride + BIRTH)[0]) for i in range(n)]


def find_base(world, people):
    """Offset of the save's person array: first four records must line up at SAVE_STRIDE."""
    at = world.find(people[0][0])
    while at != -1:
        if all(world.startswith(people[k][0], at + k * SAVE_STRIDE) for k in range(1, 4)):
            return at
        at = world.find(people[0][0], at + 1)
    raise AssertionError('person array not found in save')


def main(argv):
    dry = '--dry-run' in argv
    saves = [Path(a) for a in argv if not a.startswith('--')]
    scen = sorted(Path(r'C:\Games\san11\Romance Of Three Kingdom 11 PK\world_custom\scenario').glob('SCEN001.S11'))
    assert scen, 'deployed scenario not found'
    roster = json.loads((PROJ / 'data/persons/roster.json').read_text(encoding='utf-8'))
    wanted = {}
    for p in roster:
        if p.get('face', -1) >= 2400:
            wanted[(p['sei'] + p['mei'], p['birth'])] = p['face']

    people = scenario_persons(scen[0])
    report = []
    for path in saves:
        original = path.read_bytes()
        _, world = unpack(original)
        base = find_base(world, people)
        out = bytearray(world)
        changed = skipped = 0
        touched = set()
        for i, (_, name, birth) in enumerate(people):
            face = wanted.get((name, birth))
            if face is None:
                continue
            at = base + i * SAVE_STRIDE + FACE
            was = struct.unpack_from('<h', out, at)[0]
            if was != face:
                struct.pack_into('<h', out, at, face)
                changed += 1
                touched.update((at, at + 1))
            else:
                skipped += 1
        assert len(out) == len(world)
        new = pack(original, bytes(out))
        _, check = unpack(new)
        assert check == bytes(out), 'round-trip mismatch'
        diff = {i for i, (a, b) in enumerate(zip(check, world)) if a != b}
        assert diff <= touched, sorted(diff - touched)[:5]
        if not dry:
            backup = GAME / 'portrait_backups' / f'save-{datetime.now():%Y%m%d-%H%M%S}'
            backup.mkdir(parents=True, exist_ok=True)
            (backup / path.name).write_bytes(original)
            path.write_bytes(new)
            assert unpack(path.read_bytes())[1] == bytes(out)
        report.append(dict(save=path.name, base=hex(base), people=len(people),
                           matched=changed + skipped, changed=changed, already=skipped,
                           dry_run=dry, size=[len(original), len(new)]))
    print(json.dumps(report, ensure_ascii=False, indent=1))


if __name__ == '__main__':
    main(sys.argv[1:])
