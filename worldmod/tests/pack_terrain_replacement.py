"""Build an opt-in exact-content terrain package; never modify game resources.

The decoded-image signature assumes A8R8G8B8 plus BOX mips. It is an offline
fixture, not proof that the game's optional DXT1 conversion produces these bytes.
"""
from pathlib import Path
import json
import hashlib
import struct
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def mip_bytes(image):
    levels = []
    image = image.convert('RGBA')
    while True:
        levels.append(image.tobytes('raw', 'BGRA'))
        if image.size == (1, 1):
            return levels
        image = image.resize((max(1, image.width//2), max(1, image.height//2)), Image.Resampling.BOX)


def encode_entry(source, target):
    original = Image.open(source).convert('RGBA')
    sample = Image.open(target).convert('RGBA').resize((1024, 1024), Image.Resampling.LANCZOS)
    if not (0 < original.width <= 1024 and 0 < original.height <= 1024):
        raise ValueError('source dimensions outside package limits')
    if original.width != original.height:
        raise ValueError('this sample builder requires square input')
    a, b = mip_bytes(original), mip_bytes(sample)
    if sum(map(len, a)) > 1024*1024:
        raise ValueError('source identity exceeds 1 MiB limit')
    data = struct.pack('<7I', original.width, original.height, 21, len(a), 1024, 1024, len(b)) + b''.join(a+b)
    return data, {'source': str(source), 'target': str(target), 'source_mips':len(a), 'target_mips':len(b),
                  'source_bytes':sum(map(len,a)), 'target_bytes':sum(map(len,b))}


def build(pairs, output):
    if not 0 < len(pairs) <= 4:
        raise ValueError('one to four exact identities per package')
    encoded = [encode_entry(source,target) for source,target in pairs]
    data = struct.pack('<8sI', b'WMTR0002', len(encoded)) + b''.join(row[0] for row in encoded)
    if len(data)>32*1024*1024:
        raise ValueError('package exceeds 32 MiB payload limit')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    return {'file': str(output), 'sha256': hashlib.sha256(data).hexdigest(),
            'entries': [row[1] for row in encoded], 'runtime_identity_verified': False,
            'source_format': 'A8R8G8B8, BOX mip chain; exact match required', 'deployed': False}


if __name__ == '__main__':
    folder = ROOT/'build/terrain-assets'
    result = build([(folder/'12c0-01.png', ROOT/'assets/terrain-samples/ground-12c0-01-v2.png'),
                    (folder/'12c3-01.png', ROOT/'assets/terrain-samples/ground-12c3-01-v1.png')], folder/'replacement-v2.wmtr')
    (folder/'replacement-v2.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
