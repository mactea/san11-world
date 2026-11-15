"""Validate CSV output from build_skills.bat and build_skill_telemetry.bat."""
import csv
import json
import struct
from verify_skill_trap_telemetry_tests import verify_traps
from verify_skill_vine_telemetry_tests import verify_vine
from verify_skill_counter_vine_telemetry_tests import verify_counter_vine
from verify_skill_assist_vine_telemetry_tests import verify_assist_vine
from verify_skill_aggregate_telemetry_tests import verify_aggregate
from verify_skill_assist_defense_telemetry_tests import verify_assist_defense
from verify_skill_ai_weight_telemetry_tests import verify_ai_weight
from verify_skill_strategy_status_telemetry_tests import verify_strategy_status
from verify_skill_strategy_ambush_telemetry_tests import verify_strategy_ambush
from verify_skill_merchant_telemetry_tests import verify_merchant
from verify_skill_fire_consumer_telemetry_tests import verify_fire_consumer
from verify_skill_person_owner_telemetry_tests import verify_person_owner
from verify_skill_research_owner_telemetry_tests import verify_research_owner
from verify_skill_domestic_owner_telemetry_tests import verify_domestic_owner
from verify_skill_diplomacy_eligibility_telemetry_tests import verify_diplomacy_eligibility
from verify_skill_strategy_ground_telemetry_tests import verify_strategy_ground
from verify_skill_lucky_unit_event_telemetry_tests import verify_lucky_unit_event
from verify_skill_lucky_duel_telemetry_tests import verify_lucky_duel
from verify_skill_lucky_duel_post_telemetry_tests import verify_lucky_duel_post
from verify_skill_lucky_duel_response_telemetry_tests import verify_lucky_duel_response
from verify_skill_lucky_mission_telemetry_tests import verify_lucky_mission
from verify_skill_diplomacy_interaction_telemetry_tests import verify_diplomacy_interaction
from verify_skill_diplomacy_contest_telemetry_tests import verify_diplomacy_contest
from verify_skill_reach_owner_telemetry_tests import verify_reach_owner
from verify_skill_strategy_owner_telemetry_tests import verify_strategy_owner
from verify_skill_query_context_telemetry_tests import verify_query_context
from verify_skill_fire_immunity_telemetry_tests import verify_fire_immunity
from verify_skill_area_friendly_telemetry_tests import verify_area_friendly
from verify_skill_raw_priority_telemetry_tests import verify_raw_priority
from verify_skill_tribal_affinity_telemetry_tests import verify_tribal_affinity
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def rows(name):
    path = ROOT / 'worldmod/build' / name
    assert path.read_bytes().startswith(b'\xef\xbb\xbf'), path
    with path.open(encoding='utf-8-sig', newline='') as stream:
        result = list(csv.DictReader(stream))
    assert result and all(len(row) == 36 and None not in row for row in result), path
    assert [int(row['seq']) for row in result] == list(range(1, len(result) + 1)), path
    for row in result:
        if row['stage'] == 'check_matched':
            assert all(row[key] == '' for key in ('metric', 'before', 'after', 'delta', 'baseline_after', 'bonus_delta'))
        elif row['metric']:
            assert row['stage'] == 'applied'
            assert int(row['after']) - int(row['before']) == int(row['delta'])
            if row['baseline_after']:
                assert int(row['after']) - int(row['baseline_after']) == int(row['bonus_delta'])
        else:
            assert row['stage'] == 'applied'
            assert (row['scene'], row['event']) in {
                ('movement', 'cliff_immunity'), ('movement', 'poison_immunity'), ('movement', 'terrain_fire_immunity'),
                ('movement', 'steering_water_cell_committed'),
                ('movement', 'steering_water_cell_entered_base'),
                ('movement', 'zoc_bypass_route_cell_committed'),
                ('movement', 'zoc_bypass_route_cell_entered_base'),
                ('unrest', 'bandit_roll_suppressed'), ('unrest', 'tribal_camp_replaced_by_bandit_camp'),
                ('loyalty', 'loyalty_decay_check_skipped'), ('exploration', 'person_discovered'),
                ('environment', 'disaster_new_candidate_excluded'), ('environment', 'disaster_activation_skipped'),
                ('environment', 'disaster_spread_candidate_excluded'), ('environment', 'harvest_scheduled_from_priority_pool'),
                ('diplomacy', 'negotiation_skill_result_resolved'),
                ('diplomacy', 'negotiation_eligibility_check_bypassed'),
                ('unit_event', 'unit_event_casualty_candidate_excluded'),
                ('duel', 'duel_lucky_roll_suppressed'),
                ('duel', 'duel_lucky_postcheck_blocked'),
                ('duel', 'duel_lucky_response_cached'),
                ('mission', 'mission_lucky_result_classified'),
                ('diplomacy', 'negotiation_interaction_state_written'),
                ('diplomacy', 'negotiation_contest_dispatched'),
                ('officer_recruitment', 'remote_recruitment_order_assigned'), ('officer_recruitment', 'remote_recruitment_attempt_resolved'),
                ('battle', 'injury_selection_blocked'), ('battle', 'injury_candidate_excluded'),
                ('battle', 'assist_skill_chance_used'),
                ('battle', 'double_attack_resolved'),
                ('battle', 'capture_guarantee_roll_resolved'),
                ('battle', 'capture_prisoner_after_defeat_resolved'),
                ('battle', 'bloodroad_capture_attempt_blocked'),
                ('battle', 'bloodroad_member_escape_resolved'),
                ('battle', 'plunder_roll_resolved'), ('battle', 'plunder_item_transferred'),
                ('battle', 'ironwall_capture_multiplier_reduced'),
                ('battle', 'lucky_capture_attempt_skipped'),
                ('battle', 'casualty_selection_blocked'), ('battle', 'casualty_candidate_excluded'),
                ('battle', 'same_force_area_target_excluded'),
                ('turn_update', 'fire_casualty_processing_skipped'),
                ('strategy', 'strategy_immunity_forced_failure'),
                ('strategy', 'strategy_stat_forced_success'), ('strategy', 'strategy_stat_forced_failure'),
                ('strategy', 'strategy_forced_critical_decision'),
                ('strategy', 'strategy_chain_subcommand_resolved'), ('strategy', 'strategy_reflected_subcommand_resolved'),
                ('strategy', 'magic_command_resolved'), ('strategy', 'magic_target_decision'),
                ('strategy', 'magic_target_immunity_forced_failure'), ('strategy', 'magic_target_forced_success'),
                ('strategy', 'extended_strategy_command_resolved')}
            assert all(row[key] == '' for key in ('before', 'after', 'delta', 'baseline_after', 'bonus_delta'))
        if row['context_json']:
            assert isinstance(json.loads(row['context_json']), dict)
    return result


