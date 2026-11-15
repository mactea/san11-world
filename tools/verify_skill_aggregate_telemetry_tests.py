"""Verify component operations without allocating shared loss to each skill."""
import json
import struct


def verify_aggregate(rows):
    data=rows('aggregate_test.csv');i=0;writes=set();applied_count=0
    multiplier=struct.unpack('<f',struct.pack('<f',.008))[0]
    seed=(4*0x6c078965+0x3039)&0xffffffff
    for count in (2,3,6,12):
        for profile in range(4):
            for balance in (0,100,1000):
                components=[n for n in range(count) if profile!=1 or n%2==0]
                query_count=len(components)+(count//2 if profile==2 else count if profile==3 else 0)
                queries=data[i:i+query_count];i+=query_count
                assert len(queries)==query_count and all(r['stage']=='check_matched' for r in queries)
                damages=[0 if profile==2 and n%2 else (125+10*n)//2 if profile==3 else 125+10*n for n in range(count)]
                requested=int((10*count+70+(seed>>16)%(5*count))*multiplier*sum(damages))
                write=None
                for n in reversed(components):
                    roles=['forced_critical']+(['damage_immunity'] if profile==2 and n%2 else ['physical_halving'] if profile==3 else [])
                    for role in roles:
                        r=data[i];i+=1;applied_count+=1;c=json.loads(r['context_json'])
                        if write is None:
                            write=c['aggregate_settlement_id'];assert write not in writes;writes.add(write)
                        assert c==dict(aggregate_settlement_id=write,component_index=n,component_count=count,component_role=role,
                            component_source_unit=4093 if n%2 else 4095,tactic_id=-1,record_kind=0,native_attack_mode=0,
                            calculated_component_damage=damages[n],shared_requested_troop_loss=requested,
                            shared_resource_before=balance,shared_resource_after=max(0,balance-requested),
                            damage_attribution='component_operation_with_shared_commit_context',counterfactual_evaluated=False)
                        effect=23 if role=='forced_critical' else 27 if role=='damage_immunity' else 31
                        person=(601 if n%2 else 600) if effect==23 else 1000
                        assert (r['stage'],r['scene'],r['event'],int(r['effect_skill']),int(r['person_id']))==(
                            'applied','battle','aggregated_attack_component_settled',effect,person)
                        assert r['target_unit']=='4094' and r['target_force']=='3' and int(r['caller'])==0x5b1c6c
                        assert r['actor_unit']==(str(4093 if n%2 else 4095) if effect==23 else '4094')
                        if effect==23:assert (r['metric'],r['before'],r['after'],r['delta'])==('settled_component','0','1','1')
                        else:assert (r['metric'],int(r['before']),int(r['after']))==('calculated_physical_troop_damage',125+10*n,damages[n])
                        assert not r['baseline_after'] and not r['bonus_delta']
    assert i==len(data) and len(writes)==48
    edges=rows('aggregate_edges_test.csv');i=0;edge_applied=0
    for fault in range(14):
        assert all(r['stage']=='check_matched' for r in edges[i:i+2]);i+=2
        persons={0:[601,600],3:[601],4:[601,600],11:[600],12:[600]}.get(fault,[])
        for person in persons:
            r=edges[i];i+=1;edge_applied+=1;c=json.loads(r['context_json'])
            assert (r['event'],r['person_id'],r['metric'],r['delta'])==('aggregated_attack_component_settled',str(person),'settled_component','1')
            assert c['component_index']==(1 if person==601 else 0) and c['component_count']==2
            assert c['shared_requested_troop_loss']==199 and c['shared_resource_before']==1000
            assert c['shared_resource_after']==(0 if fault==4 else 801)
    assert i==len(edges)
    return dict(aggregate_rows=len(data)+len(edges),aggregate_applied_rows=applied_count+edge_applied,
        aggregate_query_rows=len(data)+len(edges)-applied_count-edge_applied,aggregate_groups=48,aggregate_boundary_inputs=14)
