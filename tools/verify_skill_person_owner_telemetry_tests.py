"""Validate identity expiry without suppressing fresh loyalty recipients."""
import json


def verify_person_owner(rows):
    growth=rows('growth_owner_edges_test.csv');index=0
    for path in range(2):
        for owned in (84,249):
            for fault in range(10):
                if fault==9 and owned==84:continue
                q=growth[index];index+=1
                assert (q['stage'],q['effect_skill'],q['person_id'],q['owned_skill'],q['event'])==(
                    'check_matched','84','600',str(owned),'guidance_eligibility')
                if fault:continue
                r=growth[index];index+=1
                assert (r['stage'],r['person_id'],r['owned_skill'],r['target_person'],r['target_force'])==(
                    'applied','600',str(owned),'1000','2')
                assert (r['event'],r['metric'],r['before'],r['after'],r['baseline_after'])==(
                    ('ability_experience_bonus','ability_experience','0','40','20') if path==0 else
                    ('weapon_experience_bonus','weapon_experience_total','10','50','30'))
                context=json.loads(r['context_json']);assert context['requested_experience']==20 and context['grant_caller']>0
                assert context.get('attribute_id' if path==0 else 'aptitude_id')==0
    assert index==len(growth)==42
    spouse=rows('spouse_owner_edges_test.csv');index=0
    for cache in range(2):
        for self in (False,True):
            person='1000' if self else '601'
            for fault in range(10):
                q=spouse[index];index+=1
                assert (q['stage'],q['effect_skill'],q['person_id'],q['owned_skill'],q['event'])==(
                    'check_matched','99',person,'99','spousal_bonus_eligibility')
                if fault:continue
                r=spouse[index];index+=1
                assert (r['stage'],r['person_id'],r['target_person'],r['event'],r['metric'],r['before'],r['after'],r['baseline_after'])==(
                    'applied',person,'1000','spousal_attribute_refreshed','ability','42','51' if cache else '54','50' if cache else '53')
                assert json.loads(r['context_json'])==dict(attribute_id=0,cache='base' if cache else 'current',native_bonus=1)
                assert r['caller']==str(0x48a2fd if cache else 0x48a2ee)
    assert index==len(spouse)==44
    loyalty=rows('loyalty_owner_edges_test.csv');index=0
    for owned in (96,249):
        for fault in range(12):
            if fault==9 and owned==96:continue
            q=loyalty[index];index+=1
            assert (q['stage'],q['effect_skill'],q['person_id'],q['owned_skill'],q['event'])==(
                'check_matched','96','600',str(owned),'loyalty_aura_eligibility')
            # Recipients are observed at each real skip. Their current records
            # need not equal their state at the earlier city-provider lookup.
            if fault not in (0,7,8):continue
            r=loyalty[index];index+=1
            assert (r['stage'],r['person_id'],r['owned_skill'],r['target_person'],r['target_base'],r['event'])==(
                'applied','600',str(owned),'1000','8','loyalty_decay_check_skipped')
            assert json.loads(r['context_json'])==dict(loyalty_before=90,loyalty_after=90,decay_check_skipped=True)
    assert index==len(loyalty)==29
    return dict(person_owner_rows=115,person_owner_applied_rows=14,person_owner_query_rows=101)
