"""Build statically derived uncompressed ground signatures, without a game run."""
from pathlib import Path
import hashlib
import json
import struct

from PIL import Image
from ground_loader_contract import audit_loader, decode_wft, ground_identity
from inventory_terrain_assets import ground_contract
from pack_terrain_replacement import mip_bytes

ROOT = Path(__file__).resolve().parents[1]


def read_resource(pack, resource):
    with pack.open('rb') as file:
        header = file.read(16)
        if len(header) != 16 or header[:4] != b'LINK':
            raise ValueError('not LINK')
        count = struct.unpack_from('<I', header, 4)[0]
        if not 0 <= resource < count:
            raise ValueError('resource outside table')
        file.seek(16+resource*8)
        offset, size = struct.unpack('<II', file.read(8))
        if offset < 16+count*8 or offset+size > pack.stat().st_size:
            raise ValueError('resource outside pack')
        file.seek(offset)
        return file.read(size)


def build():
    folder = ROOT/'build/terrain-assets'
    exe = ROOT/'build/render-game/san11pk_render.exe'
    contract = ground_contract(exe)
    blocks = audit_loader(exe, folder/'loader-contract')
    project = json.loads((ROOT.parent/'project.json').read_text(encoding='utf-8'))
    pack = Path(project['game_dir'])/'Media/san11pkres.bin'
    entries, encoded = [], []
    for resource, name in [(0x12c0,'ground-12c0-01-v2.png'), (0x12c3,'ground-12c3-01-v1.png')]:
        raw = read_resource(pack,resource)
        meta, levels, _ = decode_wft(raw)[1]
        target = ROOT/'assets/terrain-samples'/name
        with Image.open(target) as image:
            target_mips = mip_bytes(image.convert('RGBA').resize((1024,1024),Image.Resampling.LANCZOS))
        for half in (False,True):
            shape, identity = ground_identity(meta, levels[0], half)
            encoded.append(struct.pack('<7I',*shape,1024,1024,len(target_mips))+identity+b''.join(target_mips))
            entries.append({'resource':hex(resource), 'index':1, 'half_size':half,
                            'source_shape':list(shape), 'source_bytes':len(identity),
                            'source_sha256':hashlib.sha256(identity).hexdigest(),
                            'wft_sha256':hashlib.sha256(raw).hexdigest(),
                            'target':str(target.relative_to(ROOT)),
                            'target_sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
    data = struct.pack('<8sI',b'WMTR0002',len(encoded))+b''.join(encoded)
    if len(data)>32*1024*1024:
        raise ValueError('package exceeds existing loader limit')
    output = folder/'replacement-loader-profiles.wmtr'
    output.write_bytes(data)
    result = {'file':output.name,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),
              'game_launched':False,'runtime_identity_verified':False,'deployed':False,
              'profile':'24-bit ground, X8R8G8B8 format supported, compression disabled; full/half-size branches',
              'unsupported':'DXT1 conversion and device format fallback; exact mismatch retains original',
              'source_levels':1,'target_levels':11,'entries':entries,
              'exe_sha256':contract['exe_sha256'],'loader_blocks':blocks}
    (folder/'replacement-loader-profiles.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result


if __name__ == '__main__':
    result = build()
    print(json.dumps({k:result[k] for k in ('file','sha256','bytes','profile','runtime_identity_verified')}))
