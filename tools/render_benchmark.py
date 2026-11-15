"""Capture a normal-play render baseline without changing input or focus."""
import argparse
import configparser
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import time

import gamectl


CPU_STAGES = ('draw', 'state', 'begin', 'texture', 'pipeline', 'upload', 'record', 'submit', 'wait',
              'close', 'execute', 'signal', 'return_color', 'return_depth')
FLUSH_REASONS = ('explicit', 'clear', 'end_scene', 'color_target', 'depth_target',
                 'fallback_dp', 'fallback_dip', 'fallback_dpup', 'fallback_dipup', 'present',
                 'texture_budget', 'draw_limit', 'upload_limit', 'reset', 'process_vertices')


def wait_for_render(seconds):
    """Wait for two fresh foreground windows; never activate or send game input."""
    deadline = time.monotonic() + seconds
    stable = 0
    while time.monotonic() < deadline:
        raw = gamectl.run(['render_timing_reset', 'wait 1000', 'render_status'], timeout=10)
        foreground = re.search(r'^Present foreground: .* span_ms=([\d.]+)$', raw, re.MULTILINE)
        fresh = bool(foreground and 900 <= float(foreground[1]) <= 1500 and
                     'Present background: samples=0 ' in raw and
                     'automation=0 failed_calls=0 device_resets=0' in raw)
        stable = stable + 1 if fresh else 0
        if stable == 2:
            return
    raise SystemExit('Timed out waiting for sustained foreground rendering; no capture accepted.')


def profile_delta(before, after):
    """CPU wall time only; draw and begin contain nested stage measurements."""
    def parse(raw):
        if 'Native CPU profile: enabled=1 cumulative=1' not in raw:
            return {}
        return {name: (int(calls), float(ms)) for name, calls, ms in re.findall(
            r'^Native CPU (\w+): calls=(\d+) total_ms=([\d.]+)$', raw, re.MULTILINE)}
    start, end = parse(before), parse(after)
    if not all(name in start and name in end for name in CPU_STAGES):
        return None
    delta = {name: {'calls': end[name][0] - start[name][0],
                    'total_ms': end[name][1] - start[name][1]} for name in CPU_STAGES}
    if any(value['calls'] < 0 or value['total_ms'] < 0 for value in delta.values()):
        return None
    if not delta['draw']['calls'] or not delta['submit']['calls']:
        return None
    return delta


def flush_delta(before, after):
    """Active batch attempts only; no-op flush calls do not create batches."""
    def parse(raw):
        if 'Native flush profile: enabled=1 cumulative=1 scope=active_batches' not in raw:
            return {}
        return {name: tuple(map(int, (batches, draws, empty))) for name, batches, draws, empty in re.findall(
            r'^Native flush (\w+): batches=(\d+) draws=(\d+) empty=(\d+)$', raw, re.MULTILINE)}
    start, end = parse(before), parse(after)
    if not all(name in start and name in end for name in FLUSH_REASONS):
        return None
    delta = {name: dict(zip(('batches', 'draws', 'empty'), (b - a for a, b in zip(start[name], end[name]))))
             for name in FLUSH_REASONS}
    if any(value < 0 for row in delta.values() for value in row.values()):
        return None
    if not sum(row['batches'] for row in delta.values()):
        return None
    return delta


