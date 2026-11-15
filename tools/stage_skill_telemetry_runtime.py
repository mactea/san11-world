"""Physically copy the installed world into an isolated telemetry test directory."""
import argparse
import configparser
import hashlib
import json
import shutil
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT / 'outputs/skill-telemetry/runtime-staging.json')
    args = parser.parse_args()
    game = args.game.resolve(strict=True)
    built = ROOT / 'worldmod/build/d3d9.dll'
    verification = json.loads((ROOT / 'outputs/skill-telemetry/verification.json').read_text(encoding='utf-8'))
    if sha256(built) != verification['dll_sha256']:
        raise ValueError('Built DLL differs from the verified build')
    if sha256(game / 'san11pk_world.exe') != verification['exe_sha256']:
        raise ValueError('Game executable differs from the tested executable')
    original_ini = (game / 'worldmod.ini').read_bytes()
    config = configparser.ConfigParser()
    config.read_string(original_ini.decode('utf-8-sig'))
    section = config['san11pk_world']
    if section.get('data_dir') != 'world_custom' or section.get('save_dir') != 'UserData_kit':
        raise ValueError('Expected installed world_custom and isolated UserData_kit paths')
    stage = ROOT / 'worldmod/build' / ('telemetry-runtime-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    if not str(stage).isascii():
        raise ValueError('The native game requires an ASCII staging path')
    roots = [('Media', 'Media'), ('Doc', 'Doc'), ('world_custom', 'world_custom'),
             ('UserData_kit', 'UserData_telemetry')]
    files = [(game / name, name) for name in (
        'san11pk_world.exe', 'DrvMgt.dll', 'ali213.bin', '00000000.256',
        'STcheck.wma', 'winmm.dll', 'worldmusic.ini')]
    files.extend((path, path.name) for path in sorted(game.glob('worldmod_*.txt')))
    for source_root, target_root in roots:
        files.extend((path, str(Path(target_root) / path.relative_to(game / source_root)))
                     for path in sorted((game / source_root).rglob('*')) if path.is_file())
    required = [game / 'worldmod.ini', game / 'd3d9.dll', built] + [path for path, _ in files]
    if not all(path.is_file() and not path.is_symlink() for path in required):
        raise ValueError('Missing source files or linked files in staging inputs')
    total = sum(path.stat().st_size for path, _ in files) + built.stat().st_size
    if shutil.disk_usage(stage.parent).free < total + 1024 ** 3:
        raise ValueError('Insufficient free disk space for a physical copy')
    protected = {str(path): sha256(path) for path in (game / 'd3d9.dll', game / 'worldmod.ini')}
    stage.mkdir()
    records = []
    print(f'Staging {len(files)} files ({total:,} bytes) into {stage}', flush=True)
    for index, (source, relative) in enumerate(files):
        target = stage / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        before = sha256(source)
        shutil.copy2(source, target)
        if sha256(target) != before or sha256(source) != before:
            raise ValueError(f'Copy mismatch or changing source: {source}')
        records.append({'source': str(source), 'target': relative.replace('\\', '/'),
                        'size': target.stat().st_size, 'sha256': before})
        if index % 25 == 0:
            print(f'Verified {index + 1}/{len(files)} files', flush=True)
    shutil.copy2(built, stage / 'd3d9.dll')
    if sha256(stage / 'd3d9.dll') != verification['dll_sha256']:
        raise ValueError('Staged DLL mismatch')
    section['skill_telemetry'] = '1'
    section['save_dir'] = 'UserData_telemetry'
    with (stage / 'worldmod.ini').open('w', encoding='ascii', newline='') as stream:
        config.write(stream)
    for path, digest in protected.items():
        if sha256(Path(path)) != digest:
            raise ValueError(f'Original installation changed: {path}')
    report = {'stage': str(stage), 'exe': str(stage / 'san11pk_world.exe'),
              'created_at': datetime.now().astimezone().isoformat(),
              'physical_copy': True, 'copied_files': len(records), 'copied_bytes': total,
              'dll_sha256': sha256(stage / 'd3d9.dll'),
              'ini_sha256': sha256(stage / 'worldmod.ini'),
              'original_protected_sha256': protected, 'files': records,
              'copy_hashes_verified': True, 'runtime_started': False,
              'live_gameplay_validated': False, 'production_installed': False}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Isolated copy ready; manifest: {args.out}', flush=True)


if __name__ == '__main__':
    main()
