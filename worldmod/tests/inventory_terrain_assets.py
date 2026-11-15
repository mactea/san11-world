"""Inspect local LINK/WFT textures without launching or modifying the game."""
import hashlib
import json
from pathlib import Path
import struct
from PIL import Image, ImageDraw, ImageFont
import pefile
from ground_loader_contract import decode_wft

ROOT = Path(__file__).resolve().parents[1]
SELECTED = (0x12BA, 0x12BC, 0x12BD, 0x12BE, 0x12C0, 0x12C1, 0x12C2, 0x12C3, 0x12EC)


def ground_contract(exe):
    pe = pefile.PE(str(exe))
    blocks = ((0x40F391, 14, '71a2cfe8c0a065fdf99cc6564d7980c974081720803aee30e5a786ec5bab0c71'),
              (0x40F3E5, 13, 'a900dc5dd48aecf0b9f166ff4894df1ecff4bc57ab6317df1308dcfebd774a32'),
              (0x76356C, 16, '00d0b3d8484387d4640e62e2dbd31bca27d2fcf0cb427529cfd8bbc7bdc3083b'),
              (0x8A0778, 16, '64a86041d9f656dcb9fc8b73e664cbaf01edeb0dc232f3ccd42166c3e902659b'),
              (0x40F484, 41, 'cfa28f6b37762b7f6c09dcb2344c8f5d7c2336400e92f9f2693d15edf12a48ce'),
              (0x41F7A0, 56, '4dd1daea6b429361b63a749f589dfd87d93b590d01325f84254b4f1edede316c'),
              (0x421DC4, 75, '036a82d930cf8b3b648e66a79819a838e5203d4e8995b6dcd16eda27d80eccde'),
              (0x41DDD6, 65, '51824396882ccc6ca7feaee46fd97a724b4203df3c32f9a2f23df17219d2c6a2'))
    for address, size, expected in blocks:
        if hashlib.sha256(pe.get_data(address-pe.OPTIONAL_HEADER.ImageBase, size)).hexdigest() != expected:
            raise ValueError('ground loader contract changed')
    resources = struct.unpack('<4I', pe.get_data(0x76356C-pe.OPTIONAL_HEADER.ImageBase, 16))
    names = struct.unpack('<4I', pe.get_data(0x8A0778-pe.OPTIONAL_HEADER.ImageBase, 16))
    result = []
    for season, resource, pointer in zip(('spring', 'summer', 'autumn', 'winter'), resources, names):
        name = pe.get_data(pointer-pe.OPTIONAL_HEADER.ImageBase, 100).split(b'\0', 1)[0].decode('ascii')
        if name != f'media/stage/ground/ground_{season}.wft':
            raise ValueError('ground season name changed')
        result.append({'season': season, 'resource': hex(resource), 'name': name})
    return {'exe_sha256': hashlib.sha256(Path(exe).read_bytes()).hexdigest(), 'seasons': result,
            'optional_conversion': {'format': 'DXT1', 'call': '0x40f49b', 'function': '0x44fa50',
                                    'condition': 'saved value from 0x8a58d8 is nonzero; runtime setting not inspected'},
            'texture_selection': {'getter':'0x41f7a0', 'record_stride':36, 'layer_indexes_record_directly':True},
            'uv': {'period_x':'original texture width / 32', 'period_y':'original texture height / 32',
                   'sample_period_vertices':[2,2], 'replacement_keeps_original_geometry_uv':True},
            'verified_blocks': [{'address': hex(a), 'bytes': n, 'sha256': h} for a, n, h in blocks]}


