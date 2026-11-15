import json


def verify_lucky_mission(rows):
    matrix=rows('lucky_mission_test.csv');before=rows('lucky_mission_before_test.csv');index=0
    def query(person,owned,preview=False):
        nonlocal index
        row=matrix[index];index+=1
        assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'])==('check_matched','32',str(person),str(owned))
        assert (row['scene'],row['event'])==(('unclassified_query','skill_predicate') if preview else ('mission','lucky_result_eligibility'))
    for family,caller in enumerate((0x5b7b77,0x5bffb0,0x5d1555),1):
        for person in (600,14162):
            for owned in (32,249):
                query(person,owned);row=matrix[index];index+=1
                assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'],row['scene'],row['event'])==(
                    'applied','32',str(person),str(owned),'mission','mission_lucky_result_classified')
                assert (row['force_id'],row['target_person'],row['target_force'],row['caller'])==('2',str(person),'2',str(caller))
                assert (row['person_name'],row['ruler_id'],row['actor_city'],row['game_year'],row['game_month'],row['game_day'])==(
                    'Guide' if person==600 else 'Other','635','8','194','1','1')
                assert all(row[key]=='-1' for key in ('actor_unit','target_unit','target_city','target_base','equipment_id','target_x','target_y','terrain_id'))
                assert not any(row[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
                assert json.loads(row['context_json'])==dict(result_family=family,native_result_code=2,
                    result_scope='actual_mission_classifier_dispatch',counterfactual_evaluated=False)
    for family in range(3):
        for owned in (32,249):
            for fault in range(1,7):
                if fault==5 and owned==32:continue
                query(600,owned)
    for family in range(3):
        for owned in (32,249):query(600,owned,True)
    assert index==len(matrix)==63
    assert len(before)==18
    expected=[(person,owned) for family in range(3) for person in (600,14162) for owned in (32,249)]
    expected += [(600,owned) for family in range(3) for owned in (32,249)]
    for row,(person,owned) in zip(before,expected):
        assert (row['stage'],row['scene'],row['event'],row['effect_skill'],row['person_id'],row['owned_skill'])==(
            'check_matched','unclassified_query','skill_predicate','32',str(person),str(owned))
    return dict(lucky_mission_rows=63,lucky_mission_applied_rows=12,lucky_mission_query_rows=51,
                lucky_mission_before_rows=18,lucky_mission_before_applied_rows=0)
