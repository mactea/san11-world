"""Write persons_bio.bin: the biographies (列传) of the extended officers for worldmod (M10_PERSONS.md).

Koei's officers 0..849 keep their messages in S11MSG02.S11; the extended officers 850..PS-1 get theirs from this file,
which worldmod (persons.cpp personBio) reads from its data directory (worldmod.ini data_dir=).

Format: "WPBI", u32 first id, u32 n, u32 offsets[n + 1] (relative to the end of the offset table), then the texts in
the game's encoding (cp950), each NUL-terminated; an empty text is a lone NUL.

usage: python tools/persons_bio.py [roster.json] <out persons_bio.bin>
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def build(roster):
    roster = sorted(roster, key=lambda x: x['id'])
    first = roster[0]['id'] if roster else 850
    if [x['id'] for x in roster] != list(range(first, first + len(roster))):
        raise SystemExit('roster ids are not consecutive')
    texts, offs, o = [], [], 0
    for x in roster:
        t = (x.get('bio') or '').encode('cp950', errors='replace') + b'\0'
        offs.append(o)
        texts.append(t)
        o += len(t)
    offs.append(o)
    return b'WPBI' + struct.pack('<II', first, len(roster)) + struct.pack(f'<{len(offs)}I', *offs) + b''.join(texts)


def main(argv):
    if len(argv) == 1:
        src, out = ROOT / 'data' / 'persons' / 'roster.json', Path(argv[0])
    elif len(argv) == 2:
        src, out = Path(argv[0]), Path(argv[1])
    else:
        raise SystemExit(__doc__)
    roster = json.loads(src.read_text(encoding='utf-8'))
    data = build(roster)
    out.write_bytes(data)
    print(f'{len(roster)} biographies -> {out} ({len(data)} bytes)')


if __name__ == '__main__':
    main(sys.argv[1:])
