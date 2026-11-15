"""Inspect the staged game's real startup log and CSVs without controlling its UI."""
import argparse
import csv
import hashlib
import io
import json
import re
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def inspect_csv(path):
    data = path.read_bytes()
    complete = data.endswith(b'\r\n')
    if not complete:
        data = data[:data.rfind(b'\r\n') + 2]
    if not data.startswith(b'\xef\xbb\xbf'):
        raise ValueError(f'CSV is missing its UTF-8 BOM: {path}')
    reader = csv.DictReader(io.StringIO(data.decode('utf-8-sig'), newline=''), strict=True)
    rows = list(reader)
    if len(reader.fieldnames or []) != 36:
        raise ValueError(f'Unexpected CSV header: {path}')
    shared_writes = {}
    aggregate_writes = {}
    for expected, row in enumerate(rows, 1):
        if None in row or None in row.values() or int(row['seq']) != expected:
            raise ValueError(f'Malformed or out-of-sequence CSV row {expected}: {path}')
        if row['stage'] not in ('applied', 'check_matched'):
            raise ValueError(f'Unexpected stage in row {expected}: {path}')
        if row['stage'] == 'check_matched' and row['metric']:
            raise ValueError(f'Query row has an effect metric in row {expected}: {path}')
        if row['stage'] == 'check_matched' or not row['metric']:
            if any(row[key] for key in ('before', 'after', 'delta', 'baseline_after', 'bonus_delta')):
                raise ValueError(f'Invented numerical values in row {expected}: {path}')
        else:
            if int(row['after']) - int(row['before']) != int(row['delta']):
                raise ValueError(f'Incorrect delta in row {expected}: {path}')
            if row['baseline_after'] and int(row['after']) - int(row['baseline_after']) != int(row['bonus_delta']):
                raise ValueError(f'Incorrect bonus delta in row {expected}: {path}')
        if row['context_json']:
            context = json.loads(row['context_json'])
            if not isinstance(context, dict):
                raise ValueError(f'Unexpected context JSON in row {expected}: {path}')
            if 'shared_resource_write_id' in context:
                token = context['shared_resource_write_id']
                if type(token) is not int or token <= 0 or row['stage'] != 'applied':
                    raise ValueError(f'Invalid shared resource write in row {expected}: {path}')
                shared_writes.setdefault((row['pid'], token), []).append(row)
            if 'aggregate_settlement_id' in context:
                token=context['aggregate_settlement_id'];count=context['component_count'];index=context['component_index']
                if (type(token) is not int or token<=0 or type(count) is not int or not 2<=count<=12 or
                    type(index) is not int or not 0<=index<count or row['stage']!='applied' or
                    row['event']!='aggregated_attack_component_settled' or row['metric'] not in ('settled_component','calculated_physical_troop_damage')):
                    raise ValueError(f'Invalid aggregate attribution in row {expected}: {path}')
                aggregate_writes.setdefault((row['pid'],token),[]).append((row,context))
    for group in shared_writes.values():
        if len(group) > 2 or len({row['effect_skill'] for row in group}) != len(group):
            raise ValueError(f'Duplicate shared resource attribution: {path}')
        if len(group) == 2:
            if {row['effect_skill'] for row in group} != {'24', '50'} or any(
                group[0][key] != group[1][key] for key in ('thread', 'caller', 'metric', 'before', 'after', 'delta', 'actor_unit', 'target_unit')
            ):
                raise ValueError(f'Inconsistent shared resource attribution: {path}')
    for group in aggregate_writes.values():
        identities=set();first,context=group[0]
        for row,item in group:
            identity=(item['component_index'],item['component_role'],row['effect_skill'],row['person_id'])
            if identity in identities or any(first[k]!=row[k] for k in ('thread','caller','target_unit','target_force')) or any(
                context[k]!=item[k] for k in ('component_count','shared_requested_troop_loss','shared_resource_before','shared_resource_after')):
                raise ValueError(f'Inconsistent aggregate attribution: {path}')
            identities.add(identity)
    return {'file': str(path), 'complete_snapshot': complete, 'rows': len(rows),
            'applied_rows': sum(row['stage'] == 'applied' for row in rows),
            'effects': sorted({int(row['effect_skill']) for row in rows}),
            'people': sorted({int(row['person_id']) for row in rows}),
            'forces': sorted({int(row['force_id']) for row in rows}),
            'shared_resource_writes': len(shared_writes),
            'aggregate_settlements': len(aggregate_writes),
            'unpaired_shared_resource_writes': sum(len(group) != 2 for group in shared_writes.values())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=ROOT / 'outputs/skill-telemetry/runtime-staging.json')
    parser.add_argument('--compatibility', type=Path, default=ROOT / 'outputs/skill-telemetry/runtime-patch-compatibility.json')
    parser.add_argument('--out', type=Path, default=ROOT / 'outputs/skill-telemetry/runtime-inspection.json')
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    stage = Path(manifest['stage'])
    unchanged = all(sha256(Path(path)) == digest for path, digest in manifest['original_protected_sha256'].items())
    save_files = [record for record in manifest['files'] if '/SaveData/' in record['target']]
    save_hashes = {record['source']: sha256(Path(record['source'])) for record in save_files}
    save_baseline = manifest.get('current_original_save_sha256',
                                 {record['source']: record['sha256'] for record in save_files})
    if save_baseline.keys() != save_hashes.keys():
        raise ValueError('Save baseline must cover every protected save file')
    unchanged_saves = all(save_hashes[path] == digest for path, digest in save_baseline.items())
    dll_matches = sha256(stage / manifest.get('dll_file', 'd3d9.dll')) == manifest['dll_sha256']
    loader_matches = 'loader_sha256' not in manifest or sha256(stage / 'd3d9.dll') == manifest['loader_sha256']
    exe_record = next(record for record in manifest['files'] if record['target'] == 'san11pk_world.exe')
    exe_matches = sha256(stage / exe_record['target']) == exe_record['sha256']
    ini_matches = sha256(stage / 'worldmod.ini') == manifest['ini_sha256']
    log_path = stage / 'worldmod_san11pk_world.log'
    log = log_path.read_text(encoding='utf-8', errors='replace') if log_path.exists() else ''
    installed = re.search(r'skills: installed (\d+) hooks', log)
    expected_hooks = json.loads(args.compatibility.read_text(encoding='utf-8'))['hooks']
    problems = [line for line in log.splitlines() if any(word in line for word in (
        'ERROR:', 'MISMATCH ', 'BAD EXPR ', 'OVERFLOW ', 'CSV write failed', 'skill telemetry: FAILED'))]
    files = [inspect_csv(path) for path in sorted((stage / 'skill_logs').glob('*.csv'))]
    startup = bool('worldmod loaded;' in log and installed and int(installed[1]) == expected_hooks
                   and 'skill telemetry: enabled' in log and files and not problems
                   and dll_matches and loader_matches and exe_matches and ini_matches)
    report = {'observed_at': datetime.now().astimezone().isoformat(), 'stage': str(stage),
              'startup_observed': startup, 'runtime_log_present': log_path.exists(),
              'installed_hooks': int(installed[1]) if installed else None,
              'expected_hooks': expected_hooks, 'startup_errors': problems,
              'compatibility_report': str(args.compatibility), 'compatibility_report_sha256': sha256(args.compatibility),
              'staged_dll_matches': dll_matches, 'staged_exe_matches': exe_matches,
              'staged_loader_matches': loader_matches,
              'staged_ini_matches': ini_matches, 'original_dll_and_ini_unchanged': unchanged,
              'original_save_files_unchanged': unchanged_saves, 'original_save_files_checked': len(save_files),
              'original_save_baseline': 'current_observation' if 'current_original_save_sha256' in manifest else 'initial_copy',
              'original_save_files_match_initial_copy': all(save_hashes[record['source']] == record['sha256'] for record in save_files),
              'csv_files': files, 'live_gameplay_validated': False, 'production_installed': False}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))
    if not all((dll_matches, loader_matches, exe_matches, ini_matches, unchanged, unchanged_saves)) or problems:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
