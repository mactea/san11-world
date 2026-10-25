"""Snapshot the current roster into a resumable, identity-based portrait queue."""
import hashlib
import json
from collections import Counter
from datetime import datetime
from pathlib import Path

PROJ = Path(__file__).resolve().parents[2]
ROOT = PROJ / 'data/persons/portraits'


def key(person):
    if person.get('qid'):
        return person['qid']
    identity = f"{person['name_full']}|{person.get('name_orig', '')}|{person['birth']}|{person['death']}"
    return hashlib.sha256(identity.encode()).hexdigest()[:20]


def main():
    source = PROJ / 'data/persons/roster.json'
    data = source.read_bytes()
    roster = json.loads(data)
    path = ROOT / 'queue.json'
    previous = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'people': []}
    old = {p['key']: p for p in previous['people']}
    installed = json.loads((ROOT / 'installed.json').read_text(encoding='utf-8'))
    catalog_path = ROOT / 'catalog.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8')) if catalog_path.exists() else []
    assigned = {(p['name'], p['face']) for p in installed['roster_people'] + catalog}
    people = []
    for p in roster:
        k = key(p)
        state = old.get(k, {})
        face = p.get('face', -1)
        is_installed = (p['name_full'], face) in assigned
        status = 'installed' if is_installed else state.get('status', 'pending')
        if face >= 0 and not is_installed and k not in old:
            status = 'existing_unverified'
        people.append(dict(key=k, roster_id=p['id'], name=p['name_full'],
                           name_orig=p.get('name_orig', ''), birth=p['birth'], death=p['death'],
                           region=p['region'], sex=p['sex'], role=p['role'], face=face,
                           bio=p['bio'], sources=p['src'], status=status,
                           assets=state.get('assets', []), references=state.get('references', [])))
    keys = [p['key'] for p in people]
    # Distinct source records can share a Wikidata key; retain both and flag for identity review.
    duplicates = {k: n for k, n in Counter(keys).items() if n > 1}
    result = dict(updated_at=datetime.now().isoformat(), source_sha256=hashlib.sha256(data).hexdigest(),
                  total=len(people), status_counts=dict(Counter(p['status'] for p in people)),
                  duplicate_identity_keys=duplicates, people=people)
    path.write_text(json.dumps(result, ensure_ascii=False, indent=1), encoding='utf-8')
    summary = {k: v for k, v in result.items() if k != 'people'}
    (ROOT / 'progress.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=False))


if __name__ == '__main__':
    main()
