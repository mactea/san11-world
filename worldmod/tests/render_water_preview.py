"""Make a comparison sheet from GPU readbacks, without launching the game."""
import argparse
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFont

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, default=Path(__file__).resolve().parents[1] / 'build')
parser.add_argument('--output', type=Path)
args = parser.parse_args()
output = args.output or args.build_dir / 'water-preview-comparison.png'
font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 17)
small = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 14)
sheet = Image.new('RGB', (1104, 710), '#17212b')
draw = ImageDraw.Draw(sheet)
draw.text((20, 12), 'Water / shoreline prototype - GPU readbacks', font=font, fill='white')
draw.text((20, 39), 'Synthetic geometry, original atlases, 16 corner masks. Opt-in effect; not an in-game screenshot.', font=small, fill='#bdc9d5')
for row, (vendor, label) in enumerate((('8086', 'Intel Arc Pro'), ('10de', 'NVIDIA RTX 500 Ada'))):
    y = 78 + row * 310
    draw.text((20, y), label, font=font, fill='white')
    images = [Image.open(args.build_dir / f'water-preview-{vendor}-{phase}.bmp').convert('RGB')
              for phase in ('baseline', 'phase0', 'phase1')]
    assert all(image.size == (256, 256) for image in images)
    images.append(ImageChops.difference(images[0], images[1]).point(lambda value: min(255, value * 12)))
    for col, (image, name) in enumerate(zip(images, ('Original', 'Prototype / phase 0', 'Prototype / phase 1', 'Difference x12'))):
        x = 20 + col * 270
        draw.text((x, y + 27), name, font=small, fill='#bdc9d5')
        sheet.paste(image, (x, y + 49))
sheet.save(output)
print(output)
