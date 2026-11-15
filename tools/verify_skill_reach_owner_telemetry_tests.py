import json


def verify_reach_owner(rows):
    matrix=rows('reach_owner_test.csv');index=0
    def query(provider):
        nonlocal index
        row=matrix[index];index+=1
        person=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3) else 65
        assert (row['stage'],row['scene'],row['event'],row['effect_skill'],row['person_id'],row['owned_skill'])==(
            'check_matched','strategy','extended_range_permission_checked','65',str(person),str(owned))
    def applied(shape,provider,tech,success):
        nonlocal index
        row=matrix[index];index+=1
        person=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3) else 65
        assert (row['stage'],row['scene'],row['event'],row['effect_skill'],row['person_id'],row['owned_skill'])==(
            'applied','strategy','extended_strategy_command_resolved','65',str(person),str(owned))
        base={1:529,2:530,3:531,4:575}.get(shape,-1)
        assert (row['target_unit'],row['target_base'],row['target_city'],row['target_force'])==(
            '4094' if shape==5 else '-1',str(base),'529' if shape==1 else '-1','-1' if shape==0 else '3')
        assert (row['actor_unit'],row['equipment_id'],row['target_person'],row['force_id'],row['target_x'],row['target_y'],row['caller'])==(
            '4095','3','-1','2','1','5' if tech else '3',str(0x5933a0))
        assert row['terrain_id']=='0'
        assert not any(row[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
        assert json.loads(row['context_json'])==dict(strategy_id=0,distance=4 if tech else 2,
            normal_range_max=3 if tech else 1,skill_range_max=4 if tech else 2,native_permission_mask=1,
            energy_before=100,energy_after=90,requested_energy_delta=-10,native_command_return=success,
            result_scope='paid_extended_range_command_not_target_outcome',counterfactual_evaluated=False)
    for shape in range(6):
        for provider in range(4):
            for tech in (0,1):
                for success in (0,1):query(provider);applied(shape,provider,tech,success)
    for provider in range(4):
        for fault in range(12,16):query(provider)
    for provider in (2,3):query(provider)
    for provider in range(4):
        for outcome in (17,18):query(provider);applied(5,provider,0,1)
    for shape in range(6):
        for tech in (0,1):query(0)
    assert index==len(matrix)==238
    return dict(reach_owner_rows=238,reach_owner_applied_rows=104,reach_owner_query_rows=134,
        reach_owner_ground_positive_inputs=80,reach_owner_unit_positive_inputs=16,
        reach_owner_stale_affiliation_controls=16,reach_owner_post_cost_target_outcome_controls=8)
