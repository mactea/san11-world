import json


def verify_area_friendly(rows):
    matrix=rows('area_friendly_test.csv');index=0
    providers=((600,44),(14162,44),(600,249),(14162,249))
    def query(person,owned,preview=False):
        nonlocal index
        row=matrix[index];index+=1
        assert (row['stage'],row['effect_skill'],int(row['person_id']),int(row['owned_skill']))==('check_matched','44',person,owned)
        assert (row['scene'],row['event'])==(('unclassified_query','skill_predicate') if preview else ('battle','same_force_area_target_eligibility'))
    def applied(person,owned,kind):
        nonlocal index
        row=matrix[index];index+=1
        assert (row['stage'],row['effect_skill'],int(row['person_id']),int(row['owned_skill']))==('applied','44',person,owned)
        assert (row['scene'],row['event'],int(row['caller']))==('battle','same_force_area_target_excluded',0x595388)
        assert (row['force_id'],row['actor_city'],row['actor_unit'],row['target_force'],row['equipment_id'])==('2','8','4095','2','3')
        assert (row['game_year'],row['game_month'],row['game_day'])==('194','1','1')
        assert row['target_person']=='-1'
        assert (row['target_unit'],row['target_base'],row['target_city'])==(
            ('4094','-1','-1') if kind==0 else ('-1','529','529' if kind==1 else '-1'))
        assert (row['target_x'],row['target_y'],row['terrain_id'])==('2','1','7')
        assert all(not row[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
        assert json.loads(row['context_json'])==dict(tactic_id=7,command_target_x=2,command_target_y=1,
            exclusion_reason='same_force',counterfactual_evaluated=False)
    for kind in range(5):
        for person,owned in providers:query(person,owned);applied(person,owned,kind)
    for kind in range(5):
        for owned in (44,249):query(600,owned);applied(600,owned,kind)
    for person,owned in providers:query(person,owned)
    for invalid in range(12):query(600,44)
    for person,owned in providers:query(person,owned,True)
    assert index==len(matrix)==80
    return dict(area_friendly_rows=80,area_friendly_applied_rows=30,area_friendly_query_rows=50)
