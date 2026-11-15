import json


def verify_diplomacy_contest(rows):
    matrix=rows('diplomacy_contest_test.csv');before=rows('diplomacy_contest_before_test.csv');index=0
    def query(row,owned,person):
        assert (row['stage'],row['event'],row['scene'],row['effect_skill'],row['owned_skill'],row['person_id'])==(
            'check_matched','negotiation_skill_eligibility','diplomacy','86',str(owned),str(person))
    def state(row,kind,owned,person):
        assert (row['stage'],row['event'],row['person_id'],row['owned_skill'],row['effect_skill'])==(
            'applied','negotiation_interaction_state_written',str(person),str(owned),'86')
        assert (row['target_person'],row['target_force'])==('601','249')
        assert json.loads(row['context_json'])==dict(command_type=kind,initial_result_code=1,native_mode=2,native_flag=1,
            result_scope='native_interaction_state_before_ui',target_role='force_ruler',counterfactual_evaluated=False)
    for kind,caller in ((16,0x68ac15),(18,0x68b962),(20,0x68c070)):
        for person in (600,14162):
            for owned in (86,249):
                for result in (0,1,2):
                    for parallel in (False,True):
                        q,s,r=matrix[index:index+3];index+=3;query(q,owned,person);state(s,kind,owned,person)
                        assert (r['stage'],r['scene'],r['event'],r['effect_skill'],r['owned_skill'],r['person_id'])==(
                            'applied','diplomacy','negotiation_contest_dispatched','86',str(owned),str(person))
                        assert (r['force_id'],r['target_person'],r['target_force'],r['caller'])==('2','1000','3',str(caller))
                        assert (r['person_name'],r['ruler_id'],r['actor_city'],r['game_year'],r['game_month'],r['game_day'])==(
                            'Guide' if person==600 else 'Other','635','8','194','1','1')
                        assert all(r[key]=='-1' for key in ('target_city','target_base','target_unit','actor_unit','equipment_id','target_x','target_y','terrain_id'))
                        assert not any(r[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
                        assert json.loads(r['context_json'])==dict(command_type=kind,command_target_force=249,command_target_ruler=601,
                            native_result_code=result,native_resolution='scheduled' if result==1 else 'automatic',packet_type=25,
                            result_scope='native_contest_wrapper_return',counterfactual_evaluated=False)
                        assert q['thread']==s['thread']
                        assert (r['thread']!=s['thread'])==parallel
    for owned in (86,249):
        for fault in range(1,26):
            if fault==17 and owned==86:continue
            q,s=matrix[index:index+2];index+=2;query(q,owned,600);state(s,16,owned,600)
    for kind in (16,18,20):query(matrix[index],86,600);index+=1
    assert index==len(matrix)==317
    index=0
    for kind in (16,18,20):
        for owned in (86,249):
            for result in (0,1,2):
                q,s=before[index:index+2];index+=2;query(q,owned,600);state(s,kind,owned,600)
    assert index==len(before)==36
    return dict(diplomacy_contest_rows=317,diplomacy_contest_applied_rows=193,diplomacy_contest_query_rows=124,
        diplomacy_contest_dispatch_rows=72,diplomacy_contest_before_rows=36,diplomacy_contest_before_dispatch_rows=0)
