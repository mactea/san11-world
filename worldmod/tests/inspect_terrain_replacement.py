"""Measure and visualize the saved redraws; does not alter any texture."""
from pathlib import Path
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
folder = ROOT/'build/terrain-assets'
font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 16)
sheet = Image.new('RGB', (1064, 890), '#17212b')
draw = ImageDraw.Draw(sheet)
draw.text((16, 10), 'Ground index 1: exact original season aliases; two redraw candidates; not deployed', font=font, fill='white')
results = []
for row, (rid, asset, label) in enumerate((('12c0','ground-12c0-01-v2.png','Spring / summer / autumn'),
                                           ('12c3','ground-12c3-01-v1.png','Winter'))):
    original = Image.open(folder/f'{rid}-01.png').convert('RGB')
    target = Image.open(ROOT/'assets/terrain-samples'/asset).convert('RGB')
    a,b = np.asarray(target,dtype=float),np.asarray(original,dtype=float)
    metrics = {'asset':asset,'mean_rgb_original':b.mean((0,1)).tolist(),'mean_rgb_redraw':a.mean((0,1)).tolist(),
               'horizontal_edge_delta':float(abs(a[:,0]-a[:,-1]).mean()),'vertical_edge_delta':float(abs(a[0]-a[-1]).mean()),
               'horizontal_interior_delta':float(abs(np.diff(a,axis=1)).mean()),'vertical_interior_delta':float(abs(np.diff(a,axis=0)).mean()),
               'art_accepted':False,'runtime_identity_verified':False}
    results.append(metrics)
    y=44+row*410
    draw.text((16,y),label+' | Original 64x64 | Redraw | 2x2 repeat inspection',font=font,fill='#bdc9d5')
    sheet.paste(original.resize((256,256),Image.Resampling.NEAREST),(16,y+30))
    sheet.paste(target.resize((256,256),Image.Resampling.LANCZOS),(288,y+30))
    tile=target.resize((190,190),Image.Resampling.LANCZOS)
    for dy in (0,190):
        for dx in (0,190):sheet.paste(tile,(568+dx,y+30+dy))
    draw.text((16,y+300), '11 target mips; source byte matching includes all 7 fixture mips.',font=font,fill='white')
    draw.text((16,y+328), 'Seam, material appearance and real UV acceptance remain pending.',font=font,fill='#e5be8a')
(folder/'replacement-art-qa.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
sheet.save(folder/'replacement-art-comparison.png')
print(json.dumps(results))
