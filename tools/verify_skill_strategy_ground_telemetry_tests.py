import json


def verify_strategy_ground(rows):
    matrix=rows('strategy_ground_test.csv');index=0
    for effect in (53,57,62,63):
        for provider in range(6):
            people=[(600,effect)] if provider==0 else [(14162,effect)] if provider==1 else (
                [(600,249),(14162,effect)] if provider==2 else [(14162,249)] if provider==3 else
                [(600,effect),(14162,effect)] if provider==4 else [(600,effect),(14162,249)])
            who=people[0] if provider in (0,2,5) else people[-1]
            for kind in range(5):
                for person,owned in people:
                    check_query(matrix[index],effect,person,owned);index+=1
                r=matrix[index];index+=1
                assert (r['stage'],r['effect_skill'],r['person_id'],r['owned_skill'],r['event'])==(
                    'applied',str(effect),str(who[0]),str(who[1]),'strategy_stat_forced_success')
                assert (r['scene'],r['force_id'],r['actor_unit'],r['equipment_id'],r['target_unit'],r['target_force'],r['target_base'],r['target_city'])==(
                    'strategy','2','4095','4','-1','3' if kind else '-1','529' if kind else '-1','529' if kind==1 else '-1')
                assert (r['target_x'],r['target_y'],r['terrain_id'],r['caller'])==('2','1','5',str(0x593402))
                assert json.loads(r['context_json'])==dict(strategy_id=1,attacking_unit=4095,attacking_force=2,
                    stat_id=2,provider_value=120 if provider==5 else 110,opponent_intelligence=0,success_chance=100,
                    native_immunity_flag=0,decision_site={53:0x5aec4f,57:0x5aec5e,62:0x5aec6d,63:0x5aec7c}[effect],
                    counterfactual_evaluated=False,target_kind='building' if kind else 'terrain')
    for effect in (53,57,62,63):
        for fault in range(1,10):check_query(matrix[index],effect,600,effect);index+=1
    for effect in (53,57,62,63):
        for preview in (True,False):check_query(matrix[index],effect,600,effect,preview);index+=1
    assert index==len(matrix)==344
    return dict(strategy_ground_rows=344,strategy_ground_applied_rows=120,strategy_ground_query_rows=224)


def check_query(row,effect,person,owned,preview=False):
    assert (row['stage'],row['effect_skill'],row['person_id'],row['owned_skill'])==('check_matched',str(effect),str(person),str(owned))
    assert row['scene']==('unclassified_query' if preview else 'strategy')
    assert row['event']==('skill_predicate' if preview else 'stat_effect_eligibility')


if __name__=='__main__':
    import csv
    from pathlib import Path
    root=Path(__file__).resolve().parents[1]
    def read(name):
        with (root/'worldmod/build'/name).open(encoding='utf-8-sig',newline='') as stream:return list(csv.DictReader(stream))
    print(json.dumps(verify_strategy_ground(read)))
