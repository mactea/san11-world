"""Render the current merchant mesh into the native properties header card."""
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from preview_trade_ship import render
from build_trade_ship_model import build_mesh


def main():
    root = Path(__file__).resolve().parents[1]
    pixels = np.array(render(build_mesh(), 2.25, .38, 512).convert('RGBA'))
    pixels[:, :, 3] = np.where(np.any(pixels[:, :, :3] != [25, 37, 43], axis=2), 255, 0)
    icon = Image.fromarray(pixels)
    icon = icon.crop(icon.getbbox())
    icon.thumbnail((60, 44), Image.Resampling.LANCZOS)
    card = Image.new('RGBA', (480, 48))
    card.alpha_composite(icon, (0, (48-icon.height)//2))
    draw = ImageDraw.Draw(card)
    font = 'C:/Windows/Fonts/msjh.ttc'
    draw.text((68, 0), '大型海船 · 雙桅雙帆', font=ImageFont.truetype(font, 19), fill=(235, 216, 159))
    draw.text((68, 27), '寬厚船體 · 分段艙篷 · 開闊貨運甲板',
              font=ImageFont.truetype(font, 14), fill=(205, 218, 213))
    target = root / 'data/trade_ships/merchant/properties.png'
    card.save(target)
    print(target)


if __name__ == '__main__':
    main()
