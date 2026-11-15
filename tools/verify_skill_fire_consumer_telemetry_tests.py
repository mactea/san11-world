"""Check native fire modifiers against ordered fixtures and actual resource receipts."""
import json


def verify_fire_consumer(rows):
    matrix=rows('fire_consumer_test.csv');index=0
    cases=[(base,profile,balance,False) for base in (False,True) for profile in range(8) for balance in (100,10000)]
    cases.extend((False,profile,1000,True) for profile in (2,3))
    for base,profile,balance,neutral in cases:
        damage=0 if base else 317;steps=[]
        if profile in (1,5,6,7):
            steps.append((62,14162 if profile==7 else 600,62,damage,damage*2,0x5b1271,True));damage*=2
        if profile in (2,5,7):
            steps.append((31,1000,249 if profile==7 else 31,damage,damage*2,0x5b1289,False));damage*=2
        elif profile in (3,6):
            steps.append((62,1000,62,damage,0,0x5b12a6,False));damage=0
        for effect,person,owned,before,after,modifier,offensive in steps:
            q=matrix[index];index+=1
            assert (q['stage'],q['scene'],q['event'])==('check_matched','battle','fire_modifier_eligibility')
            assert (q['effect_skill'],q['person_id'],q['owned_skill'],q['force_id'])==tuple(map(str,(effect,person,owned,2 if offensive else 3)))
        # The original building branch applies these modifiers to its unused
        # troop field, so its actual HP write must not produce modifier events.
        if base:
            continue
        for effect,person,owned,before,after,modifier,offensive in steps:
            r=matrix[index];index+=1
            assert (r['stage'],r['scene'],r['event'],r['caller'])==('applied','battle','fire_damage_modifier_settled',str(0x592538))
            assert (r['effect_skill'],r['person_id'],r['owned_skill'])==tuple(map(str,(effect,person,owned)))
            assert (r['metric'],int(r['before']),int(r['after']))==('calculated_fire_troop_damage',before,after)
            assert (r['force_id'],r['target_force'],r['target_unit'],r['target_base'])==('2' if offensive else '3','3','4094','-1')
            assert (r['actor_unit'],r['equipment_id'])==(('4095','4') if offensive else ('4094','2'))
            assert (r['target_x'],r['target_y'])==('2','1')
            assert json.loads(r['context_json'])==dict(fire_consumer=True,native_modifier_caller=modifier,
                provider_role='attacker' if offensive else 'defender',source_unit=-1 if neutral else 4095,
                source_base=-1,source_force=-1 if neutral else 2,actor_base=-1,component_final_damage=damage,
                requested_total_loss=damage,resource_before=balance,resource_after=max(0,balance-damage),
                damage_attribution='native_calculation_step_with_whole_commit_context',counterfactual_evaluated=False)
    assert index==len(matrix)==58
    edges=rows('fire_consumer_edges_test.csv')
    assert [r['stage'] for r in edges]==['check_matched','applied']+['check_matched']*10+['applied']
    assert edges[-2]['event']=='skill_predicate'
    assert all((r['effect_skill'],r['person_id'],r['owned_skill'])==('31','1000','31') for r in edges)
    for r in (edges[1],edges[-1]):
        assert (r['event'],r['before'],r['after'])==('fire_damage_modifier_settled','317','634')
        context=json.loads(r['context_json'])
        assert (context['resource_before'],context['resource_after'])==(1000,366)
    return dict(fire_consumer_rows=71,fire_consumer_applied_rows=22,fire_consumer_query_rows=49)
