"""Check native assist immunity decisions separately from query and zero loss."""
import json


def verify_assist_defense(rows):
    data=rows('assist_defense_test.csv');index=0
    def source_query(r):
        assert (r['stage'],r['event'],r['effect_skill'],r['person_id'])==('check_matched','assist_eligibility','26','600')
    def chance(r,result):
        assert (r['stage'],r['event'],r['effect_skill'],r['person_id'])==('applied','assist_skill_chance_used','26','600')
        assert json.loads(r['context_json'])==dict(primary_unit=4093,primary_force_at_decision=2,native_chance_percent=30,native_result=result,counterfactual_evaluated=False)
    def actor(mask,provider,effect):
        person=1000 if provider==0 or provider==2 else 14162
        if mask==3 and effect==28 and provider!=2:person=14162 if provider==0 else 1000
        return str(person),'249' if provider==2 else str(effect)
    def query(r,mask,provider,effect):
        person,owned=actor(mask,provider,effect)
        assert (r['stage'],r['scene'],r['event'],r['effect_skill'],r['person_id'],r['owned_skill'])==(
            'check_matched','battle','damage_immunity_eligibility',str(effect),person,owned)
    def defense(r,mask,provider,effect,troops,damage):
        person,owned=actor(mask,provider,effect)
        assert (r['stage'],r['event'],r['effect_skill'],r['person_id'],r['owned_skill'])==(
            'applied','damage_immunity_assist_attack_settled',str(effect),person,owned)
        assert (r['metric'],int(r['before']),int(r['after']),r['delta'])==('troops',troops,troops,'0')
        assert (r['actor_unit'],r['target_unit'],r['force_id'],r['target_force'],r['equipment_id'])==('4094','4094','3','3','4')
        assert r['caller']==str(0x5b1c6c) and not r['baseline_after'] and not r['bonus_delta']
        assert json.loads(r['context_json'])==dict(attacking_unit=4095,tactic_id=-1,record_kind=3,native_attack_mode=-1,
            calculated_troop_damage_before_immunity=damage,settled_packet_troop_damage=0,native_chance_percent=50,counterfactual_evaluated=False)
    def settled(r,troops,damage):
        assert (r['stage'],r['event'],r['person_id'],r['effect_skill'],r['metric'])==('applied','assist_attack_settled','600','26','troops')
        assert (int(r['before']),int(r['after']),int(r['delta']))==(troops,troops-damage,-damage)
        assert (r['actor_unit'],r['target_unit'],r['force_id'],r['target_force'])==('4095','4094','2','3')
        assert json.loads(r['context_json'])==dict(primary_unit=4093,primary_force_at_decision=2,record_kind=3,native_chance_percent=30,
            requested_troop_loss=damage,damage_attribution='whole_assist_attack_not_marginal_skill_bonus',counterfactual_evaluated=False)
    for branch in range(3):
        for mask in (1,2,3):
            for provider in range(4):
                for troops in (2999,3000):
                    for damage in (499,500):
                        for seed in (0,5):
                            state=seed
                            def draw(percent):
                                nonlocal state
                                state=(state*0x6c078965+0x3039)&0xffffffff
                                return (state>>16)%100<percent
                            assert draw(30 if branch==0 else 50 if branch==1 else 20)
                            if branch==0:source_query(data[index]);chance(data[index+1],1);index+=2
                            effect=-1
                            if provider!=3 and troops<3000 and mask&1:
                                query(data[index],mask,provider,27);index+=1
                                if draw(50):effect=27
                            if effect<0 and provider!=3 and damage<500 and mask&2:
                                query(data[index],mask,provider,28);index+=1
                                if draw(50):effect=28
                            if effect>=0:defense(data[index],mask,provider,effect,troops,damage);index+=1
                            if branch==0:settled(data[index],troops,0 if effect>=0 else damage);index+=1
    assert index==len(data)
    edges=rows('assist_defense_edges_test.csv');index=0
    for fault in (0,1,2,4,6,8,10):
        source_query(edges[index]);chance(edges[index+1],1);query(edges[index+2],1,0,27);index+=3
        if fault in (0,2,8):defense(edges[index],1,0,27,2999,499);index+=1
        if fault in (0,10):settled(edges[index],2999,0);index+=1
    query(edges[index],1,0,27);defense(edges[index+1],1,0,27,2999,499);index+=2
    for branch in (0,1):
        if branch==0:source_query(edges[index]);chance(edges[index+1],1);index+=2
        query(edges[index],1,2,27);index+=1
        r=edges[index];index+=1
        assert (r['stage'],r['event'],r['effect_skill'],r['owned_skill'],r['person_id'])==('check_matched','vine_physical_reduction_eligibility','31','249','1000')
        defense(edges[index],1,2,27,2999,499);index+=1
        if branch==0:settled(edges[index],2999,0);index+=1
        r=edges[index];index+=1
        assert (r['event'],r['effect_skill'],r['person_id'],r['metric'],r['before'],r['after'])==(
            'vine_physical_reduction_settled','31','1000','calculated_physical_troop_damage','0','0')
        assert json.loads(r['context_json'])==dict(tactic_id=-1,record_kind=3,native_attack_mode=-1,attacking_unit=4095,
            calculation_stage='before_parent_coefficients',settled_packet_damage=0,resource_before=2999,resource_after=2999,
            damage_attribution='native_halving_step_with_whole_commit_context',counterfactual_evaluated=False)
    assert all(r['stage']=='check_matched' and r['event']=='skill_predicate' for r in edges[index:]) and len(edges[index:])==2
    failed=rows('assist_defense_failed_test.csv');assert len(failed)==2;source_query(failed[0]);chance(failed[1],0)
    all_rows=data+edges+failed;applied=sum(r['stage']=='applied' for r in all_rows)
    return dict(assist_defense_rows=len(all_rows),assist_defense_applied_rows=applied,assist_defense_query_rows=len(all_rows)-applied,
        assist_defense_inputs=288,assist_defense_failed_inputs=3,assist_defense_boundary_inputs=11)
