import csv
import json
from pathlib import Path


def verify_tribal_affinity(rows):
    matrix=rows('tribal_affinity_test.csv');index=0
    mapping=((0,1,2),(21,20,19),(26,23,24),(41,40,38))
    def common(row,person):
        assert row['stage']=='check_matched'
        assert (row['person_id'],row['force_id'],row['ruler_id'],row['actor_city'],row['game_year'],row['game_month'],row['game_day'])==(
            str(person),'2','635','8','194','1','1')
        assert all(row[key]=='-1' for key in ('target_person','target_unit','equipment_id','actor_unit','terrain_id'))
    def query(owner,person,effect,base,kind):
        nonlocal index
        row=matrix[index];index+=1;common(row,person)
        assert (row['effect_skill'],row['owned_skill'],row['scene'],row['event'])==(
            str(effect),str(effect),'ai_evaluation','tribal_base_mapping_checked')
        caller=(0x5e9cbe,0x5ed9d2)[owner]
        assert int(row['caller'])==caller
        assert (row['target_base'],row['target_force'],row['target_city'],row['target_x'],row['target_y'])==(
            str(base),'3',str(base if kind==0 else -1),'2','1')
        assert json.loads(row['context_json'])==dict(native_owner=caller-5,raw_id_only=True,
            selection_stage=('candidate_list_filter','member_selection_gate')[owner])
    for owner in range(2):
        for person in (600,14162):
            for raw in (-1,90,91,92,93,94,99,249):
                for base in (*range(-1,43),529):
                    for kind in range(3):
                        if 91<=raw<=94 and base in mapping[raw-91]:query(owner,person,raw,base,kind)
    for person in (600,14162):
        for effect in range(91,95):
            for kind,base in enumerate(mapping[effect-91]):
                for present in range(2):query(0,person,effect,base,kind)
    assert index==192
    fallback=matrix[index:index+8]
    for owned in (95,249):
        for person in (600,14162):
            for base in (0,41):
                row=matrix[index];index+=1;common(row,person)
                assert (row['owned_skill'],row['effect_skill'],row['scene'],row['event'])==(
                    str(owned),'95','unclassified_query','skill_predicate')
                assert all(row[key]=='-1' for key in ('target_base','target_force','target_city','target_x','target_y'))
                assert not row['context_json']
    for owner in range(2):
        for person in (600,14162):
            for effect in range(91,95):query(owner,person,effect,529,0)
    assert index==len(matrix)==216 and len({row['caller'] for row in fallback})==1
    root=Path(__file__).resolve().parents[1]
    with (root/'outputs/skill-telemetry/tribal-affinity-before.csv').open(encoding='utf-8-sig',newline='') as stream:before=list(csv.DictReader(stream))
    keys=('stage','scene','event','owned_skill','effect_skill','person_id','force_id','ruler_id','actor_city',
        'game_year','game_month','game_day','target_base','target_force','target_city','context_json')
    assert [[row[key] for key in keys] for row in before]==[[row[key] for key in keys] for row in fallback]
    return dict(tribal_affinity_rows=216,tribal_affinity_applied_rows=0,tribal_affinity_query_rows=216,
        tribal_affinity_new_query_rows=208,tribal_affinity_unchanged_95_fallback_rows=8,tribal_affinity_expanded_city_query_rows=16)
