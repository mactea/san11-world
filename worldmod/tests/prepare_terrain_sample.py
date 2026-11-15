"""Build mip/readback fixtures and QA measurements for an imagegen sample.

Resampling is the texture build step, not a claim of seamless artistic repair.
No original game texture or resource pack is changed.
"""
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from inventory_terrain_assets import main as inventory_assets

ROOT = Path(__file__).resolve().parents[1]
inventory_assets()
sample = ROOT/'assets/terrain-samples/ground-12c0-01-v1.png'
original = ROOT/'build/terrain-assets/12c0-01.png'
output = ROOT/'build/terrain-assets/sample-v1'
output.mkdir(parents=True, exist_ok=True)
image = Image.open(sample).convert('RGB')
reference = Image.open(original).convert('RGB')
levels, pixels = [], []
level = image.resize((1024, 1024), Image.Resampling.LANCZOS)
while True:
    name = f'mip-{len(levels):02d}.png'
    level.save(output/name)
    raw = level.convert('RGBA').tobytes('raw', 'BGRA')
    levels.append({'file': name, 'width': level.width, 'height': level.height,
                   'offset': sum(len(p) for p in pixels), 'bytes': len(raw),
                   'sha256': hashlib.sha256(raw).hexdigest()})
    pixels.append(raw)
    if level.width == 1:
        break
    level = level.resize((level.width//2, level.height//2), Image.Resampling.BOX)
(output/'sample-mips.bgra').write_bytes(b''.join(pixels))
a = np.asarray(image, dtype=np.float64)
b = np.asarray(reference, dtype=np.float64)
seams = {'horizontal_edge_mean_delta': float(abs(a[:, 0]-a[:, -1]).mean()),
         'vertical_edge_mean_delta': float(abs(a[0]-a[-1]).mean()),
         'interior_horizontal_mean_delta': float(abs(np.diff(a, axis=1)).mean()),
         'interior_vertical_mean_delta': float(abs(np.diff(a, axis=0)).mean())}
report = {'source': str(sample.relative_to(ROOT)), 'source_sha256': hashlib.sha256(sample.read_bytes()).hexdigest(),
          'reference': 'autumn ground WFT 0x12c0 / image 1', 'original_size': list(reference.size),
          'generated_size': list(image.size), 'build_size': [1024, 1024], 'format': 'A8R8G8B8',
          'mip_filter': 'LANCZOS base resize, BOX encoded-channel mip averages; no sRGB conversion',
          'mips': levels, 'total_payload_bytes': sum(len(p) for p in pixels),
          'mean_rgb_original': b.mean(axis=(0, 1)).tolist(), 'mean_rgb_sample': a.mean(axis=(0, 1)).tolist(),
          'edge_metrics': seams, 'exact_opposite_edges': bool(np.array_equal(a[:, 0], a[:, -1]) and np.array_equal(a[0], a[-1])),
          'seam_acceptance': 'pending visual/in-game acceptance; metrics do not prove seamlessness',
          'deployed': False, 'all_seasons_redrawn': False}
(output/'sample-build.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 16)
sheet = Image.new('RGB', (1064, 610), '#17212b')
draw = ImageDraw.Draw(sheet)
draw.text((16, 10), 'Autumn ground sample - resource 0x12c0, image 1 - not deployed', font=font, fill='white')
draw.text((16, 38), 'Original 64x64 enlarged | Generated redraw | 2x2 repeat inspection', font=font, fill='#bdc9d5')
sheet.paste(reference.resize((256,256), Image.Resampling.NEAREST), (16, 76))
sheet.paste(image.resize((256,256), Image.Resampling.LANCZOS), (288, 76))
tile = image.resize((248,248), Image.Resampling.LANCZOS)
for y in (76,324):
    for x in (560,808):
        sheet.paste(tile, (x,y))
draw.text((16, 354), 'Sample mip chain: 1024 to 1, 11 levels', font=font, fill='white')
draw.text((16, 381), 'Edges are not pixel-identical; seamlessness is not certified.', font=font, fill='#e5be8a')
draw.text((16, 408), 'Other seasons and actual UV/terrain-type mapping remain pending.', font=font, fill='#bdc9d5')
sheet.save(output/'sample-comparison.png')
print(json.dumps({'output': str(output), 'mips': len(levels), 'bytes': report['total_payload_bytes'], 'edge_metrics': seams}))
