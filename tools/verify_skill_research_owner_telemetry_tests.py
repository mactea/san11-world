"""Verify actual research-owner binding, including a foreign-frame consumer."""
import json


def verify_research_owner(rows):
    matrix=rows('research_owner_edges_test.csv');index=0
    for owned in (84,249):
        for fault in range(18):
            q=matrix[index];index+=1
            assert (q['stage'],q['effect_skill'],q['person_id'],q['owned_skill'],q['scene'],q['event'])==(
                'check_matched','84','600',str(owned),'research','research_gold_cost_eligibility')
            if fault not in (0,17):continue
            r=matrix[index];index+=1;before=950 if fault==17 else 1000
            assert (r['stage'],r['effect_skill'],r['person_id'],r['owned_skill'],r['event'],r['metric'])==(
                'applied','84','600',str(owned),'research_gold_cost_reduced','gold')
            assert tuple(map(int,(r['before'],r['after'],r['baseline_after'],r['bonus_delta'])))==(before,before-50,before-101,51)
            assert (r['target_base'],r['target_city'],r['target_force'],r['caller'])==('8','8','2',str(0x5d8f47))
            assert json.loads(r['context_json'])==dict(base_gold_cost=101,requested_gold_cost=50)
    for owned in (84,249):
        q=matrix[index];index+=1
        assert (q['stage'],q['effect_skill'],q['person_id'],q['owned_skill'])==('check_matched','84','600',str(owned))
    assert index==len(matrix)==42
    return dict(research_owner_rows=42,research_owner_applied_rows=4,research_owner_query_rows=38)
