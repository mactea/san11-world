"""Validate native AI weight calculations without counting them as casts."""
import json


def verify_ai_weight(rows):
    effects=(64,63,62,65,66,70,71,67)
    def query(r,effect,person,owned):
        assert (r['stage'],r['scene'],r['event'],int(r['effect_skill']),int(r['person_id']),int(r['owned_skill']))==(
            'check_matched','ai_evaluation','strategy_role_weight_eligibility',effect,person,owned)
    def applied(r,effect,person,owned):
        assert (r['stage'],r['scene'],r['event'],int(r['effect_skill']),int(r['person_id']),int(r['owned_skill']))==(
            'applied','ai_evaluation','strategy_role_weight_selected',effect,person,owned)
        assert (r['metric'],r['before'],r['after'],r['delta'])==('ai_strategy_role_weight','524','1024','500')
        assert (r['actor_unit'],r['target_unit'],r['force_id'],r['target_force'],r['equipment_id'],r['target_x'],r['target_y'])==(
            '4095','4095','2','2','3','1','1')
        assert r['caller']==str(0x5abfc4) and not r['baseline_after'] and not r['bonus_delta']
        assert json.loads(r['context_json'])==dict(native_query_priority=effects.index(effect),calculation_only=True,
            calculation_stage='before_intelligence_and_other_score_terms',result_scope='native_ai_role_weight_not_command_execution',counterfactual_evaluated=False)
    data=rows('ai_weight_test.csv');index=0
    for effect in effects:
        for provider in range(4):
            person=14162 if provider==1 else 600;owned=249 if provider==2 else effect
            query(data[index],effect,person,owned);applied(data[index+1],effect,person,owned);index+=2
    for effect in (64,63,62,65):
        query(data[index],effect,14162,effect);applied(data[index+1],effect,14162,effect);index+=2
    query(data[index],64,600,249);applied(data[index+1],64,600,249);index+=2
    assert index==len(data)==74
    edges=rows('ai_weight_edges_test.csv');assert len(edges)==11
    query(edges[0],65,600,65);applied(edges[1],65,600,65)
    for r in edges[2:]:query(r,65,600,65)
    return dict(ai_weight_rows=85,ai_weight_applied_rows=38,ai_weight_query_rows=47,
        ai_weight_inputs=37,ai_weight_stale_inputs=9)
