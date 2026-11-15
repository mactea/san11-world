import json


def verify_lucky_duel_post(rows):
    matrix=rows('lucky_duel_casualty_test.csv');index=0
    def query(person,owned,preview=False):
        nonlocal index
        row=matrix[index];index+=1
        assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'])==('check_matched','32',str(person),str(owned))
        assert (row['scene'],row['event'])==(('unclassified_query','skill_predicate') if preview else ('duel','lucky_postcheck_eligibility'))
    for kind in (0,1):
        for side in (0,1):
            for slot,person in enumerate((600,14162,601)):
                for owned in (32,249):
                    query(person,owned);row=matrix[index];index+=1
                    assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'],row['scene'],row['event'])==(
                        'applied','32',str(person),str(owned),'duel','duel_lucky_postcheck_blocked')
                    assert (row['force_id'],row['target_person'],row['target_force'],row['caller'])==(
                        '3',str(person),'3',str(0x50a5ab if kind else 0x50a52e))
                    assert all(row[key]=='-1' for key in ('actor_unit','target_unit','target_city','target_base','equipment_id','target_x','target_y'))
                    assert not any(row[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
                    assert json.loads(row['context_json'])==dict(check_id=kind+1,selected_side=side,selected_slot=slot,
                        controller_phase=3 if kind else 2,duel_action=5,native_result=0,
                        result_scope='native_duel_followup_check',counterfactual_evaluated=False)
    for kind in (0,1):
        for owned in (32,249):
            for fault in range(1,11 if owned==249 else 10):query(14162,owned)
    for kind in (0,1):
        for owned in (32,249):query(600,owned,True)
    assert index==len(matrix)==90
    return dict(lucky_duel_post_rows=90,lucky_duel_post_applied_rows=24,lucky_duel_post_query_rows=66)
