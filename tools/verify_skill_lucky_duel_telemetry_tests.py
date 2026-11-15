import json


def verify_lucky_duel(rows):
    matrix=rows('lucky_duel_test.csv');index=0
    def query(person,owned,preview=False):
        nonlocal index
        row=matrix[index];index+=1
        assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'])==('check_matched','32',str(person),str(owned))
        assert (row['scene'],row['event'])==(('unclassified_query','skill_predicate') if preview else ('duel','lucky_roll_eligibility'))
    for mode in range(3):
        for side in (0,1):
            for person in (600,14162,601):
                for owned in (32,249):
                    query(person,owned);row=matrix[index];index+=1
                    assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'],row['scene'],row['event'])==(
                        'applied','32',str(person),str(owned),'duel','duel_lucky_roll_suppressed')
                    assert (row['force_id'],row['target_person'],row['target_force'],row['caller'])==(
                        '3',str(person),'3',str(0x50aadf if mode==0 else 0x50ab61))
                    assert all(row[key]=='-1' for key in ('actor_unit','target_unit','target_city','target_base','equipment_id','target_x','target_y'))
                    assert not any(row[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
                    assert json.loads(row['context_json'])==dict(duel_action=5 if mode==0 else 4 if mode==1 else 7,
                        current_side=side,opponent_person=1000,record_index=2,selector_result=0,
                        result_scope='native_duel_action_result_flag',counterfactual_evaluated=False)
    for owned in (32,249):
        for fault in range(1,11 if owned==249 else 10):query(14162,owned)
    for owned in (32,249):query(600,owned,True)
    assert index==len(matrix)==93
    return dict(lucky_duel_rows=93,lucky_duel_applied_rows=36,lucky_duel_query_rows=57)
