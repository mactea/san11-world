"""Check native vine halving separately from later scaling and actual losses."""
import json
import struct
import csv
from pathlib import Path


def verify_vine(rows):
    data=rows('vine_test.csv');index=0
    def actor(provider):return (1000,31) if provider==0 else (601,31) if provider==1 else (1000,249) if provider==2 else (14162,31)
    def query(row,provider):
        person,owned=actor(provider)
        assert (row['stage'],row['scene'],row['event'],row['effect_skill'],row['person_id'],row['owned_skill'],row['force_id'])==(
            'check_matched','battle','vine_physical_reduction_eligibility','31',str(person),str(owned),'3')
    def applied(row,provider,damage,coefficient,tactic,queued,balance):
        person,owned=actor(provider);halved=damage//2
        factor={0:1.,3:.85,4:.75,5:.65}[coefficient];factor=struct.unpack('<f',struct.pack('<f',factor))[0]
        final=int(halved*factor)
        assert (row['stage'],row['scene'],row['event'],row['effect_skill'],row['person_id'],row['owned_skill'])==(
            'applied','battle','vine_physical_reduction_settled','31',str(person),str(owned))
        assert (row['metric'],int(row['before']),int(row['after']))==('calculated_physical_troop_damage',damage,halved)
        assert (row['actor_unit'],row['target_unit'],row['force_id'],row['target_force'],row['equipment_id'],row['target_x'],row['target_y'])==('4094','4094','3','3','2','2','1')
        assert row['caller']==str(0x5b1c6c if queued else 0x584fdf)
        assert json.loads(row['context_json'])==dict(tactic_id=tactic,record_kind=0,native_attack_mode=0,attacking_unit=4095,
            calculation_stage='before_parent_coefficients',settled_packet_damage=final,resource_before=balance,
            resource_after=max(0,balance-final),damage_attribution='native_halving_step_with_whole_commit_context',counterfactual_evaluated=False)
    for tactic in (-1,12):
        for provider in range(4):
            for damage in (1,317):
                for mode in (0,3,4,5):
                    for queued in (0,1):
                        for balance in (100,1000):
                            query(data[index],provider);applied(data[index+1],provider,damage,mode,tactic,queued,balance);index+=2
    assert len(data)==index==512
    edges=rows('vine_edges_test.csv');assert len(edges)==17
    query(edges[0],0);applied(edges[1],0,317,0,-1,0,1000)
    for row in edges[2:-1]:query(row,0)
    assert (edges[-1]['stage'],edges[-1]['event'])==('check_matched','skill_predicate')
    copies=rows('vine_copy_test.csv');assert len(copies)==8
    for provider in range(4):
        query(copies[provider*2],provider);applied(copies[provider*2+1],provider,317,0,-1,1,1000)
    path=Path(__file__).resolve().parents[1]/'worldmod/build/vine_absent_test.csv'
    assert path.read_bytes().startswith(b'\xef\xbb\xbf')
    with path.open(encoding='utf-8-sig',newline='') as stream:
        reader=csv.DictReader(stream);assert list(reader)==[] and len(reader.fieldnames)==36
    return dict(vine_rows=537,vine_applied_rows=261,vine_query_rows=276,vine_absent_inputs=144,vine_absent_rows=0)
