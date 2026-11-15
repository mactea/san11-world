import json


def verify_diplomacy_eligibility(rows):
    matrix=rows('diplomacy_eligibility_test.csv');index=0
    for owned in (86,249):
        for fault in range(15):
            if fault in (2,12,13,14):continue
            q=matrix[index];index+=1
            assert (q['stage'],q['effect_skill'],q['person_id'],q['owned_skill'])==(
                'check_matched','86','14162' if fault==4 else '600','86' if fault==4 else str(owned))
            assert q['scene']==('unclassified_query' if fault in (10,11) else 'diplomacy')
            if fault==0:
                r=matrix[index];index+=1
                check_applied(r,owned)
    for owned in (86,249):
        q,r=matrix[index:index+2];index+=2
        assert (q['stage'],q['owned_skill'])==('check_matched',str(owned))
        check_applied(r,owned)
    assert index==len(matrix)==28
    return dict(diplomacy_eligibility_rows=28,diplomacy_eligibility_applied_rows=4,diplomacy_eligibility_query_rows=24)


def check_applied(row,owned):
    assert (row['stage'],row['effect_skill'],row['owned_skill'],row['person_id'],row['scene'],row['event'])==(
        'applied','86',str(owned),'600','diplomacy','negotiation_eligibility_check_bypassed')
    assert (row['force_id'],row['target_force'],row['target_person'],row['caller'])==('2','249','14162',str(0x5b1fdf))
    assert not any(row[key] for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
    assert json.loads(row['context_json'])==dict(command_type=16,gate='0x5bc650',counterfactual_evaluated=False)


if __name__=='__main__':
    import csv
    from pathlib import Path
    root=Path(__file__).resolve().parents[1]
    def read(name):
        with (root/'worldmod/build'/name).open(encoding='utf-8-sig',newline='') as stream:return list(csv.DictReader(stream))
    print(json.dumps(verify_diplomacy_eligibility(read)))