def gpu_upload_delta(before, after):
    """Completed native batches only; GPU results lag by up to four batch slots."""
    def parse(raw, label):
        match = re.search(rf'^Native {label} profile: (.+)$', raw, re.MULTILINE)
        if not match:
            return None
        return dict(re.findall(r'(\w+)=([\w.]+)', match[1]))
    result = {}
    for label, fields in [('GPU', ('batches', 'total_ms', 'errors')),
                          ('upload', ('bytes', 'buffer_copies', 'buffer_hits', 'texture_copies', 'texture_bytes'))]:
        start, end = parse(before, label), parse(after, label)
        if not start or not end or any(s.get('enabled') != '1' or s.get('cumulative') != '1' for s in (start, end)):
            return None
        if any(field not in s for field in fields for s in (start, end)):
            return None
        delta = {field: (float(end[field]) - float(start[field]) if field == 'total_ms'
                         else int(end[field]) - int(start[field])) for field in fields}
        if any(value < 0 for value in delta.values()):
            return None
        if label == 'GPU':
            if (any(s.get('scope') != 'native_batches' or 'pending' not in s for s in (start, end)) or
                    delta['errors'] or not delta['batches'] or delta['total_ms'] <= 0):
                return None
            delta.update(pending_before=int(start['pending']), pending_after=int(end['pending']))
        result[label.lower()] = delta
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--scene', required=True, help='save/view description for this comparison')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=30)
    parser.add_argument('--native-profile', action='store_true',
                        help='require WORLDMOD_TERRAIN_PROFILE=1 and capture CPU, GPU, upload and flush reason deltas')
    parser.add_argument('--wait-for-render', type=int, default=0,
                        help='wait up to this many seconds for fresh foreground frames before warmup (0-600)')
    args = parser.parse_args()
    if not 15 <= args.seconds <= 60:
        parser.error('seconds must be between 15 and 60 (bounded sample ring)')
    if not 0 <= args.wait_for_render <= 600:
        parser.error('wait-for-render must be between 0 and 600 seconds')
    if args.out.exists():
        parser.error('output already exists; choose a new file')
    gamectl.GAME = args.game_dir.resolve()
    ini = configparser.ConfigParser()
    ini.read(gamectl.GAME / 'worldmod.ini', encoding='utf-8')
    if not ini.has_section('san11pk_render'):
        parser.error('expected the isolated san11pk_render test configuration')
    section = ini['san11pk_render']
    if section.getint('automation', 0) or section.getint('trace', 0):
        parser.error('use automation=0 and trace=0, then restart the test game')
    if args.wait_for_render:
        print(f'Waiting up to {args.wait_for_render}s for sustained foreground rendering; no input will be sent.', flush=True)
        wait_for_render(args.wait_for_render)
    print('Keep the loaded game in the foreground. Sampling starts in 5 seconds.', flush=True)
    before = ''
    commands = ['wait 5000', 'render_timing_reset', f'wait {args.seconds * 1000}', 'render_status']
    if args.native_profile:
        before = gamectl.run(['wait 5000', 'render_timing_reset', 'render_status'], timeout=30)
        commands = [f'wait {args.seconds * 1000}', 'render_status']
    raw = gamectl.run(commands,
                      timeout=args.seconds + 30)
    profile = profile_delta(before, raw) if args.native_profile else None
    flush = flush_delta(before, raw) if args.native_profile else None
    gpu_upload = gpu_upload_delta(before, raw) if args.native_profile else None
    groups = {}
    for name in ('foreground', 'background'):
        match = re.search(rf'^Present {name}: (.+)$', raw, re.MULTILINE)
        if match:
            groups[name] = {key: float(value) for key, value in re.findall(r'(\w+)=([\d.]+)', match[1])}
    config = re.search(r'automation=(\d+) failed_calls=(\d+) device_resets=(\d+)', raw)
    backend = re.search(r'render requested=(\w+) actual=(\w+)', raw)
    terrain = re.search(r' terrain=(\w+)', raw)
    native = re.search(r'native_submitted_draws=(\d+).* enabled=(\d+) failed=(\d+)', raw)
    requested_terrain = section.get('terrain_renderer', 'original')
    terrain_valid = bool(terrain and requested_terrain == 'original' and terrain[1] == 'original')
    if requested_terrain == 'dx12':
        terrain_valid = bool(terrain and terrain[1] == 'dx12_mixed' and native and int(native[1]) > 0 and
                             native.groups()[1:] == ('1', '0') and 'native_shared_queue=1' in raw)
    mode = section.getint('render_present_interval', 0)
    presentation = re.search(r'Presentation: windowed=1 interval=([0-9a-fA-F]+)(?: frame_limit=(\d+))?', raw)
    mode_valid = mode == 0 or bool(presentation and mode in (1, 2, 3) and
                                  int(presentation[1], 16) == (1 if mode == 1 else 0x80000000) and
                                  (mode != 3 or presentation[2] == '60'))
    anisotropy = section.getint('terrain_anisotropy', 0)
    filtering = re.search(r'Terrain filtering: requested=(-?\d+) effective=(\d+) applied_draws=(\d+) skipped_draws=(\d+) errors=(\d+)', raw)
    filter_valid = anisotropy == 0 and (not filtering or filtering.groups()[:2] == ('0', '0'))
    if anisotropy in (2, 4, 8, 16):
        filter_valid = bool(filtering and int(filtering[1]) == anisotropy and
                            1 < int(filtering[2]) <= anisotropy and int(filtering[3]) > 0 and
                            filtering[5] == '0')
    valid = bool(config and backend and config.groups() == ('0', '0', '0') and
                 backend[1] == backend[2] == section.get('render_backend', 'system') and mode_valid and filter_valid and
                 terrain_valid and (not args.native_profile or (profile is not None and gpu_upload is not None and flush is not None)) and
                 groups.get('background', {}).get('samples') == 0 and
                 args.seconds * 950 <= groups.get('foreground', {}).get('span_ms', 0) <= args.seconds * 1100)
    report = {'utc': datetime.now(timezone.utc).isoformat(), 'scene': args.scene,
              'seconds': args.seconds, 'valid_foreground_capture': valid,
              'metric': 'CPU Present; not displayed FPS or GPU execution time',
              'ini': dict(section), 'present': groups, 'raw_status': raw}
    if args.native_profile:
        report.update(native_cpu=profile, native_gpu_upload=gpu_upload, native_flush=flush, raw_status_before=before,
                      native_flush_note='Active batch attempts by cause, including batches with zero draws; '
                                        'empty batches may contain uploads. Counters are cumulative across Reset. '
                                        'Live snapshots are not atomic; boundary counts may differ from CPU/GPU stages.',
                      native_gpu_note='Timestamp span of completed native command batches, including barriers/copies; '
                                      'excludes D3D9On12 fallback, COPY MSAA resolve and Present. '
                                      'Collected at existing fence reuse/reset, no added waits; up to four batches lag. '
                                      'Not whole-frame GPU time; boundary batches may cross the sample window.',
                      native_cpu_note='CPU wall time including instrumentation; cumulative snapshot deltas. '
                                      'draw includes stages, begin includes wait, submit may occur inside draw; '
                                      'submit includes close/execute/signal/return_color/return_depth; '
                                      'do not sum overlapping stages or interpret wait as GPU execution time.')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('x', encoding='utf-8') as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    print(raw, end='')
    print(f'Saved {args.out}; valid_foreground_capture={valid}')
    if not valid:
        raise SystemExit('Capture rejected: check diagnostic DLL, focus, device errors, and backend; report retained.')


if __name__ == '__main__':
    main()
