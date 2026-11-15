"""Expected counterattack damage after vine, immunity and native parent scaling."""
import json
import struct


def verify_counter_vine(rows):
    matrix=rows('counter_vine_test.csv');index=0
    def query(row,effect,person,owned):
        assert (row['stage'],row['scene'],row['event'],row['effect_skill'],row['person_id'],row['owned_skill'],row['force_id'])==(
            'check_matched','battle','vine_physical_reduction_eligibility' if effect==31 else 'counterattack_immunity_eligibility',str(effect),str(person),str(owned),'3')
    def vine(row,person,owned,flag,before,half,final,troops):
        assert (row['stage'],row['event'],row['effect_skill'],row['person_id'],row['owned_skill'],row['caller'])==(
            'applied','vine_physical_reduction_settled','31',str(person),str(owned),str(0x5b1c6c))
        assert (row['metric'],int(row['before']),int(row['after']))==('calculated_physical_troop_damage',before,half)
        assert (row['force_id'],row['target_force'],row['actor_unit'],row['target_unit'],row['equipment_id'],row['target_x'],row['target_y'])==('3','3','4094','4094','4','2','1')
        assert json.loads(row['context_json'])==dict(tactic_id=-1,record_kind=0,native_attack_mode=flag+1,attacking_unit=4095,
            calculation_stage='before_parent_coefficients',settled_packet_damage=final,resource_before=troops,resource_after=troops-min(troops,final),
            damage_attribution='native_halving_step_with_whole_commit_context',counterfactual_evaluated=False)
    for combo in range(3):
        for provider in (0,1):
            for water in (0,1):
                for flag in (0,1):
                    for route in (0,1):
                        for count in (1,3):
                            for optional in (0,1):
                                if route and (count!=1 or optional):continue
                                for troops in (100,1000):
                                    for damage in (1,125):
                                        person=14162 if provider else 1000;owned=249 if combo else 31;effect=14 if water else 13
                                        if combo:query(matrix[index],effect,person,owned);index+=1
                                        query(matrix[index],31,person,owned);index+=1
                                        if combo==2:
                                            immune=matrix[index];index+=1
                                            assert (immune['stage'],immune['event'],immune['effect_skill'],immune['person_id'],immune['owned_skill'])==(
                                                'applied','counterattack_immunity_attack_settled',str(effect),str(person),'249')
                                            assert (immune['metric'],int(immune['before']),int(immune['after']))==('troops',troops,troops)
                                            ctx=json.loads(immune['context_json']);assert (ctx['native_attack_mode'],ctx['calculated_counter_troop_damage'],ctx['settled_packet_troop_damage'])==(flag+1,damage,0)
                                        before=0 if combo==2 else damage;half=before//2;final=half
                                        if not route and count>1:final=int((100.-count*5.)*half*struct.unpack('<f',struct.pack('<f',.01))[0])
                                        vine(matrix[index],person,owned,flag,before,half,final,troops);index+=1
    assert index==len(matrix)==1440
    edges=rows('counter_vine_edges_test.csv');assert len(edges)==11
    query(edges[0],31,1000,31);vine(edges[1],1000,31,0,125,62,52,1000)
    for r in edges[2:]:query(r,31,1000,31)
    return dict(counter_vine_rows=1451,counter_vine_applied_rows=641,counter_vine_query_rows=810)
