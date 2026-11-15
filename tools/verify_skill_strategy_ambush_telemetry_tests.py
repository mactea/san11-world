import json


def verify_strategy_ambush(rows):
    positive=rows('strategy_ambush_test.csv');index=0
    for effect in (16,58,59,63,67):
        for provider in range(4):
            for mode in (0,1,2):
                for balance in (0,100,1000):
                    group=positive[index:index+3];index+=3
                    assert [r['event'] for r in group]==['critical_effect_eligibility','strategy_forced_critical_decision',
                                                        'strategy_critical_ambush_troops_written']
                    assert [r['stage'] for r in group]==['check_matched','applied','applied']
                    actor=14162 if provider&1 else 600;owned=effect if provider<2 else 249
                    assert all((int(r['effect_skill']),int(r['owned_skill']),int(r['person_id']),r['force_id'])==
                               (effect,owned,actor,'2') for r in group)
                    r=group[2]
                    assert (r['scene'],r['metric'],int(r['before']),int(r['after']),int(r['delta']))==(
                        'strategy','troops',balance,max(0,balance-125),-min(balance,125))
                    assert (r['actor_unit'],r['equipment_id'],r['target_unit'],r['target_force'],r['target_x'],r['target_y'],
                            r['target_base'],r['target_city'])==('4095','4','4094','3','2','1','-1','-1')
                    assert int(r['caller'])==0x5b1c6c and not r['baseline_after'] and not r['bonus_delta']
                    assert json.loads(r['context_json'])==dict(strategy_id=5,critical_result=True,record_kind=1,
                        requested_troop_loss=125,command_mode=mode,damage_attribution='whole_ambush_effect_not_marginal_skill_bonus',
                        counterfactual_evaluated=False)
    assert index==len(positive)==540
    edges=rows('strategy_ambush_edges_test.csv');assert len(edges)==18
    assert [r['event'] for r in edges]==['critical_effect_eligibility','strategy_forced_critical_decision']*9
    assert all(r['effect_skill']==r['owned_skill']=='63' and r['person_id']=='600' for r in edges)
    mixed=rows('strategy_ambush_mixed_test.csv');assert len(mixed)==12
    assert [r['event'] for r in mixed]==['critical_effect_eligibility','strategy_forced_critical_decision',
                                       'aggregated_attack_component_settled','strategy_critical_status_written']*3
    writes=[]
    for n,balance in enumerate((0,100,1000)):
        r=mixed[4*n+2];context=json.loads(r['context_json']);writes.append(context.pop('aggregate_settlement_id'))
        assert (r['metric'],r['before'],r['after'],r['delta'],r['effect_skill'],r['person_id'])==('settled_component','0','1','1','63','600')
        assert int(r['caller'])==0x5b1c6c and r['target_unit']=='4094' and r['actor_unit']=='4095'
        assert context==dict(component_index=0,component_count=2,component_role='strategy_critical_ambush',component_source_unit=4095,
            tactic_id=-1,record_kind=1,native_attack_mode=0,calculated_component_damage=125,shared_requested_troop_loss=192,
            shared_resource_before=balance,shared_resource_after=max(0,balance-192),
            damage_attribution='component_operation_with_shared_commit_context',counterfactual_evaluated=False)
    assert len(set(writes))==3
    return dict(strategy_ambush_rows=570,strategy_ambush_applied_rows=378,strategy_ambush_query_rows=192,
                strategy_ambush_troop_written_rows=180,strategy_ambush_aggregate_component_rows=3)
