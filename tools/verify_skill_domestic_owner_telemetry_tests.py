"""Verify native domestic commits and multi-worker receipt consumption."""


def verify_domestic_owner(rows):
    matrix=rows('domestic_owner_edges_test.csv');index=0;image=None
    for effect in range(79,84):
        weapon={79:-1,80:1,81:4,82:5,83:10}[effect]
        for owned in (effect,249):
            for fault in range(11):
                q=matrix[index];index+=1
                assert (q['stage'],q['effect_skill'],q['person_id'],q['owned_skill'],q['event'])==(
                    'check_matched',str(effect),'1500',str(owned),'command_effect_eligibility')
                assert q['scene']==('recruitment' if effect==79 else 'production')
                if fault:continue
                for member in range(2 if effect>=82 else 1):
                    r=matrix[index];index+=1
                    assert (r['stage'],r['effect_skill'],r['person_id'],r['owned_skill'],r['target_base'],r['target_force'],r['equipment_id'])==(
                        'applied',str(effect),'1500',str(owned),'8','2',str(weapon))
                    assert (r['event'],r['metric'],r['before'],r['after'],r['baseline_after'])==(
                        ('recruitment_bonus','troops','1000','2866','2244') if effect==79 else
                        ('equipment_production_bonus',f'weapon_{weapon}','1000','3774','2386') if effect<=81 else
                        ('equipment_production_time_reduced','remaining_turns','0','4','9'))
                    assert r['target_person']==(str(1500+member) if effect>=82 else '-1')
                    offset=0x1c3ba9 if effect==79 else 0x1c6725 if effect<=81 else 0x1c70c8
                    if image is None:image=int(r['caller'])-offset
                    assert int(r['caller'])==image+offset
    for effect in range(79,84):
        for owned in (effect,249):
            q=matrix[index];index+=1
            assert (q['stage'],q['effect_skill'],q['person_id'],q['owned_skill'],q['scene'],q['event'])==(
                'check_matched',str(effect),'1500',str(owned),'unclassified_query','skill_predicate')
    assert index==len(matrix)==134
    return dict(domestic_owner_rows=134,domestic_owner_applied_rows=14,domestic_owner_query_rows=120)
