"""Create an isolated render-test game from the user's own installation.

The destination must be a new directory under worldmod/build. No source files
are modified and no links to writable game data are created.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
from make_world_exe import main as make_world_exe

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', required=True, type=Path)
    ap.add_argument('--out', type=Path, default=ROOT / 'worldmod/build/render-game')
    ap.add_argument('--data-name', default='world_custom')
    ap.add_argument('--backend', choices=['system', '9on12'], default='system')
    ap.add_argument('--terrain', choices=['original', 'dx12'], default='original')
    a = ap.parse_args()
    if a.terrain == 'dx12' and a.backend != '9on12':
        ap.error('DX12 terrain requires --backend 9on12')
    source, target = a.game.resolve(), a.out.resolve()
    build = (ROOT / 'worldmod/build').resolve()
    if not target.is_relative_to(build) or target == build or target.exists():
        ap.error('destination must be a NEW directory inside worldmod/build')
    data = source / a.data_name
    if not data.is_dir() or Path(a.data_name).name != a.data_name:
        ap.error('data-name must name a world directory inside the source game')
    cities = json.loads((data / 'bases_cities.json').read_text(encoding='utf-8'))
    if cities['C'] < 249:
        ap.error('this test preset requires at least 249 cities (249 forces)')
    info = json.loads((data / 'world_info.json').read_text(encoding='utf-8'))
    dll = build / 'd3d9.dll'
    if not dll.is_file():
        ap.error('build worldmod first')
    target.mkdir(parents=True)
    for name in ['san11pk.exe', 'DrvMgt.dll', '00000000.256']:
        if (source / name).is_file():
            shutil.copy2(source / name, target / name)
    # Keep the game assets private. Omit only duplicate archive backups.
    shutil.copytree(source / 'Media', target / 'Media', ignore=shutil.ignore_patterns('* - *.bin'))
    shutil.copytree(data, target / a.data_name)
    for p in (ROOT / 'worldmod').glob('*.txt'):
        shutil.copy2(p, target / p.name)
    shutil.copy2(dll, target / 'd3d9.dll')
    # Match worldmod.cpp's map stack requirement, rounded up to a MiB. The PE
    # reserve also applies to driver/UI worker threads; 48 MiB each can exhaust
    # the x86 address space under D3D9On12 even when the game is idle.
    stack_reserve = (info['cols'] * info['rows'] * 5 + (8 << 20) + (1 << 20) - 1) & ~((1 << 20) - 1)
    make_world_exe(target, 'san11pk_render.exe', stack_reserve=stack_reserve)
    # Rebuild scenarios for this checkout's schema instead of reusing extended
    # persons/trade saves or scenarios from a different branch.
    subprocess.run([sys.executable, str(ROOT / 'tools/convert_scen.py'), '--game', str(target),
                    '--cities-json', str(data / 'bases_cities.json'), '--out', str(target / a.data_name),
                    '--forces', '249'], check=True)
    config = f'''[mod]
enable=1
verify_only=0
[san11pk_render]
width={info['cols']}
height={info['rows']}
china_x={info['china_hi']}
china_y={info['china_lo']}
world_terrain=1
data_dir={a.data_name}
bases=1
cities={cities['C']}
forces=249
units=1000
automation=1
auto_skip=1
trace=0
save_dir=UserData_render
render_backend={a.backend}
terrain_renderer={a.terrain}
'''
    (target / 'worldmod.ini').write_text(config, encoding='ascii')
    print(f'Render test ready: {target}')


if __name__ == '__main__':
    main()