def main():
    trap_counts=verify_traps(rows)
    vine_counts=verify_vine(rows)
    counter_vine_counts=verify_counter_vine(rows)
    assist_vine_counts=verify_assist_vine(rows)
    aggregate_counts=verify_aggregate(rows)
    assist_defense_counts=verify_assist_defense(rows)
    ai_weight_counts=verify_ai_weight(rows)
    strategy_status_counts=verify_strategy_status(rows)
    strategy_ambush_counts=verify_strategy_ambush(rows)
    merchant_counts=verify_merchant(rows)
    fire_consumer_counts=verify_fire_consumer(rows)
    person_owner_counts=verify_person_owner(rows)
    research_owner_counts=verify_research_owner(rows)
    domestic_owner_counts=verify_domestic_owner(rows)
    diplomacy_eligibility_counts=verify_diplomacy_eligibility(rows)
    strategy_ground_counts=verify_strategy_ground(rows)
    lucky_unit_event_counts=verify_lucky_unit_event(rows)
    lucky_duel_counts=verify_lucky_duel(rows)
    lucky_duel_post_counts=verify_lucky_duel_post(rows)
    lucky_duel_response_counts=verify_lucky_duel_response(rows)
    lucky_mission_counts=verify_lucky_mission(rows)
    diplomacy_interaction_counts=verify_diplomacy_interaction(rows)
    diplomacy_contest_counts=verify_diplomacy_contest(rows)
    reach_owner_counts=verify_reach_owner(rows)
    strategy_owner_counts=verify_strategy_owner(rows)
    query_context_counts=verify_query_context(rows)
    query_context_counts.update(verify_fire_immunity(rows))
    query_context_counts.update(verify_area_friendly(rows))
    query_context_counts.update(verify_raw_priority(rows))
    query_context_counts.update(verify_tribal_affinity(rows))
    actor_rows=rows('actor_metadata_test.csv');actor_index=0
    for layout in range(2):
        cities=530 if layout else 42;bases=cities+45
        for person in ((0,635,850,14162,14612) if layout else (0,635,1099)):
            for location in (0,cities-1,cities,bases-1,bases+17,-1,5000):
                row=actor_rows[actor_index];actor_index+=1
                assert (row['person_id'],row['person_name'],row['force_id'],row['ruler_id'],row['ruler_name'])==(
                    str(person),'劉備' if person==635 else '夏育',str(252 if layout else 45),'635','劉備')
                assert row['actor_city']==str(location if 0<=location<cities else -1)
                assert (row['game_year'],row['game_month'],row['game_day'])==('194','6','11')
                assert (row['stage'],row['scene'],row['event'],row['metric'],row['delta'])==('applied','metadata_test','actor_readback','gold','1')
                assert json.loads(row['context_json'])==dict(layout=layout,location=location,home_base=23,city_limit=cities)
    assert actor_index==len(actor_rows)==56
    actor_edges=rows('actor_metadata_edges_test.csv');assert len(actor_edges)==20
    for fault,row in enumerate(actor_edges):
        unknown=fault>=16;no_force=unknown or fault in (1,2,3,4,7,8);no_ruler=no_force or fault in (5,6)
        assert (row['person_id'],row['person_name'])==('-1','') if unknown else (row['person_id'],row['person_name'])==('14162','夏育')
        assert (row['force_id'],row['ruler_id'],row['ruler_name'])==('-1' if no_force else '252','-1' if no_ruler else '635','' if no_ruler else '劉備')
        assert row['actor_city']==str(-1 if unknown or fault in (9,14) else 529 if fault==15 else 12)
        assert (row['game_year'],row['game_month'],row['game_day'])==(('0','0','0') if unknown or fault in (10,11,12,13) else ('194','6','11'))
        assert (row['stage'],row['scene'],row['event'],row['metric'],row['delta'])==('applied','metadata_test','actor_readback','gold','1')
        assert json.loads(row['context_json'])==dict(metadata_case=fault)
    fire_aftermath=rows('fire_aftermath_test.csv');assert len(fire_aftermath)==28
    fire_providers=((52,1000,52),(62,1000,62),(52,601,52),(62,601,62),(52,1000,249),(52,601,52),(62,14162,62))
    for profile,(effect,person,owned) in enumerate(fire_providers):
        for visibility in (0,1):
            query,applied=fire_aftermath[profile*4+visibility*2:profile*4+visibility*2+2]
            for row in (query,applied):
                assert (row['effect_skill'],row['person_id'],row['owned_skill'],row['force_id'],row['scene'])==(str(effect),str(person),str(owned),'2' if person==14162 else '3','turn_update')
            assert (query['stage'],query['event'])==('check_matched','fire_casualty_protection_eligibility')
            assert (applied['stage'],applied['event'],applied['caller'])==('applied','fire_casualty_processing_skipped',str(0x582fed if effect==52 else 0x583012))
            assert (applied['actor_unit'],applied['target_unit'],applied['target_force'],applied['equipment_id'],applied['target_x'],applied['target_y'])==('4094','4094','3','2','2','1')
            assert json.loads(applied['context_json'])==dict(native_handler='0x582fb0',scope='turn_update_fire_casualties',counterfactual_evaluated=False)
    fire_preview=rows('fire_aftermath_preview_test.csv');assert len(fire_preview)==2
    assert [(r['stage'],r['effect_skill'],r['event']) for r in fire_preview]==[('check_matched','52','skill_predicate'),('check_matched','62','skill_predicate')]
    fire_edges=rows('fire_aftermath_edges_test.csv');assert len(fire_edges)==11
    assert [r['stage'] for r in fire_edges]==['check_matched','applied']+['check_matched']*9
    assert all(r['effect_skill']=='62' and r['owned_skill']=='62' and r['person_id']=='1000' for r in fire_edges)
    assert fire_edges[1]['event']=='fire_casualty_processing_skipped'
    casualty=rows('casualty_test.csv');casualty_index=0
    casualty_kinds=('ordinary_death','ordinary_injury','tactic_injury','tactic_death');casualty_sites=(0x59736d,0x59742c,0x59750d,0x59760d)
    def casualty_row(row,kind,effect,person,owned,selected=-1,applied=False,force=3):
        assert (row['effect_skill'],row['person_id'],row['owned_skill'],row['force_id'],row['scene'])==(str(effect),str(person),str(owned),str(force),'battle')
        if not applied:
            assert (row['stage'],row['event'])==('check_matched','casualty_protection_eligibility');return
        assert (row['stage'],row['event'],row['caller'])==('applied','casualty_selection_blocked' if effect==52 else 'casualty_candidate_excluded',str(0x597220 if effect==52 else 0x59729e))
        assert (row['actor_unit'],row['target_unit'],row['target_person'],row['target_force'],row['equipment_id'])==('4094','4094','-1' if effect==52 else str(person),'3','2')
        assert (row['target_x'],row['target_y'],row['target_base'],row['target_city'])==('2','1','-1','-1')
        assert json.loads(row['context_json'])==dict(selection_scope=casualty_kinds[kind],native_selector_caller=casualty_sites[kind],selected_person=selected,counterfactual_evaluated=False)
    for kind in range(4):
        for profile in range(11):
            for pick in (0,1):
                if profile==10:continue
                guard=profile in (0,1,2,7,8);effect=52 if guard else 32
                people=(1000,601) if profile==6 else (14162,) if profile in (8,9) else (601,) if profile in (1,4) else (1000,)
                selected=-1 if guard or profile==6 else 601 if profile in (3,5) else 1000
                owned=249 if profile in (2,5) else effect
                for person in people:
                    casualty_row(casualty[casualty_index],kind,effect,person,owned);casualty_index+=1
                for person in people:
                    casualty_row(casualty[casualty_index],kind,effect,person,owned,selected,True);casualty_index+=1
    assert casualty_index==len(casualty)==176
    casualty_preview=rows('casualty_preview_test.csv');assert len(casualty_preview)==3
    assert [(r['stage'],r['event'],r['effect_skill'],r['person_id']) for r in casualty_preview]==[
        ('check_matched','skill_predicate','52','1000'),('check_matched','skill_predicate','32','1000'),('check_matched','skill_predicate','32','601')]
    casualty_edges=rows('casualty_edges_test.csv');casualty_index=0
    for fault in range(10):
        for person in ((601,) if fault==1 else (1000,601)):
            casualty_row(casualty_edges[casualty_index],0,32,person,32,force=4 if fault==6 and person==601 else 3);casualty_index+=1
        for person in ((1000,601) if fault==0 else (601,) if fault in (2,8) else ()):
            casualty_row(casualty_edges[casualty_index],0,32,person,32,-1,True);casualty_index+=1
    for person in (1000,601):casualty_row(casualty_edges[casualty_index],0,32,person,32);casualty_index+=1
    assert casualty_index==len(casualty_edges)==25
    concurrent = rows('skill_telemetry_test.csv')
    assert len(concurrent) == 401 and len({row['thread'] for row in concurrent[:400]}) == 4
    assert all(row['person_name'] == 'A,"B' and row['ruler_name'] == '劉備' for row in concurrent)
    assert concurrent[-1]['stage'] == 'applied' and concurrent[-1]['delta'] == '20'
    live_read=rows('telemetry_live_read_test.csv')
    assert len(live_read)==2
    for row,before,after in zip(live_read,(10,20),(11,25)):
        assert (row['stage'],row['scene'],row['event'],row['metric'])==('applied','test','control','gold')
        assert (int(row['before']),int(row['after']),int(row['delta']))==(before,after,after-before)
        assert row['person_id']=='14162' and row['person_name']=='A,"B' and row['ruler_name']=='劉備'
    for name in ('telemetry_context_test.csv', 'telemetry_session_old_test.csv', 'telemetry_session_new_test.csv'):
        control = rows(name)
        assert len(control) == 1
        assert control[0]['stage'] == 'applied' and control[0]['scene'] == 'test' and control[0]['event'] == 'control'
        assert control[0]['owned_skill'] == control[0]['effect_skill'] == '61'
        assert (control[0]['before'], control[0]['after'], control[0]['delta']) == ('10', '11', '1')

    precision_all = rows('precision_telemetry_test.csv')
    precision = [r for r in precision_all if r['stage'] == 'applied']
    assert len(precision_all) == 6 and len(precision) == 3
    assert [(int(r['before']), int(r['after']), int(r['baseline_after']), int(r['bonus_delta'])) for r in precision] == [
        (1000, 1050, 1025, 25), (9980, 10000, 10000, 0), (10000, 10000, 10000, 0)]
    assert all(r['stage'] == 'applied' and r['effect_skill'] == '20' for r in precision)
    def precision_query(row, owned='20', person='1500'):
        assert (row['stage'], row['scene'], row['event']) == ('check_matched', 'battle', 'precision_reward_eligibility')
        assert (row['effect_skill'], row['owned_skill'], row['person_id']) == ('20', owned, person)
    def precision_effect(row, before, after, baseline, force='1', owned='20', person='1500',kind=2,base=23):
        assert (row['stage'], row['scene'], row['event']) == ('applied', 'battle', 'defeat_technology_reward_bonus')
        assert (row['effect_skill'], row['owned_skill'], row['person_id']) == ('20', owned, person)
        assert row['metric'] == 'technology_points' and row['force_id'] == force and row['target_force'] == force
        assert (int(row['before']), int(row['after']), int(row['baseline_after'])) == (before, after, baseline)
        assert int(row['bonus_delta']) == after - baseline
        assert row['actor_unit'] == ('999' if kind==1 else '-1')
        assert json.loads(row['context_json']) == dict(source_base=base if kind==2 else -1,source_unit=999 if kind==1 else -1,
            defeated_unit=998,defeated_force=3,provider_force=int(force),reward_force=int(force),defeat_root='0x4be790')
    for row in precision_all:
        if row['stage'] == 'check_matched':precision_query(row)
    native_precision = rows('precision_native_test.csv');precision_index=0
    for expanded,kind in ((False,1),(False,2),(True,1),(True,2)):
        for owned in ('20','249'):
            person='1500' if owned=='20' else '14162'
            for player in (False,True):
                for has_object in (False,True):
                    for force in ('0','248' if expanded else '41'):
                        for before in (0,1,1000,9975,9980,9999,10000,65535):
                            precision_query(native_precision[precision_index],owned,person)
                            precision_effect(native_precision[precision_index+1],before,min(10000,before+50),min(10000,before+25),force,owned,person,kind)
                            precision_index+=2
    assert precision_index==len(native_precision)==1024
    rejected_precision=rows('precision_rejected_test.csv');assert len(rejected_precision)==1;precision_query(rejected_precision[0])
    precision_source=rows('precision_session_source_test.csv');assert len(precision_source)==1;precision_query(precision_source[0])
    precision_session=rows('precision_session_after_test.csv');assert len(precision_session)==2
    precision_query(precision_session[0]);precision_effect(precision_session[1],1050,1100,1075)
    precision_context=rows('precision_context_test.csv');assert len(precision_context)==12
    for index,row in enumerate(precision_context):
        if index==1:precision_effect(row,1000,1050,1025)
        else:precision_query(row)
    precision_total=precision_all+native_precision+rejected_precision+precision_source+precision_session+precision_context
    precision_root=rows('precision_root_test.csv');root_index=0
    for kind in (1,2):
        for mutation in range(10):
            precision_query(precision_root[root_index]);root_index+=1
            if mutation in (0,6,7,8,9) or kind==1 and mutation==5:
                precision_effect(precision_root[root_index],1000,1050,1025,kind=kind);root_index+=1
    assert root_index==len(precision_root)==31
    precision_total+=precision_root
    precision_flow=rows('precision_flow_test.csv');flow_index=0
    for kind,base in ((1,23),(2,23),(2,529)):
        for profile in range(10):
            if profile==0 or kind==2 and profile==7:continue
            selected=14162 if profile in (2,8) or kind==2 and profile in (4,5) else 1500
            owned='249' if profile==3 else '20'
            precision_query(precision_flow[flow_index],owned,str(selected))
            precision_effect(precision_flow[flow_index+1],1000,1050,1025,owned=owned,person=str(selected),kind=kind,base=base)
            assert int(precision_flow[flow_index]['caller'])==(0x4bf079 if kind==1 else 0x4bf126)
            flow_index+=2
    assert flow_index==len(precision_flow)==50
    precision_total+=precision_flow
    assert len(precision_total)==1127 and sum(r['stage']=='applied' for r in precision_total)==553

    eligibility = rows('income_telemetry_test.csv')
    assert len(eligibility) == 9
    assert [int(r['effect_skill']) for r in eligibility[:7]] == [57, 79, 82, 87, 88, 89, 90]
    assert all(r['person_id'] == '1500' and r['scene'] == 'economy' and
               r['event'] == 'city_income_eligibility' for r in eligibility)

    upkeep = rows('upkeep_telemetry_test.csv')
    assert [(int(r['before']), int(r['after']), int(r['baseline_after']), int(r['bonus_delta'])) for r in upkeep] == [
        (1000, 1000, 700, 300), (100, 100, 0, 100), (0, 0, 0, 0), (1000, 1000, 700, 300),
        (1000, 1000, 700, 300), (100, 100, 0, 100)]
    assert all(r['effect_skill'] == '78' and r['target_base'] == '52' and r['target_city'] == '-1' and
               r['target_force'] == '2' for r in upkeep)
    assert [r['owned_skill'] for r in upkeep] == ['78', '78', '78', '249', '78', '78']
    assert [r['person_id'] for r in upkeep[-2:]] == ['14162', '14612']

    income = rows('income_bonus_telemetry_test.csv')
    applied = [r for r in income if r['stage'] == 'applied']
    assert len(applied) == 16
    assert {r['person_id'] for r in income if r['stage'] == 'check_matched'} == {'1500', '1501'}
    for group, effect in enumerate((87, 87, 88, 88)):
        batch = applied[group * 4:group * 4 + 4]
        assert [int(r['before']) for r in batch] == [1000, 4990, 5000, 1000]
        assert [int(r['after']) for r in batch] == [1100, 5000, 5000, 1100]
        assert [int(r['bonus_delta']) for r in batch] == [100, 10, 0, 100]
        assert [int(r['target_base']) for r in batch] == [8, 8, 8, 52]
        assert [int(r['target_city']) for r in batch] == [8, 8, 8, -1]
        assert all(r['owned_skill'] == '249' and r['effect_skill'] == str(effect) and
                   r['person_id'] == '1500' and r['baseline_after'] == r['before'] for r in batch)
    income_context_source=rows('income_context_source_test.csv')
    assert len(income_context_source)==7 and all(r['stage']=='check_matched' for r in income_context_source)
    income_context_after=rows('income_context_after_test.csv')
    assert len(income_context_after)==9
    income_nested=rows('income_nested_test.csv');assert len(income_nested)==21
    for offset in (0,10,20):
        assert (income_nested[offset]['stage'],income_nested[offset]['person_id'],income_nested[offset]['owned_skill'])==('check_matched','1501','87')
    def income_context_effect(row,effect,person,city):
        assert (row['stage'],row['scene'],row['effect_skill'],row['owned_skill'],row['person_id'])==('applied','economy',str(effect),'249',str(person))
        assert (row['target_base'],row['target_city'],row['target_force'])==(str(city),str(city),'2')
        assert row['metric']==('gold' if effect in (87,89) else 'food') and row['before']=='1000'
        assert (row['after'],row['baseline_after'],row['bonus_delta'])==(('1100','1000','100') if effect<89 else ('1051','1103','-52'))
    for group,expected in ((income_context_after,(87,88)),(income_nested,(87,88,87,88))):
        effects=[r for r in group if r['stage']=='applied'];assert len(effects)==len(expected)
        for row,effect in zip(effects,expected):income_context_effect(row,effect,1500,8)
    native_income=rows('income_native_collection_test.csv');native_income_index=0
    # Independently enumerate the expected native list after status filtering.
    # Empty base, failed append and no-skill lists produce no attribution.
    income_lists=((),((1500,249),(1501,87)),((1500,87),(1501,88)),
                  ((1500,-1),(1501,249)),((1501,87),),((1500,249),(1501,87)),
                  ((1500,249),(1500,249)),(),(),((1500,-1),(1501,87)))
    for people in income_lists:
        providers={}
        for person,owned in people:
            effects=(57,79,82,87,88,89,90) if owned==249 else (owned,) if owned>=0 else ()
            for effect in effects:
                row=native_income[native_income_index];native_income_index+=1
                assert (row['stage'],row['scene'],row['event'],row['caller'])==('check_matched','economy','city_income_eligibility',str(0x4cea41))
                assert (row['effect_skill'],row['owned_skill'],row['person_id'])==(str(effect),str(owned),str(person))
                providers.setdefault(effect,(person,owned))
        for effect in (87,88):
            if effect not in providers:continue
            person,owned=providers[effect];row=native_income[native_income_index];native_income_index+=1
            assert (row['stage'],row['scene'],row['event'])==('applied','economy','gold_income_bonus' if effect==87 else 'food_income_bonus')
            assert (row['effect_skill'],row['owned_skill'],row['person_id'])==(str(effect),str(owned),str(person))
            assert (row['target_base'],row['target_city'],row['target_force'])==('8','8','2')
            assert (row['metric'],row['before'],row['after'],row['delta'],row['baseline_after'],row['bonus_delta'])==('gold' if effect==87 else 'food','1000','1100','100','1000','100')
    assert native_income_index==len(native_income)==53
    assert sum(r['stage']=='applied' for r in native_income)==12
    revenue_context=rows('revenue_context_test.csv');revenue_context_index=0
    for effect in (87,88,89,90):
        for mutation in range(7):
            batch=revenue_context[revenue_context_index:revenue_context_index+4];revenue_context_index+=4
            assert [r['effect_skill'] for r in batch]==['87','88','89','90']
            assert all((r['stage'],r['owned_skill'],r['person_id'])==('check_matched','249','600') for r in batch)
            if mutation==0:
                income_context_effect(revenue_context[revenue_context_index],effect,600,0);revenue_context_index+=1
    assert revenue_context_index==len(revenue_context)==116
    revenue_off_context=rows('revenue_offmonth_context_test.csv');assert len(revenue_off_context)==6
    for index,row in enumerate(revenue_off_context):
        if index in (1,2):
            assert (row['stage'],row['event'],row['effect_skill'],row['owned_skill'],row['person_id'])==('applied','between_month_gold_collection','89','249','600')
            assert (row['before'],row['after'],row['baseline_after'])==('1000','1051' if index==1 else '1010','1000')
            assert row['target_base']==('0' if index==1 else '52')
        else:
            assert (row['stage'],row['effect_skill'],row['owned_skill'],row['person_id'])==('check_matched','89','249','600')
    domestic = rows('domestic_telemetry_test.csv')
    applied_domestic = [r for r in domestic if r['stage'] == 'applied']
    assert len(applied_domestic) == 24
    # Actual queries only: the counterfactual calculation must not produce telemetry queries.
    domestic_queries = [r for r in domestic if r['stage'] == 'check_matched']
    assert len(domestic_queries) == 36
    assert all(r['event'] == 'command_effect_eligibility' and
               r['scene'] == ('recruitment' if r['effect_skill'] == '79' else 'production') for r in domestic_queries)
    assert all(r['person_id'] == '1500' and r['target_base'] == '8' and r['target_force'] == '2' for r in applied_domestic)
    for group, effect in enumerate((80, 81)):
        batch = applied_domestic[group * 4:group * 4 + 4]
        assert [int(r['before']) for r in batch] == [1000, 98500, 99500, 100000]
        assert [int(r['after']) for r in batch] == [3774, 100000, 100000, 100000]
        assert [int(r['baseline_after']) for r in batch] == [2386, 99886, 100000, 100000]
        assert [int(r['bonus_delta']) for r in batch] == [1388, 114, 0, 0]
        assert all(r['effect_skill'] == str(effect) and r['equipment_id'] == ('1' if effect == 80 else '4') for r in batch)
    recruited = applied_domestic[8:12]
    assert [int(r['before']) for r in recruited] == [1000, 98500, 99500, 100000]
    assert [int(r['after']) for r in recruited] == [2866, 100000, 100000, 100000]
    assert [int(r['bonus_delta']) for r in recruited] == [622, 256, 0, 0]
    assert all(r['effect_skill'] == '79' and r['metric'] == 'troops' and r['equipment_id'] == '-1' for r in recruited)
    timed = applied_domestic[12:]
    assert all(r['before'] == '0' and r['after'] == '4' and r['baseline_after'] == '9' and
               r['bonus_delta'] == '-5' and r['metric'] == 'remaining_turns' for r in timed)
    assert [r['equipment_id'] for r in timed] == [str(i) for i in (5, 6, 7, 8, 10, 11) for _ in range(2)]
    assert [r['target_person'] for r in timed] == ['1500', '1501'] * 6
    assert [r['effect_skill'] for r in timed] == ['82'] * 8 + ['83'] * 4
    battle = rows('battle_telemetry_test.csv')
    battle_applied = [r for r in battle if r['stage'] == 'applied']
    battle_queries = [r for r in battle if r['stage'] == 'check_matched']
    assert len(battle_applied) == 34 and len(battle_queries) == 33
    assert all(r['target_unit'] == '17' and r['target_force'] == '2' and r['force_id'] == '2' and
               r['actor_unit'] == '17' and
               r['target_base'] == '-1' and r['equipment_id'] == '-1' and r['baseline_after'] == r['before']
               for r in battle_applied)
    for group, cap in enumerate((100, 120)):
        for path, (effect, grant) in enumerate(((30, 5), (30, 5), (11, 10), (75, 5), (76, 10))):
            batch = battle_applied[group * 15 + path * 3:group * 15 + path * 3 + 3]
            assert [int(r['before']) for r in batch] == [10, cap - 2, cap]
            assert [int(r['after']) for r in batch] == [10 + grant, cap, cap]
            assert [int(r['bonus_delta']) for r in batch] == [grant, 2, 0]
            assert all(r['effect_skill'] == str(effect) and r['person_id'] == '600' and r['owned_skill'] == '249'
                       and r['metric'] == 'energy' and r['scene'] == ('turn_update' if effect in (75, 76) else 'battle') for r in batch)
    absorbed = battle_applied[30:33]
    assert [int(r['before']) for r in absorbed] == [1000, 9990, 10000]
    assert [int(r['after']) for r in absorbed] == [1100, 10000, 10000]
    assert [int(r['bonus_delta']) for r in absorbed] == [100, 10, 0]
    assert all(r['effect_skill'] == '22' and r['metric'] == 'troops' for r in absorbed)
    assert battle_applied[-1]['effect_skill'] == '30' and battle_applied[-1]['person_id'] == '1500'
    assert battle_applied[-1]['before'] == '10' and battle_applied[-1]['after'] == '15'
    # Expanded members rejected by the native rest branch appear only as eligibility queries.
    rejected = [r for r in battle if r['person_id'] == '1500' and r['effect_skill'] in ('75', '76')]
    assert len(rejected) == 2 and all(r['stage'] == 'check_matched' for r in rejected)
    recovery_source = rows('battle_session_source_test.csv')
    assert len(recovery_source) == 3 and all(r['stage'] == 'check_matched' for r in recovery_source)
    assert [r['effect_skill'] for r in recovery_source] == ['11', '75', '76']
    recovery_session = rows('battle_session_after_test.csv')
    recovery_context = rows('battle_context_test.csv')
    for recovery, expected_rows in ((recovery_session, 8), (recovery_context, 23)):
        actual = [r for r in recovery if r['stage'] == 'applied']
        assert len(recovery) == expected_rows and len(actual) == 5
        for row, effect, before, after, event in zip(actual, (30, 11, 22, 75, 76),
                (10, 10, 1000, 10, 10), (15, 20, 1100, 15, 20),
                ('attacked_energy_recovery', 'defeat_energy_recovery', 'attack_troops_absorbed',
                 'field_energy_recovery', 'military_band_energy_bonus')):
            assert (row['effect_skill'], row['owned_skill'], row['person_id']) == (str(effect), '249', '600')
            assert row['event'] == event and row['scene'] == ('turn_update' if effect in (75, 76) else 'battle')
            assert row['metric'] == ('troops' if effect == 22 else 'energy')
            assert (int(row['before']), int(row['after']), int(row['baseline_after'])) == (before, after, before)
            assert int(row['bonus_delta']) == after - before
            assert (row['actor_unit'], row['target_unit'], row['force_id'], row['target_force']) == ('17', '17', '2', '2')
    pressure = rows('pressure_telemetry_test.csv')
    pressure_applied = [r for r in pressure if r['stage'] == 'applied']
    pressure_queries = [r for r in pressure if r['stage'] == 'check_matched']
    assert len(pressure_applied) == 30 and len(pressure_queries) == 97
    assert all(r['actor_unit'] == '16' and r['target_unit'] == '17' and r['force_id'] == '2' and
               r['target_force'] == '3' and r['scene'] == 'battle' and r['event'] == 'attack_energy_pressure' and
               r['metric'] == 'energy' for r in pressure_applied)
    expected = [(9, 600, 9, 30, 25, 30, -5), (9, 600, 9, 3, 0, 3, -3), (9, 600, 9, 0, 0, 0, 0),
                (10, 600, 10, 30, 10, 30, -20), (10, 600, 10, 10, 0, 10, -10), (10, 600, 10, 0, 0, 0, 0)]
    for person, owned in ((600, 249), (601, 10)):
        expected += [(10, person, owned, before, after, baseline, bonus)
                     for before, after, baseline, bonus in ((30, 10, 25, -15), (10, 0, 5, -5), (3, 0, 0, 0), (0, 0, 0, 0))]
    expected = expected * 2 + [(10, 600, 10, 50, 30, 50, -20), (10, 600, 10, 30, 10, 30, -20)]
    keys = ('effect_skill', 'person_id', 'owned_skill', 'before', 'after', 'baseline_after', 'bonus_delta')
    assert [tuple(int(r[k]) for k in keys) for r in pressure_applied] == expected
    assert sum(r['event'] == 'pressure_settlement_revalidation' for r in pressure_queries) == 47
    movement = rows('movement_telemetry_test.csv')
    movement_applied = [r for r in movement if r['stage'] == 'applied']
    movement_queries = [r for r in movement if r['stage'] == 'check_matched']
    assert len(movement_applied) == 20 and len(movement_queries) == 23
    expected_movement = []
    for effect, terrain, event in ((6, 11, 'cliff_immunity'), (8, 4, 'poison_immunity'), (62, 0, 'terrain_fire_immunity')):
        expected_movement += [(effect, person, owned, terrain, event)
                              for person, owned in ((600, effect), (600, 249), (14162, effect), (600, effect))]
    expected_movement += [(62, 14162, 62, 4, 'terrain_fire_immunity')] + [(6, 600, 6, 11, 'cliff_immunity')] * 2
    expected_movement += [(31, person, owned, 0, 'terrain_fire_damage_doubled')
                          for person, owned in ((600, 31), (600, 249), (14162, 31), (600, 31))]
    expected_movement += [(62, 600, 62, 0, 'terrain_fire_immunity')]
    assert [(int(r['effect_skill']), int(r['person_id']), int(r['owned_skill']), int(r['terrain_id']), r['event'])
            for r in movement_applied] == expected_movement
    assert all(r['scene'] == 'movement' and r['target_unit'] == r['actor_unit'] == '17' and
               r['force_id'] == r['target_force'] == '2' and r['target_x'] == '1' and r['target_y'] == '2' and
               int(r['caller']) == 0x5848a9 for r in movement_applied)
    vines = [r for r in movement_applied if r['effect_skill'] == '31']
    assert len(vines) == 4 and all(r['metric'] == 'route_hazard_damage' and r['before'] == '67' and
                                 r['after'] == '134' and r['baseline_after'] == '67' and r['bonus_delta'] == '67' for r in vines)
    assert sum(r['event'] == 'movement_hazard_eligibility' for r in movement_queries) == 20
    assert sum(r['scene'] == 'unclassified_query' for r in movement_queries) == 3
    movement_stats = rows('movement_stat_telemetry_test.csv')
    stat_applied = [r for r in movement_stats if r['stage'] == 'applied']
    stat_queries = [r for r in movement_stats if r['stage'] == 'check_matched']
    assert len(stat_applied) == 14 and len(stat_queries) == 16
    expected_stats = [
        (2, 600, 2, 1, 25, 20), (3, 600, 3, 4, 23, 20), (7, 600, 7, 1, 30, 25),
        (2, 600, 249, 1, 25, 20), (2, 600, 249, 4, 25, 23), (2, 14162, 2, 4, 25, 23),
        (7, 600, 249, 1, 30, 25), (2, 14162, 2, 1, 25, 20), (3, 14162, 3, 4, 23, 20),
        (7, 14162, 7, 1, 30, 25), (2, 600, 2, 1, 25, 20), (2, 600, 2, 1, 3, 254),
        (2, 600, 2, 1, 25, 20), (2, 600, 2, 1, 25, 20)]
    assert [tuple(int(r[k]) for k in ('effect_skill', 'person_id', 'owned_skill', 'equipment_id', 'after', 'baseline_after'))
            for r in stat_applied] == expected_stats
    assert all(r['scene'] == 'unit_stats' and r['event'] == 'movement_capacity_refreshed' and
               r['metric'] == 'movement_points' and r['before'] == '0' and r['target_unit'] == r['actor_unit'] == '17' and
               r['force_id'] == r['target_force'] == '2' and int(r['caller']) == 0x496eff for r in stat_applied)
    assert sum(r['event'] == 'movement_capacity_eligibility' for r in stat_queries) == 15
    assert sum(r['scene'] == 'unclassified_query' for r in stat_queries) == 1
    revenue = rows('revenue_telemetry_test.csv')
    revenue_applied = [r for r in revenue if r['stage'] == 'applied']
    revenue_queries = [r for r in revenue if r['stage'] == 'check_matched']
    assert len(revenue_applied) == 300 and len(revenue_queries) == 131
    expected_revenue = []

    def revenue_payment(state, base, metric, amount, normal, effect, event, person, owned,
                        month=1, day=1, bonus_person=None, bonus_owned=None):
        before = state[base, metric]
        after = min(5000, before + amount)
        state[base, metric] = after
        if bonus_person is not None:
            expected_revenue.append((87 if metric == 'gold' else 88, bonus_person, bonus_owned,
                                     'gold_income_bonus' if metric == 'gold' else 'food_income_bonus',
                                     month, day, base, metric, before, after, before))
        expected_revenue.append((effect, person, owned, event, month, day, base, metric,
                                 before, after, min(5000, before + normal)))

    for month in (1, 2):
        for person in (600, 14162):
            for facility in (False, True):
                for before in (1000, 4990, 5000):
                    state = {(base, metric): before for base in (0, 52) for metric in ('gold', 'food')}
                    full_food = 103 * 3 // 2 if facility else 103
                    food = full_food // 2
                    normal_food = full_food if month == 1 else 0
                    revenue_payment(state, 0, 'gold', 51, 103, 89, 'monthly_gold_installment', person, 249, month)
                    revenue_payment(state, 0, 'gold', 25, 51, 89, 'monthly_gold_bonus_installment', person, 249, month,
                                    bonus_person=person, bonus_owned=249)
                    revenue_payment(state, 0, 'food', food, normal_food, 90, 'food_collection_installment', person, 249, month)
                    if month == 1:
                        revenue_payment(state, 0, 'food', food // 2, full_food // 2, 90, 'food_bonus_installment',
                                        person, 249, month, bonus_person=person, bonus_owned=249)
                    revenue_payment(state, 52, 'gold', 10, 20, 89, 'monthly_gold_installment', person, 249, month)
                    revenue_payment(state, 52, 'gold', 5, 10, 89, 'monthly_gold_bonus_installment', person, 249, month,
                                    bonus_person=person, bonus_owned=249)
                    revenue_payment(state, 52, 'food', food // 5, normal_food // 5, 90, 'food_collection_installment', person, 249, month)
                    if month == 1:
                        revenue_payment(state, 52, 'food', food // 5 // 2, full_food // 5 // 2, 90, 'food_bonus_installment',
                                        person, 249, month, bonus_person=person, bonus_owned=249)
    # Two matching providers still produce one tax grant, attributed to the first.
    state = {(0, 'gold'): 1000}
    revenue_payment(state, 0, 'gold', 51, 103, 89, 'monthly_gold_installment', 600, 89)
    # Separate wealth/rice providers must not inherit the tax/collection attribution.
    state = {(0, 'gold'): 1000, (0, 'food'): 1000}
    revenue_payment(state, 0, 'gold', 51, 103, 89, 'monthly_gold_installment', 600, 89)
    revenue_payment(state, 0, 'gold', 25, 51, 89, 'monthly_gold_bonus_installment', 600, 89,
                    bonus_person=14162, bonus_owned=87)
    revenue_payment(state, 0, 'food', 77, 154, 90, 'food_collection_installment', 600, 90)
    revenue_payment(state, 0, 'food', 38, 77, 90, 'food_bonus_installment', 600, 90,
                    bonus_person=14162, bonus_owned=88)
    for day in (11, 21):
        for person, owned in ((600, 89), (600, 249), (14162, 89), (600, 89)):
            for before in (1000, 4990, 5000):
                state = {(0, 'gold'): before, (52, 'gold'): before}
                for base, amount in ((0, 51), (52, 10)):
                    revenue_payment(state, base, 'gold', amount, 0, 89, 'between_month_gold_collection', person, owned, day=day)
    state = {(0, 'gold'): 1000}
    revenue_payment(state, 0, 'gold', 51, 0, 89, 'between_month_gold_collection', 600, 89, day=11)
    state = {(0, 'gold'): 1000, (52, 'gold'): 1000}
    for _ in range(2):
        for base, amount in ((0, 51), (52, 10)):
            revenue_payment(state, base, 'gold', amount, 0, 89, 'between_month_gold_collection', 600, 89, day=11)
    revenue_keys = ('effect_skill', 'person_id', 'owned_skill', 'event', 'game_month', 'game_day',
                    'target_base', 'metric', 'before', 'after', 'baseline_after')
    assert [tuple(r[k] if k in ('event', 'metric') else int(r[k]) for k in revenue_keys)
            for r in revenue_applied] == expected_revenue
    assert all(r['scene'] == 'economy' and r['game_year'] == '194' and r['force_id'] == r['target_force'] == '2' and
               r['target_city'] == ('0' if r['target_base'] == '0' else '-1') and r['target_unit'] == '-1'
               for r in revenue_applied)
    assert sum(r['event'] == 'between_month_tax_eligibility' for r in revenue_queries) == 27
    assert sum(r['event'] == 'city_income_eligibility' for r in revenue_queries) == 103
    assert sum(r['scene'] == 'unclassified_query' for r in revenue_queries) == 1
    unrest = rows('unrest_telemetry_test.csv')
    unrest_applied = [r for r in unrest if r['stage'] == 'applied']
    unrest_queries = [r for r in unrest if r['stage'] == 'check_matched']
    assert len(unrest_applied) == 47 and len(unrest_queries) == 104
    assert all(r['scene'] == 'unrest' and r['event'] == 'city_unrest_eligibility' for r in unrest_queries)
    assert all(r['scene'] == 'unrest' and r['force_id'] == '2' and r['target_unit'] == '-1' for r in unrest_applied)
    suppressed = [r for r in unrest_applied if r['effect_skill'] == '95']
    assert len(suppressed) == 11
    expected_suppression = [(person, owned, order) for person, owned in ((600, 95), (600, 249), (14162, 95), (600, 95))
                            for order in (60, 79)] + [(600, 95, 70)] * 3
    assert [(int(r['person_id']), int(r['owned_skill']), json.loads(r['context_json'])['public_order'])
            for r in suppressed] == expected_suppression
    assert all(r['target_city'] == r['target_base'] == '0' and r['target_force'] == '2' and
               r['event'] == 'bandit_roll_suppressed' and int(r['caller']) == 0x58ea0a and
               json.loads(r['context_json']) == dict(public_order=order, native_threshold=60, ordinary_threshold=80, rng_skipped=True)
               for r, (_, _, order) in zip(suppressed, expected_suppression))
    redirected = [r for r in unrest_applied if r['effect_skill'] != '95']
    expected_redirects = []
    for base in (42, 249):
        for city, effect, relative in ((0, 91, 2), (19, 92, 0), (23, 93, 1), (38, 94, 3)):
            expected_redirects += [(effect, person, owned, city, base + relative, base + 4)
                                   for person, owned in ((600, effect), (600, 249), (14162, effect), (600, effect))]
        expected_redirects += [(91, 600, 91, 0, base + 2, base + 4)] * 2
    assert len(redirected) == 36
    assert [(int(r['effect_skill']), int(r['person_id']), int(r['owned_skill']), json.loads(r['context_json'])['protected_city'],
             json.loads(r['context_json'])['original_force'], int(r['target_force'])) for r in redirected] == expected_redirects
    assert all(r['target_city'] == '-1' and r['target_base'] == '1000' and r['target_x'] == '17' and r['target_y'] == '34' and
               r['event'] == 'tribal_camp_replaced_by_bandit_camp' and int(r['caller']) == 0x58eaaa and
               json.loads(r['context_json']) == dict(protected_city=city, original_force=original, actual_force=actual)
               for r, (_, _, _, city, original, actual) in zip(redirected, expected_redirects))
    unrest_context=rows('unrest_collection_context_test.csv');unrest_context_index=0
    for city,effect,relative in ((0,91,2),(19,92,0),(23,93,1),(38,94,3)):
        for mode in range(8):
            query_batch=[(600,249,e,'unrest') for e in range(91,96)]
            if mode in (1,2,3,5,6):query_batch.append((14162,95,95,'economy' if mode==1 else 'unrest'))
            if mode in (1,2,6):query_batch += [(600,249,e,'unrest') for e in range(91,96)]
            for person,owned,queried,scene in query_batch:
                row=unrest_context[unrest_context_index];unrest_context_index+=1
                assert (row['stage'],row['scene'],row['event'],row['person_id'],row['owned_skill'],row['effect_skill'])==('check_matched',scene,'city_income_eligibility' if scene=='economy' else 'city_unrest_eligibility',str(person),str(owned),str(queried))
                assert (row['actor_city'],row['force_id'],row['game_year'],row['caller'])==(str(city),'2','194',str(0x4cea41))
            if mode not in (0,1,2,6):continue
            roll,camp=unrest_context[unrest_context_index:unrest_context_index+2];unrest_context_index+=2
            for row,applied_effect in ((roll,95),(camp,effect)):
                assert (row['stage'],row['scene'],row['owned_skill'],row['effect_skill'],row['person_id'],row['force_id'])==('applied','unrest','249',str(applied_effect),'600','2')
            assert (roll['event'],roll['caller'],roll['target_base'],roll['target_city'],roll['target_force'])==('bandit_roll_suppressed',str(0x58ea0a),str(city),str(city),'2')
            assert json.loads(roll['context_json'])==dict(public_order=70,native_threshold=60,ordinary_threshold=80,rng_skipped=True)
            assert (camp['event'],camp['caller'],camp['target_base'],camp['target_city'],camp['target_force'],camp['target_x'],camp['target_y'])==('tribal_camp_replaced_by_bandit_camp',str(0x58eaaa),'1000','-1','253','17','34')
            assert json.loads(camp['context_json'])==dict(protected_city=city,original_force=249+relative,actual_force=253)
    assert unrest_context_index==len(unrest_context)==272
    assert sum(r['stage']=='applied' for r in unrest_context)==32
    growth = rows('growth_telemetry_test.csv')
    growth_applied = [r for r in growth if r['stage'] == 'applied']
    growth_queries = [r for r in growth if r['stage'] == 'check_matched']
    assert len(growth_applied) == 204 and len(growth_queries) == 206
    assert sum(r['event'] == 'guidance_eligibility' for r in growth_queries) == 204
    assert sum(r['scene'] == 'unclassified_query' for r in growth_queries) == 2
    expected_growth = []
    # The native no-guidance outcomes are also executed in the C++ fixture.
    weapon_cases = [(0, 10, 20, 0, 50, 30), (0, 140, 10, 1, 10, 150), (1, 190, 10, 2, 10, 350),
                    (2, 200, 30, 3, 0, 580), (2, 249, 10, 3, 0, 600), (0, 10, 400, 1, 255, 405)]
    thresholds = (150, 200, 250)
    for person, owned in ((600, 84), (600, 249), (14162, 84), (600, 84)):
        for kind in range(5):
            for before in (0, 2990, 3000):
                expected_growth.append((person, owned, 'ability_experience_bonus', 'ability_experience', before,
                                        min(3000, before + 40), min(3000, before + 20),
                                        dict(attribute_id=kind, requested_experience=20)))
        for kind in range(6):
            for level, progress, request, after_level, after_progress, normal in weapon_cases:
                expected_growth.append((person, owned, 'weapon_experience_bonus', 'weapon_experience_total',
                                        sum(thresholds[:level]) + progress, sum(thresholds[:after_level]) + after_progress,
                                        normal, dict(aptitude_id=kind, requested_experience=request, level_before=level,
                                                     level_after=after_level, progress_before=progress, progress_after=after_progress)))
    def growth_context(row):
        context = json.loads(row['context_json'])
        assert 0 < context.pop('grant_caller') < 2 ** 32
        return context

    assert [(int(r['person_id']), int(r['owned_skill']), r['event'], r['metric'], int(r['before']),
             int(r['after']), int(r['baseline_after']), growth_context(r)) for r in growth_applied] == expected_growth
    assert all(r['effect_skill'] == '84' and r['scene'] == 'growth' and r['target_person'] == '1000' and
               r['force_id'] == r['target_force'] == '2' and r['target_city'] == '-1' for r in growth_applied)

    spouse = rows('spouse_telemetry_test.csv')
    spouse_applied = [r for r in spouse if r['stage'] == 'applied']
    spouse_queries = [r for r in spouse if r['stage'] == 'check_matched']
    assert len(spouse_applied) == 90 and len(spouse_queries) == 100
    assert all(r['event'] == 'spousal_bonus_eligibility' for r in spouse_queries)
    expected_spouse = []
    for person in (601, 1000, 1000):  # spouse, self, then self-first when both own native 99
        for base in (0, 50, 100):
            for kind in range(5):
                for cache, offset in (('current', 3), ('base', 0)):
                    expected_spouse.append((person, max(1, min(100, base + offset + 1)),
                                            max(1, min(100, base + offset)), dict(attribute_id=kind, cache=cache, native_bonus=1)))
    assert [(int(r['person_id']), int(r['after']), int(r['baseline_after']), json.loads(r['context_json']))
            for r in spouse_applied] == expected_spouse
    assert all(r['effect_skill'] == r['owned_skill'] == '99' and r['scene'] == 'person_stats' and
               r['event'] == 'spousal_attribute_refreshed' and r['metric'] == 'ability' and r['before'] == '42' and
               r['target_person'] == '1000' and r['force_id'] == r['target_force'] == '2' for r in spouse_applied)

    loyalty = rows('loyalty_telemetry_test.csv')
    loyalty_applied = [r for r in loyalty if r['stage'] == 'applied']
    loyalty_queries = [r for r in loyalty if r['stage'] == 'check_matched']
    assert len(loyalty_applied) == 13 and len(loyalty_queries) == 6
    assert all(r['scene'] == 'loyalty' and r['event'] == 'loyalty_aura_eligibility' for r in loyalty_queries)
    expected_loyalty = [(person, owned, value, 2) for person, owned in ((600, 96), (600, 249), (600, 96), (14162, 96))
                        for value in (100, 90, 0)] + [(600, 96, 0, 3)]
    assert [(int(r['person_id']), int(r['owned_skill']), json.loads(r['context_json'])['loyalty_before'], int(r['target_force']))
            for r in loyalty_applied] == expected_loyalty
    assert all(r['effect_skill'] == '96' and r['scene'] == 'loyalty' and r['event'] == 'loyalty_decay_check_skipped' and
               r['target_person'] == '1000' and r['target_city'] == r['target_base'] == '8' and r['force_id'] == '2' and
               json.loads(r['context_json']) == dict(loyalty_before=value, loyalty_after=value, decay_check_skipped=True)
               for r, (_, _, value, _) in zip(loyalty_applied, expected_loyalty))
    discovery = rows('discovery_telemetry_test.csv')
    discovery_applied = [r for r in discovery if r['stage'] == 'applied']
    discovery_queries = [r for r in discovery if r['stage'] == 'check_matched']
    assert len(discovery_applied) == 48 and len(discovery_queries) == 53
    expected_discovery = [(actor, owned, count, city) for actor in (600, 14162) for owned in (85, 249)
                          for count in (1, 3) for city in (8, 530, -1) for _ in (False, True)]
    assert [(int(r['person_id']), int(r['owned_skill']), json.loads(r['context_json'])['candidate_count'],
             json.loads(r['context_json'])['requested_city']) for r in discovery_applied] == expected_discovery
    assert all(r['effect_skill'] == '85' and r['scene'] == 'exploration' and r['event'] == 'person_discovered' and
               r['target_person'] == '1000' and r['target_force'] == '3' and r['force_id'] == '2' and
               r['target_city'] == r['target_base'] == '8' and int(r['caller']) == 0x5d5708 and
               json.loads(r['context_json']) == dict(status_before=7, status_after=4, location_before=9,
                   location_after=city if city >= 0 else 9, requested_city=city, candidate_count=count,
                   discovery_chance=100, counterfactual_evaluated=False)
               for r, (_, _, count, city) in zip(discovery_applied, expected_discovery))
    assert all(r['event'] == 'discovery_eligibility' and r['scene'] == 'exploration' for r in discovery_queries[:-1])
    assert discovery_queries[-1]['scene'] == 'unclassified_query' and discovery_queries[-1]['event'] == 'skill_predicate'
    research = rows('research_telemetry_test.csv')
    research_applied = [r for r in research if r['stage'] == 'applied']
    research_queries = [r for r in research if r['stage'] == 'check_matched']
    assert len(research_applied) == 48 and len(research_queries) == 62
    expected_research = [(actor, owned, cost, before) for actor, owned in ((600, 84), (600, 249), (14162, 84), (600, 84))
                         for cost in (0, 101, 65535) for before in (0, 100, 1000, 100000)]
    assert [(int(r['person_id']), int(r['owned_skill']), json.loads(r['context_json'])['base_gold_cost'], int(r['before']))
            for r in research_applied] == expected_research
    assert all(r['effect_skill'] == '84' and r['scene'] == 'research' and r['event'] == 'research_gold_cost_reduced' and
               r['metric'] == 'gold' and r['force_id'] == r['target_force'] == '2' and
               r['target_city'] == r['target_base'] == '8' and r['target_person'] == '-1' and
               int(r['after']) == max(0, before - cost // 2) and int(r['baseline_after']) == max(0, before - cost) and
               int(r['caller']) == 0x5d8f47 and json.loads(r['context_json']) == dict(base_gold_cost=cost, requested_gold_cost=cost // 2)
               for r, (_, _, cost, before) in zip(research_applied, expected_research))
    assert all(r['scene'] == 'research' and r['event'] == 'research_gold_cost_eligibility' for r in research_queries[:60])
    assert research_queries[60]['scene'] == 'unclassified_query' and research_queries[61]['scene'] == 'research'
    environment = rows('environment_telemetry_test.csv')
    environment_applied = [r for r in environment if r['stage'] == 'applied']
    environment_queries = [r for r in environment if r['stage'] == 'check_matched']
    assert len(environment_applied) == 42 and len(environment_queries) == 50
    expected_environment = [(layout, phase, kind, mode) for layout in (0, 1) for phase in range(3)
                            for kind in (0, 1) for mode in range(4) if layout or mode != 2]
    assert [(int(r['person_id']), int(r['owned_skill']), int(r['target_city']), r['event'], json.loads(r['context_json']))
            for r in environment_applied] == [
        (14162 if mode == 2 else 600, 249 if mode == 1 else 97, 529 if layout else 8,
         ('disaster_new_candidate_excluded', 'disaster_activation_skipped', 'disaster_spread_candidate_excluded')[phase],
         dict(disaster_kind=kind, active_flags=0, pending_flags=(1 << kind) if phase == 1 else 0,
              holder_bound=14612 if layout else 1099, counterfactual_evaluated=False))
        for layout, phase, kind, mode in expected_environment]
    assert all(r['effect_skill'] == '97' and r['scene'] == 'environment' and r['target_city'] == r['target_base'] and
               r['force_id'] == r['target_force'] == '2' and r['target_person'] == '-1' and
               int(r['caller']) == (0x58f0d3, 0x58f28f, 0x58f3c2)[phase]
               for r, (_, phase, _, _) in zip(environment_applied, expected_environment))
    assert all(r['event'] == 'disaster_protection_eligibility' and r['scene'] == 'environment' for r in environment_queries[:-1])
    assert environment_queries[-1]['scene'] == 'unclassified_query'
    harvest = rows('harvest_telemetry_test.csv')
    harvest_applied = [r for r in harvest if r['stage'] == 'applied']
    harvest_queries = [r for r in harvest if r['stage'] == 'check_matched']
    assert len(harvest_applied) == 9 and len(harvest_queries) == 39
    expected_harvest = [(14162 if mode == 2 else 600, 249 if mode == 1 else 98, 529 if layout else 8)
                        for layout in (0, 1) for mode in range(4) if layout or mode != 2] + [(14162, 98, 528), (600, 98, 529)]
    assert [(int(r['person_id']), int(r['owned_skill']), int(r['target_city'])) for r in harvest_applied] == expected_harvest
    assert all(r['effect_skill'] == '98' and r['scene'] == 'environment' and r['event'] == 'harvest_scheduled_from_priority_pool' and
               r['target_city'] == r['target_base'] and r['force_id'] == r['target_force'] == '2' and
               int(r['caller']) == 0x58f750 and json.loads(r['context_json']) == dict(harvest_kind=2,
                   pending_flags_before=0, pending_flags_after=4, priority_pool=True, preference_roll_percent=50,
                   counterfactual_evaluated=False) for r in harvest_applied)
    assert all(r['scene'] == 'environment' and r['event'] == 'harvest_preference_eligibility' for r in harvest_queries[:-1])
    assert harvest_queries[-1]['scene'] == 'unclassified_query'
    diplomacy = [r for r in rows('diplomacy_telemetry_test.csv') if r['event']!='negotiation_interaction_state_written']
    diplomacy_applied = [r for r in diplomacy if r['stage'] == 'applied']
    diplomacy_queries = [r for r in diplomacy if r['stage'] == 'check_matched']
    assert len(diplomacy_applied) == 258 and len(diplomacy_queries) == 306
    expected_diplomacy = []
    for kind in (16, 18, 20):
        for owned in (86, 249):
            for difficulty in (0, 2):
                for player in (0, 1):
                    for sample in (0, 1):
                        for interactive in (False, True):
                            for final in (0, 1, 2):
                                if difficulty == 2 and player and not sample:
                                    continue
                                expected_diplomacy.append((600, owned, 249 if difficulty else 3, 14162,
                                    dict(command_type=kind, initial_result_code=1, final_result_code=final if interactive else 1,
                                         interactive_resolution=interactive, target_role='force_ruler', counterfactual_evaluated=False)))
        expected_diplomacy.append((600, 86, -1, -1, dict(command_type=kind, initial_result_code=1, final_result_code=1,
            interactive_resolution=False, target_role='force_ruler', counterfactual_evaluated=False)))
        expected_diplomacy.append((14162, 86, 249, 1000, dict(command_type=kind, initial_result_code=1, final_result_code=2,
            interactive_resolution=True, target_role='force_ruler', counterfactual_evaluated=False)))
    assert [(int(r['person_id']), int(r['owned_skill']), int(r['target_force']), int(r['target_person']), json.loads(r['context_json']))
            for r in diplomacy_applied] == expected_diplomacy
    assert all(r['effect_skill'] == '86' and r['scene'] == 'diplomacy' and r['event'] == 'negotiation_skill_result_resolved' and
               r['force_id'] == '2' and r['target_city'] == r['target_base'] == '-1' and
               int(r['caller']) == {16: 0x5b2250, 18: 0x5b6510, 20: 0x5cc650}[context['command_type']]
               for r, (_, _, _, _, context) in zip(diplomacy_applied, expected_diplomacy))
    assert sum(r['scene'] == 'unclassified_query' for r in diplomacy_queries) == 3
    assert all(r['scene'] == 'unclassified_query' or (r['scene'] == 'diplomacy' and r['event'] == 'negotiation_skill_eligibility')
               for r in diplomacy_queries)
    diplomacy_owner=[r for r in rows('diplomacy_owner_edges_test.csv') if r['event']!='negotiation_interaction_state_written'];diplomacy_owner_index=0
    for kind in (16,18,20):
        for owned in (86,249):
            for fault in range(14):
                if fault==11 and owned==86:continue
                q=diplomacy_owner[diplomacy_owner_index];diplomacy_owner_index+=1
                assert (q['stage'],q['effect_skill'],q['person_id'],q['owned_skill'],q['event'])==(
                    'check_matched','86','600',str(owned),'negotiation_skill_eligibility')
                if fault==0:
                    r=diplomacy_owner[diplomacy_owner_index];diplomacy_owner_index+=1
                    assert (r['stage'],r['effect_skill'],r['person_id'],r['owned_skill'],r['target_force'],r['target_person'])==(
                        'applied','86','600',str(owned),'249','14162')
                    assert r['event']=='negotiation_skill_result_resolved'
                    assert json.loads(r['context_json'])==dict(command_type=kind,initial_result_code=1,final_result_code=2,
                        interactive_resolution=True,target_role='force_ruler',counterfactual_evaluated=False)
    assert diplomacy_owner_index==len(diplomacy_owner)==87
    construction = rows('construction_telemetry_test.csv')
    construction_applied = [r for r in construction if r['stage'] == 'applied']
    construction_queries = [r for r in construction if r['stage'] == 'check_matched']
    assert len(construction_applied) == 102 and len(construction_queries) == 106
    expected_construction = []
    for mode in range(4):
        for amount in (1, 88, 405):
            for completed in (0, 1):
                for before in (0, 900, 990, 1000):
                    expected_construction.append((14162 if mode == 2 else 600, 249 if mode == 1 else 77,
                        before, min(1000, before + 2 * amount), min(1000, before + amount),
                        dict(facility_kind=49, base_hp_amount=amount, requested_hp_amount=2 * amount, max_hp=1000, completed_before=completed)))
    for maximum, before, normal, after in ((0, 0, 0, 0), (32768, 0, -32768, -32768), (65535, 0, -1, -1),
                                          (32768, -32768, 0, 0), (1000, -1, 87, 175), (65535, 32760, 0, 0)):
        expected_construction.append((600, 77, before, after, normal,
            dict(facility_kind=49, base_hp_amount=88, requested_hp_amount=176, max_hp=maximum, completed_before=1)))
    assert [(int(r['person_id']), int(r['owned_skill']), int(r['before']), int(r['after']), int(r['baseline_after']),
             json.loads(r['context_json'])) for r in construction_applied] == expected_construction
    assert all(r['effect_skill'] == '77' and r['scene'] == 'construction' and r['metric'] == 'facility_hp' and
               r['event'] == ('facility_repair_hp_bonus' if context['completed_before'] else 'facility_construction_hp_bonus') and
               r['force_id'] == '2' and r['target_force'] == '3' and r['target_base'] == '9000' and r['target_city'] == '-1' and
               r['actor_unit'] == '4095' and r['target_unit'] == '-1' and int(r['caller']) == 0x5850cf
               for r, (_, _, _, _, _, context) in zip(construction_applied, expected_construction))
    assert sum(r['scene'] == 'unclassified_query' for r in construction_queries) == 2
    assert all(r['scene'] == 'unclassified_query' or (r['scene'] == 'construction' and r['event'] == 'construction_hp_eligibility')
               for r in construction_queries)
    construction_world = rows('construction_world_test.csv')
    construction_owner = rows('construction_owner_test.csv')
    assert len(construction_world) == 1 and construction_world[0]['stage'] == 'check_matched'
    assert construction_world[0]['event'] == 'construction_hp_eligibility'
    owner_applied = [r for r in construction_owner if r['stage'] == 'applied']
    owner_queries = [r for r in construction_owner if r['stage'] == 'check_matched']
    assert (len(owner_applied), len(owner_queries)) == (3, 21)
    assert [(int(r['person_id']), int(r['owned_skill']), int(r['before']), int(r['after']),
             int(r['baseline_after']), int(r['bonus_delta'])) for r in owner_applied] == [
                 (600, 77, 0, 176, 88, 88), (14162, 77, 0, 176, 88, 88), (600, 249, 0, 176, 88, 88)]
    assert all(r['effect_skill'] == '77' and r['event'] == 'facility_construction_hp_bonus' and
               r['actor_unit'] == '4095' and r['target_base'] == '9000' for r in owner_applied)
    assert [r['stage'] for r in construction_owner[:6]] == ['check_matched', 'applied'] * 3
    assert all(r['stage'] == 'check_matched' for r in construction_owner[6:])
    assert all(r['effect_skill'] == '77' for r in construction_world + owner_queries)
    critical = rows('critical_telemetry_test.csv')
    critical_applied = [r for r in critical if r['stage'] == 'applied']
    critical_queries = [r for r in critical if r['stage'] == 'check_matched']
    assert len(critical_applied) == 131 and len(critical_queries) == 149
    critical_effects = [0, 15, 23, 25] + list(range(34, 49))
    def critical_weapon(effect):
        return 2 if effect in (35, 43) else 3 if effect in (36, 44) else 4 if effect in (25, 37, 45) else 8 if effect == 46 else 10 if effect in (38, 47) else 1
    def critical_tactic(effect):
        return -1 if effect in (15, 23, 25, 40) else 14 if effect == 46 else 16 if effect in (38, 47) else 0
    expected_critical = [(600, effect, owned, before, max(0, before - 250), critical_weapon(effect), critical_tactic(effect))
                         for effect in critical_effects for owned in (effect, 249) for before in (0, 100, 1000)]
    expected_critical += [(14162, 34, 34, 1000, 750, 1, 0), (14162, 34, 34, 1000, 750, 1, 0),
                          (14162, 41, 41, 1000, 750, 1, 0), (14162, 48, 48, 1000, 750, 1, 0),
                          (600, 42, 249, 1000, 750, 1, 0), (14162, 34, 34, 1000, 750, 1, 0),
                          (600, 48, 48, 1000, 750, 1, 0), (600, 42, 42, 1000, 750, 1, 0), (600, 42, 42, 750, 500, 1, 0)]
    expected_critical += ([(600, 42, 42, 1000, 750, 1, 0)] * 3 + [(600, 42, 42, 750, 500, 1, 0)]) * 2
    lifecycle = rows('critical_lifecycle_test.csv')
    lifecycle_applied = [r for r in lifecycle if r['stage'] == 'applied']
    lifecycle_queries = [r for r in lifecycle if r['stage'] == 'check_matched']
    session = rows('critical_session_test.csv')
    session_applied = [r for r in session if r['stage'] == 'applied']
    session_queries = [r for r in session if r['stage'] == 'check_matched']
    assert (len(lifecycle_applied), len(lifecycle_queries), len(session_applied), len(session_queries)) == (11, 1036, 2, 12)
    assert all(r['effect_skill'] == r['owned_skill'] == '42' and r['person_id'] == '600' and
               r['scene'] == 'battle' and r['event'] == 'critical_eligibility' for r in lifecycle_queries + session_queries)
    expected_critical += [(600, 42, 42, 1000, 750, 1, 0)] * 3
    expected_critical += ([(600, 42, 42, 1000, 750, 1, 0)] * 3 + [(600, 42, 42, 750, 500, 1, 0)]) * 2
    expected_critical += [(600, 42, 42, 1000, 750, 1, 0)] * 2
    checked_critical = critical_applied + lifecycle_applied + session_applied
    assert [(int(r['person_id']), int(r['effect_skill']), int(r['owned_skill']), int(r['before']), int(r['after']),
             int(r['equipment_id']), json.loads(r['context_json'])['tactic_id']) for r in checked_critical] == expected_critical
    assert all(r['scene'] == 'battle' and r['event'] == 'forced_critical_attack_settled' and r['metric'] == 'troops' and
               r['force_id'] == '2' and r['target_force'] == '3' and r['target_unit'] == '4094' and r['actor_unit'] == '4095' and
               r['target_x'] == '2' and r['target_y'] == '1' and r['target_base'] == r['target_city'] == '-1' and
               not r['baseline_after'] and not r['bonus_delta'] and int(r['caller']) == 0x584fdf and
               json.loads(r['context_json']) == dict(tactic_id=tactic, record_kind=0, critical=True, requested_troop_loss=250,
                   damage_attribution='whole_attack_not_marginal_skill_bonus', counterfactual_evaluated=False)
               for r, (_, _, _, _, _, _, tactic) in zip(checked_critical, expected_critical))
    assert sum(r['scene'] == 'unclassified_query' for r in critical_queries) == 1
    assert all(r['scene'] == 'unclassified_query' or (r['scene'] == 'battle' and r['event'] == 'critical_eligibility') for r in critical_queries)
    critical_targets = rows('critical_targets_test.csv')
    target_applied = [r for r in critical_targets if r['stage'] == 'applied']
    target_queries = [r for r in critical_targets if r['stage'] == 'check_matched']
    assert (len(target_applied), len(target_queries)) == (918, 586)
    expected_targets = []
    # Independent expected numeric/resource/target matrix for the three native
    # common-apply CALL sites. HP and garrison troops remain separate observations.
    def target_expected(effect, owned, before, after, base, city, unit, metric, requested, caller):
        return (effect, owned, before, after, base, city, unit, metric, requested, caller,
                -1 if effect == 17 else critical_tactic(effect), critical_weapon(effect))
    for effect in critical_effects:
        for owned in (effect, 249):
            for before in (0, 100, 1000):
                expected_targets.append(target_expected(effect, owned, before, max(0, before - 250), -1, -1, 4094,
                                                        'troops', 250, 0x5b1c6c))
    building_effects = [0, 17, 23, 25] + list(range(34, 49))
    for base, city in ((8, 8), (45, -1), (60, -1)):
        for effect in building_effects:
            for owned in (effect, 249):
                for before in (0, 100, 1000):
                    expected_targets.append(target_expected(effect, owned, before, max(0, before - 250), base, city, -1,
                                                            'troops', 250, 0x5b1a74))
                    expected_targets.append(target_expected(effect, owned, before, max(0, before - 125), base, city, -1,
                                                            'facility_hp', 125, 0x5b1aaf))
    for base, city, cap in ((8, 8, 100000), (45, -1, 30000), (60, -1, 30000)):
        expected_targets.append(target_expected(17, 17, cap + 1, cap, base, city, -1, 'troops', 0, 0x5b1a74))
        expected_targets.append(target_expected(17, 17, 1001, 1000, base, city, -1, 'facility_hp', 0, 0x5b1aaf))
    for effect in building_effects:
        for owned in (effect, 249):
            for before in (0, 100, 1000):
                expected_targets.append(target_expected(effect, owned, before, max(0, before - 125), 9000, -1, -1,
                                                        'facility_hp', 125, 0x5b1aaf))
    assert len(expected_targets) == 918
    for r, expected in zip(target_applied, expected_targets):
        effect, owned, before, after, base, city, unit, metric, requested, caller, tactic, weapon = expected
        assert (int(r['effect_skill']), int(r['owned_skill']), int(r['before']), int(r['after']),
                int(r['target_base']), int(r['target_city']), int(r['target_unit']), r['metric'], int(r['caller']),
                int(r['equipment_id'])) == (effect, owned, before, after, base, city, unit, metric, caller, weapon)
        assert r['person_id'] == '600' and r['force_id'] == '2' and r['target_force'] == '3' and r['actor_unit'] == '4095'
        assert r['scene'] == 'battle' and r['event'] == 'forced_critical_attack_settled'
        assert r['target_x'] == '2' and r['target_y'] == '1' and not r['baseline_after'] and not r['bonus_delta']
        request_key = 'requested_hp_loss' if metric == 'facility_hp' else 'requested_troop_loss'
        assert json.loads(r['context_json']) == dict(tactic_id=tactic, record_kind=0, critical=True,
            damage_attribution='whole_attack_not_marginal_skill_bonus', counterfactual_evaluated=False, **{request_key: requested})
    assert all(r['scene'] == 'battle' and r['event'] == 'critical_eligibility' for r in target_queries)
    critical_owner = rows('critical_owner_test.csv')
    assert len(critical_owner) == 32 and all(r['stage'] == 'check_matched' and r['scene'] == 'battle' and
        r['event'] == 'critical_eligibility' and r['person_id'] == '600' and r['owned_skill'] == r['effect_skill'] == '23'
        for r in critical_owner)
    position_owners = rows('critical_position_owners_test.csv')
    position_applied = [r for r in position_owners if r['stage'] == 'applied']
    position_queries = [r for r in position_owners if r['stage'] == 'check_matched']
    assert (len(position_applied), len(position_queries)) == (64, 96)
    expected_position = [(42, 1000, 750, 0), (23, 750, 500, -1)] * 16
    expected_position += [(42, 1000, 750, 0)] * 16
    expected_position += [(23, 1000, 750, -1), (23, 750, 500, -1)] * 8
    for r, (effect, before, after, tactic) in zip(position_applied, expected_position):
        assert (int(r['effect_skill']), int(r['before']), int(r['after'])) == (effect, before, after)
        assert r['owned_skill'] == '249' and r['person_id'] == '600' and r['force_id'] == '2'
        assert r['scene'] == 'battle' and r['event'] == 'forced_critical_attack_settled' and r['metric'] == 'troops'
        assert r['target_force'] == '3' and r['actor_unit'] == '4095' and r['target_unit'] == '4094'
        assert r['target_x'] == '2' and r['target_y'] == '1' and r['equipment_id'] == '1' and int(r['caller']) == 0x584fdf
        assert not r['baseline_after'] and not r['bonus_delta']
        assert json.loads(r['context_json']) == dict(tactic_id=tactic, record_kind=0, critical=True, requested_troop_loss=250,
            damage_attribution='whole_attack_not_marginal_skill_bonus', counterfactual_evaluated=False)
    assert all(r['scene'] == 'battle' and r['event'] == 'critical_eligibility' and r['owned_skill'] == '249' and
               r['person_id'] == '600' for r in position_queries)
    assert [sum(r['effect_skill'] == str(effect) for r in position_queries) for effect in (17, 23, 42)] == [32, 32, 32]

    strategy_cost = rows('strategy_cost_test.csv')
    strategy_applied = [r for r in strategy_cost if r['stage'] == 'applied']
    strategy_queries = [r for r in strategy_cost if r['stage'] == 'check_matched']
    assert (len(strategy_applied), len(strategy_queries)) == (505, 515)
    normal_costs = (10, 10, 15, 15, 10, 10, 20, 50, 50)
    expected_strategy = []
    for cap in (100, 120):
        for plan, normal in enumerate(normal_costs):
            for before in (0, 1, 5, 10, 100, 120, 255):
                for person, owned in ((600, 64), (14162, 64), (600, 249), (14162, 249)):
                    expected_strategy.append((person, owned, before, min(cap, max(0, before - 1)),
                                              min(cap, max(0, before - normal)), plan, normal))
    for r, (person, owned, before, after, baseline, plan, normal) in zip(strategy_applied[:-1], expected_strategy):
        assert (int(r['person_id']), int(r['owned_skill']), int(r['before']), int(r['after']), int(r['baseline_after'])) == (
            person, owned, before, after, baseline)
        assert json.loads(r['context_json']) == dict(strategy_id=plan, normal_energy_cost=normal,
            requested_energy_cost=1, counterfactual_evaluated=True)
    assert len(expected_strategy) == 504
    tail = strategy_applied[-1]
    assert (tail['person_id'], tail['owned_skill'], tail['before'], tail['after'], tail['baseline_after'], tail['bonus_delta']) == (
        '600', '64', '10', '9', '', '')
    assert json.loads(tail['context_json']) == dict(strategy_id=0, normal_energy_cost=0, requested_energy_cost=1,
                                                counterfactual_evaluated=False)
    assert all(r['effect_skill'] == '64' and r['scene'] == 'strategy' and r['event'] == 'strategy_energy_cost_override' and
        r['metric'] == 'energy' and r['force_id'] == r['target_force'] == '2' and r['actor_unit'] == r['target_unit'] == '4095' and
        r['target_base'] == r['target_city'] == '-1' and r['equipment_id'] == '4' and
        r['target_x'] == r['target_y'] == '1' and int(r['caller']) == 0x593445 for r in strategy_applied)
    assert sum(r['scene'] == 'unclassified_query' for r in strategy_queries) == 5
    assert all(r['effect_skill'] == '64' and (r['scene'] == 'unclassified_query' or (
        r['scene'] == 'strategy' and r['event'] == 'energy_cost_eligibility')) for r in strategy_queries)
    after_attack = rows('after_attack_test.csv')
    after_applied = [r for r in after_attack if r['stage'] == 'applied']
    after_queries = [r for r in after_attack if r['stage'] == 'check_matched']
    assert (len(after_applied), len(after_queries)) == (126, 190)
    expected_after = []
    def after_expected(effect, person, owned, victim=-1, before=None, after=None, context=None):
        return dict(effect=effect, person=person, owned=owned, victim=victim, before=before, after=after, context=context)
    for person, owned in ((600, 49), (14162, 49), (600, 249), (14162, 249)):
        for status in (0, 1, 2):
            for before in (0, 4, 5):
                for turns in (1, 2):
                    expected_after.append(after_expected(49, person, owned, before=before, after=min(5, before + turns),
                        context=dict(status_before=status, status_after=1, requested_turns=turns,
                                     attacking_power=100, defending_power=70, counterfactual_evaluated=False)))
    for person, owned in ((600, 51), (14162, 51), (600, 249), (14162, 249)):
        for level in (0, 1, 2):
            for victim in (1000, 601):
                expected_after.append(after_expected(51, person, owned, victim, level, level + 1,
                    dict(requested_injury_level=level + 1, counterfactual_evaluated=False)))
    protected_context = dict(attacking_unit=4095, attacking_force=2, selection_scope='mighty_after_attack',
                             counterfactual_evaluated=False)
    for source_owned in (51, 249):
        for guard_owned in (52, 249):
            expected_after.append(after_expected(52, 1000, guard_owned, context=protected_context))
    for source_owned in (51, 249):
        for protected_owned in (32, 249):
            for mask in (1, 2, 3):
                if mask != 3:
                    expected_after.append(after_expected(51, 600, source_owned, 601 if mask == 1 else 1000, 0, 1,
                        dict(requested_injury_level=1, counterfactual_evaluated=False)))
                for flag, person in ((1, 1000), (2, 601)):
                    if mask & flag:
                        expected_after.append(after_expected(32, person, protected_owned, person, context=protected_context))
    for guard_owned in (52, 249):
        expected_after.append(after_expected(52, 1000, guard_owned, context=protected_context))
    assert len(expected_after) == 126
    event_names = {49: 'dashing_status_applied', 51: 'mighty_injury_level_increased',
                   52: 'injury_selection_blocked', 32: 'injury_candidate_excluded'}
    after_callers = {49: 0x593be1, 51: 0x59775e, 52: 0x597220, 32: 0x59729e}
    for r, expected in zip(after_applied, expected_after):
        effect = expected['effect']
        defensive = effect in (32, 52)
        assert (int(r['effect_skill']), int(r['person_id']), int(r['owned_skill']), int(r['target_person'])) == (
            effect, expected['person'], expected['owned'], expected['victim'])
        assert r['scene'] == 'battle' and r['event'] == event_names[effect] and int(r['caller']) == after_callers[effect]
        assert r['force_id'] == ('3' if defensive else '2') and r['actor_city'] == ('60' if defensive else '8')
        assert r['target_force'] == '3' and r['target_unit'] == '4094' and r['target_base'] == r['target_city'] == '-1'
        assert r['actor_unit'] == ('4094' if defensive else '4095') and r['equipment_id'] == ('2' if defensive else '4')
        assert r['target_x'] == '2' and r['target_y'] == '1' and not r['baseline_after'] and not r['bonus_delta']
        assert json.loads(r['context_json']) == expected['context'], (r['seq'], r['context_json'], expected['context'])
        if defensive:
            assert not r['metric'] and not r['before'] and not r['after'] and not r['delta']
        else:
            assert r['metric'] == ('status_turns' if effect == 49 else 'injury_level')
            assert (int(r['before']), int(r['after'])) == (expected['before'], expected['after'])
    assert [sum(r['effect_skill'] == str(effect) and r['scene'] == 'battle' for r in after_queries)
            for effect in (49, 51, 52, 32)] == [79, 88, 6, 16]
    assert sum(r['scene'] == 'unclassified_query' for r in after_queries) == 1
    assert all(r['scene'] == 'unclassified_query' or (r['scene'] == 'battle' and r['event'] ==
        ('after_attack_eligibility' if int(r['effect_skill']) in (49, 51) else 'injury_protection_eligibility'))
        for r in after_queries)
    immunity = rows('strategy_immunity_test.csv')
    immune_applied = [r for r in immunity if r['stage'] == 'applied']
    immune_queries = [r for r in immunity if r['stage'] == 'check_matched']
    assert (len(immune_applied), len(immune_queries)) == (62, 69)
    expected_immune = []
    providers = ((1000, None), (14162, None), (1000, 249), (14162, 249))
    for plan in (2, 3):
        for effect in (61, 72 if plan == 2 else 73, 74):
            for person, owned in providers:
                expected_immune.append((plan, effect, person, effect if owned is None else owned))
    for plan in range(8):
        for person, owned in providers:
            expected_immune.append((plan, 61, person, 61 if owned is None else owned))
    for plan in (2, 3):
        expected_immune.extend(((plan, 72 if plan == 2 else 73, 1000, 72 if plan == 2 else 73),
                                (plan, 61, 1000, 249), (plan, 74, 14162, 74)))
    assert len(expected_immune) == len(immune_applied)
    for r, (plan, effect, person, owned) in zip(immune_applied, expected_immune):
        assert (int(r['effect_skill']), int(r['person_id']), int(r['owned_skill'])) == (effect, person, owned)
        assert r['scene'] == 'strategy' and r['event'] == 'strategy_immunity_forced_failure' and int(r['caller']) == 0x593402
        assert r['force_id'] == r['target_force'] == '3' and r['actor_city'] == '60'
        assert r['actor_unit'] == r['target_unit'] == '4094' and r['equipment_id'] == '2'
        assert r['target_x'] == '2' and r['target_y'] == '1' and r['target_person'] == r['target_base'] == r['target_city'] == '-1'
        assert not r['metric'] and not r['before'] and not r['after'] and not r['delta'] and not r['baseline_after'] and not r['bonus_delta']
        assert json.loads(r['context_json']) == dict(strategy_id=plan, attacking_unit=4095, attacking_force=2,
            success_chance=0, native_immunity_flag=1, counterfactual_evaluated=False)
    assert [sum(r['effect_skill'] == str(effect) and r['scene'] == 'strategy' for r in immune_queries)
            for effect in (61, 72, 73, 74)] == [42, 11, 5, 10]
    assert sum(r['scene'] == 'unclassified_query' for r in immune_queries) == 1
    assert all(r['scene'] == 'unclassified_query' or r['event'] == 'immunity_eligibility' for r in immune_queries)
    maxima = rows('strategy_maximum_test.csv')
    max_applied = [r for r in maxima if r['stage'] == 'applied']
    max_queries = [r for r in maxima if r['stage'] == 'check_matched']
    assert (len(max_applied), len(max_queries)) == (205, 321)
    native_maxima = [
        (0, 60, True, 0x5aebd4), (0, 63, True, 0x5aebe7), (0, 53, False, 0x5aebfe),
        (0, 57, False, 0x5aec0d), (0, 62, False, 0x5aec1c), (0, 63, False, 0x5aec2b),
        (1, 53, False, 0x5aec4f), (1, 57, False, 0x5aec5e), (1, 62, False, 0x5aec6d), (1, 63, False, 0x5aec7c),
        (2, 54, False, 0x5aed28), (2, 57, False, 0x5aed3d), (2, 63, False, 0x5aed52),
        (2, 60, True, 0x5aed69), (2, 63, True, 0x5aed7c),
        (3, 55, False, 0x5aeeaa), (3, 57, False, 0x5aeebf), (3, 63, False, 0x5aeed4),
        (3, 60, True, 0x5aeeeb), (3, 63, True, 0x5aeefa),
        (4, 57, False, 0x5af012), (4, 63, False, 0x5af025),
        (5, 57, False, 0x5af0cb), (5, 63, False, 0x5af0e0), (5, 60, True, 0x5af0f7), (5, 63, True, 0x5af10a),
        (6, 56, False, 0x5af216), (6, 57, False, 0x5af22b), (6, 63, False, 0x5af240),
        (6, 60, True, 0x5af257), (6, 63, True, 0x5af266)]
    expected_maxima = []
    for plan, effect, defensive, site in native_maxima:
        first, second = (1000, 601) if defensive else (600, 14162)
        for person, owned, value in ((first, effect, 110), (second, effect, 110), (first, 249, 110),
                                     (second, 249, 110), (second, effect, 110), (first, effect, 120)):
            expected_maxima.append((plan, effect, defensive, site, person, owned, value))
    for effect in (53, 54, 55, 56, 57, 62):
        plan = 0 if effect in (53, 62) else {54: 2, 55: 3, 56: 6, 57: 4}[effect]
        site = {0: 0x5aec2b, 2: 0x5aed52, 3: 0x5aeed4, 4: 0x5af025, 6: 0x5af240}[plan]
        for value in (69, 70):
            expected_maxima.append((plan, 63, False, site, 14162, 63, 110))
    expected_maxima.extend([(0, 57, False, 0x5aec0d, 600, 57, 110)] * 4)
    expected_maxima.extend(((0, 60, True, 0x5aebd4, 1000, 60, 110), (2, 54, False, 0x5aed28, 600, 54, 110)))
    assert len(expected_maxima) == 204
    for r, (plan, effect, defensive, site, person, owned, value) in zip(max_applied[:-1], expected_maxima):
        assert (int(r['effect_skill']), int(r['person_id']), int(r['owned_skill'])) == (effect, person, owned)
        assert r['scene'] == 'strategy' and r['event'] == ('strategy_stat_forced_failure' if defensive else 'strategy_stat_forced_success')
        assert int(r['caller']) == 0x593402 and r['force_id'] == ('3' if defensive else '2')
        assert r['actor_city'] == ('60' if defensive else '8') and r['target_force'] == '3'
        assert r['actor_unit'] == ('4094' if defensive else '4095') and r['target_unit'] == '4094'
        assert r['equipment_id'] == ('2' if defensive else '4') and r['target_x'] == '2' and r['target_y'] == '1'
        assert r['target_person'] == r['target_base'] == r['target_city'] == '-1'
        assert all(not r[key] for key in ('metric', 'before', 'after', 'delta', 'baseline_after', 'bonus_delta'))
        assert json.loads(r['context_json']) == dict(strategy_id=plan, attacking_unit=4095, attacking_force=2,
            stat_id=2, provider_value=value, opponent_intelligence=100 if defensive else 70, success_chance=0 if defensive else 100,
            native_immunity_flag=1 if defensive else 0, decision_site=site, counterfactual_evaluated=False)
    assert max_applied[-1]['event'] == 'strategy_immunity_forced_failure' and max_applied[-1]['effect_skill'] == '61'
    assert max_applied[-1]['person_id'] == '1000'
    assert [sum(r['effect_skill'] == str(effect) and r['event'] == 'stat_effect_eligibility' for r in max_queries)
            for effect in (53, 54, 55, 56, 57, 60, 62, 63)] == [20, 18, 11, 11, 69, 48, 20, 122]
    assert sum(r['scene'] == 'unclassified_query' for r in max_queries) == 1
    assert sum(r['event'] == 'immunity_eligibility' for r in max_queries) == 1
    strategy_critical = rows('strategy_critical_test.csv')
    strat_crit_applied = [r for r in strategy_critical if r['stage'] == 'applied']
    strat_crit_queries = [r for r in strategy_critical if r['stage'] == 'check_matched']
    assert (len(strat_crit_applied), len(strat_crit_queries)) == (177, 234)
    expected_strat_crit = []
    def strat_crit_providers(plan, effect):
        stat = effect in (58, 59)
        for index, (person, owned) in enumerate(((600, effect), (14162, effect), (600, 249),
                                                (14162, 249), (14162 if stat else 600, effect), (600, effect))):
            value = (60 if effect == 59 else 120) if index == 5 else (40 if effect == 59 else 110)
            expected_strat_crit.append((plan, effect, person, owned, value if stat else -1))
    for plan in (0, 2, 3, 4, 5, 6):
        for effect in (63, 67, 58, 59):
            strat_crit_providers(plan, effect)
    strat_crit_providers(5, 16)
    expected_strat_crit.extend(((0, 58, 600, 58, 71), (0, 59, 600, 59, 0)))
    for plan in (0, 2, 3, 4, 5, 6):
        for effect, value in ((63, -1), (67, -1), (58, 110), (59, 69)):
            expected_strat_crit.append((plan, effect, 600, 249, value))
        if plan == 5:
            expected_strat_crit.append((5, 16, 600, 249, -1))
    assert len(expected_strat_crit) == 177
    for r, (plan, effect, person, owned, value) in zip(strat_crit_applied, expected_strat_crit):
        assert (int(r['effect_skill']), int(r['person_id']), int(r['owned_skill'])) == (effect, person, owned)
        assert r['scene'] == 'strategy' and r['event'] == 'strategy_forced_critical_decision' and int(r['caller']) == 0x593424
        assert r['force_id'] == '2' and r['actor_city'] == '8' and r['target_force'] == '3'
        assert r['actor_unit'] == '4095' and r['target_unit'] == '4094' and r['equipment_id'] == '4'
        assert r['target_person'] == r['target_base'] == r['target_city'] == '-1' and r['target_x'] == '2' and r['target_y'] == '1'
        assert all(not r[key] for key in ('metric', 'before', 'after', 'delta', 'baseline_after', 'bonus_delta'))
        assert json.loads(r['context_json']) == dict(strategy_id=plan, critical_chance=100, critical_result=True,
            stat_id=2 if effect in (58, 59) else -1, provider_value=value,
            opponent_intelligence=70 if effect in (58, 59) else -1, counterfactual_evaluated=False)
    assert [sum(r['effect_skill'] == str(effect) and r['event'] == 'critical_effect_eligibility' for r in strat_crit_queries)
            for effect in (16, 58, 59, 63, 67)] == [7, 76, 66, 42, 42]
    assert sum(r['scene'] == 'unclassified_query' for r in strat_crit_queries) == 1
    fire = rows('strategy_fire_test.csv')
    fire_applied = [r for r in fire if r['stage'] == 'applied']
    fire_queries = [r for r in fire if r['stage'] == 'check_matched']
    fire_writes = [r for r in fire if r['event'] == 'strategy_critical_fire_turns_written']
    assert (len(fire_applied), len(fire_queries), len(fire_writes)) == (897, 607, 296)
    assert sum(r['event'] == 'strategy_stat_forced_success' for r in fire_applied) == 301
    assert sum(r['event'] == 'strategy_forced_critical_decision' for r in fire_applied) == 300
    fire_index = 0
    for effect in (63, 67, 58, 59):
        for provider in range(4):
            person, helper = (14162, 600) if provider & 1 else (600, 14162)
            owned = effect if provider < 2 else 249
            for mode in (0, 1, 2):
                for dispatched in (False, True):
                    for sample, seed in enumerate((0, 4, 1)):
                        group = fire[fire_index:fire_index + 5]
                        fire_index += 5
                        assert [r['event'] for r in group] == ['stat_effect_eligibility', 'strategy_stat_forced_success',
                            'critical_effect_eligibility', 'strategy_forced_critical_decision', 'strategy_critical_fire_turns_written']
                        assert [int(r['person_id']) for r in group] == [helper, helper, person, person, person]
                        assert [int(r['effect_skill']) for r in group] == [53, 53, effect, effect, effect]
                        assert [int(r['owned_skill']) for r in group] == [53, 53, owned, owned, owned]
                        assert all(r['scene'] == 'strategy' for r in group)
                        assert json.loads(group[1]['context_json']) == dict(strategy_id=0, attacking_unit=4095, attacking_force=2,
                            stat_id=2, provider_value=110, opponent_intelligence=70, success_chance=100,
                            native_immunity_flag=0, decision_site=0x5aebfe, counterfactual_evaluated=False)
                        stat = effect in (58, 59)
                        assert json.loads(group[3]['context_json']) == dict(strategy_id=0, critical_chance=100, critical_result=True,
                            stat_id=2 if stat else -1, provider_value=(40 if effect == 59 else 110) if stat else -1,
                            opponent_intelligence=70 if stat else -1, counterfactual_evaluated=False)
                        assert (group[4]['thread'] != group[3]['thread']) == dispatched
                        assert len({r['thread'] for r in group[:4]}) == 1
                        value = seed
                        for _ in range(3):
                            value = (value * 0x6c078965 + 0x3039) & 0xffffffff
                        turns = 2 + (value >> 16) % 3
                        assert turns == sample + 2
                        r = group[4]
                        assert json.loads(r['context_json']) == dict(strategy_id=0, critical_result=True, requested_fire_turns=turns,
                            command_mode=mode, fire_packet_mode=1, map_width=200, map_height=200, native_stride=200,
                            counterfactual_evaluated=False)
                        assert (int(r['before']), int(r['after'])) == ((0, 4, 7)[sample], turns)
    assert fire_index == 1440
    tail_events = []
    for reason in range(8):
        tail_events += ['stat_effect_eligibility']
        if reason not in (4, 5, 6):
            tail_events += ['strategy_stat_forced_success']
        tail_events += ['critical_effect_eligibility']
        if reason not in (4, 5, 6):
            tail_events += ['strategy_forced_critical_decision']
    tail_events += ['stat_effect_eligibility', 'strategy_stat_forced_success', 'critical_effect_eligibility',
        'strategy_forced_critical_decision', 'strategy_critical_fire_turns_written', 'strategy_critical_fire_turns_written',
        'stat_effect_eligibility', 'strategy_stat_forced_success']
    assert [r['event'] for r in fire[1440:1474]] == tail_events
    assert [(r['before'], r['after']) for r in fire_writes[288:290]] == [('7', '2'), ('2', '2')]
    for index, (mode, dispatched) in enumerate((m, d) for m in (0, 1, 2) for d in (False, True)):
        group = fire[1474 + index * 5:1479 + index * 5]
        assert [r['event'] for r in group] == ['stat_effect_eligibility', 'strategy_stat_forced_success',
            'critical_effect_eligibility', 'strategy_forced_critical_decision', 'strategy_critical_fire_turns_written']
        assert [r['person_id'] for r in group] == ['14162', '14162', '600', '600', '600']
        assert group[4]['before'] == '7' and group[4]['after'] == '4'
        assert (group[4]['thread'] != group[3]['thread']) == dispatched
        assert json.loads(group[4]['context_json']) == dict(strategy_id=0, critical_result=True, requested_fire_turns=4,
            command_mode=mode, fire_packet_mode=1, map_width=249, map_height=530, native_stride=249,
            counterfactual_evaluated=False)
    for r in fire_applied:
        assert r['force_id'] == '2' and r['actor_city'] == '8' and r['target_force'] == '3'
        assert r['actor_unit'] == '4095' and r['target_unit'] == '4094' and r['equipment_id'] == '4'
        assert r['target_person'] == r['target_base'] == r['target_city'] == '-1'
        assert (r['target_x'], r['target_y']) == (('250', '230') if int(r['seq']) > 1474 else ('2', '1'))
        assert not r['baseline_after'] and not r['bonus_delta']
        if r['event'] == 'strategy_critical_fire_turns_written':
            assert r['metric'] == 'fire_turns' and int(r['caller']) == 0x5924cb and r['terrain_id'] == '0'
        else:
            assert not r['metric'] and not r['before'] and not r['after'] and not r['delta']
    assert [sum(r['event'] == event for r in fire_queries) for event in ('stat_effect_eligibility', 'critical_effect_eligibility')] == [304, 303]
    followup = rows('strategy_followup_test.csv')
    followup_applied = [r for r in followup if r['stage'] == 'applied']
    followup_queries = [r for r in followup if r['stage'] == 'check_matched']
    followup_results = [r for r in followup if r['event'] in ('strategy_chain_subcommand_resolved', 'strategy_reflected_subcommand_resolved')]
    assert (len(followup_applied), len(followup_queries), len(followup_results)) == (217, 224, 113)
    expected_followup = []
    positive_events = []
    for mode, effect, count in ((2, 66, 5), (1, 68, 9)):
        for provider in range(4):
            person = ((600, 14162) if mode == 2 else (1000, 601))[provider & 1]
            for plan in range(count):
                for success in (True, False):
                    expected_followup.append((mode, effect, person, effect if provider < 2 else 249, plan, int(success)))
                    positive_events.append('followup_eligibility')
                    if success and plan < 7:
                        positive_events += ['stat_effect_eligibility', 'strategy_stat_forced_success']
                    elif not success and plan != 8:
                        positive_events += ['immunity_eligibility', 'strategy_immunity_forced_failure']
                    positive_events.append('strategy_chain_subcommand_resolved' if mode == 2 else 'strategy_reflected_subcommand_resolved')
    expected_followup.append((2, 66, 600, 66, 0, 1)) # target owner changes to4 after native command; snapshot remains3
    assert [r['event'] for r in followup[:424]] == positive_events
    assert [r['event'] for r in followup[424:]] == [
        'followup_eligibility', 'stat_effect_eligibility',
        'followup_eligibility', 'stat_effect_eligibility',
        'followup_eligibility', 'stat_effect_eligibility', 'strategy_stat_forced_success',
        'followup_eligibility', 'stat_effect_eligibility', 'strategy_stat_forced_success',
        'followup_eligibility', 'stat_effect_eligibility', 'strategy_stat_forced_success',
        'followup_eligibility', 'stat_effect_eligibility', 'strategy_stat_forced_success', 'strategy_chain_subcommand_resolved']
    for r, (mode, effect, person, owned, plan, result) in zip(followup_results, expected_followup):
        chain = mode == 2
        assert (int(r['effect_skill']), int(r['person_id']), int(r['owned_skill'])) == (effect, person, owned)
        assert r['scene'] == 'strategy' and int(r['caller']) == 0x593a75
        assert r['force_id'] == ('2' if chain else '3') and r['actor_city'] == ('8' if chain else '60')
        assert r['target_force'] == ('3' if chain else '2') and r['actor_unit'] == ('4095' if chain else '4094')
        assert r['target_unit'] == ('4094' if chain else '4095') and r['equipment_id'] == ('4' if chain else '2')
        assert (r['target_x'], r['target_y']) == (('2', '1') if chain else ('1', '1'))
        assert r['target_person'] == r['target_base'] == r['target_city'] == '-1'
        assert all(not r[key] for key in ('metric', 'before', 'after', 'delta', 'baseline_after', 'bonus_delta'))
        assert json.loads(r['context_json']) == dict(strategy_id=plan, command_mode=mode, native_result=result,
            source_force_at_start=2 if chain else 3, target_snapshot='before_execution', counterfactual_evaluated=False)
    assert [sum(r['effect_skill'] == str(effect) and r['event'] == 'followup_eligibility' for r in followup_queries)
            for effect in (66, 68)] == [46, 72]
    assert all(int(r['caller']) == (0x5936a0 if r['effect_skill'] == '66' else 0x5939e2)
               for r in followup_queries if r['event'] == 'followup_eligibility')
    assert sum(r['event'] == 'strategy_stat_forced_success' for r in followup_applied) == 52
    assert sum(r['event'] == 'strategy_immunity_forced_failure' for r in followup_applied) == 52
    charm = rows('strategy_charm_test.csv')
    charm_applied = [r for r in charm if r['stage'] == 'applied']
    charm_queries = [r for r in charm if r['stage'] == 'check_matched']
    charm_effects = [r for r in charm if r['event'] == 'strategy_charm_probability_doubled']
    assert (len(charm_applied), len(charm_queries), len(charm_effects)) == (237, 693, 236)
    charm_bases = ((1, 15, 56, 99), (34, 73, 99, 99), (1, 1, 45, 99), (1, 1, 55, 99),
                   (34, 73, 99, 99), (1, 1, 49, 99), (1, 1, 45, 99))
    expected_charm = []
    charm_index = 0
    for plan, bases in enumerate(charm_bases):
        for before in bases:
            for person, owned in ((600, 69), (14162, 69), (600, 249), (14162, 249)):
                for gender in (0, 1, 2):
                    for seed in (0, 4):
                        query = charm[charm_index]
                        charm_index += 1
                        assert query['stage'] == 'check_matched' and query['event'] == 'charm_eligibility'
                        assert (int(query['person_id']), int(query['owned_skill'])) == (person, owned)
                        if gender == 0:
                            assert charm[charm_index]['event'] == 'strategy_charm_probability_doubled'
                            charm_index += 1
                            after = min(100, before * 2)
                            sample = (((seed * 0x6c078965 + 0x3039) & 0xffffffff) >> 16) % 100
                            expected_charm.append((plan, person, owned, before, after, int(sample < after)))
    assert charm_index == 896
    for person, owned in ((600, 69), (14162, 69), (600, 249), (14162, 249)):
        expected_charm.extend([(5, person, owned, 50, 100, 1)] * 2)
    expected_charm.extend([(0, 600, 69, 56, 100, 1)] * 3) # source gender, stock high-ID gate, sparse-slot native behavior
    expected_charm.append((0, 600, 249, 15, 30, 0)) # two holders still use native first provider and a single doubling
    assert len(expected_charm) == 236
    for r, (plan, person, owned, before, after, result) in zip(charm_effects, expected_charm):
        assert r['scene'] == 'strategy' and r['effect_skill'] == '69' and r['metric'] == 'success_chance_percent'
        assert (int(r['person_id']), int(r['owned_skill']), int(r['before']), int(r['after'])) == (person, owned, before, after)
        assert int(r['baseline_after']) == before and int(r['bonus_delta']) == after - before and int(r['caller']) == 0x5af49e
        assert r['force_id'] == '2' and r['actor_city'] == '8' and r['target_force'] == '3'
        assert r['actor_unit'] == '4095' and r['target_unit'] == '4094' and r['equipment_id'] == '4'
        assert r['target_person'] == r['target_base'] == r['target_city'] == '-1' and r['target_x'] == '2' and r['target_y'] == '1'
        assert json.loads(r['context_json']) == dict(strategy_id=plan, native_result=result, target_gender_guard_result=0,
            counterfactual_scope='final_probability_step_without69', counterfactual_evaluated=True)
    assert [r['event'] for r in charm[896:]] == (
        ['charm_eligibility', 'strategy_charm_probability_doubled'] * 10 + ['charm_eligibility'] +
        ['charm_eligibility', 'strategy_charm_probability_doubled'] * 2 + ['stat_effect_eligibility', 'strategy_stat_forced_success',
         'skill_predicate'] + ['charm_eligibility'] * 6)
    assert sum(r['event'] == 'charm_eligibility' for r in charm_queries) == 691
    assert sum(r['scene'] == 'unclassified_query' for r in charm_queries) == 1
    assert [r['effect_skill'] for r in charm_applied if r['event'] != 'strategy_charm_probability_doubled'] == ['57']
    defense = rows('defense_telemetry_test.csv')
    defense_session = rows('defense_session_test.csv')
    defense_index = 0
    for mask in (1, 2, 3):
        for provider in range(4):
            person = 14162 if provider % 2 else 600
            owned = 249 if provider >= 2 or mask == 3 else 27 if mask == 1 else 28
            for troops in (2999, 3000, 10000):
                for damage in (499, 500, 750):
                    for tactic in (-1, 0, 31, 32):
                        for seed in (0, 4):
                            state, effect = seed, None
                            if tactic < 0 or tactic > 31:
                                for skill, eligible in ((27, troops < 3000 and mask & 1), (28, damage < 500 and mask & 2)):
                                    if effect is not None or not eligible:
                                        continue
                                    query = defense[defense_index]
                                    defense_index += 1
                                    assert (query['event'], int(query['effect_skill']), int(query['person_id']), int(query['owned_skill'])) == (
                                        'damage_immunity_eligibility', skill, person, owned)
                                    state = (state * 0x6c078965 + 0x3039) & 0xffffffff
                                    if (state >> 16) % 100 < 50:
                                        effect = skill
                            if effect is not None:
                                for caller in (0x584fdf, 0x5b1c6c):
                                    actual = defense[defense_index]
                                    defense_index += 1
                                    assert actual['event'] == 'damage_immunity_attack_settled'
                                    assert (int(actual['effect_skill']), int(actual['person_id']), int(actual['owned_skill'])) == (effect, person, owned)
                                    assert (int(actual['before']), int(actual['after']), int(actual['caller'])) == (troops, troops, caller)
                                    assert json.loads(actual['context_json']) == dict(attacking_unit=4095, tactic_id=tactic,
                                        calculated_troop_damage_before_immunity=damage, settled_packet_troop_damage=0,
                                        native_chance_percent=50, counterfactual_evaluated=False)
    assert defense_index == 360 and len(defense) == 388
    assert [r['event'] for r in defense[defense_index:]] == (
        ['damage_immunity_eligibility', 'damage_immunity_attack_settled'] +
        ['damage_immunity_eligibility'] * 2 + ['damage_immunity_attack_settled'] * 2 +
        ['damage_immunity_eligibility', 'damage_immunity_attack_settled', 'skill_predicate'] +
        ['damage_immunity_eligibility'] * 12 + ['damage_immunity_attack_settled', 'damage_immunity_eligibility',
        'forced_critical_attack_settled', 'damage_immunity_attack_settled'] + ['damage_immunity_eligibility'] * 3)
    defense_effects = [r for r in defense + defense_session if r['event'] == 'damage_immunity_attack_settled']
    assert len(defense_effects) == 183
    for actual in defense_effects:
        assert actual['scene'] == 'battle' and actual['metric'] == 'troops' and actual['effect_skill'] in ('27', '28')
        assert actual['force_id'] == actual['target_force'] == '3' and actual['actor_city'] == '60'
        assert actual['actor_unit'] == actual['target_unit'] == '4094' and actual['equipment_id'] == '4'
        assert actual['target_base'] == actual['target_city'] == actual['target_person'] == '-1'
        assert (actual['target_x'], actual['target_y']) == ('2', '1') and not actual['baseline_after'] and not actual['bonus_delta']
    cap = [r for r in defense_effects if r['before'] == '12000']
    assert len(cap) == 1 and cap[0]['after'] == '10000' and cap[0]['delta'] == '-2000'
    assert all(r['delta'] == '0' for r in defense_effects if r not in cap)
    assert [r['event'] for r in defense_session] == ['damage_immunity_eligibility', 'damage_immunity_attack_settled']
    ambush = rows('ambush_attack_test.csv')
    ambush_session = rows('ambush_attack_session_test.csv')
    ambush_index = 0
    for mask in (1, 2, 3):
        for provider in range(4):
            person = 14162 if provider % 2 else 600
            owned = 249 if provider >= 2 or mask == 3 else 27 if mask == 1 else 28
            for troops in (2999, 3000, 10000):
                for damage in (499, 500, 750):
                    for flag in (0, 1):
                        for seed in (0, 4):
                            state, effect = seed, None
                            for skill, eligible in ((27, troops < 3000 and mask & 1), (28, damage < 500 and mask & 2)):
                                if effect is not None or not eligible:
                                    continue
                                query = ambush[ambush_index]
                                ambush_index += 1
                                assert (query['event'], int(query['effect_skill']), int(query['person_id']), int(query['owned_skill'])) == (
                                    'damage_immunity_eligibility', skill, person, owned)
                                state = (state * 0x6c078965 + 0x3039) & 0xffffffff
                                if (state >> 16) % 100 < 50:
                                    effect = skill
                            if effect is not None:
                                for caller in (0x584fdf, 0x5b1c6c):
                                    actual = ambush[ambush_index]
                                    ambush_index += 1
                                    assert actual['event'] == 'damage_immunity_ambush_settled'
                                    assert (int(actual['effect_skill']), int(actual['person_id']), int(actual['owned_skill'])) == (effect, person, owned)
                                    assert (int(actual['before']), int(actual['after']), int(actual['caller'])) == (troops, troops, caller)
                                    assert json.loads(actual['context_json']) == dict(attacking_unit=4095, tactic_id=-1,
                                        calculated_troop_damage_before_immunity=damage, settled_packet_troop_damage=0,
                                        native_chance_percent=50, counterfactual_evaluated=False)
    assert ambush_index == 360 and len(ambush) == 404
    assert [r['event'] for r in ambush[ambush_index:]] == (
        ['damage_immunity_eligibility'] * 2 + ['damage_immunity_ambush_settled'] +
        ['critical_eligibility', 'forced_critical_ambush_settled', 'critical_eligibility', 'damage_immunity_eligibility',
         'forced_critical_ambush_settled', 'damage_immunity_ambush_settled'] * 4 +
        ['damage_immunity_eligibility', 'damage_immunity_ambush_settled'] + ['damage_immunity_eligibility'] * 13 +
        ['damage_immunity_ambush_settled', 'damage_immunity_eligibility'])
    ambush_defense = [r for r in ambush + ambush_session if r['event'] == 'damage_immunity_ambush_settled']
    ambush_critical = [r for r in ambush if r['event'] == 'forced_critical_ambush_settled']
    assert len(ambush_defense) == 184 and len(ambush_critical) == 8
    for actual in ambush_defense:
        assert actual['scene'] == 'battle' and actual['metric'] == 'troops' and actual['effect_skill'] in ('27', '28')
        assert actual['force_id'] == actual['target_force'] == '3' and actual['actor_city'] == '60'
        assert actual['actor_unit'] == actual['target_unit'] == '4094' and actual['equipment_id'] == '4'
        assert actual['baseline_after'] == actual['bonus_delta'] == '' and actual['delta'] == '0'
        assert (actual['target_x'], actual['target_y']) == ('2', '1')
    for actual, effect, owned, loss in zip(ambush_critical, (23,)*4+(40,)*4, (23, 23, 249, 249, 40, 40, 249, 249), (499, 0, 499, 0)*2):
        assert actual['person_id'] == '1000' and int(actual['effect_skill']) == effect and int(actual['owned_skill']) == owned
        assert actual['force_id'] == '2' and actual['target_force'] == '3' and actual['actor_city'] == '8'
        assert actual['actor_unit'] == '4095' and actual['target_unit'] == '4094' and actual['equipment_id'] == '1'
        assert (int(actual['before']), int(actual['after']), int(actual['delta'])) == (2999, 2999-loss, -loss)
        assert not actual['baseline_after'] and not actual['bonus_delta']
        assert json.loads(actual['context_json']) == dict(tactic_id=-1, record_kind=1, critical=True,
            requested_troop_loss=loss, damage_attribution='whole_attack_not_marginal_skill_bonus', counterfactual_evaluated=False)
    assert [r['event'] for r in ambush_session] == ['damage_immunity_eligibility', 'damage_immunity_ambush_settled']
    ironwall = rows('ironwall_test.csv')
    ironwall_index = 0
    for count in (2, 3, 6, 12):
        for person, owned in ((600, 29), (14162, 29), (600, 249), (14162, 249)):
            for troops in (0, 100, 1000, 10000):
                for damage in (0, 125, 700):
                    for mode in (0, 1, 2, 3, 4):
                        query, count_row, troop_row = ironwall[ironwall_index:ironwall_index+3]
                        ironwall_index += 3
                        assert query['event'] == 'ironwall_eligibility' and int(query['caller']) == 0x586568
                        assert (count_row['event'], troop_row['event']) == ('ironwall_attack_group_reduced', 'ironwall_attack_settled')
                        for actual in (query, count_row, troop_row):
                            assert (int(actual['person_id']), int(actual['owned_skill'])) == (person, owned)
                        assert (count_row['metric'], int(count_row['before']), int(count_row['after']), int(count_row['caller'])) == (
                            'attack_records', count, 1, 0x586589)
                        assert (troop_row['metric'], int(troop_row['before']), int(troop_row['after']), int(troop_row['caller'])) == (
                            'troops', troops, max(0, troops-damage), 0x5b1c6c)
                        context = dict(attacking_unit=4095, handler_mode=mode, attack_records_before=count, attack_records_after=1,
                            requested_troop_loss=damage, damage_attribution='whole_remaining_attack_not_prevented_loss', counterfactual_evaluated=False)
                        assert json.loads(count_row['context_json']) == json.loads(troop_row['context_json']) == context
    assert ironwall_index == 2880 and len(ironwall) == 2906
    assert [r['event'] for r in ironwall[ironwall_index:]] == (
        ['ironwall_eligibility', 'ironwall_attack_group_reduced', 'ironwall_attack_settled', 'ironwall_attack_settled'] +
        ['ironwall_eligibility', 'ironwall_attack_group_reduced', 'ironwall_attack_settled'] * 3 +
        ['ironwall_eligibility'] * 11 + ['skill_predicate', 'ironwall_eligibility'])
    ironwall_applied = [r for r in ironwall if r['stage'] == 'applied']
    assert len(ironwall_applied) == 1929 and all(r['effect_skill'] == '29' for r in ironwall)
    for actual in ironwall_applied:
        assert actual['force_id'] == actual['target_force'] == '3' and actual['actor_city'] == '60'
        assert actual['actor_unit'] == actual['target_unit'] == '4094' and actual['equipment_id'] == '4'
        assert (actual['target_x'], actual['target_y']) == ('2', '1') and not actual['baseline_after'] and not actual['bonus_delta']
        assert actual['target_base'] == actual['target_city'] == actual['target_person'] == '-1'
    assert [r['after'] for r in ironwall[2880:2884] if r['metric'] == 'troops'] == ['875', '750']
    cap = [r for r in ironwall_applied if r['metric'] == 'troops' and r['before'] == '12000']
    assert len(cap) == 1 and cap[0]['after'] == '10000' and cap[0]['delta'] == '-2000'
    assert ironwall[2890]['person_id'] == '600' and ironwall[2890]['owned_skill'] == '249' # duplicate holder: native first
    assist = rows('assist_test.csv')
    assist_index = 0
    relations = ((0,0,0,0,0), (1,0,0,0,0), (0,1,0,0,0), (0,0,1,1,0), (0,0,0,1,0), (0,0,0,0,1), (0,0,1,0,1))
    for provider in range(5):
        person = 14162 if provider in (1,3) else 600
        owned = 249 if provider >= 2 else 26
        for spouse, sworn, dislike, friend, like in relations:
            eligible = provider < 4 and not spouse and not sworn
            used = eligible and not dislike
            chance = 50 if spouse or sworn else 30 if used or friend else 20 if like and not dislike else 0
            for troops in (0,100,1000):
                for damage in (125,600):
                    for seed in (0,4):
                        sample = ((((seed*0x6c078965+0x3039)&0xffffffff)>>16)%100) if chance else 100
                        result = int(sample < chance)
                        for spawn in (0,1):
                            if eligible:
                                query = assist[assist_index]
                                assist_index += 1
                                assert (query['event'], int(query['caller']), int(query['person_id']), int(query['owned_skill'])) == (
                                    'assist_eligibility', 0x585558, person, owned)
                            if used:
                                actual = assist[assist_index]
                                assist_index += 1
                                assert actual['event'] == 'assist_skill_chance_used' and int(actual['caller']) == 0x587220
                                assert (int(actual['person_id']), int(actual['owned_skill'])) == (person, owned)
                                assert json.loads(actual['context_json']) == dict(primary_unit=4093, primary_force_at_decision=2,
                                    native_chance_percent=30, native_result=result, counterfactual_evaluated=False)
                                if result and spawn:
                                    actual = assist[assist_index]
                                    assist_index += 1
                                    assert actual['event'] == 'assist_attack_settled' and int(actual['caller']) == 0x5b1c6c
                                    assert (int(actual['person_id']), int(actual['owned_skill']), int(actual['before']), int(actual['after'])) == (
                                        person, owned, troops, max(0,troops-damage))
                                    assert json.loads(actual['context_json']) == dict(primary_unit=4093, primary_force_at_decision=2,
                                        record_kind=3, native_chance_percent=30, requested_troop_loss=damage,
                                        damage_attribution='whole_assist_attack_not_marginal_skill_bonus', counterfactual_evaluated=False)
    assert assist_index == 840 and len(assist) == 862
    assert [r['event'] for r in assist[assist_index:]] == (
        ['assist_eligibility', 'assist_skill_chance_used'] * 9 + ['assist_attack_settled', 'skill_predicate',
        'assist_eligibility', 'assist_skill_chance_used'])
    assist_applied = [r for r in assist if r['stage']=='applied']
    assert len(assist_applied) == 371 and sum(r['event']=='assist_attack_settled' for r in assist) == 73
    for actual in assist_applied:
        assert actual['effect_skill']=='26' and actual['force_id']=='2' and actual['target_force']=='3' and actual['actor_city']=='8'
        assert actual['actor_unit']=='4095' and actual['target_unit']=='4094' and actual['equipment_id']=='1'
        assert (actual['target_x'],actual['target_y'])==('2','1') and not actual['baseline_after'] and not actual['bonus_delta']
    aggregate = rows('single_aggregate_test.csv')
    assert len(aggregate)==10 and [r['event'] for r in aggregate] == [
        'damage_immunity_eligibility','damage_immunity_attack_settled','damage_immunity_eligibility','damage_immunity_ambush_settled',
        'critical_eligibility','forced_critical_attack_settled','critical_eligibility','forced_critical_ambush_settled','critical_eligibility','aggregated_attack_component_settled']
    for actual, effect, person, force, after in zip([r for r in aggregate if r['stage']=='applied'][:4], (27,27,23,23), (1000,1000,600,600), (3,3,2,2), (1000,1000,875,875)):
        assert (int(actual['effect_skill']),int(actual['person_id']),int(actual['force_id']))==(effect,person,force)
        assert (int(actual['before']),int(actual['after']),int(actual['caller']))==(1000,after,0x5b1c6c)
        assert actual['target_unit']=='4094' and actual['target_force']=='3' and not actual['baseline_after'] and not actual['bonus_delta']
    component=aggregate[-1];context=json.loads(component['context_json'])
    assert (component['person_id'],component['effect_skill'],component['metric'],component['delta'])==('600','23','settled_component','1')
    assert context['component_index']==0 and context['component_count']==2 and context['calculated_component_damage']==125
    assert (context['shared_requested_troop_loss'],context['shared_resource_before'],context['shared_resource_after'])==(126,1000,874)
    counter = rows('counter_immunity_test.csv')
    counter_index = 0
    for mask in (1,2,3):
        for provider in range(4):
            person = 14162 if provider%2 else 1000
            owned = 249 if provider>=2 or mask==3 else 13 if mask==1 else 14
            for water in (0,1):
                effect = 14 if water else 13
                eligible = bool(mask & (2 if water else 1))
                for flag in (0,1):
                    for route in (0,1):
                        for count in (1,3):
                            for optional in (0,1):
                                if route and (count!=1 or optional):
                                    continue
                                for troops in (0,100,1000):
                                    for damage in (0,125):
                                        for seed in (0,4):
                                            if not eligible:
                                                continue
                                            query = counter[counter_index]
                                            counter_index += 1
                                            assert (query['event'],int(query['effect_skill']),int(query['person_id']),int(query['owned_skill'])) == (
                                                'counterattack_immunity_eligibility',effect,person,owned)
                                            state = (seed*0x6c078965+0x3039)&0xffffffff
                                            if (state>>16)%100 < 50:
                                                actual = counter[counter_index]
                                                counter_index += 1
                                                assert actual['event']=='counterattack_immunity_attack_settled'
                                                assert (int(actual['effect_skill']),int(actual['person_id']),int(actual['owned_skill']))==(effect,person,owned)
                                                assert (int(actual['before']),int(actual['after']),int(actual['caller']))==(troops,troops,0x5b1c6c)
                                                assert json.loads(actual['context_json']) == dict(responding_unit=4095,native_attack_mode=flag+1,
                                                    native_water_branch=bool(water),calculated_counter_troop_damage=damage,calculation_stage='before_parent_scaling',
                                                    settled_packet_troop_damage=0,native_chance_percent=50,counterfactual_evaluated=False)
    assert counter_index==2880 and len(counter)==2901
    assert [r['event'] for r in counter[counter_index:]] == ['counterattack_immunity_eligibility']*16 + [
        'counterattack_immunity_eligibility','counterattack_immunity_attack_settled',
        'counterattack_immunity_eligibility','counterattack_immunity_attack_settled','counterattack_immunity_eligibility']
    counter_applied = [r for r in counter if r['stage']=='applied']
    assert len(counter_applied)==962
    for actual in counter_applied:
        assert actual['scene']=='battle' and actual['metric']=='troops' and actual['force_id']==actual['target_force']=='3'
        assert actual['actor_city']=='60' and actual['actor_unit']==actual['target_unit']=='4094' and actual['equipment_id']=='4'
        assert (actual['target_x'],actual['target_y'])==('2','1') and not actual['baseline_after'] and not actual['bonus_delta']
        assert actual['target_base']==actual['target_city']==actual['target_person']=='-1'
    cap=[r for r in counter_applied if r['before']=='12000']
    assert len(cap)==1 and cap[0]['after']=='10000' and cap[0]['delta']=='-2000'
    assert counter_applied[-1]['person_id']=='1000' and counter_applied[-1]['owned_skill']=='249' # first matching of two holders
    double = rows('double_attack_test.csv')
    double_index = 0
    for provider in range(6):
        person = 14162 if provider in (1,3) else 600
        owned = 249 if provider in (2,3,5) else 12
        for mode in (0,1,2,3,4):
            for primary in (0,1):
                for troops in (0,100,1000,12000):
                    for damage in (0,125,600):
                        for seed in (0,4):
                            if primary or mode in (2,4) or provider==4:
                                continue
                            query = double[double_index]
                            double_index += 1
                            assert (query['event'],int(query['caller']),int(query['person_id']),int(query['owned_skill'])) == (
                                'double_attack_eligibility',0x586eeb,person,owned)
                            state = (seed*0x6c078965+0x3039)&0xffffffff
                            if (state>>16)%100 >= 50:
                                continue
                            settled,resolved = double[double_index:double_index+2]
                            double_index += 2
                            before = min(max(0,troops-25),10000)
                            assert (settled['event'],int(settled['before']),int(settled['after']),int(settled['caller'])) == (
                                'double_attack_settled',before,max(0,before-damage),0x5b1c6c)
                            assert json.loads(settled['context_json']) == dict(parent_mode=mode,record_kind=4,native_chance_percent=50,
                                requested_troop_loss=damage,damage_attribution='whole_extra_attack_not_marginal_skill_bonus',counterfactual_evaluated=False)
                            assert resolved['event']=='double_attack_resolved' and int(resolved['caller'])==0x586ffc
                            assert json.loads(resolved['context_json'])==dict(parent_mode=mode,child_mode=2,native_chance_percent=50,
                                native_result=0,counterfactual_evaluated=False)
                            assert all(int(r['person_id'])==person and int(r['owned_skill'])==owned for r in (settled,resolved))
    assert double_index==len(double)==720
    double_edges = rows('double_attack_edges_test.csv')
    assert [r['event'] for r in double_edges] == [
        'double_attack_eligibility','double_attack_resolved','double_attack_eligibility',
        'double_attack_eligibility','double_attack_resolved','double_attack_eligibility','double_attack_resolved',
        'double_attack_eligibility','double_attack_resolved','double_attack_eligibility','double_attack_eligibility',
        'double_attack_eligibility','double_attack_eligibility','double_attack_settled','double_attack_resolved',
        'double_attack_eligibility','double_attack_eligibility','double_attack_eligibility','double_attack_settled',
        'double_attack_resolved','skill_predicate']
    assert json.loads(double_edges[-2]['context_json'])['native_result']==7
    assert all((r['before'],r['after'])==('975','850') for r in double_edges if r['metric'])
    coeffects = rows('double_attack_coeffects_test.csv')
    coeffects_index = 0
    for combination in (1,2,3):
        for mode in (0,1,3):
            for seed in (0,3):
                state=(seed*0x6c078965+0x3039)&0xffffffff
                if combination&2:
                    state=(state*0x6c078965+0x3039)&0xffffffff
                immune=bool(combination&2) and (state>>16)%100<50
                events=[('double_attack_eligibility',12,600)]
                if combination&1: events.append(('critical_eligibility',23,14162))
                if combination&2: events.append(('damage_immunity_eligibility',27,1000))
                if combination&1: events.append(('forced_critical_double_attack_settled',23,14162))
                if immune: events.append(('damage_immunity_double_attack_settled',27,1000))
                events.extend([('double_attack_settled',12,600),('double_attack_resolved',12,600)])
                for event,effect,person in events:
                    actual=coeffects[coeffects_index];coeffects_index+=1
                    assert (actual['event'],int(actual['effect_skill']),int(actual['person_id']))==(event,effect,person)
                    if actual['stage']=='applied':
                        assert actual['force_id']==('3' if effect==27 else '2')
                        assert actual['actor_unit']==('4094' if effect==27 else '4095')
                        if actual['metric']:
                            assert (int(actual['before']),int(actual['after']),int(actual['caller']))==(975,975 if immune else 850,0x5b1c6c)
                        else:
                            assert json.loads(actual['context_json'])['parent_mode']==mode
    assert coeffects_index==len(coeffects)==96
    double_applied=[r for r in double+double_edges+coeffects if r['stage']=='applied']
    assert len(double_applied)==422
    double_query_count=len(double)+len(double_edges)+len(coeffects)-len(double_applied)
    for actual in double_applied:
        assert actual['scene']=='battle' and actual['target_unit']=='4094' and actual['target_force']=='3'
        assert (actual['target_x'],actual['target_y'])==('2','1') and not actual['baseline_after'] and not actual['bonus_delta']
        assert actual['target_base']==actual['target_city']==actual['target_person']=='-1'
        if actual['effect_skill']=='12':
            assert actual['force_id']=='2' and actual['actor_unit']=='4095'
    pincer = rows('pincer_test.csv')
    pincer_index = 0
    pincer_turns = set()
    for count in (0,1,2,6,12):
        for provider in range(6):
            person=14162 if provider in (1,3) else 600
            owned=249 if provider in (2,3,5) else 18
            for mode in (0,1,2,3,4):
                for primary in (0,1):
                    for status in (0,1,2):
                        for before in (0,4,5):
                            for seed in (0,3,4,9):
                                if count<=1 or primary or provider==4:
                                    continue
                                query=pincer[pincer_index];pincer_index+=1
                                assert (query['event'],int(query['person_id']),int(query['owned_skill']),int(query['caller']))==(
                                    'pincer_eligibility',person,owned,0x586bea)
                                # First draw is the original multi-record damage
                                # scaling, second the skill chance, third duration.
                                state=(seed*0x6c078965+0x3039)&0xffffffff
                                state=(state*0x6c078965+0x3039)&0xffffffff
                                if (state>>16)%100>=50:
                                    continue
                                state=(state*0x6c078965+0x3039)&0xffffffff
                                turns=1+(state>>16)%2;pincer_turns.add(turns)
                                actual=pincer[pincer_index];pincer_index+=1
                                assert (actual['event'],int(actual['person_id']),int(actual['owned_skill']),int(actual['caller']))==(
                                    'pincer_status_applied',person,owned,0x586c2b)
                                assert (actual['metric'],int(actual['before']),int(actual['after']))==('status_turns',before,min(5,before+turns))
                                assert json.loads(actual['context_json'])==dict(handler_mode=mode,attack_records=count,native_chance_percent=50,
                                    status_before=status,status_after=1,requested_turns=turns,counterfactual_evaluated=False)
    assert pincer_index==len(pincer)==4050 and pincer_turns=={1,2}
    pincer_applied=[r for r in pincer if r['stage']=='applied']
    assert len(pincer_applied)==1350
    for actual in pincer_applied:
        assert actual['effect_skill']=='18' and actual['force_id']=='2' and actual['actor_unit']=='4095'
        assert actual['target_unit']=='4094' and actual['target_force']=='3' and actual['equipment_id']=='4'
        assert (actual['target_x'],actual['target_y'])==('2','1') and not actual['baseline_after'] and not actual['bonus_delta']
        assert actual['target_base']==actual['target_city']==actual['target_person']=='-1'
    pincer_edges=rows('pincer_edges_test.csv')
    assert [r['event'] for r in pincer_edges]==['pincer_eligibility']*8+[
        'ironwall_eligibility','ironwall_attack_group_reduced','ironwall_attack_settled','skill_predicate']
    assert pincer_edges[9]['effect_skill']==pincer_edges[10]['effect_skill']=='29'
    assert (pincer_edges[9]['before'],pincer_edges[9]['after'])==('2','1')
    assert (pincer_edges[10]['before'],pincer_edges[10]['after'])==('1000','1000')
    pincer_long=rows('pincer_long_false_report_test.csv')
    pincer_long_index=0
    for status in (0,1,2):
        for before in (0,4,5):
            for seed in (3,9):
                query,actual=pincer_long[pincer_long_index:pincer_long_index+2];pincer_long_index+=2
                assert query['event']=='pincer_eligibility' and actual['event']=='pincer_status_applied'
                assert (actual['before'],int(actual['after']))==(str(before),min(5,before+(1 if seed==3 else 2)))
                assert json.loads(actual['context_json'])==dict(handler_mode=0,attack_records=2,native_chance_percent=50,
                    status_before=status,status_after=1,requested_turns=1 if seed==3 else 2,counterfactual_evaluated=False)
    assert pincer_long_index==len(pincer_long)==36
    ironwall_capture=rows('ironwall_capture_test.csv');ironwall_capture_index=0
    def ironwall_capture_row(row,person,owned,count,chance=None,result=None):
        assert (row['effect_skill'],row['owned_skill'],row['person_id'],row['force_id'],row['ruler_id'],row['actor_city'])==('29',str(owned),str(person),'3','700','60')
        assert row['scene']=='battle'
        if chance is None:
            assert (row['stage'],row['event'],row['caller'])==('check_matched','ironwall_capture_eligibility',str(0x4b17f6))
            assert not row['context_json'];return
        assert (row['stage'],row['event'],row['caller'])==('applied','ironwall_capture_multiplier_reduced',str(0x4b18ac))
        assert (row['actor_unit'],row['target_unit'],row['target_person'],row['target_force'],row['equipment_id'])==('4094','4094',str(person),'3','2')
        assert (row['target_x'],row['target_y'],row['target_base'],row['target_city'])==('2','1','-1','-1')
        assert not any(row[k] for k in ('metric','before','after','delta','baseline_after','bonus_delta'))
        assert json.loads(row['context_json'])==dict(native_surround_count=count,used_capture_multiplier=100,native_capture_chance_percent=chance,
            native_result=result,attacking_unit=4095,attacking_base=-1,attacking_force=2,result_scope='capture_multiplier_not_final_escape',counterfactual_capture_chance_evaluated=False)
    for profile in range(6):
        for count in (0,1,2,6):
            for terrain in (0,3):
                for seed in (0,4,54):
                    state=seed
                    for slot in range(2):
                        holder=profile==3 or (slot==0 and profile in (0,2)) or (slot==1 and profile in (1,4))
                        multiplier=1 if count<=0 else 100*(1 if holder else count)
                        chance=int(6*multiplier*(1.5 if terrain else 1)*struct.unpack('<f',struct.pack('<f',.01))[0])
                        result=0
                        if chance>0:state=(state*0x6c078965+0x3039)&0xffffffff;result=int((state>>16)%100<chance)
                        if not holder or count<=0:continue
                        person=1000 if slot==0 else 14162 if profile==4 else 601;owned=249 if profile==2 else 29
                        ironwall_capture_row(ironwall_capture[ironwall_capture_index],person,owned,count);ironwall_capture_index+=1
                        if count>1:
                            ironwall_capture_row(ironwall_capture[ironwall_capture_index],person,owned,count,chance,result);ironwall_capture_index+=1
    assert ironwall_capture_index==len(ironwall_capture)==180
    ironwall_capture_edges=rows('ironwall_capture_edges_test.csv');ironwall_capture_index=0
    for fault in range(8):
        ironwall_capture_row(ironwall_capture_edges[ironwall_capture_index],1000,29,6);ironwall_capture_index+=1
        if fault==0:
            ironwall_capture_row(ironwall_capture_edges[ironwall_capture_index],1000,29,6,5,0);ironwall_capture_index+=1
    assert ironwall_capture_index+1==len(ironwall_capture_edges)==10
    assert (ironwall_capture_edges[-1]['stage'],ironwall_capture_edges[-1]['scene'],ironwall_capture_edges[-1]['event'])==('check_matched','unclassified_query','skill_predicate')
    ironwall_capture_coeffects=rows('ironwall_capture_coeffects_test.csv');assert len(ironwall_capture_coeffects)==10
    for case,owned in enumerate((29,249)):
        query,defender,actual,*rolls=ironwall_capture_coeffects[case*5:case*5+5]
        assert (query['stage'],query['event'],query['effect_skill'],query['person_id'],query['force_id'])==('check_matched','capture_eligibility','19','600','2')
        ironwall_capture_row(defender,1000,owned,6)
        ironwall_capture_row(actual,1000,owned,6,100,1)
        for row,victim in zip(rolls,(1000,601)):
            assert (row['stage'],row['event'],row['effect_skill'],row['person_id'],row['force_id'],row['target_person'])==('applied','capture_guarantee_roll_resolved','19','600','2',str(victim))
            assert json.loads(row['context_json'])==dict(native_capture_chance_percent=100,native_result=1,result_scope='candidate_roll_not_final_disposition',counterfactual_evaluated=False)
    lucky_capture=rows('lucky_capture_test.csv');lucky_index=0
    def lucky_capture_row(row,person,owned,applied=False):
        assert (row['effect_skill'],row['person_id'],row['owned_skill'],row['force_id'],row['scene'])==('32',str(person),str(owned),'3','battle')
        if not applied:
            assert (row['stage'],row['event'],row['caller'])==('check_matched','capture_luck_eligibility',str(0x4b17b9));return
        assert (row['stage'],row['event'],row['caller'])==('applied','lucky_capture_attempt_skipped',str(0x4b17bb))
        assert (row['actor_unit'],row['target_unit'],row['target_person'],row['target_force'],row['equipment_id'])==('4094','4094',str(person),'3','2')
        assert (row['target_x'],row['target_y'],row['target_base'],row['target_city'])==('2','1','-1','-1')
        assert json.loads(row['context_json'])==dict(attacking_unit=4095,attacking_base=-1,attacking_force=2,capture_random_sample_skipped=True,
            result_scope='native_capture_candidate_roll_skipped',counterfactual_capture_evaluated=False)
    for profile in range(6):
        for count in (1,2):
            for item in (0,1):
                for guarantee in (0,1):
                    for seed in (0,4):
                        if guarantee:
                            row=lucky_capture[lucky_index];lucky_index+=1
                            assert (row['stage'],row['event'],row['effect_skill'],row['person_id'],row['force_id'])==('check_matched','capture_eligibility','19','600','2')
                        for slot in range(count):
                            if slot==0 and item:continue
                            holder=profile==3 or slot==0 and profile in (0,2) or slot==1 and profile in (1,4)
                            person=1000 if slot==0 else 14162 if profile==4 else 601;owned=249 if profile==2 else 32
                            if holder:
                                lucky_capture_row(lucky_capture[lucky_index],person,owned);lucky_index+=1
                                lucky_capture_row(lucky_capture[lucky_index],person,owned,True);lucky_index+=1
                            elif guarantee:
                                row=lucky_capture[lucky_index];lucky_index+=1
                                assert (row['stage'],row['event'],row['effect_skill'],row['person_id'],row['force_id'],row['target_person'])==('applied','capture_guarantee_roll_resolved','19','600','2',str(person))
                                assert json.loads(row['context_json'])==dict(native_capture_chance_percent=100,native_result=1,result_scope='candidate_roll_not_final_disposition',counterfactual_evaluated=False)
    assert lucky_index==len(lucky_capture)==168
    lucky_edges=rows('lucky_capture_edges_test.csv');assert len(lucky_edges)==10
    lucky_capture_row(lucky_edges[0],1000,32);lucky_capture_row(lucky_edges[1],1000,32,True)
    for row in lucky_edges[2:8]+lucky_edges[9:]:lucky_capture_row(row,1000,32)
    assert (lucky_edges[8]['stage'],lucky_edges[8]['event'],lucky_edges[8]['scene'])==('check_matched','skill_predicate','unclassified_query')
    capture=rows('capture_test.csv');capture_index=0
    for provider in range(6):
        for bloodroad in (0,1,2,3):
            for count in (1,2):
                for item in (0,1):
                    for luck in (0,1):
                        for disposition in (0,1):
                            for distance in (0,1):
                                for seed in (0,4):
                                    blocked=bloodroad in (1,3) or (bloodroad==2 and count==2)
                                    actor=601 if bloodroad==2 else 1000
                                    owned=249 if bloodroad==3 else 33
                                    events=[]
                                    if blocked:
                                        events=[('bloodroad_eligibility',33,actor,owned,-1),('bloodroad_capture_attempt_blocked',33,actor,owned,-1)]
                                        events += [('bloodroad_member_escape_resolved',33,actor,owned,v) for v in (1000,601)[:count]]
                                    else:
                                        actor=14162 if provider in (1,3) else 600
                                        owned=249 if provider in (2,3,5) else 19
                                        guarantee=provider!=4 and distance==1
                                        if guarantee:events.append(('capture_eligibility',19,actor,owned,-1))
                                        if luck and not item:
                                            events += [('capture_luck_eligibility',32,1000,32,-1),('lucky_capture_attempt_skipped',32,1000,32,1000)]
                                        settled=[]
                                        for slot,victim in enumerate((1000,601)[:count]):
                                            if slot==0 and (item or luck):continue
                                            if guarantee:
                                                events.append(('capture_guarantee_roll_resolved',19,actor,owned,victim))
                                                if disposition:settled.append(('capture_prisoner_status_written',19,actor,owned,victim))
                                        events+=settled
                                        events += [('capture_prisoner_after_defeat_resolved',e,p,o,v) for _,e,p,o,v in settled]
                                    for event,effect,actor,owned,victim in events:
                                        actual=capture[capture_index];capture_index+=1
                                        assert (actual['event'],int(actual['effect_skill']),int(actual['person_id']),int(actual['owned_skill']),int(actual['target_person'])) == (
                                            event,effect,actor,owned,victim)
                                        if actual['stage']!='applied':continue
                                        assert actual['force_id']==('2' if effect==19 else '3') and actual['target_force']=='3'
                                        assert actual['actor_unit']==('4095' if effect==19 else '4094') and actual['target_unit']=='4094'
                                        assert actual['equipment_id']==('4' if effect==19 else '2')
                                        context=json.loads(actual['context_json'])
                                        if event=='lucky_capture_attempt_skipped':
                                            lucky_capture_row(actual,1000,32,True)
                                        elif event=='capture_guarantee_roll_resolved':
                                            assert context==dict(native_capture_chance_percent=100,native_result=1,
                                                result_scope='candidate_roll_not_final_disposition',counterfactual_evaluated=False)
                                            assert int(actual['caller'])==0x4b18ac
                                        elif event=='capture_prisoner_status_written':
                                            assert (actual['metric'],actual['before'],actual['after'],int(actual['caller']))==('person_status','3','5',0x4a9724)
                                            assert context==dict(native_capture_chance_percent=100,final_disposition='prisoner_status_written',counterfactual_evaluated=False)
                                        elif event=='capture_prisoner_after_defeat_resolved':
                                            assert int(actual['caller'])==0x4be790
                                            assert context==dict(native_capture_chance_percent=100,person_status_after=5,
                                                resolution_scope='after_defeat_return',counterfactual_evaluated=False)
                                        elif event=='bloodroad_capture_attempt_blocked':
                                            assert int(actual['caller'])==0x4beeb4
                                            assert context==dict(attacking_unit=4095,attacking_force=2,effect_scope='native_capture_attempt_skipped',counterfactual_evaluated=False)
                                        else:
                                            assert int(actual['caller'])==0x4bf186
                                            assert context==dict(attacking_unit=4095,attacking_force=2,place_before=4181,place_after=5,
                                                person_status_after=3,protected_unit_removed=True,counterfactual_evaluated=False)
    assert capture_index==len(capture)==4264
    capture_edges=rows('capture_edges_test.csv')
    assert [r['event'] for r in capture_edges]==(['capture_eligibility','capture_guarantee_roll_resolved']*7+
        ['capture_eligibility']+['capture_eligibility','capture_guarantee_roll_resolved']+
        ['bloodroad_eligibility','bloodroad_capture_attempt_blocked']*5+['bloodroad_member_escape_resolved']+
        ['bloodroad_eligibility','bloodroad_capture_attempt_blocked']*2+['bloodroad_member_escape_resolved']+
        ['bloodroad_eligibility','bloodroad_capture_attempt_blocked','bloodroad_member_escape_resolved','skill_predicate'])
    assert [r['target_person'] for r in capture_edges if r['event']=='bloodroad_member_escape_resolved']==['601','601','1000']
    capture_applied=[r for r in capture+capture_edges if r['stage']=='applied']
    assert len(capture_applied)==2939
    assert sum(r['event']=='capture_prisoner_status_written' for r in capture_applied)==70
    assert sum(r['event']=='bloodroad_member_escape_resolved' for r in capture_applied)==1539
    for actual in capture_applied:
        assert actual['scene']=='battle' and (actual['target_x'],actual['target_y'])==('2','1')
        assert actual['target_base']==actual['target_city']=='-1' and not actual['baseline_after'] and not actual['bonus_delta']
    capture_base=rows('capture_base_test.csv');capture_base_index=0
    def capture_base_row(row,event,base,actor,owned,victim=-1):
        assert (row['scene'],row['event'],row['effect_skill'],row['person_id'],row['owned_skill'],row['force_id'])==('battle',event,'19',str(actor),str(owned),'2')
        assert (row['actor_unit'],row['equipment_id'],row['target_base'],row['target_city'])==('-1','-1','-1','-1')
        if event=='capture_eligibility':
            assert (row['stage'],row['caller'],row['target_person'])==('check_matched',str(0x4b1577),'-1');return
        assert (row['stage'],row['target_unit'],row['target_force'],row['target_person'],row['target_x'],row['target_y'])==('applied','4094','3',str(victim),'2','1')
        context=dict(source_base=base,native_capture_chance_percent=100,counterfactual_evaluated=False)
        if event=='capture_guarantee_roll_resolved':
            context.update(native_result=1,result_scope='candidate_roll_not_final_disposition');caller=0x4b18ac
        elif event=='capture_prisoner_status_written':
            context.update(final_disposition='prisoner_status_written');caller=0x4a9724
            assert (row['metric'],row['before'],row['after'],row['delta'])==('person_status','3','5','2')
        else:
            assert event=='capture_prisoner_after_defeat_resolved'
            context.update(person_status_after=5,resolution_scope='after_defeat_return');caller=0x4be790
        assert json.loads(row['context_json'])==context and int(row['caller'])==caller
    for base in (23,42,52):
        for provider in range(6):
            for count in (1,2):
                for disposition in (0,1):
                    for distance in (0,1):
                        for seed in (0,4):
                            if provider==4 or not distance:continue
                            actor=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3,5) else 19
                            events=[('capture_eligibility',-1)]
                            events += [('capture_guarantee_roll_resolved',v) for v in (1000,601)[:count]]
                            if disposition:
                                events += [('capture_prisoner_status_written',v) for v in (1000,601)[:count]]
                                events += [('capture_prisoner_after_defeat_resolved',v) for v in (1000,601)[:count]]
                            for event,victim in events:
                                capture_base_row(capture_base[capture_base_index],event,base,actor,owned,victim);capture_base_index+=1
    assert capture_base_index==len(capture_base)==480
    capture_base_edges=rows('capture_base_edges_test.csv');capture_base_edge_index=0
    for profile in range(12):
        actor=14162 if profile in (9,10) else 600;owned=249 if profile==11 else 19
        events=[('capture_eligibility',-1)]
        if profile!=7:events.append(('capture_guarantee_roll_resolved',1000))
        if profile==3 or profile>=9:events += [('capture_prisoner_status_written',1000),('capture_prisoner_after_defeat_resolved',1000)]
        for event,victim in events:
            capture_base_row(capture_base_edges[capture_base_edge_index],event,23,actor,owned,victim);capture_base_edge_index+=1
    assert capture_base_edge_index==len(capture_base_edges)==31
    capture_base_collection=rows('capture_base_collection_test.csv');capture_collection_index=0
    for variant,base in enumerate((23,42,52,529,529)):
        for high in (14162,14612):
            for profile in range(14):
                if variant==3 or profile in (0,9,11,12):continue
                actor=high if profile in (2,4,5,7,8,10) else 600
                owned=249 if profile==3 else 19
                for event,victim in (('capture_eligibility',-1),('capture_guarantee_roll_resolved',1000),
                                     ('capture_prisoner_status_written',1000),('capture_prisoner_after_defeat_resolved',1000)):
                    capture_base_row(capture_base_collection[capture_collection_index],event,base,actor,owned,victim);capture_collection_index+=1
    assert capture_collection_index==len(capture_base_collection)==320
    assert sum(r['stage']=='applied' for r in capture_base_collection)==240
    handoff=rows('bloodroad_handoff_test.csv');handoff_index=0
    for bloodroad in (1,2,3):
        for count in (1,2):
            if bloodroad==2 and count==1:continue
            for mode in (0,1,2):
                for repeat in range(16):
                    actor=601 if bloodroad==2 else 1000
                    owned=249 if bloodroad==3 else 33
                    expected=[('bloodroad_eligibility',-1),('bloodroad_capture_attempt_blocked',-1)]
                    if mode!=2:expected += [('bloodroad_member_escape_resolved',v) for v in (1000,601)[:count]]
                    for event,victim in expected:
                        actual=handoff[handoff_index];handoff_index+=1
                        assert (actual['event'],int(actual['effect_skill']),int(actual['person_id']),int(actual['owned_skill']),
                            int(actual['target_person']),actual['force_id'])==(event,33,actor,owned,victim,'3')
                        if event=='bloodroad_eligibility':continue
                        assert (actual['scene'],actual['actor_unit'],actual['target_unit'],actual['target_force'],
                            actual['equipment_id'],actual['target_x'],actual['target_y'])==('battle','4094','4094','3','2','2','1')
                        assert actual['target_base']==actual['target_city']=='-1'
                        assert not actual['baseline_after'] and not actual['bonus_delta']
                        context=json.loads(actual['context_json'])
                        if victim==-1:
                            assert int(actual['caller'])==0x4beeb4
                            assert context==dict(attacking_unit=4095,attacking_force=2,
                                effect_scope='native_capture_attempt_skipped',counterfactual_evaluated=False)
                        else:
                            assert int(actual['caller'])==0x4bf186
                            assert context==dict(attacking_unit=4095,attacking_force=2,place_before=4181,place_after=5,
                                person_status_after=3,protected_unit_removed=True,counterfactual_evaluated=False)
    assert handoff_index==len(handoff)==736
    handoff_edges=rows('bloodroad_handoff_edges_test.csv')
    assert [r['event'] for r in handoff_edges]==['bloodroad_eligibility','bloodroad_capture_attempt_blocked']*7
    assert all((r['effect_skill'],r['person_id'],r['owned_skill'],r['force_id'])==('33','1000','33','3') for r in handoff_edges)
    handoff_applied=[r for r in handoff+handoff_edges if r['stage']=='applied']
    assert len(handoff_applied)==503 and sum(r['event']=='bloodroad_member_escape_resolved' for r in handoff_applied)==256
    plunder=rows('plunder_test.csv');plunder_index=0
    def plunder_row(actual,event,actor,owned,victim=-1,context=None,ruler=600):
        assert (actual['event'],actual['effect_skill'],int(actual['person_id']),int(actual['owned_skill']),
            int(actual['target_person']),actual['force_id'],int(actual['ruler_id']),actual['scene'])==(event,'21',actor,owned,victim,'2',ruler,'battle')
        assert int(actual['caller'])=={'plunder_eligibility':0x4be9b5,'plunder_roll_resolved':0x4be9f6,'plunder_item_transferred':0x4bebbb}[event]
        if event=='plunder_eligibility':assert actual['stage']=='check_matched';return
        assert (actual['stage'],actual['target_force'],actual['actor_unit'],actual['target_unit'],
            actual['equipment_id'],actual['target_x'],actual['target_y'])==('applied','3','4095','4094','4','2','1')
        assert actual['target_base']==actual['target_city']=='-1'
        assert not actual['metric'] and not actual['baseline_after'] and not actual['bonus_delta']
        assert json.loads(actual['context_json'])==context
    def plunder_roll_context(result):
        return dict(native_chance_percent=10,native_result=result,result_scope='attempt_roll_not_item_transfer',
            provider_selection='last_matching_member',counterfactual_evaluated=False)
    def plunder_item_context(item,owner,ruler=600):
        return dict(item_id=item,owner_before=owner,owner_after=ruler,treasury_force_before=-1,treasury_force_after=-1,
            native_chance_percent=10,recipient_scope='attacking_ruler',provider_selection='last_matching_member',counterfactual_evaluated=False)
    for provider in range(8):
        for count in (1,2):
            for items in range(5):
                for gate in (0,1):
                    for scan in (0,1):
                        for seed in (0,1,2,3,4,9,11,17):
                            holders=[[(600,21)],[(14162,21)],[(600,249)],[(14162,249)],[(600,21),(14162,21)],
                                [(600,21),(14162,249)],[],[(600,249),(14162,21),(600,249)]][provider] if scan else []
                            for actor,owned in holders:
                                plunder_row(plunder[plunder_index],'plunder_eligibility',actor,owned);plunder_index+=1
                            if not holders or not gate:continue
                            actor,owned=holders[-1];state=(seed*0x6c078965+0x3039)&0xffffffff;success=int((state>>16)%100<10)
                            plunder_row(plunder[plunder_index],'plunder_roll_resolved',actor,owned,context=plunder_roll_context(success));plunder_index+=1
                            candidates=[n for slot in range(count) for n in range(items) if n%2==slot]
                            if success and candidates:
                                if len(candidates)>=2:state=(state*0x6c078965+0x3039)&0xffffffff
                                selected=candidates[(state>>16)%len(candidates) if len(candidates)>=2 else 0]
                                victim=601 if selected%2 else 1000
                                plunder_row(plunder[plunder_index],'plunder_item_transferred',actor,owned,victim,plunder_item_context(selected,victim));plunder_index+=1
    assert plunder_index==len(plunder)==2376
    plunder_edges=rows('plunder_edges_test.csv')
    assert len(plunder_edges)==30
    for mutation in range(1,15):
        ruler=14613 if mutation==8 else 14162 if mutation==14 else 600
        query,roll=plunder_edges[(mutation-1)*2:mutation*2]
        plunder_row(query,'plunder_eligibility',600,21,ruler=ruler)
        plunder_row(roll,'plunder_roll_resolved',600,21,context=plunder_roll_context(1),ruler=ruler)
    plunder_row(plunder_edges[28],'plunder_item_transferred',600,21,1000,plunder_item_context(0,1000,14162),ruler=14162)
    assert (plunder_edges[-1]['event'],plunder_edges[-1]['stage'],plunder_edges[-1]['scene'],plunder_edges[-1]['person_id'],
        int(plunder_edges[-1]['caller']))==('skill_predicate','check_matched','unclassified_query','600',0x4be9b5)
    plunder_applied=[r for r in plunder+plunder_edges if r['stage']=='applied']
    assert len(plunder_applied)==631
    magic_confuse=rows('magic_confuse_test.csv');magic_index=0
    def magic_coeffect_row(r,effect,actor,owned,source_stat,defensive):
        caller=0x591fd9 if effect==61 else 0x591ffa if effect==57 else 0x59201c if effect==60 else 0x592029 if defensive else 0x592007
        assert (r['stage'],r['scene'],r['event'],int(r['effect_skill']),int(r['person_id']),int(r['owned_skill']),
                r['force_id'],r['actor_unit'],r['equipment_id'],r['target_unit'],r['target_force'],int(r['caller'])) == (
            'applied','strategy','magic_target_immunity_forced_failure' if defensive else 'magic_target_forced_success',
            effect,actor,owned,'3' if defensive else '2','4094' if defensive else '4095','2' if defensive else '4','4094','3',caller)
        assert (r['target_base'],r['target_city'],r['target_x'],r['target_y'])==('-1','-1','2','1')
        assert all(not r[k] for k in ('metric','before','after','delta','baseline_after','bonus_delta'))
        assert json.loads(r['context_json'])==dict(strategy_id=7,attacking_unit=4095,attacking_force=2,
            source_intelligence=source_stat,target_intelligence=80,native_result=0 if defensive else 1,defensive=defensive,
            result_scope='per_target_decision_not_status_store',counterfactual_evaluated=False)
    def magic_row(actual,event,effect,actor,owned,unit=-1,base=-1,city=-1,before=None,after=None,metric='',context=None):
        assert (actual['event'],int(actual['effect_skill']),int(actual['person_id']),int(actual['owned_skill']),
            actual['force_id'],actual['scene'],actual['target_person'])==(event,effect,actor,owned,'2','strategy','-1')
        if event=='magic_eligibility':
            assert actual['stage']=='check_matched' and int(actual['caller'])==(0x5920c4 if effect==70 else 0x592f1e);return
        caller={'magic_command_resolved':0x592050 if effect==70 else 0x592ed0,'magic_target_decision':0x592246,
            'magic_status_written':0x5922b4,'magic_lightning_resource_written':0x5b1c6c if unit>=0 else 0x5b1aaf if metric=='facility_hp' else 0x5b1a74}[event]
        assert actual['stage']=='applied' and int(actual['caller'])==caller
        assert (actual['actor_unit'],actual['equipment_id'],int(actual['target_unit']),int(actual['target_base']),int(actual['target_city']))==('4095','4',unit,base,city)
        assert actual['target_force']==('-1' if event=='magic_command_resolved' else '3')
        assert (actual['target_x'],actual['target_y'])==('2','2' if unit==4093 else '1')
        assert not actual['baseline_after'] and not actual['bonus_delta'] and actual['metric']==metric
        if metric:assert (int(actual['before']),int(actual['after']))==(before,after)
        assert json.loads(actual['context_json'])==context
    def magic_command_context(success,critical):
        return dict(native_success_argument=success,critical_argument=critical,native_return=1,
            result_scope='command_return_not_per_target_effect',counterfactual_evaluated=False)
    def magic_damage_context(loss,critical):
        return dict(requested_loss=loss,critical_argument=critical,damage_attribution='whole_lightning_effect',counterfactual_evaluated=False)
    for provider in range(6):
        for success in (0,1):
            for critical in (0,1):
                for magic_guard in range(4):
                    for intelligence in range(3):
                        for before in (0,4,5):
                            for seed in (0,3,4,9):
                                actor=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3,4) else 70;source_stat=(100,60,80)[intelligence]
                                if provider!=5:magic_row(magic_confuse[magic_index],'magic_eligibility',70,actor,owned);magic_index+=1
                                state=seed
                                if success:
                                    for slot,stat in enumerate((80,100)):
                                        blocked=slot==0 and (magic_guard==1 or source_stat<stat and magic_guard in (2,3))
                                        if blocked:
                                            actual=magic_confuse[magic_index];magic_index+=1
                                            assert (actual['event'],actual['scene'],actual['stage'],actual['person_id'],actual['effect_skill'],actual['force_id'])==(
                                                'skill_predicate','unclassified_query','check_matched','1000',str((0,61,60,63)[magic_guard]),'3')
                                            result=0
                                            if provider!=5:
                                                magic_coeffect_row(magic_confuse[magic_index],(0,61,60,63)[magic_guard],1000,
                                                    (0,61,60,63)[magic_guard],source_stat,True);magic_index+=1
                                        else:
                                            state=(state*0x6c078965+0x3039)&0xffffffff;result=int((state>>16)%100<100-stat//2)
                                        if provider!=5:
                                            magic_row(magic_confuse[magic_index],'magic_target_decision',70,actor,owned,unit=4093 if slot else 4094,
                                                context=dict(native_result=result,source_intelligence=source_stat,target_intelligence=stat,
                                                    result_scope='per_target_decision_not_status_store',counterfactual_evaluated=False));magic_index+=1
                                        if result:
                                            state=(state*0x6c078965+0x3039)&0xffffffff;status=2 if (state>>16)%100<50 else 1
                                            if provider!=5:
                                                magic_row(magic_confuse[magic_index],'magic_status_written',70,actor,owned,unit=4093 if slot else 4094,
                                                    metric='status_turns',before=before,after=min(5,before+2+critical),
                                                    context=dict(status_before=0,status_after=status,requested_turns=2+critical,critical_argument=critical,counterfactual_evaluated=False));magic_index+=1
                                if provider!=5:
                                    magic_row(magic_confuse[magic_index],'magic_command_resolved',70,actor,owned,context=magic_command_context(success,critical));magic_index+=1
    assert magic_index==len(magic_confuse)==11310
    magic_thunder=rows('magic_thunder_test.csv');magic_index=0
    for provider in range(6):
        for success in (0,1):
            for critical in (0,1):
                for base in (False,True):
                    for before in (0,100,1000,5000):
                        for seed in (0,4):
                            actor=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3,4) else 71
                            if provider!=5:magic_row(magic_thunder[magic_index],'magic_eligibility',71,actor,owned);magic_index+=1
                            state=seed
                            if success:
                                for slot in (0,1):
                                    state=(state*0x6c078965+0x3039)&0xffffffff;building=base and slot==0
                                    loss=(700 if building else 1500)+(state>>16)%(300 if building else 1000)
                                    if provider==5:continue
                                    if building:
                                        magic_row(magic_thunder[magic_index],'magic_lightning_resource_written',71,actor,owned,base=60,
                                            metric='troops',before=before,after=before,context=magic_damage_context(0,critical));magic_index+=1
                                    magic_row(magic_thunder[magic_index],'magic_lightning_resource_written',71,actor,owned,
                                        base=60 if building else -1,unit=-1 if building else 4093 if slot else 4094,
                                        metric='facility_hp' if building else 'troops',before=before,after=min(1000,max(0,before-loss)) if building else max(0,before-loss),
                                        context=magic_damage_context(loss,critical));magic_index+=1
                            if provider!=5:
                                magic_row(magic_thunder[magic_index],'magic_command_resolved',71,actor,owned,context=magic_command_context(success,critical));magic_index+=1
    assert magic_index==len(magic_thunder)==1040
    magic_footprint=rows('magic_footprint_test.csv');magic_index=0
    for provider in (0,2,3):
        for critical in (0,1):
            for before in (0,100,1000,5000):
                for seed in (0,4):
                    actor=14162 if provider==3 else 600;owned=71 if provider==0 else 249
                    state=seed;damage=[]
                    for _ in range(3):state=(state*0x6c078965+0x3039)&0xffffffff;damage.append(700+(state>>16)%300)
                    loss=damage[0]+damage[1]//10+damage[2]//10
                    magic_row(magic_footprint[magic_index],'magic_eligibility',71,actor,owned);magic_index+=1
                    magic_row(magic_footprint[magic_index],'magic_lightning_resource_written',71,actor,owned,base=8,city=8,
                        metric='troops',before=before,after=before,context=magic_damage_context(0,critical));magic_index+=1
                    magic_row(magic_footprint[magic_index],'magic_lightning_resource_written',71,actor,owned,base=8,city=8,
                        metric='facility_hp',before=before,after=min(1000,max(0,before-loss)),context=magic_damage_context(loss,critical));magic_index+=1
                    magic_row(magic_footprint[magic_index],'magic_command_resolved',71,actor,owned,context=magic_command_context(1,critical));magic_index+=1
    assert magic_index==len(magic_footprint)==192
    magic_edges=rows('magic_edges_test.csv');expected=[]
    for effect in (70,71):
        for mutation in range(1,10):
            events=['magic_eligibility']
            if effect==70:
                events+=['magic_target_decision']
                if mutation in (6,8,9):
                    if mutation!=6:events+=['magic_status_written']
                    events+=['magic_target_decision']
                    if mutation!=6:events+=['magic_status_written']
                    events+=['magic_command_resolved']
            elif mutation>=6:
                if mutation==7:events+=['magic_lightning_resource_written']*2
                events+=['magic_command_resolved']
            expected += [(event,str(effect)) for event in events]
    assert [(r['event'],r['effect_skill']) for r in magic_edges]==expected and len(magic_edges)==43
    magic_all=magic_confuse+magic_thunder+magic_footprint+magic_edges
    magic_applied=[r for r in magic_all if r['stage']=='applied']
    assert len(magic_all)==12585 and len(magic_applied)==8599
    magic_coeffect=rows('magic_coeffect_test.csv');co_index=0
    for branch in range(5):
        for provider in range(6):
            for stat in (60,80,100):
                effect=(61,57,63,60,63)[branch];defensive=branch==0 or branch>=3
                assert magic_coeffect[co_index]['event']=='magic_eligibility';co_index+=1
                forced=provider!=5 and (effect==61 or (stat<80 if defensive else stat>80))
                if forced:
                    q=magic_coeffect[co_index];co_index+=1
                    actor=(601 if provider in (1,3) else 1000) if defensive else 14162 if provider in (1,3) else 600
                    owned=249 if provider in (2,3) or not defensive and provider==4 else effect
                    assert (q['stage'],int(q['effect_skill']),int(q['person_id']),int(q['owned_skill']))==('check_matched',effect,actor,owned)
                    magic_coeffect_row(magic_coeffect[co_index],effect,actor,owned,stat,defensive);co_index+=1
                result=0 if forced and defensive else 1 if forced else int(((4*0x6c078965+0x3039)&0xffffffff)>>16)%100<60
                r=magic_coeffect[co_index];co_index+=1
                assert r['event']=='magic_target_decision' and r['effect_skill']=='70'
                assert json.loads(r['context_json'])['native_result']==int(result)
    for effect,actor,defensive,stat in ((61,1000,True,100),(57,600,False,100),(60,1000,True,60)):
        assert magic_coeffect[co_index]['event']=='magic_eligibility';co_index+=1
        q=magic_coeffect[co_index];co_index+=1
        assert (q['stage'],int(q['effect_skill']))==('check_matched',effect)
        magic_coeffect_row(magic_coeffect[co_index],effect,actor,249,stat,defensive);co_index+=1
        assert magic_coeffect[co_index]['event']=='magic_target_decision';co_index+=1
    assert co_index==len(magic_coeffect)==262
    preview=rows('magic_coeffect_preview_test.csv')
    assert len(preview)==1 and preview[0]['stage']=='check_matched' and preview[0]['effect_skill']=='61'
    reach=rows('reach_strategy_test.csv');reach_index=0
    reach_costs=(10,10,15,15,10,10,20,50,50)
    def reach_row(actual,actor,owned,plan,distance,normal,energy,success):
        after=min(100,max(0,energy-reach_costs[plan]))
        assert (actual['event'],actual['scene'],actual['stage'],int(actual['person_id']),int(actual['owned_skill']),actual['effect_skill'],
            actual['force_id'],actual['target_unit'],actual['target_base'],actual['target_city'],actual['actor_unit'],actual['equipment_id'])==(
            'extended_strategy_command_resolved','strategy','applied',actor,owned,'65','2','4094','-1','-1','4095','3')
        assert actual['target_force']==('2' if plan in (1,4) else '3') and (actual['target_x'],actual['target_y'])==('1',str(1+distance))
        assert int(actual['caller'])==0x5933a0 and not actual['metric'] and not actual['baseline_after'] and not actual['bonus_delta']
        assert json.loads(actual['context_json'])==dict(strategy_id=plan,distance=distance,normal_range_max=normal,skill_range_max=normal+1,
            native_permission_mask=1<<plan,energy_before=energy,energy_after=after,requested_energy_delta=-reach_costs[plan],native_command_return=success,
            result_scope='paid_extended_range_command_not_target_outcome',counterfactual_evaluated=False)
    for provider in range(6):
        for plan in range(9):
            for technology in (0,1):
                for distance in range(1,5):
                    for energy in (0,1,100,255):
                        for mode in (0,1):
                            for success in (0,1):
                                actor=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3,4) else 65
                                normal=(3 if technology else 1) if plan==0 else 1 if plan in (5,6) else 2
                                if mode or provider==5:continue
                                query=reach[reach_index];reach_index+=1
                                assert (query['event'],query['scene'],query['stage'],int(query['person_id']),int(query['owned_skill']),query['effect_skill'],query['force_id'])==(
                                    'extended_range_permission_checked','strategy','check_matched',actor,owned,'65','2')
                                if distance==normal+1 and energy:
                                    reach_row(reach[reach_index],actor,owned,plan,distance,normal,energy,success);reach_index+=1
    assert reach_index==len(reach)==3420
    reach_edges=rows('reach_strategy_edges_test.csv')
    assert [r['event'] for r in reach_edges]==['extended_range_permission_checked']*8+['extended_strategy_command_resolved']+['extended_range_permission_checked']*3
    reach_row(reach_edges[8],600,65,2,3,2,100,1)
    reach_applied=[r for r in reach+reach_edges if r['stage']=='applied']
    assert len(reach_applied)==541
    assert len({r['caller'] for r in reach+reach_edges if r['stage']=='check_matched'})==1
    steering=rows('steering_test.csv');steering_index=0
    def steering_row(r,actor,weapon,x,y,from_pos,to_pos,count,ordinary,actual,terrain):
        assert (r['stage'],r['scene'],r['event'],r['effect_skill'],r['owned_skill'],int(r['person_id']),r['force_id'],r['ruler_id'],r['actor_city'])==(
            'applied','movement','steering_water_cell_committed','5','5',actor,'2','635','12')
        assert (r['actor_unit'],r['target_unit'],r['target_force'],r['target_base'],r['target_city'],r['target_person'])==('17','17','2','-1','-1','-1')
        assert (int(r['equipment_id']),int(r['target_x']),int(r['target_y']),int(r['terrain_id']),int(r['caller']))==(weapon,x,y,terrain,0x584ad1)
        assert all(not r[k] for k in ('metric','before','after','delta','baseline_after','bonus_delta'))
        expected={'from':from_pos,'to':to_pos,'route_cells':count,'base_cost_without_5':ordinary,'base_cost_with_5':actual,
            'result_scope':'committed_route_base_terrain_input','final_step_cost_evaluated':False,'monsoon_trade_fire_shore_adjustments_evaluated':False}
        assert json.loads(r['context_json'])==expected
    for width,height in ((200,200),(249,530),(530,249)):
        x,y=height-3,width-5
        for provider in range(6):
            for transport in range(2):
                for weapon in range(9,12):
                    for table in range(5):
                        if provider>=4 or table in (1,2):continue
                        for offset,terrain in ((1,7),(3,8)):
                            ship=9 if transport else weapon;cost=8+2*(ship-9)+(terrain==8)
                            steering_row(steering[steering_index],(600,14162,1000,600)[provider],ship,x,y+offset,
                                x|(y<<16),x|((y+3)<<16),3,255 if table==3 else cost,cost+2 if table==4 else cost-1,terrain)
                            steering_index+=1
    assert steering_index==len(steering)==432
    steering_edges=rows('steering_edges_test.csv');assert len(steering_edges)==41
    steering_row(steering_edges[0],600,9,2,2,0x10002,0x20002,1,8,7,7)
    for n in range(40):steering_row(steering_edges[n+1],600,9,2,n+2,0x10002,0x290002,40,8,7,7)
    steering_entry=rows('steering_entry_test.csv');entry_index=0
    def steering_entry_row(row,actor,weapon,x,y,start,end,count,normal,actual,terrain,base,city,transfer=False,location=None):
        assert (row['stage'],row['scene'],row['event'],row['owned_skill'],row['effect_skill'],row['person_id'])==('applied','movement','steering_water_cell_entered_base','5','5',str(actor))
        assert (row['force_id'],row['ruler_id'],row['actor_city'],row['actor_unit'],row['target_unit'],row['target_force'])==('2','635','12','17','17','2')
        assert (row['target_base'],row['target_city'],row['target_person'],row['equipment_id'],row['target_x'],row['target_y'],row['terrain_id'],row['caller'])==(str(base),str(city),str(actor),str(weapon),str(x),str(y),str(terrain),str(0x4bf636 if transfer else 0x4bf61f))
        assert json.loads(row['context_json'])=={
            'from':start,'to':end,'route_cells':count,'base_cost_without_5':normal,'base_cost_with_5':actual,
            'result_scope':'native_base_entry_transfer_committed' if transfer else 'base_entry_member_location_committed','final_step_cost_evaluated':False,
            'monsoon_trade_fire_shore_adjustments_evaluated':False,'entered_base':base,'member_location_after':base if location is None else location}
    for width,height in ((200,200),(249,530),(530,249)):
        x,y=height-3,width-5
        for base,city in ((23,23),(42,-1),(52,-1),(529,529)):
            for provider in range(6):
                for transport in (0,1):
                    if provider>=4:continue
                    actor=(600,14162,1000,600)[provider];weapon=9 if transport else 10
                    for offset,terrain in ((1,7),(3,8)):
                        normal=8+2*(weapon-9)+(terrain==8)
                        steering_entry_row(steering_entry[entry_index],actor,weapon,x,y+offset,x|(y<<16),x|((y+3)<<16),3,normal,normal-1,terrain,base,city);entry_index+=1
    assert entry_index==len(steering_entry)==192
    steering_entry_edges=rows('steering_entry_edges_test.csv');assert len(steering_entry_edges)==43
    for row in steering_entry_edges[:2]+steering_entry_edges[-1:]:
        steering_entry_row(row,600,9,2,2,0x10002,0x20002,1,8,7,7,23,23)
    for index,row in enumerate(steering_entry_edges[2:-1]):
        steering_entry_row(row,600,9,2,index+2,0x10002,0x290002,40,8,7,7,23,23)
    native_transfer=rows('entry_native_transfer_test.csv');native_transfer_index=0
    for base in (23,529):
        for mode in range(3):
            for cap in (1000,4990,5000):
                for provider in range(6):
                    if provider>=4:continue
                    actor=(600,14162,1000,600)[provider];weapon=10 if mode==0 else 9
                    for offset,terrain in ((1,7),(3,8)):
                        normal=8+2*(weapon-9)+(terrain==8)
                        steering_entry_row(native_transfer[native_transfer_index],actor,weapon,10,10+offset,0xa000a,0xd000a,3,normal,normal-1,terrain,base,base,mode==2,5000 if mode==2 else base);native_transfer_index+=1
    assert native_transfer_index==len(native_transfer)==144
    native_transfer_edges=rows('entry_native_transfer_edges_test.csv');native_transfer_index=0
    for mutation in (0,1,7,8):
        for offset,terrain in ((1,7),(3,8)):
            normal=10+(terrain==8)
            steering_entry_row(native_transfer_edges[native_transfer_index],600,10,10,10+offset,0xa000a,0xd000a,3,normal,normal-1,terrain,23,23,mutation==1,5000 if mutation==1 else 23);native_transfer_index+=1
    assert native_transfer_index==len(native_transfer_edges)==8
    movement_map=rows('movement_map_test.csv');assert len(movement_map)==16
    for layout,(width,height) in enumerate(((249,530),(530,249))):
        for i,(effect,terrain,event) in enumerate(((6,11,'cliff_immunity'),(8,4,'poison_immunity'),(62,0,'terrain_fire_immunity'),(31,0,'terrain_fire_damage_doubled'))):
            q,a=movement_map[layout*8+i*2:layout*8+i*2+2]
            assert (q['stage'],q['event'],int(q['effect_skill']),q['person_id'])==('check_matched','movement_hazard_eligibility',effect,'600')
            assert (a['stage'],a['event'],int(a['effect_skill']),a['person_id'],a['target_unit'],a['actor_unit'],a['target_force'])==('applied',event,effect,'600','17','17','2')
            assert (int(a['target_x']),int(a['target_y']),int(a['terrain_id']),int(a['caller']))==(height-2,width-2,terrain,0x5848a9)
            if effect==31:assert (a['metric'],a['before'],a['after'],a['baseline_after'],a['bonus_delta'])==('route_hazard_damage','67','134','67','67')
    attack_reach=rows('attack_reach_test.csv');attack_index=0
    def attack_reach_row(row,effect,actor,owned,weapon,distance,normal,maximum,terrain,x,y,before,caller,tactic=-1,shared_write=None):
        event='extended_range_attack_settled' if effect==24 else 'forest_ranged_attack_settled'
        if tactic>=0:event='extended_range_tactic_settled' if effect==24 else 'forest_ranged_tactic_settled'
        assert (row['stage'],row['scene'],row['event'],int(row['effect_skill']),int(row['owned_skill']),int(row['person_id']))==('applied','battle',event,effect,owned,actor)
        assert (row['force_id'],row['ruler_id'],row['actor_city'],row['target_force'],row['target_unit'],row['actor_unit'],row['target_base'],row['target_city'])==('2','635','8','3','4094','4095','-1','-1')
        assert (int(row['equipment_id']),int(row['target_x']),int(row['target_y']),int(row['terrain_id']),int(row['caller']))==(weapon,x,y,terrain,caller)
        assert (row['metric'],int(row['before']),int(row['after']),int(row['delta']),row['baseline_after'],row['bonus_delta'])==('troops',before,max(0,before-250),-min(250,before),'','')
        mask=1 if tactic<0 else 1<<tactic
        if mask>=1<<31:mask-=1<<32
        context=json.loads(row['context_json'])
        if shared_write is not None:assert context.pop('shared_resource_write_id',None)==shared_write
        assert context==dict(tactic_id=tactic,record_kind=0,distance=distance,normal_range_max=normal,actual_range_max=maximum,native_permission_mask=mask,
            requested_troop_loss=250,damage_attribution='whole_attack_not_marginal_skill_bonus',counterfactual_evaluated=False)
    def attack_reach_query(row,effect,actor,owned,tactic=False):
        event='attack_range_permission_revalidation' if effect==24 else 'forest_attack_permission_revalidation'
        if tactic:event='tactic_range_permission_revalidation' if effect==24 else 'tactic_forest_permission_revalidation'
        assert (row['stage'],row['scene'],row['event'],int(row['effect_skill']),int(row['owned_skill']),int(row['person_id']),row['force_id'])==(
            'check_matched','battle',event,effect,owned,actor,'2')
    for width,height in ((200,200),(249,530)):
        x,y=height-10,width-10
        for weapon in (3,6,7):
            for provider in range(6):
                for tech in range(2):
                    for distance in range(1,6):
                        for terrain in (0,5):
                            for allow in range(2):
                                for before in (0,100,1000,10000):
                                    for route in range(2):
                                        effect=50 if weapon==3 else 24;actor=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3,4) else effect
                                        query=provider!=5 and (weapon!=3 or terrain==5 and 1<distance<=2+tech)
                                        normal=2+tech if weapon==3 else 2 if weapon==6 else 3
                                        maximum=normal+(weapon!=3)
                                        if query:
                                            attack_reach_query(attack_reach[attack_index],effect,actor,owned);attack_index+=1
                                        if query and allow and (weapon==3 or distance==maximum):
                                            attack_reach_row(attack_reach[attack_index],effect,actor,owned,weapon,distance,normal,maximum,terrain,x,y+distance,before,0x5b1c6c if route else 0x584fdf);attack_index+=1
    assert attack_index==len(attack_reach)==7760
    attack_edges=rows('attack_reach_edges_test.csv');attack_index=0
    for effect in (24,50):
        for mutation in range(17):
            if mutation in (11,12):continue
            attack_reach_query(attack_edges[attack_index],effect,600,effect);attack_index+=1
            if mutation==0:
                distance=3 if effect==24 else 2
                attack_reach_row(attack_edges[attack_index],effect,600,effect,6 if effect==24 else 3,distance,2,3 if effect==24 else 2,0 if effect==24 else 5,1,1+distance,1000,0x584fdf);attack_index+=1
    assert attack_index==len(attack_edges)==32
    attack_copy=rows('attack_reach_copy_test.csv');attack_index=0
    for effect in (24,50):
        for route in range(2):
            attack_reach_query(attack_copy[attack_index],effect,600,effect);attack_index+=1
            distance=3 if effect==24 else 2
            attack_reach_row(attack_copy[attack_index],effect,600,effect,6 if effect==24 else 3,distance,2,3 if effect==24 else 2,0 if effect==24 else 5,1,1+distance,1000,0x5b1c6c);attack_index+=1
    assert attack_index==len(attack_copy)==8
    attack_reach_all=attack_reach+attack_edges+attack_copy;attack_reach_applied=sum(r['stage']=='applied' for r in attack_reach_all)
    assert attack_reach_applied==886
    attack_tactic=rows('attack_tactic_test.csv');tactic_index=0
    for width,height in ((200,200),(249,530)):
        x,y=height-10,width-10
        for tactic in (12,15,6,7,8,31):
            effect=24 if tactic in (12,15) else 50
            for provider in range(6):
                actor=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3,4) else effect
                for available in range(2):
                    for natural in range(2):
                        for allow in range(2):
                            for distance in range(1,5):
                                for route in range(2):
                                    for tech in range(2):
                                        if effect==24 and (natural or tech):continue
                                        maximum=3 if effect==24 else 2+(tactic in (6,7,8) and tech)
                                        query=provider!=5 and available and (effect==24 or not natural and distance<=maximum)
                                        if query:
                                            attack_reach_query(attack_tactic[tactic_index],effect,actor,owned,True);tactic_index+=1
                                        if query and allow and (effect==50 or distance==3):
                                            attack_reach_row(attack_tactic[tactic_index],effect,actor,owned,3 if effect==50 else 6 if tactic==12 else 7,
                                                distance,2 if effect==24 else maximum,maximum,5 if effect==50 else 0,x,y+distance,1000,
                                                0x5b1c6c if route else 0x584fdf,tactic);tactic_index+=1
    assert tactic_index==len(attack_tactic)==1500
    tactic_edges=rows('attack_tactic_edges_test.csv');tactic_index=0
    for effect in (24,50):
        for route in range(2):
            for mutation in range(14):
                attack_reach_query(tactic_edges[tactic_index],effect,600,effect,True);tactic_index+=1
                if mutation==0:
                    distance=3 if effect==24 else 2
                    attack_reach_row(tactic_edges[tactic_index],effect,600,effect,6 if effect==24 else 3,distance,2,3 if effect==24 else 2,
                        0 if effect==24 else 5,1,1+distance,1000,0x5b1c6c if route else 0x584fdf,12 if effect==24 else 6);tactic_index+=1
    assert tactic_index==len(tactic_edges)==60
    tactic_all=attack_tactic+tactic_edges;tactic_applied=sum(r['stage']=='applied' for r in tactic_all)
    assert tactic_applied==424
    shared_write_ids=set()
    def coeffect_case(data,index,tactic,skills,natural,allow,distance,before,caller,x,y,commit=True,retain=(True,True)):
        first,other=skills
        holders={effect:next(((person,skill) for person,skill in ((600,first),(14162,other)) if skill in (effect,249)),None) for effect in (24,50)}
        has24,has50=bool(holders[24]),bool(holders[50]);maximum=2+has24
        queries=[]
        if has24:queries.append((24,False))
        if has24 and has50 and distance==3 and not natural:queries.append((50,True))
        if has50 and has24:queries.append((24,True))
        if has50 and not natural and distance<=maximum:queries.append((50,False))
        for effect,generic in queries:
            actor,owned=holders[effect];row=data[index];index+=1
            if generic:
                assert (row['stage'],row['scene'],row['event'],int(row['effect_skill']),int(row['person_id']),int(row['owned_skill']),row['force_id'])==(
                    'check_matched','unclassified_query','skill_predicate',effect,actor,owned,'2')
            else:attack_reach_query(row,effect,actor,owned,True)
        effects=[]
        if commit and allow and has24 and distance==3 and (natural or has50) and retain[0]:effects.append(24)
        if commit and allow and has50 and not natural and distance<=maximum and retain[1]:effects.append(50)
        shared=None
        if len(effects)==2:
            shared=json.loads(data[index]['context_json'])['shared_resource_write_id']
            assert type(shared) is int and shared>0 and shared not in shared_write_ids;shared_write_ids.add(shared)
            assert all(data[index][key]==data[index+1][key] for key in ('pid','thread','caller','actor_unit','target_unit','before','after','delta','metric'))
        for effect in effects:
            actor,owned=holders[effect]
            attack_reach_row(data[index],effect,actor,owned,3,distance,2 if effect==24 else maximum,maximum,5,x,y+distance,before,caller,tactic,shared);index+=1
        return index
    coeffects=rows('attack_coeffect_test.csv');co_index=0
    providers=((24,50),(50,24),(249,-1),(24,-1),(50,-1),(-1,-1),(249,249),(24,249),(-1,249))
    for width,height in ((200,200),(249,530)):
        for tactic in (12,15):
            for skills in providers:
                for natural in range(2):
                    for allow in range(2):
                        for distance in range(1,5):
                            for route in range(2):
                                for before in (0,100,1000,10000):
                                    co_index=coeffect_case(coeffects,co_index,tactic,skills,natural,allow,distance,before,0x5b1c6c if route else 0x584fdf,height-10,width-10)
    assert co_index==len(coeffects)==9376 and len(shared_write_ids)==192
    co_copy=rows('attack_coeffect_copy_test.csv');co_index=0
    for tactic in (12,15):
        for skills in ((24,50),(50,-1),(249,-1),(-1,249)):
            distance=3 if skills in ((24,50),(249,-1)) else 2
            for route in range(2):co_index=coeffect_case(co_copy,co_index,tactic,skills,0,1,distance,1000,0x5b1c6c,1,1)
    assert co_index==len(co_copy)==72 and len(shared_write_ids)==200
    co_stale=rows('attack_coeffect_stale_test.csv');co_index=0
    for tactic in (12,15):
        for route in range(2):
            for mutation in range(11):
                retain=(mutation not in (1,3,5),mutation not in (2,4,6)) if mutation<7 else (False,False)
                co_index=coeffect_case(co_stale,co_index,tactic,(24,50),0,1,3,1000,0x5b1c6c if route else 0x584fdf,1,1,retain=retain)
    assert co_index==len(co_stale)==208 and len(shared_write_ids)==204
    co_aggregate=rows('attack_coeffect_aggregate_test.csv');co_index=0
    for tactic in (12,15):
        for skills in ((24,50),(50,-1),(249,-1),(-1,249)):
            distance=3 if skills in ((24,50),(249,-1)) else 2
            for count in (0,1,2):co_index=coeffect_case(co_aggregate,co_index,tactic,skills,0,1,distance,1000,0x5b1c6c,1,1,commit=count==1)
    assert co_index==len(co_aggregate)==84 and len(shared_write_ids)==208
    co_all=coeffects+co_copy+co_stale+co_aggregate;co_applied=sum(row['stage']=='applied' for row in co_all)
    assert co_applied==1124 and len(co_all)==9740
    def bole_query(row,actor):
        assert (row['stage'],row['scene'],row['event'],row['owned_skill'],row['effect_skill'],int(row['person_id']),row['force_id'])==(
            'check_matched','officer_recruitment','remote_candidate_eligibility','108','146',actor,'2')
    def bole_applied(row,actor,affinity,queued,outcome):
        after_force=-1 if queued or not outcome else 2
        assert (row['stage'],row['scene'],row['event'],row['owned_skill'],row['effect_skill'],int(row['person_id']))==(
            'applied','officer_recruitment','remote_recruitment_order_assigned' if queued else 'remote_recruitment_attempt_resolved','108','146',actor)
        assert (row['force_id'],row['ruler_id'],row['actor_city'],row['target_person'],int(row['target_force']),row['target_base'],row['target_city'])==('2','635','8','1001',after_force,'60','60')
        assert int(row['caller'])==0x5c5940 and all(row[key]=='' for key in ('metric','before','after','delta','baseline_after','bonus_delta'))
        assert all(row[key]=='-1' for key in ('actor_unit','target_unit','equipment_id','target_x','target_y','terrain_id'))
        assert json.loads(row['context_json'])==dict(source_base=8,command_force=2,executor_person=1000,candidate_location=60,candidate_location_force=3,
            affinity_distance=affinity,native_outcome=-1 if queued else outcome,order_assigned=bool(queued),order_parameter_2=3 if queued else -1,
            status_before=4,status_after=4 if queued or not outcome else 3,force_before=-1,force_after=after_force,
            location_after=60 if queued or not outcome else 8,counterfactual_evaluated=False)
    bole=rows('bole_test.csv');bole_index=0
    for holder in range(5):
        for affinity in (0,25,26):
            for mode in range(2):
                for outcome in range(2):
                    for append_fail in range(2):
                        for same in range(2):
                            for parent in range(2):
                                if holder!=3 and not same:
                                    actor=14162 if holder==1 else 1000 if holder==4 else 600
                                    bole_query(bole[bole_index],actor);bole_index+=1
                                    if affinity<=25 and not append_fail and parent:
                                        bole_applied(bole[bole_index],actor,affinity,mode,outcome);bole_index+=1
    assert bole_index==len(bole)==224
    bole_edges=rows('bole_edges_test.csv');bole_index=0
    for mutation in range(15):
        for mode in range(2):
            if mutation>=9 and not mode:continue
            bole_query(bole_edges[bole_index],600);bole_index+=1
            if mutation==0:bole_applied(bole_edges[bole_index],600,25,mode,1);bole_index+=1
    assert bole_index==len(bole_edges)==26
    bole_unwind=rows('bole_unwind_test.csv');assert len(bole_unwind)==1;bole_query(bole_unwind[0],600)
    bole_all=bole+bole_edges+bole_unwind;bole_applied_count=sum(row['stage']=='applied' for row in bole_all)
    assert bole_applied_count==34
    zoc=rows('zoc_route_test.csv');zoc_index=0
    def zoc_query(r,effect,actor,owned):
        assert (r['stage'],r['scene'],r['event'],int(r['effect_skill']),int(r['person_id']),int(r['owned_skill']),r['force_id'])==(
            'check_matched','movement','zoc_flags_revalidation',effect,actor,owned,'2')
    def zoc_applied(r,effect,actor,owned,terrain,equipment,x,y,offset,count,base=False,blocker_position=None,object_id=500,base_position=None):
        assert (r['stage'],r['scene'],r['event'],int(r['effect_skill']),int(r['person_id']),int(r['owned_skill']),r['force_id'],r['ruler_id'],r['actor_city'])==(
            'applied','movement','zoc_bypass_route_cell_committed',effect,actor,owned,'2','635','12')
        assert (r['target_unit'],r['actor_unit'],r['target_force'],r['target_base'],r['target_city'],r['target_person'])==('17','17','2','-1','-1','-1')
        assert (int(r['target_x']),int(r['target_y']),int(r['terrain_id']),int(r['equipment_id']),int(r['caller']))==(x,y+offset,terrain,equipment,0x584ad1)
        assert all(not r[k] for k in ('metric','before','after','delta','baseline_after','bonus_delta'))
        expected={'from':x|(y<<16),'to':x|((y+count)<<16),'route_cells':count,'steps_after_cell':count-offset,
            'blocker_base' if base else 'blocker_unit':23 if base else 18,'blocker_force':3,'blocker_position':(x-1)|((y+1+(x&1))<<16) if blocker_position is None else blocker_position,
            'result_scope':'committed_route_continued_past_native_zoc_stop','movement_point_saving_evaluated':False,'counterfactual_path_evaluated':False}
        if base:expected.update(blocker_object=object_id,blocker_base_position=expected['blocker_position'] if base_position is None else base_position)
        assert json.loads(r['context_json'])==expected
    for x,y in ((194,194),(525,243)):
        for effect in (0,1,4):
            for provider in range(6):
                for terrain in (0,7,8):
                    for weapon in (4,6):
                        for blocker in range(6):
                            actor=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3,4) else effect
                            query=provider!=5 and (effect==4 or weapon==4)
                            bypassed=query and blocker==1 and (terrain!=0 if effect==4 else terrain==0)
                            if query:zoc_query(zoc[zoc_index],effect,actor,owned);zoc_index+=1
                            if bypassed:
                                for offset in (1,2):
                                    zoc_applied(zoc[zoc_index],effect,actor,owned,terrain,9 if terrain else weapon,x,y,offset,3);zoc_index+=1
    assert zoc_index==len(zoc)==840
    zoc_priority=rows('zoc_priority_test.csv');zoc_index=0
    priorities=[[(0,14162,0)],[(0,600,249),(4,600,249)],[(1,600,1),(4,14162,4)],[(1,14162,1),(4,600,4)],[(0,14162,0),(4,1000,4)],[(1,600,1),(4,14162,4)]]
    for scenario,events in enumerate(priorities):
        for effect,actor,owned in events:zoc_query(zoc_priority[zoc_index],effect,actor,owned);zoc_index+=1
        for effect,actor,owned in events:
            zoc_applied(zoc_priority[zoc_index],effect,actor,owned,7 if effect==4 else 0,0 if scenario==5 else 4,10,10,2 if effect==4 else 1,3);zoc_index+=1
    assert zoc_index==len(zoc_priority)==22
    zoc_preview=rows('zoc_preview_test.csv');assert len(zoc_preview)==2
    for r,effect in zip(zoc_preview,(1,4)):zoc_query(r,effect,600,effect)
    zoc_edges=rows('zoc_edges_test.csv');zoc_index=0
    for effect in (1,4):
        for mutation in range(19):
            if mutation in (11,13,14):continue
            zoc_query(zoc_edges[zoc_index],effect,600,effect);zoc_index+=1
            if mutation in (0,15):
                for offset in (1,2):
                    zoc_applied(zoc_edges[zoc_index],effect,600,effect,0 if effect==1 else 7,4 if effect==1 else 9,10,10,offset,40 if mutation==15 else 3);zoc_index+=1
    assert zoc_index==len(zoc_edges)==40
    zoc_all=zoc+zoc_priority+zoc_preview+zoc_edges;zoc_applied_count=sum(r['stage']=='applied' for r in zoc_all)
    assert len(zoc_all)==904 and zoc_applied_count==139
    zoc_base=rows('zoc_base_test.csv');zoc_base_index=0
    for x,y in ((194,194),(525,243)):
        for effect in (0,1,4):
            for provider in range(6):
                for terrain in (0,7,8):
                    for weapon in (4,6):
                        for scenario in range(10):
                            actor=14162 if provider in (1,3) else 600;owned=249 if provider in (2,3,4) else effect
                            query=provider!=5 and (effect==4 or weapon==4)
                            bypassed=query and scenario in (0,8,9) and (terrain!=0 if effect==4 else terrain==0)
                            if query:zoc_query(zoc_base[zoc_base_index],effect,actor,owned);zoc_base_index+=1
                            if bypassed:
                                for offset in (1,2):
                                    unit_first=scenario==9 and offset==1
                                    zoc_applied(zoc_base[zoc_base_index],effect,actor,owned,terrain,9 if terrain else weapon,x,y,offset,3,
                                        base=not unit_first,blocker_position=(x-1)|((y+(x&1))<<16) if unit_first else None,object_id=65534 if scenario==8 else 500);zoc_base_index+=1
    assert zoc_base_index==len(zoc_base)==1560
    zoc_base_preview=rows('zoc_base_preview_test.csv');assert len(zoc_base_preview)==2
    for row,effect in zip(zoc_base_preview,(1,4)):zoc_query(row,effect,600,effect)
    zoc_base_edges=rows('zoc_base_edges_test.csv');zoc_base_index=0
    for effect in (1,4):
        for mutation in range(22):
            if mutation in (19,20):continue
            zoc_query(zoc_base_edges[zoc_base_index],effect,600,effect);zoc_base_index+=1
            if mutation in (0,18,21):
                for offset in (1,2):
                    zoc_applied(zoc_base_edges[zoc_base_index],effect,600,effect,0 if effect==1 else 7,4 if effect==1 else 9,10,10,offset,40 if mutation==21 else 3,base=True);zoc_base_index+=1
    assert zoc_base_index==len(zoc_base_edges)==52
    zoc_footprint=rows('zoc_base_footprint_test.csv');zoc_base_index=0
    for effect in (1,4):
        for object_id in (500,65534):
            for mutation in range(4):
                zoc_query(zoc_footprint[zoc_base_index],effect,600,effect);zoc_base_index+=1
                if mutation in (0,3):
                    for offset in (1,2):
                        zoc_applied(zoc_footprint[zoc_base_index],effect,600,effect,0 if effect==1 else 7,4 if effect==1 else 9,10,10,offset,3,base=True,object_id=object_id,base_position=8|(11<<16));zoc_base_index+=1
    assert zoc_base_index==len(zoc_footprint)==32
    zoc_base_all=zoc_base+zoc_base_preview+zoc_base_edges+zoc_footprint;zoc_base_applied=sum(r['stage']=='applied' for r in zoc_base_all)
    assert len(zoc_base_all)==1646 and zoc_base_applied==388
    def zoc_entry_applied(row,effect,actor,owned,terrain,weapon,x,y,offset,base=False,transfer=False,location=None,entry_base=23):
        assert (row['stage'],row['scene'],row['event'],row['effect_skill'],row['owned_skill'],row['person_id'])==('applied','movement','zoc_bypass_route_cell_entered_base',str(effect),str(owned),str(actor))
        assert (row['force_id'],row['ruler_id'],row['actor_city'],row['target_unit'],row['actor_unit'],row['target_force'],row['target_base'],row['target_city'],row['target_person'])==('2','635','12','17','17','2',str(entry_base),str(entry_base),str(actor))
        assert (row['target_x'],row['target_y'],row['terrain_id'],row['equipment_id'],row['caller'])==(str(x),str(y+offset),str(terrain),str(weapon),str(0x4bf636 if transfer else 0x4bf61f))
        enemy=(x-1)|((y+1+(x&1))<<16)
        expected={'from':x|(y<<16),'to':x|((y+3)<<16),'route_cells':3,'steps_after_cell':3-offset,
            'blocker_base' if base else 'blocker_unit':24 if base else 18,'blocker_force':3,'blocker_position':enemy,
            'result_scope':'native_base_entry_transfer_continued_past_native_zoc_stop' if transfer else 'base_entry_route_continued_past_native_zoc_stop','movement_point_saving_evaluated':False,
            'counterfactual_path_evaluated':False,'entered_base':entry_base,'member_location_after':entry_base if location is None else location}
        if base:expected.update(blocker_object=501,blocker_base_position=enemy)
        assert json.loads(row['context_json'])==expected
    zoc_entry=rows('zoc_entry_test.csv');zoc_entry_index=0
    for x,y in ((10,10),(525,243)):
        for effect in (0,1,4):
            for provider in range(4):
                for base in (False,True):
                    actor=14162 if provider==1 else 600;owned=249 if provider==2 else effect
                    zoc_query(zoc_entry[zoc_entry_index],effect,actor,owned);zoc_entry_index+=1
                    for offset in (1,2):
                        zoc_entry_applied(zoc_entry[zoc_entry_index],effect,actor,owned,7 if effect==4 else 0,9 if effect==4 else 4,x,y,offset,base);zoc_entry_index+=1
    assert zoc_entry_index==len(zoc_entry)==144
    zoc_entry_edges=rows('zoc_entry_edges_test.csv');zoc_entry_index=0
    for effect in (1,4):
        for base in (False,True):
            for mutation in range(19):
                if mutation==16:continue
                zoc_query(zoc_entry_edges[zoc_entry_index],effect,600,effect);zoc_entry_index+=1
                if mutation in (0,17,18):
                    for offset in (1,2):
                        zoc_entry_applied(zoc_entry_edges[zoc_entry_index],effect,600,effect,7 if effect==4 else 0,9 if effect==4 else 4,10,10,offset,base);zoc_entry_index+=1
    assert zoc_entry_index==len(zoc_entry_edges)==96
    zoc_entry_coeffects=rows('zoc_entry_coeffects_test.csv');zoc_entry_index=0
    for profile in range(4):
        first=0 if profile in (1,3) else 1;owned=249 if first==0 else 1;water_actor=600 if first==0 else 14162
        zoc_query(zoc_entry_coeffects[zoc_entry_index],first,600,owned);zoc_entry_index+=1
        zoc_query(zoc_entry_coeffects[zoc_entry_index],4,water_actor,249 if first==0 else 4);zoc_entry_index+=1
        if profile!=2:
            zoc_entry_applied(zoc_entry_coeffects[zoc_entry_index],first,600,owned,0,4,10,10,1);zoc_entry_index+=1
        zoc_entry_applied(zoc_entry_coeffects[zoc_entry_index],4,water_actor,249 if first==0 else 4,7,4,10,10,2);zoc_entry_index+=1
        steering_entry_row(zoc_entry_coeffects[zoc_entry_index],14162 if profile==1 else 1000,10,10,12,0xa000a,0xd000a,3,10,9,7,23,23);zoc_entry_index+=1
    assert zoc_entry_index==len(zoc_entry_coeffects)==19
    assert sum(r['stage']=='applied' for r in zoc_entry+zoc_entry_edges+zoc_entry_coeffects)==131
    transfer_zoc=rows('entry_transfer_zoc_test.csv');transfer_zoc_index=0
    for effect in (1,4):
        for base in (False,True):
            zoc_query(transfer_zoc[transfer_zoc_index],effect,600,effect);transfer_zoc_index+=1
            for offset in (1,2):
                zoc_entry_applied(transfer_zoc[transfer_zoc_index],effect,600,effect,7 if effect==4 else 0,9 if effect==4 else 4,10,10,offset,base,True,5000);transfer_zoc_index+=1
    for profile in range(3):
        zoc_query(transfer_zoc[transfer_zoc_index],1,600,1);transfer_zoc_index+=1
        zoc_query(transfer_zoc[transfer_zoc_index],4,14162,4);transfer_zoc_index+=1
        if profile==2:continue
        steering_entry_row(transfer_zoc[transfer_zoc_index],1000,10,10,12,0xa000a,0xd000a,3,10,9,7,23,23,True,5000);transfer_zoc_index+=1
        if profile==0:
            zoc_entry_applied(transfer_zoc[transfer_zoc_index],1,600,1,0,4,10,10,1,False,True,5000);transfer_zoc_index+=1
        zoc_entry_applied(transfer_zoc[transfer_zoc_index],4,14162,4,7,4,10,10,2,False,True,5000);transfer_zoc_index+=1
    assert transfer_zoc_index==len(transfer_zoc)==23
    native_transfer_zoc=rows('entry_native_transfer_zoc_test.csv');native_transfer_zoc_index=0
    for entry_base in (23,529):
        for effect in (0,1,4):
            for mode in range(3):
                for provider in range(4):
                    for base in (False,True):
                        actor=14162 if provider==1 else 600;owned=249 if provider==2 else effect
                        zoc_query(native_transfer_zoc[native_transfer_zoc_index],effect,actor,owned);native_transfer_zoc_index+=1
                        for offset in (1,2):
                            zoc_entry_applied(native_transfer_zoc[native_transfer_zoc_index],effect,actor,owned,7 if effect==4 else 0,
                                9 if effect==4 else 4,10,10,offset,base,mode==2,5000 if mode==2 else entry_base,entry_base)
                            native_transfer_zoc_index+=1
    assert native_transfer_zoc_index==len(native_transfer_zoc)==432
    print(json.dumps(dict(passed=True, columns=36, concurrent_rows=len(concurrent),live_read_rows=len(live_read),actor_metadata_rows=76,actor_metadata_applied_rows=76,
                          casualty_rows=204,casualty_applied_rows=92,casualty_query_rows=112,
                          fire_aftermath_rows=41,fire_aftermath_applied_rows=15,fire_aftermath_query_rows=26,
                          precision_rows=len(precision), eligibility_rows=len(eligibility),
                          precision_total_rows=len(precision_total),precision_total_applied_rows=553,precision_total_query_rows=574,
                          upkeep_rows=len(upkeep), income_bonus_rows=len(applied), domestic_applied_rows=len(applied_domestic),
                          income_context_rows=159,income_context_applied_rows=12,income_context_query_rows=147,
                          income_native_collection_rows=53,income_native_collection_applied_rows=12,income_native_collection_query_rows=41,
                          battle_applied_rows=len(battle_applied), battle_query_rows=len(battle_queries),
                          recovery_boundary_applied_rows=10,recovery_boundary_query_rows=len(recovery_source)+len(recovery_session)+len(recovery_context)-10,
                          pressure_applied_rows=len(pressure_applied), pressure_query_rows=len(pressure_queries),
                          movement_applied_rows=len(movement_applied), movement_query_rows=len(movement_queries),
                          movement_stat_applied_rows=len(stat_applied), movement_stat_query_rows=len(stat_queries),
                          revenue_applied_rows=len(revenue_applied), revenue_query_rows=len(revenue_queries),
                          unrest_applied_rows=len(unrest_applied), unrest_query_rows=len(unrest_queries),
                          unrest_collection_context_rows=272,unrest_collection_context_applied_rows=32,unrest_collection_context_query_rows=240,
                          growth_applied_rows=len(growth_applied), growth_query_rows=len(growth_queries),
                          spouse_applied_rows=len(spouse_applied), spouse_query_rows=len(spouse_queries),
                          loyalty_applied_rows=len(loyalty_applied), loyalty_query_rows=len(loyalty_queries),
                          discovery_applied_rows=len(discovery_applied), discovery_query_rows=len(discovery_queries),
                          research_applied_rows=len(research_applied), research_query_rows=len(research_queries),
                          environment_applied_rows=len(environment_applied), environment_query_rows=len(environment_queries),
                          harvest_applied_rows=len(harvest_applied), harvest_query_rows=len(harvest_queries),
                          diplomacy_applied_rows=len(diplomacy_applied), diplomacy_query_rows=len(diplomacy_queries),
                          diplomacy_owner_rows=87,diplomacy_owner_applied_rows=6,diplomacy_owner_query_rows=81,
                          construction_applied_rows=len(construction_applied), construction_query_rows=len(construction_queries),
                          construction_owner_rows=25, construction_owner_applied_rows=3, construction_owner_query_rows=22,
                          critical_applied_rows=len(critical_applied), critical_query_rows=len(critical_queries),
                          critical_lifecycle_applied_rows=len(lifecycle_applied), critical_lifecycle_query_rows=len(lifecycle_queries),
                          critical_session_applied_rows=len(session_applied), critical_session_query_rows=len(session_queries),
                          critical_targets_applied_rows=len(target_applied), critical_targets_query_rows=len(target_queries),
                          critical_owner_query_rows=len(critical_owner),
                          critical_position_owner_applied_rows=len(position_applied), critical_position_owner_query_rows=len(position_queries),
                          strategy_cost_applied_rows=len(strategy_applied), strategy_cost_query_rows=len(strategy_queries),
                          after_attack_applied_rows=len(after_applied), after_attack_query_rows=len(after_queries),
                          strategy_immunity_applied_rows=len(immune_applied), strategy_immunity_query_rows=len(immune_queries),
                          strategy_maximum_applied_rows=len(max_applied), strategy_maximum_query_rows=len(max_queries),
                          strategy_critical_applied_rows=len(strat_crit_applied), strategy_critical_query_rows=len(strat_crit_queries),
                          strategy_fire_applied_rows=len(fire_applied), strategy_fire_query_rows=len(fire_queries),
                          strategy_fire_write_rows=len(fire_writes),
                          strategy_followup_applied_rows=len(followup_applied), strategy_followup_query_rows=len(followup_queries),
                          strategy_followup_resolved_rows=len(followup_results),
                          strategy_charm_applied_rows=len(charm_applied), strategy_charm_query_rows=len(charm_queries),
                          strategy_charm_probability_rows=len(charm_effects),
                          defense_applied_rows=sum(r['stage'] == 'applied' for r in defense + defense_session),
                          defense_query_rows=sum(r['stage'] == 'check_matched' for r in defense + defense_session),
                          defense_immunity_settled_rows=len(defense_effects),
                          ambush_applied_rows=sum(r['stage'] == 'applied' for r in ambush + ambush_session),
                          ambush_query_rows=sum(r['stage'] == 'check_matched' for r in ambush + ambush_session),
                          ambush_defense_rows=len(ambush_defense), ambush_forced_critical_rows=len(ambush_critical),
                          ironwall_applied_rows=len(ironwall_applied), ironwall_query_rows=len(ironwall)-len(ironwall_applied),
                          assist_applied_rows=len(assist_applied), assist_query_rows=len(assist)-len(assist_applied),
                          single_aggregate_applied_rows=5, single_aggregate_query_rows=5,
                          counter_immunity_applied_rows=len(counter_applied),counter_immunity_query_rows=len(counter)-len(counter_applied),
                          double_attack_applied_rows=len(double_applied),double_attack_query_rows=double_query_count,
                          pincer_applied_rows=len(pincer_applied)+20,pincer_query_rows=len(pincer)+len(pincer_edges)+len(pincer_long)-len(pincer_applied)-20,
                          capture_applied_rows=len(capture_applied),capture_query_rows=len(capture)+len(capture_edges)-len(capture_applied),
                          ironwall_capture_rows=200,ironwall_capture_applied_rows=79,ironwall_capture_query_rows=121,
                          lucky_capture_rows=322,lucky_capture_applied_rows=217,lucky_capture_query_rows=105,
                          capture_base_rows=511,capture_base_applied_rows=379,capture_base_query_rows=132,
                          capture_base_collection_rows=320,capture_base_collection_applied_rows=240,capture_base_collection_query_rows=80,
                          bloodroad_handoff_applied_rows=len(handoff_applied),bloodroad_handoff_query_rows=len(handoff)+len(handoff_edges)-len(handoff_applied),
                          plunder_applied_rows=len(plunder_applied),plunder_query_rows=len(plunder)+len(plunder_edges)-len(plunder_applied),
                          magic_applied_rows=len(magic_applied),magic_query_rows=len(magic_all)-len(magic_applied),
                          magic_coeffect_rows=863,magic_coeffect_applied_rows=731,magic_coeffect_query_rows=132,
                          reach_applied_rows=len(reach_applied),reach_query_rows=len(reach)+len(reach_edges)-len(reach_applied),
                          steering_applied_rows=len(steering)+len(steering_edges),movement_map_applied_rows=8,movement_map_query_rows=8,
                          steering_entry_rows=235,steering_entry_applied_rows=235,steering_entry_query_rows=0,
                          entry_native_transfer_rows=175,entry_native_transfer_applied_rows=165,entry_native_transfer_query_rows=10,
                          entry_native_transfer_zoc_rows=432,entry_native_transfer_zoc_applied_rows=288,entry_native_transfer_zoc_query_rows=144,
                          attack_reach_applied_rows=attack_reach_applied,attack_reach_query_rows=len(attack_reach_all)-attack_reach_applied,
                          attack_tactic_applied_rows=tactic_applied,attack_tactic_query_rows=len(tactic_all)-tactic_applied,
                          attack_coeffect_applied_rows=co_applied,attack_coeffect_query_rows=len(co_all)-co_applied,attack_coeffect_shared_resource_writes=len(shared_write_ids),
                          bole_applied_rows=bole_applied_count,bole_query_rows=len(bole_all)-bole_applied_count,
                          zoc_applied_rows=zoc_applied_count,zoc_query_rows=len(zoc_all)-zoc_applied_count,
                          zoc_base_applied_rows=zoc_base_applied,zoc_base_query_rows=len(zoc_base_all)-zoc_base_applied,
                          zoc_entry_rows=259,zoc_entry_applied_rows=131,zoc_entry_query_rows=128,zoc_entry_coeffect_steering_rows=4,**trap_counts,**vine_counts,**counter_vine_counts,**assist_vine_counts,**aggregate_counts,**assist_defense_counts,**ai_weight_counts,**strategy_status_counts,**strategy_ambush_counts,**merchant_counts,**fire_consumer_counts,**person_owner_counts,**research_owner_counts,**domestic_owner_counts,**diplomacy_eligibility_counts,**strategy_ground_counts,**lucky_unit_event_counts,**lucky_duel_counts,**lucky_duel_post_counts,**lucky_duel_response_counts,
                          **lucky_mission_counts,**diplomacy_interaction_counts,**diplomacy_contest_counts,**reach_owner_counts,**strategy_owner_counts,**query_context_counts)))


if __name__ == '__main__':
    main()
