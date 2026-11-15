import json


def verify_strategy_status(rows):
    positive=rows('strategy_status_test.csv');index=0
    stages=['check_matched','applied','check_matched','applied','applied']
    events=['stat_effect_eligibility','strategy_stat_forced_success','critical_effect_eligibility',
            'strategy_forced_critical_decision','strategy_critical_status_written']
    for plan in (2,3,5):
        for effect in (63,67,58,59,16):
            if effect==16 and plan!=5:continue
            for provider in range(4):
                for mode in (0,1,2):
                    for before in (0,4,5):
                        group=positive[index:index+5];index+=5
                        assert [r['stage'] for r in group]==stages
                        assert [r['event'] for r in group]==events
                        selected=14162 if provider&1 else 600;helper=600 if provider&1 else 14162
                        owned=effect if provider<2 else 249
                        assert [(int(r['person_id']),int(r['effect_skill']),int(r['owned_skill'])) for r in group]==[
                            (helper,57,57),(helper,57,57),(selected,effect,owned),(selected,effect,owned),(selected,effect,owned)]
                        assert all(r['force_id']=='2' and r['scene']=='strategy' for r in group)
                        assert json.loads(group[1]['context_json'])['strategy_id']==plan
                        assert json.loads(group[3]['context_json'])['strategy_id']==plan
                        state=4
                        for _ in range(3 if plan==5 else 2):state=(state*0x6c078965+0x3039)&0xffffffff
                        duration=1+(state>>16)%2 if plan==5 else 3
                        status=2 if plan==2 else 1;r=group[4]
                        assert (r['metric'],int(r['before']),int(r['after']),int(r['delta']))==(
                            'status_turns',before,min(5,before+duration),min(5,before+duration)-before)
                        assert (r['target_unit'],r['target_force'],r['actor_unit'],r['equipment_id'],r['target_x'],r['target_y'],
                                r['target_base'],r['target_city'],r['target_person'])==('4094','3','4095','4','250','230','-1','-1','-1')
                        assert int(r['caller'])=={2:0x591943,3:0x591b93,5:0x592938}[plan]
                        assert not r['baseline_after'] and not r['bonus_delta']
                        assert json.loads(r['context_json'])==dict(strategy_id=plan,critical_result=True,
                            status_before=0,status_after=status,requested_turns=duration,command_mode=mode,
                            result_scope='critical_strategy_status_commit_not_marginal_bonus',counterfactual_evaluated=False)
    assert index==len(positive)==2340
    edges=rows('strategy_status_edges_test.csv');assert len(edges)==32
    assert [r['event'] for r in edges]==events[:4]*8
    assert all(r['effect_skill']==('57' if n%4<2 else '63') and r['person_id']==('14162' if n%4<2 else '600')
               for n,r in enumerate(edges))
    assert all('strategy_critical_status_written'!=r['event'] for r in edges)
    return dict(strategy_status_rows=2372,strategy_status_applied_rows=1420,strategy_status_query_rows=952,
                strategy_status_written_rows=468)
