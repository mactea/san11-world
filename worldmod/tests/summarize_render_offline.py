"""Summarize an accepted offline run; never launch the game or deploy files."""
import argparse
import json
import math
from pathlib import Path
import re
import statistics


def summarize(run):
    suite = json.loads((run / 'summary.json').read_text(encoding='utf-8-sig'))
    if not suite['passed'] or not suite['gpu_preference_restored']:
        raise ValueError('only a successful, restored run can be summarized')
    result = {'run': run.name, 'timing_unit': 'microseconds per 32 draws, CPU wall time including waits',
              'excluded_profile_iteration': 1, 'game_fps_measured': False, 'gpus': {}}
    for gpu in (1, 2):
        log = (run / f'gpu{gpu}-native.stdout.log').read_text(encoding='utf-8', errors='replace')
        adapter = re.search(r'Test backend:.*adapter=([^\r\n]+)', log).group(1)
        rows = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines()
                if line.startswith('Terrain perf scenario=')]
        scenarios = {}
        for scenario in ('small-reuse', 'large-reuse', 'large-rewrite', 'full-span-rewrite'):
            paths = {}
            for path in ('prefix', 'compact'):
                selected = [r for r in rows if r['scenario'] == scenario and r['upload'] == path
                            and r['iteration'] != '1']
                if len(selected) != 14 or any(r['parity'] != 'exact' for r in selected):
                    raise ValueError(f'incomplete comparison: {adapter} {scenario} {path}')
                values = sorted(float(r['submit_us']) for r in selected)
                paths[path] = {'samples': len(values), 'median_us': statistics.median(values),
                               'min_us': values[0], 'max_us': values[-1],
                               'p95_us': values[math.ceil(len(values) * .95) - 1],
                               'vertex_bytes': sorted({int(r['vertex_bytes']) for r in selected}),
                               'copies': sorted({int(r['copies']) for r in selected}),
                               'batches': sorted({int(r['batches']) for r in selected})}
            paths['median_reduction_percent'] = 100 * (1 - paths['compact']['median_us'] / paths['prefix']['median_us'])
            scenarios[scenario] = paths
        water = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines()
                 if line.startswith('Water parity ')]
        production_water = 'PASS production water layout/' in log
        scenarios_count = 6 if production_water else 3
        expected = {(str(i), str(s), str(p)) for i in range(3) for s in range(scenarios_count) for p in range(64)}
        actual = {(r['iteration'], r['scenario'], r['phase']) for r in water}
        if len(water) != len(expected) or actual != expected or any(int(r['bad_channels']) for r in water):
            raise ValueError(f'incomplete water reference: {adapter}')
        objects = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines()
                   if line.startswith('Object parity ')]
        if objects:
            families = 1 + max(int(r.get('family', '0')) for r in objects)
            samples = (1, 4) if 'samples' in objects[0] else (1,)
            expected_objects = {(str(i), str(f), str(n), str(m), str(c)) for i in range(3)
                                for f in range(families) for n in samples for m in range(1 if f>=4 else 2) for c in range(8)}
            actual_objects = {(r['iteration'], r.get('family', '0'), r.get('samples', '1'), r['mode'], r['case']) for r in objects}
            if len(objects) != len(expected_objects) or actual_objects != expected_objects or any(int(r['bad_channels']) for r in objects):
                raise ValueError(f'incomplete object reference: {adapter}')
        screen = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines()
                  if line.startswith('Screen parity ')]
        if screen:
            expected_screen = {(str(i), str(l), str(n), str(c)) for i in range(3) for l in range(3) for n in (1, 4) for c in range(1+max(int(r['case']) for r in screen))}
            actual_screen = {(r['iteration'], r['layout'], r['samples'], r['case']) for r in screen}
            if len(screen)!=len(expected_screen) or actual_screen!=expected_screen or any(int(r['bad_channels']) for r in screen):
                raise ValueError(f'incomplete screen reference: {adapter}')
        up = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines() if line.startswith('UP parity ')]
        if up:
            expected_up = {(str(i), str(l), str(n), str(t), str(k)) for i in range(3)
                           for l in range(6) for n in (1, 4) for t in (4, 5, 6) for k in range(3)}
            actual_up = {(r['iteration'], r['layout'], r['samples'], r['topology'], r['kind']) for r in up}
            if len(up) != len(expected_up) or actual_up != expected_up or any(int(r['bad_channels']) for r in up):
                raise ValueError(f'incomplete UP reference: {adapter}')
        preview = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines() if line.startswith('PASS water preview ')]
        expected_preview = {(str(i), str(n)) for i in range(3) for n in ((1, 4) if preview and 'samples' in preview[0] else (1,))}
        if preview and (len(preview) != len(expected_preview) or {(r['iteration'], r.get('samples', '1')) for r in preview} != expected_preview or
                        any(int(r['changed_pixels']) <= 1000 or int(r['animated_pixels']) <= 1000 or
                            any(r[key] != '1' for key in ('disabled_exact', 'transparent_mask', 'full_fog')) for r in preview)):
            raise ValueError(f'incomplete water preview checks: {adapter}')
        default_contract = {}
        for backend in ('9on12', 'native'):
            contract_log = (run / f'gpu{gpu}-{backend}.stdout.log').read_text(encoding='utf-8', errors='replace')
            contracts = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in contract_log.splitlines()
                         if line.startswith('Default texture contract ')]
            expected_contract = {(str(i), str(f), op) for i in range(3) for f in (21, 894720068)
                                 for op in ('UpdateTexture', 'rewrite', 'partial-UpdateSurface')}
            if contracts and (len(contracts) != 18 or {(r['iteration'], r['format'], r['operation']) for r in contracts} != expected_contract or
                              any(r['exact'] != '1' or r['levels'] != '3' or r['dynamic'] != '0' for r in contracts)):
                raise ValueError(f'incomplete qualified DEFAULT texture contract: {adapter} {backend}')
            default_contract[backend] = len(contracts)
        sample = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines()
                  if line.startswith('Texture parity case=redraw-sample-mips ')]
        if sample and (len(sample) != 12 or any(int(r['bad_pixels']) for r in sample)):
            raise ValueError(f'incomplete redraw mip comparison: {adapter}')
        replacement = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines()
                       if line.startswith('Texture parity case=replacement-')]
        selectors = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines()
                     if line.startswith('PASS replacement selector ')]
        if replacement and (len(replacement)!=66 or any(int(r['bad_pixels']) for r in replacement) or
                            len(selectors)!=3 or {r['iteration'] for r in selectors}!={'0','1','2'} or
                            any(r[k]!='1' for r in selectors for k in ('exact_all_mips','bounds','mutation_fallback','budget_fallback','off_restored'))):
            raise ValueError(f'incomplete terrain replacement checks: {adapter}')
        loader = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines()
                  if line.startswith('Texture parity case=loader-profile-')]
        loader_flags = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in log.splitlines()
                        if line.startswith('PASS ground loader profiles ')]
        if loader and (len(loader)!=72 or any(int(r['bad_pixels']) for r in loader) or len(loader_flags)!=3 or
                       {r['iteration'] for r in loader_flags}!={'0','1','2'} or
                       any(r['profiles']!='4' or r['scales']!='4' or r['source_levels']!='1' or r['mutation_fallback']!='1' for r in loader_flags)):
            raise ValueError(f'incomplete ground loader profile checks: {adapter}')
        result['gpus'][adapter] = {'performance': scenarios, 'water_comparisons': len(water),
                                  'loader_profile_comparisons':len(loader),
                                  'loader_profile_max_rgb_error':max((int(r['max_error']) for r in loader),default=None),
                                  'replacement_comparisons':len(replacement),
                                  'replacement_max_rgb_error':max((int(r['max_error']) for r in replacement),default=None),
                                  'qualified_default_texture_contract': default_contract,
                                  'redraw_mip_comparisons': len(sample),
                                  'redraw_mip_max_rgb_error': max((int(r['max_error']) for r in sample), default=None),
                                  'up_comparisons': len(up),
                                  'up_max_rgb_error': max((int(r['max_error']) for r in up), default=None),
                                  'water_preview': preview,
                                  'object_comparisons': len(objects),
                                  'screen_comparisons': len(screen),
                                  'screen_max_rgb_error': max((int(r['max_error']) for r in screen), default=None),
                                  'object_max_rgb_error': max((int(r['max_error']) for r in objects), default=None),
                                  'production_water': production_water,
                                  'water_max_rgb_error': max(int(r['max_error']) for r in water),
                                  'native_quality': [line for line in log.splitlines() if line.startswith('Native quality ')],
                                  'presentation_msaa': [line for line in log.splitlines() if line.startswith('PASS presentation MSAA=')],
                                  'quality_caps': [line for line in log.splitlines() if line.startswith('Quality caps:')]}
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    result = summarize(args.run)
    output = args.run / 'offline-comparison.json'
    output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(output)
