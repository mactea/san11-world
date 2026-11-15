"""Independent expected trap formulas and ordered native-fixture CSV receipts."""
import json


def verify_traps(rows):
    matrix=rows('trap_test.csv');index=0
    for root in range(3):
        for base in (False,True):
            for kind in (16,23):
                for profile in range(9):
                    if base and profile==8:
                        continue
                    for balance in (100,10000):
                        source=profile in (1,5,6,7)
                        vine=profile in (2,5,7);fire=profile in (3,6)
                        cliff=profile in (4,5,6,7,8)
                        source_person=14162 if profile==7 else 600
                        cliff_person=601 if profile in (5,6) else 14162 if profile==8 else 1000
                        primary=[];aux=[]
                        def step(items,effect,person,owned,before,after,caller,offensive=False):
                            items.append(dict(effect=effect,person=person,owned=owned,before=before,after=after,modifier=caller,offensive=offensive))
                            return after
                        damage=(167 if base else 367) if kind==16 else (867 if base else 1567)
                        if kind==16 and source:
                            damage=step(primary,62,source_person,62,damage,damage*2,0x5b168c,True)
                        if vine:
                            damage=step(primary,31,1000,249 if profile==7 else 31,damage,damage*2 if kind==16 else damage//2,0x5b16a1 if kind==16 else 0x5b17a7)
                        elif fire and kind==16:
                            damage=step(primary,62,1000,62,damage,0,0x5b1791)
                        if cliff:
                            damage=step(primary,6,cliff_person,249 if profile==7 else 6,damage,damage//10,0x5b17c6)
                        extra=0 if base else 317
                        if root:
                            if source:
                                extra=step(aux,62,source_person,62,extra,extra*2,0x5b1271,True)
                            if vine:
                                extra=step(aux,31,1000,249 if profile==7 else 31,extra,extra*2,0x5b1289)
                            elif fire:
                                extra=step(aux,62,1000,62,extra,0,0x5b12a6)
                        for expected in primary+aux:
                            q=matrix[index];index+=1
                            assert (q['stage'],q['scene'],q['event'])==('check_matched','battle','trap_modifier_eligibility')
                            assert (q['effect_skill'],q['person_id'],q['owned_skill'],q['force_id'])==tuple(map(str,(expected['effect'],expected['person'],expected['owned'],2 if expected['offensive'] else 3)))
                        for auxiliary,items in ((False,primary),(True,aux)):
                            for expected in items:
                                r=matrix[index];index+=1
                                hp=base and not auxiliary
                                before=1000 if hp else balance
                                total=damage+(117 if root else 0) if hp else 0 if base else damage+(extra if root else 0)
                                context=dict(trap_kind=-1 if auxiliary else kind,auxiliary_fire=auxiliary,native_modifier_caller=expected['modifier'],
                                             provider_role='attacker' if expected['offensive'] else 'defender',source_unit=4095,source_base=-1,source_force=2,
                                             actor_base=8 if base and not expected['offensive'] else -1,component_final_damage=extra if auxiliary else damage,
                                             requested_total_loss=total,resource_before=before,resource_after=max(0,before-total),
                                             damage_attribution='native_calculation_step_with_whole_commit_context',counterfactual_evaluated=False)
                                caller=(0x5b1aaf if hp else 0x5b1a74 if base else 0x5b1c6c) if root==0 else 0x597de5 if hp else 0x597e02 if base else 0x59787a
                                assert (r['stage'],r['scene'],r['event'],r['caller'])==('applied','battle','trap_damage_modifier_settled',str(caller))
                                assert (r['effect_skill'],r['person_id'],r['owned_skill'])==tuple(map(str,(expected['effect'],expected['person'],expected['owned'])))
                                assert (r['metric'],int(r['before']),int(r['after']))==('calculated_trap_hp_damage' if hp else 'calculated_trap_troop_damage',expected['before'],expected['after'])
                                assert (r['force_id'],r['target_force'],r['target_unit'],r['target_base'],r['target_city'])==('2' if expected['offensive'] else '3','3','-1' if base else '4094','8' if base else '-1','8' if base else '-1')
                                assert (r['actor_unit'],r['equipment_id'])==('4095','4') if expected['offensive'] else (r['actor_unit'],r['equipment_id'])==(('-1','-1') if base else ('4094','2'))
                                assert (r['target_x'],r['target_y'])==('2','1')
                                assert json.loads(r['context_json'])==context,(root,base,kind,profile,balance,expected,r['context_json'],context)
    assert index==len(matrix)==792
    edges=rows('trap_edges_test.csv');assert len(edges)==18
    assert [r['stage'] for r in edges]==['check_matched','applied']+['check_matched']*11+['check_matched','check_matched','applied','check_matched','applied']
    assert [r['event'] for r in edges[13:]]==['trap_modifier_eligibility','skill_predicate','trap_damage_modifier_settled','trap_modifier_eligibility','trap_damage_modifier_settled']
    assert all((r['effect_skill'],r['person_id'],r['owned_skill'])==('6','1000','6') for r in edges)
    preview=rows('trap_preview_test.csv');assert len(preview)==5
    assert all(r['stage']=='check_matched' and r['event']=='skill_predicate' for r in preview)
    return dict(trap_rows=815,trap_applied_rows=399,trap_query_rows=416)
