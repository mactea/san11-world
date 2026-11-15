def verify_raw_priority(rows):
    matrix=rows('raw_priority_test.csv');index=0
    def query(person,caller):
        nonlocal index
        row=matrix[index];index+=1
        assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'],row['scene'],row['event'])==(
            'check_matched','84',str(person),'84','officer_selection','guidance_worker_priority_checked')
        assert int(row['caller'])==caller
        assert (row['force_id'],row['ruler_id'],row['actor_city'],row['game_year'],row['game_month'],row['game_day'])==('2','635','8','194','1','1')
        assert all(row[key]=='-1' for key in ('target_person','target_unit','target_base','target_city','target_force','equipment_id','target_x','target_y','terrain_id'))
        assert not row['context_json']
    for first in (-1,77,84,249):
        for second in (-1,77,84,249):
            for mode in range(3):
                for a in (0,50,100):
                    for b in (0,50,100):
                        for kind in (0,4):
                            if first==84:query(600,0x5dea48)
                            if second==84:query(14162,0x5dea50)
    query(14162,0x5dea50)
    assert index==len(matrix)==433
    return dict(raw_priority_rows=433,raw_priority_applied_rows=0,raw_priority_query_rows=433)
