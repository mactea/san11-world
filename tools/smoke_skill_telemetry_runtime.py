"""Run a bounded, hidden process test of the isolated DLL initializer; no UI input."""
import argparse
import hashlib
import json
import subprocess
import time
from datetime import datetime
from pathlib import Path
from inspect_skill_telemetry_runtime import inspect_csv


ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=ROOT / 'outputs/skill-telemetry/runtime-staging.json')
    parser.add_argument('--out', type=Path, default=ROOT / 'outputs/skill-telemetry/runtime-smoke.json')
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    stage = Path(manifest['stage']).resolve(strict=True)
    stage.relative_to((ROOT / 'worldmod/build').resolve(strict=True))
    exe = stage / 'san11pk_world.exe'
    record = next(item for item in manifest['files'] if item['target'] == exe.name)
    if exe.resolve() == Path(record['source']).resolve():
        raise ValueError('The smoke test must not start the original installation')
    if sha256(exe) != record['sha256'] or sha256(stage / manifest.get('dll_file', 'd3d9.dll')) != manifest['dll_sha256']:
        raise ValueError('Staged executable/DLL differs from the verified inputs')
    if 'loader_sha256' in manifest and sha256(stage / 'd3d9.dll') != manifest['loader_sha256']:
        raise ValueError('Staged loader differs from the preserved installed loader')
    if sha256(stage / 'worldmod.ini') != manifest['ini_sha256']:
        raise ValueError('Staged configuration differs from the isolated configuration')
    # Keep native game startup hidden, and stop at DLL initialization rather
    # than interact with a menu or leave a game session running unattended.
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
    started_ns = time.time_ns()
    started = time.monotonic()
    print('Launching isolated DLL-initialization smoke test (30-second limit)', flush=True)
    process = subprocess.Popen([str(exe)], cwd=stage, startupinfo=startup,
                               creationflags=subprocess.CREATE_NO_WINDOW,
                               stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    log_path = stage / 'worldmod_san11pk_world.log'
    reason = 'timeout'
    log = ''
    csv_files = []
    terminated = False
    live_csv_reads = []
    live_csv_read_error = None
    try:
        while time.monotonic() - started < 30:
            if log_path.exists() and log_path.stat().st_mtime_ns >= started_ns:
                log = log_path.read_text(encoding='utf-8', errors='replace')
            csv_files = [path for path in (stage / 'skill_logs').glob(f'skills-*-{process.pid}-*.csv')
                         if path.stat().st_mtime_ns >= started_ns]
            if 'skill telemetry: enabled' in log and 'garrison: installed;' in log and csv_files:
                reason = 'dll_init_observed'
                try:
                    if process.poll() is None:
                        live_csv_reads = [inspect_csv(path) for path in csv_files]
                except (OSError, ValueError) as error:
                    live_csv_read_error = str(error)
                break
            if process.poll() is not None:
                reason = 'process_exited'
                break
            time.sleep(0.01)
    finally:
        # Popen retains the exact process handle. Never terminate by process
        # name or a PID that could refer to another game session.
        if process.poll() is None:
            process.terminate()
            terminated = True
        process.wait(timeout=10)
    if log_path.exists():
        log = log_path.read_text(encoding='utf-8', errors='replace')
    report = {'started_at': datetime.fromtimestamp(started_ns / 1e9).astimezone().isoformat(),
              'stage': str(stage), 'pid': process.pid, 'elapsed_seconds': round(time.monotonic() - started, 3),
              'stop_reason': reason, 'terminated_by_test': terminated, 'exit_code': process.returncode,
              'dll_init_observed': reason == 'dll_init_observed', 'csv_files': [str(path) for path in csv_files],
              'csv_read_while_process_alive': live_csv_reads, 'live_csv_read_error': live_csv_read_error,
              'startup_errors': [line for line in log.splitlines() if any(word in line for word in (
                  'ERROR:', 'MISMATCH ', 'BAD EXPR ', 'OVERFLOW ', 'skill telemetry: FAILED', 'CSV write failed'))],
              'log_file': str(log_path), 'dll_sha256': manifest['dll_sha256'],
              'dll_file': manifest.get('dll_file', 'd3d9.dll'), 'loader_sha256': manifest.get('loader_sha256'),
              'ui_input_performed': False, 'menu_verified': False,
              'live_gameplay_validated': False, 'production_installed': False}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    if log_path.exists():
        snapshot = args.out.with_suffix('.log')
        snapshot.write_bytes(log_path.read_bytes())
        report['log_snapshot_file'] = str(snapshot)
        report['log_snapshot_sha256'] = sha256(snapshot)
    args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False), flush=True)
    if not report['dll_init_observed'] or report['startup_errors'] or not live_csv_reads or live_csv_read_error:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