def decode_linear(data):
    if len(data) < 16 or data[:8] != b'WFTX0010':
        raise ValueError('not WFTX0010')
    length, count = struct.unpack_from('<II', data, 8)
    if length != len(data) or not 0 < count <= 4096:
        raise ValueError('length/count mismatch')
    offset, images = 16, []
    for index in range(count):
        if offset + 8 > len(data):
            raise ValueError('truncated image header')
        width, height, bits = struct.unpack_from('<HHI', data, offset)
        offset += 8
        if not width or not height or bits not in (24, 32):
            raise ValueError('only validated linear 24/32-bit layouts are decoded')
        size = width * height * (bits // 8)
        if offset + size > len(data):
            raise ValueError('truncated pixels')
        pixels = data[offset:offset+size]
        images.append(({'index': index, 'width': width, 'height': height, 'bits': bits,
                        'payload_offset': offset, 'payload_bytes': size,
                        'sha256': hashlib.sha256(pixels).hexdigest()}, pixels))
        offset += size
    if offset != len(data):
        raise ValueError('unexplained trailing bytes; do not infer mipmaps')
    return images


def main():
    project = json.loads((ROOT.parent / 'project.json').read_text(encoding='utf-8'))
    pack = Path(project['game_dir']) / 'Media/san11pkres.bin'
    output = ROOT / 'build/terrain-assets'
    output.mkdir(parents=True, exist_ok=True)
    records, thumbs = [], []
    contract = ground_contract(ROOT/'build/render-game/san11pk_render.exe')
    with pack.open('rb') as source:
        header = source.read(16)
        if header[:4] != b'LINK':
            raise ValueError('not LINK')
        count = struct.unpack_from('<I', header, 4)[0]
        if count > (pack.stat().st_size-16)//8:
            raise ValueError('invalid LINK table')
        table = struct.unpack('<'+'II'*count, source.read(count*8))
        for resource in range(count):
            offset, size = table[resource*2:resource*2+2]
            if offset+size > pack.stat().st_size:
                raise ValueError('entry outside pack')
            source.seek(offset)
            if size < 16 or source.read(8) != b'WFTX0010':
                continue
            source.seek(offset)
            data = source.read(size)
            record = {'resource': hex(resource), 'size': size, 'sha256': hashlib.sha256(data).hexdigest()}
            try:
                images = decode_linear(data)
                record.update(layout='linear_bgr_or_bgra', images=[meta for meta, _ in images])
                if resource in SELECTED:
                    for meta, pixels in images:
                        mode, raw = ('RGB', 'BGR') if meta['bits'] == 24 else ('RGBA', 'BGRA')
                        image = Image.frombytes(mode, (meta['width'], meta['height']), pixels, 'raw', raw)
                        name = f"{resource:04x}-{meta['index']:02d}.png"
                        image.save(output/name)
                        if meta['index'] < (8 if resource < 0x12C0 else 4):
                            thumbs.append((name, image.convert('RGB')))
            except ValueError:
                try:
                    extended = decode_wft(data)
                    record.update(layout='explicit_mips_palettes' if extended else 'empty',
                                  images=[meta for meta, _, _ in extended])
                except ValueError as extended_error:
                    record.update(layout='unverified', reason=str(extended_error))
            records.append(record)
    by_resource = {r['resource']: r for r in records}
    seasons = [by_resource[row['resource']]['images'] for row in contract['seasons']]
    if any(len(images) != 36 for images in seasons):
        raise ValueError('unexpected ground texture count')
    changed = [i for i in range(36) if len({images[i]['sha256'] for images in seasons}) > 1]
    aliases = []
    for index in range(36):
        groups = {}
        for row, images in zip(contract['seasons'], seasons):
            groups.setdefault(images[index]['sha256'], []).append(row['season'])
        aliases.append({'index': index, 'identical_payload_groups': list(groups.values())})
    report = {'pack': str(pack), 'game_launched': False, 'wft_resources': len(records),
              'ground_contract': contract, 'season_changed_indices': changed,
              'season_aliases': aliases,
              'selected_candidate_resources': [hex(i) for i in SELECTED],
              'classification_note': 'Season/resource mapping, direct layer indexing and UV period rule are statically verified. Terrain-type associations are statistical, not unique labels; do not assign grass/road solely from appearance.',
              'mipmap_note': 'Extended records use explicit header mip/palette counts and DWORD-aligned rows; no generated mip chain inferred.',
              'resources': records}
    (output/'inventory.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 14)
    sheet = Image.new('RGB', (4*272, ((len(thumbs)+3)//4)*164), '#17212b')
    draw = ImageDraw.Draw(sheet)
    for index, (name, image) in enumerate(thumbs):
        x, y = (index%4)*272, (index//4)*164
        draw.text((x+8,y+4), name, font=font, fill='white')
        image.thumbnail((256,128))
        sheet.paste(image, (x+8,y+27))
    sheet.save(output/'contact-sheet.png')
    print(json.dumps({'wft_resources': len(records), 'linear_resources': sum(r['layout'] == 'linear_bgr_or_bgra' for r in records),
                      'validated_resources': sum(r['layout'] != 'unverified' for r in records),
                      'selected_images': sum(len(r.get('images', [])) for r in records if int(r['resource'],16) in SELECTED), 'output': str(output)}))


if __name__ == '__main__':
    main()
