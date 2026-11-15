"""Check kind3 vine effects without inventing skill26 on relationship attacks."""
import json
import struct


def verify_assist_vine(rows):
    matrix=rows('assist_vine_test.csv');index=0
    def chance(q,a,result):
        assert (q['stage'],q['event'],q['effect_skill'],q['person_id'],q['owned_skill'])==('check_matched','assist_eligibility','26','600','26')
        assert (a['stage'],a['event'],a['effect_skill'],a['person_id'])==('applied','assist_skill_chance_used','26','600')
        assert json.loads(a['context_json'])['native_result']==result
    def vine(q,a,provider,damage,mode,troops):
        person=14162 if provider==1 else 1000;owned=249 if provider==2 else 31;half=damage//2
        f=struct.unpack('<f',struct.pack('<f',{0:1.,3:.85,4:.75,5:.65}[mode]))[0];final=int(half*f)
        assert (q['stage'],q['event'],q['effect_skill'],q['person_id'],q['owned_skill'],q['force_id'])==(
            'check_matched','vine_physical_reduction_eligibility','31',str(person),str(owned),'3')
        assert (a['stage'],a['event'],a['effect_skill'],a['person_id'],a['owned_skill'])==(
            'applied','vine_physical_reduction_settled','31',str(person),str(owned))
        assert (a['metric'],int(a['before']),int(a['after']))==('calculated_physical_troop_damage',damage,half)
        assert (a['actor_unit'],a['target_unit'],a['equipment_id'],a['force_id'],a['target_force'],a['caller'])==('4094','4094','4','3','3',str(0x5b1c6c))
        assert json.loads(a['context_json'])==dict(tactic_id=-1,record_kind=3,native_attack_mode=-1,attacking_unit=4095,
            calculation_stage='before_parent_coefficients',settled_packet_damage=final,resource_before=troops,resource_after=max(0,troops-final),
            damage_attribution='native_halving_step_with_whole_commit_context',counterfactual_evaluated=False)
    for branch in range(3):
        for provider in range(4):
            for damage in (1,317):
                for mode in (0,3,4,5):
                    for troops in (100,1000):
                        if branch==0:chance(matrix[index],matrix[index+1],1);index+=2
                        query=matrix[index] if provider!=3 else None
                        if query:index+=1
                        half=damage if provider==3 else damage//2
                        f=struct.unpack('<f',struct.pack('<f',{0:1.,3:.85,4:.75,5:.65}[mode]))[0];final=int(half*f)
                        if branch==0:
                            r=matrix[index];index+=1
                            assert (r['stage'],r['event'],r['effect_skill'],r['person_id'],r['metric'],int(r['before']),int(r['after']))==(
                                'applied','assist_attack_settled','26','600','troops',troops,max(0,troops-final))
                        if query:vine(query,matrix[index],provider,damage,mode,troops);index+=1
    assert index==len(matrix)==480
    failed=rows('assist_vine_failed_test.csv');assert len(failed)==8
    for n in range(4):chance(failed[2*n],failed[2*n+1],0)
    edges=rows('assist_vine_edges_test.csv');index=0
    for fault in (0,1,2,4,6,10,12):
        chance(edges[index],edges[index+1],1);query=edges[index+2];index+=3
        assert (query['event'],query['effect_skill'])==('vine_physical_reduction_eligibility','31')
        if fault in (0,10,12):
            a=edges[index];index+=1;assert (a['event'],a['effect_skill'])==('assist_attack_settled','26')
        if fault in (0,2):vine(query,edges[index],0,317,0,1000);index+=1
    assert index==len(edges)==26
    return dict(assist_vine_rows=514,assist_vine_applied_rows=288,assist_vine_query_rows=226)
