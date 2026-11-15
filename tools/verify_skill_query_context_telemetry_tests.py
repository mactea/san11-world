def verify_query_context(rows):
    matrix=rows('query_context_test.csv');index=0
    def query(effect,person,owned,scene,event):
        nonlocal index
        row=matrix[index];index+=1
        assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'],row['scene'],row['event'])==(
            'check_matched',str(effect),str(person),str(owned),scene,event)
        assert (row['force_id'],row['ruler_id'],row['actor_city'],row['game_year'],row['game_month'],row['game_day'])==('2','635','8','194','1','1')
        assert all(row[key]=='-1' for key in ('target_person','target_unit','target_base','target_city','target_force','equipment_id','target_x','target_y','terrain_id'))
        assert not row['context_json']
    for first in (-1,77,249):
        for second in (-1,77,249):
            for a in (0,50,100):
                for b in (0,50,100):
                    if first>=0:query(77,600,first,'officer_selection','construction_worker_priority_checked')
                    if second>=0:query(77,14162,second,'officer_selection','construction_worker_priority_checked')
    for first in (-1,99,249):
        for second in (-1,99,249):
            if first>=0:query(99,600,first,'marriage','spousal_dialogue_eligibility')
            elif second>=0:query(99,14162,second,'marriage','spousal_dialogue_eligibility')
    for effect in (77,99):
        for owned in (effect,249):query(effect,600,owned,'unclassified_query','skill_predicate')
    query(77,600,77,'officer_selection','construction_worker_priority_checked')
    query(99,600,249,'marriage','spousal_dialogue_eligibility')
    assert index==len(matrix)==122
    return dict(query_context_rows=122,query_context_applied_rows=0,query_context_query_rows=122)
