import json

EVENT='negotiation_interaction_state_written'


def verify_diplomacy_interaction(rows):
    def state(row,kind,owned,person=600,force=249,target=14162):
        assert (row['stage'],row['event'],row['scene'],row['effect_skill'],row['owned_skill'],row['person_id'])==(
            'applied',EVENT,'diplomacy','86',str(owned),str(person))
        assert (row['force_id'],row['target_force'],row['target_person'],row['caller'])==('2',str(force),str(target),str(0x5bd7bf))
        assert all(row[key]=='-1' for key in ('actor_unit','target_unit','target_city','target_base','equipment_id','target_x','target_y','terrain_id'))
        assert not any(row[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
        assert json.loads(row['context_json'])==dict(command_type=kind,initial_result_code=1,native_mode=2,native_flag=1,
            result_scope='native_interaction_state_before_ui',target_role='force_ruler',counterfactual_evaluated=False)
    def query(row,owned):
        assert (row['stage'],row['event'],row['scene'],row['effect_skill'],row['owned_skill'],row['person_id'])==(
            'check_matched','negotiation_skill_eligibility','diplomacy','86',str(owned),'600')
    old_matrix=[r for r in rows('diplomacy_telemetry_test.csv') if r['event']==EVENT];index=0
    for kind in (16,18,20):
        for owned in (86,249):
            for difficulty in (0,2):
                for player in (0,1):
                    for sample in (0,1):
                        if difficulty==2 and player and not sample:continue
                        for final in (0,1,2):state(old_matrix[index],kind,owned,force=249 if difficulty else 3);index+=1
        state(old_matrix[index],kind,86,14162,249,1000);index+=1
    assert index==len(old_matrix)==129
    owner=[r for r in rows('diplomacy_owner_edges_test.csv') if r['event']==EVENT];index=0
    for kind in (16,18,20):
        for owned in (86,249):
            for fault in range(13):
                if fault==11 and owned==86:continue
                state(owner[index],kind,owned);index+=1
    assert index==len(owner)==75
    matrix=rows('diplomacy_interaction_test.csv');index=0
    for kind in (16,18,20):
        for owned in (86,249):
            for result in (0,1):
                for final in (0,1,2):
                    query(matrix[index],owned);index+=1
                    state(matrix[index],kind,owned);index+=1
                    if result:
                        row=matrix[index];index+=1
                        assert (row['stage'],row['event'],row['effect_skill'],row['owned_skill'],row['person_id'])==(
                            'applied','negotiation_skill_result_resolved','86',str(owned),'600')
                        assert json.loads(row['context_json'])==dict(command_type=kind,initial_result_code=1,final_result_code=final,
                            interactive_resolution=True,target_role='force_ruler',counterfactual_evaluated=False)
    for kind in (16,18,20):
        for owned in (86,249):
            for fault in range(1,10):
                if fault==9 and owned==86:continue
                query(matrix[index],owned);index+=1
    for kind in (16,18,20):
        for owned in (86,249):query(matrix[index],owned);index+=1
    for fault in range(6):query(matrix[index],249);index+=1
    assert index==len(matrix)==153
    before=rows('diplomacy_interaction_before_test.csv');assert len(before)==6
    for row,owned in zip(before,(86,249)*3):query(row,owned)
    return dict(diplomacy_interaction_rows=153,diplomacy_interaction_applied_rows=54,diplomacy_interaction_query_rows=99,
        diplomacy_interaction_state_rows=240,diplomacy_interaction_existing_matrix_state_rows=204,
        diplomacy_interaction_before_rows=6,diplomacy_interaction_before_applied_rows=0)
