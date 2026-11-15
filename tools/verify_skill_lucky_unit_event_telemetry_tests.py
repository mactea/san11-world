import json


def verify_lucky_unit_event(rows):
    matrix=rows('lucky_unit_event_test.csv');index=0
    def query(person,owned,preview=False):
        nonlocal index
        r=matrix[index];index+=1
        assert (r['stage'],r['effect_skill'],r['person_id'],r['owned_skill'])==('check_matched','32',str(person),str(owned))
        assert (r['scene'],r['event'])==(('unclassified_query','skill_predicate') if preview else ('unit_event','casualty_candidate_eligibility'))
    def applied(person,owned):
        nonlocal index
        r=matrix[index];index+=1
        assert (r['stage'],r['effect_skill'],r['person_id'],r['owned_skill'],r['scene'],r['event'])==(
            'applied','32',str(person),str(owned),'unit_event','unit_event_casualty_candidate_excluded')
        assert (r['force_id'],r['target_person'],r['target_force'],r['target_unit'],r['actor_unit'],r['equipment_id'],r['target_x'],r['target_y'],r['caller'])==(
            '2',str(person),'2','4095','4095','4','2','1',str(0x58308f))
        assert not any(r[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
        assert json.loads(r['context_json'])==dict(selector_call=0x58308a,selector_result=0,
            result_scope='excluded_from_unit_event_candidate_list',counterfactual_evaluated=False)
    for owned in (32,249):
        for person in (600,14162,601):query(person,owned);applied(person,owned)
    for person,owned in ((600,32),(14162,249),(601,32)):query(person,owned);applied(person,owned)
    for fault in range(1,10):query(600,32)
    for person,owned in ((600,32),(14162,249),(601,32)):query(person,owned,True)
    assert index==len(matrix)==30
    return dict(lucky_unit_event_rows=30,lucky_unit_event_applied_rows=9,lucky_unit_event_query_rows=21)
