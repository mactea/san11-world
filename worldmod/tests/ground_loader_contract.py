"""Static WFT/ground loading contracts; never load or execute the game EXE."""
import hashlib
import struct

import capstone
import pefile


BLOCKS = (
    ('ground_options', 0x40f340, 0x40f380, '271ee2cc7d8b090b51540f5b81c119487a4cf8606031782b8d5773db713cb69e'),
    ('format_choice', 0x44e3c0, 0x44e426, '597c9639da40a9b86990061378f9223954dc6a1cc254fb4075d5e8b167cff4d9'),
    ('texture_create', 0x44f38b, 0x44f450, '29bee0e167795047fa2a520d623eb6807e55dce1d108ac39c966fabb36ed1429'),
    ('texture_copy_levels', 0x44f450, 0x44f4ae, '8496c2f55cabc9148d307a8595cc2b5601ed2c6be63157cb3650c5479e379605'),
    ('record_reader', 0x452765, 0x4527fd, 'c8d3e71b8da33423f57b1bb51eb6d7574a287fd813c409cfbd1ac8118345ac15'),
    ('mip_payload_size', 0x450fe0, 0x45101b, '6140fe5d470baa48b94aecf90de393628e73d290c7eed8cd79bfc72d48ebd679'),
    ('half_dispatch', 0x452830, 0x4529aa, 'ff86f71b14814604d24635e8ae9e023b75402819364aa7cbc2af4cc45ce0c678'),
    ('half_bgr', 0x452150, 0x452367, '1e7e0f4920c1d0c03c9b0d0ab617d0d0b2df70fb40147c1c8c354babb9159a57'),
    ('half_average', 0x4510e0, 0x45117c, 'f74a70c49fcdaeb09275f83f9eab6769e9b83597aa460aa7f1fdf2ce9c73b917'),
    ('pixel_dispatch', 0x454d60, 0x454e29, '10beeae418e3bae725ac990b038d7dfaf1c726d244ce28f8742301ecfdd4ee7a'),
    ('bgr_copy', 0x454c6b, 0x454ce8, 'db8d2e9b895e6ffaa980000feaed26eb65aee8b99ec499da339cc8efa2c66180'),
    ('bgra_store', 0x4549c0, 0x4549f4, '16c775c4629652bf6bb5b0e92022c4bc7818a7fa38f8d7f87a994eecf5283339'),
    ('compressed_conversion', 0x44e480, 0x44e55e, 'f17196bd9fab498809ca9e7341abed1990df7cb613b9b01526095f63f6c4714e'),
)


def audit_loader(exe, output=None):
    pe = pefile.PE(str(exe))
    if pe.OPTIONAL_HEADER.ImageBase != 0x400000 or pe.FILE_HEADER.Machine != 0x14c:
        raise ValueError('unsupported game architecture/base')
    result = []
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    if output:
        output.mkdir(parents=True, exist_ok=True)
    for name, begin, end, expected in BLOCKS:
        data = pe.get_data(begin-0x400000, end-begin)
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError(f'loader contract changed: {name}')
        result.append({'name': name, 'begin': hex(begin), 'end': hex(end), 'sha256': expected})
        if output:
            (output/f'{name}.asm').write_text('\n'.join(f'{a:08x} {op} {args}' for a, _, op, args in md.disasm_lite(data, begin)), encoding='utf-8')
    return result


def decode_wft(data):
    """Read explicit mip/palette counts and DWORD-aligned rows, retaining bytes."""
    if len(data) < 16 or data[:8] != b'WFTX0010':
        raise ValueError('not WFTX0010')
    length, count = struct.unpack_from('<II', data, 8)
    if length != len(data) or count > 4096:
        raise ValueError('length/count mismatch')
    offset, images = 16, []
    for index in range(count):
        if offset+8 > len(data):
            raise ValueError('truncated image header')
        width, height, bits, palettes, extra_mips, reserved = struct.unpack_from('<HH4B', data, offset)
        offset += 8
        if not width or not height or bits not in (8, 24, 32) or reserved or (bits != 8 and palettes):
            raise ValueError('unverified WFT pixel layout')
        if bits == 8 and not palettes:
            raise ValueError('indexed image missing palette')
        levels, meta = [], []
        for level in range(extra_mips+1):
            w, h = width >> level, height >> level
            if not w or not h:
                raise ValueError('mip exceeds dimensions')
            row = w*(bits//8)
            pitch = (row+3)&~3
            size = pitch*h
            if offset+size > len(data):
                raise ValueError('truncated mip payload')
            payload = data[offset:offset+size]
            tight = b''.join(payload[y*pitch:y*pitch+row] for y in range(h))
            levels.append(tight)
            meta.append({'width': w, 'height': h, 'pitch': pitch, 'offset': offset, 'bytes': size,
                         'sha256': hashlib.sha256(tight).hexdigest()})
            offset += size
        palette_bytes = palettes*1024
        if offset+palette_bytes > len(data):
            raise ValueError('truncated palette')
        palette = data[offset:offset+palette_bytes]
        offset += palette_bytes
        images.append(({'index': index, 'width': width, 'height': height, 'bits': bits,
                        'palette_count': palettes, 'mip_count': extra_mips+1, 'mips': meta,
                        'palette_sha256': hashlib.sha256(palette).hexdigest() if palette else None}, levels, palette))
    if offset != len(data):
        raise ValueError('unexplained trailing bytes')
    return images


def ground_identity(meta, pixels, half=False):
    """Reproduce the supported uncompressed 24-bit, single-level ground branch.

    X8R8G8B8, managed, one level. Half-size uses floor(sum of 2x2 / 4),
    not Pillow's rounded BOX chain. Compressed/fallback formats stay unverified.
    """
    width, height = meta['width'], meta['height']
    if meta['bits'] != 24 or meta['mip_count'] != 1 or width != 64 or height != 64 or len(pixels) != width*height*3:
        raise ValueError('identity builder only verifies 64x64, 24-bit, single-level ground')
    if half:
        source = pixels
        pixels = bytes(sum(source[((y+dy)*width+x+dx)*3+c] for dy in (0,1) for dx in (0,1))//4
                       for y in range(0,height,2) for x in range(0,width,2) for c in range(3))
        width //= 2
        height //= 2
    bgra = b''.join(pixels[i:i+3]+b'\xff' for i in range(0,len(pixels),3))
    return (width, height, 22, 1), bgra
