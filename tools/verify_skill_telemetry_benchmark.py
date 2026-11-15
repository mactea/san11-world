"""Validate sustained production-writer CSV output separately from game fixtures."""
import argparse
import csv
import hashlib
import json
import platform
from collections import Counter
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--folder', type=Path, default=ROOT / 'worldmod/build/telemetry-benchmark')
    parser.add_argument('--out', type=Path, default=ROOT / 'outputs/skill-telemetry/writer-benchmark.json')
    args = parser.parse_args()
    folder = args.folder.resolve(strict=True)
    folder.relative_to((ROOT / 'worldmod/build').resolve(strict=True))
    measurement = folder / 'measurements.json'
    result = json.loads(measurement.read_text(encoding='utf-8'))
    assert result['writer_only'] and result['synthetic_resolver'] and not result['live_gameplay_validated']
    expected = {'disabled': (1000000, 1, 0, 0), 'queries': (20000, 1, 20000, 0),
                'applied': (20000, 1, 20000, 20000), 'mixed_single': (100000, 1, 100000, 10000),
                'mixed_four': (100000, 4, 100000, 10000)}
    assert [case['name'] for case in result['cases']] == list(expected)
    files = []
    for case in result['cases']:
        calls, workers, expected_rows, expected_applied = expected[case['name']]
        assert (case['calls'], case['workers'], case['expected_rows'], case['expected_applied']) == (
            calls, workers, expected_rows, expected_applied)
        assert case['resolver_calls'] == expected_rows
        path = folder / f"{case['name']}.csv"
        if not expected_rows:
            assert not case['enabled'] and not path.exists()
            continue
        assert case['enabled']
        with path.open('rb') as stream:
            assert stream.read(3) == b'\xef\xbb\xbf'
        counts, applied, threads, pids, total = Counter(), 0, set(), set(), 0
        with path.open(encoding='utf-8-sig', newline='') as stream:
            reader = csv.DictReader(stream)
            for total, row in enumerate(reader, 1):
                assert len(row) == 36 and None not in row and None not in row.values()
                assert int(row['seq']) == total
                person = int(row['person_id'])
                assert 14159 <= person < 14159 + workers
                assert row['person_name'] == 'CSV,"actor' and row['ruler_name'] == 'CSV ruler'
                assert (row['owned_skill'], row['effect_skill'], row['scene']) == ('249', '24', 'benchmark')
                assert (row['force_id'], row['ruler_id'], row['actor_city']) == ('249', '14162', '529')
                assert (row['game_year'], row['game_month'], row['game_day']) == ('194', '6', '11')
                threads.add(row['thread']);pids.add(row['pid'])
                counts[person, row['stage']] += 1
                if row['stage'] == 'applied':
                    applied += 1
                    assert row['event'] == 'resource_committed' and row['metric'] == 'troops'
                    assert (row['before'], row['after'], row['delta']) == ('1000', '900', '-100')
                    assert row['baseline_after'] == row['bonus_delta'] == ''
                    assert (row['target_unit'], row['actor_unit'], row['target_base']) == ('999', '998', '8')
                    assert (row['target_x'], row['target_y'], row['terrain_id']) == ('900', '1700', '6')
                    assert json.loads(row['context_json']) == {
                        'performance_fixture': True, 'whole_change_not_marginal_skill_bonus': True}
                else:
                    assert row['stage'] == 'check_matched' and row['event'] == 'predicate'
                    assert all(row[key] == '' for key in (
                        'metric', 'before', 'after', 'delta', 'baseline_after', 'bonus_delta', 'context_json'))
        assert (total, applied, len(threads), len(pids)) == (expected_rows, expected_applied, workers, 1)
        for person in range(14159, 14159 + workers):
            assert counts[person, 'applied'] == expected_applied // workers
            assert counts[person, 'check_matched'] == (expected_rows - expected_applied) // workers
        files.append({'file': str(path), 'rows': total, 'applied_rows': applied,
                      'bytes': path.stat().st_size, 'sha256': sha256(path)})
    result.update(verified_at=datetime.now().astimezone().isoformat(), csv_validation_passed=True,
                  csv_rows=sum(item['rows'] for item in files), csv_files=files,
                  measurement_file=str(measurement), measurement_sha256=sha256(measurement),
                  platform=platform.platform(), processor=platform.processor(),
                  executable_sha256=sha256(ROOT / 'worldmod/build/skill_telemetry_benchmark.exe'),
                  source_sha256={name: sha256(ROOT / name) for name in (
                      'worldmod/src/skill_telemetry.cpp', 'worldmod/src/skill_telemetry.h',
                      'worldmod/test/skill_telemetry_benchmark.cpp',
                      'worldmod/test/build_skill_telemetry_benchmark.bat',
                      'tools/verify_skill_telemetry_benchmark.py')},
                  game_hook_overhead_measured=False, production_ready=False, goal_complete=False)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'csv_validation_passed': True, 'csv_rows': result['csv_rows'],
                      'applied_rows': sum(item['applied_rows'] for item in files),
                      'out': str(args.out)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
