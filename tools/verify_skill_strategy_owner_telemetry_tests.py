import json


def verify_strategy_owner(rows):
    matrix=rows('strategy_owner_test.csv');index=0
    def query(provider,preview=False):
        nonlocal index
        row=matrix[index];index+=1
        person=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3) else 64
        assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'])==('check_matched','64',str(person),str(owned))
        assert (row['scene'],row['event'])==(('unclassified_query','skill_predicate') if preview else ('strategy','energy_cost_eligibility'))
    def applied(provider,cap,before,after):
        nonlocal index
        row=matrix[index];index+=1
        person=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3) else 64
        baseline=min(after,max(0,before-10));assert baseline==min(cap,max(0,before-10))
        assert (row['stage'],row['scene'],row['event'],row['effect_skill'],row['person_id'],row['owned_skill'])==(
            'applied','strategy','strategy_energy_cost_override','64',str(person),str(owned))
        assert (row['metric'],row['before'],row['after'],row['delta'],row['baseline_after'],row['bonus_delta'])==(
            'energy',str(before),str(after),str(after-before),str(baseline),str(after-baseline))
        assert (row['force_id'],row['target_force'],row['actor_unit'],row['target_unit'],row['equipment_id'])==('2','2','4095','4095','4')
        assert (row['target_x'],row['target_y'],row['caller'])==('1','1',str(0x593445))
        assert all(row[key]=='-1' for key in ('target_person','target_base','target_city','terrain_id'))
        assert json.loads(row['context_json'])==dict(strategy_id=0,normal_energy_cost=10,requested_energy_cost=1,counterfactual_evaluated=True)
    for provider in range(4):
        for advanced in (0,1):
            for before in (10,120,255):
                cap=120 if advanced else 100;query(provider);applied(provider,cap,before,min(cap,before-1))
    for provider in range(4):query(provider);applied(provider,100,9,8)
    for owned in (64,249):
        for stage in (0,1):
            for fault in range(1,16):
                if fault==15 and owned==64:continue
                query(0 if owned==64 else 2)
    for owned in (64,249):query(0 if owned==64 else 2)
    for provider in range(4):query(provider,True)
    assert index==len(matrix)==120
    combined=rows('strategy_reach_owner_test.csv');co_index=0
    for provider in range(4):
        for tech in (0,1):
            for success in (0,1):
                range_person=14162 if provider==0 else 600;cost_person=14162 if provider==1 else 600
                range_owned=cost_owned=249 if provider>=2 else None
                if range_owned is None:range_owned,cost_owned=65,64
                q65,q64,cost,reach=combined[co_index:co_index+4];co_index+=4
                assert (q65['stage'],q65['effect_skill'],q65['person_id'],q65['owned_skill'],q65['event'])==(
                    'check_matched','65',str(range_person),str(range_owned),'extended_range_permission_checked')
                assert (q64['stage'],q64['effect_skill'],q64['person_id'],q64['owned_skill'],q64['event'])==(
                    'check_matched','64',str(cost_person),str(cost_owned),'energy_cost_eligibility')
                assert (cost['stage'],cost['event'],cost['effect_skill'],cost['person_id'],cost['owned_skill'])==(
                    'applied','strategy_energy_cost_override','64',str(cost_person),str(cost_owned))
                assert (cost['before'],cost['after'],cost['delta'],cost['baseline_after'],cost['bonus_delta'])==('9','8','-1','0','8')
                assert (cost['force_id'],cost['target_force'],cost['target_unit'],cost['equipment_id'],cost['caller'])==('2','2','4095','3',str(0x593445))
                assert json.loads(cost['context_json'])==dict(strategy_id=0,normal_energy_cost=10,requested_energy_cost=1,counterfactual_evaluated=True)
                assert (reach['stage'],reach['event'],reach['effect_skill'],reach['person_id'],reach['owned_skill'])==(
                    'applied','extended_strategy_command_resolved','65',str(range_person),str(range_owned))
                assert (reach['force_id'],reach['target_force'],reach['target_unit'],reach['equipment_id'],reach['caller'])==('2','3','4094','3',str(0x5933a0))
                assert (reach['target_x'],reach['target_y'],reach['terrain_id'])==('1','5' if tech else '3','0')
                assert json.loads(reach['context_json'])==dict(strategy_id=0,distance=4 if tech else 2,
                    normal_range_max=3 if tech else 1,skill_range_max=4 if tech else 2,native_permission_mask=1,
                    energy_before=9,energy_after=8,requested_energy_delta=-1,native_command_return=success,
                    result_scope='paid_extended_range_command_not_target_outcome',counterfactual_evaluated=False)
    assert co_index==len(combined)==64
    return dict(strategy_owner_rows=120,strategy_owner_applied_rows=28,strategy_owner_query_rows=92,
        strategy_owner_stale_or_preview_applied_rows_removed=53,strategy_owner_foreign_frame_attributions_corrected=4,
        strategy_reach_owner_rows=64,strategy_reach_owner_applied_rows=32,strategy_reach_owner_query_rows=32)
