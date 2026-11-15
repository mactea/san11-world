"""Compile the production embedded HLSL with the installed Windows SDK."""
from pathlib import Path
import os
import re
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'src/terrain_dx12.cpp').read_text(encoding='utf-8')
shader = re.search(r'const char\* shader = R"\((.*?)\)";', source, re.S)
if not shader:
    raise SystemExit('production shader literal not found')
build = root / 'build'
build.mkdir(exist_ok=True)
path = build / 'terrain-validation.hlsl'
path.write_text(shader.group(1), encoding='utf-8')
sdk = Path(os.environ['ProgramFiles(x86)']) / 'Windows Kits/10/bin'
compilers = list(sdk.glob('*/x64/fxc.exe'))
if not compilers:
    raise SystemExit('Windows SDK fxc.exe is required')
compiler = max(compilers, key=lambda p: tuple(map(int, p.parent.parent.name.split('.'))))
for preview in (False, True):
    for stage, entry in [('vs', 'vertexMain'), ('ps', 'pixelMain')]:
        defines = ['/D', 'WATER_EFFECT=1'] if preview else []
        subprocess.run([str(compiler), '/nologo', '/T', stage + '_5_0', '/E', entry,
                        *defines, '/Fo', str(build / (('water-preview.' if preview else 'terrain.') + stage)), str(path)], check=True)
print('PASS production terrain and optional water effect shaders')
