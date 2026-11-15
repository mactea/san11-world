import json


def verify_fire_immunity(rows):
    matrix=rows('strategy_fire_immunity_test.csv');index=0
    def query(effect,person,owned,preview=False):
        nonlocal index
        row=matrix[index];index+=1
        assert (row['stage'],int(row['effect_skill']),int(row['person_id']),int(row['owned_skill']))==(
            'check_matched',effect,person,owned)
        assert (row['scene'],row['event'])==(('unclassified_query','skill_predicate') if preview else ('strategy','immunity_eligibility'))
    def applied(effect,person,owned):
        nonlocal index
        row=matrix[index];index+=1
        assert (row['stage'],int(row['effect_skill']),int(row['person_id']),int(row['owned_skill']))==(
            'applied',effect,person,owned)
        assert (row['scene'],row['event'],int(row['caller']))==('strategy','strategy_immunity_forced_failure',0x593402)
        assert (row['force_id'],row['actor_city'],row['actor_unit'],row['target_unit'],row['target_force'],row['equipment_id'])==(
            '3','60','4094','4094','3','2')
        assert (row['target_x'],row['target_y'])==('2','1')
        assert all(not row[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
        assert json.loads(row['context_json'])==dict(strategy_id=0,attacking_unit=4095,attacking_force=2,
            success_chance=0,native_immunity_flag=1,counterfactual_evaluated=False)
    for person,owned in ((1000,62),(14162,62),(1000,249),(14162,249)):
        for enemy in (False,True):query(62,person,owned);applied(62,person,owned)
    for owned in (62,249):query(62,1000,owned);applied(62,1000,owned)
    for person,owned in ((1000,62),(14162,249)):
        for chance_only in (False,True):query(62,person,owned,True)
    for invalid in range(6):query(62,1000,62)
    query(61,14162,61);applied(61,14162,61)
    assert index==len(matrix)==32
    return dict(fire_immunity_rows=32,fire_immunity_applied_rows=11,fire_immunity_query_rows=21,
        fire_immunity_new_applied_rows=10)
