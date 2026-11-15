"""Check skill-hook byte ranges against the installed world's patch recipes."""
import argparse
import hashlib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT / 'outputs/skill-telemetry/patch-compatibility.json')
    args = parser.parse_args()
    source = (ROOT / 'worldmod/src/skills.cpp').read_text(encoding='utf-8')
    table = source.split('Hook g_hooks[] = {', 1)[1].split('\n};', 1)[0]
    hooks = [(int(a, 16), int(n), fn) for a, n, fn in re.findall(
        r'\{(0x[0-9a-f]+),\s*"[^"\n]+",\s*(\d+),\s*(\w+)', table)]
    assert hooks
    domestic_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',source)]
    assert len(domestic_guarded)==5
    diplomacy_path=ROOT / 'worldmod/src/skill_diplomacy_telemetry.inc'
    diplomacy_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',diplomacy_path.read_text(encoding='utf-8'))]
    assert len(diplomacy_guarded)==1
    maximum_source = (ROOT / 'worldmod/src/skill_strategy_maximum_telemetry.inc').read_text(encoding='utf-8')
    guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{0x[0-9a-f]+,(0x[0-9a-f]+),\d+,(?:true|false),(\d+),', maximum_source)]
    assert len(guarded) == 31
    ground_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',maximum_source)]
    assert len(ground_guarded)==2
    lucky_unit_path=ROOT / 'worldmod/src/skill_lucky_unit_event_telemetry.inc'
    lucky_unit_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',lucky_unit_path.read_text(encoding='utf-8'))]
    assert len(lucky_unit_guarded)==2
    lucky_duel_path=ROOT / 'worldmod/src/skill_lucky_duel_telemetry.inc'
    lucky_duel_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',lucky_duel_path.read_text(encoding='utf-8'))]
    assert len(lucky_duel_guarded)==4
    lucky_duel_post_path=ROOT / 'worldmod/src/skill_lucky_duel_post_telemetry.inc'
    lucky_duel_post_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',lucky_duel_post_path.read_text(encoding='utf-8'))]
    assert len(lucky_duel_post_guarded)==4
    lucky_duel_response_path=ROOT / 'worldmod/src/skill_lucky_duel_response_telemetry.inc'
    lucky_duel_response_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',lucky_duel_response_path.read_text(encoding='utf-8'))]
    assert len(lucky_duel_response_guarded)==6
    lucky_mission_path=ROOT / 'worldmod/src/skill_lucky_mission_telemetry.inc'
    lucky_mission_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',lucky_mission_path.read_text(encoding='utf-8'))]
    assert len(lucky_mission_guarded)==12
    diplomacy_interaction_path=ROOT / 'worldmod/src/skill_diplomacy_interaction_telemetry.inc'
    diplomacy_interaction_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',diplomacy_interaction_path.read_text(encoding='utf-8'))]
    assert len(diplomacy_interaction_guarded)==5
    diplomacy_contest_path=ROOT / 'worldmod/src/skill_diplomacy_contest_telemetry.inc'
    diplomacy_contest_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',diplomacy_contest_path.read_text(encoding='utf-8'))]
    assert len(diplomacy_contest_guarded)==14
    query_context_path=ROOT / 'worldmod/src/skill_query_context_telemetry.inc'
    query_context_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',query_context_path.read_text(encoding='utf-8'))]
    assert len(query_context_guarded)==2
    fire_immunity_path=ROOT / 'worldmod/src/skill_strategy_immunity_telemetry.inc'
    fire_immunity_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',fire_immunity_path.read_text(encoding='utf-8'))]
    assert len(fire_immunity_guarded)==1
    area_friendly_path=ROOT / 'worldmod/src/skill_area_friendly_telemetry.inc'
    area_friendly_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',area_friendly_path.read_text(encoding='utf-8'))]
    assert len(area_friendly_guarded)==4
    raw_priority_path=ROOT / 'worldmod/src/skill_raw_priority_telemetry.inc'
    raw_priority_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',raw_priority_path.read_text(encoding='utf-8'))]
    assert len(raw_priority_guarded)==1
    tribal_affinity_path=ROOT / 'worldmod/src/skill_tribal_affinity_telemetry.inc'
    tribal_affinity_guarded=[(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',tribal_affinity_path.read_text(encoding='utf-8'))]
    assert len(tribal_affinity_guarded)==3
    critical_path = ROOT / 'worldmod/src/skill_strategy_critical_telemetry.inc'
    critical_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', critical_path.read_text(encoding='utf-8'))]
    assert len(critical_guarded) == 8
    fire_path = ROOT / 'worldmod/src/skill_strategy_fire_telemetry.inc'
    fire_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', fire_path.read_text(encoding='utf-8'))]
    assert len(fire_guarded) == 9
    status_path = ROOT / 'worldmod/src/skill_strategy_status_telemetry.inc'
    status_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', status_path.read_text(encoding='utf-8'))]
    assert len(status_guarded) == 3
    ambush_strategy_path = ROOT / 'worldmod/src/skill_strategy_ambush_telemetry.inc'
    ambush_strategy_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', ambush_strategy_path.read_text(encoding='utf-8'))]
    assert len(ambush_strategy_guarded) == 1
    followup_path = ROOT / 'worldmod/src/skill_strategy_followup_telemetry.inc'
    followup_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', followup_path.read_text(encoding='utf-8'))]
    assert len(followup_guarded) == 5
    charm_path = ROOT / 'worldmod/src/skill_strategy_charm_telemetry.inc'
    charm_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', charm_path.read_text(encoding='utf-8'))]
    assert len(charm_guarded) == 2
    defense_path = ROOT / 'worldmod/src/skill_defense_telemetry.inc'
    defense_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', defense_path.read_text(encoding='utf-8'))]
    assert len(defense_guarded) == 4
    ironwall_path = ROOT / 'worldmod/src/skill_ironwall_telemetry.inc'
    ironwall_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', ironwall_path.read_text(encoding='utf-8'))]
    assert len(ironwall_guarded) == 5
    assist_path = ROOT / 'worldmod/src/skill_assist_telemetry.inc'
    assist_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', assist_path.read_text(encoding='utf-8'))]
    assert len(assist_guarded) == 6
    counter_path = ROOT / 'worldmod/src/skill_counter_immunity_telemetry.inc'
    counter_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', counter_path.read_text(encoding='utf-8'))]
    assert len(counter_guarded) == 10
    double_path = ROOT / 'worldmod/src/skill_double_attack_telemetry.inc'
    double_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', double_path.read_text(encoding='utf-8'))]
    assert len(double_guarded) == 4
    pincer_path = ROOT / 'worldmod/src/skill_pincer_telemetry.inc'
    pincer_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', pincer_path.read_text(encoding='utf-8'))]
    assert len(pincer_guarded) == 6
    capture_path = ROOT / 'worldmod/src/skill_capture_telemetry.inc'
    capture_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', capture_path.read_text(encoding='utf-8'))]
    assert len(capture_guarded) == 14
    handoff_path = ROOT / 'worldmod/src/skill_bloodroad_handoff_telemetry.inc'
    handoff_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', handoff_path.read_text(encoding='utf-8'))]
    assert len(handoff_guarded) == 11
    plunder_path = ROOT / 'worldmod/src/skill_plunder_telemetry.inc'
    plunder_guarded = [(int(a, 16), int(n)) for a, n in re.findall(
        r'\{(0x[0-9a-f]+),(\d+),"', plunder_path.read_text(encoding='utf-8'))]
    assert len(plunder_guarded) == 9
    magic_path = ROOT / 'worldmod/src/skill_magic_telemetry.inc'
    magic_source = magic_path.read_text(encoding='utf-8')
    magic_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',magic_source)]
    magic_recipes = {int(a,16):line.strip() for a,line in re.findall(r'\{(0x[0-9a-f]+),[012]\}, // ([^\n]+)',magic_source)}
    assert len(magic_guarded)==8 and len(magic_recipes)==15
    reach_path = ROOT / 'worldmod/src/skill_reach_telemetry.inc'
    reach_source = reach_path.read_text(encoding='utf-8')
    reach_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',reach_source)]
    reach_recipes = {int(a,16):line.strip() for a,line in re.findall(r'\{(0x[0-9a-f]+),[012]\}, // ([^\n]+)',reach_source)}
    assert len(reach_guarded)==3 and len(reach_recipes)==5
    steering_path = ROOT / 'worldmod/src/skill_steering_telemetry.inc'
    steering_source = steering_path.read_text(encoding='utf-8')
    steering_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',steering_source)]
    steering_recipes = {int(a,16):line.strip() for a,line in re.findall(r'\{(0x[0-9a-f]+),[01]\}, // ([^\n]+)',steering_source)}
    assert len(steering_guarded)==8 and len(steering_recipes)==2
    attack_reach_path = ROOT / 'worldmod/src/skill_attack_reach_telemetry.inc'
    attack_reach_source = attack_reach_path.read_text(encoding='utf-8')
    attack_reach_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',attack_reach_source)]
    attack_reach_recipes = {int(a,16):line.strip() for a,line in re.findall(r'\{(0x[0-9a-f]+),[0123]\}, // ([^\n]+)',attack_reach_source)}
    assert len(attack_reach_guarded)==5 and len(attack_reach_recipes)==7
    bole_path = ROOT / 'worldmod/src/skill_bole_telemetry.inc'
    bole_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',bole_path.read_text(encoding='utf-8'))]
    assert len(bole_guarded)==7
    precision_path = ROOT / 'worldmod/src/skill_precision_telemetry.inc'
    precision_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',precision_path.read_text(encoding='utf-8'))]
    assert len(precision_guarded)==6
    aftermath_path = ROOT / 'worldmod/src/skill_fire_aftermath_telemetry.inc'
    aftermath_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',aftermath_path.read_text(encoding='utf-8'))]
    assert len(aftermath_guarded)==4
    trap_path = ROOT / 'worldmod/src/skill_trap_telemetry.inc'
    trap_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',trap_path.read_text(encoding='utf-8'))]
    assert len(trap_guarded)==18
    research_gold_path = ROOT / 'worldmod/src/skill_growth_telemetry.inc'
    research_gold_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',research_gold_path.read_text(encoding='utf-8'))]
    assert len(research_gold_guarded)==2
    vine_path = ROOT / 'worldmod/src/skill_vine_telemetry.inc'
    vine_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',vine_path.read_text(encoding='utf-8'))]
    assert len(vine_guarded)==7
    ai_weight_path = ROOT / 'worldmod/src/skill_ai_weight_telemetry.inc'
    ai_weight_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',ai_weight_path.read_text(encoding='utf-8'))]
    assert len(ai_weight_guarded)==10
    zoc_path = ROOT / 'worldmod/src/skill_zoc_telemetry.inc'
    zoc_source = zoc_path.read_text(encoding='utf-8')
    zoc_guarded = [(int(a,16),int(n)) for a,n in re.findall(r'\{(0x[0-9a-f]+),(\d+),"',zoc_source)]
    zoc_recipes = {int(a,16):line.strip() for a,line in re.findall(r'\{(0x[0-9a-f]+),[0123]\}, // ([^\n]+)',zoc_source)}
    assert len(zoc_guarded)==7 and len(zoc_recipes)==8
    entries, files = [], []
    for path in sorted(args.game.glob('worldmod*.txt')):
        raw = path.read_bytes()
        files.append(dict(file=path.name, sha256=hashlib.sha256(raw).hexdigest()))
        for number, line in enumerate(raw.decode('utf-8-sig', errors='replace').splitlines(), 1):
            match = re.match(r'^(?:CAVE|REWRITE)\s+([0-9a-fA-F]{8})\s+(\d+)\s', line)
            if match:
                start, size = int(match[1], 16), int(match[2])
            else:
                match = re.match(r'^([0-9a-fA-F]{8})\s+(\d+)\s+(\d+)\s', line)
                if not match:
                    continue
                start, size = int(match[1], 16) + int(match[2]), int(match[3])
            entries.append((start, size, path.name, number, line))
    assert entries
    overlaps, guard_overlaps = [], []
    for start, size, recipe, number, line in entries:
        for address, length in tribal_affinity_guarded + raw_priority_guarded + area_friendly_guarded + fire_immunity_guarded + query_context_guarded + diplomacy_contest_guarded + diplomacy_interaction_guarded + lucky_mission_guarded + lucky_duel_response_guarded + lucky_duel_post_guarded + lucky_duel_guarded + lucky_unit_guarded + ground_guarded + diplomacy_guarded + domestic_guarded + research_gold_guarded + guarded + status_guarded + ambush_strategy_guarded + critical_guarded + fire_guarded + followup_guarded + charm_guarded + defense_guarded + ironwall_guarded + assist_guarded + counter_guarded + double_guarded + pincer_guarded + capture_guarded + handoff_guarded + plunder_guarded + magic_guarded + reach_guarded + steering_guarded + attack_reach_guarded + bole_guarded + zoc_guarded + precision_guarded + aftermath_guarded + trap_guarded + vine_guarded + ai_weight_guarded:
            if start < address + length and address < start + size:
                m7 = address == 0x5a0770 and size == 4 and any(
                    start == at and re.match(pattern, line) for at, pattern in (
                        (0x5a07aa, r'^005a07a7 3 4 8d0485680efb06 B20(?:\s|$)'),
                        (0x5a079a, r'^005a0798 2 4 69c0c8000000 W(?:\s|$)'),
                        (0x5a0783, r'^005a0782 1 4 3dc8000000 H(?:\s|$)'),
                        (0x5a078f, r'^005a078d 2 4 81f9c8000000 W(?:\s|$)')))
                m7 = m7 or (address == 0x5ae000 and start == 0x5ae087 and size == 4 and
                            re.match(r'^005ae086 1 4 b9680efb06 B20(?:\s|$)', line) is not None)
                m7 = m7 or (size == 4 and any(address == guard and start == at and re.match(pattern, line)
                    for guard, at, pattern in (
                        (0x588460, 0x58846a, r'^00588469 1 4 b950936a09 AREAHEX(?:\s|$)'),
                        (0x588cb0, 0x588cea, r'^00588ce9 1 4 b950936a09 AREAHEX(?:\s|$)'))))
                status_cave = address == 0x496350 and start == 0x496350 and size == 6 and re.fullmatch(
                    r'CAVE 00496350 6 8a44240484c0 8a44240483792402750c3c1e7602b01e884128c204008a44240484c0e9356349f0 @29=rel:496356 ; long false report support', line) is not None
                person_bound = address == 0x484de0 and start == 0x484df2 and size == 4 and re.match(
                    r'^00484df1 1 4 3d4b040000 PT-1(?:\s|$)', line) is not None and any(
                    e[0] == 0x490b09 and e[1] == 4 and e[2] == recipe and re.match(
                    r'^00490b08 1 4 3d4b040000 PT-1(?:\s|$)', e[4]) for e in entries)
                item_treasury = address == 0x484de0 and start == 0x484e06 and size == 5 and re.fullmatch(
                    r'CAVE 00484e06 5 83f8297f03 3d0000007f0f8f034e48f0e9fb4d48f0 @1=abs:C-1 @7=rel:484e0e @12=rel:484e0b ; imm8->imm32 cmp eax, 0x29 = C-1', line) is not None
                magic_m7 = size==4 and start in magic_recipes and line.split(';',1)[0].strip()==magic_recipes[start]
                reach_m7 = size==4 and start in reach_recipes and line.split(';',1)[0].strip()==reach_recipes[start]
                steering_m7 = size==4 and start in steering_recipes and line.split(';',1)[0].strip()==steering_recipes[start]
                attack_reach_immediate = size==4 and start in attack_reach_recipes and line.split(';',1)[0].strip()==attack_reach_recipes[start]
                zoc_immediate = size==4 and start in zoc_recipes and line.split(';',1)[0].strip()==zoc_recipes[start]
                tribal_city_bound = address==0x5f6610 and start==0x5f6637 and size==5 and re.fullmatch(
                    r'CAVE 005f6637 5 83fe297f04 81fe0000007f0f8f34665ff0e92b665ff0 @2=abs:C-1 @8=rel:5f6640 @13=rel:5f663c ; imm8->imm32 cmp esi, 0x29 = C-1',line) is not None
                guard_overlaps.append(dict(guard=hex(address), recipe=recipe, line=number, text=line,
                                           recognized_tribal_city_bound_cave=tribal_city_bound,
                                           recognized_m7_immediate=m7, recognized_status_setter_cave=status_cave,
                                           recognized_m10_person_bound=person_bound,recognized_item_treasury_cave=item_treasury,
                                           recognized_magic_m7_immediate=magic_m7,recognized_reach_m7_immediate=reach_m7,recognized_steering_m7_immediate=steering_m7,recognized_attack_reach_immediate=attack_reach_immediate,recognized_zoc_immediate=zoc_immediate))
        for address, length, function in hooks:
            if start >= address + length or address >= start + size:
                continue
            recognized = (function == 'tuntianBridge' and start == 0x59bfd5 and size == 4 and
                          re.match(r'^0059bfd4 1 4 3d4b040000 PT-1(?:\s|$)', line) is not None and
                          any(e[0] == 0x490b09 and e[1] == 4 and e[2] == recipe and
                              re.match(r'^00490b08 1 4 3d4b040000 PT-1(?:\s|$)', e[4]) for e in entries))
            overlaps.append(dict(hook=hex(address), function=function, recipe=recipe, line=number,
                                 text=line, recognized_m10_pair=recognized))
    result = dict(hooks=len(hooks), patch_entries=len(entries), overlaps=overlaps, recipe_files=files,
                  guarded_comparison_sites=len(guarded), guard_overlaps=guard_overlaps,
                  guarded_strategy_ground_layouts=len(ground_guarded),
                  guarded_lucky_unit_event_layouts=len(lucky_unit_guarded),
                  guarded_lucky_duel_layouts=len(lucky_duel_guarded),
                  guarded_lucky_duel_post_layouts=len(lucky_duel_post_guarded),
                  guarded_lucky_duel_response_layouts=len(lucky_duel_response_guarded),
                  guarded_lucky_mission_layouts=len(lucky_mission_guarded),
                  guarded_diplomacy_interaction_layouts=len(diplomacy_interaction_guarded),
                  guarded_diplomacy_contest_layouts=len(diplomacy_contest_guarded),
                  guarded_query_context_layouts=len(query_context_guarded),
                  guarded_strategy_fire_immunity_layouts=len(fire_immunity_guarded),
                  guarded_area_friendly_layouts=len(area_friendly_guarded),
                  guarded_raw_priority_layouts=len(raw_priority_guarded),
                  guarded_tribal_affinity_layouts=len(tribal_affinity_guarded),
                  tribal_affinity_source_sha256=hashlib.sha256(tribal_affinity_path.read_bytes()).hexdigest(),
                  raw_priority_source_sha256=hashlib.sha256(raw_priority_path.read_bytes()).hexdigest(),
                  area_friendly_source_sha256=hashlib.sha256(area_friendly_path.read_bytes()).hexdigest(),
                  fire_immunity_source_sha256=hashlib.sha256(fire_immunity_path.read_bytes()).hexdigest(),
                  query_context_source_sha256=hashlib.sha256(query_context_path.read_bytes()).hexdigest(),
                  diplomacy_contest_source_sha256=hashlib.sha256(diplomacy_contest_path.read_bytes()).hexdigest(),
                  diplomacy_interaction_source_sha256=hashlib.sha256(diplomacy_interaction_path.read_bytes()).hexdigest(),
                  lucky_mission_source_sha256=hashlib.sha256(lucky_mission_path.read_bytes()).hexdigest(),
                  lucky_duel_response_source_sha256=hashlib.sha256(lucky_duel_response_path.read_bytes()).hexdigest(),
                  lucky_duel_post_source_sha256=hashlib.sha256(lucky_duel_post_path.read_bytes()).hexdigest(),
                  lucky_duel_source_sha256=hashlib.sha256(lucky_duel_path.read_bytes()).hexdigest(),
                  lucky_unit_event_source_sha256=hashlib.sha256(lucky_unit_path.read_bytes()).hexdigest(),
                  guarded_strategy_critical_layouts=len(critical_guarded),
                  critical_guard_source_sha256=hashlib.sha256(critical_path.read_bytes()).hexdigest(),
                  guarded_strategy_fire_layouts=len(fire_guarded),
                  guarded_strategy_status_layouts=len(status_guarded),
                  guarded_research_gold_layouts=len(research_gold_guarded),
                  guarded_domestic_layouts=len(domestic_guarded),
                  guarded_diplomacy_eligibility_layouts=len(diplomacy_guarded),
                  diplomacy_guard_source_sha256=hashlib.sha256(diplomacy_path.read_bytes()).hexdigest(),
                  research_gold_guard_source_sha256=hashlib.sha256(research_gold_path.read_bytes()).hexdigest(),
                  guarded_strategy_ambush_layouts=len(ambush_strategy_guarded),
                  ambush_strategy_guard_source_sha256=hashlib.sha256(ambush_strategy_path.read_bytes()).hexdigest(),
                  status_guard_source_sha256=hashlib.sha256(status_path.read_bytes()).hexdigest(),
                  fire_guard_source_sha256=hashlib.sha256(fire_path.read_bytes()).hexdigest(),
                  guarded_strategy_followup_layouts=len(followup_guarded),
                  followup_guard_source_sha256=hashlib.sha256(followup_path.read_bytes()).hexdigest(),
                  guarded_strategy_charm_layouts=len(charm_guarded),
                  charm_guard_source_sha256=hashlib.sha256(charm_path.read_bytes()).hexdigest(),
                  guarded_defense_layouts=len(defense_guarded),
                  defense_guard_source_sha256=hashlib.sha256(defense_path.read_bytes()).hexdigest(),
                  guarded_ironwall_layouts=len(ironwall_guarded),
                  ironwall_guard_source_sha256=hashlib.sha256(ironwall_path.read_bytes()).hexdigest(),
                  guarded_assist_layouts=len(assist_guarded),
                  assist_guard_source_sha256=hashlib.sha256(assist_path.read_bytes()).hexdigest(),
                  guarded_counter_immunity_layouts=len(counter_guarded),
                  counter_immunity_guard_source_sha256=hashlib.sha256(counter_path.read_bytes()).hexdigest(),
                  guarded_double_attack_layouts=len(double_guarded),
                  double_attack_guard_source_sha256=hashlib.sha256(double_path.read_bytes()).hexdigest(),
                  guarded_pincer_layouts=len(pincer_guarded),
                  pincer_guard_source_sha256=hashlib.sha256(pincer_path.read_bytes()).hexdigest(),
                  guarded_capture_layouts=len(capture_guarded),
                  capture_guard_source_sha256=hashlib.sha256(capture_path.read_bytes()).hexdigest(),
                  guarded_bloodroad_handoff_layouts=len(handoff_guarded),
                  bloodroad_handoff_guard_source_sha256=hashlib.sha256(handoff_path.read_bytes()).hexdigest(),
                  guarded_plunder_layouts=len(plunder_guarded),
                  plunder_guard_source_sha256=hashlib.sha256(plunder_path.read_bytes()).hexdigest(),
                  guarded_magic_layouts=len(magic_guarded),magic_guard_source_sha256=hashlib.sha256(magic_path.read_bytes()).hexdigest(),
                  guarded_reach_layouts=len(reach_guarded),reach_guard_source_sha256=hashlib.sha256(reach_path.read_bytes()).hexdigest(),
                  guarded_steering_layouts=len(steering_guarded),steering_guard_source_sha256=hashlib.sha256(steering_path.read_bytes()).hexdigest(),
                  guarded_bole_layouts=len(bole_guarded),bole_guard_source_sha256=hashlib.sha256(bole_path.read_bytes()).hexdigest(),
                  guarded_precision_layouts=len(precision_guarded),precision_guard_source_sha256=hashlib.sha256(precision_path.read_bytes()).hexdigest(),
                  guarded_attack_reach_layouts=len(attack_reach_guarded),attack_reach_guard_source_sha256=hashlib.sha256(attack_reach_path.read_bytes()).hexdigest(),
                  guarded_zoc_layouts=len(zoc_guarded),zoc_guard_source_sha256=hashlib.sha256(zoc_path.read_bytes()).hexdigest(),
                  guard_source_sha256=hashlib.sha256((ROOT / 'worldmod/src/skill_strategy_maximum_telemetry.inc').read_bytes()).hexdigest(),
                  source_sha256=hashlib.sha256(source.encode('utf-8')).hexdigest(),
                  unrecognized_overlaps=sum(not x['recognized_m10_pair'] for x in overlaps),
                  unrecognized_guard_overlaps=sum(not (x['recognized_m7_immediate'] or x['recognized_status_setter_cave'] or x['recognized_m10_person_bound'] or x['recognized_item_treasury_cave'] or x['recognized_magic_m7_immediate'] or x['recognized_reach_m7_immediate'] or x['recognized_steering_m7_immediate'] or x['recognized_attack_reach_immediate'] or x['recognized_zoc_immediate'] or x['recognized_tribal_city_bound_cave']) for x in guard_overlaps),
                  limitation='Static recipe ranges only; runtime module mutations and full gameplay require separate validation.')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: result[key] for key in ('hooks', 'patch_entries', 'unrecognized_overlaps')}))
    assert not result['unrecognized_overlaps'], 'Unrecognized overlap with an installed world patch'
    assert not result['unrecognized_guard_overlaps'], 'Installed world patch changes a guarded strategy comparison'


if __name__ == '__main__':
    main()
