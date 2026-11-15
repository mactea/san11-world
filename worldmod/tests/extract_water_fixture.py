"""Extract the two known local WFTX0010 water atlases; never execute the game.

This supports only the hash-checked stock resource, not arbitrary WFT formats.
Copyrighted pixels remain under the ignored build directory.
"""
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = 'be26061a15da2da6c390ab6bb0d9d8d744474ed169aac4c628e6e083bd19b758'


def decode(data):
    if hashlib.sha256(data).hexdigest() != EXPECTED:
        raise ValueError('unsupported water resource hash')
    if data[:8] != b'WFTX0010' or struct.unpack_from('<II', data, 8) != (len(data), 2):
        raise ValueError('invalid water header')
    atlases, offset = [], 16
    for _ in range(2):
        if struct.unpack_from('<HHI', data, offset) != (256, 256, 32):
            raise ValueError('unsupported water texture layout')
        offset += 8
        atlases.append(data[offset:offset + 256 * 256 * 4])
        offset += 256 * 256 * 4
    if offset != len(data):
        raise ValueError('unexpected water payload')
    return atlases


def extract():
    project = json.loads((ROOT.parent / 'project.json').read_text(encoding='utf-8'))
    pack = Path(project['game_dir']) / 'Media/san11pkres.bin'
    with pack.open('rb') as source:
        header = source.read(16)
        if header[:4] != b'LINK' or struct.unpack_from('<I', header, 4)[0] <= 0x12ec:
            raise ValueError('missing water resource in LINK pack')
        source.seek(16 + 8 * 0x12ec)
        offset, size = struct.unpack('<II', source.read(8))
        source.seek(offset)
        data = source.read(size)
    atlases = decode(data)
    output = ROOT / 'build'
    output.mkdir(exist_ok=True)
    records = []
    for i, pixels in enumerate(atlases):
        name = f'water-atlas-{i}.bgra'
        (output / name).write_bytes(pixels)
        records.append({'file': name, 'width': 256, 'height': 256, 'format': 'A8R8G8B8',
                        'sha256': hashlib.sha256(pixels).hexdigest()})
    (output / 'water-fixture.json').write_text(json.dumps({'resource': '0x12ec', 'sha256': EXPECTED,
                                                         'atlases': records}, indent=2), encoding='utf-8')
    print('PASS two hash-checked stock water atlases extracted without executing the game')


if __name__ == '__main__':
    extract()
