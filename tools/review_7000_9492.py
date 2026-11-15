"""Conservative source review ledger; never modifies the playable roster.

Located passages are not date adjudications. Only the explicitly enumerated,
individually read passages below support a historical date or identity decision.
"""
import collections
import json
import re
from pathlib import Path

import opencc

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / 'san11world-persons/data/persons'
CC = opencc.OpenCC('t2s')
CACHE = {}


def read(path):
    path = Path(path)
    if path not in CACHE:
        CACHE[path] = CC.convert(path.read_text(encoding='utf-8'))
    return CACHE[path]


def evidence(path, phrase, citation, interpretation=None):
    text = read(path)
    position = text.find(phrase)
    if position < 0:
        raise ValueError(f'Missing adjudicated passage: {phrase}')
    result = dict(path=str(path.resolve()), offset=position,
                  offset_semantics='matched_phrase_start', matched_phrase=phrase,
                  excerpt_offset=max(0, position-100),
                  excerpt=text[max(0, position-100):position+len(phrase)+160],
                  citation=citation, normalization='OpenCC t2s; offsets in normalized text')
    if interpretation:
        result['interpretation'] = interpretation
    return result


def date(year, ev, age=None, **extra):
    result = dict(year=year, precision='year', calendar='historical_CE_year',
                  source=ev['citation'], excerpt=ev['excerpt'], **extra)
    if age:
        result.update(precision='inferred_year_from_sui_age',
                      calculation=f'{year+age-1} - {age} + 1 = {year}',
                      caveat='Inferred using traditional nominal age; not an attested birth date')
    return result


def main():
    roster = json.loads((SOURCE/'roster.json').read_text(encoding='utf-8'))
    roster_by_id = {row['id']: row for row in roster}
    ledger = {row['id']: row for row in map(json.loads, (ROOT/'out/person_chronology/ledger.jsonl').open(encoding='utf-8'))}
    candidates = {row['id']: row for row in map(json.loads, (ROOT/'out/person_chronology/calibration_candidates.jsonl').open(encoding='utf-8'))}
    extracted = {}
    for path in (SOURCE/'build').glob('cands_*.jsonl'):
        for line in path.open(encoding='utf-8'):
            row = json.loads(line)
            extracted[row['key']] = row
    names = collections.defaultdict(list)
    for row in roster:
        names[CC.convert(row['name_full'])].append(row['id'])
    results = {}
    for ident in range(7000, 9493):
        row, audit = roster_by_id[ident], ledger[ident]
        passages, missing = [], []
        for key in row['keys']:
            parts = key.split(':')
            path = SOURCE/'_cache/chunks'/f'{parts[1]}.txt'
            if not path.exists():
                missing.append(key)
                continue
            text = read(path)
            name = CC.convert(extracted.get(key, {}).get('name', parts[-1]))
            hits = list(re.finditer(re.escape(name), text)) if len(name)>1 else []
            if not hits:
                missing.append(key)
                continue
            # Each located passage is a lead only: neighboring dates can concern
            # another person or an earlier event retold in a later year.
            for hit in hits[:2]:
                pos = hit.start()
                passages.append(dict(source_key=key, citation=extracted.get(key, {}).get('src'),
                    path=str(path.resolve()), offset=pos,
                    excerpt=text[max(0,pos-90):pos+360],
                    normalization='OpenCC t2s', verification='located_passage_not_individual_date_adjudication'))
        claims = audit['upstream_claims']
        unresolved = ['birth_date_not_independently_corroborated', 'death_date_not_independently_corroborated',
                      'first_career_year_not_established']
        collision = [i for i in names[CC.convert(row['name_full'])] if i!=ident]
        if collision:
            unresolved.append('same_name_candidates_require_identity_comparison')
        if missing:
            unresolved.append('some_original_passages_missing_or_name_not_located')
        results[ident] = dict(id=ident, keys=row['keys'], name=row['name_full'],
            historical_birth=None, historical_death=None, proposed_appear=None,
            appearance_basis='retain game estimate pending a justified first career year; source first mention is not initial service',
            duplicate_of=None, identity_status='not_individually_adjudicated', evidence=passages,
            review_status='needs_further_research', unresolved=unresolved,
            current_game={k:row[k] for k in ['birth','appear','death']},
            source_birth_claims_unverified=claims['birth'], source_death_claims_unverified=claims['death'],
            same_name_candidate_ids=collision, missing_source_keys=missing,
            source_identities_all_adjudicated=False,
            review_scope='cached historical source passages; not an exhaustive search of all historical literature')
        bound = candidates[ident].get('appearance_no_later_than')
        if bound:
            results[ident]['unadjudicated_activity_candidate'] = bound

    annals = SOURCE/'_cache/texts/资治通鉴.txt'
    # These subjects, acts and annual contexts were individually read. Month and
    # ganzhi dates stay Chinese-calendar labels, without invented Gregorian days.
    deaths = {
        7000:(336,'成主期以从子尚书仆射武陵公载有俊才'),
        7006:(336,'斩其将荣伯保'),7023:(336,'皝兵追及楷、鉴，斩之'),
        7036:(350,'因不食而卒'),7040:(336,'丁衡、游毅、孙机等，皆仁所信用也'),
        7049:(336,'皝兵追及楷、鉴，斩之'),
        7067:(349,'斩张豺于平乐市'),7069:(337,'赵王虎恶之，腰斩成公段'),
        7072:(337,'杀颜等三十馀人'),7073:(337,'仇池氐王杨毅族兄初，袭杀毅'),
        7079:(337,'其夜，杀邃及其妃张氏'),
        7111:(338,'又鸩杀寿养弟安北将军攸'),
        7086:(338,'汉仆射任颜谋反，诛'),
        7099:(338,'景骞、田褒、姚华、许涪及征西将军李遐'),
        7109:(338,'景骞、田褒、姚华、许涪及征西将军李遐'),
        7114:(338,'将军李西等怀奸乱政，皆收杀之'),
        7116:(338,'寿奏建宁王越、景骞'),
        7117:(338,'许涪及征西将军李遐、将军李西等怀奸乱政，皆收杀之'),
        7102:(338,'冀阳流寓之士共杀太守宋烛以降于赵'),
        7125:(338,'大姓王清等密谋应赵，泳收斩之'),
        7127:(338,'景骞、田褒、姚华、许涪及征西将军李遐'),
        7138:(338,'景骞、田褒、姚华、许涪及征西将军李遐'),
        7152:(338,'武宁令广平孙兴晓谕吏民共收屈，数其罪而杀之'),
        7166:(339,'汉李弈寇巴东，守将劳杨败死'),
        7169:(339,'评等与战，斩晃、支首'),7175:(339,'评等与战，斩晃、支首'),
        7400:(346,'弈单骑突门，门者射而杀之'),
        7566:(346,'县令敦煌车济不降，伏剑而死'),7578:(347,'林邑复陷日南，杀督护刘雄'),
        7706:(349,'夜斩松、才于西中华门，并杀苞'),
        7760:(350,'大怒，诛𫍲及其子伯阳'),
        7950:(354,'袭钩，杀之，南奔段龛'),
        7966:(355,'以庶人礼葬之，并杀其二子'),7968:(355,'遂为兵人所杀。混等枭其首'),
        7973:(355,'九月，生杀梁后及毛贵、梁楞、梁安'),
        7975:(355,'九月，生杀梁后及毛贵、梁楞、梁安'),
        7976:(355,'宋奴使其姑子梁式王刺杀杨初'),
        7979:(355,'得右仆射段纯，杀之'),7981:(355,'九月，生杀梁后及毛贵、梁楞、梁安'),
        7993:(355,'生杀弱儿及其九子、二十七孙'),8009:(356,'生怒，凿其顶而杀之'),
        8018:(356,'乃杀王堕'),8028:(356,'引弓射牢，杀之'),
        8709:(367,'曹毂寻卒'),9449:(400,'丙寅，卫将军东亭献侯王珣卒'),
        7002:(347,'斩其将杜勋、汲鱼'),7009:(336,'王冰自杀'),
        7051:(346,'己卯，都乡文穆侯何充卒'),7292:(346,'而竟诛朱轨'),
        7304:(351,'吐谷浑叶延卒'),7307:(341,'左仆射蔡兴、右仆射李嶷皆坐直谏死'),
        7384:(365,'戊子，益州刺史建城襄会周抚卒'),
        7401:(345,'贬广为临邛侯，广自杀'),7427:(345,'收当、思明斩之，夷其三族'),
        7431:(353,'安北将军魏统卒'),7530:(356,'匈奴大人刘务桓卒'),
        7557:(347,'斩其将杜勋、汲鱼'),7677:(349,'闵、农攻斩伏都等'),
        7750:(349,'引朴斩之'),7779:(351,'俊乃知张举之妄而杀之'),
        7825:(361,'东安简伯郗昙卒'),7833:(361,'高昌卒'),
        7847:(352,'王午为其将秦兴所杀'),7868:(353,'襄斩憬，并其众'),
        8007:(366,'戊寅，皇后庾氏崩'),8095:(367,'甲子，燕太尉建宁敬公阳骛卒'),
        8097:(357,'生乃诛太师、录尚书事、广宁公鱼遵'),
        8113:(359,'燕人杀段勤'),8125:(359,'匈奴刘悉勿祈卒'),
        8148:(361,'起兵攻澄，杀之，并灭其族'),8166:(370,'仇池公杨世卒'),
        8167:(360,'仇池公杨俊卒'),8177:(370,'癸酉，袁真卒'),
        8284:(361,'归义侯李势卒'),8782:(370,'袁真以梁国内史沛郡朱宪'),
        8797:(371,'擒瑾及辅并其宗族，送建康斩之'),
        8500:(387,'八月，广武人执大豫送姑臧，斩之'),
        9172:(377,'十二月，临海太守郗超卒'),
        9486:(398,'引宝入龙城外邸，弑之'),
    }
    for ident,(year, phrase) in deaths.items():
        ev = evidence(annals, phrase, f'資治通鑑·{year}年条',
                      'Individually read direct death/kill statement and annual context; not a mere name/date cooccurrence')
        result=results[ident]
        result['historical_death']=date(year,ev)
        result['evidence'].append(ev)
        result['review_status']='primary_text_corroborated'
        result['identity_status']='dated_event_subject_individually_checked'
        result['unresolved'].remove('death_date_not_independently_corroborated')

    # Additional individually read passages. Here "unknown" is specifically
    # unknown in the cited consulted passage, never a claim about all literature.
    checked_unknown = {
        'tj_12':[7003,7007,7010,7011,7015,7016,7018,7022,7026,7028,7030,7031,7032,7033,7035,7037,7041,7046,7047,7048,7056,7059,7060,7068,7090,7119,7123,7131,7148,7154,7156,7157,7168],
        'hy_00':[7005,7020], 'js_02':[7012],
        'sl_02':[7013], 'js_12':[7014,7024],
        'sl_08':[7021], 'tj_16':[7044],
        'tj_13':[7057,7061,7066,7078,7089,7091,7095,7098,7105,7118,7120,7100,7101,7129,7136,7139,7146,7147,7150,7151,7167,7172,7173],
        'sl_09':[7062,7085,7122], 'sl_00':[7063,7088,7092,7106,7126,7133,7135,7153,7170], 'sz_00':[7110],
        'sl_01':[7017,7034,7042,7052,7058,7064,7074,7077,7124,7130,7132,7137,7174,7176],
        'js_14':[7140], 'js_08':[7004,7025,7087,7141], 'js_16':[7134],
        'tj_15':[7050,7112,7128], 'js_13':[7045,7081,7171],
    }
    for chunk,ids in checked_unknown.items():
        for ident in ids:
            result=results[ident]
            ev=next((e for e in result['evidence'] if f'/{chunk}.txt' in e['path'].replace('\\','/')),None)
            if ev is None:
                raise ValueError(f'Missing read passage {ident} in {chunk}')
            ev['verification']='individually_read_named_subject_passage; no explicit birth or dated death in this consulted passage'
            result.update(review_status='source_checked_dates_unknown',identity_status='named_subject_passage_checked')
    for ident,chunk,phrase in [(7019,'tj_12','安定皇甫岌、岌弟真'),(7027,'sl_08','中常侍蘓马')]:
        ev=evidence(SOURCE/'_cache/chunks'/f'{chunk}.txt',phrase,chunk,
                    'Individually read identity by sibling/name variant; no explicit birth/death in consulted passage')
        results[ident]['evidence'].append(ev)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='name_variant_or_abbreviated_subject_checked')
    for ident, month, ganzhi in [(7051,1,'己卯'),(7384,6,'戊子'),(8007,5,'戊寅'),(8095,12,'甲子'),(8177,2,'癸酉'),(8782,1,'己亥'),(8500,8,None),(9172,12,None)]:
        results[ident]['historical_death'].update(precision='lunar_month_with_ganzhi_day' if ganzhi else 'lunar_month',
            month=month, day_ganzhi=ganzhi, calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')

    # Explicit dated biographies, with birth only a labelled nominal-age inference.
    biographies = [
        (7473,'js_09',317,18,'建武元年薨，年十八'),
        (7650,'js_11',368,66,'三年卒，时年六十六'),
        (7656,'js_17',349,47,'永和五年卒，年四十七'),
        (7726,'js_11',345,41,'永和元年卒，时年四十一'),
        (7740,'js_06',353,49,'永和九年卒，时年四十九'),
        (7743,'js_12',377,73,'太元二年卒，年七十三'),
        (7396,'gs_01',414,83,'以晋义熙十年遇疾绵笃'),
        (7855,'gs_01',380,74,'晋太元五年卒于都。春秋七十有四'),
        (8003,'js_09',341,31,'咸康七年薨，年三十一'),
        (8122,'js_10',384,72,'太元九年卒，时年七十二'),
        (8214,'js_09',381,66,'太元六年，晞卒于新安，时年六十六'),
        (8699,'js_12',358,38,'升平二年卒，时年三十八'),
        (8728,'js_09',358,36,'升平二年卒于官，年三十六'),
        (8740,'gs_01',395,73,'以晋太元二十年卒'),
        (8749,'sz_00',396,73,'年七十三，泰元二十一年'),
        (8877,'gs_01',402,76,'元兴元年卒。春秋七十有六'),
        (8974,'js_17',384,55,'太元九年卒，年五十五'),
        (9107,'wsh_01',396,63,'皇始元年卒官，时年六十三'),
        (9214,'gs_01',412,76,'以晋义熙八年卒于寺中。春秋七十有六'),
        (9315,'js_13',388,46,'十三年，卒于官，时年四十六'),
        (9362,'gs_01',417,72,'晋义熙十三年卒于山舍。春秋七十二'),
    ]
    for ident,chunk,year,age,phrase in biographies:
        source_key=next((k for k in roster_by_id[ident]['keys'] if f':{chunk}:' in k),None)
        ev=evidence(SOURCE/'_cache/chunks'/f'{chunk}.txt', phrase,
                    extracted.get(source_key,{}).get('src',chunk),
                    f'Biographical subject and reign-year sequence individually checked; death age {age} is traditional nominal age')
        result=results[ident]
        result.update(historical_death=date(year,ev),historical_birth=date(year-age+1,ev,age=age),
                      review_status='primary_text_corroborated',identity_status='biographical_subject_checked')
        result['evidence'].append(ev)
        result['unresolved']=[x for x in result['unresolved'] if not x.startswith(('birth_date_','death_date_'))]
        result['unresolved'].append('birth_year_is_nominal_age_inference_not_independently_attested')

    # Genuine identity collisions in the merge; no single date applies to all keys.
    conflicts = {
        7054:('劉表議論者劉光 mixed with 安定劉光/侯子光佛太子337起事者; ancient text explicitly supports latter alias only',SOURCE/'_cache/chunks/sl_02.txt','刘光安定人也一云侯子光'),
        7065:('前趙劉昶妻慈鄉君張氏 mixed with 後趙石邃妃張氏337遭殺; mother of劉聰 appears elsewhere in same chunk and cannot identify this row',SOURCE/'_cache/chunks/sl_00.txt','封昶妻张氏为慈乡君'),
        7680:('成漢張才 and 後趙殿中將軍張才 conflated; later-Zhao official killed349',annals,'夜斩松、才于西中华门'),
        7746:('後趙涼州刺史石寧 killed350; 骠騎將軍石寧 still active351; separate identities',annals,'赵凉州刺史石宁独据上邽不下'),
        8128:('Record mixes 前涼張瓘弟張琚 with 關中張琚; do not transfer either death across keys',SOURCE/'_cache/chunks/js_01.txt','不杀张琚，关中非国家有也'),
        9054:('Record mixes 仇池楊國子楊安 with 前秦征仇池將楊安; split source keys before calibration',SOURCE/'_cache/chunks/yi_01.txt','纂后为杨安所杀'),
        9234:('後趙皇帝石世 killed349; CBDB givesdeath363 but no birth; game birth338 is filled, not CBDB evidence. Isolate erroneous date pending identity/source correction',annals,'封世为谯王，废刘氏为太妃，寻皆杀之'),
        9289:('陶侃監軍部將李根 active328; CBDB index358 is NOT birth, and circa421 death belongs to an unproved 中書令 identity. Cross-source identity remains unproved',annals,'监军部将李根请筑白石垒'),
    }
    for ident,(reason,path,phrase) in conflicts.items():
        result=results[ident]
        result.update(identity_status='merged_homonyms_require_split',review_status='identity_conflict')
        result['unresolved'].append(reason)
        result['evidence'].append(evidence(path,phrase,'原文身份冲突对读',reason))

    # Duplicates are adjudicated from matching unusual acts, office and kinship,
    # not from name equality or similar artificial birth years.
    duplicates = {
        7029:(7028,'汶山太守，向沈死後被推西夷校尉、北出枳遭李恭費黑邀獲，蘭維/蘭繼同一事件異文',SOURCE/'_cache/chunks/sl_09.txt','共推汶山太守兰继为西夷校尉'),
        7038:(7037,'北海人慕容廆股肱，同一名單，逄/逢異文',SOURCE/'_cache/chunks/sl_02.txt','北海逢羡广平游邃'),
        7043:(7020,'蜀郡太守，與楊芬常歆共推蘭維/繼、北出枳遭俘，程融/鄭融及江陽/江夏異文',SOURCE/'_cache/chunks/sl_09.txt','蜀郡太守江夏郑融'),
        7103:(6989,'封奕/封弈同一前燕相國，字子專、祖封釋，建熙六年卒；不同拼字與填補年代不可拆成兩人',SOURCE/'_cache/chunks/sl_03.txt','封奕字子专渤海蓚人也'),
        8859:(6989,'前燕封弈，350燕軍平冀州之相國，与封奕(字子专)同人',SOURCE/'_cache/chunks/tj_17.txt','封弈'),
        7096:(7384,'周訪之子抚/扶，與襄城太守石覽同救荀崧，字形異文；周訪傳明示二子抚、光',SOURCE/'_cache/chunks/tj_13.txt','访遣子扶帅兵三千，与览共救崧'),
        7121:(7191,'段護遼/段遼，同一遼西段部主，石虎征遼西後奔平岡投燕遇害；親屬/卒年異說另存',SOURCE/'_cache/chunks/wsh_05.txt','末波死，国人立日陆眷弟护辽为主'),
        7179:(7107,'彭夫護/彭天護，同父蕩仲/仲蕩，佯敗誘賈疋墜澗並殺之，單一事件人名異文',SOURCE/'_cache/chunks/js_08.txt','为夫护所害'),
        7441:(7234,'後趙侍中，戲孫珍目疾，被石宣誅父子',SOURCE/'_cache/chunks/sl_02.txt','崔约'),
        7409:(7254,'石虎主書，持節監石斌，因呵禁淫獵遭殺',SOURCE/'_cache/chunks/sl_02.txt','遣主书礼仪持节监之'),
        7294:(7185,'成漢李演上書勸李壽奉晉而遭殺；extract date341 is not independent evidence',annals,'李演复上书言之；寿怒，杀演'),
        7457:(7080,'苻洪子、苻健弟、苻堅父苻雄；same 前秦輔國將軍',SOURCE/'_cache/chunks/wsh_02.txt','遣弟辅国将军雄率步骑五千入自潼关'),
        8532:(7764,'何充弟何準，二何佞佛之典同源；7764 also needs officer/隐士 key split',SOURCE/'_cache/chunks/js_12.txt','二郗谄于道，二何佞于佛'),
        8729:(7561,'王廙子王胡之，丹楊尹/司州刺史，同一人',annals,'胡之，廙之子也'),
        9221:(7842,'關中杜洪之司馬張琚；356使者追記其已敗亡，不是356在世',SOURCE/'_cache/chunks/sl_09.txt','杜洪张琚'),
        9229:(7846,'據長安杜洪；356使者追記先前事，不是356在世',SOURCE/'_cache/chunks/sl_09.txt','杜洪张琚'),
    }
    for ident,(target,reason,path,phrase) in duplicates.items():
        result=results[ident]
        result.update(duplicate_of=target,identity_status='same_historical_individual_corroborated',review_status='duplicate')
        result['evidence'].append(evidence(path,phrase,'重複身份原文對讀',reason))
        result['unresolved'].append('canonical_dates_require_separate_review')
    results[7764]['review_status']='identity_conflict'
    results[7764]['identity_status']='merged_homonyms_require_split'
    results[7764]['unresolved'].append('何充弟隱士何準 mixed with 虞允之部將何準; do not merge military activity into the recluse')
    results[8696]['unresolved'].append('王羲之321–379 versus303–361 are conflicting chronological traditions; age59 alone does not decide; compare ID12183 identity before redirect')
    results[8105]['unresolved'].append('世說庾氏譜 says咸和六年遇害 at age19, but narrative ties death to蘇峻之亂; retain chronology conflict rather than silently resolve331/328')
    results[8937]['unresolved'].append('僧基397卒68 implies330 birth, but建元二年344 founding event precedes stated出家21; biography chronology requires critical review')
    results[8500]['unresolved'].append('通鑑387八月 explicit death versus compressed呂光傳386 narrative; prefer dated annals but retain variant')
    results[9234]['component_death']={'identity':'後趙皇帝石世','year':349,'precision':'year','calendar':'historical_CE_year'}
    results[7746]['component_death']={'identity':'後趙涼州刺史石寧','year':350,'precision':'year','calendar':'historical_CE_year'}
    # A proven activity before the installed appear is an upper bound, not a
    # warrant to delay officers with earlier careers to the first cached mention.
    results[9234]['proposed_appear']=348
    results[9234]['evidence'].append(evidence(annals,'遂立世为太子，以刘昭仪为后','資治通鑑·348年条','石世348立太子，文内称今世方十岁；不能据此接收CBDB同名出生338'))
    results[9234]['appearance_basis']='後趙皇帝石世 component only: explicit appointment as太子348 in石虎紀事; split CBDB identity before applying'
    results[9289]['proposed_appear']=328
    results[9289]['appearance_basis']='陶侃部將李根 component only: explicitly constructs白石壘328; latest justified appearance bound, not proven initial service'
    results[9486]['historical_death'].update(month=5,day_ganzhi='丁亥',precision='lunar_month_with_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9289]['identity_status']='cross_source_identity_unproved'
    results[7111]['unresolved'].append('死亡原因异文：通鉴作鸩杀，华阳国志作病还死道中、李寿佯言被药杀；338死亡年度可取，死因不可混同')
    results[8709]['historical_death'].update(precision='year_from_annual_narrative_context',year_alternatives=[367,368])
    results[8709]['unresolved'].append('通鉴367年条称寻卒；十六国春秋提及368之事件可能追述，保留纪年异文')
    # He Chong's age and Wang Xun's incompatible chronological traditions.
    ev=evidence(SOURCE/'_cache/chunks/js_12.txt','永和二年卒，时年五十五','晉書·何充傳','Explicit death346, age55; inferred birth292 by nominal age')
    results[7051]['historical_birth']=date(292,ev,age=55)
    results[7051]['evidence'].append(ev)
    results[7051]['unresolved'].remove('birth_date_not_independently_corroborated')
    ev=evidence(SOURCE/'_cache/chunks/js_09.txt','四年，以疾解职。岁余，卒，时年五十二','晉書·王導傳附王珣','Conflicts with 通鑑400 death: biography says more than a year after 隆安四年400; retain variant')
    results[9449]['evidence'].append(ev)
    results[9449]['historical_birth']=date(349,ev,age=52)
    results[9449]['historical_birth']['calculation']='400 (preferred 通鑑 death year) - 52 (晉書 nominal age) + 1 = 349'
    results[9449]['historical_birth']['year_alternatives']=[349,350]
    results[9449]['historical_death']['year_alternatives']=[400,401]
    results[9449]['unresolved'].extend(['通鉴400五月卒与晋书隆安四年解职岁余卒(401)冲突；349/350生均只由52虚岁倒算','CBDB c_index_year327 is not an activity date'])
    results[9449]['proposed_appear']=363
    results[9449]['appearance_basis']='通鑑363明确桓温任王珣主簿，游戏366晚于已知任职；363是最迟登场上限，并不证明首次出仕'
    results[9449]['evidence'].append(evidence(annals,'王珣为主簿，每事必与二人谋之','資治通鑑·363年条','Already serving363, so game appear366 must be earlier'))
    for ident,chunk,phrase,year in [(7001,'hy_00','而霸、保皆暴病死',336),(7070,'wsh_00','建国十年卒。太祖即位，追赠秦王',347),(9423,'wsh_01','神\ue154元年卒',428)]:
        ev=evidence(SOURCE/'_cache/chunks'/f'{chunk}.txt',phrase,chunk,'Named biographical subject and reign-year context read; birth remains unattested')
        results[ident].update(historical_death=date(year,ev),review_status='primary_text_corroborated',identity_status='biographical_subject_checked')
        results[ident]['evidence'].append(ev)
        results[ident]['unresolved'].remove('death_date_not_independently_corroborated')
    ev=evidence(SOURCE/'_cache/chunks/sz_00.txt','到升平末','比丘尼傳·淨檢傳','Hagiographic account of departure/ascent at age70 late in升平357–361; not an exact361 death certificate')
    results[7076].update(historical_death=dict(year_range=[357,361],precision='late_reign_period',calendar='Chinese_reign_period_CE_equivalent',tradition_or_dispute=True,source=ev['citation'],excerpt=ev['excerpt']),
        historical_birth=dict(year_range=[288,292],precision='inferred_range_from_nominal_age70_and_late_reign_death',calendar='historical_CE_year',source=ev['citation'],excerpt=ev['excerpt']),review_status='primary_text_corroborated',identity_status='biographical_subject_checked')
    results[7076]['evidence'].append(ev)
    results[7076]['unresolved'].append('升平末非精确361，且腾空而上为宗教传记叙述；年月日未确证')
    results[7075]['evidence'].append(evidence(annals,'久之，与文鸯、邵续皆为后赵所杀','資治通鑑·321年条','Forward summary after capture321; 久之 does not prove exact death321'))
    results[7075]['identity_status']='biographical_subject_checked_death_date_unresolved'
    results[7075]['unresolved'].append('321俘后久之遇害；不能仅因放在321年条即自动改卒年为321')
    originals={x['id']:x for x in map(json.loads,(ROOT/'out/person_chronology/original_source_evidence.jsonl').open(encoding='utf-8'))}
    # The 163 CBDB records in this shard were read as original field tables,
    # including their notes and range qualifiers. Zero means unknown. The many
    # Tackett values are estimates for genealogical reconstruction, not exact
    # dates recovered from an inscription or an annal in this review.
    for ident,original in originals.items():
        if ident not in results or not original.get('cbdb_originals'):
            continue
        raw=original['cbdb_originals']
        result=results[ident]
        result['secondary_database_field_review']='original date fields/notes read; zero/NULL unknown; index year is never activity; range qualifiers retained'
        result['evidence'].append(dict(citation='CBDB original BIOG_MAIN date fields',path=str((ROOT/'out/person_chronology/original_source_evidence.jsonl').resolve()),excerpt=json.dumps(raw,ensure_ascii=False),verification='original_database_fields_individually_read; primary historical references not independently read here'))
        only_database=all(k.startswith('cbdb:') for k in result['keys'])
        if only_database and ident!=9286:
            result.update(review_status='secondary_source_checked',identity_status='original_database_identity_record_read_not_independently_proven')
            result['unresolved'].append('database_cited_primary_text_or_inscription_still_requires_reading')
            if len(raw)==1 and raw[0].get('c_deathyear') and raw[0].get('c_dy_range')==2:
                row=raw[0]
                result['historical_death']=dict(year=row['c_deathyear'],relation='circa',precision='database_estimated_year',calendar='CE_as_recorded_in_CBDB',source=f'CBDB BIOG_MAIN c_personid={row["c_personid"]}',excerpt=row.get('c_notes'),range_code=2,verification='secondary_database_estimate_not_primary_text_corroborated')
                result['unresolved'].append('estimated_death_must_not_be_installed_as_exact_year_without_preserving_approximation')
    for ident in [9234,9286,9289,9449]:
        raw=originals[ident].get('cbdb_originals',[])
        results[ident]['evidence'].append(dict(citation='CBDB original BIOG_MAIN date fields',path=str((ROOT/'out/person_chronology/original_source_evidence.jsonl').resolve()),excerpt=json.dumps(raw,ensure_ascii=False),verification='original_database_fields_read; index_year_not_activity; approximate_death_not_exact'))
    results[9286].update(review_status='identity_conflict',identity_status='anonymous_same_name_records_not_demonstrably_identical')
    results[9286]['unresolved'].append('CBDB183961/183987两个宋某不能凭匿名姓氏合并；431/424均为Tackett约估卒年(c_dy_range2)，368/361是指数年')
    for ident in [9491,9492]:
        wiki=originals[ident]['wikipedia_originals'][0]
        ev=dict(citation=f'Wikipedia: {wiki["title"]}',url=f'https://en.wikipedia.org/w/index.php?oldid={wiki["revision_id"]}',excerpt=wiki['lead_wikitext'][:1600],revision_id=wiki['revision_id'],verification='cached original read; parent verified page QID')
        results[ident]['evidence'].append(ev)
        results[ident]['identity_status']='qid_matched_secondary_source_identity_checked'
        results[ident]['review_status']='secondary_source_checked'
    ev=results[9492]['evidence'][-1]
    results[9492]['historical_birth']=dict(year=120,relation='circa',precision='approximate_year',calendar='CE_as_reported',source=ev['citation'],excerpt='(c. 120 AD – after 175 AD)')
    results[9492]['historical_death']=dict(year=175,relation='after',precision='lower_bound',calendar='CE_as_reported',source=ev['citation'],excerpt='(c. 120 AD – after 175 AD)')
    results[9492]['appearance_basis']='game birth+16 estimate136 remains unverified; source approx120 is not an exact birth and no first career date established'
    results[9491]['unresolved'].append('179 only in Short description in consulted lead; full body and scholarly source needed before death replacement; birth112 unsupported')

    # Combined explicit annal date + biographical age, each separately located.
    combined_ages=[
        (7932,357,50,'js_13','未至，卒于历阳，时年五十','夏五月庚午，镇西将军谢尚卒',False),
        (8612,377,58,'js_11','寻卒，时年五十八','丙辰，征西大将军、荆州刺史桓豁卒',True),
        (8760,384,61,'js_00','太元九年，崩于显阳殿，年六十一',None,False),
        (8999,357,27,None,'时年二十七，是岁晋升平元年也',None,False),
        (9172,377,42,'js_10','年四十二，先愔卒',None,False),
        (9295,361,19,None,'五月丁巳，帝崩于显阳殿，时年十九',None,False),
        (7754,376,72,'jk_02','年七十二解驾违世',None,False),
    ]
    for ident,year,age,chunk,age_phrase,death_phrase,is_annals in combined_ages:
        path=SOURCE/'_cache/chunks'/f'{chunk}.txt' if chunk else SOURCE/'_cache/texts/晋书.txt'
        ev=evidence(path,age_phrase,'建康實錄·太元元年许穆条' if ident==7754 else '晉書·本人紀傳',f'Nominal death age{age}; birth inferred from independently read death context{year}, not an exact birth date')
        result=results[ident]
        result['evidence'].append(ev)
        if death_phrase:
            death_ev=evidence(annals if is_annals else SOURCE/'_cache/texts/晋书.txt',death_phrase,f'{"資治通鑑" if is_annals else "晉書帝紀"}·{year}年条','Explicitly dated death of named subject')
            result['evidence'].append(death_ev)
        else:
            death_ev=ev
        result['historical_death']=result['historical_death'] or date(year,death_ev)
        result['historical_birth']=date(year-age+1,ev,age=age)
        result.update(review_status='primary_text_corroborated',identity_status='biographical_subject_checked')
        result['unresolved']=[u for u in result['unresolved'] if u not in ['birth_date_not_independently_corroborated','death_date_not_independently_corroborated']]
        result['unresolved'].append('birth_year_inferred_from_nominal_age_not_explicit_birth_record')
    results[9295]['proposed_name']='司馬聃'
    results[9295]['historical_death'].update(month=5,day_ganzhi='丁巳',precision='lunar_month_with_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9295]['proposed_appear']=344
    results[9295]['appearance_basis']='建元二年344九月丙申立太子，己亥即帝位，年2；帝紀所載幼帝不適用遊戲出生+16規則'
    results[9295]['evidence'].append(evidence(SOURCE/'_cache/texts/晋书.txt','穆皇帝讳聃，字彭子，康帝子也。建元二年九月丙申，立为皇太子','晉書·帝紀第八','Next sentences record即位 at nominal age2 in344, independently supporting inferred birth343 rather than CBDB342'))
    results[9295]['unresolved'].append('CBDB作司馬耽342生/卒20与帝纪聃343生推算/卒19不合，采用帝纪并保留差异')
    ev=evidence(SOURCE/'_cache/chunks/js_00.txt','太和六年崩，葬于敬平陵','晉書·廢帝孝庾皇后傳','Same道憐、庾冰女 identity; year371 conflicts with 通鑑366五月戊寅')
    results[8007]['evidence'].append(ev)
    results[8007]['historical_death']['year_alternatives']=[366,371]
    results[8007]['unresolved'].append('通鉴366五月卒与晋书太和六年371冲突，原文年号需校勘；当前优选通鉴明列年月干支')
    for ident,chunk,phrase,interpretation in [
        (7083,'sl_03','阳裕字士伦右北平无终人也','Read entire biography through及死皝甚悼之比葬三临时年六十一; age61 attested but no explicit death year in this biography'),
        (7104,'js_17','张忠，字巨和，中山人也','Read complete biography:年在期颐 means advanced age about100, not a precise age/date; 苻坚召至长安后返行华山卒，no explicit year'),
    ]:
        ev=evidence(SOURCE/'_cache/chunks'/f'{chunk}.txt',phrase,'原书本人传',interpretation)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='complete_short_biography_read')
        results[ident]['evidence'].append(ev)
    results[7083]['reported_death_age']={'age':61,'system':'traditional_nominal_age','source':'十六国春秋·陽裕傳','date_derivation':'none because death year not explicitly established'}
    results[7088]['appearance_upper_bound']={'year':290,'relation':'at_or_before','basis':'本人传明确晋武帝召见并拜右部都尉；武帝在位结束290，此为可靠上限而非初仕年'}
    results[7088]['unresolved'].append('游戏出生291/登场307晚于已在晋武帝朝任职；历史生年未知，不能继续把291解释为史实')
    for ident,chunk,phrase,interpretation in [
        (7084,'sl_01','南安[一作新平]厐寔等各起兵江淮间','龐/厐字形异体；王如侯脱同时起兵，未记生卒'),
        (7093,'sl_01','汝南王佑奔建康','Explicit surname abbreviated within 晋宗室王爵; escape is not death'),
        (7094,'js_01','滔历位散骑常侍，薨','章武王混小子，嗣新蔡后还袭章武，known death but no explicit year'),
        (7097,'js_13','悦弟潜谋杀逊','周悦弟潜，谋以赵混子涛代王逊，事觉被诛；本段没有确切年号'),
        (7142,'js_13','赵混子涛代为刺史','同上王逊传，赵涛为赵混子，死亡无明年'),
        (7145,'js_17','郭文，字文举，河内轵人也','Complete biography read through以十五日终、万宠葬祭；no explicit birth/death year; 年三十 refers to earlier mountain travels'),
        (7149,'tj_13','州人以秘子寿领州事','Retrospective account of顧秘子顧壽 killed by梁碩; not necessarily the annual heading date'),
    ]:
        ev=evidence(SOURCE/'_cache/chunks'/f'{chunk}.txt',phrase,'原书本人事迹段',interpretation)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='abbreviated_or_variant_named_subject_checked')
        results[ident]['evidence'].append(ev)
    for ident,phrase in [(7155,'京为蜀王'),(7158,'恒为代王'),(7159,'持为赵王'),(7160,'操为魏王'),(7161,'旭为丹杨王'),(7162,'晃为临川王'),(7163,'皋为零陵王'),(7164,'鸿为楚王'),(7165,'鸾为燕王'),(7178,'张实二女徽光、丽光为贵人')]:
        ev=evidence(SOURCE/'_cache/chunks/tj_12.txt',phrase,'資治通鑑·312年册封、纳贵人条','Explicit 劉聰父子 or 張實父女 relationship establishes abbreviated personal name; no birth/death date in consulted passage')
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='kinship_and_title_identify_abbreviated_subject')
        results[ident]['evidence'].append(ev)
    for ident,chunk,phrase,year in [(7113,'sl_09','因尽杀雄诸子纳等',338),(7115,'hy_00','并杀雄子豹等',338),(7144,'sl_01','雄攻安次斩其部大人那楼竒',338),(7108,'tj_16','乃相与杀屈而立孤',338)]:
        ev=evidence(SOURCE/'_cache/chunks'/f'{chunk}.txt',phrase,'原书·338年条','Named abbreviated subject and containing annual context read; no birth date attested')
        results[ident].update(historical_death=date(year,ev),review_status='primary_text_corroborated',identity_status='dated_event_subject_individually_checked')
        results[ident]['evidence'].append(ev)
        results[ident]['unresolved'].remove('death_date_not_independently_corroborated')
    results[8500]['historical_death']['year_alternatives']=[386,387]
    results[8500]['evidence'].append(evidence(SOURCE/'_cache/texts/十六国春秋.txt','张大豫自西郡入临洮','十六國春秋·後涼錄一·太安元年十一月至十二月叙次','Death narrative is placed after十一月 and before十二月/太安二年; incompatible with explicit通鑑387八月. 晉書 compresses same episode before呂光改元太安; not an independently explicit death386 sentence'))
    results[8500]['unresolved'].append('Explicit annals387八月 versus 十六國春秋太安元年386冬叙次: historical year remains disputed; exclude strict exact-year patch')
    ev=evidence(SOURCE/'_cache/chunks/sz_02.txt','春秋八十三，晋泰元五年卒','名僧傳抄·惠永傳','Actual original text states380, not a JSON extraction error;師竺曇現、凌雲精舍 contrasts with 高僧傳潘姓慧永/西林寺414')
    results[7396]['evidence'].append(ev)
    results[7396].update(review_status='identity_conflict',identity_status='same_or_distinct_monk_traditions_unresolved',
        historical_birth=dict(year_alternatives=[298,332],precision='disputed_inferred_year_from_nominal_age83',calendar='historical_CE_year',source='名僧傳抄/高僧傳',calculation=['380-83+1=298','414-83+1=332']),
        historical_death=dict(year_alternatives=[380,414],precision='source_traditions_conflict',calendar='historical_CE_year',source='名僧傳抄/高僧傳',excerpt=ev['excerpt']))
    results[7396]['unresolved'].append('Both biographies explicitly age83, but death380 versus414 and different teacher/temple: cannot assert an uncontested correction to332–414 or an all-keys identity merge')
    for result in results.values():
        if len(result['keys'])==1 and result['review_status'] in ['primary_text_corroborated','source_checked_dates_unknown','duplicate'] and not result['keys'][0].startswith(('cbdb:','gl:','wk:')):
            result['source_identities_all_adjudicated']=True
        if (result['historical_birth'] or result['historical_death']) and not result['source_identities_all_adjudicated']:
            result['date_scope']='dated source component only; all merged source identities must be adjudicated before applying to whole game row'
    # Two spellings and the same linked family/office biography were read in
    # both original source chunks, rather than inferred from the name alone.
    results[7103]['source_identities_all_adjudicated']=True
    # Both original named lists/kinship contexts, not merely extraction JSON,
    # have been read in these two-source cases.
    for ident in [7107,7155,7158,7159,7160,7161,7162,7163,7164,7165,7178,8760,9295]:
        results[ident]['source_identities_all_adjudicated']=True
    # Withholding deletion is essential where the canonical row itself is mixed
    # or not every source component of the candidate has been checked.
    for ident in [8532,8729]:
        result=results[ident]
        result['candidate_duplicate_of']=result['duplicate_of']
        result['duplicate_of']=None
        result['review_status']='needs_further_research'
        result['identity_status']='duplicate_component_found_whole_record_redirect_withheld'
        result['source_identities_all_adjudicated']=False
        result['unresolved'].append('Do not delete whole row until all source keys and any mixed canonical identity are resolved')

    # Cross-shard identity audit, explicitly requested by the coordinator.
    # No birth/death is installed on a root record that mixes mother and wife.
    def chunk_ev(chunk,phrase,citation,meaning):
        return evidence(SOURCE/'_cache/chunks'/f'{chunk}.txt',phrase,citation,meaning)

    def wiki_identity(ident,excerpt):
        wiki=originals[ident]['wikipedia_originals'][0]
        return dict(citation=f'Wikipedia identity: {wiki["title"]}',
            url=f'https://en.wikipedia.org/w/index.php?oldid={wiki["revision_id"]}',
            revision_id=wiki['revision_id'],excerpt=excerpt,
            verification='original cached revision read; page QID verification by coordinator; identity only, not dates')

    zuo_evs=[
        chunk_ev('js_15','祚字太伯，博学雄武','晉書·張軌傳附張祚','張重華庶兄、與馬氏私通、字太伯、和平君主，非別一張祚'),
        chunk_ev('js_17','祚遣其太子太和以执友礼造之','晉書·宋纖傳','涼州君主張祚與太子太和，和王室傳同一身份'),
        chunk_ev('sl_04','生闻张祚见杀玄靓幼冲','十六國春秋·前秦錄','回述前涼王祚被殺、玄靚幼沖，非此年仍生或其他張祚'),
        chunk_ev('wsh_00','重华庶兄祚杀曜灵而自立','魏書·序紀','重華兄祚、廢曜靈，身份鎖定'),
        chunk_ev('wsh_04','祚，字太伯。既统任','魏書·張寔傳附張祚','同字太伯、涼公、和平、太和之父，全部6原書支一人'),
        chunk_ev('sl_09','张祚字太伯小字螽斯骏之庶长子也','十六國春秋·前涼錄','根片sl09也同字、父、兄弟及篡位事件'),
    ]
    results[7968]['evidence'].extend(zuo_evs)
    results[7968]['source_identities_all_adjudicated']=True
    results[7968]['unresolved'].append('庶兄重華而遊戲出生332，重華原書年齡異說327/330/331；332不可作已證生年，仍待估算政策處理')
    cross=[dict(id=12804,keys=roster_by_id[12804]['keys'],review_status='duplicate',duplicate_of=7968,
        candidate_duplicate_of=None,identity_status='same_former_liang_ruler_all_source_keys_checked',
        source_identities_all_adjudicated=True,evidence=zuo_evs+[
            wiki_identity(12804,'full_name=Zhang Zuo (張祚); father=Zhang Jun; predecessor=Zhang Yaoling; era_name=Heping'),
            dict(citation='CBDB BIOG_MAIN17859 original fields',excerpt=json.dumps(originals[12804]['cbdb_originals'],ensure_ascii=False),verification='Original notes explicitly identify the Former Liang ruler; 346/348 chronology is not supported by the read primary annals')],
        unresolved=['Identity merge does not validate CBDB348 death; primary sources support355. Preserve contradictory database claim separately; do not copy its index285 into life/activity.','Birth not established; no new dates assigned on this cross-shard decision'])]

    jun_general=chunk_ev('js_11','获襄将张骏、杨凝等，徙于寻阳','晉書·桓沖傳','姚襄將張駿，尋陽遷徒殺趙毗，與涼王公庭不同身份')
    jun_death=chunk_ev('tj_18','姚襄故将张骏杀江州督护赵毘','資治通鑑·363年十一月','姚襄將張駿363北叛被桓沖討斬')
    jun_prince=chunk_ev('js_15','骏字公庭，幼而奇伟','晉書·張軌傳附張駿','張寔子、嗣張茂、涼州牧西平公，為前涼主')
    cross.append(dict(id=12812,keys=roster_by_id[12812]['keys'],review_status='primary_text_corroborated',duplicate_of=None,candidate_duplicate_of=None,
        identity_status='distinct_homonym_from_8486_confirmed',evidence=[jun_general,jun_death,jun_prince],
        unresolved=['撤銷8486同名候選：12812為前涼張駿字公庭，8486為姚襄部將；不可合併。','This decision compares these identities only; root source-key/date review remains authoritative.']))
    results[8486].update(review_status='primary_text_corroborated',identity_status='yao_xiang_general_distinct_from_former_liang_ruler',source_identities_all_adjudicated=True,
        historical_death=date(363,jun_death))
    results[8486]['evidence'].extend([jun_general,jun_death,chunk_ev('sl_06','部将张骏杨凝等皆为温所获徙于寻阳','十六國春秋·姚襄傳','第三支同姚襄將與尋陽，8486的全部3key同一身份')])
    results[8486]['unresolved']=[u for u in results[8486]['unresolved'] if u!='death_date_not_independently_corroborated']
    results[8486]['historical_death'].update(month=11,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')

    gou_ev=chunk_ev('tj_18','母苟氏为皇太后，妃苟氏为皇后','資治通鑑·357年条','Same sentence explicitly separates 苻堅母 and 苻堅妃; two women with same surname')
    gou_ev2=chunk_ev('sl_04','母茍氏为皇太后妃茍氏为皇后','十六國春秋·永興元年条','Separate mother and wife in original reign record')
    gou_mother=chunk_ev('sl_05','太后茍氏略阳氐人坚所生母也','十六國春秋·堅太后茍氏傳','Mother of Fu Jian; postpartum ritual/murder of苻法 stories belong to mother, not his wife')
    gou_wsh=chunk_ev('wsh_02','见母苟氏通李威之事','魏書·苻健傳附苻堅','Also explicitly the mother; not spouse')
    cross.append(dict(id=10094,keys=roster_by_id[10094]['keys'],review_status='identity_conflict',duplicate_of=None,candidate_duplicate_of=None,
        identity_status='merged_fu_jian_mother_and_wife_require_source_key_split',source_identities_all_adjudicated=False,
        evidence=[gou_ev,gou_ev2,gou_mother,gou_wsh,wiki_identity(10094,'Empress Gou: husband Fu Jiān; original lead identifies her as wife. Lead says355 accession, conflicting with explicit357 primary reign record; identity only accepted.')],
        component_identities=[dict(keys=[k for k in roster_by_id[10094]['keys'] if not k.startswith('gl:')],identity='苻雄妻、苻堅母苟太后',keep_separate_from=9252),
            dict(keys=['gl:Q5374525'],identity='苻堅妻苟皇后',candidate_duplicate_of=9252)],
        unresolved=['整條hold，不得整條redirect9252或刪除。建議僅gl:Q5374525移至9252；四條母親原書key保留另一實體。','No top-level birth/death assigned to this mixed entity. 母375明德 versus妻獻穆 must not be conflated.']))
    results[9252].update(review_status='source_checked_dates_unknown',identity_status='fu_jian_wife_distinct_from_his_mother',source_identities_all_adjudicated=True)
    results[9252]['evidence'].extend([gou_ev,gou_ev2])

    kez_mother=chunk_ev('sl_03','俊后可足浑氏尚书令豫章公翼之从姊也','十六國春秋·景昭可足渾氏傳','Murong Jun wife/Murong Wei mother; cousin elder sister of Kezuhun Yi')
    kez_daughter=chunk_ev('sl_03','浑氏太后从弟尚书令豫章公翼女也','十六國春秋·建熙十年四月','Murong Wei wife is Yi daughter, a clan niece and daughter-in-law of Empress Dowager; not the dowager')
    kez_daughter2=chunk_ev('tj_18','太后从弟尚书令豫章公翼之女也','資治通鑑·369年四月','Explicitly same Wei wife as sl03 branch in mixed9271 and clean9454')
    kez_sister=chunk_ev('tj_18','以其妹长安君妻垂','資治通鑑·358年条','Chang’an Jun, dowager’s younger sister, married Murong Chui; a third woman')
    cross.append(dict(id=9825,keys=roster_by_id[9825]['keys'],review_status='primary_text_corroborated',duplicate_of=None,candidate_duplicate_of=None,
        identity_status='murong_jun_wife_murong_wei_mother_distinct_from_9271_9454',source_identities_all_adjudicated=True,
        evidence=[kez_mother,kez_daughter,kez_daughter2,kez_sister,
            chunk_ev('sl_05','评遂与太后可足浑氏共谋诛之','十六國春秋·慕容垂傳','Same Former Yan dowager conspiring with慕容評'),
            chunk_ev('sl_06','以其妹长安君妻垂','十六國春秋·慕容垂段后傳','Same dowager contrasted with her sister'),
            wiki_identity(9825,'spouse=Murong Jun; issue=Murong Ye, Murong Wei, Murong Chong; reign353–360 as empress consort')],
        unresolved=['不要合9271或9454：太后、太后妹長安君、太后從弟翼女，共三人。','Identity-only cross-shard review does not replace root date adjudications.']))
    results[9271].update(review_status='identity_conflict',identity_status='merged_murong_wei_wife_and_murong_chui_wife',duplicate_of=None,candidate_duplicate_of=None,source_identities_all_adjudicated=False)
    results[9271]['evidence'].extend([kez_daughter,kez_daughter2,kez_sister,kez_mother])
    results[9271]['component_identities']=[dict(keys=['sl:sl_03:233:可足渾氏'],identity='慕容暐妻、可足渾翼女',candidate_duplicate_of=9454),dict(keys=['tj:tj_18:99:可足渾氏'],identity='長安君、慕容垂繼室、太后妹')]
    results[9271]['unresolved'].append('Split first: sl03 branch duplicates9454; tj18 branch is separate Chang’an Jun. Neither is dowager9825.')
    results[9454].update(review_status='source_checked_dates_unknown',identity_status='murong_wei_wife_and_kezuhun_yi_daughter',source_identities_all_adjudicated=True)
    results[9454]['evidence'].extend([kez_daughter,kez_daughter2,kez_mother])

    sayu_heir=chunk_ev('sk_00','立王子<斯由>为太子','三國史記·美川王十五年','Micheon son Sayu appointed crown prince; birth not stated')
    sayu_death=chunk_ev('sk_01','<丽>王<&斯由{钊}>，力战拒之，中流矢死','三國史記·百濟近肖古王二十六年','Goguryeo king Sayu/Zhao (國岡王 in next accession account); not an unrelated same-name officer')
    cross.append(dict(id=12795,keys=roster_by_id[12795]['keys'],review_status='duplicate',duplicate_of=7322,candidate_duplicate_of=None,
        identity_status='gogugwon_sayu_micheon_son_same_person',source_identities_all_adjudicated=True,
        evidence=[sayu_heir,sayu_death,wiki_identity(12795,'birth_name=Go Sayu/Yu/Soe; father=Micheon; crown prince January314–February331; regnal name=Guggangsang Taewang')],
        unresolved=['Birthday not given; do not infer it from age at succession.','sk00 cache jumps from volume18 heading to23: cannot claim to have read missing Goguryeo volume18 biography.','Wiki infobox reign end372 conflicts with its death371; exact16 November conversion was not independently verified in this identity review.']))
    results[7322].update(review_status='source_checked_dates_unknown',identity_status='gogugwon_sayu_micheon_son_identified',source_identities_all_adjudicated=True)
    results[7322]['evidence'].extend([sayu_heir,sayu_death])
    results[7322]['attested_death_regnal_year']={'ruler':'百濟近肖古王','year':26,'season':'冬','source':'三國史記·百濟本紀','CE_conversion':'conventionally371; exact civil month/day not independently checked here'}
    results[7322]['unresolved'].append('本人Goguryeo卷18正文在缓存缺失；生日未知，另见百济纪26年冬中流矢死。')
    # Live primary transcription fills the cached volume18 gap. Volume30's
    # original synchronistic table supplies the CE conversion; no Wikipedia day
    # conversion is accepted merely because it occurs in an infobox.
    sayu_online=dict(citation='三國史記卷18·高句麗本紀六·故國原王',
        url='https://zh.wikisource.org/w/index.php?oldid=2270202&title=三國史記/卷18',
        source_revision_id=2270202,reviewed_on='2026-11-08',
        excerpt='諱斯由；是月二十三日，薨。',
        source_context='Opening identifies Sayu, also Zhao, made crown prince in Micheon15 and succeeding in Micheon32. Reign41 winter month10 records arrow wound and death on month day23.',
        verification='entire Gogugwon section read online; named subject and regnal month/day directly adjudicated')
    sayu_table=dict(citation='三國史記卷30·年表中',
        url='https://zh.wikisource.org/w/index.php?oldid=2020967&title=三國史記/卷30',
        source_revision_id=2020967,reviewed_on='2026-11-08',
        excerpt='辛未 簡文皇帝 咸安元年；四十一 故國原王薨',
        source_context='Same row: Gogugwon41, BaekjeGeunchogo26, JinXianan1 (371). Accession row pairs Gogugwon1 with JinXianhe6 (331). Crown prince year Micheon15 is JinJianxing2 (314).',
        verification='primary synchronistic table read; CE year conversion independently corroborated, not inferred from biography estimates')
    results[7322]['evidence'].extend([sayu_online,sayu_table])
    results[7322]['historical_death']=dict(year=371,month=10,day=23,precision='lunisolar_day',
        calendar='historical_Korean_lunisolar_CE_year_by_Jin_synchronism',source=sayu_online['citation'],
        excerpt=sayu_online['excerpt'],source_urls=[sayu_online['url'],sayu_table['url']],
        conversion='Gogugwon41 = JinXianan1 = CE371 by original volume30 table; civil/Gregorian/Julian day not converted')
    results[7322].update(review_status='primary_text_corroborated',identity_status='gogugwon_sayu_primary_biography_and_parentage_checked')
    results[7322]['unresolved']=[u for u in results[7322]['unresolved'] if u!='death_date_not_independently_corroborated' and not u.startswith('本人Goguryeo卷18正文')]
    results[7322]['appearance_upper_bound']={'year':314,'basis':'美川王15立王子斯由為太子；卷30年表對建興2=314','meaning':'already formally active by314; not proof of first career year; retain game appearance estimate unless contradictory'}
    cross[-1]['evidence'].extend([sayu_online,sayu_table])
    cross[-1]['unresolved']=[u for u in cross[-1]['unresolved'] if not u.startswith('sk00 cache')]
    cross[-1]['unresolved'].append('Cached gap filled from fixed-revision primary volume18 and synchronistic volume30; canonical7322 records lunar371-10-23, without converting to16November.')
    # Close the already-read first batch. A reviewed passage is not proof that
    # every merged source key or an unknown date has been resolved.
    for ident,chunk,phrase,meaning,all_keys in [
        (7008,'js_11','数月病卒','熊遠傳接前卷：敦引為長史後數月病卒，沒有明列卒年；其餘來源尚未逐鍵核完',False),
        (7039,'tj_12','王浚屡以手书召邃兄畅','遊邃兄暢，後文與浚俱沒是前瞻追述，不能用當條年份作卒年；sl03原文未定位',False),
        (7055,'tj_12','悝为高平王','劉聰冊封皇子之列，與sl00高平王悝相合；生卒未記',True),
        (7107,'tj_12','彭仲荡之子天护','sl00同殺賈疋事件，父名蕩仲/仲蕩、梁州/涼州有異文；未記生卒',True),
        (7143,'sl_00','大鸿胪边鸿','劉聰遣大鴻臚賜石勒弓矢冊命；未記生卒，不能據同類使者事件與范龕合併',True),
        (7177,'hy_00','汉中民张咸等讨难敌','三原書同漢中反氐首領；sl09稱楊茂搜、其餘稱楊難敵，事件敘次有差別，不作生卒依據；CBDB日期空',True),
        (7180,'yi_01','猗卢遣子曰利孙','索頭猗盧子利孫救劉琨；原段不載生卒，與六修是否同人未證',True),
    ]:
        ev=chunk_ev(chunk,phrase,'原書·本批已讀事迹段',meaning)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='consulted_named_subject_dates_unknown',source_identities_all_adjudicated=all_keys)
        results[ident]['evidence'].append(ev)
        results[ident]['unresolved'].append(meaning)
    results[7075].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=False)
    results[7071]['evidence'].append(chunk_ev('sl_00','上党李熹','十六國春秋·劉淵傳','晉武帝議涼州軍事，上黨李熹舉劉淵；與晉書上黨李憙字季和身份相合，但另一劉盛評語支尚不足單獨排除同名'))
    results[7071].update(candidate_duplicate_of=3869,duplicate_of=None,identity_status='li_xi_name_variant_candidate_needs_last_source_identity_check')
    results[7071]['unresolved'].append('李憙傳及齊王攸出鎮的敘述不可單憑出鎮通常紀年就定其卒晚於283；不據此否定遊戲282卒。')
    fu_death=evidence(annals,'秦东海敬武王雄攻乔秉于雍；丙申，卒','資治通鑑·354年六月','同段稱子堅襲爵；父洪、兄健、子堅鎖定苻雄，明記六月丙申卒')
    results[7080].update(historical_death=date(354,fu_death,month=6,sexagenary_day='丙申'),review_status='primary_text_corroborated',identity_status='fu_jian_father_and_fu_jian_ruler_brother',source_identities_all_adjudicated=True)
    results[7080]['historical_death'].update(precision='lunar_month_with_sexagenary_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7080]['evidence'].extend([fu_death,chunk_ev('ssxy_01','伯父健及父雄','世說新語注引車頻秦書','苻堅之伯父健與父雄，同前秦王室人物，非其他同名')])
    results[7080]['unresolved']=[u for u in results[7080]['unresolved'] if u!='death_date_not_independently_corroborated']
    for chunk,phrase in [('js_01','而为苻雄所败'),('js_18','健又遣子生、弟雄'),('sl_03','健遣雄略地渭北'),('sl_04','丞相东海王雄'),('sl_05','苻雄字符才洪之季子也'),('sl_09','为秦苻雄所破'),('tj_17','健遣苻雄徇渭北'),('wsh_02','遣弟辅国将军雄率步骑五千入自潼关')]:
        results[7457]['evidence'].append(chunk_ev(chunk,phrase,'原書·苻雄八來源逐鍵身份核對','本人傳、父洪兄健子堅及350/354征戰敘事相互吻合，已讀全八來源的本人段落'))
    results[7457].update(duplicate_of=7080,candidate_duplicate_of=None,review_status='duplicate',identity_status='same_fu_xiong_all_eight_primary_source_keys_checked',source_identities_all_adjudicated=True)
    results[7039]['evidence'].append(chunk_ev('sl_03','初王浚屡以手书招其兄','十六國春秋·游邃傳','兄名用𪽈異體，與通鑑暢對讀；同勸留而赴王浚、與浚俱沒敘事，兩來源身份均已核實'))
    results[7039]['source_identities_all_adjudicated']=True
    results[7039]['unresolved']=[u for u in results[7039]['unresolved'] if 'sl03原文未定位' not in u]
    results[7038]['evidence'].extend([chunk_ev('sl_02','北海逢羡广平游邃','十六國春秋·慕容廆紀','北海逢羨列股肱'),chunk_ev('sl_03','与逢羡宋奭皆尝为昌黎太守','十六國春秋·游邃傳','與遊邃宋奭同昌黎太守經歷，與逄羨同一名單身份')])
    results[7038]['source_identities_all_adjudicated']=True
    results[7043].update(duplicate_of=None,candidate_duplicate_of=None,review_status='identity_conflict',identity_status='merged_shu_and_shaoling_prefects_named_zheng_rong',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None)
    results[7043]['evidence'].extend([chunk_ev('sl_09','蜀郡太守江夏郑融','十六國春秋·成漢錄','與楊芬常歆共推蘭繼、出枳被李恭費黑俘，候選程融異文'),chunk_ev('js_19','邵陵太守郑融','晉書·杜弢傳','在荊湘為杜弢所害的邵陵太守，不能僅憑同名視為同一人')])
    results[7043]['component_identities']=[dict(keys=['sl:sl_09:333:鄭融'],identity='蜀郡太守鄭融，程融異文候選',candidate_duplicate_of=7020),dict(keys=['js:js_19:125:鄭融'],identity='杜弢所害邵陵太守鄭融')]
    results[7043]['unresolved'].append('撤回整條duplicate7020：蜀郡太守與邵陵太守兩支先分清，不能刪除混合記錄。')
    # Round 2: 50 reviewed rows, including canonical7166. Sources are literal
    # passages read in this round, with per-key identity decisions below.
    round2_reads={
        7051:[('js_00','中书令何充'),('js_11','司空何充称其明惠'),('js_12','何充，字次道'),('js_16','内史何充上疏'),('sl_03','遂与何充等奏听皝称燕王'),('sz_00','晋永和四年春'),('tj_16','都乡文穆侯何充卒'),('wsh_02','骠骑将军何充立岳子聃'),('js_03','会稽内史何充笺'),('js_10','骠骑何充辅政'),('js_13','何充荐沉于王导'),('js_14','会稽内史何充引为参军'),('js_15','司空何充为扬州'),('js_17','兄充为骠骑将军'),('sg_04','司空文穆公充恽之孙也'),('gs_00','司空何充复更修造'),('gs_01','尚书令何充仆射'),('ssxy_00','何充字次道庐江人'),('ssxy_01','时何充为敦主簿'),('ssxy_02','充导妻姊之子'),('ssxy_03','何充性好佛道')],
        7071:[('tj_12','李熹见之，叹曰'),('sl_00','上党李熹')],
        8729:[('js_12','颐之弟胡之，字修龄'),('js_16','郡将王胡之深异之'),('js_17','裒长史王胡之亦劝焉'),('tj_16','长史王胡之说裒'),('tj_18','吴兴太守王胡之为司州刺史'),('js_14','太守王胡之名知人'),('ssxy_00','胡之字修龄琅邪临沂人王廙之子也'),('ssxy_01','太守王胡之避司马无忌之难'),('ssxy_02','修龄王胡之小字也'),('ssxy_03','王胡之与无忌长甚相昵')],
        7561:[('tj_17','以丹杨君王胡之代之，未行而卒')],
        7166:[('tj_16','守将劳杨败死')],
        7181:[('sl_00','北地太守曲昌'),('tj_13','北地太守麹昌'),('js_15','太守麹昌遣使求救')],
        7182:[('sl_00','右仆射朱纪女为贵妃'),('tj_08','继母朱氏遇之无道'),('ssxy_00','继室以庐江朱氏生览')],
        7183:[('sl_01','武遂令李回'),('tj_13','武遂令李回')],
        7184:[('js_11','破蜀将李桓于江阳'),('js_13','时朱轨、赵诱、李桓率众'),('sl_09','破别将李桓于江阳')],
        7185:[('hy_00','摅从兄演自越'),('tj_16','寿怒，杀演')],
        7186:[('sl_01','马严士众多李潜军人')],
        7187:[('js_08','手擒贼帅李羌')],
        7188:[('tj_13','始平太守杨像'),('js_15','始平太守杨像')],
        7189:[('sl_09','流民杨武大略汉中吏民来奔'),('js_07','逆贼王如余党李运、杨武')],
        7190:[('js_11','西阳太守樊峻精兵一万'),('js_13','西阳太守樊峻以万人'),('sl_02','西阳太守樊峻与毛寳'),('tj_16','毛宝、樊峻突围出走')],
        7191:[('sl_01','虎将伐鲜卑段辽于辽西'),('sl_02','段辽弟兰来降'),('sl_03','出奔段辽辽素闻其才'),('tj_14','段疾陆眷之孙辽欲夺其位'),('tj_15','段辽素闻其才'),('tj_16','段辽谋反于燕'),('js_01','鱼窃乘'),('js_03','末波死，弟辽代立'),('js_08','就陆眷之孙辽立')],
        7192:[('js_11','将军路永、毛宝'),('js_13','毛宝，字硕真'),('js_17','亮令毛宝屯邾城'),('sl_01','毛寳为征虏将军戍邾城'),('sl_02','西阳太守樊峻与毛寳'),('tj_15','平南参军荥阳毛宝'),('tj_16','毛宝、樊峻突围出走'),('js_09','获其将毛宝、高宝、梁堪'),('js_19','会毛宝援宣')],
        7193:[('sl_02','沐坚字壁强河间人也'),('tj_17','留宁北将军沐坚戍幽州')],
        7194:[('sl_00','京都既陷嵩亦为曜所灭'),('tj_13','焦嵩、陈安举兵逼上邽'),('js_15','焦嵩，安定人')],
        7195:[('sl_08','西平王叔与曹袪余党'),('js_14','西平王叔与曹祛余党')],
        7196:[('tj_13','汉中常侍王沈')],
        7197:[('sl_00','为子纳司空太原王沉女'),('sl_00','中常侍王沉宣懐'),('js_16','王沉，字彦伯，高平人也')],
        7198:[('sl_00','大将军从事中郎王皮'),('tj_13','大将军从事中郎王皮')],
        7199:[('sl_08','督护王该等率歩骑五千人'),('tj_13','将军王该帅步骑五千'),('js_14','遣督护王该送诸郡贡计')],
        7200:[('sl_00','少府皇甫阳'),('tj_13','少府皇甫阳')],
        7201:[('tj_13','扶风太守竺爽'),('js_15','扶风太守竺爽')],
        7202:[('ssxy_00','建兴四年与第五琦据荆州')],
        7203:[('sl_00','中书监范隆左仆射马景女皆为贵人')],
        7204:[('tj_13','平乐太守董霸')],
        7205:[('sl_02','杀将军蔡懐'),('tj_16','杀将军蔡怀')],
        7206:[('sl_02','郝庄随相蔡熊等五人')],
        7207:[('sl_02','郑豹谈玄郝庄')],
        7208:[('js_08','南和令赵领'),('sl_01','南和令赵领')],
        7209:[('sl_02','谈玄郝庄随相')],
        7210:[('sl_00','中宫仆射郭猗'),('tj_13','中宫仆射郭猗')],
        7211:[('sl_02','杀将军郑豹谈玄'),('tj_16','杀郑豹等五将军')],
        7212:[('sl_00','中黄门陵修等皆宠幸用事')],
        7213:[('js_09','南蛮校尉陶称'),('js_11','又以陶称为南中郎将'),('tj_16','称将二百人下见亮'),('js_14','南蛮校尉陶称')],
        7214:[('sl_08','贼曹佐髙昌隗瑾'),('tj_13','贼曹佐高昌隗瑾'),('js_14','贼曹佐高昌隗瑾')],
        7215:[('sl_02','郑豹谈玄郝庄随相蔡熊等五人')],
        7216:[('tj_13','平夷太守雷炤')],
        7217:[('sl_00','上郡太守韦藉')],
        7218:[('sl_01','冯睹率众降勒')],
        7219:[('tj_13','长史乙郍娄冯追谢之')],
        7220:[('js_00','隗举其从子佣娶第三者'),('js_14','并免尚书傅玩、郎刘佣官')],
        7221:[('sl_00','骑兵将军刘勲追讨之'),('tj_13','骑兵将军刘勋追击之')],
        7222:[('js_13','遣长史刘浚留镇扬口垒'),('tj_13','留长史刘浚镇扬口垒')],
        7223:[('sl_00','刘颉邀击之为固所败')],
        7224:[('sl_09','晋守将孟彦刘齐李秋等皆死之'),('tj_16','守将孟彦、刘齐、李秋皆死')],
        7225:[('sl_09','守将劳扬战败死之'),('tj_16','守将劳杨败死')],
    }
    round2_evidence={}
    for ident,readings in round2_reads.items():
        evs=[chunk_ev(chunk,phrase,'第2批原書身份/事迹逐條核讀','Read actual named paragraph and its family/office/event context; extraction output is only an index, not evidence') for chunk,phrase in readings]
        round2_evidence[ident]=evs
        result=results[ident]
        result['evidence'].extend(evs)
        result['last_manual_review_batch']='round2_50_rows_7051_7071_7166_7561_8729_7181_7225'
        result['source_keys_adjudicated_this_batch']=[k for k in result['keys'] if len(k.split(':'))>2 and k.split(':')[1] in [x[0] for x in readings]]
        if ident in [7051,8729,7561,7166]:
            continue
        result.update(review_status='source_checked_dates_unknown',identity_status='original_named_context_individually_reviewed',source_identities_all_adjudicated=True)
    # Empty CBDB originals supply a name but no independent identity context.
    for ident in [7188,7190,7201]:
        results[ident]['source_identities_all_adjudicated']=False
        results[ident]['unresolved'].append('Original CBDB row was read: exact name, but all dates, notes and offices empty. Primary keys agree; the additional database identity cannot be independently adjudicated from a bare name.')
    results[7051]['source_identities_all_adjudicated']=True
    results[7051]['source_chronology_conflicts']=[dict(source='比丘尼傳·建福寺淨檢尼傳',claim='永和四年348見何充、施別宅立寺',conflicts_with='晉書永和二年卒55、通鑑346正月己卯卒',resolution='Same Buddhist patron He Chong, not a new person; conflicting temple activity year retained as a source error/dispute, never used as proof of348 survival.')]
    results[7051]['unresolved'].append('sz00稱永和四年348見何充，與兩部紀傳346卒衝突；寺院活動記年須校勘，未用它改卒年或延後登場。')
    results[7071].update(review_status='source_checked_dates_unknown',identity_status='two_li_xi_passages_read_last_identity_link_unproven',source_identities_all_adjudicated=False)
    results[7071]['unresolved'].append('劉盛少時獲李熹評語，沒有字籍官職；雖與上黨李熹/李憙相容，仍不足只憑同名整條併入3869。')
    results[7071]['component_appearance_constraints']=[dict(keys=['sl:sl_00:25:李熹'],year=290,relation='at_or_before',basis='晉武帝問將於上黨李熹並論樹機能，武帝卒290；僅給此來源身份的活動上限',conflict_with_game_estimate='birth292 and appear308 postdate the attested Western Jin audience; full row identity remains on hold')]
    wh_death=round2_evidence[7561][0]
    results[7561].update(historical_death=date(356,wh_death),review_status='primary_text_corroborated',identity_status='wang_yi_son_wang_huzhi_xiuling',source_identities_all_adjudicated=True)
    results[7561]['unresolved']=[u for u in results[7561]['unresolved'] if u!='death_date_not_independently_corroborated']
    results[7561]['evidence'].extend(round2_evidence[8729])
    results[8729].update(duplicate_of=7561,candidate_duplicate_of=None,review_status='duplicate',identity_status='same_wang_huzhi_all_ten_source_keys_adjudicated',source_identities_all_adjudicated=True)
    results[8729]['unresolved']=[u for u in results[8729]['unresolved'] if not u.startswith('Do not delete whole row')]
    results[8729]['identity_adjudication']='修齡、王廙子、褚裒長史、南平/吳興太守識車胤沈勁、司州未行病卒；十來源與7561父籍職事相合。tj18是364條回述先前任官，不能當364仍活。'
    for ident,note in {
        7181:'曲/麴/麹昌均同北地太守、劉曜圍城、麴允誤信陷城而潰、昌突圍長安；沒有生卒日期。候選5869只有CBDB名，未據名字併入。',
        7186:'李回原為李潛府左長史、馬嚴士卒多為潛舊軍；李潛只被追述，不是當條仍活證據。',
        7187:'索綝擒李羌並不等於李羌當年被殺，死亡未載。',
        7189:'王如餘黨、楊難敵受賂陷張光與漢中流民歸成，兩來源同一漢中事件首領楊武；年月敘次不能推生卒。',
        7193:'沐堅字壁強、石沖留守幽州，傳稱尋病遂殞但不明列卒年；349任職可證，不能強作349卒。',
        7194:'晉書/春秋稱長安敗後尋為寇滅，通鑑318仍有焦嵩與陳安攻上邽；前瞻敘述不能當316確切卒年。',
        7195:'西平王叔與麴儒等起事，張寔斬儒等沒有單獨明記王叔死亡，不以等字包辦卒年。',
        7196:'全段為劉聰中常侍王沈，並非太原司空王沈；未記其生卒。7197有同宦官的一支但另混兩人，不能整條重定向。',
        7198:'王皮與劉惇為郭猗迫作偽證，兩來源職銜及事件吻合，未記生卒。',
        7199:'張寔遣督護/將軍王該送貢並援長安，同一使節將領；沒有生卒。',
        7200:'皇甫陽只明記被劉曜俘；後文魯充、梁緯自殺/被殺不能轉套到皇甫陽。',
        7202:'原注寫第五琦，可能第五猗異文；此段建興四年是與杜弢/荊州局勢追述的起點，竟為所害未獨立定年，先留未知不合併6134。',
        7203:'範/范氏為范隆女、劉聰貴人，以父名和冊納列舉確認省略姓氏，312冊納不是出生。',
        7204:'董霸與雷炤叛晉歸成，未載生卒。',
        7208:'南和令、廣川渤海千戶背石勒歸邵續，兩來源同一趙領。',
        7210:'前趙中宮僕射郭猗、誣劉乂同一事件；未記生卒。',
        7212:'原書作陵修，非程序生造；僅中黃門寵臣列名，生卒不載。',
        7214:'張寔贼曹佐高昌隗瑾諫獎言制度，三來源同字官籍與奏議，未記生卒。',
        7216:'雷炤為平夷太守叛降成；與雷照字形可能相通，但跨條合併尚未逐鍵做，未記生卒。',
        7217:'韋藉為上郡太守，劉曜來時奔南鄭；逃亡不是死亡，生卒未知。',
        7218:'馮睹率眾降石勒，馬嚴赴水死亡不能套至馮睹。',
        7219:'乙郍婁馮為慕容廆長史追謝吐谷渾，此段317條是吐谷渾早年回述，不能當317仍在任或始仕。',
        7220:'js00僅劉隗從子佣、娶鄭阿春三妹，js14是咸康初郎劉佣免官，未提供同籍或親系佐證；兩名同名者身份連結未全證。',
        7221:'劉勳/勲/勋同前趙騎兵將軍追郭默趙固，兩來源明銜明事相同，未記生卒。',
        7222:'劉浚同王廙長史、守揚口、朱伺守門敘事，未明記其本人死亡；朱伺創卒不能轉套。',
        7223:'劉頡邀擊趙固敗績，不是戰死記錄，生卒不載。',
    }.items():
        results[ident]['unresolved'].append(note)
    results[7220].update(source_identities_all_adjudicated=False,identity_status='same_name_two_jin_contexts_link_not_yet_proven')
    results[7181]['candidate_duplicate_of']=5869
    results[7202]['candidate_duplicate_of']=6134
    # Three women/men with the same name are never collapsed as one entity.
    for ident,status,components,note in [
        (7182,'merged_han_zhao_consord_and_wang_xiang_stepmother',[
            dict(keys=['sl:sl_00:183:朱氏'],identity='朱紀女、劉聰貴妃'),dict(keys=['tj:tj_08:95:朱氏','ssxy:ssxy_00:15:朱氏'],identity='王融繼室、王覽母、王祥後母')], '兩位朱氏夫子不同，需分支；任何一支日期不可套整條。'),
        (7184,'jin_officer_and_cheng_han_general_link_unproven',[
            dict(keys=['js:js_13:186:李桓'],identity='與趙誘朱軌同行、後與王廙相持之李桓，身份轉變待釐清'),dict(keys=['js:js_11:156:李桓','sl:sl_09:454:李桓'],identity='李勢蜀將，江陽敗於周撫曹據')], '317荊州李桓與345蜀將李桓未有中間經歷連結；不因同名合併為已證身份。'),
        (7197,'merged_taiyuan_minister_eunuch_and_gaoping_writer',[
            dict(keys=['sl:sl_00:284:王沉'],identity='同一chunk兼太原司空王沉婚姻追述與劉聰中常侍王沉',branches=[dict(identity='前趙宦官王沉',candidate_duplicate_of=7196),dict(identity='司空太原王沈',candidate_duplicate_of=10977)]),dict(keys=['js:js_16:212:王沉'],identity='高平寒素文人王沉字彥伯，著釋時論')], '同一sl key內也有兩同名者，不能只拆sourcekey就完成；另有高平彥伯，至少三身份。'),
    ]:
        results[ident].update(review_status='identity_conflict',identity_status=status,source_identities_all_adjudicated=False,component_identities=components,historical_birth=None,historical_death=None,duplicate_of=None)
        results[ident]['unresolved'].append(note)
    # Dated deaths: no birth estimate is substituted for an unknown birthday.
    for ident,year,month,ev_index in [(7185,339,None,1),(7190,339,9,3),(7191,339,None,5),(7192,339,9,6),(7205,339,9,1),(7206,339,9,0),(7207,339,9,0),(7209,339,9,0),(7211,339,9,1),(7213,339,None,2),(7215,339,9,0),(7224,340,3,1)]:
        ev=round2_evidence[ident][ev_index]
        results[ident].update(historical_death=date(year,ev),review_status='primary_text_corroborated')
        results[ident]['unresolved']=[u for u in results[ident]['unresolved'] if u!='death_date_not_independently_corroborated']
        if month:
            results[ident]['historical_death'].update(month=month,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7192].update(source_identities_all_adjudicated=False,identity_status='mao_bao_jin_general_verified_du_tao_captive_branch_not_fully_linked')
    results[7192]['unresolved'].append('339卒確指字碩真晉將毛寶；js09有杜弢將毛寶被陶侃俘，或可先被俘後仕晉，但未得原傳連結；整條保留hold，不能以合理故事代替身份實證。')
    results[7192]['date_scope']='dated Jin general component; earlier Du Tao captive branch not fully adjudicated'
    # Labour Yang has an actual annal-year disagreement, not merely spelling.
    laoyang_tj=round2_evidence[7166][0]
    laoyang_sl=round2_evidence[7225][0]
    disputed_lao=dict(year_alternatives=[339,340],month=12,precision='disputed_year_lunar_month',calendar='Chinese_lunisolar',source='資治通鑑339十二月 / 十六國春秋漢興三年340十二月',excerpt=laoyang_tj['excerpt'],alternatives=[dict(year=339,month=12,source='資治通鑑·咸康五年',excerpt=laoyang_tj['excerpt']),dict(year=340,month=12,source='十六國春秋·漢興三年',excerpt=laoyang_sl['excerpt'])])
    results[7166].update(historical_death=disputed_lao,review_status='primary_text_corroborated',identity_status='badong_defender_lao_yang_with_conflicting_death_annal_year',source_identities_all_adjudicated=True)
    results[7166]['evidence'].append(laoyang_sl)
    results[7166]['unresolved'].append('同一李奕攻巴東守將敗死事件，勞楊/勞揚字異，通鑑339十二月與春秋340十二月衝突，不作單年修改。')
    results[7225].update(duplicate_of=7166,candidate_duplicate_of=None,review_status='duplicate',identity_status='same_badong_defender_lao_yang_variant_spelling',source_identities_all_adjudicated=True)
    results[7225]['unresolved'].append('只合身份，不解年代爭議；canon7166列339/340兩說。')
    round3_reads={
        7226:[('js_15','黄门侍郎史淑在姑臧'),('sl_08','黄门郎史淑侍御史王冲'),('tj_13','黄门郎史淑、侍御史王冲'),('tj_14','愍帝使者史淑在姑臧'),('js_14','密遣黄门郎史淑')],
        7227:[('js_07','彭城康王释子钦为融嗣'),('tj_15','徙乐成王钦为河间王')],
        7228:[('js_01','烈王无忌字公寿'),('js_11','桓温及谯王无忌赞成其计'),('sl_09','谯王司马无忌'),('tj_16','无忌，承之子也'),('ssxy_03','无忌字公夀丞子也')],
        7229:[('js_15','别驾吴绍谏曰'),('sl_08','陇西太守呉绍'),('tj_13','陇西太守吴绍'),('tj_14','别驾吴绍谏曰'),('js_14','陇西太守吴绍')],
        7230:[('tj_12','谓其子勰曰'),('tj_13','周勰以其父遗言'),('js_07','勰字彦和'),('js_12','陈郡周勰为谢安主簿')],
        7231:[('js_07','临淮太守蔡豹斩铁以闻')],
        7232:[('hy_00','建宁太守孟彦'),('sl_09','晋守将孟彦刘齐李秋等皆死之'),('tj_16','守将孟彦、刘齐、李秋皆死')],
        7233:[('sl_01','乌丸审广渐裳郝袭')],
        7234:[('tj_16','闻之怒，诛约父子')],
        7235:[('js_15','蔑后南渡江，中兴初，为侍中')],
        7236:[('js_06','亢字季阳')],
        7237:[('js_08','诵使将张皮简精卒千人夜渡河'),('sl_00','潜遣耿稚张皮等简精卒千人')],
        7240:[('js_02','猗卢子右贤王日律孙')],
        7241:[('js_01','将军李恒、田嵩'),('tj_13','使将军李恒督左甄'),('tj_14','将军李恒帅甲卒二万'),('wsh_02','江夏太守李恒攻承'),('js_07','敦使庐江太守李恒'),('js_15','敦遣魏乂、李恒攻之'),('js_18','周抚、李恒、谢雍为爪牙')],
        7242:[('sl_09','晋守将孟彦刘齐李秋等皆死之'),('tj_16','守将孟彦、刘齐、李秋皆死')],
        7243:[('js_08','部将格增潜济入皮垒'),('sl_00','部将格增潜济入稚营垒')],
        7244:[('hy_00','建女婿杨虎保黄金山'),('tj_12','建婿杨虎收馀众击光'),('tj_13','杨虎掠汉中吏民以奔成')],
        7245:[('js_08','与匹䃅使荣邵期一时俱发'),('tj_13','匹䃅遣左长史荣邵')],
        7246:[('sz_00','后遇沙门法始')],
        7247:[('sl_01','乌丸审广渐裳郝袭')],
        7248:[('js_08','会王成从平阳逃来')],
        7249:[('js_12','棱字文子'),('js_03','洛阳令王棱为越所信'),('js_18','敦从弟豫章太守棱日夜切谏'),('js_19','敦从弟棱爱如骁武')],
        7250:[('sl_01','参军王续赍金宝遗末杯')],
        7251:[('sl_08','黄门郎史淑侍御史王冲'),('tj_13','黄门郎史淑、侍御史王冲'),('js_10','陈人王冲拥众荆州')],
        7252:[('js_08','纳好奕棋，王隐谓之曰'),('js_13','王隐，字处叔'),('js_15','吴皮、王隐之徒，无赖凶人')],
        7253:[('sl_08','灭冦将军田齐'),('js_14','灭寇将军田齐')],
        7254:[('tj_16','斌杀仪，又欲杀贺度')],
        7255:[('js_16','疏族范广愍而养之'),('sl_00','疎族范广')],
        7256:[('js_08','子石元为世子')],
        7257:[('js_08','从事中郎臣续澹')],
        7258:[('js_02','以𫖮兄玄孙序为𫖮后'),('js_12','从弟馗早亡，二息序')],
        7259:[('js_08','督护董昭与贼战')],
        7260:[('js_13','子勃，过江上《江表传》')],
        7261:[('tj_13','许朝督右甄'),('js_07','许朝督右甄')],
        7262:[('sl_00','侍中许肃冐难侍左右')],
        7263:[('sl_08','故太守贾骞'),('tj_13','安故太守贾骞'),('js_14','故太守贾骞')],
        7264:[('sl_01','晋故东莱太守南阳赵彭')],
        7265:[('js_07','赵胤领其父余兵'),('js_08','右卫将军赵胤讨平之'),('js_11','毛宝、赵胤、匡术'),('js_12','导将以赵胤为护军'),('js_18','建威将军赵胤'),('sl_01','晋冠军将军赵胤'),('tj_13','赵诱子胤'),('tj_14','建威将军赵胤'),('tj_15','以左卫将军赵胤为历阳太守'),('tj_16','委任诸将赵胤、贾宁'),('wsh_02','右卫将军赵胤'),('js_09','冠军将军赵胤'),('js_10','建威将军赵胤'),('js_19','赵胤复遣将军甘苗')],
        7266:[('js_08','前兼鸿胪边邈')],
        7267:[('sl_01','邵续使兄子济攻勒渤海')],
        7268:[('sl_01','乌丸审广渐裳郝袭')],
        7269:[('sl_09','扶风邓芝等各率流民')],
        7270:[('js_13','建兴中，陈声率诸无赖'),('sg_17','陈声曹辅斗筲小吏')],
        7271:[('js_15','刘咸攻韩璞于冀城'),('sl_00','刘咸攻韩璞于冀城'),('sl_08','太府司马韩璞'),('tj_13','太府司马韩璞'),('tj_14','张茂使将军韩璞'),('tj_15','骏使韩璞、辛岩救之'),('js_14','太府司马韩璞')],
        7272:[('sl_01','遣参军髙少奉书推崇勒')],
        7273:[('tj_13','赵诱、朱轨及陵江将军黄峻'),('js_07','陵江将军黄峻等讨曾')],
        7274:[('js_13','东莞太守侯史旄'),('sl_02','东莞太守侯史旄')],
        7275:[('js_08','挹弟启，启弟述'),('js_12','浩遣刘启、王彬之击襄'),('sl_01','获演弟启送于襄国'),('sl_02','兖州刺史刘启以鄄城归晋'),('sl_06','浩复遣龙骧将军刘启'),('tj_13','虎获演弟启以归'),('tj_17','赵兖州刺史刘启自鄄城来奔'),('tj_17','刘启、王彬之攻姚益')],
    }
    for ident,readings in round3_reads.items():
        result=results[ident]
        result['evidence'].extend(chunk_ev(chunk,phrase,'第3批原書逐條核讀','Actual kinship/office/event context read; unknown dates are unknown in these consulted passages, not an exhaustive literature claim') for chunk,phrase in readings)
        result.update(review_status='source_checked_dates_unknown',identity_status='original_named_or_kinship_abbreviated_subject_reviewed',source_identities_all_adjudicated=True,last_manual_review_batch='round3_7226_7275')
        result['source_keys_adjudicated_this_batch']=[k for k in result['keys'] if k.split(':')[1] in [x[0] for x in readings]]
    for ident in [7238,7239]:
        results[ident]['last_manual_review_batch']='round3_7226_7275'
        results[ident]['evidence'].append(dict(citation='CBDB BIOG_MAIN original row reread in round3',excerpt=json.dumps(originals[ident]['cbdb_originals'],ensure_ascii=False),verification='Original birth/death0, no month/day or activity dates; index313 is not birth/activity. Named book reference not yet inspected.'))
        results[ident]['unresolved'].append('CBDB原欄已重讀，但新唐書75B/魏書15所引譜系原文尚未實讀；不能把索引313當活動或生年。')
    for ident,chunk,phrase,year,month in [(7228,'js_01','进号前将军，永和六年薨',350,None),(7232,'tj_16','守将孟彦、刘齐、李秋皆死',340,3),(7234,'tj_16','闻之怒，诛约父子',340,None),(7242,'tj_16','守将孟彦、刘齐、李秋皆死',340,3),(7254,'tj_16','斌杀仪，又欲杀贺度',340,None),(7273,'tj_13','诱等皆败死',317,None)]:
        ev=chunk_ev(chunk,phrase,'第3批·原書所列年號/紀年核對','Death is explicitly of this subject; CE year follows the actual annal heading or the stated reign year')
        results[ident].update(historical_death=date(year,ev),review_status='primary_text_corroborated')
        results[ident]['evidence'].append(ev)
        results[ident]['unresolved']=[u for u in results[ident]['unresolved'] if u!='death_date_not_independently_corroborated']
        if month:results[ident]['historical_death'].update(month=month,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    summer_iron=chunk_ev('js_07','建兴初，铁已聚众数百人，临淮太守蔡豹斩铁以闻','晉書·周玘傳','Only Jianxing early period, not an explicit individual CE year; no birth date')
    results[7231].update(historical_death=dict(earliest_year=313,latest_year=316,precision='reign_period_not_exact_year',calendar='Chinese_reign_period',reign='建興初',source=summer_iron['citation'],excerpt=summer_iron['excerpt']),review_status='primary_text_corroborated')
    results[7231]['evidence'].append(summer_iron)
    for ident,note in {
        7226:'愍帝使史淑赴涼、317送詔、324張茂卒後仍為晉使授張駿，五支官事連續。324在世可證，生卒未知。',
        7227:'釋子欽嗣融為樂成王、330徙河間王，同宗爵繼承，未記生卒。',
        7228:'丞/承之子公壽、譙王、追王廙子復仇及桓溫伐蜀五來源相合；年小獲免不是精確年齡，不能據此補生日。',
        7229:'317陇西太守從韓璞救國、後張茂別駕諫靈鈞台，同一前涼吳紹升遷脈絡，未記生卒。',
        7233:'審廣、漸裳、郝襲三烏丸首領背王浚歸石勒，未記生卒。',
        7235:'庾袞子蔑南渡，元帝中興初侍中，父子省姓確認，未記生卒。',
        7236:'三張之季張亢字季陽、作歷贊，與兄張載張協分清；兄協的永嘉初終於家不能當張亢卒年。',
        7237:'李矩郭誦麾下張皮夜渡襲劉粲，同318年戰事；沒有生卒。',
        7240:'王浚使猗盧子右賢王日律孫攻段氏，同一戰事通鑑稱六修；候選六修異名待與跨記錄所有keys核對，未逕合7180利孫。',
        7241:'周訪左甄將李恆、王敦爪牙/魏乂共攻譙王承、庐江太守誣周札，七來源同晉荊湘將領脈絡；沒有直接生卒年月。',
        7243:'李矩格增夜渡解張皮/耿稚圍，同一人，不能把通鑑所寫格增以外的耿稚卒績套其生卒。',
        7244:'王建婿楊虎在厄水/黄金山與張光楊難敵作戰並掠漢中歸成；與7189楊武可能同事件異名，尚無法排除並存首領，不先合併。',
        7245:'段匹磾左長史榮邵奉表建康又被留，使節同一人，未記生卒。',
        7246:'法始為淨檢初得度之師；後續淨檢卒年、他比丘尼卒年不可套法始。',
        7247:'烏丸首領漸裳歸石勒，原書有此名，非程式誤分紀年詞；生卒不載。',
        7248:'王成從平陽逃來告南陽王保稱號，未記生卒，不與其他各朝王成按同名合併。',
        7250:'石勒參軍王續送賄段末杯間劉琨段匹磾，同段續提及邵續但二人不同，生卒未知。',
        7253:'滅寇將軍田齊與韓璞等317援長安，兩來源職事吻合，未記生卒。',
        7255:'范隆疏族范廣養育之，兩傳同親系；范隆死於聰/曜之世兩說非范廣卒年，不能轉套。范廣早年養隆可判其活動不晚於惠帝末306。',
        7256:'索綝子石元受立世子，省姓確認；不以封爵時年假推生卒。',
        7257:'劉琨從事中郎續澹奉還章綬，未記生卒；文中卒不上達是終究未能呈達，不是死亡。',
        7258:'荀崧從弟馗子序受臨淮公封，兩原傳同爵同親系；中興初年各數歲與遊戲294估生未必相容，不能把數歲硬換確切生年。',
        7259:'祖逖督護董昭擊樊雅，同名於曹魏司徒不足合併，未記生卒。',
        7260:'虞溥子勃南渡獻父江表傳，父卒62不能套到子，子未記生卒。',
        7261:'周訪右甄將許朝與李恆同戰杜曾，雙原文同一人，未記生卒。',
        7262:'許肅侍愍帝臨崩後請殮再請就死，原文明確聰義而免之；不能當317已被殺。',
        7263:'安故太守賈騫/賈騰異文，張寔韓璞救長安三原文同官事；不明生卒。',
        7264:'晉東萊太守南陽趙彭拒事石勒，勒禮送歸家；表忠不等於當時遇害，生卒未知。',
        7265:'趙誘子胤承父兵戰杜曾，後晉禁軍將、討王敦蘇峻祖約郭默、王導護軍；14來源同一軍旅脈絡，未記生卒。',
        7266:'段匹磾欲遣前兼鴻臚邊邈詣保，欲遣未必真的出使；沒有生卒。',
        7267:'邵續兄子濟攻石勒渤海，原書省姓；與邵續另兄子竺、存不可只因同親屬身分混合，生卒不明。',
        7268:'審廣漸裳同列之郝襲，未記生卒。',
        7269:'扶風流民帥鄧芝歸成，不能與蜀漢義陽鄧芝同名合併；原文僅此事，生卒未知。',
        7271:'前涼太府司馬韓璞317出援、322–323攻冀城、327救枹罕戰敗返受赦七來源相合；斬首二萬指軍眾，不是韓璞被斬，生卒未知。',
        7272:'原文髙少，趙固參軍奉書石勒；字形高/髙已核，沒有生卒。',
        7274:'東莞太守侯史旄坞被徐龕攻破，沒有明確其人被殺，不把據坞之年當卒年。',
    }.items():results[ident]['unresolved'].append(note)
    results[7240]['candidate_alias_of']='拓跋六修；record mapping not yet adjudicated'
    results[7244]['candidate_duplicate_of']=7189
    results[7255]['appearance_upper_bound']=dict(year=306,relation='at_or_before',basis='范隆四歲由范廣養育，之後范隆惠帝時已學成隱跡；取惠帝在位末306為保守活動上限，不作范廣生年')
    for ident,identity,components,note in [
        (7230,'merged_wu_xing_zhou_qi_son_and_chenjun_xie_an_clerk',[
            dict(keys=results[7230]['keys'][:3],identity='義興周玘子周勰字彥和'),dict(keys=[results[7230]['keys'][3]],identity='陳郡周勰，謝安主簿、韓伯中正不通之')], '父籍、活動圈與世代分支不同，先拆清再去重。'),
        (7249,'luoyang_magistrate_and_wang_dun_cousin_link_unproven',[
            dict(keys=['js:js_03:360:王稜'],identity='東海王越所信洛陽令王棱'),dict(keys=[k for k in results[7249]['keys'] if 'js_03' not in k],identity='王琛子、王導王敦從弟、豫章太守字文子')], '後三來源同族同任豫章且為王敦誘王如刺死，但洛陽令一支尚無父籍或轉官明證。'),
        (7251,'imperial_censor_and_jingzhou_band_leader_link_unproven',[
            dict(keys=results[7251]['keys'][:2],identity='愍帝侍御史王沖、與史淑持詔涼州'),dict(keys=[results[7251]['keys'][2]],identity='陳人王沖率眾荊州欲奉應詹刺史')], '兩個同名者尚無籍貫親系或轉折連結，不使史淑同事日期套至荊州首領。'),
        (7252,'historian_wang_yin_and_guanzhong_leader_not_linked',[
            dict(keys=results[7252]['keys'][:2],identity='史家王隱字處叔，論修晉史'),dict(keys=[results[7252]['keys'][2]],identity='麴允關中部將/豪右王隱，與吳皮並列')], '兩支不同角色仍待核，不把史家本傳資料套整條。'),
        (7270,'wu_court_favorite_and_eastern_jin_rebel_not_linked',[
            dict(keys=['sg:sg_17:100:陳聲'],identity='孫吳受寵小吏陳聲，孫休朝諫疏所譏'),dict(keys=['js:js_13:177:陳聲'],identity='晉建興中江上無賴首領陳聲，妻弟閻晉斬之降朱伺')], '跨朝同名缺身分連結，未把朱伺傳陳聲之死套孫吳寵臣。'),
    ]:
        results[ident].update(review_status='identity_conflict',identity_status=identity,source_identities_all_adjudicated=False,component_identities=components,historical_birth=None,historical_death=None,duplicate_of=None)
        results[ident]['unresolved'].append(note)
    liuqi_ev=chunk_ev('js_08','永和九年，随中军将军殷浩北伐，为姚襄所败，启战没','晉書·劉琨傳附劉輿諸子','Family original explicitly identifies Liu Yu son/Liu Yan brother Qi, later Eastern Jin general, death353')
    results[7275].update(review_status='primary_text_corroborated',source_identities_all_adjudicated=False,identity_status='liu_yu_son_qi_and_yanzhou_governor_branch_link_unproven')
    results[7275]['evidence'].append(liuqi_ev)
    results[7275]['historical_death']=None
    results[7275]['component_identities']=[dict(identity='劉輿子、劉演弟、殷浩將劉啟',keys=[k for k in results[7275]['keys'] if 'sl_02' not in k],historical_death=date(353,liuqi_ev)),dict(identity='石祗兗州刺史劉啟，351自鄄城歸晉',keys=['sl:sl_02:234:劉啟','tj:tj_17:205:劉啟'],identity_link='同一tj17 key內兼此人及353殷浩將；可能同一人但尚未以父籍/職轉原文鎖定')]
    results[7275]['unresolved'].append('劉演弟被俘與後為殷浩將，在晉書家傳直接相連；351兗州刺史歸晉與353之將或同人但尚缺職轉連結，353卒只列branch，整條hold。')
    # The actual books cited by the two CBDB records were then obtained/read.
    tuoba_table=dict(citation='新唐書卷75下·元氏宰相世系',url='https://ancient-china-books.github.io/xintangshu/OEBPS/Text/4205.html',reviewed_on='2026-11-08',excerpt='什翼犍七子：一曰寔君，二曰翰，三曰閼婆，四曰壽鳩，五曰紇根，六曰力真，七曰窟咄。',source_context='The subsequent paragraph identifies Shiyijian sixth son Lizhen and his sons Yilie/Yijin. The table supplies kinship only, no birth/death dates for either son.',verification='Primary historical genealogy transcription read, lines172–182; CBDB cited XTS75B confirmed rather than treating index313 as a life date')
    shi_age=evidence(SOURCE/'_cache/texts/魏书.txt','帝即位于繁畤之北，时年十九，称建国元年','魏書·序紀·昭成帝即位','建國元年與李壽殺期稱漢同年338；十九虛歲反推父生320，非兩子生日')
    for ident,phrase in [(7238,'辽西公意烈，昭成子力真之子也'),(7239,'常山王遵，昭成子寿鸠之子也')]:
        ev=evidence(SOURCE/'_cache/texts/魏书.txt',phrase,'魏書卷15·昭成子孫傳','Original identifies Shiyijian son, parent of the named grandson; no birth/death for the father in this passage')
        results[ident]['evidence'].extend([ev,tuoba_table,shi_age])
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='shiyijian_son_original_genealogy_and_cbdb_reference_verified',source_identities_all_adjudicated=True,historical_birth=None,historical_death=None)
        results[ident]['unresolved']=[u for u in results[ident]['unresolved'] if '新唐書75B/魏書15所引譜系原文尚未實讀' not in u]
        results[ident]['unresolved'].append('CBDB所引原書補讀完成，明確為什翼犍子；遊戲297生/313登場早於父320推生，不可當史實，生日仍未知，估算修正交主合併器。')
        results[ident]['family_chronology_constraints']=[dict(relation='son_of',parent='拓跋什翼犍',parent_birth_year=320,parent_birth_precision='inferred_from_nominal_age19_in338',source='魏書序紀及卷15/新唐書75下',constraint='child birth cannot precede the father birth; current estimated297/appearance313 impossible',action='retain historical birth unknown; recalibrate game estimates separately, do not invent exact child birthday')]
    round4_reads={
        7276:[('sl_00','贝丘王翼光自厘城'),('tj_13','汉贝丘王翼光觇知之')],
        7277:[('js_08','述为季龙侍中，随启归国'),('sl_00','河间王述发氐羌之众'),('sl_01','河间王刘述发氐羌之众'),('tj_15','赵主曜遣河间王述'),('sl_02','述为虎侍中随启归晋')],
        7278:[('sl_01','因遣卜泰及卜玄')],
        7279:[('sl_00','侍中乔豫和苞上疏'),('sl_08','宋毅及和苞张阆'),('tj_14','侍中乔豫、和苞上疏'),('js_14','宋毅及和苞、张阆')],
        7281:[('sl_00','车骑将军乔泰王腾'),('sl_01','车骑将军乔泰马忠王腾'),('tj_13','车骑将军乔泰、王腾')],
        7282:[('sl_02','会稽朱左车鲁国孔纂'),('tj_13','鲁国孔纂、泰山胡母翼')],
        7283:[('tj_13','琅邪国右常侍会稽孙霄'),('js_09','琅邪国右常侍会稽孙霄')],
        7284:[('js_15','扬烈将军宋辑'),('sl_00','辛岩宋辑辛晏'),('sl_08','宋毅及和苞张阆宋辑'),('tj_15','扬烈将军宋辑'),('js_14','张阆、宋辑、辛韬')],
        7285:[('tj_13','朱提审炤、建宁爨畺')],
        7286:[('js_08','与散骑郎张嶷')],
        7287:[('sl_08','破羗都尉张诜'),('js_14','破羌都尉张诜')],
        7288:[('tj_13','破羌都尉张诰')],
        7289:[('sl_08','辛韬张选董广'),('js_14','辛韬、张选、董广')],
        7290:[('js_13','龛将于药斩抚'),('sl_02','龛部将于药斩抚'),('tj_13','徐龛部将于药追斩之')],
        7291:[('sl_02','会稽朱左车鲁国孔纂'),('tj_13','会稽朱左车、鲁国孔纂')],
        7292:[('js_13','时朱轨、赵诱、李桓'),('sl_02','建武十二年夏五月尚书朱轨'),('tj_16','而竟诛朱轨'),('js_07','襄阳太守朱轨'),('js_18','遣部将朱轨、赵诱伐杜曾'),('js_19','而朱轨息昌、赵诱息胤')],
        7293:[('sl_09','右仆射李嶷数以直言'),('tj_16','右仆射李嶷皆坐直谏死'),('js_04','弟嶷亡，表去官')],
        7294:[('sl_09','扩从兄演自越嶲上书')],
        7295:[('js_08','流人坞主张平、樊雅'),('js_13','樊雅自号谯郡太守'),('tj_13','流民张平、樊雅各聚众')],
        7296:[('js_08','其中弟叔军好学有智谋'),('sl_01','弟叔军好学有智谋'),('tj_13','其弟叔军谓匹䃅曰')],
        7297:[('js_13','遣参军殷乂诣平、雅'),('sl_05','遣叅军殷乂诣平')],
        7298:[('sl_09','与建宁爨亮偕来归附')],
        7300:[('js_11','尚书郎卢𬘭将入直'),('js_03','唯志与子谧、兄子𬘭')],
        7301:[('sl_00','潜遣耿稚张皮'),('tj_13','其将耿稚等夜济河')],
        7302:[('sl_02','泰山胡母翼自蓟逃奔昌黎'),('tj_13','泰山胡母翼自蓟逃奔昌黎')],
        7303:[('jk_01','时莘固为临海太守')],
        7304:[('sl_02','其子叶延立保于白兰山'),('tj_15','使辅其子叶延'),('tj_17','吐谷浑叶延卒'),('js_18','叶延年十岁，其父为羌酋姜聪所害'),('sl_03','叶延年十岁常缚草为人'),('wsh_04','叶延死，子碎奚立'),('yi_00','嗣立二十三年，年三十三'),('yi_02','其后吐谷浑孙叶延')],
        7305:[('sl_08','辛韬张选董广'),('js_14','辛韬、张选、董广')],
        7306:[('js_14','遣牙门蔡忠奉表江南'),('sl_08','遣牙门蔡忠奉表江南'),('tj_13','遣牙门蔡忠奉表诣建康')],
        7307:[('hy_00','任调与司马蔡兴'),('sl_09','尚书左仆射蔡兴切諌'),('tj_16','左仆射蔡兴、右仆射李嶷'),('wsh_02','尚书左仆射蔡兴直言切谏')],
        7308:[('js_12','其后著作郎虞预'),('js_13','著作郎虞预私撰'),('js_14','虞预，字叔宁'),('js_10','虞预称美之'),('js_15','聘喜弟预女为妻')],
        7309:[('js_08','将军卫策邀击于谷水'),('sl_01','冯铁卫策等邀击于汴水')],
        7310:[('tj_13','汉嘉夷王冲归'),('hy_00','汉嘉夷王冲归遣子入质')],
        7311:[('sl_08','辛韬张选董广'),('js_14','辛韬、张选、董广'),('sg_04','攸姑子辛韬曽问攸')],
        7312:[('js_08','兄子武邑内史存'),('js_13','且邵存已据贼垒'),('sl_01','与续子缉兄子存竺等婴城固守'),('tj_14','与续子缉、兄子存、竺等婴城固守')],
        7313:[('js_17','太史令陈卓奏用二十二日')],
        7314:[('sl_02','遣谒者陶辽重申前命')],
        7315:[('sl_09','晋平夷太守朱提雷照')],
        7316:[('sl_01','郝袭靳市等于襄国')],
        7317:[('js_18','靳康女者，不知何许人也'),('sl_00','遇靳康劫之以归'),('sl_01','卫将军靳康等起兵杀凖'),('tj_13','遇靳康，劫延以归')],
        7318:[('js_08','太常韩胤等奉迎梓宫'),('sl_00','太常韩胤等奉迎紫宫'),('tj_13','太常韩胤等奉迎梓宫')],
        7319:[('ssxy_01','顾显字孟着吴郡人')],
        7320:[('sl_00','及将军马忠等杀准'),('sl_01','乔泰马忠王腾')],
        7321:[('js_08','头亲党冯宠率其属四百'),('tj_13','头党冯宠帅其众降逖')],
        7323:[('js_08','遣将魏硕掠豫州诸郡')],
        7324:[('js_08','琨将龙季猛迫于乏食')],
        7325:[('sl_01','任播崔浚为史学祭酒')],
    }
    for ident,readings in round4_reads.items():
        result=results[ident]
        result['evidence'].extend(chunk_ev(chunk,phrase,'第4批·同卷同事件成組原文核讀','Actual passages read with common annual context; every source key still receives its own named/kinship/office adjudication') for chunk,phrase in readings)
        result['last_manual_review_batch']='round4_7276_7325_except_completed7322'
        result['source_keys_adjudicated_this_batch']=[k for k in result['keys'] if k.split(':')[1] in [x[0] for x in readings]]
        result['source_identities_all_adjudicated']=True
        if result['review_status']=='needs_further_research':result.update(review_status='source_checked_dates_unknown',identity_status='original_event_and_office_identity_reviewed')
    for ident in [7280,7299]:
        results[ident]['last_manual_review_batch']='round4_7276_7325_except_completed7322'
        results[ident]['evidence'].append(dict(citation='CBDB original row reread',excerpt=json.dumps(originals[ident]['cbdb_originals'],ensure_ascii=False),verification='7280 estimated deathcirca379;7299 XTS73A.20a reference not yet directly obtained; index is not a life date'))
    for ident,note in {
        7276:'貝丘王翼光守懷城及報耿稚夜襲，姓劉由漢宗室王銜及同敘事鎖定；未記生卒。',
        7278:'卜玄與卜泰奉六璽降劉曜，兩人須分清；使者未記被殺，生卒不載。',
        7281:'靳準左右喬泰共弒準，同官同事件三來源，非卜泰；靳氏族誅不可轉套喬泰。',
        7282:'魯國孔纂與朱左車胡母翼自薊奔慕容廆，同籍同行兩來源；未記生卒。',
        7283:'元帝琅邪國右常侍會稽孫霄諫幼王厚葬，兩來源同職奏；幼王焕卒年不可套給孫霄。',
        7284:'宋輯318列窦涛援上邽之軍，327為揚烈將軍會韓璞伐秦州，五來源同一前涼將；敗軍斬二萬不等於本人死。',
        7285:'朱提審炤歸成一條，或雷炤/雷照傳寫異名，但需要校本方能定，不能只憑相近字或同地直接合7216/7315。',
        7286:'散騎郎張嶷與戴嵩謗戴邈受除名，不是蜀漢武將張嶷；未記生卒。',
        7288:'破羌都尉張誥與張詵為同官、同張寔勸奉晉王奏議的名字異文，身份可合但生卒不詳。',
        7289:'張選與董廣辛韜等前涼援南陽王保，兩書名列同軍；未記生卒。',
        7290:'徐龕將于藥斬周撫，三來源同人同事；周撫的卒年不能作于藥卒年。',
        7291:'會稽朱左車與孔纂胡母翼自薊奔慕容廆，原籍姓名與同伴相合，未記生卒。',
        7295:'譙城樊雅受桓宣勸降，張平被殺不是樊雅被殺；未記生卒。',
        7296:'段匹磾弟叔軍建議殺劉琨，原書省姓但親系明；劉琨遇害不代表叔軍當年死。',
        7297:'兩支是同一祖逖遣使、諷馬廄破大鑊被張平斬的殷乂；春秋前秦錄張平傳接張蚝，把早年祖逖坞主張平故事混入，不能把這段當357殷乂活動。本人卒年未明列。',
        7298:'成漢受歸附的建寧爨亮，與爨畺/爨量可能是名字傳寫異文，未有全源裁定不逕合。未記生卒。',
        7300:'盧志兄子𬘭與入直受刁協辱的尚書郎盧綝，綝/𬘭簡繁異體已核；未記生卒。304已隨盧志護乘輿，不能用後世出場估算代史實。',
        7301:'郭誦李矩麾下耿稚夜渡襲劉粲，與張皮是並列不同將；突圍軍眾傷亡不能套其卒年。',
        7302:'泰山胡母翼、朱左車孔纂同行避難，兩來源同人；未記生卒。',
        7303:'建康實錄注實作莘固，臨海太守與羊曼待客對比；沒有生卒，不能因罕見姓字斷為虛構。',
        7304:'八原書同吐谷渾孫/吐延子葉延，射草人復仇、曾祖弈洛韓、公孫之子稱姓等鎖定；碎奚/碎妥/辟奚為其子名異文，非另葉延。',
        7305:'前涼董廣與張選辛韜同軍，兩原書列名一致，未記生卒。',
        7306:'張寔牙門蔡忠奉表江南，同一使者與奏事三來源；未記生卒。',
        7307:'四書同司馬蔡興勸李壽稱帝、後左僕射直諫被殺，身份全核，通鑑341卒採明列紀年；未推生年。',
        7308:'虞預字叔寧、初名茂、虞喜弟、孫晷妻父、王導時著作郎，五原文身份一致；CBDB僅姓名欄且日期注官全空，該額外支尚不提供獨立身份。本人傳只稱年老歸卒，沒有明確年日。',
        7309:'祖逖將衛策谷水擊魏碩、汴水截劉夜堂糧，兩來源同軍脈絡；未記生卒。',
        7310:'漢嘉夷王衝歸遣子入質，兩來源一致；衝歸是本人名而不是“王沖，歸”字誤斷，華陽明文可覆核。',
        7312:'邵續兄子存武邑內史與段文鴦出兵，後已據徐龕贼垒；四原書同世同軍，不把邵續其他兄子竺濟併入。未記生卒。',
        7313:'元帝登阼擇日的太史令陳卓，未以其奏日作生卒；與三國太史同名可能而尚未全源核併。',
        7314:'晉謁者陶遼318重申封慕容廆，未記生卒。',
        7315:'晉平夷太守朱提雷照殺孟桓降成，或7216雷炤同人但年代敘次不同；尚保留候選不整條刪。',
        7316:'烏丸靳市與審廣等被徙襄國，未記生卒，與被誅靳準家族不能僅同姓合。',
        7317:'衛將軍靳康劫王延、共殺靳準、後其女向劉曜稱已滅父母兄弟，四源關係一致。',
        7318:'晉太常韓胤318奉迎二帝梓宮，使節未到而靳準已敗，不代表韓胤遇害；三來源同事，生卒未知。',
        7319:'顧顯字孟著、吳郡人、顧榮兄子，太興中騎郎早卒；沒有單獨確切卒年，不把太興中任官硬當當年卒。',
        7320:'平陽將馬忠同喬泰弒靳準，原書二支一致，不與三國吳蜀的馬忠按名字混合；靳氏族誅不等於此將被殺。',
        7321:'李頭黨馮寵率眾歸祖逖，兩書同軍同事，李頭被殺不套馮寵。',
        7323:'陳川將魏碩掠豫州為衛策破，未說魏碩被殺；生卒不載。',
        7324:'劉琨將龍季猛殺楊橋如綏降段匹磾，沒有其本人卒年，不把受害者死亡套給殺者。',
        7325:'後趙史學祭酒任播與崔浚同列，與前趙任播可能重複但尚未核全對方來源，生卒未記。',
    }.items():results[ident]['unresolved'].append(note)
    results[7287].update(duplicate_of=7288,candidate_duplicate_of=None,review_status='duplicate',identity_status='same_po_qiang_commandant_zhang_shen_gao_variant_in_same_memorial',source_identities_all_adjudicated=True)
    results[7288]['evidence'].extend(results[7287]['evidence'][-2:])
    results[7315]['candidate_duplicate_of']=7216
    results[7285]['candidate_duplicate_of']=7315
    results[7300]['appearance_upper_bound']=dict(year=304,relation='at_or_before',basis='晉書盧志傳：惠帝蕩陰敗後王浚攻鄴，兄子綝已隨盧志護帝歸洛；此事件304，非首仕年')
    results[7308]['source_identities_all_adjudicated']=False
    # Actual same-name collisions override earlier single-component dates.
    for ident,identity,components,note in [
        (7277,'liu_yu_son_shu_and_former_zhao_hejian_prince_link_unproven',[
            dict(keys=['js:js_08:326:劉述','sl:sl_02:381:劉述'],identity='劉輿子、劉啟弟、歸晉驍騎將軍劉述'),dict(keys=['sl:sl_00:445:劉述','sl:sl_01:412:劉述','tj:tj_15:103:劉述'],identity='328劉曜遣守秦州河間王述')], '皇族王爵支與劉輿士族子支缺親系/職轉明證，不以同名直接合。'),
        (7279,'liang_officer_and_liu_yao_attendant_he_bao_unlinked',[
            dict(keys=['sl:sl_00:388:和苞','tj:tj_14:61:和苞'],identity='劉曜侍中和苞，與喬豫諫宮殿陵墓'),dict(keys=['sl:sl_08:117:和苞','js:js_14:224:和苞'],identity='張寔318援司馬保將和苞')], '相近年代可有政權轉仕，但未得原文橋接，暫hold。'),
        (7292,'merged_jin_xiangyang_general_and_later_zhao_minister_zhu_gui',[
            dict(keys=['js:js_13:184:朱軌','js:js_07:166:朱軌','js:js_18:197:朱軌','js:js_19:101:朱軌'],identity='襄陽太守朱軌，317為杜曾所殺、子昌食仇肉',historical_death=dict(year=317,precision='year',calendar='historical_CE_year',source='晉書王敦傳建武初、周訪/杜曾傳；通鑑317女觀湖')),dict(keys=['sl:sl_02:95:朱軌','tj:tj_16:129:朱軌'],identity='後趙尚書朱軌，346因道路不修被石虎誅',historical_death=results[7292]['historical_death']),dict(keys=['cbdb:405691'],identity='只具姓名，無法判哪位朱軌')], '撤回頂層346；317卒晉將不可能346再被石虎殺，同名兩人及空CBDB需拆分。'),
        (7293,'merged_li_zhong_brother_and_cheng_han_right_deputy',[
            dict(keys=['js:js_04:50:李嶷'],identity='晉李重弟嶷，李重300卒前弟已亡'),dict(keys=['sl:sl_09:445:李嶷','tj:tj_16:165:李嶷'],identity='成漢李壽右僕射李嶷，341直諫被殺',historical_death=dict(year=341,precision='year',calendar='historical_CE_year',source='通鑑咸康七年；春秋漢興四年'))], '跨支至少相隔四十年卒敘，不可整條套341。'),
        (7311,'merged_xun_you_cousin_and_former_liang_general_xin_tao',[
            dict(keys=['sg:sg_04:153:辛韜'],identity='荀攸姑子辛韜，魏書注所述問曹操取冀州事'),dict(keys=['sl:sl_08:118:辛韜','js:js_14:226:辛韜'],identity='318前涼援司馬保將辛韜')], '荀攸214已卒，其姑子與318涼將缺世代連結，不能把前者移入西晉末一代。'),
    ]:
        results[ident].update(review_status='identity_conflict',identity_status=identity,component_identities=components,source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,duplicate_of=None)
        results[ident]['unresolved'].append(note)
    ye_age=chunk_ev('js_18','在位二十三年卒，年三十三','晉書·吐谷渾傳·葉延','Death351 by explicit annals plus nominal age33 implies319; age10 at father death329 implies320, a genuine one-year inference disagreement')
    results[7304]['evidence'].append(ye_age)
    results[7304]['historical_birth']=dict(year_alternatives=[319,320],precision='disputed_inference_from_nominal_age',calendar='historical_CE_year',source='晉書/宋書年33與父卒時年10，配通鑑329/351紀年',calculation=['351-33+1=319','329-10+1=320'],excerpt=ye_age['excerpt'],caveat='No exact birthday; preserve age/regnal counting disagreement rather than pick one estimate')
    results[7304]['unresolved'].append('同本傳父329卒時葉延年10及351卒年33反推生320/319相差一年；在位23可能包首尾，不能消去原年齡矛盾。')
    jin_death=chunk_ev('tj_13','收靳氏男女，无少长皆斩之','資治通鑑·318年末','Joined with Jin Kang daughter original expressly saying father/mother/brothers killed; not applied to unrelated QiaoTai/MaZhong')
    results[7317].update(historical_death=date(318,jin_death),review_status='primary_text_corroborated')
    results[7317]['evidence'].append(jin_death)
    round5_reads={
        7326:[('js_13','武威将军侯礼'),('sl_02','武威将军侯礼')],
        7327:[('sl_01','大中大夫傅彪贾满江轨')],
        7328:[('tj_13','刘贵妃为左皇后')],
        7329:[('sl_00','周庸为安定太守'),('tj_13','张𫖮、周庸据阴密')],
        7330:[('sl_02','高句骊将如奴子据于河城'),('tj_13','高句丽将如奴子据于河城')],
        7332:[('sl_01','石泰石同石谦孔隆撰大单于志')],
        7333:[('sl_02','宋诙劝廆献捷江东')],
        7334:[('js_08','三秦人尹桓、解武')],
        7335:[('sl_01','任播崔浚为史学祭酒')],
        7336:[('sl_01','燕成周内史崔焘'),('sl_02','徙崔焘高瞻韩恒石琮'),('tj_13','以崔焘、高瞻、韩恒、石琮'),('tj_14','崔焘为主簿'),('tj_16','燕成周内史崔焘')],
        7337:[('js_11','怿字叔预'),('sl_02','亮弟怿时为辅国将军'),('tj_15','亮与弟怿、条、翼'),('tj_16','豫州刺史庾怿以酒饷'),('js_09','颍川庾怿为功曹')],
        7338:[('sl_01','续咸及庾景为律学祭酒')],
        7339:[('tj_13','凉州军士张冰得玺'),('js_14','上军士张冰得玺'),('sl_08','上言军士张氷')],
        7340:[('sl_01','左司马张屈六'),('tj_13','司马张屈六、程遐')],
        7341:[('js_08','功曹张景'),('js_13','王伏都、张景'),('sl_02','张景为之后继'),('js_04','华兄子景'),('js_10','逐会稽相张景')],
        7342:[('sl_01','以张离张良刘群刘谟'),('sl_02','右仆射张离领五兵尚书'),('tj_16','虎遣尚书张离帅骑'),('tj_17','张离亦帅腾二千')],
        7343:[('sl_00','张𫖮为新平太守'),('tj_13','张𫖮、周庸据阴密')],
        7344:[('sl_00','昔乐工成凭今居何职')],
        7345:[('sl_01','记室佐明楷程机撰上党国记')],
        7346:[('sl_01','前将军李寒领司兵勲教国子')],
        7347:[('sl_01','参军傅畅杜嘏并领经学祭酒'),('sl_02','光禄勲杜嘏')],
        7348:[('sl_00','并州祭酒桓回于途遇一老父')],
        7349:[('sl_00','杨曼为雍州刺史'),('tj_13','其将杨曼、王连据陈仓'),('tj_14','王连战死，杨曼奔南氐')],
        7351:[('sl_01','傅彪贾满江轨撰大将军起居注')],
        7352:[('sl_02','督护新兴王寓击走之'),('sl_03','长史王寓等勒众万五千'),('tj_15','督护新兴王寓击走之'),('tj_16','长史王寓等将兵万五千'),('hhs_02','司隶校尉王寓，出于宦官')],
        7353:[('sl_01','参军石泰石同石谦孔隆')],
        7354:[('sl_01','参军石泰石同石谦孔隆')],
        7355:[('sl_01','参军石泰石同石谦孔隆')],
        7356:[('sl_01','记室佐明楷程机撰上党国记')],
        7357:[('js_13','征虏将军羊鉴'),('js_18','敦谓羊鉴及子应'),('sl_02','太子左卫率羊鉴'),('tj_13','太子左卫率太山羊鉴'),('tj_14','尚书令刁协劾奏鉴，免死除名'),('wsh_02','语其舅羊鉴及子应'),('js_09','圣恩降天地之施，全其首领')],
        7358:[('js_15','敦煌计吏耿访到长安'),('sl_08','炖煌计吏耿访到长安'),('tj_15','敦煌计吏耿访自汉中入江东')],
        7361:[('js_08','三秦人尹桓、解武')],
        7362:[('sl_01','法曹令史贯志造辛亥制度'),('tj_13','法曹令史贯志')],
        7363:[('sl_01','傅彪贾满江轨撰大将军起居注')],
        7364:[('js_13','治书御史郝嘏为行台'),('tj_14','护军长史郝嘏'),('js_10','护军长史郝嘏'),('ssxy_01','𫖮长史郝嘏及左右文武')],
        7365:[('sl_00','侍中司徒郭汜等谏曰'),('sl_01','兼司徒郭汜等持节'),('tj_13','兼司徒郭汜授勒'),('tj_14','中山王雅、郭汜、朱纪')],
        7366:[('sl_00','曜遂回灭郭迈')],
        7367:[('sl_03','中尉郑林为军谘祭酒'),('tj_13','北海郑林客于东莱'),('tj_14','黄泓、郑林参军事'),('tj_16','中尉郑林为军谘祭洒')],
        7368:[('sl_03','左长史韩寿斩其将阿佛和度加'),('tj_16','韩寿斩高句丽将阿佛和度加')],
        7369:[('yl_03','长史陈协以为'),('yl_00','陈协数日辄进阮步兵酒一壶')],
        7370:[('sl_00','小曰月华皆国色也'),('tj_13','月华为右皇后')],
        7371:[('sl_02','乐浪太守鞠彭'),('sl_03','鞠殷乐浪太守彭之子也'),('tj_13','鞠羡既死，苟晞复以羡子彭'),('tj_16','乐浪太守鞠彭'),('tj_17','彭时为燕大长秋')],
        7372:[('sl_02','徙崔焘高瞻韩恒石琮'),('sl_03','韩恒为中书令'),('tj_13','恒，安平人'),('tj_15','参军韩恒驳曰')],
        7373:[('js_08','逖遣将韩潜等镇东台'),('sl_01','祖逖将韩潜与勒将桃豹'),('tj_14','祖逖将韩潜与后赵将桃豹')],
        7374:[('jk_01','守太常马龟等修复山陵')],
        7375:[('sl_02','郎中令高诩为玄莵太守'),('sl_03','高诩刘佩皆中流矢卒'),('tj_13','处士辽东高诩'),('tj_15','郎中令高诩为玄菟太守'),('tj_16','内史高诩')],
    }
    for ident,readings in round5_reads.items():
        result=results[ident]
        result['evidence'].extend(chunk_ev(chunk,phrase,'第5批·同卷分人原文審讀','Read actual named office/kinship/event context; source presence is distinguished from an exact biographical date') for chunk,phrase in readings)
        result.update(review_status='source_checked_dates_unknown',identity_status='actual_original_passage_read_and_subject_adjudicated',source_identities_all_adjudicated=True,last_manual_review_batch='round5_7326_7375')
        result['source_keys_adjudicated_this_batch']=[k for k in result['keys'] if k.split(':')[1] in [x[0] for x in readings]]
    for ident in [7350,7359,7360]:
        results[ident]['last_manual_review_batch']='round5_7326_7375'
        # These database originals were read in the earlier full163-row field
        # audit; do not relabel that work as a new primary-source review.
        results[ident]['unresolved'].append('第5批保留先前CBDB原欄審核；所引原始家譜/墓誌尚無新實讀，不把本批連號涵蓋當作原文已查。')
    ranou=dict(citation='三國史記卷17·美川王二十年',url='https://zh.wikisource.org/w/index.php?title=三國史記/卷17&oldid=2150507',reviewed_on='2026-11-08',excerpt='我將如孥據于河城…張統，掩撃擒之',source_context='Full319 episode read: CuiBi comes to Goguryeo, his nephew Tao admits alliance scheme, MurongHui takes territory; the Goguryeo general at Hecheng is captured by ZhangTong. Same unique named event as ZizhiTongjian and ShiliuguoChunqiu call 如奴子.',verification='Cached sk00 has missing glyph after如; direct fixed-revision transcription restores 如孥, not accepted from extraction JSON')
    results[7330]['evidence'].append(ranou)
    results[7331].update(review_status='duplicate',identity_status='ranou_runuzi_same_hecheng_general_captured_by_zhang_tong',duplicate_of=7330,candidate_duplicate_of=None,source_identities_all_adjudicated=True,last_manual_review_batch='round5_7326_7375')
    results[7331]['evidence'].append(ranou)
    results[7330]['unresolved'].append('如孥/如奴子同河城守將，被張統俘；俘虜不是卒年，本人生卒未知。')
    # Most names in the319 appointments are merely attested, without life dates.
    for ident in [7327,7332,7335,7338,7345,7346,7351,7353,7354,7355,7356,7363]:
        results[ident]['unresolved'].append('實讀石勒319設官/修史整段並逐人定位：列經史律學祭酒或著書人員，不載此人生卒；319任職不等於其出生/初仕年。')
    for ident,note in {
        7326:'武威將軍侯禮與蔡豹羊鑑同討徐龕，兩來源官事相合；未記生卒。',
        7328:'劉聰原劉貴妃立左皇后，不與先前劉娥死後、劉英及賜懷帝的小劉貴人僅憑姓氏合；此條未記生卒。',
        7329:'司馬保安定太守周庸據陰密，兩來源同軍同城；城破未明記其人被殺，生卒未知。',
        7333:'宋詼/宋誙可能宋該傳寫異文，所讀為獻捷江東表及慕容廆僚議，不載生卒；尚未把其他宋該來源全核故不逕刪。',
        7334:'尹桓與解武盜發漢陵，未記生卒。',
        7336:'崔毖兄子崔燾先依慕容廆為主簿，後成周內史應石虎；五來源同燕官事，未載生卒。',
        7339:'張冰/氷為涼州軍士得玉璽，三來源同人；315年冬的發現年不是其生卒。',
        7340:'石勒左司馬張屈六與張敬張賓程遐勸進，雙來源同官同列，未載生卒。',
        7342:'石勒門生主書張離後石虎五兵尚書/右僕射，349迎石遵，四來源連續同後趙重臣；張豺死不能套張離，佛圖澄傳稱後被殺也未明年。',
        7343:'張顗/𫖮同司馬保新平太守，兩來源同城同將，未載生卒。',
        7344:'成憑是鬼神故事中被問及的昔樂工，不是那個已死近50年的麻子軒；後者的死亡時距不可套成憑。只有故事記載，無獨立生卒材料。',
        7347:'杜嘏319經學祭酒、348光祿勳任石世少傅，兩書同後趙學官脈絡；未記生卒。',
        7348:'并州祭酒桓回講遇故人鬼魂一事；成憑/桓回是敘事中活人，死近50年的是麻子軒。未據志怪補生卒。',
        7349:'司馬保雍州刺史楊曼320陳倉敗後奔南氐，未被說戰死；三原文同事，與東晉羊曼不可混。',
        7357:'太山羊鑑討徐龕失利免死、後王敦舅參與托後事，七來源同晉官；前文蔡豹被誅與王敦卒年不可套羊鑑。',
        7358:'敦煌計吏耿訪入江東、滯梁州、送張駿詔命三來源相合；送詔333/334不是生卒，未載本人死亡。',
        7361:'解武與尹桓盜漢陵，沒有其人被殺的記載。',
        7362:'法曹令史貫志作辛亥制，兩原文同職同書，名有原文不是程式誤分；未記生卒。',
        7364:'蔡豹行台治書御史、周顗護軍長史郝嘏，四書同晉官，周顗被殺不套郝嘏。',
        7365:'前趙兼司徒郭汜授石勒又諫救游子遠，四書同劉曜朝臣；不與漢末郭汜同名合，未記生卒。',
        7366:'春秋只稱劉曜回滅郭邁，滅字可指敗其軍，未獨立明言其人被殺，不硬定卒年。',
        7367:'北海鄭林隨鞠彭入燕、受參軍後為中尉軍諮祭酒，四原文同燕文臣脈絡；拒車帛不等於終身未仕，不藉此拆出第二人。',
        7370:'靳準小女月華右皇后、與姊月光分清；春秋右貴妃劉氏旁注一作月華，保留文異而不將劉姓妻與她合成一人；本人卒年未載。',
        7371:'鞠羡子彭為東萊太守避曹嶷、338樂浪、356大長秋致書子鞠殷，五原書同父子與官事；356是本人仍活動，不是殷一人獲官的旁引。未記生卒。',
        7372:'安平韓恆避亂入燕、諫慕容廆、後慕容俊中書令，四原書同一文臣脈絡；未記生卒。',
        7373:'祖逖將韓潛陳川故城東台、汴水截糧、封丘駐兵，三來源同人，未記生卒。',
        7374:'建康實錄319守太常馬龜與梁堪修山陵，與318韓胤迎梓宮不是同一批派使，不憑相似任務硬合；未載生卒。',
    }.items():results[ident]['unresolved'].append(note)
    results[7371]['attested_activity_years']=[319,338,356]
    results[7371]['last_attested_activity']=dict(year=356,basis='通鑑永和十二年，鞠殷受東萊太守、其父彭任燕大長秋並書戒遺車馬',source='tj17 + 十六國春秋鞠殷傳',meaning='Game death estimates before356 contradict this; historical death still unknown')
    # Explicit year and age statements support the following calibrations.
    yu_death=chunk_ev('tj_16','二月，怿饮鸩而卒','資治通鑑·342年二月','庾怿對王允之毒酒事，同本人傳年50卒')
    yu_age=chunk_ev('js_11','怿闻，遂饮鸩而卒，时年五十','晉書·庾亮傳附庾怿','Nominal50 at death342 yields293; not a documented birthday')
    results[7337].update(historical_birth=date(293,yu_age,age=50),historical_death=date(342,yu_death,month=2),review_status='primary_text_corroborated')
    results[7337]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7337]['evidence'].extend([yu_death,yu_age])
    for ident,chunk,phrase,year,month in [(7368,'tj_16','韩寿斩高句丽将阿佛和度加',342,11),(7375,'sl_03','高诩刘佩皆中流矢卒',344,None)]:
        ev=chunk_ev(chunk,phrase,'第5批原書明列紀年戰事','本人被斬/中矢卒，非僅軍隊敗績')
        results[ident].update(historical_death=date(year,ev),review_status='primary_text_corroborated')
        results[ident]['evidence'].append(ev)
        if month:results[ident]['historical_death'].update(month=month,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    for ident,identity,components,note in [
        (7341,'zhang_jing_source_branches_not_yet_linked',[
            dict(keys=['js:js_08:400:張景'],identity='李矩功曹張景'),dict(keys=['js:js_13:133:張景','sl:sl_02:343:張景'],identity='後趙石勒將張景助徐龕'),dict(keys=['js:js_04:153:張景'],identity='張華兄子景，華遇害後被徙漢中'),dict(keys=['js:js_10:82:張景'],identity='304石冰逐出的會稽相張景')], '四支缺籍貫父系/職轉連結，不能因同名整條套日期。'),
        (7352,'merged_han_eunuch_and_former_yan_wang_yu',[
            dict(keys=['hhs:hhs_02:49:王寓'],identity='東漢宦官出身司隸校尉王寓，陷張奐'),dict(keys=[k for k in results[7352]['keys'] if not k.startswith('hhs:')],identity='前燕新興督護/長史王寓334–342，北道軍皆敗沒')], '後漢與前燕相距約170年，必須拆；342敗沒未直接當全記錄卒年。'),
        (7369,'chen_xie_jinwen_recommendee_and_tao_kan_clerk_unlinked',[
            dict(keys=['yl:yl_00:13:陳協'],identity='阮籍薦司馬昭修九龍堰之陳協'),dict(keys=['yl:yl_03:2:陳協'],identity='陶侃長史陳協，解授鎧夢')], '兩逸書引文年代不同且無父系官遷連結，不以可假設長壽證同人。'),
    ]:
        results[ident].update(review_status='identity_conflict',identity_status=identity,component_identities=components,source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,duplicate_of=None)
        results[ident]['unresolved'].append(note)
    round6a_reads={
        7376:[('js_12','初，降人魏脱卒，其弟憬代领部曲')],
        7377:[('sl_09','江阳太守庐州侯馥'),('hy_01','江阳太守侯馥世明'),('hy_01','使馥同郡人张迎晓喻之')],
        7378:[('sl_01','平原乌丸展广刘哆')],
        7379:[('sl_00','中书监刘均领国子祭酒'),('js_18','大司空刘均举产'),('sl_01','大司空刘均举产')],
        7380:[('js_08','石勒将刘夜堂以驴千头'),('sl_01','勒将刘夜堂以驴千头'),('tj_14','后赵将刘夜堂以驴千头')],
        7381:[('sl_01','长史刘宵诣勒乞降')],
        7382:[('js_08','刘岳以外援不至'),('sl_00','广平王岳为侍中都督中外诸军事进封中山王'),('sl_01','刘曜遣将刘岳'),('tj_14','赵主曜使中山王岳'),('tj_15','遣刘岳、刘震等从男女'),('js_17','刘曜遣从弟岳攻勒'),('sl_02','遣弟中山王岳来攻勒')],
        7383:[('sl_08','牙门将史初收之'),('tj_14','牙门将史初收之')],
        7385:[('sl_00','侍中乔豫和苞上疏'),('tj_14','侍中乔豫、和苞上疏')],
        7386:[('sl_01','孟卓为右执法郎'),('sl_02','孟卓字君伟广平人也')],
        7387:[('js_08','振威将军宋始'),('sl_00','左中郎将宋始'),('sl_01','尹安宋始宋恕赵慎'),('tj_14','尹安、宋始、宋恕、赵慎')],
        7388:[('sl_00','振威将军宋恕'),('sl_01','尹安宋始宋恕赵慎'),('tj_14','尹安、宋始、宋恕、赵慎')],
        7389:[('js_08','弘农太守尹安'),('sl_00','别将尹安赵慎'),('sl_01','尹安宋始宋恕赵慎'),('tj_14','尹安、宋始、宋恕、赵慎')],
        7390:[('sl_01','平原乌丸展广刘哆')],
        7391:[('yi_01','尚书郎张亮议')],
        7392:[('sl_01','张班为左执法郎')],
        7393:[('sl_09','使馥同郡人张迎晓谕之'),('hy_01','使馥同郡人张迎晓喻之')],
    }
    for ident,readings in round6a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(chunk,phrase,'第6批A·原書逐人審讀','Actual named event/office/family passage reviewed; no guessed exact birth/death introduced') for chunk,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='named_subject_and_all_listed_source_contexts_reviewed',source_identities_all_adjudicated=True,last_manual_review_batch='round6a_7376_7393_except_7384')
    for ident,note in {
        7376:'魏脫已卒、弟憬領部曲之事在殷浩北伐前追述，沒有單獨明列卒年；不以北伐年直接充卒年。',
        7377:'侯馥字世明、江陽太守被李雄俘不屈、得赦後數年卒；數年不能換作精確卒年，也不當被俘時已死。',
        7378:'烏丸劉哆與展廣徙襄國，沒有生卒；不與其他劉氏王臣只按姓併入。',
        7379:'劉曜朝國子祭酒/中書監劉均後大司空薦任產，同朝同儒教重臣，三來源身份一致，未記生卒。',
        7380:'劉夜堂運糧桃豹、韓潛馮鐵截之三原書一致；盡獲之可指運糧驢糧，不直接判劉夜堂當年被殺。',
        7381:'徐龕長史劉宵赴石勒請降，後蔡豹死不套給劉宵，生卒不詳。',
        7382:'劉曜從弟劉岳原廣平王進中山王，原書有明文爵遷，七來源同325石梁敗俘、328與劉曜相見者；被俘不等於死，坑九千是士卒不是岳，生卒未知。',
        7383:'張寔牙門史初捕劉弘，劉弘被轘與張寔被殺均非史初本人卒年，雙來源同人。',
        7385:'劉曜侍中喬豫與和苞諫宮殿陵墓，兩來源同官同奏；未記生卒。',
        7386:'孟卓字君偉廣平人、後趙右執法郎，兩來源同官；旁記一作趙孟卓保留名字異文，不硬改姓，未記生卒。',
        7387:'洛陽四將宋始與宋恕尹安趙慎名單及叛降過程四來源一致；宋始一軍被虜不是明言將本人被殺，未記生卒。',
        7388:'宋恕同洛陽四軍將，不能與宋始字相近而合為一人，未記生卒。',
        7389:'尹安同弘農太守、洛陽四軍將，四來源同軍同事，未記生卒。',
        7390:'烏丸首領展廣與劉哆同被徙，兩人明確並列不能混，未記生卒。',
        7391:'南齊書回述晉太興三年320尚書郎張亮議夷王軍號，該年是本人活動，不是南齊時人；生卒未記。',
        7392:'後趙左執法郎張班與右執法郎孟卓分掌九品選舉，原文實有其名，未記生卒。',
        7393:'侯馥同郡人張迎為李雄勸降使者，兩來源同事；數年卒指侯馥，不能轉套張迎。',
    }.items():results[ident]['unresolved'].append(note)
    results[7382]['last_attested_activity']=dict(year=328,source='資治通鑑·咸和三年末',basis='劉曜被俘至襄國，石勒使劉岳等盛服見曜，曜驚其仍活',meaning='325被俘不能當卒；328確在世，精確卒年尚未從所讀段落取得')
    # Round 6B: the same source key can itself contain two homonyms.
    zhou319=chunk_ev('tj_13','刘遐、徐龛击周抚于寒山，破斩之','資治通鑑·太興二年319二月','沛人周堅一名撫；不是周訪子道和')
    zhou365=chunk_ev('tj_18','六月，戊子，益州刺史建城襄会周抚卒','資治通鑑·興寧三年365六月戊子','周訪子、周楚父，與寒山周堅分開')
    zhou_identity=chunk_ev('js_07','抚字道和。强毅有父风','晉書·周訪傳附周撫','完整本人傳承訪二子抚光，末興寧三年卒、子楚嗣')
    zhou_alias=chunk_ev('js_13','沛人周坚，一名抚','晉書·劉遐傳','明列寒山坞主別名，蔡豹傳同卷另有其319被斬，鄧岳傳另記訪子')
    results[7384].update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='identity_conflict',identity_status='merged_zhou_jian_alias_fu_killed_319_and_zhou_fang_son_daohе_died_365',source_identities_all_adjudicated=False,last_manual_review_batch='round6b_identity_hold',component_identities=[dict(identity='沛人周堅，一名撫；彭城坞主、殺周默降石勒',historical_death=date(319,zhou319,month=2)),dict(identity='周訪子周撫字道和，周光兄、周楚父；益州刺史',historical_death=date(365,zhou365,month=6,day_ganzhi='戊子'))])
    results[7384]['evidence'].extend([zhou319,zhou365,zhou_identity,zhou_alias,chunk_ev('js_12','访即遣子抚率兵三千人会石览','晉書·荀崧傳','周訪子抚救宛，姓名省姓並不缺證')])
    results[7384]['unresolved'].append('js_13/sl_01/sl_02每個key內均含兩名周撫；須段落級拆分，CBDB394855只有姓名無官履亦未歸支，不能整條套365或319。魏書把擒錢鳳寫作周撫，晉書本人傳明記弟光，屬敘事異文保留。')
    results[7096].update(duplicate_of=None,candidate_duplicate_of=7384,review_status='source_checked_dates_unknown',identity_status='zhou_fang_son_but_candidate_target_7384_contains_homonyms',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None)
    results[7096]['unresolved'].append('撤回整條重定向：7384已證混入319年被斬周堅/撫，須先拆其周訪子支；本條救荀崧仍指周訪子，不丟既有證據。')
    xu322=chunk_ev('tj_14','于百尺楼上扑杀之','資治通鑑·永昌元年322秋七月','石虎擒泰山徐龕，石勒殺之')
    xu349=chunk_ev('js_17','龛执节不挠，为贼所害','晉書·褚裒傳·永和五年349代陂役','褚裒督護徐龕被李菟敗殺，與322亡者不可能同人')
    results[7394].update(historical_birth=None,historical_death=None,review_status='identity_conflict',identity_status='merged_taishan_rebel_322_and_chu_pou_officer_349',source_identities_all_adjudicated=False,component_identities=[dict(identity='泰山太守徐龕，反復降晉趙',historical_death=date(322,xu322,month=7)),dict(identity='褚裒督護徐龕，代陂戰死',keys=['js:js_17:170:徐龕'],historical_death=date(349,xu349))],last_manual_review_batch='round6b_identity_hold')
    results[7394]['evidence'].extend([xu322,xu349,chunk_ev('sl_02','徐龛太山人也勇果薄行','十六國春秋·徐龕傳','全傳從寒山之役至石勒百尺樓殺，與349另一徐龕隔離')])
    results[7394]['component_identities'][1]['keys']=['js:js_17:61:徐龕']
    results[7394]['unresolved'].append('js_17同卷另一徐龕還見郭璞傳袁颺被逼不得之郡，該處屬早期泰山支，故連js_17 key也須段落拆分，不能只按key整體分支。')
    round6b_reads={7395:[('tj_13','普根母惟氏立之'),('tj_14','妻惟氏，忌代王郁律之强')],7398:[('sl_09','牂牁文猛'),('hy_01','文猛，皆区区稽颡')],7399:[('sl_09','汉国太守梓潼文琰'),('hy_01','汉国太守梓潼文琰')],7402:[('sl_00','杨韬于南安'),('sl_01','张春杨次与别将杨韬不协'),('tj_14','张春、杨次与别将杨韬不协')],7403:[('sl_00','樊氏即故张后侍婢也'),('tj_13','故张后侍婢樊氏为上皇后'),('wsh_01','樊氏是聪张后之侍婢也')],7405:[('sl_01','遣叅将王愉使勒')],7406:[('sl_01','章武人王眷起兵于科斗垒')]}
    for ident,readings in round6b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第6批B·原書本人段落','Actual identity/event passage read; no exact life dates attested') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_listed_primary_source_identities_reviewed',source_identities_all_adjudicated=True,last_manual_review_batch='round6b')
    for ident,note in {7395:'普根母、賀傉母、拓跋猗㐌妻惟氏同人；314及321掌國政非生卒。',7398:'與文琰等同被李雄俘，數年卒指侯馥，非文猛卒。',7399:'梓潼人漢國太守，並列不同郡守，被俘不是被殺，侯馥卒不可套。',7402:'司馬保將楊韜與張春楊次不協，後歸劉曜；被殺的是楊次及司馬保，不是楊韜。',7403:'三書同劉聰樊后，原張后侍婢；318尊弘道后，生卒未載。劉氏被殺概述不能逐個硬套樊氏死亡。',7405:'祖逖參將出聘石勒王愉，非後代王愉因同名直接合；未載生卒。',7406:'章武王眷據科斗壘，所讀未記其卒年；不能把石虎敗乞活王平移給王眷。'}.items():results[ident]['unresolved'].append(note)
    for ident,readings in {7397:[('js_07','并获第五猗、胡混、挚瞻等'),('ssxy_00','瞻字景游京兆长安人')],7400:[('hy_00','李奕自晋寿举毙薮'),('sl_09','奕单骑突门门者射而杀之')],7401:[('hy_00','势之弟大将军广以势无子'),('sl_09','太和二年秋八月汉王广'),('tj_16','贬广为临邛侯，广自杀'),('wsh_02','势弟汉王广以势无子')]}.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第6批B·姓名省姓與異文核明','Read actual named-subject narrative, not a full-name substring alone') for ch,phrase in readings)
        results[ident].update(source_identities_all_adjudicated=True,last_manual_review_batch='round6b',review_status='primary_text_corroborated',identity_status='all_source_identity_contexts_reviewed')
    li346=chunk_ev('sl_09','嘉宁元年秋九月巴东太守杨谦','十六國春秋·嘉寧元年346十月李奕兵變','下接冬十月李奕叛被射殺；華陽國志冬同事，通鑑346冬作李弈')
    results[7400]['historical_death']=date(346,li346,month=10)
    results[7400]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7400]['unresolved'].append('李奕/弈為同成漢李壽李勢大將，兩鍵全讀一致；月十僅春秋詳記，華陽國志冬不構成另一卒年。華陽國志此處文字毙薮明顯傳本訛損，不據此另造事件。')
    li345=chunk_ev('tj_16','贬广为临邛侯，广自杀','資治通鑑·永和元年345秋','漢主李勢弟，非漢武帝時李廣利；春秋太和二年八月同事')
    results[7401]['historical_death']=date(345,li345)
    results[7401]['unresolved'].append('四來源皆李勢弟漢王廣；通鑑/華陽省姓故完整李廣字串未命中不等於缺原文；不可誤取呂光傳李廣利。春秋八月誅貶後尋自殺，卒月不硬鎖八月。')
    zhi316=chunk_ev('ssxy_00','建兴四年与第五琦据荆州以距敦竞为所害','世說新語注引摯氏世本','原文明列建興四年316；第五琦/猗姓名異文保留，不由29歲另編出生年')
    results[7397]['historical_death']=date(316,zhi316)
    results[7397]['unresolved'].append('摯瞻字景游、摯虞兄子摯育子，非虞之親子；世本明列316被王敦害。晉書周訪傳只述被俘送王敦、斬猗，未明列瞻卒年，不把鄰文317年代當明載異說；29歲任郡時未明紀年，出生仍未知。')
    round6c_reads={7407:[('js_13','将军留宠、陆党力战，死之')],7408:[('wsh_00','桓帝皇后祁氏，生三子')],7410:[('js_17','寻而范贲、萧敬相继作乱'),('sl_09','以雄玉衡八年卒子贲嗣'),('tj_13','长生子侍中贲为丞相'),('tj_16','故国师范长生之子贲为帝'),('tj_17','龙骧将军朱焘出范贲，斩之'),('js_07','立范贤子贲为帝')],7412:[('js_00','遣使持节兼太尉万胜奉册')],7413:[('js_12','典客令万默领诸胡')],7414:[('sl_01','遣左常侍董树报聘')],7416:[('hy_00','谢恕举郡为晋，寿破之'),('sl_09','牂牁太守谢恕'),('hy_01','迁柯太守谢恕为涪陵太守')],7417:[('js_15','牙门赵仰皆弘乡人')],7418:[('sl_00','别将尹安赵慎'),('sl_01','尹安宋始宋恕赵慎'),('tj_14','尹安、宋始、宋恕、赵慎')],7419:[('sl_00','羌酋大军须运粮以给昌')],7420:[('sl_09','辛凭字士彦陇西狄道人也')],7421:[('sl_00','光禄大夫游子逺谏曰'),('tj_14','光禄大夫游子远谏曰'),('sl_01','游子逺幼有姿貌聪亮好学不倦')],7422:[('js_08','续呼其兄子竺等曰'),('sl_01','与续子缉兄子存竺等婴城固守'),('tj_14','续呼兄子竺等谓曰')],7423:[('js_08','已共推其息缉为营主'),('sl_01','与续子缉兄子存竺等婴城固守'),('tj_14','与续子缉、兄子存、竺等婴城固守')],7425:[('sl_08','左司马阴元等以实子骏幼冲'),('tj_14','左司马阴元等以寔子骏尚幼')]}
    for ident,readings in round6c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第6批C·同傳同事件人物集中逐人審讀','Source context actually read; sentence subjects distinguished') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_listed_source_contexts_reviewed',source_identities_all_adjudicated=True,last_manual_review_batch='round6c')
    for ident,note in {7407:'本人明言戰死蔡豹檀丘役，但本傳該段未直接列年，死亡年需另讀紀年對照，不能只據蔡豹後卒年硬套。',7412:'太興三年320兼太尉萬勝奉虞后追冊，死者是虞后而非使者，未知生卒。',7413:'典客令萬默被疑偏胡擬大辟，孔坦獨不署，原文不載已執行，不把擬刑當卒。',7414:'董樹為石勒左常侍出聘祖逖，與被斬叛將童建分清，不同人物不能因鄰句而合。',7416:'牂牁/涪陵任職議、歸晉又據城抗李奕均謝恕同一南中官員，候馥數年卒不套給他。',7418:'趙慎是洛陽四軍之一，非宋始被虜軍本人；三來源同人，未記生卒。',7419:'原文羌酋大軍須，名可能應含大字，暫保留原錄名並列原字串；被劉雅敗非明言本人卒。',7420:'辛憑字士彥敦煌守、子髦被辛晏扣留仍勸討之，並非辛理因憂死；兩傳相鄰不套卒。',7421:'游子遠15歲見張華未記該事年；320下獄後赦並拜大司徒，不能誤判下令速殺即已執行。未記生卒。',7422:'邵續兄子竺、與存缉守厭次後降；叔續被殺不可套本人。',7423:'邵續子缉承營主/冀州官位，與兄子存竺分開；父被殺不等於缉同卒。',7425:'張寔左司馬陰元推茂，未記生卒，寔320遇害不是元卒。'}.items():results[ident]['unresolved'].append(note)
    results[7408].update(review_status='duplicate',duplicate_of=7395,identity_status='same_tuoba_yituo_wife_mother_of_pugen_henу_hena')
    results[7408]['unresolved'].append('魏書祁氏與通鑑惟氏同桓帝猗㐌妻、普根/惠帝賀傉/煬帝紇那母、殺郁律掌國事；姓名傳本異文，母子與獨特事件對應，非僅姓名相似。')
    fan349=chunk_ev('tj_17','龙骧将军朱焘出范贲，斩之','資治通鑑·永和五年349夏四月','范長生子範賁，六鍵同承相位與僭立被討')
    results[7410].update(historical_death=date(349,fan349,month=4),review_status='primary_text_corroborated')
    results[7410]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    zhao320=chunk_ev('sl_08','乃诛阎渉赵卬及党与数百人','十六國春秋·張茂·太興三年320六月','晉書趙仰/春秋通鑑趙卬同張寔牙門謀弒')
    results[7417].update(historical_death=date(320,zhao320),review_status='primary_text_corroborated')
    results[7417]['evidence'].append(zhao320)
    for ident,readings,components,note in [
        (7404,[('sl_00','相国粲使其党王平'),('sl_01','使虎袭乞活王平于梁城'),('tj_13','汉相国粲使其党王平')],[dict(identity='317劉粲黨王平，誘太弟乂/義披甲'),dict(identity='乞活首領王平，梁城敗石虎')],'兩支所屬及事跡不同，缺家族官遷聯繫，同名近年代仍不能直接合。'),
        (7411,[('sl_01','中山丁零翟鼠叛勒'),('sl_03','丁零翟鼠率其所部来降'),('tj_17','丁零翟鼠帅所部降燕')],[dict(identity='石勒時中山丁零翟鼠，逃子胥關代郡'),dict(identity='351降慕容儁丁零翟鼠，封歸義王')],'相隔約35年、同族同名可疑同人但未見身份承接；暫不把351在世強加早期支。'),
        (7415,[('sl_09','永昌谢俊牂牁文猛'),('sl_09','执太守赵俊[晋帝纪作谢俊]'),('hy_01','永昌谢俊')],[dict(identity='侯馥傳旁列永昌謝俊'),dict(identity='325張龍擒涪陵太守趙俊，一作謝俊')],'同一sl_09 key含永昌謝俊及趙俊異名，未見轉郡聯結，不先判同人或同時生卒。')]:
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第6批C·同名支別','Actual passages read; branch links remain unproved') for ch,phrase in readings)
        results[ident].update(review_status='identity_conflict',identity_status='same_name_branches_not_linked',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,component_identities=components,last_manual_review_batch='round6c')
        results[ident]['unresolved'].append(note)
    for ch,phrase in [('tj_14','长沙邓骞闻之'),('js_10','邓骞，子长真，长沙人'),('js_14','粲以父骞有忠信言'),('js_16','主簿邓骞与道融劝卓')]:results[7424]['evidence'].append(chunk_ev(ch,phrase,'第6批C·鄧騫四原文','甘卓主簿、長沙長真、鄧粲父可連同人；完整傳末大司農卒但未列年'))
    results[7424].update(review_status='source_checked_dates_unknown',identity_status='primary_sources_agree_cbdb_name_only_unlinked',source_identities_all_adjudicated=False,last_manual_review_batch='round6c')
    results[7424]['unresolved'].append('CBDB428590原始行僅鄧騫姓名，日期籍貫官履均空；四原書同長沙官、鄧粲父，原庫支尚缺身份識別信息，保留門檻false。')
    round7a_reads={7426:[('tj_14','其将阴监将兵迎之')],7427:[('hy_00','收当、思明斩之'),('sl_09','收马当解思明斩之'),('tj_16','以尚书令马当为六军都督')],7428:[('tj_14','责宫门小执法冯翥')],7431:[('sl_02','兖州刺史魏统豫州牧张遇'),('tj_17','兖州刺史魏统、荆州刺史乐弘')],7433:[('sl_02','杀其所生母齐氏')],7434:[('sl_02','材官将军刘佩攻乙连不克'),('sl_03','高诩刘佩皆中流矢卒'),('tj_16','玄菟太守河间刘佩曰')],7435:[('js_16','丞刘荣坐事当死')],7437:[('js_08','新蔡内史周闳率众筑垒')],7438:[('sl_04','乔秉起雍'),('tj_17','乔秉起雍')],7440:[('tj_13','其母崔氏固止之')]}
    for ident,readings in round7a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第7批A·原書逐條審讀','Actual subject, office and event context read') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_original_source_identity_contexts_reviewed',source_identities_all_adjudicated=True,last_manual_review_batch='round7a')
    ma345=chunk_ev('sl_09','收马当解思明斩之','十六國春秋·太和二年345秋八月','與華陽及通鑑同李勢弟廣請立太弟之案')
    results[7427].update(historical_death=date(345,ma345,month=8),review_status='primary_text_corroborated')
    results[7427]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    liupei344=chunk_ev('sl_03','高诩刘佩皆中流矢卒','十六國春秋·建元二年344伐宇文','前燕材官將軍/玄菟太守劉佩，三來源職軍脈絡相符')
    results[7434].update(historical_death=date(344,liupei344),review_status='primary_text_corroborated')
    for ident,note in {7426:'張寔將陰監阻司馬保入涼，非陰元，亦不只憑陰姓合陰鑒；生卒未知。',7428:'馮翥忌胡諱觸怒但原文明言不罪，不能作被誅卒年。',7433:'石斌生母齊氏被石虎殺，完整傳已讀但此傳無單獨紀年，待原紀年核對；石斌後349被殺不是母卒年。',7435:'堂邑丞劉榮擬死罪，解械救火後自著械未載執行，末卒于官指李廣，不能套其卒。',7437:'周閎為祖逖築塢將，祖逖病死不等於周閎卒，未知日期。',7438:'雍地起兵喬秉同兩來源，353起兵、354苻雄攻雍後本人結果未在此段明示。',7440:'溫嶠母崔氏卒訊為追述，318聞喪不等於318才卒，保留歷史卒年未知。'}.items():results[ident]['unresolved'].append(note)
    for ident,readings,components,note in [
        (7430,[('js_08','逖遣韩潜、冯铁等追击'),('sl_01','别将冯铁卫策等邀击'),('tj_14','别将冯铁邀击'),('js_09','苏峻将冯铁杀侃子')],[dict(identity='320祖逖將馮鐵汴水截糧'),dict(identity='蘇峻將馮鐵殺陶侃子後降石勒，被召殺')],'兩支未明轉仕聯繫，不能整條套後支卒年。'),
        (7436,[('sl_01','主簿刘茂献捷于汉'),('sl_02','司徒刘茂特进郎闿'),('tj_13','其副刘茂皆为将军'),('tj_17','司徒刘茂、特进郎闿'),('wsh_01','主簿刘茂献捷于曜')],[dict(identity='319石勒主簿出使劉曜而逃歸'),dict(identity='352冉魏司徒劉茂與郎闓自殺')],'相隔33年但兩朝可能相承，仍缺同人官履連結，352只存司徒支，不因姓名直接判全條卒。'),
        (7439,[('js_06','承字文子。参安东军事'),('tj_14','南平太守夏侯承'),('jk_00','裴夏侯承卫旌'),('js_10','南平太守夏侯承')],[dict(identity='夏侯淳子字文子，322隨甘卓討敦獲免'),dict(identity='孫權黃武時步騭上疏稱荊州名士夏侯承')],'晉書明父夏侯淳，與吳黃武年間名士跨約一世紀，來源不可合；被敦欲殺後明言免，不作322卒。')]:
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第7批A·跨事同名待拆','Named passages actually read; unproved links not silently accepted') for ch,phrase in readings)
        results[ident].update(review_status='identity_conflict',identity_status='multiple_source_identity_branches_require_link_or_split',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,component_identities=components,last_manual_review_batch='round7a')
        results[ident]['unresolved'].append(note)
    liu352=chunk_ev('tj_17','司徒刘茂、特进郎闿','資治通鑑·永和八年352','兩人憂冉閔征燕不返皆自殺；限司徒支')
    results[7436]['component_identities'][1]['historical_death']=date(352,liu352)
    feng_bad=chunk_ev('tj_13','冯者帅其众降','資治通鑑·建興四年316','快取姓名有私用區缺字，提取名馮者遺掉偏旁；實際率眾降非被殺')
    results[7429]['evidence'].append(feng_bad)
    results[7429].update(review_status='source_checked_dates_unknown',identity_status='original_event_read_name_glyph_incomplete',source_identities_all_adjudicated=False,last_manual_review_batch='round7a')
    results[7429]['unresolved'].append('原文馮者缺字，須另一傳本確完整姓名；不能把快取漏字姓名當完整史名。馬嚴投水死不套給此降將。')
    for ident,ch,phrase in [(7432,'hy_00','巴西鲜思明共谋据成都'),(7513,'sl_09','马当解思明谏曰'),(7513,'tj_16','马当、解思明谏曰')]:results[ident]['evidence'].append(chunk_ev(ch,phrase,'華陽國志鮮/春秋通鑑解思明異名對照','同巴西李壽謀臣、勸稱藩、同與馬當諫立李廣，獨特事歷相同'))
    results[7513].update(source_identities_all_adjudicated=True,identity_status='same_cheng_han_counsellor_all_sources_linked',review_status='primary_text_corroborated',historical_death=date(345,ma345),last_manual_review_batch='round7a_related_canonical')
    results[7432].update(source_identities_all_adjudicated=True,duplicate_of=7513,identity_status='xian_jie_siming_variant_same_cheng_han_counsellor',review_status='duplicate',last_manual_review_batch='round7a')
    results[7432]['unresolved'].append('鮮思明為華陽傳本姓氏異文；與解思明同巴西人同謀李壽稱藩及同馬當被殺，不按單一字形另立。')
    wei=chunk_ev('tj_17','安北将军魏统卒，弟憬代领部曲','資治通鑑·353年姚襄殷浩猜忌事','與晉書魏脫卒其弟憬代領同一獨特繼領事；敘事回述不能直接鎖353卒')
    results[7431]['evidence'].append(wei)
    results[7431]['unresolved'].append('351兗州刺史魏統歸晉，後安北將軍卒弟憬繼領；卒語嵌353背景回述未直接紀月，不把353無條件當明載卒年。')
    results[7376].update(review_status='duplicate',duplicate_of=7431,source_identities_all_adjudicated=True,identity_status='wei_tuo_tong_variant_same_deceased_brother_of_wei_jing')
    results[7376]['evidence'].append(wei)
    results[7376]['unresolved'].append('魏脫/統異文由弟魏憬代領部曲之獨特事明確接合，不與他人同姓合；353是回述上下文不是精確卒年。')
    # Candidate key scope is a locator constraint, not every homonym in its book.
    # Rechecking the extraction bio narrows these keys; evidence remains the book.
    results[7384]['unresolved']=[n for n in results[7384]['unresolved'] if not n.startswith('js_13/sl_01/sl_02每個key')]
    results[7384]['unresolved'].append('逐鍵範圍複核：js_13:215只鄧岳同逃之周訪子，sl_02:315只襄陽退走之訪子，不能因同卷另一同名便說其key混。真正混合key是sl_01:289（候選bio本身合318殺周默及330襄陽敗），js_14:11另明指寒山前後叛晉支；CBDB394855只有名無履歷。需拆候選而非按整卷名字命中硬分。')
    results[7384]['source_key_identity_adjudications']=[dict(key=k,identity=('mixed_early_zhou_jian_and_late_zhou_fang_son' if k.startswith('sl:sl_01:') else 'early_zhou_jian_alias_fu' if k.startswith('js:js_14:') else 'cbdb_identity_unlinked' if k.startswith('cbdb:') else 'zhou_fang_son_daohе')) for k in results[7384]['keys']]
    results[7394]['unresolved']=[n for n in results[7394]['unresolved'] if not n.startswith('js_17同卷另一徐龕')]
    results[7394]['unresolved'].append('js_17:61候選bio明指褚裒督護代陂死者，並未把同卷郭璞傳徐龕也納入此key；其餘五key候選bio明指泰山太守，兩支身份差異成立，非單純同卷名字命中造成hold。')
    results[7394]['source_key_identity_adjudications']=[dict(key=k,identity='chu_pou_officer_killed_349' if k.startswith('js:js_17:') else 'taishan_rebel_killed_322') for k in results[7394]['keys']]
    results[7415].update(review_status='source_checked_dates_unknown',identity_status='same_yongchang_xie_jun_captured_by_li_xiong',source_identities_all_adjudicated=True,historical_birth=None,historical_death=None)
    results[7415].pop('component_identities',None)
    results[7415]['unresolved']=[n for n in results[7415]['unresolved'] if not n.startswith('同一sl_09 key含永昌謝俊')]
    results[7415]['unresolved'].append('撤回僅因同卷另一姓名而生的hold：sl_09:353候選明限定永昌謝俊，hy_01同侯馥旁列永昌謝俊，兩原文身份可閉合。325涪陵趙俊一作謝俊不在該候選bio範圍。本人生卒未載，侯馥數年卒不可套。')
    round7b_reads={7442:[('sl_00','太史令康相言于聪曰')],7443:[('js_08','汝阳太守张敞')],7444:[('js_12','吴兴徐馥为贼'),('js_07','馥党惧，攻馥，杀之')],7445:[('js_08','董瞻、于武、谢浮等十余部')],7448:[('hy_00','杨茂搜子难敌、坚头'),('sl_00','与弟坚头南奔汉中'),('sl_09','杨难敌及弟坚头'),('tj_13','与少子坚头分领部曲'),('tj_14','与弟坚头南奔汉中'),('yi_01','与坚头分部曲'),('sl_01','与弟坚头同奔汉中'),('wsh_04','与弟坚头分部曲')],7450:[('sl_02','宇文别部涉奕干'),('sl_03','遂斩奕干宇文士卒'),('tj_16','遂斩涉夜干')],7451:[('sl_08','焦崧陈安举兵冦陇右'),('js_14','焦崧、陈安寇陇石')],7452:[('sl_01','建徳校尉王和掘得员石')],7453:[('sl_01','牙门将王波为记室参军'),('sl_02','中书监王波议曰'),('sl_09','中书监王波议'),('tj_14','以牙门将王波为记室参军'),('tj_15','王波为中书令'),('tj_16','中书监王波曰'),('js_01','与王波为之撰朝仪')],7454:[('js_08','欲执台使王英'),('sl_01','欲执台使王英'),('tj_14','欲执台使王英')],7455:[('sl_08','武威姑臧人有白兴者')],7456:[('js_08','从子汝南太守济'),('js_17','执约兄子济'),('sl_01','汝南人执内史祖济'),('tj_14','后赵将石生寇汝南，执内史祖济')]}
    for ident,readings in round7b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第7批B·實讀原書人物段落','Source-key candidate identity scope checked against original office/family/event') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_listed_original_source_identities_reviewed',source_identities_all_adjudicated=True,last_manual_review_batch='round7b')
    for ident,note in {7442:'康相為劉聰太史令諫天象，預言三分天下不作已發生的本人生卒。',7443:'祖逖汝陽太守張敞築武牢南壘，非西漢同名張敞，原書未記生卒。',7445:'于武為張平所統坞主，張平被謝浮斬不等于于武死。原名于/於按原錄保留。',7448:'八鍵同楊茂搜子、難敵弟、楊盤父右賢王河池支；楊難敵卒334不套堅頭，子盤任王不獨立證父卒。宋書太興四321與通鑑323南奔有事年異載，不推作生卒。',7451:'兩鍵只指陳安同寇陇右焦崧。未把另一焦嵩僅憑音近直接合，出戰事不是本人生卒。',7452:'石勒建德校尉王和出土權石，斬死者為上句從事中郎劉奧非王和；無生卒。',7453:'王波由牙門將記室定選、裴憲同撰朝儀至石虎中書令/監七來源可閉同人；340被黜仍白衣領職，不作卒年。',7454:'王英為晉台使，邵洎欲執而段匹磾阻，沒有被殺記載，段匹磾後死不能套王英。',7456:'祖逖、祖約為兄弟，兩書均從子/兄子汝南官祖濟，321築壘與326被俘四來源身份可閉；被執送石勒非必殺，不把326當卒。'}.items():results[ident]['unresolved'].append(note)
    xu315=chunk_ev('tj_13','馥党惧，攻馥，杀之','資治通鑑·建興三年315春正月','徐馥起兵314、被同黨殺315，不把起兵年當卒年')
    results[7444].update(historical_death=date(315,xu315,month=1),review_status='primary_text_corroborated')
    results[7444]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    bai344=chunk_ev('sl_08','乃于姑臧市轘而杀之','十六國春秋·建元二年344秋八月白興案','白興本人為妾使妻為婢而受轘，原文明列紀年')
    results[7455].update(historical_death=date(344,bai344,month=8),review_status='primary_text_corroborated')
    results[7455]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    she344=chunk_ev('tj_16','遂斩涉夜干','資治通鑑·建元二年344伐宇文','與春秋建元二年同344')
    she345=chunk_ev('wsh_05','建国八年，晃伐逸豆归','魏書·宇文莫槐傳·建國八年345','下文明殺驍將涉亦干；同宇文滅亡戰役與通鑑差一年')
    results[7450].update(review_status='primary_text_corroborated',historical_death=dict(year=None,year_alternatives=[344,345],precision='conflicting_source_years',calendar='historical_CE_year',alternatives=[dict(year=344,source=she344),dict(year=345,source=she345)]))
    results[7450]['evidence'].extend([she344,she345])
    results[7450]['unresolved'].append('涉奕干/夜干/亦干為宇文逸豆歸南羅大，同與慕容翰霸交戰被斬。通鑑春秋344與魏書建國八345一歲差，保留源別，不硬填單一卒年。南羅大為官號不併入姓名。')
    results[7449].update(review_status='duplicate',duplicate_of=7450,identity_status='same_yuwen_general_she_yigan_yeghan_name_variant',historical_death=results[7450]['historical_death'],source_identities_all_adjudicated=True,last_manual_review_batch='round7b')
    results[7449]['evidence'].extend([she344,she345])
    results[7449]['unresolved'].append('魏書涉亦干同宇文主同戰役同敗殺，歸7450保留345/344紀年異載，重定向不消掉異說。')
    # Li Yang has genuinely different candidate subjects, not merely book hits.
    for ch,phrase in [('js_03','乡人幽州刺史李阳'),('sl_01','邑人李阳相近'),('tj_14','与李阳邻居'),('js_13','竟陵太守李阳'),('sl_02','竟陵太守李阳'),('tj_15','竟陵太守李阳'),('tj_16','竟陵太守李阳'),('wsh_02','其将李阳说曰'),('js_09','督护竟陵太守李阳')]:results[7447]['evidence'].append(chunk_ev(ch,phrase,'李陽逐鍵所指分支核查','candidate bio同樣分幽州刺史/石勒鄰人/晉竟陵太守三支，不是整卷盲配'))
    results[7447].update(review_status='identity_conflict',identity_status='merged_western_jin_youzhou_jin_jingling_and_shi_le_neighbor_li_yang',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,last_manual_review_batch='round7b',component_identities=[dict(identity='西晉幽州刺史李陽，京師大俠，王衍借其名止郭氏'),dict(identity='石勒武鄉鄰人李陽、321拜參軍都尉'),dict(identity='晉竟陵太守李陽，陶侃督護，328–339軍事')])
    results[7447]['unresolved'].append('candidate js03本身明幽州刺史、tj14本身明石勒鄰人、其餘多鍵明竟陵守；三身份缺聯繫，不能僅因同名跨職強合。')
    for ch,phrase in [('sl_03','范阳太守李产'),('tj_14','范阳李产避乱依逖'),('tj_17','范阳太守李产')]:results[7446]['evidence'].append(chunk_ev(ch,phrase,'李產三原書本人段落','321祖約姻親歸范陽及350李績父降燕，皆已讀，尚無兩事直接同人引文'))
    results[7446].update(review_status='source_checked_dates_unknown',identity_status='li_chan_fanyang_subjects_read_connection_pending',source_identities_all_adjudicated=False,last_manual_review_batch='round7b')
    results[7446]['unresolved'].append('321范陽李產攜宗族歸鄉與350范陽守李績父可能同人，所讀三段尚未直接承前生涯；暫不只憑同名同郡硬關門。本人無生卒明年。')
    round7c_reads={7458:[('js_08','董瞻、于武、谢浮等十余部')],7459:[('sl_01','卫麟为右中郎将')],7460:[('js_08','逖诱浮使取平'),('tj_13','乃诱其部将谢浮，使杀之')],7461:[('js_06','宜都太守谭该'),('tj_14','宜都太守谭该'),('js_10','宜都太守谭该')],7462:[('js_15','姑臧令辛岩'),('sl_00','张阆窦涛辛岩宋辑'),('sl_08','姑臧令辛岩'),('tj_15','武兴太守辛岩')],7463:[('js_13','遐妻骁果有父风'),('sl_01','邵续深器异之以女妻焉'),('tj_14','遐妻，邵续女也')],7464:[('js_08','续弟乐安内史洎'),('sl_01','邵续之弟乐安内史洎'),('tj_14','邵续之弟乐安内史洎')],7465:[('js_08','蓬陂坞主陈川'),('sl_01','龙骧将军陈川以浚仪降勒'),('tj_13','蓬陂坞主陈川'),('js_14','周抚、陈川相系背叛')],7466:[('wsh_00','遣使韩畅加崇爵服')],7469:[('sl_00','安使将军刘烈赵罕袭汧城')],7470:[('sl_00','镇北将军策进屯汾阴'),('sl_01','镇北将军刘策救之'),('tj_13','镇北将军刘策屯汾阴')],7471:[('tj_13','从弟路孤帅其部落降于郁律'),('wsh_00','其从弟路孤率部落内附')],7472:[('sl_00','靳准遣侍中卜泰'),('sl_01','使侍中卜泰'),('tj_13','靳准使侍中卜泰'),('tj_14','左光禄大夫卜泰')],7473:[('js_02','以皇子裒为琅邪王'),('js_09','琅邪孝王裒字道成'),('js_13','琅邪王裒始受封'),('sl_01','车骑将军琅邪王裒'),('tj_13','冬，十月，丁未，琅邪王裒薨'),('js_04','琅邪王裒为车骑将军'),('ssxy_01','谓裒有大成之度胜于明帝')]}
    for ident,readings in round7c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第7批C·原文省姓/異文人物核查','Actual named role/family/context read; others deaths not transferred') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_primary_identities_reviewed',last_manual_review_batch='round7c')
    for ident,note in {7458:'張平所統坞主董瞻與督護董昭原文分列，不能因姓同字近直接合；未知生卒。',7459:'衛麟降後趙受右中郎將，後文段匹磾文鴦邵續被害未明及衛麟，不能套321卒。',7460:'謝浮為張平部將，受祖逖誘斬張平；被斬者張平非浮。',7461:'三來源同322宜都守譚該露檄討敦；夏侯承獲免、甘卓被害不直接證譚該死亡。',7462:'辛岩/巖321姑臧令與327武興太守參軍，三涼書同張茂骏官脈絡；運糧敗不等于本人卒。',7463:'邵續女劉遐妻三來源同救夫及326焚田防甲仗事，父續卒和夫遐卒均不可套給她。',7464:'邵洎為邵續弟樂安內史、阻段匹磾歸晉，三書一致；降趙不等于死，後續殺匹磾等未明及洎。',7465:'陳川蓬陂坞主援祖逖、殺李頭後降石勒、徙部眾，同四原書；所部移徙非本人死亡，未知生卒。',7466:'韓暢為晉元帝使代加郁律爵服，遭拒不等于被殺，原文不言生卒。',7469:'陳安將劉烈襲汧城，呼延瑜斬的是陳安弟集與張明支，不套劉烈，無生卒。',7470:'劉曜鎮北將軍劉策救靳明，後安定王策與之可疑同人，但本批三key本身限定鎮北官；沒有生卒明文。',7471:'劉虎從弟路孤降郁律、娶帝女，兩來源同事，原文省劉姓非無名；無生卒。',7472:'卜泰靳准侍中轉劉曜左光祿大夫，318奉璽與323諫立儲四來源身份可閉，靳氏全族被殺不套其姓卜。'}.items():results[ident]['unresolved'].append(note)
    pou317=chunk_ev('tj_13','冬，十月，丁未，琅邪王裒薨','資治通鑑·建武元年317十月丁未','元帝荀氏子、明帝弟、宣城改琅邪王')
    pou18=chunk_ev('js_09','建武元年薨，年十八','晉書·元帝諸子·琅邪孝王裒','317卒虛歲18反推300，不是生日實載')
    results[7473].update(review_status='primary_text_corroborated',historical_birth=date(300,pou18,age=18),historical_death=date(317,pou317,month=10,day_ganzhi='丁未'))
    results[7473]['historical_death'].update(precision='lunar_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7473]['evidence'].extend([pou317,pou18])
    results[7473]['unresolved'].append('世說主文欲立簡文與注引中興書欲立裒兩說有異，該key候選範圍明裒且注原文明人；不因同卷另一皇子而混生卒。七來源同道成，虛歲推生保留精度。')
    for ident,readings,components,note in [(7468,[('hy_01','太守章陵刘威又为作颂'),('sg_05','兄子威又代从政'),('sg_10','宏子咸徐州刺史'),('js_15','刘咸攻韩璞'),('sl_00','遣将军刘咸攻韩璞'),('sl_08','刘咸[一作威]攻韩璞'),('tj_14','其将刘咸攻韩璞')],[dict(identity='犍為太守章陵劉威，為程貞作頌'),dict(identity='曹操時劉勳兄子威，代父政後免官'),dict(identity='西晉劉宏子咸，徐州刺史'),dict(identity='前趙劉曜將劉咸，一作威')],'原候選bio四分支清楚，不能因咸威傳本異名把曹操時及西晉官和前趙宗室全合。'),(7475,[('tj_14','敦参军吕猗'),('js_10','敦参军吕猗'),('js_17','吕猗母皇氏'),('ssxy_03','防军吕漪说敦曰')],[dict(identity='王敦參軍呂猗/漪，322讒周顗戴淵'),dict(identity='幸靈治病呂猗母皇氏傳主之子')],'幸靈傳之呂猗未載王敦官履或籍貫，與讒臣支可能同人但缺連結；母病癒無本人生卒。')]:
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第7批C·候選範圍與真原文雙核','Identity branch actually present in candidate and book; no chapter-wide false merge') for ch,phrase in readings)
        results[ident].update(review_status='identity_conflict',identity_status='candidate_identity_branches_not_adjudicated_as_one',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,component_identities=components,last_manual_review_batch='round7c')
        results[ident]['unresolved'].append(note)
    for ch,phrase in [('tj_14','遣参军司马赞、孙双奉表诣台'),('js_10','遣参军司马赞、孙双奉表诣台')]:results[7474]['evidence'].append(chunk_ev(ch,phrase,'甘卓參軍司馬贊兩原文','322奉表使者同人，未載生卒'))
    results[7474].update(review_status='source_checked_dates_unknown',identity_status='primary_contexts_agree_cbdb_identity_unlinked',source_identities_all_adjudicated=False,last_manual_review_batch='round7c')
    results[7474]['evidence'].append(dict(citation='CBDB392892 original row inspected',excerpt=json.dumps(originals[7474].get('cbdb_originals',[]),ensure_ascii=False)))
    results[7467]['evidence'].append(dict(citation='CBDB135487 BIOG_MAIN original row inspected',excerpt=json.dumps(originals[7467]['cbdb_originals'],ensure_ascii=False),interpretation='Index317 code2912 only; no birth death activity years notes or offices. Does not identify legendary Magu or establish CE317 activity.'))
    results[7467].update(review_status='secondary_source_checked',identity_status='database_name_only_magu_identity_not_established',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,last_manual_review_batch='round7c')
    results[7467]['unresolved'].append('原庫只麻姑名與指數317，不能當317出生或登場，也不能不查身份便套傳說仙人履歷。')
    round8a_reads={7476:[('tj_14','襄阳太守周虑'),('wsh_02','襄阳太守周虑'),('js_10','襄阳太守周虑')],7477:[('tj_14','宜都内史天门周级'),('js_15','叔父级为宜都内史')],7478:[('sl_01','平阳大户周置')],7479:[('sl_00','卫军将军呼延瑜'),('tj_14','卫将军呼延瑜')],7480:[('js_12','祗字承祖')],7481:[('tj_14','使参军孙双诣武昌'),('js_10','使参军孙双诣武昌')],7482:[('sl_01','孙黙以郡叛降扵勒')],7484:[('tj_14','与零陵太守尹奉')],7485:[('js_06','大将干瓒作难'),('js_11','部将干瓒、戴羲等作乱'),('js_13','大将干瓒、戴羲等作乱'),('tj_16','翼部将干瓒等作乱')]}
    for ident,readings in round8a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第8批A·原書本人事件實讀','Named subject and sentence death target distinguished') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='listed_primary_contexts_reviewed',source_identities_all_adjudicated=ident not in [7476,7480,7481],last_manual_review_batch='round8a')
    for ident in [7476,7480,7481]:
        results[ident]['evidence'].append(dict(citation='CBDB original database record directly inspected',excerpt=json.dumps(originals[ident]['cbdb_originals'],ensure_ascii=False),interpretation='Name only; all dates notes offices absent. Primary identities agree but this independent database branch remains unlinked.'))
        results[ident]['identity_status']='primary_sources_agree_cbdb_name_only_unlinked'
        results[ident]['unresolved'].append('原CBDB條目只姓名無官履日期，不能因姓名相同宣告全部keys身份已核全。')
    for ident,note in {7476:'殺甘卓者周慮為襄陽太守，322是殺卓活動年非周慮本人卒年。',7477:'周級為天門宜都內史、周該叔；侄該拷死不言故，級反獲免，不能套322卒。',7478:'周置率平陽雜戶降石勒，不是後文被殺靳氏，生卒未載。',7479:'呼延瑜敗斬陳安弟集、張明，不能反套殺人者卒年；所讀無本人生卒。',7480:'孔祗字承祖為周札功曹，冒刃葬札；孔坦父是侃非祗，鄰傳不可混父子，札324被殺不套祗。',7481:'孫雙為甘卓參軍先諫敦又奉表，同兩原書，甘卓被殺不直接證雙死。',7482:'孫默/黙琅邪內史降趙，孫姓官員原文實有姓名，非無名；無生卒。',7484:'零陵守尹奉隨譙王承反敦，未把同名寧州刺史自動合；周圍將吏死不等於尹奉死。'}.items():results[ident]['unresolved'].append(note)
    gan345=chunk_ev('tj_16','朱焘与安西长史江A170，建武司马毛穆之、将军袁真等共诛之','資治通鑑·永和元年345七月後','幹瓚殺曹據叛亂被諸將誅，本人死亡明言；無生日或年齡')
    results[7485].update(review_status='primary_text_corroborated',historical_death=date(345,gan345))
    results[7485]['evidence'].append(gan345)
    for ch,phrase in [('sl_00','宣氏为弘徳皇后'),('js_01','嫡母宣氏不之礼')]:results[7483]['evidence'].append(chunk_ev(ch,phrase,'宣氏候選兩身份核查','前趙劉聰皇后與裴潛嫡妻不是同人'))
    results[7483].update(review_status='identity_conflict',identity_status='merged_liu_cong_empress_and_pei_qian_wife_xuan',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,last_manual_review_batch='round8a',component_identities=[dict(keys=['sl:sl_00:337:宣氏'],identity='劉聰后318被劉粲尊弘德后，武氏/宣懷養女異文另待核'),dict(keys=['js:js_01:24:宣氏'],identity='西晉裴秀嫡母宣氏、裴潛妻')])
    results[7483]['unresolved'].append('原候選身份兩支不同年代家屬，不能同為宣姓婦便合。前趙支武/宣氏傳本異名仍需獨立核清，不把靳氏滅族概述套死。')
    round8b_reads={7486:[('sl_00','及将军张明等率步骑')],7487:[('sl_09','雄将张龙寇巴东')],7488:[('js_11','纪睦、徐宁奉王使'),('ssxy_02','徐宁字安期东海郯人')],7489:[('js_11','部将干瓒、戴羲等作乱'),('js_13','大将干瓒、戴羲等作乱')],7491:[('tj_14','参军李梁说卓'),('js_10','参军李梁说卓')],7492:[('js_01','参军桓罴说承'),('tj_14','参军桓罴说谯王承')],7493:[('sl_00','陇西太守梁勋'),('tj_14','陇西太守梁勋')],7494:[('wsh_02','司马杨朗'),('ssxy_01','王大将军始下杨朗'),('ssxy_02','世彦识器理致')],7495:[('tj_14','从事武延'),('js_15','从事武延')],7496:[('tj_13','武氏号弘德皇后')],7497:[('sl_01','立忽跋邻为单于')],7498:[('js_08','欲害其从叔𬴊')],7499:[('js_15','长史泛祎怒曰')],7500:[('js_11','远弟缙，名亚于远')],7502:[('tj_14','建昌太守长沙王循')],7504:[('sl_00','尚书郎王扩[一作王犷]'),('tj_14','光国中郎将王犷')],7505:[('tj_14','卓兄子卬为敦参军')]}
    for ident,readings in round8b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第8批B·原書同事人物分段審讀','Subject actually identified in read narrative, not neighboring name/date capture') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_primary_contexts_adjudicated',source_identities_all_adjudicated=ident!=7493,last_manual_review_batch='round8b')
    for ident,note in {7487:'成漢張龍攻巴東、涪陵，與晉書東漢同名不自動合；敗走不等於被殺，未知生卒。',7488:'徐寧字安期東海郯人由縣令入尚書吏部及江州官；庾翼所言按察免官無死亡，生卒未載。',7491:'甘卓參軍李梁勸按兵，兩來源同人；卓死不是梁死。',7492:'王敦使桓羆被譙王承囚，囚禁不等於殺；無生卒。',7493:'陇西梁勳同楊韜降前趙，遷長安不是被殺。CBDB408897原行僅名無官履年月，門檻false。',7494:'弘農楊朗字世彥、楊淮子為王敦司馬，後被收欲殺因明帝崩得免；325帝崩不能套楊朗卒。兩世說明父名與官履相承。',7495:'武延隨譙王承送喪京師、事畢還，死亡者主簿桓雄及王承，不是武延。',7496:'原文武氏弘德皇后與春秋宣氏同帝同尊號可疑異文，但7483混裴潛妻不可整條重定向；本條生卒未知。',7497:'忽跋鄰為段末杯殺截附真後所立單于；死者是截附真不是忽跋鄰。',7498:'段驎快取字𬴊，劉琨申冤表稱匹磾從叔、與末波拒其奪國，原文實有人；被謀害未等於已害。',7499:'氾/汜/泛禕為張茂長史，所言宜斬馬岌不是自己被斬，無卒年。',7500:'熊縉為熊遠弟、熊鳴鵠父王敦主簿鄱陽太守，前句數月病卒指兄熊遠；縉卒年不詳。',7502:'長沙王循為建昌太守反王敦，旁邊鄭澹被斬非循卒，未記生卒。',7504:'王擴一作王獷同前趙光國中郎將說楊難敵，官與使事兩來源直接接合，不因字形另人；無卒年。',7505:'甘卬甘卓兄子、王敦參軍回勸甘卓，甘卓遇害不作卬死；原文省甘姓可由叔侄關係核明。'}.items():results[ident]['unresolved'].append(note)
    zhang322=chunk_ev('sl_00','曜卫军将军呼延瑜逆击斩之','十六國春秋·光初五年322','所斬為陳安弟集及將張明等，並列將軍本人，非只敗軍')
    results[7486].update(historical_death=date(322,zhang322),review_status='primary_text_corroborated')
    results[7486]['evidence'].append(zhang322)
    dai345=chunk_ev('js_11','翼长史江[A170]、司马朱焘、将军袁真等共诛之','晉書·庾翼傳·永和元年345翼卒後未幾','上句明幹瓚戴羲叛，非只誅一無名黨眾')
    results[7489].update(historical_death=date(345,dai345),review_status='primary_text_corroborated')
    results[7489]['evidence'].append(dai345)
    results[7493]['evidence'].append(dict(citation='CBDB408897 original fields read',excerpt=json.dumps(originals[7493]['cbdb_originals'],ensure_ascii=False)))
    # Cao Ju's early unlocated career link and name-only CBDB stay outside the
    # well-attested Yuzhou/Xiyang commander's death branch.
    cao345=chunk_ev('tj_16','杀冠军将军曹据','資治通鑑·永和元年345庾翼卒後叛亂','庾翼部下曹據為叛軍所殺')
    for ch,phrase in [('js_11','西阳太守曹据'),('sl_09','西阳太守曹据'),('tj_15','滕含部将曹据抱帝'),('tj_16','杀冠军将军曹据')]:results[7490]['evidence'].append(chunk_ev(ch,phrase,'曹據各鍵身份原文核對','329滕含部將與庾翼西陽太守之履歷連結尚未明文取得'))
    results[7490].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=False,identity_status='cao_ju_primary_career_link_and_cbdb_unresolved',historical_birth=None,historical_death=None,last_manual_review_batch='round8b',component_identities=[dict(identity='329滕含部將曹據抱成帝出石頭'),dict(identity='庾翼西陽太守冠軍將軍曹據',historical_death=date(345,cao345)),dict(identity='CBDB404637姓名空履歷支')])
    results[7490]['evidence'].append(dict(citation='CBDB404637 original row',excerpt=json.dumps(originals[7490]['cbdb_originals'],ensure_ascii=False)))
    for ch,phrase in [('js_17','主簿王振以洋为妖'),('hy_01','弟振，字仲远')]:results[7503]['evidence'].append(chunk_ev(ch,phrase,'王振候選範圍核查','祖約主簿與王化弟巴東守兩支，原文省姓名不憑章內鄰字識別'))
    results[7503].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='zu_yue_clerk_and_wang_hua_brother_unlinked',historical_birth=None,historical_death=None,last_manual_review_batch='round8b',component_identities=[dict(identity='322祖約主簿王振，獲戴洋救赦'),dict(identity='王化弟王振字仲遠，廣都令巴東守')])
    results[7503]['unresolved'].append('兩條候選本身分不同官事、前者無籍貫父兄，尚未取得連結；同名可能同人不強斷兩人，待核而不套年代。')
    for ch,phrase in [('js_11','毒酒饷江州刺史王允之'),('js_12','允之字深猷'),('js_18','王舒遣子允之为督护'),('sl_01','建武将军王允之戍芜湖'),('tj_14','王敦从子允之'),('tj_15','扬烈将军王允之'),('tj_16','江州刺史王允之'),('js_19','扬烈将军王允之'),('ssxy_03','按诸书皆云王允之事')]:results[7501]['evidence'].append(chunk_ev(ch,phrase,'王允之九原書脈絡實讀','王舒子字深猷；世說主文羲之注校允之，不能把主文錯名另外套羲之生卒'))
    yun_death=evidence(SOURCE/'_cache/texts/晋书.txt','冬十月甲午，卫将军王允之卒','晉書·康帝紀·咸康八年342十月甲午','帝即位年同咸康八，下一段才建元元年343')
    yun_age=chunk_ev('js_12','未到，卒，年四十','晉書·王舒傳附允之','卒年342虛歲40反推303；未到會稽任即卒，非任滿數年')
    results[7501].update(review_status='primary_text_corroborated',identity_status='primary_wang_shu_son_identified_cbdb_name_only_unlinked',source_identities_all_adjudicated=False,historical_birth=date(303,yun_age,age=40),historical_death=date(342,yun_death,month=10,day_ganzhi='甲午'),date_scope='nine named primary contexts only; CBDB413890 contains only name and no identifying career fields',last_manual_review_batch='round8b')
    results[7501]['historical_death'].update(precision='lunar_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7501]['evidence'].extend([yun_death,yun_age,dict(citation='CBDB413890 original row read',excerpt=json.dumps(originals[7501]['cbdb_originals'],ensure_ascii=False))])
    results[7501]['unresolved'].append('sl01建武元為後趙石虎紀年335，不能誤映晉元帝建武317；所記建武將軍又是官號。CBDB空履歷身份支仍待聯結，九原文日期不直接套整條。')
    round8c_reads={7506:[('js_01','魏乂、将军李恒、田嵩')],7507:[('js_08','默后为石匆所败')],7508:[('js_13','宣谓祖智曰'),('js_17','使兄子智与延追之'),('sl_01','使兄子智与延追之'),('tj_15','其从子智、衍并劝成之'),('js_19','从子智及衍并倾险好乱')],7509:[('tj_14','都尉秦康'),('js_10','都尉秦康')],7510:[('tj_14','罗英至广州约陶侃同进'),('js_10','参军罗英至广州')],7511:[('tj_14','遣参军虞冲与骞'),('js_10','参军邓骞、虞冲至长沙')],7512:[('sl_01','执将军卫荣而归')],7514:[('js_18','诸葛瑶、邓岳、周抚'),('tj_14','诸葛瑶、邓岳、周抚'),('wsh_02','与其将诸葛瑶等纵酒淫逸')],7515:[('tj_14','李恒、谢雍为爪牙'),('js_18','李恒、谢雍为爪牙')],7516:[('sl_03','天水赵俱陇西牛夷北地辛牢'),('sl_04','光禄大夫赵俱为洛州刺史'),('tj_17','天水赵俱、陇西牛夷'),('sl_05','赵俱南安羌酋也一云天水人')],7517:[('sl_00','将军刘烈赵罕袭汧城')],7518:[('js_09','武昌太守邓岳驰白'),('js_11','刺史邓岳留不听去'),('js_13','邓岳，字伯山，陈郡人'),('js_18','诸葛瑶、邓岳、周抚'),('sl_09','广州刺史邓岳将兵击宁州'),('tj_14','陈郡邓岳收'),('tj_15','邓岳守西城'),('tj_16','广州刺史邓岳将兵击汉宁州'),('wsh_02','钱凤、邓岳、周抚'),('js_07','我与伯山同亡'),('js_10','遣邓岳、缪坦收若思'),('sl_03','邓伯山昔送此犀皮两裆铠')],7519:[('js_08','遣参军郑雄诣矩谋之')],7520:[('js_15','茂谓参军陈珍'),('sl_00','平虏将军陈珍'),('sl_08','问于参军陈珍'),('tj_14','茂谓参军陈珍')],7521:[('js_12','与振威将军陶回'),('js_13','陶回，丹阳人'),('tj_15','司徒司马丹杨陶回')],7522:[('js_12','宣城内史陆喈')]}
    for ident,readings in round8c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第8批C·原文官事家屬逐鍵審讀','Read original person/event passage; source key scope respected') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident not in [7509,7511],identity_status='all_listed_primary_source_identities_reviewed',last_manual_review_batch='round8c')
    for ident in [7509,7511]:
        results[ident]['evidence'].append(dict(citation='Original CBDB BIOG_MAIN source row inspected',excerpt=json.dumps(originals[ident]['cbdb_originals'],ensure_ascii=False)))
        results[ident]['identity_status']='two_primary_contexts_agree_cbdb_name_only_unlinked'
        results[ident]['unresolved'].append('CBDB原行僅名，日期/注/官履均空，不能以同名自動核全keys。')
    for ident,note in {7506:'田嵩王敦將322攻譙王承，同前趙田崧無履歷連結，不按同音合；未記生卒。',7507:'石匆為石勒養子襲郭默，原文實作匆，可能石聰異文待跨來源核，不能把郭默/李矩之卒移給他。',7508:'祖智為祖約兄/從子，追敵僅以身免、後勸叛蘇峻；五來源親屬主帥脈絡一致，士卒皆死不包括智本人。',7509:'甘卓都尉秦康勸斷彭澤，樂道融憂死不套秦康。',7510:'甘卓參軍羅英使廣州約陶侃，兩來源同使事，未記生卒。',7511:'甘卓參軍虞衝與鄧騫赴長沙，虞望戰死不是虞衝；未知生卒。',7512:'衛榮被石他俘歸，隨後大疫死者十分二三非直接明言本人，未知生卒。',7514:'諸葛瑤同王敦爪牙與王應秘喪飲宴，三書同一集團；王敦324死不等於瑤死。',7515:'謝雍同王敦爪牙，未記其生卒。',7516:'趙俱南安羌酋一云天水，從苻洪從事中郎、苻健河內太守光祿大夫洛州刺史，後苻生命尚書令辭而憂卒，四書官遷直接連結；即位後憂卒未給獨立紀年，不把355一概作明載卒年。',7517:'陳安將趙罕同劉烈襲汧城，前文斬陳集張明不套本人，未記生卒。',7518:'鄧岳字伯山陳郡、後改岱，十二鍵同王敦參軍/逃西陽蠻/平蘇峻/廣州刺史；據字連邓伯山，殺周戴不是他本人死。本人傳有卒但未明年，不能直接套337出夜郎或339伐寧州年；熊遠後病卒等鄰文亦不套。',7519:'郭默參軍鄭雄往問李矩，未記生卒，默後被殺不套。',7520:'張茂參軍陳珍救韓璞抗劉曜，四來源同事同官；兵力1500/1800異文不拆人，不推作卒年。',7522:'顧眾為宣城內史陸喈辯解王敦之怒，未明陸喈被殺，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    tao_bad=chunk_ev('js_13','导深愧之咸和二年','晉書·陶回傳本地轉錄','此年327與上文蘇峻平後、吳興四年明顯衝突，不硬作可信卒年')
    results[7521]['evidence'].extend([tao_bad,dict(citation='晉書卷078陶回·維基文庫oldid1506266',url='https://zh.wikisource.org/w/index.php?title=晉書/卷078&oldid=1506266',excerpt='蘇峻之役...以功封康樂伯...在郡四年...咸和二年，以疾辭職...未拜，卒，年五十一。',interpretation='Original parallel transcription also reads咸和二年, not an independently verified emendation to咸康二年; date remains withheld.')])
    results[7521]['source_chronology_conflicts']=[dict(source='晉書陶回傳',reported_resignation_year=327,reported_death_age=51,conflict='Same biography places these after328–329蘇峻 war and four years吴兴; cannot resolve by silently changing和 to康',parallel_transcription='Wikisourceoldid1506266 also咸和二年')]
    results[7521]['unresolved'].append('完整傳已讀，末咸和二年327辭官後卒年51與前述平蘇峻及在郡四年相矛盾。另本原文也作和，不擅校康/336，不以未定卒年反推生日；待版本校勘。')
    for ident,readings,components,note in [
        (7523,[('js_18','靳康女者'),('sl_00','立妻靳氏为皇后'),('tj_13','立其妻靳氏为皇后')],[dict(identity='靳康女，劉曜誅族欲納妾，拒絕並救弟'),dict(identity='劉粲靳皇后318冊立')],'兩支不同親屬與宮廷位置，不能同姓靳便合。sl00原候選明劉粲妻，不能把同卷靳月光315自殺誤納此key。'),
        (7524,[('js_08','韩据女为匹䃅儿妾'),('sl_01','韩据女为匹䃅儿妾'),('sl_04','纳遇继母韩氏为昭仪'),('tj_17','秦主健纳张遇继母韩氏为昭仪'),('js_03','谓妻韩氏曰'),('ssxy_02','山妻韩氏觉公与二人')],[dict(identity='韓據女、段匹磾兒妾'),dict(identity='張遇繼母、苻健韓昭儀'),dict(identity='山濤妻韓氏，識嵇康阮籍')],'三書候選支確實不同夫/父，必須拆韓氏女性，而非同卷另一名字產生的假hold。'),
        (7525,[('js_17','韩绩，字兴齐，广陵人'),('tj_13','殿中将军韩绩'),('js_10','殿中将军韩绩')],[dict(identity='廣陵嘉興隱士韓績字興齊、韓建子，咸康末拒博士'),dict(identity='318殿中將軍韓績奉命撤御座')],'隱士傳少以潛退為操不交當世，與宮廷殿中將軍未見履歷橋；可能轉隱但不先認同。所讀通鑑實元帝318即位不是317，勿將晉王/皇帝兩次進位混。')]:
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第8批C·候選明確多身份核查','Actual candidate subjects and primary originals both distinguish these branches') for ch,phrase in readings)
        results[ident].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='distinct_or_unlinked_same_name_candidate_branches',historical_birth=None,historical_death=None,component_identities=components,last_manual_review_batch='round8c')
        results[ident]['unresolved'].append(note)
    round9a_reads={7526:[('tj_14','西曹书佐韩阶'),('js_15','韩阶，长沙人')],7527:[('sl_06','晋冠军将军髙季遣将邀击之'),('tj_17','襄攻冠军将军高季于外黄')],7528:[('js_01','南蛮校尉魏乂'),('tj_14','从母弟南蛮校尉魏乂'),('wsh_02','从母弟南蛮校尉魏乂'),('js_10','承为魏乂所败'),('js_15','遣其将魏乂围承'),('js_16','及魏乂来攻'),('ssxy_03','从母弟魏乂攻丞')],7529:[('js_17','约将鲁延求追贼'),('sl_01','约将鲁延请追之')],7530:[('sl_02','匈奴刘务桓入贡'),('tj_16','虎卒，子务桓立'),('tj_17','匈奴大人刘务桓卒'),('wsh_00','十九年春正月，刘务桓死'),('wsh_02','务桓，一名豹子'),('sl_08','务桓一名豹子')],7531:[('sl_00','平西将军厚攻杨曼'),('sl_09','镇西将军刘厚追击之'),('tj_14','镇西将军刘厚追击之')],7532:[('sl_01','东莱太守刘巴长广太守吕披')],7533:[('sl_00','右军将军刘干攻平襄克之'),('tj_14','右军将军刘干攻平襄')],7534:[('sl_00','敞为齐王'),('tj_13','敞为齐王')],7535:[('sl_00','冲为淮南王'),('tj_13','冲为淮南王')]}
    for ident,readings in round9a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第9批A·同役及家屬原文審讀','Actual named office/family/date context read') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_listed_primary_contexts_read_and_adjudicated',source_identities_all_adjudicated=True,last_manual_review_batch='round9a')
    for ident,note in {7526:'韓階與武延送譙王承喪後還，桓雄被害/承被殺都非韓階本人卒。',7527:'髙/高字形差同外黃冠軍將軍，春秋先記他遣將敗襄，通鑑續稱會季卒，不是被姚襄斬；不能改成戰死。',7528:'七來源同王敦從母弟魏乂，322攻譙王承臨湘、殺虞悝等並任鄧騫別駕；被殺者諸臣不是魏乂，所讀無本人卒年。',7529:'魯延與祖智追趙兵僅身免，士卒皆死不含將本人，兩原文同人，無生卒。',7530:'劉虎子務桓一名豹子，劉衛辰父、悉勿祈父、閼陋頭兄，六來源父兄官爵同人；平北左/右賢王異文保留，不拆兩人。',7531:'劉厚為劉曜將，平西攻楊曼至鎮西追楊難敵，同軍持續履歷；後文李琀李稚死非厚死。',7532:'東萊太守劉巴323降石虎，不與蜀漢尚書令僅同名合；被殺曹嶷、坑士眾不直接證他卒。',7533:'劉曜右軍將軍劉幹/干攻平襄，兩書同人，陳安被斬不套其卒。',7534:'劉曜子敞封齊王319，原文冊封在世可確，不以所處亂世擬定生卒；受王爵並不等於出仕起年。',7535:'劉曜子衝封淮南王319，與劉敞/劉熙諸子分列，封爵不作生日或擅設首仕年。'}.items():results[ident]['unresolved'].append(note)
    gao355=chunk_ev('tj_17','会季卒，襄进据许昌','資治通鑑·永和十一年355五月','本人明言卒，未說戰死')
    results[7527].update(review_status='primary_text_corroborated',historical_death=date(355,gao355,month=5))
    results[7527]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7527]['evidence'].append(gao355)
    wuhuan356=chunk_ev('wsh_00','十九年春正月，刘务桓死','魏書·序紀·昭成建國十九年356正月','通鑑永和十二年356年初同死弟閼頭代立')
    results[7530].update(review_status='primary_text_corroborated',historical_death=date(356,wuhuan356,month=1))
    results[7530]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7530]['evidence'].append(wuhuan356)
    for ident in [7534,7535]:results[ident]['attested_activity_years']=[319]
    round9b_reads={7536:[('sl_00','封子袭为长乐王'),('tj_13','封子袭为长乐王')],7538:[('sl_00','髙为鲁王'),('tj_13','高为鲁王')],7539:[('sl_01','长广太守吕披')],7540:[('tj_14','赵辅威将军呼延青人')],7541:[('js_13','逊使将军姚崇、爨琛距之'),('sl_09','姚岳[一作崇下同]'),('tj_14','使将军姚岳等拒之')],7542:[('sl_02','宇文大人悉独官'),('tj_13','宇文大人悉独官'),('sl_03','悉独官虽拥大众')],7543:[('sl_00','宋亭斩赵募以上邽降'),('tj_14','宋亭斩赵募，以上邽降')],7544:[('sl_08','宛戍都尉宋矩矩乃自刎'),('sl_09','宋矩字处规'),('tj_16','宛戍都尉敦煌宋矩'),('js_15','宛戍都尉宋矩'),('js_16','宋矩，字处规')],7545:[('sl_00','将军平先丘中伯'),('tj_14','遣将军平先等追之')]}
    for ident,readings in round9b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第9批B·原書實讀同役與字形異文','Original named subject and death target checked') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_named_source_contexts_reviewed',source_identities_all_adjudicated=True,last_manual_review_batch='round9b')
    for ident,note in {7536:'劉曜子襲319封長樂王，在世封爵不作出生或首仕年；生卒不詳。',7538:'劉曜子高/髙319封魯王，與劉襲閶衝敞分列，不以同家族死亡概述套本人卒年。',7539:'呂披長廣太守323降趙，曹嶷被殺非呂披死；無生卒。',7540:'呼延青人為殺石容陳安之將，春秋呼延清為異文候選但原名未獨立查全不硬合；殺人者非被殺者，未記卒年。',7541:'姚崇/岳由原文一作崇直連同螗螂之役，不與唐相姚崇合。王遜怒而卒、蜀兵渡水溺死皆非姚岳，鞭打不作本人死亡。',7542:'宇文悉獨官319棘城敗僅身免、俘的是其眾，三來源同人；不以主將敗陣作卒。',7543:'宋亭斬趙募降前趙，死者是趙募非宋亭，無生卒。',7544:'敦煌宋矩字處規宛戍都尉、先殺妻子自刎，五來源同人。通鑑永和二346與春秋張重華即位後未幾一致，不把下一年麻秋再攻重複敘事硬當另一卒年。',7545:'平先奪陳安矛追之，陳安後為呼延青人斬，非平先死；雙原文同將軍，未記生卒。'}.items():results[ident]['unresolved'].append(note)
    song346=chunk_ev('tj_16','先杀妻子而后自刎','資治通鑑·永和二年346宋矩死節','same sentence subject 宋矩，不是鄰文翌年軍役')
    results[7544].update(review_status='primary_text_corroborated',historical_death=date(346,song346))
    results[7544]['evidence'].append(song346)
    for ch,phrase in [('sl_00','阐为太原王'),('tj_13','阐为太原王'),('sg_10','权复以璋子阐为益州刺史')]:results[7537]['evidence'].append(chunk_ev(ch,phrase,'劉闡兩父系原文核查','劉曜子太原王與劉璋子吳益州刺史/御史中丞不同人'))
    results[7537].update(review_status='identity_conflict',identity_status='merged_liu_yao_son_and_liu_zhang_son_liu_chan',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,last_manual_review_batch='round9b',component_identities=[dict(identity='前趙劉曜子闡，319太原王',keys=['sl:sl_00:356:劉闡','tj:tj_13:321:劉闡']),dict(identity='劉璋子闡一名緯，孫吳益州刺史/御史中丞',keys=['sg:sg_10:252:劉闡'])])
    results[7537]['unresolved'].append('三國劉璋子疾終於家沒有年，前趙劉曜子封王不是生日；兩父系原文均明，不可混。')
    round9c_reads={7546:[('sl_00','太史令弁广明言于曜')],7547:[('sl_03','留世子晔守龙城'),('tj_17','留世子晔守龙城')],7548:[('sl_01','河西鲜卑日六延叛勒'),('tj_13','击鲜卑日六延于朔方')],7549:[('tj_16','以其将李弈为前锋'),('wsh_02','太保李弈击广于涪城')],7551:[('hy_00','太守李谦'),('sl_09','太守李谦')],7552:[('sl_00','太常梁胥等缮复之')],7553:[('sl_00','杨伯支斩姜冲儿'),('tj_14','杨伯支斩姜冲儿')],7554:[('sl_09','将军乐次费佗李干')],7555:[('sl_01','参军樊坦清贫'),('tj_14','参军樊坦为章武内史'),('sl_02','樊坦京兆人仕勒为参军')]}
    for ident,readings in round9c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第9批C·原文本人段落審核','Actual name/role context read; speculative exact years not introduced') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_primary_contexts_read',source_identities_all_adjudicated=ident!=7546,last_manual_review_batch='round9c')
    for ident,note in {7546:'原字串太史令弁廣明言於曜，弁廣明/弁廣+明言分詞需另一版本或別處姓名核實，不把程式抽取的三字名視為已決。無生卒。',7548:'日六延朔方敗，斬二萬是士卒概述未點本人，不能當其人319卒；兩來源同河西鮮卑首領。',7551:'同成漢越巂太守李謙被斯叟圍後徙民，兩來源同任回同事。華陽太寧元323與春秋玉衡十七327的起事年代不同，保留活動年歧異，不推為生卒。',7552:'劉曜太常梁胥與鎮軍將軍襲修祖墓，天災不是本人卒年，未知生卒。',7553:'楊伯支殺姜沖兒以陇城降，死者非伯支，雙來源同人，無生卒。',7554:'樂次從李琀稚攻下辨，原文末深入無繼皆殺語緊接琀稚，不把全前列將名一律當明載同卒；更不能套後文李玝347戰死。',7555:'京兆樊坦參軍章武內史三來源同人，誤稱羯賊令勒笑而賜財不罪，非忌諱致誅，未記卒年。'}.items():results[ident]['unresolved'].append(note)
    results[7546]['identity_status']='original_role_read_name_segmentation_pending'
    ye356=chunk_ev('sl_03','秋七月丙子太子晔卒','十六國春秋·元璽五年356七月丙子','慕容儁長子世子晔→353皇太子，通鑑永和十二同卒日')
    results[7547].update(historical_death=date(356,ye356,month=7,day_ganzhi='丙子'),review_status='primary_text_corroborated',identity_status='murong_jun_crown_prince_ye_two_sources_adjudicated')
    results[7547]['historical_death'].update(precision='lunar_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7547]['evidence'].extend([ye356,chunk_ev('tj_17','丙子，燕献怀太子晔卒','資治通鑑·永和十二年356七月','與前燕錄同太子及卒日')])
    results[7549].update(duplicate_of=7400,review_status='duplicate',identity_status='li_yi_yi_variant_same_cheng_han_taibao_and_guang_attacker',source_identities_all_adjudicated=True)
    results[7549]['unresolved'].append('與7400李奕同李壽前鋒謀臣/太保、攻弟廣于涪城及女入李勢后宮之獨特事，四書全讀，可歸并，不只按名字音同。卒346在canonical保留。')
    li384=originals[7550]['cbdb_originals']
    results[7550]['evidence'].append(dict(citation='CBDB192791 original database row reread',excerpt=json.dumps(li384,ensure_ascii=False),interpretation='c_index_year321 is not birth. Notes explicitly Death year estimated by Tackett(cc384), dy_range2; bibliography noted but original epitaph not yet acquired.'))
    results[7550].update(review_status='secondary_source_checked',identity_status='anonymous_li_original_database_estimate_not_epitaph_corroborated',source_identities_all_adjudicated=False,historical_birth=None,last_manual_review_batch='round9c')
    results[7550]['unresolved'].append('約384卒只原庫Tackett估算，原拓片/墓誌補遺7:138-139文物1999.1:93洛陽新獲續504尚未取得；不能記精確384，index321非出生。')
    round9d_reads={7556:[('js_08','参军殷峤')],7557:[('sl_09','斩秋将杜勲汲鱼'),('tj_16','斩其将杜勋、汲鱼'),('js_15','斩秋将杜勋、汲鱼')],7558:[('js_13','将军姚崇、爨琛'),('sl_09','姚岳[一作崇下同]爨琛')],7559:[('sl_06','以太原王亮为长史'),('tj_17','以太原王亮为长史')],7560:[('js_13','州人复立逊中子坚'),('tj_14','宁州刺史王坚不能御寇')],7563:[('js_08','石良率精兵五千袭矩'),('sl_01','石良帅精骑五千掩袭矩营')],7564:[('sl_08','斩秋将綦母安'),('wsh_04','艾击斩秋将綦母安')],7565:[('sl_00','路松多起兵于新平扶风'),('tj_13','屠各路松多起兵于新平'),('tj_14','路松多奔陇城'),('sl_01','刘牙赵牢路松多等坚戍不已')]}
    for ident,readings in round9d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第9批D·分支名義與原文事件核讀','Book actually read; whole-book homonyms not automatically assigned to candidate') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='listed_original_primary_contexts_reviewed',source_identities_all_adjudicated=ident!=7565,last_manual_review_batch='round9d')
    for ident,note in {7556:'殷嶠郭默參軍受解印，郭默南奔以及後被誅均非殷嶠卒，未記生卒。',7558:'爨琛與姚崇/岳戰堂狼，王遜怒死及蜀兵溺死不套爨琛；無生卒。',7559:'王亮兩key候選都明姚襄長史太原人，不因同卷370慕容令故事有渤海王亮而製造假混人。永和十二356本人諫攻洛陽後俄卒。',7560:'王堅王遜中子寧州刺史，太寧末325被徵返後病卒未獨立明年，不能把徵命年直接當卒年；兩書同父子官職。',7563:'石良石勒將襲李矩執郭誦弟，兩來源同役，李矩/郭默後亡不套此人。',7564:'綦母安為張重華即位後謝艾首戰斬麻秋將，346；不同於下一年再戰的杜勛汲魚，不因謝艾多次獲勝把所有戰死都放347。',7565:'路松多319附司馬保、320奔陇城與323陳安陇城小將同名同地可能轉屬，尚缺直接履歷連結；前後四段均已讀，留身份橋待補，不把陳安本人死強套。'}.items():results[ident]['unresolved'].append(note)
    fish347=chunk_ev('tj_16','斩其将杜勋、汲鱼','資治通鑑·永和三年347四月枹罕之戰','汲魚本人並列斬名；屬謝艾第二次大戰，不同首戰綦母安346')
    results[7557].update(review_status='primary_text_corroborated',historical_death=date(347,fish347,month=4))
    results[7557]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7557]['evidence'].append(fish347)
    wang356=chunk_ev('sl_06','俄而亮卒襄哭之甚恸','十六國春秋·永和十二年356洛陽之役','姚襄長史王亮，非同卷後燕370龍城王亮')
    results[7559].update(review_status='primary_text_corroborated',historical_death=date(356,wang356))
    results[7559]['evidence'].append(wang356)
    qimu346=chunk_ev('sl_08','斩秋将綦母安','十六國春秋·張重華·永和二年346初立後之役','下一年永和三年四月再攻另有杜勛汲魚，勿合兩次戰役')
    results[7564].update(review_status='primary_text_corroborated',historical_death=date(346,qimu346))
    results[7564]['evidence'].append(qimu346)
    for ch,phrase in [('js_15','临洮人翟楷、石琮'),('sl_00','临洮人翟楷石琮'),('sl_08','临洮人翟楷石琮'),('sl_02','柳城都尉石琮'),('tj_13','琮，鉴之孙也'),('tj_15','柳城都尉石琮')]:results[7562]['evidence'].append(chunk_ev(ch,phrase,'石琮各key身份範圍實核','候選本身分臨洮附趙與前燕石鑑孫柳城都尉，不單因同卷同名判混'))
    results[7562].update(review_status='identity_conflict',identity_status='lintao_rebel_and_former_yan_shi_jian_grandson_unlinked',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,last_manual_review_batch='round9d',component_identities=[dict(identity='323臨洮石琮與翟楷逐令長附劉曜'),dict(identity='319由遼東歸棘城石鑑孫石琮、334柳城都尉')])
    results[7562]['unresolved'].append('兩地相去遠且319先入燕/323西方再活動/334又柳城的履歷未明，不能單憑同名承接；三涼書與三燕書各支均已實讀。')
    round9e_reads={7566:[('sl_08','县令炖煌车济守节不屈'),('sl_09','车济字万度'),('tj_16','县令敦煌车济不降'),('js_16','车济，字万度')],7567:[('js_08','郭诵弟元复为贼所执'),('sl_01','郭诵之弟元')],7568:[('js_04','裕字思旷'),('js_06','东阳太守阮裕'),('js_12','阮裕尝戏之'),('js_13','陈留阮裕有重名'),('ssxy_00','裕字思旷陈留尉氏人'),('ssxy_01','阮光禄'),('ssxy_02','阮裕少有徳行'),('ssxy_03','阮思旷奉大法敬信甚至')],7569:[('sl_00','太子太傅韩广'),('tj_14','太子太保韩广')],7570:[('sl_00','将军平先丘中伯')],7571:[('js_11','暨阳人任谷')],7572:[('sl_09','政事皆委于寿及司徒何点'),('tj_15','政事皆委于寿及司徒何点')],7573:[('hy_00','参军傅颖、治中张淳'),('sl_08','初遣参军傅颖'),('sl_09','参军傅颕治中张淳'),('js_15','骏遣傅颖假道于蜀')],7574:[('jk_01','诏御史刘彜徃芜湖'),('wsh_02','遣御史刘彝发敦瘗')],7575:[('js_15','从事刘庆谏曰'),('sl_08','从事刘庆諌曰'),('tj_14','从事刘庆谏曰')]}
    for ident,readings in round9e_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第9批E·原書及字形別名逐鍵審讀','Actual person context and neighboring event subjects checked') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_listed_original_primary_identities_adjudicated',source_identities_all_adjudicated=True,last_manual_review_batch='round9e')
    for ident,note in {7567:'郭元為郭誦弟李矩部下，被石良俘作書勸降，原文省姓非另一無名者；沒有被殺記錄，未知生卒。',7568:'阮裕字思曠陳留尉氏、阮主簿/光祿、東陽臨海守八鍵同人。完整傳卒年62，世說引別傳61，有享年異文但均未明卒年，不能固定一年倒推生日。世說子牖/本傳佣名異另保留，不套兒子早卒給父。',7569:'韓廣為劉曜太子太傅/太保諫換劉熙，同卜泰之奏兩書同人；不與秦末燕王同名合，無生卒。',7570:'丘中伯與平先追陳安為本人實名，近文陳安死非中伯死，未載生卒。',7571:'任谷暨陽自稱道術，郭璞諫不宜禁內，同一敘事人物可定位；孕蛇等異聞不當可驗生理或生卒年月史證，未載生卒。',7572:'成漢司徒何點輔李班、為班所師兩原書同人，不與梁隱士何點因同名合；無生卒。',7573:'傅穎/颕前涼張骏參軍使成漢請假道，四來源同使與張淳分清，李雄/他使被殺不套其卒。春秋初使列年有前後複敘，非生卒證據。',7574:'劉彝/彜324為御史掘王敦屍行刑，死者是敦而非彝；兩原書同任務，古字形不代表未有本人。',7575:'劉慶張骏從事諫冬討辛晏，三書同奏；沒有本人卒年，也不與劉曜太子同姓混。'}.items():results[ident]['unresolved'].append(note)
    che346=chunk_ev('tj_16','县令敦煌车济不降，伏剑而死','資治通鑑·永和二年346麻秋金城之役','本人拒降死節，同宋矩前後而非後年枹罕再攻')
    results[7566].update(review_status='primary_text_corroborated',historical_death=date(346,che346))
    results[7566]['evidence'].append(che346)
    results[7568]['reported_nominal_death_age_alternatives']=[dict(age=62,source='晉書阮裕本傳'),dict(age=61,source='世說新語注引阮光祿別傳')]
    # An additionally encountered narrative misattributes a father/son episode.
    results[7518]['evidence'].append(chunk_ev('jk_01','临阵斩前锋何康邓岳等','建康實錄·太寧二324','此新增旁證稱陣斬鄧岳，注又混岳/其子遐斬蛟事；與12鍵本人後履不合，另列文獻問題，不憑一支擅改本人卒324'))
    results[7518]['source_chronology_conflicts']=[dict(source='建康實錄jk01',excerpt='临阵斩前锋何康邓岳等...岳陈郡人也字子遐...襄阳城北...入水截蛟',problem='邓岳本人传324逃免、后历广州339在职；截蛟实际属其子邓遐字应远，注人名/父子混淆待校本')]
    round10a_reads={7576:[('sl_01','彭城内史刘续续败遁走'),('tj_14','彭城内史刘续复据兰陵石城')],7577:[('sl_01','败晋将军刘长')],7579:[('sl_00','推巴归善王句渠知为主'),('tj_14','推巴酋句渠知为主')],7580:[('yi_01','建平夷王向弘')],7581:[('js_18','吴儒斩沈充'),('tj_14','误入故将吴儒家'),('wsh_02','沈充将吴儒斩充'),('js_16','为部曲将吴儒所杀'),('sl_03','后竟杀儒以报仇'),('ssxy_02','吴儒斩首于京都')],7582:[('js_18','语参军吕宝曰'),('tj_14','顾谓参军吕宝曰'),('wsh_02','语参军吕宝曰')],7583:[('js_12','李闳、曹统、周光与默并力'),('js_18','周光斩钱凤'),('tj_14','寻阳太守周光闻敦举兵'),('js_07','光少有父风')],7584:[('js_08','谟以𫖮故，频居显职'),('js_18','阿奴，谟小字也'),('ssxy_01','周谟字叔治')],7585:[('js_08','取𫖮息闵'),('sl_06','以吏部尚书周闵为中军将军'),('tj_17','以吏部尚书周闵为中军将军'),('js_04','御史中丞周闵奏裕'),('js_10','闵字子骞')]}
    for ident,readings in round10a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第10批A·候選角色與本人原文審讀','Original passages actually read, death subjects separated from neighboring victims') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_listed_primary_contexts_read_and_adjudicated',source_identities_all_adjudicated=ident!=7584,last_manual_review_batch='round10a')
    for ident,note in {7576:'劉續324彭城內史敗走至326復據蘭陵石城，兩原文同官地；遁走/城拔未明本人卒。',7577:'晉將軍劉長敗於石瞻，敗非卒，未記生卒。',7579:'句渠知320為巴人所推歸善王稱大秦平趙；後句氏宗黨五千被攻滅並非直接點本人死，不據群體語定卒。',7580:'南齊書回述晉太興三320建平夷王向弘請拜，非南齊活動；同任賜折衝將軍當平鄉侯親晉王，不以320作出生或卒年。',7581:'吳儒324殺沈充，後沈勁報父仇殺儒但未明年；不能把324沈充死套吳儒。世說注吳儒斬首於京都主語是吳儒送沈充首，非吳儒被斬。六來源同沈充故將。',7582:'呂寶三候選明王敦參軍324病榻言語，不把同晉書片後文涼呂光左丞呂寶納此key；死者王敦而非寶，無本人生卒。',7583:'周光為周訪子周撫弟、尋陽太守，捕錢鳳後隨溫嶠平蘇峻，四書身份可直接聯；年11見王敦沒有獨立事件年不反推生日，卒官未明年。',7584:'周謨字叔治顗嵩弟、阿奴小字三原文互證；卒贈金紫光祿大夫未明年。CBDB135464僅名、日期注官履皆空，不能把此key默認全核。',7585:'周閔字子騫周顗子，周謨請其襲父爵，354中軍/355左僕射，五來源官履親屬同人；傳記卒未明年，父顗死不套子，也不把354任職當卒。'}.items():results[ident]['unresolved'].append(note)
    results[7580]['attested_activity_years']=[320]
    results[7584]['evidence'].append(dict(citation='Original CBDB135464 row inspected',excerpt=json.dumps(originals[7584]['cbdb_originals'],ensure_ascii=False),interpretation='Name only; no independent identity bridge in empty date/office/note fields'))
    for ch,phrase in [('js_18','朱蕃使督护刘雄戍于日南'),('sl_09','遂陷日南杀督护刘雄'),('tj_16','杀督护刘雄'),('yi_02','后遣督护刘雄戍日南')]:results[7578]['evidence'].append(chunk_ev(ch,phrase,'劉雄·日南原文及蜀事段落錯置問題','All four candidate scopes read; Cheng-Han candidate explicitly says killed at Chengdu but its underlying original unexpectedly inserts 日南'))
    xiong347=chunk_ev('tj_16','秋，七月，林邑复陷日南，杀督护刘雄','資治通鑑·永和三347七月','Date only assigned to explicitly 日南朱蕃所遣督護 branch')
    results[7578].update(review_status='identity_conflict',identity_status='nanai_commander_and_dislocated_cheng_han_candidate_unresolved',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,last_manual_review_batch='round10a',component_identities=[dict(identity='交州朱蕃遣戍日南之督護劉雄',historical_death=date(347,xiong347,month=7)),dict(identity='春秋成漢范賁傳候選稱成都被殺劉雄，底本卻夾入日南條，需校簡')])
    results[7578]['component_identities'][0]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7578]['evidence'].append(xiong347)
    results[7578]['unresolved'].append('成漢范賁段成都/德陽敘事中突接遂陷日南殺督護劉雄，疑錯簡/混入交州紀事；不是逕斷有兩位劉雄。候選本身依此作成都被殺，未校定前不把日南347卒套整條。')
    round10b_reads={7586:[('jk_01','义兴人周骞杀敦所置太守刘芳')],7587:[('sl_00','呼延清[纪事本末作呼延青人]')],7588:[('js_18','害太守夏侯览'),('tj_16','日南太守夏侯览贪纵'),('yi_02','台遣夏侯览为太守')],7589:[('hy_00','参军傅颖、治中张淳'),('sl_08','又遣治中从事张淳'),('sl_09','傅颕治中张淳修好于雄'),('tj_15','遣治中从事张淳称籓于成'),('js_15','遣治中从事张淳称籓于蜀')],7590:[('sl_01','托候部掘咄哪')],7591:[('js_18','中军司马曹浑'),('tj_14','中军司马曹浑'),('wsh_02','中军司马曹浑')],7592:[('sl_01','典农使者朱表')],7594:[('sl_09','李弇字子完'),('js_15','祖弇，仕张轨'),('sl_10','祖弇[音奄]')],7595:[('sl_09','永和三年从征西至于山阳战死'),('tj_14','征东将军李寿及琀弟玝'),('tj_15','以李玝为征北将军')],7597:[('sl_09','巴郡太守杨谦退保建平'),('sl_09','巴东太守杨谦击势将申阳'),('tj_15','巴东太守杨谦'),('tj_16','征虏将军杨谦弃涪城'),('tj_16','萧敬文杀征虏将军杨谦')],7598:[('wsh_02','段匹䃅弟秃率壮士千人')],7600:[('tj_14','吴兴沈桢说沈充')]}
    for ident,readings in round10b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第10批B·本人官事及原文字形審讀','Actual candidate subject read, not a generic homonym hit') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_listed_primary_contexts_read_and_adjudicated',source_identities_all_adjudicated=ident!=7594,last_manual_review_batch='round10b')
    for ident,note in {7586:'周騫324義興殺王敦所置劉芳，死者是芳非騫；通鑑作周蹇為可能異文但未以音同強并。',7588:'三書同日南太守夏侯覽，永和三347林邑范文害之以屍祭天；不是三國趙雲同鄉夏侯蘭。',7589:'五來源同張骏治中使李雄及橋贊告密，卒致命是終於完成使命，不是張淳死於建康；使事年份複敘有異，不當生卒。',7590:'掘咄哪為托候部首領，石虎大破及俘牛馬不明其人被俘或被殺，不能以全軍敗作卒。',7591:'曹渾三書同324中軍司馬夜擊王含，何康死非曹渾；江寧/越城同役不另拆。',7592:'朱表典農使者與霍皓陸充巡行農桑，未記生卒。',7594:'李弇字子完原名良因妻梁改名，李暠祖父/武衛將軍天水太守安世亭侯，三原書同祖父官爵；李昶18卒非李弇。CBDB507556原行僅名、日期官履注全空，未核全此key。',7595:'李玝/華陽私字午為李蕩子李班弟、與李壽攻陰平及334奔晉，三key親屬官事可聯；不把李琀稚戰死套他，347山陽本人戰死有獨立直文。',7597:'楊謙巴郡/巴東太守至征虜將軍，春秋一傳連330退建平、346擊申陽、347退德陽，與通鑑兩鍵同官地事件；347十二月蕭敬文殺本人直載。',7598:'魏書作段匹磾弟禿、與曹渾陳嵩破王含，通鑑同役稱秀，異字支可聯；7599尚有李矩部將支無直接履歷橋，不先整條指向可能混合目標。',7600:'沈楨324為明帝說沈充，後沈充死非楨死，無生卒。'}.items():results[ident]['unresolved'].append(note)
    results[7594]['evidence'].append(dict(citation='Original CBDB507556 row inspected',excerpt=json.dumps(originals[7594]['cbdb_originals'],ensure_ascii=False)))
    lan347=chunk_ev('js_18','至永和三年，文率其众攻陷日南，害太守夏侯览','晉書四夷林邑·永和三347','本人明確被害，三原書同年')
    results[7588].update(review_status='primary_text_corroborated',historical_death=date(347,lan347))
    results[7588]['evidence'].append(lan347)
    qing_variant=chunk_ev('sl_00','呼延清[纪事本末作呼延青人]','十六國春秋前趙錄六·姓名異文直注','同輔威將軍獲石容循跡斬陳安，明文異文非單憑同音')
    results[7587].update(review_status='duplicate',duplicate_of=7540,identity_status='explicit_textual_variant_huyan_qing_ren',source_identities_all_adjudicated=True)
    results[7540]['evidence'].append(qing_variant)
    results[7540]['unresolved'].append('續核7587春秋原文直注呼延清紀事本末作呼延青人，同獲石容斬陳安事，異文身份已連，可收7587；未知本人生卒。')
    wu347=chunk_ev('sl_09','玝遂奔晋厯巴郡襄阳宜都太守龙骧将军永和三年从征西至于山阳战死','十六國春秋蜀錄·李琀稚玝家屬傳','玝本人347戰死，非兄琀稚323死')
    results[7595].update(review_status='primary_text_corroborated',historical_death=date(347,wu347))
    results[7595]['evidence'].append(wu347)
    yang347=chunk_ev('tj_16','十二月，振威护军萧敬文杀征虏将军杨谦','資治通鑑·永和三347十二月','本人被殺直載，與330巴東守由春秋履歷接合')
    results[7597].update(review_status='primary_text_corroborated',historical_death=date(347,yang347,month=12))
    results[7597]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7597]['evidence'].append(yang347)
    for ident,readings,components,note in [(7593,[('hy_00','永和三年，从征西于山阳战死'),('js_07','访率怅下将李午')],[dict(identity='李蕩子李班弟李玝，華陽字作午，成漢奔晉',candidate_duplicate_of=7595,historical_death=date(347,wu347)),dict(identity='周訪313部將李午，追斬張彥')],'候選hy本身是成漢宗室，晉書key則313周訪部將。名字午/玝OCR混不能使此兩不同履歷自動同人，整條不重定向7595。'),(7596,[('js_13','苻健将杨平戍许昌'),('sl_00','北平太守杨平邑振')],[dict(identity='352苻健將楊平戍許昌'),dict(identity='324前趙北平太守楊平修徐水廟碑')],'兩官地政權間無實讀履歷橋，不能只以同名承接；許昌襲破不明本人死。'),(7599,[('js_08','季弘、李瑰、段秀'),('tj_14','遣将军段秀、中军司马曹浑')],[dict(identity='325李矩南歸部將段秀'),dict(identity='324明帝將段匹磾弟秀，魏書同役作禿',related_id=7598)],'兩個候選各自角色已讀，可能324勤王後往北任李矩但缺直接履歷橋，暫不斷為確定不同人也不整條合併。')]:
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第10批B·原候選不同履歷支實核','Candidate scope differs; absence of bridge does not assert historically impossible identity') for ch,phrase in readings)
        results[ident].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='unlinked_candidate_identity_branches',historical_birth=None,historical_death=None,component_identities=components,last_manual_review_batch='round10b')
        results[ident]['unresolved'].append(note)
    round10c_reads={7601:[('js_18','遣其乡人沈祯谕充'),('js_10','遣参军沈祯衔命奉授')],7603:[('js_07','平南将军王润'),('sl_09','斩王誓王润'),('tj_16','斩王誓、王润')],7604:[('js_18','含子瑜散骑常侍')],7605:[('js_12','兄子安成太守籍之'),('js_10','世子文学王籍之')],7606:[('js_07','斩伪尚书仆射王誓'),('js_18','伪尚书仆射王誓'),('sl_09','斩王誓王润'),('tj_16','斩王誓、王润')],7607:[('tj_14','参军王骘聘于赵'),('js_15','遣参军王骘聘于刘曜'),('sl_08','遣参军王隲聘于赵')],7608:[('sl_01','东莞太守竺珍')],7609:[('sl_09','索袭字伟祖'),('js_17','索袭，字伟祖')],7610:[('js_16','续咸，字孝宗'),('sl_01','理曹参军上党续咸'),('sl_01','廷尉续咸上书切谏'),('tj_13','理曹参军上党续咸'),('tj_15','廷尉上党续咸苦谏'),('sl_02','续咸字孝宗')],7611:[('sl_09','梁州太守董慬以兴古叛降于雄')],7612:[('sl_01','东海太守萧诞以郡降勒')]}
    for ident,readings in round10c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第10批C·本人完整小傳或官事原段實讀','Named role and actual death target checked') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_listed_primary_contexts_reviewed',source_identities_all_adjudicated=ident not in [7601,7605],last_manual_review_batch='round10c')
    for ident,note in {7601:'兩原文分元帝參軍奉授賀循與324明帝遣沈充鄉人沈禎，前者沒有鄉里/家屬連結，名字及朝廷使職相容但尚缺明確履歷橋；兩鍵已讀，不斷為兩人，保留身份待連。324支與7600沈楨同使命異字，但整條有未連支不先重定向。',7603:'王潤為成漢故平南將軍347反，周撫斬於彭模，三書同人；非桓溫親手斬，日期347由通鑑年段確證。',7604:'原文王含子瑜散騎常侍，非同片桓溫攻蜀中書監王瑜，候選明王含子不能被另一同名污染。無生卒，不把含應父子被沉江擴套所有兒子。',7605:'王籍之世子文學與王彬兄子安成守兩支皆已讀，前者無親屬/里籍，官遷相容但未直接連履歷；保留全key未核明，不把王彬卒59歲套侄。',7606:'王誓成漢尚書僕射降而再叛，與王潤同347周撫彭模所斬，四書身份同人。',7607:'王騭/隲前涼參軍使劉曜，三書同齊桓盟會答語；未記生卒，禮遣還不當死。',7608:'竺珍東莞太守324降石勒，非被斬者，無生卒。',7609:'索襲字偉祖敦煌隱逸，陰澹請三老後病卒年79，兩原文一致；張茂時只是前訪問時段，不能硬定卒於324或以79倒算固定生日。',7610:'續咸字孝宗上黨，師杜預、劉琨從事中郎至石勒理曹廷尉，五key官履全接；年97死石虎世僅寬時段，不硬造固定卒年/出生年。331諫而赦免非處死。',7611:'董慬為梁州太守與爨亮李逷附李雄，未記生卒；原姓名慬非自動替換董勤。',7612:'蕭誕東海太守324與竺珍降趙，未記本人死，無生卒。'}.items():results[ident]['unresolved'].append(note)
    results[7609]['reported_nominal_death_age']=79
    results[7610]['reported_nominal_death_age']=97
    for ident in [7603,7606]:
        ev=chunk_ev('tj_16','温命益州刺史周抚镇彭模，斩王誓、王润','資治通鑑·永和三347平蜀後','周撫奉命斬兩名，非鄰文李勢降京之後卒')
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(347,ev))
        results[ident]['evidence'].append(ev)
    for ch,phrase in [('hy_00','中书监王嘏'),('sl_09','散骑常侍王嘏'),('tj_16','散骑常侍王嘏'),('js_09','子嘏嗣，尚鄱阳公主')]:results[7602]['evidence'].append(chunk_ev(ch,phrase,'王嘏·成漢臣與王琨子兩候選原文','Whole candidate scopes actually span distinct polity/father/family chronology'))
    results[7602].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='cheng_han_official_and_eastern_jin_wang_kun_son_merged',historical_birth=None,historical_death=None,component_identities=[dict(identity='成漢李班賓友/李壽使臣/李勢中書監王嘏'),dict(identity='東晉王琨子王嘏、尚鄱陽公主、王恢父')],last_manual_review_batch='round10c')
    results[7602]['unresolved'].append('晉書key原文為王導子孫王琨→嘏→恢，前三key為成漢李氏重臣；不能把同名跨家族合。兩支卒皆無明年。')
    round10d_reads={7613:[('sl_00','生擒伊余于阵'),('tj_14','生擒伊馀，尽俘其众')],7614:[('sl_00','其酋大虚除权渠自号秦王'),('tj_14','其酋虚除权渠自号秦王')],7615:[('js_12','斐字思行，潭之兄子也'),('js_18','散骑常侍虞斐问疾'),('js_16','桓彝、虞斐慰劳饥人')],7617:[('js_13','许迈，字叔玄'),('js_00','会有道士许迈者')],7618:[('tj_14','遣参军贺鸾就沈充于吴'),('js_07','又遣参军贺鸾就沈充')],7619:[('js_15','时辛晏兵于枹罕'),('sl_00','张阆辛晏率众来降'),('sl_08','凉州别将陇西辛晏'),('sl_09','至狄道省墓遇枹罕护军辛晏反叛'),('tj_14','凉州将辛晏据枹罕'),('tj_15','枹罕护军辛晏告急'),('wsh_04','陇西人辛晏以枹罕降之')],7621:[('sl_09','惟有一子髦至狄道省墓')],7622:[('sl_00','是年前颍丘太守郎宣')],7623:[('jk_01','左衞将军陈嵩段秀'),('wsh_02','左卫参军陈嵩')],7624:[('js_18','诏遣侍中陈晷')],7625:[('sl_01','典劝都尉陆充')]}
    for ident,readings in round10d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第10批D·原書同役及家傳實讀','Actual subject checked; no death inferred from capture/neighboring victim/reign span') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_primary_contexts_read_and_adjudicated',last_manual_review_batch='round10d')
    for ident,note in {7613:'虛除伊餘/伊馀由父權渠及生擒同事確聯，320被俘遷長安未明被殺，不作卒年。',7614:'虛除權渠氐羌酋秦王兵敗後拜征西西戎公，兩來源同父子及降授事，未記生卒。',7615:'候選虞斐底本姓名實作虞斐（馬旁缺字），字思行虞潭兄子，與桓彝同吏部郎/救饑使、324散騎常侍問王敦疾，三書同社交官履；官未達而喪沒明年，不以王敦324死套他。',7617:'許邁字叔玄一名映、後改玄字遠遊，兩來源同道士山水客；本人傳永和二346入西山後莫測所終，羽化不是可驗卒年，也不以見郭璞未弱冠和郭璞死年逆推固定生日。',7618:'賀鸞王敦參軍324赴吳與沈充殺周氏，兩書同任務，被殺周札兄弟子非賀鸞。',7619:'辛晏陇西枹罕護軍阻兵→325歸張骏→327被劉胤迫降，七來源官地轉附明確同人，與辛岩另列不能混；張茂時執髦舊事不套為本人卒。',7621:'辛髦為辛憑獨子，原文省姓，省墓被辛晏俘沒有死字，不能作卒；獲關內侯是父憑非子髦。',7622:'郎宣前颍丘太守與楊平邑振修徐水廟碑，廟碑/鳳鳥皆非本人生卒紀年。',7623:'陳嵩324與曹渾段秀/禿同夜擊王含；建康稱左衛將軍魏書參軍官銜異文，事件隊友同可聯，不拆兩人。原文何康被斬不是嵩卒。',7624:'陳晷324侍中奉命問敦病，單key可定位，不因同名自動套西晉陇西襄武字子明；王敦死非使臣死，無生卒。',7625:'陸充典勸都尉324巡州郡課農桑與霍皓朱表同列，無生卒。'}.items():results[ident]['unresolved'].append(note)
    results[7615]['source_name_glyph_note']='Original transcriptions have 虞斐, not a demonstrated plain 斐-only legal name; candidate spelling retained for downstream orthographic review.'
    results[7617]['last_attested_activity']=346
    pei_est=originals[7616]['cbdb_originals']
    results[7616]['evidence'].extend([chunk_ev('js_07','太子詹事裴劭','晉書·趙王倫傳301篡位','本人301從事中郎，非死亡記載'),dict(citation='Original CBDB195403 row read',excerpt=json.dumps(pei_est,ensure_ascii=False),interpretation='Death cc385 is explicitly Tackett estimate, index322 is not birth; original epitaph not acquired and identity unlinked to301 official')])
    results[7616].update(review_status='identity_conflict',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='301_jin_official_and_cbdb_estimated_late_fourth_century_subject_unlinked',component_identities=[dict(identity='301趙王倫從事中郎/太子詹事裴劭'),dict(identity='CBDB195403裴劭，Tackett估約385卒，index322非生年',historical_death=dict(year=385,precision='circa_year_estimate',calendar='historical_CE_year',source='CBDB195403 original note',excerpt='Death year estimated by Tackett (cc385)'))],last_manual_review_batch='round10d')
    results[7616]['unresolved'].append('相隔84年並非不可能但沒有家屬/墓誌橋，不能把385估卒套301太子詹事。CBDB所引墓誌補遺/洛陽新獲尚未取得；原始證據兩支保留。')
    results[7620]['evidence'].append(dict(citation='Original CBDB178652 row read',excerpt=json.dumps(originals[7620]['cbdb_originals'],ensure_ascii=False),interpretation='Index322 is not birth; estimated cc385 has dy_range2 and no original epitaph citation/context acquired'))
    results[7620].update(review_status='secondary_source_checked',historical_birth=None,source_identities_all_adjudicated=False,identity_status='cbdb_estimated_death_only_original_epitaph_not_acquired',last_manual_review_batch='round10d',historical_death=dict(year=385,precision='circa_year_estimate',calendar='historical_CE_year',source='CBDB178652 original note',excerpt='Death year estimated by Tackett (cc385)'))
    results[7620]['unresolved'].append('約385卒只原庫估算，未取得墓誌正文，index322不可當生年/活動年，故仍未成第一手全核。')
    round11a_reads={7626:[('sl_01','以右常侍霍皓为劝课大夫')],7627:[('js_18','充司马顾扬说充')],7629:[('js_15','参军马岌劝茂亲征'),('js_15','酒泉太守马岌上言'),('sl_08','参军马岌劝茂亲出拒之'),('sl_09','马岌仕茂为叅军'),('tj_14','参军马岌劝茂亲出拒战'),('tj_17','尚书马岌切谏，坐免官'),('js_17','酒泉太守马岌，高尚之士也')],7630:[('js_15','右长史马谟'),('sl_08','右长史马谟'),('tj_14','右长史马谟')],7631:[('js_15','将军宋辑、魏纂'),('sl_08','使将军宋辑魏纂'),('sl_09','乃讽其主簿魏纂诬鉴谋反')],7632:[('sl_02','将军杜勲鱼汲死之')],7633:[('js_18','参军龚护战没'),('sl_09','叅军龚护战死'),('tj_16','参军龚护战死')],7634:[('sl_00','长安人刘终于崩所得白玉')],7635:[('tj_14','以平原刘赞为祭酒')],7636:[('sl_00','前军将军刘黑'),('sl_01','曜前军刘黒击破'),('tj_14','赵前军将军刘黑')],7637:[('js_08','梁志、司马尚、季弘')],7638:[('js_08','司马尚、季弘、李瑰')],7639:[('sl_01','矩长史崔宣率众二千降勒'),('tj_14','矩长史崔宣帅其馀众二千降于后赵')],7640:[('wsh_01','父潜，仕慕容𬀩'),('wsh_01','悦传子潜，潜传玄伯'),('wsh_01','玄伯父潜为兄浑诔手笔草本')]}
    for ident,readings in round11a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第11批A·同卷事件及完整人物段逐鍵審讀','Original named actor, family, death subject and candidate scope checked') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',identity_status='all_primary_contexts_read_and_adjudicated',source_identities_all_adjudicated=ident not in [7631],last_manual_review_batch='round11a')
    for ident,note in {7626:'霍皓324由右常侍為勸課大夫巡州郡，所讀未記生卒。',7627:'顧揚與7628通鑑key顧颺同沈充司馬、同三策同逃吳，可連單支；7628另有平蘇峻與餘杭令兩個未連履歷key，不能整條合併。',7629:'馬岌323參軍、張骏酒泉守、重華左長史、張祚尚書原傳直連六key，354免官不是被斬（斬者丁琪）；後復位謀刺王擢不成被殺，本傳未獨立明死亡年，不把354免官當明載卒年。',7630:'馬謨前涼右長史324使史淑拜張骏三書同官同事，張茂死不套本人。',7631:'326將軍徙民與張骏主簿魏纂兩支原文均讀，時主官相容但無直接履歷橋故全key暫未閉合。志怪段其後三年纂死承張骏之死而非誣陰鑒當年，不能從候選固定一個卒年；不把陰鑒死套魏纂。',7634:'劉終321獻白玉拜奉瑞大夫是原文明確角色，玉上讖語與山崩不屬本人生卒，未載年月。',7635:'平原劉贊321教慕容皝祭酒，非吳將劉贊，未載生卒；教學活動不當出生年。',7636:'劉黑/黒前趙前軍將軍325八特阪破石聰，三書同人；前後劉岳俘/王騰死均非黑本人，無生卒。',7637:'司馬尚325棄家隨李矩南歸，死者矩非尚，不與戰國將軍同名合。',7638:'季弘325隨李矩南歸，矩墜馬卒不包括弘，無生卒。',7639:'崔宣李矩長史325率餘眾降後趙，兩原書同人，李矩卒不是崔宣卒。',7640:'崔潛崔悅子崔玄伯父、慕容暐黃門侍郎，父子傳書與兄渾誄草本皆本傳直文。CBDB31518注明Wei Shu24.610，與原魏書卷24崔玄伯傳同具體引書可連，不按空名裸行；index325非出生，生卒記不詳。'}.items():results[ident]['unresolved'].append(note)
    results[7627]['candidate_duplicate_of']=7628
    results[7640]['evidence'].append(dict(citation='CBDB31518 original note checked against cited 魏書卷24崔玄伯傳',excerpt=json.dumps(originals[7640]['cbdb_originals'],ensure_ascii=False),interpretation='Unlike a bare homonym row, explicit Wei Shu24.610 reference leads to reviewed family biography; no birth/death dates in original database.'))
    fish_variant=chunk_ev('sl_02','谢艾乗胜追撃将军杜勲鱼汲死之','十六國春秋後趙錄·永和三347枹罕之役','同麻秋張瑁謝艾/杜勳成對事件，魚汲是汲魚倒文')
    results[7632].update(review_status='duplicate',duplicate_of=7557,identity_status='inverted_name_ji_yu_same_ma_qiu_officer',source_identities_all_adjudicated=True)
    results[7632]['evidence'].append(fish_variant)
    results[7557]['evidence'].append(fish_variant)
    results[7632]['unresolved'].append('魚汲/汲魚三原書與此書同成對杜勳、麻秋單馬敗退、謝艾張瑁勝役，倒文身份已核，不與未連同名合；347四月卒存canonical。')
    gong347=chunk_ev('tj_16','参军龚护战死，矢及温马首','資治通鑑·永和三347三月桓溫伐蜀笮橋','本人戰死，三書同時地官')
    results[7633].update(review_status='primary_text_corroborated',historical_death=date(347,gong347,month=3))
    results[7633]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7633]['evidence'].append(gong347)
    for ch,phrase in [('js_12','护军参军顾飏'),('tj_14','司马顾飏说充'),('js_17','余杭令顾飏与葛洪')]:results[7628]['evidence'].append(chunk_ev(ch,phrase,'顧颺·三key原候選履歷核讀','Actual key scopes differentiated; not asserting historical distinctness merely from different offices'))
    results[7628].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='three_gu_yang_role_contexts_without_career_bridge',historical_birth=None,historical_death=None,component_identities=[dict(identity='324沈充司馬顧颺/揚，三策後逃吳',related_id=7627),dict(identity='328護軍參軍顧颺，從王舒平蘇峻'),dict(identity='餘杭令顧颺與葛洪訪郭文')],last_manual_review_batch='round11a')
    results[7628]['unresolved'].append('三角色皆實讀，時地可能相容但沒有字/家屬或轉官直接橋；不先斷成三個不同人，也不以此允許整條去重7627。')
    round11b_reads={7641:[('sl_00','遣将军康平冦魏兴及南阳')],7642:[('tj_14','庾珉族人怡为廷尉评'),('js_10','颍川庾怡为廷尉评')],7643:[('js_08','季弘、李瑰、段秀')],7644:[('js_02','博士郭熙、杜援'),('tj_15','博士郭熙、杜援'),('js_16','弟援，高平相')],7645:[('sl_02','贵嫔栁氏尚书耆之女也')],7646:[('js_08','江霸、梁志、司马尚')],7647:[('wsh_01','嬖人杨柯、牟成')],7648:[('tj_14','尚书召乐广子谟为郡中正'),('js_03','谟字弘范，征虏将军'),('js_10','南阳乐谟为郡中正')],7649:[('js_08','骞韬、江霸、梁志')],7651:[('sl_01','次子宣为使持节'),('sl_02','乃积柴于邺北树标其上'),('tj_15','次子宣为使持节'),('tj_16','赵太子宣帅步骑二万'),('wsh_01','立次子宣为太子'),('js_17','石宣将杀石韬'),('gs_00','为伪赵太子右宣所敬云')],7652:[('sl_01','韬为前锋将军司隶校尉'),('sl_02','秦公韬为太尉'),('tj_15','韬为前锋将军、司隶校尉'),('tj_16','赵王虎以秦公韬为太尉'),('wsh_01','宣及宣弟秦公韬')],7653:[('sl_01','主簿程琅谏曰')],7654:[('js_08','主簿苟远')],7655:[('tj_14','以尚书左仆射荀松为光禄大夫')]}
    for ident,readings in round11b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第11批B·親屬及紀年原文實讀','Actual candidate key role checked; source absence of date not inferred from extracted fields') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7644,identity_status='all_listed_original_primary_identities_read',last_manual_review_batch='round11b')
    for ident,note in {7641:'康平劉曜將寇魏興南陽，石他/田崧被殺是別人，無生卒。',7642:'同卞壸駁私命拒官之奏，廷尉評庾怡兩書同人；晉書語稱父命通鑑庾珉族人，父子/族人親屬精度有異，保留原表達而不強斷珉親子，未知生卒。',7643:'李瑰隨李矩南歸，未明本人死；李矩325墜馬卒不套。',7644:'博士杜援331議元會敬王導與杜夷弟高平相兩支實讀，前者無里籍家屬，尚缺履歷橋故全key未閉合；不得把荀奕332死或杜夷之卒套杜援。',7645:'石虎柳貴嬪尚書柳耆女、兄附石宣而連坐死，原字栁；其後所納耆少女是妹妹，不能把妹妹後續當被殺柳妃仍生。',7646:'梁志同325李矩南歸百餘將之一，無本人生卒。',7647:'楊柯為石宣嬖人348殺石韜，與春秋楊杯/通鑑楊柸同役可候選異文；原key只有魏書，不先合未核目標，死者石韜/宣非楊柯。',7648:'樂謨樂廣子字弘範，從325郡中正召命、後征虜將軍吳郡內史三key父子與名直接同；父樂廣去世不套謨生卒。',7649:'江霸325隨李矩南歸，未記生卒。',7651:'七key同石虎次子河間王/皇太子宣，殺弟韜後被虎焚殺；高僧傳伪趙太子右宣右疑石字訛，但稱太子宣唯一身份一致。焚死348八月，無生日，不套少子數歲或石世10歲。',7652:'五key同石虎子樂安王後秦公石韜，348八月被兄宣使楊柸等夜殺佛精舍；殺人者非韜，其年齡無明文。',7653:'程琅325諫獵而獲封關內侯，當日馬觸木死不是程琅死，無生卒。',7654:'苟遠325李矩主簿隨行，未記本人死亡。',7655:'荀松325左僕射轉光祿錄尚書，單key已定位；不憑同音名強合荀崧條，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    liu348=chunk_ev('sl_02','坐其二兄有宠于宣亦杀之','十六國春秋·石虎建武十四348宣韜案','本人連坐被殺；下句新納柳氏是其妹')
    results[7645].update(review_status='primary_text_corroborated',historical_death=date(348,liu348))
    results[7645]['evidence'].append(liu348)
    xuan348=chunk_ev('sl_02','四面纵火烟炎际天','十六國春秋·石虎建武十四348八月','石宣本人受焚，置於八月殺韜後九月議儲前')
    tao348=chunk_ev('sl_02','斫杀韬于精舍','十六國春秋·石虎建武十四348八月社日夜','韜本人遇害，原文社日夜而非公曆日，保留農曆月')
    for ident,ev in [(7651,xuan348),(7652,tao348)]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(348,ev,month=8))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    wang_reads=[('js_11','述字怀祖'),('js_13','王述'),('sl_01','使王述煮盐于角飞城'),('tj_15','年三十，尚未知名'),('tj_16','征虏长史王述'),('tj_17','以前会稽内史王述为扬州刺史'),('tj_18','扬州刺史王述'),('js_09','王述与会稽王笺'),('js_12','太原王述雅敬重之'),('sg_09','承子述字懐祖'),('ssxy_00','王述别传曰述字怀祖'),('ssxy_01','王述转尚书令'),('ssxy_02','王述从骠骑功曹出为宛陵令'),('ssxy_03','万取太原王述女名荃')]
    results[7650]['evidence'].extend(chunk_ev(ch,phrase,'王述·逐鍵候選範圍及本人全傳實核','Thirteen eastern-Jin family/career contexts versus312 salt worker; unrelated same-chapter 河間王述 excluded from tj15 candidate') for ch,phrase in wang_reads)
    wang368=chunk_ev('js_11','三年卒，时年六十六','晉書王述本傳·太和三368','王承子太原懷祖本人，傳中太和二年乞骸骨後第三年卒；虛歲66')
    wang303=date(303,wang368,age=66)
    results[7650].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='eastern_jin_wang_cheng_son_and_312_shi_le_salt_worker_merged',historical_birth=None,historical_death=None,component_identities=[dict(identity='太原王承子懷祖、王坦之父王述',keys=[k for k in results[7650]['keys'] if not k.startswith('sl:sl_01:190:')],source_identities_all_adjudicated=True,historical_birth=wang303,historical_death=date(368,wang368)),dict(identity='312石勒使在角飛城煮鹽王述',keys=['sl:sl_01:190:王述'],historical_birth=None,historical_death=None)],last_manual_review_batch='round11b')
    results[7650]['evidence'].append(wang368)
    results[7650]['unresolved'].append('東晉13鍵親屬字爵、官履均已核，卒368虛歲66推生303；sl01另312煮鹽者無橋、年齡/政權異，隔離不把已證生卒套整條。通鑑tj15同卷河間王述非其候選，勿再增假第三支。')
    round11c_reads={7657:[('sl_01','太史令赵揽固諌'),('sl_02','太史令赵揽宻言'),('sl_02','先是散骑常侍赵揽言于虎'),('tj_16','太史令赵揽谏曰')],7658:[('js_08','参军郭方')],7660:[('js_15','武陵人阎曾夜叩门'),('sl_08','武陵人阎曽夜叩府门'),('tj_14','武陵阎曾夜叩府门')],7661:[('sl_01','黎阳人陈武妻一产三男一女')],7662:[('js_08','历阳陈训又谓人曰'),('js_07','善相者庐江陈训')],7663:[('js_00','护军将军冯怀'),('js_02','侍中冯怀议曰'),('js_12','黄门侍郎冯怀驳曰'),('js_17','太守冯怀上疏'),('tj_15','侍中冯怀议'),('tj_16','太常冯怀以问含'),('js_15','太常冯怀以问于含'),('ssxy_01','冯懐字祖思长乐人')],7664:[('js_08','将军骞韬')],7665:[('sl_01','讨鲜卑郁粥于离石')],7666:[('sl_01','晋都尉鲁潜以许昌叛降于勒'),('tj_14','都尉鲁潜以许昌叛')],7667:[('tj_14','赵以汝南五咸为太尉')],7668:[('hy_00','雄妻任无子')],7669:[('js_13','临淮太守刘矫率将士数百'),('tj_14','临淮太守刘矫掩袭遐营')],7670:[('sl_02','斩京兆太守刘秀离'),('tj_17','斩京兆太守刘秀离')],7673:[('sl_02','孙伏都刘铢等结羯士三千人'),('tj_17','孙伏都、刘铢等帅羯士三千')],7674:[('sl_01','晋济岷太守刘闿'),('tj_14','济岷太守刘闿等杀')],7675:[('js_01','建昌太守长沙王循')]}
    for ident,readings in round11c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第11批C·同役及字形家屬原文逐鍵核讀','Primary originals individually read; chronology of neighboring persons not reassigned') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident not in [7663,7667],identity_status='all_listed_original_primary_contexts_read',last_manual_review_batch='round11c')
    for ident,note in {7657:'趙攬338諫伐燕鞭免後復太史令、348宣韜案中散騎常侍被誅，三key直接同石虎術數臣持續事；王波被斬不是攬當時卒，死亡據後案本人直文。',7658:'郭方李矩參軍325隨行，矩死非方死，無生卒。',7660:'閻曾/曽武陵人321假先君命諫台，張茂不殺，三書同事；辛岩請殺未執行不能記321卒。',7661:'黎陽陳武321攜妻四胞胎獻祥，孩子當年生非父陳武生，無本人生卒；不同東吳武將。',7662:'陳訓術人廬江/歷陽稱地不必互斥，原文同相者類型與周訪陶侃/祖逖時代；兩段均已讀，預言別人壽命不等於自己的生卒，不因預言應驗硬造年月。',7663:'馮懷字祖思長樂太常→護軍與侍中/黃門禮議同顏含官事可連，唯327任旭後上疏之太守支無任郡/家屬橋，八鍵全已讀但整體未閉合；顏含年93、陸曄年74、荀奕332卒均不套本人。',7664:'騫韜同325隨李矩南歸諸將，無本人生卒。',7665:'鬱粥鮮卑離石首領321敗奔烏丸，原文明逃而非死，俘其眾不套本人。',7666:'魯潛都尉325以許昌降趙，雙原書同官城，未記其卒；石他/田崧死不套。',7667:'本地通鑑字串確作汝南五咸，疑汝南王咸的王誤五，前趙帝親族封汝南王可比對但未作版本校勘，不把抽取五姓當已定姓名；326任太尉非生卒。',7668:'任氏為李雄妻，養李班又養李期，同原家傳明確，無本人生卒；李班/雄死不套母。',7669:'劉矯326臨淮守斬田防等平劉遐部叛，兩書同官事；殺人者不是被殺者，無生卒。',7670:'劉秀離349九月後趙京兆守為司馬勳治中劉煥所斬，兩書同將同地。',7673:'劉銖與孫伏都349十二月羯士攻冉閔敗被誅，兩書列同謀並明近日孫劉構逆支黨伏誅，原年段在350新年之前，不擅推350。',7674:'劉闓326濟岷守殺夏侯嘉以下邳降趙，死者嘉而非闓；二原書同身份，無生卒。',7675:'長沙王司馬循為建昌太守322與譙王承盟誓，未記本人戰死，劉翼戰死及承被害不套循。'}.items():results[ident]['unresolved'].append(note)
    zhao348=chunk_ev('sl_02','及宣杀韬虎疑其知而不告亦诛之','十六國春秋建武十四348石宣案·趙攬','本人被誅直載，非早年鞭免或王波被斬')
    results[7657].update(review_status='primary_text_corroborated',historical_death=date(348,zhao348))
    results[7657]['evidence'].append(zhao348)
    xiu349=chunk_ev('sl_02','九月勲出骆谷破遵长城戍壁于悬钩','十六國春秋太寧元349九月司馬勳役','同一句下接劉煥斬京兆太守劉秀離；年份石虎死後石遵在位')
    results[7670].update(review_status='primary_text_corroborated',historical_death=date(349,xiu349,month=9))
    results[7670]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7670]['evidence'].append(xiu349)
    zhu349=chunk_ev('tj_17','近日孙、刘构逆，支党伏诛','資治通鑑永和五349十二月孫伏都劉銖案','前文明劉銖伏都同謀，後詔孫劉伏誅；未跨350新年')
    results[7673].update(review_status='primary_text_corroborated',historical_death=date(349,zhu349,month=12))
    results[7673]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7673]['evidence'].append(zhu349)
    for ident,readings,components,note in [(7659,[('js_02','博士郭熙、杜援'),('tj_15','博士郭熙、杜援'),('sl_00','晋新兴太守郭熙[一作颐]')],[dict(identity='331晉博士郭熙'),dict(identity='西晉新興太守郭熙一作頤、舉劉聰')],'兩候選各自實讀但未連轉官、字或家屬，兼原文郭熙一作頤需校名，不以同名強合。'),(7671,[('tj_14','光禄太夫刘绥为大司徒'),('js_05','高平刘绥为委伯'),('js_10','子绥，初举秀才'),('sl_00','前新赠大司徒烈愍公崔岳中书令曹恂晋阳太守王忠太子洗马刘绥等'),('ssxy_02','绥字万安高平人祖奥太祝令父斌')],[dict(identity='326前趙光祿大夫劉綏→大司徒'),dict(identity='高平萬安、劉斌子，兗州八伯委伯/驃騎長史'),dict(identity='劉隗子綏、劉波父，322隨父奔勒後卒'),dict(identity='劉曜少時恩人、太子洗馬劉綏，320已故追贈')],'原父系高平劉斌子與劉隗子不同，前趙已死洗馬320追贈又不能套326在任大司徒。四branch皆已讀，卒年無明文，禁止把鄰文父隗61歲套綏。'),(7672,[('js_13','子肇年幼，成帝以徐州授郗鉴'),('tj_14','遐子肇尚幼'),('js_03','南郡太守刘肇赂戎'),('ssxy_01','南郡太守刘肇遗筒中笺布')],[dict(identity='326尚幼劉遐子肇、後散騎侍郎'),dict(identity='西晉南郡太守劉肇、行賄王戎為劉毅所劾')],'劉毅285已卒前任司隸時之南郡守不可能同326幼子，兩支原親屬時代明確。無確切本人卒年。')]:
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第11批C·實際候選多身份核查','Original roles/fathers/deceased versus living chronology separated') for ch,phrase in readings)
        results[ident].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='distinct_or_unlinked_original_candidate_branches',historical_birth=None,historical_death=None,component_identities=components,last_manual_review_batch='round11c')
        results[ident]['unresolved'].append(note)
    chu_reads=[('js_12','征北将军褚裒以为长史'),('js_17','褚裒，字季野'),('sl_02','晋征北大将军褚裒'),('tj_15','豫章太守褚裒'),('tj_16','征豫章太守褚裒'),('tj_17','都乡元穆侯褚裒'),('wsh_02','聃征北将军褚裒'),('yi_01','褚裒以徐、兖劲卒'),('js_00','父裒，见《外戚传》'),('js_01','江州刺史褚裒当之镇'),('js_06','征北将军褚裒闻其名'),('js_10','征北将军褚裒镇京口'),('js_14','卫将军褚裒上疏荐和'),('js_16','征北将军褚裒又引为参军'),('js_18','褚裒时为豫章太守'),('ssxy_00','褚裒字季野河南阳翟人'),('ssxy_01','河南禇季野'),('ssxy_02','禇季野皮里阳秋'),('ssxy_03','褚季野问孙盛')]
    results[7656]['evidence'].extend(chunk_ev(ch,phrase,'褚裒20鍵本人原文範圍複核','All19 historical-text keys read; variant禇/褚 and matching surname,字,office,皇后father established') for ch,phrase in chu_reads)
    results[7656]['evidence'].append(dict(citation='CBDB30916 original identity/date fields reread',excerpt=json.dumps(originals[7656]['cbdb_originals'],ensure_ascii=False),interpretation='303/349/age47/name and specific Chu Bao biography references identify same subject; cited funerary inscription not claimed independently inspected'))
    chu349=chunk_ev('tj_17','十二月，己酉，卒','資治通鑑永和五349十二月己酉·褚裒','緊接都鄉元穆侯褚裒惭愤發疾；與本人傳永和5及年47同')
    results[7656].update(review_status='primary_text_corroborated',source_identities_all_adjudicated=True,identity_status='all20_source_keys_chu_jiye_empress_father_adjudicated',historical_death=date(349,chu349,month=12,day_ganzhi='己酉'),last_manual_review_batch='round11c')
    results[7656]['historical_death'].update(precision='lunar_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7656]['evidence'].append(chu349)
    results[7656]['unresolved']=[u for u in results[7656]['unresolved'] if u!='some_original_passages_missing_or_name_not_located']
    results[7656]['unresolved'].append('二十key身份已逐核，墓誌本體仍未取得不作獨立史證。原晉書349被掠敘慕容皝已348卒是敘事人名問題，通鑑未沿用；不以此否定本人明載349卒。')
    round12a_reads={7676:[('sl_02','中书令孟凖'),('tj_17','中书令孟准')],7677:[('sl_02','龙骧将军孙伏都刘铢'),('sl_08','将军孙伏都等侵冦不辍'),('tj_16','将军孙伏都攻金城'),('tj_17','孙伏都、刘铢等帅羯士'),('wsh_02','鉴弟苞与胡张才、孙伏都'),('js_15','麻秋、孙伏都等侵寇')],7679:[('sl_00','将军黄秀帛成冦酂')],7682:[('sl_02','立妃张氏为皇后'),('sl_02','并诛郑太后张皇后及太子衍'),('tj_17','并杀郑太后、张后、太子衍')],7683:[('sl_01','将军张阖等叛杀下邳内史夏侯嘉')],7684:[('sl_02','侍中徐统叹曰'),('sl_04','司隶校尉高平徐统'),('tj_17','侍中徐统叹曰'),('sl_05','惟髙平徐綂见而竒之')],7685:[('hy_00','师友天水文夔'),('sl_09','陇西董融天水文夔')],7686:[('sl_02','左长史石光司马曹曜'),('tj_17','左长史石光、司马曹曜')],7689:[('sl_02','髙力督定阳梁犊'),('sl_03','梁犊之乱虎遣洪'),('sl_06','虎末年谪戍梁犊反'),('tj_17','高力督定阳梁犊'),('wsh_02','讨平梁犊')],7690:[('js_16','沈劲，字世坚'),('sl_03','沈劲劲字世坚'),('tj_14','钱举'),('tj_18','三月，克之，执扬武将军沈劲'),('js_18','充子劲竟灭吴氏')]}
    for ident,readings in round12a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第12批A·同案群臣及本人傳逐鍵實讀','Original roles/families and actual death statement checked; retrospective references not life extensions') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_primary_contexts_read_and_adjudicated',last_manual_review_batch='round12a')
    for ident,note in {7676:'孟準/凖中書令勸遵削閔、同遵349十一月被殺，兩書同官同案。',7677:'孫伏都後趙將346侵涼至349龍驤將軍孫劉案，六鍵同石虎石鑑政權軍事履歷，魏書壓縮張才案及孫伏都案不另延年；349十二月被冉閔李農斬。',7679:'帛成劉曜將與黃秀冦酂，魏該奔襄陽不是帛成死亡，未記生卒。',7682:'石遵張皇后三原段同夫及封後/遇害事，349十一月被殺；不能與同趙石邃妃張氏或同姓女性合。',7683:'張闔326以下邳降趙並殺夏侯嘉，死者嘉而非闔，無生卒。',7684:'徐統/綂高平知人者、七歲苻堅及少遊王猛兩相人故事同地方司隸名臣，349侍中服毒。371/372召其子孫是苻堅追念，不表示徐統活到該年。本地春秋旁敘石虎至晉太和五年死與其年段/十五年矛盾，通鑑永和五349戊辰獨立確年，不沿錯字延到370。',7685:'文夔天水李班師友，兩書同董融並列；班卒/班年26或16等異文不是文夔年齡，無生卒。',7686:'曹曜石苞司馬349勸止攻鄴被誅，雙原書同案明包括曹曜，無生日；只確年不把前句八月退兵硬當本人明載卒月。',7689:'梁犢定陽高力督349起兵後為姚弋仲石斌所斬，五來源同亂及主將；通鑑正月起敘至春末未逐月立標，不把正月起兵一律當正月死。',7690:'沈勁世堅吳興武康沈充子、復仇吳儒至洛陽守城，五來源家屬官履直接聯。年30餘不得仕沒有獨立紀年不反推固定出生；父充324卒不套子，365三月本人遇害。'}.items():results[ident]['unresolved'].append(note)
    same34911=chunk_ev('tj_17','并杀郑太后、张后、太子衍、孟准、王鸾及上光禄张斐','資治通鑑永和五349十一月石遵案','名列遇害者，孟準與張皇后不是主殺者')
    for ident in [7676,7682]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(349,same34911,month=11))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(same34911)
    sun349=chunk_ev('tj_17','闵、农攻斩伏都等','資治通鑑永和五349十二月孫劉案','伏都本人被斬，原敘在350新年之前')
    results[7677].update(review_status='primary_text_corroborated',historical_death=date(349,sun349,month=12))
    results[7677]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7677]['evidence'].append(sun349)
    xu349=chunk_ev('tj_17','侍中徐统叹曰：“乱将作矣，吾无为预之。”仰药而死','資治通鑑永和五349四月戊辰','戊辰矯詔同日服藥，翌己巳石虎卒')
    results[7684].update(review_status='primary_text_corroborated',historical_death=date(349,xu349,month=4,day_ganzhi='戊辰'))
    results[7684]['historical_death'].update(precision='lunar_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7684]['evidence'].append(xu349)
    cao349=chunk_ev('sl_02','左长史石光司马曹曜等固諌苞怒诛光等百余人','十六國春秋太寧元349石苞謀攻鄴','明列曹曜勸諫，被誅同組百餘；不硬擬卒月')
    results[7686].update(review_status='primary_text_corroborated',historical_death=date(349,cao349))
    results[7686]['evidence'].append(cao349)
    du349=chunk_ev('tj_17','遂与斌等击犊于荥阳，大破之，斩犊首而还','資治通鑑永和五349梁犢亂','同年石虎死前，本人被斬明文，非討亂者死亡')
    results[7689].update(review_status='primary_text_corroborated',historical_death=date(349,du349))
    results[7689]['evidence'].append(du349)
    jin365=chunk_ev('tj_18','三月，克之，执扬武将军沈劲','資治通鑑興寧三365三月洛陽陷','下文慕輿虔諫殺遂殺之，同春秋建熙六及晉書興寧三')
    results[7690].update(review_status='primary_text_corroborated',historical_death=date(365,jin365,month=3))
    results[7690]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7690]['evidence'].append(jin365)
    results[7678]['evidence'].append(dict(citation='CBDB183986 original row read',excerpt=json.dumps(originals[7678]['cbdb_originals'],ensure_ascii=False),interpretation='Tackett cc387 dy_range2 explicitly estimated; index324 not birth; no original epitaph acquired'))
    results[7678].update(review_status='secondary_source_checked',historical_birth=None,historical_death=dict(year=387,precision='circa_year_estimate',calendar='historical_CE_year',source='CBDB183986 original note',excerpt='Death year estimated by Tackett (cc387)'),identity_status='original_cbdb_estimate_only_original_epitaph_not_acquired',source_identities_all_adjudicated=False,last_manual_review_batch='round12a')
    results[7678]['unresolved'].append('約387卒僅庫內估年，原墓誌未取得，index324不可作生年或在世活動；不記精確387。')
    for ident,readings,components,note in [(7681,[('sl_02','上光禄大夫张斐'),('tj_17','上光禄张斐'),('js_14','轨遣将军张斐')],[dict(identity='349後趙上光祿張斐',historical_death=date(349,same34911,month=11)),dict(identity='311張軌將張斐衛京，洛陽陷沒於賊')],'311沒於賊不必指死，可能降仕但原文未連後趙高官，保留兩支身份未連，不以349卒套整條也不斷311卒。'),(7687,[('sl_02','其友李松劝鉴'),('sl_02','中书令李松殿中将军张才'),('sl_09','又遣参军李松伐巴郡'),('tj_16','遣参军李松攻汉巴郡'),('tj_17','中书令李松、殿中将军张才'),('js_07','松、能为皓所杀')],[dict(identity='後趙石鑑友/中書令李松，349十二月被誅'),dict(identity='339東晉庾氏參軍李松攻江陽執李閎黃植'),dict(identity='西晉交阯牙門李松，孫皓所殺年不詳')],'三原候選實跨孫吳末、東晉、後趙；庾翼/亮主帥有異但同擒二人戰役為一支，不造假第四人。各支日期不得套整條。'),(7688,[('sl_02','李迈死之'),('tj_17','王龛、李迈将锐卒三千'),('wsh_02','又杀李迈'),('js_07','襄阳中正李迈，杀之')],[dict(identity='349褚裒部將李邁，代陂戰死',historical_death=dict(year=349,precision='year',calendar='historical_CE_year',source='十六國春秋太寧元代陂李邁死之',excerpt='军次代阪为农所败李迈死之')),dict(identity='約300襄陽中正李邁，孫秀所殺')],'孫秀害襄陽中正死於西晉，不能與349褚裒將同人；兩死亡支隔離。')]:
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第12批A·候選各支姓名角色及卒事實核','Same-name source scope actually differs; dates retained within branch only') for ch,phrase in readings)
        results[ident].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='distinct_or_unlinked_original_candidate_branches',historical_birth=None,historical_death=None,component_identities=components,last_manual_review_batch='round12a')
        results[ident]['unresolved'].append(note)
    results[7681]['component_identities'][0]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    round12b_reads={7691:[('tj_17','始平人为勖聚兵自称将军')],7692:[('tj_14','左卫将军廙、侍中侃、彬')],7693:[('sl_01','永昌门门候王假'),('tj_14','永昌门候王假')],7694:[('tj_14','河南太守王瞻于邾')],7695:[('sl_01','河南太守王羡于邾城')],7696:[('sl_02','武卫将军王鸾'),('sl_02','左卫将军王鸾等遵在位'),('tj_17','武卫将军王鸾'),('tj_17','王鸾及上光禄张斐')],7697:[('sl_02','王龛李迈领锐卒三千'),('sl_02','龛执节不挠寻亦被害'),('tj_17','王龛、李迈将锐卒三千'),('wsh_02','逆围督护王龛于薛')],7698:[('js_15','遣皇甫该御之'),('sl_08','遣皇甫该御之')],7700:[('sl_02','左长史石光司马曹曜等固諌苞怒诛光等百余人')],7701:[('sl_02','中领军石成侍中石启前河东太守石晖'),('tj_17','中领军石成、侍中石启、前河东太守石晖')],7702:[('sl_02','虎以石成为镇逺将军'),('sl_02','中领军石成侍中石启'),('tj_16','赵镇远将军石成'),('tj_17','中领军石成、侍中石启')],7703:[('sl_01','斌为左卫将军封太原王'),('sl_02','石斌虎之庶子母齐氏'),('sl_03','从燕王石斌讨之'),('sl_06','与石斌等击犊于荥阳'),('tj_15','徙平原王斌为章武王'),('tj_16','燕公斌督边州'),('tj_17','张豺使张雄矫诏杀斌'),('js_17','勒爱子斌暴病死')],7704:[('sl_02','前河东太守石晖谋诛闵农'),('tj_17','前河东太守石晖谋诛闵、农')],7705:[('sl_00','休屠王石武以桑城来降'),('tj_14','休屠王石武以桑城降赵'),('sl_01','休屠王石武引兵攻上邽')]}
    for ident,readings in round12b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第12批B·同卷石氏官將與封爵家屬逐鍵審讀','Candidate scope, death subject, name variant and kinship checked against actual originals') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident not in [7691,7705],identity_status='all_named_primary_source_contexts_read',last_manual_review_batch='round12b')
    for ident,note in {7691:'本地原串始平人為勖聚兵，為勖是否正確人名待版本比對；同春秋馬昻起洛氏葛谷事件可能異文但不能只按時代合。討滅之/誅三千餘家未獨立點本人卒，不作349明載卒。',7692:'侍中王侃為王導從弟同宗待罪，原文省姓但上文王導率從弟明確；不把王廙後被殺套王侃，生卒不詳。',7693:'王假永昌門候326欲捕微行石勒，反被拜振忠都尉，兩書同人；沒有被殺，無生卒。',7694:'王瞻326河南守邾城被石瞻攻，與7695春秋王羨同官地年月攻者及前後下邳事，可判同事件姓名異文；城拔不直接證太守卒。',7695:'王羨與7694王瞻同326十二月石瞻攻邾河南守，按獨特事件身份合併；正字仍並列，不單憑音近。',7696:'王鸞349武衛迎石遵、左衛勸誅閔、十一月同遵被殺，兩書全候選官事互接；非同名他朝人。',7697:'王龕349二月伐沛至代陂敗執後被害，三書同褚裒督護及李邁搭配，不與322徐龕合；未知生日，卒年349明確。',7698:'皇甫該327御劉胤振武，同張骏遣兵兩原書同人，出師失利不明被殺，未知生卒。',7700:'石苞左長史石光349固諫被誅，單key可核；7699另有幽州刺史與CBDB未連支，不能整條指向7699去重。',7701:'石啟349十二月侍中，與石成石暉謀殺閔農而被殺，雙書同案明包括本人。',7702:'石成339/340鎮遠將軍擊燕至349中領軍，三key同後趙將持續軍履，原sl02候選本身含兩任且未有父系/官事矛盾；死者呼延晃張支不套其早卒，349宮廷案另有本人死。',7703:'石斌八key同虎庶子勒養兒，春秋明勒取以為兒直連兩父稱呼；太原/平原王爵異文不拆人。佛圖澄傳暴死又蘇非最終卒，349四月張雄矯詔殺本人；沒有確切生日。',7704:'石暉後趙前河東太守349同成啟被誅，非旁支前秦石越等，未知生日。',7705:'石武休屠王以桑城降前趙，秦州刺史酒泉王，三原書同323攻陳安上邽陇城；陳安被斬非武卒。CBDB507546原行僅名無日期注官履，未核全該key。'}.items():results[ident]['unresolved'].append(note)
    variant_wang=chunk_ev('sl_01','石瞻攻河南太守王羡于邾城拔之','十六國春秋趙王八326十二月','與通鑑同官同邾城同攻者年月，候選身份同，羨/瞻正字待校')
    results[7695].update(review_status='duplicate',duplicate_of=7694,source_identities_all_adjudicated=True,identity_status='wang_zhan_xian_variant_same_326_zhu_henan_governor')
    results[7694]['evidence'].append(variant_wang)
    for ident in [7696]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(349,same34911,month=11))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(same34911)
    kan349=chunk_ev('sl_02','龛执节不挠寻亦被害','十六國春秋太寧元349代陂之役','直接王龕本人死，與通鑑/魏書被俘情節同人')
    results[7697].update(review_status='primary_text_corroborated',historical_death=date(349,kan349))
    results[7697]['evidence'].append(kan349)
    results[7700].update(review_status='primary_text_corroborated',historical_death=date(349,cao349),candidate_duplicate_of=7699)
    stone349=chunk_ev('tj_17','中领军石成、侍中石启、前河东太守石晖谋诛闵、农；闵、农皆杀之','資治通鑑永和五349十二月','三人並列謀誅而被殺，非孫劉案叛眾統計套卒')
    for ident in [7701,7702,7704]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(349,stone349,month=12))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(stone349)
    bin349=chunk_ev('tj_17','张豺使张雄矫诏杀斌','資治通鑑永和五349四月','四月乙丑遵被遣後戊辰之前，未硬指同一干支日')
    results[7703].update(review_status='primary_text_corroborated',historical_death=date(349,bin349,month=4))
    results[7703]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7703]['evidence'].append(bin349)
    results[7705]['evidence'].append(dict(citation='CBDB507546 original row read',excerpt=json.dumps(originals[7705]['cbdb_originals'],ensure_ascii=False)))
    results[7699]['evidence'].extend(chunk_ev(ch,phrase,'石光兩角色及CBDB核查','Actual candidate scopes checked, career bridge not supplied by name only') for ch,phrase in [('sl_02','幽州刺史石光拥兵数万'),('tj_16','赵幽州刺史石光拥兵数万'),('tj_17','左长史石光、司马曹曜')])
    results[7699]['evidence'].append(dict(citation='CBDB416784 original row read',excerpt=json.dumps(originals[7699]['cbdb_originals'],ensure_ascii=False)))
    results[7699].update(review_status='identity_conflict',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='youzhou_governor_and_shi_bao_zhangshi_and_bare_cbdb_unlinked',component_identities=[dict(identity='340幽州刺史石光，懦弱徵還'),dict(identity='349石苞左長史石光',historical_death=date(349,cao349),related_id=7700),dict(identity='CBDB416784石光，所有日期官履注均空')],last_manual_review_batch='round12b')
    results[7699]['unresolved'].append('幽州守貶後轉長史可能相容但未見實際轉任橋，且CBDB裸行未連；7699整條不能接7700硬去重，日期只保長史支。')
    round12c_reads={7706:[('sl_01','苞乐平王'),('sl_02','以乐平公苞代镇长安'),('sl_02','并诛苞新兴王祗'),('tj_15','苞封乐平王'),('tj_16','以乐平公苞代镇长安'),('tj_17','并杀苞'),('wsh_02','鉴弟苞与胡张才')],7707:[('sl_02','故燕王斌子衍为皇太子'),('tj_17','故燕王斌子衍为皇太子')],7708:[('sl_02','时沛王冲镇蓟'),('tj_17','时沛王冲镇蓟')],7709:[('sl_04','从祖右光禄大夫永安公侯'),('sl_04','永安公侯率众讨之未克而卒'),('tj_18','永安公侯为太尉'),('tj_18','永安威公侯讨之，未克而卒')],7710:[('hy_00','天水文夔、陇西董融'),('sl_09','陇西董融天水文夔')],7711:[('sl_02','杀遵南阳太守袁景'),('tj_17','杀赵南阳太守袁景')],7712:[('js_00','奏免国相诸葛颐'),('js_12','恢兄颐，字道回')],7713:[('js_04','放字思度'),('js_04','到州少时，暴发渴'),('tj_14','至州。少时，病卒。放，咸之族子也')],7714:[('sl_02','始平人马昻起兵于洛氏葛谷')],7715:[('sl_00','将军黄秀帛成冦酂'),('tj_14','赵将黄秀等寇酂')],7716:[('tj_15','遣司马何仍诣亮'),('js_19','遣司马何仍诣亮')],7717:[('tj_14','赵主曜长子俭，次子胤'),('sl_00','义真曜子俭字')],7718:[('sl_00','征西将军刘贡于南安'),('tj_14','陈安围赵征西将军刘贡于南安'),('sl_01','征西将军刘贡于南安')]}
    for ident,readings in round12c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第12批C·封爵父系及同役原書實讀','Actually read original actor/family/relative age and independent reign context') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_original_primary_subjects_adjudicated',last_manual_review_batch='round12c')
    for ident,note in {7706:'石苞石虎子樂平王公、代石鑑鎮長安至349夜攻閔農被石鑑誅，六key同封爵家屬官事；不是西晉太尉石苞。魏書把夜攻與孫劉案壓縮，不另定卒350。',7707:'石衍石斌子而非石遵親子，遵立太子後349十一月同被殺，兩書父系一致；不套世子年齡。',7708:'沛王石衝349五月起兵討遵、平棘敗元氏賜死，兩原書同主將地點；坑三萬是士卒另項，死亡本人有明文。',7709:'苻侯苻堅從祖永安公、太尉，甘露元=升平三359討高離未克而卒，兩書身份及紀年一致；侯是名非無名永安侯，不擅記某一卒月。',7710:'隴西董融李班師友與文夔並列兩原書同人，李班年/卒都不是董融生卒。',7711:'袁景後趙南陽守349十月司馬勳拔宛所殺，兩書同人，不同於晉守郭啟被俘。',7712:'諸葛頤道回為恢兄、終太常，326琅邪相被免與當時晉朝官履相容，實讀兩本人段；鄭太妃薨或弟恢卒62歲均不套頤，無生卒。',7713:'阮放思度父𫖮祖略，卒年44而本傳未獨立定年。另通鑑太寧元323敘其任交州旋卒，與本人傳成帝幼沖325後任交州敘次相抵；不默採323倒推280或把本傳改年。保留源別衝突待校，不把鄰文阮修42歲套本人。',7714:'馬昂原字昻，349始平洛氏葛谷起兵為石苞攻滅；與7691為勖通鑑同始平/石苞/三千家可能異文，名字未定不硬合。討滅之誅三千家不獨立明本人卒。',7715:'黃秀劉曜將326/327寇酂，魏該奔襄陽不是黃秀死，兩key同事人；春秋複敘活動年不套生卒。',7716:'何仍蘇峻司馬327向庾亮拒徵，兩原書同奏語同任務，峻後戰死不套何仍，未知生卒。',7717:'劉儉字義真劉曜長子、劉胤兄臨海王，兩書同父與封國；10歲/身長七尺五是胤不是儉，群臣323追述漢主劉聰時冊封不表示當323新封或當時本人仍活。',7718:'劉貢劉曜征西將軍南安守與石武擊陳安三書同人；陳安欲斬劉貢未成，實斬陳安不能反套貢卒，無生卒。'}.items():results[ident]['unresolved'].append(note)
    bao349=chunk_ev('tj_17','夜斩松、才于西中华门，并杀苞','資治通鑑永和五349十二月石鑑夜攻案','樂平王石苞本人同案被殺，非其先前殺長史案')
    results[7706].update(review_status='primary_text_corroborated',historical_death=date(349,bao349,month=12))
    results[7706]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7706]['evidence'].append(bao349)
    results[7707].update(review_status='primary_text_corroborated',historical_death=date(349,same34911,month=11))
    results[7707]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7707]['evidence'].append(same34911)
    chong349=chunk_ev('tj_17','获冲于元氏，赐死','資治通鑑永和五349五月石衝之亂','五月遵即位後、六月葬虎前，同春秋是月段')
    results[7708].update(review_status='primary_text_corroborated',historical_death=date(349,chong349,month=5))
    results[7708]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7708]['evidence'].append(chong349)
    hou359=chunk_ev('tj_18','永安威公侯讨之，未克而卒','資治通鑑升平三359春高離反','與春秋甘露元同年，後夏四月邓羌平高離非苻侯卒月')
    results[7709].update(review_status='primary_text_corroborated',historical_death=date(359,hou359))
    results[7709]['evidence'].append(hou359)
    yuan349=chunk_ev('tj_17','冬，十月，释悬钩，拔宛城，杀赵南阳太守袁景','資治通鑑永和五349十月','本人明列被殺，與晉郭啟被俘分開')
    results[7711].update(review_status='primary_text_corroborated',historical_death=date(349,yuan349,month=10))
    results[7711]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7711]['evidence'].append(yuan349)
    results[7713].update(historical_birth=None,historical_death=None,reported_nominal_death_age=44,source_chronology_conflicts=[dict(source='資治通鑑tj14太寧元年323',reported_death_year_context=323,excerpt='未几吏部郎阮放求为交州刺史...至州少时病卒'),dict(source='晉書阮放本人傳',reported_appointment_context='成帝幼沖庾氏執政325後',reported_death_age=44,excerpt='时成帝幼冲...放求为交州...到州少时...遂卒年四十四')])
    results[7714]['candidate_duplicate_of']=7691
    kuang_reads=[('js_11','赵胤、匡术、刘仕'),('js_12','匡术以苑城归顺'),('js_13','时匡术有宠于峻'),('sl_01','司徒中郎匡术攻克马头坞'),('tj_15','阜陵令匡术亦劝峻反'),('wsh_02','阜陵令匡术'),('js_09','路永、匡术、贾宁'),('js_10','怀德令匡术'),('js_14','路永、匡术、宁等皆峻心腹'),('js_19','使怀德令匡术守苑城'),('ssxy_01','匡术为阜陵令逃亡无行')]
    results[7719]['evidence'].extend(chunk_ev(ch,phrase,'匡術11鍵·本人完整事件鏈實讀','同蘇峻心腹→內結王導/奉帝未成→苑城降→晉將討郭默及攻馬頭') for ch,phrase in kuang_reads)
    results[7719].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all11_keys_kuang_shu_su_jun_officer_then_jin_general_adjudicated',last_manual_review_batch='round12c',attested_activity_years=[327,329,330,332])
    results[7719]['unresolved'].append('阜陵令/懷德令轉任稱呼、苑城/宛城字形由同蘇峻黨與王導接合事核同，11key均實讀。孔群卒官、任讓被誅不套匡術，332攻馬頭還在世，未知本人生卒；蘇峻328死不推其卒。')
    for ch,phrase in [('tj_14','追谥前妃卜氏为元悼皇后'),('js_15','继母卜氏遇之无道'),('sl_00','追谥胤母卜氏为元悼皇后')]:results[7720]['evidence'].append(chunk_ev(ch,phrase,'卜氏兩女性原候選逐鍵核讀','劉曜前妃與王延繼母各自夫子不同，不按同姓合併'))
    results[7720].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='liu_yao_former_consort_and_wang_yan_stepmother_merged',historical_birth=None,historical_death=None,component_identities=[dict(identity='劉曜前妃劉胤母、卜泰姊，323已故追諡'),dict(identity='王延繼母卜氏，王延父妻')],last_manual_review_batch='round12c')
    results[7720]['unresolved'].append('前趙元悼卜后323追諡不是323明載死亡，且不能把該女性生卒套王延繼母。王延60初仕或318被靳準殺非卜氏年齡/卒。')
    round12d_reads={7721:[('yl_04','周存，字道名，上谷渔阳人')],7722:[('sl_00','冠军将军呼延那鸡率亲御郎二千骑'),('sl_08','冠军将军呼延那鸡率亲御郎三千')],7723:[('sl_01','中常侍严震参决'),('sl_02','中常侍严震赵升'),('tj_15','严震之权过于主相'),('tj_17','中常待严震、赵升')],7724:[('js_13','群字敬林，严叔父也'),('ssxy_01','孔群在横塘为匡术所逼'),('ssxy_03','鸿胪卿孔群好饮酒')],7725:[('wsh_05','逊昵延死，子乞得龟立'),('wsh_05','别部人逸豆归杀乞得龟而自立'),('sl_02','宇文乞得归[一作龟]'),('tj_15','宇文乞得归为其东部大人逸豆归所逐'),('sl_01','勒加宇文乞得归官爵使之击慕容廆'),('tj_14','乞得归据浇水以拒皝')],}
    for ident,readings in round12d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第12批D·原書引書及同事異文身份審讀','Actual originals read; unknown years and unlinked careers retained honestly') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7723,identity_status='all_primary_key_contexts_read',last_manual_review_batch='round12d')
    results[7721]['unresolved'].append('太平御覽引燕書周存字道名、渔陽人，尋母離段投慕容廆又辭去；文中存亡是問母消息，不是本人卒，王彭祖亂/期月停留不倒推生卒。')
    results[7722]['unresolved'].append('呼延那雞劉胤冠軍將軍斷韓璞糧道兩書同人；親御郎二千/三千兵數異文不是不同人，也不當出生/卒年，無本人卒。')
    results[7723]['unresolved'].append('332石勒中常侍嚴震參綜政事、333隔絕與350冉魏中常侍同名同職，生存相容但原文未明歷石虎諸朝之轉仕橋；兩段各已讀，不斷為確定不同人，先不把350卒套整條。')
    yan350=chunk_ev('tj_17','中常待严震、赵升','資治通鑑永和六350四月冉閔誅李農案','同句魏主閔殺李農並群臣，原待字為侍訛，對應春秋中常侍')
    results[7723].update(historical_birth=None,historical_death=None,identity_status='332_later_zhao_and350_ranwei_eunuch_contexts_read_career_bridge_pending',component_identities=[dict(identity='332/333石勒寵中常侍嚴震'),dict(identity='350冉魏中常侍嚴震',historical_death=date(350,yan350,month=4))])
    results[7723]['component_identities'][1]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7723]['evidence'].append(yan350)
    results[7724]['unresolved'].append('孔群敬林孔嚴叔父、孔愉從弟，橫塘匡術及七百斛酒事三key原文互證，同為中丞/鴻臚官履；卒官未獨立明年，王導去世/孔嚴子孫死不套群。')
    gui333=chunk_ev('tj_15','宇文乞得归为其东部大人逸豆归所逐，走死于外','資治通鑑咸和八333宇文易主','歸龜由春秋直注明異文，獨立紀年依通鑑，不將春秋325段後接死事作獨立325卒年')
    results[7725].update(review_status='duplicate',duplicate_of=6224,identity_status='yuwen_qidegui_gui_explicit_text_variant_and_unique_succession',historical_birth=None,historical_death=date(333,gui333),source_identities_all_adjudicated=True)
    results[7725]['evidence'].append(gui333)
    results[7725]['unresolved'].append('已與4000片協調：6224/6372/7725五key逐源實讀，春秋歸一作龜明注及浇水戰/逸豆歸取代確聯，歸並6224。通鑑333走死、魏書逸豆歸殺死亡方式不同；春秋把死事接325戰敗段無獨立卒年，保留壓縮叙次問題，不硬製325精確異說。根11001 Wiki鍵另待根核。')
    results[7725]['source_chronology_notes']=[dict(source='十六國春秋sl02慕容廆紀',issue='325浇水敗後接其東部大人逸豆歸逐而殺之，沒有獨立卒年；通鑑將易主卒事單列333'),dict(source='魏書宇文傳/資治通鑑',issue='同一被逸豆歸取代之主，前者說被殺，後者說走死於外；不擅定唯一死因')]
    round13a_reads={7726:[('js_06','翼薨，大将干瓒'),('js_11','翼字稚恭'),('js_11','翼时年二十二'),('js_12','庾翼将悉'),('js_13','时安西将军庾翼'),('js_17','安西将军庾翼北征'),('js_18','温与庾翼友善'),('sl_02','都督江荆等诸军事庾翼'),('sl_03','庾翼答书'),('sl_09','南郡太守庾翼'),('tj_15','亮使弟翼'),('tj_16','都亭肃侯庾翼'),('yi_01','稚恭至襄阳'),('js_01','庾翼之镇襄阳'),('js_09','殷浩、庾翼'),('js_14','庾翼代亮'),('ssxy_00','庾翼别传'),('ssxy_01','翼娶髙平刘绥女'),('ssxy_02','庾翼名辈'),('ssxy_03','翼率众入沔')],7727:[('tj_15','遣参军徐会'),('js_19','遣参军徐会')],7728:[('wsh_00','昭成皇后慕容氏'),('wsh_00','二十三年夏六月，皇后慕容氏崩')],7729:[('sl_03','友人成泮')],7730:[('hy_00','李奕从兄干'),('sl_09','将军乐次费佗李干'),('sl_09','奕从兄广汉太守干'),('tj_16','李弈从兄广汉太守干'),('wsh_02','广汉太守李干')]}
    for ident,readings in round13a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第13批A·原書本人傳及相連事件逐鍵實讀','Original narrative read including named relative, independent reign context and attribution corrections') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_primary_key_subjects_adjudicated',last_manual_review_batch='round13a')
    yi345=chunk_ev('tj_16','都亭肃侯庾翼疽发于背','資治通鑑永和元345秋七月庚午','本人疽發表子後庚午卒，下一句曹據被殺不能套翼')
    yiage=chunk_ev('js_11','永和元年卒，时年四十一','晉書庾翼本傳','345 - 41 + 1 = 305；另一蘇峻作逆時22歲記載與327白衣備石頭事件推306不同')
    results[7726].update(review_status='primary_text_corroborated',historical_birth=dict(year=None,year_alternatives=[305,306],precision='conflicting_nominal_age_inferences',calendar='historical_CE_year',source='晉書本人傳享年41及作逆時22；通鑑327白衣備石頭',excerpt=yiage['excerpt'],calculations=['345 - 41 + 1 = 305','327 - 22 + 1 = 306'],caveat='前段作逆泛稱亦可能包含328戰敗時；不消除此年齡敘次歧義，不硬定單年'),historical_death=date(345,yi345,month=7,sexagenary_day='庚午'),attested_activity_years=[327,340,343])
    results[7726]['historical_death'].update(precision='lunar_month_sexagenary_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7726]['evidence'].extend([yi345,yiage])
    results[7726]['unresolved'].append('19key全讀核同亮弟稚恭荊州庾翼。世說獻扇武帝故事注已明糾為庾懌，不把獻扇算翼三世紀活動；小庾欲為漢高魏武故事原注也疑傳聞。晉書薦溫於成帝/桓溫傳明帝為帝名異記，339春秋翼遣李松而通鑑亮遣為主官異記，均保原文不拆本人。出生年依卒41推305，前段22與327白衣備石頭對讀推306，但作逆涵328戰敗，故保歧義；首仕在平亂後始辟，327只是白衣領軍活動證據。')
    results[7727]['unresolved'].append('徐會兩key同蘇峻參軍327往祖約結盟推崇；後蘇峻死不套其卒，未見本人生卒。')
    mur360=chunk_ev('wsh_00','二十三年夏六月，皇后慕容氏崩','魏書序紀建國二十三360夏六月','與皇后傳建國二十三卒互證；昭成所娶元真女，不同先前元真妹')
    results[7728].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(360,mur360,month=6))
    results[7728]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7728]['evidence'].append(mur360)
    results[7728]['unresolved'].append('候選明指元真女、献明帝及秦明王母，非前妃元真妹；序紀344長孫秩迎后到和龍與本人傳吻合。下文獻明賀后享年46不套慕容后，出生未知。')
    results[7729]['unresolved'].append('成泮為陽裕友，勸其應段疾陸眷召；後陽裕卒61是裕，非成泮，無成泮本人生卒。')
    results[7730].update(attested_activity_years=[338])
    results[7730]['unresolved'].append('李幹乾四key同成漢李奕從兄、廣漢太守338子廣盟誓後徙漢嘉；春秋並收323軍將同名相容。323皆殺指李琀李稚，不套李幹。華陽魏書春秋干與大臣通謀與通鑑干告大臣謀之敘述異文保留；無生卒，不因323敗軍推死亡。')
    round13b_reads={7731:[('sl_03','产子绩字伯阳'),('tj_17','产子绩为幽州别驾'),('tj_18','伯阳一人，请独裁')],7732:[('js_08','敦密使妖人李脱诬嵩'),('tj_14','王敦诬周嵩、周莛与李脱'),('js_07','即营中杀筵及脱、弘')],7733:[('sl_02','以司马李农为太宰领太尉'),('sl_02','闵诛李农及其三子'),('sl_06','梁犊反败李农于荥阳'),('tj_15','使司空李农旦夕问起居'),('tj_16','抚军将军李农为使持节'),('tj_17','魏主闵杀李农及其三子'),('wsh_01','其司空李农谏'),('wsh_02','使其司空李农领万余骑'),('js_17','又使司空李农旦夕亲问')],7734:[('js_11','云字云子'),('js_12','人有举桓云者'),('tj_18','司徒昱以建武将军桓云代之'),('wsh_02','涣过皖，攻谯国内史桓云')],7735:[('wsh_05','日陆眷死，弟乞珍代立')],7736:[('wsh_05','乞珍死，子务目尘代立')],7737:[('wsh_05','务目尘死，就六眷立')],7738:[('wsh_05','其伯祖日陆眷')],7739:[('wsh_05','欲杀其从叔羽鳞及末波')],7740:[('js_06','惇字思悛'),('js_06','养志二十余年，永和九年卒'),('ssxy_00','江惇曰山遐为东阳'),('ssxy_02','江惇字思悛陈留人')]}
    for ident,readings in round13b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第13批B·本人傳與相鄰帝紀同卷成組實讀','Original actor, kinship and dating context read, not extracted candidate dates') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7734,identity_status='all_listed_primary_subjects_adjudicated' if ident!=7734 else 'wei_shu328_huan_yun_vs_tongjian_huan_xuan_and360_huan_yun',last_manual_review_batch='round13b')
    ji360=chunk_ev('tj_18','出为章武太守，以忧卒','資治通鑑升平四360年末','李產子伯陽，360拒右僕射後出章武憂卒；不獨斷為十一月死')
    results[7731].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(360,ji360))
    results[7731]['evidence'].append(ji360)
    results[7731]['unresolved'].append('三key同范陽李產子伯陽：郡功曹→王午主簿/幽州別駕→太子中庶子/司徒長史→章武。弱冠於石虎伐段遼事只能約年齡，不補精確出生；360年末憂卒，出任十一月非明本人卒月。')
    tuo324=chunk_ev('js_07','即营中杀筵及脱、弘','晉書周札傳與通鑑太寧二324正月周氏案','晉書本人並列李脫被殺，通鑑同案獨立紀年；李八百自稱非真壽命')
    results[7732].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(324,tuo324))
    results[7732]['evidence'].append(tuo324)
    results[7732]['unresolved'].append('三key同李八百方士及王敦陷周氏案。周嵩傳說受命誣人、周札傳說亦同案被殺，角色敘法不同不拆人；八百歲自稱妖術不採出生。324正月為通鑑周氏案，晉書李脫死未獨立明月，保年精度。')
    nong350=chunk_ev('tj_17','魏主闵杀李农及其三子','資治通鑑永和六350夏四月','同冉魏齊王太宰而不是別人同名；與春秋四月段吻合')
    results[7733].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(350,nong350,month=4))
    results[7733]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7733]['evidence'].append(nong350)
    results[7733]['unresolved'].append('八key均同後趙李農司空、佛圖澄問起居、營州牧/凡城、梁犢案/廣宗/代陂到冉魏齊王完整官事鏈。三子同誅無姓名年齡，不套成年或出生；349討梁犢敗非死亡。')
    results[7734]['evidence'].append(chunk_ev('tj_15','涣、抚过皖，因攻谯国内史桓宣','資治通鑑咸和三328','同魏書祖渙過皖之役、桓宣守皖毛寶救；非父桓彝宣城傳，魏書雲字待校'))
    cloud360=chunk_ev('js_11','升平四年卒，赠平南将军','晉書桓彝附桓雲本傳','溫弟雲子360卒，不能套魏書328譙國守雲支')
    results[7734].update(review_status='identity_conflict',historical_birth=None,historical_death=None,component_identities=[dict(identity='桓彝子桓溫弟桓雲字雲子',keys=[k for k in results[7734]['keys'] if not k.startswith('wsh:')],historical_death=date(360,cloud360)),dict(identity='魏書328譙國內史桓雲；通鑑同役為桓宣，待校',keys=[k for k in results[7734]['keys'] if k.startswith('wsh:')])])
    results[7734]['evidence'].append(cloud360)
    results[7734]['unresolved'].append('魏書候選本人就是328守皖譙國內史桓雲；通鑑同役守皖為桓宣，晉書父桓彝則任宣城、並非譙國，不是僅同卷同名。雲子本傳何充參軍/義成/江州至360卒另三key閉合；魏書雲/通鑑宣異文，未校前不套整條360卒。')
    for ident,note in {7735:'段日陸眷弟乞珍代立，卒後子務目塵代，父子次序明確但原文無獨立生卒年，不按繼位表推填。',7736:'段乞珍子務目塵、就六眷父，所統戶/騎數非享年；魏書稱晉穆帝時王浚封公與王浚活動年代不合，保原文帝名問題，不把該句推至東晉穆帝或當卒年。',7737:'魏書就六眷為務目塵子、日陸眷姪孫，襄國擒末波後與石勒盟；死後子幼而匹磾奔喪，但本篇未獨立明卒年，暫不按劉琨死年補318。就/疾等異文需與他ID全key另核，未整條硬去重。',7738:'段日陸眷就六眷伯祖、乞珍兄，遭賣庫辱官家奴後遼西招亡；其死與弟代未獨立明年，不能拿後世部族敘事年代作其生卒。',7739:'段羽鱗為匹磾從叔，與末波俱被謀殺而預防，後相攻；欲殺不等於已殺，段就六眷死非其死，無本人年。'}.items():results[ident]['unresolved'].append(note)
    dun353=chunk_ev('js_06','养志二十余年，永和九年卒，时年四十九','晉書江統附江惇傳','353卒虛歲49推305，世說引徐廣晉紀同49且江虨弟核身份')
    results[7740].update(review_status='primary_text_corroborated',historical_birth=date(305,dun353,age=49),historical_death=date(353,dun353))
    results[7740]['evidence'].append(dun353)
    results[7740]['unresolved'].append('三key均江統子江虨弟思悛，東陽隱者山遐減威猛與本傳避東陽相合。353卒49按虛歲353-49+1=305，非確切生日；蘇峻之亂避地並非首次受職，屢徵不就，不把327當初仕。')
    round13c_reads={7741:[('sl_03','留其将王他以数千人守蓟')],7742:[('tj_17','执王佗，斩之')],7743:[('js_12','彪之字叔武'),('tj_17','吏部尚书王彪之言于会稽王昱'),('tj_18','以太常王彪之为左仆射'),('wsh_02','乃谋于尚书仆射王彪之'),('js_06','代王彪之为尚书仆射'),('js_13','尚书令王彪之等以外寇为谏'),('sg_07','晋永和中廷尉王彪之与扬州刺史殷浩书'),('sl_05','吏部尚书王彪之上会稽王昱笺'),('ssxy_01','王彪之别传曰彪之从伯导'),('ssxy_00','具其草以示仆射王彪之'),('ssxy_03','虎犊彪之小字也彪之字叔虎')],7744:[('sl_02','尚书令王谟侍中王衍及中常侍'),('tj_17','并尚书令王谟、侍中王衍')],7745:[('sl_02','尚书令王谟侍中王衍及中常侍'),('tj_17','并尚书令王谟、侍中王衍')],7747:[('tj_17','赵凉州刺史石宁独据上邽不下')]}
    for ident,readings in round13c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第13批C·同役諸人及王彪之逐源實讀','Read source identity scopes and separate explicit personal dates from neighboring deaths') for ch,phrase in readings)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7743,identity_status='all_original_primary_subjects_adjudicated' if ident!=7743 else '11_primary_keys_wang_bin_son_identity_closed_bare_cbdb414525_unlinked',last_manual_review_batch='round13c')
    tuo350=chunk_ev('tj_17','乙巳，俊拔蓟，执王佗，斩之','資治通鑑永和六350三月乙巳','春秋同王午留王他守薊、俊攻陷斬，異體與獨特事件閉合')
    for ident in [7741,7742]:
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(350,tuo350,month=3,sexagenary_day='乙巳'))
        results[ident]['historical_death'].update(precision='lunar_month_sexagenary_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(tuo350)
        results[ident]['unresolved'].append('王他/佗兩原文同王午留守薊數千人、乙巳燕主俊陷城所斬，非僅同字去重；合7741保來源他字並以佗為別名，生年無據。')
    results[7742].update(review_status='duplicate',duplicate_of=7741,identity_status='wang_tuo_ta_same350_ji_garrison_capture_variant')
    for ident in [7744,7745]:
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(350,nong350,month=4))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(nong350)
        results[ident]['unresolved'].append('兩原書同350四月冉閔誅李農案；王衍侍中不是311西晉王衍，王謨尚書令不是其他時代同名。兩人本人均在被殺並列，無出生。')
    biao377=chunk_ev('js_12','太元二年卒，年七十三','晉書王彪之本傳','377卒73虛歲推305，11主史key均同王彬子白須；裸CBDB未連故只支日期')
    results[7743].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=None,component_identities=[dict(identity='晉王彬子叔武/叔虎王彪之，白須',keys=[k for k in results[7743]['keys'] if not k.startswith('cbdb:')],historical_birth=date(305,biao377,age=73),historical_death=date(377,biao377)),dict(identity='CBDB414525同名裸行未連',keys=['cbdb:414525'])])
    results[7743]['evidence'].extend([biao377,dict(citation='CBDB414525 original row read',excerpt=json.dumps(originals[7743]['cbdb_originals'],ensure_ascii=False))])
    results[7743]['unresolved'].append('11原書key親讀同王彬子白須，晉書叔武/世說引王氏譜叔虎為字異記，父兄及20白鬚足以核同。三國志是裴注引東晉永和書不是三國仍仕；晉書謝安傳宮室終建/本人傳終不建為記事異說不另造人。CBDB414525生日卒年指數官注均空，仍未身份連接；305/377僅保已核原書支，不因任一原書證實套裸鍵。')
    ning350=chunk_ev('tj_17','赵凉州刺史石宁独据上邽不下，十二月，苻雄击斩之','資治通鑑永和六350十二月','下文卷99始永和七351，獨立明35012；同卷351奔柏人骠騎另人')
    results[7747].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(350,ning350,month=12),candidate_duplicate_of=7746)
    results[7747]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7747]['evidence'].append(ning350)
    results[7747]['unresolved'].append('單key明確上邽涼州刺史，350十二月死，351奔柏人為另支；7746本身含兩支不可硬刪合，僅候選拆支重複。')
    round13d_reads={7748:[('js_11','乃遣将军朱绰讨贼别帅于芜湖')],7749:[('tj_15','约遣兄子沛内史涣'),('wsh_02','约命兄逖子沛国内使涣'),('js_19','命逖子沛内史涣')],7750:[('sl_03','主簿程朴请且与赵连和'),('tj_17','主簿程朴请且与赵连和')],7751:[('hy_00','长史略阳罗恒'),('sl_09','征南将军罗恒费黑救之'),('sl_09','长史略阳罗恒'),('tj_15','朱提太守杨术与成将罗恒'),('tj_16','长史略阳罗恒'),('tj_16','罗恒、解思明复议奉晋')],7752:[('js_11','其长史裨惠以郡兵寡弱'),('tj_15','其长史裨惠以郡兵寡弱')],7753:[('tj_15','女婿淮南太守许柳'),('wsh_02','女婿淮南太守许柳'),('js_19','女婿淮南太守许柳'),('ssxy_00','许栁【许氏谱曰栁字季祖')],7754:[('jk_02','长史许穆卒')],7755:[('sl_02','严震赵升等五月'),('tj_17','严震、赵升。闵遣使')]}
    for ident,qs in round13d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第13批D·同役將佐與原注姓名實讀','Full actor passages read, deaths of defeated rivals and descendants excluded') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_primary_key_subjects_adjudicated',last_manual_review_batch='round13d')
    for ident,note in {7748:'桓彝將朱綽即候選硃綽，蘇峻亂奉命於蕪湖破敵；桓彝後死與朱綽無關，無本人生卒。',7749:'三key同祖逖子、祖約兄子沛內史祖渙，參蘇峻及皖桓宣役。後祖約全族百餘人誅未列渙在場，不拿祖約330族誅當渙精確卒年。',7751:'四key同成漢羅恒、台登將及略陽長史/尚書令；327楊術死、338李期死、339李演被殺均非恒，生卒未知。華陽鮮思明/他書解思明為同搭檔異文不影響恒身份。',7752:'兩書桓彝長史裨惠均勸按甲、後勸通和，兩說被拒不代表本人被殺；桓彝死不套惠，無本人生卒。',7753:'四key許柳栁、字季祖許允孫許猛子、蘇峻丹陽尹事閉合。妻為祖約女（晉書通鑑）/祖逖子渙女（世說引劉謙之）為親屬異記，保留不擅消；世說本人罪誅未明年，主文後並斬二人是任讓與許永思妣，不套許柳卒329或其子年齡。'}.items():results[ident]['unresolved'].append(note)
    pu349=chunk_ev('tj_17','吾不堪为天子邪，而云列国乎！”引朴斩之','資治通鑑永和五349石鑑即位後','程樸本人斬，位於349十二月褚裒卒之前；無獨立卒月')
    results[7750].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(349,pu349))
    results[7750]['evidence'].append(pu349)
    mu376=chunk_ev('jk_02','年七十二解驾违世','建康實錄太元元376年許穆本傳','是歲許穆卒下附本人，虛歲72推305；道教解駕語解作該傳死亡紀錄，不作出生確日')
    results[7754].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=dict(year=None,year_alternatives=[370,376],precision='source_context_and_annotation_disagreement',calendar='historical_CE_year',source=mu376['citation'],excerpt=mu376['excerpt'],caveat='正文376是歲卒，原注真誥370隱化；注中姓名亦缺字，需校本，不能逕以宗教語排除歧義'),reported_nominal_death_age=72)
    results[7754]['evidence'].extend([mu376,chunk_ev('jk_02','以太和五年扵茅山北隐化','建康實錄許穆條原注引真誥','正文376與注370隱化歧義及缺字姓名待校，不用單一生卒值')])
    results[7754]['unresolved'].append('許穆一名謐、思玄許邁弟，建康實錄太元元年明列是歲卒、附傳72；正文376與72歲可按虛歲推305，但原注引真誥太和五370茅山北隱化且姓名許阙缺字，隱化可能死亡另說未能排除，保370/376源別待校而撤單年與出生推值；不把許翙或兒孫得道年齡套本人。')
    results[7755].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(350,nong350,month=4))
    results[7755]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7755]['evidence'].append(nong350)
    results[7755]['unresolved'].append('兩書同冉魏中常侍趙升，350四月李農案並列被殺；無生年，不按宦官職名推年齡。')
    round13e_reads={7756:[('sl_09','涪陵太守巴西赵弼为龙所擒'),('hy_01','涪陵太守巴西赵弼')],7757:[('sz_00','道容，本历阳人，住乌江寺')],7758:[('sl_02','追斩靳豚于阴安乡'),('tj_17','追斩靳豚于阴安')],7759:[('sl_02','其子伯阳亦遇害'),('tj_17','诛𫍲及其子伯阳'),('js_16','并杀其子伯阳')],7760:[('sl_01','黄门郎韦𫍲驳曰'),('sl_02','韦𫍲字宪道京兆人'),('tj_16','侍中京兆韦𫍲谏曰'),('tj_17','诛𫍲及其子伯阳'),('js_16','韦𫍲，字宪道，京兆人')],7761:[('sl_00','没于黑匿郁鞠部'),('sl_01','索头郁鞠率众三万来降'),('sl_02','讨索头郁鞠克之'),('tj_14','没于黑匿郁鞠部'),('tj_15','索头郁鞠帅众三万降于赵')],7762:[('sl_02','卢江黄泓北平阳耽为谋主'),('sl_03','黄泓字始长'),('tj_12','庐江黄泓、代郡鲁昌为谋主'),('tj_14','黄泓、郑林参军事'),('tj_17','从事中郎黄泓曰'),('tj_18','燕故太史黄泓叹曰'),('js_17','黄泓，字始长'),('sl_06','年九岁问太史令黄泓'),('sl_07','太史令黄泓善相')],7763:[('sz_00','遣弟子不如檀。晋言'),('gs_00','遣弟子不如檀。此言法饶'),('sl_02','道士法饶进曰'),('tj_17','道士法饶进曰')]}
    for ident,qs in round13e_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第13批E·同卷本人及宗教原傳逐鍵審讀','Actually read source biographies, lifespan claims and explicit historical action context') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident not in [7761,7763],identity_status='all_original_primary_subjects_adjudicated' if ident not in [7761,7763] else 'different_source_subject_branches_not_collapsed',last_manual_review_batch='round13e')
    results[7756]['unresolved'].append('兩原書同巴西人涪陵守趙弼被張龍擒後屈服；華陽段末數年卒主語侯馥，不是被擒諸太守趙弼，不能套卒年。')
    results[7757]['unresolved'].append('本人為尼僧、歷陽人烏江/新林道容，原書明帝至簡文孝武事無精確生卒；太元中絕跡不知所在及敕葬衣缽不是確切死亡年，不套前後尼傳70或75歲。')
    tun350=chunk_ev('tj_17','追斩靳豚于阴安','資治通鑑永和六350蒼亭之戰','靳豚本人被追斬不是僅士卒戰損統計')
    results[7758].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(350,tun350))
    results[7758]['evidence'].append(tun350)
    wei350=chunk_ev('tj_17','闵方欲抚纳群胡，大怒，诛𫍲及其子伯阳','資治通鑑永和六350十一月','緊接十一月攻襄國立胤單于，父子本人同誅；351追贈不算父仍活')
    for ident in [7759,7760]:
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(350,wei350,month=11))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(wei350)
        results[ident]['unresolved'].append('韋謏原書字𫍲，字憲道京兆；前趙黃門郎→石氏黃門/侍中九列→冉魏光祿大夫，父子伯陽官事全閉合。350十一月同誅，351追贈大司徒為後事；七郡/四九列/六尚書次數非享年。')
    results[7761].update(review_status='identity_conflict',historical_birth=None,historical_death=None,component_identities=[dict(identity='黑匿部郁鞠收留劉胤、遣歸劉曜受左賢王'),dict(identity='索頭郁鞠337三萬降趙、後張舉自雁門討克')])
    results[7761]['unresolved'].append('五key全原文已讀，但黑匿與索頭族部名不同，323/324前趙左賢王至337降趙/342受討未見轉部承接，不斷為必不同亦不以同名整合；討克不直接明本人死亡。')
    results[7762].update(reported_nominal_death_age=97,attested_activity_years=[349,370])
    results[7762]['unresolved'].append('九key同慕容氏太史始長，參軍/太白天象/慕容農九歲問答/相慕容德等官事閉合；廬江與魏郡斥丘為籍貫異記。本人傳97卒並稱卒後三年慕容垂興，但興可指383起兵或384建號，且三年數法待校，不硬定單一出生卒年。370燕亡歸家是本人實際活動，不是後世追錄。')
    fa351=chunk_ev('tj_17','闵支解法饶父子','資治通鑑永和七351襄國敗後','道士法饒與其子被支解，與三世紀朱士行僧弟子無身份橋')
    results[7763].update(review_status='identity_conflict',historical_birth=None,historical_death=None,component_identities=[dict(identity='不如檀、漢譯法饒，朱士行弟子送般若梵本洛陽',keys=[k for k in results[7763]['keys'] if k.startswith(('sz:','gs:'))]),dict(identity='351冉魏道士法饒，勸冉閔出戰、敗後父子支解',keys=[k for k in results[7763]['keys'] if k.startswith(('sl:','tj:'))],historical_death=date(351,fa351))])
    results[7763]['evidence'].append(fa351)
    results[7763]['unresolved'].append('四key是三世紀西行僧弟子與351冉魏方術道士兩個角色，時間相距近百年且僧傳身份無橋，不按法饒譯名合整人；朱士行80卒是師父，不如檀無生日卒年，351僅道士支。')
    round13f_reads={7765:[('sl_02','大单于胤及左仆射刘琦等送于祗尽杀之'),('tj_17','降胡栗特康等执大单于胤及左仆射刘琦'),('js_16','闵拜其子胤为大单于')],7766:[('sl_02','尚书刘钦刘休等及诸将士死者')],7767:[('sl_02','尚书刘钦刘休等及诸将士死者')],7768:[('sl_02','大单于胤及左仆射刘琦等送于祗尽杀之'),('tj_17','降胡栗特康等执大单于胤及左仆射刘琦')],7769:[('js_12','父耽，晋陵太守'),('js_12','使内史刘耽尽以一部谷'),('js_09','以沛国刘耽为司马'),('js_14','以沛国刘耽为司马'),('sg_10','次耽晋陵内史耽子恢字真长')],7770:[('js_17','导复引匡术弟孝'),('tj_15','其将匡孝分兵先薄赵胤军'),('js_19','峻与匡孝将八千人逆战')],7771:[('js_13','岳与周抚俱奔蛮王向蚕'),('js_07','蛮酋向蚕纳之')],7772:[('tj_14','义兴人周蹇杀王敦所署太守刘芳')],7773:[('tj_15','遣将军夏侯长等间行'),('js_09','乃遣将军夏侯长等间行')],7774:[('sl_01','荥阳太守尹矩野王太守张进'),('tj_15','后赵荥阳太守尹矩')],7775:[('js_11','条字幼序'),('tj_15','亮与弟怿、条、翼及郭默')]}
    for ident,qs in round13f_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第13批F·襄國同役及蘇峻亂諸將實讀','Original text per source-key scope read; family members ages and enemy deaths excluded') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_primary_key_subjects_adjudicated',last_manual_review_batch='round13f')
    yin351=chunk_ev('tj_17','赵王祗杀之','資治通鑑永和七351襄國敗後','前句明指大單于冉胤及左僕射劉琦被執送趙，兩人非泛將士死者')
    soldiers351=chunk_ev('sl_02','尚书刘钦刘休等及诸将士死者','十六國春秋永興二351襄國敗','劉欽劉休兩尚書具名在死亡並列，不由泛兵數推卒')
    for ident in [7765,7766,7767,7768]:
        ev=yin351 if ident in [7765,7768] else soldiers351
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,ev))
        results[ident]['evidence'].append(ev)
        results[ident]['unresolved'].append('351襄國之役本人死亡明列：冉胤/劉琦被栗特康送石祗處死，劉欽/劉休具名列戰死尚書；不互套官銜/親屬，不把韋父子350死套冉胤。無本人出生，未擅補日月。')
    for ident,note in {7769:'四key劉宏子、劉惔父、沛國劉耽相合；東海王沖司馬至晉陵內史官履相容，劉惔本傳沛國相人父耽與裴注劉宏次子耽晋陵內史補橋。裴注子恢字真長與惔同字為名文異記，不另拆。張闓64卒與司馬沖31卒不是耽，無本人生卒。',7770:'三原書匡術弟孝與蘇峻將同名、時代/軍人身份相容，王導引用後王蒙以軍國文武異容進諫，沒有另一匡孝的身份矛盾；明確死於矛的是蘇峻，不能套328卒。王濛39歲死亦不套孝。',7771:'兩書同西陽蠻王向蠶，324納鄧岳周撫拒殺窮歸客，無生日卒年；周抚後365死不套向蠶。',7772:'周蹇義興人324反王敦而殺其太守劉芳；劉芳被殺不是周蹇卒，無生卒。',7773:'兩原書同郗鑑將夏侯長328間行溫嶠獻截糧策，百日預言為賊軍潰期不算本人年齡或死亡年。',7774:'兩書同328後趙滎陽守尹矩與張進降劉曜；劉曜戰敗/石瞻死不是尹矩，無本人生卒。',7775:'兩key庾亮弟幼序條、328與怿翼同奔尋陽，後討黃韜/李高；卒官未獨立明年，前後怿342翼345之卒不套條。'}.items():results[ident]['unresolved'].append(note)
    round14a_reads={7776:[('js_12','前陵江将军张悊为峻收兵于吴')],7777:[('js_11','讨峻党张曜'),('tj_15','击峻党张曜')],7778:[('jk_01','分遣管商张瑾等东冦钱塘吴县'),('wsh_02','瑾、商等破庾冰前军于无锡'),('js_12','而贼率张健、马流攻无锡')],7779:[('sl_02','征北大将军张举攻燕凡城'),('sl_02','太保张举太宰赵鹿'),('sl_03','遣征北张举袭凡城'),('sl_03','俊知张举之妄而杀之'),('tj_16','征北大将军张举攻燕凡城'),('tj_17','豺与太尉张举谋诛司空李农'),('tj_17','俊乃知张举之妄而杀之')],7780:[('sl_01','尹矩野王太守张进'),('tj_15','野王太守张进等皆降之'),('sl_02','张进元城屠各人')],7781:[('tj_15','侃部将彭世、李千'),('wsh_02','军士彭世、李千'),('js_09','李阳部将彭世斩峻于阵'),('js_19','牙门彭世、李千')],7782:[('js_12','众乃遣郎中徐机告谟'),('sl_02','司空璞尚书令徐机'),('tj_17','尚书令徐机、中书监卢谌')],7783:[('js_12','与将军徐逊、陈孺')],7784:[('tj_15','参军曹纳曰'),('js_09','参军曹纳以为大业京口之捍')],7785:[('js_12','骁将李闳、曹统、周光'),('js_00','宗正曹统尚之')]}
    for ident,qs in round14a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第14批A·同卷連續審讀本人身份範圍','Every listed source actor read; unclear career links kept pending and death only within established branch') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident not in [7780,7782,7785],identity_status='all_primary_key_subjects_adjudicated' if ident not in [7780,7782,7785] else 'all_source_passages_read_but_career_bridge_pending',last_manual_review_batch='round14a')
    for ident,note in {7776:'張悊蘇峻收兵吳郡後從顧眾反正、高莋破弘徽，沒有本人生卒；不可因敵軍崩潰套死。',7777:'兩key同蘇峻張曜擊敗王彰，王彰是敗將不是曜死亡，無生卒。',7778:'建康實錄魏書同蘇峻管商搭檔、東寇破無錫張瑾；晉書同無錫役稱張健，為6595候選字形異文，已與4000片聯絡其全key未核，暫不硬合。',7781:'四書同328矛擊蘇峻之彭世，陶侃部將/李陽部將/牙門/軍士上下隸屬相容；被斬是蘇峻不是彭世，未知生卒。',7783:'徐遜王舒所遣將軍、與允之陳孺朱焘武康破賊，無本人生卒，王舒卒或朱焘後年不套徐遜。',7784:'兩原書同郗鑑參軍曹納，勸撤廣陵而將斬後釋，未實際處斬，不能記328卒。'}.items():results[ident]['unresolved'].append(note)
    results[7778]['candidate_duplicate_of']=6595
    ju351=chunk_ev('tj_17','俊乃知张举之妄而杀之','資治通鑑永和七351燕知璽在鄴','與春秋燕錄同乞援詐璽；趙錄四月劉顯殺張舉異記亦351，無共同確月')
    results[7779].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,ju351),source_chronology_notes=[dict(source='春秋後趙錄sl02',reported_death_context='351四月劉顯殺祗及太保張舉'),dict(source='春秋前燕錄sl03及通鑑tj17',reported_death_context='351悦綰還後慕容俊因詐璽誅，叙於八月後十一月前')])
    results[7779]['evidence'].append(ju351)
    results[7779]['unresolved'].append('四key同後趙征北大將軍/太尉、李農友、襄國使燕乞師張舉。卒年各支均351，但趙錄載劉顯四月殺、燕錄與通鑑載俊因詐璽殺，月/死因異說保留，不硬定月日；非漢末張舉。')
    results[7780].update(source_identities_all_adjudicated=False,identity_status='328_yewang_governor_and_zhao_inspector_zhang_pili_career_not_linked',component_identities=[dict(identity='328野王太守張進與尹矩降劉曜'),dict(identity='元城屠各刺奸外部都督張霹靂張進')])
    results[7780]['unresolved'].append('三key全讀，同後趙守與刺奸官職不互斥，但原文無任官橋，不能光以同名核全；均無生卒。')
    results[7782].update(review_status='identity_conflict',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,component_identities=[dict(identity='328顧眾郎中徐機通蔡謨謀起義'),dict(identity='351冉魏尚書令徐機',historical_death=date(351,soldiers351))])
    results[7782]['unresolved'].append('晉郎中與冉魏尚書令兩段都已讀，轉仕可能但未見跨朝履歷，不斷必不同亦不把351卒套前支，日期只保冉魏支。')
    results[7785].update(source_identities_all_adjudicated=False,identity_status='jin_princess_husband_and328_general_unlinked',component_identities=[dict(identity='宗正曹統尚臨海公主'),dict(identity='328李閎周光同列驍將曹統')])
    results[7785]['unresolved'].append('兩原文均晉人而無矛盾，但宗正尚主與驍將未見連接家屬/任官橋，先保待核；臨海公主流落非曹統本人受賣，未知生卒。')
    round14b_reads={7786:[('js_12','遣督护朱祈等九军')],7787:[('tj_15','侃部将彭世、李千'),('wsh_02','军士彭世、李千'),('js_19','牙门彭世、李千')],7788:[('js_17','遣部将李概将兵到卢江')],7789:[('sl_02','侍中李𬘭中书监卢谌')],7790:[('js_13','宣遣子戎重请'),('tj_15','宣乃遣其子戎与随俱迎陶侃')],7791:[('js_13','约遣司马桓抚等迎之'),('tj_15','约遣司马桓抚等迎之')],7792:[('js_11','浩父羡为长沙'),('tj_15','羡，融之兄也'),('tj_16','殷羡为长沙相'),('js_09','长史殷羡曰'),('js_12','父羡，字洪乔'),('js_16','太守殷羡见宣茅茨不完'),('ssxy_00','豫章太守殷羡见宣茅茨不完'),('ssxy_03','羡字洪乔陈郡人')],7793:[('js_00','丹杨尹殷融'),('js_00','太常殷融议依郑玄义'),('js_11','殷融谏之'),('tj_15','亮司马陈郡殷融'),('tj_16','丹杨尹殷融谏之'),('js_09','殷融诣侃谢曰'),('js_10','丹阳尹殷融议曰'),('js_12','与叔父融俱好'),('js_14','祖融，太常卿'),('ssxy_01','殷融字洪逺陈郡人'),('ssxy_02','殷洪逺何如曰逺有致思'),('ssxy_03','殷融已见')],7794:[('js_12','遣督护沈伊距管商于吴县')],7795:[('tj_15','峤从弟充曰'),('js_09','峤从弟充言于峤')]}
    for ident,qs in round14b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第14批B·殷氏親屬主幹及同役原書逐源讀核','Read originals, sibling/father-son identities and named actors rather than applying neighboring dates') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_primary_subjects_adjudicated',last_manual_review_batch='round14b')
    for ident,note in {7786:'顧眾督護朱祈與李閎庱亭敗馬流陶陽，九軍是部隊數不是本人年；無本人生卒。',7787:'三key同李千與彭世矛擊蘇峻，死者為峻不是千；牙門/軍士為身份層次不矛盾，無生卒。',7788:'李概328祖約遣將到廬江眾散，非本人被殺；戴洋後文預言年齡47是劉胤，與李概無關，未知生卒。',7790:'兩key同桓宣子戎，328求毛寶救父及330與王隨迎陶侃；被疑郭默黨的是父但後釋，不套任何人之死，生卒未知。',7791:'兩key同祖約司馬桓撫，迎蘇峻米被毛寶襲奪及與祖渙攻桓宣；被斬蘇峻、毛寶中箭均不是撫卒，無生卒。',7792:'八key全讀同殷浩父洪喬、殷融兄，陶侃長史→豫章/長沙→光祿勳；名父兄在通鑑與本人傳可互橋，豫章范宣拒改屋與世說原注同人。范宣54卒、陶侃334死、庾翼345死不套殷羨；終光祿勳未明年，未知本人生卒。',7793:'11key全讀同殷羨弟、殷浩叔洪遠，司馬→丹陽尹→太常/吏部尚書，孫殷覬/仲堪；世說主文江左太常父子說與注中兄子浩不合，依原注保叔侄異說不造生卒。太常禮議褚裒父女不是融父女，子孫隆安卒不套祖融，原文卒未明年。',7794:'虞潭督護沈伊328吳縣拒管商失敗，未說被殺，虞潭79卒不套沈伊。',7795:'兩書同溫嶠從弟充，328議推陶侃盟主，無本人生卒，不因溫嶠329卒套從弟。'}.items():results[ident]['unresolved'].append(note)
    li351=chunk_ev('sl_02','侍中李𬘭中书监卢谌','十六國春秋永興二351襄國敗','侍中李綝原簡化𬘭，列於本人具名戰死群臣，非後世同名')
    results[7789].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,li351))
    results[7789]['evidence'].append(li351)
    results[7789]['unresolved'].append('單key本人李綝為冉魏侍中，351襄國敗同盧諶等具名死亡，非盧諶父子或師友年齡；無生年。')
    round14c_reads={7796:[('js_07','并子含，初为庾冰轻车长史'),('js_11','司马滕含攻贼石头城'),('tj_15','含，修之孙也'),('wsh_02','庾冰司马滕含'),('js_09','奋威长史滕含抱天子'),('js_19','轻车长史滕含击破之'),('gs_01','晋长沙太守腾含')],7797:[('js_17','寻而牵腾叛约'),('sl_01','其将牵腾帅众降于苏峻'),('tj_15','其将牵腾帅众出降'),('js_19','其将牵腾率众出降')],7798:[('sl_01','南阳太守王国以郡降'),('sl_09','瓘遣司马张姚王国'),('tj_15','南阳太守王国'),('tj_17','瓘遣司马张姚、王国')],7799:[('js_11','南郡太守王愆期'),('js_13','亮遣司马王愆期'),('js_13','南蛮校尉王愆期守江陵'),('sl_01','征西司马王愆期'),('tj_15','以后事付右司马王愆期'),('wsh_02','督护王愆期'),('js_05','长子愆期，流寓江南'),('js_09','以后事付右司马王愆期'),('ssxy_03','遣叅军王愆期推征西陶侃')],7800:[('sl_02','有告王泰招集秦人将奔闗中'),('tj_17','会有告王泰欲叛入秦者')],7801:[('sl_02','少府王鬰尚书刘钦'),('tj_17','少府王郁帅众数千守鉴')],7802:[('sl_01','赵胤遣将军甘留讨祖约')],7803:[('sl_02','杀闵宁北将军白同'),('sl_03','恪克中山斩白同')],7804:[('sl_02','石朴字玄真渤海南皮人'),('js_00','苞曾孙朴字玄真'),('sl_08','侍中石璞切谏止之'),('tj_16','璞，苞之曾孙也'),('tj_17','胡睦及司空石璞'),('js_08','渤海石璞')],7805:[('sl_02','丞相乐安王炳'),('tj_17','丞相安乐王炳')]}
    for ident,qs in round14c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第14批C·本人源傳及跨朝角色審讀','Actual original passage read, excluding corrupt cached key and unbridged family or career subjects') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident not in [7796,7798,7799],identity_status='all_original_primary_subjects_adjudicated' if ident not in [7796,7798,7799] else 'primary_branches_read_some_source_link_or_text_integrity_pending',last_manual_review_batch='round14c')
    results[7796]['unresolved'].append('六晉書通鑑魏書key同滕修孫、滕並子含、庾冰輕車長史到廣州刺史，卒官諡戴無獨立年；抱帝歸舟為本人/部將曹據另記，不推本人生卒。sz02本人段後於round15c按叙事找回，姓名OCR勝舍與gs01騰含對勘參後項更正，不再認定正文缺失；長沙守與前六key人物仍缺橋。')
    results[7796]['source_file_integrity_issues']=[dict(key='sz:sz_02:1:滕含',path=str(SOURCE/'_cache/chunks/sz_02.txt'),issue='Cached chapter contains detached collated glyphs/table of contents; specified biography passage not recoverable in this chunk; not evidence of historical absence')]
    results[7797]['unresolved'].append('四key同祖約部將牽騰、約遁後率眾出降，春秋作降蘇峻與通鑑/晉書晉軍受降敘次異說；不憑受降方異文造兩個同名部將，亦不套祖約後族誅為牽騰死。')
    results[7798].update(source_identities_all_adjudicated=False,identity_status='328_nanyang_governor_and355_liang_sima_unlinked',component_identities=[dict(identity='328晉南陽太守王國降石堪'),dict(identity='355前涼張瓘司馬王國與張姚討馬基')])
    results[7798]['unresolved'].append('四key實讀為兩組：328南陽守/355前涼司馬，sl09開篇實是前涼錄段不是成漢將；相距27年、官履可能但未連轉仕，不用同名套全；馬基田旋死並非王國卒。')
    results[7799].update(source_identities_all_adjudicated=False,identity_status='seven_jin_general_keys_closed_wang_jie_son_commentator_link_pending',component_identities=[dict(identity='晉溫嶠陶侃督護/右司馬、庾氏司馬南郡守南蠻校尉王愆期',keys=[k for k in results[7799]['keys'] if not k.startswith('js:js_05:')]),dict(identity='王接長子王愆期，渡江注公羊列女後傳',keys=[k for k in results[7799]['keys'] if k.startswith('js:js_05:')])])
    results[7799]['unresolved'].append('八key全段實讀，七條軍政角色互有官事鏈；王接長子注經者稀見同名、時代可相容，但此原傳未寫軍職，軍傳未寫父王接，暫留身份連接待補，不斷為必不同。桓宣被替未赴任憤卒不是王愆期病卒，陶侃33476歲亦不套本人。')
    tai351=chunk_ev('tj_17','会有告王泰欲叛入秦者，闵杀之','資治通鑑永和七351襄國敗後','本人後被冉閔殺，不是襄國軍中死也不是王泰殺石琨')
    results[7800].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,tai351))
    results[7800]['evidence'].append(tai351)
    results[7800]['unresolved'].append('兩key同冉魏衛將軍諫固壘、病不出/被告欲奔秦而夷三族，351本人殺；石琨邯鄲敗軍萬人不是王泰死，無生年。')
    yu351=chunk_ev('sl_02','少府王鬰尚书刘钦刘休等及诸将士死者','十六國春秋永興二351襄國敗','少府王鬱本人具名死亡，與349御龍觀監石鑑同官身份')
    results[7801].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,yu351))
    results[7801]['evidence'].append(yu351)
    results[7801]['unresolved'].append('王鬱郁鬰字形同少府，349與王簡監石鑑、351列戰死，兩key官事無衝突；無出生年。')
    results[7802].update(candidate_duplicate_of=None)
    results[7802]['unresolved'].append('甘留為趙胤遣將攻祖約，與通鑑/晉書甘苗同役疑名訛；未核甘苗全部keys之前不整條硬去重，祖約敗奔不是甘留死。')
    bai351=chunk_ev('sl_03','恪克中山斩白同','十六國春秋永和七351秋中山役','白同本人被斬，侯龕出降非侯龕同死；張舉被殺另事')
    results[7803].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,bai351))
    results[7803]['evidence'].append(bai351)
    results[7803]['unresolved'].append('兩key同冉魏寧北將軍白同固守中山，恪/彪攻城主將敘法不同保留但同役被斬本人清楚，未強定卒月。')
    pu351=chunk_ev('tj_17','胡睦及司空石璞、尚书令徐机','資治通鑑永和七351襄國敗','司空石璞具名戰死，與春秋傳冉魏司空羌胡亂被軍士殺同年異說死因')
    results[7804].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,pu351))
    results[7804]['evidence'].append(pu351)
    results[7804]['unresolved'].append('六key明石樸一作璞、玄真渤海南皮石苞曾孫，由晉書本傳/通鑑父系接合至後趙侍中/冉魏司空；不是僅同姓石勒宗親。春秋軍士殺/通鑑襄國戰死方式異記、司徒/司空為官履來源差異，卒年351據獨立通鑑，未推出生。')
    bing351=chunk_ev('sl_02','夏四月刘显杀祗及丞相乐安王炳','十六國春秋永興二351四月','炳與祗俱虎庶子明注、本人具名被殺，通鑑安樂爵字倒文')
    results[7805].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,bing351,month=4))
    results[7805]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7805]['evidence'].append(bing351)
    results[7805]['unresolved'].append('石炳兩key後趙丞相同劉顯襄國案，安樂/樂安爵次異文不是兩王；不同304石冰，351四月殺無生年。')
    round14d_reads={7806:[('sl_01','右卫将军石逺等帅骑卫曜而北')],7807:[('js_11','司马朱焘为南蛮校尉'),('js_12','扬烈司马朱焘'),('js_13','司马朱焘等共平之'),('js_13','父焘，以才干历西蛮校尉'),('sl_09','周抚与龙骧朱焘击贲斩之'),('tj_16','司马义阳朱焘为南蛮校尉'),('tj_17','龙骧将军朱焘出范贲'),('js_07','龙骧将军朱焘击破斩之')],7808:[('js_13','约遣祖焕、桓抚等欲袭湓口'),('js_17','约到历阳，祖焕问洋')],7809:[('js_12','临平人范明亦谓众曰')],7810:[('js_11','范汪谓冰曰'),('js_12','范汪，字玄平'),('js_12','时年六十五，卒于家'),('tj_15','会南阳范汪至寻阳'),('tj_16','范汪谓冰曰'),('tj_18','坐失期，免为庶人，遂废，卒于家'),('js_13','吏部尚书范汪举安'),('js_16','稚少知名，辟大将军掾，早卒。子汪'),('ssxy_03','范汪别曰汪字平')],7811:[('js_13','鄱阳太守纪睦'),('wsh_02','鄱阳太守纪睦'),('js_11','纪睦、徐宁奉王使纠罪人')],7812:[('sl_08','理曹郎中索询諌曰'),('tj_15','理曹郎中索询谏曰'),('js_15','理曹郎中索询谏曰')],7813:[('sl_02','尚书胡睦进曰'),('sl_02','车骑将军胡睦侍中'),('tj_17','尚书胡睦进曰'),('tj_17','胡睦及司空石璞')],7814:[('sl_01','晋南阳都尉董幼叛'),('sl_01','南蛮校尉董幼寇襄阳')],7815:[('js_18','潭即以子楚为督护')]}
    for ident,qs in round14d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第14批D·史書同傳與原名異文逐人審讀','Original paragraphs and dating context read, including age and career language without inferred unknown years') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_primary_key_subjects_adjudicated',last_manual_review_batch='round14d')
    for ident,note in {7806:'石遠趙右衛將軍328勒擒劉曜後護送曜北，曜被擒非石遠亡，無本人生卒。',7807:'七key硃朱燾同義陽庾翼司馬/南蠻校尉、345與江虨毛穆之平干戴、349龍驤將軍與周撫伐范賁，朱序父西蠻/益州官履相合。被殺曹據、范賁及周撫365死非朱燾，未得其本人生卒。',7809:'范明範明臨平人，328議紫壁險要後為顧眾參軍，率500宗黨；斬錢弘非范明死亡，後顧眾34673歲不套明。',7810:'八key皆范晷孫范稚子玄平，早年依庾氏→328護軍/平南二府始解褐→庾亮佐→346留溫府→東陽/徐兗→361失期免→姑孰訪溫→65卒。本人傳無獨立卒年，通鑑361條遂廢卒於家為概述後事，不能把361直接當死亡並倒推297，更不能套旁文荀羨35838歲。卒65保享年，出生卒年仍未知。',7811:'三key同溫嶠鄱陽守紀睦出舟軍與後奉王使糾察被罷，名字官履相容。僅被免官不算被殺，無生卒。',7812:'三key同前涼理曹郎中索詢328諫張駿勿襲劉曜秦雍、張駿羊酒禮，非劉曜或張駿死亡，不補生卒。',7814:'董幼328晉南陽都尉率襄陽降石堪、330後趙南蠻校尉與郭敬攻襄陽同原書明官地承接；降軍非本人死，未知生卒。',7815:'孫氏傳明虞潭子楚為督護，原文確有此名；不逕併虞仡或周楚。祖母孫氏咸和末95歲死是祖母，不套孫虞楚，無生卒。'}.items():results[ident]['unresolved'].append(note)
    results[7807]['attested_activity_years']=[328,345,349]
    results[7808].update(review_status='duplicate',duplicate_of=7749,identity_status='zu_huan_huan_same_zu_yue_ally_event_variant',historical_birth=None,historical_death=None)
    results[7808]['unresolved'].append('祖煥兩key同祖約黨：與桓撫襲湓口攻桓宣敗毛寶、歷陽問戴洋；與7749祖渙同一獨特事件/上下屬，煥渙異文歸7749，沒有新增歷史人或卒年。')
    results[7810].update(historical_birth=None,historical_death=None,reported_nominal_death_age=65,first_attested_career_year=328,appearance_upper_bound=328,attested_activity_years=[328,339,343,346,361])
    firstfan=chunk_ev('js_12','始解褐，参护军事','晉書范汪本傳對讀通鑑咸和三328','京師亂後西歸尋陽、溫嶠當日辟，明稱始解褐；上界僅軍職登場，不是出生')
    results[7810]['evidence'].append(firstfan)
    hu351=chunk_ev('tj_17','胡睦及司空石璞、尚书令徐机','資治通鑑永和七351襄國敗','胡睦本人列陣亡，長蘆之前敗還不等於那時卒')
    results[7813].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,hu351))
    results[7813]['evidence'].append(hu351)
    results[7813]['unresolved'].append('兩key同冉魏尚書勸登帝位後車騎將軍胡睦，長蘆敗單騎還非死亡，351襄國本人明列戰死，無生年。')
    round14e_reads={7816:[('js_14','耽字彦道'),('tj_15','耽，涣之曾孙也'),('wsh_02','王导使袁耽潜诱纳之'),('js_09','导使参军袁耽潜讽诱永'),('ssxy_03','躭字彦道陈郡阳夏人')],7817:[('tj_15','以其亲信许方等补司马督'),('wsh_02','使乡人许方为司马'),('js_10','所亲信许方等补司马督')],7818:[('js_12','御史中丞谢藻行龙骧将军')],7819:[('js_11','委任赵胤、贾宁等诸将'),('tj_15','用参军贾宁计'),('tj_16','委任诸将赵胤、贾宁等'),('wsh_02','长乐人贾宁劝峻杀王导'),('js_09','路永、匡术、贾宁并说峻'),('js_19','用参军贾宁计'),('ssxy_02','何次道尝送东人瞻望见贾宁')],7820:[('sl_03','武威贾玄硕为左长史'),('tj_17','健以中书令贾玄硕始者不上尊号')],7821:[('tj_17','丞相安乐王炳、太宰赵庶')],7822:[('sl_05','赵焕父琨为秦尚书皇始元年')],7823:[('sl_02','太保张举太宰赵鹿'),('sl_01','孔豚赵鹿支屈六等又赴之')],7824:[('js_11','率将军路永、毛宝'),('sl_01','龙骧将军路永戍牛渚'),('sl_02','晋豫州刺史路永以郡来降'),('tj_15','苏峻腹心路永、匡术、贾宁'),('tj_16','豫州刺史路永叛奔赵'),('js_09','时路永、匡术、贾宁'),('js_14','导使耽潜说路永')],7825:[('js_13','北中郎将郗昙以疾病退还彭城'),('tj_18','东安简伯郗昙卒'),('js_09','先爵封其子昙'),('js_10','昙字重熙'),('js_12','于时郗愔及弟昙奉天师道'),('sl_03','北中郎将郄昙次高平'),('ssxy_02','北中郎郄昙以疾还彭城'),('ssxy_03','郄昙别传曰昙字重熈')]}
    for ident,qs in round14e_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第14批E·世說原注和官事跨書逐鍵核讀','Actually read identity, kinship and year contexts; uncertain later summaries not equated to current annal year') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7823,identity_status='all_original_primary_subjects_adjudicated' if ident!=7823 else 'early_shi_le_bandit_and_late_taizai_career_unlinked',last_manual_review_batch='round14e')
    for ident,note in {7816:'五key同袁涣曾孫冲子彦道、子質、二妹殷浩謝尚妻及博賭助桓溫、王導參軍誘路永完整主幹。卒25但咸康初歷陽游騎事件只是被黜，其後復從事中郎方卒，未獨立明年，不把335當卒再倒推311。',7817:'三key同蘇峻親信/鄉人許方監守成帝、司馬督殿中監，劉超鐘雅遇害不是許方被害，無生卒。',7818:'謝藻御史中丞行龍驤將軍328率軍渡江守嘉興/錢唐，文中斬二軍主未具名，不能當謝藻死，無本人生卒。',7819:'七key同長樂賈寧建寧，王應諸葛瑤故交→蘇峻謀主→先降→王導委任將/新安太守，世說原注提供先降後仕橋。此人不死終上客是何充評語不是卒，何充346死不套寧。',7824:'七key路永蘇峻腹心→袁耽誘降→330隨亮討郭默→335牛渚→345豫州刺史降趙屯壽春，同原書官事連續相容，不因345降敵填卒；未知出生卒年。'}.items():results[ident]['unresolved'].append(note)
    results[7816].update(historical_birth=None,historical_death=None,reported_nominal_death_age=25)
    jia351=chunk_ev('tj_17','使人告玄硕与司马勋通，并其诸子皆杀之','資治通鑑永和七351司馬勳入關役後','左長史至中書令賈玄碩具名被誣殺；四月軍事與被殺未獨立定卒月')
    results[7820].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,jia351))
    results[7820]['evidence'].append(jia351)
    results[7820]['unresolved'].append('兩key武威賈玄碩苻健左長史/中書令、拒秦王號到遭誣通司馬勳同人，351本人與子被殺；諸子未具名年齡不補出生。')
    zhaoshu351=chunk_ev('tj_17','丞相安乐王炳、太宰赵庶等十馀人，传首于邺','資治通鑑永和七351四月劉顯弒祗','太宰趙庶本人同被殺；趙鹿字形另候選有早年支未硬合')
    results[7821].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,zhaoshu351,month=4),candidate_duplicate_of=7823)
    results[7821]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7821]['evidence'].append(zhaoshu351)
    results[7821]['unresolved'].append('單key太宰趙庶奔襄國後351四月被劉顯同石祗石炳殺，與春秋太宰趙鹿同事件異文；7823含十八騎早年人未連，不能整條先刪。')
    kun351=chunk_ev('sl_05','赵焕父琨为秦尚书皇始元年','十六國春秋前秦皇始元351趙煥傳','明父趙琨尚書被司馬勳殺；尋尸者是兒趙煥')
    results[7822].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,kun351))
    results[7822]['evidence'].append(kun351)
    results[7822]['unresolved'].append('趙煥父趙琨351司馬勳入攻秦州獲殺，屍被棄而兒尋得，明本人卒無月日；不是司馬勳或趙煥卒，無出生。')
    results[7823].update(review_status='identity_conflict',historical_birth=None,historical_death=None,component_identities=[dict(identity='約304石勒群盜十八騎趙鹿'),dict(identity='351襄國太宰趙鹿，通鑑同役稱趙庶',historical_death=date(351,zhaoshu351,month=4),related_id=7821)])
    results[7823]['unresolved'].append('兩key早期十八騎趙鹿與晚期太宰趙鹿，年代雖可相容但無爵官轉任橋，且晚支通鑑作庶；不斷為必不同，先不套351整人，7821僅晚支重複候選。')
    tan361=chunk_ev('tj_18','东安简伯郗昙卒','資治通鑑升平五361正月','句在正月戊戌大赦後，二月范汪任命之前，勿誤作二月卒')
    tanage=chunk_ev('js_10','寻卒，年四十二','晉書郗曇本傳','同郗鑑子、重熙、北中郎、徐兗，361-42+1=320虛歲推生')
    results[7825].update(review_status='primary_text_corroborated',historical_birth=date(320,tanage,age=42),historical_death=date(361,tan361,month=1))
    results[7825]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7825]['evidence'].extend([tan361,tanage])
    results[7825]['unresolved'].append('八key全讀郗鑑子重熙、郗愔弟郗璿弟、郗恢父、同北中郎將高平退彭城官役；郗郄字形同人。晉書稱先爵高平侯予子/本人初東安伯爵異記保留不以爵異拆人。361正月卒42按虛歲推320，非生日；謝萬傳相鄰42歲是謝萬，歲數依曇本人傳獨立取。')
    round15a_reads={7826:[('wsh_02','西阳太守邓岱'),('yl_00','广州刺史邓岱书'),('yl_07','刺史邓岱留')],7827:[('sl_03','王午遣其将郑生来拒'),('tj_17','王午遣其将郑生拒战')],7828:[('tj_14','乡人钱举匿之'),('js_16','乡人钱举匿之'),('sl_03','乡人钱举匿之')],7829:[('js_12','左卫将军陈光上疏请伐胡'),('js_13','邵陵人陈光率部落'),('sl_01','颍川陈光起兵攻约'),('tj_15','约将陈光起兵攻约'),('tj_16','左卫将军陈光请伐赵'),('wsh_02','颍川人陈光率其属攻之'),('js_19','颖川人陈光率其属攻之')],7830:[('js_12','将军陈孺率精锐千人增戍海浦')],7831:[('js_18','张茂妻陆氏，吴郡人也'),('tj_14','故吴内史张茂妻陆氏')],7832:[('tj_15','前义兴太守顾从'),('js_12','敦平，除太子中庶子，为义兴太守')],7833:[('sl_02','髙昌屯野王'),('sl_03','高昌为安西将军'),('sl_06','秦将髙昌李厯战于麻田'),('tj_17','高昌、李历战于麻田'),('tj_18','张平、李历、高昌皆遣使降燕'),('tj_18','高昌卒，燕河内太守吕护并其众')],7834:[('js_08','从子雄统其众')],7835:[('tj_15','遣督护龚登帅兵诣峤'),('js_09','遣督护龚登率众赴峤')]}
    for ident,qs in round15a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第15批A·已讀連續原書逐人核身份及年份','Read full original actor/relative contexts, no death inferred from battle defeat or another person age') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7829,identity_status='all_original_primary_subjects_adjudicated' if ident!=7829 else 'three_chen_guang_roles_unlinked',last_manual_review_batch='round15a')
    results[7826].update(review_status='duplicate',duplicate_of=7518,identity_status='deng_dai_yue_explicit_renaming_and_matching_xiyang_guangzhou_career')
    results[7826]['unresolved'].append('三key邓岱西陽/廣州守與7518邓岳同人，已核晉書明後竟改名為岱及本人口伯山/廣州官事。御覽引中興書年81亡是葛洪不是邓岱，不取為本人享年；魏書328西陽與後廣州官履閉合，並入7518保別名。')
    results[7518]['evidence'].extend(results[7826]['evidence'][-3:])
    zheng351=chunk_ev('tj_17','王午遣其将郑生拒战，评击斩之','資治通鑑永和七351秋南安之役','鄭生王午將，慕容評本人擊斬明列')
    results[7827].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(351,zheng351))
    results[7827]['evidence'].append(zheng351)
    results[7827]['unresolved'].append('兩key同王午將鄭生351南安拒慕容評被斬；未擅補月日，無出生。')
    results[7828]['unresolved'].append('三key同吳興沈勁鄉人錢舉324匿勁免株連，後沈勁365死只是追敘錢舉舊事，不表示舉活到365；沈充被吳儒殺亦非錢舉卒，無生卒。')
    results[7829].update(review_status='identity_conflict',historical_birth=None,historical_death=None,component_identities=[dict(identity='328穎川陳光祖約將叛攻約、誤擒閻禿奔勒'),dict(identity='桓宣傳邵陵陳光率數百家降宣'),dict(identity='339晉左衛將軍陳光請伐後趙壽陽')])
    results[7829]['unresolved'].append('七key全段讀；穎川祖約叛將奔勒/邵陵率戶降宣/339晉左衛三支籍貫去向未橋，可能轉仕但不憑同名核全。閻禿被擒不等陳光被殺，各支均無本人生卒。')
    results[7830]['unresolved'].append('陳孺328王舒將，海浦千人/武康三千軍數不當年齡；被斬首數百是敵軍，無生卒。')
    results[7831]['unresolved'].append('兩key張茂妻吳郡陸氏324傾產率夫部曲討沈充、謝不克，明是妻而非其他陸后；張茂被沈充殺非陸氏卒，未知生卒。')
    results[7832].update(candidate_duplicate_of=6138)
    results[7832]['unresolved'].append('單key顧從前義興守328與虞潭蔡謨起義；已讀晉書顧眾傳同前官同交涉同役，疑從眾異文，6138三key仍由4000片核全，暫候選不硬去重；無生卒。')
    chang361=chunk_ev('tj_18','高昌卒，燕河内太守吕护并其众','資治通鑑升平五361二月段','後趙故將高昌反覆燕晉秦、白馬奔滎陽後本人卒明記')
    results[7833].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(361,chang361))
    results[7833]['evidence'].append(chang361)
    results[7833]['unresolved'].append('五key同後趙故將高昌屯野王、前秦將麻田與李歷戰襄、降燕安西後又降晉秦，通鑑明三次轉附可橋。麻田馬中流矢死主語姚襄坐騎，不是高昌或姚襄死；本人生卒只卒361明文，無出生，不改成候選352戰亡。')
    results[7834]['unresolved'].append('魏雄為魏該從子，該病卒後統眾；死的是叔該不是雄，不套328卒或魏該出生。')
    results[7835]['unresolved'].append('兩key同陶侃督護龔登328赴溫嶠又被追回，非王愆期/襲登自動異名；調兵往返無本人生卒。')
    round15b_reads={7836:[('js_03','属冉闵诛石氏'),('js_08','至冉闵败后，群遇害'),('js_13','会冉闵之子智'),('js_17','及冉闵僭号'),('sl_02','石闵字永曽小字棘奴'),('sl_02','辛卯闵至龙城'),('sl_03','己卯闵至蓟'),('sl_06','冉闵之乱弋仲据滠头'),('tj_17','辛卯，燕人斩冉闵于龙城'),('wsh_00','魏郡人冉闵'),('wsh_02','闵本姓冉，乃复其姓'),('hhs_12','冉闵大将军蒋干'),('js_16','至冉闵，又署为光禄大夫'),('js_19','冉闵据邺称魏'),('sl_01','游撃将军石[一作冉]闵'),('tj_16','惟游击将军石闵一军独全'),('gs_01','迄冉闵之乱')],7837:[('sl_02','诛显及其公卿已下百余人'),('sl_06','石祗为刘显所杀'),('tj_17','闵杀显及其公卿己下百馀人')],7839:[('wsh_02','劲贼孔卢、张偏等数十人')],7840:[('wsh_02','劲贼孔卢、张偏等数十人')],7841:[('sl_02','左仆射张干右仆射郎肃皆自杀'),('tj_17','左仆射张干、右仆射郎肃皆自杀')],7842:[('sl_04','勲乃伪请琚于坐杀之'),('tj_17','五月，秦主健攻张琚于宜秋，斩之')],7843:[('sl_01','魏炀帝[讳纥那]'),('tj_14','代王贺傉卒，弟纥那立'),('tj_15','代王纥那奔燕'),('wsh_00','炀皇帝讳纥那立')],7845:[('tj_14','谕降李逖，州境遂安')]}
    for ident,qs in round15b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第15批B·冉魏主幹及原書名/年異文逐鍵實讀','Read each actual original named actor and chronology; late annotations and relatives years not used as personal lifespan') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_primary_subjects_adjudicated',last_manual_review_batch='round15b')
    min352=chunk_ev('tj_17','辛卯，燕人斩冉闵于龙城','資治通鑑永和八352五月辛卯','位於五月秦主攻張琚及燕軍增援後六月戴施前，獨立明卒年月日')
    results[7836].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,min352,month=5,sexagenary_day='辛卯'))
    results[7836]['historical_death'].update(precision='lunar_month_sexagenary_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7836]['evidence'].append(min352)
    results[7836]['unresolved'].append('16key均冉閔永曾棘奴、石氏養孫後復冉姓稱魏。後漢書輿服志是後注引晉陽秋蔣干傳璽，非東漢本人活動；晉書石勒載記是後世政權概述。通鑑tj16闵名瞻與養以為子一句混其父，而同段石閔昌黎獨全及勸除蒲洪主角仍閔；春秋明父瞻字弘武本名良、石虎養子，保父系異文，不自動并父輩。父被俘12歲及328閔甚幼不是本人出生明年。卒352五月辛卯據通鑑；春秋趙錄把同辛卯卒接八月鄴陷後壓縮、前燕錄泛說斬龍城，保叙次/遏陘地名細異不硬另定八月。盧諶350/351為別人死年，不套閔。')
    liuxian352=chunk_ev('sl_02','诛显及其公卿已下百余人','十六國春秋永興三352春正月襄國陷','劉顯本人殺，姚弋仲傳他殺石祗不是劉顯已死351')
    results[7837].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,liuxian352,month=1))
    results[7837]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7837]['evidence'].append(liuxian352)
    results[7837]['unresolved'].append('三key同石祗將劉顯351弒祗後自帝襄國、352被冉閔殺。後秦錄旁文73卒是姚弋仲，不能以劉顯351殺祗當其死，未知出生。')
    for ident in [7838,7844]:
        results[ident].update(review_status='secondary_source_checked',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,identity_status='cbdb_original_row_read_but_ancient_person_identity_not_independently_verified',last_manual_review_batch='round15b',source_reported_estimates=[dict(year=390,precision='approximately_estimated_by_Tackett',source='CBDB original c_notes',qualifier='cc390, c_dy_range=2')])
        results[ident]['evidence'].append(dict(citation='CBDB original row read; not an ancient independent source',excerpt=json.dumps(originals[ident]['cbdb_originals'],ensure_ascii=False)))
        results[ident]['unresolved'].append('原CBDB卒390明為Tackett cc390估算且range=2；index327/type29非出生亦非活動。太守無地無任年不足獨立身份橋，史實生卒留空，遊戲估算保留不冒充史證。')
    for ident,note in {7839:'孔盧單key為蘇碩勁賊擊李陽且浦，後被滕含擊退的是賊軍，未具名說孔盧被殺，無生卒。',7840:'張偏單key同孔盧蘇碩且浦役，張偏不是張健/張瑾自動異名；無個人被殺明文，不套蘇碩329死。',7843:'四key同代炀帝紇那、賀傉弟、325立/329奔宇文/335復立/337奔燕；魏書下一句一年而崩是復立烈帝翳槐，不是紇那。未知出生卒年，不把337失位當卒。',7845:'益州太守李逖叛成後尹奉諭降，殺的是爨量不是李逖；325歸附為活動，不明生卒，不能用叛變/歸降定死亡。'}.items():results[ident]['unresolved'].append(note)
    zhang352=chunk_ev('tj_17','左仆射张干、右仆射郎肃皆自杀','資治通鑑永和八352八月鄴陷','張幹乾同左僕射、王簡郎肅同自殺，庚午開門不是另明本人自殺干支日')
    results[7841].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,zhang352,month=8))
    results[7841]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7841]['evidence'].append(zhang352)
    results[7841]['unresolved'].append('兩key張幹/乾同冉魏左僕射鄴陷自殺，352八月無生年；與張才/張進不合姓名。')
    ju352=chunk_ev('tj_17','五月，秦主健攻张琚于宜秋，斩之','資治通鑑永和八352五月','與春秋皇始二352相同年度，誰殺/何時不同故不採五月套整條')
    results[7842].update(review_status='primary_text_corroborated',source_identities_all_adjudicated=True,historical_birth=None,historical_death=date(352,ju352),source_chronology_notes=[dict(source='春秋sl04皇始二352開篇',reported_event='杜洪說司馬勳於坐殺張琚，杜洪自秦王後被苻健五月殺'),dict(source='通鑑tj17永和八352',reported_event='張琚殺杜洪自秦王，五月苻健宜秋斬張琚')])
    results[7842]['evidence'].append(ju352)
    results[7842]['unresolved'].append('兩key同杜洪馮翊司馬/宜秋張琚、弟張光，身份獨特相合，與前涼張瓘弟張琚分明。原書互換杜洪張琚死/自秦王角色，死亡方式及月異記不可消，但年度兩書均352故只保年，不以通鑑五月硬套。')
    results[7843]['attested_activity_years']=[325,327,335,337]
    round15c_reads={7846:[('js_01','京兆人杜洪以豪族陵琚'),('sl_03','杜洪窃据长安'),('sl_04','五月健率步骑二万攻杜洪于宜秋斩之'),('tj_17','洪自以右族，轻琚，琚遂杀洪'),('wsh_02','时京兆杜洪窃据长安'),('sl_05','时杜洪据长安')],7847:[('sl_02','宁西将军王午'),('sl_03','王午为部将秦兴所杀'),('tj_17','王午为其将秦兴所杀')],7848:[('sl_02','尚书令王简左仆射张干'),('tj_17','尚书王简、少府王郁'),('tj_17','尚书令王简、左仆射张干')],7849:[('tj_15','赵胤遣部将甘苗击祖约于历阳'),('js_19','赵胤复遣将军甘苗从三焦上历阳')],7850:[('sl_00','北羌王盆句除'),('sl_01','北羌王盆句除'),('tj_14','北羌王盆句除')],7851:[('sl_01','常侍眭洪执乂不弹')],7852:[('sl_02','虎小男混永和八年将妻妾数人奔京师')],7853:[('sl_02','汝阴王琨以其妻妾奔晋'),('tj_17','赵汝阴王琨以其妻妾来奔')],7854:[('sl_03','王午为部将秦兴所杀吕护杀兴'),('tj_17','王午为其将秦兴所杀。吕护杀兴')],7855:[('sz_02','春秋七十四，太元五年卒'),('gs_01','晋太元五年卒于都。春秋七十有四')]}
    for ident,qs in round15c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第15批C·原傳/帝紀死亡及源字對勘','Actual source event read, independent death year distinguished from delayed retelling and age not shifted to relatives') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7847,identity_status='all_primary_key_subjects_adjudicated' if ident!=7847 else 'three_primary_wang_wu_keys_read_bare_cbdb_unlinked',last_manual_review_batch='round15c')
    du352=chunk_ev('sl_04','五月健率步骑二万攻杜洪于宜秋斩之','十六國春秋皇始二352五月','與通鑑同352但殺者/先後不同，不取月份套全')
    results[7846].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,du352),source_chronology_notes=[dict(source='晉書司馬勳傳/春秋sl04',reported_event='杜洪說司馬勳殺張琚；杜洪自秦王，352五月苻健殺杜洪'),dict(source='通鑑tj17',reported_event='352張琚殺杜洪而自秦王，五月苻健殺張琚')])
    results[7846]['evidence'].append(du352)
    results[7846]['unresolved'].append('六key京兆杜洪據長安→拒苻健→宜秋、弟郁、與張琚相陵主幹閉合。晉書/春秋與通鑑互換洪琚殺與自秦王角色，保源別死因/月份衝突；兩者有獨立共同352年故僅保年，未任擇五月。根9229若Wiki同此人不丟原文異說。')
    wu352=chunk_ev('tj_17','王午为其将秦兴所杀','資治通鑑永和八352閏十月段','與春秋同秦興弒王午、呂護殺興，非王佗早350死')
    results[7847].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=None,component_identities=[dict(identity='後趙寧西將軍/幽州刺史魯口安國王王午',keys=[k for k in results[7847]['keys'] if not k.startswith('cbdb:')],historical_death=date(352,wu352)),dict(identity='CBDB414005王午裸行未連',keys=['cbdb:414005'])])
    results[7847]['evidence'].extend([wu352,dict(citation='CBDB414005 original row read',excerpt=json.dumps(originals[7847]['cbdb_originals'],ensure_ascii=False))])
    results[7847]['unresolved'].append('三原書王午349勸遵、幽州王佗/李績官事至魯口被秦興殺閉合；CBDB所有日期官注空，未連故352只支。主將王午與被留薊350斬的部將王佗不是一人。')
    jian352=chunk_ev('tj_17','尚书令王简、左仆射张干、右仆射郎肃皆自杀','資治通鑑永和八352八月鄴陷','同349尚書王簡監石鑑至352尚書令，職遷不矛盾')
    results[7848].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,jian352,month=8))
    results[7848]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7848]['evidence'].append(jian352)
    results[7848]['unresolved'].append('兩key同冉魏尚書王簡監石鑑與後尚書令鄴陷自殺，王朗一作異名未藉此并其他王朗，生年未知。')
    results[7849]['unresolved'].append('兩key同趙胤將甘苗歷陽討祖約，與7802春秋甘留同唯一主將/地點/祖約夜遁事件，苗留字異文核同。保較多來源常用苗字為canonical7849，7802→7849。未知生卒。')
    results[7802].update(review_status='duplicate',duplicate_of=7849,source_identities_all_adjudicated=True,identity_status='gan_liu_miao_same_zhao_yin_liyanga_zu_yue_defeat_variant')
    results[7802]['evidence'].extend(results[7849]['evidence'][-2:])
    results[7802]['unresolved'].append('補核7849兩key後，甘留/苗同歷陽趙胤討祖約役閉合，撤候選保整條→7849，無新生卒。')
    results[7850]['unresolved'].append('三key同劉曜安國將軍北羌王盆句除、石他自雁門上郡襲掠3000落，敗軍俘部不是明本人被殺，無生卒。')
    results[7851]['unresolved'].append('眭洪329朝會常侍執乂不彈下理官，僅治罪無明處死，不記329卒；邵恭同被治不是其別名。')
    hun352=chunk_ev('sl_02','虎小男混永和八年将妻妾数人奔京师','十六國春秋石鑑末附後事永和八352','明與妻妾奔京後收廷尉旋斬，無卒月')
    results[7852].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,hun352),candidate_duplicate_of=7853)
    results[7852]['evidence'].append(hun352)
    results[7852]['unresolved'].append('原書小男石混352妻妾奔京旋斬；與同書及通鑑汝陰王琨352妻妾奔晉斬建康市相似，可能混琨異文重複，但未校本證實小男/王爵個體，暫候選不硬刪，不推出生。')
    kun352b=chunk_ev('tj_17','赵汝阴王琨以其妻妾来奔，斩于建康市','資治通鑑永和八352正月襄國後','本人具名汝陰王石琨，被晉殺不是冉閔殺')
    results[7853].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,kun352b,month=1),candidate_duplicate_of=7852)
    results[7853]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7853]['evidence'].append(kun352b)
    results[7853]['unresolved'].append('兩key同石虎汝陰王琨、冉魏諸戰至352襄國滅後來奔建康斬；與7852石混同年近同敘事疑重複待校，小男/爵未證前不直接并。無生年。')
    qin352=chunk_ev('tj_17','王午为其将秦兴所杀。吕护杀兴','資治通鑑永和八352閏月魯口','秦興殺王午後自身被呂護殺，兩個本人分別死亡')
    results[7854].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,qin352))
    results[7854]['evidence'].append(qin352)
    results[7854]['unresolved'].append('兩key秦興王午將，352殺午後立即呂護殺興，非殺人者當然生存未知的泛記，有自己死亡明文；無出生。')
    fayi380=chunk_ev('gs_01','晋太元五年卒于都。春秋七十有四','高僧傳竺法義傳','名僧傳抄亦74太元5卒，380-74+1=307虛歲出生')
    results[7855].update(review_status='primary_text_corroborated',historical_birth=date(307,fayi380,age=74),historical_death=date(380,fayi380))
    results[7855]['evidence'].append(fayi380)
    results[7855]['unresolved'].append('兩原傳竺法義從竺法深、始寧保山、咸安二病念觀音、帝買新亭墓同人；名僧傳抄九歲問13出家/高僧傳13遇深，幼年年齡異記保留。兩者獨立同380卒74，虛歲推307非生日；後宋孝武伐凶幸其寺是後世，非義活至宋。')
    # The sz02 biography was recovered by narrative rather than a name hit.
    szhan=chunk_ev('sz_02','晋长沙大守荆洲胜舍钦安风德','名僧傳抄昙翼傳與高僧傳對勘','sz02姓名OCR作勝舍，與gs01騰含捨宅請道安派昙翼同事；原文段已找回，不再說全文缺失')
    results[7796]['evidence'].append(szhan)
    results[7796]['source_file_integrity_issues']=[dict(key='sz:sz_02:1:滕含',path=str(SOURCE/'_cache/chunks/sz_02.txt'),issue='Biography recovered at normalized offset '+str(szhan['offset'])+'; name OCR 勝舍 and 太守/荊州 glyphs require collation; not a missing biography')]
    results[7796]['unresolved'].append('續查更正：sz02後段有昙翼本人傳並非正文全部缺；原姓名作勝舍，已按捨宅/師徒/寺名與gs01騰含對勘。原先找不到段落描述撤回，人名OCR待校；即使故事身份可接，與前六key軍政滕含仍缺橋，whole維持false。')
    round15d_reads={7856:[('js_10','建康令管旆'),('tj_15','建康令管旆')],7857:[('tj_15','召其将纥扢泥')],7858:[('sl_09','范贲萧敬相继作乱'),('tj_16','振威护军萧敬文杀征虏将军杨谦'),('tj_17','桓温使司马勋助周抚讨萧敬文于涪城'),('js_07','自二月至于八月，乃出降'),('js_17','范贲、萧敬相继作乱'),('sg_12','及萧敬叛乱')],7859:[('sl_03','中山苏林起兵于无极'),('tj_17','中山苏林起兵于无极')],7860:[('sl_01','遣主簿赵封送传国玺')],7861:[('sl_00','将军蒋英辛恕拥众'),('sl_01','将军蒋英辛恕拥众'),('tj_15','将军蒋英、辛恕拥众')],7862:[('sl_01','侍郎邵恭执麾不降')],7863:[('sl_02','右仆射郎肃皆自杀'),('tj_17','右仆射郎肃皆自杀')],7864:[('sl_01','郎闿为光禄大夫'),('sl_02','特进郎闿相谓曰'),('tj_15','郎闿为光禄大夫'),('tj_17','特进郎闿相谓曰')],7865:[('sl_03','苏彦遣其将金光')]}
    for ident,qs in round15d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第15批D·同役逐人原文實讀','Named subject checked in actual text, killing a relative or commander is not this subject death') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round15d')
    xiao352=chunk_ev('tj_17','桓温使司马勋助周抚讨萧敬文于涪城，斩之','資治通鑑永和八352八月段；晉書周撫傳獨立八月出降被斬','347十二月殺楊謙/謹為起兵，352八月本人被斬，諸傳省作蕭敬同涪城/譙秀宕渠事件')
    results[7858].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,xiao352,month=8))
    results[7858]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7858]['evidence'].append(xiao352)
    results[7858]['unresolved'].append('六key：三國志為裴注譙秀事、晉書隱逸/春秋末譙秀省稱蕭敬，同桓溫滅蜀後范賁之亂與宕渠避難，非三國本人活動。楊謙/謹347被殺不是敬文死；譙秀80/90不是本人年齡。')
    for ident,phrase in [(7859,'闰月，戊子'),(7863,'右仆射郎肃皆自杀'),(7864,'司徒刘茂、特进郎闿相谓曰')]:
        ev=chunk_ev('tj_17',phrase,'資治通鑑永和八352本人死亡段','同卷紀年及本人敘事核定')
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(352,ev))
        results[ident]['evidence'].append(ev)
    results[7859]['historical_death'].update(month=10,lunar_leap_month=True,sexagenary_day='戊子',precision='lunar_month_sexagenary_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7863]['historical_death'].update(month=8,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    for ident,note in {7856:'兩key同329建康令與劉超鍾雅謀奉帝，事洩被殺具名為超雅，不能套管旆卒。',7857:'329羌姜聰刺吐延，吐延召纥扢泥輔葉延後抽劍死；死者非纥扢泥，無其生卒。',7859:'352十月丙戌起兵，閏十月戊子慕輿根助恪攻斬蘇林，無出生。',7860:'趙封奉傳國璽送石勒，僅晚出重編本具名，無其生卒；不套上邽其他公侯被殺。',7861:'三書同蔣英辛恕據長安歸石勒，劉胤戰敗被殺非辛恕。',7862:'邵恭朝會執麾不降下理官，治罪不等於處死；僅SL晚出重編記載。',7863:'郎肅冉魏右僕射與王簡張幹鄴陷自殺，八月；開門庚午不是本人確切自殺日。',7864:'四key同光祿大夫郎闓後特進、與劉茂自殺，非冉閔四月戰敗等同冉閔死亡；本人352，生年未知。',7865:'金光為蘇彥將，遭慕容恪斬具名，但此key僅SL晚出重編，保352為該本記載，不稱獨立原始確年。'}.items():results[ident]['unresolved'].append(note)
    results[7865]['late_reconstruction_statement']={'year':352,'event':'蘇彥將金光襲慕容恪被斬','source':'明屠喬孫項琳重編十六國春秋sl03，非崔鴻原著直傳'}
    round15e_reads={7867:[('sl_03','髙开被创而卒'),('tj_17','高开被创而卒')],7868:[('js_12','初，降人魏脱卒，其弟憬代领部曲'),('sl_06','浩潜遣安北将军魏憬'),('tj_17','襄斩憬，并其众')],7869:[('hy_00','丘奥退保宜都')],7870:[('sl_01','侍中任潘等参议')],7871:[('hy_00','遣任邵屯巴'),('sl_09','春正月寿引还遣任邵屯巴东')],7872:[('sl_08','有侯子瑜先死得其曽孙亮')],7873:[('js_11','匡术、刘仕等步骑二万'),('js_12','刘仕一军早已入淮'),('sl_01','遣将军刘仕救歴阳'),('tj_17','执陈留内史刘仕')],7874:[('sl_02','群从弟启启弟述')],7875:[('sl_00','妾叔父昶无子'),('tj_14','妾幼鞠于叔父昶'),('js_03','兖州刺史刘昶字公荣'),('js_17','济阴刘公荣有知人之鉴'),('sg_09','时同郡刘公荣名知人'),('ssxy_01','时同郡刘公荣名知人'),('ssxy_03','而刘昶在坐不与焉')]}
    for ident,qs in round15e_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第15批E·將領/外戚/同名異人源段逐鍵實讀','Actual original subject and dates checked; no guessed birth or death') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident not in [7873,7875],identity_status='all_listed_source_subjects_adjudicated' if ident not in [7873,7875] else 'read_all_keys_but_person_branches_unlinked',last_manual_review_batch='round15e')
    results[7866].update(review_status='secondary_source_checked',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb_identity_awaiting_independent_epigraph_read',last_manual_review_batch='round15e')
    results[7866]['evidence'].append(dict(citation='CBDB32419 original row read',excerpt=json.dumps(originals[7866]['cbdb_originals'],ensure_ascii=False)))
    results[7866]['unresolved'].append('原CBDB註Yan Zhenqing WJ xibiao p1，生卒空、index325/type1112不是出生或活動明年；未讀顏真卿系表本人段前不稱古史核實，遊戲估算保留。')
    for ident,year,phrase in [(7867,352,'高开被创而卒'),(7868,353,'襄斩憬，并其众')]:
        ev=chunk_ev('tj_17',phrase,'資治通鑑永和八352/永和九353本人死亡','獨立通鑑紀年，非晚出SL自證')
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(year,ev))
        results[ident]['evidence'].append(ev)
    results[7867]['historical_death'].update(month=4,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7867]['unresolved'].append('兩key同高瞻子、昌黎守討劉準封放、慕容恪參軍廉臺役被創卒，352四月，非冉閔五月被斬日；無出生。')
    results[7868]['unresolved'].append('三key同魏統/脫弟魏憬，兄卒代部曲後殷浩潛遣襲姚襄而被襄斬。晉書兄作魏脫、通鑑魏統字異，故事獨特同一。晉書編叙魏憬死在許昌敗前但無獨立年，通鑑353明年可採；SL353十月屬壓縮段，不套十月。安北將軍通鑑屬兄，非必魏憬本人。')
    for ident,note in {7869:'華陽330冬監軍丘奧退宜都，重編SL異作毌丘奧但此key只丘奧；退兵非死，無生卒。',7870:'任潘侍中議五德與授石弘兵書，單SL晚出重編不供確年，無生卒。',7871:'華陽330李壽伐巴東、331還任邵屯巴與SL同人，巴郡/巴東駐地異文保留，軍事行動無生卒。',7872:'侯亮在SL省稱曾孫亮，祖侯子瑜已死是夢，不是侯亮卒，祈連/祁連字異；單晚出重編不推確年。',7873:'四key皆實讀。330庾亮將、335歷陽援將、339入淮軍隊可連軍事主幹；354被郭敞執的陳留內史劉仕相容但無官遷/同役明橋，保支未全閉合，非因同卷另人臆造混人。354被執不等死亡。',7874:'SL本人家傳為劉群從弟劉啟、弟述，段末波部至趙尚書僕射、歸晉殷浩北伐姚襄陷；無獨立生卒年，晚出本不憑其叙次確353卒。',7875:'實讀七key：SL/TJ劉曜后劉氏叔父昶前趙外戚326；晉書王戎/世說任誕為阮籍酒友兖州刺史劉昶公榮；SG/世說賞譽同郡沛國武周知人者公榮；晉書范喬傳作濟陰公榮。前趙外戚與竹林通士必須隔離，公榮兩籍支待族系橋。范粲84/范喬事年不套公榮。'}.items():results[ident]['unresolved'].append(note)
    results[7873]['component_identities']=[dict(identity='330–339晉軍將劉仕',keys=[k for k in results[7873]['keys'] if not k.startswith('tj:')]),dict(identity='354陳留內史劉仕',keys=['tj:tj_17:320:劉仕'],attested_activity_years=[354])]
    results[7875].update(review_status='identity_conflict',historical_birth=None,historical_death=None,component_identities=[dict(identity='前趙劉曜后叔父劉昶',keys=[k for k in results[7875]['keys'] if k.startswith(('sl:','tj:'))]),dict(identity='兗州刺史竹林酒友劉昶公榮',keys=['js:js_03:119:劉昶','ssxy:ssxy_03:75:劉昶']),dict(identity='沛國武周友劉公榮',keys=['sg:sg_09:134:劉公榮','ssxy:ssxy_01:291:劉昶']),dict(identity='濟陰器重范喬的劉公榮',keys=['js:js_17:104:劉公榮'])])
    round16a_reads={7876:[('sl_04','克鄠斩刘珍夏侯显'),('tj_17','克鄠，斩刘珍、夏侯显')],7877:[('sl_00','刘氏名芳侍中刘皑'),('tj_14','立芳为皇后')],7878:[('js_13','武昌太守刘诩'),('tj_15','武昌太守刘诩'),('js_04','更以雍弟诩子')],7879:[('js_11','咸兄阐亡奔苏峻'),('tj_14','宗党卞阐亡奔苏峻')],7880:[('tj_14','三子绰、超、演皆废为庶人')],7881:[('tj_14','三子绰、超、演皆废为庶人'),('js_07','绰为奉车都尉、奉朝请')],7882:[('tj_14','三子绰、超、演皆废为庶人'),('js_07','幽其子淮陵王超'),('js_07','超兄弟皆没于刘聪')],7883:[('sl_04','克鄠斩刘珍夏侯显'),('tj_17','克鄠，斩刘珍、夏侯显')],7884:[('sl_04','十一月雄克池阳斩孔特'),('sl_05','孔特等起兵应遇'),('tj_17','于是孔持起池阳'),('tj_17','秦丞相雄克池阳，斩孔特')],7885:[('sl_08','遣鸿胪孟毅拜骏'),('tj_15','赵遣鸿胪孟毅拜骏')]}
    for ident,qs in round16a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第16批A·關中同役及宗室系譜逐人逐鍵實讀','Actual source subjects read; no birthplace or dates inferred from name matches') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident not in [7878,7882],identity_status='all_listed_source_subjects_adjudicated' if ident not in [7878,7882] else 'source_subjects_read_branches_not_unified',last_manual_review_batch='round16a')
    zhen353=chunk_ev('tj_17','十二月，清河王法、苻飞克鄠，斩刘珍、夏侯显','資治通鑑永和九353十二月','劉珍夏侯顯具名同被斬，獨立早源紀年，不套張遇七月卒')
    for ident in [7876,7883]:
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(353,zhen353,month=12))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(zhen353)
        results[ident]['unresolved'].append('兩key同鄠起兵、張遇七月案後求晉救，353十二月被苻法苻飛攻斬；無出生，不套張遇死亡。')
    kong353=chunk_ev('tj_17','秦丞相雄克池阳，斩孔特','資治通鑑永和九353冬','同卷前文起兵作孔持、死作孔特，非以異名另人；獨立確年，十一月單列SL暫僅年')
    results[7884].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(353,kong353))
    results[7884]['evidence'].append(kong353)
    results[7884]['unresolved'].append('三key同池陽起兵請晉救、苻雄攻斬，通鑑自身持/特異寫可閉合，不和孔持再另造一人；無出生。')
    results[7878].update(historical_birth=None,historical_death=None,component_identities=[dict(identity='330武昌太守劉詡',keys=['js:js_13:161:劉詡','tj:tj_15:350:劉詡']),dict(identity='劉和子雍弟詡、子焉襲劉頌爵',keys=['js:js_04:18:劉詡'])])
    results[7882].update(review_status='identity_conflict',historical_birth=None,historical_death=None,component_identities=[dict(identity='南頓王宗子司馬超、綽演兄弟',keys=['tj:tj_14:314:司馬超']),dict(identity='齊王冏子淮陵王超、冰英兄弟',keys=['js:js_07:435:司馬超'])])
    for ident,note in {7877:'兩key同劉皑女芳、劉曜先劉后所薦堂妹；SL立后光初十328與TJ326二月不同，源別保留，不據晚本年13推出生316，不把先后死326當芳死。',7878:'三key實讀，軍政太守與劉頌家族未有籍貫/任官橋；西晉末子襲爵與330任守可相容，但不能無證合。無本人生卒。',7879:'兩key同卞咸兄阐、咸與司馬宗俱誅後阐奔蘇峻受庇，326被殺的是弟咸/宗，不是本人。',7880:'司馬宗子演與綽超326同廢為庶人，宗被殺不是三子死，無生卒；不得把齊王冏子超也合入此家。',7881:'兩key同司馬宗子綽，326被廢、咸康復籍為奉車都尉，與開卷孫綽同名完全不同，但candidate此key明宗子，不因同卷孫綽而hold。無生卒。',7882:'兩key父兄弟明不同：宗子超/綽演vs冏子超/冰英，確混人，撤整條任何生卒。冏子311洛陽沒於劉聰亦未明即被殺，不武斷311卒。',7885:'兩key同石勒鴻臚孟毅330拜張駿九/五錫受留後遣還，羌敗/歸附皆非本人死，無生卒。'}.items():results[ident]['unresolved'].append(note)
    results[7877]['source_chronology_notes']=[dict(source='通鑑咸和元326二月',event='立劉芳為后'),dict(source='明重編SL光初十328',event='立劉芳為后、年13')]
    round16b_reads={7887:[('sl_02','孙辉字光休')],7888:[('tj_15','默将宋侯缚默父子出降'),('js_09','攻之转急，宋侯遂缚默求降')],7889:[('sl_02','奋今何在左右对曰为赵郡守')],7890:[('sl_02','宣咸陇西人仕虎至太仆卒于官')],7891:[('sl_06','祖遇仕石虎为特进')],7892:[('gs_00','康僧渊。本西域人。生于长安'),('ssxy_01','康僧渊初过江未有知者'),('ssxy_02','康僧渊在豫章去郭数十里立精舍'),('ssxy_03','康僧渊目深而鼻髙')],7893:[('gs_00','与康法畅支敏度等俱过江')],7894:[('sl_01','刘宝张曀仆呼延莫')],7895:[('sl_08','宣以部将张植为前锋'),('sl_09','张植仕骏为西域校尉'),('js_18','宣以部将张植为前锋')]}
    for ident,qs in round16b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第16批B·源篇本人/親屬段逐條實讀','Subject, relatives and source tier checked in actual text') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7887,identity_status='all_listed_source_subjects_adjudicated' if ident!=7887 else 'primary_sunhui_read_cbdb_xts_reference_unread',last_manual_review_batch='round16b')
    results[7887]['evidence'].append(dict(citation='CBDB31581 original row read',excerpt=json.dumps(originals[7887]['cbdb_originals'],ensure_ascii=False)))
    for ident,note in {7887:'SL射聲校尉孫輝光休/父中書令𫖮/子緯孫周兄烈世系已讀，無生卒。CBDB新唐書73B3b引文未取原世系，不以光休名命中逕作全key閉合。',7888:'兩key郭默將宋侯縛默父子降陶侃，隨後軍門被斬主語郭默，不是宋侯；四十同黨未具名不能當侯死330。',7889:'宣奮是太僕宣咸子，父卒五年後石虎夢見父才擢子廷尉太常，無夢年不推出生卒/活動確年，單晚出本。',7890:'宣咸太僕卒官但無卒年，五年後夢亦無紀年；晚出本記載不得用石虎在位任年倒填。',7891:'崔遇為崔逞祖、崔諒子、瑜父，特進在石虎朝無個人年，崔逞後被誅不套祖遇。',7892:'四key同西域血統長安生、康法暢支敏度同過江，殷浩清談/王導深目高鼻/豫章精舍本人事件閉合；卒寺無年。晉成帝時僅活動政權期不推出生卒。',7893:'康法暢同僧淵過江、麈尾答庾亮廉者不取，與下一傳中山康法朗是兩個人，不憑同姓法某強合；無生卒。',7894:'SL石勒早期十八騎張曀僕列名，後代朝事不是本人，單晚出記載無生卒。',7895:'三key同楊宣前鋒張植流沙西征、張駿西域校尉後都尉、救金城。岑彭被殺只是地名諫詞、死者是其坐騎不是本人，不取其為卒年。SL後段重華金城役無獨立明年，不輸出確活動值。'}.items():results[ident]['unresolved'].append(note)
    sunzhuo_reads=[('js_11','车骑参军孙绰亦致书谏'),('tj_18','散骑常待领著作郎孙绰'),('js_06','绰字兴公。博学善属文'),('js_07','年五十八，卒'),('js_12','孙绰为之诔云'),('js_13','尝与孙绰等泛海'),('js_17','尝与孙绰商略诸风流人'),('sg_05','盛从父弟绰字兴公廷尉正'),('sg_17','𬘭父绰为安民都尉'),('sz_01','孙绰道贤论'),('gs_00','孙绰为之赞曰'),('gs_01','故孙绰为之赞曰'),('ssxy_00','孙绰赋遂初筑室畎川'),('ssxy_01','太原孙绰转相祖尚'),('ssxy_02','孙绰与庾亮牋曰'),('ssxy_03','世说是孙绰习凿齿言')]
    results[7886]['evidence'].extend(chunk_ev(ch,phrase,'第16批B補·孫綽17key逐源原篇審讀','Distinguish Eastern Wu Sun Chen father from Eastern Jin author; late commentary does not imply author alive during narrated events') for ch,phrase in sunzhuo_reads)
    results[7886]['evidence'].append(dict(citation='CBDB69877 original row read',excerpt=json.dumps(originals[7886]['cbdb_originals'],ensure_ascii=False)))
    results[7886].update(review_status='identity_conflict',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='eastern_wu_sunchen_father_mixed_with_eastern_jin_writer_cbdb_unlinked',last_manual_review_batch='round16b',component_identities=[dict(identity='東晉孫楚孫、孫統弟孫綽興公',keys=[k for k in results[7886]['keys'] if k not in ['sg:sg_17:187:孫綽','cbdb:69877']],reported_death_age={'age':58,'precision':'nominal_age_only_no_death_year'},attested_activity_years=[343,362]),dict(identity='東吳孫綝父安民都尉綽',keys=['sg:sg_17:187:孫綽']),dict(identity='CBDB69877裸行孫綽',keys=['cbdb:69877'],source_reported_activity_range=[330,365])])
    results[7886]['unresolved'].append('17key原文逐一實讀：東吳孫綝父安民都尉綽與東晉孫楚孫興公確定異人；SG魏志孫資注孫氏譜則明興公廷尉正、孫盛從父弟，歸東晉作者而非三國當事人。GS/SZ作者為前代沙門贊不等本人與沙門同時；世說孫興公別名與遂初/天台賦同人。晉書卒58無卒年，不硬314–371。CBDB330–365活動範圍無注，未獨立連身份，僅保來源原值不作已證。整條暫不可按東晉生日或活動自動修。')
    round16c_reads={7896:[('sl_02','张楼阳平人也')],7897:[('yl_06','将军张珍领郡县民丁万人')],7898:[('sl_02','太守张冲降之'),('sl_08','金城太守张冲降之'),('sl_09','蛮骑校尉张冲'),('tj_16','太守张冲请降'),('js_15','金城太守张冲降于秋')],7899:[('sl_02','张谧冀州人也')],7900:[('sl_02','张越上党武乡人勒之姊夫')],7901:[('sl_02','张跃字世渊')],7902:[('js_12','会张遇反，谢尚又败绩'),('js_13','初，苻健将张遇降尚'),('sl_02','豫州牧张遇荆州刺史乐弘'),('sl_04','秋七月遇与中黄门刘晃'),('sl_05','张遇欲引关中诸将'),('sl_06','镇西将军张遇反于许昌'),('tj_17','秋，七月，遇与黄门刘晁谋夜袭健')],7903:[('ssxy_01','石虎司徐正名知人')],7904:[('gs_00','与康法畅支敏度等俱过江')],7905:[('jk_01','曹劢为陈留王以绍魏后')],7906:[('sl_08','西域长史李栢请击叛将赵真'),('js_15','四域长史李柏请击叛将赵贞')],7907:[('sl_06','父樊石虎乐安太守左中郎将')],7908:[('sl_01','居与邑人李阳相近'),('sl_02','李阳武乡人也勒幼时与阳邻居')],7910:[('sl_01','従事中郎李鳯'),('tj_15','从事中郎李凤')],8127:[('sl_01','使牙门将张弥徙洛阳'),('sl_02','虎建武二年弥率众一万徙洛阳'),('tj_15','使牙门将张弥行徙洛阳')]}
    for ident,qs in round16c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第16批C·原文同役/同名逐鍵核讀','Actual paragraph read, unique repeated event adjudicated; execution and defeat distinguished') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round16c')
    yu353=chunk_ev('tj_17','秋，七月，遇与黄门刘晁谋夜袭健','資治通鑑永和九353七月張遇案','此前351冉魏豫州歸晉→352許昌反晉求秦救→353入關與劉晃謀襲健伏誅，七key同事鏈閉合')
    results[7902].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(353,yu353,month=7))
    results[7902]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7902]['evidence'].append(yu353)
    results[7902]['unresolved'].append('七key同許昌張遇及入秦伏誅故事，晉書謝尚傳苻健將降尚概括與通鑑351先冉魏轉晉叙次略異保留，不另造同時同城張遇。劉晁/晃黃門異字非本人；謝尚戰敗與本人伏誅相隔一年。')
    results[7897].update(review_status='duplicate',duplicate_of=8127,source_identities_all_adjudicated=True,identity_status='zhang_zhen_mi_unique_luoyang_bells_transport_variant')
    results[7897]['evidence'].extend(results[8127]['evidence'][-3:])
    results[7897]['unresolved'].append('御覽引趙書張珍領萬人徙洛陽六鐘、翁仲銅駝飛廉、鐘沉盟津，與三key張彌同唯一事件，原人名異寫不另造將軍，→8127；8127不是東吳使者2504。無生卒。')
    results[8127]['unresolved'].append('三key實讀武德城張彌被勒赦後牙門將、336移洛陽鐘虡至邺；御覽張珍同萬人同物同盟津沉鐘事件可同，不與東吳使者張彌2504混。搬運沈鐘與士卒饑死非張彌本人死。')
    for ident,note in {7896:'臨水長張樓酷政歌謠無紀年，單晚出本無生卒。',7898:'五key金城張衝降麻秋，SL張植傳前段蛮騎校尉張衝後金城太守本人同軍旅脈絡；降趙不是死，車濟宋矩拒降死是他人，不套其卒346。',7899:'張謐冀州少受陸雲賞、石勒司馬，陸雲為太守段無明年，不推出生卒；單晚出本。',7900:'石勒姊夫上黨武鄉張越廣威將軍，博戲折脛被殺無明年，不能以勒在位末333當卒，單晚出本。',7901:'張躍世淵清河武城、石弘師衛軍長史，前段97卒為荀綽不是躍，無生卒。',7903:'徐正石虎司隸賞苻堅6歲，這是本人活動旁證、苻堅6不是正出生年齡；世說車頻秦書後注未明原年，無生卒。',7904:'支敏度與康僧淵康法暢成帝時過江、著譯經錄，與支遁道林不是同人，不套成帝327即出生。無生卒。',7905:'建康實錄魏武玄孫曹劢，原game bio稱孫錯為玄孫；326十月受陳留王明活動但生卒未知，不能據受爵倒填308。',7906:'李柏/栢同西域長史請擊趙真/貞失敗，張駿減死免處決，王恢被殺為歷史比喻不是本人已被殺，無生卒。',7907:'李先家傳父樊石虎樂安守左中郎將，李先後歸魏與父活動分開，單晚出本無父生卒。',7908:'兩key武鄉李陽石勒童年鄰人與老拳沤麻池明同，始興太守是晚年職，非晉竟陵太守李陽，不把同名合。無生卒。',7910:'兩key石勒從事中郎李鳳330尚書共參事，石虎次子宣/裴憲等親屬年不可套本人，無生卒。'}.items():results[ident]['unresolved'].append(note)
    results[7909].update(review_status='secondary_source_checked',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb_tackett_estimate_original_read_epitaph_unconsulted',last_manual_review_batch='round16c',source_reported_estimates=[dict(year=391,precision='approximately_estimated_by_Tackett',source='CBDB c_notes cc391 and c_dy_range2')])
    results[7909]['evidence'].append(dict(citation='CBDB194408 original row read',excerpt=json.dumps(originals[7909]['cbdb_originals'],ensure_ascii=False)))
    results[7909]['unresolved'].append('原CBDB391明Tackett cc391估算、range2，index328/type29非生或活動。所引文物1999.1頁93/洛陽新獲續504墓誌尚未讀，不稱古史確年。')
    round16d_reads={7911:[('js_15','桑虞，字子深'),('sl_05','桑虞字子深')],7912:[('js_06','代桓景为护军将军'),('js_13','父景，有当世才干'),('js_13','丹阳尹桓景佞事王导'),('tj_15','丹杨尹桓景，为人谄巧'),('js_01','丹阳尹桓景等饯于版桥'),('ssxy_03','褒与桓景共免之')],7913:[('js_17','太守杨宣画其象于阁上'),('sl_08','西胡校尉沙州刺史杨宣'),('sl_09','太守杨宣画其像于阁上'),('tj_15','骏遣将杨宣伐龟兹'),('tj_16','以西胡校尉杨宣为刺史'),('js_15','又使其将杨宣率众越流沙'),('js_18','张骏遣沙州刺史杨宣'),('wsh_04','以西胡校尉杨宣为刺史')],7914:[('sl_09','建平监军毌丘奥退屯宜阳'),('tj_15','监军毌丘奥退保宜都'),('js_07','后代毌丘奥监巴东诸军事'),('js_09','遣毌丘奥于巴东'),('sg_09','宗子奥巴东监军益州刺史')],7915:[('tj_15','祖逖有胡奴曰王安'),('js_19','初，逖有胡奴曰王安'),('sl_02','王安本胡人也')],7916:[('js_12','浩遣刘启、王彬之击襄于山桑'),('sl_06','王彬之攻益于山桑'),('tj_17','启、彬之皆败死')],7917:[('sl_01','陶侃遣兼长史王敷来聘')],7918:[('sl_02','王谟字思贤')],7919:[('js_13','豫州西曹王随'),('tj_15','豫州西曹王随')],7920:[('sl_02','申录字道时为广昌令')],7921:[('gs_01','武邑太守卢歆闻安清秀')],7922:[('sl_01','刺史临深遣司马管光')],7923:[('sl_02','赵兴太守籍抜收送诣虎')],7924:[('sl_08','付索绥以着凉春秋'),('sl_09','索绥字士艾')],7925:[('sl_00','司空刘均举参军台产'),('js_18','台产，字国俊'),('sl_01','台产字国隽')]}
    for ident,qs in round16d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第16批D·同篇官職與親屬本人逐鍵實讀','Source role scope and chronology checked, posthumous or relatives statements not personal lifespan') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7913,identity_status='all_listed_source_subjects_adjudicated' if ident!=7913 else 'western_regions_general_and_later_songxian_patron_unlinked',last_manual_review_batch='round16d')
    results[7869].update(review_status='duplicate',duplicate_of=7914,source_identities_all_adjudicated=True,identity_status='qiu_ao_shortened_guanqiu_ao_same_badong_monitor_retreat')
    results[7869]['evidence'].extend(results[7914]['evidence'][-5:])
    results[7869]['unresolved'].append('新核7914五源與華陽同330李壽費黑任邵伐巴東、建平監軍退宜都，毌丘奧省複姓丘奧，可整條→7914；SG注補父毌丘宗祖俭，非三國本人在世。')
    bin353=chunk_ev('tj_17','启、彬之皆败死','資治通鑑永和九353冬張重華卒後山桑役','殷浩遣劉啟王彬之攻姚益、姚襄援擊具名二人死，無出生')
    results[7916].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(353,bin353))
    results[7916]['evidence'].append(bin353)
    results[7913].update(historical_birth=None,historical_death=None,component_identities=[dict(identity='張駿西胡校尉沙州刺史楊宣、伐西域',keys=[k for k in results[7913]['keys'] if k not in ['js:js_17:198:楊宣','sl:sl_09:102:楊宣']]),dict(identity='張祚時太守楊宣、為宋纖畫像頌',keys=['js:js_17:198:楊宣','sl:sl_09:102:楊宣'])])
    for ident,note in {7911:'兩書同桑虞子深魏郡、少14喪父衝、石勒武城令劉征長史349辭晉任。SL追加五世同居苻朗拜其母與前段母已亡矛盾，晉書止卒官無年，不把苻朗青州時當虞仍在世；原書異敘保留不推日期。',7912:'五key同丹陽尹護軍桓景、伊父，褚裒司馬無忌拔刀由景救護事件獨特可橋；ssxy03開卷桓景真為桓亮不是此candidate本人，該key屬後段板橋相救，不誤以同卷另同名整條hold。陶回51卒非景，無生卒。',7913:'八key逐原文讀，六key西域楊宣閉合；張祚時太守宋纖畫像兩key缺與前沙州刺史的官遷/家族橋，兼容不等已證同人，日期留空全key未閉。宋纖82不是宣年齡。',7914:'五key同巴東監軍退宜都、陶侃遣守後周撫代任，SG毌丘俭注明宗子奧巴東監軍益州刺史，後注晉事不是他三國活動；7869華陽丘奧同一役可合，無生卒。',7915:'三key同祖逖胡奴後石勒將救祖道重，10歲與後歸江南主語道重，不推王安出生/後趙滅後仍在世。',7916:'三key同劉啟殷浩軍山桑被姚襄殺，353本人死，不套殷浩廢放/姚益退兵為其死亡。',7917:'王敷陶侃兼長史聘石勒送珍獸，單晚出本無生卒，不自動并王輔異字。',7918:'王謨曲陽令口齒甕鼻、張賓薦後部從事，與冉魏尚書令王謨350被殺無橋，保同名異支不硬合，無生卒。',7919:'兩key同豫州西曹王隨為桓宣辯郭默案、帶桓戎迎陶侃，無生卒；夜郎興古兵役無明本人死。',7920:'申錄廣昌令祥瑞後三公，未明為申鐘異字，單晚出本無生卒。',7921:'武邑太守盧歆請道安講經，同書後道安45再冀/石虎死前敘事，45不是歆歲，無明年不可倒推歆生卒。',7922:'管光州司馬討羌兵敗，不明自身死或秦州罹亂皆殺，不記330卒；單晚本活動未作已證輸入。',7923:'籍拔/籍抜趙興太守抓麻襦，麻襦自言光和至今數百年是神異說不套籍拔壽，無生卒；單晚本。',7924:'兩key索綏士艾著涼春秋封平樂亭侯，父戢與鄰段索襲75死、老子父72/懷胎80皆非其壽數。玄靚5年祈雨僅晚出本叙次，不供確生卒或活動修正。',7925:'三key台產國俊/隽上洛卜士、劉均薦諫博士祭酒至太子少師，晉書明本人全傳無卒年，旁文台崇/前段游子遠被殺不是產。'}.items():results[ident]['unresolved'].append(note)
    round17a_reads={7926:[('sl_05','濳遣东莱华挺授虞')],7927:[('js_11','洪兄子望为记室参军')],7928:[('js_11','以西阳太守蒋巽代之')],7929:[('sl_02','薛闾训字祖明')],7930:[('js_01','季龙悉诛挹'),('sl_02','悉诛挹瑴')],7931:[('sl_09','通重华妃裴氏'),('tj_17','杀重华妃裴氏及谢艾')],7933:[('sl_08','父昌石勒时为黄门侍郎')],7934:[('js_13','石勒荆州刺史郭敬'),('js_18','获苻健荆州刺史郭敬'),('sl_01','路逢郭敬泣拜'),('tj_15','赵荆州监军郭敬寇襄阳'),('js_07','石勒将郭敬率骑攻抚'),('js_09','走石勒将郭敬'),('sl_02','郭敬字季子邬人')],7935:[('sl_01','右司马郭殷'),('tj_15','右司马郭殷')],7936:[('sl_02','郭穆字景通北平人')],7937:[('yl_04','乡人河东陈嘏、平阳马恬')],7938:[('yl_04','乡人河东陈嘏、平阳马恬')],7939:[('sl_01','张骏遣长史马诜'),('sl_02','遣别驾马诜来朝'),('sl_08','使长史马诜称臣于勒'),('tj_15','使其长史马诜称臣'),('tj_16','马诜')],7940:[('sl_02','魏豹字叔虎范阳人')]}
    for ident,qs in round17a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第17批A·親屬/軍事角色及源層級原文審讀','Actual source paragraph checked; personal death not inferred from neighbouring subject') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7934,identity_status='all_listed_source_subjects_adjudicated' if ident!=7934 else 'later_qin_jingzhou_governor_unlinked_to_earlier_zhao_general',last_manual_review_batch='round17a')
    pei353=chunk_ev('tj_17','杀重华妃裴氏及谢艾','資治通鑑永和九353年末張祚立為涼公後','本人裴氏具名被害，非重華死當其卒；SL只通淫不獨證死亡')
    results[7931].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(353,pei353))
    results[7931]['evidence'].append(pei353)
    results[7934].update(historical_birth=None,historical_death=None,component_identities=[dict(identity='後趙郭敬季子、襄陽荊州軍職',keys=[k for k in results[7934]['keys'] if k!='js:js_18:269:郭敬']),dict(identity='354前秦荊州刺史郭敬於上洛被桓溫別軍俘',keys=['js:js_18:269:郭敬'])])
    for ident,note in {7926:'SL華挺東萊（晉書東莞異文）奉晉命授桑虞，遣使段無本人生卒；349僅本書叙事，不做自動活動輸入。',7927:'葛洪兄子望廣州刺史鄧岳記室參軍，洪遯羅浮之年非望出生；洪81卒非望壽數，無生卒。',7928:'蔣巽西陽守代陳頵梁州，緊接69卒主語原傳陳頵，不是蔣巽，不推生卒。',7929:'薛閭訓祖明樂安人，沒於石勒是淪陷後授幽州刺史不是本人死亡，單晚出本無生卒。',7930:'兩key裴憲子挹/瑴同遭魚誣陷，JS魚姓邢/SL郝異文保留。太子中庶子散騎常侍語法明屬瑴，candidate將其套挹未必對；悉誅挹瑴明死亡但本人段無確年，不能只據石虎將伐段遼逕定337。',7931:'兩key同張重華妃裴氏、祚通淫，通鑑353年末明殺，SL只通不是否認死。無出生，重華子10歲不等其生母已定。',7933:'趙昌為趙逸溫父、石勒黃門侍郎，兩子姚興/赫連勃勃/魏太武活動是後代，不推出父卒年；單晚出本。',7934:'七key實讀。後趙邬人郭敬恩石勒→荊州監軍襄陽戰六key閉合；JS桓溫傳354秦荊州郭敬被俘與前趙將兼容但缺轉秦生平橋。不能僅同官同名全套，故分支保留。被獲不等死，無生卒。',7935:'兩key後趙右司馬郭殷330尚書→334石弘廢黜使者→335司空，同朝官遷閉合，無生卒，並非郭敖異名。',7936:'郭穆景通北平人清河守→被勒所得→輕車將軍，沒於勒不等其卒，單晚出本。',7937:'御覽引晉中興書陳嘏河東、與鄧攸馬恬出逃勒，牛馬被劫與鄧攸棄兒不是其生卒，無明年。',7938:'御覽引晉中興書平陽馬恬與鄧攸陳嘏出逃，鄧攸弟早亡不是馬恬死，不套叛逃當卒。',7939:'五key同張駿長史/別駕使趙馬詵，330奉勒/339表詞冒犯虎同職鏈。石璞諫後未殺，不能以欲斬當339卒，無生卒。',7940:'魏豹叔虎范陽太山守廪丘見亡妾後翌日卒，雖明死亡仍無年；不是戰國魏豹或張樓後太保桃豹，同卷文字不混人。'}.items():results[ident]['unresolved'].append(note)
    shang_reads=[('js_11','江夏相谢尚'),('js_12','后诣谢尚而说之'),('js_13','谢尚，字仁祖'),('js_13','未至，卒于历阳，时年五十'),('sl_02','西中郎将谢尚'),('sl_04','晋镇西将军谢尚攻'),('sl_06','豫州刺史谢尚于寿春'),('tj_16','江夏相谢尚、长山令王蒙'),('tj_17','以安西将军谢尚'),('js_01','后刘惔、谢尚共论中朝人士'),('js_05','温峤尝谓鲲子尚'),('js_14','南中郎将谢尚领宣城内史'),('js_16','谢尚时镇牛渚'),('js_17','谢尚清易令达'),('sl_05','晋镇西谢尚攻张遇于许昌'),('hj_03','谢尚时镇牛渚'),('ssxy_00','谢尚字仁祖陈郡人鲲之子'),('ssxy_01','镇西谢尚时镇牛渚'),('ssxy_02','桓宣武表云谢尚神怀挺率'),('ssxy_03','小妹名女正适谢尚')]
    results[7932]['evidence'].extend(chunk_ev(ch,phrase,'第17批A補·謝尚19key原文逐源身份核讀','Eastern Jin Ren Zu biography, military chronology, familial references and author appendix distinguished') for ch,phrase in shang_reads)
    results[7932].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_19_keys_same_xie_kun_son_renzu_including_yuanhong_appendix',last_manual_review_batch='round17a',reported_death_age={'age':50,'precision':'nominal_age','source':'晉書謝尚傳'},source_reported_death_era={'text':'升平初進四州督，病篤徵衛將軍未至卒','source':'晉書謝尚傳','calendar':'Eastern_Jin_Shēngpíng_357_to_361','meaning':'era wording without exact death year in consulted paragraph, do not force357 or birth308'})
    results[7932]['unresolved'].append('19key全讀並閉合謝鯤子仁祖：江夏相/豫州安西/鎮西至衛將軍，王濛同訪殷浩、蔡謨食彭蜞、袁宏牛渚被賞、袁躭妹女正夫、康獻后舅相合。HJ03在後漢紀附袁宏傳節引晉書，不是東漢謝尚同名混入。晉書只升平初任四州督後卒50，所讀段未具確卒年；TJ356王胡之代尚未行卒主語胡之，不能套謝尚卒356。保50與源年號，暫不以流行357倒推308；待帝紀獨立精年。')
    round17b_reads={7941:[('sl_01','魏该弟遐等率其部众'),('tj_15','魏该弟遐帅其部众'),('sl_02','晋别将魏遐率其兄该部众')],7942:[('yl_05','燕人庞世为光禄勋')],7943:[('js_15','郎中丁琪又谏曰'),('sl_09','郎中丁琪复谏曰'),('tj_17','郎中丁琪复谏曰'),('wsh_04','郎中了琪谏祚僭窃')],7944:[('sl_00','太史令任义进曰')],7945:[('js_18','杀温将应庭、刘泓'),('sl_04','杀温将应诞刘泓')],7946:[('sl_00','乃立刘皑女芳为皇后')],7947:[('tj_17','燕黄门侍郎宋斌等谋奉冉智')],7948:[('sl_01','都水使者张渐等监营邺宫')],7949:[('sl_04','初生将强怀与桓温战没')],7950:[('sl_03','翰子钩为乐陵太守'),('tj_17','燕乐陵太守慕容钩')],7951:[('js_18','杀温将应庭、刘泓')],7952:[('js_11','司马应诞为龙骧将军、襄阳太守'),('sl_04','杀温将应诞刘泓'),('tj_16','以司马应诞为襄阳太守'),('js_10','玄弟诞，有器干，历六郡太守')],7954:[('sl_04','五月王擢拔陈仓杀健扶风内史毛难'),('tj_17','王擢拔陈仓，杀秦扶风内史毛难')]}
    for ident,qs in round17b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第17批B·原書藍田役及同人字形逐鍵審讀','Source subject and event chronology checked, unique shared killing establishes variant names') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round17b')
    ding354=chunk_ev('tj_17','祚大怒，斩之于阙下','資治通鑑永和十354正月丁琪諫稱王案','與魏書了琪同唯一郎中諫僭窃死於闕下，丁/了字異')
    results[7943].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(354,ding354,month=1))
    results[7943]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7943]['evidence'].append(ding354)
    # JS names the victims; TJ independently dates the same distinctive battle.
    lantian=chunk_ev('tj_17','温与秦兵战于蓝田','資治通鑑永和十354四月己亥','與晉書桓溫傳苻生單騎親陷陣殺應庭劉泓的獨特同一戰役，聯合紀年')
    deathbattle=chunk_ev('js_18','杀温将应庭、刘泓','晉書桓溫傳；通鑑永和十354四月藍田戰紀年','具名本人死亡由晉書支持，非SL唯一證據')
    for ident in [7945,7952]:
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(354,deathbattle))
        results[ident]['evidence'].extend([deathbattle,lantian])
    results[7951].update(review_status='duplicate',duplicate_of=7952,source_identities_all_adjudicated=True,identity_status='ying_ting_dan_unique_lantian_victim_with_liu_hong_name_variant')
    results[7951]['evidence'].extend(results[7952]['evidence'][-2:])
    for ident,year,phrase,month in [(7947,354,'燕黄门侍郎宋斌等谋奉冉智',9),(7950,354,'秃不胜忿，秋，七月，袭钩，杀之',7),(7954,354,'王擢拔陈仓，杀秦扶风内史毛难',5)]:
        ev=chunk_ev('tj_17',phrase,'資治通鑑永和十354本人被殺段','逐人原文明死，未套親屬/敵將年份')
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(year,ev,month=month))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    duan=chunk_ev('js_18','慕容垂妻段氏，字元妃','晉書列女傳段元妃完整段','父段儀、妹季妃夫慕容德、本人垂后與寶麟迫自殺，足以閉合CBDB后身份；本段無明CE卒年')
    results[7953].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='cbdb_explicit_duan_yuanfei_linked_to_actual_jinshu_biography',last_manual_review_batch='round17b',source_reported_dates={'death':dict(year=396,qualifier='CBDB建興11 range1；古傳敘垂死寶立後自殺，本段未獨立給CE年')})
    results[7953]['evidence'].extend([duan,dict(citation='CBDB135488 original row read',excerpt=json.dumps(originals[7953]['cbdb_originals'],ensure_ascii=False))])
    for ident,note in {7941:'三key同魏該弟遐率石城部眾降郭敬，魏該死不等遐死，無生卒。',7942:'御覽趙書龐世燕人光祿勳，卒後無弔但無明年，不能以後趙末350當其卒。',7943:'四key郎中丁琪/了琪同諫張祚稱王而被斬，354正月；五十餘載是張氏奉晉總期非本人年齡。',7944:'任義太史令解劉曜夢，“遠3年近700日”是預言應驗期不是其壽數，無生卒，單晚出本。',7945:'兩key劉泓同桓溫將藍田被苻生殺，JS本人句與TJ354藍田紀年可聯，只取卒年；不和同卷魏晋韋泓混。',7946:'劉皑侍中父劉芳皇后，劉昶/先劉后臨終非父皑死亡，無生卒，單晚本。',7947:'宋斌為宋燭子，354九月謀奉冉智被誅具名，並非其父338已死，不套冉智母年。',7948:'張漸都水使者與任汪建邺宮，暴水流木是開工緣由非本人死，無生卒，單晚本。',7949:'強懷與桓溫戰沒僅SL回述、子強延未封健死；不把妻樊氏遭苻生射死當強懷當年卒，不用晚本独證硬354。',7950:'兩key慕容翰子樂陵守鉤，354七月朱禿襲殺，翰先被賜死344非鉤死，生年未知。',7951:'JS應庭與SL應誕同桓溫藍田將、同被苻生斬且同伴劉泓，唯一同事可校庭誕異字；7952全四key閉合後→7952。',7952:'四key應誕：應詹次子兄玄、龍驤歷六郡、庾翼司馬襄陽守與桓溫將同江荊軍職鏈；JS應庭字異同劉泓藍田被殺獨特事件。父詹咸和6卒53不是誕年，不推出生。死亡354聯JS具名與TJ同役，非單SL。',7953:'CBDB明慕容垂后並注段元妃自殺；原晉書父儀妹季妃/寶麟逼殺全部讀，身份閉合。建興11=396是原庫明給但尚未讀獨立原古紀年段，保source_reported_dates，不將index329作生或活動，亦不依垂年自動補生日。',7954:'兩key扶風內史毛難354五月陳倉被王擢殺，不套秦苻健355卒。'}.items():results[ident]['unresolved'].append(note)
    wangmeng_reads=[('js_12','沛国刘惔、太原王蒙'),('tj_15','导辟太原王蒙为掾'),('tj_16','长山令王蒙常伺其出处'),('js_00','司徒左长史蒙之女也'),('js_06','长山令王蒙'),('js_13','弱冠，诣王蒙'),('js_17','王蒙，字仲祖，哀靖皇后父也'),('js_17','年三十九卒'),('ssxy_00','何充与王蒙刘惔好尚不同'),('ssxy_01','许询谢安王蒙'),('ssxy_02','王蒙子修并已见'),('ssxy_03','王蒙别传曰丞相王导辟名士')]
    results[7955]['evidence'].extend(chunk_ev(ch,phrase,'第17批C·王濛十二key原文/原庫審讀','Primary biography family and literary aliases checked; unrelated ages excluded') for ch,phrase in wangmeng_reads)
    results[7955]['evidence'].append(dict(citation='CBDB69868 original row read',excerpt=json.dumps(originals[7955]['cbdb_originals'],ensure_ascii=False)))
    results[7955].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='eleven_primary_wangmeng_keys_linked_bare_cbdb_unlinked',last_manual_review_batch='round17c',component_identities=[dict(identity='東晉太原王濛仲祖、長山令/司徒左長史、哀靖后父',keys=[k for k in results[7955]['keys'] if not k.startswith('cbdb:')],reported_death_age={'age':39,'precision':'nominal_age_no_death_year_in_biography'}),dict(identity='CBDB69868王濛裸行',keys=['cbdb:69868'],source_reported_dates={'birth':309,'death':347,'death_age':38})])
    results[7955]['unresolved'].append('11原key閉合：濛/蒙仲祖太原王訥子、王修王蘊父、哀靖后父；王長史/劉尹友與長山令同人。JS06江惇353卒49與JS12荀羨28皆他人，JS17王修24亦非濛。本人傳39卒無年；CBDB309–347/38無注與古傳虛歲39/算法有差，裸行身份未獨立閉合，不採其精年倒推生卒。')
    round17d_reads={7956:[('tj_15','其从子智、衍并劝成之'),('js_19','从子智及衍并倾险好乱')],7957:[('sl_03','子苌为太子靓为平原公'),('sl_04','太子苌拒桓温为流矢所中'),('tj_17','秦太子苌之拒桓温也'),('wsh_02','健遣其太子苌追温'),('sl_05','健遣雄同太子苌等帅众拒之')],7958:[('tj_15','侍中荀弈议')],7959:[('sl_04','京兆太守薛珍击青埿城'),('tj_17','顺阳太守薛珍'),('wsh_02','顺阳太守薛珍')],7960:[('tj_15','逖妻，柳之姊也'),('js_19','逖妻，柳之姊也')],7961:[('sl_02','行辽东相韩矫'),('tj_15','辽东相太原韩矫')],7962:[('sl_09','宁州秀才厐遗起义兵应之')],7963:[('sl_09','与任攀等为前锋')],7964:[('hy_00','又诈造妹婿任调书'),('sl_09','又诈造妹夫任调书'),('tj_16','寿乃诈为妹夫任调书')],7965:[('tj_15','遣刘岳、刘震等从男女盛服')],7966:[('js_15','庭坚为建康王'),('sl_09','封子庭坚为金泽侯'),('tj_17','子庭坚为建康侯')],7967:[('js_15','子泰和为太子'),('sl_09','子泰和为太子'),('js_15','诛祚二子')],7969:[('sl_04','匈奴右贤王曹毂左贤王刘卫辰')],7970:[('sl_00','使金创医李永与同载疗之'),('tj_15','使医李永与同载')],7971:[('hy_00','进李闳为征东、荆州'),('js_11','执伪荆州刺史李闳'),('js_12','兰陵太守李闳共守庱亭'),('sl_02','获蜀将李闳黄桓'),('sl_09','以李闳为征东将军荆州刺史'),('tj_15','督护李闳、轻车长史滕含'),('tj_16','闳，恭之子也'),('js_09','鉴遣参军李闳追斩之'),('js_19','督护李闳及轻车长史滕含')],7972:[('wsh_04','宋奴使姑子梁三王因侍直手刃杀初')],7973:[('js_12','雷弱儿、梁安等诈云杀苻健'),('sl_03','梁安为右长史'),('sl_04','左仆射梁安右仆射叚纯'),('sl_06','潜诱苻健大臣梁安雷弱儿'),('tj_17','洛阳梁安为长史'),('sl_05','左仆射梁安之女也')],7974:[('tj_17','宋奴使其姑子梁式王刺杀杨初'),('yi_01','宋奴使姑子梁式王因侍直手刃杀初')],7975:[('sl_03','安定梁楞为前将军'),('sl_04','司隶校尉梁楞为尚书令'),('tj_17','生杀梁后及毛贵、梁楞、梁安')],8709:[('sl_03','潜结匈奴左贤王曹毂'),('sl_05','匈奴曹毂刘卫辰帅众冦杏城'),('tj_18','曹毂寻卒') ]}
    for ident,qs in round17d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第17批D·家族案/互校名異與原段逐鍵審讀','Each actual source subject and original chronology read; inferred lifespan not substituted for unknown') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7971,identity_status='all_listed_source_subjects_adjudicated' if ident!=7971 else 'jin_jiangnan_general_and_chenghan_ligong_son_separate_branches',last_manual_review_batch='round17d')
    chang354=chunk_ev('tj_17','秦太子苌之拒桓温也，为流矢所中，冬，十月，卒','資治通鑑永和十354十月','五key同苻健長子太子苌、被桓溫軍矢傷後卒，非姚苌/茍苌')
    results[7957].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(354,chang354,month=10))
    results[7957]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7957]['evidence'].append(chang354)
    xue354=chunk_ev('tj_17','自矜其勇而咎温之持重；温杀之','資治通鑑永和十354桓溫六月退師後薛珍案','順陽守與SL冊府引京兆守同青泥/長安/桓溫軍獨特前鋒事件，職地異文保留')
    results[7959].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(354,xue354))
    results[7959]['evidence'].append(xue354)
    sons355=chunk_ev('tj_17','以庶人礼葬之，并杀其二子','資治通鑑永和十一355閏九月張祚敗','太和/庭堅兩名在前文受封，二子被害；SL又記庭堅後受封需保異叙不推晚年在世')
    results[7967].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,sons355))
    results[7967]['evidence'].append(sons355)
    results[7966].update(historical_birth=None,historical_death=None,source_chronology_notes=[dict(source='晉書張玄靚段與通鑑355閏九月',event='祚二子皆殺、庭堅前文為其子'),dict(source='明重編SL09張祚傳末',event='天錫即位改葬祚、封子庭堅金澤侯；不明追封或字訛，不當本人仍活明證')],source_reported_dates={'death':dict(year=355,source='晉書/通鑑二子皆殺')})
    results[7966]['evidence'].append(sons355)
    results[7969].update(review_status='duplicate',duplicate_of=8709,source_identities_all_adjudicated=True,identity_status='cao_gu_same_liu_weichen_revolt_and_east_west_cao_family')
    results[8709].update(review_status='primary_text_corroborated',source_identities_all_adjudicated=True,identity_status='three_original_cao_gu_keys_adjudicated_same_revolt_diplomacy')
    results[7969]['evidence'].extend(results[8709]['evidence'][-3:])
    results[7969]['historical_death']=None
    results[7971].update(review_status='identity_conflict',historical_birth=None,historical_death=None,component_identities=[dict(identity='東晉蘇峻役蘭陵守/郗鑒參軍督護李閎',keys=['js:js_12:177:李閎','tj:tj_15:116:李閎','js:js_09:309:李閎','js:js_19:196:李閎']),dict(identity='成漢李恭子、338荊州刺史339被晉俘李閎',keys=[k for k in results[7971]['keys'] if k not in ['js:js_12:177:李閎','tj:tj_15:116:李閎','js:js_09:309:李閎','js:js_19:196:李閎']])])
    liang355=chunk_ev('tj_17','九月，生杀梁后及毛贵、梁楞、梁安','資治通鑑永和十一355九月','梁安/梁楞兩本人具名被殺，後世復官不延長壽命')
    for ident in [7973,7975]:
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,liang355,month=9))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(liang355)
    shi355=chunk_ev('tj_17','初子国诛式王及宋奴','資治通鑑永和十一355正月仇池楊初案','三王/式王同宋奴姑子刺初後為國殺，異名同獨特親屬事件')
    results[7974].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,shi355,month=1))
    results[7974]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7974]['evidence'].append(shi355)
    results[7972].update(review_status='duplicate',duplicate_of=7974,source_identities_all_adjudicated=True,identity_status='liang_san_shi_wang_same_assassination_kin_variant')
    results[7972]['evidence'].extend(results[7974]['evidence'][-3:])
    for ident,note in {7956:'祖約從子衍與智贊亂327，後祖氏被誅未具名衍在場，不套330族誅；兩key同人無生卒。',7957:'五key苻健長子太子苌，351立至354十月傷卒，苻雄病卒/姚苌/茍苌異人不混；無出生。',7958:'荀弈組子侍中議元會拜王導之禮，不是荀藩子荀闓異名，無生卒。',7959:'三key順陽守薛珍、SL引冊府京兆守同青泥前鋒，魏書亦同持重爭論溫殺，獨立354確年；不用SL職名異記另造人。',7960:'兩key祖逖妻許柳姊勸柳勿反，非祖約女許柳妻；未明本人死，不套祖逖321或祖約330。',7961:'兩key韓矯遼東相與封抽請封慕容廆、後333奔皝司馬，無生卒；慕容仁/遼東降非其死亡。',7962:'SL厐遺即龐異體、寧州秀才應斯叟，楊術台登死是援者不是龐遺。單晚本無生卒。',7963:'SL任攀寧州前鋒，華陽作邵攀待原名校、此單key不先硬合。諸軍敗退非本人死，無生卒。',7964:'三key任回子任調、李壽妹婿：攻寧州→偽造其書→勸壽稱帝→鎮北梁州東羌，有親屬及職鏈，無生卒；數年天子卜語是李壽不是其壽數。',7965:'劉震與劉嶽同被勒令見劉曜，328仍存的明文，曜說以為灰土是過去誤判，非實已死；無生卒。',7966:'三key同張祚子庭堅，JS/TJ二子皆誅355與SL末天錫即位後封庭堅並列保存；後受爵可能追封/編本訛文，不能當本人363仍活。暫撤頂層確卒保原早源355，不據此推出生或活動延後。',7967:'張泰和/太和祚太子，兩key同前涼嫡子非同卷成漢李洪冒李勢子。JS誅祚二子/TJ獨立355年可核卒，無出生；宋纖82非太和。',7969:'單SL與8709三key曹轂同劉衛辰並叛、曹活弟、二子東西曹、使郭辯潛燕，左右賢王異文不拆；全四源閉合→8709。SL此段365後尋死非獨立明365卒，不加第三卒年，8709原367/368源別保留。',7970:'兩key金創醫李永同載療劉曜328，曜被殺非醫永死，無生卒。',7971:'九key實讀後分支：328–329東晉蘭陵守/郗鑒督護平蘇峻，338–339成漢李恭子荊州刺史被晉俘，原來源無轉仕父系橋；切勿拿任官339套此前晉軍，兩支無本人生卒。',7972:'魏書梁三王宋奴姑子與TJ/南齊書梁式王同刺楊初、楊國誅，三/式字異唯一事件可合→7974。',7973:'六key梁安前秦長史→左僕射/梁后父、與雷弱兒假通殷浩，洛陽/略陽籍異記保留；355九月與梁楞毛貴梁后同案被殺，後贈官不活。',7974:'兩key梁式王宋奴姑子刺初後國誅，355正月確年，同魏書三王；無出生。',7975:'三key安定梁楞輔苻洪→仆射司隸→尚書令輔健遺詔、355九月生殺；後苻堅復官是追贈，無出生。'}.items():results[ident]['unresolved'].append(note)
    round18a_reads={7976:[('sl_02','仇池公杨初袭破西城'),('sl_04','飞攻氐王杨初于仇池'),('sl_08','雄之有憾于南氐杨初'),('sl_09','张先杨初皆阻兵不服'),('tj_15','仇池氐王杨毅族兄初'),('tj_16','武都氐王杨初遣使'),('tj_17','杨初袭赵西城'),('wsh_04','毅族兄初袭杀毅'),('yi_01','毅族兄初袭杀毅'),('js_15','雄又有憾于南氐杨初'),('sl_05','苻飞为杨初所败')],7977:[('tj_17','故仇池公杨毅弟宋奴'),('wsh_04','毅小弟宋奴使姑子'),('yi_01','毅小弟宋奴使姑子')],7978:[('sl_04','怀妻樊氏于道上书')],7979:[('sl_03','叚纯为左司马'),('sl_04','右仆射叚纯吏部尚书辛牢'),('tj_17','得右仆射段纯，杀之')],7980:[('sl_03','龛弟罴骁勇有智谋'),('tj_17','龛弟罴，骁勇有智谋')],7981:[('sl_03','氐酋毛贵为单于辅相'),('sl_04','毛贵为太尉张遇为司空'),('tj_17','生杀梁后及毛贵、梁楞、梁安')],7982:[('js_15','张掖人王鸾颇知神道'),('sl_09','张掖人王鸾颇知神道'),('tj_17','张掖人王鸾知术数'),('wsh_04','有王鸾者，云')],7983:[('sl_09','西平田旋复要酒泉太守马基'),('js_15','西平田旋要酒泉太守马基')],7984:[('js_12','贼管商乘船旁出'),('tj_15','遣其将管商、张健、弘徽'),('wsh_02','使其党张瑾、管商率众'),('js_19','张健、管商、弘徽等入晋陵')],7985:[('js_11','宣城人纪世和率义故葬之')],7986:[('js_15','遣张掖太守索孚代瓘'),('sl_09','遣张掖太守索孚代瓘'),('tj_17','遣张掖太守索孚代瓘'),('wsh_04','参军索孚谏曰')]}
    for ident,qs in round18a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第18批A·仇池族系/涼州同案原文逐鍵審讀','Actual role and event read; posthumous grants and victims distinguished') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round18a')
    yang355=chunk_ev('tj_17','梁式王刺杀杨初；初子国诛式王及宋奴','資治通鑑永和十一355正月仇池案','楊初與宋奴兩本人死亡明文，別於楊毅先死及後楊國被弒')
    for ident in [7976,7977]:
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,yang355,month=1))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(yang355)
    chun355=chunk_ev('tj_17','得右仆射段纯，杀之','資治通鑑永和十一355六月苻生未逾年改元案','苻健卒後生即位改元，段純議非禮被殺，非段陵/段龕自動異名')
    results[7979].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,chun355,month=6))
    results[7979]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7979]['evidence'].append(chun355)
    pi355=chunk_ev('tj_17','罴固请不已，龛怒，杀之','資治通鑑永和十一355末段羆諫拒慕容恪','弟羆被兄龕殺在下一年度356戰前，與鈎/欽另人')
    results[7980].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,pi355))
    results[7980]['evidence'].append(pi355)
    mao355=chunk_ev('tj_17','九月，生杀梁后及毛贵、梁楞、梁安','資治通鑑永和十一355九月','毛貴為梁后舅、太傅輔政被殺本人，後復贈不是活到357')
    results[7981].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,mao355,month=9))
    results[7981]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7981]['evidence'].append(mao355)
    for ident,phrase in [(7982,'以鸾为妖言，斩以徇'),(7986,'瓘闻之，斩孚')]:
        ev=chunk_ev('tj_17',phrase,'資治通鑑永和十一355張祚遣兵討張瓘案','本人具名被殺、源叙次八月前，保守只取年')
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,ev))
        results[ident]['evidence'].append(ev)
    for ident,note in {7976:'11key同仇池楊毅族兄初、子國、南氐首領及349西城/353敗苻飛。JS張淳南氐楊初為333而335/337繼位本是先首領/後自稱君相容，不憑稱王把同仇池楊初拆人。魏書/南齊書/TJ明355正月弒死。SL後張先楊初擒授為回述政績，不證本人另年仍活。無出生。',7977:'三key楊毅弟宋奴使姑子刺楊初、國誅，355正月死；二子佛奴佛狗逃秦、孫楊定後事不是他仍活。',7978:'樊氏強懷妻為子強延請封被苻生射殺，單晚出SL敘355後未獨立古紀年，不以此確年入補丁。丈夫戰死與妻被射不同事。',7979:'三key段/叚純左司馬至右僕射，355六月改元議非禮被殺，後追復官不延生卒；與苻洪左司馬段陵不自動合。',7980:'段龕弟羆請河上拒恪而被龕殺，355年末，SL接十一月/通鑑十二月後叙，僅年；與弟欽356被恪俘不同人。',7981:'三key毛貴單于輔相/司空太尉至太傅、梁后舅，同苻洪健輔臣官鏈，355九月死；非元末同名毛貴，無出生。',7982:'四key王鸞張掖術士諫祚、軍出必敗三不道及族誅355，二十日預言不是其壽年；無生日。',7983:'兩key田旋西平與酒泉馬基應衛綝同被張姚王國斬。JS本人段接瓘立後不具確年，SL355年底叙次未獨立，歷史卒年暫空；待通鑑馬基案同事補年。',7984:'四key蘇峻將管商與張健弘徽東軍，敗庱亭後降庾亮非被斬，諸部死者未具名不套本人；無生卒。',7985:'紀世和宣城人葬桓彝，彝32853死非世和年，後咸安改贈亦非本人活至371；無生卒。',7986:'四key索孚參軍諫張駿石田出伊吾後張掖守代瓘枹罕被殺，同前涼職鏈無矛盾；355被瓘斬早源TJ/JS支持，不用SL獨占月份。無出生。'}.items():results[ident]['unresolved'].append(note)
    round18b_reads={7987:[('sl_03','苻菁鱼遵所过城邑'),('sl_04','平昌王菁勒兵入东宫'),('tj_17','健执菁，数而杀之'),('wsh_02','兄子扬武将军菁率众七千'),('sl_05','苻菁健兄子也')],7988:[('hy_00','朱提太守董炳固城'),('sl_09','朱提太守犍为董炳婴城自守'),('tj_15','朱提太守董炳城守')],7989:[('sl_08','司兵赵长迎秋于西郊'),('sl_09','右长史赵长尉缉等结异姓兄弟'),('tj_17','领军将军赵长等惧罪'),('wsh_04','领军赵长等开宫门应之'),('js_15','遣司兵赵长迎秋西郊')],7990:[('sl_01','约左右阎秃貌类约'),('tj_15','约左右阎秃，貌类约'),('js_19','约左右阎秃貌类约')],7991:[('js_09','夏至，杀斌'),('js_13','陶侃使其子平西参军斌'),('sl_01','陶侃遣其子平西参军斌'),('tj_15','太尉侃遣其子平西参军斌')],7992:[('js_12','健遣马流、陶阳等往攻之')],7993:[('js_12','长安人雷弱儿、梁安等诈云'),('sl_03','以南安雷弱儿为辅国将军'),('sl_04','健以雷弱儿为大司马'),('sl_06','潜诱苻健大臣梁安雷弱儿'),('tj_17','生杀弱儿及其九子、二十七孙'),('sl_05','雷弱儿南安羌酋也')],7994:[('hy_00','宁州刺史尹奉遣建宁太守霍彪'),('sl_09','宁州刺史尹奉遣建宁太守霍彪'),('tj_15','宁州刺史尹奉遣建宁太守霍彪'),('tj_16','汉建宁太守孟彦执其刺史霍彪'),('sg_12','弋孙彪晋越嶲太守')],7995:[('sl_01','彭城王堪遣将军韩雍救之')],7996:[('sl_09','酒泉太守马基背瓘起兵'),('tj_17','酒泉太守马基起兵'),('js_15','酒泉太守马基背瓘应')],7997:[('js_12','张健、马流攻无锡'),('js_12','健遣马流、陶阳等往攻之')],7998:[('sl_02','护军平原乙逸'),('sl_03','以玄莵太守乙逸为尚书'),('tj_15','护军平原乙逸'),('tj_17','以玄菟太守乙逸为尚书'),('tj_18','征幽州刺史乙逸为左光禄大夫')],7999:[('tj_15','述延卒，子傉大寒立'),('sl_10','述延死子傉大寒立')],8000:[('sl_05','陇西鲜卑乞伏司繁于度坚山'),('sl_10','大寒死子司繁立'),('tj_15','大寒卒，子司繁立'),('wsh_04','父司繁，拥部落降于苻坚')]}
    for ident,qs in round18b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第18批B·軍將與族史附叙原段逐鍵實讀','Actual source event read; retrospective succession does not date lifespan to chapter heading') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=ident!=7994,identity_status='all_listed_source_subjects_adjudicated' if ident!=7994 else 'four_ningzhou_keys_linked_sanguozhi_huoyi_grandson_unbridged',last_manual_review_batch='round18b')
    jing355=chunk_ev('tj_17','健执菁，数而杀之','資治通鑑永和十一355六月庚辰苻菁謀太子案','庚辰勒兵入東宮、父叔健登門而兵散、本人被殺明文')
    results[7987].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,jing355,month=6))
    results[7987]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7987]['evidence'].append(jing355)
    zhao355=chunk_ev('tj_17','易揣等引兵入殿，收长等，杀之','資治通鑑永和十一355閏九月','長是趙長，與張祚同亂但非一人；魏書參與刺祚異叙保留')
    results[7989].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,zhao355))
    results[7989]['evidence'].append(zhao355)
    lei355=chunk_ev('tj_17','生杀弱儿及其九子、二十七孙','資治通鑑永和十一355十二月末','趙韶董榮譖雷弱兒，本人族誅明文；九子27孫非年齡')
    results[7993].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,lei355,month=12))
    results[7993]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7993]['evidence'].append(lei355)
    ma355=chunk_ev('tj_17','酒泉太守马基起兵以应𬘭，瓘遣司马张姚、王国击斩之','資治通鑑永和十一355閏月張瓘入姑臧後、冬十月前','JS具名同張姚王國斬馬基田旋二人，聯合獨立紀年355')
    tian355=chunk_ev('js_15','斩基、旋二人之首，传姑臧','晉書張玄靚傳；通鑑永和十一355馬基同役','田旋有早源本人斬首句與同伴馬基的獨立355同役可聯，補回此前未能定年')
    for ident,ev in [(7996,ma355),(7983,tian355)]:
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(355,ev))
        results[ident]['evidence'].extend([ma355,tian355])
    results[7991].update(historical_birth=None,historical_death=None,source_chronology_notes=[dict(source='晉書陶侃傳',event='侃喪歸長沙、夏殺斌；父表/卒叙咸和七332'),dict(source='通鑑咸和九334',event='陶侃六月卒，僅供父年與兒喪事參考，非兒本人明卒年')])
    results[7994].update(historical_birth=None,historical_death=None,component_identities=[dict(identity='332建寧守/334寧州刺史/339被孟彥執霍彪',keys=[k for k in results[7994]['keys'] if not k.startswith('sg:')]),dict(identity='三國志裴注霍弋孫、晉越嶲守彪',keys=['sg:sg_12:180:霍彪'])])
    results[7998]['attested_activity_years']=[333,352,357]
    results[7998]['last_attested_activity']={'year':357,'source':'資治通鑑升平元357春正月','basis':'乙逸自幽州被徵左光祿大夫、夫婦鹿車與子璋相遇；不把璋其後居顯當父仍活'}
    for ident,note in {7987:'五key苻健兄子菁、七千渡河焚橋、平昌王拒溫、355六月勒兵反健被殺，同族官鏈閉合，無出生。庚辰為起兵日，不另硬說斬日亦庚辰。',7988:'三key晉朱提守董炳332守城、333春正月降成漢，兵降不是死，無生卒。',7989:'五key前涼司兵趙長→重華寵右長史祚結兄弟→領軍，355易揣殺。魏書說長刺祚後廚士殺祚，與JS/TJ長先被殺事件次序不同保源別，不當成兩個趙長。無出生。',7990:'三key祖約左右閻禿貌類約被陳光誤擒，擒不等殺、不套祖約330死，無生卒。',7991:'四key陶侃子平西參軍斌/陶夏弟，樊城軍與歸喪被兄殺明同人；古段未明本人卒年，父陶侃JS叙332與TJ334異記不拿其中一個年逕套斌，出生亦未知。',7992:'單JS蘇峻將陶陽與馬流攻庱亭敗，兵斬2000未具名陽死，無生卒。',7993:'六key雷弱兒南安羌酋/長安大臣、與梁安假詐殷浩、後丞相；長安為任事居地不妨原南安族。355十二月本人九子27孫同誅，無出生；後追贈不是活357。',7994:'五key原段均讀：四書南中霍彪建寧守→寧州刺史→339被孟彥送晉；SG注只霍弋孫彪晉越嶲守無任年，年代職地可相容但缺與此軍事人物的族譜明橋，暫不全key套，非斷定兩人。被執非死。',7995:'韓雍石堪救馬頭不及後掠海虞，失敗非自身死亡，單晚出本無生卒。',7996:'三key同馬基酒泉守應衛綝、張姚王國斬，通鑑獨立355紀年與JS同田旋斬首可聯；補7983田旋355。無出生。',7997:'馬流單JS蘇峻將無錫/庱亭役，軍敗斬獲不是明本人死；無生卒。',7998:'五key乙逸平原東夷護軍333奔皝→玄菟守352龍城留臺尚書→幽州守357左光祿、子璋與鹿車同人。357仍活明文可約束遊戲估卒，不推其真卒年或年齡。',7999:'兩key述延子傉大寒、子司繁相承；TJ329附述族史而SL遷徙主角署大寒/TJ署述延，來源叙系有差。大寒卒無明年，不將所附329當本人卒年，亦不推出生。',8000:'四key大寒子司繁、國仁父、降苻堅南單于鎮勇士川，名字同源族系閉合。SL371建元7為精年但晚本獨占；WS只敘降秦無明年，故不輸入371為確活動。TJ329附述世系非司繁即位/出生明年；死後國仁前鋒383不等司繁活至383，所讀段無獨立明卒年，不硬376。'}.items():results[ident]['unresolved'].append(note)
    round18c_reads={8001:[('sl_02','辽东佟寿'),('sl_03','司马佟寿'),('tj_15','司马辽东佟寿')],8002:[('hy_00','破走司马勋'),('js_01','后遂之曾孙勋字伟长'),('js_11','梁州刺史司马勋以梁益叛'),('js_13','梁州刺史司马勋反'),('js_18','命梁州刺史司马勋出子午道'),('sl_02','晋梁州刺史司马勲'),('sl_03','杜洪张琚遣使招晋梁州刺史司马勲'),('sl_04','司马勲既还汉中'),('sl_05','司马勲出子午道'),('sl_09','建威将军司马勲'),('tj_15','诏建威将军司马勋'),('tj_16','参军司马勋为梁州刺史'),('tj_17','梁州刺史司马勋帅众赴之'),('tj_18','温皆斩之'),('js_07','梁州刺史司马勋等会抚')],8003:[('js_09','东海哀王冲，字道让'),('js_11','东海王冲为长水校尉'),('tj_12','以其子冲继越后'),('tj_16','辛酉，东海哀王冲薨'),('js_07','以第三子冲奉越后'),('js_14','东海王冲为长水校尉')],8004:[('tj_15','朝廷遣督护乔球'),('sl_02','因遣督护乔球')],8005:[('sl_02','羗酋姜聪'),('tj_15','羌酋姜聪刺之'),('js_18','为羌酋姜聪所刺'),('sl_03','姜聪诸将已屠脍之'),('wsh_04','姜聪'),('yi_00','姜聪')],8008:[('sl_03','皝帐下督张英'),('tj_15','皝帐下督张英')],8009:[('sl_03','强平'),('sl_04','生舅左光禄大夫强平'),('sl_05','强平略阳氐人'),('tj_17','左光禄大夫强平谏'),('wsh_02','生舅强平切谏')],8011:[('sl_03','燕守将慕容兰'),('tj_17','燕将慕容兰屯汴城'),('js_12','慕容兰以数万众屯汴城')],8012:[('sl_02','慕容评率众攻邺'),('sl_03','世子俊帅前军师将军评'),('sl_04','慕容评为给事中'),('sl_05','慕容评示之以奢'),('tj_15','其弟军师将军评'),('tj_16','燕前军师慕容评'),('tj_17','慕容评为辅弼将军'),('tj_18','以燕故臣慕容评为给事中'),('js_17','谄事慕容评'),('sl_06','垂为太傅评所忌')],8013:[('sl_01','屈弟孤寛和仁厚'),('tj_16','不如屈弟孤仁厚'),('wsh_00','高凉王孤，平文皇帝之第四子')],8014:[('sl_03','故赵乐陵朱秃'),('tj_17','燕主俊具朱秃五刑')],8015:[('sl_09','以兴古太守朱提李播')],8016:[('sl_03','洪弟郁劝洪迎健'),('sl_04','洛州刺史杜郁'),('tj_17','洛州刺史杜郁')],8017:[('tj_17','仇池公杨国从父俊杀国自立'),('wsh_04','毅族兄初袭杀毅'),('yi_01','毅族兄初袭杀毅')],8018:[('sl_03','王堕为右将军'),('sl_04','乃杀司空王堕'),('tj_17','乃杀王堕'),('sl_05','王堕字安生京兆霸城人')],8019:[('sl_08','骏受诏遣部曲督王丰')],8020:[('sl_03','阳郡公王腾'),('tj_17','王腾寇鄄城')],8021:[('sk_01','拜<真义>为内臣佐平')],8022:[('sl_04','召太医令程延'),('tj_17','召太医令程延'),('wsh_02','太医程延')],8023:[('sl_03','安定程肱'),('sl_04','金紫光禄大夫程肱'),('tj_17','金紫光禄大夫程肱谏')],8024:[('tj_15','侨人盖肫掠人女为妻')],8025:[('sl_03','斩右长史袁范等'),('tj_17','斩右长史袁范等')]}
    for ident,qs in round18c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第18批C·原傳及通鑑紀年逐鍵實讀','Actual subject role and surrounding narrative read; unrelated ages and retrospective notices excluded') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round18c',historical_birth=None,historical_death=None)
    for ident,ch,phrase,year,month in [(8002,'tj_18','温皆斩之',366,5),(8003,'tj_16','辛酉，东海哀王冲薨',341,8),(8009,'tj_17','生怒，凿其顶而杀之',356,4),(8011,'tj_17','燕将慕容兰屯汴城，羡击斩之',356,11),(8014,'tj_17','燕主俊具朱秃五刑',356,11),(8016,'tj_17','洛州刺史杜郁，随之甥也',356,1),(8017,'tj_17','仇池公杨国从父俊杀国自立',356,None),(8018,'tj_17','乃杀王堕',356,1),(8020,'tj_17','城坏，获腾，斩之',356,None),(8022,'tj_17','安知吾食枣！”遂斩之',356,10),(8023,'tj_17','金紫光禄大夫程肱谏，以为妨农，生杀之',356,3),(8025,'tj_17','丙申，恪大破龛于淄水',356,1)]:
        ev=chunk_ev(ch,phrase,f'資治通鑑獨立紀年：{year}年'+(f'農曆{month}月' if month else '年內'),'本人死亡句及當年/月份上下文實讀；非抽取欄位證據')
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(year,ev))
        if month:results[ident]['historical_death'].update(month=month,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    chongage=chunk_ev('js_09','咸康七年薨，年三十一','晉書東海哀王沖傳','咸康七=341，31為本人虛歲，推生311；不是確生日')
    results[8003]['historical_birth']=date(311,chongage,age=31)
    results[8003]['historical_death']['day_ganzhi']='辛酉'
    results[8003]['evidence'].append(chongage)
    results[8025]['historical_death']['day_ganzhi']='丙申'
    results[8007]['historical_death']['year']=None
    results[8007]['historical_death']['precision']='disputed_year'
    results[8007]['unresolved'].append('嚴格校勘：通鑑366與晉書371異說未決，頂層year撤空，保留year_alternatives，不把優選366供單確年補丁。')
    results[8012]['last_attested_activity']=dict(year=370,source='資治通鑑太和五370十二月',basis='燕亡後入秦為給事中，明活至370；不取單晚本372范陽守為已證年')
    results[8012]['attested_activity_years']=[336,339,349,370]
    for ident in [8006,8010]:
        results[ident].update(review_status='secondary_source_checked',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,identity_status='cbdb_original_row_read_epitaph_not_independently_consulted',last_manual_review_batch='round18c',source_reported_estimates=[dict(year=394,precision='approximately_estimated_by_Tackett',source='CBDB c_notes cc394 and c_dy_range2')])
        results[ident]['evidence'].append(dict(citation='CBDB original BIOG_MAIN row individually read',excerpt=json.dumps(originals[ident]['cbdb_originals'],ensure_ascii=False),verification='database_row_read; epitaph_not_consulted'))
        results[ident]['unresolved'].append('CBDB index331/type29不是出生年，卒394明標Tackett cc394估算/range2，不能升為史實確年；宋恭洛陽新獲304、徐景初文物1999.1:93等所引墓誌尚未取得，身份待補。')
    for ident,note in {8001:'三key同慕容仁司馬遼東佟壽，曾奉皝討仁而降仁、336仁敗奔高句麗；翟楷龐鑑被斬不是佟壽死。未讀墓誌，不擅以冬壽墓推日期，生卒空。',8002:'15key全讀，司馬遂曾孫勳字偉長、令狐泥養子、316十餘歲無精生年；梁州守、349關中、354子午、365反、366五月朱序周楚擒送桓溫斬同官鏈。五月戊寅是庾后崩日，非勳斬日。JS/SL殺張琚說與TJ張琚殺杜洪另存，非本人換人。',8003:'六key元帝子沖道讓、石婕妤所生、過繼越後，東海哀王同人；341八月辛酉卒31推生311虛歲。311幼兒過繼非武將初仕，不能以此強設登場年。裴妃/世子毗/隆安彥璋死不套本人。',8004:'兩key督護喬球朝廷救石聰彭彪未至，虎殺的是聰等不是喬球，無生卒。',8005:'六key羌酋姜聰刺吐延，葉延母明言仇賊已為諸將屠膾，故知道被殺但母子回述未明其殺年，不逕套吐延329；昂城/帛城異文保留。',8008:'兩key皝帳下督張英百騎擒段宇文使，同335事件。殺使者非張英死，不與漢末同名自動合。',8009:'五key苻健妻弟、生舅強平，356四月諫風變被凿頂殺，有TJ/WS獨立早源，非SL單源確年；無出生。',8011:'三key慕容蘭汴城守為荀羨臨陣斬，356十一月後十二月前，JS荀羨38歲非慕容蘭年齡。',8012:'10key皝弟評、俊叔上庸王、輔弼至太傅、潞川敗奔高句麗送秦給事中，黃泓所涉許敦諂評亦同廷同人；慕容垂稱叔父可合。無本人生卒，372范陽守僅晚出SL，獨立TJ明370給事中活動可保。',8013:'三key平文第四子孤、屈弟、昭成弟、讓國迎兄、監北部同人；本傳薨無年，子斤長安死及後代卒年不套孤。',8014:'兩key樂陵青州刺史朱禿殺慕容鉤後奔龕，356十一月被押薊具五刑，明死刑；斬鉤354不是自己卒年。',8015:'單SL李播朱提籍興古守轉南廣，333記載是晚出重編活動，不作已證校準輸入，無生卒。',8016:'三key杜洪弟郁早降健、後洛州守王堕甥，被趙韶譖而生殺356正月；洪弟與王堕甥是兼具族系不矛盾。TJ隨之甥為字訛/傳本異字，SL明堕之甥不另造隨。',8017:'三key楊初子國、誅式王宋奴繼仇池公，被從父俊殺；魏書南齊書明永和十二、TJ356歲末總結，確年356不硬12月。',8018:'四key霸城王墮安生、苻洪司馬、健司空受遺、生殺356正月；董龍是董榮小字不是王墮字，後追復贈葬非復生。',8019:'單SL張駿部曲督王豐報謝並334同耿訪齎印，原key限定這支，不能僅同卷張軌從事同名就硬混；晚本活動不入已證字段，無生卒。',8020:'兩key段龕徐州刺史陽都公（SL陽郡異文）王騰先降燕後攻鄄城，356被荀羨俘斬。JS荀羨傳王騰非currentkey但同案已讀，保守確年不硬秋幾月。',8021:'真義比流王三十年受內臣佐平，原文本月十/七有校記；無生卒，未取得王即位絕對紀年同系錨，不把附近公元條套年。',8022:'三key苻生太醫程延診食棗被斬，同人無出生；TJ356十月條明殺，不把病枣認成他病死。',8023:'三key安定軍諮祭酒到金紫光祿程肱，356三月諫渭橋徭役被殺，同官鏈無出生。',8024:'單TJ僑人蓋肫奪女誣劉胤，郭默斬劉胤張滿非蓋肫死亡，無生卒。',8025:'兩key段龕右長史袁範356正月淄水被慕容恪斬；TJ丙申而SL丙戌異日不混，獨立早源採TJ干支但保晚本異記。'}.items():results[ident]['unresolved'].append(note)
    round19a_reads={8026:[('sl_04','尚书仆射贾玄石'),('wsh_02','尚书仆射贾玄石')],8027:[('js_15','陇西贾陵等十二人'),('sl_08','选陇西贾陵等十二人'),('tj_15','选陇西贾陵等十二人')],8028:[('sl_03','辛牢为右司马'),('sl_04','引弓射牢杀之'),('tj_17','引弓射牢，杀之'),('wsh_02','引弓射牢而杀之')],8029:[('tj_15','遣其将郭太追之'),('tj_16','虎遣将军郭太、麻秋')],8030:[('sl_03','龛友辟闾蔚被创'),('tj_17','齐王龙辟闾蔚被创'),('sl_07','浑闾父蔚昔同段龛')],8031:[('sl_02','居就令刘程以城降')],8032:[('tj_15','封彭城王纮子浚为高密王')],8033:[('gs_00','朗弟子令韶')],8034:[('sl_01','父周曷朱一名乞翼加'),('wsh_01','父周曷朱，一字乞翼加')],8035:[('js_13','嗣子沉')],8036:[('sl_01','宇文屋孤并献名马')],8039:[('sl_04','太史令康权言于生'),('tj_18','六月，太史令康权言于秦主生')],8040:[('gs_00','康法朗。中山人')],8041:[('sl_02','皝别将张萌逆击败之'),('tj_15','都尉张萌击擒之')],8042:[('sl_02','谒者徐孟闾丘幸'),('tj_15','谒者徐孟策拜慕容皝')],8043:[('sz_00','父忡，仕伪赵为'),('sl_02','徐忡东莞人仕勒为外兵郎')],8044:[('sl_02','城大慕舆埿并力拒守'),('sl_03','轻车将军慕舆埿追获其母周氏')],8045:[('tj_15','城大慕舆泥并力拒守'),('tj_16','轻车将军慕舆泥追获其母周氏'),('tj_17','命慕舆泥槎山通道')],8046:[('gs_01','并州支昙讲阴持入经')],8047:[('gs_00','雅弟子昙习祖述先师')],8048:[('sl_01','兰陵朱纵杀刺史郭祥'),('tj_15','兰陵朱纵斩刺史郭祥')],8049:[('sl_03','河间李黑聚众千余') ]}
    for ident,qs in round19a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第19批A·傳記同卷集中逐人實讀','Actual paragraph read; surname omission and variant characters checked against role and relatives') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=ident!=8039,identity_status='all_listed_source_subjects_adjudicated' if ident!=8039 else 'two_qin_historian_keys_linked_cbdb_descent_unbridged',last_manual_review_batch='round19a')
    xin356=chunk_ev('tj_17','壬戌，生宴群臣于太极殿','資治通鑑永和十二356正月壬戌','辛牢被生射殺；原文同次宴會引弓具名，月份取當年正月')
    results[8028].update(review_status='primary_text_corroborated',historical_death=date(356,xin356,month=1,day_ganzhi='壬戌'))
    results[8028]['historical_death'].update(precision='lunar_month_sexagenary_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8028]['evidence'].append(xin356)
    bi356=chunk_ev('tj_17','齐王龙辟闾蔚被创','資治通鑑永和十二356正月淄水之戰','蔚被創後恪遣人求、已死，非僅軍敗；恪358自愧是追述不延蔚壽命')
    results[8030].update(review_status='primary_text_corroborated',historical_death=date(356,bi356,month=1))
    results[8030]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8030]['evidence'].append(bi356)
    kang357=chunk_ev('tj_18','六月，太史令康权言于秦主生','資治通鑑升平元357六月','本人諫天變為苻生撲殺；CBDB裸祖譜未與此人閉合，僅軍政來源支採用')
    results[8039]['component_identities']=[dict(identity='前秦太史令康權',keys=[k for k in results[8039]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=date(357,kang357,month=6)),dict(identity='CBDB32491康權、後裔希銑32492之祖譜待核',keys=['cbdb:32491'])]
    results[8039]['evidence'].append(kang357)
    for ident in [8037,8038,8039,8050]:
        results[ident]['evidence'].append(dict(citation='CBDB original BIOG_MAIN row individually read',excerpt=json.dumps(originals[ident]['cbdb_originals'],ensure_ascii=False),verification='database_reference_read; cited_ancient_genealogy_not_independently_consulted'))
    for ident in [8037,8038,8050]:
        results[ident].update(review_status='secondary_source_checked',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,identity_status='cbdb_reference_read_cited_genealogy_identity_unverified',last_manual_review_batch='round19a')
    results[8044]['evidence'].extend([chunk_ev('sl_03','平北将军武强公慕舆埿','十六國春秋建熙八367雲中','埿逃、慕輿賀辛戰沒，不能套後者卒年'),chunk_ev('tj_17','命慕舆泥槎山通道','資治通鑑永和六350伐趙','同輕車將軍開道、342獲周氏、334柳城守，埿泥異體同人')])
    results[8044].update(review_status='duplicate',duplicate_of=8045,identity_status='muyu_ni_character_variant_five_source_subjects_adjudicated')
    results[8045]['evidence'].extend([chunk_ev('sl_03','轻车将军慕舆埿槎山开道','十六國春秋前燕錄','同通鑑350槎山開道及342王母周氏俘獲，埿泥字異合一'),chunk_ev('sl_02','城大慕舆埿并力拒守','十六國春秋慕容皝柳城守','與TJ同石琮/段蘭牛尾谷軍役，跨書身份閉合')])
    results[8045]['attested_activity_years']=[334,339,342,350]
    results[8045]['last_attested_activity']=dict(year=350,source='資治通鑑永和六350',basis='軍前槎山開道；367雲中僅SL，未當已證年')
    results[8048]['source_chronology_notes']=[dict(source='資治通鑑',year=334,month=12,event='朱縱斬郭祥歸晉奔淮南'),dict(source='明重編十六國春秋',year=335,event='同事件接建武元年條，非第二次重演；不直接輸入單確活動年')]
    for ident,note in {8026:'兩key苻生尚書僕射賈玄石美貌、妻問遭殺同人。WS只列暴政連事無本人獨年；SL接356十月不是獨立早源，知道被殺但卒年空，不與賈玄碩自動合。',8027:'三key隴西賈陵配耿訪十二人、托商達詔張駿，333達涼州前已有使職，不把首次載名當初仕；本人無生卒。',8028:'四key北地辛牢苻洪中郎、健右司馬/吏部、生尚書令酒監，356正月壬戌射死同人；後追復官非仍活，無出生。',8029:'兩key郭太石虎將333擒石堪、338同麻秋追段遼官軍鏈相容；炙殺者石堪、斬3000是部眾，不能當郭太卒，無生卒。',8030:'三key辟閭蔚段龕友、子渾、356正月淄水被創不治；南燕後檄文父蔚昔死是回述不延至399。TJ辟闾/SL闢閭字異同，無出生。',8031:'單SL劉程慕容仁居就令降皝，斬仁置守宰不具名程、不能因泛稱而推死；無生卒，334僅晚本不輸已證活動。',8032:'單TJ司馬紘子浚330封高密，非西晉宣帝子扶風武王駿自動同人；封爵不證生年亦未必武職初仕，無生卒。',8033:'呂韶即康法朗弟子令韶，雁門呂姓、柳泉山刻師木像；孫綽正像論語明本人同名，法乘13、竺潛18/24等鄰傳年齡不套；無生卒。',8034:'兩key石勒父周曷朱又乞翼加部落小帥，石勒14歲/20歲等均子年齡，父無生卒；不拿後來追尊當父在世。',8035:'原晉書孔群嗣子沉字德度省姓，孔坦從弟何充薦王導，子𫷷孫琳之，同candidate角色；孔群卒於官是父不是本人，無生卒。',8036:'單SL宇文屋孤獻馬並其使宴，無生卒，建平紀年仍單晚本不能輸已證；不憑宇文酋長名猜同乞得歸。',8037:'崔牧CBDB只XTS72C.27a祖譜定位、所有生卒0/無任官，未取所引新唐書表，身份年代待核，不能套遊戲3世紀估算為史實。',8038:'崔蔭CBDB只XTS72C.27a祖譜定位，無生卒任官，本人年代待所引新唐書表，未知不填。',8039:'SL/TJ康權太史令357六月因天象諫生被撲殺，可確軍政支；CBDB32491只有See descendant Xixian32492，未得後裔祖譜身份橋，故整人日期空、死亡只留支，不以同名硬套。',8040:'康法朗中山僧西行病僧、還中山門徒數百，原文明後不知所終；法雅格義/孫綽贊同人。無確生卒，郎弟子刻像在後無獨年。',8041:'兩key張萌皝將拒徒河、武興擒李詠同人，李詠被擒不是張萌死，無生卒。',8042:'兩key徐孟晉謁者策皝被仁扣、後歸建康，扣留與使還非生卒，不拿335/336使還為本人死。',8043:'兩key徐忡東莞、女安令首/徐姓令首、外兵郎至石虎清河（SL清和）太守，完整同一女出家故事可合。淨檢70及鄰傳年齡不是父忡，父無生卒；佛圖澄示前生是宗教敘事非可定父生年。',8044:'埿與泥五key已逐原段核同柳城石琮守、342擒周氏、350槎山開道，→8045；367雲中敗走未死，戰沒是慕輿賀辛。無生卒。',8045:'三TJ與兩SL慕輿泥/埿同人閉合，334至350獨立活動、367僅SL不進已證約束。未知生卒，不硬把棄城逃當死。',8046:'原文並州支昙講陰持入經，昙講是姓名/下講為動詞省略，按candidate與竺法濟同道安師角色核；道安45及晚卒年不套師，無生卒。',8047:'竺法雅弟子曇習為太子石宣所敬（原右宣字訛），不因石宣348被誅推出曇習同死；無生卒。',8048:'朱/硃縱同蘭陵徐州從事殺郭祥彭城歸晉、奔淮南，TJ334十二月與SL335建武條叙次差並列保存；出奔非卒，無生卒。',8049:'李黑河間千人叛燕被傅顏斬，單SL記357無獨早源確年；歷史卒年留空，晚本死亡說保證據，銅馬49是馬齡不是李黑。',8050:'柳恭原CBDB只周書22.369，無生卒任官，未得該原傳不定祖輩年代；遊戲估算保留但史實不填。'}.items():results[ident]['unresolved'].append(note)
    round19b_reads={8051:[('sl_02','京兆樊[一作绥]经竺龙')],8052:[('sl_02','段龛[兰之子]据陈留'),('sl_03','龛面缚出降并执朱秃'),('tj_17','龛，兰之子也'),('tj_18','燕主俊杀段龛'),('wsh_05','郁兰死，子龛代之'),('sl_07','段龛干纪取灭于前朝'),('sl_08','慕容恪之攻围段龛十旬不拔')],8053:[('js_17','太医殷腾及外国道士'),('sl_02','太医殷腾及外国道士')],8054:[('gs_00','乃毗浮相昙等')],8055:[('sl_02','扬威将军淑虞攻乌丸')],8056:[('sl_09','其将焦会罗凯等')],8057:[('sl_02','游于鄠县爰赤眉家')],8058:[('sl_03','陇西牛夷北地辛牢'),('sl_04','公嫌所载轻乎朕将以鱼公爵位处公'),('tj_17','陇西牛夷、北地辛牢'),('tj_18','夷惧，归而自杀')],8059:[('sl_02','将军牛旋杨康等来御'),('sl_09','乃杀宿嫌牛旋等'),('tj_16','张重华使将军牛旋御之'),('wsh_04','乃杀宿嫌牛旋等')],8060:[('sl_01','王羌兄子擢与羌有仇'),('sl_02','镇逺将军王擢表'),('sl_03','俊以王擢为益州刺史'),('sl_04','十一月王擢帅众来降'),('sl_08','张瓘败赵将王擢于三交城'),('sl_09','王擢屯结陇上为秦苻雄所破'),('tj_16','凉州将张瓘败赵将王擢'),('tj_17','十一月，擢帅众降秦'),('js_15','王擢屯结陇上'),('sl_05','转攻王擢于陇上')],8061:[('sl_01','虎遣将军王朗讨'),('sl_02','领军将军王朗言于虎'),('sl_04','青州刺史王[晋书作袁]朗于卢氏堡'),('tj_15','赵将王朗攻之'),('tj_16','初，赵领军王朗言'),('tj_17','王朗、麻秋自长安赴洛阳')],8062:[('tj_15','尚书令王瑰为司徒')],8063:[('sl_09','尚书令王瓌为司徒')],8064:[('sl_01','秦州牧休屠王羌叛勒')],8065:[('sl_02','侍御史王齐祭辽东公廆'),('tj_15','侍御史王齐祭辽东公廆')],8066:[('sl_01','申锺为侍中'),('sl_02','司徒申锺諌曰'),('sl_03','申锺为大将军右长史'),('tj_15','魏郡申钟为侍中'),('tj_16','司徒申钟谏曰'),('tj_17','以申钟为大将军右长史')],8067:[('gs_00','乃毗浮相昙等')],8068:[('sl_02','石肇勒之昆弟也')],8069:[('js_17','时石葱将叛'),('sl_02','俄而石葱叛走')],8070:[('tj_15','逖庶子道重，始十岁'),('js_19','潜取逖庶子道重'),('sl_02','窃取逖庶子道重匿之')],8071:[('js_18','依胶东富人程安寿')],8072:[('gs_00','法雅。河间人')],8073:[('sl_02','赤眉与经为左右丞相龙谌为左右大司马')],8074:[('sl_09','其将焦会罗凯等')],8075:[('tj_15','初，丁零翟斌，世居康居')]}
    for ident,qs in round19b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第19批B·同史卷原文身份及紀年審讀','Passage and relevant campaign/family context actually read; death of another subject not transferred') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=ident!=8061,identity_status='all_listed_source_subjects_adjudicated' if ident!=8061 else 'later_qin_qingzhou_wang_yuan_lang_not_bridged_to_zhao_general',last_manual_review_batch='round19b')
    kan357=chunk_ev('tj_18','燕主俊杀段龛，坑其徒三千馀人','資治通鑑升平元357六月後秋七月前','龕本人明被俊殺，魏書又明毒目而殺；不把356降當卒')
    results[8052].update(review_status='primary_text_corroborated',historical_death=date(357,kan357))
    results[8052]['evidence'].append(kan357)
    niu357=chunk_ev('tj_18','夷惧，归而自杀','資治通鑑升平元357五月後六月前','牛夷本人自殺明文；魚遵同被誅18口不是牛夷年齡')
    results[8058].update(review_status='primary_text_corroborated',historical_death=date(357,niu357))
    results[8058]['evidence'].append(niu357)
    daozhong=chunk_ev('tj_15','逖庶子道重，始十岁','資治通鑑咸和五330祖約族誅案','父逖子道重當時10虛歲，330-10+1=321；本人逃免非330死')
    results[8070].update(review_status='primary_text_corroborated',historical_birth=date(321,daozhong,age=10))
    results[8070]['evidence'].append(daozhong)
    results[8063].update(review_status='duplicate',duplicate_of=8062,identity_status='chenghan_wang_gui_same_office_succession_variant_character')
    results[8062]['evidence'].append(chunk_ev('sl_09','尚书令王瓌为司徒','十六國春秋李期玉恒元年','與TJ李班時掌政、期即位後驃騎尚書令升司徒，瑰瓌字異唯一職鏈'))
    results[8061]['component_identities']=[dict(identity='後趙王朗334至350領軍車騎、杜洪舊主',keys=['sl:sl_01:481:王朗','sl:sl_02:83:王朗','tj:tj_15:281:王朗','tj:tj_16:122:王朗'],historical_birth=None,historical_death=None),dict(identity='356前秦青州王朗、JS異文袁朗',keys=['sl:sl_04:97:王朗'],historical_birth=None,historical_death=None),dict(identity='TJ17 candidate跨後趙車騎及前秦青州、待官鏈橋',keys=['tj:tj_17:65:王朗'])]
    results[8066]['last_attested_activity']=dict(year=352,source='資治通鑑永和八352八月',basis='申鍾鄴陷送薊後任燕大將軍右長史，非與自殺三僕射同死')
    for ident,note in {8051:'樊經原文樊[一作綏]經，與爰赤眉劉光杜陽聚眾，同段石廣斬的是劉光並無樊經個別死亡，單晚本無生卒。',8052:'七key段蘭/郁蘭子龕、陳留至廣固齊王、公、降燕伏順將軍，魏書玄恭即慕容恪避魏先祖諱字異；357被俊殺，非356淄水敗/降即死。南燕檄文與劉裕圍城舊事是後世回述不延壽，無出生。',8053:'兩key後趙太醫殷騰治石邃子疾，三日後死的是小阿彌，非醫殷騰；無生卒。',8054:'高僧傳法雅段毗浮與相曇辨格義訓門徒，無生卒；不拿師徒或石宣死年填。',8055:'淑虞單SL皝揚威將軍斬悉羅侯，悉羅侯死不是虞死。無生卒、334僅晚出本不輸已證活動。',8056:'焦會同羅凱李玝將334棄涪城降晉，死者李班/羅演非焦會，無生卒，晚出本活動不獨作已證。',8057:'爰赤眉鄠縣劉光岳父左右丞相，同劉光妖術敘事，劉光被斬不具名說赤眉亦死；無生卒。',8058:'四key隴西牛夷苻洪從事中郎/健安集或綏集將軍、生金紫至中軍，357懼魚遵案歸自殺；求荊州出鎮上洛地點敘法不同並存，不自動拆。無出生。',8059:'四key前涼牛旋347拒趙枹罕至張瓘宿嫌被殺，WS/SL說水火變而殺無獨立確年，不硬把瓘359死套旋；SL戰沙阜與TJ退枹罕/楊康沙阜細次異保留，無生卒確年。',8060:'十key休屠王羌兄子王擢、趙西部將至涼秦州守、降秦尚書，隴右連續官軍事件可合；SL涼錄把奔涼/龍黎前移351/352與TJ352/353次序差，本文不拿單晚本年份作生卒。354十一月降秦非死亡，歷史生卒空；張弘宋修陣沒非王擢。',8061:'六key均已讀，四後趙角色同334追朱縱、領軍諫伐材、349車騎入關、350赴襄國；TJ17自身candidate又收356前秦青州王朗，SL原注晉書袁朗，未讀明仕秦身分橋，故暫整人hold並分支，非斷言一定異人。無本人生卒。',8062:'TJ王瑰李班尚書令、李期司徒，SL王瓌同獨特官鏈字異，吸收8063；無生卒，不拿班弒/期死套本人。',8063:'單SL成漢尚書令王瓌→司徒，同8062瑰字異兩key已實讀閉合→8062，無生卒。',8064:'王羌休屠王叛勒、兄子擢與石生攻之、奔涼，出奔非死；單SL無生卒，族王頭銜不推出生。',8065:'兩key王齊祭廆使被仁留、335南返遇風後棘城受命，廆死333非齊死；無生卒。',8066:'六key申鍾/钟魏郡侍中→趙司徒→魏太尉→燕大將軍右長史，同廷官鏈與石璞趙攬諫詔可合；352送薊後任職明仍活，自殺為王簡張幹郎肅，無生卒。',8067:'相曇同毗浮辨格義，原文無生卒，不套後文康法朗/法乘年齡。',8068:'石肇勒昆弟長樂守劉典兄女婿怕妻，石堪被炙是鄰傳不是他，單SL無生卒。',8069:'兩key石蔥將叛逃走與食蔥隱語，同佛圖澄傳，出走非死；不是石聰因同音自動合，石勒子斌病是鄰事，無生卒。',8070:'三key祖逖庶子道重、王安匿僧避祖約330族誅、石氏亡後歸晉。330十虛歲推321生，並非自己330死；石氏亡後歸無明日年，不推精卒或350/351精返。',8071:'程安壽膠東富人護韋逞母宋氏流徙冀州，母80歲/苻堅太學事在後，不證程此時仍活或80；無生卒。',8072:'竺法雅河間、與法朗格義、道安法汰釋疑、高邑寺、弟子曇習同僧傳完整身份，無生卒；石宣敬弟子非其年壽。',8073:'竺龍劉光大司馬與嚴謀/諶同，未具名同劉光斬，無生卒；單晚本活動不確年輸入。',8074:'羅凱李玝將同焦會降晉，無生卒，不拿同卷班舅羅演被誅當羅凱死。',8075:'翟斌丁零世居康居徙中國330來趙封句町王，封爵活動非出生。只有本key來源未足串後燕翟斌族系，先保此身份生卒未知，未按同名自動跨片合。'}.items():results[ident]['unresolved'].append(note)
    round20a_reads={8076:[('sl_01','祖耶奕干父周曷朱')],8077:[('sl_04','清河王法与左卫将军飞克鄠'),('sl_05','苻法字永则小字阿法'),('tj_17','清河王法、苻飞分讨孔持等'),('tj_18','赐法死'),('wsh_02','以法为丞相、东海公')],8078:[('js_12','诈云杀苻健、苻眉'),('wsh_00','姚襄为苻眉所杀'),('sl_04','健兄子辅国将军黄眉自洛阳来奔'),('sl_05','黄眉怒谋杀生自立'),('sl_06','健兄子苻黄眉自洛阳西奔'),('tj_17','卫将军广平王黄眉'),('tj_18','黄眉怒，谋弑生')],8079:[('sl_04','尚书董荣为右仆射'),('tj_17','董龙是何鸡狗'),('tj_18','诛生幸臣中书监董荣'),('sg_12','益州刺史董荣图画周像'),('sl_05','董龙[荣之小字也]')],8080:[('js_13','尝与蔡系送客于征虏亭'),('ssxy_01','蔡系字子叔济阳人司徒谟第二子')],8081:[('sl_02','襄国人薛合有二子')],8082:[('sl_03','杀枣强令卫颜')],8083:[('sl_02','谢乐子等众至数千人')],8084:[('js_11','杀江州督护赵毗')],8085:[('sl_08','西域长史李栢请击叛将赵真')],8086:[('sl_04','右仆射赵韶为左仆射'),('tj_17','左仆射赵韶恶之'),('tj_18','左仆射赵韶等二十馀人'),('js_09','韶于狱自尽'),('sl_05','赵韶董荣奸佞乱政')],8087:[('sl_02','道进佛图澄弟子也')],8088:[('sl_02','邢嘏河间鄚人也')],8089:[('js_01','与河间邢鱼有隙')],8090:[('sl_02','邴辅栎陵人也')],8091:[('sl_02','与河间郝鱼有隙')],8092:[('sl_02','谒者徐孟闾丘幸')],8093:[('sl_05','依胶东富人陈安寿')],8094:[('sl_01','堂阳人陈猪妻一产三男')],8095:[('sl_02','辽东太守阳鹜为才而让之'),('sl_03','皝临终谓俊曰阳士秋'),('tj_15','辽东太守阳骛为才而让之'),('tj_16','左长史阳骛郎中令'),('tj_17','慕容恪、封弈、阳骛攻之'),('tj_18','十二月，甲子，燕太尉建宁敬公阳骛卒')],8096:[('sl_01','韩晞为左仆射'),('tj_15','韩晞为尚书左仆射')],8097:[('sl_03','冯翊鱼遵为后将军'),('sl_04','八月以左仆射鱼遵为司空'),('tj_17','冯翊鱼遵为右将军'),('tj_18','广宁公鱼遵，并其七子、十孙'),('wsh_02','诛太师鱼遵父子一十八人'),('sl_05','鱼遵冯翊人也')],8098:[('sl_08','参军黄斌进曰'),('js_15','参军黄斌进曰')],8099:[('js_14','并免尚书傅玩')],8100:[('js_10','彝字大伦') ]}
    for ident,qs in round20a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第20批A·前秦政變與晉燕人物逐來源原段審讀','Subject identified from actual office/family/event context; branch conflicts and uncertain source identity isolated') for ch,phrase in qs)
        closed=ident not in [8079,8080,8086]
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=closed,identity_status='all_listed_source_subjects_adjudicated' if closed else 'source_components_unresolved_or_mixed',last_manual_review_batch='round20a')
    for ident,phrase,year,month,day in [(8077,'赐法死',357,11,None),(8078,'黄眉怒，谋弑生；发觉，伏诛',357,5,None),(8095,'十二月，甲子，燕太尉建宁敬公阳骛卒',367,12,'甲子'),(8097,'生乃诛太师、录尚书事、广宁公鱼遵',357,None,None)]:
        ev=chunk_ev('tj_18',phrase,f'資治通鑑{year}年本人死亡原句'+(f'農曆{month}月' if month else ''),'獨立通鑑年序實讀，非後追復官爵或被他人引用仍在世')
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(year,ev))
        if month:results[ident]['historical_death'].update(month=month,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        if day:results[ident]['historical_death']['day_ganzhi']=day
        results[ident]['evidence'].append(ev)
    dong357=chunk_ev('tj_18','诛生幸臣中书监董荣','資治通鑑升平元357六月堅即位後','董榮趙韶同誅，只能給前秦支，不能套益州刺史同名')
    for ident,identity in [(8079,'前秦董榮小字董龍'),(8086,'前秦趙韶南安趙俱從弟')]:
        results[ident]['component_identities']=[dict(identity=identity,keys=[k for k in results[ident]['keys'] if not k.startswith('sg:') and not k.startswith('js:')],historical_birth=None,historical_death=date(357,dong357,month=6))]
        results[ident]['evidence'].append(dong357)
    results[8079]['component_identities'].append(dict(identity='三國志注益部耆舊傳益州刺史董榮、圖譙周像',keys=['sg:sg_12:268:董榮'],historical_birth=None,historical_death=None))
    results[8086].update(review_status='identity_conflict',identity_status='mixed_qin_minister_and_jin_taocheng_subordinate')
    results[8086]['component_identities'].append(dict(identity='陶稱部將趙韶、咸康五339奏列此前已獄中自盡',keys=['js:js_09:269:趙韶'],historical_birth=None,historical_death=None,death_upper_bound=339))
    results[8080]['evidence'].append(dict(citation='CBDB original BIOG_MAIN row12007 individually read',excerpt=json.dumps(originals[8080]['cbdb_originals'],ensure_ascii=False),verification='genealogical_reference_not_bridged_to_Cai_Mo_second_son'))
    results[8080]['component_identities']=[dict(identity='蔡謨次子子叔、撫軍長史',keys=[k for k in results[8080]['keys'] if not k.startswith('cbdb:')]),dict(identity='CBDB12007兄劭祖穆、撫州守記錄，待祖譜原文',keys=['cbdb:12007'])]
    results[8091].update(review_status='duplicate',duplicate_of=8089,identity_status='xing_hao_yu_same_stolen_horse_pei_brothers_false_accusation')
    results[8089]['evidence'].append(chunk_ev('sl_02','与河间郝鱼有隙','十六國春秋裴憲二子被讒事','與晉書邢魚相同裴氏兄弟/竊馬/段遼/石虎伐遼唯一事件，姓名源別同人'))
    results[8093].update(review_status='duplicate',duplicate_of=8071,identity_status='chen_cheng_anshou_same_jiaodong_patron_of_song_and_wei_cheng')
    results[8071]['evidence'].append(chunk_ev('sl_05','依胶东富人陈安寿','十六國春秋韋逞母宋氏傳','與晉書程安壽同膠東富人養護母子、鹿車負周官、學家多士大夫語，陳程姓異同一人'))
    for ident,note in {8076:'石勒祖耶奕於source本作耶奕干（魏書邪弈于），子周曷朱部落小率，同祖父位次；無本人生卒，石勒14/20歲不能套祖父。',8077:'五key苻雄庶長子法永則阿法，清河王→357政變讓弟堅→東海公，357十一月死。TJ/SL太后李威謀賜，魏書簡稱堅疑忌殺，主使異叙保留而年度依TJ；其子陽後事不是法仍活，無出生。',8078:'七key苻健兄子眉/黃眉自洛陽西奔、廣平王、強平諫、討姚襄357五月，返後謀生事洩被誅；JS雷弱兒诈說已殺苻眉是假報不是353真死，魏書姚襄為眉殺省名同戰役。無出生。',8079:'五key實讀：四前秦董榮董龍著作郎→右僕射/中書監、357誅；SG益部耆舊傳益州刺史董榮畫譙周像未有與前秦仕履橋，暫分支而非斷定同人；前秦357六月只分支日期，整人空。',8080:'兩書蔡系子叔蔡謨次子、謝萬送支遁爭席、撫軍長史可合；CBDB12007只引兄劭祖穆且稱撫州守，尚無蔡謨父系原文橋，不因index330就合，整人hold且出生未知。',8081:'薛合二子被鮮卑奴殺，父目睹請援不等父亦死；無生卒，單SL不拿兒女死當父卒。',8082:'衛顏棗強令為李黑殺明死亡，但357紀年僅晚出SL，日期頂層仍未知；晚本表述留引文，不能作已證校準。',8083:'謝樂子劉光杜陽山大將軍，石廣斬劉光不具名說樂子死，無生卒，單SL。',8084:'趙毗桓沖未到江州前為姚襄降將張駿所殺，原傳未明精年；桓沖在州13年至桓溫卒可作約束線索但不當本人確卒年，出生亦空。',8085:'單SL趙真戊己校尉叛張駿敗李柏，JS同卷另作趙貞被擒已讀但未獨立判本key是否同名待校，不據被擒當死，無生卒。',8086:'五key有明不可能同生人：JS陶稱奏339之前趙韶已獄中自盡，而前秦趙俱從弟355–357侍生左僕射、357被堅誅。top生卒空，branch秦357六月、陶氏部將只卒339前/當年不精化；不可拿357延陶將壽。',8087:'道進澄弟子諫虎釋楊軻，同沙門身份，楊軻命懸/後殺不是進死；無生卒，單晚出本不推年份。',8088:'邢嘏河間鄚隱者叛降勒征北參軍，侄蓋嗣、無本人生卒；下宣咸太仆卒/五年後夢不套嘏。',8089:'晉書邢魚與SL郝魚河間同竊裴瑴馬告段遼、誣石虎將襲、致裴挹瑴誅唯一事件同人，吸收8091保姓字異；魚被獲非明被殺，無生卒。',8090:'邴輔櫟陵材官營襄國台榭，無生卒，鄰傳韋𫍲被誅不套。',8091:'郝魚同8089邢魚跨二書竊馬誣裴案身份實讀閉合→8089；未知生卒。',8092:'閭丘幸同徐孟謁者策皝被仁留，只有本key無生卒，不能以留使當死。',8093:'陳安壽與8071程安壽兩書同膠東富人養韋逞母子、學家歎，姓異同人→8071。宋母80歲非壽年，無生卒。',8094:'陳豬妻一產三男獲賜乳婢蠲三歲，三歲免役不是年齡；無生卒，單晚出本活動不輸確年。',8095:'六key陽騖/陽鹜士秋、遼東守王誕讓左長史、三輔→尚書令司空太尉四朝，同子瑤可合；TJ367十二月甲子卒獨立源，不從SL泛年耆推生年。',8096:'兩key韓晞石虎左僕射同一任官序列，無生卒；不套石勒死/虎篡當本人死。',8097:'六key馮翊魚遵洪長史、健太子太師到受遺太師、生因大魚謠殺357，七子十孫合18口與WS父子18相應非年齡。無出生，357九月追復葬不等仍活。',8098:'兩key張駿參軍黃斌諫嚴刑→敦煌守，同引法制言語可合；無生卒。',8099:'傅玩尚書咸康初與戴抗劉傭被顧和劾免，免官非死，無生卒。',8100:'刁彝大倫協子，復仇父墓後自首赦、吏部郎吳國內史北中郎徐兖鎮廣陵卒，卒官無明年；父協被殺322及子逵暢404誅並非彝卒年，歷史生卒未知。'}.items():results[ident]['unresolved'].append(note)
    round20b_reads={8101:[('js_10','畴兄子劭，有才干'),('js_11','侍中刘劭曰')],8102:[('sl_04','青州刺史王[晋书作袁]朗于卢氏堡'),('tj_17','将军刘度攻秦青州刺史王朗'),('sl_01','中书著作郎王度'),('tj_15','著作郎王度')],8103:[('ssxy_02','刘恢字道生沛国人')],8104:[('js_01','弟珍年八岁，以小弗坐')],8105:[('ssxy_01','亮有大儿数岁雅重之质'),('ssxy_03','诸葛令女庾氏妇既寡')],8106:[('sl_01','冯莫张崇曹显为尚书'),('hy_01','随时同郡繁令张崇'),('js_15','太守张崇义之')],8107:[('js_15','咸康中，太守张虞上疏')],8108:[('sl_02','俊遣将军恱绾率骑三万'),('sl_03','悦绾榼卢城大人也'),('sl_06','俊遣御难将军悦绾'),('tj_16','悦绾'),('tj_17','悦绾'),('tj_18','尚书左仆射广信公悦绾')],8109:[('js_14','劾奏尚书左丞戴抗')],8110:[('sl_08','骏遣参军曲护等上疏'),('tj_15','遣参军麹护上疏'),('js_15','后骏遣参军麹护上疏')],8111:[('sl_01','曹显为尚书')],8112:[('tj_18','特进、姑臧侯樊世'),('sl_05','樊世本氐豪也')],8113:[('js_08','末波之子勤鸠集胡羯'),('sl_02','立义将军段勤鸠集胡羯'),('sl_03','俊杀尚书郎鲜卑段勤'),('tj_17','勤，末柸之子'),('tj_18','燕人杀段勤，勤弟思来奔')],8114:[('sl_02','立夫人叚氏为王后'),('tj_18','燕吴王垂娶段末柸女'),('sl_03','其妃段氏自以贵姓')],8115:[('sl_01','晋将军淳于安攻琅邪费县')],8116:[('sl_01','南中郎将王国征西司马王愆期')],8117:[('js_13','击贼将石遇，破之'),('sl_01','征虏将军石遇率骑七千'),('sl_02','遵将石遇攻宛陷之'),('tj_15','赵征虏将军石遇攻桓宣')],8118:[('yl_09','侍御史秦武奏')],8119:[('js_16','范宣，字宣子'),('js_17','师事术士范宣于豫章'),('ssxy_00','宣字子宣陈留人'),('ssxy_02','范宣未尝入公门'),('ssxy_03','戴安道就范宣学')],8120:[('js_17','子庄，字祖休')],8121:[('sl_03','赵故章武太守贾坚'),('tj_17','初，勃海贾坚'),('tj_18','燕泰山太守贾坚屯山茌')],8122:[('js_10','愔字方回'),('js_18','平北将军郗愔'),('tj_16','父愔，简默冲退'),('tj_18','以会稽内史郗愔为都督'),('js_12','郗愔有伧奴善知文章'),('ssxy_02','郄司空在北府桓宣武'),('ssxy_03','郄愔信道甚精勤')],8123:[('sl_01','广陵相陈光帅众卫京师'),('tj_15','广陵相陈光将兵入卫京师')],8124:[('sl_01','冯莫张崇曹显为尚书')],8125:[('tj_18','匈奴刘悉勿祈卒'),('wsh_00','悉勿祈死，弟卫辰立'),('wsh_02','后务桓子悉勿祈逐阏陋头'),('sl_08','务桓子悉勿祈逐阏陋头')]}
    for ident,qs in round20b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第20批B·東晉前燕原傳及紀年逐鍵實讀','Actual subject and surrounding chronology read; conflicting age/year and merged identities retained separately') for ch,phrase in qs)
        closed=ident not in [8102,8106,8114]
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=closed,identity_status='all_listed_source_subjects_adjudicated' if closed else 'mixed_or_unbridged_source_components',last_manual_review_batch='round20b')
    for ident,phrase,year,month in [(8108,'绾先有疾，自力厘校户籍',368,11),(8112,'坚怒，斩之。于是群臣见猛皆屏息',358,None),(8121,'数日，坚愤惋而卒',358,None),(8125,'匈奴刘悉勿祈卒',359,None)]:
        ev=chunk_ev('tj_18',phrase,f'資治通鑑{year}年本人死亡段'+(f'農曆{month}月' if month else ''),'當年上下文獨立讀明；軍戰或親屬死與本人卒區別')
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(year,ev))
        if month:results[ident]['historical_death'].update(month=month,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    yin384=chunk_ev('js_10','太元九年卒，时年七十二','晉書郗愔傳','太元九384、本人72虛歲，推生313；父鑒子超之卒不移給愔')
    results[8122].update(review_status='primary_text_corroborated',historical_birth=date(313,yin384,age=72),historical_death=date(384,yin384))
    results[8122]['evidence'].append(yin384)
    qin359=chunk_ev('tj_18','燕人杀段勤，勤弟思来奔','資治通鑑升平三359春二月後','早源359，晚重編SL光壽二358十月，同死亡事源年不合')
    results[8113]['historical_death']=dict(year=None,year_alternatives=[358,359],precision='disputed_year',calendar='historical_CE_year',source='通鑑359 vs 明重編SL358',excerpt=qin359['excerpt'])
    results[8113]['evidence'].append(qin359)
    results[8105]['source_chronology_notes']=[dict(source='世說容止本文',event='庾亮長子蘇峻時遇害',year_range=[327,329]),dict(source='同條劉孝標引庾氏譜',event='年十九咸和六年遇害',year=331,age=19)]
    results[8102].update(review_status='identity_conflict',identity_status='jin_general_liu_wang_du_mixed_with_zhao_author_wang_du',component_identities=[dict(identity='356晉將劉度，JS異文王度攻盧氏堡',keys=[k for k in results[8102]['keys'] if k.startswith('sl:sl_04:') or k.startswith('tj:tj_17:')]),dict(identity='後趙著作郎王度、議禁漢人佛寺',keys=[k for k in results[8102]['keys'] if k.startswith('sl:sl_01:') or k.startswith('tj:tj_15:')])])
    results[8106]['component_identities']=[dict(identity='後趙尚書張崇',keys=[k for k in results[8106]['keys'] if k.startswith('sl:')]),dict(identity='蜀郡繁令張崇何隨同時清廉',keys=[k for k in results[8106]['keys'] if k.startswith('hy:')]),dict(identity='吳興守張崇禮吳逵',keys=[k for k in results[8106]['keys'] if k.startswith('js:')])]
    duan358=chunk_ev('tj_18','而段氏竟死于狱中','資治通鑑升平二358歲末','慕容垂元配段末柸女，非慕容皝王后')
    results[8114].update(review_status='identity_conflict',identity_status='mixed_murong_huang_queen_and_murong_chui_first_wife',component_identities=[dict(identity='慕容皝夫人337立王后段氏',keys=[k for k in results[8114]['keys'] if k.startswith('sl:sl_02:')],historical_birth=None,historical_death=None),dict(identity='慕容垂元配段末柸女、令寶母、獄死358',keys=[k for k in results[8114]['keys'] if not k.startswith('sl:sl_02:')],historical_birth=None,historical_death=date(358,duan358))])
    results[8114]['evidence'].append(duan358)
    for ident,note in {8101:'兩key劉疇兄子劭、成帝侍中羽扇解說同朝職相合，無本人生卒；庾懌50自鴆是鄰傳，不是劭。',8102:'四key實讀確混：356晉將劉度/王度攻盧氏是軍人來源支，335/338後趙著作郎王度佛教議禁是文官支；SL注晉書作王只與軍將姓異，不足把著作郎合。整人生卒空、勿刪整行。',8103:'劉恢字道生沛國車騎司馬，庾翼致桓溫書與王濛稱同一人，卒36無明年；不要因同36歲混丹陽尹劉惔真長。',8104:'司馬珍章武王滔子休弟八歲不坐兄罪，331襲爵大宗正薨無年；八歲在蘇峻平的回述上下文，未另取獨立年不硬反生，331襲爵非出生。',8105:'兩key庾亮長子阿恭、諸葛恢女前夫同人，本文蘇峻時遇害與注庾譜咸和六331年19相衝，OCR姓名字防需回版本；不把331定成確卒或從19反生。寡婦後再嫁是後事，不延庾會壽。',8106:'三key原段明讀，但後趙尚書、蜀郡繁令何隨同時、吳興守禮吳逵無父系/轉仕橋，不能僅名同合三身份；均無生卒。吳逵卒於家不是張崇。',8107:'張虞咸康中太守表吳寧故許孜，明臣不及見/孜沒積年，不能拿孜80歲或元康年當虞；無生卒。',8108:'六key悅/恱綰榼盧城大、凡城御難守、救襄國、並州牧、左僕射罷蔭戶，368十一月病劇卒早TJ明年，秦370追念拜子不是父仍活。SL本傳同病卒，未知出生。',8109:'戴抗尚書左丞被顧和劾贓百萬，付法議罪不明已斬，無生卒；與傅玩同免別人。',8110:'曲/麴/麹護三key張駿參軍上疏請郗鑒庾亮江沔齊舉，同使文可合；TJ335 vsSL337敘年異保留不作單確活動推估，無生卒。',8111:'曹顯單SL石虎尚書，列任官非出生，無生卒。',8112:'兩key樊世氐豪佐健定關中姑臧侯辱王猛、爭楊璧婚被斬，同人；TJ升平二358獨立確年，無出生。',8113:'五key段末波子勤、後趙建義、黎陽繹幕趙王、降燕尚書郎同人，弟思陪/思聰/思字異保留。TJ359與SL358十月被誅為同事真年差，不頂層保單年，先並列異說。',8114:'三key混皝夫人王后與垂元配段末柸女，母輩/子媳身份分開；358獄死只垂妻支，整人日期空。不能把皝妻337登后認為垂妻活動。',8115:'單SL晉將淳于安攻費縣俘數千是對敵戰果，非本人死，無生卒。',8116:'王國晉南中郎救襄陽與毛寶愆期同，原candidate已明非降石堪南陽守同名，當前單key不能據名硬合；無生卒。',8117:'四key石遇後趙征虜攻襄陽章山敗退、後遵部攻宛，軍職地域同可合而無本人卒記；軍饑疫不明本人疫死，349陷宛非死亡。',8118:'御覽卷957引晉咸康起居注秦武侍御史劾陵令，杉樹萎死不是秦武死，咸康是職事年限非本人生卒；無日期。',8119:'五key范宣宣子/子宣陳留、豫章授戴逵兄女、隱不仕/韓伯饋絹同族學角色。卒54但無明年，JS插敘太元中范寧立學是另一人不可把范宣活到太元或反算生；10歲誦經也無獨年，日期空。',8120:'翟湯子莊祖休隱居耕釣、州府公車不就，卒56無明年；父73不能當子年歲，子矯孫法賜後事不延壽。',8121:'三key賈堅勃海趙殿中督→燕樂陵守/泰山守、七百山茌拒荀羨，358被俘後雨中憤卒（SL簡稱斬源別）。350年60餘只能上限年代不能編確生年，史實birth空。',8122:'七key郗鑒子方回郗超父、師天師道聚錢、褚裒何充長史、臨海/會稽/北府、369移兵權、384卒72同人。世說批評篡箋為謬/中興書責不從征的緣由差保留，不拆身份。虛歲反生313，非確生日。',8123:'兩key廣陵相陳光郗鑒派入衛京師同335遊騎驚報，非僅同名潁川陳光自動合；無生卒。',8124:'馮莫單SL同四尚書任官序列，無生卒。',8125:'四key務桓子悉勿祈、阏頭侄、衛辰兄同親族及奪眾情節，TJ359卒、WS昭成建國22同年獨立相應。被衛辰殺的是本人之子，不強加本人亦被殺，無出生。'}.items():results[ident]['unresolved'].append(note)
    ni367=chunk_ev('tj_18','泥弃城走，振威将军慕舆贺辛战没','資治通鑑太和二367八月雲中役','武強公幽州兵主泥逃走、賀辛戰沒；與SL同慕輿埿官軍地事，補獨立早源活動')
    results[8045]['evidence'].append(ni367)
    results[8045]['attested_activity_years']=[334,339,342,350,367]
    results[8045]['last_attested_activity']=dict(year=367,source='資治通鑑太和二367八月',basis='泥棄雲中城走，非戰沒的慕輿賀辛；已讀獨立原文，撤此前只有SL367的限制')
    results[8045]['unresolved'].append('續讀TJ18郗愔367九月任官前，現親核八月泥棄雲中城句，367已有獨立早源，可約束遊戲估卒；此前367僅SL的保守說法已被此新讀證據補正。仍未知真卒年。')
    round21a_reads={8127:[('sl_01','使牙门将张弥徙洛阳'),('sl_02','张弥字巨秦汲郡人也'),('tj_15','使牙门将张弥行徙洛阳')],8128:[('js_01','勋乃伪请琚'),('js_15','瓘弟琚'),('sl_03','以冯翊张琚为司马'),('sl_09','瓘与弟琚皆自杀'),('tj_17','凉王祚收张瓘弟琚及子嵩'),('tj_18','瓘与弟琚皆自杀')],8129:[('sl_04','强德健后之弟也'),('tj_18','特进、光禄大夫强德')],8130:[('sl_03','捕诛贼首木谷禾')],8131:[('sl_02','中军将军李咏夜袭武兴'),('tj_15','段辽遣中军将军李咏袭慕容皝')],8132:[('sl_02','兼郎中令辽东杨景送王齐徐孟')],8133:[('sl_03','遣大鸿胪温统拜袁真'),('tj_18','统未逾淮而卒')],8134:[('hy_00','大姓爨深等助炳'),('tj_15','爨深为交州刺史')],8135:[('tj_15','尚方令解飞'),('sl_02','解飞不知何许人')],8136:[('jk_02','尚书谢褒已下免官')],8137:[('sl_08','市长谭详请出仓糓'),('js_15','市长谭详请出仓谷')],8138:[('sl_03','寿充奔髙句骊'),('tj_15','佟寿、郭充、翟楷')],8139:[('sl_04','获其将炖煌阴据'),('sl_08','从事阴据谏曰'),('js_15','从事阴据谏曰')],8140:[('tj_15','兼郎中令辽东阳景送徐孟')],8141:[('sl_02','宋诙刘瞻[一作睦]石琮为常伯'),('tj_15','宋该、刘睦、石琮为常伯')],8143:[('js_13','严字彭祖'),('tj_17','尚书左丞孔严言于殷浩'),('js_11','孔严著《通葛论》'),('ssxy_02','学义不及孔严')],8144:[('sl_11','父僚为张玄靓龙骧将军')],8145:[('sl_02','遣扬烈将军宋回称藩于赵'),('tj_15','遣扬烈将军宋回称籓于赵')],8146:[('sl_11','曾祖配祖悌世仕张轨子孙')],8147:[('sl_06','曽祖奭晋昌黎太守'),('sl_01','护军宋晃等皆应之'),('sl_02','宋晃平熙张泓为将军'),('sl_03','俊赦之更名曰活'),('tj_15','晃，奭之子也'),('tj_16','护军宋晃等皆应之'),('tj_17','宋晃于燕，燕王俊赦之')],8148:[('js_15','混卒，又以澄代之'),('sl_09','宋澄字玄安混之弟也'),('tj_17','混与弟澄西走'),('tj_18','凉右司马张邕恶宋澄专政')],8149:[('wsh_04','玄安司马张邕起兵杀玄安')],8150:[('js_15','弘、悠皆没') ]}
    for ident,qs in round21a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第21批A·同卷族系與軍役逐鍵審讀','Actual source paragraph read; partial chronology and family role not copied across mixed subjects') for ch,phrase in qs)
        closed=ident not in [8128,8139]
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=closed,identity_status='all_listed_source_subjects_adjudicated' if closed else 'source_components_require_separation_or_bridge',last_manual_review_batch='round21a')
    for ident in [8126,8142,8147]:
        results[ident]['evidence'].append(dict(citation='CBDB original BIOG_MAIN row individually read',excerpt=json.dumps(originals[ident]['cbdb_originals'],ensure_ascii=False),verification='original_database_reference_read'))
    results[8126].update(review_status='secondary_source_checked',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='bare_cbdb_tackett_estimate_no_independent_identity_reference',last_manual_review_batch='round21a',source_reported_estimates=[dict(year=397,precision='approximately_estimated_by_Tackett',source='CBDB cc397 c_dy_range2')])
    qiaoev=evidence(SOURCE/'_cache/texts/宋书.txt','靖生郡功曹翘，是为皇考','宋書武帝紀首世系','原書明翹為裕父、靖子，閉合CBDB12173父宋武帝裕身份，無本人生卒')
    results[8142].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='cbdb_explicit_liuyu_father_linked_to_songshu_genealogy',last_manual_review_batch='round21a')
    results[8142]['evidence'].append(qiaoev)
    huoev=evidence(SOURCE/'_cache/texts/魏书.txt','祖活，中书监。父恭','魏書卷三十三宋隱傳','CBDB31278明引此傳；宋隱曾祖奭、祖活中書監與宋晃奭子更名活同代同官，閉合第八key')
    results[8147]['evidence'].append(huoev)
    for ident,phrase,year,month in [(8129,'已陈尸于市，坚驰使赦之，不及',359,8),(8133,'统未逾淮而卒',369,None),(8148,'起兵攻澄，杀之，并灭其族',361,9)]:
        ev=chunk_ev('tj_18',phrase,f'資治通鑑{year}年'+(f'農曆{month}月' if month else ''),'本人死亡句與同年上下文，不以官去任或被俘當死')
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(year,ev))
        if month:results[ident]['historical_death'].update(month=month,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    yanev=chunk_ev('js_13','五年，以疾去职，卒于家','晉書孔嚴傳太和五370','本人太和中吳興守接五年疾去職卒，非祖奕/子弟孫恩遇害日期')
    results[8143].update(review_status='primary_text_corroborated',historical_death=date(370,yanev))
    results[8143]['evidence'].append(yanev)
    results[8132].update(review_status='duplicate',duplicate_of=8140,identity_status='yang_surname_variant_same_liaodong_langzhongling_mission')
    results[8140]['evidence'].append(chunk_ev('sl_02','兼郎中令辽东杨景送王齐徐孟','十六國春秋慕容皝咸康二336九月','與TJ陽景同劉斌護送徐孟等，楊陽姓異唯一使職事件'))
    results[8149].update(review_status='duplicate',duplicate_of=8148,identity_status='song_cheng_explicit_courtesy_name_xuanan_same_brother_of_hun')
    results[8149]['evidence'].extend([chunk_ev('sl_09','宋澄字玄安混之弟也','十六國春秋宋澄傳','澄字玄安明文且同登帶石山/張邕滅宋事件，與WS避以字稱可合'),chunk_ev('tj_18','起兵攻澄，杀之，并灭其族','資治通鑑升平五361九月','宋玄安/宋澄同人獨立確年')])
    results[8149]['historical_death']=results[8148]['historical_death']
    guanzhang=chunk_ev('tj_18','瓘与弟琚皆自杀','資治通鑑升平三359張瓘宋混案','僅涼州張瓘弟琚來源支，不能套關中同名張琚')
    results[8128].update(review_status='identity_conflict',identity_status='mixed_guanzhong_zhangju_and_liangzhou_zhangguan_brother',candidate_duplicate_of=7842,component_identities=[dict(identity='關中杜洪司馬張琚、同7842候選源支',keys=[k for k in results[8128]['keys'] if k.startswith('js:js_01:') or k.startswith('sl:sl_03:')],historical_birth=None,historical_death=None),dict(identity='涼州張瓘弟琚',keys=[k for k in results[8128]['keys'] if not k.startswith('js:js_01:') and not k.startswith('sl:sl_03:')],historical_birth=None,historical_death=date(359,guanzhang))])
    results[8128]['evidence'].append(guanzhang)
    results[8139]['component_identities']=[dict(identity='張駿從事諫穀三倍貸陰據',keys=[k for k in results[8139]['keys'] if not k.startswith('sl:sl_04:')]),dict(identity='張天錫將敦煌陰據、枹罕被俘後放還',keys=[k for k in results[8139]['keys'] if k.startswith('sl:sl_04:')])]
    results[8150]['candidate_duplicate_of']=8948
    results[8150]['unresolved'].append('JS宗悠同張弘王擢龍黎案，SL/TJ稱宋修有同事件別名候選8948；該record另含347守枹罕及355宋混兄支，待8948全key身份審完再硬合，現在不刪宗悠。沒軍可為俘，不當卒。')
    for ident,note in {8126:'崔脩之原CBDB死亡明Tackett cc397估算/range2，index334type29非生年，無可核原文身份引證，歷史生卒空，保估算遊戲年。',8127:'三key張彌巨秦汲郡、武德守梁臣同坑赦到336牙門徙鐘同人，原已承7897張珍御覽異名唯一鐘案不撤；無生卒。',8128:'六key混關中杜洪司馬/司馬勳坐殺說與涼張瓘弟殺張祚359自殺；兩人必拆，不能把JS勳殺琚或TJ苻健斬關中琚套瓘弟，亦不能把359延關中人壽。關中支與7842源別死亡衝突保留，整行duplicate_of仍空。',8129:'兩key強德健后弟/生太后弟、特進光祿、王猛收斬，359八月獨立TJ；非強平356自動異名，無出生。',8130:'木谷禾賊首被誅明文但359只有晚重編SL，歷史確年不填，未知出生。',8131:'兩key李詠段遼中軍夜襲武興被張萌擒，擒非死，無生卒。',8132:'楊景單SL與陽景TJ同遼東兼郎中令護送王齊徐孟劉斌336九月，姓異同事→8140，不與他人只同名合。',8133:'兩key燕大鴻臚溫統使冊袁真未渡淮卒，369獨立TJ原句；非袁真370卒，無出生。',8134:'兩key爨深建寧大姓332助朱提、334交州刺史，與SL爨琛同任官候選未憑近音硬合；生卒未知。',8135:'兩key尚方令解飛飛橋及木機器巧工，同虎時尚方職，飛橋不成非本人死，無生卒；不以宗教機關奇談反推生年。',8136:'建康實錄咸康二336稅米空懸案謝褒尚書免官，免不等死，無生卒。',8137:'兩key市長譚詳三倍貸饑谷、陰據駁，張駿納諫非殺譚詳，無生卒。',8138:'郭充與佟壽336奔高句麗，翟楷龐鑑被斬非郭充死，兩key身份同，無生卒。',8139:'三key都實讀，張駿時從事陰據與張天錫將367/371被俘還的敦煌陰據隔數十年且無族系/轉職橋，可能同人但不能據同名把晚活動套早從事，暫支隔離。無確生卒。',8140:'陽景同8132楊景兼郎中令遼東護送晉使336九月，吸收楊姓異文；無生卒。',8141:'兩key常伯劉瞻/劉睦，SL明一作睦，337與宋詼石琮同任官群，合一身份；無生卒。',8142:'CBDB明宋武帝裕父，親讀宋書武帝紀首靖生郡功曹翹皇考世系閉合；333index/type13不等本人明生年，裕363出生不是父出生或卒，歷史生卒空。',8143:'四key孔嚴彭祖倫子愉從子、殷浩別駕/左丞、著通葛論、吳興守同文學官鏈，JS太和五370疾去職卒明年；王坦之46/叔群卒不是本人年齡。無出生。',8144:'宋僚宋繇父、張玄靚龍驤武興守為張邕殺；單SL本人名與生子未獨立早源，不以泛稱滅宋氏361硬補父精年，無生卒確年。',8145:'兩key宋回皝揚烈將軍稱藩求趙兵337，明使職同；無生卒，與張邵/宋詼等不僅近音硬合。',8146:'宋悌配子繇祖，世仕張氏明族系，無本人精任官/生卒，不將子孫活動當本人年。',8147:'八key宋晃奭子→叛皝奔高句麗→俊赦改活中尉/中書監；SL宋隱祖活與CBDB所引WS33宋隱祖活中書監、曾祖奭同父系官位，全key原文閉合。宋隱字處默/SL處道異記不改祖身份；無生卒。',8148:'四key宋混弟澄字玄安、登帶石山、兄死繼輔、361九月張邕殺滅宋，同WS玄安可合吸收8149；預兆童言不是年齡，本人無出生。',8149:'WS宋玄安即宋澄字玄安，有SL明字+同帶石山傷足/張邕滅宋事件閉合→8148，361死亡來源TJ，非年號回推。',8150:'宗悠同張弘王擢龍黎兵沒，沒可為被俘，不逕取死亡；同役宋修候選待8948全key核，不以音近直接合。無生卒。'}.items():results[ident]['unresolved'].append(note)
    round21b_reads={8151:[('sl_02','宋晃平熙张泓为将军'),('sl_03','以护军平熙领将作大匠'),('tj_15','宋晃、平熙、张泓为将军'),('tj_18','护军将军平熙帅步骑八万')],8152:[('js_15','奋节将军康妙奉表')],8153:[('js_15','典军将军张宁')],8154:[('js_14','酒泉太守张镇潜引秦州刺史贾龛')],8155:[('js_17','彭和正等受业独拜床下者')],8156:[('sl_08','陈寓从事徐虓华驭'),('js_15','从事徐虓、华驭等至京师')],8157:[('sl_02','庶弟建武将军幼稚'),('sl_03','庶弟建武幼稚广威军'),('tj_15','慕容幼、慕容稚、佟寿')],8158:[('sl_02','宁逺将军汗司马辽东佟寿'),('sl_03','庶弟建武幼稚广威军寜逺汗'),('tj_15','汗性骁果，以千馀骑为前锋')],8159:[('sl_02','庶弟建武将军幼稚'),('sl_03','庶弟建武幼稚广威军'),('tj_15','慕容幼、慕容稚、佟寿')],8160:[('sl_02','广威将军军[弟名]'),('sl_03','封抚军将军军为襄阳王'),('tj_15','慕容军帅所部降于皝'),('tj_16','广威将军慕容军'),('tj_17','封抚军将军军为襄阳王')],8161:[('sl_02','慕舆根等昼夜力战'),('sl_03','慕舆根榼卢城大人也'),('tj_16','帐下将慕舆根谏曰'),('tj_17','折冲将军慕舆根曰'),('tj_18','使右卫将军傅颜就内省诛根'),('sl_05','慕舆根言于隽曰')],8162:[('sl_03','因讨鲜卑木提于白狼克之')],8163:[('js_15','伯祖龙骧将军、广晋太守、长宁侯卓'),('sl_10','伯祖龙骧将军广晋太守长宁侯卓')],8164:[('wsh_04','祖太，张祚先武卫将军')],8165:[('sl_01','虎将李穆率骑五千送魏烈帝'),('tj_15','赵将李穆纳拓跋翳槐'),('wsh_00','石虎遣将李穆率骑五千纳烈帝')],8166:[('sl_04','时仇池公杨世遣使称臣'),('tj_18','仇池公杨世卒'),('wsh_04','安死，子世自立为仇池公'),('yi_01','四年，俊卒，子世立'),('sl_05','仇池公杨世以地降坚')],8167:[('tj_17','仇池公杨国从父俊杀国自立'),('tj_18','仇池公杨俊卒'),('wsh_04','国子安叛苻生，杀俊'),('yi_01','四年，俊卒，子世立')],8168:[('sl_08','蜀人桥赞宻以告淳'),('tj_15','蜀人桥赞密以告淳'),('js_15','蜀人桥赞密以告淳')],8169:[('hy_00','建宁民毛衍、罗屯等反')],8170:[('sl_09','汜昭字嗣先炖煌人也')],8171:[('sl_01','鲜卑涉𪻺'),('tj_15','鲜卑涉𪻺众二万')],8172:[('sl_09','索充炖煌人充夣天上有二棺')],8173:[('hy_00','建宁民毛衍、罗屯等反')],8174:[('sl_08','陈寓从事徐虓华驭'),('js_15','从事徐虓、华驭等至京师')],8175:[('sl_02','使折冲将军兰勃戍之'),('tj_15','留折冲将军兰勃守之')]}
    for ident,qs in round21b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,phrase,'第21批B·燕部族/仇池世系同卷逐人審讀','Actual campaign/family paragraph read; genealogical alternatives preserved rather than silently normalized') for ch,phrase in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round21b')
    for ident,phrase,year,month in [(8161,'使右卫将军傅颜就内省诛根',360,2),(8166,'是岁，仇池公杨世卒',370,None),(8167,'仇池公杨俊卒，子世立',360,1)]:
        ev=chunk_ev('tj_18',phrase,f'資治通鑑{year}年'+(f'農曆{month}月' if month else ''),'本人死亡與當年上下文實讀；不套子孫/同族的死亡')
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(year,ev))
        if month:results[ident]['historical_death'].update(month=month,precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    results[8166]['genealogical_source_variants']=[dict(source='魏書仇池傳',father='楊安',succession='國→俊→安→世'),dict(source='南齊書仇池傳/通鑑',father='楊俊',succession='國→俊→世')]
    results[8167]['source_chronology_notes']=[dict(source='魏書',death_mode='楊安叛苻生殺俊，無明年'),dict(source='南齊書/通鑑',death_year=360,death_mode='卒，世繼')]
    results[8164]['candidate_duplicate_of']=7594
    results[8164]['evidence'].append(chunk_ev('js_15','亡祖武卫将军、天水太守、安世亭侯弇','晉書李玄盛上表','李暠曾祖柔/祖弇與WS祖太同世系同武衛官；名字源別有異，目標7594有裸CBDB未核，先候選不整合'))
    for ident in [8169,8173]:results[ident]['attested_activity_years']=[333]
    for ident,note in {8151:'四key平熙337同宋晃張泓任將、357護軍從垂虔討敕勒、監廆皝二廟，同官軍鏈；俘斬10萬非本人死，無生卒。',8152:'康妙前涼奮節將軍與韓博送盟文桓溫，天錫所約六年舉兵是約定不是康妙死亡/生年；無生卒。',8153:'張寧前涼典軍將軍同馬芮壇盟，無生卒，後376馬達等敗不是寧死。',8154:'單JS張鎮酒泉守張越兄、令狐亞舅，張軌病時圖代、張寔軍臨歸罪同人；斬功曹魯連非鎮死，正文在州八年屬軌，無本人生卒，不套遊戲默認年代。',8155:'彭和正祈嘉受業在朝卿士郡守之一、名不保證何官，祈嘉20歲與壽終非本人，無生卒。',8156:'兩key徐虓從事同陳宇/寓華馭使京、詔縣令，身份同使鏈，無本人生卒，張駿卒40非徐虓。',8157:'三key慕容皝庶弟幼建武將軍、333與稚軍汶城被仁獲，336東走中還；兵皆沒是俘不明死，不套仁336被賜死。無生卒。',8158:'三key汗皝庶弟寧遠、333討仁不入還、334牛尾谷千騎敗、337送趙質而還同人，軍大敗不等汗本人死，無生卒。',8159:'三key稚皝庶弟與幼同被仁獲、336出奔，TJ只說幼中還而SL幼稚俱中還，歸途細異保留不拆人；無生卒。',8160:'五key軍皝弟廣威、336降兄、339襲趙/344宇文/346扶餘到354襄陽王同長官軍鏈，無生卒；不是軍隊通詞誤造人物。',8161:'六key慕輿根榼盧城大、折衝廣威、諫守棘城/伐趙、魯口、蘇林、馮鴦到太師顧命反360二月誅，同生涯；與同謀慕輿幹不同人不因同姓合，無出生。',8162:'木提鮮卑首領被封奕討白狼，克之不明斬首本人，單晚本無生卒。',8163:'李卓李暠伯祖龍驤廣晉守長寧侯，兩key同李暠表文字閉合；表中亡祖是弇，不是說卓有卒年，無生卒。',8164:'WS李太李暠祖、柔子、祚先武衛，與三書7594李弇同暠祖柔子武衛對應；太/弇名字異記、目標CBDB507556全空未橋，先候選不硬重定向。無生卒。',8165:'三key後趙李穆送拓跋翳槐337五千騎及338復乙回，同官軍事無生卒；不是北周六世紀李穆僅同名合。',8166:'五key仇池公楊世、弟統子纂，360繼立至370卒。同職族地年代閉合，但WS稱安子、TJ/南齊俊子是源世系異說保留，不自動刪安或改父。未知出生，370年度有兩獨立紀年。',8167:'四key楊俊國從父/從叔356殺國、仇池公360死，WS國子安殺俊與南齊/TJ俊卒繼世源別保留；無明額外死亡年份，不憑模式差硬另出第二俊。無出生。',8168:'三key橋贊蜀人密告張淳雄欲覆殺東峽，同使命事件；張淳活歸非贊生卒，無日期。',8169:'華陽咸和八333毛衍建寧民與羅屯起事殺邵攀，死者守邵攀非衍，無生卒；333本人起事可約束遊戲估算。',8170:'汜昭嗣先敦煌州主簿夜金拒賄，無生卒；不把鄰汜禕後官、辛攀7歲套昭。',8171:'兩key涉璝原文涉𪻺鮮卑二萬由郭權統，役同333潼關攻石挺，將名源字保留；郭權後殺與石生死非涉璝明死，無生卒。',8172:'索充敦煌夢二棺被索紞占二官、功曹孝廉，夢婦生男而無生卒，SL附重華後不證本人346後在世；不拿老子72/胎80鄰文套其年齡。',8173:'羅屯同毛衍333起事殺邵攀，無本人被殺或生年，333實在活動只用約束估算不改歷史卒。',8174:'兩key華馭從事同徐虓陳宇/寓使京授縣令，無生卒；張駿40卒非本人。',8175:'兩key蘭勃皝折衝將軍好城逼乙連、截段遼糧車337同事，俘糧非本人死，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round22a_reads={8176:[('js_14','袁瑰，字山甫'),('tj_15','国子祭酒袁瑰')],8177:[('js_09','袁真叛逆'),('js_11','寻阳太守袁真'),('js_13','袁真以寿阳叛'),('js_18','袁真病死'),('wsh_02','温遂归罪袁真'),('js_16','兄坦之为袁真功曹'),('tj_18','二月，癸酉，袁真卒'),('yl_01','袁贞为监运')],8178:[('js_17','子歆，字幼安')],8179:[('jk_02','第三子翙字道翔')],8180:[('ssxy_03','虎子据小字')],8181:[('ssxy_01','谢裒儿'),('ssxy_03','尚书谢裒尚叔也')],8182:[('js_15','辛挹、郭普距之')],8183:[('js_15','辛挹、郭普距之')],8184:[('js_15','护羌参军陈宇')],8185:[('sl_08','护羌参军陈寓')],8186:[('sl_02','皇甫真阳协为冗骑常侍'),('tj_15','皇甫真、阳协为冗骑常侍')],8187:[('js_15','从事中郎韩博')],8188:[('sl_01','县令韩强在长城县'),('sl_01','长城徙人韩强获玄玉玺')],8189:[('js_15','中坚将军马芮')],8190:[('js_14','麹佩移檄废轨')],8191:[('sl_03','方士丁进有宠'),('tj_18','方士丁进有宠')],8192:[('js_17','子琚，位至大宗正')],8193:[('js_10','主簿何无忌及家人')],8194:[('js_10','庐江何锐为安丰太守')],8195:[('sl_01','上谷相侯龛'),('sl_03','中山太守上谷侯龛'),('tj_17','魏中山太守上谷侯龛')],8196:[('js_04','子咏，过江为交州刺史')],8197:[('js_18','常从督冉曾、公乘雄')],8198:[('js_18','常从督冉曾、公乘雄')],8199:[('hy_00','母冉贱，雄妻任养为子'),('tj_15','初，期母冉氏贱')],8200:[('ssxy_02','刘奭字文时')]}
    for ident,qs in round22a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第22批A·本人及親屬主語逐條審讀','Actual source passage read; relatives ages/deaths and religious transformation not silently assigned to subject') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round22a')
    results[8177].update(review_status='needs_further_research',identity_status='yuan_zhen_military_sources_and_yuan_zhen_yl_name_variant_unbridged',source_identities_all_adjudicated=False)
    yzev=chunk_ev('tj_18','二月，癸酉，袁真卒','資治通鑑太和五370二月癸酉','僅袁真軍事人物來源支；御覽袁貞未橋，日期不套整條')
    results[8177]['component_identities']=[dict(identity='壽陽叛將袁真',keys=[k for k in results[8177]['keys'] if not k.startswith('yl:')],historical_birth=None,historical_death=date(370,yzev,month=2,day_in_sexagenary_cycle='癸酉'),identity_adjudication='partially_read_cross_source_military_chain'),dict(identity='語林監運袁貞',keys=[k for k in results[8177]['keys'] if k.startswith('yl:')],historical_birth=None,historical_death=None)]
    results[8177]['evidence'].append(yzev)
    results[8178].update(source_identities_all_adjudicated=False,review_status='needs_further_research',identity_status='jinshu_chuxin_father_pou_bare_cbdb_lao_lineage_reference_unbridged')
    dev=chunk_ev('tj_18','恪大怒，奏收斩之','資治通鑑升平五361二月後三月前丁進案','方士求媚恪使殺評反被斬，俊/暐字源誤保留；不取已死俊仍在位')
    results[8191].update(review_status='primary_text_corroborated',historical_death=date(361,dev))
    results[8191]['evidence'].append(dev)
    results[8185].update(review_status='duplicate',duplicate_of=8184,identity_status='chen_yu_same_qiang_staff_mission_xiping_chancellor_yu_character_variant')
    results[8184]['evidence'].append(chunk_ev('sl_08','护羌参军陈寓','十六國春秋張駿傳陳寓','與JS陳宇同護羌參軍、徐虓華馭使京、詔西平相，唯一事件官鏈閉合'))
    results[8176]['attested_activity_years']=[337]
    results[8179]['religious_transformation_statement']={'reported_year':370,'expression':'太和五年茅山北隱化','qualification':'宗教隱化報告，不自動等同可靠生物死亡紀年；父許穆七十二歲不可套子許翙'}
    results[8180]['source_reported_age_at_death']={'age':33,'source':'世說新語劉注謝據字玄道條','calendar_year':None}
    for ident,note in {8176:'兩key袁瑰山甫涣曾孫、國子祭酒興學同人；JS懸車告老尋卒無確年，不按70歲反造生卒。337請學可約束活動。',8177:'袁真軍事來源主鏈壽陽叛/子瑾370卒已讀，但仍有未逐條讀完key；御覽語林袁貞監運與范玄平對語未有字號家系身份橋，不能只近音字異全合。370死亡只放軍事支，整行尚未閉合。',8178:'JS褚裒子歆幼安散騎常侍秘書監；裒349年47歲非歆。CBDB30917只指族祖Lao30914未提供此父官橋，index333type11不是生年，整行仍待橋。',8179:'許翙道翔許穆第三子，建康實錄注真誥太和五年隱化屬宗教記述，不逕當死亡；72歲解駕是父許穆。生卒不詳，遊戲年保估算。',8180:'謝據虎子玄道裒第二子朗父，劉注享年33但無可閉合卒年；不憑相鄰謝安或謝朗年推算。',8181:'兩世說key謝裒幼儒衡子、石父、尚叔同世系；葬後三日不是精卒日，注兄葬/本文叔異記保留，無生卒確年。',8182:'辛挹枹罕與宋修張弘郭普拒敵，斬敵非本人死亡，無生卒。',8183:'郭普同枹罕張璩部守戰，李嘉被戮不是郭普死，無生卒。',8184:'陳宇護羌參軍同徐虓華馭入京、授西平相，與SL陳寓唯一使職同事閉合，吸收8185；無生卒。',8185:'陳寓即JS陳宇同護羌參軍使京授西平相、同行徐虓華馭，不僅同近音；重定向8184，無生卒。',8186:'兩key陽協337與皇甫真冗騎常侍同官任命表，無生卒。',8187:'韓博從事中郎同康妙奉天錫盟文桓溫，有韓盧刁氏戲語；盟六年不是本人六歲或卒，無生卒。',8188:'單SL韓強333縣令獲璽與337長城徙人獻璽二說，身分獻璽尺寸及源敘異保留，不造生卒；晚重編記載不作確年活動輸入。',8189:'馬芮中堅將軍同張寧遙晉壇盟，無生卒；別後文馬達勿混。',8190:'麹佩移檄廢張軌同曹祛張越事，張軌在州八年不是佩年齡；無獨立確年不按當前錯默認334出生倒推；無生卒。',8191:'兩key丁進方士欲媚恪勸殺評反被斬，TJ361叙次獨立支持年；TJ俊與SL暐主名異留記，不據錯主名改成俊生前，無出生。',8192:'任琚旭子官大宗正終於家，咸和二327旭卒不是琚卒；無本人生卒年。',8193:'何無忌甘卓主簿與家人勸警322前後，甘卓四子被害不說主簿死；不直接合劉宋開國同名何無忌，無生卒。',8194:'何銳廬江安豐守推孫惠下人被孫惠攻殺，後病卒47屬孫惠，非銳；所讀句殺年不明，無生卒確年。',8195:'三key上谷侯龕段遼上谷相338降趙、冉魏中山守351降燕拜中尉352攻鄴，同地域轉屬官鏈可連，斬白同非龕死，無生卒。',8196:'傅詠暢子過江交州刺史太子右率；咸和五330卒是父暢，年49是傅宣，不套傅詠，無生卒。',8197:'公乘雄元帝腹心常從督與冉曾為敦所害明文，但叙次敦第二叛病無本人精年句，暫不硬填322或324；無出生。',8198:'冉曾元帝腹心常從督與公乘雄被王敦害，未讀明本人確年句，不由叙次逕造卒年；無出生。',8199:'兩key冉氏李雄妾李期生母、任氏養期明文；李雄班期被害皆非冉氏死亡，無生卒。',8200:'劉奭文時彭城、祖昶父濟、車騎諮議長沙相散騎常侍明人；非漢元帝劉奭，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round22b_reads={8201:[('js_09','辟刘弘曾孙安为掾属'),('js_09','安、恭惧，自赴水而死')],8202:[('yl_06','刘寅于厌次县筑垒')],8203:[('ssxy_03','刘爰之字遵祖')],8204:[('ssxy_01','刘简作桓宣武别驾')],8205:[('js_10','子讷嗣，谨饬有石庆之风')],8206:[('yl_01','刘起迁中书舍人')],8207:[('js_14','曾祖距，广陵相')],8208:[('sz_02','晋外兵郎𣘐阳令潜')],8209:[('js_10','子滔嗣')],8210:[('js_10','瞻弟眈，尚书郎')],8211:[('js_10','壶第三子瞻')],8212:[('js_10','子诞嗣')],8213:[('js_05','豫章太守史畴')],8214:[('js_09','武陵威王晞，字道叔'),('js_09','太元六年，晞卒于新安')],8215:[('js_17','时司马飏为乌程令')],8216:[('js_16','吉挹，字祖冲'),('js_16','挹闭口不言，不食而死')],8217:[('js_16','兄坦之为袁真功曹'),('ssxy_00','吴氏谱曰坦之字处靖')],8218:[('js_09','部将吴寄曰'),('js_10','陶侃遣将吴寄以兵救之')],8219:[('js_07','亦为周凯所侮')],8220:[('js_07','勰弟彝，少知名')],8221:[('js_10','恬、颐并历卿守')],8222:[('js_07','札兄靖子懋晋陵太守')],8223:[('js_07','札长子澹，太宰府掾')],8224:[('js_10','以弟颐长子琳为嗣')],8225:[('yl_01','主自告吴兴太守周礼以闻')]}
    for ident,qs in round22b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第22批B·元帝時臣屬及周卞世系逐人審讀','Actual prose read; source silence does not imply exhaustive absence from all historical works') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round22b')
    results[8214].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='jinshu_biography_read_remaining_eleven_keys_not_yet_fully_read')
    sxev=chunk_ev('js_09','太元六年，晞卒于新安，时年六十六','晉書武陵威王晞傳','381卒66歲，只此來源支實讀，不冒稱十二keys已核')
    results[8214]['component_identities']=[dict(identity='晉書武陵威王晞道叔來源支',keys=[k for k in results[8214]['keys'] if k.startswith('js:js_09:')],historical_death=date(381,sxev),historical_birth=date(316,sxev,age=66))]
    results[8214]['evidence'].append(sxev)
    results[8215].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='jinshu_simayang_read_bare_cbdb392900_no_bridge')
    results[8201]['death_upper_bound']=339
    results[8201]['unresolved'].append('JS庾亮上表追述陶稱往罪，劉弘曾孫安與楊恭先自投水，表時339只是死亡上界不是確卒年；同名漢淮南王不可合。')
    for ident,note in {8202:'御覽引晉中興書劉寅厭次拒勒被烹明死亡但無年；可能劉演字異須按實際他record職役核，現在不單名去重，不由石勒年代補卒。',8203:'劉爰之遵祖沛郡、殷浩賞識推薦庾亮獨榻失望羊公鶴，中書郎宣城守；無生卒，羊祜與鶴不是本人死。',8204:'劉簡仲約南陽喬孫挺子、桓溫別駕東曹參軍同譜，無生卒。',8205:'劉訥超子享父、謹飭石慶風中書侍郎下邳內史，劉超328遇害不是訥卒；無生卒。',8206:'御覽劉起中書舍人忠慎清苦，台省初建敘事與劉超傳近文但名字不一未獨立橋，不僅段落相似硬合，無生卒。',8207:'劉距毅曾祖廣陵相世系唯一記述，毅405後戰功不是距活動；無生卒。',8208:'卓潛南陽外兵郎𣘐陽令、惠精昙戒兄，19出家70卒屬弟，鄰竺永83/380卒亦非潛，生卒空。',8209:'卞滔敦子嗣爵，敦憂卒非滔卒，不把前粹/壸父子死套滔；無生卒。',8210:'卞眈壸第三子瞻弟尚書郎，壸及前二子戰死並不含後二子，無生卒。',8211:'卞瞻壸第三子廣州刺史，同傳兩兄戰死不等本人，無生卒。',8212:'卞誕壸長孫承父祖爵，字形私用字父瞫勿自動當該名異人；無生卒。',8213:'史疇豫章守笨伯同四伯名，光逸63/70及任放兄聃死都不是史疇；無生卒。',8214:'JS本傳晞道叔381年66歲可反推316虛歲，但本行12keys尚餘未逐鍵讀完，日期隔離已讀支不進整人嚴格補丁；已讀同名正文不冒充全key閉合。',8215:'JS司馬颺烏程令免→泰山太守鎮武徐龕阻→增兵助祖逖稱疾得赦，一身份無生卒；CBDB392900原庫全空無親官橋，不能按同名宣告全核。',8216:'吉挹祖沖馮翊、祖朗自殺非本人；魏興拒韋鐘失城不食死明文，所讀JS本傳無獨立精年，不以襄陽陷年常識直接補死379，無出生。',8217:'兩key吳坦之處靖道助、隱之兄與韓伯母鄰同家系閉合；JS袁真敗弟乞代命釋兄 vs SS大吳不免哀制屬本人經歷/死亡敘法不同，無明年代對撞，保留兩說不造卒。',8218:'兩key吳寄陶侃將援周顗、夜魚忍饑破杜弢同戰役官鏈，無生卒，敗亡敵軍非寄死。',8219:'周凱侮東萊王恢使恢怨，王恢被周玘殺、玘56卒非凱本人，不反算，無生卒。',8220:'周彝勰弟玘子元帝丞相掾早亡，早亡無精年無精齡；不把兄父五十六歲套彝。',8221:'周恬顗次子同颐歷卿守，父顗被害不說三子亦死，無生卒。',8222:'周懋靖子晉陵守清流亭侯，周氏五侯遇王敦錢鳳清洗，覆族不自動定本人生死精年，無生卒。',8223:'周澹札長子太宰府掾，札被攻殺非明文澹死，無生卒。',8224:'周琳颐長子嗣伯闵、東陽守文父，不以伯闵卒套琳；無生卒。',8225:'御覽周禮吳興守聽臨海公主告、上聞殺錢溫，錢溫死非周禮，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round23a_reads={8226:[('js_07','次子稚，察孝廉')],8227:[('js_07','赞弟缙太子文学')],8228:[('js_09','外甥周翼并小'),('ssxy_00','翼字子卿陈郡人')],8229:[('js_07','楚遣其子诗平之')],8230:[('js_07','筵弟赞大将军从事中郎')],8231:[('ssxy_00','周镇罢临川郡还都')],8232:[('js_10','恬、颐并历卿守')],8233:[('ssxy_02','于时寻阳周馥')],8234:[('sl_03','以单男为雁门太守'),('tj_18','又攻雁门，杀太守单男')],8235:[('js_09','都尉夏施盗官柳'),('ssxy_00','都尉夏施盗')],8236:[('js_12','殿中侍御史奚郎')],8237:[('js_18','至刺史姜壮时')],8238:[('yi_02','时交州刺史姜庄')],8239:[('js_16','宗人夷吾，有美名')],8240:[('js_13','祖父奕，全椒令')],8241:[('ssxy_00','孔氏谱曰沈字德度'),('ssxy_02','防稽孔沈')],8242:[('js_17','兄嘉，桓温征西长史'),('js_18','孟嘉字万年'),('ssxy_01','武昌孟嘉作庾太尉州从事'),('ssxy_02','孟万年及弟少孤')],8243:[('js_12','谒者仆射孟洪就加册命')],8244:[('yl_00','侯孟纯等矫诏入城门')],8245:[('yl_03','北人诸将孙凯等谋')],8246:[('js_07','子嗣，有绰风')],8247:[('js_14','放字齐庄，幼称令慧'),('ssxy_00','孙放别传曰放字齐庄'),('ssxy_03','孙放别传曰放兄弟并秀异')],8248:[('sl_02','朝鲜令昌黎孙泳'),('sl_03','南部都尉孙泳急入安乐'),('tj_16','朝鲜令昌黎孙泳'),('tj_17','都尉孙泳急入安乐')],8249:[('js_14','潜字齐由，为豫章太守'),('ssxy_00','孙潜字齐由太原人')],8250:[('sl_02','武宁令广平孙兴'),('sl_03','北平太守孙兴亦表言'),('tj_16','武宁令广平孙兴'),('tj_17','北平太守孙兴亦表言'),('tj_18','燕豫州刺史孙兴请攻洛阳')]}
    for ident,qs in round23a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第23批A·周氏後裔及燕官臣逐人核讀','Actual subject family/office paragraph read, no transfer of relative death/age') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round23a')
    snev=chunk_ev('tj_18','又攻雁门，杀太守单男','資治通鑑升平五361九月後十月前張平案','單男為雁門守被張平殺，獨立確年，不把張平滅亡套本人精月')
    results[8234].update(review_status='primary_text_corroborated',historical_death=date(361,snev))
    results[8234]['evidence'].append(snev)
    results[8238].update(review_status='duplicate',duplicate_of=8237,identity_status='jiang_zhuang_jiaozhou_governor_hanji_xiezhuo_rinan_mission_same_person_variant')
    results[8237]['evidence'].append(chunk_ev('yi_02','时交州刺史姜庄','南齊書林邑傳','姜莊同JS姜壯交州刺史派韓戢謝稚/謝擢任日南、貪殘致林邑攻，獨特官事字異閉合'))
    results[8241].update(review_status='duplicate',duplicate_of=8035,identity_status='kong_shen_same_dedu_qun_son_yi_grandson_langye_literature')
    results[8241]['evidence'].append(chunk_ev('js_13','沉字德度，有美名','晉書孔群傳','8035孔沉德度群子；SS孔沈德度群子奕孫琅邪王文學，同家系明文，沉沈異字可合'))
    results[8228]['source_reported_age_at_death']={'age':64,'calendar_year':None,'source':'世說新語劉注周氏譜'}
    results[8242]['source_reported_age_at_death']={'age':53,'calendar_year':None,'source':'晉書桓溫傳孟嘉附傳/世說劉注嘉別傳'}
    results[8248]['attested_activity_years']=[338,350]
    results[8250]['attested_activity_years']=[338,349,351,362]
    for ident,note in {8226:'周稚札次子察孝廉不行，兄澹及父札死亡不轉套本人；無生卒。',8227:'周縉靖子赞弟太子文學都鄉侯、五侯族清洗背景，但未具名本人卒年，無生卒。',8228:'兩key周翼子卿陳郡郗鑒甥、幼受吐飯、剡令心喪同事；譜64歲而卒無對應CE年，不倒推出生。',8229:'周詩楚子奉父平蜀李高，楚同年卒不等詩亦卒；無生卒。',8230:'周贊靖子筵弟大將軍從事中郎武康侯，族難泛記不推出本人精年；無生卒。',8231:'周鎮康時陳留尉氏、祖和父震臨川吳興守，雨漏船王導表薦，無生卒。',8232:'周頤顗子琳生父、闵弟歷卿守，伯父父祖卒不套本人；無生卒。',8233:'尋陽周馥評桓溫平蜀憶王敦，劉注馥周抚孫且曾敦掾的世系/官事原記照存，不按世代私自改成抚；無生卒，不合311被討周馥。',8234:'兩key單男雁門守張平361殺明文，未知生；SL360任不是出生或卒。',8235:'兩key夏施陶侃都尉盜武昌西門柳自植同獨特事件，惶怖謝罪不是被誅，無生卒。',8236:'奚朗/奚郎同殿中侍御史湘東候任王彪之拒、卜術進身，源正文兩字互見明異體不拆；無生卒。',8237:'姜壯交州刺史遣韓戢日南侵刻，與南齊姜莊同官同人同前後使職事件，吸收8238；死亡夏侯覽/范文非姜壯，無生卒。',8238:'姜莊同JS姜壯交州刺史韓戢謝擢/稚日南前史唯一事件→8237，無生卒。',8239:'孔夷吾孔衍宗人元帝主簿侍中太子左衛率卒贈太僕，32053是衍不是夷吾；無生卒。',8240:'孔奕孔嚴祖全椒令識酒水惠政而卒，嚴370卒不能倒用祖，無生卒。',8241:'孔沈德度會稽山陰、祖奕父群琅邪王文學與8035JS孔沉同世系字官確合；孫綽譽孔家金非另一孔，重定向8035。',8242:'四key孟嘉萬年宗曾孫/陋少孤兄桓溫長史、龍山落帽同生涯，53卒無確年；少孤詐報兄病及萬年可死是評語非當時真死，不以假報定卒。',8243:'孟洪謁者僕射奉蔡謨復官冊命，謨永和12卒76非孟洪年齡死亡；無生卒。',8244:'孟純郭默部矯詔殺劉胤、御覽斷句侯孟純源字保留；被斬胤/張滿荀楷李慕非孟純死，不由郭默伏法套全黨。無生卒。',8245:'孫凱許昌北人諸將欲逼王浹留、牽馬被手刃斬截泛記未明本人死亡，無生卒。',8246:'孫嗣孫綽子文章相亞中軍參軍早亡，58是父綽非嗣，無確生卒。',8247:'三key孫放齊莊盛次子、庾亮七八歲問齊莊周、庾園客戲諱同人，終長沙相無精卒；不按7/8虛齡猜生年。',8248:'四key孫泳昌黎朝鮮令338拒趙與350徒河/徙河南都尉救安樂庫相連，孫希是子不把子活動當父；無生卒，兩明年活動只約束估算。',8249:'兩key孫潛齊由盛長子豫章守、拒殷仲堪參軍憂卒同人，源未獨立讀CE年不直接填397；無出生。',8250:'五key孫興廣平武寧令338拒趙、349北平守請伐、351中山守善撫到362豫州刺史攻洛陽，同燕官鏈；無生卒，末活動362不當死亡。'}.items():results[ident]['unresolved'].append(note)
    round23b_reads={8251:[('js_06','子腾嗣，以博学著称'),('ssxy_02','僧奴孙腾小字也')],8252:[('js_09','遣将军宋夏、陈修')],8253:[('js_10','丞相行参军宋挺')],8254:[('js_03','荆州别驾宗澹')],8255:[('hy_01','子长生，字彭祖')],8256:[('sl_01','常覇东夷校尉'),('sl_02','武原令常霸'),('tj_16','武原令常霸')],8257:[('js_15','蔑生愿，安成太守')],8258:[('js_10','尚书郎弘讷议')],8259:[('yl_04','太守张崇之义其志行')],8260:[('hy_00','侍中李艳及张烈'),('sl_09','侍中李艳及张烈'),('sl_08','祖烈魏外黄令')],8261:[('sl_01','代相张牧')],8262:[('ssxy_00','张之字祖希'),('ssxy_03','张吴兴年八岁亏齿')],8263:[('js_13','茂弟盎，为周札将军')],8264:[('js_17','遣使者张兴备礼'),('sl_09','遣使者张兴辟为太子少傅'),('sl_03','护遣其将张兴率劲卒'),('tj_18','吕护遣其将张兴出战')],8265:[('sl_02','新昌人张衡执县宰')],8266:[('js_09','遣谘议参军张诞讨五溪夷')],8267:[('js_01','祎子舆，字公安')],8268:[('js_15','玄靓右司马张邕'),('sl_09','右司马张邕恶澄专擅'),('tj_18','邕兵悉散走，邕自刎死'),('sl_11','繇生而僚为张邕所杀'),('wsh_04','邕，一名野')],8269:[('js_09','默将张丑、宋侯')],8270:[('js_09','侃命张夔子隐为参军')],8271:[('sl_02','有女令首聪敏好学'),('sz_00','安令首，本姓徐')],8272:[('js_19','淮陵故吏徐深、艾毅')],8273:[('js_19','遣长史徐玮宣檄诸屯')],8274:[('js_10','子玄嗣，位至散骑侍郎')],8275:[('js_10','子谧嗣，历义兴太守')]}
    for ident,qs in round23b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第23批B·本人段落及跨代同名審讀','Actual cited prose read; incompatible centuries/office chains isolated as source branches') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round23b')
    results[8259].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='zhang_chongzhi_name_boundary_and_wuxing_governor_identity_need_collation')
    results[8260].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_cao_wei_zhanggui_grandfather_and_chenghan_lishou_supporter',component_identities=[dict(identity='曹魏外黃令、張軌祖烈',keys=[k for k in results[8260]['keys'] if k.startswith('sl:sl_08:')],historical_birth=None,historical_death=None),dict(identity='成漢勸李壽稱帝張烈',keys=[k for k in results[8260]['keys'] if not k.startswith('sl:sl_08:')],historical_birth=None,historical_death=None)])
    xev=chunk_ev('tj_18','吕护遣其将张兴出战，傅颜击斩之','資治通鑑升平五361秋七月後九月前','僅呂護將張興支；前涼張祚徵王纖使者張興另支不得套')
    results[8264].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='unbridged_qianliang_wangxian_envoy_and_luhu_general_same_name',component_identities=[dict(identity='前涼張祚徵王纖使者張興',keys=[k for k in results[8264]['keys'] if k.startswith('js:') or k.startswith('sl:sl_09:')],historical_birth=None,historical_death=None),dict(identity='呂護將張興野王被傅顏斬',keys=[k for k in results[8264]['keys'] if k.startswith('tj:') or k.startswith('sl:sl_03:')],historical_birth=None,historical_death=date(361,xev))])
    results[8264]['evidence'].append(xev)
    yongev=chunk_ev('tj_18','邕兵悉散走，邕自刎死','資治通鑑升平五361十一月','張天錫討邕本人自刎，與WS殺邕敘法異而年份獨立明，不套宋氏死亡月份')
    results[8268].update(review_status='primary_text_corroborated',historical_death=date(361,yongev,month=11))
    results[8268]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8268]['evidence'].append(yongev)
    for ident,note in {8251:'孫騰伯海僧奴統子廷尉博學、與衛君長評同人；SS父字私用字𬘘不私自改作綽，JS明統子。父余姚令卒、弟登早終非騰卒，無生卒。',8252:'宋夏陶侃將據湓口討郭默，郭默及黨被殺不等夏死；無生卒。',8253:'宋挺丞相行參軍劉陶門人娶故主妾、盜布建興遇赦後阮抗請長史劉隗劾，病死無本段精年，建興罪不是死年；無出生。',8254:'宗澹荊州別駕譖王舒，王舒病卒非宗澹，無生卒。',8255:'常長生彭祖常璩家系前述父常寬附傳之子，州主簿資中令治中早亡未精年，不把父卒交州梁碩難套子；無生卒。',8256:'三key常霸/常覇武原令/內史338應趙後奔鄴同地同役，誅叛城眾不是明本人死，無生卒。',8257:'庾願蔑子安成守，衮祖四子命名及蔑南渡不是願出生精年；無生卒。',8258:'弘訥尚書郎論卞壸贈典，48歲戰死屬壸、二子俱死非訥；無生卒。',8259:'原御覽引晉中興書吳達事「太守張崇之義其志行」已讀，張崇之名界需對勘版本；與8106吳興守張崇禮吳逵或為同故事異名，但8106本身多身份混合，暫不硬合，生卒不詳。',8260:'三key實讀卻混跨代兩人：SL08張軌祖曹魏外黃令烈 vsHY/SL09成漢338勸壽帝張烈；不能讓祖烈活到338，不整行日期或去重，兩支保留。',8261:'張牧段遼代相338同馬鮑侯龕降趙，兵降不等死，僅SL晚重編無生卒。',8262:'兩key張玄之祖希澄孫、顧和外孫九歲觀佛與張吳興八歲虧齒應答，SS明玄之已見閉合；兒童歲數不同事件不出生，卒於郡無精年。',8263:'張盎茂弟周札將、沈充討札死，本句未明紀年，茂及三子同役非盎子，無確生卒。',8264:'前涼張祚使者張興徵王纖與呂護將張興野王361被斬無仕遷橋，跨地同名不硬合；361僅燕支死亡，王纖82死非使者。',8265:'張衡新昌人334執縣宰降皝，縣宰被斬不是張衡死，亦非東漢科學家僅同名；單SL不作確年活動輸入，無生卒。',8266:'張誕陶侃諮議參軍討五溪降之，賊降非誕死，無生卒。',8267:'張輿公安祎子華孫襲爵南渡丞相掾太子舍人，祖父父叔同300被害非輿死；無生卒。',8268:'五key張邕一名野玄靓右司馬殺宋澄族/宋僚、姦馬氏→天錫討而361十一月自刎同人，WS稱殺屬簡略；SL澄殺360與TJ361源別不默修，邕本人死年361獨立支持。無出生。',8269:'張醜郭默將阻降後黨死40泛稱，陶侃傳又明張醜被縛送，不由40人通稱硬判其精卒年；無生卒。',8270:'張隱夔子陶侃參軍酬昔恩，父夔事及旁邊劉安投水不套隱；無生卒。',8271:'兩key安令首本姓徐東莞父忡、佛圖澄門出家建賢寺同傳，安為法姓不另造新族；鄰淨檢70升空非本人死，無生卒。',8272:'徐深淮陵故吏同艾毅再請蘇峻內史，蘇峻18歲孝廉及反死不套徐深；無生卒。',8273:'徐瑋蘇峻長史宣檄諸屯，蘇峻歲數死亡不套徐瑋；無生卒。',8274:'應玄詹子嗣爵散騎侍郎誕兄，33153卒是父詹不是玄；無生卒。',8275:'戴謐邈子嗣義興守大司農，邈卒/若思被害皆非謐卒，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round24a_reads={8277:[('sl_01','上庸公日归率众戍长安'),('tj_16','上庸公日归帅众戍长安')],8278:[('sl_01','渡辽将军曹伏'),('tj_16','渡辽将军曹伏')],8279:[('js_16','曹毗，字辅佐'),('ssxy_01','曹毗字辅佐')],8280:[('ssxy_02','茂之字永世彭城人')],8281:[('js_19','朱轨息昌、赵诱息胤')],8282:[('ssxy_02','奋命朱辟为副')],8285:[('js_10','属李匡幸荷殊宠')]}
    for ident,qs in round24a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第24批A·本人與死亡主語分辨','Actual original passage read, other subjects deaths and merely figurative expressions excluded') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round24a')
    results[8276].update(review_status='secondary_source_checked',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='bare_cbdb181908_tackett_estimate_no_original_identity_bridge',last_manual_review_batch='round24a',source_reported_estimates=[dict(year=399,precision='approximately_estimated_by_Tackett',source='CBDB cc399 c_dy_range2')])
    for ident,note in {8276:'已讀原CBDB181908敬歸，399明Tackett估算/range2、index336type29不是生年，只有太守無地無親屬原文身份橋；歷史生卒不詳，保遊戲估算。',8277:'日歸趙上庸公與涉歸戍長安告石廣，死者石廣不是二歸；TJ獨立讀到同役同人，不造出生卒。',8278:'兩key曹伏趙渡遼將軍青州海戍輸糧伐燕338同使命，斬四萬屬石宣敵人非曹伏；無生卒。',8279:'兩key曹毗輔佐譙國博士光祿勳文人同名號文章，祖休高祖/曾祖記異照存；54卒是庾闡非毗，毗傳光祿勳卒無確年，無生卒。',8280:'曹茂之永世彭城、韶孫曼子尚書郎，蜍為小字，厭厭九泉下是活人譏喻不是死亡，無生卒。',8281:'朱昌朱軌子與趙胤求杜曾復父讎，脔食的是杜曾肉，非朱昌死；無生卒。',8282:'朱闢桓石虔救桓沖副手，17/18屬石虔不是副手，無生卒。',8285:'李匡王導屬、淳于伯枉案劉隗彈請免，淳于伯被誅非匡死，導引咎免又未問不當本人卒，無生卒。'}.items():results[ident]['unresolved'].append(note)
    guowen_ev=chunk_ev('js_17','余杭令顾飏与葛洪共造之','晉書郭文傳','全傳文舉河內軹人入余杭山，明葛洪同訪；與5823郭文舉單葛洪傳閉合，跨片由對方寫重定向')
    results[7145]['evidence'].append(guowen_ev)
    more24={8283:[('js_15','有陇西人李俨'),('sl_04','送俨归长安坚以俨'),('sl_05','讨平李俨以功'),('sl_09','陇西李俨据郡'),('tj_17','陇西人李俨据郡'),('tj_18','至长安，坚以俨为光禄勋')],8284:[('hy_00','寿卒，势立，改元太和'),('js_18','时李势微弱'),('sg_11','永和三年讨李势'),('sl_09','李势字子仁寿之长子'),('tj_16','李势无道，臣民不附'),('tj_18','是岁，归义侯李势卒'),('wsh_00','司马聃擒李势'),('wsh_02','势，字子仁'),('js_14','李势君臣不过自力一战'),('js_17','至李势时卒'),('wsh_05','建国中，李势在蜀'),('ssxy_01','华阳国志曰李势字子仁')]}
    for ident,qs in more24.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第24批A·前涼李儼與成漢李勢全key閉合','Actual same office/family/campaign source prose read, conquest and dependent deaths not mistaken for own death') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round24a')
    results[8283]['last_attested_activity']=367
    results[8283]['attested_activity_years']=[355,366,367]
    results[8283]['unresolved'].append('六key陇西李儼355自立奉晉年號、依秦通涼、366敛岐來屬到367王猛入城執還秦拜光祿勳歸安侯同生涯；斬的是賀肫非李儼，敗被執不等死。JS純為子、TJ兄子明親屬源別保留；無生卒，367本人獲官在世可修估算。')
    lsev=chunk_ev('tj_18','是岁，归义侯李势卒','資治通鑑升平五361歲末','亡蜀降晉歸義侯李勢本人年度死亡，非347國亡或父壽343卒')
    results[8284].update(review_status='primary_text_corroborated',historical_death=date(361,lsev),appearance_upper_bound=343,attested_activity_years=[343,347])
    results[8284]['evidence'].append(lsev)
    results[8284]['unresolved'].append('十二key成漢李勢子仁壽子廣兄、347桓溫伐降→歸義侯361卒全閉合。SG注孫盛親見劉玄為討勢347同役，WS獠敘亦同，非陳壽本人可知未來；HY343壽卒勢立故登場估算不得晚於343，但已早仕不聲稱343真正初仕。無出生，SL壽死壽即位明抄誤按其上下勢立身份校而保源。Gong壯至勢時卒不套勢死亡，父祖國祚年不作歲數。')
    round24b_reads={8286:[('sl_01','幽州刺史李孟退奔易京'),('tj_16','幽州刺史李孟退保易京')],8287:[('ssxy_02','志字温祖江夏钟武人')],8288:[('js_19','逸为李汤所执')],8289:[('ssxy_03','李轨字范江夏人')],8290:[('tj_15','丹杨丞杜乂'),('tj_16','时杜乂、殷浩'),('js_11','京兆杜乂、陈郡殷浩'),('js_17','杜乂，字弘理'),('sg_06','嘏子乂字洪治'),('ssxy_02','杜乂字治京兆人')],8291:[('js_09','交阯太守杜宝')],8292:[('js_03','国子祭酒杜彝')],8293:[('js_16','遗命子晏曰')],8294:[('js_16','援子潜，右卫将军')],8295:[('ssxy_00','式桓歆小字也')],8296:[('yl_08','桓石虎是桓征西儿')],8297:[('js_03','佐著作郎桓稚')],8298:[('js_10','庐江太守梁龛')],8299:[('js_09','安、恭惧，自赴水而死'),('sg_12','少与犍为杨恭友善')],8300:[('js_10','杨方，字公回'),('js_10','固辞还乡里，终于家')]}
    for ident,qs in round24b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第24批B·本人名號及死亡主語逐條審讀','Actual text reviewed including source annotation corrections; no unqualified transfer of dates') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round24b')
    results[8289].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='annotation_explicitly_denies_ligui_as_wei_zhan_nephew_speaker')
    results[8289]['component_identities']=[dict(identity='劉注中興書李軌弘範江夏尚書郎、劉氏甥',historical_birth=None,historical_death=None),dict(identity='本文王不留行評衛展之舅說者，劉注謂應弘度非弘範',historical_birth=None,historical_death=None)]
    results[8299].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_shuhan_zhangyi_early_deceased_friend_and_jin_taocheng_victim',component_identities=[dict(identity='蜀漢張裔犍為友楊恭早死、裔養其孤',keys=[k for k in results[8299]['keys'] if k.startswith('sg:')],historical_birth=None,historical_death=None),dict(identity='東晉陶稱部將楊恭懼威自赴水，339表追述已死',keys=[k for k in results[8299]['keys'] if k.startswith('js:')],historical_birth=None,historical_death=None,death_upper_bound=339)])
    results[8286]['attested_activity_years']=[338]
    for ident,note in {8286:'兩key李孟後趙幽州刺史338退保易京同事，退非死，無生卒。',8287:'李志溫祖江夏鍾武、祖重父慕員外常侍南康相，與曹蜍九泉下是活人譏評不是卒日，無生卒。',8288:'李湯擒蘇逸送斬車騎府，斬死者蘇逸非李湯；無生卒。',8289:'候選把李軌弘範當衛展甥說王不留行，但原劉注明「軌劉氏之甥此應弘度非弘範」已否定對應；注本身提供李軌尚書郎真身份，但speaker與此注人物不能整併，保支待拆無日期。',8290:'六key杜乂杜預孫錫子、京兆丹陽丞成恭杜后父同族官鏈；字弘理/弘治洪治源別保留。JS早卒女后生後而終、咸康追贈並非本人當時仍活；334/340才名回述不當活動年，無出生卒確年。',8291:'杜寶交阯守反對溫放之征林邑被誅明文，放之卒官非杜寶年；所讀本句無CE精年，不硬補生卒。',8292:'杜彝國子祭酒同刁協郊祀須還洛案，華恒病卒及名郊議不是本人死，無生卒。',8293:'杜晏杜夷子蒼梧守，32366歲是父夷、臨終訓子不說晏亦卒；無生卒。',8294:'杜潛杜援子右衛將軍，太寧元66父輩杜夷非潛，無生卒。',8295:'桓歆叔道桓溫第三子小字式、尚書，幼言受杖輕文同譜閉合；父溫卒不套子，無生卒。',8296:'御覽俗說桓石虎征西兒小名惡子拔虎箭奇聞，17/18石虔及鎮惡不能僅近義把此自動合石虔；僅敘事身份未知可靠生卒。',8297:'桓稚王澄故吏佐著作郎敦平後表冤，44歲被殺屬王澄非稚；無生卒。',8298:'梁龕廬江太守婦喪未除宴同周顗被免削爵，明日當除服非死亡日期，無生卒。',8299:'兩key明混人：SG蜀張裔少友楊恭早亡養孤成人，與JS東晉陶稱將楊恭339表追述自投水跨百餘年；不得把兩死者同名合，歷史日期分支皆未知，東晉死上界339不當精卒年。',8300:'楊方公回會稽郡吏讀經諸葛恢/虞預/賀循推、王導掾高梁太守著五經鉤沈、年老歸卒同生涯；以年老非精齡，後薛兼323卒非方卒，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round25a_reads={8301:[('tj_15','以叔父坚头之子盘'),('yi_01','以坚头子盘'),('wsh_04','以坚头子盘')],8302:[('js_16','新淦人杨羡')],8303:[('js_09','使都尉杨举为先驱')],8304:[('js_07','杀征虏将军杨谨'),('tj_16','十二月，振威护军萧敬文杀征虏将军杨谦')],8305:[('js_10','功曹荣建固谏')],8306:[('js_03','太守乐凯言于敦')],8307:[('sl_01','遣子乞特真'),('tj_16','遣其子乞特真奉表')],8308:[('sl_03','建威将军段刚'),('tj_17','上党人冯鸯逐燕太守段刚'),('tj_18','张平袭燕平阳，杀段刚')],8309:[('js_12','慕容俊攻段兰于青州'),('sl_02','辽遣弟兰来拒'),('tj_15','复遣其弟兰与慕客翰'),('tj_16','宇文逸豆归执段辽弟兰'),('tj_17','初，段兰卒于令支'),('js_08','其子兰复聚兵'),('sl_03','段兰之子龛')],8310:[('ssxy_02','殷允出西')],8311:[('js_14','父师，骠骑谘议参军')],8312:[('js_14','父康，吴兴太守')],8313:[('js_18','泾令江播豫焉')],8314:[('js_06','子敳，历琅邪内史')],8315:[('js_05','大鸿胪陈留江泉')],8316:[('js_14','父济，安东参军')],8317:[('js_14','灌字道群。父瞢'),('ssxy_02','江灌字道群陈留人')],8318:[('js_14','灌字道群。父瞢')],8319:[('js_14','子蔚，吴兴太守')],8320:[('sl_01','虎使襄城公涉归'),('js_18','其父涉归分部落')],8321:[('js_09','弟式之，新建县侯')],8322:[('js_07','含弟子遁，交州刺史')],8323:[('js_11','缙子鸣鹄')],8324:[('sl_01','典农中郎将王典'),('tj_16','典农中郎将王典')],8325:[('hy_00','李奕、王利等劝寿')]}
    for ident,qs in round25a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第25批A·仇池段氏及江殷世系逐人核讀','Actual same subject passage reviewed, source naming errors retained and mixed subjects isolated') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round25a')
    results[8305].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='gan_zhuo_gongcao_rongjian_read_bare_cbdb410326_unbridged')
    jy=chunk_ev('tj_16','十二月，振威护军萧敬文杀征虏将军杨谦','資治通鑑永和三347十二月','JS周撫傳楊謹同征虜將軍萧敬文殺涪城事件字異，7597三keys此前已閉合，非僅同音姓名')
    results[8304].update(review_status='duplicate',duplicate_of=7597,identity_status='yang_jin_qian_variant_same_xiaojingwen_victim_zhenglu_general',historical_death=date(347,jy,month=12))
    results[8304]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8304]['evidence'].append(jy)
    results[7597]['evidence'].append(chunk_ev('js_07','杀征虏将军杨谨','晉書周撫傳','吸收8304字異謹，與TJ347十二月謙同官同被殺事件，不更動其他來源'))
    gd=chunk_ev('tj_18','张平袭燕平阳，杀段刚、韩苞','資治通鑑升平五361九月後十月前','本人平陽太守段剛被張平殺，不把355逐出上黨當死亡')
    results[8308].update(review_status='primary_text_corroborated',historical_death=date(361,gd))
    results[8308]['evidence'].append(gd)
    results[8309].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='duanlan_father_of_duankan_and_jinshu_xunxian_campaign_misnaming_unresolved',component_identities=[dict(identity='段遼弟蘭、龕父、卒令支在龕南徙前',keys=[k for k in results[8309]['keys'] if not k.startswith('js:js_12:')],historical_birth=None,historical_death=None,death_upper_bound=350),dict(identity='JS荀羨傳青州段蘭已沒，事件與段龕356相對、疑蘭龕誤',keys=[k for k in results[8309]['keys'] if k.startswith('js:js_12:')],historical_birth=None,historical_death=None)])
    results[8320].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_zhao_xiangcheng_duke_shegui_and_muronghui_tuyuhun_father',component_identities=[dict(identity='後趙襄城公涉歸338戍長安',keys=[k for k in results[8320]['keys'] if k.startswith('sl:')],historical_birth=None,historical_death=None),dict(identity='慕容廆及吐谷渾父涉歸，與10996同源支候選',keys=[k for k in results[8320]['keys'] if k.startswith('js:')],historical_birth=None,historical_death=None,candidate_duplicate_of=10996)])
    for ident,note in {8301:'三key楊盤堅頭子毅從弟河池公右賢王同世系官鏈，334/335任封不是生年，337被殺是毅非盤，無生卒。',8302:'楊羨新淦人羅含州將引主簿，離職送返新淦非死亡，羅含身世年齡不套本人；無生卒。',8303:'楊舉陶侃都尉先驅討杜弢，戰敗或敵死非舉卒，無生卒。',8304:'楊謹JS征虜將軍萧敬文殺涪城與TJ347十二月楊謙同官同事件，目標7597三key此前全閉合，字異硬去重→7597，未知出生。',8305:'榮建甘卓功曹諫散兵，甘卓四子死不說功曹死；原CBDB410326所有日期親屬引文官職全空，不能僅同名勾全核，歷史不詳待橋。',8306:'樂凱武昌太守諫缪坦奪菜田，王舒為梁州守病卒不是樂凱，無生卒。',8307:'兩key段乞特真遼子338奉表獻馬趙，同父子與獨特使事閉合；獻降非死，無生卒。',8308:'三key段剛燕上黨守355被馮鴦逐、361平陽守張平襲殺，軍鎮轉任同燕官鏈；非段龕音近混合，未知出生，361死亡獨立明文。',8309:'七key實讀：主幹段遼弟蘭/龕父、338敗投宇文、343送趙屯令支、350以前已卒；TJ初字回述不定350精卒。JS荀羨傳青州救段蘭明與龕356之役相似、父已先死，不能將該key直接套父壽；整條hold兩支。JS63其子蘭父系簡敘亦保源差，不硬抹。',8310:'殷允子思陳郡康第六子吏部尚書，郗超托袁宏照顧出西，無生卒；不要把殷仲堪父殷師/殷允僅同族合。',8311:'殷師融子仲堪父晉陵守驃騎諮議沙陽男，床下蟻動是父病非本人卒年/歲數；無生卒。',8312:'殷康融子顗父吳興守，顗憂卒非康，未憑殷允父康同名自行硬併，無生卒。',8313:'江播涇令助害桓彝，桓溫18復仇時播已終，死者三子是彪兄弟；18屬溫非播，不由溫歲數估播精卒年，無生卒。',8314:'江敳江虨子琅邪內史驃騎諮議、恒夷父，江惇永和9年49卒非敳；無生卒。',8315:'江泉陳留大鴻臚谷伯以能食，55被殺屬任放兄不是江泉，無生卒。',8316:'江濟逌父安東參軍、逌少孤證早亡但無精年，逌58歲病卒非父年；無生卒。',8317:'兩key江灌道群逌從弟瞢子、吳興守桓溫屢抑→溫薨後尚書中護軍未拜吳郡守卒，無明精年，不把溫373直接套灌。',8318:'江瞢灌父尚書郎世系，兒子溫後卒非父，無生卒。',8319:'江蔚逌子吳興守，逌58卒不是蔚，無生卒。',8320:'兩key混跨代二涉歸：後趙襄城公與日歸338戍長安/告石廣，及JS慕容廆吐谷渾父早亡；不讓廆父活338。JS支候選10996、整行不刪不移頂層日期。',8321:'溫式之溫嶠子放之弟新建侯散騎常侍，放之卒官非弟，無生卒。',8322:'滕遁含弟子交州刺史、修曾孫恬之魏郡死非遁；無生卒。',8323:'熊鳴鵠熊缙子武昌太守、遠從子，遠病卒/缙終鄱陽不是鳴鵠死亡；無生卒。',8324:'兩key王典後趙典農中郎將338海濱屯田同曹伏戍海配套使命，軍萬不是年齡，不把地方兵殺數作卒；無生卒。',8325:'HY王利劝李壽先稱成都王送期晉，candidate本key特指此338事；同卷前301趙廞黨另王利不可因同卷同名就說此key已混。未知生卒，仍不把任調等後誅套王利。'}.items():results[ident]['unresolved'].append(note)
    round25b_reads={8326:[('js_09','劭字敬伦'),('ssxy_01','王劭王荟共诣宣武'),('ssxy_02','大奴王劭也已见')],8327:[('js_09','协字敬祖')],8328:[('sl_02','辽东人王岌密信请降'),('tj_15','辽东人王岌密信请降')],8329:[('js_12','淡子度世，骁骑将军')],8330:[('js_10','淮南小中正王式')],8331:[('ssxy_03','彭之字安夀琅邪人')],8332:[('ssxy_00','微字幼仁琅邪人')],8333:[('js_07','祭酒东莱王恢')],8334:[('js_12','子淡嗣，历位右卫将军')],8335:[('js_11','涛字茂略')],8336:[('js_11','祎之字文邵'),('ssxy_02','僧恩王祎之小字也')],8337:[('js_01','王暠子丹阳丞耆之'),('ssxy_02','耆之字修载琅邪人'),('ssxy_03','无忌于坐防刀斫耆之')],8338:[('ssxy_02','临之字仲产琅邪人')],8339:[('sl_01','王华为渡辽将军'),('sl_02','横海将军王华帅舟师'),('tj_16','王华为渡辽将军')],8340:[('js_09','荟字敬文'),('ssxy_01','荟字敬文丞相最小子'),('ssxy_03','王卫军云酒正自引人箸胜地')],8341:[('js_00','父遐，见《外戚传》'),('js_10','以散骑常侍王遐为军司'),('js_17','王遐，字桓子')],8342:[('js_10','遣卓兄子行参军仰求和'),('js_16','卓兄子仰时为敦参军')],8343:[('sz_00','达居丧失礼')],8344:[('js_05','谭荐干宝、范珧于朝'),('js_09','范达子珧为湘东太守')],8345:[('sl_01','四角王簿句大'),('tj_15','北羌王薄句大')],8346:[('js_10','景子友嗣，官至廷尉')],8347:[('js_10','景弟鉴，太子庶子')],8348:[('js_03','高官督护缪坦'),('js_10','遣邓岳、缪坦收若思')],8349:[('js_07','遣督护缪蕤、李恒')],8350:[('js_14','其二舅罗崇、罗友'),('ssxy_03','防阳罗友有大韵')]}
    for ident,qs in round25b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第25批B·王氏兄弟世系同卷逐人審讀','Actual cited family/office passage read; childhood age, posthumous title, relatives deaths excluded from exact chronology') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round25b')
    results[8341].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='queen_father_wangxia_closed_but_early_daiyuan_junsi_branch_no_explicit_family_link')
    results[8341]['component_identities']=[dict(identity='簡文順王皇后父王遐桓子、王述從叔',keys=[k for k in results[8341]['keys'] if not k.startswith('js:js_10:')],historical_birth=None,historical_death=None),dict(identity='322前戴若思散騎常侍軍司王遐、可能同人但無字系明橋',keys=[k for k in results[8341]['keys'] if k.startswith('js:js_10:')],historical_birth=None,historical_death=None)]
    results[8336]['source_reported_age_at_death']={'expression':'年未三十','age_lt':30,'calendar_year':None,'source':'晉書王述傳附祎之/世說王氏世家'}
    for ident,note in {8326:'三key王劭敬倫導第五子大奴、鳳毛、與荟訪桓温同人；本傳卒贈車騎無精年，40748是嗣子王謐非劭，無生卒。',8327:'王協敬祖導子抚軍參軍武岡侯早卒、嗣子謐生父劭，40748是謐非協，不倒算生卒。',8328:'兩key王岌遼東襄平334密信降皝同役，皝欲坑後免眾非岌死，無生卒。',8329:'王度世淡子峤孫驍騎將軍，峤卒官不是孫，無生卒。',8330:'王式淮南小中正繼母返前夫家禮議，父及母亡皆非本人死；無生卒。',8331:'王彭之安壽彬子正孫、黃門郎小字虎防，彪之為彭之第三弟、20白須屬彪之不套彭之歲數，無生卒。',8332:'王微幼仁荆產、祖乂父澄右軍司馬尚書郎，劉尹清風評語不是生卒；不同南朝畫家僅同名，無生卒。',8333:'東萊王恢元帝鎮東祭酒陰通周玘夏鐵，夏鐵死後奔玘被殺埋豕牢明文，所讀無明CE精年，不套玘56歲死；無生卒。',8334:'王淡峤子右衛侍中中護軍尚書廣州刺史，父峤庐陵病卒非淡，無生卒。',8335:'王濤茂略堂邑鑑弟著作郎無錫令早卒，41是兄鑑，無生卒確年。',8336:'兩key王祎之文邵/文劭僧恩述次子坦之弟尚尋陽公主中書侍郎，未30卒只是上限年齡；46歲屬兄坦之，無生卒確年。',8337:'三key王耆之修載、王廙第三子/JS作暠子丹陽丞，無忌拔刀裒景救免同獨特宴事件閉合，父字廙/暠異記照存；水舸逃免不死，無生卒。',8338:'王臨之仲產彪之子東陽守阿林、原注明林應臨，無生卒；不因林名拆人。',8339:'三key王華後趙渡遼將軍338水師/341横海襲安平，同趙海軍將轉號鏈，非南朝宋王華只同名；無生卒。',8340:'三key王荟敬文導少子施粥活饑、桓温席收庾氏欲退、王衛軍飲酒同傳別傳稱號。衛將軍為卒贈不得以題號證活時官，無生卒。',8341:'王遐本紀皇后父與外戚桓子王述從叔明閉合，咸安二/寧康初追贈年不同源別不是本人活到373；JS10戴若思軍司常侍王遐無字系明橋，時間可以相容但不強行全keys確同人，整行hold。無生卒。',8342:'兩key甘仰卓兄子王敦參軍求和令卓旋軍同親屬同使命，叔卓四子被害未說仰死；無生卒。',8343:'皇甫達北地太子舍人妙相張珮華夫，15歲是妻適嫁年齡、20餘隱修及永和說法/卒亦妻事，無本人出生卒。',8344:'兩key范珧獲譚薦朝與陶侃酬范達子湘東守同元帝東晉初官人脈可接，年向70屬薦者譚非珧，不倒算生卒；範范姓字異不是兩人。',8345:'兩key簿句大/薄句大北羌四角王334敗斌韜奔馬蘭郭敖追反敗同事，郭敖誅死不是句大，無生卒。',8346:'紀友紀景子瞻孫廷尉嗣、長景早卒非友，無生卒。',8347:'紀鑑紀景弟瞻子太子庶子大將軍從事中郎、先瞻卒，僅相對先卒無精年；無生卒。',8348:'兩key缪坦王敦高官督護奪菜田→322使同鄧岳害戴若思、周顗，官事同一敦府鏈；被害是戴周非缪，無生卒。',8349:'缪蕤敦督護與李恒受周訪節制擊張彥，斬張彥非蕤死，周訪折齒非蕤傷，無生卒。',8350:'兩key羅友習鑿齒舅、桓温府人襄陽郡→廣益刺史乞食記蜀街同生涯；晉陽秋稱他仁（宅仁源字）保留，不以地名數量/500人食器作歲，卒益州無精年，無生卒。'}.items():results[ident]['unresolved'].append(note)
    results[8317]['unresolved'].append('世說劉注稱江虨從弟、JS稱江逌從弟，從兄名字源別照存；本人道群及尚書中護軍官鏈同，未憑此把江逌/虨也自動合一。')
    round26a_reads={8351:[('js_09','江州别驾罗洞')],8352:[('yl_03','浈阳令羊嗣')],8353:[('js_05','羊固拜临海太守'),('ssxy_01','固字道安太山人')],8354:[('ssxy_01','羊楷字道茂')],8355:[('ssxy_00','羊权为黄门侍郎')],8356:[('ssxy_02','王子敬与羊绥善')],8357:[('js_05','聃字彭祖')],8358:[('yl_07','胡毋崇为永康令')],8359:[('js_19','徐深、艾毅重请峻')],8360:[('yl_00','参军荀楷、李慕')],8361:[('js_10','督护荀璲领数百人')],8362:[('js_12','籍嗣位，至散骑常侍')],8363:[('wsh_01','子显，知名于时')],8364:[('js_03','俊子仰之，大长秋')],8365:[('js_03','子俊嗣，为尚书郎')],8366:[('js_15','潜遣东莞人华挺')],8367:[('js_05','茂嗣爵')],8368:[('js_17','临安令万宠迎置县中')],8369:[('sl_09','以董皎为相国'),('tj_16','相国董皎、侍中王嘏'),('wsh_02','乃加相国董皎大都督')],8370:[('ssxy_02','萧轮字祖周乐安人')],8371:[('js_10','子颙，先兼卒')],8372:[('js_07','访部将苏温收曾'),('js_09','侃将郑攀、苏温、马俊'),('js_19','其将马俊、苏温等执曾')],8373:[('ssxy_00','虞存弟謇作郡主簿'),('ssxy_02','魏𫖮虞球虞存谢奉'),('ssxy_03','虞存嘲之曰')],8374:[('ssxy_00','范汪棋品曰謇字道真')],8375:[('js_12','子谷，位至吴国内史')]}
    for ident,qs in round26a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第26批A·羊華家系及地方臣屬逐人審讀','Actual subject prose read; dynasty reigns and chronological anecdotes do not create exact birth/death years') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round26a')
    for ident,note in {8351:'羅洞江州別駕議攻榻杭解匡術圍，出計非死亡，後溫峤病卒非洞；無生卒。',8352:'羊嗣湞陽令貪被逐、將須淖沈欄中不明本人死，尹虞手劍死的是功曹，無生卒。',8353:'兩key羊固道安泰山坦子臨海守黃門侍郎書家、宴豐同羊曼對比，55歲死是羊曼不是固；後贈大鴻臚未精年，無生卒。',8354:'羊楷道茂忱子繇孫尚書郎娶諸葛恢次女，庾亮子蘇峻害屬另一女婿，不套羊楷；無生卒。',8355:'羊權道輿悅子黃門侍郎尚書左丞、亡伯羊秉無嗣，32歲及亡伯死皆不是權年齡死亡；無生卒。',8356:'羊綏中書郎與獻之善少亡，前支遁丘墓詩375及後獻之38645歲不是綏卒，無生卒確年。',8357:'羊聃彭祖曼弟庐陵守虐殺簡良族、獄原命後夢祟旬日疾死；無明紀年，55歲死是兄曼，不造聃出生卒。',8358:'胡毋崇永康令苛暴受杖除名民，受刑罷不等死，無生卒。',8359:'艾毅淮陵故吏同徐深請蘇峻再任，蘇峻年齡反亂死不套艾毅，無生卒。',8360:'荀楷劉胤參軍同張滿李慕為郭默斬，無該引文CE年；腊日酒豚起釁與殺害非必同日年，暫不填精卒，不與魏荀愷僅近音合。',8361:'荀璲卞敦督護率數百援蘇峻難，卞敦愧卒非璲，無生卒。',8362:'荀籍蕤子嗣爵散騎常侍大長秋，父蕤卒官、叔荀羡35838歲非本人，無生卒。',8363:'莫顯含子題父代昭成時左常侍，昭成在位338–376是任官寬界非出生卒；父左將軍卒及子題賜死不套本人，無生卒。',8364:'華仰之恒孫俊子大長秋，69歲及清貧卒屬祖恒，不反算仰之生卒。',8365:'華俊恒子仰之父尚書郎，祖輩69卒不套俊；無生卒。',8366:'華挺東莞人349受朝廷命授桑虞寧朔青州使，桑虞辭命卒官非華挺；沒有直接族系橋不合後世華寶父同名，無生卒。',8367:'華茂譚子化弟嗣爵，討汲桑死是兄化、王敦時譚卒非茂，無生卒。',8368:'萬寵臨安令庇郭文、文卒葬之，十五日二十日是郭文絕粒終日非寵年齡死，無生卒。',8369:'三key董皎成漢壽相國→勢時與王嘏議祖廟、加大都督分災同官鏈。347國亡不等皎死，史敘分災不是真實死亡，無生卒。',8370:'蕭輪祖周樂安才學善三禮常侍國子博士、孫丞公婦父擬太常同明族官，劉尹說可作三公只是評語非任三公；無生卒。',8371:'薛顒兼子先兼卒，323兼卒只父界而本人生卒精年未知，無出生。',8372:'三key蘇溫陶侃將同馬俊迎杜曾，後擒曾降周訪同完整轉屬；JS周訪傳稱訪部將而杜曾傳稱曾將源敘角度保留，不因轉屬誤split；杜曾斬非溫死，無生卒。',8373:'三key虞存道長偉子謇兄何充上佐、四族之俊及嘲魏顗同士族圈；長琳是道長/和琳兩虞合稱非另給存字長琳，不錯套魏顗死刑玩笑。無生卒。',8374:'虞謇道真存弟郡主簿功曹、請斷何充常客止，母父或哥哥誄未本人卒年，無生卒。',8375:'虞谷虞斐思行子、潭兄孫，父命谷謁桓彝→吳國內史，官未達而喪是父非谷；父字拆碼斐保原，無本人生卒。'}.items():results[ident]['unresolved'].append(note)
    round26b_reads={8376:[('ssxy_02','永字君长成阳人'),('ssxy_03','卫君长为温公长史')],8377:[('js_14','子方平嗣')],8378:[('js_17','逐太守袁晏')],8379:[('js_14','猷字申甫')],8380:[('js_14','准子冲，字景玄')],8381:[('js_14','质字道和')],8382:[('gs_01','仆射褚昱诸葛惔等奏')],8383:[('js_09','寻阳太守褚诞')],8384:[('ssxy_01','许璪字思文')],8385:[('gs_01','仆射褚昱诸葛惔等奏')],8386:[('ssxy_01','恢子衡字峻文')],8387:[('js_12','拔郡人谢奉'),('ssxy_01','谢奉字道'),('ssxy_02','虞球虞存谢奉')],8388:[('js_18','继以谢擢，侵刻如初')],8389:[('js_12','永嘉太守谢毅')],8390:[('hhs_09','谢沉书曰'),('js_14','谢沈，字行思')],8391:[('yi_02','所亲韩戢、谢稚')],8392:[('js_13','子韶，字穆度'),('ssxy_03','韶字穆度万子车骑司马')],8393:[('js_10','子隰，康帝时官至临海太守'),('sg_17','世子隰临海太守')],8394:[('tj_16','遣都尉赵槃如赵')],8395:[('js_11','又攻伪南中郎将赵盘'),('sl_02','皝遣都尉赵盘如赵'),('sl_03','遣南中郎将赵盘'),('tj_18','遣南中郎将赵盘'),('js_12','俊将王腾、赵盘寇琅邪')],8396:[('js_07','子龚，与诱俱死'),('tj_13','戊寅，赵诱、朱轨')],8397:[('js_14','父育，郡主簿')],8398:[('sl_01','居就令游泓'),('sl_02','居就令游泓'),('tj_16','居就令游泓'),('sl_03','邃子泓仕为居就令')],8399:[('js_09','时兄子迈、外甥周翼'),('ssxy_00','鉴兄子迈字思逺')],8400:[('ssxy_03','郝隆七月七日出日中仰卧')]}
    for ident,qs in round26b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第26批B·人物本傳與後世引文作者辨析','Actual prose read, retrospective quotation and disputed kinship not silently converted into living dates') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round26b')
    for ident in [8382,8385]:results[ident].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='gaoseng_huiyuan_retrospective_petition_name_needs_original_petition_collation')
    results[8389].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='yongjia_xieyi_read_bare_cbdb424691_no_bridge')
    results[8390]['source_reported_age_at_death']={'age':52,'calendar_year':None,'source':'晉書謝沈傳'}
    results[8391].update(review_status='duplicate',duplicate_of=8388,identity_status='xie_zhi_zhuo_variant_same_jiang_zhuang_rinan_hanji_successor')
    results[8388]['evidence'].append(chunk_ev('yi_02','所亲韩戢、谢稚','南齊書林邑傳','同JS謝擢為姜壯/莊所使日南前後韓戢侵刻官，獨特職事鏈字異可合'))
    results[8394].update(review_status='duplicate',duplicate_of=8395,identity_status='zhaopan_same_yan_duwei_338_mission_pan_character_variant')
    results[8394]['evidence'].append(chunk_ev('sl_02','皝遣都尉赵盘如赵','十六國春秋燕皝咸康四338','與TJ趙槃同都尉奉皝命問虎師期並還棘城，槃盤異字；8395全部key已讀同燕將官鏈'))
    gong_js=chunk_ev('js_07','子龚，与诱俱死','晉書趙誘傳','明文父誘子龔同役俱死，不是僅把父卒猜套子')
    gong_tj=chunk_ev('tj_13','戊寅，赵诱、朱轨及陵江将军黄峻与曾战于女观湖，诱等皆败死','資治通鑑建武元317年女觀湖之戰','已讀同年標題及八月後冬十月前語境，獨立定父誘此役317；合JS龔俱死確定本人同年，暫不取精月日')
    gongdate=date(317,gong_tj,derivation='晉書明子龔與父誘俱死；通鑑明誘女觀湖此役在317，跨書同役反推年，非獨立直接點名龔紀年')
    gongdate.update(precision='year_inferred_from_explicit_same_event_cross_reference',source='晉書趙誘傳子龔俱死＋資治通鑑建武元317年父誘女觀湖戰死',excerpt=gong_js['excerpt']+'\n'+gong_tj['excerpt'])
    results[8396].update(review_status='primary_text_corroborated',historical_death=gongdate)
    results[8396]['evidence'].extend([gong_js,gong_tj])
    results[8395]['attested_activity_years']=[338,356,366,367]
    results[8398]['genealogical_source_variants']=[dict(source='資治通鑑',relationship='游邃兄子'),dict(source='十六國春秋前燕錄九',relationship='游邃子')]
    for ident,note in {8376:'兩key衛永君長成陽左軍/溫嶠長史、白石山作文共遊同名號官鏈，非衛展只同姓；無生卒。',8377:'袁方平喬子山松父義興琅邪守大司馬掾，36歲是父喬、孫恩殺山松非方平，無生卒。',8378:'袁晏梁國太守被民逐戴洋占祖約平城，逐非死，占辛酉/太寧三年流星不直接本人卒，無生卒。',8379:'袁猷申甫瑰弟宏祖、呂江都武康令侍中衛尉同世系，瑰退休告老非猷卒；無生卒。',8380:'袁衝景玄準子耽父光祿勳，25歲死是子耽不是衝，不倒推生卒。',8381:'袁質道和耽子湛豹父琅邪內史東陽守，父耽25早亡及子湛仆射卒不套質；無生卒。',8382:'高僧傳慧遠傳追述成帝幼冲庾冰/何充沙門敬王議作仆射褚昱，源字待與弘明集原奏及晉書對勘，不能只近字硬改褚裒/褚翜；也不把桓玄403回述當本人仍活。生卒不詳。',8383:'褚誕尋陽守同鄧岳抗蘇峻列旗，是溫峤檄文名人，非本人生卒；無日期。',8384:'許璪思文義興陽羡祖艷父裴、王導從事吏部侍郎入帳鼾睡，故事無生卒；後庾會19遇害別人不套。',8385:'高僧傳慧遠傳同奏作諸葛惔，是否諸葛恢源名誤需原奏對勘，暫不硬改或跨片去重；成帝時敬王舊議不證桓玄時仍活，生卒不詳。',8386:'諸葛衡峻文恢子娶邓攸女荥陽守，父恢亡後婚小妹是父喪非衡死，無生卒。',8387:'三key謝奉弘道山陰端孫鳳子、何充拔郡吏→安南廣州吏部尚書失官還東四族俊、謝安奇士評同職圈，免官非死，無生卒。',8388:'謝擢姜壯交州使日南繼韓戢侵刻，與南齊謝稚同官同前後任同責任事件，吸收8391；夏侯覽林邑殺不是謝擢死，無生卒。',8389:'謝毅永嘉守赦後殺周矫被殷浩收廷尉案，王彪之拒受未記毅死；原CBDB424691名之外全空無橋，不以同名标全核。無生卒。',8390:'兩key謝沈行思會稽山陰斐曾孫秀子，JS明著後漢書百卷，HHS志引謝沉書為相同著史作者名字沈沉異寫；漢明帝東平王蒼事是著書內容，不能把謝沈放東漢朝官。52卒無精年，不補出生。',8391:'謝稚日南守姜莊所親繼韓戢，與JS謝擢事件官鏈唯一可合→8388；不套夏侯覽被殺347為己卒。',8392:'兩key謝韶穆度萬子恩父車騎司馬小字封/封胡等諸說原記保留，42歲死是父萬非韶；早卒無精年，不倒算出生。',8393:'兩key賀隰賀循子臨海守、JS明康帝時官，SG注父循太興二31960歲非子；無生卒。',8394:'趙槃單TJ338皝都尉赴趙聽師期，與8395SL02趙盤同使命明事字異合→8395，不新增人。',8395:'五key趙盤燕皝都尉338使趙還→俊將寇琅邪→暐南中郎將366宛城367桓豁執送京同燕軍官連續鏈，吸收8394槃異字，擒非死；無生卒，367親自被擒證在世只約束估算。',8396:'趙龔趙誘子與誘同死明文，TJ317誘女觀湖死獨立年，據明同役關係反推龔317卒，已標cross_reference推定而非原書單句直載；不取精月日，無出生。',8397:'車育胤父南平郡主簿王胡之勸專學，胤孝武/元顯時死不是父育，無生卒。',8398:'四key游泓邃家居就令338應趙奔高句麗同官事，TJ邃兄子與SL子是源別親屬異說保留，不以此另造泓，無生卒。',8399:'兩key郗邁思遠鑒兄子、幼受吐飯過江少府中護軍同人，鑒339卒非邁，64歲是周翼非邁，無生卒。',8400:'郝隆佐治汲郡桓温府參軍、七夕晒書/遠志小草譏謝安，年日七七非生卒；劉注寮屬名職字作仕吳至征西將軍疑傳抄省誤，不依此強推為吳朝大將或生年，保原記無生卒。'}.items():results[ident]['unresolved'].append(note)
    round27a_reads={8401:[('sl_01','虎遣将军郭泰麻秋'),('sl_02','虎遣其将郭泰追撃之')],8402:[('js_09','将军郭开从称往长沙赴丧')],8403:[('js_11','子骜，官至临贺太守')],8404:[('js_12','使参军郑袭戍准阴')],8405:[('js_10','子诞，位至中军参军')],8406:[('js_04','佣，早卒')],8407:[('js_04','宁，鄱阳太守')],8408:[('js_04','无子，从孙广嗣')],8409:[('js_10','奋武将军阮抗请为长史')],8410:[('js_04','子晞之，南顿太守')],8411:[('js_04','普，骠骑谘议参军')],8412:[('js_09','别驾阮朗并不从')],8413:[('ssxy_03','牖字彦伦裕长子也')],8414:[('gs_01','义阳太守院保')],8415:[('js_14','收泾令陈干杀之')],8416:[('yl_01','虞谭、陈略、曹毗、毛安之')],8417:[('yl_07','陈舒议至尊临温公夫人丧')],8418:[('sl_01','长安人陈良夫奔黑羌'),('tj_15','长安人陈良夫奔黑羌')],8419:[('js_09','而夏病卒'),('js_17','父夏，以无行被废')],8420:[('js_09','岱，散骑侍郎')],8421:[('js_09','伏波将军陶延夜趣巴陵')],8422:[('js_09','瞻息弘袭侃爵')],8423:[('js_09','旗，历位散骑常侍')],8424:[('js_07','浚子湮，字恭之')],8425:[('js_07','猷宣城内史，王导右军长史'),('js_07','宣城太守陶猷灭之')]}
    for ident,qs in round27a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第27批A·阮陶家系與地方臣屬同卷逐人核讀','Actual source paragraph read, indirect narrative sequence retained without manufacturing precise dates') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round27a')
    results[8413]['candidate_duplicate_of']=8406
    results[8413]['source_reported_age_at_death']={'expression':'年未弱冠','age_lt':20,'calendar_year':None,'source':'世說新語阮思曠大兒疾卒及阮氏譜'}
    for ident,note in {8401:'兩key郭泰虎將333捕石堪至338與麻秋追段遼同趙軍鏈，炙殺石堪/斬敵3000非本人；無生卒。單SL各年無獨立早源時不輸活動。',8402:'郭開陶稱將被縛倒懸帆樯仰彈酷刑，原段未說死亡，亦不以339上表時當刑時年；無生卒。',8403:'郭驁郭璞子臨賀守，父324被王敦殺非子，驁敖不同形可另候選但本條不憑近音擅合；無生卒。',8404:'鄭襲荀羡參軍戍淮陰，28/35838歲屬荀羡，無生卒。',8405:'鍾誕鍾雅子中軍參軍早卒，蘇峻殺雅超不是誕死，勿合卞誕同名省姓；無精生卒。',8406:'阮傭阮裕三子名序首人早卒歆之父，裕62歲非己；SS阮牖裕長子未弱冠亡為同人異字候選8413，但JS僅列名序未明字號，不以早卒父同就硬刪。無生卒。',8407:'阮寧裕子鄱陽守腆萬齡父，62歲屬父裕，子任官元熙非寧本人活到元熙；無生卒。',8408:'阮廣孚從孫嗣爵，孚未赴廣州49歲卒非廣，無生卒。',8409:'阮抗奮武將軍泰山守請宋挺長史被劉隗劾免，病死者宋挺非抗，無生卒。',8410:'阮晞之阮放子南頓守，44歲發渴死為父放，無生卒。',8411:'阮普裕子驃騎諮議參軍，父62卒非普，無生卒。',8412:'阮朗溫放之交州別駕反對征林邑與杜寶同誅，放之卒官不同本人；本句無精紀年，不套源升平寬政權區間，無生卒。',8413:'阮牖彦倫裕長子州主簿未弱冠疾亡，三寶祈祷敘不造卒年；JS8406裕第一子傭早卒可異字候選但缺字號子歆之橋，不硬合。年未20只年齡界非精生年。',8414:'院保義陽守與法遇書信施遺，院字原記未對勘不直接改阮保；後襄陽寇與法遇60卒非本人，無生卒。',8415:'陳幹涇令為謝尚收殺，顧和論尚違赦擅刑，甲戌赦所指未明年日不能當卒紀年；無生卒。',8416:'陳略御覽引何法盛晉中興書左衛將軍任官名列與虞潭曹毗毛安之，列不同任並非同時任或同年卒，無生卒。',8417:'陳舒議至尊吊溫公夫人服，夫人喪非本人死，不由溫公任號反推出生；無生卒。',8418:'兩key陳良夫長安奔黑羌334結薄句大侵擾北地馮翊，郭敖敗誅非良夫死，無生卒。',8419:'兩key陶夏侃次子都亭侯後世子→殺斌庾亮表未至病卒，陶淡父無行廢同家系閉合；父侃334喪後未明夏死同年，慎不填334精卒，淡15/16絕粒非父年，無生卒。',8420:'陶岱侃子散騎侍郎，兄稱339被誅不說岱同死，無生卒。',8421:'陶延侃伏波將軍與鄭攀巴陵破王貢，張奕杜弢等後死非延，無生卒。',8422:'陶弘瞻子侃孫襲侃爵光祿勳、綽之父，candidate本key明瞻子，只核該scope；同段「洪辟丞相掾早卒」是侃長子另一人，不能因同音混套本人的父子世代。無生卒。',8423:'陶旗侃子散騎常侍郴縣伯咸和末侍郎，歷代子定襲之謙之卒及宋國除非本人同年，無生卒。',8424:'陶湮恭之浚子猷兄臨海守黃門侍郎，韓晃殺于湖令馥是子非父，無生卒。',8425:'陶猷恭豫浚子湮弟宣城內史/王導右軍長史滅孫弼同人，陶侃族其餘不同人不因姓合；無生卒。'}.items():results[ident]['unresolved'].append(note)
    round27b_reads={8426:[('js_09','琦，司空掾')],8427:[('js_09','范，最知名'),('js_16','侃子胡奴尝于曲室抽刃'),('gs_01','郡人陶范苦相要留'),('hj_03','侃子胡奴尝于曲室抽刃'),('ssxy_01','胡奴陶范别见')],8428:[('js_17','祖茂，武昌太守')],8429:[('js_09','臻弟舆，果烈善战')],8430:[('ssxy_02','陆迈与俱')],8431:[('js_11','怿遣牙门霍佐'),('sl_02','遣牙门霍佐迎将士妻子')],8432:[('js_10','初，京兆韦泓')],8433:[('js_18','使韩戢领日南太守'),('yi_02','韩戢、谢稚')],8434:[('hy_00','太史令韩约上言')],8435:[('sl_03','遣督护韩苞'),('tj_18','杀段刚、韩苞')],8436:[('sl_09','太史令韩豹奏曰')],8437:[('js_10','子毗嗣，官至散骑侍郎')],8438:[('js_14','子淳，历尚书吏部郎')],8439:[('js_14','时宗人球亦有令闻')],8440:[('ssxy_03','辟疆呉郡人')],8441:[('sl_01','渔阳太守马鲍')],8442:[('gs_01','晋长沙太守腾含')],8443:[('js_11','子耆，官至散骑常侍')],8444:[('js_12','谢奉、魏𫖮等以为佐吏'),('ssxy_02','孔沈魏𫖮虞球'),('ssxy_03','魏长齐雅有体量')],8445:[('sl_02','将军鲜于亮拥段辽'),('sl_03','左常侍鲜于亮与数骑'),('tj_16','赵将军范阳鲜于亮失马'),('tj_16','左常侍鲜于亮曰'),('tj_17','以慕容恪、鲜于亮为前驱')],8446:[('js_09','奋宗将军袭保')],8447:[('js_09','谒者仆射任瞻锡谥'),('ssxy_03','任瞻字育长乐安人')],8448:[('sl_08','右长史任处领国子祭酒')],8449:[('sz_00','晋咸康中沙门僧建')],8450:[('sl_08','鄯善王元礼[一作孟]'),('js_15','鄯善王元孟献女')]}
    for ident,qs in round27b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第27批B·陶氏餘支及西域僧人逐條原文核讀','Actual paragraph reviewed; indirect upper bounds distinguished from exact activity years') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round27b')
    results[8427].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='taokan_son_hunu_closed_four_keys_gs_xunyang_resident_same_name_unbridged',component_identities=[dict(identity='陶侃子范胡奴、光祿勳及袁宏東征賦事',keys=[k for k in results[8427]['keys'] if not k.startswith('gs:')],historical_birth=None,historical_death=None),dict(identity='慧永過潯陽苦留西林寺郡人陶范，可能同人但本段無父字官橋',keys=[k for k in results[8427]['keys'] if k.startswith('gs:')],historical_birth=None,historical_death=None)])
    results[8436].update(review_status='duplicate',duplicate_of=8434,identity_status='han_bao_yue_variant_same_taishiling_warning_liban_palace_conspiracy')
    results[8434]['evidence'].append(chunk_ev('sl_09','太史令韩豹奏曰','十六國春秋李班傳','與HY韓約同太史令警班宮室陰謀兵氣戒親戚、越期弒班唯一事件，豹約字異同人，不取晚本獨有年月'))
    baoev=chunk_ev('tj_18','张平袭燕平阳，杀段刚、韩苞','資治通鑑升平五361九月後十月前','平陽督護韓苞同段剛被殺本人，不以敵叛降作死亡')
    results[8435].update(review_status='primary_text_corroborated',historical_death=date(361,baoev))
    results[8435]['evidence'].append(baoev)
    results[8449]['appearance_upper_bound']=342
    results[8449]['appearance_constraint_explanation']='比丘尼傳明僧建咸康中於月支得僧祇尼羯磨戒本；咸康335–342，故登場估算不能晚於342，此上界不表示342仍在世，亦不推生卒。'
    results[8445]['attested_activity_years']=[338,342,350]
    results[8432]['evidence'].extend([chunk_ev('js_10','詹卒，遂制朋友之服','晉書應詹傳韋泓附','同傳詹咸和六331卒，泓遂制服哭止宿草，明聯同次喪事；38歲薦書在此前不反推出生'),chunk_ev('js_10','以咸和六年卒，时年五十三','晉書應詹傳331喪事紀年','死者詹非泓；僅與泓同事制服句合證泓在世331，不能套泓卒年')])
    results[8432]['last_attested_activity']=331
    results[8432]['activity_constraint_explanation']='JS同篇應詹咸和六331卒、泓遂制服朋友哭止宿草，明聯同次事件，故331在世；荐書38歲屬更早不反推出生。'
    for ident in [8433,8388]:
        results[ident]['appearance_upper_bound']=347
        results[ident]['appearance_constraint_explanation']='JS林邑/南齊林邑明韓戢、謝擢/稚先任，繼者夏侯覽永和三347至日南；只能作登場最晚上界，不當347本人仍在世或真正初仕。'
    for ident,note in {8426:'陶琦侃子司空掾，與旗為同段相鄰不同兩人，勿將旗散騎常侍郴伯套琦；無生卒。',8427:'四key陶范道則侃第十/第九子胡奴、袁宏賦拔刀同父事閉合；HJ03是袁宏傳附錄引晉書，不是東漢活人。GS慧永傳郡人陶范同潯陽名可能同人但無明家系官橋，暫支隔離整行hold，無生卒。',8428:'陶茂武昌太守陶潛祖侃子，陶淡/潛故事年齡病卒皆非茂，無生卒。',8429:'陶輿侃兄子臻弟武威將軍、破杜弢後戰重創卒，未明本段CE年；不與張舆/王輿同名合，無確生卒。',8430:'陸邁功高吳郡吏部郎阻蘇峻焚吳，蘇峻奉詔討沈充是324故事回述，不把後328叛死套邁；無生卒。',8431:'兩key霍佐庾怿魏興牙門迎將士妻子反驅300降趙同使命，50鴆死是庾怿非霍，無生卒。',8432:'韋泓元量京兆貧依應詹、薦元帝時38歲少府卿，331詹死朋友服明其尚在世，38不是死亡年齡或331當年歲，無出生卒確年。',8433:'兩key韓戢姜壯/莊使任日南、侵刻死後繼謝擢同官鏈，死在夏侯覽347來前無精卒，不能把夏侯覽被殺347套韓，無生卒確年。',8434:'HY韓約太史令李班宮室陰謀親戚兵氣奏，同SL韓豹明唯一同奏同班越弒事可合吸收8436；無生卒。',8435:'兩key韓苞燕平陽督護、361張平襲殺同段剛明文，未知出生，不合成韓豹太史令。',8436:'韓豹即HY韓約同太史令警班宮室陰謀親戚兵氣，豹約異名同事件，不是韓苞361軍督護；重定向8434，無生卒。',8437:'顧毗榮子嗣爵散騎侍郎，父317追封公及卒非毗，無生卒。',8438:'顧淳和子尚書吏部郎黃門侍郎左衛將軍，35164歲屬父和非淳，無生卒。',8439:'顧球和宗人州別駕顧榮稱令聞，顧和2歲喪父不是球年，族叔榮死不是球，無生卒。',8440:'顧闢疆吳郡功曹平北參軍名園主人斥王獻之傲慢，無生卒，不將王獻之45卒套主人。',8441:'馬鮑段遼漁陽守338同侯龕張牧降趙，降非死，只晚重編SL無生卒。',8442:'騰含高僧傳長沙太守江陵宅建長沙寺請道安命法汰住；騰/滕姓名可異寫但與早年滕含其他源未核家系，不直接硬合。後文僧人行年非本人，無生卒。',8443:'高耆高崧子散騎常侍，崧殷浩被貶客問非耆事，無生卒。',8444:'三key魏顗長齊會稽尹孫說子山陰令、何充郡吏四族俊、虞存戲三章同士族，談者死是玩笑非死刑，無生卒。',8445:'四key鮮于亮范陽趙將338三藏口被俘歸皝左常侍、342木底南軍、350伐趙前驅同官軍鏈，捕為在世不死。出生卒未知；338/342/350為本人具名實際活動可約束估算。',8446:'龔保奮宗將軍抗蘇峻，JS前作襲保後檄稱郭趙龔三將是同段姓異寫，非陶侃督護龔登同姓合，無生卒。',8447:'兩key任瞻育長樂安父琨諒者仆射天門守、339王導冊諡使同官鏈，339冊他人諡不是本人卒；無生卒。',8448:'任處張駿右長史國子祭酒339行辟雍，單SL晚重編具年不作早源確年活動輸入，無生卒。',8449:'僧建沙門咸康中於月支得尼羯磨戒本，男性取尼戒文本不等本人為女尼；升平末70升空者淨檢非僧建，無生卒。咸康最晚342只登場上界不作仍活年。',8450:'兩key鄯善王元孟/元禮一作孟獻女張駿建賓遐觀同唯一君臣事，SL335非本人出生；JS無精年不強套SL晚重編年活動，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round28a_reads={8451:[('js_04','刘惔'),('js_11','征虏将军刘惔'),('js_12','刘惔'),('tj_15','沛国刘惔'),('tj_16','刘惔'),('js_01','刘惔'),('js_13','刘惔'),('js_14','刘惔'),('js_17','刘惔'),('js_18','刘惔'),('sg_10','耽子恢字真长'),('gs_00','刘惔'),('ssxy_00','惔字真长'),('ssxy_01','刘惔'),('ssxy_02','刘惔'),('ssxy_03','刘惔')],8452:[('tj_15','保母刘芝'),('sl_02','刘芝')],8453:[('js_09','督刘遐'),('tj_16','长史刘遐'),('js_17','吏部尚书刘遐')],8454:[('js_17','刘鲕字长鱼')],8455:[('sl_02','吕护'),('sl_03','吕护杀兴'),('tj_17','吕护'),('tj_18','吕护退守小平津，中流矢而卒')],8456:[('js_13','子混嗣')],8457:[('js_13','张狢渡二万骑攻邾城')],8458:[('tj_16','张貉陷邾城')],8459:[('sl_02','张貉[一作张贺度下同]'),('tj_16','张贺度'),('tj_17','张贺度、段勤、刘国、靳豚')],8460:[('js_12','龙骧将军徐玄')],8461:[('sl_03','以弟友[一作交]'),('sl_03','范阳王友于信都'),('tj_17','左贤王友为范阳王'),('tj_18','刘拔刺杀征东将军、冀州刺史、范阳王友')],8462:[('tj_15','其妻曹氏'),('sg_04','骠骑将军曹洪女有美色'),('ssxy_03','骠骑将军曹洪女有色')],8463:[('sl_02','朱保败晋师于白石'),('tj_16','朱保败晋兵于白石')],8464:[('hy_00','尚书广汉李摅为御史'),('sl_09','李扩[一作摅]')]}
    for ident,qs in round28a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第28批A逐人原文及同名源支審讀','Actual paragraph consulted; source-specific identity scope distinguished from shared names') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round28a')
    results[8451].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='fifteen_mingshi_sources_closed_js11_military_official_unbridged',component_identities=[dict(identity='沛國劉惔真長、父耽丹陽尹，SG源作恢但字父官閉合',keys=[k for k in results[8451]['keys'] if not k.startswith('js:js_11:')],historical_birth=None,historical_death=None),dict(identity='JS11征虜將軍監沔中軍事義成太守劉惔，缺字父遷官橋',keys=[k for k in results[8451]['keys'] if k.startswith('js:js_11:')],historical_birth=None,historical_death=None)])
    results[8453].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='early_jin_general_versus_post326_shangshu_mixed',component_identities=[dict(identity='324溫嶠督劉遐追錢鳳武將，對照5990軍將支326已卒',keys=[k for k in results[8453]['keys'] if k.startswith('js:js_09:')],candidate_duplicate_of=5990,historical_birth=None,historical_death=None),dict(identity='339郗鑒長史及345吏部尚書劉遐',keys=[k for k in results[8453]['keys'] if not k.startswith('js:js_09:')],historical_birth=None,historical_death=None)])
    results[8462].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='wangdao_wife_and_caohong_daughter_xuncan_wife_mixed',component_identities=[dict(identity='王導妻曹氏，成帝拜見',keys=[k for k in results[8462]['keys'] if k.startswith('tj:')],historical_birth=None,historical_death=None),dict(identity='曹洪女荀粲妻，病亡後荀粲哀悼歲餘卒29，29非妻享年',keys=[k for k in results[8462]['keys'] if not k.startswith('tj:')],historical_birth=None,historical_death=None)])
    results[8457].update(review_status='duplicate',duplicate_of=8458,identity_status='zhanghe_same_zhucheng_assault_name_boundary_error')
    results[8458]['evidence'].append(chunk_ev('js_13','张狢渡二万骑攻邾城','晉書毛寶傳','同TJ張貉邾城攻陷毛寶樊峻赴江事件；渡字屬句內率兵動詞/傳寫，不是第三姓名字'))
    results[8458]['attested_activity_years']=[339]
    results[8459]['candidate_duplicate_of']=8458
    results[8463]['attested_activity_years']=[339]
    for ident,ph,month in [(8455,'秋，七月，吕护退守小平津，中流矢而卒',7),(8461,'六月，甲戌，燕征东参军刘拔刺杀征东将军、冀州刺史、范阳王友',6)]:
        ev=chunk_ev('tj_18',ph,'資治通鑑隆和元362本人死亡','直接本人死亡及月份；SL友卒七月、護卒六月與TJ月序相反，晚本不作等權早源但保異文')
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(362,ev,month=month))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
        results[ident].setdefault('source_limitations',[]).append('SL晚重編本與TJ的六七月次序相反，採明確獨立TJ362年月並保留晚本異文，不用SL充獨立證實。')
    for ident,note in {8451:'十五名士源支可閉合；SG作恢但父耽字真長丹陽尹同，不與8103道生同名合。JS11征虜監沔中義成守缺身份橋暫hold，不憑現代常見310–347補日期。',8452:'石邃保母劉芝宜城君納賄干政兩key同角色；石邃子生三日死不是芝卒，無生卒。',8453:'324軍將與339長史345吏部尚書支有326死亡界隔離；5990自身也混晚吏部支，僅早支候選，不整條重定向。',8454:'高密劉鮞長魚咸康博士老辭不拜壽終，咸康是召官非生卒，無精年。',8455:'征虜降晉執鄭系、安國王呂護殺秦興、野王轉燕同官地鏈。TJ362七月小平津中流矢卒；SL把護卒寫六月，晚本差保。無出生。',8456:'孔坦子混嗣爵，前卒及王導祭父非孔混本人死，未知生卒。',8457:'張狢渡候選姓名誤吸句內渡字，JS與TJ同張貉率軍陷邾毛寶樊峻赴江唯一戰；可整條合8458，不把死亡6000作將死。',8458:'張貉339陷邾城明活動，JS張狢渡句同人可吸收8457。SL稱一作賀度只是晚重編異名，可候選8459但暫不僅依晚注硬合後來賀度整條。無生卒。',8459:'三key張賀度趙征北諫石斌畋獵、349衛軍討梁犢、350蒼亭敗同趙將官鏈；蒼亭明斬靳豚非賀度死。與張貉8458僅SL一作注橋，留候選不刪。',8460:'徐玄蔡謨龍驤將軍守中洲防趙船，蔡謨77卒不是徐，無生卒。',8461:'左賢王友范陽王友到冀州刺史同官爵，SL友一作交；TJ362六月甲戌劉拔刺殺，SL七月異次保。無出生。',8462:'王導妻與曹洪女荀粲妻確混；妻病亡後29卒是荀粲，兩妻均無精生卒，不套名字或丈夫享年。',8463:'朱保白石敗晉殺鄭豹五將兩key同一役，339明本人活動，死者敵人非保。無生卒。',8464:'李攄/擴晚本明一作，廣漢李毅孫御史入南中same，從兄演被壽怒殺非本人，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round28b_reads={8465:[('sl_04','特进领御史中丞梁平老'),('sl_04','朔方桓侯梁平老卒'),('sl_05','乃与特进梁平老强汪'),('tj_17','略阳梁平老'),('tj_18','特进、领御史中丞梁平老')],8466:[('js_12','时王恬服阕'),('js_09','恬字敬豫'),('ssxy_00','王恬字敬豫导次子'),('ssxy_01','虨与王恬等棋第一品'),('ssxy_02','简文目敬豫为朗豫'),('ssxy_03','琅邪王恬女字女宗')],8467:[('yl_08','诏临邑使主范柳所贡物')],8468:[('js_17','子洪袭爵')],8469:[('js_15','其子生亦有孝行')],8470:[('sl_02','遣叅军赵松击巴郡江阳')],8472:[('js_17','城阳邴郁字弘文')],8473:[('hy_00','立妻阎氏为后'),('sl_09','立妻阎氏为皇后'),('tj_15','成主期立皇后阎氏'),('tj_16','尊母阎氏为皇太后'),('js_18','张天锡妾阎氏、薛氏，并不知何许人也')],8474:[('js_11','以武昌太守陈嚣为辅国将军'),('sl_09','遣武昌太守陈嚣为辅国将军'),('tj_16','更以武昌太守陈嚣为梁州刺史')],8475:[('sl_02','叅军鞠运献捷京师'),('tj_16','遣长史刘翔、参军鞠运来献捷论功')]}
    for ident,qs in round28b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第28批B同篇原文逐人核讀','Text read directly; family members ages, late reconstruction dates and author retellings distinguished') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round28b')
    results[8473].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='three_yan_women_distinct_spouses_mixed',component_identities=[dict(identity='李期妻閻后335冊立',keys=[k for k in results[8473]['keys'] if k.startswith(('hy:','sl:','tj:tj_15:'))],historical_birth=None,historical_death=None),dict(identity='李壽妻李勢嫡母閻后，344尊太后',keys=[k for k in results[8473]['keys'] if k.startswith('tj:tj_16:')],historical_birth=None,historical_death=None),dict(identity='張天錫妾閻氏，與薛氏夫病篤時自刎，夫後疾瘳',keys=[k for k in results[8473]['keys'] if k.startswith('js:')],historical_birth=None,historical_death=None)])
    results[8474].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='three_texts_wuchang_liangzhou_general_closed_bare_cbdb_unbridged',component_identities=[dict(identity='武昌太守陳囂梁州刺史趣子午漢中',keys=[k for k in results[8474]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=None),dict(identity='CBDB430718陳囂只有姓名無生卒官原來源關係橋',keys=['cbdb:430718'],historical_birth=None,historical_death=None)])
    results[8474]['evidence'].append(dict(source='CBDB原庫430718',path='out/person_chronology/original_source_evidence.jsonl',excerpt='c_name_chn=陳囂; c_index_year=null; c_birthyear=null; c_deathyear=null; c_notes=null; offices=[]',interpretation='已核原庫欄位但無橋，不能只同名當所有keys閉合'))
    results[8471].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb_tackett_estimate_without_original_genealogy_or_career_bridge',last_manual_review_batch='round28b')
    results[8471]['evidence'].append(dict(source='CBDB原庫178627辛顏',path='out/person_chronology/original_source_evidence.jsonl',excerpt='c_index_year=337; c_index_year_type_code=29; c_birthyear=null; c_deathyear=400; c_dy_range=2; c_notes=Death year estimated by Tackett (cc400); offices=[]',interpretation='337為索引年非生年；400為Tackett估年且帶range限定，不作精確歷史卒年，原始譜牒未取得'))
    results[8465]['attested_activity_years']=[354,357]
    results[8465]['late_reconstruction_dates']={'historical_death':dict(year=372,month=11,source='十六國春秋建元八年冬十一月梁平老卒',precision='late_reconstruction_lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only',not_independently_corroborated=True)}
    results[8467]['appearance_upper_bound']=342
    results[8467]['appearance_constraint_explanation']='太平御覽引晉咸康起居注臨邑使主范柳進貢；咸康335–342，最晚登場上界342不作342在世年。'
    results[8475]['attested_activity_years']=[339]
    for ident,note in {8465:'梁平老略陽特進御史中丞與苻堅善、參政變同四key；354/357獨立TJ明活動。372十一月卒目前僅SL建元八年，移晚重編記載不確證，不推生年。',8466:'六key王恬敬豫導次子、棋第一品、吳會稽內史、桓沖岳父同族官技藝鏈，40卒是王允之不是恬；來源無本人精生卒，父卒339服闋不作本人死亡。',8467:'臨邑使主范柳貢絳綾，原咸康起居注僅朝代寬年段335–342，不是出生卒年；342只最晚登場上界。',8468:'虞胤子洪襲爵，咸康元335卒追贈衛軍者父胤，不套洪。無生卒。',8469:'許生孜子圖父像朝夕拜，父85歲卒元帝旌表皆非本人生卒。',8470:'趙松SL庾亮參軍擊巴郡江陽，TJ同事作李松，姓異需版本考校；單key身份範圍清楚，339僅晚SL不供自動活動修正，無生卒。',8471:'已閱CBDB原庫辛顏178627，索引337type29不是生/活動年，400range2且明Tackettestimated不是確卒，缺原譜牒待考。',8472:'邴鬱弘文城陽邴原曾孫、咸康博士辭疾壽終，曾祖東漢及召徵咸康不作本人生卒，無精年。',8473:'三女性確混：李期妻335閻皇后、李壽妻李勢母閻太后、張天錫閻妾；任一丈夫卒與李期24歲不能套妻，妾自刎未紀年且天錫後病瘳，頂層日期隔離。',8474:'JS庾亮趣子午、SL文庾翼遣、TJ庾亮趣漢中為同武昌守梁州刺史軍事，遣者晚本差保；CBDB430718裸名無橋，未全keys關係核明，339活動留文本支不整行輸入。',8475:'鞠運與劉翔獻捷論功燕參軍同兩key；TJ咸康五339冬，SL咸康六340八月，活動年源別保留，獨立TJ339可用，不以更晚使劉翔卒套運。'}.items():results[ident]['unresolved'].append(note)
    round28c_reads={8476:[('yl_02','贼守马羡奔走')],8477:[('sl_02','免郎中魏敻[一作象]')],8478:[('sl_02','获蜀将李闳黄桓')],8479:[('js_11','巴郡太守黄植'),('sl_09','巴郡太守黄植'),('tj_16','巴郡太守黄植')],8480:[('sl_02','晋义阳将军黄冲'),('tj_16','义阳将军黄冲')],8481:[('sl_08','国王龙熈拒战'),('js_18','遣子熙归本国为王'),('js_18','熙率群下四万人肉袒降于宣'),('wsh_05','前凉张轨所讨龙熙之胤')],8482:[('sl_07','德母公孙夫人方姙')],8483:[('sl_02','吏部尚书刘真'),('js_07','刘真安乡公')],8484:[('sl_02','太子詹事孙珍'),('tj_16','太子詹事孙珍病目')],8485:[('js_15','与大臣张钦等谋讨之'),('sl_09','与大臣疎宗张钦等谋诛之'),('tj_18','与大臣张钦等谋诛之')]}
    for ident,qs in round28c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第28批C寧州及石趙前涼原文逐人覆核','Read actual event and subject scope, distinguishing capture and ancestral recollection from death') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round28c')
    results[8478].update(review_status='duplicate',duplicate_of=8479,identity_status='huanghuan_zhi_variant_same_lihong_bajun_captive_event')
    results[8479]['evidence'].append(chunk_ev('sl_02','获蜀将李闳黄桓','十六國春秋石趙錄李閎被俘事','SL黃桓與獨立JS/TJ巴郡守黃植同伴李閎、庾亮偏軍江陽被俘同一事件異字；不是據同姓強合'))
    results[8479]['attested_activity_years']=[339]
    results[8480]['attested_activity_years']=[339]
    results[8476]['attested_activity_years']=[335]
    results[8483].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='sima_jiong_favorite_versus_shihu_libu_shangshu_unbridged',component_identities=[dict(identity='西晉齊王冏親信五公安鄉公劉真',keys=[k for k in results[8483]['keys'] if k.startswith('js:')],historical_birth=None,historical_death=None),dict(identity='後趙石虎吏部尚書劉真加光祿',keys=[k for k in results[8483]['keys'] if k.startswith('sl:')],historical_birth=None,historical_death=None)])
    ev=chunk_ev('tj_18','事泄，钦等皆死','資治通鑑興寧元363秋八月','郭太妃謀誅天錫事泄，具名張欽等皆死；玄靚14及天錫18是另兩人歲數')
    results[8485].update(review_status='primary_text_corroborated',historical_death=date(363,ev,month=8))
    results[8485]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8485]['evidence'].append(ev)
    for ident,note in {8476:'太平御覽引王隱晉書咸康元335王隨攻寧州賊守馬羨奔走明本人在世，奔不是死，無生卒。',8477:'魏敻一作象石虎郎中選官免庶人，免官不誅，單SL339不供早源活動年，無生卒。',8478:'黃桓同李閎江陽被庾亮軍所獲，對照獨立JS/TJ巴郡黃植同次役異字，可合8479；被俘非死，無生卒。',8479:'黃植巴郡守同李閎339江陽被俘送京三key同，SL02黃桓同事件可吸收8478，不把被俘339當卒年。',8480:'黃衝義陽將軍339降夔安兩key同；鄭進郡名SL歷陽/TJ義陽是另一官差異不套本人，降非死無生卒。',8481:'龍熙/熈焉耆王四萬降楊宣、張植兩傳同；魏書龍鳩尸卑那祖先追記所討龍熙，勿將北魏事件當祖先仍在世。WS前涼張軌 vsJS張駿主名異記保留，不據之推年代；呂光時遣子仍無本段確CE，不造出生卒。',8482:'單SL皝夫人公孫為慕容德母，咸康二336生的是德非母；夢日入臍非歷史生日證，未知生卒，晚本不輸活動年。',8483:'西晉齊王冏五公親信安鄉公與後趙吏部尚書劉真兩key缺字父遷仕橋，年代可跨但不可僅同名認同，hold分支未知生卒。',8484:'孫珍太子詹事病目受崔約嘲而告石宣，同兩key，崔約父子誅非珍死，無生卒。',8485:'三key張欽郭太妃謀誅天錫事泄伏法同人，TJ363八月上下文明本人卒；十四玄靚/十八天錫非欽，未知生年。'}.items():results[ident]['unresolved'].append(note)
    round28d_reads={8487:[('sl_09','颀以旧怨杀曲俭')],8488:[('sl_09','中常侍王广使于赵'),('tj_16','中常侍王广使于赵'),('sl_00','将军王广死之')],8489:[('sl_02','中谒者令申扁'),('tj_16','中谒者令申扁')],8490:[('tj_15','日南夷帅范稚'),('js_16','稚少知名，辟大将军掾，早卒'),('yi_02','日南西卷县夷帅范稚家奴')],8492:[('wsh_01','司虞中郎将贾霸')],8493:[('tj_18','张骏杀江州督护赵毘')],8494:[('hy_00','高士巴西龚壮'),('sl_09','密问龚壮谋以自安'),('tj_16','龚壮曰'),('wsh_02','其臣龚壮作诗七首'),('hy_01','少与巴西龚壮俱知名'),('js_17','龚壮，字子玮')],8496:[('sl_09','宋桶梦内中有一人着赤衣')],8497:[('sl_01','父曰寇觅')],8498:[('js_11','统字长仁'),('ssxy_02','赤玉庾统小字')],8499:[('sl_09','张世度炖煌人也')]}
    for ident,qs in round28d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第28批D石趙及成漢臣屬同卷逐人核讀','Person-specific original passage consulted; mixed records isolated instead of filling uncertain dates') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round28d')
    results[8488].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='western_jin_battle_victim_and_chenghan_envoy_mixed',component_identities=[dict(identity='李壽中常侍王廣使趙',keys=[k for k in results[8488]['keys'] if not k.startswith('sl:sl_00:')],historical_birth=None,historical_death=None),dict(identity='漢趙染攻長安渭陽死晉將王廣，先於李壽使趙',keys=[k for k in results[8488]['keys'] if k.startswith('sl:sl_00:')],historical_birth=None,historical_death=None)])
    results[8490].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='rinnan_chief_and_fangui_son_fanwang_father_mixed',component_identities=[dict(identity='日南西卷夷帥范稚、范文原主',keys=[k for k in results[8490]['keys'] if not k.startswith('js:')],historical_birth=None,historical_death=None),dict(identity='南陽范晷次子稚、范汪父，辟大將軍掾早卒',keys=[k for k in results[8490]['keys'] if k.startswith('js:')],historical_birth=None,historical_death=None)])
    results[8498]['source_reported_age_at_death']={'age':29,'calendar_year':None,'source':'晉書庾懌附統傳年二十九卒'}
    results[8494]['source_reported_death_period']={'expression':'至李勢時卒','source':'晉書隱逸龔壯傳','exact_CE_year':None}
    ev=chunk_ev('tj_18','十一月，姚襄故将张骏杀江州督护赵毘','資治通鑑興寧元363十一月','本人具名死亡，後桓沖斬的是張駿不是趙毘')
    results[8493].update(review_status='primary_text_corroborated',historical_death=date(363,ev,month=11))
    results[8493]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8493]['evidence'].append(ev)
    for ident,excerpt in {8491:'c_personid=31929; c_name_chn=蕭源之; c_index_year=336; c_index_year_type_code=2412; birthyear=0; deathyear=0; notes=Xiao(1) Yuanzhi [31929] XTS, 71B.1a-1b.; offices=[]',8495:'c_personid=17862; c_name_chn=唐輝; c_index_year=null; birthyear=0; deathyear=0; notes=Tang(2) Hui [17862] See genealogical tables of ancestor, Li(2) [17851].; offices=[]'}.items():
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='raw_cbdb_reference_read_original_genealogical_table_not_yet_consulted',last_manual_review_batch='round28d')
        results[ident]['evidence'].append(dict(source='CBDB原庫本人記錄',path='out/person_chronology/original_source_evidence.jsonl',excerpt=excerpt,interpretation='0為無值非公元零年；索引及原書頁號不是本人實際生卒活動年，待取得譜表'))
    for ident,note in {8487:'曲儉張天錫即位年被張頎私怨殺，僅SL晚本不能獨立確證363；後白狗鬼祟暴卒是張頎非儉，不互套。',8488:'王廣確混漢趙攻長安渭陽晉戰死將與後來李壽中常侍使趙；SL早支死亡不可套使臣，頂層生卒空。',8489:'申扁後趙中謁者令石虎石宣同寵典機密兩key閉合；生殺決於扁是權力不是本人卒，無生卒。',8490:'日南夷帥范稚范文主與南陽范晷子范稚確不同；JS原文子汪承稚，candidate若將汪歸廣是摘要錯；勿用前者召年填後者，無兩人精生卒。',8491:'蕭源之CBDB31929指XTS71B.1a–1b尚未讀該原譜，index336type2412非生年，未知，保估算。',8492:'魏書賈霸司虞中郎將造石虎獵車，其他傳作司農費霸可能同事件姓官異文，但本片僅單key先核清，不以賈費近形硬合；無生卒。',8493:'趙毘江州督護被姚襄故將張駿殺，TJ363十一月明本人死，斬張駿是後事另一人，無出生。',8494:'六key龔壯巴西子瑋、父叔李特殺、說李壽奉晉終不仕同唯一事。HY01少與邓迪齊名為年少回述不作此刻尚活；JS至李勢時卒無精年，之前郭文15日卒非龔壯。',8495:'唐輝CBDB17862僅轉祖唐栗17851譜表、無生卒官，不把祖先資料或0填本人，待原表。',8496:'宋桶索紞解夢兩杖赤衣三日食肉，本人無死生年；75卒索紞非宋桶，僅晚SL。',8497:'寇覓石虎父石背邪子四子虎第四；6/7歲、17/18歲及311送葛陂都是兒子石虎，不倒推父生卒，僅晚SL。',8498:'庾統長仁赤玉庾懌子衛將軍為父追贈，JS29歲卒無CE；父50歲鸩死不是統，SS父字擇OCR異寫依同長仁尋陽官橋同人，不推342生卒。',8499:'張世度16歲京師大疫葬族人十餘，無精確疫年不得猜特定瘟疫反推生年，晚SL無生卒。'}.items():results[ident]['unresolved'].append(note)
    round29a_reads={8501:[('js_15','从弟从事中郎宪切谏')],8502:[('sl_09','字洪茂炖煌人')],8503:[('js_15','以兄珪在𬘭中为疑')],8504:[('sl_03','说中常侍彧弘'),('tj_16','说中常侍彧弘')],8505:[('sl_02','夜袭皝子遵于兴国城'),('tj_15','夜袭皝子遵于兴国城')],8506:[('yi_00','卢孙什翼鞬勇壮')],8507:[('js_18','势使其叔父福'),('sl_03','颕川太守李福战死'),('sl_09','迁势及叔父福等十余人于建康'),('tj_16','李福进攻彭模'),('tj_18','颍川太守李福战死')],8508:[('js_18','周𫖮母李氏，字络秀'),('sl_09','寿纳李凤女生势及广'),('sl_09','母李氏为太后妻李氏为皇后'),('tj_16','立妻李氏为皇后'),('js_09','又道子既为皇太妃所爱'),('sg_07','李贵人生赞哀王恊'),('ssxy_02','有女名络秀'),('ssxy_02','充妻李氏名婉字淑文')],8509:[('js_13','夔安、李菟等五万人'),('js_17','为石遵将李菟所败'),('sl_02','李莵五将军'),('sl_03','邺女子李莵知而告之'),('tj_16','张貉、李菟等五将军'),('tj_18','邺女子李菟知而告之')]}
    for ident,qs in round29a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第29批A張慕容及李氏源支逐人原文審讀','Actual candidate role checked against book paragraph; generic female surnames and male/female homonyms separated') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round29a')
    results[8505]['attested_activity_years']=[337]
    results[8506]['candidate_duplicate_of']=10987
    results[8507].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='chenghan_lishi_uncle_and_jin_yingchuan_governor_unbridged',component_identities=[dict(identity='成漢李勢叔父福，347彭模敗降後送建康',keys=[k for k in results[8507]['keys'] if not k.startswith(('sl:sl_03:','tj:tj_18:'))],historical_birth=None,historical_death=None),dict(identity='晉潁川太守李福364敗燕戰死，無家系橋可判是否前人歸晉後',keys=[k for k in results[8507]['keys'] if k.startswith(('sl:sl_03:','tj:tj_18:'))],historical_birth=None,historical_death=None)])
    ev=chunk_ev('tj_18','颍川太守李福战死','資治通鑑興寧二364夏四月甲辰','僅晉潁川守分支本人死亡；SL寫二月不同本，不能套到未橋李勢叔父')
    results[8507]['component_identities'][1]['historical_death']=date(364,ev,month=4,day_ganzhi='甲辰')
    results[8507]['component_identities'][1]['historical_death'].update(precision='lunar_day_ganzhi',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8507]['evidence'].append(ev)
    groups8508=[('周浚妻妾李絡秀、周顗周嵩周謨母',['js:js_18:65:李絡秀','ssxy:ssxy_02:301:李絡秀']),('李鳳女李壽妾、李勢李廣生母',['sl:sl_09:448:李氏']),('李勢妻李皇后',['tj:tj_16:180:李氏','sl:sl_09:450:李氏']),('簡文李夫人、孝武道子母皇太妃',['js:js_09:116:李氏']),('曹丕李貴人、曹協母',['sg:sg_07:94:李氏']),('李豐女李婉淑文賈充前妻',['ssxy:ssxy_02:295:李婉']),('CBDB135462裸名李絡秀未親屬官橋',['cbdb:135462'])]
    results[8508].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='six_distinct_li_women_plus_bare_cbdb_mixed',component_identities=[dict(identity=nm,keys=ks,historical_birth=None,historical_death=None) for nm,ks in groups8508])
    results[8508]['evidence'].append(dict(source='CBDB原庫135462',path='out/person_chronology/original_source_evidence.jsonl',excerpt='c_name_chn=李絡秀; index/birth/death/notes=null; offices=[]',interpretation='只裸名無族屬字官，不算自動橋至周顗母'))
    results[8509].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='shizhao_male_general_and_ye_female_tomb_informant_mixed',component_identities=[dict(identity='趙將李菟339邾城、349代陂敗徐龕',keys=[k for k in results[8509]['keys'] if not k.startswith(('sl:sl_03:','tj:tj_18:'))],historical_birth=None,historical_death=None),dict(identity='鄴女子李菟告石虎屍所在，秦滅燕王猛誅',keys=[k for k in results[8509]['keys'] if k.startswith(('sl:sl_03:','tj:tj_18:'))],historical_birth=None,historical_death=None,source_reported_death_context='及秦滅燕王猛為之誅李菟，此為359章內後事追述，不能說359已死')])
    for ident,note in {8501:'張天錫從弟張憲從事中郎諫寵梁景劉肅，原文省姓而候選範圍清楚，不合其他張憲同名；六年與太元376是相鄰事件非本人死亡，無生卒。',8502:'張斌洪茂敦煌作蒲卜酒賦、舉孝廉夢未字停，一作賦文是晚本異字非卒年；無生卒。',8503:'張珪張瓘兄在衛綝軍中雙方因兄弟遲疑經年，未說珪死，不把討瓘/馬基田旋死套珪。',8504:'中常侍彧弘受劉翔說為慕容皝燕王冊命入言兩key同，乙卯冊命使是郭悕，不混兩人；無生卒。',8505:'皝子慕容遵337六月興國城拒段屈雲夜襲兩key同人；斬屈雲是SL晚本非遵死，TJ明拒戰可在世337，無生卒。',8506:'宋書夷蛮記盧孫什翼鞬陰山聚眾、苻堅破執長安聽北歸、死開字涉珪代立。與10987昭成王同名君主鏈候選，但父子/北歸死法異於北魏系統，保原說不套他源卒年或刪除未全key目標。',8507:'三key李勢叔父福347敗降送建康與兩key晉潁川守364戰死未家系遷官橋，後者可能前者仕晉而未能確認，故hold不是僅不同時代便斷言異人。頂層無生卒；364四月甲辰只晉守分支，SL二月異次保留。',8508:'六女性明混：絡秀、李鳳女、李勢妻、簡文李夫人、曹丕李貴人、賈充李婉，另CBDB裸名待橋。SS註周氏譜批絡秀妾說應妻，保存妻妾來源異說；95歲是前虞潭母非絡秀。任何女性的生卒均不互套。',8509:'後趙339/349男將李菟與鄴城告石虎墓女子李菟明性別角色不同，女子誅見及秦滅燕後事回述不能誤當359章本人已死；47卒褚裒非將軍，頂層日期隔離。'}.items():results[ident]['unresolved'].append(note)
    round29b_reads={8510:[('js_00','光禄勋留擢')],8511:[('js_17','祈嘉，字孔宾')],8512:[('sl_09','童巽字子举')],8513:[('sl_09','索丞字伯夷')],8514:[('sl_01','祖曰背邪')],8515:[('sl_03','腾为淮阳公'),('sl_04','汝南公腾谋反伏诛'),('tj_17','腾为淮阳公'),('tj_18','秦汝南公腾谋反，伏诛')],8516:[('js_00','尚书令诸葛恢'),('js_17','尚书令诸葛恢奏绩名望犹轻'),('sl_03','尚书诸葛恢'),('tj_16','尚书诸葛恢'),('sg_09','靓子恢字道眀'),('ssxy_01','恢字道明琅邪阳都人祖诞'),('ssxy_02','诸葛道明女为庾儿妇'),('ssxy_03','诸葛令王丞相共争姓族'),('js_12','诸葛恢，字道明')],8517:[('js_00','尚书谢广'),('sl_03','中护军谢广'),('tj_16','中护军谢广')],8518:[('js_15','叛将赵贞')],8519:[('sl_03','兼大鸿胪郭悕[一作希]')],8520:[('tj_16','兼大鸿胪郭烯')],8521:[('js_00','散骑常侍邓逸'),('js_13','岳弟逸，字茂山')],8522:[('js_17','惟与阴颙、齐好友善')],8523:[('js_17','惟与阴颙、齐好友善')],8524:[('js_10','顾景、丁琛、傅晞')],8525:[('yl_01','母丘氏封安阳县君')]}
    for ident,qs in round29b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第29批B帝后配祀與人物原傳逐人核讀','Direct source paragraph reviewed; same-name office roles separated where identity bridge remains absent') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round29b')
    results[8516]['source_reported_age_at_death']={'age':62,'calendar_year':None,'source':'晉書諸葛恢傳卒年六十二，該句沒有確CE死亡年'}
    results[8516]['evidence'].append(chunk_ev('js_12','卒，年六十二','晉書諸葛恢傳','本人62虛歲可保，但無精確卒年，不能憑現代常見345反推284'))
    results[8516]['attested_activity_years']=[342]
    results[8516]['evidence'].append(chunk_ev('tj_16','尚书令诸葛恢并受顾命','資治通鑑咸康八342六月壬辰成帝顧命','本人在世活動，不是成帝卒年當本人卒'))
    results[8520].update(review_status='duplicate',duplicate_of=8519,identity_status='guoxi_same_dahonglu_envoy_to_muronghuang_variant_fire_radical')
    results[8519]['evidence'].append(chunk_ev('tj_16','兼大鸿胪郭烯','資治通鑑咸康七341冊燕使','與SL郭悕一作希同兼大鴻臚持節棘城冊皝並劉翔北還唯一使命，烯悕希寫法異同人'))
    results[8519]['attested_activity_years']=[341]
    for ident,groups in {8517:[('JS00配楊后祀尚書謝廣',['js:js_00:57:謝廣']),('SL/TJ劉翔北還中護軍謝廣',['sl:sl_03:11:謝廣','tj:tj_16:280:謝廣'])],8521:[('JS00配祀散騎常侍鄧逸',['js:js_00:60:鄧逸']),('JS13鄧嶽弟逸字茂山後代廣州',['js:js_13:219:鄧逸'])]}.items():
        results[ident].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='same_jin_official_name_and_generation_but_office_genealogy_bridge_incomplete',component_identities=[dict(identity=nm,keys=ks,historical_birth=None,historical_death=None) for nm,ks in groups])
    ev=chunk_ev('tj_18','秦汝南公腾谋反，伏诛','資治通鑑興寧二364八月敘次','文本明騰為苻生弟；351名單淮陽騰與汝南桐亦均同父兄，故父兄不能排除桐誤騰，不補改封年')
    results[8515].update(review_status='needs_further_research',historical_death=None,source_identities_all_adjudicated=False,identity_status='huaiyang_teng_vs_runan_teng_or_tong_uncertain_name_title_combination',component_identities=[dict(identity='351苻健子淮陽公騰，名單另有汝南公桐',keys=[k for k in results[8515]['keys'] if k.startswith(('sl:sl_03:','tj:tj_17:'))],historical_birth=None,historical_death=None),dict(identity='364稱苻生弟汝南公騰，可能改封騰亦可能桐傳字異',keys=[k for k in results[8515]['keys'] if k.startswith(('sl:sl_04:','tj:tj_18:'))],historical_birth=None,historical_death=date(364,ev))])
    results[8515]['evidence'].append(ev)
    results[8515]['unresolved'].append('351同一名單明騰淮陽、桐汝南兩位均苻健子，故364父兄相同並不足排除桐傳寫騰；未取得改封原文前撤頂層卒364，只留被稱汝南公騰分支，整條hold。')
    for ident,note in {8510:'光祿勳留擢成帝咸康配楊后祀議者，留為姓非劉，不把文末武帝左芬泰始八272誤當留擢生年；無生卒。',8511:'祈嘉字孔賓酒泉、20餘歲隱去敦煌、重華征儒林祭酒天錫尊先生壽終，20餘無對應CE，不以張祚時宋纖80歲套本人，無生卒。',8512:'童巽子舉太守京兆諒門下送其喪護柩獲釋，卒官是諒不是巽；叩頭破血不是死亡，僅SL無生卒。',8513:'索丞伯夷善筝悲歌稱雍門調，玄居先生是前段索襲，後索紞75卒非本人，僅SL無生卒。',8514:'背邪石虎祖、寇覓父，虎17/18歲及311葛陂非祖活動；不可把兒孫年代逆推出祖生卒，僅SL。',8515:'四key前後同父兄但351名單騰淮陽與桐汝南並列，364卻稱汝南公騰；改封或桐誤騰未定，不只靠父兄同就合，頂層無生卒，364僅稱汝南公騰者源支。',8516:'八原key諸葛恢道明陽都靚子、尚書令及庾會羊楷江虨岳父同官家系，SG道眀異字。額外JS12本傳62卒原未載精年，現再親讀全本穆帝紀永和元345五月本人卒，故284為虛歲推生；TJ342顧命仍僅本人活動。',8517:'尚書謝廣配祀與同代中護軍謝廣劉翔饯别可能同人，SL/TJ中護軍分支已唯一事件閉合，與JS尚書支未見字父遷官橋，先hold無生卒不斷異人。',8518:'前涼叛將趙貞敗李柏，殺王恢是張駿援漢武帝典故非趙貞被誅，不把減死李柏套趙，無生卒。',8519:'兼大鴻臚郭悕一作希與TJ郭烯341冊燕同使命同伴同棘城唯一可合8520；無生卒。',8520:'郭烯同SL郭悕/希兼大鴻臚持節冊皝同劉翔北還341同人，重定向8519；非焉耆王郭熙等近名硬合，無生卒。',8521:'散騎常侍鄧逸配楊后祀與鄧嶽弟逸茂山監交廣可能同人，同名同朝但缺字父官橋暫hold；岳卒是兄不是逸本人死。',8522:'陰顒宋纖友，張祚冊太子傅和80餘卒為宋纖，不套朋友生卒，原句不明本人官。',8523:'齊好宋纖友，與陰顒為兩人非四字複姓，纖80歲及张祚年不套齊好，無生卒。',8524:'丁琛卞壺父喪辭命表所列高枕在家者，漸冉五載是壺任師佐非琛仕年；無生卒。',8525:'丘氏庾琛妻明穆后文君母，追贈車騎為夫非丘；安陽縣君與從母荀氏永宣君不同人，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round29c_reads={8526:[('sl_01','北单于乙回')],8527:[('sz_01','随舅令狐氏游学许洛')],8528:[('ssxy_00','任让在帝前戮侍中钟雅')],8529:[('ssxy_00','任名颙时官在都')],8530:[('js_17','放继充')],8531:[('js_18','南康人何钦')],8532:[('js_12','二何佞于佛'),('js_17','何准，字幼道')],8533:[('gs_01','庐江何默申明开难')],8534:[('gs_01','寺主僧珍尝往夏口')],8536:[('js_14','南郡刘尚公同志友善')],8537:[('sl_04','京兆刘湛学明于图记')],8538:[('js_14','与南阳刘𬴊之、南郡刘尚公'),('ssxy_02','南阳刘𬴊之髙率善史传'),('ssxy_03','刘𬴊之一字遗民')],8539:[('yl_07','衡阳区纸者，甚有巧思')],8540:[('js_18','选尚南康长公主'),('ssxy_02','温尚明帝女南康长公主')],8541:[('js_01','恭王俊字道度')],8542:[('ssxy_03','恢尚庐陵长公主名南弟')],8543:[('js_09','子哀王安国立，未逾年薨')],8544:[('js_01','以其子康为汉中太守')],8545:[('js_02','以西阳王羕子悝为肜嗣')],8546:[('js_01','愔字敬王')],8547:[('js_01','玄嗣立。会庚戌制不得藏户')],8548:[('js_12','彭城王绂上言')],8549:[('js_07','子恭王统立'),('js_14','时汝南王统、江夏公卫崇')],8550:[('js_02','怀王子翘自石氏归国得立')]}
    for ident,qs in round29c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第29批C隱逸僧傳及宗室同卷逐人原文審讀','Candidate subject scope read directly; ages, succession, honorary titles and recalled events checked for correct subject') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round29c')
    results[8532]['source_reported_age_at_death']={'age':47,'calendar_year':None,'source':'晉書何准傳年四十七卒，升平元357僅追贈非卒年'}
    results[8532]['evidence'].append(chunk_ev('js_17','年四十七卒。升平元年，追赠','晉書何准傳','本人47歲但357為追贈；JS12同名何准軍將 first hit 不屬本key之二何信佛人物'))
    results[8535].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb_diaohong_genealogical_relationship_conflict_original_table_unread',last_manual_review_batch='round29c')
    results[8535]['evidence'].append(dict(source='CBDB原庫11932刁弘',path='out/person_chronology/original_source_evidence.jsonl',excerpt="index340type29; birth0; death403; notes=Renming Da cidian has Hong younger brother of Kui11931 whereas genealogical tables his only son; see descendant Yanneng11925 and ancestor Gong11927; offices=[]",interpretation='原庫已閱但403尚未史書支持、逵弟/子明源別衝突；索引340非生年，不作原始史料確證'))
    for ident,note in {8526:'北單于乙回為敵那逐後石虎遣李穆復立，本人未死；338僅SL晚重編不輸獨立活動年，未知生卒。',8527:'令狐氏為慧遠舅游學許洛，13及21歲均外甥慧遠非舅，男性不可看氏誤作女性；無生卒。',8528:'任讓樂安隨蘇峻殺鍾雅劉超，後本段成帝稱殺我侍中者與陸全並斬，未列精CE年不套鄰章人物22歲卒，不猜328/329死。',8529:'任顒語林注臨海在都官與王導揚州賓宴人物，正文省姓任注名顒可閉合，不是另一任讓；無生卒。',8530:'何放何准三子首位放繼充，伯充34655歲卒非放死；本傳繼承無精生卒。',8531:'何欽南康聚眾據險王敦加四品將軍，敦後逆誅非本人死，無生卒。',8532:'本key何准是何充弟第五信佛二何，並非JS12首見朱燾同戰何准武將。附原傳幼道穆章后父47卒、357追贈不能卒357推生311，生卒確年未知。',8533:'何默廬江申明竺法開即色空論難，遠/開僧行年死亡非本人，無生卒。',8534:'僧珍武昌寒溪寺主夢火護像是陶侃時故事，被後慧遠建寺請像回述，不能說僧珍在慧遠建寺當時仍活；無生卒。',8535:'CBDB刁弘403待原表校，notes逵弟/逵子不同源關係明衝突；未任意選譜表或用index340生日，hold。',8536:'劉尚公南郡隱士與南陽劉驎之友邓粲起桓冲别驾，邓粲病足與卒不套尚公，無生卒。',8537:'劉湛京兆張雕師教圖記，建武四338是張雕從師非劉湛出生，後雕被誅及追贈不是師死；單SL不作活動確證。',8538:'三key南陽劉驎之子驥遺民陽岐高士同桓沖征長史/張玄寄魚事，非廬山劉程之遺民近號可亂合。SS原字子私碼保，無本人精生卒，不把別人的病卒套己。',8539:'區紙衡陽巧匠中宗補尚方左校，區為姓紙為名按原文不改名；木婦鼠市故事非生卒，不猜中宗寬年代生日。',8540:'兩key明帝女南康長公主桓溫妻同人；SS正文李勢妹與妒記注李勢女源別不混成公主女，父明帝與夫溫死年不套公主，無生卒。',8541:'司馬俊道度紘子出嗣高密王略散騎常侍，咸康八342死是父紘非俊，無生卒。',8542:'司馬南弟原世說晉陽秋注盧陵長公主、正文劉尚晉明帝女，恢字形異真長同詩故事；先保原文父屬，不由現代記憶改父，無生卒。',8543:'安國裒子嗣琅邪未逾年薨，裒建武元317十八死非安國享年/精死年；未逾年界無明哪CE不強寫317或318，無生卒。',8544:'司馬康司馬勳子桓溫任漢中守，後與勳並誅息陇子未明是否康，不能把366被誅自動套康，無生卒。',8545:'司馬悝西陽羕子元帝時嗣梁肜早薨，傳指元帝朝非個人生卒紀年，後翹/逢/珍之被害與己異，無精生卒。',8546:'司馬愔敬王無忌少子廣晉伯早卒，350薨為父無忌，哥哥恬390薨及嗣允之402被害不套愔；無生卒。',8547:'司馬玄紘子彭城王藏五戶被桓溫奏宥，庚戌制是法令制定非本人生死；父342死及子孫薨不套本人，無精年。',8548:'司馬紱原簡文作绂彭城王議樂賢堂佛像，可能紘傳字但無字父橋不合宗室近字；所頌先帝逝不是本人死，無生卒。',8549:'兩key汝南王統祐子恭王與顧和劾其為庶母三年服同唯一王爵名，咸和元326死是父祐，後義熙遵之伏誅非統，無生卒。',8550:'司馬翹梁懷禧子自石氏歸立聲王散騎常侍薨無子，父沒311不作本人卒年、後王珍之被害異人，無生卒。'}.items():results[ident]['unresolved'].append(note)
    diaoev=evidence(SOURCE/'_cache/texts/宋书.txt','毅既至，高祖命诛弘','宋書卷一武帝紀上元興三404二月京口舉義','本次實讀前後元興元年→二年→三年二月上下文、修司馬刁弘具名，非前句桓修弟桓弘；404本人被誅，CBDB11932族系/官職未橋仍只源支')
    results[8535]['evidence'].extend([evidence(SOURCE/'_cache/texts/宋书.txt','修司马刁弘率文武佐吏来赴','宋書卷一武帝紀上具名官身份','與被誅弘同段指代明，非僅同名抽取'),diaoev])
    results[8535]['component_identities']=[dict(identity='CBDB11932刁弘逵弟/逵子不同源、裸庫卒403待核',keys=['cbdb:11932'],historical_birth=None,historical_death=None),dict(identity='宋書元興三404桓修司馬刁弘被劉裕誅',keys=[],candidate_source_record='cbdb:11932',historical_birth=None,historical_death=date(404,diaoev),identity_bridge_to_cbdb_unresolved=True)]
    results[8535]['unresolved'].append('4000片提示後本次已親讀宋書卷一元興三404二月舉義：桓修司馬刁弘赴城被劉裕誅，404支獨立原文確證。但原庫本人無官族橋，403與404不擇一灌頂層，待身份/原譜閉合。')
    round30a_reads={8551:[('yl_01','南康宣公主兴男')],8552:[('js_01','及息陇子、长史梁惮')],8553:[('js_17','吴猛，豫章人也')],8554:[('js_05','吴郡吴震并学行清修')],8555:[('js_12','矫从兄球诣州诉冤')],8556:[('js_12','赦后杀郡人周矫')],8557:[('ssxy_03','周闵女名马头')],8558:[('sl_08','骏遣其将和𬴊谢艾')],8559:[('yl_01','元帝封孔亭为崇圣侯')],8560:[('sz_02','王导、孔敷并通风契'),('gs_01','王导孔敷并承风敬友')],8561:[('js_18','母孔氏卒，上疏解职')],8562:[('js_17','孟陋，字少孤'),('ssxy_02','处士名陋字少孤')],8563:[('js_15','孙晷，字文度'),('js_15','时年饥谷贵，人有生刈其稻者')],8564:[('yl_01','常侍孙朝，八年告老')],8565:[('yl_05','孙略字文度'),('yl_08','孙略，字文度')],8566:[('ssxy_03','太原孙绰女字阿恒')],8567:[('ssxy_02','宋祎曽为王大将军妾')],8568:[('ssxy_02','时人欲题目髙坐而未能')],8569:[('ssxy_00','和尚胡名尸黎密')],8570:[('js_05','琅邪太妃山氏，聃之甥也'),('js_09','及妃山氏薨，祔葬')],8571:[('js_15','以从孙翰袭封')],8572:[('js_09','有善相者师圭谓侃')],8573:[('gs_00','尝遇康昕')],8574:[('sl_02','有康泓者昔在北间')],8575:[('gs_00','康法识亦有义学之功')]}
    for ident,qs in round30a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第30批A孝友宗室與名僧源文逐人核讀','Directly read subject paragraphs; life spans in miracle traditions, successor deaths and other persons ages not transferred') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round30a')
    results[8551].update(review_status='duplicate',duplicate_of=8540,identity_status='xingnan_mingdi_daughter_huanwen_wife_nankang_princess_same')
    results[8540]['evidence'].append(chunk_ev('yl_01','南康宣公主兴男','太平御覽引晉中興書公主名號父母夫','與已核JS/SS南康長公主明帝女桓溫妻同唯一婚姻；YL明興男母庾后，可吸8551'))
    results[8565].update(review_status='duplicate',duplicate_of=8563,identity_status='sunlue_sungui_variant_same_wendu_wu_yuyu_daughter_wife_harvest_story')
    results[8563]['evidence'].extend([chunk_ev('yl_05','孙略字文度','太平御覽引晉中興書孫略','JS孫晷同字文度吳人、避盜刈稻再送稻、妻虞預女四重橋，不僅近字同名合'),chunk_ev('yl_08','孙略，字文度','太平御覽百谷部引晉中興書','同飢年谷貴避盜割稻唯一故事')])
    results[8563]['source_reported_age_at_death']={'age':38,'calendar_year':None,'source':'晉書孫晷傳會卒時年三十八'}
    results[8563]['evidence'].append(chunk_ev('js_15','会卒，时年三十八','晉書孫晷傳','本人38無CE精年；何充稱司空為慣用追官不推孫晷在346以後仍活'))
    results[8569].update(review_status='duplicate',duplicate_of=8568,identity_status='shilimi_gaozuo_western_monk_same_peer_group_variant_transcription')
    results[8568]['evidence'].append(chunk_ev('ssxy_00','和尚胡名尸黎密','世說新語高坐別傳及塔寺記注','與SS02高坐傳尸利宻同高坐、周顗庾亮桓彝群友西域高僧；塔寺記元帝建墓敘次與同篇周侯死後作祝有時限疑，不據它硬定卒年'))
    for ident,note in {8551:'YL明帝長女庾后生南康宣公主興男、先遂安縣主嫁桓溫，與8540兩key明帝女桓溫妻唯一身份閉合可合；不合新安道福等相鄰公主。',8552:'陇子司馬勳息與父長史梁惮等送桓溫並斬，本JS段無精CE，現另以TJ366同送溫誅案明錨補366；同篇另一子康是否陇子未字號橋不合且不套康死亡。',8553:'吳猛豫章孝子40得神方、庾亮疾時旬日死未敛失尸為道術敘述，百餘歲卒是前段鮑靚非猛；無精生卒，不由庾亮340死自動同年。',8554:'吳震吳郡與朱鳳學行清修老未調由華譚薦著作佐郎，不是吳隱之子等同姓擅合；華譚病卒非震，無生卒。',8555:'周球周矯從兄訴赦後殺矯、殷浩收謝毅王彪之不受，球為獄主不表示球處死，無生卒。',8556:'周矯永嘉郡人被謝毅赦後殺，未明赦紀年無精卒年；球從兄活著申冤不是同人，無出生。',8557:'周馬頭汝南周閔女郗超妻拒兄弟迎還，生同室死同穴是誓言非死年，無生卒。',8558:'和驎源私字𬴊張駿將同謝艾討南羌于闐，咸康八342單SL晚重編不供獨立活動修正；無生卒。',8559:'孔亭元帝封崇聖侯不食邑，本段無本人父系世數，不憑孔子後裔通說補生卒；元帝朝寬年不當生日。',8560:'兩key孔敷同王導敬友竺法義/法深徒講法華、始寧保山僧唯一鏈，38074卒是法義非孔敷；不拿後人回憶推孔敷活到380，無生卒。',8561:'孔氏桓溫母臨賀敬太夫人卒詔葬宛陵，本段無確CE；追贈及旬月使者八至非月曆卒日，無生卒精年。',8562:'孟陋少孤武昌孟宗後孟嘉弟兩key同宗、桓溫簡文辟不就；萬年可死為戲語且兄孟嘉假報病不是任何人確死，無生卒。',8563:'孫晷文度吳富春孫秀曾孫虞預女夫38歲卒無CE；YL孫略同字同妻同割稻送盜故事四橋合8565，絕不取何充追贈司空官號為活年下界。',8564:'孫朝員外常侍告老給秩；YL所引同句咸康八342王導奏與導339已卒矛盾，八年告老也不可擅解80歲；保源句待版本，不供342活動與推生日。',8565:'兩YL孫略文度吳人、妻虞預女及飢年盜刈稻避後送稻同JS孫晷，確同人可合8563，不套鄰文陶淡15歲與霍原死。',8566:'孫阿恒孫綽女王氏阿智妻（本段註字形作王防之、字文將，未另改定丈夫姓名），顽嚚與詐婚故事無歲年；范汪亡兒是下一故事非她，無生卒。',8567:'宋禕王敦妾後屬謝尚答田舍貴人，源注未詳但正文角色明確；不合男性宋祎官員，無生卒。',8568:'高坐尸利宻同尸黎密西域王子讓弟、王導周顗庾亮桓彝同友，音譯異寫可吸8569；父王國未知不得補國家或生卒。',8569:'高坐別傳尸黎密與SS02高坐尸利宻同高僧可合8568；周侯死後胡祝回述及塔寺記元帝立墓寺敘次需另考，不硬定卒323/343。',8570:'琅邪孝王裒山妃與羊聃甥琅邪太妃撫成帝同王府后妃身份，請命未說本人死；裒317十八死、羊聃後旬日死非山妃，不套。',8571:'嵇翰嵇紹從孫嗣爵成帝奉朝請後還本宗，太元以孫曠嗣不是翰當年活著/死年，與下文含君道不同人，無生卒。',8572:'師圭善相陶侃左手成公字，師為姓圭名原文清楚；授八州都督及334死陶侃非圭，無生卒。',8573:'康昕自謂草法過康法識兩人各仿右軍，24歲講經是法友、25出家是後支遁非昕；無生卒。',8574:'康泓北間先聞單道開故事後從役南海親見撰傳讚，363袁宏訪屍為後事非康泓本人當時仍活；單道開百餘歲不套泓，單SL無生卒。',8575:'康法識竺潛弟子義學草隸寫經、與康昕二人比書同段不同人；24法友25支遁年齡不套識，無生卒。'}.items():results[ident]['unresolved'].append(note)
    results[9108]['evidence'].append(chunk_ev('sl_06','温司马许逊曰','十六國春秋姚襄錄桓溫伊水役','按原候選溫司馬scope親讀太玄西北卜戰果段；未見字敬之父肅旌陽令橋，不和道教許遜Q1428729只同名合，也不預斷兩角色一定異人'))
    results[9108].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='single_source_huanwen_sima_military_prognosticator_scope_adjudicated',last_manual_review_batch='round30a_bonus')
    results[9108]['unresolved'].append('單SL溫司馬許遜預言姚襄敗走洛陽北山，年代在晚重編356伊水役；生卒無明文且不把姚襄死亡傳言套許遜。根12162道教許遜是否同人待字父官橋，不憑同名或角色差異肯定合/分。')
    round30b_reads={8576:[('ssxy_03','长子宣娶宣武弟桓豁之女')],8577:[('ssxy_00','庾法畅人物论')],8578:[('js_14','庾翼子爰客')],8579:[('js_11','中子袭尝贷官绢十匹')],8580:[('js_12','张凭，字长宗'),('ssxy_01','张凭举孝廉出都'),('ssxy_03','张苍梧是张凭之祖')],8581:[('yi_02','捕鱼人张系世')],8582:[('sz_02','又名昙戒，本姓卓'),('gs_01','释昙戒。一名慧精')],8583:[('js_17','乃更引其兄述')],8584:[('js_11','石季龙汝南太守戴开'),('sl_02','虎汝南太守戴开'),('tj_16','赵汝南太守戴开')],8585:[('js_00','帝令卜者扈谦筮之')],8586:[('sk_01','<近肖古王>，<比流王>第二子也'),('sk_01','<丽>王<&斯由{钊}>，力战拒之，中流矢死'),('sk_01','三十年，秋七月，<高句丽>来攻北鄙')],8587:[('ssxy_03','支愍度才鉴清出')],8588:[('gs_01','竺法兴支法渊于法道')],8589:[('sl_01','鲜卑敌那所逐')],8590:[('sl_01','撃朔方鲜卑斛摩头'),('tj_16','击朔方鲜卑斛摩头')],8591:[('gs_01','于法兰。高阳人')],8592:[('sz_02','后闻于法道讲放光经'),('gs_01','竺法兴支法渊于法道')],8593:[('gs_01','于道邃。炖煌人')],8594:[('sz_00','为明伯连所虏')],8595:[('sz_00','明感，本姓朱')],8596:[('gs_01','乃与弟子昙一昙二')],8597:[('gs_01','乃与弟子昙一昙二')],8598:[('js_12','秣陵令曲安远补句容令')],8599:[('yl_01','元帝诏封魏后曹励')],8600:[('sl_04','毂遣弟活拒战'),('tj_18','斩毂弟活')]}
    for ident,qs in round30b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第30批B家系與晉僧及百濟原文逐人審讀','Actual subject paragraphs read; regnal dates, age data, index years and retrospective narration kept distinct') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round30b')
    results[8584]['attested_activity_years']=[343]
    results[8590]['attested_activity_years']=[338]
    results[8582]['source_reported_age_at_death']={'age':70,'calendar_year':None,'source':'高僧傳曇戒及名僧傳抄惠精'}
    results[8582]['source_reported_death_period']={'expression':'隆安中疾病…俄而遷化','reign':'隆安','CE_range':[397,401],'source':'名僧傳抄惠精本人段','not_an_exact_death_year':True}
    results[8582]['evidence'].append(chunk_ev('sz_02','隆安中疾病','名僧傳抄惠精（慧精/曇戒）','本人隆安中疾病後遷化70歲；為397–401寬期不能誤定397或401死亡或反推精生年'))
    results[8593]['source_reported_age_at_death']={'age':31,'calendar_year':None,'source':'高僧傳于道邃交趾遇疾終春秋三十一'}
    results[8593]['evidence'].append(chunk_ev('gs_01','于交趾遇疾而终。春秋三十有一','高僧傳于道邃','本人31歲但無CE卒年，師于法蘭同遊不同人，出生不反推'))
    skdeath=chunk_ev('sk_01','冬十一月，王薨。古记云','三國史記卷24百濟本紀近肖古王三十年','已讀本王全段及後近仇首子嗣段，三十年冬十一月本人卒；CE換算復用本批先前親讀原卷30年表錨，不用抽取JSON當新證')
    sktable=next(e for e in results[7322]['evidence'] if e.get('source_revision_id')==2020967)
    results[8586]['evidence'].extend([skdeath,{**sktable,'reuse_note':'此前同輪親讀卷30原表證據復用：近肖古26與故國原41同辛未晉咸安元371。非新獨立來源、非新標讀日期。'}])
    results[8586].update(review_status='primary_text_corroborated',historical_death=date(375,skdeath,month=11,regnal_year=30,conversion='371 + (30 - 26) = 375; regnal26 synchronized with JinXianan1 in original SK30 table previously read'))
    results[8586]['historical_death'].update(precision='lunar_month_CE_year_from_regnal_synchronism',calendar='Baekje_regnal_lunisolar_year_synchronized_to_Jin_CE_year')
    ev=chunk_ev('tj_18','八月，坚击毂，破之，斩毂弟活','資治通鑑興寧三365八月','原header興寧三365，弟活本人明斬；SL同戰只斬4000非必含弟，死亡據獨立TJ')
    results[8600].update(review_status='primary_text_corroborated',historical_death=date(365,ev,month=8))
    results[8600]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8600]['evidence'].append(ev)
    results[8451]['evidence'].append(chunk_ev('js_12','年三十六，卒官','晉書劉惔真長傳（本次張憑傳前全段重讀）','本人名士劉惔36歲卒，無精CE死亡年，與整條另一軍將key未橋；只保名士分支年齡不反推生日'))
    results[8451]['component_identities'][0]['source_reported_age_at_death']={'age':36,'calendar_year':None,'source':'晉書劉惔傳'}
    for ident,note in {8576:'庾宣庾友弘之長子桓豁女夫，父友免死是妻求伯桓溫命不是宣死，源友字惠彥/弘之注差保，無生卒。',8577:'庾法暢人物論作者注評法深義學，是後世著書引用不當法深講經當年其已任官，未給生卒。',8578:'庾爰客原稱庾翼子與孫盛子放戲父名，小字或本名未在本句分，不能僅父翼就合爰之/方之，無生卒。',8579:'庾襲庾冰中子貸官絹杖責，冰49卒非襲，七子列名同本人可清身份，無生卒。',8580:'三key張憑長宗吳郡蒼梧張鎮孫、劉惔薦太常博士、幼戲祖父同事閉合，前劉惔36歲卒不是憑；憑卒為美士的卒是終成不是死，無精生卒。',8581:'張繫世捕鱼者海口得佛花趺，經一歲承前高悝得像而無精CE，371採珠得光者董宗之非繫世，無生卒。',8582:'SZ正文名惠精又名曇戒姓卓南陽卓潜弟、道安弟子同GS明慧精別名；目錄慧精和正文惠精異寫同寺太后寺同學。GS70無年，SZ隆安397–401患病旋遷化70歲，保寬期不補精生卒。',8583:'戴述逵兄應司馬晞引鼓琴，逵傳所載年歲與病卒均弟非述，不憑後代戴顒死亡套兄，無生卒。',8584:'三key戴開趙汝南守數千降庾翼同官事，TJ343七月明活動，降非死，無生卒。',8585:'扈謙簡文卜後房二貴男，與許邁對答不同人，李陵容生孝武/道子和她死年皆非卜者本人；無生卒。',8586:'近肖古比流第二子繼契、子近仇首、26攻平壤斯由中矢死，三十冬十一月卒。復用前批親讀SK30近肖古26=晉咸安元371錨，375=371+(30−26)；出生不詳，不以346即位當真初仕。根12794GL候選同人待QID原頁核。',8587:'支愍度過江與伧道人權立心無義，年積講義非精年，名德沙門題目/孫綽贊為後引不造活年，無生卒。',8588:'支法淵與竺法興于法道並于法蘭同時、以才華稱，三人不同不可把蘭象林死或法開60歲套淵，無生卒。',8589:'敵那鮮卑逐北單于乙回為李穆擊破，破非明誅；338單晚SL不輸獨立活動，未知生卒。',8590:'斛摩頭朔方鮮卑338石宣敗之斬首四萬兩key同役，四萬泛兵不是主將本人死，明338活而未知卒。',8591:'于法蘭高陽15出家、冠年名起、遊剡欲赴西域象林病死無精CE，法開60歲是弟子道邃31另一弟子不套師，未知生卒。',8592:'兩key于法道善放光義解、慧精19出家前所聽法師同義學角色，19與隆安70卒屬慧精非道，源法蘭法渊同時名士鏈不混弟子；無生卒。',8593:'于道邃敦煌16出家師蘭隨西域交趾病終31歲無CE，法開60與蘭死別人不套邃，無精生卒。',8594:'明伯連是明感到青州復被擄的擄者，原句為明伯連所虜；不是朱明感父親，更不能把名伯連擅改朱伯連。單key人物範圍清楚，無生卒。',8595:'明感本姓朱高平、已婚夫兒贖後出家，題康明感。原永和四348渡江見何充與充346卒不相容，且鄰慧湛建元二344住同寺；保原文時序衝突不擅將348校344，不供活動年，無生卒。',8596:'曇一法汰弟子同曇二沿江、受命論破道恒心無義，法汰疾及後371深敬非其生卒；與道壹不同名不能合，無生卒。',8597:'曇二同法汰四十弟子沿江，與曇一同段明兩人，論難主力曇一與慧遠不能套二；無生卒。',8598:'曲安遠秣陵令簡文欲補句容被王彪之拒，以卜術得進源評論；又GS堂邑守曲安遠詔竺法曠可能後仕同人但本key清楚，未另硬合，無生卒。',8599:'曹勵元帝封魏後陳留王，魏後為魏祀繼嗣非女性后妃；不憑相似曹勱/曹奐/曹勵名字就合，無生卒。',8600:'兩key曹活曹毂弟365同川拒秦，SL只斬4000無明弟死，TJ明八月斬毂弟活足獨立本人生卒證；哥哥後卒不可套弟，未知出生。'}.items():results[ident]['unresolved'].append(note)
    round31a_reads={8601:[('js_05','晋陵朱凤')],8602:[('tj_16','导以江夏李充为丞相掾'),('js_13','孙绰、李充、许询、支遁'),('js_16','李充，字弘度'),('ssxy_00','李充字度江夏郢人')],8603:[('ssxy_02','李𫷷是茂曽第五子')],8604:[('sl_03','普遂降于虎从虎南归死于丧乱'),('tj_16','普遂降赵，从赵军南归，死于丧乱')],8605:[('hy_00','侍中李艳及张烈'),('sl_09','侍中李艳及张烈'),('tj_16','侍中李艳等劝寿自称帝')],8606:[('js_17','杜不愆，庐江人')],8607:[('js_16','毗次子歆，举秀才')],8608:[('js_10','斩贼率杜发')],8609:[('sz_01','便呼林伯升')],8610:[('sl_05','安使呼林百升'),('gs_01','安便呼林百升')],8612:[('js_11','豁字朗子'),('sl_03','黄门郎桓豁'),('sl_04','晋荆州刺史桓豁'),('tj_18','荆州刺史桓豁'),('js_13','征西将军桓豁司马'),('js_16','桓豁表挹为魏兴太守'),('gs_01','征西将军桓朗子'),('ssxy_02','豁字朗子温之弟'),('ssxy_03','宣武弟桓豁女也')],8613:[('js_01','长史梁惮、司马金壹')],8614:[('sl_01','诸大人梁盖'),('tj_16','诸大人梁盖'),('wsh_00','大人梁盖等杀屈')],8615:[('gs_01','求同郡杨德慎女')],8616:[('wsh_05','弟郁兰抚军将军')],8617:[('js_12','母殷氏，高明有行'),('js_16','康伯母，殷浩之姊'),('ssxy_00','康伯母扬州刺史殷浩之妹'),('ssxy_02','母殷夫人自成之'),('ssxy_03','韩康伯母殷随孙绘之之衡阳')],8618:[('js_18','子彪兄弟三人居丧')],8619:[('js_15','济阳江淳少有高操')],8620:[('gs_01','开有弟子法威')],8621:[('js_10','伯息忠诉辞称枉')],8622:[('sl_04','索虏乌延'),('sl_05','索虏乌延')],8623:[('js_17','长子恪，领军将军')],8624:[('sz_00','道师京都所谓王濮阳也')],8625:[('gs_00','琅琊王琝师事于密')]}
    for ident,qs in round31a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第31批A文士母系與軍將同卷原文逐人核讀','All candidate identities compared with actual named roles; nominal age and retrospective death clauses not converted without date anchors') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round31a')
    results[8609].update(review_status='duplicate',duplicate_of=8610,identity_status='lin_bosheng_baisheng_same_daoan_night_host_word_riddle')
    results[8610]['evidence'].append(chunk_ev('sz_01','便呼林伯升','本地SZ01釋道安傳林伯升','與GS/SL兩木林馬具容一斛百升、雷雨夜訪主人名同唯一故事；伯百字異不屬另一新人物'))
    results[8602]['appearance_upper_bound']=338
    results[8602]['appearance_constraint_explanation']='TJ338述李充已為王導掾著學箴；任掾可在此前，故338僅最晚登場上界，不當真正初仕或338仍在世。'
    results[8605]['attested_activity_years']=[338]
    results[8614]['attested_activity_years']=[338]
    results[8612]['attested_activity_years']=[366,367]
    results[8612]['source_reported_age_at_death']={'age':58,'calendar_year':None,'source':'晉書桓豁傳尋卒時年五十八，本段不載確CE卒年'}
    results[8612]['evidence'].append(chunk_ev('js_11','寻卒，时年五十八','晉書桓豁傳','本人58歲未給確年，不能只由現代通說377反推320；兄溫373、弟冲384非本人'))
    results[8603]['source_reported_death_period']={'expression':'永和中卒','reign':'永和','CE_range':[345,356],'source':'世說新語文字志李𫷷注','not_an_exact_death_year':True}
    results[8616]['candidate_duplicate_of']=8309
    results[8616]['unresolved'].append('WS段郁蘭與8309主體段蘭同護遼弟、龕父、趙鮮卑令支鏈；但8309另JS12荀羨傳段蘭/龕角色支未閉整行，不能硬重定向到混合target。')
    results[8611].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb_liuzhuo_tackett_estimate_original_epigraphy_unread',last_manual_review_batch='round31a')
    results[8611]['evidence'].append(dict(source='CBDB原庫194427柳卓',path='out/person_chronology/original_source_evidence.jsonl',excerpt='index340type29; birthnull; death403range2; notes=補遺7:138–139; 文物1999.1:93; 洛陽新獲續504. Death year estimated by Tackett (cc403); office太守firstyear0',interpretation='403明估且帶range2非確卒，index340非生年，太守始年0無值；原墓誌未取得不算已核生卒'))
    duev=chunk_ev('js_10','斩贼率杜发','晉書應詹傳杜發本人死亡','原傳應詹趙胤竹格渡江戰明斬杜發；原句沒有年，另用TJ同役紀年對照，不宣稱TJ也點名杜發')
    dutime=chunk_ev('tj_14','充、凤从竹格渚渡淮','資治通鑑太寧二324竹格役','324年號原header；同竹格渚、應詹趙胤、王敦黨同役，TJ先不利後劉遐蘇峻勝與JS應詹勝歸功敘法不同，僅對照戰役年代')
    results[8608].update(review_status='primary_text_corroborated',historical_death=date(324,duev))
    results[8608]['historical_death'].update(source='晉書應詹傳本人斬；資治通鑑太寧二324同竹格役紀年',precision='year_inferred_from_explicit_same_event_cross_reference',derivation='JS names DuFa killed in YingZhan/ZhaoYin battle at Zhuge; TJ dates matching battle to324. TJ does not name DuFa; victory attribution differs. No death month/day inferred.')
    results[8608]['evidence'].extend([duev,dutime])
    results[8568]['evidence'].append(chunk_ev('gs_00','晋咸康中卒。春秋八十余','高僧傳帛尸梨密（高坐）','本次王琝師事段前親讀：GS咸康335–342卒80餘、成帝樹刹，與SS塔寺記元帝墓寺源敘次不同；不取任一精卒年'))
    results[8568]['source_reported_death_periods']=[dict(expression='晉咸康中卒春秋八十餘',CE_range=[335,342],source='高僧傳帛尸梨密'),dict(expression='元帝於冢邊立寺',implied_death_before_or_during='JinYuandi reign, conflicts with GaosengZhuanXianKang',source='世說所引塔寺記')]
    for ident,note in {8601:'朱鳳晉陵與吳震同被華譚薦著作佐郎，譚卒非鳳，不以同名就合別處朱鳳，無生卒。',8602:'四key李充弘度江夏父矩、王導掾學箴、殷浩助剡縣、東土文士同官文鏈，無精生卒。王羲之蘭亭前列同好不代表名單每個人都353當場，338任掾上界非精初仕。',8603:'李廞源𫷷宗子李重第五子清真躄疾不婚，兄式54卒非廞，永和345–356中卒僅寬期不取某年或反推出生。',8604:'李普李洪弟338棘城勸逃而降趙，後死於喪亂句為後事回述未定哪年，不把338首見或349趙亂自動定卒，無生年。',8605:'三key李艷侍中同蔡興任調張烈勸李壽稱帝338本人明活動，李闳兄弟等他人死不套艷，無生卒。',8606:'杜不愆廬江從外祖郭璞學易、後桓嗣建威參軍，郗超20餘病及40死、80雉卜預言不套本人。未從祖郭璞卒年自動推外孫精出生，無生卒。',8607:'杜歆杜轸孫毗次子舉秀才，父毗叔秀遭害均非歆，無生卒。',8608:'杜發應詹趙胤竹格戰斬本人，原game卒356無支持；JS同役死亡與TJ324同竹格戰跨書紀年，保推定方式與戰功敘法差，不誇原句直書324。',8609:'林伯升GS林百升同道安師徒雷雨夜兩木林馬具容一斛百升宿主人謎字故事，伯百異可合8610；不當歷史生卒證，未知年。',8610:'兩key林百升夜宿主人與SZ林伯升獨一同謎字可吸8609，史傳故事是否真有此人無獨立人物證，不造生卒。',8611:'柳卓CBDB卒403為Tackett估且range2，原墓誌未親讀，保待考不把太守firstyear0填公元零年。',8612:'九key桓豁朗子溫弟、征西荊州司馬謝玄、父石虔、女庾宣妻同官父兄爵字閉合。GS稱桓朗子為字非另人。JS本人58卒無精CE但另有本輪先前親讀全本TJ377七月丙辰卒，現本次重讀保留377與虛歲推320；366/367明本人活動可用。',8613:'梁惮司馬勳長史與勳子陇子金壹同送溫斬，本JS未明年但現與TJ366同送溫並斬案跨參可補366；不把他人梁谦/郭惮同音合，不造精月日，未知出生。',8614:'三key梁蓋代大人殺屈推孤迎什翼犍同一繼位政事，338明本人活動；烈帝卒是翳槐、後斤死長安非梁蓋，無生卒。',8615:'楊德慎竺僧度王晞未婚妻苕華父，同郡東莞，婚前母父庶母次第亡，16歲是王晞及苕華非父，無生卒精年。',8616:'魏書段郁蘭護遼弟、龕父趙令支軍同8309主段蘭，郁蘭非第三人；8309混疑JS青州段蘭/龕支不可整條硬歸，不把魏書壓縮338奔趙當死/與343活動矛盾，無生卒確年。',8617:'五key殷氏皆韓伯康伯母、殷浩姊/妹源差保而不分人，同吳隱之哭感薦與熨斗、衡陽孫繪之事。9旬女另一王羲之郗夫人非殷氏；繪之遭桓亮難時殷在場但本段未紀年另待考，無生卒。',8618:'江彪江播子與二弟居父喪被桓溫刃，源省江姓名scope清；15父喪18復仇為桓溫歲，與別處溫生年有口徑差，不強推331/329死亡；無出生。',8619:'江淳濟陽高操訪孫晷東陽，孫晷38死不是淳死，無生卒。',8620:'法威于法開弟子受師寄載論支遁小品，法開60卒不是威；不是王彪之友傅微同音可合，無生卒。',8621:'淳于忠伯子申冤，建興中血逆流處死者父伯非忠，訴訟兩月四年為父運糧制度不當忠生卒，無精年。',8622:'兩SL烏延同通曹毂劉衛辰叛秦、晚SL04寫擊斬，365確年暫僅晚重編，無獨立早源不填歷史確卒或活動，與漢末烏丸烏延不同役不可亂合。',8623:'王恪遐桓子光祿勳長子領軍、欣之歡之父，父追贈寧康非恪死；8341父整條仍含別軍司待橋，不由兒把父混支全部認同，無生卒。',8624:'王濮陽是京都稱清水道師的稱號而非已證三字本名，簡文為府主時事之立道舍，帝即位道容救烏巢是後事非王道師仍在場；無生卒。',8625:'王琝琅琊師事帛尸梨密作序原姓名確作琝，不憑近形合後來王珉；師咸康卒80餘、成帝塔為師非王琝生卒。身份史料不足之處保未知，無生卒。'}.items():results[ident]['unresolved'].append(note)
    huoev=evidence(SOURCE/'_cache/texts/资治通鉴.txt','丙辰，征西大将军、荆州刺史桓豁卒','資治通鑑全本太元二377秋七月丙辰','本次重讀全本原header與前後月序，舊combined_ages已實讀此證，本批九keys閉合後保留；TJ分塊抽取止370不代表原text止370')
    huoage=chunk_ev('js_11','寻卒，时年五十八','晉書桓豁傳','本人死亡58虛歲；獨立全TJ377死亡錨可推320生，非明生日')
    results[8612].update(review_status='primary_text_corroborated',historical_death=date(377,huoev,month=7,day_ganzhi='丙辰'),historical_birth=date(320,huoage,age=58))
    results[8612]['historical_death'].update(precision='lunar_month_with_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8612]['historical_birth']['death_anchor_source']=huoev['citation']
    results[8612]['source_reported_age_at_death']['calendar_year']=377
    results[8612]['evidence'].extend([huoev,huoage])
    results[8612]['unresolved'].append('舊證保護覆核：只讀TJ18分塊曾誤以377無可取早源；全本TJ仍可取得並本次重讀377七月丙辰本人卒，故保早存377與58虛歲推320。生年仍推算非生日直接記載。')
    xuntime=chunk_ev('tj_18','擒勋及其党，送大司马温；温皆斩之','資治通鑑太和元366五月敘次朱序周楚討司馬勳','只提供這次送溫斬案366年錨；不是說TJ點名陇子或梁惮，兩人具名明斬在JS')
    xunparty=chunk_ev('js_01','及息陇子、长史梁惮、司马金壹等送于温，并斩之','晉書司馬勳傳具名同送溫並斬','司馬勳、息陇子、長史梁惮、司馬金壹逐名均送桓溫並斬，明同次處刑；不等同另一未橋兒子康')
    for ident in [8552,8613]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(366,xunparty))
        results[ident]['historical_death'].update(source='晉書司馬勳傳具名同送並斬；資治通鑑太和元366同案紀年',precision='year_inferred_from_explicit_same_event_cross_reference',derivation='JS explicitly lists named Longzi/LiangDan sent with SimaXun to HuanWen and all executed. TJ dates same named-commanders same-destination execution case to366. No month/day inferred for individual.')
        results[ident]['evidence'].extend([xunparty,xuntime])
        results[ident]['unresolved'].append('跨書同案補核：JS本人具名同司馬勳送溫並斬，TJ366朱序周楚擒勳及黨送溫皆斬同案錨，故366年可確定為跨參推定，非原本人單句直紀。未知生年，不取月日。')
    geev=evidence(SOURCE/'_cache/texts/晋书.txt','尚书令、金紫光禄大夫、建安伯诸葛恢卒','晉書帝紀穆帝永和元345五月','本次親讀完整帝紀原永和元年header至六月：五月戊寅大雩後諸葛恢卒；五月可承月序，不把大雩干支戊寅自動當恢卒日')
    geage=chunk_ev('js_12','卒，年六十二','晉書諸葛恢傳','前批已讀本人62虛歲，現以獨立帝紀345五月死亡錨推284生，非生日直接記載')
    results[8516].update(review_status='primary_text_corroborated',historical_death=date(345,geev,month=5),historical_birth=date(284,geage,age=62))
    results[8516]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8516]['historical_birth']['death_anchor_source']=geev['citation']
    results[8516]['source_reported_age_at_death']['calendar_year']=345
    results[8516]['evidence'].extend([geev,geage])
    results[8516]['unresolved'].append('全本補核已取得帝紀永和元345五月諸葛恢卒，解此前八key局部本傳缺紀年的待定；62虛歲反推345−62+1=284。月份不自動取大雩戊寅為本人死亡日。')
    round31b_reads={8626:[('sl_05','晋叛臣王瑾固守寿春')],8627:[('js_17','遐少子臻，崇德卫尉')],8628:[('ssxy_03','万取太原王述女名荃')],8629:[('ssxy_03','阿智王防之小字')],8630:[('sl_04','新平王雕陈说图谶')],8631:[('gs_01','竺僧度。姓王。名晞')],8632:[('gs_01','竺僧敷。未详氏族')],8633:[('gs_01','竺僧辅。邺人')],8634:[('gs_01','事沙门竺昙印为师')],8635:[('gs_00','竺法友。志业强正')],8636:[('gs_00','竺法济幼有才藻'),('gs_01','太阳竺法济并州支昙')],8637:[('gs_01','竺法兴支法渊于法道')],8638:[('gs_00','竺法蕴悟解入玄')],8639:[('gs_01','时沙门竺道邻造无量寿像')],8640:[('js_06','友人范荣期'),('js_12','子启，字荣期'),('ssxy_01','范启字荣期慎阳人'),('ssxy_03','王文度范荣期俱为简文所要')],8641:[('js_05','疑郡人简良等为贼')],8642:[('js_05','兄子贲尚公主')],8643:[('ssxy_03','婚泰山胡母氏女')],8644:[('js_11','贼帅艾秀遣使归诚')],8645:[('sl_03','幼为赵公'),('sl_04','征北将军淮南公幼'),('sl_05','征北将军淮南公幼'),('tj_17','幼为赵公'),('tj_18','淮南公幼帅杏城之众')],8646:[('js_13','郡人莫鸿，南土豪族')],8647:[('ssxy_02','球字和琳防稽余姚人')],8648:[('ssxy_03','小妹名女正适谢尚')],8649:[('ssxy_03','大妹名女皇适殷浩')],8650:[('sz_01','又授弟子觅历高声梵呗'),('gs_00','又授弟子觅历高声梵呗')]}
    for ident,qs in round31b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第31批B僧友宗室與文字異名原文逐人核讀','Read specific named source passages, separating shared monk names, courtesy names and family members ages') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round31b')
    results[8636].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='faji_qian_pupil_author_and_taiyang_yinchiru_teacher_daoan_unbridged',component_identities=[dict(identity='竺潛神足竺法濟、高逸沙門傳作者',keys=['gs:gs_00:109:竺法濟'],historical_birth=None,historical_death=None),dict(identity='太陽竺法濟陰持入經講師道安受業者',keys=['gs:gs_01:28:竺法濟'],historical_birth=None,historical_death=None)])
    results[8632]['source_reported_age_at_death']={'expression':'春秋七十餘','age_gt':70,'calendar_year':None,'source':'高僧傳竺僧敷'}
    results[8633]['source_reported_age_at_death']={'age':60,'calendar_year':None,'source':'高僧傳竺僧輔'}
    results[8629]['source_name_transcription']='王防之（本地原文）；候選王處之待字形版本校，不另造人物'
    results[8645]['appearance_upper_bound']=351
    results[8645]['appearance_constraint_explanation']='TJ351苻健封子幼趙公，登場最晚上界；不稱初仕或出生年。'
    youev=chunk_ev('tj_18','冬，十月，征北将军、淮南公幼帅杏城之众乘虚袭长安，李威击斩之','資治通鑑興寧三365冬十月苻幼本人被斬','全角色對照五key；先趙公後淮南公爵異保留，不編改封時間；前淮南公苻生已先死非同名幼')
    results[8645].update(review_status='primary_text_corroborated',historical_death=date(365,youev,month=10))
    results[8645]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8645]['evidence'].append(youev)
    jinev=evidence(SOURCE/'_cache/texts/资治通鉴.txt','袁瑾、朱辅求救于秦','資治通鑑咸安元371正月壽春援軍','袁瑾壽春反晉、桓溫圍、王鑒張蚝二萬援、為溫遣將敗、退慎城，與SL05王瑾同一主帥求援役五橋，可校王袁源字異。另8797含未閉CBDB不得整條刪。')
    jindeath=evidence(SOURCE/'_cache/texts/资治通鉴.txt','擒瑾及辅并其宗族，送建康斩之','資治通鑑咸安元371袁瑾本人被送斬','先丁亥陷壽春後送建康斬，丁亥為陷城日非誅死日；只確年371，不取正月作必死月。')
    results[8626].update(review_status='primary_text_corroborated',proposed_name='袁瑾',candidate_duplicate_of=8797,identity_status='sl_wangjin_unique_shouchun_rebel_yuanjin_variant_identified',historical_death=date(371,jindeath))
    results[8626]['historical_death'].update(precision='year_identity_identified_from_unique_siege_cross_reference',identity_basis='SL王瑾same Shouchun rebel requested WangJian/ZhangHao relief20000, HuanWen dispatched night attack, Qin retreatedShencheng; independent TJ names leaderYuanJin')
    results[8626]['candidate_duplicate_scope']='8797史書主支袁瑾，該條另CBDB423586未全身份閉合；不先整條重定向'
    results[8626]['evidence'].extend([jinev,jindeath])
    results[7697]['evidence'].append(chunk_ev('js_17','龛执节不挠，为贼所害','晉書褚裒傳徐龕代陂被害（同TJ王龕）','JS稱徐龕同伐沛獲支重、魯郡五百家、褚裒遣三千、代陂敗被害；同TJ/WS王龕唯一事件角色，姓王徐及敵將李農李菟源差保，非322泰山徐龕。'))
    kanev=results[7697]['evidence'][-1]
    results[7697].update(review_status='primary_text_corroborated',historical_death=date(349,kanev))
    results[7697]['historical_death'].update(precision='year_name_variant_identified_and_event_cross_reference',death_year_anchor='TJ永和五349代陂王龕李邁役；JS本人徐龕明被害，非單SL斷死')
    results[7697]['source_name_variants']=['王龕（TJ/WS/SL）','徐龕（JS褚裒傳同代陂役，異於322泰山徐龕）']
    results[7697]['unresolved'].append('本批4000片6190提示後，本人本輪已實讀JS17同代陂徐龕明被害，配TJ349年同一支重/五百家/三千人役可補早源死亡349；舊SL349仍保晚重編層。6190原整條混322徐龕，不整合。')
    for ident,note in {8626:'SL05王瑾壽春叛臣，與全TJ371袁瑾主帥同王鑒張蚝二萬救援、桓溫將夜襲、退慎城五橋，同人姓字差可提名校袁瑾。371送斬只年，不以丁亥陷城日當死日。8797整條附CBDB待閉只候選，不硬刪。',8627:'王臻王遐少子崇德衛尉，父寧康追贈不是本人死亡，無生卒。',8628:'王荃王述女謝萬妻，非男性王荟近字，正文謝萬肩舆怒父是丈夫與父事，無本人日期。',8629:'王處之本原注王防之字文將阿智，王述子王坦之弟孫阿恒夫角色鏈清楚，候選處/原防字形待校；不因異字另造一人，無生卒。',8630:'王雕新平太史令陳圖讖被王猛勸堅誅，後新平獻玉器追贈回述不證本人382活；建武四從京兆劉湛學是338從師非生日，僅SL不供確年生卒。',8631:'竺僧度王姓名晞字玄宗東莞，16定婚同齡楊苕華後出家著毗曇旨歸，原明後不知所終；年16非卒歲或生日，不合北齊王晞同名。',8632:'竺僧敷西晉末移江左瓦官講放光道行著神無形論，70餘卒無CE，與相鄰僧輔60另人，保年齡但不倒推生日。',8633:'竺僧輔鄴人戒行與道安隱澤後上明，琅琊王字缺碼請戒師，60卒無精年，不硬補王姓刺史名/任期為死年。',8634:'竺曇印竺法曠出家師，病危由曠七日祈禱而差，不是病死，法曠40276卒非師生卒。',8635:'竺法友竺潛弟子一宿誦阿毗曇、24歲講說，24是講經齡非卒齡，無生卒。',8636:'兩竺法濟key一為竺潛神足高逸沙門傳作者，一為太陽陰持入經師道安受業者。可能同人前後事但無籍字師承/書名橋，暫分支hold，不只同名就認同。',8637:'竺法興與法蘭同时並法渊法道三僧各有名才義，是不同人，師友象林死非己，無生卒。',8638:'竺法蘊竺潛神足善放光般若，同段法友24、支遁25等非己，無生卒。',8639:'竺道鄰造無量壽像、法曠為起殿，40276死為法曠非道鄰，未知生卒。',8640:'四key范榮期=范啟字榮期范堅子黃門侍郎，與孫綽天台賦金石評唯一事、王坦之推前及郗超友同人；沒精生卒不把父堅死套子。',8641:'簡良羊聃廬陵疑賊冤殺具名者，200餘泛屠非簡良享年，奏案未明殺死精紀年，不套羊聃後旬日死，無生卒確年。',8642:'羊賁羊聃兄子尚公主因叔罪請解婚不許，叔除名病卒非己；此段公主無名不擅補哪位，無生卒。',8643:'胡毋氏原胡母氏女婚的是樂安高柔世遠、何充冠軍參軍，與三國高柔文惠不同，不合夫名即套263等年。20為婚時年齡非卒齡，無生日卒年。',8644:'艾秀贼帅遣使归庾怿、上洛降500，未明精年，庾怿50鸩卒非秀，無生卒。',8645:'五key苻幼苻健子351趙公、後365淮南公杏城眾反襲長安李威擊斬；爵異保而不造改封年，前淮南苻生早死不作混幼，365十月原TJ本人斬，未知出生。351封爵只登場上界。',8646:'莫鴻豫章豪殺縣令被劉胤誅，胤任守後咸和初之前無本句精年，莫姓非賀/周同音合，無精生卒。',8647:'虞球和琳會稽餘姚父基祖授黃門侍郎，孫綽所目長琳是虞存道長與球和琳合稱，不把本人字改長琳，無生卒。',8648:'袁女正袁耽小妹謝尚妻，與姊女皇殷浩妻同段兩人，不合匿名袁氏他人；夫35750卒非女正，無生卒。',8649:'袁女皇袁耽大妹殷浩妻，與妹女正謝尚妻不同，袁耽戲願更一妹配桓溫非有第三女性事實，無生卒。',8650:'兩key覓歷帛尸梨密授高聲梵唄弟子同師曲目，咸康335–342八十餘卒是師非覓歷；無本人生卒。'}.items():results[ident]['unresolved'].append(note)
    liurowev=dict(citation='CBDB20261017原庫KIN_DATA194427與178277父子',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),tables=['KIN_DATA','KINSHIP_CODES','BIOG_MAIN'],excerpt='194427柳卓 → 子178277柳恬，c_source32033；178277柳恬 → 父194427柳卓；原notes(Tackett)subjectid139462,targetid160396,relationS,pageitemR35215。另子柳輔、柳傑、柳奮。',interpretation='本次只讀原SQLite原始親屬行，不用抽取JSON當新史證；兩方向是同一關係非兩獨立證。用以同正史家系橋身份，403估年不因這個變成實證。')
    liusong=evidence(SOURCE/'_cache/texts/宋书.txt','曾祖卓，自本郡迁于襄阳，官至汝南太守。祖恬，西河太守','宋書柳元景傳柳卓/柳恬家系','親讀柳元景字孝仁河東解人、曾祖卓汝南守、祖恬西河守、父憑馮翊守，與原CBDB柳卓→柳恬、太守角色橋合；傳內未記卓生卒年。')
    results[8611].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='original_cbdb_kin_liuzhuo_liutian_matched_songshu_genealogy_taishou',last_manual_review_batch='round31b_bonus_cbdb')
    results[8611]['evidence'].extend([liurowev,liusong])
    results[8611]['unresolved'].append('補核修正：原先所取BIOG_MAIN沒有KINDATA才未橋身份；現原庫柳卓子柳恬與親讀宋書同父子加太守身份明可閉。403仍Tackett c_dy_range2估、index340type29非生日；未取得所引墓誌則不宣生卒已證。')
    results[8797].setdefault('original_cbdb_source_locators',[]).append(dict(personid=423586,table='BIOG_SOURCE_DATA',sources=[dict(textid=3689,title='紹興府志',pages='section_id=62633'),dict(textid=68942,title='Wikidata',pages='Q65849678')],kin_rows_consulted=0,interpretation='原庫不是無來源，但未取此方志官職段前不只憑書名認同或斷異；sourceflag不變'))
    results[8474].setdefault('original_cbdb_source_locators',[]).append(dict(personid=430718,table='BIOG_SOURCE_DATA',sources=[dict(textid=6267,title='陝西通志',pages='section_id=51478'),dict(textid=66734,title='ctext',pages='8609122'),dict(textid=68942,title='Wikidata',pages='Q65878304')],kin_rows_consulted=0,interpretation='原庫有方志及ctext來源尚未取得原段，不以裸名強連；sourceflag不變'))
    round32a_reads={8651:[('yl_00','相龙计好')],8652:[('ssxy_00','永字思妣')],8653:[('ssxy_03','许文思往顾和许')],8654:[('js_06','高阳许询'),('js_10','高士许询'),('js_13','高阳许询'),('ssxy_00','许询字'),('ssxy_01','许掾【询也】'),('ssxy_02','按询集询出都迎姊于路赋诗'),('ssxy_03','讷询小字')],8655:[('gs_01','谢庆绪大相推重'),('ssxy_02','谢敷字庆绪')],8656:[('js_13','朗字长度'),('js_18','胡谓谢朗'),('ssxy_00','胡儿谢朗小字也'),('ssxy_01','东阳谢朗也'),('ssxy_02','谢胡儿作著作郎'),('ssxy_03','封谓朗')],8657:[('gs_00','陈郡谢琨赞成其业')],8658:[('js_10','贺循、谢端、顾景、丁琛、傅晞')],8659:[('yl_01','与孙绰、谢询、谢尚、支遁等')],8660:[('js_10','备礼举桂阳谷俭为秀才')],8661:[('gs_01','时同寺沙门道嵩')],8662:[('gs_01','时沙门道恒颇有才力')],8663:[('gs_01','又有沙门道护')],8664:[('js_12','宗年十三，云年十一')],8665:[('js_12','邵广盗官幔三张')],8666:[('js_12','宗年十三，云年十一')],8667:[('ssxy_03','王右军郄夫人谓二弟')],8668:[('ssxy_03','融字景山愔第二子')],8669:[('ssxy_00','献之娶髙平郗昙女名道茂')],8670:[('js_14','邓粲，长沙人')],8671:[('js_16','担其儿及其弟子绥'),('ssxy_00','弟子绥服攸齐衰三年')],8672:[('gs_01','释僧先。冀州人')],8673:[('gs_01','复有释道宝者')],8674:[('js_01','及息陇子、长史梁惮、司马金壹等')],8675:[('js_11','将兵都尉钱颀陈事合旨')]}
    for ident,qs in round32a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第32批A名士僧人家屬逐條原文審讀','Read named passage in context, distinguish relative ages, retrospective events and individual birth/death') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round32a')
    renyear=evidence(SOURCE/'_cache/texts/资治通鉴.txt','陶侃与任让有旧，为请其死','資治通鑑咸和四329二月平蘇峻處刑任讓','全文329二月丙戌攻石頭後、丁亥大赦前，陶侃求赦任讓不許乃殺。SS同陶求任讓/許永而帝拒以殺侍中，明同一處刑案；TJ未點許永。')
    xuyong=chunk_ev('ssxy_00','并斩二人','世說新語許柳子許永思妣/任讓同案並斬','原注永字思妣，陶公欲並宥兩人，帝以任讓殺侍中不許並斬；非將許柳死亡年套許永。')
    results[8652].update(review_status='primary_text_corroborated',historical_death=date(329,xuyong))
    results[8652]['historical_death'].update(source='世說新語許永任讓同案並斬；資治通鑑329平峻任讓處刑紀年',precision='year_inferred_from_explicit_same_event_cross_reference')
    results[8652]['evidence'].extend([renyear,xuyong])
    results[8528].update(review_status='primary_text_corroborated',historical_death=date(329,renyear))
    results[8528]['evidence'].append(renyear)
    results[8653].update(candidate_duplicate_of=8384,source_name_transcription='許文思；本地注文許琛已見，另SS01許璪字思文',identity_status='single_key_xuwensi_guhes_companion_transcription_candidate_xuzao_not_deleted')
    results[8653]['evidence'].append(chunk_ev('ssxy_01','许璪字思文义兴阳羡人','世說新語許璪與顧和同遊宴','思文/文思倒字與顧和同行、注已見可疑同人，尚待許琛/璪版本字校，暫候選而不硬刪。'))
    results[8674].update(review_status='primary_text_corroborated',historical_death=date(366,xunparty))
    results[8674]['historical_death'].update(source='晉書司馬勳傳金壹同送並斬；資治通鑑太和元366同案紀年',precision='year_inferred_from_explicit_same_event_cross_reference')
    results[8674]['evidence'].extend([xunparty,xuntime])
    for ident,note in {8651:'計好為海西公宮內嬖近，相龍另人。宮人所生與親近之說是廢立政治指控不等於已證生父；371廢帝追述不證本人當時在世，無生卒。',8652:'許永字思妣許柳子，陶欲宥任讓須並全許永，帝拒任讓殺侍中而並斬二人；TJ同案329二月錨，僅推329年不套父柳卒或假造生日。',8653:'許文思與顧和共枕、注許琛已見；SS01許璪思文同顧和丞相從事遊宴，疑轉錄字異，未取得版本逐字校，僅候選8384不硬去重，無生卒。',8654:'七key許詢玄度/阿訥高陽、許允玄孫、清談支遁王羲之同游身份閉合。候選誤稱許允孫，原注明玄孫，保原層；母華軼女、送母正文與注迎姊有異說；早卒無確年不補。',8655:'高僧傳謝慶緒推于道邃與SS注謝敷字慶緒會稽處士同人，SS少微犯月俄死無精紀年；31卒是道邃非謝敷。',8656:'六key謝朗字長度父據母王韜女綏、胡兒東陽太守謝安侄俱合；42卒是前謝萬非朗，SS封胡羯末分配異文保留，不硬從小字另併謝淵，無朗精生卒。',8657:'陳郡謝琨贊高坐墓寺為原實名。與謝鯤可能字異但高坐卒年/建寺元成帝版本已有衝突，不據陳郡同姓自動合323卒謝鯤，也不取成帝年份推本人卒。',8658:'謝端與賀循顧景丁琛傅晞在卞壺辭起復表中為荷恩高枕者，卞壺母喪/任職非本人生卒，無確年。',8659:'御覽引王隱晉書原謝詢與孫綽謝尚支遁蘭亭宴集，名謝非許。可能許詢字誤但未版本橋，不只同游即改姓合8654，更不可把王羲之初渡江當蘭亭353精年，無生卒。',8660:'谷儉桂陽甘卓所舉秀才應策高第除中郎，終身不仕卒家無年；甘卓後遷安南/王敦亂非谷儉當時活死年。',8661:'道嵩瓦官僧敷同寺才解與道安書，70餘卒是僧敷而非道嵩，無生卒。',8662:'荊州心無義沙門道恒與法汰慧遠論辯，法汰太元12年68卒屬後一主傳人物非道恒，不套日期。',8663:'道護冀州飛龍山同道安行化，明後不知所終，鄰僧先襄陽疾卒不屬道護，生卒不詳。',8664:'邵宗邵廣子13歲請自沒贖父，成帝從范堅正父死刑，不等於宗被殺；案件無精年不能推其出生。',8665:'邵廣盜幔三張，成帝從范堅議正死刑，明死但只成帝期未具確年，生卒年不詳，不把父死刑套二兒。',8666:'邵雲邵廣次子11歲與兄宗請為奴贖父，案未精年無可倒推生年，父正死刑非雲被殺。',8667:'郗氏王右軍夫人鑒女愔曇姊，90為謝表自言年齡無CE；髮白齒落不是死亡，不填305/399。',8668:'郗融字景山小字倉郗愔第二子，辟琅琊王文學未拜早卒，兄嘉賓超非己，無精年。',8669:'郗道茂郗曇女王獻之前妻，病笃上章是獻之而非妻；咸寧中尚余姚公主字年本身有疑不自動當離婚確年，生卒不詳。',8670:'鄧粲長沙高潔友劉驎之尚公，桓沖辟別駕後病笃乞骸，未明即卒，父騫為元明紀材料非其本人生卒。',8671:'兩key鄧綏小名遺民為鄧攸亡弟子，非和熹鄧太后。叔攸棄親子留綏，叔卒服喪三年屬相對記載，未校叔JS咸和元326與他書年異前不下本人活動CE，更不套叔卒為己。',8672:'僧先冀州常山淵弟子，少與道安相識飛龍山相會，後襄陽疾卒無年；鄰僧輔60卒非僧先，未知生卒。',8673:'道寶王姓琅琊丞相王導弟，出家仰山，前法崇後法義死亡均非道寶，無生卒。',8674:'金壹司馬勳司馬同陇子梁惮明送桓溫並斬，TJ朱序周楚討勳同送溫斬案366推年，非自行套司馬勳所有親屬死年。',8675:'錢頎庾翼將兵都尉陳事合旨擢五呂將軍，穀200斛是賞賜不是年齡；未明本人精活動或生卒年，無確年。'}.items():results[ident]['unresolved'].append(note)
    round32b_reads={8676:[('sl_09','天锡妾阎氏薛氏')],8677:[('js_17','陶淡，字处静')],8678:[('js_12','其兄子俶不敢问之')],8680:[('js_01','西戎司马隗粹并切谏'),('tj_18','别驾雍端、西戎司马隗粹切谏')],8681:[('js_01','梁州别驾雍端'),('tj_18','别驾雍端、西戎司马隗粹切谏')],8682:[('ssxy_03','王丞相有幸妾姓雷')],8683:[('js_11','时卒士韩怅逃之归首')],8684:[('ssxy_01','夷字君齐吴郡人')],8685:[('ssxy_00','张之顾敷是顾和中外孙'),('ssxy_02','敷字祖根吴郡吴人')],8686:[('js_10','贺循、谢端、顾景、丁琛、傅晞')],8687:[('js_18','温遣督护高武据鲁阳'),('tj_16','高句丽王钊果遣弟武'),('tj_17','遣督护高武据鲁阳')],8688:[('js_09','侃妻龚氏亦固劝自行')],8689:[('yl_05','龚玄之字道玄')],8690:[('sl_02','刘宁及弟崇帅胡骑三千奔晋阳'),('sl_03','范阳公刘宁屯据莸城'),('tj_16','别将杨康败刘宁于沙阜'),('tj_17','安西将军刘宁自安定击之')],8691:[('tj_16','遐，简之子也'),('js_03','遐字彦林'),('js_11','山遐作余姚'),('ssxy_00','遐字彦林河内人祖涛')],8692:[('js_11','龢字道季'),('js_12','颍川庾龢名重一时'),('ssxy_00','道季庾龢小字'),('ssxy_02','庾道季云思理伦和'),('ssxy_03','庾道季防之语戴')],8693:[('sl_02','平西将军张伏都')],8694:[('tj_16','子寔君监其南')],8695:[('hy_00','遣右将军李位都讨之'),('sl_09','右将军李位都攻拔丹川'),('tj_16','镇东将军李位都迎诣温降')]}
    for ident,qs in round32b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第32批B原卷本人角色逐鍵實讀','Adjudicate actual named subject and distinguish homonyms, familial ages and narrative dates') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round32b')
    results[8687].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_jin_huanwen_duhu_gaowu_and_goguryeo_kingbrother_wu',component_identities=[dict(identity='桓溫督護高武據魯陽',keys=['js:js_18:277:高武','tj:tj_17:381:高武'],historical_birth=None,historical_death=None),dict(identity='高句麗王釗弟武342拒燕北道',keys=['tj:tj_16:54:高武'],historical_birth=None,historical_death=None)])
    kai=chunk_ev('tj_18','别驾雍端、西戎司马隗粹切谏，勋皆杀之','資治通鑑興寧三365司馬勳反前殺諫臣','兩人同具職名明殺，隨後十一月勳入劍閣，下接太和元366。保365年，不從後366勳死套兩人。')
    for ident in [8680,8681]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(365,kai))
        results[ident]['evidence'].append(kai)
    shijun=evidence(SOURCE/'_cache/texts/资治通鉴.txt','乃执寔君及斤，至长安，车裂之','資治通鑑太元元376十二月代亂後寔君被殺','整年header太元元376與十二月什翼犍還雲中、庶長子寔君弒父、燕鳳對堅、至長安車裂連文親讀；取年376，轉送跨地不自擅精月日。')
    results[8694].update(review_status='primary_text_corroborated',historical_death=date(376,shijun),last_attested_activity=376)
    results[8694]['evidence'].append(shijun)
    results[8679].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='original_cbdb_luwanzai_genealogy_locator_not_primary_text_obtained')
    results[8679]['evidence'].append(dict(citation='CBDB20261017原庫BIOG_MAIN/KIN_DATA陸萬載33728',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='c_birthyear0/c_deathyear0/c_index_year338/type1112/source33727; notes XTS73B.12a/ZhouJiayou2.303；KIN父33727陸始、次子33729陸子真，source0；BIOG_SOURCE僅WikidataQ45427723。',interpretation='本次實查原庫行與親屬名；未取得新唐書73下原譜，不能把338索引年作出生，也不能以原庫關係本身冒稱原史書已讀。'))
    results[8685]['source_reported_age_at_death']={'age':23,'calendar_year':None,'source':'世說新語注顧愷之家傳'}
    results[8687]['component_attested_activity_years']={'高句麗王弟武':[342],'桓溫督護高武':[356]}
    for ident,note in {8676:'張天錫妾閻氏與薛氏本人自刎明死，主君病後竟愈；傳無本人精年，非成漢李期妻閻氏/李壽妻閻氏，亦不取前馬氏興元卒。僅SL晚重編不作早源實證。',8677:'陶淡處靜陶侃孫夏子，15六服食非卒年；莫知所終，前龔玄之58卒後陶潛義熙解印均非淡，生卒不詳。',8678:'陸俶陸納兄子為謝安備盛饌，被叔杖40非40歲，後兄子禽犯罪另人；陸納亡與子長生早亡均非俶，未知生卒。',8679:'原CBDB陸萬載父始子子真、notes新唐書73B12a已取得來源定位但原譜尚未取；338index1112來父33727非本人出生，不把資料庫行當獨立史證。',8680:'隗粹西戎司馬與別駕雍端諫司馬勳反而殺，TJ365叙明其年；本人死於勳舉反初，不是勳366伏誅年。',8681:'雍端梁州別駕與隗粹諫反明殺，365年具名TJ；勿從朱序次年擒勳同案全套366。',8682:'雷氏王導幸妾生恬洽，雷尚書是蔡謨嘲號非正式尚書官，不合導正妻曹氏，未知生卒。',8683:'韓悵卒士失牛逃後歸首受拷，自誣偷牛而王坦之議罪疑從輕獲原，不是伏法死者，無生卒。',8684:'顧夷君齊吳人祖廞父霸，八賢論答謝萬；顧君齊不是另人，未就州主簿，無本人年歲生卒。',8685:'顧敷祖根顧和中外孫，兩則7歲清談/佛像故事同張玄之顧和，注23卒無紀年。兩童同7歲vs別則9/7歲屬記事異同，不擅填顧敷生日。',8686:'顧景卞壺表荷恩高枕名列者，與謝端等不同人，旁文卞壺委質5載非顧景年齡生卒。',8687:'三key實混：TJ16為高句麗王釗弟武北道5萬拒燕342，JS18/TJ17為晉桓溫督護高武魯陽356；國別親屬官職不同不能同人，頂層生卒/活動不套任一支。',8688:'龔氏陶侃妻蘇峻亂固勸侃自行，龔登是督護不是妻同一人，侃瞻之死非妻；無生卒。',8689:'龔玄之道玄四徵不就，孫放為推薦武陵守非玄之任職，無年齡日期；可後補JS17主傳但本唯一key已明本人。',8690:'四key同趙將劉寧及弟崇3000騎奔晉陽與後燕車騎范陽公同鏈，趙→燕→秦→燕前後轉仕不因國變拆人。TJ347沙阜敗、349梁犢敗均非被殺，SL355歸罪不作單晚源已證活動，無生卒。',8691:'四key山遐彥林山簡子濤孫，余姚括戶與東陽峻法同人。八旬萬餘口與往年2000戶不可當歲數；康帝責政後卒官無精年，書信追述未必當年活，不填342死。',8692:'五key庾龢道季亮子丹陽尹中領軍同人，15歲諫叔翼襄陽是活動年齡非卒齡；太和初代王恪後卒官無精年，不填366死亡。',8693:'SL張伏都平西將軍伐涼州河西敗還，與TJ347孫伏都涉同役可疑姓異但未明版本校不強合，敗還明非戰死；晚重編紀年不推嚴格日期。',8694:'單key寔君為什翼犍庶長子監南部，與世子寔/寔子珪不同。全TJ376十二月弒父後秦送長安車裂本人明斬；只取376年，未擅月份或父子生年推算。',8695:'華陽/SL/TJ李位都右將軍李壽討孟彥與後鎮東迎桓溫降，同成漢官將前後歷仕，未明中間升遷年。347降不是處死，不套李勢降後別支被殺。'}.items():results[ident]['unresolved'].append(note)
    round32c_reads={8696:[('js_12','性简贵，与王羲之雅相友善'),('js_13','王羲之，字逸少'),('tj_16','前江州刺史王羲之夙有令名'),('tj_17','中军将军王羲之以书止之'),('tj_18','王羲之与桓温笺'),('js_04','有以问王羲之'),('js_07','会稽内史王羲之引为右军长史'),('js_10','与姊夫王羲之'),('js_17','王羲之见而目之'),('ssxy_02','按王氏谱羲之是敦从父兄子'),('ssxy_00','王右军与谢太傅共登冶城'),('ssxy_01','逸少羲之小字'),('ssxy_03','按诸书皆云王允之事而此言羲之疑误')],8697:[('sl_02','侍中郑系王谦常侍卢谌崔约')],8698:[('sl_02','国子祭酒聂熊'),('sl_03','秘书监聂熊闻恒言')],8699:[('js_12','颍川荀羡少有令闻'),('js_13','令殷浩、荀羡还据合肥'),('sl_03','晋遣北中郎将荀羡'),('tj_16','擢羡为吴国内史'),('tj_17','以吴国内史荀羡为使持节'),('tj_18','徐、兖二州刺史荀羡有疾'),('js_10','时北中郎荀羡有疾'),('sg_04','崧子羡字令则'),('ssxy_00','荀中郎在京口')],8700:[('sl_03','相国莫浅浑将兵来伐'),('tj_16','宇文逸豆归遣其相莫浅浑')]}
    for ident,qs in round32c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第32批C王氏荀氏原文逐源核讀','Read actual cited biography role, separating named people and conflicting chronological statements') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round32c')
    wangcore=[k for k in results[8696]['keys'] if not k.startswith('ssxy:ssxy_03:')]
    results[8696].update(review_status='identity_conflict',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,identity_status='xizhi_core_thirteen_keys_identified_ss03_contains_explicit_yunzhi_misattribution',component_identities=[dict(identity='王羲之逸少父曠子凝之徽之獻之，十三key身份閉合',keys=wangcore,historical_birth=None,historical_death=None),dict(identity='SS03混合編錄：王右軍本事與注文指為王允之的佯睡故事',keys=['ssxy:ssxy_03:12:王羲之'],historical_birth=None,historical_death=None,disputed_anecdote_subject='王允之：諸書皆云王允之事而此言羲之疑誤')])
    wangoath=chunk_ev('js_13','维永和十一年三月癸卯朔，九日辛亥','晉書王羲之傳355三月九日墓誓','親讀全文起家、殷浩往復書、永和九353蘭亭及本355墓誓、退居書謝萬、59卒。傳內絕對紀年止活動，沒明卒年。')
    wangage=chunk_ev('js_13','年五十九卒，赠金紫光禄大夫','晉書王羲之傳卒59','59是年齡但本傳不配CE卒年，故不能單由年齡鎖361或379；13謁周顗亦非出生年直接記載。')
    results[8696]['evidence'].extend([wangoath,wangage,dict(citation='CBDB20261017原庫BIOG_MAIN/KIN_DATA25793',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='王羲之birth321/death379/deathage59；父王曠、子凝之獻之徽之；notes Yanlu3.4a–4b/Giles821–822/ZhouJiayou1.98/QTW397.21a–26a王師乾神道碑；WikidataQ378564。',interpretation='原庫直接欄位不是程序推算，身份父子可橋正史；但321379原始年代論證未取得，不能升格獨立早期確證。'),dict(citation='陶弘景真誥卷16歷史注文（root已親讀之委派證據）',url='https://zh.wikisource.org/wiki/真誥/卷016',excerpt='升平五年辛酉亡，年五十九',verification='delegated_individually_read_by_root_record12183',interpretation='根代理已讀姓名王右軍逸少父曠及355墓誓注文，早源明361年59推303；此代理未另在線取文，透明保委派層。鬼故事已五年非365在世證。')])
    results[8696]['source_reported_chronologies']=[dict(source='CBDB25793原庫直接日期欄位，notes所引年份論證未取',birth=321,death=379,age=59,source_layer='database_report_not_independent_primary_chronology'),dict(source='陶弘景真誥16注文root已實讀',birth=dict(year=303,relation='inferred_nominal_age',calculation='361−59+1'),death=dict(year=361,expression='升平五年辛酉亡'),age=59,verification='delegated_root12183'),dict(source='晉書80親讀本傳',age=59,death_year=None)]
    results[8696]['component_attested_activity_years']={'王羲之本人主支':[353,355]}
    results[8696]['candidate_duplicate_of']=12183
    results[8696]['candidate_duplicate_scope']='root12183 GL王羲之單人已核；本條SS03含允之錯傳不整條刪'
    xunage=chunk_ev('js_12','升平二年卒，时年三十八','晉書荀崧附荀羨傳358卒38','本人生卒傳原記，不套郗曇42或父荀崧；與TJ349授刺史28虛歲反推322而非此38反推321有一年差。')
    xun28=chunk_ev('tj_17','时年二十八，中兴方伯未有如羡之少者','資治通鑑永和五349十二月荀羨授徐州刺史28','年header349同褚裒己酉卒後授羨28，可條件推322；JS卒35838推321，兩種年齡對錨不可默擇。')
    results[8699].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(358,xunage),source_reported_chronologies=[dict(source='晉書荀羨傳',death=358,death_age=38,birth_inferred=321,calculation='358−38+1'),dict(source='資治通鑑349授刺史',activity=349,age=28,birth_inferred=322,calculation='349−28+1')],attested_activity_years=[349])
    results[8699]['evidence'].extend([xunage,xun28])
    for ident,note in {8696:'十四key主體王羲之逸少父曠子凝徽獻之，CBDB家系可橋。但SS03候選本人bio收佯睡避敦逆謀故事、劉注已明諸書作王允之；不是旁卷同名而是本key混錄錯事，隔離此故事不把<10歲用推生。原CBDB321379、委派已讀真誥36159推303均保源別，頂層不擇精年。JS353蘭亭/355墓誓為本人活動非死亡；誓墓非首次登場。',8697:'王謙後趙侍中與鄭系盧諶崔約不向申扁拜，不合後世同名王謙；無生卒，僅SL晚重編未補已證活動。',8698:'聶熊趙國子祭酒注穀梁與燕秘書監、東宮傅同名同經學官序可橋，但韓恒自建龍城改名議後二傅不等於本人死；未以事功名號推生卒。兩key均SL晚層保限制。',8699:'九key荀羨令則父崧尚尋陽公主、28徐州刺史、救段龕同人，原SS注字全則為傳抄異字不另拆。JS358卒38可證死亡358；TJ34928虛歲反推322vs35838反推321一年差，出生撤單321保兩條年齡錨，不擅選。JS北救記段蘭與SL/TJ龕差保原文，不認荀氏另人。',8700:'莫淺渾宇文逸豆歸相343伐燕，慕容翰擊之大敗僅身免，明逃免非343戰死，生卒未知；首次見於此役不是初仕。'}.items():results[ident]['unresolved'].append(note)
    round33a_reads={8701:[('sl_02','侍中郑系王谦'),('tj_17','执洛州刺史郑系'),('js_08','荥阳郑系')],8702:[('tj_16','高句丽王钊果遣弟武'),('wsh_02','掘高丽王钊父利墓'),('wsh_04','利子钊，烈帝时'),('yi_02','弗利死，子刘代立')],8703:[('sl_02','平北将军尹农攻燕凡城'),('sl_03','赵平北将军尹农率众寇凡城'),('tj_16','赵平北将军尹农攻燕凡城')],8704:[('sl_02','散骑侍郎崔收沈璧于河')],8705:[('js_11','翼以长子方之为义成太守'),('js_13','以子方之为建武将军'),('tj_16','翼以长子方之为义城太守'),('ssxy_03','留长子方之守襄阳')],8707:[('sl_02','慕容彪攻陷中山'),('sl_03','左将军彪戍之'),('sl_03','左将军彪[一作彭]为武昌王'),('tj_16','慕容彪追击，破之'),('tj_17','左将军彭为武昌王')],8708:[('sl_03','振威将军慕舆贺辛战没'),('tj_18','振威将军慕舆贺辛战没')],8709:[('sl_03','匈奴左贤王曹毂'),('sl_05','匈奴曹毂刘卫辰帅众'),('tj_18','匈奴右贤王曹毂')],8710:[('js_13','令宣进伐石季龙将李罴'),('sl_02','桓宣伐虎将李罴'),('tj_16','击赵将李罴于丹水')]}
    for ident,qs in round33a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第33批A高句麗/趙燕與庾氏逐鍵本人原文核讀','Read all listed passages, preserving manuscript name/year differences and avoiding defeated/captured as killed') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round33a')
    results[8701].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='zhao_shizhong_zhengxi_and_later_luozhou_governor_not_uniquely_bridged')
    results[8705].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='four_texts_yuyi_son_fangzhi_clear_bare_cbdb399749_source_not_obtained',original_cbdb_source_locators=[dict(personid=399749,textid=4736,title='襄陽府志',pages='section_id=5190264',qid='Q65874284',kin_rows_consulted=0)])
    results[8706].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='original_cbdb_named_fu_jian_wife_poetry_source_not_obtained',last_manual_review_batch='round33a_original_db_checked')
    results[8706]['evidence'].append(dict(citation='原CBDB134775張氏(苻堅妻)',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),tables=['BIOG_MAIN','KIN_DATA','BIOG_SOURCE_DATA','TEXT_CODES'],excerpt='名張氏(苻堅妻)，生卒NULL，index340type03；主源明清婦女著作數據庫MQWWPoetID3223；Q65801036。',interpretation='原庫本人名及資料庫文學來源定位，未讀所據詩文傳記，不直接同匿名張后或用340作生日。'))
    results[8702].update(review_status='duplicate',duplicate_of=7322,identity_status='same_gogugwon_sayu_zhao_yeotbulli_son_wandu_campaign_adjudicated')
    results[8702]['source_name_variants']=['釗（TJ/WS）','劉（梁書本地傳抄）','斯由（SK）']
    results[7322]['evidence'].extend(results[8702]['evidence'][-4:])
    results[7322]['unresolved'].append('補全8702四key：TJ/WS釗父乙弗利丸都同役與SK故國原斯由/美川王子橋，梁書原名劉、343敘事，WS建國四341、TJ342同役紀年異保源別，身份不因傳抄異字拆人。8702可整條重定向7322，但不能取342陷城為卒。')
    results[8702]['duplicate_basis']='此前7322已親讀SK故國原王斯由、美川王子及342丸都母周氏被虜與371百濟殺王；此輪TJ/WS同父乙弗利與丸都王釗、梁書劉同父同城同敵完全橋合。'
    hexin=chunk_ev('tj_18','八月，什翼犍攻云中，泥弃城走，振威将军慕舆贺辛战没','資治通鑑太和二367八月慕輿賀辛戰沒','原年367，慕輿泥棄城走非賀辛逃，賀辛本人戰沒；與SL建熙八同役可獨立確證。')
    results[8708].update(review_status='primary_text_corroborated',historical_death=date(367,hexin,month=8))
    results[8708]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8708]['evidence'].append(hexin)
    luweb=dict(citation='新唐書卷73下宰相世系表陸氏（公開原文親讀）',url='https://zh.wikisource.org/wiki/新唐書/卷073下',retrieval='agent-reach Jina Reader',excerpt='始字祖興，五兵尚書、侍中。二子：俶、萬載。萬載，臨海太守、祕書監、侍中。四子：道玩、叔元、羣、子真。子真字同宗，宋東陽太守。',verification='individually_read_original_text',interpretation='與原CBDB父33727陸始、子33729陸子真及原notesXTS73B12a對橋，萬載身份閉合，不從父索引338當出生；同時佐8678俶為陸納侄。')
    results[8679].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='original_cbdb_father_shi_son_zizhen_matched_xintangshu_genealogy')
    results[8679]['evidence'].append(luweb)
    results[8678]['evidence'].append(luweb)
    results[8679]['unresolved'].append('公開原典補核已親讀新唐書73下原所指陸始二子俶萬載/萬載子子真，父子與官職明橋，撤未取原譜身份hold；原譜無本人生卒年月仍不詳。')
    wangdirect=dict(citation='陶弘景真誥卷016王逸少歷史注文（本代理本輪公開原文親讀）',url='https://zh.wikisource.org/wiki/真誥/卷016',retrieval='agent-reach Jina Reader',excerpt='王逸少有事，係禁中已五年…逸少即王濆兄曠之子…後為會稽太守。永和十一年去郡，告靈不復仕…至升平五年辛酉歲亡，年五十九。今乙丑年，說云五年，則亡後被係。',verification='individually_read_original_text',interpretation='獨立本輪讀父曠/會稽/355退官及升平五361卒59，得名義303生。宗教鬼官框架中歷史注文而非365仍活。與root委派是同一史源重讀，不算兩獨立記載。')
    results[8696]['evidence'].append(wangdirect)
    results[8696]['source_reported_chronologies'][1]['verification']='individually_read_by_this_agent_also_root_same_source_not_independent_two'
    for ident,note in {8701:'JS/SL趙所擢荥陽鄭系/侍中同支，TJ351被高崇呂護執而降晉為洛州刺史鄭系；同名官轉可能但無字父或同事件橋，暫未全閉，不以擒降當死。',8702:'四key高句麗釗/梁書劉父乙弗利丸都陷同人，可合7322斯由。WS建國四341、TJ342、梁建元元343同役紀年差保源別不擅消；陷城單馬逃不是死亡，371卒仍取SK已讀本人役。',8703:'尹農三key後趙平北將軍攻凡城不克而還，SL黜庶人不是殺，不因名農推寒門年齡，無生卒。',8704:'崔收散騎侍郎沉璧靈昌河，沉的是璧非本人，地震壓死工匠未點其死，不套百餘死或工匠被斬；僅SL晚層生卒不詳。',8705:'四文本本人庾翼長子方之義成/義城太守建武將軍襄陽可閉，CBDB399749裸名無親屬日期且原源襄陽府志section5190264尚未取，wholehold。先誤猜甘肅而查卷27無其人已糾，不把讀錯志算此key完成。',8706:'張氏苻堅妻原CBDB+MQWW詩人來源已取得定位未取傳，未知年月；index340type03非生年，不先同所有苻氏張妻。',8707:'四key慕容彪/彭左將軍武昌王：SL原直注一作彭，弟左將軍戍威德追白勝與後武昌王稱號連，TJ同左將軍彭封武昌王，身份閉；中山白同死非彪，無生卒。',8708:'慕輿賀辛367八月雲中戰沒明本人，慕輿泥同役棄城走另人，SL/TJ兩源閉合，未知生日。',8709:'曹轂匈奴王365反秦杏城降、為郭辯託身潛燕使同人，SL左賢/TJ右賢源差原留，弟活斬為8600非曹轂。未見本人卒精年，不填弟365。',8710:'李羆趙將丹水敗桓宣，桓宣被貶病卒非李羆；三源同役，無本人生卒。'}.items():results[ident]['unresolved'].append(note)
    round33b_reads={8711:[('js_13','穆之字宪祖'),('tj_16','以参军毛穆之为建武司马'),('tj_17','留颖川太守毛穆之')],8712:[('js_00','尚书仆射江[A170]议应曰太夫人'),('js_06','[A170]字思玄'),('js_11','翼长史江[A170]'),('tj_16','朱焘与安西长史江A170'),('tj_18','仆射江A170启'),('ssxy_01','江虨字思陈留人'),('ssxy_02','刘尹云人言江虨田舍'),('ssxy_03','刘尹江虨王叔虎孙兴公同坐')],8713:[('sl_02','皝遣商人王车'),('sl_03','乃遣商人王车通市'),('tj_16','乃遣商人王车通市')],8714:[('sl_03','并州刺史王霸自甘松出救之'),('tj_16','并州刺史王霸自甘松出救之')],8715:[('sl_03','右将军白胜并州刺史王霸'),('tj_16','右将军白胜、并州刺史王霸')],8717:[('sl_04','李俨将贺肫说俨'),('tj_18','李俨将贺肫说俨')],8718:[('sl_03','其大人长孙秩迎后于燕'),('tj_16','其大人长孙秩迎妇于燕'),('wsh_00','遣大人长孙秩迎后慕容氏')],8719:[('sl_03','王宪刘明忠臣也'),('tj_16','参军王宪、大夫刘明')],8720:[('sk_00','拜<康世>为伊伐') ]}
    for ident,qs in round33b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第33批B趙燕使者/名臣各源本人原文實讀','Read actual named passage in full local context, not candidate JSON summaries; separate relative deaths and individual events') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round33b')
    maoev=evidence(SOURCE/'_cache/texts/资治通鉴.txt','癸未，使右将军毛虎生帅众三万击巴中','資治通鑑太元四379三月毛虎生救魏興','全本379年header至三月癸未親讀，本人右將軍兵三萬救魏興敗退巴東；JS13毛穆之名宪祖/武生、子球益州巴西巴東同鏈，虎生武生文字異記而非另一毛姓。')
    maokin=evidence(SOURCE/'_cache/texts/资治通鉴.txt','以虎生子球为梓潼太守','資治通鑑寧康元373毛虎生子球','JS13穆之以子球梓潼、伐巴西退巴東同父子官職橋，跨書更名清楚；373叙與379魏興案不混作卒年。')
    results[8711].update(last_attested_activity=379,attested_activity_years=[344,345,356,379])
    results[8711]['evidence'].extend([maoev,maokin])
    results[8711]['source_name_variants']=['毛穆之','宪祖（字）','武生（JS小字）','毛虎生（TJ全本後段）']
    jiangev=chunk_ev('tj_18','仆射江A170启','資治通鑑興寧元363三月江虨議服','哀帝周太妃薨後議缌麻，江虨本人發言。母周氏死亡非虨卒；A170為該底本文字缺碼。')
    results[8712]['evidence'].append(jiangev)
    results[8712]['attested_activity_years']=[345,363]
    tuge=chunk_ev('tj_18','猛斩肫，以俨归','資治通鑑太和二367李儼枹罕賀肫斬','本人王猛所斬，四月援軍後五月慕容恪卒之前敘次，保367年不取五月恪卒或強定四月日。')
    results[8717].update(review_status='primary_text_corroborated',historical_death=date(367,tuge))
    results[8717]['evidence'].append(tuge)
    caoev=chunk_ev('tj_18','曹毂寻卒，秦分其部落为二','資治通鑑太和二367曹轂卒','就在賀肫與五月慕容恪卒/郭辯使燕後，七月之前本人尋卒，獨立早源明367敘次。不得因尋字猜精月或把弟活365套本人。')
    results[8709].update(review_status='primary_text_corroborated',historical_death=date(367,caoev))
    results[8709]['evidence'].append(caoev)
    results[8709]['unresolved'].append('相鄰原文補核：此前只定位365曹活被斬未見轂卒，現實讀TJ18太和二367郭辯使燕後曹轂尋卒，補本人367死亡，前未知描述指此前範圍。')
    muev=evidence(SOURCE/'_cache/texts/魏书.txt','崇子遂留在伏士中','魏書拓跋儀傳穆崇子遂留告父謀逆','本次親讀原父子關係與逾牆告狀、太祖秘恕。與CBDB唯一穆遂留父穆崇可閉身份；後天賜六409賜死是拓跋儀非遂留，不能套本人。')
    results[8716].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='cbdb_mu_suiliu_father_muchong_matched_weishu_named_son',last_manual_review_batch='round33b_cbdb_primary')
    results[8716]['evidence'].extend([dict(citation='CBDB20261017原庫穆遂留194683',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父穆崇，子穆忸頭/穆觀（KINsource32033）；c_deathyear405/c_index_year342type29；notes Death year estimated by Tackett(cc405)；補遺7:138–139/文物1999.1:93/洛陽新獲續504。',interpretation='本次原庫親屬與日期限定實查，405明估年非確切，342索引非出生。未讀原墓誌，不宣子系/估卒已正史全證。'),muev])
    for ident,year in [(8713,340),(8714,344),(8715,344),(8718,344)]:
        results[ident]['appearance_upper_bound']=year
        results[ident]['appearance_constraint_explanation']='本批親讀TJ年header下本人具名商旅迎翰/軍事援宇文/迎后事件，只作登場最晚上界非初仕生年或死亡年。'
    for ident,note in {8711:'三key毛穆之毛寶子宪祖武生與全TJ虎生子球梓潼、益州右將軍巴西巴東同鏈，379本人領軍仍活可約束估卒，不擅把379軍事敗退當死。JS先襄陽失守後回敘蜀漢、與TJ373/379兩次巴西混敘，保史傳叙次問題；未明本人卒年不能補377或379。',8712:'八key江虨字思玄江統子，底本A170同名缺碼；庾翼長史平干瓚→護軍/會稽→仆射議周太妃同官事，SS思玄陳留善棋/田宅屯身份閉。弟江惇35349卒、父統310卒均非虨，未明本人生卒。',8713:'王車燕商人通宇文偵慕容翰並埋弓助歸，三key同獨特任務。TJ咸康六340年header及二月翰歸，340為本人已有活動上界非初仕，無生卒。',8714:'王霸趙并州刺史與白勝344援宇文敗退，不合東漢王霸，也不合慕容霸；被追破不明死，344活動上界，未知生卒。',8715:'白勝趙右將軍同344援宇文、攻威德城不克退敗，鄰慕容翰被賜死非白勝，無生卒，344活動上界。',8716:'穆遂留原CBDB父崇與魏書崇子遂留逾牆告狀原文橋明，身份撤hold；405Tackett估卒、342索引非出生仍不取。409儀被賜死不是穆遂留。原墓誌與所記二子未取則不能稱其親屬全證。',8717:'賀肫李儼將367建議反擊王猛被儼告而猛斬本人；儼送長安授光祿與肫不同，未知生日，死亡僅年。',8718:'三key長孫秩代大人建國七344迎慕容皇后（皝女）同人同役，不合後世同姓，活動344上界非初仕，后六月至不是秩生卒。',8719:'劉明燕大夫諫事觸怒本擬大辟已被恕死免禁，封裕奏後復官，不得當345已處死；和王憲另人、劉翔亦另人，無生卒。',8720:'康世新羅訖解王36年伊伐官，37年率騎逐倭，王47年薨是君主非臣；此本句無CE同步未擅換算本人出生或死年，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    round33c_reads={8721:[('js_11','爰之有翼风'),('js_12','庾爰之肯避温乎'),('tj_16','以庾爰之代其任'),('js_16','庾爰之以宣素贫'),('ssxy_01','爰之字仲真翼第二子'),('ssxy_02','颍川庾爰之尝以问其伯文康'),('ssxy_03','庾翼子园客同为学生')],8722:[('js_11','羲少有时誉'),('js_14','庾楷，征西将军亮之孙，会稽内史羲小子也'),('ssxy_01','道恩庾羲小字')],8724:[('yl_05','尚书郎徐禅诣喜咨焉')]}
    for ident,qs in round33c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第33批C庾氏與永和議禮逐源親讀','Read actual named person, guarding Yuyi/Yuliang sons against same-surname relative dates') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round33c')
    results[8721]['source_reported_age_at_death']={'age':36,'calendar_year':None,'source':'世說新語注中興書庾爰之'}
    xuid=evidence(SOURCE/'_cache/texts/宋书.txt','徐湛之，字孝源','宋書61徐湛之傳本人身份','本次親讀本傳父徐逵之、祖欽之、子聿之及生平始末；與原CBDB193926父逵之子聿之完全橋，原406估卒/343索引不應套入晉初。')
    xudie=evidence(SOURCE/'_cache/texts/宋书.txt','湛之惊起趣北户，未及开，见害。时年四十四','宋書61徐湛之劉劭弒文帝同日遇害44','前文劭入弒之旦其夕上與湛之屏人語至曉未滅燭，明同一旦遇害44；不以父415戰死或母之喪套本人。')
    xuday=evidence(SOURCE/'_cache/texts/宋书.txt','甲子，上崩于含章殿，时年四十七','宋書5文帝紀元嘉三十453二月甲子','本次親讀元嘉30正月header、二月壬子賑恤後甲子上崩；皇帝47非徐湛之44，僅提供本傳明同旦事件的年月干支錨。')
    results[8723].update(review_status='primary_text_corroborated',historical_birth=date(410,xudie,age=44),historical_death=date(453,xudie,month=2,day_ganzhi='甲子'),source_identities_all_adjudicated=True,identity_status='cbdb193926_xutan_zhi_father_kuizhi_son_yuzhi_matched_songshu61',first_attested_career_year=429,last_manual_review_batch='round33c_cbdb_primary')
    results[8723]['historical_death'].update(source='宋書61徐湛之同文帝遇弒旦死亡；宋書5元嘉三十453二月甲子紀日',precision='lunar_month_ganzhi_day_inferred_from_explicit_same_day_cross_reference',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only',derivation='Biography explicitly XutanZhi killed at dawn when LiuShao killed emperor; emperor annals dates same event Yuanjia30/2/jiazi. No Gregorian day conversion claimed.')
    results[8723]['historical_birth']['death_anchor_source']='宋書元嘉三十453弒帝同案；44虛歲反推453−44+1=410'
    results[8723]['evidence'].extend([dict(citation='CBDB20261017原庫徐湛之193926',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父徐逵之、子徐聿之；c_deathyear406/index343type29；notes Death year estimated by Tackett(cc406)；ctext676612/Q10296403。',interpretation='原庫親屬吻合宋書，406明估卒不是來源異說的史籍死亡年，可撤估而取實證453。'),xuid,xudie,xuday,evidence(SOURCE/'_cache/texts/宋书.txt','六年，东宫始建，起家补太子洗马','宋書徐湛之元嘉六429起家','元嘉二425除佐郎員外侍郎並不就，六年明起家任太子洗馬；429是本傳明初任，非出生也不把幼時422封侯抹掉。')])
    results[8725].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb_lizigu_only_genealogy_tombstone_not_obtained',last_manual_review_batch='round33c_original_db_checked')
    results[8725]['evidence'].append(dict(citation='原CBDB195422李子固BIOG_MAIN/KIN_DATA/BIOG_SOURCE_DATA',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birthNULL/death406/index343type29；notes Death year estimated by Tackett(cc406)，補遺7:138–139/文物1999.1:93/洛陽新獲續504；KIN子李慶某，source32033；Q45588463。',interpretation='明估卒406與索引343不能當本人生卒，原墓誌內容尚未取得、子名末字缺，不能靠李氏同名拼到別人。'))
    for ident,note in {8721:'七key庾爰之仲真園客庾翼次子、伯庾亮、徙豫章接范宣貧同人，SS01中興書36卒無年，不擅推生日；SS03先劉爰之遵祖是別段別人，本candidate明園客父翼非把劉人混入。345父翼41卒不是爰之死。',8722:'三key庾羲道恩叔和庾亮子、準楷父吳國內史與後會稽內史同族鏈；JS稱亮次子而SS注第三子名次異保。與冰長子始彥庾希不同，不把希372被誅套羲；本方見用卒無年。',8723:'原CBDB索引343/估卒406明模板估算；本次宋書61本人父逵之、子聿之明橋非其他徐湛之。劉劭弒文帝同旦453二月甲子本人遇害44可虛歲推410。425辟不就、429起家明載，不誤當三世紀將領。出生為算法非生日，公曆月日未換。',8724:'徐禪尚書郎永和初太廟迭毀咨虞喜，喜所著數十萬言非徐禪作品；永和初只寬期非精年，無生卒。',8725:'李子固原庫記子李慶某、共同墓誌文獻定位已得，原墓誌未取得，不能把空缺或模板估406索引343算原史料已核，待取文後閉身份。'}.items():results[ident]['unresolved'].append(note)
    round34a_reads={8726:[('sl_09','势将爨𬱟奔附于晋')],8727:[('sl_03','王宪刘明忠臣也'),('tj_16','参军王宪、大夫刘明')],8728:[('js_09','洽字敬和'),('js_10','石季龙冠军将军王洽参军'),('js_12','与琅邪王洽齐名'),('gs_01','领军王洽东亭王珣'),('ssxy_02','王洽字敬和丞相导第三子'),('ssxy_03','车骑王洽别见')],8730:[('sl_03','苻坚将苻庾据陜城来降'),('sl_04','魏公庾安西将军'),('sl_05','杨安攻拔苻庾于陜城'),('wsh_02','魏公庾反于陕')],8731:[('tj_17','廋为魏公'),('tj_18','洛州刺史魏公廋')],8732:[('sl_03','栁为晋公'),('sl_04','从兄晋公栁为车骑大将军'),('sl_05','强后欲立少子晋王栁'),('sl_09','晋王苻桞遣参军阎负梁殊'),('tj_17','柳为晋公'),('tj_18','晋公柳数出挑战'),('wsh_02','晋公柳反于蒲坂'),('sl_07','苻柳起兵枹罕')],8733:[('sl_03','武为燕公'),('sl_04','燕公武作乱'),('tj_17','武为燕公'),('tj_18','燕公武谋作乱'),('wsh_02','燕公武反于安定')],8734:[('sl_04','双为河南公'),('tj_18','秦州刺史赵公双'),('wsh_02','坚弟赵公双反于上邽'),('sl_05','赵公双之叛'),('sl_07','其将苻双据陜以叛')]}
    for ident,qs in round34a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第34批A王洽同名及四苻宗室各源原文核讀','Read actual role and genealogy in cited passages; preserve source errors rather than silently regularizing dates or persons') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round34a')
    wangdeath=chunk_ev('js_09','升平二年卒于官，年三十六','晉書王導附王洽傳358卒36','王導子洽敬和，二子珣珉；只主支不套JS10後趙王洽。')
    wang26=chunk_ev('ssxy_02','年二十六而卒','世說新語注中興書王洽卒26','同注明王洽字敬和導第三子、中領軍中書令不拜，身份同主支但卒齡26異於JS36，不暗改字。')
    corekeys=[k for k in results[8728]['keys'] if k not in ['js:js_10:120:王洽','ssxy:ssxy_03:116:王洽']]
    results[8728].update(review_status='identity_conflict',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='mixed_wangdao_son_qia_and_zhao_champion_general_qia_ss03_cheqi_annotation_unbridged',component_identities=[dict(identity='王導子王洽字敬和，珣珉父',keys=corekeys,historical_birth=None,historical_death=date(358,wangdeath),source_reported_chronologies=[dict(source='晉書65',death=358,age=36,birth_inferred=323,calculation='358−36+1'),dict(source='世說注中興書',death_year=None,age=26,conditional_birth_if_same358_anchor=333,calculation='358−26+1')]),dict(identity='後趙冠軍將軍王洽，王波上司，石虎卒後一同降晉',keys=['js:js_10:120:王洽'],historical_birth=None,historical_death=None),dict(identity='SS03羅友白羊宴之王車騎，注文作王洽，爵官鏈尚未橋',keys=['ssxy:ssxy_03:116:王洽'],historical_birth=None,historical_death=None)])
    results[8728]['evidence'].extend([wangdeath,wang26,dict(citation='原CBDB25790王洽KIN_DATA/BIOG_MAIN',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='名王洽，父王導25788、子王珣；birth0/death0/deathage0；notes most famous of Daos sons, administration JinMudi,Yanlu3.5b/ZhouJiayou1.91。',interpretation='本次查父子和原零日期，獨立橋至JS導子敬和主支。不是JS10後趙王洽，也不由CBDB0作生卒。')])
    souev=chunk_ev('tj_18','乃赐廋死，原其七子','資治通鑑太和三368十二月魏公苻廋賜死','本年十二月猛等拔陝獲魏公廋送長安，堅問反原因後賜死，並明高祖健之子身份。')
    results[8731].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(368,souev,month=12))
    results[8731]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8731]['evidence'].append(souev)
    results[8730].update(review_status='duplicate',duplicate_of=8731,historical_birth=None,historical_death=None,identity_status='fu_yu_sou_variant_same_weigong_shancheng_rebellion_all_four_keys_adjudicated',duplicate_basis='SL/WS魏公庾陝城四公反秦，TJ魏公廋陝城父健七子被赦同事件親屬爵官，庾廋字形異而同人，target8731兩key已閉')
    results[8731]['evidence'].extend(results[8730]['evidence'][-4:])
    liuev=chunk_ev('tj_18','九月，王猛等拔蒲阪，斩晋公柳及其妻子','資治通鑑太和三368九月苻柳本人斬','同名晉公柳健之子、世子良守蒲坂、西趨長安被羌擊敗，九月本人連妻子斬，無精日。')
    results[8732].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(368,liuev,month=9))
    results[8732]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8732]['evidence'].append(liuev)
    wuev=chunk_ev('tj_18','秋，七月，王鉴等拔上邽，斩双、武，宥其妻子','資治通鑑太和三368七月苻武/雙上邽斬','燕公武棄安定奔雙上邽，王鑒等拔上邽具名斬雙武。妻子宥，不是斬妻子；不同九月柳連妻子斬。')
    results[8733].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(368,wuev,month=7))
    results[8733]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8733]['evidence'].append(wuev)
    results[8734].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='zhaogong_shuang_core_clear_sl07_shancheng_shuang_sou_mislabel_unresolved',component_identities=[dict(identity='苻雄子苻堅弟趙公雙上邽',keys=[k for k in results[8734]['keys'] if not k.startswith('sl:sl_07:')],historical_birth=None,historical_death=dict(date(368,wuev,month=7),precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')),dict(identity='SL07慕容德傳陝城反秦之苻雙：疑當魏公廋，僅姓名雙不強定',keys=['sl:sl_07:7:苻雙'],candidate_duplicate_of=8731,historical_birth=None,historical_death=None)])
    results[8734]['evidence'].append(wuev)
    zhangid=evidence(SOURCE/'_cache/texts/晋书.txt','坚大惭，顾谓其夫人张氏曰','晉書苻堅載記淝水後顧妻張氏','夫堅、夫人張氏與原CBDB134775丈夫31645苻堅關係相同，原其丈夫337384等估年不作橋紀年。')
    zhangdeath=evidence(SOURCE/'_cache/texts/晋书.txt','中山公诜及张夫人并自杀。是岁太元十年也','晉書苻堅載記385張夫人自殺','本人張夫人並自殺，直接是歲太元十385，毋須依SL晚重編補年；苻堅48卒是丈夫非張氏年齡。')
    results[8706].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(385,zhangdeath),source_identities_all_adjudicated=True,identity_status='cbdb_named_zhang_wife_fujian_matched_jinshu_fujian_wife_zhangshi',last_manual_review_batch='round34a_bonus_cbdb')
    results[8706]['evidence'].extend([zhangid,zhangdeath,dict(citation='原CBDB134775丈夫關係補讀',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='KIN丈夫31645苻堅，source9601；本人名張氏(苻堅妻)。',interpretation='夫妻二名與晉書夫人張氏同身關係，並非只裸張姓；由原墓誌/文學名定位至原正史本人，不把丈夫資料庫估年套妻。')])
    for ident,note in {8726:'原SL爨𬱟為李勢將冬十二月奔晉，非同卷爨亮/爨琛。奔降非死亡，僅SL晚層未補嚴格年，生卒不詳。',8727:'王憲燕參軍與劉明諫而被黜後封裕奏復本官，原死刑已恕非345處死；同人兩key閉，無生卒。',8728:'七key真混後趙王波上司王洽與王導子王洽，不能用王導子358死套趙將。主支JS36卒vsSS注中興書26卒保齡異，撤舊頂層323/358，358只主支。SS03王車騎注王洽官號仍待橋；GS法汰387卒非王洽387活，保時序疑點不拿僧卒推延。',8730:'庾與廋同魏公同陝城叛亂、七子獲原、父健宗室鏈，四key與8731全閉可硬去重；不混同別的苻堅之弟趙公雙。',8731:'兩TJkey苻健子魏公廋351封、368十二月賜死七子獲原，非368七月雙武之斬，保父子宗室譜，不取WS甘露建國22廣年當卒。',8732:'八key晉公柳健少子，同強太后欲立與蒲坂叛亂可橋；SL07枹罕誤地與WS稱堅從弟（TJ父健推從兄）源差保留，四公同役唯一晉公柳並非另人。368九月本人連妻子斬，未知出生。',8733:'五key苻健子燕公武351封，367安定反、368奔上邽七月被斬，妻子獲宥不連坐，與苻雙同死年而不同家父，無生日。',8734:'主支苻堅弟/雄子雙河南公後趙公上邽，368七月被斬可證只branch；SL07苻雙據陝且柳枹罕與各早源魏公廋陝/柳蒲坂不合，書源姓名與城皆錯或指代他人尚未唯一校清，不把主支死亡套整條。',8706:'公開/本地原典補核已得晉書苻堅夫人張氏與385中山公詵及張夫人並自殺具名，解先前MQWW未取的身份hold；原CBDB丈夫31645苻堅夫妻關係同，未知妻生年，父夫年48非妻年齡。獨立JS385本人死亡可保，不獨依SL。'}.items():results[ident]['unresolved'].append(note)
    round34b_reads={8735:[('jk_02','母裴氏名穆太傅主簿遐女'),('js_00','裴氏名穆，长水校尉绰孙')],8736:[('jk_02','谢石讥之云二何佞于佛'),('js_13','石字石奴'),('js_14','黄门侍郎谢石'),('sl_05','败谢石于淝南'),('gs_01','遣征虏将军谢石徐州刺史谢玄拒之'),('ssxy_01','裒子石娶恢小女名文熊')],8737:[('js_11','谢万为豫州都督'),('js_13','万字万石'),('sl_03','北中郎将谢万时据梁宋'),('sl_06','及明果遣谢万追襄'),('tj_16','谢万为从事中郎'),('tj_18','以吴兴太守谢万为西中郎将'),('js_04','尝问谢万云'),('js_09','谢万尝造恬'),('js_12','谢万讥之云'),('ssxy_00','谢万字万石太傅安弟也'),('ssxy_01','谢万作八贤论'),('ssxy_02','王右军道谢万石'),('ssxy_03','万取太原王述女名荃')]}
    for ident,qs in round34b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第34批B謝氏及杜皇后母多源逐鍵實讀','Read named individual full passage, separating family deaths/ages and mere military defeats') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round34b')
    shidi=evidence(SOURCE/'_cache/texts/资治通鉴.txt','十二月，庚子，尚书令南康襄公谢石卒','資治通鑑太元十三388十二月庚子謝石卒','親讀太元十三388年header及次年太元14之前本死亡紀日，年號年映CE388但農曆十二月勿換成公曆388十二月；未聲稱公曆月日。')
    shiage=chunk_ev('js_13','卒，时年六十二','晉書謝石傳卒62','同本傳謝石石奴、謝安弟，62配獨立TJ388死錨推327虛歲；不是謝萬42。')
    results[8736].update(review_status='primary_text_corroborated',historical_death=date(388,shidi,month=12,day_ganzhi='庚子'),historical_birth=date(327,shiage,age=62))
    results[8736]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8736]['historical_birth']['death_anchor_source']=shidi['citation']
    results[8736]['evidence'].extend([shidi,shiage])
    results[8737]['source_reported_age_at_death']={'age':42,'calendar_year':None,'source':'晉書謝萬傳'}
    resultsev=chunk_ev('js_00','宁康二年，以后母裴氏为广德县君','晉書裴穆374封廣德縣君','孝武帝立後寧康二374本人裴氏封縣君，是在世受封；此前341卒21為女杜陵陽，不是母。')
    results[8735]['evidence'].append(resultsev)
    results[8735]['attested_activity_years']=[374]
    results[8735]['appearance_upper_bound']=374
    for ident,note in {8735:'裴穆裴遐女綽孫王衍外孫、杜乂妻杜陵陽母，兩key全部家系封號杜姥宅合。374受封明活，34121卒是女非母；父隨越死非穆死，不把宅古今里數當年齡，未知生卒。',8736:'六key謝石石奴謝安弟尚書令南康公淝水都督，JS62卒配TJ388十二月庚子本人紀日推327生；生為虛歲推不是生日。JK二何佞佛作謝石vsJS/SS作謝萬的諷語作者異記保留，不能僅一則相同諷語把謝石謝萬兩兄弟硬合；二者其他父字官爵均清楚。',8737:'十三key謝萬字萬石、安弟王述女荃夫、八賢論與豫州都督同生平，42卒未具CE，本輪全JS/TJ未見精卒年，不補通行361/320。SL06追姚襄353軍事記名謝萬與他源將帥記載異同保，作晚重編史事待校不以其一役推確年；359軍潰非死亡。'}.items():results[ident]['unresolved'].append(note)
    round34c_reads={8738:[('sl_02','司农中郎将费覇帅工匠四千')],8739:[('sl_02','征东将军邓恒将兵数万'),('sl_03','赵将邓恒将兵数万'),('tj_16','征东将军邓恒将兵数万'),('tj_17','邓恒据安乐'),('sl_05','遣将军邓恒率众数万屯于乐安')],8740:[('gs_01','释昙徽。河内人')],8741:[('js_12','韩伯，字康伯'),('tj_16','颍川韩伯为谈客'),('js_14','清谈文章之士韩伯'),('js_16','与太常韩康伯邻居'),('ssxy_00','韩康伯时为丹阳尹'),('ssxy_01','韩康伯病拄杖前庭'),('ssxy_02','祭叔子云韩康伯'),('ssxy_03','韩康伯母隐古几')],8742:[('js_11','高崧，字茂琰'),('tj_17','抚军司马高崧言于昱'),('tj_18','侍中高崧谏曰'),('js_13','中丞高崧戏之'),('ssxy_00','阿酃崧小字也'),('ssxy_03','髙灵时为中丞')],8743:[('tj_16','帝二子丕、弈，皆在襁褓'),('wsh_00','司马弈臣桓温'),('wsh_02','以奕为海西县公'),('js_01','弈至黄门郎，先望卒')],8745:[('sl_02','中黄门严生不协'),('tj_16','赵中黄门严生恶尚书朱轨')]}
    for ident,qs in round34c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第34批C名士僧徒與司馬同名逐鍵實讀','Read specific source paragraphs and adjudicate candidate scope, not mere same-book name presence') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round34c')
    huiev=chunk_ev('gs_01','以晋太元二十年卒','高僧傳曇徽395卒73','親讀道安弟子河內、襄陽到荊州上明、圖安像，中食後右脅化，春秋73同段。太元20=395，虛歲反推323。')
    results[8740].update(review_status='primary_text_corroborated',historical_death=date(395,huiev),historical_birth=date(323,huiev,age=73))
    results[8740]['evidence'].append(huiev)
    results[8741]['source_reported_age_at_death']={'age':49,'calendar_year':None,'source':'晉書韓伯傳'}
    hanev=chunk_ev('js_12','卒，时年四十九，即赠太常','晉書韓伯傳49卒但無絕對年','本傳任領軍疾病改太常未拜卒49，無配CE；不從通行332380或與其母後事倒鎖年。')
    results[8741]['evidence'].append(hanev)
    dengev=chunk_ev('tj_17','王午闻魏败，时邓恒已死','資治通鑑永和八352魯口王午稱安國王追述鄧恆已死','已死為352此前，不能等同352年卒。350魯口拒燕本人活動後到352以前，保死亡上界不擅定某年。')
    results[8739]['evidence'].append(dengev)
    results[8739]['source_reported_death_bound']={'year':352,'relation':'already_dead_by_narrative_year','exact_death_year':False,'source':dengev['citation']}
    results[8739]['unresolved'].append('352王午時已死只死亡上界，不可當352精卒年；350鹿勃早襲燕敗是部將死傷，不足指鄧恆死。')
    yidi=evidence(SOURCE/'_cache/texts/晋书.txt','太元十一年十月甲申，薨于吴，时年四十五','晉書廢帝紀海西公司馬奕386十月甲申卒45','親讀廢帝諱奕延齡哀帝母弟，371被廢吳居與許龍詐迎不從後本死亡句；342為386−45+1推，僅海西公支不套司馬望子。')
    yibirth=date(342,yidi,age=45)
    yideath=date(386,yidi,month=10,day_ganzhi='甲申')
    yideath.update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8743].update(review_status='identity_conflict',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='mixed_eastern_jin_deposed_emperor_yi_and_western_jin_simawang_son_yi',component_identities=[dict(identity='海西公司馬奕字延齡成帝子哀帝弟',keys=[k for k in results[8743]['keys'] if not k.startswith('js:js_01:')],historical_birth=yibirth,historical_death=yideath),dict(identity='司馬望長子弈，黃門郎，先父卒',keys=['js:js_01:195:司馬弈'],historical_birth=None,historical_death=None,source_reported_death_bound={'year':271,'relation':'before_father_death','source':'晉書司馬望泰始七271卒67、子弈先望卒'})])
    results[8743]['evidence'].append(yidi)
    results[8728]['component_identities'][1]['candidate_duplicate_of']=8871
    results[8728]['evidence'].append(chunk_ev('tj_17','赵扬州刺史王浃举寿春降','資治通鑑349王浹趙降晉（與8728趙王洽源支候選）','JS王洽王波俱降與TJ349王浹壽春降可能同人形異，尚未全核8871兩key，先只候選不整條並王導子。'))
    for ident,note in {8738:'費霸司農中郎將為石虎造獵車、帶工4000是役夫數非本人年齡，民被誣死非費霸死亡，僅SL無可取精生卒。',8739:'五key邓恒趙征東樂安/安樂拒燕、慕容霸對峙到魯口王午同人；城名樂安安樂傳抄保留。TJ352時已死只上界，沒卒精年不推填。',8740:'單key曇徽道安弟子12投16剃、河內上明寺、太元20395卒73清楚，不套前法遇60或後昙戒70；生395−73+1=323名義推，非生日。',8741:'八key韓伯即韓康伯頴川母殷氏舅殷浩同清談太常；本傳49卒無CE，未見精卒年不補通行380332。SS稱殷浩妹、JS姊有親屬長幼異說保，吳隱之仕進與母哭聲非韓伯死年。',8742:'六key高崧茂琰父悝、阿酃/SS03髙靈同新亭送謝安中丞獨特事件橋；父悝13孝養與停父喪5年不是崧生日年齡，哀帝服藥諫後免卒家無精卒年。',8743:'四key真混：三key東晉海西公成帝子奕（文字弈）與JS01西晉司馬望長子弈。海西公386十月甲申45卒推342只branch；望子弈先父泰始七271卒不可能同人。全條未拆前不套任一支日期或重定向。',8745:'嚴生趙中黃門與朱軌不協進讒道路，蒲洪諫後朱軌竟被誅屬朱軌，不是嚴生死；SL描寫不罪而TJ末竟誅是朱軌事異，不造嚴生生卒。'}.items():results[ident]['unresolved'].append(note)
    round34d_reads={8744:[('js_07','楚字元孙'),('js_18','参军周楚、孙盛守辎重'),('sl_09','叅军孙盛周楚将羸兵守辎重'),('tj_16','留参军孙盛、周楚'),('tj_18','朱序、周楚击司马勋')],8746:[('sl_08','护军梁式执太守宋晏'),('sl_09','大夏护军梁彧[一作式]执太守宋晏'),('tj_16','护军梁式执太守宋晏'),('js_15','大夏护军梁式执太守宋晏'),('js_16','护军梁式执太守宋晏')],8747:[('sl_08','牧府相司马张耽'),('tj_16','凉州司马张耽'),('js_15','牧府相司马张耽'),('wsh_04','司马张耽荐主簿谢艾')],8748:[('sl_05','子马奴潜谋逃归杀之'),('tj_18','子马奴潜谋逃归，杀之而行')],8749:[('sz_00','昙备，本姓陶，丹阳建康人也')],8750:[('sl_02','执护军曹权胡宣'),('sl_08','执护军曹权胡宣'),('tj_16','执护军曹权、胡宣')]}
    for ident,qs in round34d_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第34批D周氏蜀事/涼使與比丘尼各人原文實讀','Read actual named passages, distinguishing captured officials from executed people and tracking shared unique events') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round34d')
    chuev=evidence(SOURCE/'_cache/texts/资治通鉴.txt','三月，壬辰，益州刺史建成定公周楚卒','資治通鑑咸安元371三月壬辰周楚卒','整年header咸安元371（年末改號，年初亦太和六）下本人原卒，父周撫365卒不屬楚。JS蜀亂太和中後是歲卒可包同371，無另明年衝突。')
    results[8744].update(review_status='primary_text_corroborated',historical_death=date(371,chuev,month=3,day_ganzhi='壬辰'))
    results[8744]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8744]['evidence'].append(chuev)
    manuev=chunk_ev('tj_18','子马奴潜谋逃归，杀之而行','資治通鑑太和四369十一月慕容垂殺子馬奴','本人馬奴被父殺、同段另少子麟逃還告狀未被殺，兩子不同；十一月辛亥朔垂出邺後同逃行役，不把出邺干支當馬奴死日。')
    results[8748].update(review_status='primary_text_corroborated',historical_death=date(369,manuev,month=11))
    results[8748]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8748]['evidence'].append(manuev)
    tanbeiev=chunk_ev('sz_00','年七十三，泰元二十一年','比丘尼傳北永安寺曇備396卒73','原文接卒，弟子曇羅續任；泰元/太元21=396，虛歲396−73+1=324。前354何后建寺非死亡，後慧湛康明感何充死亡非此尼。')
    results[8749].update(review_status='primary_text_corroborated',historical_death=date(396,tanbeiev),historical_birth=date(324,tanbeiev,age=73))
    results[8749]['evidence'].append(tanbeiev)
    for ident in [8746,8747,8750]:
        results[ident]['attested_activity_years']=[346]
        results[ident]['appearance_upper_bound']=346
        results[ident]['appearance_constraint_explanation']='親讀TJ16永和二346header下重華繼父後武街/金城/大夏首次戰事，本人具名被執送書/薦謝艾，非後永和三347另一戰次；最晚上界不是初仕或出生。'
    for ident,note in {8744:'五key周楚元孫周撫子，347彭模同孫盛守輜重、365繼益州、366討勳同人。原TJ全本371三月壬辰卒明本人，JS太和中蜀亂後是歲楚卒與371年末前太和六可合，非父365卒；未知出生。',8746:'五key宋晏前涼大夏太守被梁式執城降麻秋，後持書誘宋矩；自殺是宋矩及其妻子而非宋晏，勿346被執當卒。TJ此第一輪永和二346，不用後347另一輪戰年誤移本活動。',8747:'四key張耽涼司馬薦主簿謝艾退麻秋完全同役，WS同重華相謝艾並非他代人；TJ永和二346本人薦艾活動，無本人生卒，勿把謝艾/綦母安被殺算己。',8748:'馬奴慕容垂子逃行欲歸為父殺，另子麟逃還告狀是別人；369十一月出邺後路上明死無精日，未知出生，不把父384稱帝當己仍活。',8749:'曇備陶姓建康比丘尼，354何后造北永安寺，396太元21卒73推324；15許不嫁出家非卒歲、母生死不詳，與前康明感/後慧湛不是同人。',8750:'曹權前涼護軍武街被王擢執，胡宣同行另人，7000餘戶是遷戶數非兵齡；TJ永和二346具名被執，未明被殺，生卒不詳。'}.items():results[ident]['unresolved'].append(note)
    round35a_reads={8751:[('sl_09','左战帅李伟劝艾乘马'),('js_15','左战帅李伟劝艾乘马'),('sg_03','丰弟翼及伟仕数岁间并歴郡守')],8752:[('sl_03','故赵将李述狥赵魏'),('tj_18','故赵将李述徇赵、魏')],8753:[('sl_08','大夏护军梁式执太守宋晏'),('tj_16','护军梁式执太守宋晏'),('js_15','大夏护军梁式执太守宋晏'),('js_16','护军梁式执太守宋晏'),('sl_09','大夏护军梁彧[一作式]执太守宋晏')],8754:[('sl_09','获别将乐髙')],8755:[('sl_09','杨谦击势将申阳走之')],8756:[('sl_08','别驾从事索遐议曰'),('tj_16','别驾从事索遐曰'),('js_15','别驾从事索遐议曰')],8757:[('sl_02','执护军曹权胡宣'),('sl_08','执护军曹权胡宣'),('tj_16','执护军曹权、胡宣')],8758:[('sl_09','建武将军江夏相袁乔'),('tj_16','江夏相袁乔劝之曰'),('js_14','江夏相袁乔深器之'),('ssxy_00','乔字彦升陈郡人父瓌'),('ssxy_01','孙兴公作庾公诔袁羊曰')],8759:[('sl_08','征南将军裴恒将之'),('sl_09','前锋都督裴恒帅歩骑七万'),('tj_16','征南将军裴恒将之'),('js_15','征南将军裴恒御之')],8761:[('js_17','郭翻，字长翔')],8764:[('sl_09','军正将军任遐曰风为号令')]}
    for ident,qs in round35a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第35批A同卷前涼/桓溫軍臣逐人實讀','Read named role across original passages in a grouped chapter review; captures and transfers are not death evidence') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round35a')
    results[8751].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_three_kingdoms_lifeng_brother_and_eastern_jin_liang_xieai_battle_commander',component_identities=[dict(identity='曹魏李豐弟李偉，新平扶風郡守',keys=['sg:sg_03:381:李偉'],historical_birth=None,historical_death=None),dict(identity='前涼謝艾左戰帥李偉',keys=['sl:sl_09:146:李偉','js:js_15:84:李偉'],historical_birth=None,historical_death=None)])
    shuev=chunk_ev('tj_18','温使故赵将李述徇赵、魏，腾又与虎贲中郎将染干津击斩之','資治通鑑太和四369桓溫伐燕李述被斬','舊趙將轉晉受桓溫使，悉羅騰染干津斬本人；前段段思被擒不是同人，前八月後九月敘次只保年。')
    results[8752].update(review_status='primary_text_corroborated',historical_death=date(369,shuev))
    results[8752]['evidence'].append(shuev)
    suoev=chunk_ev('tj_16','索遐曰：“风为号令，今旌旗指敌，天所赞也。”','資治通鑑永和三347軍正索遐風旗占勝','與SL09任遐軍正同風號令指敵天贊全句、謝艾枹罕旗同場，任索姓差可校同人；JS15同別駕轉軍正。')
    results[8756]['evidence'].append(suoev)
    results[8764].update(review_status='duplicate',duplicate_of=8756,identity_status='ren_suo_xia_surname_variant_same_junzheng_flag_wind_speech',duplicate_basis='SL09軍正任遐同謝艾枹罕旌旗占風整句；TJ/JS別駕索遐轉軍正，唯一同官同役同句，target三keys全閉')
    results[8756]['evidence'].extend(results[8764]['evidence'][-1:])
    results[8764]['evidence'].append(suoev)
    results[8758]['evidence'].append(chunk_ev('js_14','乔字彦叔','晉書袁瑰附袁喬傳本人主傳補讀','親讀父瑰、褚裒辭交、從桓溫入蜀、封湘西伯以及36卒；這是一個本人主傳而不是習鑿齒祖父事件。'))
    results[8758]['source_reported_age_at_death']={'age':36,'calendar_year':None,'source':'晉書袁喬傳'}
    results[8758]['source_reported_chronological_problem']={'source':'世說言語孝武講孝經袁羊/袁喬注','event_year_reported':375,'basis':'注寧康三九月九日，主文袁羊下注喬字彥升父瓌；與晉書彥叔初官及36卒所涉年代待考','do_not_treat_as_attested_activity':True}
    for ident,note in {8751:'三key合入兩代李偉：SG李豐弟正始間新平扶風郡守、前涼謝艾左戰帥李偉相隔近百年且宗族角色不同，分支不套生卒；李豐254被殺與弟翼被收不能無句當李偉卒。',8752:'李述舊趙將為桓溫徇趙魏被悉羅騰染干津斬，369原TJ本人死確年；非華陽李特兄李述或前段段思被擒，生年不詳。',8753:'五key梁式/梁彧原SL注一作式，前涼大夏護軍執太守宋晏獻城同事件；宋矩自刎非梁式，無本人生卒。',8754:'樂高成漢別將為晉楊謙獲，獲非斬，不套緊接李奕單騎突門346死，僅SL晚層生卒不詳。',8755:'申陽成漢將被晉楊謙擊走，別將樂高獲是另一人，走非死，僅SL無生卒。',8756:'三key索遐別駕論立秋省搜狩與後轉軍正薦謝艾同人，父喪/先主喪不是其本人死；8764任遐原姓異但軍正同風旗全句可實去重，不造兩名軍正。',8757:'胡宣與曹權為武街護軍被王擢執，不等同稍後宋矩自殺或涼國張重華卒；未知生卒，346在世不等於346初任。',8758:'五key原來源名喬/袁羊父瓌官江夏相湘西伯同身份鏈，JS字彥叔/SS彥升源異保。原SS孝武講孝經寧康三375一則時間與喬早官從蜀36卒脈絡有疑，不能硬作375在世證，未取得卒精年不補生卒。主傳外只讀姓名命中不算，此輪已實讀主傳與各則。',8759:'四key裴恒前涼征南將軍廣武持久與後前鋒都督出隴同張重華軍臣，地域主君官序一致未見別人；346被批久不戰非伏誅，無生卒。',8761:'郭翻長翔武昌父察伯讷臨川釣獵、咸康末庾翼屈訪小舟，不套前翟莊56卒或後辛謐拒冉閔死。本人卒家無年，郭文/郭翻不同傳人，未知生卒。',8764:'SL09任遐軍正占風旗與TJ索遐軍正整句同場，任索姓字異可橋8756；唯一key已全核，本條重定向不獨依SL定年，日期仍不詳。'}.items():results[ident]['unresolved'].append(note)
    round35b_reads={8762:[('sl_03','永乐民郭陵见之诣皝')],8765:[('js_15','诏遣侍御史俞归'),('sl_09','并送御史俞归还京师'),('tj_16','遣侍御史俞归至凉州'),('tj_18','并送御史俞归东还')],8766:[('sl_02','将军刘浑率步骑二万会秋'),('tj_16','复遣其将刘浑'),('js_15','复遣其将刘浑')],8767:[('tj_18','令杀牙门孟妫'),('sl_06','乃杀牙门孟妫')],8768:[('sl_03','左卫将军孟髙讨擒之'),('tj_18','燕左卫将军孟高讨擒之')],8769:[('sl_02','凉将宋秦等率戸二万'),('tj_16','张重华将宋秦等帅户二万')],8770:[('js_11','希字始彦'),('js_13','时庾希入京口'),('sl_03','北中郎将庾希'),('tj_18','庾希自下邳退屯山阳'),('ssxy_01','希字始彦司空氷长子'),('ssxy_03','桓温杀庾希弟倩')],8771:[('tj_16','武成太守张悛曰'),('js_15','武城太守张悛曰')],8772:[('tj_16','虎命司虞张曷柱调之')],8773:[('sl_02','凉别将张瑁自间道'),('tj_16','别将张瑁自间道'),('js_15','张瑁从左南缘河')],8774:[('tj_16','宁戎校尉张璩从悛言'),('js_15','宁戎校尉张璩从之')],8775:[('tj_16','虎使尚书张群发近郡男女十六万人'),('sl_02','虎使尚书张郡[一作群又作麟]')]}
    for ident,qs in round35b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第35批B同章涼燕/北魏族譜實讀逐條裁定','Read actual named roles, separating kin death, captured officials, public-works casualties, and source character variants') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round35b')
    weiev=evidence(SOURCE/'_cache/texts/魏书.txt','祖楷，晋建威将军、长乐清河二郡太守。父逵','魏書韋閬傳祖楷父逵','親讀韋閬友觀京兆杜陵、祖楷晉建威長樂清河守、父逵慕容垂大長秋與避薊任咸陽武都；與原CBDB楷子逵橋，16年卒是孫閬不是楷。')
    results[8763].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='original_cbdb_weikai_son_weikui_matched_weishu43_and_xintangshu74_genealogy',last_manual_review_batch='round35b_cbdb_primary')
    results[8763]['evidence'].extend([dict(citation='原CBDB192763韋楷親屬與日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='KIN子韋逵source32033；death407/index344type29；notes Death year estimated by Tackett(cc407)；ctext633028/Q45418909；補遺7:138–139/文物1999.1:93/洛陽新獲續504。',interpretation='407明估卒、344索引非出生；原墓誌未取，ctext經Jina只取到無本文頁，不能稱該原引用已讀。'),weiev,dict(citation='新唐書卷074上東眷韋氏原譜親讀',url='https://zh.wikisource.org/wiki/新唐書/卷074上',retrieval='agent-reach Jina Reader',excerpt='東眷韋氏：穆曾孫楷，晉長樂、清河二郡守。生逵，慕容垂大長秋卿。生閬，字友觀，避地薊城，後魏太武召為咸陽太守。',verification='individually_read_original_text',interpretation='同祖楷→逵→閬兩層官籍與原CBDB子逵橋，不從後孫任郡16年倒算楷卒。')])
    guiev=chunk_ev('tj_18','五月，庚午，令杀牙门孟妫','資治通鑑太和五370五月庚午孟媯被慕容令殺','本人沙城牙門孟媯明殺，城大涉圭後反殺慕容令、再被亮誅是另一人，日庚午不套涉圭之死。')
    results[8767].update(review_status='primary_text_corroborated',historical_death=date(370,guiev,month=5,day_ganzhi='庚午'))
    results[8767]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8767]['evidence'].append(guiev)
    gaodie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','馀贼从帝射高，杀之。艾朗见高独战，亦还趋贼，并死','資治通鑑太和五370燕亡出奔孟高護主戰死','本年370燕亡後暐與左衛孟高武衛艾朗奔龍城，福祿遇盜孟高戰死；幾日轉戰精日不明，保370年，不把369擒孫元當本人被殺。')
    results[8768].update(review_status='primary_text_corroborated',historical_death=date(370,gaodie))
    results[8768]['evidence'].extend([gaodie,evidence(SOURCE/'_cache/texts/资治通鉴.txt','孟高扶侍','資治通鑑370孟高護慕容暐','本次續讀左衛將軍孟高扶暐、經護二王到遇盜死，與前369同燕左衛將軍孟高官名一貫。')])
    xideath=evidence(SOURCE/'_cache/texts/资治通鉴.txt','秋，七月，壬辰，拔其城，擒希、邈及其亲党，皆斩之','資治通鑑咸安二372七月壬辰庾希庾邈伏誅','本年header372、六月後庾希邈武遵入京口，周少孫破城同日擒斬。371殷庾初禍時希逃不是已死。')
    results[8770].update(review_status='primary_text_corroborated',historical_death=date(372,xideath,month=7,day_ganzhi='壬辰'))
    results[8770]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8770]['evidence'].append(xideath)
    results[8772]['source_name_variants']=['張曷柱（TJ）','張昌柱（SL02同司虞調蒼麟白鹿駕芝蓋）']
    results[8772]['evidence'].append(chunk_ev('sl_02','虎命司虞张昌柱调之','十六國春秋晚重編同養麟官張昌柱字異','僅校同司虞蒼麟白鹿調芝蓋唯一事件姓名字異，不以晚源建武年供精生卒。'))
    results[8775]['source_name_variants']=['張群（TJ）','張郡／群／麟（SL02原校注）']
    results[8765]['last_attested_activity']=363
    results[8765]['evidence'].append(chunk_ev('tj_18','并送御史俞归东还','資治通鑑興寧元363張天錫送俞歸歸晉','明活363本人送東還，非逝後歸葬；使張重華任命與後留至天錫可同政治生平，無明卒年。'))
    for ident,note in {8762:'郭陵永樂民見築鐵上報慕容皝而封關內侯，1174是鐵具件數非年齡，僅SL晚層未補確年生卒。',8763:'韋楷原CBDB子逵明橋魏書韋閬祖楷父逵與新唐書譜，撤身份hold。407明Tackett估卒、344index29非生日；未取原墓誌不宣確日，祖孫不同不把閬16年卒/俊57歲套楷。',8765:'四key晉侍御史俞歸使張重華、留涼至張天錫363東還同人同官，重華死353與俞歸存活363不同事件；未知生日死亡，363已證活動可約束過早估卒但不推死年。',8766:'三key劉渾後趙將率2萬援麻秋枹罕，明戰敗並未說本人死，勿合南朝劉渾或劉混同音別人，無生卒。',8767:'孟媯燕沙城牙門被慕容令370五月庚午殺，與孟高燕左衛將軍異人；涉圭因恐而佯附後殺令再被誅都是後事不同人，未知出生。',8768:'孟高燕左衛將軍369擒孫元與370護慕容暐逃亡遇盜戰死同官鏈，艾朗同死另人；無明生歲，不反向推生年。',8769:'宋秦涼將率2萬戶降趙，非被殺，2萬戶不能當兵齡；與宋矩/宋晏/宋纖各不同人，未知生卒。',8770:'六key庾希始彥冰長子、海西公妃兄徐兖刺史、京口起兵同傳鏈；371逃海陵不是371死，372七月壬辰破城皆斬原TJ明。與亮子叔和庾羲8722不同，生年不詳。',8771:'張悛武成/武城太守力主守枹罕外城、張璩從之，兩key同役。郎坦拒議後引李嘉通趙，李嘉斬是別人，不套張悛死，無生卒。',8772:'張曷柱趙司虞調蒼麟白鹿駕芝蓋，同SL張昌柱姓名字異角色全合。暴風雨死數萬是築苑役夫，非本人生卒；未知生卒。',8773:'張瑁涼別將間道截麻秋軍後勝、後被麻秋襲敗3000級未說本人死；三key同軍職同役連，346/347戰事分次不把軍敗當死，無生卒。',8774:'張璩寧戎校尉枹罕守城殺李嘉、使宋修張弘辛挹郭普拒賊，與後趙叛涼張據族名近待校不先合。趙軍萬人死不是張璩死，無生卒。',8775:'兩key張群/張郡原SL校作群又麟，趙尚書督華林苑長牆役同人，風雨役夫數萬死非本人死，不套宋朱軌因災被誅，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round36a_reads={8776:[('sl_03','亮为渤海王'),('tj_17','亮为勃海王'),('tj_18','镇东将军勃海王亮镇龙城'),('sl_06','镇东将军渤海王亮镇龙城')],8777:[('sl_04','慕容令袭燕龙城不克而死'),('sl_05','世子令断后'),('tj_18','慕容令自度终不得免'),('sl_06','遂与令等奔秦')],8778:[('tj_18','杀城郎慕容仓'),('sl_06','杀城郎慕容仓')],8779:[('sl_03','弟桓为宜都王'),('sl_05','朱嶷追慕容桓于辽东'),('tj_17','弟桓为宜都王'),('tj_18','镇北将军宜都王桓'),('sl_06','慕容凤字道翔宜都王桓之子')],8780:[('sl_02','太子宣讨鲜卑斛糓提'),('tj_16','赵太子宣击鲜卑斛谷提')],8781:[('hy_00','遣昝坚等将之'),('js_18','其将邓嵩、昝坚劝势降'),('sl_09','前将军昝坚等帅数千人'),('tj_16','前将军昝坚等将之')],8782:[('tj_18','梁国内史沛郡朱宪'),('wsh_02','真子双之等杀梁国内史朱宪')],8783:[('sl_03','汝南太守朱斌承虚来袭许昌'),('tj_18','汝南太守朱斌乘虚袭许昌')]}
    for ident,qs in round36a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第36批A燕亡與成漢歸晉同篇多人逐鍵親讀','Read exact named office, family and shared event; preserve variant responsibility and avoid inferring death from defeat') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round36a')
    liangdie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','宜都王桓杀镇东将军勃海王亮，并其众','資治通鑑太和五370燕亡慕容桓殺亮','370燕亡郭慶北追後，宜都王桓殺龍城鎮東勃海王亮，與354儁封其子勃海王亮、慕容令370五月欲襲同官居地閉鏈；未知精月日保年。')
    results[8776].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(370,liangdie))
    results[8776]['evidence'].append(liangdie)
    lingdie=chunk_ev('tj_18','癸酉，涉圭因侍直击令','資治通鑑太和五370五月癸酉慕容令之死','本月庚午殺孟媯後癸酉涉圭襲令，令單馬走、追至薛黎澤擒而殺。前秦金刀詐令回燕而死，非369父逃亡時死。')
    lingkill=chunk_ev('tj_18','涉圭追令至薛黎泽，擒而杀之','資治通鑑370慕容令被涉圭殺','與上癸酉同日連續追殺明敘，之指慕容令非其父垂或弟麟。')
    results[8777].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(370,lingkill,month=5,day_ganzhi='癸酉'))
    results[8777]['historical_death'].update(source='資治通鑑太和五370五月癸酉涉圭追慕容令殺',precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8777]['evidence'].extend([lingdie,lingkill])
    cangdie=chunk_ev('tj_18','遂帅谪戍士东袭威德城，杀城郎慕容仓','資治通鑑太和五370五月沙城起兵殺慕容倉','庚午孟媯被殺後、癸酉令死前，同月連續起事殺威德城郎倉；倉非慕容令，未知精日。')
    results[8778].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(370,cangdie,month=5))
    results[8778]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8778]['evidence'].append(cangdie)
    huandie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','郭庆遣将军朱嶷击之，桓充众单走，嶷获而杀之','資治通鑑太和五370宜都王慕容桓被朱嶷殺','接桓殺亮取眾奔遼東、韓稠先降秦不納桓、郭慶遣朱嶷擒殺，同SL05郭慶傳與SL06其子鳳父桓朱嶷所殺，五key全橋。')
    results[8779].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(370,huandie))
    results[8779]['evidence'].append(huandie)
    zhudie=chunk_ev('tj_18','袁真以梁国内史沛郡朱宪及弟汝南内史斌阴通大司马温，杀之','資治通鑑太和五370正月己亥朱憲兄弟被袁真殺','本條在370春正月己亥下明具朱憲與弟汝南內史斌（朱斌）；配此前363364汝南太守朱斌退壽春同地官。WS真子雙等責任與叛前壓敘無明年月，不擅推另一確年369。')
    for ident in [8782,8783]:
        results[ident].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(370,zhudie,month=1,day_ganzhi='己亥'))
        results[ident]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(zhudie)
    for ident,note in {8776:'四key慕容儁子亮勃海王354受封、鎮東龍城拒慕容令、燕亡叔宜都王桓殺亮並眾同主線，370死年明TJ。原子亮不是父慕容皝之子，未知出生。',8777:'四key慕容垂世子令、助父奔秦、王猛金刀詐歸燕、沙城起兵、涉圭追殺同人。370五月癸酉原TJ明連追殺，垂/麟另人；不把SL冬十一月父出邺己朔誤當令卒，未知生年。',8778:'慕容倉威德城郎為沙城起兵慕容令所殺370五月，非慕容令本人或後慕容亮被桓殺，未知生日精日。',8779:'五key宜都王桓儁弟/鳳父、鎮北征敕勒後燕亡殺亮、遼東拒納被朱嶷殺，370獨立TJ明死；鳳11歲是子不是父卒齡，不反推桓出生。',8780:'斛谷提鮮卑首領被趙太子石宣破，3萬級為部眾，未明首領本人被殺，不套343戰役當卒，生卒不詳。',8781:'四key成漢前將軍昝堅拒晉錯路犍為、兵自潰、劝李勢降，降及軍潰非個人死；諸史347同一事不造初仕/生日，無生卒。',8782:'朱憲梁國內史沛人與弟朱斌為袁真處死370正月己亥原TJ。WS真子雙之等殺朱憲放在真叛前屬無明年壓敘/責任異說，不由此另造369確卒；原top370保並補精日。',8783:'朱斌前363364汝南太守退壽春與370朱憲弟汝南內史斌同姓同地方官與壽春陣營可橋，370正月己亥具名皆殺非364逃壽春當死；未知生年。'}.items():results[ident]['unresolved'].append(note)
    round36b_reads={8784:[('js_18','告交州刺史朱蕃'),('tj_16','檄交州刺史朱蕃')],8785:[('yi_02','交州刺史朱籓后遣督护刘雄')],8786:[('tj_16','教军士李嘉潜引赵兵千馀人登城'),('js_15','璩戮李嘉以徇')],8787:[('yi_02','又遣督护陶缓、李衢讨之')],8788:[('sl_02','枹罕护军李逵率众七千来降'),('tj_16','枹罕护军李逵帅众七千降于赵'),('hhs_03','劭邑人李逵，壮直有高气')],8789:[('sl_02','重华使将军牛旋杨康等来御'),('tj_16','别将杨康败刘宁于沙阜')]}
    for ident,qs in round36b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第36批B交州及前涼同役逐鍵實讀','Read each named person and distinguish surname variants from genuinely different historical persons') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round36b')
    results[8785].update(review_status='duplicate',duplicate_of=8784,identity_status='same_jiaozhou_governor_zhu_fan_with_fan_character_variant',duplicate_basis='朱蕃/藩/籓同交州刺史，同范文横山界交涉、遣劉雄守日南的唯一事件；兩方全部keys實讀。')
    results[8784]['evidence'].append(chunk_ev('yi_02','交州刺史朱籓后遣督护刘雄','梁書林邑朱籓與晉書朱蕃同役字異','同主君文、劉雄督護、日南與横山求界皆合，不因梁書留三年與永和五年文卒叙次矛盾另造兩朱刺史。'))
    jiadie=chunk_ev('js_15','璩戮李嘉以徇','晉書張重華傳李嘉被張璩殺','TJ永和三347同枹罕圍城郎坦教李嘉引趙兵、張璩守退事件獨立定年；JS本人明戮，347年月精度保僅年。')
    results[8786].update(review_status='primary_text_corroborated',historical_death=date(347,jiadie))
    results[8786]['evidence'].append(jiadie)
    results[8788].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_former_liang_baohan_guard_and_late_han_xushao_fellow_townsman',component_identities=[dict(identity='前涼枹罕護軍李逵，率7000降趙',keys=[k for k in results[8788]['keys'] if not k.startswith('hhs:')],historical_birth=None,historical_death=None),dict(identity='後漢許劭邑人李逵，先善後隙',keys=[k for k in results[8788]['keys'] if k.startswith('hhs:')],historical_birth=None,historical_death=None)])
    for ident,note in {8784:'朱蕃交州刺史與梁書朱籓同范文請横山界/遣劉雄守日南，可去重8785。347范文殺夏侯覽、七月劉雄被殺皆非朱蕃；梁書留日南三年與五年文卒時間壓縮衝突不轉作本人生卒，未知生卒。',8785:'唯一梁書朱籓key已讀，與8784兩key同官同范文横山交涉及劉雄派守，可整體去重；字藩籓蕃異，不另造人物。',8786:'兩key郎坦教李嘉引敵入枹罕、張璩退敵同一役，JS明戮李嘉，TJ永和三347該事件定年；不套軍士年齡，出生未知。',8787:'李衢交州督護與陶緩奉朱籓命討范文，文及劉雄死亡不屬本人；梁書本段時間壓敘矛盾不猜生卒或首任年。',8788:'三key實混兩代李逵，前涼347枹罕護軍降趙與後漢許劭同鄉先友後隙無同一可能；遊戲豫州籍貫來自漢支不能套前涼將，全條日期留空待拆。',8789:'楊康前涼別將沙阜敗後趙劉寧，SL牛旋楊康同役無別人；劉寧退非楊康死，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round36c_reads={8791:[('tj_16','使所亲沈猛私谓归曰'),('js_15','使亲信人沈猛谓归曰')],8792:[('tj_18','亮为之诛涉圭'),('sl_06','亮为之诛涉圭')],8793:[('hy_00','使散骑常侍王幼送降文于温'),('sl_09','势使散骑侍郎王幼送降文于温'),('tj_16','使散骑常侍王幼送降文于温')],8794:[('js_18','中书监王瑜')],8795:[('sl_03','艾朗见髙独战亦还趣贼并死'),('tj_18','艾朗见高独战，亦还趋贼，并死')],8796:[('js_12','蕤字令远'),('tj_16','尚书左丞荀蕤曰')],8797:[('js_18','袁真病死，其将朱辅立其子瑾以嗣事'),('sl_03','扬州刺史袁真卒陈郡太守朱辅立真子瑾'),('tj_18','燕、秦皆遣兵助袁瑾'),('wsh_02','真病死，辅立其嫡子瑾')],8798:[('jk_02','髙阳人父归以琅琊太守随中宗过江')],8799:[('tj_16','引汉司空谯献之等以为参佐')],8800:[('sl_02','宦者赵生得幸于宣而无宠于韬'),('tj_16','宦者赵生得幸于宣，无宠于韬')]}
    for ident,qs in round36c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第36批C成漢降臣及燕亡諸將逐條實讀','Actual named passages; distinguish sons from fathers, variant role titles and reported capture or death') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round36c')
        if ident!=8797:results[ident].update(historical_birth=None,historical_death=None)
    yangjs=evidence(SOURCE/'_cache/texts/晋书.txt','吕纂妻杨氏，弘农人也','晉書列女呂纂妻楊氏本傳','實讀纂被超殺、侍婢殯纂、玉璽毀、超逼婚、父桓告女、楊氏自殺，父桓及夫纂與原CBDB134766兩親屬吻合。')
    yangtj=evidence(SOURCE/'_cache/texts/资治通鉴.txt','纂后杨氏将出宫','資治通鑑隆安五401呂纂后楊氏自殺','完整年header安帝隆安五401，二月超殺纂及立隆後連續楊氏父右僕射桓、拒再嫁超遂自殺，三月孫恩條之前；保401年度不強算確日。')
    results[8790].update(review_status='primary_text_corroborated',source_identities_all_adjudicated=True,identity_status='original_cbdb_luzuan_wife_yanghuan_daughter_matched_primary_biography',historical_birth=None,historical_death=date(401,yangtj),last_manual_review_batch='round36c_cbdb_primary')
    results[8790]['evidence'].extend([yangjs,yangtj,dict(citation='原CBDB134766楊氏(呂纂妻)實讀原庫',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='c_name_chn楊氏(呂纂妻)；death401神鼎三；KIN父134767楊桓、夫134765呂纂，MQWW PoetID3055；index345 type30。',interpretation='父夫雙重家系與晉書通鑑原傳一致；index345非生年，原庫神鼎三與401及通鑑神鼎改元不合不採其年號年次；日期以獨立TJ401明年header為準。')])
    shedie=chunk_ev('tj_18','亮为之诛涉圭','資治通鑑太和五370五月涉圭被亮誅','癸酉殺令後涉圭至龍城白亮隨即被誅、六月條前，只定同五月不套殺令癸酉為涉圭精死日。')
    results[8792].update(review_status='primary_text_corroborated',historical_death=date(370,shedie,month=5))
    results[8792]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8792]['evidence'].append(shedie)
    aidie=chunk_ev('tj_18','艾朗见高独战，亦还趋贼，并死','資治通鑑太和五370燕亡艾朗護慕容暐遇盜死','親讀出邺逃亡孟高艾朗護暐、數日福祿遇盜死，整年370，未知精日。殿中/武衛官稱異而同主同護逃同死，非另人。')
    results[8795].update(review_status='primary_text_corroborated',historical_death=date(370,aidie))
    results[8795]['evidence'].append(aidie)
    results[8797].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='four_primary_subject_keys_matched_yuanzhen_son_jin_cbdb_423586_original_citation_unlocated')
    for ident,note in {8790:'楊氏原CBDB夫呂纂父楊桓雙橋晉書及通鑑原本傳，401自殺明獨立TJ年header；345index非生日。原CBDB神鼎三與401不合不採年次，未把夫死精日當妻卒日；同段呂隆妻楊氏另人。',8791:'沈猛張重華親信受命私說晉俞歸求涼王爵，同完整對話兩key閉；俞歸363送歸或重華353卒不證沈猛死，生卒未知。',8792:'涉圭沙城城大殺慕容令後被亮誅，370五月但殺令癸酉非涉圭本人明死日，生年不詳。',8793:'王幼成漢散騎常侍奉李勢降文，SL侍郎/文內常侍同一使者非兩人；李勢361卒不能套王幼，無生卒。',8794:'王瑜成漢中書監被桓溫引參軍，華陽/SL王嘏同中書監有姓名字異可能但尚無全鍵對方審，不硬合；後王誓等反、被殺不是王瑜明死。',8795:'艾朗殿中將軍隨慕容暐出邺護逃與孟高同遇盜370死，兩key全閉；武衛/殿中官稱異不拆兩人，無生年。',8796:'荀蕤字令遠崧子羡兄，平蜀議封及尚書左丞兩源全閉；本傳卒吳國內史無年，羡35838卒屬弟不可套；生卒未知。',8797:'袁瑾四史key袁真嫡子、朱輔立與壽春圍城同人均已實讀；CBDB423586引用紹興府志未取本人原文，尚不可全鍵true或整條接收8626王瑾。原371送建康斬日期證據保留、嚴格整條仍hold；SL370八月陷城與獨立TJ371源次異保不採SL單年替換。',8798:'許歸不是出使涼州俞歸：JK本人為許詢玄度父，琅琊太守隨元帝南渡、會稽內史居山陰；347段是子傳追敘父舊事，不當父仍活347或初任347；未知生卒。',8799:'譙獻之成漢司空被桓溫引參佐非被殺，僅TJ單key本人官事清楚；未全查同名其他記錄不硬去重；生日死亡未知。',8800:'趙生趙宦者石宣寵臣進計殺石韜，參與集團後整案宣被火刑不自動推趙生同死；本段無本人處決句，生卒未知。'}.items():results[ident]['unresolved'].append(note)
    zhouq=[('js_08','李脱诬嵩及周筵潜相署置'),('js_07','黄门侍郎周筵忠烈至到'),('js_10','从事中郎周筵、法曹参军刘胤'),('tj_13','请免从事中郎周莚等官'),('wsh_02','以太子右率周莚率中军三千人讨沈充')]
    results[7098]['evidence'].extend(chunk_ev(ch,ph,'周筵莚莛跨片重核五個實際key','JS劉隗淳于伯從事中郎與TJ同案，JS周札附筵太子右率3000討沈充與WS同官同役，JS周嵩李脫誣案與TJ324同案；家系及職官連續閉合') for ch,ph in zhouq)
    zhoujan=evidence(SOURCE/'_cache/texts/资治通鉴.txt','春，正月，王敦诬周嵩、周莛与李脱谋为不轨','資治通鑑太寧二324正月周莛誅','讀前文周札兄子莛、從事中郎與嵩同官，後殺嵩莛及襲札；與JS筵莚同角色同案，不以形近姓名單合。')
    zhoumay=evidence(SOURCE/'_cache/texts/晋书.txt','明年五月，钱凤谋乱，遂族灭莚','晉書五行太寧元323宅異次年324五月周莚族滅','同五間六梁宅異橋周札附周筵傳，不只是同卷另一同名。五月族滅表述與TJ正月本人被殺有月份差，保兩原表述，年324一致。')
    results[7098].update(review_status='duplicate',duplicate_of=6282,source_identities_all_adjudicated=True,identity_status='zhou_yan_莚筵莛_same_zhousong_lituo_case_and_zhousha_nephew',historical_birth=None,historical_death=date(324,zhoujan),last_manual_review_batch='round36_priority7098',duplicate_basis='五key各案官職橋及JS五行同五間六梁宅異；6282唯一TJ14札兄子莛同周嵩李脫案，已通知所屬agent全鍵覆核並保月份源別')
    results[7098]['historical_death']['alternatives']=[{'year':324,'month':1,'calendar':'Chinese_lunisolar_reign_year_mapped_to_CE_year_only','source':zhoujan['citation'],'statement':'正月收嵩莛於軍中殺之'},{'year':324,'month':5,'calendar':'Chinese_lunisolar_reign_year_mapped_to_CE_year_only','source':zhoumay['citation'],'statement':'明年五月錢鳳謀亂遂族滅莚；族滅月份與本人遇殺月份是否同時待考'}]
    results[7098]['evidence'].extend([zhoujan,zhoumay])
    results[7098]['unresolved'].append('實核五key為同一周札兄子，筵莚莛字異；324年一致但TJ正月本人被殺vsJS五行五月族滅，保源別不可將正月定作無爭議整人精卒月；出生不詳。')
    round37a_reads={8801:[('tj_16','晋昌太守郎坦以城大难守'),('js_15','晋阳太守郎坦以城大难守')],8802:[('js_18','其将邓嵩、昝坚劝势降')],8803:[('yi_02','又遣督护陶缓、李衢讨之')],8804:[('hy_00','太史令韩皓上言'),('sl_09','太史令韩皓上言'),('tj_16','汉太史令韩皓上言')],8805:[('sl_09','势以问侍中冯孚')],8806:[('sl_03','侯青武邑人也一云朝那人')],8807:[('sl_02','刘覇断其手足'),('tj_16','刘霸断其手足'),('wsh_01','刘霸断其手足')],8808:[('wsh_01','宦者郝雅、刘灵拔其发')],8809:[('yl_01','使唐柱等筑龙城')],8810:[('sl_02','烈生岳仕前燕侍中')],8811:[('sl_03','安屈辽东胡人也')],8812:[('sl_06','父恭尚书令徐州刺史')],8813:[('sl_08','封孚字处道渤海蓚人也祖悛振威将军')],8814:[('sl_02','收大将军记室参军郑靖尹武等'),('tj_16','收大将军记室参军郑靖、尹武等')],8815:[('sl_05','父清，仕慕容𬀩为黄门侍郎')],8816:[('sl_06','父瑜黄门郎逞少好学')],8821:[('sl_03','张怖字文祖清河武城人也')],8822:[('sl_03','张鸿范阳人也仕皝为黄门郎')],8823:[('yl_07','《燕书》曰：慕容根善射')],8824:[('sz_00','慧湛，本姓彭，任城人也')],8825:[('sl_03','成公都晋兴元吴人也')]}
    for ident,qs in round37a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第37批A同篇族譜附名與列傳各本人實讀','Actual individual paragraph read; kin ages and deaths, execution agents, prisoners and public-works labour counts are separated from the subject') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round37a')
    results[8808]['candidate_duplicate_of']=8807
    results[8808]['unresolved'].append('魏書同句先劉靈拔發後劉霸斷肢，TJ/SL拔發斷肢均劉霸；劉靈可能劉霸字訛亦可能另宦者，原文本身分名不能只同刑案硬合。獨key身份為韜宦者，去重仍待考。')
    for ident,note in {8801:'郎坦晉昌/TJ與晉陽/JS守官名異但枹罕議棄外城、教李嘉引敵整役同人。張璩戮李嘉非郎坦，未知生卒。',8802:'鄧嵩成漢將與昝堅勸李勢降；不是鄧定反晉，降主361卒也非己，無生卒。',8803:'陶緩交州督護與李衢討范文，同僚死亡及林邑王文死不等同己卒，未知生卒。',8804:'韓皓成漢太史令荧惑宗廟議三key一致，非三國曹魏韓浩或苻秦羌豪。三書年次涉及改元異不從祭祀議倒推生日，未明卒。',8805:'馮孚成漢侍中答李勢降事，吳漢公孫述誅族歷史比喻不是本人即將死；唯SL晚源未知生卒。',8806:'侯青前燕將作大匠屯騎校尉，籍武邑一云朝那源別保，張飛是比勇非年齡/另人，本人未知生卒。',8807:'劉霸/覇後趙韜寵宦者為石宣火刑執行人，不是受刑石宣。WS劉靈同拔發官役但同段另稱霸，不能無證硬合或把宣348死套霸；三key本霸官事可閉，未知生卒。',8809:'唐柱燕書引築龍城設宮廟籍田，太祖八年是主君紀年非唐柱年齡；不在未校歷前給出生，無生卒。',8810:'孫岳明在SL孫輝族譜烈子岳前燕侍中幽州刺史右將軍，祖父孫廙等非本人死亡；不因合字孫岳檢索未中稱無文，已讀省姓岳本句，未知生卒。',8811:'安屈遼東胡人為燕暐殿中郎將，先祖世高漢安息侍子非屈本人活數百年，未明生卒。',8812:'宋恭宋隱父尚書令徐州刺史，儁徙邺時家列人；13成人屬子隱、城陷被殺屬弟洽非恭。省姓父恭原句已讀，未知生卒。',8813:'封悛為封孚祖振威將軍，孚太上三71卒不是悛；省姓祖悛原句已讀，僅晚SL無生卒。',8814:'尹武石韜大將軍記室參軍，宣收鄭靖尹武欲誣罪未明本人被殺，後石宣火刑不可套其身，未知生卒。',8815:'崔清崔宏父、崔悅子，慕容暐黃門侍郎；祖悅司徒長史及子宏入魏年事不是本人仍活，省姓父清原句已讀，未知生卒。',8816:'崔瑜崔逞父黃門郎，祖遇仕虎非瑜，逞仕燕秦晉魏被殺非父瑜；少遭亂孤貧不給父確卒年，未知生卒。',8821:'張怖文祖清河武城豫州刺史，幼牧牛採樵換書母孝是本人童年，無可定位歲數年號，未知生卒。',8822:'張鴻范陽皝黃門郎生鬚後出宮看鵝鴨，失寵不是死亡，未知生卒。',8823:'慕容根燕書引善射懸崖野羊，60餘是前條石堅非根。與其他慕容根全key尚未橋前不直接套360被誅，未知生卒。',8824:'慧湛彭姓任城尼，建元二344渡江何充敬住建福寺，前曇備39673卒與後僧基21出家非慧湛；344具名活可立上界，生卒未知。',8825:'成公都晉興元吳人，陽勇戰18但原注陽勇疑誤無確戰年，18不能反推某年出生，唯SL晚層未知生卒。'}.items():results[ident]['unresolved'].append(note)
    results[8824]['attested_activity_years']=[344]
    results[8824]['appearance_upper_bound']=344
    round37b_reads={8817:[('js_15','晋兴相常据为使持节'),('sl_04','征东将军常据又败俨兵于葵谷'),('sl_09','晋兴相常据为使持节'),('tj_17','都尉常据请出之'),('tj_18','征东将军常据向左南'),('wsh_04','都尉常据谏曰')],8818:[('js_09','太宰长史庾倩'),('js_11','倩太宰长史'),('js_12','诬涓及庾倩与晞谋反'),('js_18','诛庾倩、殷涓、曹秀等'),('ssxy_01','及温诛希弟柔倩'),('ssxy_02','倩字少彦司空氷子'),('ssxy_03','桓温杀庾希弟倩')],8819:[('js_11','柔散骑常侍'),('ssxy_01','及温诛希弟柔倩')],8820:[('js_11','后冰子蕴为广州刺史')]}
    for ident,qs in round37b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第37批B庾冰兄弟與前涼常據逐鍵實讀','Read actual named office and family role for every listed key; source shorthand without surname is linked through the enclosing biography') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round37b')
    yuev=evidence(SOURCE/'_cache/texts/资治通鉴.txt','殷涓、庾倩、曹秀、刘强、庾柔皆族诛，庾蕴饮鸩死','資治通鑑咸安元371十一月丙辰庾氏案','實讀十一月癸卯桓溫入京、己酉廢立改元、辛亥逼新蔡王誣告、丙辰庾倩柔族誅與廣州蕴飲鴆本人句。JS冰七子倩太宰柔常侍蕴廣州三人角色均合，不套庾希372死。')
    for ident in [8818,8819,8820]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(371,yuev,month=11,day_ganzhi='丙辰'))
        results[ident]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(yuev)
    for ident,note in {8817:'常據六key前涼都尉諫重華出張祚、後張天錫征東晉興相討李儼，具同政權官序與367前後軍職，未見別身份。都尉諫蟲源異保，戰敗1.7萬非其死，無生卒。',8818:'七key庾倩字少彥冰子希弟、太宰長史與武陵晞誣案全閉，SS庾倪明小字注可合非另一人；371十一月丙辰族誅原TJ，不套父冰344卒或兄希372死，出生未知。',8819:'庾柔冰子希倩弟、散騎常侍被桓溫誣武陵王黨，兩key完整家系與原TJ371十一月丙辰族誅明死；非庾友獲桓家婦求免，未知出生。',8820:'庾蕴冰子廣州刺史白狗郭璞占驗，原JS後文兄弟傳及TJ同廣州冰子蕴互橋；371十一月丙辰飲鴆死明本人，非郭璞本人被王敦害或庾希372死，未知出生。'}.items():results[ident]['unresolved'].append(note)
    round38a_reads={8826:[('wsh_00','太子献明皇帝讳寔格之，伤胁')],8827:[('js_09','掾曹秀'),('js_18','诛庾倩、殷涓、曹秀等')],8828:[('sl_02','大司农曹莫不肯署名'),('tj_16','大司农曹莫不肯署名')],8829:[('sl_02','贵嫔栁氏尚书耆之女也')],8830:[('sl_02','召太常条攸光禄勲杜嘏'),('sl_03','司空条攸等并乗舆服御'),('tj_17','太尉申钟、司空条攸等及乘舆服御')],8831:[('sl_02','力士巨鹿杨杯及牟皮牟成赵生等')],8832:[('sl_02','遣使征天水杨轲'),('js_17','杨轲，天水人也')],8833:[('js_09','著作郎殷涓'),('js_12','子涓，亦有美名'),('js_18','诛庾倩、殷涓、曹秀等')]}
    for ident,qs in round38a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第38批A同篇後趙與晉宗室案逐人原文核查','Read exact named role and surrounding family or office sequence; no death inferred from capture, kin deaths, or another person age') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round38a')
    shidie=chunk_ev('wsh_00','夏五月，薨，后追谥焉','魏書序紀昭成34年371五月太子寔薨','上下文太子獻明寔拒長孫斤刀傷脅，後句七月珪生，末桓溫廢弈立昱亦371交叉；不是庶長子寔君376弑父事。')
    results[8826].update(review_status='primary_text_corroborated',historical_death=date(371,shidie,month=5))
    results[8826]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8826]['evidence'].append(shidie)
    for ident in [8827,8833]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(371,yuev,month=11,day_ganzhi='丙辰'))
        results[ident]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(yuev)
    results[8832]['evidence'].append(chunk_ev('js_17','轲弟子以牛负之，为戍军追擒，并为所害','晉書隱逸楊軻遇害','本人被弟子負逃涼州為戍軍追擒并害明死，沒有確絕對年；不能拿前部石虎徵召年直接當其死年。'))
    results[8832]['source_reported_death_circumstance']='從秦西奔涼州，弟子牛負，被戍軍追擒并害；確年未載'
    for ident,note in {8826:'拓跋寔明獻明皇帝追諡、太子拒長孫斤傷脅371五月卒、七月珪生；不是寔君376弑父另一子，無生年不套妻賀蘭氏生日。',8827:'曹秀武陵王晞掾被桓溫誣同謀，JS兩key與TJ371十一月丙辰族誅同官同案，不把武陵晞38166卒套自己，出生不詳。',8828:'曹莫後趙大司農反對立十歲石世，二十/十歲皆君子年齡非本人；反對後石虎稱忠未殺，生卒不詳。',8829:'柳耆/栁耆尚書女石虎貴嬪，殺者女及其二兄不是父耆。又納少女不是柳耆死後追述年，省姓耆原句已讀，未知生卒。',8830:'條攸後趙太常太傅石世、後魏司空邺陷被送蓟三key同官進階存人鏈；同車王簡張乾郎肅自殺非攸死，未知生卒。',8831:'楊杯巨鹿石宣力士殺韜案，杯與柸字異為8962候選，另一源對方單key未本輪覆核前先不硬去重；宣死/被捕逃亡不是本人死，生卒不詳。',8832:'楊軻天水不婚好易隱逸，二key同全部徵召臥土床荀鋪發被故事；末明被戍軍殺但無年，不從前半石虎徵召年倒鎖卒年，未知生年。',8833:'殷涓殷浩子、武陵晞游著作郎，JS三key與TJ同父同官同案，371十一月丙辰族誅明死；父浩永和12卒不屬子，生年不詳。'}.items():results[ident]['unresolved'].append(note)
    results[8831]['candidate_duplicate_of']=8962
    round38b_reads={8834:[('sl_02','杨杯及牟皮牟成赵生等'),('tj_16','谓所幸杨柸、牟成、赵生曰'),('wsh_01','谓嬖人杨柯、牟成等曰')],8835:[('sl_02','杨杯及牟皮牟成赵生等')],8836:[('js_09','以敞比邓伯道，并早卒'),('ssxy_02','敝字茂平丞相祭酒不就袭爵堂邑公年二十有二而卒')],8837:[('yl_03','《燕书》曰：申弼，烈祖常从容问诸侍臣')],8839:[('yl_05','章该，字宣恒，为左长史')],8840:[('gs_01','竺僧朗。京兆人也')],8841:[('sl_02','颍川荀铺好竒之士也'),('js_17','颍川荀铺，好奇之士也')],8842:[('js_17','灵台令许敦害其宠'),('sl_03','灵台令许敦害其宠')],8843:[('sl_02','宦者郝稚刘覇拔其发'),('tj_16','宦者郝稚、刘霸拔其发')],8844:[('wsh_01','宦者郝雅、刘灵拔其发')],8845:[('sl_02','记室参军郑靖尹武等'),('tj_16','记室参军郑靖、尹武等')],8846:[('jk_02','卫将军营兵士陈凟女台有文在足')],8847:[('sl_03','得侍郎韩偏贿赂举为孝亷')],8848:[('sl_03','韩宰昌黎人也仕俊为谒者仆射')],8849:[('sl_06','其父庆仕燕太子詹事歴位司空')],8850:[('yl_02','鲜于休有才器')],8962:[('tj_16','谓所幸杨柸、牟成、赵生曰')]}
    for ident,qs in round38b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第38批B後趙同役、燕臣譜及僧傳本人逐條讀核','Named passage scope checked directly, including variant characters; kin and adjacent people ages and deaths are not transferred') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round38b')
    lumev=evidence(SOURCE/'_cache/texts/魏书.txt','祖偃，父邈 ，并仕慕容氏为郡太守','魏書47盧玄傳父邈祖偃','親讀盧玄字子真范陽涿人、曾祖諶晉劉琨中郎、祖偃父邈慕容氏郡太守儒雅、玄崔浩表兄。原CBDB31529父盧偃/子盧玄與引用魏書47雙層橋。病卒是子玄不是父邈。')
    results[8838].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='original_cbdb_lumiao_father_luyan_son_luxuan_bridged_weishu47_original_citation',last_manual_review_batch='round38b_cbdb_primary')
    results[8838]['evidence'].extend([lumev,dict(citation='原CBDB31529盧邈實讀原庫親屬與來源',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='c_birthyear0/c_deathyear0，index344type1112；notes XTS73A.20a/Wei Shu47.1045；KIN父31528盧偃、岳父31518崔潛、子31530盧玄。',interpretation='0為未載，index344不當出生；本輪已取原引用魏書47父子祖孫官名橋，未聲稱另新唐書譜本輪已讀。KIN76已核為岳父崔潛，不誤作母親或另造其生卒。')])
    results[8962].update(review_status='duplicate',duplicate_of=8831,identity_status='yang_bei_cup_character_variant_same_shixuan_shitao_murder_agent',duplicate_basis='兩方唯一key全讀；楊杯/柸同石宣力士受命殺石韜分封及追捕逃亡獨特事件，WS同案楊柯旁證字異')
    results[8962]['evidence'].append(chunk_ev('sl_02','力士巨鹿杨杯及牟皮牟成赵生等','SL晚本楊杯與TJ楊柸同角色','校名不用晚本單獨確年；同主令殺韜、許分國邑、史科告發後楊杯亡走。'))
    results[8831]['candidate_duplicate_of']=None
    results[8831]['evidence'].append(chunk_ev('tj_16','虎使收杨柸、牟成，皆亡去','資治通鑑楊柸與SL楊杯同追捕逃亡','與前同受石宣令殺韜同名兩字形異，可橋唯一key8962並閉合，不把逃亡當死亡。'))
    results[8844].update(review_status='duplicate',duplicate_of=8843,identity_status='hao_zhi_ya_character_variant_same_execution_role',duplicate_basis='WS郝雅/TJ與SL郝稚同石韜所親宦者、石宣火刑拔髮抽舌絞頷唯一角色；雙方唯一/二key全實讀，郝稚繩貫頷與WS上下同完整刑案，不依形近名單合')
    results[8843]['evidence'].append(chunk_ev('wsh_01','宦者郝雅、刘灵拔其发','魏書郝雅與TJ郝稚同官角色刑案異字','同韜親宦者拔髮抽舌繩貫頷火刑宣，姓名雅稚異，非受刑本人。WS劉靈與劉霸另有分名疑點不因此自動合劉二記錄。'))
    results[8836]['source_reported_age_at_death']={'age':22,'calendar_year':None,'source':'世說王氏譜茂平敝即敞'}
    results[8840]['source_reported_age_at_death']={'age':85,'calendar_year':None,'source':'高僧傳僧朗傳'}
    results[8840]['evidence'].append(chunk_ev('gs_01','后卒于山中。春秋八十有五','高僧傳僧朗85卒未繫年','實讀京兆泰山金輿谷、秦健皇始元移山、堅重、姚興/慕容德/魏珪禮供到山卒全本傳，沒有明確CE卒年，85不能單補生日315或卒400。'))
    results[8846]['source_reported_chronological_problem']={'source':'建康實錄陳凟女台','statement':'初康帝建元二年十月…明年康帝崩','problem':'建元二為344但康帝同344崩，明年措辭不合；屬褚后384卒段回敘，不把女台384仍活','do_not_treat_as_attested_activity':True}
    for ident,note in {8834:'牟成石宣嬖人殺韜案三key同，奉命與楊柸逃捕無本人死句；SL牟皮同列為另一名不硬合，生卒未知。',8835:'牟皮SL與牟成並列兩名、後捕逃寫杯皮而TJ杯成是傳本異記，未證皮成是一人不硬刪；原自身追捕亡走未明卒，生卒未知。',8836:'王敞JS王導弟與SS王敝茂平同兄及堂邑襲爵傳鏈，敝敞字異、比鄧攸/溫嶠兩弟排列互換是評品異記不另造人物；22卒無年，不把王導33963卒套己。',8837:'申弼燕書烈祖侍臣答口鐵鑽問，不等於同書姓名申鍾/申胤，無全鍵橋不套他人年紀，生卒未知。',8838:'原CBDB盧邈父盧偃子盧玄雙層可橋原魏書47，生卒0留不詳、index344不生日；子玄使宋後卒不是邈。',8839:'章該燕左長史字宣恒被太祖賜百餘匹負不起以辱貪，布數非年齡，未有明死亡，生卒未知。',8840:'竺僧朗京兆泰山金輿谷本傳已全讀，85卒未配CE；道安38573/法和80與張忠華陰死非朗。多君書問並不都是同年活，無確生日不推通行315400。',8841:'荀鋪颍川好奇造楊軻揭被兩源同人，後被戍軍殺是楊軻及其弟子不一定包括非弟子荀鋪；未知生卒。',8842:'許敦燕靈臺令嫉郭璞學生劉弘（泓）被其寬待兩key同一故事；97卒及三年後垂興屬泓非敦，不套其年齡，生卒未知。',8843:'郝稚後趙韜親宦者行石宣火刑同WS郝雅，可去重8844。石宣348被焚不是郝稚死；劉靈/霸疑另案不連帶合，無生卒。',8844:'郝雅唯一WSkey全讀同郝稚原官事刑役字異，→8843；不作另一受刑死者。',8845:'鄭靖大將軍韜記室參軍遭宣擬委罪兩源同，誣捕非明處死；石宣被火刑非靖，未知生卒。',8846:'陳臺女、營兵陳凟女、足文逃縣獄，亡去指逃走不是死亡。JK建元二/明年康帝崩有年詞衝突不擅修；384褚后卒段是追述早年，不能當女台384活，未知生卒。',8847:'韓偏燕侍郎行賄宋該舉孝廉、被禁錮終身，4歲刑屬該非偏年齡，禁錮不是伏誅，未知生卒。',8848:'韓宰昌黎燕儁謁者僕射揚威將軍，子昞/景降魏不是父宰活入魏、與韓偏韓恒不同人，未知生卒。',8849:'高慶省姓父慶原高展傳，燕太子詹事司空三子泰敬展；孫高湖參合諫/入魏非慶生卒，不把代際混套，未知生卒。',8850:'鮮于休燕書有才器辟左光祿大夫曹屬，本條未載字莊及拒冉閔死，不因近似鮮于勗/亮先去重或套其死年，未知生卒。',8962:'楊柸TJ單key已實讀同石宣殺韜分封/逃捕整案，SL楊杯同人，可→8831；兩方生卒未知，WS楊柯字異旁證不新增重疊行。'}.items():results[ident]['unresolved'].append(note)
    round39a_reads={8851:[('sl_02','洛州刺史刘国等率洛阳之众'),('tj_17','洛州刺史刘国帅洛阳之众')],8852:[('js_10','波字道则'),('tj_18','以襄城太守刘波为淮南内史')],8853:[('js_01','遣部将刘焕攻长安'),('sl_02','遣治中刘焕攻长安'),('tj_17','使治中刘焕攻长安')],8854:[('sl_02','侍中呼延盛率步骑七万人'),('tj_17','侍中呼延盛帅步骑七万')],8855:[('sz_00','妙相，本姓张，名珮华')],8856:[('sl_02','麻秋姚国等率骑拒勲'),('tj_17','麻秋、姚国等将兵拒勋')],8857:[('sl_02','弋仲子曜武将军益武卫将军若'),('sl_06','弋仲子曜武将军益武卫将军若'),('tj_17','姚弋仲子曜武将军益、武卫将军若')],8858:[('sl_02','弋仲子曜武将军益武卫将军若'),('sl_06','弋仲子曜武将军益武卫将军若'),('tj_17','姚弋仲子曜武将军益、武卫将军若')],8859:[('tj_17','五材将军封弈')],8860:[('js_11','希闻难，便与弟邈及子攸之逃于海陵陂泽中')]}
    for ident,qs in round39a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第39批A晉趙交替同卷與尼傳實讀','Read enclosing father/son and named office roles, not a standalone extracted name or automated year') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round39a')
    miaoev=chunk_ev('sz_00','今日别矣。','比丘尼傳妙相本人終語','實讀張珮華弘農父茂、15適皇甫達離婚出家、永和中太守齋法會與病終全傳，無確卒年，15婚齡不能當15歲卒。')
    results[8855]['evidence'].append(miaoev)
    boev=chunk_ev('js_10','苻坚败，朝廷欲镇靖北方','晉書劉隗附波傳淝水敗後授冀州','實讀波隗孫父綏、救襄陽免官復常侍、苻堅敗後欲督淮北冀州但疾未行、陳疏即卒；此在383淝水之後，非370鎮石頭當年死。只保存活下界383不主張卒恰383。')
    results[8852]['evidence'].append(boev)
    results[8852]['last_attested_activity']=383
    results[8852]['last_attested_activity_basis']='淝水敗後本人仍被授官並陳疏，實際年不早於383；383只是保守在世下界，不是卒年或精確授官年。'
    miaodie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','秋，七月，壬辰，拔其城，擒希、邈及其亲党，皆斩之','資治通鑑咸安二372七月壬辰庾邈同希被斬','前文庾希及弟邈武遵入京口，後周少孫破城，原邈同希本人明死；非371先殺倩柔蕴。')
    results[8860].update(review_status='primary_text_corroborated',historical_death=date(372,miaodie,month=7,day_ganzhi='壬辰'))
    results[8860]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8860]['evidence'].append(miaodie)
    for ident,note in {8851:'劉國後趙洛州刺史會石遵李城入邺，另據陽城不附冉閔同政權軍主鏈，王豺等死非己，生卒未知。',8852:'劉波道則隗孫父綏、王洽參軍降晉襄城守到370淮南內史鎮石頭兩key清楚；後傳383敗後仍授冀州但病疏卒無年，史實生卒留空而已證不早383在世可修過早遊戲估卒。',8853:'劉煥司馬勳治中出駱谷懸鉤遣攻長安三key同役；斬京兆劉秀離是別人，不能當349己死，生卒未知。',8854:'呼延盛趙侍中奉冉閔李農命與張舉石琨討石祗，後孫伏都等被斬不同臣，7萬兵不是本人年齡；生卒未知。',8855:'妙相原名張珮華弘農父茂與皇甫達離婚出家，永和法會後終無精年，鄰慧賢太和70餘非妙相；生卒未知。',8856:'姚國趙石苞將與麻秋拒司馬勳，石苞被王朗送邺及後被殺不是姚國明死，生卒未知。',8857:'姚益弋仲子曜武將軍與弟若出禁兵奔父三key省姓句均實讀，後353守山桑仍同姚襄弟；劉啟王彬之敗死不是益死，生卒未知。',8858:'姚若弋仲子武衛將軍與兄益奔滠頭三key省姓本人均讀；禁兵數千非本人年歲，不套父352死，生卒未知。',8859:'封弈燕五材將軍勸伐趙與後國相同上下文，封奕字異可候選主記錄但對方全鍵未本輪讀不硬刪；未知生卒。',8860:'庾邈冰子希弟會稽王參軍，371逃難與372入京口被擒斬明同本人，372七月壬辰原TJ；不套先371兄倩柔蕴死，出生不詳。'}.items():results[ident]['unresolved'].append(note)
    round39b_reads={8861:[('sl_02','武卫将军张季及诸公侯'),('tj_17','武卫将军张季及公侯')],8862:[('sl_02','抚军将军张沈据滏口'),('tj_17','抚军将军张沈据滏口')],8863:[('sl_02','勅雍州刺史张茂送之'),('tj_17','又敕雍州剌史张茂送之'),('js_12','与同郡张茂字伟康'),('js_18','张茂妻陆氏，吴郡人也'),('ssxy_02','伟康名茂尝梦得大象')],8864:[('sl_02','寻复使豺弟雄矫诏杀之'),('tj_17','张豺使张雄矫诏杀斌'),('sg_06','谥曰壮侯子雄嗣')],8865:[('js_17','获伪相支重'),('sl_02','获将军支重')],8866:[('sl_02','遣宦者杨环驰以告闵'),('tj_17','遣宦者杨环驰以告闵')],8867:[('sl_02','寜南将军杨群据桑壁'),('sl_04','以右卫将军杨群为豫州刺史'),('tj_17','宁南将军杨群据桑壁')],8868:[('js_18','征西督护滕畯率交广之兵伐文于卢容'),('tj_17','桓温遣督护滕畯帅交、广之兵'),('yi_02','督护滕畯、九真太守灌邃')],8869:[('yi_02','督护滕畯、九真太守灌邃')],8870:[('sl_02','右卫将军王基宻谋废遵'),('tj_17','右卫将军王基密谋废遵')],8871:[('sl_02','扬州刺史王浃举寿春归晋'),('tj_17','赵扬州刺史王浃举寿春降')],8872:[('sl_02','及石荣王铁等讨梁犊还遇遵于李城')],8873:[('js_17','前锋督护王颐之等径造彭城'),('sl_02','前锋督护王頥之等径造彭城'),('tj_17','前锋督护王颐之等径造彭城'),('js_12','子颐之嗣，仕至东海内史')],8874:[('sl_02','光禄大夫石岳抚军将军石寜'),('tj_17','光禄大夫石岳、抚军石宁')],8875:[('sl_02','及石荣王铁等讨梁犊还遇遵于李城')]}
    for ident,qs in round39b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第39批B後趙崩解、晉北伐同篇逐鍵實讀','Every candidate subject paragraph read; same-name persons in different reigns or unresolved career bridges explicitly separated') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round39b')
    results[8863].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_349_zhao_yongzhou_governor_and_jin_zhangmaoweikang_wujun_governor',component_identities=[dict(identity='後趙349雍州刺史張茂被梁犢裹脅',keys=[k for k in results[8863]['keys'] if k.startswith(('sl:','tj:'))],historical_birth=None,historical_death=None),dict(identity='東晉張茂字偉康會稽三康吳郡太守陸氏夫，沈充所害',keys=[k for k in results[8863]['keys'] if not k.startswith(('sl:','tj:'))],historical_birth=None,historical_death=None,candidate_duplicate_of=6189)])
    results[8864].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_349_zhangchai_brother_and_cao_wei_zhanghe_son',component_identities=[dict(identity='後趙張豺弟雄，矯詔殺石斌',keys=[k for k in results[8864]['keys'] if not k.startswith('sg:')],historical_birth=None,historical_death=None),dict(identity='曹魏張郃嗣子雄，襲壯侯',keys=[k for k in results[8864]['keys'] if k.startswith('sg:')],historical_birth=None,historical_death=None)])
    results[8867].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='zhao_sangbi_yangqun_and_qin_yuzhou_yangqun_career_transition_not_explicitly_bridged')
    results[8868].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='three_primary_keys_same_jiaoguang_commander_cbdb_huangzhoufuzhi_subject_unlocated')
    results[8868]['evidence'].append(dict(citation='原CBDB412824滕畯來源定位實讀',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death null、無KIN；TEXT5913黃州府志section5190288、Wikidata Q65918402。',interpretation='三史中桓溫督護可合，但原府志本人官事未取，不能只同姓名就將第四key全核true。'))
    results[8873].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='three_349_chupou_vanguard_keys_bridged_wangyi_son_neishi_key_career_bridge_not_yet_located')
    for ident,note in {8861:'張季趙武衛將軍隨胡臣逃襄國，2key同名官事，屠胡死20萬不表示季必死，生卒未知。',8862:'張沈趙撫軍據滏口不附閔、鑒宦者召襲邺同人兩key，鑒被閔殺不是沈明死，生卒未知。',8863:'五key真混：349後趙雍州刺史被梁犢裹脅，與晉張茂偉康吳郡守沈充所害、陸氏夫三key不同人，已分支。不可套遊戲325生或將吳郡支整條去重6189。',8864:'三key真混：趙張豺弟雄矯詔殺石斌349與魏張郃子雄231後嗣侯，相隔百餘年宗族不同。石斌被殺/張郃死都非兩雄本人生卒，不硬整合。',8865:'支重後趙沛相為褚裒部將徐龕(JS)/王龕(SL)所獲，349軍被俘不是被斬；徐/王主將代陂死是另人，生卒未知。',8866:'楊環後趙宦者為石鑒告閔石遵將殺之，二key同密報，不是石遵宦官被殺或苻秦楊安，生卒未知。',8867:'楊群後趙寧南據桑壁與前秦352右衛豫州許昌守，兩年間可轉仕但未見明轉仕或家系橋；當前讀清三key各主體，不強稱混也不全鍵true，日期留空待接續。',8868:'滕畯三史桓溫督護交廣軍討林邑明同人，梁書討佛勝與JS/TJ討文敗可能前後戰或敘異，均不證本人卒。CBDB第四key原黃州府志未取，不把同名自動核全；無生卒。',8869:'灌邃九真太守與滕畯討范佛，700精卒不是年齡，范佛降非己死，單梁書本人清楚；無生卒。',8870:'王基趙右衛將軍與李農被閔劫密謀廢石遵，兩key同官同案；曹魏王基261卒是另一時代不合，未知生卒。',8871:'王浹後趙揚州刺史349壽春降晉，SL/TJ兩key閉；8728王洽已混王導子/趙降將，這裡只供趙支候選不全條重定向，不把城毀兵退當本人死，未知生卒。',8872:'王鐵趙將討梁犢還李城勸石遵還邺，SL單key原列明，不從同列石閔350稱帝推自己死亡，未知生卒。',8873:'王頤之前鋒督護349褚裒彭城三key同役；JS12王廙子、胡之兄東海內史同名同世可能本人，但未明轉任/官籍橋，先needs_further_research不誣真混。未橋前不向其四key套任何生卒，未知。',8874:'石岳趙光祿大夫奔襄國兩key同官，逃亡不是冉閔屠殺時本人卒，未知生卒。',8875:'石榮趙將李城與王鐵等勸石遵討張豺，SL單key本人明，石遵遇害不是石榮死，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    results[8859]['duplicate_of']=None
    results[8859]['candidate_duplicate_of']=6989
    results[8859]['evidence'].append(chunk_ev('sl_03','五材将军封奕奕曰用兵之道','十六國春秋封奕與TJ封弈同五材將軍對答','同伐趙前敵強用智弱用勢、先取薊次邺完整對答，可校字異；6989四TJ源全鍵尚未閉合，只列候選。'))
    round40a_reads={8876:[('sl_02','中黄门竺昌蒲请道安入华林园'),('gs_01','遣中使竺昌蒲请安入华林园')],8877:[('gs_01','竺法旷。姓睾。下邳人')],8878:[('sl_09','索振谏曰先王寝不安席'),('tj_17','从事索振谏曰'),('js_15','征事索振谏曰')],8879:[('sl_02','使将军苏彦周成率甲士'),('sl_03','闵将苏彦遣其将金光'),('tj_17','使将军苏彦、周成帅甲士')],8880:[('sl_02','执南阳太守郭启')],8881:[('sl_02','冲将陈暹曰'),('tj_17','其将陈暹曰')],8882:[('tj_17','诏遣谒者陈沈如燕')],8883:[('sl_03','晋穆帝使谒者陈沉拜俊')],8884:[('js_12','以淮南太守陈逵'),('sl_02','使西中郎将陈逵进据寿春'),('tj_17','西中郎将陈逵进据寿春'),('wsh_02','西中郎将陈逵进据淮南'),('sg_08','凖孙逵字林道有誉江左'),('ssxy_02','陈林道在西岸')],8885:[('sl_06','弋仲部曲马何罗博学有文才')],8886:[('js_17','后遣督护麋嶷进军下邳'),('sl_02','后遣督护麋嶷进军下邳'),('tj_17','后遣督护麋嶷进据下邳')]}
    for ident,qs in round40a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第40批A同卷燕晉使者、北伐臣與僧傳逐鍵實讀','Each actual paragraph read; captured officials, kin ages and posthumous titles are distinguished from subject dates') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round40a')
    kuangdie=chunk_ev('gs_01','元兴元年卒。春秋七十有六','高僧傳竺法曠402卒76','親讀睾姓下邳、吳興後母孝、竺曇印弟子、潛青山石室講法華無量壽、孝武迎長干寺及本人40276卒；非前昙戒70或後道壹71。')
    results[8877].update(review_status='primary_text_corroborated',historical_birth=date(327,kuangdie,age=76),historical_death=date(402,kuangdie))
    results[8877]['evidence'].append(kuangdie)
    results[8883].update(review_status='duplicate',duplicate_of=8882,identity_status='chen_shen_沈沉_character_variant_same_jin_envoy_grants_murongjun_title',duplicate_basis='SL與TJ同晉穆帝遣謁者陳沉/沈拜儁使持節大將軍大單于燕王，唯一使官整串官號同，年月差保而非因單同名合')
    results[8883]['evidence'].append(chunk_ev('tj_17','诏遣谒者陈沈如燕','資治通鑑349謁者陳沈授燕王','與SL燕書同授儁四州/二州牧等原官號略差，差在四月派/七月拜敘次不影響使人身份，未提供生卒。'))
    results[8884]['source_reported_genealogy_variants']=[{'source':'SG陳氏譜','grandfather':'凖太尉廣陵公'},{'source':'SS陳逵別傳','grandfather':'淮太尉','father':'畛光祿大夫','note':'淮/凖字形異，皆林道廣陵公西中郎，未據單字異拆人'}]
    for ident,note in {8876:'竺昌蒲石遵中使/中黃門邀道安入華林園，同上下文兩key閉；道安45返冀及385卒不是昌蒲年齡或卒日，未知生卒。',8877:'法曠睾姓下邳後母孝與曇印弟子、長干寺同一完整本傳，402元興元卒76推327虛歲，不是生日；前昙戒70/後道壹71另人。',8878:'索振涼從事諫重華喜博賜左右三key整段同人。重華27卒/24異說非索振，未知生卒。',8879:'蘇彥石閔將殺石遵與後遣金光襲慕容恪敗奔并州三key同閔屬軍官，金光被斬非蘇彥，生卒未知。',8880:'郭啟晉南陽太守宛陷被趙石遇執，司馬勳後斬趙袁景是另一太守，不把被俘當已殺，僅晚SL未知生卒。',8881:'陳暹趙石冲將苑鄉力主南進同對話兩key閉；石冲元氏賜死與3萬士卒坑不能無明本人句全套暹死，未知生卒。',8882:'陳沈晉謁者349使燕拜慕容儁全串官號同SL陳沉，可去重8883；TJ四月遣/SL七月拜可能派至前後且原叙不一保源別，不產生生日死亡。',8883:'陳沉單SLkey原官役全讀，同8882沈沉異字整串封授同一使者→8882，月份敘異不妨身份，生卒不詳。',8884:'六key陳逵林道西中郎領淮南守、歷陽/壽春軍同進退全閉。SG陳氏譜凖孫/SS別傳祖淮太尉父畛家系字異保，不因三國志注就誤定本人曹魏時人；孫策26卒/祖準官不套己，未知生卒。',8885:'馬何羅姚弋仲部曲文才、背投張豺尚書郎又還弋仲赦為參軍；弋仲35273卒非本人死，不以歷史漢武帝同名馬何羅自動合，生卒未知。',8886:'麋嶷褚裒督護前鋒下邳三key同一349北伐，王龕李邁代陂死另人，不因同兵敗套本人卒，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round40b_reads={8887:[('js_12','黄门郎丁纂征谟'),('tj_17','黄门郎丁纂征谟')],8888:[('js_17','单道开，敦煌人也'),('sl_02','单道开姓孟炖煌人也')],8889:[('sl_02','王泰崔通周成等帅步骑十二万')],8890:[('sl_03','菁与张光战于渭北之阴盘擒之'),('sl_04','张光杨初皆擅兵一方'),('wsh_02','杜洪遣将军张光逆健于潼关'),('sl_09','张先杨初皆阻兵不服'),('tj_17','苻菁与张先战于渭北，擒之')],8891:[('sl_03','慕舆句督蓟中留事'),('tj_17','慕舆句督蓟中留事')],8892:[('tj_17','慕舆于自西道出蠮螉塞')],8893:[('tj_16','虏其王玄及部落五万馀口而还')],8894:[('sl_03','遣叅军杜山伯诣建康献捷'),('tj_17','遣参军杜山伯诣建康献捷')],8896:[('sl_02','执南蛮校尉桑坦')],8897:[('sl_03','京兆叚陵为左将军领左司马'),('tj_17','京兆段陵为左将军，领左司马')],8898:[('js_09','以颖方温太真'),('ssxy_02','颍字茂英位至议郎年二十卒')],8899:[('sl_03','京兆王鱼安定程肱胡文等为军谘祭酒'),('sl_04','中书令王鱼言于生曰'),('tj_17','京兆王鱼、安定程肱、胡文等为军咨祭酒')],8900:[('sl_09','祁嘉字孔宾酒泉人也')]}
    for ident,qs in round40b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第40批B秦燕初建、晉北伐與隱逸逐鍵原文實讀','Read actual role and episode, including correct occurrence among same-book names; captures, adjacent death ages and travel dates do not become birth/death dates') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round40b')
    liumev=evidence(SOURCE/'_cache/texts/宋书.txt','祖恬，西河太守。父凭，冯翊太守','宋書柳元景傳祖恬父憑','親讀柳元景河東解，曾祖卓遷襄陽汝南守、祖恬西河守、父憑馮翊守完整家系；與原CBDB178277父柳卓194427/子柳憑178278雙層同，411是Tackett估卒非本傳死年。')
    results[8895].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='cbdb_liutian_father_liuzhuo_son_liuping_matched_songshu_yuanjing_genealogy',last_manual_review_batch='round40b_cbdb_primary')
    results[8895]['evidence'].extend([liumev,dict(citation='原CBDB178277柳恬原庫日期限定與KIN',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth null/death411；notes Death year estimated by Tackett(cc411)；index348；KIN父194427柳卓code75/子178278柳憑code180，source32033。',interpretation='411明估，348索引不能當生年；親屬橋宋書祖恬父憑與曾祖卓，但未稱Tackett原引墓誌已讀。')])
    results[8888]['evidence'].append(chunk_ev('js_17','升平三年至京师','晉書單道開359到京','原書升平三359本人到京、後南海羅浮山百餘歲卒，359非卒年；14歲沙彌是弟子非本人。'))
    results[8888]['last_attested_activity']=359
    results[8888]['source_reported_age_at_death']={'age_min_exclusive':100,'calendar_year':None,'source':'晉書單道開傳年百餘歲卒山舍'}
    results[8898]['source_reported_age_at_death']={'age':20,'calendar_year':None,'source':'世說王氏譜王颍茂英'}
    for ident,note in {8887:'丁纂晉黃門郎與紀璩/據徵蔡謨兩key同任同案，帝8歲是穆帝非纂年齡，蔡謨庶人/末殞非纂死，未知生卒。',8888:'單道開敦煌姓孟修道兩傳整身世閉；359升平3本人至京明在世，後羅浮卒百餘歲無確年，不用弟子14或袁宏訪遺骸年作其生卒。',8889:'崔通冉閔將與王泰周成蒼亭戰，敗死2.8萬是敵軍不是崔通本人，未知生卒。',8890:'張光/先為張琚弟杜洪將潼關渭北陰盤被苻菁擒五key同事；SL原注光一作先、SL09秦赦前俘張先與SL04張光同對涼使語，字異可合。WS同篇另南燕靈臺令張光不可因同名牽入候選本人（candidate明杜洪將）。被擒赦不證死，未知生卒。',8891:'慕輿句燕中部俟厘/侯厘督薊留事兩key同事件，與慕輿根夜拒鹿勃早異人，本人無生卒。',8892:'慕輿於/于燕350伐趙西道蠮螉塞主將，SL同役作根字異但單名未充分證不能強合慕輿根；不套根360誅，未知生卒。',8893:'扶餘玄TJ省族王玄被燕346虜授鎮軍配皝女，5萬餘部眾不是年齡，俘而授官未明被殺；未知生卒。',8894:'杜山伯秦苻健參軍使建康獻入長安捷修好桓溫兩key同官同役，石寧被斬別人，不作己死亡，未知生卒。',8895:'柳恬原CBDB父卓子憑雙橋宋書柳元景曾祖卓祖恬父憑、西河太守，411明Tackett估卒、348index非生日，歷史生卒留空；子憑之喪不可當恬死。',8896:'桑坦魏南蠻校尉被晉袁真攻合肥執，遷民非誅將，不把俘虜當己卒，唯SL未知生卒。',8897:'段陵京兆苻洪左將軍左司馬兩key同序封授，未把段純/段勤形近另合；苻洪中毒死非己，未知生卒。',8898:'王穎/颍茂英王導弟議郎早卒20，JS與SS兄弟評比溫嶠/鄧攸次序異保而字名親屬可合；未有絕對卒年不套導33963歲，未知生卒。',8899:'王魚京兆苻健軍諮祭酒後生中書令三key同政治官序，胡文同僚不同人，生答應國喪後殺梁后毛貴等不是殺王魚，無本人生卒。',8900:'祁嘉孔賓酒泉二十餘隱去敦煌，重華儒林祭酒到天錫尊師終壽，90或84是前郭荷本傳非嘉。20餘非絕對生日、弟子2000非年歲，唯SL無生卒。'}.items():results[ident]['unresolved'].append(note)
    maobranch=evidence(SOURCE/'_cache/texts/资治通鉴.txt','沈充拔吴国，杀内史张茂','資治通鑑永昌元322吳郡張茂遇害','實讀322王敦入京周戴被害、沈充拔吳國殺內史茂同年敘次；JS陸氏夫吳郡太守沈充害與SS偉康故事明同支，不能套後趙349雍州張茂。')
    results[8863]['evidence'].append(maobranch)
    results[8863]['component_identities'][1]['historical_death']=date(322,maobranch)
    round41a_reads={8901:[('tj_17','遣侍中纪据、黄门郎丁纂征谟')],8902:[('js_12','遣侍中纪璩、黄门郎丁纂征谟')],8905:[('js_12','使主簿谢攸对曰'),('tj_17','使主簿谢攸陈让')],8906:[('js_13','遐字应远'),('js_18','竟陵太守邓遐率三千人助祐'),('sl_03','竟陵太守邓遐率舟师三千人'),('tj_18','竟陵太守邓遐帅舟师三千人'),('js_07','使督护邓遐助抚讨之'),('ssxy_03','邓遐字应陈郡人平南将军岳之子')],8907:[('sl_03','鹿勃早将数千人夜袭燕营'),('tj_17','鹿勃早将数千人夜袭燕营')],8908:[('sl_02','幽州刺史刘凖降燕'),('sl_03','赵故太尉刘准'),('tj_17','故太守刘准，隗之兄子也'),('js_07','密遣使告镇东将军刘准')],8909:[('sl_02','沙门吴进言于虎曰'),('tj_16','沙门吴进言于虎曰')],8910:[('sl_03','吕婆楼为散骑常侍'),('sl_04','征吕婆楼为侍中左大将军'),('tj_17','吕婆楼为散骑常侍'),('tj_18','坚以问尚书吕婆楼'),('sl_05','坚与吕婆楼帅麾下数百'),('wsh_02','父婆楼，苻坚太尉')],8911:[('sl_02','将军孙威拒琨于黄丘'),('tj_17','将军孙威拒琨于黄丘')],8912:[('sl_03','大司马从事中郎广寗常炜来聘'),('tj_17','大司马从事中郎广宁常炜使于燕')],8913:[('sl_03','奕给使张安有勇力'),('tj_17','弈给使张安，有勇力'),('yl_06','董卓攻得李昊、张安毕主范中，生烹之')],8914:[('sl_02','射声校尉张艾请闵亲郊'),('tj_17','射声校尉张艾请闵亲郊')],8915:[('tj_16','讨叛虏斯骨真等万馀落'),('js_15','还讨叛虏斯骨真万余落')]}
    for ident,qs in round41a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第41批A蔡謨迎使與秦燕將臣逐鍵原文實讀','Read all named passages and adjudicate identity through office, kin and unique events; uncertain career bridges and mixed eras are held') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round41a')
    results[8902].update(review_status='duplicate',duplicate_of=8901,identity_status='ji_ju_qu_character_variant_same_shizhong_summons_caimo',duplicate_basis='TJ紀據/JS紀璩同侍中與黃門丁纂迎蔡謨、謝攸陳疾、旦至申十餘返同一完整角色事件，兩單key均全讀')
    results[8901]['evidence'].append(chunk_ev('js_12','遣侍中纪璩、黄门郎丁纂征谟','晉書紀璩同TJ紀據','同官同丁纂使職與十餘返蔡謨，據璩字異不另造人。'))
    results[8904].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb_peichong_original_epitaph_subject_not_yet_located',last_manual_review_batch='round41a_cbdb_original_checked')
    results[8904]['evidence'].append(dict(citation='原CBDB194415裴冲原庫親屬與估卒限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth null/death411，index348；Death year estimated by Tackett(cc411)；補遺7:138–139/文物1999.1:93/洛陽新獲續504；五子道護193889、道子194416、道大194417、道會194418、道賜194419。',interpretation='411估卒及348索引不當史實日期；本地宋書魏書未定位本家本人原文，原墓誌未取，只查原庫不能宣全原引已讀。'))
    results[8908].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='351_liukui_nephew_three_keys_and_306_eastern_jin_liu_zhendong_key_unbridged',component_identities=[dict(identity='351原後趙太守劉隗兄子劉準，閔幽州刺史後燕司馬',keys=[k for k in results[8908]['keys'] if not k.startswith('js:')],historical_birth=None,historical_death=None),dict(identity='西晉鎮東將軍劉準，周玘密報討陳敏',keys=[k for k in results[8908]['keys'] if k.startswith('js:')],historical_birth=None,historical_death=None,identity_link_to_other_component='unresolved_not_proven_different_person')])
    results[8913].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_yan_fengyi_attendant_and_han_dongzhuo_execution_victim',component_identities=[dict(identity='燕封弈給使張安，351挾逄約馬歸營',keys=[k for k in results[8913]['keys'] if not k.startswith('yl:')],historical_birth=None,historical_death=None),dict(identity='董卓所烹張安，與李昊同被烹',keys=[k for k in results[8913]['keys'] if k.startswith('yl:')],historical_birth=None,historical_death=None,death_circumstance='英雄記引文被董卓烹殺，未在本引明絕對年')])
    for ident,note in {8901:'紀據侍中同丁纂迎蔡謨、十餘返同JS紀璩可字異合，8歲是穆帝非己，無生卒。',8902:'紀璩JS單key實讀全同TJ紀據迎蔡謨使役，→8901；同事蔡謨辭官不證本人生卒。',8904:'裴冲411原庫明Tackett估卒已撤，348index非生日。五子已核原庫但尚缺原墓誌本文，不能冒稱原引逐條完成；待取文物/洛陽新獲續504。',8905:'謝攸蔡謨主簿對使者陳病，兩key同角色話語；帝8歲/蔡謨被免皆非本人生卒，未知。',8906:'鄧遐岳子字應遠(JS)/應玄(SS)同勇力樊噲、桓溫部將竟陵守與枋頭被免六key閉；寧康追贈不是本人仍活，免後赴山陵無確帝年不硬定369或373卒，生卒未知。',8907:'鹿勃早鄧恒將夜襲慕容儁營敗僅身免，死亡略盡是所從兵不當本人死，兩key全閉未知生卒。',8908:'351三key劉準隗兄子、幽州太守降燕同人清楚；JS07西晉討陳敏鎮東將軍相差45年未明親族生平橋，尚不斷必兩人，先分未連支hold不造確卒。',8909:'吳進趙沙門勸石虎苦役晉民，役夫暴風死數萬不是吳進死，兩key同論與官役全閉，生卒未知。',8910:'六key呂婆樓略陽氐酋散常到苻堅尚書薦王猛、司隸、呂光父太尉同政治官序；WS父婆樓省姓已實讀。呂光10歲/後399卒非父本人年齡，無生卒。',8911:'孫威閔將黃丘拒石琨敗單騎返，士卒盡非本人死，無生卒。',8912:'常煒廣寧閔大司馬從事中郎使與燕封裕問璽同長對答，拘龍城不證被殺；不套常據或封裕死，無生卒。',8913:'張安三key真混：燕封弈給使351挾逄約與英雄記董卓時被烹同李昊者跨160年。YL未明絕對死年，支留死因不擅填190或191；全條BD空不誤合。',8914:'張艾閔射聲校尉敗後請親郊止謠，法饒父子被支解非艾，生卒未知。',8915:'斯骨真前涼謝艾所討部落首領，破部俘斬數不是本人明死，兩key全閉生卒未知。'}.items():results[ident]['unresolved'].append(note)
    round41b_reads={8916:[('sl_04','左卫将军李威'),('sl_05','乃与卫军李威谋赐法死'),('tj_18','以汉阳李威为左仆射'),('wsh_02','见母苟氏通李威之事')],8917:[('sl_10','父昶字中坚幼有令誉')],8918:[('sl_02','李歴屯卫国皆相次归晋'),('sl_03','李歴为兖州刺史'),('sl_06','又与秦将髙昌李厯战于麻田'),('tj_17','又与秦将高昌、李历战于麻田'),('tj_18','其将张平、李历、高昌皆遣使降燕')],8919:[('sl_02','赵郡太守李邽亦以郡降燕'),('sl_03','赵郡太守辽西李邽举郡来降'),('sl_04','李邽为尚书'),('tj_17','魏赵郡太守辽西李邽举郡降'),('tj_18','豫州刺史李邽帅州兵五千断温粮道')],8920:[('sl_02','荆州刺史乐弘皆以城归晋'),('tj_17','荆州刺史乐弘、豫州牧张遇')],8921:[('sl_02','乐立屯许昌李歴屯卫国')],8923:[('js_18','五凝之妻谢氏，字道韫'),('ssxy_00','凝之字叔平右将军羲之第二子也'),('ssxy_03','王凝之谢夫人既徃王氏')],8925:[('sk_01','拜<真净>为朝廷佐平')]}
    for ident,qs in round41b_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第41批B秦燕轉仕、百濟左平與原CBDB婦族逐鍵讀核','Read actual office, parentage and historical year context, including shorthand father names and source character variants') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round41b')
    results[8917]['source_reported_age_at_death']={'age':18,'calendar_year':None,'source':'SL李暠本傳父昶18卒暠遺腹子','limitation':'晚重編本，只保原說；遺腹不必與子出生同公曆年，不反鎖父精年'}
    maojs=evidence(SOURCE/'_cache/texts/晋书.txt','苻登妻毛氏，不知何许人','晉書列女苻登毛后本人傳','實讀登大界營失陷毛氏跨馬力戰被執、拒姚萇欲納而被殺；原CBDB135468毛氏(苻登后)夫31647苻登唯一夫后橋。')
    maotj=evidence(SOURCE/'_cache/texts/资治通鉴.txt','杀毛后及南安王弁、北海王尚','資治通鑑太元十四389八月毛后遇害','實讀389年header、八月登據苟頭原/姚萇留姚旻守安定、夜襲大界、殺毛后及後文毛氏拒納再敘，九月條前；毛氏非魏明帝237毛后。')
    results[8922].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(389,maotj,month=8),source_identities_all_adjudicated=True,identity_status='original_cbdb_fudeng_mao_empress_matched_jinshu_lienv_and_tongjian',last_manual_review_batch='round41b_cbdb_primary')
    results[8922]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8922]['evidence'].extend([maojs,maotj,dict(citation='原CBDB135468毛氏(苻登后)原庫實讀',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death null，index347；KIN夫31647苻登code134，MQWW PoetID3054。',interpretation='原庫無確日期，347索引不出生；與晉書登妻/秦皇后身分橋後採獨立TJ389八月本人被殺。')])
    ningjs=evidence(SOURCE/'_cache/texts/晋书.txt','十一月甲寅，妖贼孙恩陷会稽，内史王凝之死之','晉書安帝紀隆安三399十一月甲寅王凝之遇害','本紀隆安三年399冬十月洛陽陷後，明十一月甲寅凝之死，同謝道韞傳孫恩害夫；未知生年。')
    ningtj=evidence(SOURCE/'_cache/texts/资治通鉴.txt','甲寅，恩陷会稽，凝之出走，恩执而杀之，并其诸子','資治通鑑隆安三399王凝之被孫恩殺','親讀王羲之子會稽內史五斗米不備戰、甲寅被殺、妻道蕴奕女被俘同家系；本地TJheader公元三三九有抄碼錯，以隆安三/己亥及JS同事399校，不從錯碼改339。TJ本地未另列十一月，用明月JS本紀。')
    results[8923].update(review_status='primary_text_corroborated',historical_death=date(399,ningjs,month=11,day_ganzhi='甲寅'),identity_status='wangningzhi_shuping_wangxizhi_son_xiedaoyun_husband_all_keys_bridged_original_cbdb')
    results[8923]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8923]['evidence'].extend([ningjs,ningtj,dict(citation='原CBDB134837王凝之原庫雙親屬核查',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death null，index351；KIN父25793王羲之code75、妻134836謝道韞code135，MQWW PoetID2001/HuWenKai10。',interpretation='同SS原注羲之次子叔平與JS道韞凝之夫婦，CBDB單支身份閉合，351索引不當出生。')])
    results[8925]['source_reported_appointment']={'reign':'近肖古王','reign_year':2,'month':1,'role':'朝廷佐平','source':'三國史記卷24','is_first_career_year_proven':False}
    results[8925]['attested_activity_years']=[347]
    results[8925]['appearance_upper_bound']=347
    results[8925]['appearance_constraint_explanation']='SK24二年正月拜佐平；此前已讀SK30辛未咸安元371故國原王薨，同SK24近肖古王26斯由戰死同步，故二年=371−(26−2)=347。拜非證初仕，不把後族身分作生年。'
    results[8925]['evidence'].append(dict(citation='三國史記SK24與SK30已讀紀年同步覆用',excerpt='SK30辛未簡文咸安元371故國原王薨；SK24近肖古王26斯由中流矢死；二年春正月拜真淨朝廷佐平。',interpretation='同一死亡同步371=26年，換算二年347；不以任官推初仕或出生。',verification='prior_individual_reading_8586_chronology_reused_with_new_subject_passage'))
    gucong=chunk_ev('tj_15','前义兴太守顾从等皆举兵应之','資治通鑑咸和二327顧從討蘇峻','本key原候選顧從為前義興太守與蔡謨王舒虞潭應討同役，6138顧眾JS父秘交州守/義興守/後仆射三keys已由4000片逐核，從眾字異可閉。')
    results[7832].update(review_status='duplicate',duplicate_of=6138,source_identities_all_adjudicated=True,identity_status='gu_cong_zhong_character_variant_same_former_yixing_governor_sujun_resistance',last_manual_review_batch='round41_priority7832',duplicate_basis='TJ顧從前義興守同蔡謨王舒虞潭討峻，JS顧眾義興守結同役；canonical6138全部3keys由4000片實核並通報true，不改其檔')
    results[7832]['evidence'].append(gucong)
    results[8863]['component_identities'][1]['confirmed_identity_match']=6189
    results[8863]['component_identities'][1]['identity_match_basis']='自身三史吳郡偉康陸氏夫沈充害，與6189唯一TJ322吳國內史被沈充殺同官同兇手；root4000已回讀對方單key，僅branch可連，整條後趙支不合。'
    for ident,note in {8916:'李威漢陽苟后姑子秦左衛/左仆射、薦王猛與謀殺苻法四key同政治人物。WS堅焚史通后記載是後世追載往事不是李威仍活晚年，不套苻法357死，未知生卒。',8917:'李昶中堅李暠父18卒、暠遺腹，省姓父昶原句已讀；沒有獨立暠出生及父卒日，遺腹也可跨年不能反鎖351或334，唯SL年齡保記不確年。',8918:'李歴/厯/歷後趙屯衛國、秦將麻田、燕兖州同政治轉仕；TJ明張平李歷高昌降燕又晉又秦正連五key時序，不誤認必混。燕攻濮本人奔荥陽而非死，生卒未知。',8919:'李邽遼西魏趙郡守降燕、369豫州斷溫糧道、370降秦尚書同軍政官序五key閉；未見同名異人家系矛盾。被遷降官不是明卒，生卒未知。',8920:'樂弘魏荊州刺史351以城歸晉兩key同，不把秦苻弘或樂立同音合，無生卒。',8921:'樂立故趙將屯許昌歸晉，與同篇荊州樂弘不同名不擅並；僅SL未知生卒。',8922:'毛后原CBDB夫苻登與JS列女母后大界拒辱全橋，TJ389八月本人被姚萇殺明年/月；不套魏明帝毛后237死或index347出生，生年未知。',8923:'四key王凝之叔平羲之次子、道韞夫、會稽內史孫恩害，CBDB父妻雙橋原文，399十一月甲寅JS紀明月日；本地TJ公元三三九錯碼不採，其隆安三己亥與本紀一致399。子被殺/妻存活別分，出生未知。',8925:'真淨百濟近肖古王2年正月拜朝廷佐平、后親戚，與僧人真淨不同不能憑名字判僧；347為已任職上界非第一仕年，生日死亡未載。'}.items():results[ident]['unresolved'].append(note)
    round41c_reads={8924:[('js_11','坦之字文度'),('js_12','王坦之又尝著《公谦论》'),('js_13','呼安及王坦之，欲于坐害之'),('js_18','遣侍中王坦之征温人相'),('tj_18','温以抚军司马王坦之为长史'),('js_10','谢安与王坦之尝诣温论事'),('js_14','桓冲、王坦之、刁彝之徒'),('js_16','惟谢安、王坦之以死守之'),('sg_09','述子坦之字文度北中郎将徐兖二州刺史'),('gs_01','谢安王文度悉皆友善'),('ssxy_00','坦之字文度太原晋阳人祖东海太守承'),('ssxy_01','论成示王中郎【王坦之已见】'),('ssxy_02','安北王坦之也'),('ssxy_03','王北中郎不为林公所知')]}
    for ident,qs in round41c_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'王坦之十四key按父述字文度與官爵原段逐鍵覆核','Read enclosing person reference: main biography, named episodes and explicit alias glosses; SG/GS citations concern Eastern Jin man rather than Three Kingdoms date') for ch,ph in qs)
        results[ident].update(review_status='primary_text_corroborated',source_identities_all_adjudicated=True,identity_status='wang_tanzhi_wendu_wangshu_son_lantian_heir_fourteen_keys_all_subjects_bridged',last_manual_review_batch='round41c')
    tanzhia=chunk_ev('js_11','坦之寻亦卒，时年四十六','晉書王述附坦之傳46卒','實讀述子坦之文度、桓溫長史、毀簡文攝政詔、共謝安輔少帝、北中郎廣陵、諫聲樂、書公謙、臨終不私憂國與卒46；不是弟祎之未30卒或子愉被殺。')
    tanzhid=evidence(SOURCE/'_cache/texts/资治通鉴.txt','夏，五月，丙午，蓝田献侯王坦之卒','資治通鑑寧康三375五月丙午王坦之卒','本年乙亥公元375，與JS同臨終書謝安桓沖惟國不私整句，爵藍田父述嗣+諡獻同主傳；本地header誤歸簡文不影響明寧康三/乙亥/375，按實際孝武年號。')
    results[8924].update(historical_birth=date(330,tanzhia,age=46),historical_death=date(375,tanzhid,month=5,day_ganzhi='丙午'))
    results[8924]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8924]['evidence'].extend([tanzhia,tanzhid,chunk_ev('ssxy_03','王中郎以围棋是坐隐','世說巧藝王中郎坐隱別稱原則讀','同本各明注王中郎文度王坦之，林公即色與不得高士同問答人物；書內以圍棋為手談語林異記不當生卒。'),chunk_ev('js_12','时王坦之、刁彝在坐','晉書陸納傳桓溫酒席王坦之','原candidate含陸納杯酒待溫之座，與同桓溫廷臣身份一致，非只看公謙主名即稱本卷全無混。')])
    results[8924]['unresolved'].append('十四keys全部本人段與字/父/官名橋：文度王述子藍田侯、北中郎、安北追諡，SG注家系是東晉不可按三國原書時代推年；GS於法開60卒不是王坦之。375寧康三五月丙午與JS46卒推330虛歲，日期非公曆生日；本地TJheader歸帝稱簡文有誤，年號乙亥明指孝武375。')
    round42_reads={8926:[('sl_03','遣其邻人赵瞻往劳之'),('tj_17','使其乡人赵瞻往劳之')],8927:[('tj_17','渤海人逄约因赵乱')],8928:[('js_17','郭荷，字承休'),('sl_09','郭荷字承休略阳人也')],8929:[('js_18','镇东将军邓定'),('sl_09','镇西将军邓定平南将军王润'),('tj_16','镇东将军邓定')],8930:[('js_07','隗文、邓定等复反'),('js_18','王誓、邓定、隗文等反')],8931:[('tj_17','平南将军高崇、征虏将军吕护执洛州刺史郑系')],8932:[('sl_02','平南将军髙棠征虏将军吕护执洛州刺史郑系')],8933:[('sl_03','广义将军岷山公黄笛上表俊')],8934:[('tj_17','使其将上官恩据洛阳')],8935:[('sl_06','畧阳伏子成为左部帅')],8936:[('js_13','施遣参军何融率壮士百入邺'),('sl_02','施遣参军何融率壮士百余人入邺'),('tj_17','施宣言使督护何融迎粮')],8937:[('sz_00','僧基，本姓明，济南人也')],8938:[('js_17','公孙永，字子阳'),('sl_03','公孙永字子阳襄平人也')],8939:[('js_17','公孙凤，字子鸾'),('sl_03','公孙凤字子鸾上谷人也')],8940:[('sl_02','刘宁及弟崇帅胡骑三千奔晋阳'),('sl_03','刘宁及弟崇帅胡骑三千奔晋阳'),('tj_17','刘宁及弟崇帅胡骑三千奔晋阳')],8941:[('sl_03','刘洛中山浦阴县人也')],8942:[('js_13','复遣行人刘猗诣尚请救'),('sl_02','蒋干遣侍中缪嵩詹事刘猗奉表归晋'),('tj_17','侍中缪嵩、詹事刘猗奉表请降')],8943:[('sl_03','兴诣郡自列称逃是兴身')],8944:[('sl_06','建武将军刘遯据仓垣'),('tj_17','荥阳太守刘遯据仓垣')],8945:[('sl_02','建兴人史科知其谋'),('tj_16','建兴人史科知其谋')],8946:[('js_00','生子道生，为世子'),('js_09','会稽思世子道生，字延长')],8947:[('sl_03','夔逸为秦州刺史'),('tj_17','夔逸为秦州刺史')],8948:[('sl_04','张弘宋修会王擢'),('sl_09','张弘宋修[一作宗愍]'),('tj_17','张重华遣将军张弘、宋修会王擢'),('js_15','璩使宋修、张弘、辛挹、郭普距之'),('sl_05','使与其将张弘宋修连兵伐雄')],8950:[('sl_06','天水尹赤为司马'),('tj_17','天水尹赤为司马'),('sl_07','族人尹赤为襄司马')],9073:[('sl_03','渤海人逢约因赵乱')]}
    for ident,qs in round42_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第42批燕晉涼同章本人與省姓家屬逐鍵實讀','Actual named passage read; collateral stories, captures, reinstatements, posthumous sightings and mass losses are not person date proof') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round42')
    results[8932].update(review_status='duplicate',duplicate_of=8931,identity_status='gao_chong_tang_character_variant_same_pingnan_luhu_zhengxi_defection',duplicate_basis='高崇/TJ髙棠/SL同平南將軍、同征虜呂護、執洛州鄭系以地降晉完整唯一案，雙方單key皆實讀')
    results[8931]['evidence'].append(chunk_ev('sl_02','平南将军髙棠征虏将军吕护执洛州刺史郑系','SL高棠與TJ高崇同官同案字異','只是校異名及去重，不依晚本單年推精生卒。'))
    results[9073].update(review_status='duplicate',duplicate_of=8927,identity_status='feng_yue_逢逄_same_bohai_governor_fengyi_abduction_later_renamed_diao',duplicate_basis='SL逢約/TJ逄約同渤海太守、封奕弈鄉友誘單馬遭張安挾馬歸營、後改名釣與亡還再降全部唯一事；雙方唯一key均核')
    results[8927]['evidence'].append(chunk_ev('sl_03','以约诱于人而遇获更其名曰钓','SL逢約改釣同TJ逄約改釣','名字逢逄字異與後釣改名在原文明記，無需另造逄釣人物。'))
    jini=chunk_ev('sz_00','十八，隆安元年卒矣','比丘尼傳僧基397卒68','親讀明姓濟南、21拒嫁出家、344褚后延興寺與本末年六十八分行，397−68+1=330虛歲推，不是21卒。')
    results[8937].update(review_status='primary_text_corroborated',historical_birth=date(330,jini,age=68),historical_death=date(397,jini))
    results[8937]['evidence'].append(jini)
    songshi=evidence(SOURCE/'_cache/texts/魏书.txt','曾祖奭，晋昌黎太守。后为慕容廆长史。祖活','魏書33宋隱傳曾祖奭祖活','原CBDB31277宋奭notes明引魏書33.773、子31278宋活，本文宋隱曾祖奭、祖活、父恭三代官名閉合；宋隱13及晚年卒不可套曾祖。')
    results[8949].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='original_cbdb_songshi_son_songhuo_matched_weishu33_original_citation',last_manual_review_batch='round42_cbdb_primary')
    results[8949]['evidence'].extend([songshi,dict(citation='原CBDB31277宋奭原庫來源與家系',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death0，index null；notes Wei Shu33.773；KIN直系祖先宋昌31276(code2)、子宋活31278(code180)。',interpretation='0留未載，code2已核為直系祖先非父，祖先宋昌年代不能套宋奭生卒；子活橋原魏書本人官籍。')])
    results[8928]['source_reported_age_at_death']={'age':84,'calendar_year':None,'source':'晉書郭荷傳'}
    results[8938]['source_reported_old_age']={'age_min_exclusive':90,'event':'隱平郭南山操尚不虧時','not_proven_age_at_death':True,'source':'晉書公孫永傳'}
    results[8946]['source_reported_age_at_death']={'age':24,'calendar_year':None,'source':'晉書會稽思世子道生傳'}
    for ident,note in {8926:'趙瞻常煒鄉人受燕主使詐勸說同對答兩key閉，炜被囚/釋非瞻死，未知生卒。',8927:'逄約渤海守遭封弈給使張安劫、改釣、後亡還再降原TJ本人，SL9073逢約同役可去重；不是因不同字新造逄釣，未知生卒。',8928:'郭荷承休略陽84卒，祖整公車五征非己，張祚征太子友後歸東山沒精年，不把祚355死或同段郭瑀372被征反推荷卒，生卒未知。',8929:'鄧定成漢鎮東(JS/TJ)/鎮西(SL)將亡國反抗與再據成都立范賁三key閉；SL另李雄早年同名不在本candidate範圍。原晚本混入日南劉雄段是記事串錯不把本人拆成另一邓定；軍敗與范賁349被斬非定明死，未知生卒。',8930:'隗文成漢餘將與鄧定反晉後立范賁兩key同，諸史斬范賁不明斬文本人，未知生卒。',8931:'高崇晉受降魏平南將軍，同SL髙棠同呂護執鄭系降晉獨役→8932字異可去重；俘鄭系不是己死，生卒未知。',8932:'髙棠SL唯一key同TJ高崇平南/呂護/鄭系整串事件，→8931；生卒不詳。',8933:'黃笛燕廣義將軍岷山公以表上遭儁責號未改不敬，不能只近黃泓名將硬合，無本人生卒。',8934:'上官恩張遇部將據洛陽，非姚襄部將或殷浩北伐遇軍死者；單TJ本人清楚，生卒未知。',8935:'伏子成略陽姚襄左部帥，馬死是襄坐騎、父弋仲死不是伏子成卒，單SL無生卒。',8936:'何融晉戴施參軍/督護入邺取傳國璽三key同役，騙運糧送枋頭又守邺敘次可不同回次不造兩何融；城陷悬缒逃非死，未知生卒。',8937:'僧基明姓濟南尼，21出家不是死，344立延興寺後397隆安元68卒推330虛歲；前慧湛傳無死年及後道馨不同人。',8938:'公孫永子陽襄平、南山隱居90餘、燕暐征邺詐狂放還、秦欲問候未至而亡兩傳全讀；90餘是活動時不是明卒齡，無確年不能推生日。',8939:'公孫鳳子鸞上谷昌黎九城山被燕暐征數年病卒兩key全閉；公孫永90餘不是鳳，未知生卒。',8940:'劉崇省姓寧及弟崇，352胡騎3000奔晉陽三key同親族事件，非劉邦兄劉仲或漢劉崇，不把冉閔邺亡當崇死，未知生卒。',8941:'劉洛中山浦陰應征弟興代役逃亡，兄弟爭代命終被燕暐赦，縣以本名捕斬是擬罪敘不是已斬兩兄弟；無生卒。',8942:'劉猗魏冉智詹事使謝尚求援被戴施止索璽，三key同案，無本人死句不套城陷，未知生卒。',8943:'劉興為兄洛代役後逃，興自陳請代兄死而與兄獲暐赦，單SL省姓本人已讀，不把請死當實死，生卒未知。',8944:'劉遯晉殷浩建武將軍/荥陽太守352據倉垣，同地同主官序兩key閉；殷浩敗、劉啟王彬之死非遯死，無生卒。',8945:'史科建興人察覺楊杯等謀殺石韜告虎，越牆免於被滅口，無本人被殺句；未知生卒。',8946:'道生延長簡文昱王簡姬子，母子永和4348幽廢與本人後24卒無絕對年不能等同348死亡。後孝武白日見道生為亡魂故事非372後本人仍活；無後過繼珣之、珣之被劉裕殺也非己，未知生卒。',8947:'夔逸故趙擁兵將352降燕授秦州刺史兩key同，不因秦州就改仕前秦，無本人死，未知生卒。',8948:'宋修涼將347枹罕守與張弘、後龍黎同張弘被俘五key可橋，原SL一作宗愍字異保；SL09永和八352/TJ敘353龍黎年異，只保記事源別不當確活動輸入或死亡，無生卒。',8949:'宋奭原CBDB子活橋魏書宋隱曾祖奭官昌黎守/廆長史，父恭屬隱父不是奭父，code2宋昌是直系祖先非父。0日期留未知，不把後宋隱老卒套奭。',8950:'尹赤天水姚襄司馬退奔秦與族尹緯傳稱赤為襄司馬三key原官家系閉。TJ奔秦vsSL07苻堅時以赤降襄禁諸尹敘向源異保，不擅修一方或以同名必拆；無本人死亡確年。',9073:'逢約SL唯一key與TJ逄約同渤海守鄉友誘捕改名釣全故事，→8927，不依晚源單年產生生日死亡。'}.items():results[ident]['unresolved'].append(note)
    round43_reads={8954:[('sl_04','张重华遣将军张弘宋修'),('sl_09','重华遣将军张弘宋修'),('tj_17','张重华遣将军张弘、宋修')],8955:[('sl_03','典书令张悕为右仆射'),('tj_17','典书令张悕为右仆射'),('sl_08','父悕为俊尚书右仆射')],8956:[('sl_02','车骑将军张温谏曰'),('sl_03','车骑将军张温切谏'),('tj_17','车骑将军张温谏曰')],8957:[('sl_06','强白为后部帅')],8958:[('tj_16','以弟交为左贤王')],8959:[('js_13','濮阳太守戴施据枋头'),('js_18','辅国将军戴施屯河上'),('sl_02','濮阳太守戴施自仓垣次于棘津'),('sl_03','戴施蒋干悬缒而下'),('tj_17','乐弘攻督护戴施于仓垣'),('tj_18','河南太守戴施奔宛'),('hhs_12','冉闵大将军蒋干以传国玺付河南太守戴施')],8960:[('sl_02','曹伏驹开门为内应'),('tj_17','曹伏驹开门纳闵')],8961:[('sl_02','中书令李垣及诸王公卿士')],8964:[('sl_03','北平乐抄少隽出与齐名')],8965:[('sl_03','谓权翼曰皇甫真故大器也'),('sl_04','太原薛讃略阳权翼见而惊'),('sl_06','权翼见而谓之曰儿方以才望自显'),('tj_17','太原薛瓒、略阳权翼为参军'),('tj_18','故姚襄参军薛赞、权翼善'),('sl_05','薛讃权翼密说坚曰'),('gs_01','坚谓仆射权翼曰')],8966:[('sl_03','生擒叛人段思'),('tj_18','温以燕降人段思为乡导')],8967:[('sz_01','安公法和对共'),('gs_01','释法和。荣阳人也')],8970:[('sl_04','谢尚遣冠军将军王侠攻许昌'),('tj_17','谢尚遣冠军将军王侠攻许昌')],8971:[('sl_06','会镇西将军张遇反于许昌使其党王恩据洛阳')],8972:[('jk_02','遣殿中都尉王惠如洛阳修卫五陵')],8973:[('js_00','简文顺王皇后，讳简姬')],8974:[('js_00','晋陵太守王蕴女'),('js_17','王蕴，字叔仁'),('js_14','光禄大夫蕴子，定皇后之兄也'),('ssxy_02','阿兴王蕴小字'),('ssxy_03','光禄王蕴也')],8975:[('sl_06','畧阳王黒郍为前部帅')]}
    for ident,qs in round43_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第43批秦燕臣與原書注引逐鍵實讀','Read named role and explicit family/alias context. A late annotation in an early book is dated by its actual subject, not by the book title') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round43')
    cuiweb=dict(citation='新唐書卷072下大房崔氏譜本人段實讀',url='https://zh.wikisource.org/wiki/新唐書/卷072下',retrieval='agent-reach Jina Reader',excerpt='廓生遄。遄生懿，字世茂。五子：連、琨、格、邈、殊，又三子：怡、豹、偘為一房，號「六房」。連字景遇，巨鹿令，號「大房」。生郡功曹綽。',verification='individually_read_original_genealogical_table',interpretation='原CBDB31265父遄、長子連次子琨，31264父廓子懿，均在引用新唐書72C譜明同家系；表未記兩人生卒，不能由後人官年倒算。')
    for ident in [8951,8953]:
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='original_cbdb_cui_family_links_matched_xintangshu72_table',last_manual_review_batch='round43_cbdb_xintangshu')
        results[ident]['evidence'].append(cuiweb)
    results[8951]['evidence'].append(dict(citation='原CBDB31265崔懿原庫實讀',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death0，notes Zhou Jiayou2.288/XTS72C44b；KIN父31264崔遄、子31941崔連/31266崔琨。',interpretation='父遄子連琨三層與原譜同人；0為未載，非公元零年。'))
    results[8953]['evidence'].append(dict(citation='原CBDB31264崔遄原庫實讀',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death0，notes Zhou Jiayou2.288/XTS72C44b；KIN父31259崔廓、子31265崔懿。',interpretation='父廓子懿橋原譜；沒有史實生卒或初仕。'))
    results[8952].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb195417_cuikun_father_of_jing_yu_original_epitaph_unread_not_same_as_table_cuikun_proven',last_manual_review_batch='round43_cbdb_original')
    results[8952]['evidence'].append(dict(citation='原CBDB195417崔琨原庫日期及子嗣限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='death413/ index350，Death year estimated by Tackett(cc413)，補遺7:138–139/文物1999.1:93/洛陽新獲續504；KIN子195416崔經/195418崔鬱。',interpretation='413明估撤史實年；新唐書另崔懿子琨對應CBDB31266，尚無經鬱父身份橋，不因同名就去重或稱已讀此墓誌。'))
    yangjie=evidence(SOURCE/'_cache/texts/魏书.txt','高祖结，仕慕容氏，卒于中山相。曾祖珍','魏書58楊播傳高祖結曾祖珍','楊播自云恒農華陰、結慕容中山相、珍魏上谷守、祖真父懿，與CBDB12362楊結中山相及子12363楊珍同名官家系，未轉用後孫父懿任官年。')
    results[8963].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='cbdb_yangjie_zhongshan_xiang_son_yangzhen_bridged_weishu_yangbo_genealogy',last_manual_review_batch='round43_cbdb_primary')
    results[8963]['evidence'].extend([yangjie,dict(citation='原CBDB12362楊結原庫notes與子嗣',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death0；notes周佳佑4.473、中山相分支，楊奉8/9世後裔作者算法異，子楊珍12363/楊繼12364。',interpretation='中山相+楊珍子雙橋魏書，不需把世數爭議硬換確年；未聲稱周氏紙本已讀。')])
    duguweb=dict(citation='新唐書卷075下獨孤氏譜原引本人段實讀',url='https://zh.wikisource.org/wiki/新唐書/卷075下',retrieval='agent-reach Jina Reader',excerpt='尸利生烏利。二子：去卑、猛。猛生副論。副論生路孤，路孤生眷，眷生羅辰，從後魏孝文徙洛陽。',verification='individually_read_original_genealogical_table',interpretation='原CBDB32902父烏利子副論、32903父猛子路孤三層全同原引用XTS75B30a；只核原譜此人物主體，不證漢帝血統傳說或各代確生卒，不能憑近名與劉猛/劉副侖自動去重。')
    for ident in [8968,8969]:
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='original_cbdb_dugu_family_links_matched_xintangshu75_table_not_equated_with_other_lius',last_manual_review_batch='round43_cbdb_xintangshu')
        results[ident]['evidence'].append(duguweb)
    results[8968]['evidence'].append(dict(citation='原CBDB32903獨孤副論原庫實讀',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death0、XTS75B30a；父32902獨孤猛/子32904獨孤路孤。',interpretation='0未載，三層吻合原譜；年代不能由世數推精。'))
    results[8969]['evidence'].append(dict(citation='原CBDB32902獨孤猛原庫實讀',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death0、XTS75B30a；父32901獨孤烏利/子32903獨孤副論。',interpretation='原譜這一猛主體已橋，不等於已證其與271匈奴劉猛同人。'))
    yundie=chunk_ev('js_17','太元九年卒，年五十五','晉書外戚王蘊384卒55','親讀蒙子叔仁定后父、開倉吴興/晉陵、光祿五兵尚書、會稽嗜酒、子恭華爽全傳，太元9384本人55卒→330虛歲，非女后38021卒。')
    results[8974].update(review_status='primary_text_corroborated',historical_birth=date(330,yundie,age=55),historical_death=date(384,yundie))
    results[8974]['evidence'].append(yundie)
    results[8967]['source_reported_age_at_death']={'age':80,'calendar_year':None,'source':'高僧傳釋法和傳'}
    results[8971].update(review_status='duplicate',duplicate_of=8934,identity_status='wang_en_shangguan_en_variant_same_zhangyu_agent_luoyang_occupation',duplicate_basis='SL姚襄傳張遇反許昌使其黨王恩據洛陽，與TJ張遇反許昌使將上官恩據洛陽同一明主明城獨事；兩方單key全讀，名稱源異保留')
    results[8971]['evidence'].append(chunk_ev('tj_17','遇怒，据许昌叛，使其将上官恩据洛阳','資治通鑑上官恩與SL王恩同洛陽將','同一張遇許昌叛、謝尚討失利前據洛陽事件，不合其他普通王恩姓名，未知生卒。'))
    for ident,note in {8951:'崔懿世茂父遄子連琨原CBDB雙橋所引新唐書大房崔譜，未載生日死亡；另SL崔懿子遘/連有字形異不靠一支覆蓋全部同名人物，這條只有CBDB一key已明主體。',8952:'崔琨413原庫明Tackett估卒撤，350index非生年。子經鬱墓誌未取得，唐譜另懿子琨CBDB31266不能單名替代此195417，待原誌。',8953:'崔遄原CBDB父廓子懿與新唐書所引三層完全同，原0日期皆不詳；不從子孫年代精推生卒。',8954:'張弘前涼將軍與宋修王擢龍黎被俘三key同，SL09年352/TJ年353敘次異保，不將斬首數千當張弘明死；無本人生卒。',8955:'張悕燕儁典書令右仆射，SL08張恂父悕明儁尚書右仆射同unique君主官爵，省姓原句已讀；恂後歸魏非父仍活，無生卒。',8956:'張溫魏冉閔車騎將軍切諫避慕容恪三key同人，不是孫吳張溫或西晉涼州將，董閔後遇害不自動推其死，無生卒。',8957:'強白姚襄後部帥，前部王黒郍左部伏子成別人，父弋仲死非其死，單SL未知生卒。',8958:'慕容交燕王儁弟左賢王，王皝348卒及從事張悕入官非其生卒；弟慕容友近字待別全源不先合，單TJ未知生卒。',8959:'七key戴施濮陽守/督護取璽到河南太守洛陽奔宛同軍官官序，HHS12是續志注引晉陽秋取璽本人不是漢代戴施。冉闵352死/諸將軍敗與胡彬送璽不是戴施死，未知生卒。',8960:'曹伏駒劉顯大將軍開襄國門納冉閔，明內應不是被屠100公卿的確定死者，兩key全閉生卒未知。',8961:'李垣魏冉智中書令邺亡送薊俘虜，王簡張乾郎肅自殺非垣，單SL生卒未知。',8963:'楊結原CBDB中山相子珍可橋魏書楊播高祖結曾祖珍，卒官但無卒年；奉8/9代屬譜計異不換年，0留未知。',8964:'樂抄北平棋手與悉羅騰齊名少雋，後參軍擒段思斬李述及被苻堅殺全屬騰，不能套抄死，未知生卒。',8965:'七key權翼姚襄參軍轉秦輔苻堅、仆射與道安被獲一人半問答、皇甫真國士/慕容鳳忠臣論皆秦尚書同主脈絡閉；姚襄被殺357、道安385卒非權翼，未知生卒。',8966:'段思燕降人為桓溫鄉導被悉羅騰生擒兩key同，李述被斬是不同人，不能把生擒當死，未知生卒。',8967:'法和SZ01實為合冊出三藏記集苻秦譯經段，安公與法和校毘婆沙，不是比丘尼。GS同道安學、後入關校新經可閉同男僧；80卒無確年不填生卒。',8968:'獨孤副論原CBDB父猛子路孤與所引新唐書三代同，無生卒；唐譜遠祖出漢世祖傳說未另考，不因此當3世紀劉副侖已合，日期不推。',8969:'獨孤猛原CBDB父烏利子副論與所引唐譜三代同，無生卒或本人活動年；不憑猛名與匈奴劉猛強並。',8970:'王俠晉謝尚冠軍將軍攻許昌、秦楊群退弘農兩源同役，沒有本人死亡或生日。',8971:'王恩SL張遇將據洛陽與TJ8934上官恩同主同城同役源異姓名，兩單key已閉→8934；不把一般王恩澤詞命中當本人原句。',8972:'王惠殿中都尉永和八遣洛陽修衛五陵，修陵非本人死亡或坟主；單JK未知生卒。',8973:'王簡姬太原王遐女簡文會稽王妃道生母，348母子幽廢後以憂死無精年，372追尊合葬不本人仍活；未知生卒。',8974:'五key王蘊叔仁/阿興、蒙子定后父王恭父、光祿會稽嗜酒全部原傳/別稱官家系閉；384太元九55卒推330虛歲，女38021及父蒙39、子恭398遇害不套本人。',8975:'王黑那原SL王黒郍略陽姚襄前部帥，時馬死是襄馬、弋仲死是襄父，未明己卒，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    round44a_reads={8976:[('sl_02','龙腾将军田香开门降评')],8977:[('sl_06','转赐卢豚生子鱼')],8978:[('jk_02','以兖州刺史秦裔等为前锋')],8979:[('sl_02','司隶校尉籍罴中书令李垣')],8980:[('sl_02','显领军将军纪路率众千余斩闗奔枋头')],8981:[('sl_02','遣侍中缪嵩詹事刘猗奉表归晋'),('tj_17','侍中缪嵩、詹事刘猗奉表请降')],8982:[('yl_07','罗腾，字叔龙，工围棋')],8983:[('js_13','振武将军胡彬率骑三百迎玺'),('sl_02','振武将军胡彬率骑三百迎玺')],8984:[('sl_02','闵大将军董闰车骑将军张温谏曰'),('sl_03','大将军董闰及车骑将军张温切谏'),('tj_17','大将军董闰、车骑将军张温谏曰')],8985:[('js_12','蔡裔者，有勇气，声若雷震'),('js_13','兄子裔字元子'),('sl_02','蔡裔仕虎振武将军')],8986:[('js_13','冉闵之子智与其大将蒋干来附'),('sl_02','留大将军蒋干等使辅太子智守邺'),('sl_03','闵大将军蒋干辅闵子智'),('tj_17','留大将军蒋干使辅太子智守邺')],8987:[('tj_17','太原薛瓒、略阳权翼为参军')],8988:[('sl_03','尊生母兰氏为文昭皇后'),('sl_05','追尊母兰氏为文昭皇后'),('wsh_00','次妃兰氏，生二子')],8990:[('sl_02','长水校尉马愿龙腾将军田香开门'),('sl_03','魏长水校尉马愿等开邺城'),('tj_17','魏长水校尉马愿等开邺城')],8991:[('sl_03','开弟商刚毅严重好学有事干')],8992:[('sl_03','清河丁娆'),('tj_17','清河丁娆')],8993:[('sl_04','遇与中黄门刘晃谋夜袭健'),('tj_17','遇与黄门刘晁谋夜袭健')],8994:[('js_12','冠军将军刘洽镇鹿台'),('sl_06','冠军将军刘洽镇鹿台')],8995:[('js_12','建武将军刘遁据仓垣')],8996:[('js_15','中卫将军史景亦没于阵')],8997:[('js_09','临川献王郁，字深仁')],8998:[('js_11','及虏周成'),('js_18','执降贼周成以归'),('sl_02','使将军苏彦周成率甲士'),('sl_06','襄自许昌攻周成于洛阳'),('tj_17','使将军苏彦、周成帅甲士')],9000:[('tj_17','阳平孙元各拥兵分据城邑'),('tj_18','燕左卫将军孟高讨擒之'),('sl_03','阳平孙原')],9373:[('sl_03','悉罗腾字叔龙范阳涿鹿人也'),('sl_04','悉罗腾为三署郎'),('tj_18','尚书郎悉罗腾皆从军')]}
    for ident,qs in round44a_reads.items():
        results[ident]['evidence'].extend(chunk_ev(ch,ph,'第44批邺亡秦燕同役及晉宮親族逐鍵原文實讀','Read each individual scope directly, including shorthand names; proposed death from captivity, adjacent casualty or posthumous report is not accepted') for ch,ph in qs)
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_listed_source_subjects_adjudicated',last_manual_review_batch='round44a')
    results[8978].update(review_status='duplicate',duplicate_of=8985,identity_status='qin_cai_yi_character_variant_same_yanzhou_governor_yinhao_vanguard',duplicate_basis='JK殷浩北伐坠馬至壽春使兖州秦裔前鋒，同JS12兖州蔡裔前鋒/JS13蔡豹兄子裔率彭城北伐唯一官役；8985三keys含雷聲故事已閉')
    results[8985]['evidence'].append(chunk_ev('jk_02','以兖州刺史秦裔等为前锋','建康實錄秦裔與晉書蔡裔同殷浩兖州前鋒','字秦/蔡異，同一浩北伐前鋒不是普通同名猜合；未精定其卒軍是哪次出兵。'))
    results[8982].update(review_status='duplicate',duplicate_of=9373,identity_status='luoteng_xiluoteng_short_name_same_shulong_go_master_lechao_rival',duplicate_basis='YL燕書羅騰字叔龍圍棋無敵後北平樂抄與齊名，SL悉羅騰字叔龍同完整棋名故事；target三keys尚書郎369隨垂370秦三署同主傳已全讀')
    results[8982]['evidence'].append(chunk_ev('sl_03','悉罗腾字叔龙范阳涿鹿人也工围碁','SL悉羅騰與燕書羅騰同字同棋事','唯同字叔龍、樂抄少年棋名整故事相合，能校省悉字，不按單騰字或棋官泛合。'))
    results[9373]['evidence'].append(chunk_ev('yl_07','罗腾，字叔龙，工围棋','御覽燕書羅騰同悉羅騰省字','sourcekey不在本target但actual主體同，可以吸收省字名；本傳秦末謀殺堅被殺是SL晚記，無獨立早源死亡年。'))
    results[8995].update(review_status='duplicate',duplicate_of=8944,identity_status='liudun_遁遯_same_yinhao_cangyuan_commander',duplicate_basis='JS殷浩建武劉遁據倉垣與SL同浩建武劉遯據倉垣，TJ荥陽守劉遯同場同城全部官序閉；遁遯異字可合')
    results[8995]['evidence'].append(chunk_ev('sl_06','建武将军刘遯据仓垣','SL劉遯與晉書劉遁同倉垣將','雙方皆殷浩北伐佈防並劉洽鹿臺同一任命組，字異校同。'))
    results[8988].update(review_status='identity_conflict',source_identities_all_adjudicated=False,identity_status='mixed_murongchui_mother_lan_and_tuoba_yituo_consor_lan',component_identities=[dict(identity='慕容皝妾慕容垂生母蘭氏，垂追文昭皇后',keys=[k for k in results[8988]['keys'] if k.startswith('sl:')],historical_birth=None,historical_death=None),dict(identity='拓跋桓帝次妃蘭氏，生藍與思帝',keys=[k for k in results[8988]['keys'] if k.startswith('wsh:')],historical_birth=None,historical_death=None)])
    results[8989].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb_peikai_peiwu_son_peifan_father_original_genealogy_subject_not_located',last_manual_review_batch='round44a_cbdb_original')
    results[8989]['evidence'].append(dict(citation='原CBDB31969裴開原庫及原引用取得限制',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death0，notes XTS71A18a；KIN父31869裴武，子31970裴範。',interpretation='新唐書卷071上Jina實取10015字符，有裴氏段但未定位這一裴武→開→範，未取得本人原句不能稱原引已核或套裴楷；0仍未知。'))
    liangyear=evidence(SOURCE/'_cache/texts/资治通鉴.txt','苟苌及掌据战于洪池','資治通鑑太元元376八月前涼亡國戰','實讀376年header及八月清石/石城津、馬建降、洪池戰、席仂死、金昌天錫降，與JS同組地名官將順序定史景所在役年。掌/常據字異同征東三萬洪池；此用整役同步，不從SL重編本定年。')
    jingdie=chunk_ev('js_15','中卫将军史景亦没于阵','晉書張天錫傳史景本人陣亡','本段征東常據、席仂戰死、趙充哲苦戰死、史景亦沒，後天錫出戰降秦；獨立TJ376洪池至金昌同役年錨，史景未另明日不套辛卯或癸巳。')
    results[8996].update(review_status='primary_text_corroborated',historical_death=date(376,jingdie))
    results[8996]['historical_death']['source']='晉書張天錫傳本人死句；資治通鑑太元元376前涼亡國同役定年'
    results[8996]['evidence'].extend([liangyear,jingdie])
    judie=chunk_ev('js_15','常据、席仂皆战死','晉書張天錫傳常據本人戰死','原書征東常據同三萬洪池/金昌涼亡戰；TJ原作掌據同征東三萬洪池自刎，二書同年376，死因細節另列不合成無爭議某死日。')
    results[8817].update(review_status='primary_text_corroborated',historical_death=date(376,judie))
    results[8817]['historical_death']['source']='晉書張天錫傳常據本人死句；資治通鑑太元元376洪池同役'
    results[8817]['evidence'].extend([judie,liangyear])
    results[8817]['source_reported_death_circumstance_variants']=[{'source':'晉書張天錫傳','name':'常據','statement':'常據席仂皆戰死'},{'source':'資治通鑑376八月辛卯','name':'掌據','statement':'洪池兵敗，董儒授馬不受，伏劍而死'}]
    results[8997]['source_reported_age_at_death']={'age':17,'calendar_year':None,'source':'晉書臨川獻王郁傳'}
    for ident,note in {8976:'田香魏龍騰將軍同馬願開邺門降燕，角色是內應，不把同城王簡等自殺或冉閔被殺套本人；唯SL未知生卒。',8977:'盧豚得慕容儁轉賜嵩女、生子魚後有寵於魏是子魚不是盧豚在魏時仍活；single晚SL本人關係明，未知生卒。',8978:'秦裔JK殷浩兖州前鋒同蔡裔，原姓名字異全閉→8985，不僅憑字近。',8979:'籍羆魏司隸校尉邺陷被俘送薊，王簡等自殺非本人已死，唯SL未知生卒。',8980:'紀路劉顯領軍斬關率千人奔枋頭，顯及百官被誅不包含已逃的本人，無生卒。',8981:'繆嵩魏侍中與劉猗求謝尚援，原使返回邺與取璽整案兩key同，未載被斬，無生卒。',8982:'羅騰燕書字叔龍棋藝/樂抄後出同SL悉羅騰全故事，省悉字→9373三key全閉，無獨立確生卒；不將對手樂抄同合。',8983:'胡彬晉振武將軍率騎300迎傳國璽兩key同，騎數非年齡，後徐州/淝水胡彬主記錄未全橋前不硬刪，無生卒。',8984:'董閏冉閔大將軍與張溫諫避燕三key同，閔後被俘斬不明閏同行俱死，無生卒。',8985:'蔡裔元子蔡豹兄子、兖州刺史浩北伐軍鋒，同JS12雷聲逐賊勇士/SL仕虎雷聲故事可橋轉晉，不因事趙即另造人；卒於軍沒有原明某次年月，未知生卒，不擅填352353。',8986:'蔣幹冉智大將軍邺守取璽四key同，不是赤壁蔣幹子翼；城破本人悬缒出走不是自殺王簡等，未知生卒。',8987:'薛瓚姚襄太原參軍，王亮卒或姚襄357死不是瓚卒，字瓚/讃源異未全核他記前不硬去重，單TJ無生卒。',8988:'蘭氏三key真混：兩SL慕容皝妾垂母追文昭，WS拓跋桓帝次妃生藍思帝弗，丈夫子不同不可一人；兩支史實BD均空，不把垂386追尊當母仍活。',8989:'裴開原CBDB父裴武子裴範已定位，但原引XTS71A18a本人段未取，不以裴氏泛族譜當本人已讀，不與裴楷同名音合；0日期未知。',8990:'馬願魏長水校尉邺開門降燕三key同，不能將城中百官自殺當願死，未知生卒。',8991:'高商高瞻子高開弟、省姓開弟商原整傳已讀，范陽改遼西為避兄亡昌黎郡；病哭嘔血而仍授遼西非本人已死，生卒未知。',8992:'丁嬈清河聚眾邑主353降燕授立節將軍兩源同，朱秃杜能孫原同批各不同人，無本人死，未知生卒。',8993:'劉晃/劉晁秦黃門與張遇謀開門，TJ同段晁後連寫晃明字異同人；門未開事覺伏誅主張遇，未明晃一同死，不無證補353卒，生年未知。',8994:'劉洽晉殷浩冠軍將軍據鹿臺兩key同，與趙冠軍王洽不同姓役不得硬合，無生卒。',8995:'劉遁JS殷浩建武倉垣同8944劉遯SL及TJ同官同城同配劉洽派軍，遁遯字異→8944，生卒不詳。',8996:'史景前涼中衛將軍在亡國同役陣沒JS本人明死，TJ376太元元洪池金昌天錫降秦整役獨立定年；不套掌據辛卯或充哲癸巳死日，出生未知。',8997:'司馬郁深仁簡文昱胡淑儀子，17薨無確年，寧康追封/孝武見亡魂都非本人仍活或當年死；未知生卒。',8998:'周成趙蘇彥同冉閔命殺石遵、後占洛陽356受姚襄圍為桓溫收降五key同軍將生平鏈，俘而歸不說處死，無生卒。',9000:'孫元陽平邑主兖州守353降燕、369勸高平清河叛為孟高擒，SL孫原同朱秃杜能丁嬈四人一組明字異，三key閉；被擒不明被殺，生卒未知。',9373:'悉羅騰字叔龍燕尚書郎369垂軍擒段思斬李述、370秦三署郎三key閉，吸收YL羅騰同字棋譜故事。SL秦末與暐謀殺堅被誅僅晚重編無獨立早源精年，不自動填385。'}.items():results[ident]['unresolved'].append(note)
    # Round44b: Yao Xiang's sixteen source subjects, including retrospective references.
    xiang_reads=[('js_08','为姚襄所败'),('js_11','又从温破姚襄'),('js_12','姚襄反叛，浩大败'),('js_13','浩遂行果为姚襄所败'),('js_18','姚襄屯水北'),('sl_02','乃悉众出与姚襄战'),('sl_03','姚襄遣使来降'),('sl_04','为姚襄所虏'),('sl_06','姚襄字景国弋仲之第五子也'),('tj_17','三月，姚襄及赵汝阴王琨各引兵救襄国'),('tj_18','姚襄将图关中'),('wsh_00','姚襄为苻眉所杀'),('wsh_02','昭成时，弋仲死，子襄代'),('js_14','姚襄去浩十里结营以逼浩'),('sl_05','姚襄略地关中'),('ssxy_03','羗姚襄上书归降')]
    for ch,phrase in xiang_reads:
        results[8999]['evidence'].append(chunk_ev(ch,phrase,'姚襄逐源本人身份核對','父弋仲兄弟苌、晉殷浩北伐山桑、洛陽桓溫伊水及三原秦役同一生平鏈；SS引晉陽秋把苻健死置於北伐之前是源敘時序問題，不能反推本人未卒。'))
    xdie=evidence(SOURCE/'_cache/texts/晋书.txt','襄败，为坚所杀，时年二十七，是岁晋升平元年也','晉書姚弋仲附姚襄本傳','本傳完整本人弋仲第五子景國至三原戰亡，明升平元357年二十七；虛歲357-27+1=331推生，非確生日。')
    xmonth=chunk_ev('tj_18','襄所乘骏马曰黧眉䯄','資治通鑑升平元357五月姚襄三原被擒斬','本年丁巳357及五月邓羌誘戰三原本人被擒斬，下一戊寅屬燕攻敕勒不套本人死日；魏書作苻眉、晉書作堅所殺保歸責異文。')
    results[8999].update(review_status='primary_text_corroborated',source_identities_all_adjudicated=True,identity_status='yaoyizhong_fifth_son_jingguo_yaochang_brother_all_sixteen_keys_adjudicated',historical_birth=date(331,xdie,age=27),historical_death=date(357,xmonth,month=5),last_manual_review_batch='round44b')
    results[8999]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8999]['evidence'].extend([xdie,xmonth])
    results[8999]['source_reported_death_circumstance_variants']=[{'source':'晉書','statement':'為堅所殺'},{'source':'魏書序紀/羌姚萇傳','statement':'為苻眉所殺'},{'source':'資治通鑑','statement':'秦兵擒而斬之'}]
    results[8999]['unresolved'].append('姚襄十六keys均本人；TJ363故將張駿叛是追稱非襄仍活，WS苌夢中自辯殺堅者兄襄非歷史在世記錄。357五月卒，27虛歲推331生；十七授兵無明年不當確首次登場，保遊戲登場估算。')
    # Round45a: grouped reading of Liang, Dai, and early Qin subjects.
    round45a_reads={9001:[('js_15','初与重华宠臣赵长、尉缉等结异姓兄弟'),('sl_09','右长史赵长尉缉等结异姓兄弟'),('tj_17','与重华嬖臣赵长、尉缉等结异姓兄弟')],9002:[('js_15','常据、席仂皆战死')],9003:[('sl_09','都尉张据及谢艾上言权幸用事')],9004:[('wsh_00','实君者，昭成皇帝之庶长子也')],9005:[('wsh_00','子斤，失职怀怒，构实君为逆')],9006:[('sl_03','故赵乐陵朱秃平原杜能清河丁娆阳平孙原'),('tj_17','赵末，乐陵朱秃、平原杜能')],9007:[('sl_04','遣叅军阎负梁殊使凉'),('sl_09','遣参军阎负梁殊使凉'),('tj_17','遣参军阎负、梁殊使于凉')],9009:[('sl_03','晋宁朔将军荣胡以彭城鲁郡叛降于俊')],9010:[('sl_02','淮南王昭入议于郑太后前')],9011:[('ssxy_02','前余姚令防稽谢胜等十五人不能赋诗')],9012:[('js_15','司兵赵充哲与苌苦战，又死')],9013:[('sl_04','以步兵校尉金城郭敬为荆州刺史'),('tj_17','以步兵校尉金城郭敬为刺史')],9014:[('sl_04','遣叅军阎负梁殊使凉'),('sl_09','遣参军阎负梁殊使凉'),('tj_17','遣参军阎负、梁殊使于凉')],9015:[('tj_17','三月，交州刺史阮敷讨林邑')],9016:[('sl_02','阴令胡人颉独鹿微告戍者')],9017:[('js_00','前太尉参军、都乡侯粪土臣何琦'),('js_15','何琦，字万伦，司空充之从兄也')],9018:[('sl_02','明为彭城王裕为武兴王')],9019:[('sl_02','明为彭城王裕为武兴王')],9020:[('sl_06','晋唐邑内史陈留刘任降于襄')],9021:[('js_15','遣其将和昊率众伐丽靬戎于南山'),('sl_09','遣别将和昊伐骊靬戎于南山')]}
    for ident,qs in round45a_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_source_key_subjects_individually_adjudicated_round45a',last_manual_review_batch='round45a')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第45批逐人原書段落身份核對','已讀本人官職/親屬及同役上下文，各列key所指本人；未明記生卒者不以政權年代、鄰人死亡、後世追記補齊。'))
    # The same county commandant's objection, with Zhang/Chang name variation.
    results[9003]['evidence'].append(chunk_ev('tj_17','都尉常据请出之','資治通鑑永和九353重華寵臣事件','同都尉請逐祚長等、謝艾上疏及重華拒絕的一組事件，SL張據/TJ常據字異；8817原有六keys已全核，不以近名單獨合併。'))
    results[9003].update(review_status='duplicate',duplicate_of=8817,identity_status='zhangju_changju_same_commandant_petition_subject')
    daidie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','乃执寔君及斤，至长安，车裂之','資治通鑑太元元376十二月滅代事件','上文十二月什翼犍還雲中，斤構寔君殺諸弟弒父，秦執二人到長安車裂，同WS昭成庶長子/孤子；只取同役年度，不聲稱有處刑明日或精月。')
    for ident in [9004,9005]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(376,daidie))
        results[ident]['evidence'].append(daidie)
    dai339=evidence(SOURCE/'_cache/texts/资治通鉴.txt','子寔君监其南','資治通鑑咸康五339五月代分部','同父什翼犍及弟孤監北、子寔君監南，339已領部，原遊戲345生/359登場估算受此約束；不把339當出生或首次出仕。')
    results[9004]['evidence'].append(dai339)
    results[9004]['attested_activity_years']=[339]
    results[9004]['appearance_upper_bound']=339
    liangdie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','秦人闻天锡杀阎负、梁殊','資治通鑑太元元376七月秦使被殺','實讀本年七月二人到姑臧、縛軍門交射、後八月秦進兵；356使凉、371送陰據、376被殺同二使，不以晚出SL單獨定卒。')
    for ident in [9007,9014]:
        results[ident].update(review_status='primary_text_corroborated',historical_death=date(376,liangdie,month=7))
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(liangdie)
    xidie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','秦兵杀军司席仂','資治通鑑太元元376八月','辛卯掌據敗死之後癸巳趙充哲戰之前，席仂明被殺；僅八月無明本人日，不能套辛卯。')
    results[9002].update(review_status='primary_text_corroborated',historical_death=date(376,xidie,month=8))
    results[9002]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9002]['evidence'].append(xidie)
    zdie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','俘斩三万八千级，充哲死','資治通鑑太元元376八月癸巳赤岸役','明癸巳秦入清塞、天錫使司兵趙充哲拒戰、本人戰死，與JS同司兵同涼亡戰橋；不借他將日期。')
    results[9012].update(review_status='primary_text_corroborated',historical_death=date(376,zdie,month=8,day_ganzhi='癸巳'))
    results[9012]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9012]['evidence'].append(zdie)
    results[9008].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='cbdb172413_original_tackett_source_unobtained',last_manual_review_batch='round45a_cbdb')
    results[9008]['evidence'].append(dict(citation='原CBDB172413楊惠嘏原庫日期限定與來源',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='c_birthyear0；c_deathyear414；c_notes Death year estimated by Tackett (cc414)；index_year351；KIN子172414、172426；source32033/71853頁133339。',interpretation='414為原庫明標估算不得確證；351指數年非生年或活動年。未取得Tackett本人原引，身份與日期仍待核。'))
    results[9017]['source_reported_age_at_death']={'age':82,'calendar_year':None,'source':'晉書何琦傳'}
    results[9017]['evidence'].append(chunk_ev('js_15','年八十二卒','晉書何琦本傳卒齡','完整本傳實讀：萬倫充從兄，14喪父阜，郡主簿泾縣令、拒導玩溫及簡文召，82卒無年；后妃傳何准從祖兄前太尉參軍357納采同族何琦，不以侍郎准卒或后404死套本人。'))
    results[9017]['appearance_upper_bound']=339
    for ident,note in {9001:'尉緝三源同張重華寵臣與祚趙長結兄弟，354贊帝號；355明死趙長不等尉緝死，生卒未知。',9003:'SL張據都尉請逐寵臣與TJ常據同角色同整案，歸8817；不把張茂或其他張據合入。',9004:'實君=寔君，339代南部大人，376殺諸弟後被秦車裂；與371先卒的寔及斤父孤皆不同人。出生不詳，345遊戲估算與339領部矛盾交主合併器。',9005:'孤之子斤376構實君後秦刑死，勿同371刺昭成而為寔格傷的長孫斤，父族/案件不同。',9006:'杜能353原趙平原邑主降燕授平原守兩源同，不因冉閔死套本人，生卒未知。',9009:'榮胡單SL晉寧朔彭城魯郡降燕，原引無生卒；不以同年燕稱帝計本人出生。',9010:'石昭單SL石虎子淮南王與鑒苞琨密議殺閔，後文遵后孟準王鸞殺非昭明死，不補349或350卒。',9011:'謝勝SS引蘭亭集序前余姚令會稽人，不能詩罰酒三斗非年齡；此人物非謝萬謝勝子，無生卒。',9013:'郭敬秦金城步兵校尉353荊州刺史/354桓溫獲，非早年石勒郭敬僅憑同名；被俘不明處死，無生卒。',9015:'阮敷交州刺史353討林邑，破五十餘壘非50歲，無生卒。',9016:'頡獨鹿梁犢349起兵告戍者，原明斬梁犢並盡餘黨未具名本人死，不強填349，唯晚SL保未知。',9017:'何琦82卒無年，原414及332遊戲估算非原文確年；王導339卒前已辟參軍，本人先已有泾令經歷，登場上界339非初仕。',9018:'冉明閔子封彭城王，無本人卒，不套其父352斬或兄胤351死。',9019:'冉裕閔子武興王，與石裕/司馬裕不同，無本人卒年。',9020:'劉任陳留唐邑內史354被郭斁執降姚襄，俘降非被殺，單SL日期不進活動校準。',9021:'和昊祚將伐麗靬南山敗還兩key同，兵敗未云本人死，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    round45b_reads={9022:[('sl_04','以上将军啖鐡为秦州刺史'),('tj_17','以上将军啖铁为秦州刺史'),('tj_18','骁骑将军邓羌、秦州刺史啖铁讨平之')],9023:[('sl_03','归赵并州刺史张平'),('sl_04','平使养子张蚝御之'),('sl_06','秦大将军冀州牧张平击败之'),('tj_17','归赵并州刺史张平'),('tj_18','张平袭燕平阳，杀段刚、韩苞')],9024:[('js_15','司兵张芳率三千人击擢'),('sl_09','司兵张芳率三千人击擢')],9025:[('sl_04','特进光禄大夫强汪'),('tj_17','吕婆楼、强汪及略阳梁平老皆与之善'),('tj_18','特进光禄大夫强汪'),('sl_05','与特进梁平老强汪等帅壮士数百潜入云龙门')]}
    for ident,qs in round45b_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=ident!=9022,identity_status='all_source_key_subjects_individually_adjudicated_round45b' if ident!=9022 else 'qin_dantie_three_history_keys_clear_cbdb_original_gazetteer_not_retrieved',last_manual_review_batch='round45b')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第45批秦涼本人逐源核對','實讀本人官地、同役將領及歸降轉仕鏈，不以兵敗或政權被滅自動判本人死。'))
    results[9022].update(review_status='needs_further_research',component_identities=[dict(identity='前秦秦州刺史啖鐵，354授官359平高離',keys=[k for k in results[9022]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=None),dict(identity='CBDB395627啖鐵原直隸秦州新志section5190525尚未取得',keys=['cbdb:395627'],historical_birth=None,historical_death=None)])
    results[9022]['evidence'].append(dict(citation='原CBDB395627及Wikidata Q65857964身份限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),source_url='https://www.wikidata.org/wiki/Q65857964',excerpt='原BIOG_MAIN無生卒；source38333直隸秦州新志section_id5190525、68942 Q65857964。原WikiData項明person CBDB ID395627，無年月、無父母官職。',interpretation='已實取公開Q頁確為此CBDB編號，但只是回引同庫，不能作獨立身份官職橋；原地方志本人段未取，整人hold、不以三史一支代整條閉合。'))
    results[9023]['attested_activity_years']=[350,361]
    results[9023]['unresolved'].append('張平趙并州刺史→秦大將軍冀州牧、姚襄約兄弟、養子蚝、降燕後361襲平陽雁門五key同鏈。TJ361平遂為秦所滅可指所據勢力消滅，未明擒殺本人，不能直接補361卒；369遊戲死亡亦非確史。')
    results[9024]['unresolved'].append('前涼司兵張芳同牛霸354擊王擢兩源閉合，非西晉河間王將張方，不以同音硬合；無生卒。')
    results[9025]['unresolved'].append('強汪與梁平老呂婆樓交苻堅、357與苻法入雲龍門、授領軍四key閉，不同左光祿強平或太后弟強德，未明本人卒年。')
    round45c_reads={9026:[('sl_03','徐磋屯好畤'),('tj_17','徐磋屯好畤')],9027:[('js_11','慕容厉围梁父'),('sl_03','散骑常侍厉为下邳王'),('tj_17','散骑常侍厉为下邳王'),('tj_18','燕抚军将军下邳王厉寇兖州')],9028:[('sl_03','俊以弟宜为代郡城郎'),('tj_17','俊以弟宜为代郡城郎')],9029:[('sl_03','氐酋毛受屯高陵'),('tj_17','氐酋毛受屯高陵')],9030:[('js_15','秦州刺史牛霸、司兵张芳率三千人'),('sl_09','秦州刺史牛霸司兵张芳率三千人'),('tj_17','遣秦州刺史牛霸等帅兵三千击擢')],9031:[('sl_03','羗酋白犊屯黄白'),('tj_17','羌酋白犊屯黄白')],9032:[('sl_03','遣叔父安告丧于京师'),('sl_04','健叔父武都王安自晋还'),('tj_17','秦王健叔父武都王安自晋还')],9033:[('sl_02','妻董氏为皇后'),('sl_03','送冉闵妻董氏太子智'),('tj_17','立妻董氏为皇后')],9034:[('sl_03','代郡人赵榼帅三百余家叛燕'),('tj_17','代郡人赵榼帅三百馀家叛燕')],9036:[('sl_05','僧朗京兆人少事佛图澄')],9037:[('sl_03','遣殿中将军刁龛送钊母周氏归国'),('tj_17','遣殿中将军刁龛送钊母周氏归其国')],9038:[('sl_03','厍傉官伟帅部众自上党来降')],9039:[('gs_01','坚弟平阳公融及朝臣石越原绍等')],9041:[('sl_03','略阳姜伯周[伯周健之舅也]为尚书令'),('tj_17','略阳姜伯周为尚书令')],9042:[('sl_03','初母羌氏梦感大罴而孕')],9043:[('sl_03','晋兰陵太守孙黑济北太守高柱'),('tj_17','兰陵太守孙黑、济北太守高柱')],9044:[('sl_04','蒲坂津监宼登于河中得大屐一只')],9045:[('sl_03','土豪封放[奕之从弟也]'),('tj_17','土豪封放，弈之从弟也'),('sl_06','祖悛父放仕俊为渤海太守'),('sl_08','祖悛振威将军父放吏部尚书')],9046:[('tj_17','库傉官伟帅部众自上党降燕')],9047:[('js_15','瓘遣司马张姚、王国将二千人伐基'),('sl_09','瓘遣司马张姚王国将兵二千人击基'),('tj_17','瓘遣司马张姚、王国击斩之')],9048:[('js_15','遣其将易揣、张玲率步骑万三千以袭之'),('sl_09','遣别将易揣张玲帅歩骑万三千以袭之'),('tj_17','遣其将易揣、张玲帅步骑万三千以袭瓘')],9049:[('sl_02','降胡栗特康等执大单于胤及左仆射刘琦'),('tj_17','降胡栗特康等执大单于胤及左仆射刘琦以降赵')],9050:[('js_15','遣其将易揣、张玲率步骑万三千以袭之'),('sl_09','遣别将易揣张玲帅歩骑万三千以袭之'),('tj_17','遣其将易揣、张玲帅步骑万三千以袭瓘')]}
    for ident,qs in round45c_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_source_key_subjects_individually_adjudicated_round45c',last_manual_review_batch='round45c')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第45批前秦前燕前涼逐人實讀','各key本人親屬/官地/同役上下文已讀；未載生卒，不借鄰人死亡或傳記所述後世年月填補。'))
    results[9036]['evidence'].append(chunk_ev('gs_01','竺僧朗。京兆人也','高僧傳卷五竺僧朗完整本人段','實讀京兆、秦皇始元移泰山、張忠林下契、金輿谷昆侖精舍、苻堅請、姚興慕容德孝武拓跋圭禮、卒山中85；與SL僧朗唯一泰山張忠同鏈，不同年代後世僧朗。無確卒年，不從85反鎖出生。'))
    results[9036]['source_reported_age_at_death']={'age':85,'calendar_year':None,'source':'高僧傳卷五竺僧朗'}
    results[9046].update(review_status='duplicate',duplicate_of=9038,identity_status='ku_kunuguanwei_shangdang_group_chieftain_same_351_submission')
    results[9046]['evidence'].append(chunk_ev('sl_03','厍傉官伟帅部众自上党来降','SL厙/TJ庫姓字同一351上黨部眾降燕','兩單key本人事件/部眾/地點相同，姓形厙厍庫異，不因同卷另有390太尉就新增混人；後期同人仍需其來源各核。'))
    results[9027]['evidence'].append(chunk_ev('tj_18','厉兵大败，单马奔还','資治通鑑太和四369桓溫北伐黃墟','征討大都督下邳王厲兵敗單馬返，是本人活著敗還，不是當役死；不可用溫以後撤兵或燕370國滅推確卒。'))
    results[9027]['last_attested_activity']=369
    results[9045]['evidence'].append(chunk_ev('tj_18','孚，放之子也','資治通鑑太和四369封孚父放橋','封孚隨垂軍、注明放子，SL封懿/封孚譜同父放渤海守吏部尚書，與351渤海授官鏈閉；369追述父子不表示放369仍活。'))
    for ident,person,note in [(9035,178653,'原CBDB178653辛某birth0/death415；notes Death year estimated by Tackett (cc415)；index352；父178652子178654；來源32033/71853頁139851。'),(9040,182351,'原CBDB182351唐契birth0/death416；notes Death year estimated by Tackett (cc416)；index353；父182350子182352/182508；來源32033/71853頁143605。')]:
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='original_tackett_subject_source_not_retrieved',last_manual_review_batch='round45c_cbdb')
        results[ident]['evidence'].append(dict(citation='原CBDB本人日期限定及親屬/來源',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=note,interpretation='實際原庫已讀、卒年明為估算，指數年非出生或活動；未取得本人原引不算完整史料核明，不強填生卒。'))
    for ident,note in {9026:'徐磋好畤氐酋兩源同，350遣子降秦，斬杜洪使不是本人死；無生卒。',9027:'慕容厲下邳王四源同封爵/366梁父與369黃墟鏈，無本人卒，失敗單馬還與戰死不同。',9028:'慕容宜俊弟350代郡城郎、354散騎常侍廬江王兩key同人，城郎古怪原字保留不擅改，無生卒。',9029:'毛受高陵氐酋，與同組好畤徐磋黃白白犢是三人，不同毛難/毛貴；無生卒。',9030:'牛霸張祚平東秦州守與張芳擊王擢三key閉，後討李儼眾潰單騎還不是死；無生卒。',9031:'白犢羌酋黃白350遣子降秦，非359高離叛將，無生卒。',9032:'苻安健叔武都王350告晉喪354還被襄虜逃返/蒲坂大司馬三key同人；其後健死或生選司空擇殺不是安明卒，無生卒。',9033:'董氏冉閔妻350后/352邺破送薊奉璽君三key同，閔被斬非后被斬；奉璽說是燕政治虛構不據以聲稱傳國璽真在本人，無生卒。',9034:'趙榼350代郡300餘家降張平兩key同，不同趙瞻/趙岐，無生卒。',9036:'僧朗=高僧傳竺僧朗泰山朗公85卒無年，SL說被請既至與GS辭老疾乃止分歧保留，兩書均不定本人卒年，勿據時代估415。',9037:'刁龕燕殿中將軍355護送高句麗王釗母周氏兩key同，非段龕；送返不是卒，無生卒。',9038:'厙傉官偉與9046庫傉官偉同351上黨部落降燕，姓異字同事；canonical9038，無生卒。',9039:'原紹GS道安傳前秦朝臣諫南伐，與石越分列兩人，敘文融淝水死不是紹死；無生卒。',9041:'姜伯周略陽健舅351尚書令兩key閉，與苻生母強太后弟強平不同人，不以姜強音近合；無生卒。',9042:'姜氏單SL苻健母首句作羌氏夢罴、後尊母姜氏同人字異；尊號不指明母當時活，無生卒。',9043:'孫黑晉蘭陵太守355與高柱高瓮降燕兩key閉，不同陽平邑主孫元；無生卒。',9044:'寇登蒲坂津監得大屐七尺三寸為物長非身高年齡，張靖被赦非登生卒，唯晚SL無生卒。',9045:'封放351封奕從弟渤海守與後燕封懿父/南燕封孚父吏部尚書四key閉；孚407卒71非放，後世追記父官不證仍活，無生卒。',9047:'張姚張瓘司馬與王國擊酒泉馬基三key同，斬基田旋非姚死；無生卒。',9048:'張玲祚將與易揣襲瓘敗逃三key閉；原文死者王鸞/索孚非玲，不作355卒。',9049:'栗特康執胤劉琦降趙兩key同，祗殺之承前胤琦俘虜，不足證康也被殺，原bio主語歧義已辨，未知生卒。',9050:'易揣與玲渡河敗逃後引兵殺趙長三key同，未云本人死；與祚被殺不可混，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    round46a_reads={9051:[('sl_05','杜慈仕生为尚书郎')],9052:[('sz_00','太守杜')],9053:[('js_11','石虔小字镇恶'),('js_13','桓石虔复有沔阳之功'),('js_18','温遣桓伊及弟子石虔等逆击'),('tj_18','南顿太守桓石虔克其南城'),('ssxy_02','桓石防司空豁之长庶也')],9054:[('js_07','宁康初，杨安寇蜀'),('sl_03','九月杨安攻晋阳'),('sl_04','以前将军杨安镇军将军毛盛为前锋都督'),('sl_05','杨安者名将也'),('sl_09','秦遣王猛及杨安王抚邵羌等'),('tj_17','国子安奔秦'),('tj_18','秦王坚复遣王猛督镇南将军杨安等十将'),('wsh_02','坚使其右将军杨安攻克汉中'),('wsh_04','国子安叛苻生，杀俊'),('yi_01','国子安为振威将军、武都太守')],9055:[('sl_09','乃使杨秋胡将灵耀于东苑'),('tj_17','祚遣杨秋胡将曜灵于东苑'),('js_15','祚寻使杨秋胡害耀灵于东苑')],9056:[('sl_03','袭俊将荣国于郎山败之')],9057:[('js_13','徽之字子猷'),('ssxy_01','王徽之字子猷'),('ssxy_02','王子猷说世目士少为朗'),('ssxy_03','王子猷居山隂夜大雪')],9058:[('sl_03','五月秦河内太守王会黎阳太守韩高'),('sl_04','夏四月河内太守王会黎阳太守韩高'),('tj_17','秦河内太守王会、黎阳太守韩高皆以郡降燕'),('js_00','会字士和，侍御史')],9059:[('sl_03','俊给事黄门侍郎申胤上言'),('tj_18','垂表司徒左长史申胤')],9060:[('yl_03','苻坚时有申香，长十尺以上')],9061:[('js_18','时博士卢壸对曰')],9062:[('tj_17','吐谷浑叶延卒，子碎妥立')],9063:[('js_18','窦滔妻苏氏，始平人也')],9064:[('sl_09','索苞炖煌人有文武材艺')],9065:[('sl_03','方为高阳公'),('tj_17','方为高阳公')],9066:[('sl_03','桐为汝南公'),('tj_17','桐为汝南公')],9067:[('sl_03','硕为北平公'),('sl_04','平昌王菁北平王硕帅众五万拒温'),('sl_09','秦苻健遣卫将军苻硕御之'),('tj_17','硕为北平公'),('js_15','健遣苻硕御之，战于龙黎')],9068:[('sl_03','觌为长乐公'),('tj_17','觌为长乐公')],9069:[('sl_03','靓为平原公'),('tj_17','靓为平原公')],9070:[('sl_05','赵焕父琨为秦尚书')],9071:[('sz_00','父珍，扶柳县令')],9072:[('sl_04','太子舍人赵诲为中护军'),('tj_17','赵韶、中护军赵诲，皆洛州刺史俱之从弟也'),('sl_05','俱从弟右仆射韶中护军诲有宠于生')],9074:[('js_12','纳字祖言'),('js_14','尚书陆纳侍讲')],9075:[('js_17','坚遣黄门郎韦华持节策吊')],9444:[('yi_01','苻坚遣杨安、苻雅等讨纂克之')]}
    for ident,qs in round46a_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=ident not in [9054,9058,9063],identity_status='all_source_key_subjects_individually_adjudicated_round46a',last_manual_review_batch='round46a')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第46批逐源本人原文核讀','實讀本人親屬、官地、上下文所指；未知生卒保留不詳，不以年齡、身高、兵數或鄰人死填本人年月。'))
    hdie=chunk_ev('js_11','太元十三年卒，追赠右将军','晉書桓豁附桓石虔完整本傳','本人小字鎮惡、豁長子、溫弟子、南頓至豫州刺史，太元十三388卒無月日年齡；後文子振405死非父。')
    results[9053].update(review_status='primary_text_corroborated',historical_death=date(388,hdie))
    results[9053]['evidence'].append(hdie)
    results[9053]['source_reported_campaign_variants']=[{'source':'晉書','statement':'從溫入關救沖'},{'source':'世說新語','statement':'從征枋頭救沖'}]
    results[9057]['evidence'].append(dict(citation='原CBDB25794王徽之父系及日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='BIOG_MAIN姓名王徽之；birth/death0；index351 type11 source25793；KIN父25793王羲之。',interpretation='父羲之同JS本傳及SS中興書本人第五子、字子猷唯一父名官職鏈；351由父系生成指數年非確生，生卒均未知。'))
    results[9057]['relative_death_sequence']={'source':'晉書王羲之附徽之','statement':'弟獻之卒，徽之奔喪彈琴，人琴俱亡；月餘亦卒','calendar_year':None}
    results[9054].update(review_status='needs_further_research',identity_status='qin_general_and_qiuchi_guo_son_link_unresolved_with_genealogical_source_conflict',component_identities=[dict(identity='前秦軍將楊安，王猛晉陽枹罕、蜀成都襄陽役',keys=[k for k in results[9054]['keys'] if k not in ['tj:tj_17:399:楊安','wsh:wsh_04:76:楊安','yi:yi_01:19:楊安']],historical_birth=None,historical_death=None),dict(identity='仇池楊國子安，355/356父被俊殺出奔秦；與前秦軍將是否同人尚待獨立橋',keys=['tj:tj_17:399:楊安','wsh:wsh_04:76:楊安','yi:yi_01:19:楊安'],historical_birth=None,historical_death=None)])
    results[9054]['source_reported_genealogy_variants']=[{'source':'魏書氐傳','excerpt':'國子安叛苻生，殺俊，復稱籓於晉。安死，子世自立為仇池公。'},{'source':'宋書氐胡傳','excerpt':'國從父楊俊復殺國自立，安奔苻生…四年，俊卒，子世立。'}]
    results[9054]['component_identities'][0]['confirmed_identity_match']=9444
    results[9444]['confirmed_component_match']={'record_id':9054,'identity':'前秦軍將分支，371同苻雅攻仇池纂；9054整條未閉不整條刪除'}
    results[9058].update(review_status='needs_further_research',identity_status='qin_henei_governor_and_jin_wanglan_son_shihe_unbridged',component_identities=[dict(identity='前秦河內太守王會，355與韓高降燕',keys=[k for k in results[9058]['keys'] if not k.startswith('js:')],historical_birth=None,historical_death=None),dict(identity='王覽子會，字士和侍御史',keys=['js:js_00:182:王會'],historical_birth=None,historical_death=None)])
    results[9063].update(review_status='needs_further_research',identity_status='dou_tao_suhui_husband_js_clear_cbdb_original_gazetteer_not_retrieved')
    results[9063]['evidence'].append(dict(citation='原CBDB507548竇滔來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='BIOG_MAIN無生卒無親屬；source38333直隸秦州新志section5190525 self_bio1。',interpretation='與9022同地方志段但該本人原書未取，不能僅據同名接JS蘇蕙夫秦州守，暫hold全部key身份。'))
    ndie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','二月，甲寅，尚书令陆纳卒','資治通鑑太元二十395二月甲寅','完整年首乙未395及本人尚書令死句，與JS玩子祖言主傳從吴興守升尚書令同身份；JS前文玩64卒與何充54卒都不是納年齡，不據以推生。')
    results[9074].update(review_status='primary_text_corroborated',historical_death=date(395,ndie,month=2,day_ganzhi='甲寅'))
    results[9074]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9074]['evidence'].append(ndie)
    for ident,note in {9051:'杜慈夢警逃苻生宴斬獨獲免，餘皆死不是慈死，唯晚SL無生卒。',9052:'杜霸原本名後一字私用缺字，題杜霸由抽取暫錄；智賢尼傳太守杜某逼賢斫傷之人身份明，不能從傷二十餘瘡推出歲數或本人死。',9053:'桓石虔豁長子鎮惡、溫弟子五key閉；JS入關救沖與SS枋頭記有役別異文保留，不拿SS十七八直接換CE；確388卒，生不詳。',9054:'十key所指角色已讀，但前秦將軍與國子安奔秦能否轉仕同人未有明父系橋；WS說安殺俊死後子世立，宋書說俊死子世立為真正父系源異。不能用抽取JSON附註不同人當史證，也不直接當已同人；頂層BD空，保分支、9444軍將單支閉合。',9055:'楊秋胡三源同東苑奉祚命殺耀靈，耀靈卒非兇手卒，無本人生卒。',9056:'榮國俊將郎山為段龕敗，不同晉降將榮胡，只說敗未說殺，未知生卒。',9057:'王徽之子猷羲之子五key閉；史述弟獻之卒月餘自卒但未具二人CE年，勿套後人常見386或338。原CBDB0日期留空351指數非生。',9058:'王會秦河內太守三key與西晉王覽子士和侍御史JS一key未有生平橋，不能憑同名合成確生卒；王薈8340是導子別人，亦不可改字硬歸。',9059:'申胤給事黃門至司徒長史，申鍾子，355論太子服與369370軍議同兩key；預言燕復建非本人活至復國，生卒未知。',9060:'申香拂蓋郎長十尺以上為身高非10歲，秦書引文無本人生卒。',9061:'盧壸博士薦韋逞母宋氏，原八十指宋母不是壸年齡，無生卒。',9062:'碎妥葉延子351繼立，原卒主葉延不是碎妥，無生卒。',9063:'竇滔蘇蕙夫前秦秦州守徙流沙明身份，CBDB原志仍待橋，回文840字不是年數，不估生卒。',9064:'索苞宋澄被羌圍時救援有5000騎，擊殺30餘是戰績非歲，唯SL無生卒。',9065:'苻方健子高陽公351封，與後來軍將苻方若接仍需原key核，無生卒。',9066:'苻桐健子汝南公351封，原無生卒。',9067:'苻碩健子北平公及354拒溫/龍黎御擢同前秦碩五key已讀；龍黎將領/年份他書記雄菁苻願不同，保來源敘事差異，不從擢敗套硕死或硬改苻願。未知生卒。',9068:'苻覿健子長樂公351封，非子堅子丕長樂公，未知生卒。',9069:'苻靚健子平原公351封，非前涼張玄靚，未知生卒。',9070:'趙煥父琨皇始元為司馬勳殺而尋屍，父卒非子煥卒，唯SL無本人生卒。',9071:'趙珍智賢尼父常山扶柳令，女太和70餘非父年齡，無生卒。',9072:'趙誨苻生太子舍人中護軍司隸/趙俱從弟三key閉，俱憂死非誨死；堅殺生倖臣20餘未具名誨，不強補357卒。',9074:'陸納祖言玩子尚書令二key閉，TJ395二月甲寅明卒；父玩64非本人卒齡，不推生，弟子道隆元熙任官非納在世。',9075:'韋華秦黃門郎策吊張忠，張忠華山道死非華死，未知生卒。',9444:'宋書371秦將楊安同苻雅攻纂，與9054軍將支同一役；9054國子安父系源異未解，不整條→本記錄。未知生卒。'}.items():results[ident]['unresolved'].append(note)
    round46b_reads={9076:[('js_18','韦逞母宋氏，不知何郡人也'),('sl_05','韦逞母宋氏不知何郡人也')],9077:[('sl_03','黎阳太守韩高以郡来降'),('sl_04','黎阳太守韩高以郡叛降于燕'),('tj_17','黎阳太守韩高皆以郡降燕')],9078:[('sl_03','济北太守高柱建兴太守高瓮'),('tj_17','济北太守高柱、建兴太守高瓮')],9079:[('sl_03','济北太守高柱建兴太守高瓮'),('tj_17','济北太守高柱、建兴太守高瓮')],9080:[('wsh_02','闵太子叡固守邺城')],9081:[('sl_02','闵子操奔鲁口'),('sl_03','送冉操诣燕军'),('tj_17','送冉操诣燕军')],9082:[('js_18','又遣江夏相刘岵、义阳太守胡骥'),('sl_03','大司马桓温遣江夏相刘岵来援')],9083:[('sl_03','高平太守刘庄将三千人守琅邪'),('tj_17','高平太守刘庄将三千人守琅邪'),('js_12','高平太守刘庄等三千人守琅邪')],9084:[('sl_09','右将军宋熈请取天龟观壊以为宅')],9085:[('sl_04','幽州刺史强[晋书作张]哲于裴氏堡'),('tj_17','攻秦幽州刺史强哲于裴氏堡')],9086:[('tj_17','右仆射董荣、侍中强国皆以佞幸进'),('sl_05','右仆射董荣侍中强国等皆以佞幸进')],9087:[('sl_04','建武将军徐盛')],9088:[('sl_03','留镇南将军尘镇广固'),('tj_17','恪留慕容尘镇广固'),('tj_18','燕青州刺史慕容尘遣司马悦明救泰山')],9089:[('sl_04','慕舆长卿等帅众七千人自轵闗'),('tj_17','燕将军慕舆长卿入轵关')],9090:[('sl_03','叅军谯国戴逯等将二千人守太山'),('js_12','参军戴逯、萧鎋二千人守泰山'),('ssxy_02','逯字安丘谯国人祖硕父绥')],9091:[('tj_17','参军谯国戴遂等将二千人守泰山')],9092:[('sl_04','羗敛岐叛坚自称益州刺史'),('sl_06','南安敛岐为右部帅'),('sl_09','羌敛岐自称益州刺史'),('tj_18','敛岐部落先属姚弋仲')],9093:[('sl_04','王猛朱肜之伦相望于岩谷'),('sl_05','朱肜京兆人隠居不仕'),('tj_18','处士京兆朱彤自代'),('wsh_00','及朱彤、张蚝、邓羌等诸道来寇'),('wsh_02','苻雅率将王统、朱彤、杨安、姚苌')],9094:[('sl_04','黄门侍郎李柔'),('wsh_00','坚将李柔、张蚝勒兵内逼'),('js_15','高祖雍，曾祖柔，仕晋并历位郡守'),('sl_10','髙祖雍曾祖柔仕晋并歴显位'),('wsh_04','曾祖柔，晋相国从事中郎、北地太守')],9095:[('sl_04','著作郎梁谠'),('sl_05','梁谠字伯言略阳氐人也')],9096:[('js_07','梁州刺史杨亮失守'),('sl_06','先是宏农杨亮归襄'),('tj_17','弘农杨亮自襄所来奔'),('tj_18','逐梁州刺史杨亮'),('wsh_02','纂请救于梁州刺史杨亮')],9097:[('js_11','获襄将张骏、杨凝等'),('sl_06','部将张骏杨凝等皆为温所获')],9098:[('tj_17','段勤与弟思陪举城降')],9099:[('sl_03','遂执其弟钦斩右长史袁范'),('tj_17','执其弟钦，斩右长史袁范')],9100:[('tj_17','段龛遣其属段蕰来求救')]}
    for ident,qs in round46b_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=ident not in [9094,9096],identity_status='all_source_key_subjects_individually_adjudicated_round46b',last_manual_review_batch='round46b')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第46批逐鍵本人及上下文實讀','已讀明本人官地親屬、同役情節；未知生卒不從同國覆亡、他人死亡或追述反推。'))
    results[9076]['evidence'].append(dict(citation='原CBDB135634韋逞母系及來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth/death0；KIN135632 code111 source9601 MQWW PoetID3291；原來源注傳主為宋氏(韋逞母).MQWW PoetID3291。',interpretation='本人原庫母系及來源明以宋氏韋逞母唯一母子，與JS/SL秦太常本人閉合；原庫無生卒，母80不是逞80。'))
    results[9080]['candidate_duplicate_of']=9816
    results[9080]['identity_status']='wei_book_ranrui_same_role_candidate_ranzhi9816_target_keys_pending'
    results[9091].update(review_status='duplicate',duplicate_of=9090,identity_status='daisui_dailu_same_xunxian_qiaoguo_two_thousand_taishan_detachment')
    results[9091]['evidence'].append(chunk_ev('js_12','参军戴逯、萧鎋二千人守泰山','晉書戴逯與通鑑戴遂原名異文同事','兩書同荀羡退下邳，先諸葛攸劉莊三千守琅邪，再譙國參軍二千守泰山；不是只憑形近。SS戴氏譜同名逯譙國軍功、戴逵兄屬兼容生平，不因另載大司農則自造同名異人。'))
    results[9094].update(review_status='identity_conflict',identity_status='qin_lirou_and_jin_beidi_governor_lihao_greatgrandfather_mixed',component_identities=[dict(identity='前秦黃門侍郎李柔、376隨張蚝入代',keys=[k for k in results[9094]['keys'] if k in ['sl:sl_04:79:李柔','wsh:wsh_00:129:李柔']],historical_birth=None,historical_death=None),dict(identity='西晉北地太守李柔，雍子弇父李暠曾祖',keys=[k for k in results[9094]['keys'] if k not in ['sl:sl_04:79:李柔','wsh:wsh_00:129:李柔']],historical_birth=None,historical_death=None)])
    results[9094]['component_identities'][1]['source_reported_genealogy_variants']=[{'source':'晉書/SL','statement':'柔子弇，弇仕張軌，暠祖'},{'source':'魏書','statement':'柔子太，太仕張祚，暠祖'}]
    results[9096].update(review_status='needs_further_research',identity_status='yaoxiang_guest_hongnong_yangliang_to_jin_liangzhou_governor_link_not_located',component_identities=[dict(identity='弘農楊亮，356自姚襄奔桓溫評襄',keys=[k for k in results[9096]['keys'] if k.startswith('sl:') or k.startswith('tj:tj_17:')],historical_birth=None,historical_death=None),dict(identity='晉梁州刺史楊亮，370李高逐、後援仇池敗',keys=[k for k in results[9096]['keys'] if not(k.startswith('sl:') or k.startswith('tj:tj_17:'))],historical_birth=None,historical_death=None)])
    for ident,note in {9076:'韋逞宋母子秦太常三key閉，母80/120生員非本人年齡，無本人生卒。',9077:'韓高秦黎陽守355同王會降燕三key閉，無JS王覽子混入本人；未知生卒。',9078:'高柱晉濟北太守355降燕兩key閉，不同高瓮，無生卒。',9079:'高甕=高瓮建興守355降燕兩key閉，不同高柱，無生卒。',9080:'冉叡WS明闵太子固守邺，同9816冉智為同事名字異文候選，待目標全部keys橋再硬合；無生卒。',9081:'冉操闵子先奔魯口、後王午送燕三key閉，不同太子冉智/胤，不以父死推子死；無生卒。',9082:'劉岵桓温江夏相討李弘/364援陳郡兩key閉，傳首是賊李弘不是岵死，未知生卒。',9083:'劉莊荀羡高平守356琅邪三千軍三key同，不同漢明帝或慕容莊；荀羡358卒38不套本人。',9084:'宋熙右將軍請毀天龜觀為宅不同宋混兄修/弟澄，唯SL無生卒。',9085:'強哲秦幽州裴氏堡二key閉，SL注晉書張哲為源姓異文；獲長卿非哲死，未知生卒。',9086:'強國苻生侍中與董榮構王堕兩key同，不同強平/強德/強汪，王堕刑死非國死；無生卒。',9087:'徐盛秦建武將軍閻負語，非吳徐盛文向隔百年，單SL不作已證活動年/生卒。',9088:'慕容塵廣固青州/鎮南將軍三key同，軍敗不明死，荀羡358病不是本人病死，未知生卒。',9089:'慕輿長卿燕將攻裴氏堡為邓羌擒，兩源兵數甲首有異保原文，被俘不等斬，未知生卒。',9090:'戴逯荀羡參軍譙國守太山、SS戴氏譜戴逵兄字安丘譙國軍功顯、封廣陵侯至大司農三key同，兵2000非歲；無生卒。',9091:'戴遂TJ譙國荀羡參軍二千守泰山與JS戴逯同完整部署→9090，不單字近合。',9092:'斂岐姚襄右部帥、部眾舊姚弋仲與367叛秦李儼白馬被擒四key閉；被擒送不明被斬，無生卒。',9093:'朱肜/彤京兆隱士猛薦及秘書監將領五key同，無本人確生卒；SL前文趙整60餘卒不是肜年齡，不偷算。',9094:'李柔真混兩角色：秦侍郎/入代將與晉北地守李暠曾祖，後者祖弇已仕張軌祖系非秦378同人。李弇/太及仕軌/祚源異保，不套後裔暠417死；兩支BD未知。',9095:'梁讜伯言氐文士、弟熙、秦著作中書侍中二key閉；前文梁平老372卒不是讜卒，僅SL無確生卒。',9096:'原文五key已讀，356弘農姚襄客歸桓溫與梁州守楊亮角色可以同人但尚未定位明轉仕/族系橋，不先判真混亦不全閉；兩支無生卒。',9097:'楊凝姚襄將356伊水被温獲徙尋陽兩key同，363張駿叛被斬非凝，無生卒。',9098:'段思陪段勤弟352絳幕同降，原思陪全字保留；359另段思奔晉未核同人，無生卒。',9099:'段欽段龕弟356淄水被俘兩key閉，被斬袁范/被傷死辟閭蔚非欽，無生卒。',9100:'段薀/蕰段龕屬求晋援，荀羡不進/王騰被斬非薀死，無生卒。'}.items():results[ident]['unresolved'].append(note)
    round47a_reads={9101:[('sl_03','乃遣其属段蕴诣晋请救')],9102:[('sl_04','秘书监王扬')],9103:[('sl_04','安逺将军范俱难')],9104:[('js_18','义阳太守胡骥讨妖贼李弘')],9107:[('tj_16','以代人燕凤为长史，许谦为郎中令'),('tj_18','郎中令许谦盗绢二匹'),('wsh_00','代人许谦盗绢二匹'),('wsh_01','许谦，字元逊，代人也')],9108:[('sl_06','温司马许逊曰不得见襄而有大功')],9109:[('js_11','太山太守诸葛攸奔邹山'),('sl_03','留太山太守诸葛攸高平太守刘庄'),('tj_17','留将军诸葛攸、高平太守刘庄'),('tj_18','泰山太守诸葛攸攻燕东郡'),('js_12','留将军诸葛攸、高平太守刘庄')],9110:[('js_18','诏又使尚书车灌止之'),('tj_17','诏遣兼司空、散骑常侍车灌'),('tj_18','诏尚书车灌止之'),('js_01','于是御史中丞车灌奏劾')],9111:[('sl_03','洛州刺史邓羌帅骑二万来援'),('sl_04','建节将军邓羗'),('sl_05','邓羗安定人也'),('sl_06','建节将军邓羌帅歩骑万五千'),('tj_17','建节将军邓羌拒长卿'),('tj_18','建节将军邓羌将步骑万五千'),('wsh_00','及朱彤、张蚝、邓羌等诸道来寇'),('wsh_02','坚遣其建节将军邓羌讨擒之'),('sl_08','坚遣其将邓羌讨擒之')],9112:[('tj_17','督护陈午、河南太守戴施以二千人戍洛阳')],9113:[('sl_03','鞠殷乐浪太守彭之子也'),('tj_17','以尚书左丞鞠殷为东莱太守')],9114:[('js_12','顾悦之，字君叔'),('js_17','父悦之，尚书左丞'),('ssxy_00','顾悦与简文同年而发蚤白')],9115:[('sl_03','逸子璋随从常数十骑'),('tj_18','子璋从数十骑')],9116:[('sl_04','仇腾为尚书领选'),('sl_05','尚书仇腾丞相长史席寳数譛毁之')],9117:[('js_18','慕容垂、傅末波等率众八万距温'),('js_10','后与贼帅傅末波等战失利'),('sl_03','长乐太守傅颜讨斩之'),('tj_18','长乐太守傅颜帅步骑五万与攸战于东阿')],9118:[('sl_04','西域胡刘康诈称刘曜子'),('tj_17','西域胡刘康诈称刘曜子')],9119:[('sl_03','平州刺史刘特率户五千来降')],9120:[('js_01','敬王恬，字元愉')],9121:[('sl_04','呼延毒起灞城'),('tj_17','呼延毒起灞城')],9122:[('sl_04','曜武将军姚益生左将军王钦卢'),('tj_18','曜武将军姚益生、左将军王钦卢')],9123:[('sl_04','遣辅国将军姚兰略地敷城'),('sl_06','遣从兄辅国将军兰略地鄜'),('tj_18','遣辅国将军姚兰略地敷城')],9124:[('js_06','统字承公。幼与绰及从弟盛过江'),('ssxy_02','孙统字承公太原人善属文'),('ssxy_03','孙统为柔集叙曰')],9125:[('sl_04','席宝为丞相长史行太子詹事'),('sl_05','尚书仇腾丞相长史席寳数譛毁之')]}
    for ident,qs in round47a_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=True,identity_status='all_source_key_subjects_individually_adjudicated_round47a',last_manual_review_batch='round47a')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第47批本人逐鍵原文覆核','實讀所引本人親屬、官地及同役段落，僅角色一致者閉合；未載生卒不從旁人死、物長或兵數補齊。'))
    results[9101].update(review_status='duplicate',duplicate_of=9100,identity_status='duanyun_duanyun_same_duankan_envoy_to_xunxian_relief')
    results[9101]['evidence'].append(chunk_ev('tj_17','段龛遣其属段蕰来求救','段蘊/薀/蕰同段龕求晉援使者','同廣固受圍、遣晉、荀羡隨往而不進，單key雙方全閉，不與段勤弟思陪同合。'))
    results[9122].update(review_status='duplicate',duplicate_of=8857,identity_status='yaoyisheng_yaoyi_same_yaoxiang_brother_yaowu_wangqinlu_recruitment')
    results[9122]['evidence'].append(chunk_ev('sl_06','遣兄曜武将军益及左将军王钦卢','姚益/益生同襄兄曜武將軍','兩書357杏城/敷城/王欽盧招羌全組相同，並非只憑姓名近；8857原三key已全核。'))
    qdie=chunk_ev('wsh_01','皇始元年卒官，时年六十三','魏書許謙完整本傳','元遜代人郎中令、從昭成至太祖、396平舒侯陽曲護軍卒63，死亡明年；虛歲可推334但與TJ338即位初拜官叙次抵牾，出生只保來源主張不進頂層確年。')
    results[9107].update(review_status='primary_text_corroborated',historical_death=date(396,qdie))
    results[9107]['evidence'].append(qdie)
    results[9107]['source_reported_chronologies']=[{'source':'魏書許謙本傳','birth_year_from_nominal_age':334,'death_year':396,'age_at_death':63,'calculation':'396-63+1=334'},{'source':'資治通鑑338什翼犍即位初置百官','first_appointment_year_as_narrated':338,'conflict':'如按出生334則四歲任郎中令；可能匯敘建國期，不能兩者同作無爭議精確年代。'}]
    results[9107]['unadjudicated_activity_candidate']=[338]
    results[9107]['attested_activity_years']=[365]
    results[9107].pop('appearance_upper_bound',None)
    sameage=chunk_ev('js_12','与简文同年，而发早白','晉書殷浩附顧悅之本人同歲記錄','本人君叔、子愷之（凱之）與SS顧氏父傳及中興書尚書丞同；同年可連簡文咸和元326七歲推320，不是假設十八歲登場。')
    emperor=evidence(SOURCE/'_cache/texts/晋书.txt','咸和元年，所生郑夫人薨。帝时年七岁','晉書簡文帝紀獨立同歲定年錨','紀主簡文昱、咸和元326年7虛歲→326-7+1=320；本紀372卒53亦同算320。顧悅之與帝同年故同一推年，非確生日。')
    results[9114].update(review_status='primary_text_corroborated',historical_birth=date(320,sameage))
    results[9114]['historical_birth'].update(precision='inferred_year_from_same_birth_year_and_reign_year_sui_age',calculation='326 - 7 + 1 = 320; 顧悅之與簡文帝同年',source='晉書殷浩附顧悅之同年；晉書簡文帝紀咸和元七歲',caveat='同歲與虛歲推算，非確生日')
    results[9114]['evidence'].extend([sameage,emperor])
    results[9114]['source_reported_office_variants']=[{'source':'晉書殷浩附傳','statement':'尚書右丞'},{'source':'晉書顧愷之傳/SS引中興書','statement':'尚書左丞'}]
    tdie=chunk_ev('js_01','太元十五年薨，追赠车骑将军','晉書譙敬王司馬恬本傳','本人無忌子元愉、海西廢時彈温、鎮北，太元十五390明卒無月日齡。父無忌永和六350薨/子尚之402死不套本人。')
    results[9120].update(review_status='primary_text_corroborated',historical_death=date(390,tdie))
    results[9120]['evidence'].append(tdie)
    for ident,person,statement in [(9105,170922,'原裴某170922 birth0/death417；notes Death year estimated by Tackett (cc417)，index354type29；父170921裴大醜、子170923裴慶孫；來源32033/71853頁131786。'),(9106,170908,'原裴良170908 birth0/death417；notes Death year estimated by Tackett (cc417)，index354type29；父170907裴保歡、子170909裴子通及170914裴叔祉；來源32033/71853頁131772。')]:
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='original_tackett_biography_or_inscription_not_retrieved',last_manual_review_batch='round47a_cbdb')
        results[ident]['evidence'].append(dict(citation='原CBDB日期限定與父子鏈實讀',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=statement,interpretation='417明為Tackett估卒，354指數非生年/活動年；本地魏書未定位該父子原句，不以泛裴氏同名替代，史實BD空仍待原引。'))
    for ident,note in {9101:'段蘊SL與9100段薀/蕰TJ段龕派晉求荀羡援同事，整條→9100，生卒未知。',9102:'王揚秦秘書監閻負稱文宗，非王猛/王魚；唯SL未有獨立確年，無生卒。',9103:'范俱難秦安遠將軍閻負語非萬人為年齡，只有晚SL源生卒未知，不借後世范具難異文未核條目去重。',9104:'胡驥晉義陽太守同劉岵討妖李弘，傳首京都主李弘非驥死，生卒未知。',9107:'許謙元遜郎中令四key同人；WS396卒63虛歲推334與TJ338初拜官真年代衝突，出生未知頂層只留396卒。338暫移未裁定候選不推遊戲生年/登場，365在任無爭議。',9108:'許遜單SL桓温司馬356預言襄走，不能未橋便同道教許遜239–374或root12162；本人生卒未知。',9109:'諸葛攸五key太山太守、荀羡軍、後燕攻東郡梁父同鏈，數次敗走未明死；源邹山/淮南逃地別不套本人死。',9110:'車灌成帝時御史中丞彈司馬纮、356散騎兼司空修陵、364尚書止桓温四key相容晉同一顯官鏈；纮342死非灌死，無生卒。',9111:'邓羌羗安定秦建節洛州/并州將九key同，平長卿/姚襄/衛辰及潞川軍鏈，SL後段鄧翼稱先君只給晚重編卒前界無確卒，未知生卒，不套姚襄357或昭成376死。',9112:'陳午桓温督護356與毛穆之戴施二千戍洛陽，不同漢陳午或某氏父陳午，未知生卒。',9113:'鞠殷鞠彭子尚書左丞至東萊守兩key同，父舊怨王弥曹嶷已死不套本人死，無生卒。',9114:'顧悅之君叔殷浩故吏/愷之父三key閉；與簡文同年可由本紀326七歲推320生，非明生日。左丞/右丞源官異保留，不因此把同字同子之父拆兩人；卒無年。',9115:'乙璋逸子閉車奉迎兩key同，父憂必敗不是預言實現/本人死，未知生卒。',9116:'仇騰357尚書領選/譖王猛降甘松護軍兩key同，原385馮翊授官若取亦晚SL不作獨立已證年，無生卒。',9117:'傅末波/傅顏晉書郗昙敗與桓温林渚、燕長樂太守東阿四key同燕軍將，原名不同保別名，郗昙年42死不是傅死；無生卒。',9118:'劉康西域胡冒稱劉曜子353平陽擒兩key同，詐稱親屬不作父系定論；被擒不明殺，未知生卒。',9119:'劉特秦平州刺史357五千戶降燕，原49歲死主赭白馬非本人，唯SL未知生卒。',9120:'司馬恬元愉譙敬王無忌子，390卒無齡，父350卒與後裔年均不套本人，生不詳。',9121:'呼延毒353灞城叛秦/354隨溫歸晉兩key同，不同呼延晃，無生卒。',9122:'姚益生姚襄兄曜武王欽盧招胡同8857姚益，各key全閉→8857，未知生卒。',9123:'姚蘭襄從兄輔國將軍敷鄜城被飛龍擒三key閉，襄27死非蘭齡死，未知生卒。',9124:'孫統承公孫楚孫纂子、綽兄、余姚令三key閉，SS高柔序述柔婚姻年20非統齡，後傳綽人物不混，未知生卒。',9125:'席寶丞相長史/太子詹事譖猛白衣領長史兩key同，惩降非處死，王猛36歲非本人齡；未知生卒。'}.items():results[ident]['unresolved'].append(note)
    round47b_reads={9126:[('js_16','中军将军慕容虔曰'),('sl_03','中军将军虔与护军将军平熙')],9127:[('tj_18','中军将军慕舆虔曰')],9128:[('sl_03','赵故卫尉常山李犊聚众数千反于普壁垒'),('tj_17','赵故卫尉常山李犊聚众数千人叛燕')],9129:[('sl_09','执法御史杜逸言于瓘')],9130:[('yi_02','刺史温放之讨破之'),('js_09','放之嗣爵，少历清官')],9131:[('sl_04','曜武将军姚益生左将军王钦卢'),('sl_06','遣兄曜武将军益及左将军王钦卢'),('tj_18','曜武将军姚益生、左将军王钦卢')],9132:[('sl_04','免左丞程卓官以王猛代之'),('tj_18','免左丞程卓官，以王猛代之')],9133:[('js_12','故范丰之属反叛于芍陂')],9134:[('sl_04','胡阳赤起司竹'),('tj_17','胡阳赤起司竹')],9135:[('sl_06','平北将军苻道龙')],9136:[('sl_04','健以领军将军愿为秦州刺史'),('tj_17','秦主健以领军将军苻愿为秦州刺史')],9137:[('sl_04','左卫将军飞讨擒之'),('tj_17','秦左卫将军苻飞讨擒之'),('sl_05','苻飞为杨初所败')],9138:[('tj_18','与故姚襄参军薛赞、权翼善')],9139:[('sl_04','西虏乞没浑邪遣子入侍')],9140:[('sl_09','又立妻叱干氏为王后')],9141:[('sl_03','宁东将军乔庶镇南将军石贤')],9142:[('js_17','宋纤，字令艾'),('sl_09','宋纎字令文[一作艾]')],9143:[('js_17','祚遣其太子太和以执友礼造之'),('tj_17','子太和为太子')],9144:[('js_13','先遣苻融、慕容、张蚝、苻方等'),('js_18','苻坚乃使其将王鉴、张蚝等率兵'),('sl_03','使虎牙将军张蚝率壮士数百'),('sl_04','平使养子张蚝御之'),('sl_05','张蚝本姓弓上党泫人也'),('tj_18','与羌相持旬馀，莫能相胜'),('wsh_00','及朱彤、张蚝、邓羌等诸道来寇'),('js_14','苻坚遣其弟融及骁将张蚝攻陷寿阳')],9145:[('sl_03','遣司马悦明救之'),('tj_18','慕容尘遣司马悦明救泰山')],9146:[('sl_03','偻为南康公'),('tj_17','偻为南康公')],9147:[('sl_03','岳为安丰王'),('tj_17','岳为安丰王')],9148:[('sl_03','宁北将军度为乐浪王'),('tj_17','宁北将军度为乐浪王')],9149:[('sl_05','遣西平公强率精骑追之'),('tj_17','以慕容强为前锋都督'),('tj_18','遣西平公强帅精骑追之')],9150:[('sl_03','徽为河间王'),('tj_17','徽为河间王')],9258:[('tj_18','平北将军苻道、龙骧将军东海王坚')]}
    for ident,qs in round47b_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,source_identities_all_adjudicated=ident not in [9136,9149],identity_status='all_source_key_subjects_individually_adjudicated_round47b',last_manual_review_batch='round47b')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第47批逐源本人及事件原文實讀','所列本人官地/親族及同役段落已讀，避免把被捕、官免、旁人戰死當成本人卒。'))
    results[9127].update(review_status='duplicate',duplicate_of=9126,identity_status='murongqian_muyuqian_same_zhongjun_shenjin_execution_advice')
    results[9127]['evidence'].append(chunk_ev('js_16','中军将军慕容虔曰','晉書慕容虔/TJ慕輿虔同沈勁死案','同中軍將軍、恪欲宥沈勁、勸不為人用必後患一案逐句同，源姓慕容/慕輿異，不以近名單獨並。'))
    results[9138].update(review_status='duplicate',duplicate_of=8987,identity_status='xuezan_xuezhan_same_yaoxiang_canjun_with_quanyi')
    results[9138]['evidence'].append(chunk_ev('tj_17','太原薛瓒、略阳权翼为参军','薛瓚/贊同姚襄參軍權翼搭配','352姚襄部曲置官與357故姚襄參軍同權翼轉秦，不同漢末薛綜或晉薛讚，目標8987單key已全閉。'))
    results[9258].update(review_status='duplicate',duplicate_of=9135,identity_status='fudao_fudaolong_same_pingbei_357_sanyuan_qin_force')
    results[9258]['evidence'].append(chunk_ev('sl_06','平北将军苻道龙','苻道/道龍同357三原平北將軍','同苻黃眉、苻堅、鄧羌萬五千御姚襄同役將領序列，名道與道龍源字別保留；9258僅TJ一key與9135僅SL一key均本人閉合。'))
    results[9136].update(review_status='needs_further_research',identity_status='fu_yuan_qinzhou_governor_two_history_keys_clear_cbdb_original_gazetteer_unretrieved',component_identities=[dict(identity='前秦領軍秦州刺史苻願353上邽',keys=[k for k in results[9136]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=None),dict(identity='CBDB420174苻願，直隸秦州新志原段未取得',keys=['cbdb:420174'],historical_birth=None,historical_death=None)])
    results[9136]['evidence'].append(dict(citation='原CBDB420174苻願來源及日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='BIOG_MAIN無生卒、無親屬；source38333直隸秦州新志section5190525，68942 Q65835997。',interpretation='與9022/9063同地方志section但尚未取本人官職原句，不以同名自動將CBDB支閉合；兩史明官仍保留，未知日期。'))
    results[9149].update(review_status='needs_further_research',identity_status='murongqiang_354_vanguard_vs_369_xiping_duke_link_unresolved',component_identities=[dict(identity='354燕前鋒都督慕容強，SL03對應作疆並封洛陽王',keys=['tj:tj_17:300:慕容強'],historical_birth=None,historical_death=None),dict(identity='369西平公強追慕容垂',keys=['sl:sl_05:154:慕容強','tj:tj_18:153:慕容強'],historical_birth=None,historical_death=None)])
    results[9149]['evidence'].append(chunk_ev('sl_03','前锋都督疆为洛阳王','354前鋒都督強/疆爵源別補核','SL同任命前鋒督荊徐後封洛陽王，與369西平公強之爵未橋；不僅因同名認同或斷定異人，待明轉爵/族系。'))
    thdie=chunk_ev('tj_17','以庶人礼葬之，并杀其二子','資治通鑑永和十一355張祚二子被殺','同書354具名祚子太和太子及庭堅建康侯，355祚被殺隨之二子皆殺；與JS太子太和見宋纖本人同。只取年度，未把祚父年齡或被殺日精確套子。')
    results[9143].update(review_status='primary_text_corroborated',historical_death=date(355,thdie))
    results[9143]['evidence'].append(thdie)
    results[9142]['source_reported_age_at_death']={'age':82,'calendar_year':None,'source':'晉書宋纖本傳；SL宋纎'}
    h385=evidence(SOURCE/'_cache/texts/资治通鉴.txt','九月，秦主丕以张蚝为侍中、司空','資治通鑑太元十385九月','本年乙酉385及本人秦丕司空在任明文，不以SL先君語推死亡；母父養子不同人不混。')
    results[9144]['evidence'].append(h385)
    results[9144]['last_attested_activity']=385
    for ident,note in {9126:'慕容虔/慕輿虔同前燕中軍將軍，357敕勒/365勸殺沈勁同鏈全閉；死者沈勁非虔，未知生卒。',9127:'慕輿虔TJ與9126JS慕容虔同中軍將軍同勸殺沈勁逐句同→9126；姓氏源別保留。',9128:'李犢趙衛尉常山普壁聚眾353降燕兩key同，明降非死，未知生卒。',9129:'杜逸前涼御史論地變移禍族人只是建議，未記本人死，唯SL未知生卒。',9130:'溫放之溫嶠子、交州破林邑兩key同；宋書作哀帝升平初帝號年號不配，保原叙不據此推精年，卒官無年，生不詳。',9131:'王欽盧左將軍同襄兄益招北羌胡三key閉，軍眾5萬戶非年齡，姚襄三原死不套本人，無生卒。',9132:'程卓秦尚書左丞357文案不治免兩key同，官免非卒，無生卒。',9133:'范豐殷浩屬芍陂反只是温劾書控辭，未記被殺，無生卒。',9134:'胡陽赤司竹叛秦/克司竹後逃灞城兩key同，殺劉珍夏侯顯非本人，未知生卒。',9135:'苻道龍SL姚襄役平北將軍與9258TJ苻道同將序列，名字源異保持，無生卒。',9136:'苻願秦州上邽守被王擢敗奔長安兩史明本人未死；CBDB地方志原句未取全條hold，不與他書苻碩硬合，未知生卒。',9137:'苻飛秦左衛新興王三key同353仇池敗/356諫強平，不同384監慕容垂者苻飛龍或苻丕僅同名近字未橋不合；無生卒。',9138:'薛贊/瓚同故姚襄參軍同權翼轉秦，字異→8987；苻生357死不是贊死，未知生卒。',9139:'乞沒渾邪西虜首領遣子侍，未云本人到秦或死，單SL生卒未知。',9140:'叱干氏張祚妻王后，不能把妻辛氏源名自動合成同一人；原段又立為后可並列兩妻，無生卒。',9141:'喬庶張平寧東將軍同蘇象石賢诸葛骧壘降燕，不同曹魏譙周；只有晚SL未知生卒。',9142:'宋纖/纎字令艾/令文來源異，同效谷隱士弟子3000、馬岌張祚太子友同故事兩key閉，82卒無確年，不從祚在位便鎖355卒或274生。',9143:'張太和張祚太子兩key閉，TJ355并殺祚二子可定355年死；宋纖82非本人齡，生不詳，未套祚死具體日。',9144:'張蠔本弓姓張平養子、秦虎賁潞川淝水將八key閉；385九月丕侍中司空獨立TJ活年，無確生卒。不把平被滅/淝水苻融死當蚝死。',9145:'悅明慕容塵司馬援山茌兩key同，賈堅此前死與荀羡病均非明死，未知生卒。',9146:'慕容僂俊弟354南康公兩key同，與其他南康王不同，無生卒。',9147:'慕容岳俊弟354安豐王兩key同，不同後燕慕容嶽是否同字需對方本人核前不合，無生卒。',9148:'慕容度寧北將軍354樂浪王兩key同，不同高句麗王釗樂浪公，無生卒。',9149:'354前鋒強與SL疆洛陽王、369西平公強兩scope未定位轉爵或族系橋，暫不全閉也不自稱真混，兩支生卒均未知。',9150:'慕容徽俊弟354河間王兩key同，不能以後燕慕容會同音合，生卒未知。',9258:'苻道TJ與9135SL道龍357三原平北同職同四將同役，源名差保持→9135。'}.items():results[ident]['unresolved'].append(note)
    round48a_reads={9151:[('sl_03','涉为渔阳王'),('tj_17','涉为渔阳王')],9152:[('sl_03','温为带方王'),('tj_17','温为带方王')],9153:[('sl_03','前锋都督疆为洛阳王')],9154:[('sl_03','秀为兰陵王'),('tj_17','秀为兰陵王')],9155:[('sl_03','纳为北海王'),('sl_08','慕容超字祖明备德兄北海王纳之子也'),('tj_17','纳为北海王')],9156:[('sl_03','子臧[一作咸]为乐安王'),('sl_04','燕卫大将军乐安王慕容臧'),('tj_18','乐安王臧讨李历于濮'),('sl_05','先发制人但除上庸王评及乐安王臧'),('sl_06','遂将旧骑奔乐安王臧于石门')],9157:[('tj_17','逮为临贺王')],9158:[('sl_03','逯[一作逮]为临贺王')],9159:[('tj_17','咸为乐安王')],9160:[('sl_03','黙为始安公'),('tj_17','默为始安公')],9161:[('sl_03','龙为歴阳王'),('tj_17','龙为历阳王')],9162:[('gs_01','年六十卒于山寺'),('ssxy_01','于法开始与支公争名'),('ssxy_03','闻于法开有名')],9166:[('tj_18','中常侍涅皓因希旨'),('sl_06','中常侍湼皓因希俊旨')],9167:[('sl_03','镇南将军石贤')],9168:[('sl_03','镇北将军苏象')],9169:[('sl_03','征西将军诸葛骧'),('tj_18','征西将军诸葛骧')],9170:[('sl_03','遂以坚子活为任城太守'),('tj_18','燕主俊以贾坚子活为任城太守')],9171:[('js_15','立妻辛氏为皇后'),('sl_09','立妻辛氏为王后'),('tj_17','立妻辛氏为王后')],9172:[('js_10','超字景兴，一字嘉宾'),('js_11','郗超亦深止之'),('js_12','又谓郗超曰'),('js_13','中书郎郗超虽素与玄不善'),('js_18','于是参军郗超进废立之计'),('tj_16','又辟郗超为抚军掾'),('tj_17','温谓掾郗超曰'),('tj_18','又以征西掾郗超为参军'),('wsh_02','问中书郎郗超曰'),('js_17','至四十，卒于中书郎'),('sz_01','高平郗超遣使遗米千'),('ssxy_00','既而谓郗超曰'),('ssxy_01','桓宣武与郗超议芟夷朝臣'),('ssxy_02','郗超与虎书'),('ssxy_03','王珣郄超并有竒才'),('gs_01','高平郤超宣述林解')],9173:[('sg_05','嘉孙敞字泰中有才识位散骑常侍'),('tj_17','江西流民郭敞等千馀人'),('sl_06','江西流民郭斁[一作敞]')],9174:[('sl_05','郎中令高弼俱奔于秦'),('tj_18','吴国典书令辽东高弼'),('sl_06','吴国典书令辽东髙弼')],9175:[('yi_01','父法弘袭爵')]} 
    for ident,qs in round48a_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=ident!=9173,identity_status='all_source_key_subjects_individually_adjudicated_round48a' if ident!=9173 else 'guojia_grandson_and_fourth_century_refugee_scopes_not_bridged',last_manual_review_batch='round48a')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第48批前燕宗室與東晉人物逐源核對','實讀本人封爵親屬、官職或同案上下文；不以旁人年齡、被害、政權覆亡推本人生卒。'))
    for ident,target in [(9158,9157),(9159,9156)]:results[ident].update(review_status='duplicate',duplicate_of=target,identity_status='same_person_unique_title_parent_and_identical_investiture_with_explicit_variant')
    results[9149]['component_identities'][0]['candidate_duplicate_of']=9153
    results[9153]['unresolved'].append('單SL明前鋒都督疆及洛陽王；9149前鋒強同任命可支級對應，但其369西平公支尚未取得轉爵橋，不能整條刪。')
    results[9155]['unresolved'].append('納/汭北海王德兄超父身份三key閉。SL超傳有苻昌誅納而段氏懷超故事，但本句未具本人死年；不能把垂起兵、超後歸年直接鎖父卒。')
    results[9156]['unresolved'].append('臧/咸俊子樂安王，354封與討李歷、369反垂密議、370石門/荥陽軍同五key閉；未明本人卒，不把勸除、燕亡或後來宗室遇害套他死。')
    results[9161]['unresolved'].append('俊弟歷陽王龍，不是慕容垂子隆；生卒不詳。')
    results[9162]['source_reported_age_at_death']={'age':60,'calendar_year':None,'source':'高僧傳于法開傳'}
    results[9162]['unresolved'].append('師法蘭、剡山居醫術及弟子法威同三key；60卒無年，不把升平五帝疾或後哀帝征召年當卒年。')
    for ident in [9163,9164]:results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,last_manual_review_batch='round48a_cbdb')
    results[9163]['evidence'].append(dict(citation='原CBDB194413李吉及原來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='Death year estimated by Tackett (cc419); index356; cf. 補遺7:138-139、文物1999.1:93、洛陽新獲續504；source32033/71853頁160382。',interpretation='419明估算撤出歷史確年，356指數年非活動年。原墓誌未取得，身份日期待核，不把庫欄位作古證。'))
    peach=dict(citation='樂府詩集卷45清商曲辭桃葉歌，引古今樂錄及隋書五行志',source_url='https://zh.wikisource.org/wiki/樂府詩集/045卷',excerpt='《古今樂錄》曰：「《桃葉歌》者，晉王子敬之所作也。桃葉，子敬妾名，緣於篤愛，所以歌之。」…桃葉復桃葉，桃樹連桃根。相憐兩樂事，獨使我殷勤。',interpretation='實取並讀完整本人注及四首詩，明桃葉為王獻之妾但無生卒。詩中桃根一語不能自行證明桃根為人或桃葉之妹。')
    results[9165].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='cbdb135442_wangxianzhi_concubine_independently_bridged_by_yuefu',last_manual_review_batch='round48a_cbdb_primary')
    for ident in [9164,9165]:results[ident]['evidence'].append(peach)
    results[9165]['evidence'].append(dict(citation='原CBDB135442親屬與原引限制',excerpt='135442桃葉；無生卒；kin134配135441王獻之；source9601先秦漢魏晉南北朝詩2002。',interpretation='古今樂錄所引妾名及配偶與庫身份可橋；未讀MQWW原版本人頁，現已讀的是樂府詩集保存古今樂錄引文，不冒稱庫原引已取。'))
    results[9164]['identity_status']='cbdb135443_taogen_human_identity_not_independently_verified'
    results[9164]['unresolved'].append('原庫桃根135443，配135441王獻之、姊妹135442桃葉；無日期。已讀樂府原詩只有桃樹連桃根，沒有明言桃根是人物或姊妹。需另取具體人物原引；不因庫關係直接完成身份核查。')
    xdie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','十二月，临海太守郗超卒','資治通鑑太元二377十二月','實讀377丁丑年標、冬十月後十二月卒及箱中桓溫密計父愔怒同本傳，明本人死亡年月；無日。')
    results[9172].update(review_status='primary_text_corroborated',historical_death=date(377,xdie,month=12))
    results[9172]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9172]['evidence'].append(xdie)
    results[9172]['evidence'].append(dict(citation='原CBDB69870郗超身份及日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='郗超、晉；birth336、death377、death_age41；KIN無；source68942 Q45683588。',interpretation='庫336/377/41不是第三個獨立古證。姓名朝代及377卒可與本人史傳橋，原始引用仍僅回引庫；41是實歲差，不能用它解決古書42與40之異。'))
    results[9172]['source_reported_chronologies']=[dict(source='晉書郗超本傳',death_year=377,reported_age=42,conditional_birth_year=336,calculation='377-42+1=336；卒年借獨立通鑑，虛歲推算非明生年'),dict(source='晉書杜不愆占筮傳神異故事',death_year=377,reported_age=40,conditional_birth_year=338,calculation='377-40+1=338；神異故事本人卒齡異說，不與42混合擇一')]
    results[9172]['historical_birth']=None
    results[9172]['unresolved'].append('17key已逐源核清郗/郄/郤超字景興嘉賓、愔子桓溫參軍同人。主傳42卒與杜不愆神異敘40卒保源別，出生不單鎖336。TJ345昱錄尚書後性格匯叙又辟超不能硬作345九歲初仕；不送此匯叙年自動修估。')
    results[9173].update(review_status='needs_further_research',source_identities_all_adjudicated=False,component_identities=[dict(identity='世語郭嘉孫敞字泰中散騎常侍',keys=[k for k in results[9173]['keys'] if k.startswith('sg:')],historical_birth=None,historical_death=None),dict(identity='354江西流民郭敞/郭斁執劉仕/劉任降姚襄',keys=[k for k in results[9173]['keys'] if not k.startswith('sg:')],historical_birth=None,historical_death=None)])
    results[9173]['unresolved'].append('兩個本人scope未有親族轉仕橋：郭嘉孫西晉散騎常侍 vs354江西流民首領。暫不宣稱必異人，但不能只同名合成整條；兩支生卒皆未明。TJ劉仕陳留內史與SL陳留劉任唐邑內史為同執降敘事源異。')
    results[9174]['unresolved'].append('前燕遼東高弼垂國典書令受巫蠱案與後郎中令從垂奔秦同臣鏈，非段氏狱死即本人死；生卒未明。')
    results[9175]['unresolved'].append('宋書作且渠法弘，蒙遜父祁復延子、襲爵中田護軍，且渠/沮渠姓源異不造兩人；未記本人生卒。')
    round48b_reads={9176:[('sl_04','光禄散骑西河任群'),('tj_18','光禄、散骑西河任群')],9178:[('js_18','僧涉者，西域人也')],9179:[('js_13','先遣征虏将军刘建修治马头城池'),('js_14','父建，有武干，为征虏将军')],9180:[('sl_03','攸部将匡超进据嵩')],9182:[('js_18','孟钦，洛阳人也')],9184:[('js_15','瓘弟琚及子嵩募数百市人'),('sl_09','祚收瓘弟琚及子嵩将杀之'),('tj_17','凉王祚收张瓘弟琚及子嵩'),('js_19','弥长史张嵩谏曰')],9185:[('sl_04','新平有长人见长五丈语于百姓张靖'),('sg_15','临困授子靖留牋')],9186:[('sl_09','诛大姓彭姚'),('js_15','诛大姓彭姚')],9187:[('sl_03','又遣督护徐冏率水军三千')],9188:[('sl_09','为厨士徐黒所杀'),('wsh_04','为厨士徐黑所杀')],9189:[('js_11','击慕容屈尘，破之')],9190:[('gs_01','时泰山复有支僧敦者')],9191:[('wsh_04','二子佛奴、佛狗逃奔苻坚'),('yi_01','二子佛奴、佛狗奔逃关中')],9192:[('wsh_04','佛狗子盛，先为监国'),('yi_01','佛狗为抚夷护军')],9193:[('sl_09','壮士杨和等四十余骑'),('tj_18','壮士杨和等四十馀骑')],9194:[('sl_03','御史中尉杨约数其残酷之罪')],9196:[('tj_18','郎中殷郇谏之'),('wsh_04','郎中殷郇陈损益')],9197:[('js_00','太常江逌议'),('js_14','江逌，字道载')],9198:[('sl_09','瓘麾下玄胪刺混不能穿甲'),('tj_18','瓘麾下玄胪刺混')],9200:[('yl_05','石垣字洪孙')]} 
    for ident,qs in round48b_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=ident not in [9184,9185],identity_status='all_source_key_subjects_individually_adjudicated_round48b' if ident not in [9184,9185] else 'multiple_unbridged_subject_branches',last_manual_review_batch='round48b')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第48批前秦前涼及晉臣逐源實讀','本人官職、親族及同案已實讀，將相鄰他人年齡、死亡與本人區分；未知年月不補。'))
    for ident,cb,n,q in [(9177,389692,'侯雅','Q65834139'),(9181,396710,'姜平子','Q65862274'),(9199,414906,'王櫂','Q65815056')]:
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,identity_status='cbdb_original_qinzhou_gazetteer_section_unobtained',last_manual_review_batch='round48b_cbdb')
        results[ident]['evidence'].append(dict(citation=f'原CBDB{cb}{n}來源及日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=f'無生卒、無KIN；source68942 {q}及38333直隸秦州新志section_id5190525。',interpretation='原庫逐條已查，原志本人原句尚未取得，不能把同一section或Q回引當獨立身份史證。遊戲337/353/373只是估算，不提升確年。'))
    song=evidence(SOURCE/'_cache/texts/晋书.txt','韦逞母宋氏，不知何郡人也','晉書列女韋逞母宋氏完整本人傳','實讀全傳：父授周官、石虎徙冀州、程安壽養護、逞仕苻堅太常、80歲授120生宣文君。80是授業時齡非卒齡；本段無明授業年及生卒。')
    results[9183].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='cbdb135632_explicit_weicheng_mother_bridged_to_jin_lienv',last_manual_review_batch='round48b_cbdb_primary')
    results[9183]['evidence'].extend([song,dict(citation='原CBDB135632宋氏韋逞母身份核對',excerpt='c_name_chn宋氏(韋逞母)；kin180子135634；source9601先秦漢魏晉南北朝詩3291，原注傳主為宋氏(韋逞母)。無生卒。',interpretation='原庫限定兒子及傳主與晉書本人明文身份橋；未冒稱已讀MQWW3291原頁。')])
    results[9183]['source_reported_age_at_activity']={'age':80,'calendar_year':None,'event':'教周官','source':'晉書列女宋氏傳'}
    results[9184].update(review_status='identity_conflict',source_identities_all_adjudicated=False,component_identities=[dict(identity='張瓘子嵩355與叔琚募市人迎宋混',keys=[k for k in results[9184]['keys'] if not k.startswith('js:js_19:')],historical_birth=None,historical_death=None),dict(identity='王彌長史張嵩311洛陽諫彌與劉曜和',keys=[k for k in results[9184]['keys'] if k.startswith('js:js_19:')],historical_birth=None,historical_death=None)])
    results[9184]['unresolved'].append('四key兩個明本人scope：瓘子嵩及王彌長史嵩，無父族轉仕橋，不能把任一支日期套整人。355祚命收嵩將殺但實際嵩募眾入城，不是本人已被殺；359瓘殺妻子30口不能無具名自動算嵩死。')
    results[9185].update(review_status='identity_conflict',source_identities_all_adjudicated=False,component_identities=[dict(identity='苻健時新平百姓張靖長人告受命故事',keys=[k for k in results[9185]['keys'] if k.startswith('sl:')],historical_birth=None,historical_death=None),dict(identity='吳張紘子靖受父臨終留箋',keys=[k for k in results[9185]['keys'] if k.startswith('sg:')],candidate_duplicate_of=3371,historical_birth=None,historical_death=None)])
    results[9185]['unresolved'].append('原SG張紘子受留箋與前秦355新平百姓兩個時代本人，不全條→3371；僅吳子靖分支可對3371，未知生卒。')
    quantree=dict(citation='新唐書卷75下權氏宰相世系',source_url='https://zh.wikisource.org/wiki/新唐書/卷075下',excerpt='權氏出自子姓…漢有左輔都尉忠，十四世孫翼，字子良，前秦右僕射、安丘敬公。生宣吉、宣褒。宣褒，後秦黃門侍郎，六世孫榮。',interpretation='實際取得並讀權氏系至德輿：原庫13617注指定XTS75B8a-9a，次子13618權宣褒與此世系逐人一致；前秦右仆射與8965全7key軍政鏈為同人，不是同名即合。無確生卒。')
    results[9195].update(review_status='duplicate',historical_birth=None,historical_death=None,duplicate_of=8965,source_identities_all_adjudicated=True,identity_status='cbdb13617_xts_explicit_son_quanxuanbao_and_qin_right_pushe_bridged',last_manual_review_batch='round48b_cbdb_primary')
    results[9195]['evidence'].extend([quantree,dict(citation='原CBDB13617權翼本人注及族系',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='For genealogy, see XTS75B8a-9a；次子13618權宣褒；c_birthyear0 c_deathyear0；source68942 Q45378254。',interpretation='已實讀指定新唐書本人世系，不取零年為歷史日期。')])
    results[8965]['evidence'].append(quantree)
    results[9197]['evidence'].append(chunk_ev('js_14','病卒，时年五十八','晉書江逌本傳卒齡','已讀本人全傳含洪祀爭論及末尾卒句，58卒無明年；原遊戲291/371相差80不是此58的明記年代，不能以game371倒推314當確證。'))
    results[9197]['source_reported_age_at_death']={'age':58,'calendar_year':None,'source':'晉書江逌本傳'}
    results[9197]['appearance_upper_bound']=346
    results[9197]['unresolved'].append('江逌字道載父濟、何充辟驃騎功曹、殷浩長史、穆哀二帝太常兩key身份閉。何充346卒前已為功曹，其前蔡謨參軍/不就征辟更早；346只登場上界，不是初仕，也不表示本人恰346在世。出生/卒年仍不詳。')
    results[9172]['evidence'].append(chunk_ev('js_10','年四十二，先愔卒','晉書郗超本傳明卒齡','與js17占筮傳至40卒不同，並列來源；獨立TJ377卒只定死年，不消除古書齡異。'))
    for ident,note in {9176:'任群西河光祿散騎被猛薦同官兩源，36歲歲五遷是王猛非任群；生卒不詳。',9178:'僧涉西域無姓沙門卒長安但未記年；每日500里不是年齡，後旱追念非仍在世。',9179:'劉建征虜將軍治馬頭與牢之父建同官同時軍脈可橋；謝萬42卒非建齡，不填367。',9180:'匡超單SL諸葛攸部將據嵩嶅，敗軍不等本人死，無生卒。',9182:'孟欽神異故事苻融命誅而逃、青州復見未明處死，不把所謂詔誅當卒年；無生卒。',9186:'彭姚陇西大姓為李儼所誅，兩源同本案。但JS叙在355建國後可匯叙政事，SL列355不是獨立早源明本人死亡年；保有被殺事實不鎖355確年。',9187:'徐冏單SL諸葛攸督護水軍3000，同攻東阿敗未明本人死；無生卒。',9188:'徐黑/黒厨士刺祚同一殺祚案，不是本人被殺，無生卒。',9189:'慕容屈塵為桓豁所破，不等被殺；是否其他慕容塵別寫需對方角色橋，暫不憑近名去重，無生卒。',9190:'支僧敦冀州泰山、人物始義論作者，前句85卒是竺僧朗，不套本人生卒。',9191:'楊佛奴宋奴子佛狗兄定父，前秦右將軍；無明本人卒，不把定394死套父。',9192:'楊佛狗宋奴子佛奴弟盛父，抚夷護軍，宋魏兩傳同；無本人生卒，盛死非狗死。',9193:'楊和壯士隨宋混40騎入城，40餘是兵數非年齡；殺瓘非和死，生卒未知。',9194:'楊約御史中尉鞭石虎尸，虎早卒非約死，單SL無生卒。',9196:'殷郇郎中諫張瓘虎生3日同原句兩key；不把瓘359死套本人。',9198:'玄胪刺混被擒後轉為心膂，非被殺；生卒未知。',9200:'石垣字洪孫北海劇人，王隱晉書逸民引本人；姚萇亂莫知所終不是已卒，不填373卒或終年。'}.items():results[ident]['unresolved'].append(note)
    round49a_reads={9201:[('sl_04','叡为巨鹿公'),('sl_04','叡弗从战于华泽叡兵败绩')],9202:[('sl_05','父敞健之世为太尉司马')],9203:[('sl_04','晖为平原公'),('sl_05','苻晖坚第二子也'),('tj_18','晖为平原公')],9205:[('sl_04','生将飞龙击兰擒之'),('sl_06','垂杀苻飞龙麟屡进策'),('tj_18','秦将苻飞龙击兰')],9206:[('yl_00','术士盖钦')],9207:[('js_18','窦滔妻苏氏，始平人也'),('sg_07','苏姫生邯郸懐王邕')],9208:[('sl_09','西平卫𬘭亦据郡叛'),('tj_17','西平人卫𬘭亦据郡叛'),('js_15','西平人卫𬘭又据郡叛')],9209:[('sl_04','丞相司马贾雍为云中护军'),('tj_18','丞相司马贾雍为云中护军'),('yi_00','坚云中护军贾雍掠其田者')],9210:[('yi_00','赫连氏有名卫臣者')],9212:[('sl_09','西平郭勋解天文'),('js_15','西平人郭勋解天文')],9213:[('js_15','玄靓以其庶母郭氏为太妃'),('sl_09','母郭夫人所生也'),('tj_18','尊庶母郭氏为太妃')],9214:[('gs_01','释慧持者。慧远之弟也'),('gs_01','以晋义熙八年卒于寺中')],9215:[('gs_01','释道立。不知何许人')],9216:[('js_16','时冠军将军陈祐守洛阳'),('js_18','冠军将军陈祐告急'),('tj_18','晋将陈祐弊卒千馀')],9218:[('sl_03','冠军将军河内太守冯鸯'),('tj_17','上党人冯鸯逐燕太守段刚'),('tj_18','冯鸯既以上党来降')],9219:[('tj_17','弟阏头立，将贰于代'),('tj_18','匈奴刘阏头部落多叛'),('wsh_00','刘务桓死，其弟阏头立'),('wsh_02','务桓死，弟阏陋头代立')],9220:[('sl_03','孙希[泳之子也]为并州刺史'),('tj_18','希，泳之弟也')],9221:[('sl_09','杜洪张琚因赵氏成资')],9222:[('sl_04','遣司马徐斌率骑袭之')],9223:[('tj_18','遣司马徐赟帅骑袭之')],9224:[('sl_03','武卫将军慕舆干潜谋诛恪及评')],9225:[('js_18','讨蛮贼文卢等')]} 
    for ident,qs in round49a_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=ident not in [9207,9218],identity_status='all_source_key_subjects_individually_adjudicated_round49a' if ident not in [9207,9218] else 'source_key_identity_scope_not_unified',last_manual_review_batch='round49a')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第49批前秦北方及佛傳本人逐源實讀','實讀本人事件及上下文，未知生卒保持空值；被誅、逃遁、旁人卒與追述分開。'))
    ruidie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','睿兵败，为泓所杀','資治通鑑太元九384華澤役','實讀甲申384年、三月起兵徵巨鹿公、四月敘戰：苻睿/叡姚萇為司馬同一本人，死明確。四月丙辰是慕容麟益兵信都，不能將該日套睿死，保年度。')
    results[9201].update(review_status='primary_text_corroborated',historical_death=date(384,ruidie))
    results[9201]['evidence'].append(ruidie)
    huidie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','三月，晖愤恚自杀','資治通鑑太元十385三月','平原悼公晖、父堅責敗、冲阿房同SL本傳，獨立明385三月卒，無日。')
    results[9203].update(review_status='primary_text_corroborated',historical_death=date(385,huidie,month=3))
    results[9203]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only');results[9203]['evidence'].append(huidie)
    flydie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','壬午，夜，垂遣世子宝将兵居前','資治通鑑太元八383十二月壬午夜苻飛龍被殺','實讀本年十二月苻堅返長安與次年384正月標界：垂氐騎副苻飛龍夜合殺，死在383歲末而非翌年384。357擒姚蘭與後廣武將軍無人物衝突。')
    results[9205].update(review_status='primary_text_corroborated',historical_death=date(383,flydie,month=12,day_ganzhi='壬午'))
    results[9205]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only');results[9205]['evidence'].append(flydie)
    results[9206].update(review_status='duplicate',duplicate_of=9182,identity_status='unique_qin_changan_banquet_execution_whirlwind_escape_narrative_name_variant')
    results[9206]['unresolved'].append('御覽前秦錄作蓋欽，晉書孟欽，同苻堅召長安以惑眾命誅、酒宴收捕化旋風逸的獨事，姓源異保持→9182，不把命誅而逃認卒。')
    results[9207].update(review_status='identity_conflict',source_identities_all_adjudicated=False,component_identities=[dict(identity='前秦竇滔妻蘇蕙字若蘭回文詩作者',keys=[k for k in results[9207]['keys'] if k.startswith('js:')],historical_birth=None,historical_death=None),dict(identity='魏文帝曹丕蘇姬生曹邕',keys=[k for k in results[9207]['keys'] if k.startswith('sg:')],historical_birth=None,historical_death=None)])
    results[9207]['unresolved'].append('明不同配偶、子女、時代的兩蘇氏，不把曹丕妾生卒/封王子追諡年套前秦女詩人。全條hold，兩支生卒未知。')
    results[9210].update(candidate_duplicate_of=9240,identity_status='helian_weichen_songshu_branch_matches_liu_weichen_qin_field_return_event_target_pending_allkeys')
    results[9210]['unresolved'].append('宋書赫連衛臣朔方、雲中賈雍襲田與9240劉衛辰同獨案；父佛佛源關係可橋，9240全6key尚待接續核完，先候選不整條硬刪。衛臣死子佛佛繼是匯叙無明死亡年，不填遊戲373。')
    h412=chunk_ev('gs_01','以晋义熙八年卒于寺中。春秋七十有六','高僧傳慧持412卒76','已讀慧持完整本傳至遺命、道泓曇蘭二弟子，義熙八412、虛歲76→337；非下一慧永414/83或僧濟45之齡。')
    results[9214].update(review_status='primary_text_corroborated',historical_death=date(412,h412),historical_birth=date(337,h412,age=76));results[9214]['evidence'].append(h412)
    for ident,cb,n,detail in [(9204,507551,'苻某','source38333直隸秦州新志section5190525，無KIN無日期'),(9211,426439,'趙盛之','source38333同section及68942 Q65861315，無KIN無日期')]:
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,identity_status='cbdb_original_qinzhou_gazetteer_person_passage_unobtained',last_manual_review_batch='round49a_cbdb')
        results[ident]['evidence'].append(dict(citation=f'原CBDB{cb}{n}限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=detail,interpretation='逐條查原庫，但未得原志本人段；苻某還是未具名人物而非獨立確名武將，不由同姓/政權估生卒或自動並苻堅。'))
    ygene=dict(citation='全唐文卷340顏真卿唐故通議大夫行薛王友柱國贈秘書少監國子祭酒太子少保顏君碑銘',source_url='https://zh.wikisource.org/wiki/全唐文/卷0340',excerpt='生州西曹騎都尉、西平侯諱𬘭，字文和。生宣城太守、御史中丞諱靖之，字茂宗。生巴陵太守、度支校尉諱騰之，字弘道，善草隸書，有風格。',interpretation='實取碑銘並讀靖之父子三代本人段：父綝/𬘭與子騰之同原CBDB kin，宣城守御史中丞身份清楚，無本人生卒；非僅名字搜尋命中。')
    results[9217].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='cbdb32420_father_yanlin_son_yantengzhi_independent_tang_epitaph_bridge',last_manual_review_batch='round49a_cbdb_primary')
    results[9217]['evidence'].extend([ygene,dict(citation='原CBDB32420顏靖之原注及族系',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='Yan Zhenqing WJ xibiao p1；父32419顏綝，子32421顏騰之；birth0 death0。',interpretation='本次讀同作者顏君碑銘，並非冒稱已取得西表原版p1；父子及本人官足以身份橋，無獨立日期。')])
    results[9218].update(review_status='needs_further_research',source_identities_all_adjudicated=False,component_identities=[dict(identity='上黨河內馮鴦降晉趙燕軍將',keys=[k for k in results[9218]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=None),dict(identity='CBDB434412馮鴦陝西通志原段未取',keys=['cbdb:434412'],historical_birth=None,historical_death=None)])
    results[9218]['evidence'].append(dict(citation='原CBDB434412來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='無生卒、無KIN；source66734 ctext6287539、68942 Q65893061、6267陝西通志section51478。',interpretation='三史本人段已讀，地方志原段未取，不能同名直接套整條日期。'))
    results[9221].update(candidate_duplicate_of=7842,identity_status='duhong_zhangju_qin_conquest_retelling_subject_clear_chronology_variant_target')
    results[9221]['unresolved'].append('SL09杜洪張琚關中為秦先帝所滅回述，對7842早期軍閥支可候選；不與8128涼瓘弟混。SL04杜洪/張琚死法與TJ互換，先保來源別不新增整條硬刪。')
    results[7842].setdefault('source_reported_chronologies',[]).append(dict(source='明重編十六國春秋SL04開篇皇始二352',statement='司馬勳偽請琚於坐殺之；杜洪自立秦王、五月苻健斬杜洪於宜秋',source_hierarchy='late_reconstruction_not_independent_early_counterevidence',interpretation='與獨立TJ張琚為秦王且352五月健斬之顯有杜洪/張琚互換，保晚源異說；不冒稱兩源全無衝突。'))
    results[7842]['evidence'].append(chunk_ev('sl_04','勲乃伪请琚于坐杀之','SL04杜洪張琚名字與死因異說回檢','晚出重編本姓名位置與TJ明不合，源層級已限定；不能把這句當獨立早源證352健斬張琚。'))
    results[9222].update(review_status='duplicate',duplicate_of=9223,identity_status='xu_bin_yun_same_jia_yong_sima_raid_and_qin_return_incident_variant')
    results[9222]['unresolved'].append('徐斌/徐贇同雲中護軍賈雍司馬，360四月襲衛辰、堅怒黜雍歸所獲同整案→9223；字形源別保留，無生卒。')
    for ident,note in {9201:'苻叡/睿堅子巨鹿公，384華澤戰死獨立TJ可證；出生無明文。',9202:'苻敞登父健世太尉司馬陇東守，生所殺但未具死年，355–357政權範圍不硬補單年；堅追贈不是敞仍活。',9203:'苻暉堅第二子平原公、385三月愤恚自殺，三key同本人，無明出生。',9205:'飛龍357擒姚蘭及383歲末丕遣垂副將被殺同秦將，不把父生年或淝水亡秦兵當其死，死亡年明383不是384。',9208:'衛綝/𬘭西平反張瓘兩兄弟互在對方同原案三key閉；兵敗未云死，無生卒。',9209:'賈雍丞相司馬雲中護軍三key同，360襲田後被黜白衣領職非死，無生卒。',9212:'郭勳/勋西平術者不應州郡，受衛綝聘同二key；相術預言不是本人死年。',9213:'郭氏重華夫人玄靓生母，JS/TJ稱庶母（相對正嫡）非應造另一後母；363謀誅天錫敗明張欽等死，不能自動算郭氏死，無生卒。',9214:'慧持慧遠弟，412卒76，生337依虛歲回算有明算法；不把18出家當18歲即武將初仕。',9215:'道立安公弟子隱覆舟山，秋卒但沒有年；後文曇戒70、法曠40276不套本人。',9216:'陳祐冠軍將軍洛陽守、與沈勁/戴施/鄧遐同軍案全3key閉；365棄城逃非被殺，沈勁被殺不是祐卒，生卒未知。',9219:'閼頭/閼陋頭同劉務桓弟、悉勿祈叔，被逐後奔代未明死；四key閉，不以冰陷就當本人溺死。',9220:'孫希360並州刺史同任命兩key，SL泳之子 vsTJ泳之弟真族系源別，保兩說不靜改；未因此造兩個同官同任命人物，生卒未知。',9223:'徐贇雲中賈雍司馬襲田案，與SL徐斌字異同人，無生卒。',9224:'慕輿干武衛與根謀篡單SL，後明根及妻子誅未明具干名死，不補360卒。',9225:'文盧蛮賊為滕畯討破傳首可知被殺，但JS置桓温356回軍後汇叙、升平改封前，不足本人明死356；不把林邑王文於盧容的拆詞當此文盧，不鎖卒年。'}.items():results[ident]['unresolved'].append(note)
    round49b_reads={9226:[('sl_03','弥从子立嶷孙岩于中山'),('tj_17','弥从子立、嶷孙岩于山中')],9227:[('js_01','桓温遣朱序讨勋'),('js_13','朱序，字次伦，义阳人也'),('sl_03','温前锋朱序邓遐'),('tj_18','朱序为征讨都护'),('js_07','楚与朱序讨平之'),('js_10','会朱序自表去职'),('sl_05','安与朱序俱获送于坚'),('sl_06','豫州刺史朱序自河阴北济'),('gs_01','朱序西镇复请还襄阳')],9228:[('js_07','广汉妖贼李弘并聚众为寇'),('js_18','讨妖贼李弘'),('tj_18','广汉妖贼李弘')],9229:[('sl_09','杜洪张琚因赵氏成资')],9230:[('sl_03','建威将军梁成'),('sl_04','梁成[梁平老之子也]'),('tj_18','建威将军梁成')],9231:[('js_15','苟苌、毛当、梁熙、姚苌来寇'),('sl_03','猛留毛当戍晋阳'),('sl_04','猛留将军武都毛当戍晋阳'),('sl_05','毛当击翟斌为斌所败当死之'),('tj_18','猛留将军武都毛当戍晋阳')],9232:[('sl_03','弥从子立嶷孙岩'),('tj_17','弥从子立、嶷孙岩')],9233:[('sl_05','王綂擢之子也'),('wsh_02','苻雅率将王统')],9234:[('sl_06','张豺之辅石世也'),('tj_16','虎嬖之，生齐公世')],9235:[('sl_04','秦州刺史西县侯雅'),('sl_05','苻雅坚之踈族也'),('tj_18','以左卫将军苻雅为秦州刺史'),('wsh_02','苻坚遣苻雅率将'),('yi_01','苻坚遣杨安、苻雅等讨纂')],9236:[('sl_03','索头单于薛云举众来降')],9237:[('js_17','惔官至南康太守，早卒')],9238:[('sl_05','求救于振威刘库仁'),('wsh_00','南部大人刘库仁走云中'),('wsh_01','刘库仁，本字没根')],9239:[('js_15','郭增、刘肃二人，并年十八九'),('sl_09','郭増刘肃二人并年十八九'),('tj_18','敦煌刘肃谓天锡曰')],9240:[('sl_04','匈奴左贤王刘卫辰遣使来降'),('sl_05','匈奴曹毂刘卫辰帅众'),('tj_18','刘卫辰遣使降秦'),('wsh_00','悉勿祈死，弟卫辰立'),('wsh_02','卫辰，务桓之第三子也'),('sl_08','卫辰务桓之第三子也')],9241:[('js_18','广州刺史胜含率众伐之')],9242:[('tj_18','臣子林宗幼弱')],9243:[('sz_01','其王帛纯曰'),('sz_01','光遂破龟兹杀纯获什')],9244:[('js_13','严与丹阳尹庾和议曰')],9245:[('sz_00','智贤，本姓赵，常山人也')],9246:[('sl_06','沙门智通固谏曰')],9247:[('sz_00','国沙门昙摩羯多为立戒坛')],9248:[('js_18','以封其次子济'),('tj_18','济为临贺县公')],9249:[('sl_09','安定梁景炖煌赵肃'),('js_15','安定梁景、敦煌刘肃')],9250:[('sl_03','梁琛广平人也'),('sl_04','给事黄门侍郎梁琛相继来聘'),('tj_18','护参军广平梁琛为中书著作郎')]} 
    for ident,qs in round49b_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=ident not in [9228,9233],identity_status='all_source_key_subjects_individually_adjudicated_round49b' if ident not in [9228,9233] else 'source_scopes_not_wholly_unified',last_manual_review_batch='round49b')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第49批燕秦將臣及僧尼逐源實讀','逐人讀本人親族官地、同案上下文；未把被捕、他人卒齡或回述追贈當本人生卒。'))
    z393=chunk_ev('js_13','太元十八年卒，赠左将军','晉書朱序本傳393卒','完整本傳實讀，義陽父朱燾、母韓築城、陷秦逃宜陽、淝水返晉、豫州洛陽→老病去任至太元十八卒明393，沒有本人卒齡生年。')
    results[9227].update(review_status='primary_text_corroborated',historical_death=date(393,z393));results[9227]['evidence'].append(z393)
    results[9228].update(review_status='needs_further_research',source_identities_all_adjudicated=False,component_identities=[dict(identity='周訪傳太和廣漢李弘詐李勢子與李金銀等',keys=[k for k in results[9228]['keys'] if k.startswith('js:js_07:') or k.startswith('tj:')],historical_birth=None,historical_death=None),dict(identity='桓溫載記355/356後升平改封前妖李弘為劉岵胡驥討破傳首',keys=[k for k in results[9228]['keys'] if k.startswith('js:js_18:')],historical_birth=None,historical_death=None)])
    results[9228]['unresolved'].append('JS07/TJ18太和5年370廣漢李弘詐李勢子，与JS18桓温北伐回軍、升平改封前劉岵胡驥傳首之李弘未有同一役橋；可能傳記移叙亦可能同名，先分源scope不單鎖死亡年。')
    for ident,target in [(9229,7846),(9221,7842)]:
        results[ident].update(review_status='duplicate',duplicate_of=target,source_identities_all_adjudicated=True,identity_status='same_early_guanzhong_duhong_zhangju_pair_distinct_from_liang_officials')
        results[ident].pop('candidate_duplicate_of',None)
        results[ident]['unresolved'].append('目標舊審已全鍵閉合且明保杜洪張琚死法互換源別；使者回述同關中兩人，身份去重不等於抹去死亡方式異說。先前待確認候選說明已由此閉合覆蓋。')
    l383=evidence(SOURCE/'_cache/texts/资治通鉴.txt','击成，大破之，斩成及弋阳太守王咏','資治通鑑太元八383十一月洛澗','實讀本年十月融克壽陽、十一月牢之精兵5000渡洛澗，具名衛將軍梁成被斬；不是淝水苻融死亡同日推套。')
    results[9230].update(review_status='primary_text_corroborated',historical_death=date(383,l383,month=11));results[9230]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only');results[9230]['evidence'].append(l383)
    mdie=evidence(SOURCE/'_cache/texts/资治通鉴.txt','大败秦兵，斩毛当','資治通鑑太元八383十二月','實讀十二月苻飛龍被殺後、384正月標前，毛當擊翟斌而慕容鳳斬之；無明本人死亡日，不套飛龍壬午。')
    results[9231].update(review_status='primary_text_corroborated',historical_death=date(383,mdie,month=12));results[9231]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only');results[9231]['evidence'].append(mdie)
    results[9233].update(review_status='needs_further_research',source_identities_all_adjudicated=False,component_identities=[dict(identity='秦王擢子統秦益州等刺史兄弟廣，姚興殺',keys=[k for k in results[9233]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=None),dict(identity='CBDB507547原直隸秦州新志本人未取',keys=['cbdb:507547'],historical_birth=None,historical_death=None)])
    results[9233]['evidence'].append(dict(citation='原CBDB507547王統來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='無生卒、無KIN；唯一source38333直隸秦州新志section5190525。',interpretation='原志本人句尚未取，不能以二史王統身份直接覆蓋未驗庫支；SL姚苌病姚興殺本身不具原年，生卒未知。'))
    shage=chunk_ev('tj_16','今世方十岁，比其二十','資治通鑑永和四348秋九月後立太子議','本年明348，劉曜安定公主為母、父石虎、齊公世，十歲為立儲時年齡，不是349被殺時十歲；虛歲推生339。')
    shdie=chunk_ev('tj_17','封世为谯王，废刘氏为太妃，寻皆杀之','資治通鑑永和五349五月','石遵庚寅入鄴辛卯封世，尋皆殺世及母；僅保349年，不把辛卯封王日當本人死日。原CBDB363卒錯置。')
    results[9234].update(review_status='primary_text_corroborated',historical_birth=date(339,shage,age=10),historical_death=date(349,shdie))
    results[9234]['evidence'].extend([shage,shdie,dict(citation='原CBDB31363石世父系及日期校核',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='birth0 death363；父31362石虎；notes Morihashi4p844；ctext945886、Q1152762、Shi Shi(emperor)等。',interpretation='原庫父石虎、帝名原Wiki標題與齊公世身份可橋。363不是獨立死亡史證，實讀早源明349已殺，撤363。')])
    k384=evidence(SOURCE/'_cache/texts/资治通鉴.txt','因作乱，夜，攻库仁，杀之','資治通鑑太元九384劉庫仁繁畤被殺','實讀甲申384年文常三郡兵亂，公孫希既破平規、欲救苻丕同魏書本傳；不是其弟眷或子顯被害。只保年不套前段甲子。')
    results[9238].update(review_status='primary_text_corroborated',historical_death=date(384,k384));results[9238]['evidence'].append(k384)
    w391=evidence(SOURCE/'_cache/texts/资治通鉴.txt','卫辰为其部下所杀','資治通鑑太元十六391十一月','實讀391辛卯年、十一月已卯出兵壬午破直力鞮、辛卯入悦跋壬辰分追、衛辰部下殺，後才十二月誅宗黨。壬辰是分軍追兵日，不當本人死亡日；保月。')
    results[9240].update(review_status='primary_text_corroborated',historical_death=date(391,w391,month=11));results[9240]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only');results[9240]['evidence'].append(w391)
    results[9210].update(review_status='duplicate',duplicate_of=9240,source_identities_all_adjudicated=True,identity_status='songshu_helian_weichen_same_weichen_bobo_father_jia_yong_incident_all_six_target_keys_closed');results[9210].pop('candidate_duplicate_of',None)
    results[9210]['unresolved'].append('9240六鍵現逐源閉合，宋書父子佛佛與魏書屈孑本名勃勃、同賈雍還田案可整人歸9240；先前等待目標說明已被本次完成覆蓋，姓源異保留。')
    results[9239]['source_reported_age_at_activity']={'age_alternatives':[18,19],'calendar_year':361,'source':'晉書劉肅郭增並年十八九；通鑑劉肅未二十','conditional_birth_years':[343,344],'calculation':'361-18+1=344 或361-19+1=343；約數不可精確生日'}
    for ident,note in {9226:'曹巖/岩曹嶷孫鞠殷招撫同二key，祖嶷死不是孫死；生卒未知。',9227:'朱序九key父燾母韓襄陽淝水返晉同鏈，393卒獨立JS明；沒有確生年，不能把參軍年份減16自稱史實。',9230:'梁成平老子建威將軍平燕、秦荊州梁成三key閉，383十一月洛澗斬。',9231:'毛當武都秦將370戍晉陽/376伐涼/383晖將同軍鏈，SL04記十一月辛亥 vsTJ/SL03十月源異不套征討月到死；獨立死亡在383十二月。',9232:'王立王彌從子鞠殷招撫兩key同，不與諸多漢晉王立只同名合，生卒未知。',9234:'石世父虎母安定公主劉氏張豺扶立三key閉；348十歲→339虛歲生，349被遵殺，CBDB363卒已校撤。',9235:'苻雅堅疏族西縣侯秦州刺史討仇池五key同，纂被殺非雅死，未知生卒。',9236:'薛雲索頭單于與王騰356同降燕，單SL無生卒，不把高句麗或其他拓跋單于合入。',9237:'何惔准子南康守早卒，父准47卒與357追贈不能套子，無本人年。',9238:'劉庫仁沒根洛垂母平文女、代南大人奉拓跋珪秦振威同三key閉，384被慕容文常殺；魏書本傳不具月，不強補日。',9239:'劉肅/張大誠，SL09後段作趙肅張大城，與JS劉肅張大誠源名/賜名字、改子與本人子之語源異保存，不按兩支各造不同人；18九只能推343/344條件範圍，未知精生卒。',9240:'衛辰務桓三子六源同，391十一月部下殺有獨立TJ，非376亡代就其死；子勃勃改赫連係父親宋書追稱赫連衛臣源別。',9241:'勝含升平末廣州刺史伐林邑，與盟還非死亡；佛是林邑王，不拆成漢姓範佛當勝含族，無生卒。',9242:'宋林宗混子幼弱未繼父而叔澄繼，混卒不是子卒，無生卒。',9243:'帛純龜茲王為呂光殺有明文，但本段只383遣兵而未具抵達殺王年；不把出發年當死年，先保卒年未知待獨立紀年。',9244:'庾和丹陽尹361與孔嚴議哀帝繼統，孔嚴字彭祖非和字，無和生卒。',9245:'智賢常山趙氏珍女，杜太守逼傷20餘而蘇不是死亡；原文明晉太和中70餘不是北魏太和；未具具體年或精齡，生卒空。',9246:'智通姚襄僧諫三原戰，惑眾者斬是姚襄喝阻命令，不記智通確已被殺，不能自填357死。',9247:'曇摩羯多357二月八洛陽泗水授具戒為活動日，不是生卒，淨檢70及升平末飛昇非羯多本人。',9248:'桓濟温次子臨賀縣公兩key閉，流徙長沙不等被殺，無生卒。',9249:'梁景與劉/趙肅總角友同兩key，JS賜本人字大奕為己子 vsSL改景子為己子真句法源異存，不自造本人子父兩武將；生卒未知。',9250:'梁琛呂護參軍→燕中書著作給事使秦→燕獄→秦猛主簿全三key閉，SL完整本傳已讀到底；沒有卒年，繫獄後明獲釋不能算被殺。'}.items():results[ident]['unresolved'].append(note)
    round50a_reads={9251:[('sl_03','秦遣黄门郎石越来聘'),('sl_04','坚复遣黄门郎石越聘燕'),('sl_05','丕遣石越率精骑讨农'),('tj_18','秦遣黄门郎石越聘于燕')],9252:[('sl_04','母茍氏为皇太后妃茍氏为皇后'),('tj_18','母苟氏为皇太后，妃苟氏为皇后')],9253:[('sl_05','以登兄同成嗣')],9254:[('sl_04','世子宏为皇太子'),('sl_05','苻宏坚长子也'),('tj_18','世子宏为皇太子')],9255:[('tj_18','敷为清河公'),('sl_05','敷为清河公')],9256:[('sl_04','熈为广平公'),('tj_18','熙为广平公')],9257:[('tj_18','睿为巨鹿公')],9259:[('tj_18','封其子阳为东海公'),('sl_05','封其子阳为东海公')],9260:[('sl_04','封生子馗为越王')],9262:[('sl_03','匈奴单于贺赖头'),('tj_18','匈奴单于贺赖头')],9263:[('js_15','赵白驹及肃二人'),('sl_09','赵白驹及肃二人'),('tj_18','赵白驹与肃二人')],9264:[('sl_09','安定梁景炖煌赵肃')],9265:[('js_15','郭增、刘肃二人，并年十八九'),('sl_09','郭増刘肃二人并年十八九')],9266:[('sz_00','晋沙门释道场')],9267:[('sl_03','庾希部将何谦')],9268:[('sl_03','刘则战于檀邱')],9269:[('sl_03','征东叅军刘拔刺杀'),('tj_18','燕征东参军刘拔刺杀')],9270:[('sl_03','武邑刘贵上书'),('tj_18','武邑刘贵上书')],9271:[('sl_03','立贵妃可足浑氏为皇后'),('tj_18','以其妹长安君妻垂')],9272:[('sl_05','太原王楷陈留王绍讨之'),('sl_06','慕容楷垂兄太宰恪之长子也'),('tj_18','楷为积弩将军')],9274:[('js_11','石秀，幼有令名')],9275:[('js_15','苟苌、毛当、梁熙、姚苌来寇'),('sl_05','武威太守彭济执熙迎光')]} 
    for ident,qs in round50a_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=ident not in [9271,9272],identity_status='all_source_key_subjects_individually_adjudicated_round50a' if ident not in [9271,9272] else 'source_scopes_not_wholly_unified',last_manual_review_batch='round50a')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第50批秦燕宗室與晉將逐源實讀','實讀本人親族封爵及同案，區分妻母、叔姪、本人與子以及先殺後追封，不由相鄰年代強推生卒。'))
    y384=evidence(SOURCE/'_cache/texts/资治通鉴.txt','大败秦兵，斩越，送首于垂','資治通鑑太元九384正月石越討慕容農','實讀384正月乙酉朔後、燕垂庚戌至鄴前，石越被農軍斬首，與四源同遣將；不套毛當383十二月死亡，也不套庚戌到鄴日為死日。')
    results[9251].update(review_status='primary_text_corroborated',historical_death=date(384,y384,month=1));results[9251]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only');results[9251]['evidence'].append(y384)
    hong405=evidence(SOURCE/'_cache/texts/资治通鉴.txt','桓玄馀党桓亮、苻宏等拥众寇乱郡县者以十数','資治通鑑義熙元405五月餘黨平定','實讀本年405五月秀軌之伏誅後，毅道規檀祗分兵討滅桓亮苻宏等，與SL完整宏傳義熙初謀叛伏誅相合；保405年，不以五月群黨總敘冒稱本人確日。')
    results[9254].update(review_status='primary_text_corroborated',historical_death=date(405,hong405));results[9254]['evidence'].append(hong405)
    for ident,target in [(9257,9201),(9264,9239)]:results[ident].update(review_status='duplicate',duplicate_of=target,identity_status='unique_parent_and_title_or_paired_liangjing_tianxi_adoption_name_variant')
    results[9259]['late_reconstruction_statement']={'source':'明重編SL05苻法傳','statement':'陽為父復仇未殺，徙高昌再徙鄯善，劫相後鄯善王殺之；未具年','historical_death_year':None}
    for ident,cb,n,d,note in [(9261,195402,'裴和',422,'父195403子195401；source32033/71853頁161516；cf補遺7:138-139文物1999.1:93洛陽新獲續504'),(9273,194407,'李勁',423,'子175133、175206；source32033/71853頁160376；同墓誌索引引文')]:
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,identity_status='cbdb_original_inscription_not_retrieved_tackett_estimate_not_historical_date',last_manual_review_batch='round50a_cbdb')
        results[ident]['evidence'].append(dict(citation=f'原CBDB{cb}{n}原注及族系',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=f'Death year estimated by Tackett(cc{d})；{note}',interpretation=f'{d}為原庫明示估算，撤頂層確卒年；原墓誌未取不能用抽取的官職/出生地充當本人已核。'))
    results[9271].update(review_status='identity_conflict',source_identities_all_adjudicated=False,component_identities=[dict(identity='慕容暐皇后可足渾翼女，太后從姪女',keys=[k for k in results[9271]['keys'] if k.startswith('sl:')],candidate_duplicate_of=9454,historical_birth=None,historical_death=None),dict(identity='長安君，慕容儁后可足渾氏妹、垂妻',keys=[k for k in results[9271]['keys'] if k.startswith('tj:')],historical_birth=None,historical_death=None)])
    results[9271]['unresolved'].append('逐key候選本人明範圍：SL03:233是369立暐后翼女，TJ18:99是太后妹長安君妻垂。兩代不同配偶真混，不因同卷另一后便泛判；SL分支候9454、不能整條→9825或9454。')
    results[9272].update(review_status='needs_further_research',source_identities_all_adjudicated=False,component_identities=[dict(identity='恪長子楷秦積弩燕太原王',keys=[k for k in results[9272]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=None),dict(identity='CBDB402980重修天津府志本人原引尚未取得',keys=['cbdb:402980'],historical_birth=None,historical_death=None)])
    results[9272]['evidence'].append(dict(citation='原CBDB402980慕容楷來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='無生卒、無KIN；source38833重修天津府志section90888、68942 Q65887104。',interpretation='三史本人身份清楚，SL具建興十年六月卒為晚出本記載；原志本人段尚未取，不直接將晚源392六月卒推整條確年。'))
    results[9272]['late_reconstruction_statement']={'source':'明重編SL06楷本傳','statement':'以建興十年六月卒諡元子奇嗣','conditional_death_year':392,'month':6}
    gx=dict(citation='甘肅通志四庫全書本卷27職官晉至魏名錄',source_url='https://zh.wikisource.org/wiki/甘肅通志_(四庫全書本)/卷27',excerpt='任蘭〈金城太守〉 梁熈〈涼州刺史〉 魏劉藻〈秦州刺史〉 費穆〈涇州長史〉',interpretation='實取公開原書并讀晉/魏分界職官，本名熈與CBDB409092完全一致，涼州刺史時間範圍及官地與前秦梁熙同。CBDB具體section版本未對頁，這是實際已讀公開本官名橋，不冒稱已取得section5190180的原版頁。')
    results[9275]['evidence'].extend([gx,dict(citation='原CBDB409092梁熈來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='原名梁熈無生卒無KIN；source38651甘肅通志section5190180及Q65907200。',interpretation='已獨立實讀該書公開版職官同名同官同朝代範圍，身份可橋；日期取早期通鑑不取庫欄。')])
    x385=evidence(SOURCE/'_cache/texts/资治通鉴.txt','武威太守彭济执熙以降，光杀之','資治通鑑太元十385梁熙被呂光殺','實讀385乙酉年八月苻堅死後、乞伏國仁建義前涼州役，楊翰勸拒光、胤酒泉安彌敗、彭濟執父全鏈；保年度，不以同年前段八月辛丑堅死日套熙。')
    results[9275].update(review_status='primary_text_corroborated',historical_death=date(385,x385),source_identities_all_adjudicated=True,identity_status='three_keys_closed_including_cbdb_same_name_office_dynastic_context_gansu_original');results[9275]['evidence'].append(x385)
    results[9265]['source_reported_age_at_activity']={'age_alternatives':[18,19],'calendar_year':361,'source':'晉書及SL郭增劉肅並年十八九','conditional_birth_years':[343,344]}
    results[9274]['source_reported_age_at_death']={'age':43,'calendar_year':None,'source':'晉書桓石秀本傳'}
    for ident,note in {9251:'石越始平秦黃門使燕、後軍將至384正月為農斬四key閉，無明出生。',9252:'母苟太后與妃苟后同句分明兩人，兩key只妻妃后支，不能套母375卒；本人生卒未知。',9253:'苻同成敞子登兄襲爵、毛興司馬與太尉，同單SL；敞被生殺不是同成死，無生卒。',9254:'苻宏堅長子太子三源同，385帶母出奔/為桓玄前鋒至405餘黨被討滅，不能把父385死套子；405卒明有獨立TJ群體討滅與本傳伏誅橋。',9255:'苻敷法子清河公兩key同，法357賜死不是敷死，無生卒。',9256:'苻熙堅子廣平公兩key同，後雍州牧與其他同姓字熙不自動混；無生卒。',9257:'睿/叡堅子巨鹿公同→9201，來源字形保留，384死亡僅在規範位。',9259:'苻陽法子東海公兩key同，賜死父不是其死；晚SL鄯善王殺無明年，生卒年均不詳。',9260:'苻馗苻生子越王單SL，父生357被殺與23歲非馗年，未知生卒。',9262:'賀賴頭單于35000口降燕兩key同，35000人口非齡；無生卒。',9263:'趙白駒劉肅共斫張邕同三key，斫不中後邕自刎非白駒死，無生卒。',9264:'趙肅SL後段與梁景總角/賜姓張及大城同JS劉肅獨案→9239，源異姓字及改本人字或改其子差異保留，不新造角色。',9265:'郭增劉肅同年18九勸除邕兩key同；年齡約數不選單生年，生卒未知。',9266:'釋道場為357二月八受戒提出戒經異議，是活動非生卒日，淨檢70與後昇天非本人，無生卒。',9267:'何謙庾希將檀丘敗燕劉則單SL，是否後北府何謙同人尚需其key橋，兵勝非卒，未知生卒。',9268:'劉則燕將檀丘敗何謙，敗還非死，單SL未知生卒。',9269:'劉拔刺慕容友兩key閉，SL七月vsTJ六月甲戌真月源異；被殺是友不拔，本人生卒未知。',9270:'劉貴武邑諫150萬兵兩key同，十三事不是十三歲，沒有本人生卒。',9274:'桓石秀43卒家無明年，在州5年不是卒年紀年；不把父豁卒或叔冲年套本人。',9275:'梁熙/熈前秦涼州刺史三key閉含原志官地朝代橋，385彭濟執交呂光殺獨立早源，無生年。'}.items():results[ident]['unresolved'].append(note)
    round50b_reads={9276:[('sl_03','将军叚崇收军北渡'),('tj_18','燕将段崇收军北渡')],9277:[('sl_05','索泮字德林炖煌人也')],9278:[('sl_05','其弟菱有隽才')],9279:[('sl_03','苻坚遣将军茍池'),('sl_04','遣将军茍池洛州刺史邓羌'),('tj_18','遣将军苟池、洛州刺史邓羌')],9281:[('sl_03','冠军陈佑守兵不过二千')],9282:[('sl_04','以京兆韦锺为魏郡太守'),('tj_18','坚以京兆韦钟为魏郡太守')],9284:[('js_07','子仲孙，兴宁初督宁州军事')],9285:[('js_17','遣使者孟公明持节')],9287:[('sl_09','张颀仕天锡为西域校尉')],9288:[('sl_03','冲为中山王'),('sl_04','燕故中山王慕容冲'),('sl_05','慕容冲遣伪尚书令高盖'),('sl_06','俄而冲为段木延所杀'),('tj_18','冲为中山王')],9289:[('tj_15','监军部将李根请筑白石垒'),('js_09','李根')],9290:[('js_15','司马纶骞奉章请命'),('tj_18','司马纶骞奉章诣建康请命')],9291:[('sl_03','陈留太守袁披'),('tj_18','陈留太守袁披')],9292:[('sl_03','塞北七国贺兰涉勒')],9293:[('js_18','使侍中颜旄宣旨')],9294:[('sl_04','平羗护军高离据略阳叛'),('tj_18','秦平羌护军高离据略阳叛')],9296:[('sl_03','司马悦希军于盟津'),('tj_18','司马悦希军于盟津')],9297:[('sl_03','侍中慕舆龙诣龙城'),('tj_18','燕侍中慕龙舆诣龙城')],9298:[('js_18','其将朱辅立其子瑾'),('sl_03','陈郡太守朱辅婴城固守'),('tj_18','陈郡太守朱辅退保彭城'),('wsh_02','陆城戍将陈郡太守朱辅')],9299:[('sl_04','鲜卑没奕于各帅众数万')],9300:[('tj_18','鲜卑没弈干各帅众数万')]} 
    for ident,qs in round50b_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=ident not in [9279,9289],identity_status='all_source_key_subjects_individually_adjudicated_round50b' if ident not in [9279,9289] else 'cbdb_branch_not_bridged_to_history_keys',last_manual_review_batch='round50b')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第50批涼秦燕晉人物逐源實讀','本人官地親屬及案件逐源讀核；同名別朝、匿名族系、傳記叙次與本人卒年區分，未確年月不補。'))
    pan385=evidence(SOURCE/'_cache/texts/资治通鉴.txt','光杀泮及皓','資治通鑑太元十385呂光破涼州','實讀西郡太守索泮、酒泉宋皓城守不降，光執讓泮與梁公之仇同SL德林本傳獨事，具名殺泮可證385；SL弟菱同被害未有此早源具名，不擴套弟。')
    results[9277].update(review_status='primary_text_corroborated',historical_death=date(385,pan385));results[9277]['evidence'].append(pan385)
    results[9278]['late_reconstruction_statement']={'source':'明重編SL05索泮傳','statement':'弟菱有隽才，伏波將軍典農都尉與泮俱被害','conditional_death_year':385,'limitation':'同役年份依兄獨立早證，但弟被害具名仍僅晚重編本，不提升頂層確年'}
    results[9279].update(review_status='needs_further_research',source_identities_all_adjudicated=False,component_identities=[dict(identity='秦苟池援燕伐涼至385驪山斬',keys=[k for k in results[9279]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=None),dict(identity='CBDB420146原秦州新志未取',keys=['cbdb:420146'],historical_birth=None,historical_death=None)])
    gc385=evidence(SOURCE/'_cache/texts/资治通鉴.txt','西燕将军慕容永斩苟池','資治通鑑太元十385三月後驪山戰','苟池左將軍俱石子右將軍同戰，具名被斬。只可掛已核三史支，CBDB未橋不得套整條；三月為前段晖自殺月未強推本人月。')
    results[9279]['evidence'].extend([gc385,dict(citation='原CBDB420146苟池來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='無生卒無KIN；source38333直隸秦州新志section5190525及Q65835882。',interpretation='原志本人段未取，名字命中不是獨立身份官系橋。')]);results[9279]['component_identities'][0]['historical_death']=date(385,gc385)
    chu=chunk_ev('js_17','褚爽，字弘茂，小字斯生','晉書外戚褚爽完整本傳','祖裒父歆，恭思后父，義興太守早卒，子秀之炎之喻之無確生卒；不是前段王爽字季明被誅同人。')
    results[9280].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='cbdb30918_chuxin_son_chuxiuzhi_father_primary_jin_bridge',last_manual_review_batch='round50b_cbdb_primary')
    results[9280]['evidence'].extend([chu,dict(citation='原CBDB30918褚爽族系原注',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='原無生卒；父30917褚歆、長子30919褚秀之等，notes RM1544。',interpretation='原庫父子三元組與晉書本人傳明相合，身份可閉；不把先人裒47或王爽被誅年套本人。')])
    results[9281].update(review_status='duplicate',duplicate_of=9216,identity_status='chen_you_same_champion_general_luoyang_shenjin_500_withdrawal_story_variant')
    vd385=evidence(SOURCE/'_cache/texts/资治通鉴.txt','谦以告钟，钟自杀','資治通鑑太元十385韋鍾自殺','已讀尚書韋鍾被冲執、子謙馮翊太守被邵安民責、告父自殺同本人，與370魏郡守/371青州守角色鏈可橋。保385年，不套前段三月晖死月為鍾死月。')
    results[9282].update(review_status='primary_text_corroborated',historical_death=date(385,vd385));results[9282]['evidence'].append(vd385)
    for ident,cb,detail in [(9286,183961,'父183960子183962，death431明Tackett估；source32033/71853頁145363'),(9286,183987,'父183986子183988，death424明Tackett估；唐研究6(2000)402-403；source32033/71853頁145390')]:
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,identity_status='two_anonymous_cbdb_genealogies_unbridged',last_manual_review_batch='round50b_cbdb')
        results[ident]['evidence'].append(dict(citation=f'原CBDB{cb}宋某匿名族系與估年限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=detail,interpretation='兩條匿名宋某父子編號不同，原墓誌未取不可只同姓某字合人，也不以估卒年異強判確定兩人，分支hold待原系譜。'))
    results[9286]['component_identities']=[dict(identity='CBDB183961宋某，父183960子183962',keys=['cbdb:183961'],historical_birth=None,historical_death=None),dict(identity='CBDB183987宋某，父183986子183988',keys=['cbdb:183987'],historical_birth=None,historical_death=None)]
    ch386=evidence(SOURCE/'_cache/texts/资治通鉴.txt','左将军韩延因众心不悦，攻冲，杀之','資治通鑑太元十一386二月西燕慕容冲死','實讀本年386丙戌、正月拓跋珪立與二月王統援王廣、燕改建興後，冲長安不歸民怨為韓延殺。與SL盛傳段木延字異保源，不套盛13歲為冲13歲。')
    results[9288].update(review_status='primary_text_corroborated',historical_death=date(386,ch386,month=2));results[9288]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only');results[9288]['evidence'].append(ch386)
    results[9288]['late_reconstruction_statement']={'source':'明重編SL04苻堅載記','statement':'初堅之滅燕，冲年十三，有龍陽之姿','conditional_birth_year':358,'calculation':'370-13+1=358；僅此晚源，尚無已讀獨立早源支持，出生不供嚴格改動'}
    results[9289].update(review_status='needs_further_research',source_identities_all_adjudicated=False,component_identities=[dict(identity='陶侃蘇峻役監軍部將李根築白石壘',keys=[k for k in results[9289]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=None),dict(identity='CBDB175259父192791子175260墓誌支未取',keys=['cbdb:175259'],historical_birth=None,historical_death=None)])
    results[9289]['evidence'].append(dict(citation='原CBDB175259李根日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父192791子175260；death421，Death year estimated by Tackett(cc421)；source32033/71853頁136273。',interpretation='421估年非原文確年；與328陶侃部將未族系橋，不能整條套在世或生卒。'))
    m344=evidence(SOURCE/'_cache/texts/晋书.txt','太子即皇帝位，时年二岁','晉書穆帝紀建元二344九月','實讀聃字彭子康帝子、344九月己亥即位2歲；與361五月崩19歲互相驗證，343為虛歲推生，不是生日。')
    m361=evidence(SOURCE/'_cache/texts/晋书.txt','五月丁巳，帝崩于显阳殿，时年十九','晉書穆帝紀升平五361五月丁巳','已讀升平四至五完整年標、崩後哀帝嗣，穆帝死亡有陰曆月干支日；361-19+1=343，非原庫342。')
    results[9295].update(review_status='primary_text_corroborated',historical_birth=date(343,m361,age=19),historical_death=date(361,m361,month=5,day_ganzhi='丁巳'),duplicate_of=None,candidate_duplicate_of=12203,source_identities_all_adjudicated=True,identity_status='cbdb30907_explicit_emperor_mu_q7396_father_sima_yue_jin_primary_bridge',last_manual_review_batch='round50b_cbdb_primary')
    results[9295]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9295]['evidence'].extend([m344,m361,dict(citation='原CBDB30907司馬耽身份日期核對',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='notes Fifth emperor of Eastern Jin；父30906司馬岳；birth342death361；原Wiki Emperor Mu of Jin、Q7396、晉穆帝。',interpretation='不是平民司馬耽，明即司馬聃晉穆帝；原342生與兩條古齡推343不合，按已讀原紀推算記343，17key規範位12203待其全鍵閉合再整條去重。')])
    jf371=evidence(SOURCE/'_cache/texts/资治通鉴.txt','擒瑾及辅并其宗族，送建康斩之','資治通鑑咸安元371正月壽春破後','本年正月丁亥擒袁瑾朱輔、送京斬，與JS及WS陳郡守立袁瑾族系清楚；保371年，丁亥是破城擒獲日非處斬日。')
    results[9298].update(review_status='primary_text_corroborated',historical_death=date(371,jf371));results[9298]['evidence'].append(jf371)
    results[9300].update(review_status='duplicate',duplicate_of=9299,identity_status='same_xianbei_chief_wuhuan_joint_surrender_and_furong_relocation_event_name_variant')
    for ident,note in {9276:'段崇呂護餘軍北渡屯野王兩key閉，退兵非死；是否後燕尚書段崇同人需其角色橋，不只同名合。',9277:'索泮字德林敦煌，天錫武威守後秦別駕、西郡守同，385呂光殺有獨立TJ，不把文中父兄官年當生年。',9278:'索菱泮弟同役被害僅SL，早源未具菱名，留晚源記載與未知生卒。',9279:'苟池三史軍將369援燕至385被斬清楚，但CBDB地方志原引未取，日期只存三史分支。',9281:'陳佑/祐洛陽守與沈勁500人留守獨事→9216；逃新城/陸渾/崖塢源地差異保留，不因此造三人。',9282:'韋鍾/鐘/锺京兆魏郡守轉青州梁州尚書同兩key，385被執後自殺有獨立TJ，生不詳。',9284:'周仲孫父光與寧益州失守後光祿勳卒單JS，未記年；免官非當年已死。',9285:'孟公明天錫使郭瑀安車征同原段，郭瑀後387卒非本人，無生卒。',9287:'張頎西域校尉殺曲儉後白犬鬼祟暴卒單晚SL；天錫僭位年屬殺儉，後卒不可自動鎖363，生卒年未知。',9288:'慕容冲儁子中山王與西燕冲五key閉，386二月韓延/段木延殺源別；370十三歲僅晚SL未當獨立出生確證，358保條件推算於晚源欄。',9289:'李根兩史328白石壘同，CBDB421估卒原墓誌支未連，整條hold不因同名合史年。',9290:'綸騫司馬363送俞歸與奉表同兩key，玄靓死與天錫年18不是其生卒。',9291:'袁披晉陳留守363長平為燕攻兩key同，未說被殺，無生卒。',9292:'賀蘭涉勒塞北七國降燕單SL，359俊寝疾非首領死，生卒未知。',9293:'顏旄晉侍中宣召桓溫，無生卒，不逕改作顏髦或合顏氏族人。',9294:'高離秦平羌護軍略陽叛兩key同，未克而卒的是討將永安公侯非高離；討平不明本人斬，生卒未知。',9295:'司馬耽=聃晋穆帝，343虛歲推生361五月丁巳卒，候12203整條全鍵目標未閉不硬刪，誤配丹徒籍不由游戲當史實。',9296:'悅希恪司馬盟津同兩key，非悅明山茌救軍，只同姓不合；無生卒。',9297:'慕輿龍SL vsTJ慕龍輿字序異，同364侍中遷宗廟百官龍城至鄴獨事，可閉；封奕次年卒非其死。',9298:'朱輔陳郡守退彭城轉陸城、立袁瑾四key父子案閉，371送京斬；原所拜荊/交等州不同史官爵源別保，不從候選荊州強改通鑑交州。',9299:'沒奕于/沒弈干同360十月與烏桓獨孤眾降秦及苻融勸遷獨案，兩字名源別，不自動擴合後來勃勃岳父所有沒奕于記錄；本人生卒未知。',9300:'沒弈干TJ同SL沒奕于降秦勸遷全案→9299，不靠近名合；未知生卒。'}.items():results[ident]['unresolved'].append(note)
    results[9295]['appearance_upper_bound']=344
    round51a_reads={9301:[('sl_04','黄门侍郎安定程宪'),('tj_18','黄门侍郎安定程宪')],9302:[('sl_04','封弟忠为河南公'),('tj_18','封弟忠为河南公')],9304:[('sl_09','从事中郎张虑舆榇切谏')],9305:[('gs_00','晋哀帝时沙门慧力所立'),('gs_01','晋兴宁中沙门慧力启乞为寺')],9306:[('js_13','伊字叔夏，父景'),('js_18','温遣桓伊及弟子石虔'),('ssxy_01','子野桓伊小字也'),('ssxy_02','桓护军亦在坐【桓伊已见】'),('ssxy_03','桓伊能挽歌')],9307:[('js_11','赵弘、赵忆等逐太守桓淡')],9308:[('js_11','秘字穆子'),('js_13','领军将军桓秘等至'),('js_14','温弟秘亦有才气')],9309:[('js_11','豁使其参军桓罴讨之'),('tj_18','桓豁使督护桓罴攻南郑')],9310:[('sl_04','雍州秀才叚铿等对策上第')],9311:[('sl_04','镇军将军毛盛为前锋都督')],9312:[('js_18','温使督护竺瑶、矫阳之'),('sl_03','宁朔将军竺瑶奔于襄阳'),('tj_18','温遣督护竺瑶等御之'),('wsh_02','桓温遣督护竺瑶')],9313:[('sl_09','少府长史纪锡上疏')],9314:[('js_11','竟陵太守罗崇讨破之'),('sl_03','竟陵太守罗崇[一作宗]'),('tj_18','太守罗崇击破之'),('js_14','其二舅罗崇、罗友')],9315:[('js_09','与陈郡谢玄为桓温掾'),('js_13','玄字幼度'),('tj_18','玄，奕之子也'),('js_07','冠军将军谢玄亲临哭之'),('js_14','谢玄为兖州，辟从事'),('js_18','羯谓谢玄'),('sl_05','诣谢玄于彭城求降'),('gs_01','徐州刺史谢玄拒之'),('ssxy_00','字幼度镇西奕第三子也'),('ssxy_01','谢车骑在安西艰中'),('ssxy_02','谢车骑问谢公'),('ssxy_03','王江州夫人语谢遏')],9318:[('js_11','及友当伏诛'),('ssxy_03','玉防庾友小字')],9321:[('tj_18','太守桓澹走保新野')],9322:[('sl_04','坚遣牛恬聘魏'),('wsh_01','苻坚遣使牛恬朝贡')],9323:[('sl_04','坚谓博士王寔曰')],9324:[('sl_09','校书祭酒索商上书极諌'),('js_15','校书祭酒索商上疏极谏')],9325:[('js_18','帝遣侍中罗含以牛酒犒温'),('js_16','罗含，字君章，桂阳耒阳人也'),('ssxy_01','罗府君别传曰含字君章'),('ssxy_02','罗君章为桓宣武从事')]} 
    for ident,qs in round51a_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=ident!=9314,identity_status='all_source_key_subjects_individually_adjudicated_round51a' if ident!=9314 else 'luochong_jingling_general_and_xizuochi_uncle_scopes_not_directly_bridged',last_manual_review_batch='round51a')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第51批晉臣及僧人逐源核讀','實讀本人名族官地及原注，區分同卷別人、敘事年代、先人子孫與本人卒齡。'))
    cai=evidence(SOURCE/'_cache/texts/宋书.txt','祖系，抚军长史。父𬘭，司徒左西属','宋書蔡廓傳父蔡綝族系','蔡廓字子度濟陽考城，曾祖謨祖系父綝/𬘭司徒左西屬，原庫蔡綝父系子廓同三代；未明蔡綝生卒，仙遊是後裔族籍不能當本人原籍。')
    results[9303].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='cbdb12008_father_caixi_son_caikuo_three_generation_songshu_bridge',last_manual_review_batch='round51a_cbdb_primary')
    results[9303]['evidence'].extend([cai,dict(citation='原CBDB12008蔡綝族系及祖籍限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父12007蔡系、次子12009蔡廓，無生卒；注see ancestor Mu12002蔡睦；原祖先族史周家猷1.163-165及歐陽修蔡襄碑。',interpretation='原族史未取得，但獨立宋書具本人三代橋，身份可閉；不是把後宋仙遊蔡氏祖籍套東晉人物。')])
    results[9305]['unresolved'].append('慧皎本人已辨安世高後修瓦官寺年代懸謬，兩key均明慧力始立寺於晉哀帝/興寧。不可用被駁的安世高事件把慧力拉回漢代或憑壽百餘年推生卒；慧力自己無生卒。')
    m384=evidence(SOURCE/'_cache/texts/资治通鉴.txt','镇军将军毛盛等将吏数十人，苌皆礼而遣之','資治通鑑太元九384姚萇破楊璧','已實讀384年本段、七月前後順序，秦鎮軍毛盛被獲禮遣未殺，與SL同職同將鏈獨立早源；384作在世年不作卒。')
    results[9311]['evidence'].append(m384);results[9311]['last_attested_activity']=384;results[9311]['independent_activity_evidence']=[m384]
    results[9314].update(review_status='needs_further_research',source_identities_all_adjudicated=False,component_identities=[dict(identity='竟陵太守羅崇/宗與桓豁攻宛破燕塵',keys=[k for k in results[9314]['keys'] if not k.startswith('js:js_14:')],historical_birth=None,historical_death=None),dict(identity='習鑿齒舅羅崇荊州從事後襄陽都督',keys=[k for k in results[9314]['keys'] if k.startswith('js:js_14:')],historical_birth=None,historical_death=None)])
    results[9314]['unresolved'].append('同溫府荊州官羅崇很可能同人，但軍事三key未明習鑿齒舅/羅友兄弟，舅支未明竟陵守；本次未取直接轉官/族系橋，保兩scope待補，不自稱證實兩人，未知生卒。')
    xage=chunk_ev('js_13','十三年，卒于官，时年四十六','晉書謝玄本傳太元十三388卒46','實讀從幼度奕子、温掾、淝水至北伐退淮陰、移東陽、會稽病終全傳；388-46+1=343，虛歲推生不是明生日。')
    x388=evidence(SOURCE/'_cache/texts/资治通鉴.txt','春，正月，康乐献武公谢玄卒','資治通鑑太元十三388正月','實讀年標戊子公元388、正月謝玄卒，與JS爵諡會稽卒同本人，無日。')
    results[9315].update(review_status='primary_text_corroborated',historical_birth=date(343,xage,age=46),historical_death=date(388,x388,month=1),source_identities_all_adjudicated=True)
    results[9315]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9315]['evidence'].extend([xage,x388,dict(citation='原CBDB136120謝玄父系與本人傳引用',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父69869謝奕；birth343death388；source25572晉書卷79列傳49、ctext5998570、Q1285752、Xie Xuan。',interpretation='原書引用本人全傳實讀且父奕、爵諡弟兄共事13key各清，非仅读抽取生日。')])
    tang=evidence(SOURCE/'_cache/texts/魏书.txt','唐和，字稚起，晋昌冥安人也','魏書唐和完整本傳','實讀父繇扶李暠、兄契伊吾、451正平元詣闕、高宗太安濟州刺史、皇興卒67與子欽孟直。原CBDB父唐瑤與書繇字異，同子欽獨系可橋；未把皇興範圍擇單年。')
    results[9316].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='cbdb182573_tangyao_yao_father_tangqin_son_northernwei_tanghe_primary_bridge',last_manual_review_batch='round51a_cbdb_primary',last_attested_activity=451)
    results[9316]['evidence'].extend([tang,evidence(SOURCE/'_cache/texts/魏书.txt','正平元年，和诣阙','魏書唐和正平元451在世','451赴闕可校否原427估死，不推451為出生或第一個職位。'),dict(citation='原CBDB182573唐和日期族系限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父182350唐瑤、子182574唐欽；Death year estimated by Tackett(cc427)；source32033/71853頁143830、Q11418521、唐和(南北朝)。',interpretation='原427明估且正史451仍活，撤歷史確卒；父瑤/繇源別存，膠水/晉標籤不敵明本傳北魏晉昌人。')])
    results[9316]['source_reported_chronologies']=[dict(source='魏書唐和傳',death_year_range=[467,471],reign='皇興中',reported_age=67,conditional_birth_year_range=[401,405],calculation='皇興467–471年各減67加1；未知其中哪年，不選精生卒')]
    results[9316]['historical_identity_correction']={'polity':'北魏','origin':'晉昌冥安','source':'魏書唐和傳','replaces_unsubstantiated_game_polity':'晉','replaces_unsubstantiated_game_origin':'膠水'}
    results[9317].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,identity_status='cbdb182612_tangchuai_original_genealogy_not_retrieved',last_manual_review_batch='round51a_cbdb')
    results[9317]['evidence'].append(dict(citation='原CBDB182612唐揣族系及估年限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父182611唐諮、子182613唐儼；death427明Tackett估；source32033/71853頁143870。',interpretation='原墓誌尚未取，不因唐和同頁簇便當同人親屬；427非確卒，未知生卒。'))
    lj=evidence(SOURCE/'_cache/texts/魏书.txt','灵弟均，赵郡太守','魏書李靈傳弟李均及子璨','實讀均子璨字世顯為後段主，前文靈父勰；CBDB父173865李勰子174027李璨唯一同族可橋，沒有均生卒年，不把璨471卒40套父均。')
    results[9319].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='cbdb174026_lixie_son_lican_father_liling_brother_weishu_bridge',last_manual_review_batch='round51a_cbdb_primary')
    results[9319]['evidence'].extend([lj,dict(citation='原CBDB174026李均族系日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父173865李勰、子174027李璨；death427明Tackett估；source32033/71853頁135011。',interpretation='獨立魏書同族系可閉；427估算撤歷史確年，無明生卒。')])
    ling=evidence(SOURCE/'_cache/texts/魏书.txt','李灵，字虎符，赵郡人','魏書李靈完整本傳','父勰字小同，神䴥拜博士、從駕臨江、高宗傅、高宗即位後洛州刺史卒63、子恢及弟均子璨，父子同原庫；高宗即位452後任命才卒，452只在世下界不說任命精年。')
    results[9320].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='cbdb173866_lixie_son_lihui_father_weishu_liling_northernwei_bridge',last_manual_review_batch='round51a_cbdb_primary',last_attested_activity=452)
    results[9320]['evidence'].extend([ling,evidence(SOURCE/'_cache/texts/魏书.txt','高宗践祚，除平南将军、洛州刺史而卒','魏書李靈卒年前界與63歲','前文世祖神䴥征、高宗受經後即位452才拜洛州，故本人卒必不早452；不將紀傳轉職概括硬鎖卒年452或師傅年。'),dict(citation='原CBDB173866李靈日期族系限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父173865李勰、子173867李恢及173996李綜；Death year estimated by Tackett(cc427)；source32033/71853頁134851、Q18236212、李灵(北魏)。',interpretation='原427明估且與高宗即位後在世矛盾，史實卒年留不詳；不是前秦3世紀人物。')])
    results[9320]['source_reported_age_at_death']={'age':63,'calendar_year':None,'source':'魏書李靈傳','death_year_not_before':452}
    results[9320]['historical_identity_correction']={'polity':'北魏','origin':'趙郡','source':'魏書李靈傳'}
    results[9318]['source_reported_relationship_variants']={'daughter_in_law_father':[{'source':'晉書庾冰傳','name':'桓秘'},{'source':'世說新語賢媛及庾氏譜注','name':'桓豁'}],'interpretation':'同庾冰三子友、希弟友子婦請溫獲免案件，岳父記載源異不靜改；本人身份仍全2key明。'}
    results[9321].update(review_status='duplicate',duplicate_of=9307,identity_status='same_nanyang_taishou_zhao_yi_wan_mutiny_dan_character_variant')
    results[9325]['source_reported_age_at_death']={'age':77,'calendar_year':None,'source':'晉書羅含本傳'}
    results[7894].update(review_status='duplicate',duplicate_of=6542,source_identities_all_adjudicated=True,identity_status='eighteen_riders_unique_group_position_zhangye_zhangyepu_variant_confirmed_cross_shard')
    results[7894]['evidence'].append(chunk_ev('sl_01','刘宝张曀仆呼延莫','SL01十八騎張曀僕與WS張噎組位覆核','本次重讀完整十八騎名組，同序劉寶/實後呼延莫前唯一張；4000片代理已親讀WS01同組張噎並確認canonical6542，保各書名異不造兩人。'))
    results[7894]['unresolved'].append('4000片已全核6542早WS同名組位，7894唯一SL可整人歸6542；張噎/曀僕及鄰名異寫保源，不丟原讀證據。')
    for ident,note in {9301:'程憲安定黃門言商賈為卿同二key，諸公降爵非憲死，無生卒。',9302:'苻忠堅弟河南公同二key，不把苻雙改趙公或叛死套忠，無生卒。',9304:'張慮從事中郎舆榇切諫是自示決死不是實死，只有SL未知生卒。',9306:'桓伊叔夏景子，淮南太守江州刺史至護軍吹笛樂師五key閉，卒官無年；SS瓦官同坐王蒙劉惔屬傳聞稱官，未用其卒前界硬鎖伊護軍任年或推初仕。',9307:'桓淡/澹南陽守趙憶億逐奔新野同事→9321反向歸本位，逐走非殺。',9308:'桓秘穆子温弟、習鑿齒友、盧悚合殷康討三key閉，先桓冲卒只上界384不是明383卒；不按先卒填單年。',9309:'桓羆督護討勳同二key，勳兵敗被斬非羆死，生卒未知。',9310:'段鏗雍州秀才秦吏部郎單SL，對策上第非年齡，未知生卒。',9311:'毛盛秦鎮軍前鋒單SL身份已閉，384被姚苌禮遣獨立TJ在世年可校估死，無確生卒。',9312:'竺瑤洛陽逃襄陽轉督護破袁瑾四key軍鏈閉，逃走不是被殺，未知生卒。',9313:'紀錫少府長史諫民勞單SL，提秦氏覆國比喻非其本人國亡時死，未知生卒。',9315:'謝玄13key各已讀奕子幼度遏、温掾淝水至會稽，388正月卒46→343虛歲生；王珣父洽36歲、王文度等對語不套玄年。',9316:'唐和原估427卒與451詣闕明矛盾，魏書北魏晋昌父繇子欽完整身份橋；皇興467–471卒67保範圍與401–405條件生，不選單年。',9318:'庾友冰三子、希弟、東陽守及媳求情兩key閉；妻父秘/豁來源異保留，不宣稱因差親名即兩個友；獲赦非371已死。',9319:'李均北魏李靈弟、勰子璨父原庫及正史閉，無生卒；子璨471卒40非本人。',9320:'李靈虎符北魏高宗傅勰子恢父全原庫支身份閉，至少452高宗即位後仍活，63卒無明年，原427估卒撤歷史確年。',9322:'牛恬秦使代，燕鳳報使同二key，不把另牛恬漢人物近名合，無生卒。',9323:'王寔五經博士與苻堅太學對談單SL，不同王實北部大人僅同音，未知生卒。',9324:'索商蕩難校書祭酒諫遊園兩key同，非索襲隱者或索泮將，只同姓不合，未知生卒。',9325:'羅含君章桂陽耒陽父綏四key閉，侍中犒温369與地方官/別駕同官鏈，77卒無CE，不推定303或380。'}.items():results[ident]['unresolved'].append(note)
    round51b_reads={9326:[('ssxy_03','晋隆和中河东裴启撰'),('ssxy_01','裴郎作语林始出')],9327:[('sl_03','南阳督护赵亿叛据宛城'),('tj_18','南阳督护赵亿据宛城降燕')],9328:[('sl_03','镇律郎郭钦奏议')],9329:[('sl_04','镇东主簿南安姚眺谏'),('tj_18','镇东主簿南安姚眺谏')],9330:[('sl_04','陇西太守姜衡'),('tj_18','陇西太守姜衡')],9331:[('sl_09','游击将军张綂向白土'),('tj_18','游击将军张统向白土')],9332:[('sl_02','及沙门支法防共登罗浮山')],9333:[('jk_02','立陈留王世子恢为陈留王')],9337:[('sl_04','与天锡将杨遹战于枹罕东'),('sl_05','与天锡将杨遹战于枹罕之东'),('sl_09','使别驾杨遹为监前锋'),('tj_18','天锡遣杨遹逆战于枹罕东'),('js_15','以别驾杨遹为监前锋')],9338:[('sl_04','王抚守侯和'),('sl_09','王抚守侯和'),('tj_18','王抚守侯和')],9339:[('sl_03','真兄腆及从子奋覆皆仕于秦'),('tj_18','燕司空皇甫真兄腆及从子奋、覆皆仕秦')],9340:[('sl_03','真兄腆及从子奋覆皆仕于秦'),('tj_18','腆为散骑常侍')],9341:[('sl_03','贵兄常侍及奋覆兄弟'),('tj_18','贵兄常侍及奋、覆兄弟')],9342:[('sl_09','羌衡守白石')],9343:[('sl_02','与弟颕叔及沙门'),('js_17','与弟颖叔及沙门支法防')],9345:[('sl_04','南安太守南安邵羌'),('sl_09','猛使邵羌追敛岐'),('tj_18','南安太守南安邵羌')],9346:[('js_17','遣使者孟公明持节'),('sl_09','处士郭瑀瑀字符瑜'),('sl_11','昞年十四就博士郭瑀学')],9347:[('tj_18','西戎主簿冯翊郭辩')],9348:[('sl_04','富商赵掇丁妃邹瓮')],9349:[('sl_03','助教尚锋')],9350:[('sl_04','屠各张罔聚众数千')]} 
    for ident,qs in round51b_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=ident not in [9326,9330],identity_status='all_source_key_subjects_individually_adjudicated_round51b' if ident not in [9326,9330] else 'source_identity_or_cbdb_branch_not_fully_bridged',last_manual_review_batch='round51b')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第51批秦燕與山林人物逐源核讀','本人官地親族與共同事件逐源實讀；參與兵敗、被俘與旁人死不自動當本人死亡，未知年月空置。'))
    results[9326].update(review_status='needs_further_research',source_identities_all_adjudicated=False)
    results[9326]['source_reported_author_identity_variants']=[dict(source='續晉陽秋及SS輕詆',name='裴啟',role='隆和中語林作者'),dict(source='裴氏家傳及SS文學注',name='裴榮',style='榮期',father='裴稚',role='語林作者',editorial_comment='榮傥別名啟乎，原注存疑')]
    results[9326]['unresolved'].append('二key同語林作者，但一原家傳明裴榮榮期父稚，另一作裴啟；劉注問榮傥別名啟乎而未斷。不能把疑別名直接認定並套裴榮父系。保作者名源異，身份待別名考；未明生卒。')
    results[9330].update(review_status='needs_further_research',source_identities_all_adjudicated=False,component_identities=[dict(identity='前秦隴西守姜衡，王猛軍守白石',keys=[k for k in results[9330]['keys'] if not k.startswith('cbdb:')],historical_birth=None,historical_death=None),dict(identity='CBDB507552姜衡秦州新志原引未取',keys=['cbdb:507552'],historical_birth=None,historical_death=None)])
    results[9330]['evidence'].append(dict(citation='原CBDB507552姜衡來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='無生卒無KIN；唯一source38333直隸秦州新志section5190525。',interpretation='兩史本人官地相同，但庫原志未讀不只同名認同；生卒空。'))
    for ident,cb,name,page,detail in [(9334,194411,'李奉','160380','父194408；原注補遺7:138-139文物1999.1:93洛陽新獲續504'),(9335,175048,'李曾','136055','父194408，子175049與175060'),(9336,174196,'李系','135183','父194408，子164060與174991'),(9344,171004,'裴三虎','131874','子171005、171029、171044')]:
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,identity_status='cbdb_original_tackett_genealogical_passage_not_obtained',last_manual_review_batch='round51b_cbdb')
        results[ident]['evidence'].append(dict(citation=f'原CBDB{cb}{name}日期族系限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=f'Death year estimated by Tackett(cc428)；source32033/71853頁{page}；{detail}',interpretation='428明估非確卒；原譜系墓誌尚未取，不把同父/同頁簇當已讀身份史證，亦不與別朝同名直接合併。'))
    results[9342].update(candidate_duplicate_of=9330,identity_status='qiangheng_jiangheng_same_baishi_defense_event_branch_confirmed_target_unclosed_cbdb')
    results[9342]['unresolved'].append('羌衡SL09與姜衡SL04/TJ同王猛楊安軍守白石、王撫侯和、邵羌追斂岐唯一三分兵，可支級對9330；其庫支未閉先不整條硬刪。')
    guo387=evidence(SOURCE/'_cache/texts/资治通鉴.txt','瑀谏不听，出城大哭','資治通鑑太元十二387郭瑀不食卒','已讀387丁亥年標、王穆攻索嘏郭瑀覆被不食卒、吕光平酒泉至388標界，與JS完整本傳王穆長史還赤崖飲氣卒同一經過。JS無齡，不擇前郭荷84為瑀齡。')
    results[9346].update(review_status='primary_text_corroborated',historical_death=date(387,guo387));results[9346]['evidence'].append(guo387)
    for ident,note in {9327:'趙億南陽督護366宛叛逐桓澹、367敗走兩key閉；是否JS趙憶同字尚需對方全key橋，不以逃亡作死年。',9328:'郭欽燕鎮律郎議木德單SL，與西晉主簿郭欽等同名未族系橋勿合，無生卒。',9329:'姚眺南安鎮東主簿諫苻廋兩key閉，廋被殺不是眺死；後汲郡守不提供本人卒年。',9331:'張統涼遊擊白土軍兩key閉，與早高句麗張統不同角色時代，不自動同名去重，無生卒。',9332:'支法防袁宏兄弟遊羅浮石室僧，見尸的是單道開不是法防死，僅SL缺生卒。',9333:'曹恢陳留王世子363被立，原書省曹姓但爵世子恢明；不以王立年減16造生年，無生卒。',9337:'楊遹涼前將軍枹罕敗五key閉，SL楊安傳破軍殺將泛語沒有具名遹被斬；俘斬七千/萬七是軍數非本人年齡或必死，生卒未知。',9338:'王撫秦建威侯和守三key閉，非王抚東晉成都刺史只同名合；無生卒。',9339:'皇甫奮真從子、腆同秦與覆兄弟兩key同郭辯假相知案，真後數歲卒不是奮卒。',9340:'皇甫腆真兄秦散騎常侍兩key明，郭辯真假結交不表示腆本人出使燕；生卒未知。',9341:'皇甫覆真從子同奮兄弟兩key閉，郭辯假相知非本人犯罪被殺；生卒未知。',9343:'袁穎叔宏弟同支法防羅浮遊兩key閉；單道開百餘歲尸骸不是穎叔年齡或死亡。',9345:'邵羌南安守追斂岐三key閉，與弟邵保或所擒敛岐不同，未知生卒。',9346:'郭瑀符瑜敦煌郭荷弟子、孟公明徵、王穆長史、劉昞岳父三key全讀閉合；387不食卒有獨立TJ，無確生年或卒齡。',9347:'郭辯馮翊西戎主簿曹毂使副，原SL03作郭办同敘source字異不新增人物；未因被皇甫真請窮治但評不許便當被殺，無生卒。',9348:'丁妃是富商男名，與趙掇鄒甕同國卿案，不因名字妃就當后妃；被諫爵降不明被殺，生卒未知。',9349:'尚鋒燕助教國子博士單SL，所講經成熟的是暐不是本人年齡，無生卒。',9350:'張罔屠各單于為鄧羌討平，原句未明具名斬首；不把軍眾數千或建節任年當生卒，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    round52a_reads={9351:[('sl_03','秘书监[一作郎]杜诠')],9352:[('sl_04','遣后将军杨成世'),('tj_18','遣后将军昜成世'),('sl_05','坚遣杨成世等讨之')],9353:[('sl_04','左将军毛嵩讨上邽'),('tj_18','左将军毛嵩分讨')],9354:[('sl_03','王欢[一作王劝]'),('js_16','王欢，字君厚，乐陵人也')],9355:[('js_11','又击苻坚将王鉴于石桥'),('js_18','其将王鉴、张蚝等率兵以救瑾'),('sl_04','武卫将军武都王鉴'),('tj_18','武卫将军王鉴')],9356:[('sl_04','左禁将军窦冲率羽林骑'),('sl_05','窦冲武都人')],9357:[('sl_04','三月杨成世为赵公双将茍兴所败'),('tj_18','秦杨成世为赵公双将苟兴所败')],9358:[('sl_04','富商赵掇丁妃邹瓮'),('tj_18','富商赵掇等车服僭侈')],9359:[('sl_04','将军冯翊郭将翟傉'),('tj_18','将军冯翊郭将、翟傉')],9360:[('sl_03','命西戎主簿郭办潜结匈奴')],9361:[('sl_04','富商赵掇丁妃邹瓮')],9362:[('gs_01','释道恒。蓝田人')],9363:[('yl_08','上谷人公孙几久隐昌黎之域')],9364:[('sl_03','兰台侍御史刘当[侃之子也]'),('tj_18','兰台治书侍御史刘当')],9365:[('sl_03','太傅参军河间刘靖'),('tj_18','太傅参军河间刘靖')],9366:[('tj_18','立琅邪王昱子昌明为会稽王')],9367:[('sl_05','乙泉戍主吴归追及'),('tj_18','乙泉戌主吴归追及')],9368:[('sl_03','散骑侍郎姜抚报使'),('sl_04','散骑侍郎姜抚报使'),('tj_18','散骑侍郎姜抚报使')],9369:[('jk_02','二美人田氏孟氏生三男')],9370:[('sl_03','将军孙葢等推锋陥阵'),('tj_18','将军孙盖等摧锋陷陈')],9371:[('sl_03','黄门侍郎封孚'),('tj_18','孚，放之子也')],9372:[('sl_03','髙平太守徐翻举郡降温'),('tj_18','高平太守徐翻举郡来降')],9374:[('tj_18','将军慕容宙帅骑一千为前锋')],9375:[('js_16','恪还，从容言于慕容晞')]} 
    for ident,qs in round52a_reads.items():
        results[ident].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='all_source_key_subjects_individually_adjudicated_round52a',last_manual_review_batch='round52a')
        for ch,phrase in qs:results[ident]['evidence'].append(chunk_ev(ch,phrase,'第52批燕秦將臣逐源核讀','逐人實讀原官職、親族、語境與同案；不只靠名字命中同卷其他同名人，未知生卒不強補。'))
    c394=evidence(SOURCE/'_cache/texts/资治通鉴.txt','冲奔汧川，汧川氐仇高执送之','資治通鑑太元十九394窦冲被擒','實讀太元十九甲午394年及七月苻登死後八月前，強熙楊多推窦冲、興討、仇高執送，同SL完整窦冲傳。被俘不明處死，394為在世年非卒。')
    results[9356]['evidence'].append(c394);results[9356]['last_attested_activity']=394;results[9356]['independent_activity_evidence']=[c394]
    for ident,target in [(9360,9347),(9363,8939)]:results[ident].update(review_status='duplicate',duplicate_of=target,identity_status='unique_identical_occupation_place_and_event_name_variant')
    h417=chunk_ev('gs_01','晋义熙十三年卒于山舍。春秋七十二','高僧傳道恒417卒72','實讀藍田道恒全傳、張忠九歲見相、姚興勸恒標還俗往來書至卒，義熙13=417，虛歲72→346。後段僧肇年齡與前僧睿67非恒。')
    results[9362].update(review_status='primary_text_corroborated',historical_birth=date(346,h417,age=72),historical_death=date(417,h417));results[9362]['evidence'].append(h417)
    yao=evidence(SOURCE/'_cache/texts/晋书.txt','孝武皇帝讳曜，字昌明，简文帝第三子也','晉書孝武帝紀姓名父系與365封爵','全文實讀開篇與二十一年卒段，明曜/昌明一人，興寧三365七月甲申會稽王、372太子即位；非一般同名司馬氏臣。')
    yd396=evidence(SOURCE/'_cache/texts/晋书.txt','秋九月庚申，帝崩于清暑殿，时年三十五','晉書孝武帝紀太元二十一396九月庚申','已讀太元十八至二十一年連年標、崩後幼称聪悟及張貴人段。35虛歲→362；同紀回述372即位十歲若虛歲→363，可能一為周歲/約數但未硬裁。')
    results[9366].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(396,yd396,month=9,day_ganzhi='庚申'),appearance_upper_bound=365)
    results[9366]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9366]['evidence'].extend([yao,yd396,evidence(SOURCE/'_cache/texts/晋书.txt','简文之崩也，时年十岁','晉書孝武帝紀即位時齡異算法限定','372十歲與396卒35若同按虛歲有一年差，不能悄取一支；可為童年約數或實歲，但史實出生暫不擇單年。')])
    results[9366]['source_reported_chronologies']=[dict(source='晉書孝武帝紀396卒35',conditional_birth_year=362,calculation='396-35+1=362，虛歲'),dict(source='晉書孝武帝紀372簡文崩時10歲',conditional_birth_year=363,calculation='372-10+1=363若作虛歲；若實歲或約數則不可精推')]
    results[9364]['source_reported_relationship_variants']={'father':[{'source':'SL03','name':'劉侃'},{'source':'TJ18','name':'劉佩'}],'interpretation':'同蘭臺侍御史369五千騎德軍石門斷糧唯一事件，父名源異不改本人同一裁定，也不把父兩字自動合人。'}
    fjs=evidence(SOURCE/'_cache/texts/晋书.txt','封孚，字处道，渤海蓚人也','晉書南燕封孚附傳完整讀核','祖悛父放、前燕後燕吏部、南燕臣，超三年卒71；同卷超義熙元405即位改太上，超三年條件407。')
    fjd=evidence(SOURCE/'_cache/texts/晋书.txt','以超三年死于家，时年七十一','晉書封孚卒年卒齡','超405改太上即位，太上3=407，71虛歲推337。與TJ406十月卒有真正一年異說，不單鎖407或337。')
    ftj=evidence(SOURCE/'_cache/texts/资治通鉴.txt','冬，十月，封孚卒','資治通鑑義熙二406十月','實讀本年安皇帝義熙二丙午公元406標、封嵩叛五樓專權後與年底、407年標界；不是407十月，與JS超三年407真異。')
    results[9371].update(review_status='primary_text_corroborated',historical_birth=None,historical_death={'year':None,'precision':'conflicting_source_years','calendar':'historical_CE_year','alternatives':[dict(year=406,month=10,calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only',source=ftj['citation'],excerpt=ftj['excerpt']),dict(year=407,source=fjd['citation'],excerpt=fjd['excerpt'])]})
    results[9371]['evidence'].extend([fjs,fjd,ftj,evidence(SOURCE/'_cache/texts/晋书.txt','以义熙元年僭嗣伪位，大赦境内，改元曰太上','晉書慕容超405即位及改太上','本人附傳超三年對應407的同書紀年基礎，保源別不將之轉成晉義熙3而不說算法。')])
    results[9371]['source_reported_chronologies']=[dict(source='晉書封孚71卒及超三年',death_year=407,conditional_birth_year=337,calculation='407-71+1=337，虛歲'),dict(source='TJ406十月卒配JS71卒齡',death_year=406,conditional_birth_year=336,calculation='406-71+1=336，跨源條件配算非原書明生年')]
    results[9375].update(candidate_duplicate_of=9828,identity_status='jin_mu_rong_xi_wei_emperor_name_variant_branch_confirmed_target_mixed')
    results[9375]['evidence'].append(chunk_ev('js_16','至慕容晞袭伪号','晉書儒林王歡傳晞為前燕主的跨傳橋','同卷另王歡傳明晞襲偽號署博士、為苻堅滅；與SL/TJ暐同王歡任國子、恪伐洛陽語帝場景可橋。9828含另SG慕容支未合，僅帝王支候不整條刪。'))
    for ident,note in {9351:'杜詮燕秘書監或郎、散騎侍郎單SL，與後世同名或杜預後人不憑姓合，生卒未知。',9352:'楊成世/昜成世秦後將軍討雙失敗三key同，敗奔非明斬，未知生卒。',9353:'毛嵩秦左將軍討武敗奔兩key同，不把毛盛/毛當近名當同，無生卒。',9354:'王歡字君厚樂陵儒者，燕暐/晞署博士至國子祭酒兩key同；JS死長安無年，SL若太子少傅是晚記不當明死年，未知生卒。',9355:'四key秦王鑑援袁瑾石橋及武都武衛征雙武同軍將；JS11前段另一琅邪侍郎王鑑不是此key候選本人，不因同卷有另人自判本key混。兵敗無明卒。',9356:'窦冲武都左禁將軍至秦丕登大司馬、姚興時被擒完整兩key閉；394仇高擒送有獨立TJ在世證，不記被殺就不補394卒。',9357:'苟興雙將勝楊成世後退敗兩key同，二旬20日不是年齡，未知生卒。',9358:'趙掇富商國貳卿同程憲奏事兩key同，受薦諸公降爵不是趙死，生卒未知。',9359:'郭將馮翊秦將軍與王鑑呂光翟傉同軍兩key閉，不因名字將而割作職稱漏人，無生卒。',9360:'郭辦/郭辯同秦西戎曹毂副使與皇甫真兄侄假相知獨案→9347，字異保留。',9361:'鄒甕富商與趙掇丁妃同案單SL，不因反奢侈詔棄市泛條就認本人已被殺，無生卒。',9362:'道恒藍田羅什弟子姚興勸俗同完整GS，41772→346虛歲生，九歲張忠相不當死亡/精活動年。',9363:'公孫幾御覽燕書與公孫鳳JS/SL同上谷昌黎土床腐食獨行全情→8939；姓名異文保，不造另一怪行隱士。',9364:'劉當369蘭臺侍御史石門五千騎同兩key，父SL侃vsTJ佩源別保，未知生卒。',9365:'劉靖河間評參軍諫耀兵兩key同，不合曹魏鎮北劉靖僅同名，無生卒。',9366:'司馬昌明即孝武帝曜，396九月庚申卒35，372即位10若都虚岁推生362/363有差，未取得明生年月前出生保源別而非強單選；365已封王為登場上界。',9367:'吳歸乙泉戍主追垂於閿鄉被令擊退兩key同，津吏被斬不是吳歸死，未知生卒。',9368:'姜撫秦散騎報燕使三key同，不把被使往來年就當首任或生年，無生卒。',9369:'孟氏海西公美人同田氏被指三男非嫡，屬桓温廢立控詞不自斷實父朱靈寶；本段不明孟氏死，三子被縊不套母。',9370:'孫蓋/葢垂將摧鋒賞被評阻兩key同，未受賞非被殺，無生卒。',9371:'封孚處道放子前燕至南燕史傳身份全閉，但JS超三=40771卒vsTJ406十月卒真差一年；頂層死亡保alternatives无單年，出生337/336只源條件。',9372:'徐翻高平守黃墟後降晋兩key同，投降非死，無生卒。',9374:'慕容宙369德前鋒一千騎二百挑戰，軍數非齡；與後燕章武王宙是否同人待其族系轉爵橋，不自動同名合。',9375:'慕容晞據同卷王歡傳明為襲號前燕主、苻堅所滅，可橋暐帝王支；9828整條含SG其他慕容key，先候選不整刪，本key無生卒明。'}.items():results[ident]['unresolved'].append(note)
    round52b_reads={9376:[('sl_03','筑为假节征虏将军'),('sl_04','猛遗慕容筑书'),('tj_18','左中郎将慕容筑为洛州刺史')],9377:[('sk_01','<斯纪>')],9378:[('sl_04','小子寅为力川侯')],9379:[('sl_04','长子玺为骆川侯')],9380:[('jk_02','嬖人朱灵寳'),('wsh_02','朱灵宝'),('yl_00','朱灵宝')],9381:[('sl_03','散骑常侍李凤'),('sl_04','李鳯'),('tj_18','散骑常侍李凤')],9382:[('sl_03','染干津'),('tj_18','染干津')],9383:[('sl_03','琛从兄奕')],9384:[('tj_18','琛从兄弈')],9385:[('sl_03','乐嵩乞师'),('sl_04','乐嵩'),('tj_18','散骑侍郎乐嵩请救')],9386:[('sl_03','檀玄攻胡陆'),('tj_18','檀玄攻湖陆')],9387:[('js_13','安之字仲祖')],9388:[('sl_03','毛虎生'),('tj_18','冠军将军毛虎生')],9389:[('sl_03','司马爨亮如邺'),('tj_18','司马爨亮如邺'),('wsh_02','遣参军爨亮')],9390:[('sl_04','京兆尹王攸'),('js_15','杜令王攸'),('sl_00','杜令王攸')],9391:[('jk_02','二美人田氏孟氏'),('yl_00','美人田氏遂生三男')],9392:[('sl_03','尚书右丞申绍'),('sl_04','常山太守申绍'),('tj_18','尚书右丞申绍'),('sl_05','尚书右丞申绍')],9393:[('tj_18','子思复鞬代统其众')],9394:[('sl_03','苟纯'),('tj_18','以侍辇苟纯为副')],9395:[('sk_01','将军<莫古解>谏曰')],9396:[('wsh_02','真子双之等杀')],9398:[('js_11','南阳督护赵弘、赵忆')],9399:[('wsh_02','袁真及赵悦开石门'),('ssxy_02','田曹中郎赵悦子')],9400:[('js_11','南阳督护赵弘、赵忆')]}
    round52b_reads[9394][0]=('sl_03','侍辇茍纯副之')
    for ident,passages in round52b_reads.items():
        r=results[ident]
        r.update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='each_source_key_subject_read_and_bridged',last_manual_review_batch='round52b')
        for ch,phrase in passages:r['evidence'].append(chunk_ev(ch,phrase,'逐鍵本人原文覆核 '+ch,'已實讀本人及相接事件語境；未見明確生卒年月，不把戰敗、父卒、他人死或後世追述套本人。'))
    results[9384].update(review_status='duplicate',duplicate_of=9383,identity_status='liangchen_cousin_qin_shangshulang_yi_unique_lodging_exchange_variant')
    results[9400].update(review_status='duplicate',duplicate_of=9327,identity_status='zhaoyi_yi_character_variant_same_nanyang_guard_expels_huandan_wan_mutiny')
    results[9390].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='qin_jingzhaoyin_and_earlier_jin_duling_not_bridged',components=[dict(keys=[k for k in results[9390]['keys'] if k.startswith('sl:sl_04')],identity='前秦京兆尹王攸獻十略拜諫議大夫',historical_birth=None,historical_death=None),dict(keys=[k for k in results[9390]['keys'] if not k.startswith('sl:sl_04')],identity='晉杜縣令王攸詣杜宣為王育執刀叱退',historical_birth=None,historical_death=None)])
    results[9397].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,identity_status='cbdb182001_doulu_chou_original_genealogy_not_retrieved',last_manual_review_batch='round52b_cbdb')
    results[9397]['evidence'].append(dict(citation='原CBDB182001豆盧醜族系日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='子182002豆盧某；death430 range2，Death year estimated by Tackett (cc430)；source32033/71853頁143254、Q22813899。',interpretation='實讀原庫，430明估非確卒；原墓誌尚未取得，不用指數年367當生年或活動年，歷史生卒不詳。'))
    for ident,note in {9376:'慕容筑左中郎將金墉守至洛陽降秦三key同，降非死，未見生卒。',9377:'斯紀傷馬逃高句麗再返告近仇首太子赤旗實兵；375即位傳先是追敘，不當375本人活動或首次出仕。',9378:'曹寅毂幼子力川侯，父尋死不是兒卒。',9379:'曹璽毂長子駱川侯，父死非子死。',9380:'朱靈寶海西公嬖人三key同，JK明床笫易誣、WS及御覽明莫能審虛實，不能把桓温廢立控詞當確證陽痿或三男生父，無生卒。',9381:'李鳳/鳯燕散騎常侍求秦救三key閉，姓名字形異，不合別朝同名。',9382:'染干津同悉羅騰擊殺李述二key閉，死者李述不是本人。',9383:'梁奕燕梁琛從兄秦尚書郎，同使館問東方拒答獨案，TJ弈為名異；9384歸此。',9384:'同梁琛從兄奕，秦尚書郎來問東方獨一館舍案，不因奕弈異寫造兩人。',9385:'樂嵩燕散騎乞秦救割虎牢、與梁琛被問秦兵強弱同三key，未知生卒。',9386:'檀玄桓温建威將軍攻胡陸/湖陸獲慕容忠同兩key，地字源異保，俘敵非自己死。',9387:'毛安之仲祖完整本傳至卒官無年；瑾修之、子泰邃被桓玄殺不能套父。',9388:'毛虎生父寶、凿巨野清水及守東燕二key閉；軍糧水道記載不等於首次仕年，無生卒。',9389:'爨亮袁真參軍/司馬通燕、袁瑾再遣三key同，官稱源別非異人；未見本人死亡。',9390:'SL04前秦京兆尹獻十略與JS15/SL00較早晉杜令受王育叱退缺族官轉接，不因同名套同人；本條分支隔離而非先定兩人，生卒均不詳。',9391:'田氏海西公美人JK與御覽同；JK田孟二美人、御覽僅田氏三男，控詞不證實子父、不把三男被縊套母死。',9392:'申紹尚書右丞薦高泰、常山守使關東、秦苻融問使同四key；TJ明申胤兄，不能套胤死亡或別人年齡。',9393:'秃發思復鞬繼椎斤統眾，110歲卒的是父椎斤；子生卒不詳。',9394:'苟純梁琛副使歸讒同兩key，琛被繫非純死亡。',9395:'莫古解太子追奔水谷城勸止，同傳先是追敘不是375仍活；馬跡至今為後世地名，非本人存活。',9396:'袁雙之袁真子殺朱憲明本人，後袁瑾被擒族誅未具雙之名，不猜其卒年。',9397:'CBDB430明Tackett估，原墓未取得，未知歷史生卒，不當程序已抽年月為史證。',9398:'趙弘與趙憶並列逐桓淡據宛不同兩人，單JS身份閉，未知生卒，不合趙億。',9399:'趙悅/悦子下邳、大司馬參軍田曹中郎左衛，與袁真開石門同温僚官鏈二key可閉，無生卒。',9400:'趙憶同SL/TJ趙億南陽督護逐桓淡據宛唯一事件，字異保；非並列趙弘，整條歸已閉9327。'}.items():results[ident]['unresolved'].append(note)
    round53a_reads={9401:[('sl_03','秦尚书郎辛劲'),('tj_18','尚书郎辛劲')],9402:[('sl_03','徐兖二州刺史郄愔')],9403:[('sl_03','散骑侍郎太原郝晷'),('sl_04','散骑常侍郝晷'),('tj_18','散骑侍郎太原郝晷')],9404:[('sl_03','车骑从事中郎髙泰'),('sl_05','髙泰字子伯'),('tj_18','车骑从事中郎高泰'),('sl_06','庆生三子泰敬展')],9405:[('jk_02','梓潼太守周彪讨平之')],9406:[('js_07','子琼嗣'),('tj_18','周楚遣子琼讨高')],9407:[('tj_18','琼子梓潼太守飏讨弘')],9408:[('sl_04','封衡[裕之子也]'),('tj_18','封衡为尚书郎')],9409:[('sl_03','部将巨武执'),('tj_18','部将巨武将缚之')],9410:[('sl_04','燕国平叡为宣威将军')],9411:[('tj_18','燕国平睿为宣威将军')],9412:[('sl_04','彭豹为阳平太守'),('tj_18','彭豹为阳平太守')],9415:[('sl_04','猛遣将军徐成'),('sl_05','遣将军徐成觇燕军'),('tj_18','猛遣将军徐成')],9416:[('sl_03','郭庆遣将军朱嶷'),('sl_05','其部将朱嶷追'),('tj_18','郭庆遣将军朱嶷'),('sl_06','燕之亡为秦将朱嶷所杀')],9417:[('js_07','蜀盗李金银')],9418:[('js_07','陇西人李高'),('tj_18','陇西人李高'),('sl_09','李高陇西人')],9419:[('sl_03','辅国司马桓寅'),('sl_04','辅国司马桓寅'),('tj_18','辅国司马桓寅')],9420:[('sz_00','杨令辩，笃信黄老')],9421:[('sl_03','执秦将杨猛'),('tj_18','执秦将杨猛')],9422:[('sl_03','获将军杨璩')],9424:[('js_18','督护竺瑶、矫阳之'),('sl_03','督护竺瑶矫阳之')],9425:[('sz_02','出家为竺昙现弟子')]}
    for ident,passages in round53a_reads.items():
        r=results[ident]
        r.update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='each_source_key_subject_read_and_bridged',last_manual_review_batch='round53a')
        for ch,phrase in passages:r['evidence'].append(chunk_ev(ch,phrase,'逐鍵本人原文覆核 '+ch,'本人原句及所屬前後事件已實讀；缺明生卒，不以候選程序日期補填。'))
    results[9402].update(review_status='duplicate',duplicate_of=8122,identity_status='xi_yin_xu_yan_inspector_huanwen369_name_variant_same_closed_target')
    results[9405].update(review_status='duplicate',duplicate_of=9407,identity_status='zhou_biao_yang_same_zitong_taishou_370_suppresses_lihong_unique_event_name_variant')
    results[9411].update(review_status='duplicate',duplicate_of=9410,identity_status='ping_rui_rui_same_yanguo_xuanwei_appointment_variant')
    results[9404]['evidence'].append(evidence(SOURCE/'_cache/texts/魏书.txt','祖庆，慕容垂司空。父泰，吏部尚书','魏書高湖傳高泰家世旁證','SL06同書明泰仕秦見前秦錄，提供對SL05子伯人物的書內交叉指向；魏書慶泰湖三代官吻合，但湖70卒不能套父泰。'))
    for ident,person,page,father,son in [(9413,184724,'146143',192871,184725),(9414,184736,'146157',192871,184737)]:
        r=results[ident]
        r.update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='cbdb_original_genealogy_not_retrieved',last_manual_review_batch='round53a_cbdb')
        r['evidence'].append(dict(citation=f'原CBDB{person}日期族系限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=f'父{father}子{son}；death431、range2、Death year estimated by Tackett (cc431)；source32033/71853頁{page}。',interpretation='原庫已讀但墓誌未取得，不以Tackett431估年作歷史確卒。'))
        r['unresolved'].append('431為原庫明標估算，出生不詳；原墓誌未取身份尚待原書橋，不把數據庫索引年當活動年。')
    results[9423].update(source_identities_all_adjudicated=True,identity_status='yanfeng_zizhang_dai_changshi_five_source_keys_closed',last_manual_review_batch='round53a_full_keys')
    for ch,phrase in [('sl_04','长史燕鳯诣秦'),('tj_16','代人燕凤为长史'),('tj_18','左长史燕凤'),('wsh_00','二十九年夏五月，遣燕凤'),('wsh_01','燕凤，字子章')]:results[9423]['evidence'].append(chunk_ev(ch,phrase,'燕鳳五來源逐鍵身份閉合 '+ch,'子章代人左長史、使秦、昭成至世祖家臣完整鏈已讀；TJ338始置百官為概括，不因它推生年。'))
    yf=chunk_ev('wsh_01','神\ue154元年卒','魏書燕鳳本傳神䴥元年卒','完整本傳本人，子才及元孫為下一代；神䴥元年428，原文無卒齡或生年月，僅確卒年。')
    results[9423]['historical_birth']=None
    results[9423]['historical_death']=date(428,yf)
    results[9423]['evidence'].extend([yf,evidence(SOURCE/'_cache/texts/魏书.txt','神\ue154元年春正月','魏書世祖紀神䴥元年改元及赫連昌被擒','同本帝紀改元年及赫連昌被擒為428年，不把文庫私用字缺字誤作未知另一年號；死亡未明月日。')])
    for ident,note in {9401:'辛勁秦尚書郎與梁琛野見之禮爭論同二key，不套辛恭靖等近名。',9402:'郄愔/郗愔同徐兖刺史桓温北征，唯一SL閉後歸8122既有全鍵規範位。',9403:'郝晷太原燕使與猛舊兩SL及TJ同人，侍郎/常侍源官異保，不定生卒。',9404:'高泰子伯、瞻從子四key同：SL06明泰仕秦見前秦錄交叉橋，慶泰湖魏書旁證；高湖70卒非泰，無生卒。',9405:'建康實錄370平廣漢李弘梓潼太守周彪與TJ同官同敵同年周飏同獨事，名異保源，歸9407，不因字形改寫原引文。',9406:'周楚子瓊兩key閉；JS遣子詩平高、TJ遣子瓊孫飏分討高弘的源別保留，不因派遣者不同就拆瓊，卒官無年。',9407:'周飏楚孫琼子梓潼守討弘；与後來周虓同官父子但缺此役名異直接橋暫候，不以虓太原病卒未明年套飏。',9408:'封衡裕子燕亡後秦尚書郎二key閉，不套父封裕或封孚年。',9409:'巨武梁山郭慶部將捕暐二key閉，所俘天子非己死。',9410:'平叡/睿燕國宣威將軍同秦任官唯一名異，9411歸此，無生卒。',9411:'睿叡名異同燕國宣威將軍→9410，保TJ早源。',9412:'彭豹秦陽平太守二key同，任官非初仕，無生卒。',9415:'徐成王猛使覘燕遲返羌請赦同三key；將斬但實獲赦，不能記370已死。',9416:'朱嶷郭慶部將捕殺慕容桓四key同，SL06慕容鳳父桓死回述非嶷當時仍活。',9417:'李金銀蜀盜與李弘並列，不把李金根異名未橋便自合；李弘被殺不等李金銀死。',9418:'李高陇西詐李雄子攻涪城同三key；討平未具本人被斬，不推卒370。',9419:'桓寅秦輔國司馬弘農守三key閉，不合桓温家族僅同姓，無生卒。',9420:'楊令辯黃老女術師為尼僧楊道馨下毒之傳，死者馨非令辯；神異因果敘述按來源記載，不硬作司法確殺。',9421:'楊猛秦將石門被燕慕容臧執，同二key，俘非死；不是王猛。',9422:'楊璩燕將被梁成獲，與前句秦楊猛不同軍不同姓名，勿因相接兩次擒將便合。',9423:'燕鳳子章五key閉，WS明428神䴥元年卒無齡，出生不詳；不把許謙39663卒或後子才卒套鳳。',9424:'矯陽之桓温督護與竺瑤武丘破燕軍同二key，無生卒。',9425:'竺曇現慧永之師，泰和中北嶺創寺明本人，不把弟子慧永414卒83推給師，生卒未知。'}.items():results[ident]['unresolved'].append(note)
    results[9407]['candidate_duplicate_of']=9456
    round53b_reads={9426:[('gs_01','竺道壹姓陆')],9427:[('sz_00','竺道馨，本姓羊')],9428:[('js_15','其将苟苌、毛当'),('sl_03','留屯骑校尉茍苌'),('sl_04','留屯骑校尉茍苌'),('wsh_02','武卫将军苟苌')],9429:[('tj_18','屯骑校尉苟长')],9430:[('sl_03','侍中兰伊让评'),('sl_04','侍中兰伊譲评'),('tj_18','侍中兰伊让评')],9431:[('wsh_02','瑾弟泓等')],9432:[('sl_03','游击将军郭庆'),('sl_04','游撃将军郭庆'),('sl_05','郭庆不知何许人'),('tj_18','游击将军郭庆')],9433:[('sl_05','平凉太守金熙'),('tj_18','垂所亲金熙')],9434:[('wsh_02','陈文报爨亮')],9435:[('sl_03','其子瑶秦苻坚时王猛荐')],9436:[('sl_04','散骑侍郎京兆韦儒'),('tj_18','散骑侍郎京兆韦儒')],9437:[('sl_03','辽东太守韩稠'),('tj_18','辽东太守韩稠')],9438:[('sl_03','散骑侍郎余蔚'),('sl_06','右光禄大夫余蔚'),('tj_18','燕散骑侍郎馀蔚')],9439:[('sl_04','参军冯诞'),('tj_18','参军冯诞')],9440:[('wsh_02','散骑侍郎刘亨')],9441:[('js_09','舍人刘彊')],9442:[('wsh_02','参军卜靖')],9443:[('sl_04','遣兄子纯谢罪'),('sl_09','遣其兄子纯谢罪'),('tj_18','遣其兄子纯谢罪')],9447:[('sl_05','世弟綂骁武得众心'),('tj_18','叔父武都太守统'),('wsh_04','统一名德'),('yi_01','以世弟统为宁东将军')],9448:[('js_11','故青州刺史武沈')],9449:[('js_09','珣字元琳'),('js_13','王珣娶万女'),('tj_18','王珣为主簿'),('js_10','王珣为温主簿'),('js_16','尚书仆射王珣'),('ssxy_02','王珣疾临困'),('gs_01','领军王洽东亭王珣'),('hj_03','尝与王珣、伏滔'),('ssxy_00','王珣游严陵濑诗叙'),('ssxy_01','东亭侯王珣迎至舍'),('ssxy_03','王珣郄超并有竒才')]}
    for ident,passages in round53b_reads.items():
        r=results[ident]
        r.update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='each_source_key_subject_read_and_bridged',last_manual_review_batch='round53b')
        for ch,phrase in passages:r['evidence'].append(chunk_ev(ch,phrase,'逐鍵本人原文覆核 '+ch,'本人段及前後語境已實讀；生卒缺確年留不詳，歧異另列而不靜改。'))
    results[9429].update(review_status='duplicate',duplicate_of=9428,identity_status='gou_chang_chang_variant_same370_tunqi_xiaowei_huguan_guard')
    results[9426]['source_reported_chronologies']=[dict(source='高僧傳竺道壹完整本傳',death_reign='隆安中',death_year_range=[397,401],reported_age=71,conditional_birth_year_range=[327,331],calculation='每候選卒年減71加1；無確卒年不挑單生年')]
    results[9426]['evidence'].append(chunk_ev('gs_01','以晋隆安中遇疾而卒','高僧傳竺道壹完整本傳隆安中卒71','陸姓吳人小壹，與汰弟子曇一大一有別；隆安僅397–401範圍，無精年，前段道曠402卒76不可套。'))
    results[9427]['source_reported_chronologies']=[dict(source='比丘尼傳道馨傳',death_reign_context='晋泰和中',death_year_range=[366,371],historical_birth=None,interpretation='同段楊令辯下毒而終，紀年只作敘事範圍，無本人卒齡；前尼39768卒屬僧基。')]
    results[9433].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='370_chui_intimate_and386_pingliang_governor_same_name_not_bridged',components=[dict(keys=[k for k in results[9433]['keys'] if k.startswith('tj:') or k.startswith('sl:')],identity='370慕容垂所親金熙被王猛賂誘令東返',historical_birth=None,historical_death=None),dict(keys=[k for k in results[9433]['keys'] if k.startswith('sl:')],identity='386平涼太守金熙與沒奕干敗姚方成',historical_birth=None,historical_death=None)])
    results[9447]['source_reported_chronologies']=[dict(source='宋書氐胡傳',sequence='太和五年世卒、統廢纂、纂聚黨殺統，後遣使詣簡文帝',death_year_range=[370,371],interpretation='簡文即位371與敘次相接，只保370–371範圍不強鎖370。'),dict(source='魏書氐傳',sequence='世死統立、纂襲殺統，咸安元371秦伐纂之前',death_year_upper_bound=371),dict(source='SL重編前秦錄苻雅傳',late_reconstruction=True,sequence='371秦雅伐纂時統帥武都眾迎降',activity_year=371,interpretation='與宋魏統已被纂殺的次序真異，待更早獨立材料，不以晚出SL在世推修估死。')]
    results[9447]['source_reported_name_variants']={'alias_de_attribution':[{'source':'魏書','name':'楊統一名德'},{'source':'宋書','name':'楊纂一名德'}],'interpretation':'原句別名德歸屬相反，不能靜移成無爭議統別名。'}
    for ident,person,page,kin in [(9445,176827,'137893','子176828褚炫'),(9446,194398,'160367','子172822高和璧、172839高振')]:
        r=results[ident]
        r.update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='cbdb_original_genealogy_not_retrieved',last_manual_review_batch='round53b_cbdb')
        r['evidence'].append(dict(citation=f'原CBDB{person}族系日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=f'{kin}；death432 range2、Death year estimated by Tackett (cc432)；source32033/71853頁{page}。',interpretation='原庫估年撤歷史確年；原墓誌未取，不因同名高祐就合魏書子集父讜支，後者尚未取得這兩子的族系橋。'))
        r['unresolved'].append('432明Tackett估，不作歷史確卒；原墓誌尚未取，身份與生卒待原文。')
    wjs=chunk_ev('js_09','四年，以疾解职。岁余，卒，时年五十二','晉書王珣本傳隆安四年解職歲餘卒52','隆安四400解職後歲餘，即逾一年才卒；與通鑑同年五月卒存在叙次歧異，不能無聲採400，亦不把模糊歲餘硬定401某月。')
    wtj=evidence(SOURCE/'_cache/texts/资治通鉴.txt','丙寅，卫将军东亭献侯王珣卒','資治通鑑隆安四400五月丙寅王珣卒','本人官爵與東亭王珣身份閉；本卷400年五月丙寅死亡明記。與JS解職歲餘卒保源別。')
    results[9449].update(review_status='primary_text_corroborated',identity_status='wangxun_yuanlin_fahu_wangqia_son_twelve_keys_closed_chronology_conflict',historical_birth=None,historical_death=dict(year=None,precision='conflicting_source_chronologies',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only',source='資治通鑑400五月丙寅 / 晉書隆安四解職歲餘卒52',alternatives=[dict(year=400,month=5,day_ganzhi='丙寅',source=wtj['citation'],excerpt=wtj['excerpt']),dict(year=None,not_before_year=401,source=wjs['citation'],excerpt=wjs['excerpt'],interpretation='400解職歲餘，401只是逾一年敘次推界，不填精年。')]))
    results[9449]['evidence'].extend([wjs,wtj,chunk_ev('js_09','升平二年卒于官，年三十六。二子：珣、珉','晉書王洽傳王珣父子橋','CBDB134773原父25790王洽同主傳父子，父36卒非本人52。'),dict(citation='原CBDB134773王珣族系日期',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父25790王洽；生卒皆null；source9601頁2005、ctext896139、Q15902305、Wang Xun (calligrapher)。',interpretation='原庫父子與JS主傳直接閉合；c_index_year327非生年活動年，不用填歷史日期。')])
    results[9449]['source_reported_chronologies']=[dict(source='TJ400卒配JS52卒齡',death_year=400,conditional_birth_year=349,calculation='400-52+1=349，跨源虛歲條件'),dict(source='JS400解職歲餘卒52',death_not_before_year=401,conditional_birth_year_not_before=350,calculation='若401卒則401-52+1=350；歲餘非精卒年，不硬選350')]
    results[9450].update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='cbdb134662_taoqian_wife_primary_biography_not_retrieved',last_manual_review_batch='round53b_cbdb')
    results[9450]['evidence'].append(dict(citation='原CBDB134662翟氏陶潛妻身份',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='丈夫3835陶潛；生卒null；source9601頁2511、Q65800966。',interpretation='原庫已讀，所引婦女資料原頁及蕭統陶潛傳未取得；公開陶淵明傳直標題404不算查完，不以368索引當本人年。'))
    for ident,note in {9426:'竺道壹陸姓小壹，完整本傳隆安397–401卒71，無精年不取常見328397；前道曠40276與弟子曇一不得混。',9427:'竺道馨羊姓太山尼僧，與楊令辯同姓結交被毒故事；晋泰和366–371範圍，不把前僧基隆安元397卒68套馨。',9428:'苟萇/長秦屯騎校尉壺關至武衛伐凉四key軍官鏈閉，9429字異歸此，無明生卒。',9429:'TJ苟長與SL茍苌同370屯騎校尉留壺關唯一事→9428；名字及書別保原。',9430:'蘭伊燕侍中奉暐命責評鬻薪水三key閉，不作蘭汗或同姓別人，無生卒。',9431:'袁泓瑾弟燕署郡守四品將軍明本人，未具被誅名，不以家族敗亡強補371。',9432:'郭慶秦游擊燒評軍追暐、幽州刺史四key閉，SL前段徐成被姚興殺不套本傳郭慶。',9433:'SL候選本身兼370垂親與386平涼守同名，原文未見轉職或族系連線，暫hold保兩支，不宣稱確定兩人，不用386套370親信在世。',9434:'陳文奉暐命報爨亮觀袁真之變單WS，非他朝近名；無生卒。',9435:'陽瑤為陽騖子，省姓本傳明王猛薦著作佐郎；父敬諡及卒無殮財不是本人死亡，無生卒。',9436:'韋儒京兆秦散騎綉衣使關東同二key，與韋鍾魏郡守不同人，無生卒。',9437:'韓稠遼東守降秦拒慕容桓同二key，桓被朱嶷殺非韓稠死。',9438:'餘/馀蔚燕散騎率扶餘質子開邺與後燕扶餘王右光禄右僕射三key族名政治連續無衝突，官爵後升非死亡；不由370敗亡猜卒。',9439:'馮誕王猛參軍論郝晷梁琛同二key，不合北魏馮熙子另馮誕僅同名，無生卒。',9440:'劉亨散騎取海西公玺與竺瑶同使，異書享/亨待對應原key；不把废帝死年當取玺人卒。',9441:'劉彊舍人被誣同武陵王反，族誅殷涓等未具劉名，不補371卒；武陵王38166卒非舍人。',9442:'卜靖楊亮參軍赴仇池敗走，單WS明人，敗非被殺，未知生卒。',9443:'李純李俨兄子枹罕請秦救三key閉，原文省姓不漏，未知生卒。',9447:'楊統世弟纂叔武都守四key本人閉，但宋魏纂殺統在秦伐纂前vs晚SL371統迎降源次序異；歷史卒年不強填370，別名德宋歸纂魏歸統也須源別保。',9448:'武沈故青州刺史庾希從母兄，潛饷希；遵為子，遵被誅不等父沈死。',9449:'王珣元琳小字法護洽子12keys閉，TJ400五月丙寅卒 vsJS400解職歲餘卒52真叙次異；卒容器yearnull，出生349或不早350只條件，非單確年。',9450:'CBDB翟氏陶潛妻原庫婚姻已讀，但原婦女頁/蕭統傳未取得不算核完；史實生卒不詳，368索引不可當年。'}.items():results[ident]['unresolved'].append(note)
    round54a_reads={9451:[('sl_04','范阳公抑为征东'),('tj_18','范阳公抑为征东')],9452:[('sl_04','留世子良守蒲阪'),('tj_18','留其世子良守蒲阪')],9453:[('js_13','长城人钱弘')],9454:[('tj_18','立皇后可足浑氏')],9455:[('sl_03','尚书令豫章公翼女')],9456:[('js_07','虓字孟威')],9459:[('sl_09','立子大懐为世子'),('js_15','立子大怀为世子')],9460:[('js_18','熙字伯道'),('tj_18','温世子熙为豫州刺史'),('ssxy_02','遐字伯道温长子')],9461:[('gs_01','后遣符丕南攻襄阳')],9462:[('tj_18','垂舅兰建'),('sl_05','垂舅兰建')],9463:[('js_14','字武子，南平人'),('ssxy_00','车武子难苦问谢'),('ssxy_01','车父作南平郡功曹')],9464:[('sl_03','定襄王渊'),('tj_18','字襄王渊')],9465:[('sl_03','东海王庄'),('sl_04','东海王慕容庄'),('sl_05','执其刺史慕容庄'),('tj_18','东海王庄')],9466:[('sl_03','上党太守南安王越'),('sl_04','上党太守南安王慕容越'),('tj_18','上党太守南安王越')],9467:[('jk_02','李金根反立李')],9468:[('sl_04','仇池公杨纂'),('sl_05','世死子纂代立'),('tj_18','仇池公杨世卒，子纂立'),('wsh_02','秦州刺史杨纂'),('wsh_04','纂聚党袭杀统'),('yi_01','纂一名德')],9469:[('sz_02','吾罪深矣。年六十一，卒于江陵'),('gs_01','后卒于江陵春秋六十')],9470:[('tj_18','汲郡赵秋曰')],9472:[('js_09','新蔡王晁使自诬')],9473:[('js_09','免其世子综官'),('gs_01','汝南世子司马综'),('ssxy_03','与晞及子综谋逆')],9474:[('js_09','字贤明，出继梁王翘'),('js_02','为翘嗣，历永安太仆')]}
    for ident,passages in round54a_reads.items():
        r=results[ident]
        r.update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='each_source_key_subject_read_and_bridged',last_manual_review_batch='round54a')
        for ch,phrase in passages:r['evidence'].append(chunk_ev(ch,phrase,'逐鍵本人原文覆核 '+ch,'原文本人及事件上下文已實讀，年月缺據不硬補；異名及身份未決另列。'))
    results[9461].update(candidate_duplicate_of=9814,identity_status='fu_pi_gs_fu_character_variant_same_xiangyang_conquest_target_keys_pending')
    results[9467].update(review_status='duplicate',duplicate_of=9417,identity_status='li_jingen_jinyin_variant_same_lihong_shu_rebellion_370')
    pxb=chunk_ev('sl_05','进拔汉川转攻梓潼降其太守周彪','SL楊安傳建元九373梓潼周彪降秦','與JS周虓373梓潼守母妻被朱肜獲而降楊安是同一次征服事件；JK另370周彪梓潼守平李弘與TJ周飏同事件，相同官名跨兩獨事閉合彪/飏/虓。SL為晚出但僅跨名橋，無生卒確年由它輸入。')
    results[9456]['evidence'].extend([pxb,chunk_ev('jk_02','梓潼太守周彪讨平之','建康實錄370梓潼周彪平李弘','同官周彪與SL373降楊安連貫，370又與TJ瓊子飏平弘同獨事；保源名異。'),chunk_ev('tj_18','琼子梓潼太守飏讨弘','通鑑370梓潼守周飏平弘','爵父瓊及梓潼守、同時平李弘，同JK周彪；JS虓瓊子梓潼守，非另外硬造被殺年。'),chunk_ev('js_07','虓竟以病卒于太原','晉書周虓完整本傳結局','母妻被獲降秦、固拒苻堅不屈，後太原病卒，子興迎喪謝玄哭贈；沒有明卒年，不用記憶補382或被殺。')])
    results[9456]['source_reported_name_variants']={'name':['周虓(JS)','周飏(TJ370)','周彪(JK370/SL373)'],'interpretation':'兩次梓潼事件與同官同父瓊的交叉閉合，保各書原字，不改史文。'}
    for ident in [9405,9407]:
        results[ident].update(review_status='duplicate',duplicate_of=9456,source_identities_all_adjudicated=True,identity_status='zhou_biao_yang_xiao_zitong_two_unique_events_cross_source_bridge')
        results[ident]['evidence'].append(pxb)
        results[ident]['unresolved'].append('本輪再讀JS周虓太原病卒及SL373梓潼周彪降楊安，以JK370周彪平弘/TJ周飏同役閉合異名，最終規範9456；仍未知生卒。')
        results[ident].pop('candidate_duplicate_of',None)
    vcar=evidence(SOURCE/'_cache/texts/资治通鉴.txt','胤惧，自杀','資治通鑑隆安四400十二月車胤自殺','已讀本年400標題、十二月戊寅星變後元顯責胤，後壬辰立燕台；不把戊寅星變日當自殺日。')
    results[9463].update(review_status='primary_text_corroborated',historical_death=date(400,vcar,month=12))
    results[9463]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9463]['evidence'].append(vcar)
    results[9468]['last_attested_activity']=371
    results[9468]['source_reported_chronologies']=[dict(source='宋書氐胡傳',capture_year=371,death_expression='纂後為楊安所殺',death_year=None,interpretation='後被楊安殺未明哪年，不把371受俘立刻當本人已死。')]
    results[9469]['source_reported_age_at_death']={'alternatives':[{'source':'高僧傳','age':60},{'source':'名僧傳抄','age':61}],'calendar_year':None,'interpretation':'兩源同杖筒自罰獨事，卒齡60/61真源差但均無卒年，不反推單生年。'}
    results[9472]['candidate_duplicate_of']=4281
    results[9473].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='wuling_shizi_sima_zong_vs_gs_runan_shizi_not_yet_bridged',components=[dict(keys=[k for k in results[9473]['keys'] if not k.startswith('gs:')],identity='武陵王晞世子綜被誣反徙新安',historical_birth=None,historical_death=None),dict(keys=[k for k in results[9473]['keys'] if k.startswith('gs:')],identity='GS法汰傳汝南世子司馬綜掘瓦官寺側',historical_birth=None,historical_death=None)])
    for ident,person,page,kin,estimate in [(9457,177344,'138467','父177343崔脩之、子177345崔亮及177354崔敬默',434),(9458,177375,'138499','父177343崔脩之、子177376崔光伯',434),(9471,194393,'160362','子194392韋千雄',435),(9475,181909,'144053','父181908敬歸、子181910敬頻',436)]:
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,identity_status='cbdb_original_genealogy_not_retrieved',last_manual_review_batch='round54a_cbdb')
        results[ident]['evidence'].append(dict(citation=f'原CBDB{person}族系日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=f'{kin}；death{estimate} range2、Death year estimated by Tackett (cc{estimate})；source32033/71853頁{page}。',interpretation='原庫明估不能當歷史確卒，原墓誌未取；有另讀正史身份橋者單獨恢復。'))
    cu=evidence(SOURCE/'_cache/texts/魏书.txt','父元孙，刘骏尚书郎','魏書崔亮傳父元孫被沈文秀害及季父幼孫','崔亮父元孫直接閉原CBDB子崔亮；刘彧即位後元孫奉使討文秀才被害，原434估卒被歷史時段否定；幼孫為亮季父同元孫弟，與兩庫同父脩之可橋。')
    results[9457].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='cbdb_cui_yuansun_son_cuiliang_weishu_family_bridge',last_attested_activity=465,appearance_upper_bound=464)
    results[9457]['evidence'].append(cu)
    results[9457]['source_reported_chronologies']=[dict(source='魏書崔亮傳',death_year_range=[465,469],interpretation='劉彧465即位後討文秀死，慕容白曜平三齊469之前；不選單466年。465僅已在世下界、464僅劉駿任官終期上界。')]
    results[9458].update(review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='cbdb_cui_yousun_yuansun_brother_cuiliang_jifu_weishu_bridge',last_attested_activity=469)
    results[9458]['evidence'].append(cu)
    for ident,note in {9451:'苻抑范陽公并州刺史鎮蒲阪兩key閉，原文省姓，不自合苻融等近族，無生卒。',9452:'苻良柳世子守蒲阪兩key同，柳與妻子斬為家屬總說，未具良死亡名不強套368卒。',9453:'錢弘長城百人聚原鄉山被朱序討擒，擒非死，無生卒。',9454:'前燕暐妻可足渾氏翼女太后從侄女單TJ，與9271長安君妹支不同，9271僅妻支候此；未知生卒。',9455:'可足渾翼豫章公尚書令暐后父，太后從弟，單SL未記本人生卒，不套女立后日當父登場精年。',9456:'周虓字孟威琼子梓潼守，母妻獲而降楊安、太原病卒無年，子興迎喪；SL/Jk周彪及TJ飏跨373降秦和370平李弘兩獨事閉異名，9405/9407歸本位。',9457:'崔元孫CBDB子亮与WS崔亮父元孙劉駿尚書郎直接橋，刘彧465後始討文秀被殺，434估錯；卒465–469範圍、未知精年，465在世下界不稱恰年任命。',9458:'崔幼孫原庫與元孫同父脩之，WS亮依季父幼孫的唯一家族叔侄橋；469白曜平三齊後遷桑干才依幼孫，故434估卒被否定，469只是存活下界。',9459:'張大懷天錫子立世子後廢高昌公兩key閉，立廢非明死，未知生卒。',9460:'桓熙伯道温長子豫州刺史三key閉，SS原注作桓遐同字父職三元組，保熙/遐異；徙長沙非殺，無生卒。',9461:'GS符丕南取襄陽獲道安朱序，即9814苻丕軍支，但target全keys未明先候；不合符堅因同符姓。',9462:'蘭建垂舅告評后陰謀兩key閉，不合蘭汗近親名，未知生卒。',9463:'車胤武子南平父育三key閉含PUA缺字，以獨立TJ400十二月自殺確卒；原生年329或333未據，不倒推。',9464:'慕容淵定襄王同暐敗逃二key，TJ字襄是字異，逃非死無生卒。',9465:'慕容莊東海王并州守晋陽被俘四key閉，未記殺不得填370卒。',9466:'慕容越南安王上黨守壺關被俘三key閉，未記殺不填370卒。',9467:'李金根JK與李金銀JS同蜀盜扶李弘圣王事件，名異歸9417；立者/被立者不混，不補死年。',9468:'楊纂世子纂统侄四書六key身份閉；宋後為楊安殺未記年，371受俘明活只在世年；德別名歸纂/統前已保源異。',9469:'法遇道安弟子杖筒自罰兩源同獨事，GS60 vsSZ61卒齡異，兩者卒江陵無年，太元二年下江陵不能當死年。',9470:'趙秋汲郡人言秦十五年復為燕有單TJ，預言不當死期，不以後驗年補本人活動。',9471:'韋惠度435估原墓未取，生卒不詳。',9472:'司馬晁新蔡王同一JS段後作晃，誣首案與4281新蔡晃支吻合；target七key待核不整刪。',9473:'JS/SS晞世子綜vsGS汝南世子綜爵支未橋，可能GS記爵異或別宗室，暫分支hold而非必說兩人；GS法汰卒38768不套綜。',9474:'司馬逢原文玉旁PUA逢，梁王翹嗣、武陵晞子字賢明兩key同，徙新安後薨無年，父晞381卒66非兒。',9475:'敬某父敬歸子敬頻原估436未取墓，匿名不按同某字併人，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    results[7778].update(review_status='duplicate',duplicate_of=6595,source_identities_all_adjudicated=True,identity_status='zhangjin_zhangjian_same_sujun_wuxi_poyu_bing_campaign_target_now_closed')
    results[7778].pop('candidate_duplicate_of',None)
    results[7778]['evidence'].append(dict(citation='4000片代理batch191全核6595張健五keys回證',verification='delegated_chronology_4000_6999',source_review='data/persons/reviews/review_4000_6999.jsonl#6595',excerpt='代理確認庾冰王舒顧眾無錫張健馬流→吳城、郗鑒大業、蘇峻餘眾殺弘徽、允之敗健及李閎追殺五key全閉，TJ329二月具名卒。',interpretation='本片早已實讀JK/WS張瑾管商無錫破庾冰及JS張健無錫獨事，本次目標全鍵回證到達才將候選改硬重定向；保健瑾原字，不冒稱本代理新親讀另片五key。'))
    round54b_reads={9476:[('js_07','使杨珍昼夜诣宣帝别庙')],9477:[('wsh_02','嬖人相龙、朱灵宝'),('yl_00','相龙计好，朱灵宝')],9479:[('js_15','晋兴相彭知正')],9480:[('wsh_00','大司马苻洛'),('wsh_01','苻洛以其年长'),('yi_01','并州刺史苻洛')],9481:[('js_15','西平相赵疑')],9483:[('js_15','广武太守辛章')],9484:[('jk_02','百济王余句')],9485:[('js_15','龙骧将军马达')],9486:[('sl_05','魏太祖征慕容寳'),('tj_18','世子令、令弟宝')],9487:[('sl_04','将作功曹熊邈')]}
    for ident,passages in round54b_reads.items():
        r=results[ident]
        r.update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='source_checked_dates_unknown',source_identities_all_adjudicated=True,identity_status='each_source_key_subject_read_and_bridged',last_manual_review_batch='round54b')
        for ch,phrase in passages:r['evidence'].append(chunk_ev(ch,phrase,'逐鍵本人原文覆核 '+ch,'實讀本人及完整事件段落，原文缺載年月不補估；多key不明身份分支另列。'))
    cbdb54={9478:(184437,'145904',436,'子184438路某'),9482:(178628,'139826',437,'父178627辛顏、子196357辛某'),9488:(178326,'139512',440,'父194427柳卓、子178327柳某'),9489:(178346,'139532',440,'父194427柳卓、子178347柳某'),9490:(178259,'139444',440,'父194427柳卓、子178260柳平')}
    for ident,(person,page,estimate,kin) in cbdb54.items():
        results[ident].update(review_status='needs_further_research',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=False,identity_status='cbdb_original_genealogy_not_retrieved',last_manual_review_batch='round54b_cbdb')
        results[ident]['evidence'].append(dict(citation=f'原CBDB{person}族系日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt=f'{kin}；death{estimate} range2，Death year estimated by Tackett (cc{estimate})；source32033/71853頁{page}。',interpretation='實讀原庫但原墓誌未取得，不把Tackett估算死亡或c_index_year當確年。'))
        results[ident]['unresolved'].append('原墓誌未取得，史實生卒不詳；上述死亡明估，兄弟同估年份並不證明同日卒，匿名子不作去重。')
    yz=evidence(SOURCE/'_cache/texts/魏书.txt','曾祖珍 ，太祖时归国，卒于上谷太守','魏書楊播傳珍真懿播四代','CBDB172571子172572楊真與珍子真父懿楊播祖系直接橋；與JS301司馬倫時祭別廟巫祝楊珍未有族官轉接，不把兩支整人校北魏。')
    results[9476].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='jin301_priest_and_northernwei_yangzhen_father_yangzhen_true_genealogy_branch_not_bridged',components=[dict(keys=[k for k in results[9476]['keys'] if k.startswith('js:')],identity='301孫秀司馬倫使楊珍祈宣帝廟巫祝',historical_birth=None,historical_death=None),dict(keys=[k for k in results[9476]['keys'] if k.startswith('cbdb:')],identity='北魏太祖時歸國上谷太守楊珍、子真、孫懿',historical_birth=None,historical_death=None)])
    results[9476]['evidence'].extend([yz,dict(citation='原CBDB172571楊珍日期族系限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='子172572楊真；death436 range2，Death year estimated by Tackett (cc436)；source32033/71853頁133503，Q26209007。',interpretation='楊真父珍可橋魏書北魏祖系，但與JS301巫祝未橋，估436不能當全記錄歷史卒年。')])
    results[9480].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='fu_luo_conquest_dai_three_military_keys_closed_cbdb_tianjin_gazetteer_unretrieved',components=[dict(keys=[k for k in results[9480]['keys'] if not k.startswith('cbdb:')],identity='前秦苻洛376滅代移窟咄長安軍事支',historical_birth=None,historical_death=None),dict(keys=[k for k in results[9480]['keys'] if k.startswith('cbdb:')],identity='CBDB420171天津府志苻洛原段未取',historical_birth=None,historical_death=None)])
    results[9480]['evidence'].append(dict(citation='原CBDB420171苻洛來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='生卒null、無KIN；source38833重修天津府志section_id=90888、Q65835983。',interpretation='原府志段未取，同名本地官目不可未讀就當滅代苻洛；三正史軍支獨立保留。'))
    kg=chunk_ev('sk_01','二十七年，春正月，遣使入<晋>朝贡','三國史記百濟近肖古王27年372朝晋','重讀近肖古26年攻平壤、27年372朝晋、28年373朝晋、30年375十一月薨，與JK咸安二372六月授當任百濟王余句鎮東樂浪守同期唯一在位王可橋；不因下一代近仇首相近名而合。')
    results[9484].update(identity_status='baekje_yugu_372_reigning_king_same_geunchogo_chronological_office_bridge',candidate_duplicate_of=12794)
    results[9484]['evidence'].append(kg)
    results[9484]['historical_death']=date(375,chunk_ev('sk_01','冬十一月，王薨。古记云','三國史記近肖古30年375十一月薨','本人近肖古本紀，前述27年372朝晋與余句授封橋；無生年，故不從346即位反填生日。'),month=11)
    results[9484]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[9484]['review_status']='primary_text_corroborated'
    mb=evidence(SOURCE/'_cache/texts/晋书.txt','慕容宝，字道祐，垂之第四子','晉書載記24慕容寶完整本傳','垂子、盛父、秦太子洗馬至後燕嗣帝，與SL崔宏傳太祖征寶、TJ垂子寶、CBDB父垂子盛全部身份可閉。')
    md=evidence(SOURCE/'_cache/texts/晋书.txt','弑之，时年四十四，在位三年，即隆安三年也','晉書慕容寶本傳399卒44','原文本身明隆安三399、44歲，非通鑑398；不暗改隆安三為二，也不拿原庫45齡取代原書44。')
    mt=evidence(SOURCE/'_cache/texts/资治通鉴.txt','引宝入龙城外邸，弑之','資治通鑑隆安二398四月丁亥寶為蘭汗弑','已實讀398年標題、四月壬戌後至丁亥到索莫汗陉、外邸被弑同段、其後六月標題；屬四月丁亥而不是六月或公曆換算。')
    results[9486].update(review_status='primary_text_corroborated',identity_status='murongbao_daoyou_chui_son_sheng_father_three_keys_closed_real_year_conflict',historical_birth=None,historical_death=dict(year=None,precision='conflicting_source_years',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only',source='晉書399卒44 / 資治通鑑398四月丁亥卒',alternatives=[dict(year=398,month=4,day_ganzhi='丁亥',source=mt['citation'],excerpt=mt['excerpt']),dict(year=399,source=md['citation'],reported_age=44,excerpt=md['excerpt'])]))
    results[9486]['evidence'].extend([mb,md,mt,dict(citation='原CBDB31262慕容寶父子日期',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父95217慕容垂、子31263慕容盛；birth355 death399 age45；notes Giles p591–592；source66734 ctext667282、Q1076801及Murong Bao。',interpretation='原庫親族與完整本傳身份閉，但35539945與JS39944不一致，且TJ卒年398；不以原庫數值為獨立古證。')])
    results[9486]['source_reported_chronologies']=[dict(source='晉書399卒44',death_year=399,conditional_birth_year=356,calculation='399-44+1=356，虛歲條件'),dict(source='TJ398卒配JS44',death_year=398,conditional_birth_year=355,calculation='398-44+1=355，跨源配算條件'),dict(source='CBDB31262/Giles注',birth_year=355,death_year=399,reported_age=45,source_level='secondary_database_not_independent_early_text')]
    original_wiki={r['id']:r for r in map(json.loads,(ROOT/'out/person_chronology/original_source_evidence.jsonl').read_text(encoding='utf-8').splitlines()) if r.get('id') in [9491,9492]}
    for ident in [9491,9492]:
        w=original_wiki[ident]['wikipedia_originals'][0]
        assert w.get('identity_check')=='qid_matches_source_record'
        r=results[ident]
        r.update(historical_birth=None,historical_death=None,duplicate_of=None,review_status='secondary_source_checked',source_identities_all_adjudicated=True,identity_status='original_wikipedia_qid_title_subject_identity_verified_and_personal_passages_read',last_manual_review_batch='round54b_original_wiki')
        r['evidence'].append(dict(citation=w['title']+' 原Wikipedia正文',url=w['url'],revision_id=w['revision_id'],page_id=w['page_id'],wikidata_qid=w['wikidata_qid'],identity_check=w['identity_check'],content_sha256=w['content_sha256'],source_level='secondary_tertiary_article_primary_references_not_independently_read',excerpt=w['lead_wikitext'][:400] if ident==9492 else 'Verus was nominated for a second consulship in 180, but he died in 179. <ref>Birley, Fasti of Roman Britain, p.121</ref>',interpretation='已讀原導言及本人履歷/結局，QID與單來源key相同；不是同名標題現代人物，不聲稱已獨立親讀所引Birley原書或原銘文。'))
    results[9491]['historical_death']=dict(year=179,precision='year',calendar='historical_CE_year',source='Wikipedia Gnaeus Julius Verus original revision1361593591 citing Birley Fasti p121',excerpt='Verus was nominated for a second consulship in 180, but he died in 179.',source_level='secondary_tertiary')
    results[9491]['source_reported_chronologies']=[dict(source='原Wikipedia本文援Birley Fasti p118f',office='tribune in legio X Fretensis',year_range=[132,135],interpretation='已服役區間，不當132本人真正初仕；若需登場上界135，仍須維持來源二手層級，不稱銘文已親核。')]
    results[9492]['historical_birth']=dict(year=120,precision='circa_year',calendar='historical_CE_year',source='Wikipedia Quintus Antistius Adventus original revision1320796856 lead',excerpt='(c. 120 AD – after 175 AD)',source_level='secondary_tertiary')
    results[9492]['historical_death']=dict(year=None,after_year=175,precision='after_year',calendar='historical_CE_year',source='Wikipedia Quintus Antistius Adventus original revision1320796856 lead',excerpt='(c. 120 AD – after 175 AD)',source_level='secondary_tertiary')
    for ident,note in {9476:'JS301廟祈巫祝楊珍與CBDB子真=魏書北魏太祖入國上谷守楊珍族系缺橋，兩scope分隔，原436估不填整人史卒；不憑同名硬定二人也不全合。',9477:'相龍與朱靈寶海西公嬖人兩key同，御覽相龍計好句或標點異保；莫能審虛實為原文限定，不當三子生父確證。',9479:'彭知正晋興相與辛章趙疑議斷秦糧單JS，不套常據席仂史景戰死，無生卒。',9480:'苻洛三正史同376滅代，但宋書說擒犍長安 vsWS犍云中崩及窟咄被遷有叙別；CBDB天津府志原段未讀全條hold，無生卒。',9481:'趙疑西平相與辛章彭知正單JS，不把別人戰死套自己。',9483:'辛章廣武守單JS，保城謀糧道未具死，不定376卒。',9484:'余句372百濟當任王與SK近肖古27年朝晋、在位346–375唯一國王時序橋明，375十一月薨無生年；候選12794根片target待回證，不整刪。',9485:'馬達龍驤秦軍前萬人請降，降而軍散非被殺，未知生卒。',9486:'慕容寶垂子盛父三keys閉，JS39944 vsTJ398四月丁亥卒真源差，CBDB35539945另二手差；歷史生空卒yearnull保源，不能照程序常見單年覆蓋。',9487:'熊邈故趙將作功曹後秦將作長史尚方丞，器玩宮室評語非生卒紀年，單SL史生卒不詳。',9491:'Gnaeus Julius Verus原文Q1376479身份明，179卒不只Short description而正文援Birley明說；出生未知，132–135軍護民官是履歷區間非已證真正初仕，仍二手層級。',9492:'Quintus Antistius Adventus原文Q1365424身份明，約120生、175後卒；175不可改確卒，治理不列顛約173–176作者亦明very tentatively，保估層級，不由120+16造確初仕136。'}.items():results[ident]['unresolved'].append(note)
    round55a_reads={7001:[('hy_00','以仲兄霸为中领军'),('sl_09','以仲兄霸为镇南中领军'),('tj_15','以兄霸为中领军')],7002:[('sl_02','将军杜勲鱼汲死之'),('sl_09','斩秋将杜勲汲鱼'),('tj_16','斩其将杜勋、汲鱼'),('js_14','遣参军杜勋献马'),('js_15','斩秋将杜勋、汲鱼'),('sl_08','遣参军杜勲献马')],7003:[('sl_02','以杜群为辽东相'),('tj_12','平原杜群、刘翔'),('tj_15','以杜群为辽东相'),('sl_03','同郡刘翔杜群')],7004:[('js_08','督选杨璋'),('sl_00','帅督护杨璋')],7005:[('hy_00','宜都太守犍为杨芬'),('sl_09','宜都太守犍为杨芬')],7006:[('sl_02','斩其将荣伯保'),('tj_15','斩其将荣伯保')],7010:[('sl_01','舍人王子春'),('tj_12','舍人王子春'),('tj_13','王子春等及王浚使者')],7011:[('tj_12','河内王尼见绥书'),('js_00','城阳王尼见绥书疏'),('js_05','毕卓、王尼、阮放')]}
    for ident,passages in round55a_reads.items():
        results[ident].update(source_identities_all_adjudicated=True,identity_status='early_queue_all_source_key_subjects_individually_read_and_bridged',last_manual_review_batch='round55a_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵hold回補逐鍵覆核 '+ch,'原文本人語境實讀，僅身份全閉才解除allkeys門檻；日期異說與混同分支另存。'))
    db=chunk_ev('hy_00','而霸、保皆暴病死','華陽國志李期咸康二336霸保暴病死','前文咸康元335、二年336具名霸保，李雄子李期兄；SL同336稱不病、皆云期鴆殺屬異說，任氏傳其後盡為壽殺是晚出總括抵牾，保獨立當代華陽明年不以總括改338。')
    results[7001]['historical_death']=date(336,db)
    results[7001]['historical_birth']=None
    results[7001]['evidence'].extend([db,chunk_ev('sl_09','霸镇北大将军保不病而死皆云期鸩杀之','晚出SL玉恒二336霸保死因異說','與華陽暴病死原句異，皆云是傳聞限定，不能當確證期謀殺；任氏附傳又總說諸子盡為壽殺保源矛盾，不轉成另一確切本人卒年。')])
    results[7001]['source_reported_death_cause_variants']=[dict(source='華陽國志',statement='霸保皆暴病死'),dict(source='SL重編蜀錄李期',statement='不病而死皆云期鴆殺',source_level='late_reconstruction_hearsay'),dict(source='SL重編任氏傳',statement='諸子其後盡為壽殺',source_level='late_reconstruction_generalization_conflicts_with_named336_deaths')]
    d347=chunk_ev('tj_16','斩其将杜勋、汲鱼','資治通鑑永和三347謝艾破麻秋斬杜勳','本人是後趙麻秋軍將，與早先張軌參軍獻馬是否轉仕未見橋；日期只屬軍將分支。')
    results[7002].update(review_status='needs_further_research',source_identities_all_adjudicated=False,historical_birth=None,historical_death=None,identity_status='liang_early_zhanggui_envoy_and_later_zhao_maqiu_commander_not_bridged',components=[dict(keys=[k for k in results[7002]['keys'] if not (k.startswith('js:js_14') or k.startswith('sl:sl_08'))],identity='347後趙麻秋軍將杜勳/勲為謝艾斬',historical_birth=None,historical_death=date(347,d347)),dict(keys=[k for k in results[7002]['keys'] if k.startswith('js:js_14') or k.startswith('sl:sl_08')],identity='張軌參軍杜勳獻朝馬500匹毯布30000匹',historical_birth=None,historical_death=None)])
    results[7002]['evidence'].append(d347)
    results[7006]['historical_birth']=None
    results[7006]['historical_death']=date(336,chunk_ev('tj_15','斩其将荣伯保','資治通鑑咸康二336三月榮伯保被斬','已讀336年標題與三月段；SL同336作七月，月份源異故頂層只保年，不暗選精月。'))
    results[7006]['source_reported_chronologies']=[dict(source='資治通鑑336',death_year=336,death_month=3),dict(source='SL重編336',death_year=336,death_month=7,source_level='late_reconstruction')]
    wn=evidence(SOURCE/'_cache/texts/晋书.txt','王尼，字孝孙，城阳人也，或云河内人','晉書王尼完整本傳姓名籍貫兩說及何綏警語','同傳明城陽或河內，何綏書警語橋TJ及JS00，名士放達與JS05謝鯤畢卓阮放友圈相符；洛陷後江夏、王澄卒後父子餓死無明年，不把後文謝鯤322仕歷當王尼仍活。')
    results[7011]['evidence'].append(wn)
    results[7011]['historical_birth']=None
    results[7011]['historical_death']=None
    for ident,note in {7001:'回補3keys同李雄子期兄中領軍鎮南，獨立華陽明336暴病死，晚SL不病鴆殺與任傳諸子盡壽殺源別保，無生年。',7002:'回補六key全讀發現原347卒不可套整條：張軌獻馬使臣(JS14/SL08)與347趙將(SL02/09,TJ16,JS15)缺轉仕橋，top生卒撤空、347存軍支，不先斷兩人或整合。',7003:'杜群平原同宋該劉翔先王浚段氏歸慕容廆後遼東相四key閉，群號非群臣，不套封奕或宋該卒年。',7004:'楊璋李矩督選/督護夜擊劉暢同二key，軍職記法源異，無生卒。',7005:'楊芬犍為宜都守二key同313涪陵北出枳被李恭費黑俘，俘非死；鄭融/程融與蘭繼/維為同行其他姓名異不套本人。',7006:'榮伯保段遼將同馬兜山被封奕斬二key閉，年同336，TJ三月vsSL七月，頂層只確年留月異。',7010:'王子春石勒舍人313末奉偽降王浚、314回報同三key連續使節事件，董肇/筆為另一姓名異，不合前秦王子春無證支，無生卒。',7011:'王尼孝孫完整本傳明城陽或河內，TJ何綏書與JS00同本人，JS05放達友群可閉；本傳王澄卒後父子餓死無年，不能拿傳後王敦逆事延活322，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    round55b_reads={7008:[('js_10','熊远，字孝文'),('js_11','敦深惮其正而有谋，引为长史'),('tj_12','熊远上书'),('tj_13','散骑常侍熊远')],7013:[('sl_02','襄平令王永'),('sl_03','襄平令王永')],7014:[('js_12','太傅参军王玄代翜为郡'),('js_03','子玄，字眉子'),('js_05','名士王玄、阮修'),('ssxy_01','王字眉子夷甫子'),('ssxy_02','庾太尉少为王眉子所知'),('ssxy_03','王太尉问眉子汝叔名士')],7015:[('tj_12','参军王贡告捷'),('tj_13','弢使王贡出挑战'),('js_09','王贡还，至竟陵')],7016:[('sl_02','乐浪王遵说统'),('tj_12','乐浪王遵说统')],7018:[('sl_02','安定皇甫岌岌弟真'),('tj_12','崔毖请皇甫岌为长史'),('sl_03','其兄岌晋东夷校尉')],7019:[('sl_02','皇甫岌岌弟真'),('sl_03','皇甫真字楚季'),('sl_04','皇甫真为奉车都尉'),('tj_12','岌与弟真即时俱至'),('tj_15','主簿皇甫真切谏'),('tj_17','典书令皇甫真'),('tj_18','皇甫真言于恪')]}
    for ident,passages in round55b_reads.items():
        results[ident].update(source_identities_all_adjudicated=True,identity_status='early_queue_all_source_key_subjects_individually_read_and_bridged',last_manual_review_batch='round55b_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵逐鍵本人回補 '+ch,'本人原段已實讀，別字與省姓按字、父子、官職、同一事件橋合；無明年不填歷史生卒。'))
    results[7008].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='xiongyuan_four_jin_chronicle_keys_closed_cbdb_shaoxing_gazetteer_unretrieved',historical_birth=None,historical_death=None,components=[dict(keys=[k for k in results[7008]['keys'] if not k.startswith('cbdb:')],identity='熊遠孝文豫章南昌、元帝主簿至會稽內史敦長史',historical_birth=None,historical_death=None),dict(keys=[k for k in results[7008]['keys'] if k.startswith('cbdb:')],identity='CBDB413456紹興府志熊遠原段尚未取',historical_birth=None,historical_death=None)])
    results[7008]['evidence'].append(dict(citation='原CBDB413456熊遠來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='生卒null、無KIN；source3689紹興府志(1424) section_id=62636、Q28414107、熊远。',interpretation='名人可能同會稽內史但原志未讀，不假稱全文已核；JS完整本傳王敦至石頭後為敦長史數月病卒無明年，不硬鎖某年。'))
    results[7014]['evidence'].append(chunk_ev('ssxy_01','后行陈留太守大行威罚为坞人所害','世說識鑒王玄眉子陳留威罰被害','與JS12梁國內史將遷陳留殺耿奴、餘黨殺玄的官序及威殺可橋，JS03荀籓陳留將赴祖逖為盜害為另一敘述；不憑父衍311死便把玄同年卒。'))
    results[7014]['source_reported_death_circumstance_variants']=[dict(source='JS12卞翜傳',statement='由梁國將遷陳留，斬耿奴後餘黨殺玄'),dict(source='JS03王衍子玄傳',statement='陳留守屯尉氏將赴祖逖為盜害'),dict(source='SS識鑒晉諸公贊',statement='行陳留守大行威罰為塢人害')]
    results[7015]['evidence'].append(chunk_ev('js_09','贡遂来降。而韬败走','晉書陶侃傳王貢參軍反叛後轉杜弢軍再降','本傳先本人告捷王敦、矯命用杜曾而反，再杜弢將王貢挑戰、被陶侃勸降，原文直接閉早使臣與後賊將，不誤判同名兩人。'))
    for ident,note in {7008:'四正史熊遠孝文主簿諫元帝、會稽內史拒沈充至敦長史同，無明生卒；CBDB紹興府志1424本原段未取wholehold，不能只因有同名會稽守就當五key已核。',7013:'王永襄平令與孫機舉遼東應仁同兩key，孫機將軍/前大司農記法異為他人，不作王永身份衝突，生卒未知。',7014:'王玄字眉子衍子、澄侄、陳留/梁國守六key已閉，名士謝鯤友群與字父祖接；被害敘事JS12斬耿奴餘黨vsJS03赴祖逖盜害源別保，無精年不套衍311卒。',7015:'王貢陶侃參軍告捷→矯命用曾而叛→弢將→勸降，JS09完整本傳直接官事橋TJ12/13，不因兩軍服務就自拆同名；未知生卒。',7016:'王遵樂浪人說張統率千家歸廆拜參軍兩key同，不能誤取本卷彭城王遵石遵同字；未知生卒。',7018:'皇甫岌安定崔毖請長史不就、與弟真同歸廆三key同，SL03省兄岌不漏，無明生卒。',7019:'皇甫真楚季岌弟七key典書秘書顧命恪與秦奉車同鏈閉；弱冠拜郎與來歸概述不硬配311推生年，數歲卒無精年，未知生卒。'}.items():results[ident]['unresolved'].append(note)
    round55c_reads={7021:[('sl_08','太府参军索辅'),('js_14','太府参军索辅')],7022:[('sl_02','兰陵缪恺'),('tj_12','兰陵缪恺')],7023:[('js_15','临洮人翟楷'),('sl_00','临洮人翟楷'),('sl_02','东夷校尉翟楷'),('sl_03','仁以司马翟楷'),('sl_08','临洮人翟楷'),('tj_15','慕容仁以司马翟楷')],7026:[('sl_01','王子春董笔'),('tj_12','王子春、董肇'),('tj_13','复遣董肇奉表')],7028:[('hy_00','汶山太守涪陵兰维'),('tj_12','汶山太守兰维')],7030:[('sl_00','荡晋将军兰阳'),('tj_12','荡晋将军兰阳')],7031:[('sl_02','以河东裴嶷'),('tj_12','裴嶷清方有干略'),('tj_13','裴嶷言于廆'),('tj_14','裴嶷至建康'),('sl_03','裴嶷字文冀')],7032:[('sl_02','宋奭及裴开'),('tj_12','嶷与武子开'),('tj_14','裴开为司马'),('tj_15','长史裴开为军谘祭酒'),('sl_03','武子开字士先')],7035:[('hy_00','恭、费黑所破获'),('sl_09','雄遣李恭费黑'),('tj_12','成将李恭、费黑'),('tj_14','征南将军费黑'),('tj_15','寿督征南将军费黑')],7036:[('js_17','辛谧，字叔重'),('sl_00','杜骜辛谧及北宫纯'),('sl_02','征士辛谧'),('tj_17','故晋散骑常侍陇西辛谧')],7040:[('sl_03','丁衡游毅孙机'),('tj_15','丁衡、游毅、孙机')],7041:[('sl_02','广平游邃'),('tj_12','广平游邃'),('tj_13','以游邃为龙骧长史'),('tj_14','裴嶷、游邃为长史'),('sl_03','游邃广平人也')]}
    for ident,passages in round55c_reads.items():
        results[ident].update(source_identities_all_adjudicated=True,identity_status='early_queue_all_source_key_subjects_individually_read_and_bridged',last_manual_review_batch='round55c_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人逐鍵回補 '+ch,'本人及相連官事原文實讀，缺年不猜；跨身份未有橋者分支而不整條套日期。'))
    zk=chunk_ev('tj_15','皝兵追及楷、鉴，斩之','資治通鑑咸康二336慕容仁敗翟楷被斬','身份為仁司馬領東夷校尉，非未經橋合的323臨洮逐縣令者；336只放燕分支。')
    results[7023].update(review_status='needs_further_research',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,identity_status='323_lintao_rebel_and334_murongren_sima_di_kai_not_bridged',components=[dict(keys=[k for k in results[7023]['keys'] if k.startswith('js:') or k.startswith('sl:sl_00') or k.startswith('sl:sl_08')],identity='323臨洮人翟楷石琮逐縣令應劉曜',historical_birth=None,historical_death=None),dict(keys=[k for k in results[7023]['keys'] if k.startswith('tj:') or k.startswith('sl:sl_02') or k.startswith('sl:sl_03')],identity='334–336慕容仁司馬東夷校尉翟楷',historical_birth=None,historical_death=date(336,zk))])
    results[7023]['evidence'].append(zk)
    results[7036]['historical_birth']=None
    results[7036]['historical_death']=date(350,chunk_ev('tj_17','因不食而卒','資治通鑑永和六350辛謐不食卒','本人原段上故晉散騎常侍陇西辛謐、冉閔備禮征太常及書，已讀350年標題；不以常見二手351套。'))
    results[7040]['historical_birth']=None
    results[7040]['historical_death']=date(336,chunk_ev('tj_15','丁衡、游毅、孙机等，皆仁所信用也','資治通鑑336正月慕容仁敗游毅等被斬','同段皝執而斬之，獨立TJ確年；非SL孤據確證。'))
    wy=chunk_ev('sl_03','执而斩之王永自杀幼稚佟寿郭充翟楷厐鉴皆东走','SL重編慕容仁敗王永自殺與TJ王冰互文名異','逐句順序：丁衡游毅孫機信用執斬→王永自殺→幼稚佟壽郭充翟楷龐鑒東走；TJ同一獨事件序列只該位作王冰，永冰字形傳異，非僅同名或同卷別人。')
    results[7013].update(review_status='duplicate',duplicate_of=7009,identity_status='wangyong_wangbing_character_variant_same_murongren_suicide_unique_parallel_sequence')
    results[7013]['evidence'].extend([wy,chunk_ev('tj_15','皝执而斩之；王冰自杀','資治通鑑王冰自殺獨事對讀','與SL原句位置前後名組完全同，可橋王永/冰；早獨立TJ規範7009已單key閉，7013保原襄平令兩key及異文。')])
    for ident,note in {7021:'索輔張軌太府參軍請復五銖兩key同，軌314卒60非本人；無生卒。',7022:'繆愷蘭陵與皇甫兄弟同典慕容廆機要二key閉，非其他繆氏近名；無生卒。',7023:'回補六key分323臨洮應曜與334–336燕仁司馬兩scope，無轉職族橋，336僅燕支、不套整人；兩地先後並非證明必異人，但也不直接同名合。',7026:'董肇/董筆與王子春同石勒313–314偽奉王浚使三key閉，名字異同獨使事保，不誤作董昭或其他同名。',7028:'蘭維涪陵汶山守向沈卒後推西夷、北出被李恭費黑俘同二key；被獲不等被殺，無生卒。',7030:'蘭陽漢蕩晋將軍助守西平拒劉琨普根同兩key，軍令不是本人登場精年，無生卒。',7031:'裴嶷文冀昶子武弟、攜武子開投廆、長史使建康返至遼東相樂浪守五key閉；無明生卒，不把兄武喪套本人。',7032:'裴開武子士先、嶷侄、車騎司馬至軍諮祭酒五key閉，SL同卷下一高瞻子高開352重創卒非裴開，無生卒。',7035:'費黑成雄將擒蘭維313、征南討斯叟、330從壽取巴東五key軍鏈閉，無生卒，不把李雄或李期年套本人。',7036:'辛謐叔重陇西狄道兼散騎慰關中、311俘於漢、石氏征不就、冉閔征太常不食卒四key同，TJ明350年，JS無齡不填生年。',7040:'游毅丁衡孫機同仁信任被皝執斬兩key閉，TJ336明記獨立支持，非游邃或游𪽈近親。',7041:'游邃廣平長史、兄游𪽈被王浚召害、子泓居就令五key閉，長史整朝儀與配裴嶷同官鏈，兄314死不能套本人。',7013:'本輪續讀SL03結局王永自殺，與TJ王冰在丁衡游毅孫機執斬後、幼稚佟壽名組東走前唯一同句互文，確屬永/冰名異；最終整人歸7009，保襄平令原keys。'}.items():results[ident]['unresolved'].append(note)
    dk=evidence(SOURCE/'_cache/texts/晋书.txt','子逵，字伯道，逵弟畅，字仲远','晉書刁協傳子彝附刁逵完整段','刁彝子逵、廣州刺史平越中郎將至西中郎豫州鎮歷陽，與CBDB11931父11930彝及廣州顯宦本人可橋；原庫弘列子而本傳弟序不一另存關係源別，不能把弘去向套逵。')
    dkd=evidence(SOURCE/'_cache/texts/资治通鉴.txt','逵弃城走，为其下所执，斩于石头','資治通鑑元興三404刁逵被執斬石頭','已讀404義軍起後建康議官、諸葛長民被槛車至當利出走、逵棄歷陽被執斬及弟子雍送秦，具名非泛族誅；不取後世社錢報怨評語為另一次死亡。')
    results[7053].update(review_status='primary_text_corroborated',historical_birth=None,historical_death=date(404,dkd),source_identities_all_adjudicated=True,identity_status='cbdb11931_diaoyi_son_guangzhou_governor_diaokui_bodao_jin_tj_bridge',last_manual_review_batch='round55d_cbdb_primary_closure')
    results[7053]['evidence'].extend([dk,dkd,dict(citation='原CBDB11931刁逵身份日期限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父11930刁彝、c_kin_code182長子11932刁弘；生卒齡皆0表示未載；notes廣州太守富資及族系注、sourcectext92474、Q28415853。',interpretation='原0不可當公元0；父名及廣州官與JS完整段同人可閉，但弘親等源異不得私改子為弟或反向把整個刁弘記錄強合。')])
    results[7053]['source_reported_relationship_variants']={'diao_hong':[dict(source='CBDB11931/11932',relationship='逵長子弘'),dict(source='晉書刁協附傳',expression='子逵…逵弟暢…次子弘…兄弟子侄',interpretation='該段親等表述/排序與原庫不同，保原文，不借本次逵身份閉合替弘另記錄消除爭議。')]}
    results[7053]['unresolved'].append('本次原庫父彝廣州官與JS伯道逵橋成，TJ404本人棄城被執石頭斬明卒年；出生不詳，弘父子/兄弟關係原文與庫別保，不套弘404被宋書誅或JS亡不知所之源別到逵。')
    round56a_reads={7044:[('js_13','声将阎晋、郑进皆死战'),('sl_02','歴阳太守郑进'),('tj_16','义阳太守郑进皆降')],7046:[('sl_02','北平阳耽'),('tj_12','北平阳耽'),('tj_14','阳耽为军谘祭酒')],7047:[('hy_00','始平太守胡子序领州'),('tj_12','始平太守胡子序领梁州')],7048:[('sl_02','代郡鲁昌'),('tj_12','代郡鲁昌'),('tj_13','征虏将军鲁昌说廆')],7049:[('sl_02','辽东相厐鍳单骑遁走'),('sl_03','厐鉴领辽东相'),('tj_15','前平州别驾庞鉴领辽东相')],7050:[('js_13','丁潭，字世康'),('tj_15','侍中丁潭侍从'),('js_12','丁潭有公望而无公才'),('sg_16','子弥字钦逺仕晋至梁州刺史孙潭'),('ssxy_02','丁潭有公望而无公才')]}
    for ident,passages in round56a_reads.items():
        results[ident].update(source_identities_all_adjudicated=True,identity_status='early_queue_all_source_key_subjects_individually_read_and_bridged',last_manual_review_batch='round56a_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人原文回補 '+ch,'逐鍵本人及相接事件實讀，身份疑點或年齡異文另存，不以死戰當死亡。'))
    results[7044].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='guosheng_general_and339_yiyang_taishou_zhengjin_career_bridge_not_located',components=[dict(keys=[k for k in results[7044]['keys'] if k.startswith('js:')],identity='朱伺討郭聲時聲將鄭進死戰',historical_birth=None,historical_death=None),dict(keys=[k for k in results[7044]['keys'] if not k.startswith('js:')],identity='339夔安攻荊州時鄭進太守降趙',historical_birth=None,historical_death=None)])
    results[7044]['historical_birth']=None
    results[7044]['historical_death']=None
    results[7044]['source_reported_office_variants']=[dict(source='SL重編',office='歷陽太守'),dict(source='資治通鑑',office='義陽太守')]
    results[7047]['historical_name_correction']={'name':'胡子序','reason':'胡為姓，原資料鬍是繁化所致，非鬍鬚稱謂；原keys照存。'}
    results[7050]['historical_birth']=None
    results[7050]['historical_death']=None
    results[7050]['last_attested_activity']=342
    results[7050]['source_reported_age_at_death']={'age':80,'calendar_year':None,'source':'晉書丁潭本傳','death_year_not_before':342,'interpretation':'康帝342即位後屢求退、退居後80卒，342僅在世下界，不當精卒年或生263。'}
    results[7050]['source_reported_relationship_variants']={'relation_to_dinggu':[dict(source='JS丁潭傳及SG虞翻注',relationship='孫，父彌、祖固'),dict(source='SS會稽後賢記注',relationship='固曾孫')],'interpretation':'同世康山陰、光祿與王導評語身份明；輩數源異保，不能假裝一支原書未寫曾孫。'}
    for ident,note in {7044:'郭聲將鄭進與339太守鄭進缺投降後轉官橋，未定必異人，暫兩scope待核；死戰是盡力作戰不是本人已死，不能因這詞就造卒日。SL歷陽vsTJ義陽另官郡源差。',7046:'陽耽北平廆謀主至軍諮祭酒三key同，非陽裕名字近即合，無生卒。',7047:'胡子序始平守張光死後推梁州、守漢中不能退走兩key同，胡姓非鬍鬚；逃走不等死，無生卒。',7048:'魯昌代郡廆謀主、征虜勸奉琅邪為帝三key同，不合魯徽等，無生卒。',7049:'龐鑑前平州別駕、仁署遼東相與翟楷同逃被追斬三key閉，獨立TJ336明死年。',7050:'丁潭世康山陰吳固孫彌子五key身份閉；SG省姓名孫潭光祿吻合，不漏；SS作固曾孫與JS/SG祖孫輩差保。JS康帝後80卒無明年，歷史生卒不填，342僅在世下界。'}.items():results[ident]['unresolved'].append(note)
    round56b_reads={7052:[('sl_01','东曹掾傅遘'),('sl_08','伷伷子遘仕后赵至太常')],7056:[('sl_00','西阳王璇等皆领武卫'),('tj_12','西阳王璇皆领武卫')],7057:[('sl_01','晋尚书燕国刘翰'),('tj_13','故尚书燕国刘翰'),('sl_03','燕国刘翰徳素长者')],7059:[('sl_00','光禄大夫单冲'),('tj_12','光禄大夫单冲')],7060:[('sl_01','冯翊严嶷'),('tj_12','冯翊严嶷'),('wsh_01','王如、侯脱、严嶷'),('js_03','简又为贼严嶷所逼'),('js_19','冯翊严嶷')],7061:[('js_02','督护孙纬疑其诈'),('sl_01','浚督护孙纬'),('tj_13','王浚督护孙纬')],7062:[('sl_09','郡主簿张宅梦'),('js_17','郡主簿张宅梦')],7066:[('sl_08','长史张玺等表实'),('tj_13','长史张玺等表世子实')],7067:[('sl_01','广平游纶张豺'),('sl_02','戎昭将军张豺'),('sl_06','张豺之辅石世'),('tj_12','广平游纶、张豺'),('tj_16','戎昭将军张豺'),('tj_17','张豺为镇卫大将军')],7068:[('js_08','河南徐润者'),('sl_00','河南徐润以音律'),('tj_12','河南徐润以音律')],7069:[('sl_01','腰斩成公段'),('tj_15','赵左校令成公段')],7071:[('sl_00','上党李熹'),('tj_12','李熹见之')],7072:[('sl_01','中庶子李颜'),('tj_15','中庶子李颜'),('wsh_01','中庶子李颜'),('sl_02','中庶子李颜')]}
    for ident,passages in round56b_reads.items():
        results[ident].update(source_identities_all_adjudicated=True,identity_status='early_queue_all_source_key_subjects_individually_read_and_bridged',last_manual_review_batch='round56b_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人原文回補 '+ch,'原文本人段落與上下官事實讀，曖昧身份分支另隔，生卒未明不補。'))
    results[7052].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='fu_gou_dongcao_envoy_and_fu_rong_ancestor_taichang_genealogy_not_explicitly_bridged',components=[dict(keys=[k for k in results[7052]['keys'] if k.startswith('sl:sl_01')],identity='314石勒東曹掾傅遘兼左長史獻王浚首',historical_birth=None,historical_death=None),dict(keys=[k for k in results[7052]['keys'] if k.startswith('sl:sl_08')],identity='傅融祖系傅伷子傅遘後趙太常',historical_birth=None,historical_death=None)])
    results[7061].update(review_status='needs_further_research',source_identities_all_adjudicated=False,identity_status='sunwei_wangjun_duhu_and_xts_sunhui_son_sunzhou_father_unbridged',components=[dict(keys=[k for k in results[7061]['keys'] if not k.startswith('cbdb:')],identity='314王浚督護孫緯識石勒詐入並遮擊',historical_birth=None,historical_death=None),dict(keys=[k for k in results[7061]['keys'] if k.startswith('cbdb:')],identity='CBDB31582孫輝子孫周父、新唐書73B祖系孫緯',historical_birth=None,historical_death=None)])
    results[7061]['evidence'].append(dict(citation='原CBDB31582孫緯族系來源限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='父31581孫輝、子31583孫周；生卒0未知；notes XTS73B.3b，Q45419955。',interpretation='原族表頁尚未取得，未見王浚督護與此父子族系的直接轉接，不用同名把整人歸一。'))
    zc=chunk_ev('tj_17','斩张豺于平乐市，夷其三族','資治通鑑永和五349五月庚寅張豺被斬','已讀349年石虎末疾、五月石遵自李城起軍，己丑執豺、庚寅入城斬平樂市，辛卯廢世；庚寅為原農曆干支，不換公曆。')
    results[7067]['historical_birth']=None
    results[7067]['historical_death']=date(349,zc,month=5,day_ganzhi='庚寅')
    results[7067]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7067]['evidence'].append(zc)
    cd=chunk_ev('tj_15','赵王虎恶之，腰斩成公段','資治通鑑咸康三337正月庚辰成公段被腰斬','本年正月庚辰群臣上尊號、庭燎油灌燒死人而斬造者，下句辛巳稱大趙天王，死亡干支承前庚辰。')
    results[7069]['historical_birth']=None
    results[7069]['historical_death']=date(337,cd,month=1,day_ganzhi='庚辰')
    results[7069]['historical_death'].update(precision='lunar_month_ganzhi_day',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7069]['evidence'].append(cd)
    la=chunk_ev('tj_15','虎怒，收李颜等诘问','資治通鑑337七月石邃陰謀案李顏被殺','七月飲於顏舍、太子行冒頓事計，顏被收具始末後虎殺之；同年非侯子光佛太子，是另一案。')
    results[7072]['historical_birth']=None
    results[7072]['historical_death']=date(337,la,month=7)
    results[7072]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7072]['evidence'].append(la)
    lx=evidence(SOURCE/'_cache/texts/晋书.txt','帝畴咨将帅，上党李憙曰','晉書劉元海載記上黨李憙推薦劉淵的獨立早源橋','SL00同一問將、孔恂蛟龍雲雨答語、元海稱王渾李憙鄉曲稱達，原JS確寫憙；與3869季和上黨銅鞮李憙本傳同名同籍同晉廷，非靠後世wiki。')
    results[7071].update(review_status='duplicate',duplicate_of=3869,identity_status='lixi_xi_xi_variant_js_liuyuan_parallel_event_shangdang_same_li_xi_closed_target')
    results[7071]['historical_birth']=None
    results[7071]['historical_death']=None
    results[7071]['evidence'].extend([lx,evidence(SOURCE/'_cache/texts/晋书.txt','李憙，字季和，上党铜鞮人','晉書李憙本人本傳與劉元海傳同人對讀','已讀本人司隸太子太傅至光祿、及卒無年，本片不另取常見208297；0850片代理3869六keys已全閉並親讀元海原段同意去重。')])
    results[7071].pop('candidate_duplicate_of',None)
    for ident,note in {7052:'314石勒東曹獻首使與傅融祖系後趙太常傅遘仍缺族官轉接，可能同人但不靠稀名同朝硬閉；兩key原段均讀，BD未知。',7056:'劉璇/璿漢西陽王武衛典禁兵，劉和命攻乂反挾乂歸聰同兩key，原文省姓不漏，非蜀漢太子劉璿；未知生卒。',7057:'劉翰燕國王浚故尚書、石勒署幽州又奔段、枣嵩薦德素長者三key同王浚敗事件明橋，生卒未知。',7059:'單衝乂舅光祿大夫勸太弟退讓同二key，單是姓不是兵數，未知生卒。',7060:'嚴嶷馮翊流民與侯脫王如同起、宛援被俘送平陽五key同；JS03以洛陽陷後總敘逼山簡與SL/TJ310俘先後不同不強成死亡年，囚非殺。',7061:'孫緯王浚督護三軍事key明閉314識詐遮勒，CBDB父孫輝子孫周XTS73B祖表未取仍整條hold；原0未知不作公元0。',7062:'張宅郡主簿索紞夢占後謀反誅同二key，后三年是無起年預言，不能配田豹年硬造卒；該key本人為索紞夢占中的張宅，未套同卷其他隱士卒年，死亡年未知。',7066:'張璽張軌長史314己丑軌死後表寔攝位兩key同，314死者軌非璽。',7067:'張豺六key同312苑鄉游綸→石虎戎昭安定公主→輔石世→349五月庚寅遵斬；SL06是姚弋仲馬何羅前事回述，不因該chunk兼後燕後秦便判另張豺。',7068:'徐潤河南音律受劉琨寵晉陽令譖令狐盛三key同，被殺的是盛、遭害者琨父母，不把312陷晉陽當徐潤明卒。',7069:'成公段趙左校令作庭燎燒人受腰斬兩key同，複姓成公名段；337正月庚辰明同日，非侯子光丞相案。',7071:'李熹/李憙西晋上黨名臣兩key同，獨立JS元海傳同事明憙字、3869本傳同籍，0850片已六key閉同意→3869；TJ310只是盛少時的回溯，不當310仍活，不補208297等未實證BD。',7072:'李顏石邃中庶子四key同一337七月東宮案，SL02石邃附傳是回述不另人；子男女26死屬石邃，李顏本人收訊後殺月明無日。'}.items():results[ident]['unresolved'].append(note)
    round56c_reads={7073:[('tj_15','仇池氐王杨毅族兄初'),('yi_01','难敌卒，子毅立'),('wsh_04','难敌死，子毅立')],7075:[('js_08','幽州刺史鲜卑段匹䃅'),('sl_01','刘翰不欲从勒乃奔叚疋䃅'),('sl_02','建兴末随琨投段疋䃅'),('tj_12','疾陆眷弟匹䃅'),('tj_13','乃归段匹䃅'),('tj_14','段末柸攻段匹䃅'),('js_03','建兴末，随琨投段匹䃅'),('js_09','段匹䃅本自遐方'),('js_18','往段匹䃅遣使求效忠节'),('wsh_05','就六眷与弟匹䃅'),('ssxy_00','为段日防所害')]}
    for ident,passages in round56c_reads.items():
        results[ident].update(source_identities_all_adjudicated=True,identity_status='early_queue_all_source_key_subjects_individually_read_and_bridged',last_manual_review_batch='round56c_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人原文回補 '+ch,'已逐鍵讀本人語境，段匹/疋磾䃅/SS段日防皆以劉琨盟兄弟/害琨同事、父兄段務勿塵疾陸眷共同身份橋合；不以晚年追述當確年。'))
    yy=chunk_ev('yi_01','三年，毅族兄初袭杀毅','宋書氐胡傳咸康三337楊毅為初襲殺','前咸康元335封征南，三年337初襲殺毅，與TJ同337本人死亡，不套其後楊初355被殺年。')
    results[7073]['historical_birth']=None
    results[7073]['historical_death']=date(337,yy)
    results[7073]['evidence'].append(yy)
    dpj=chunk_ev('js_08','经年，国中谋推匹䃅为主，事露，被害','晉書段匹磾附傳本人死亡時距','完整相關本傳末稱到襄國後經年國中推立事露被害，無明死亡年；不能把入趙當日或常見二手321取作古源明卒年。')
    dpt=chunk_ev('tj_14','久之，与文鸯、邵续皆为后赵所杀','資治通鑑段匹磾321入趙後追敘結局','321被俘授冠軍後久之才與文鴦邵續皆殺，是附敘後事，不能硬鎖殺在321年；原文也未必三人同日死。')
    results[7075].update(historical_birth=None,historical_death=None,review_status='source_checked_dates_unknown')
    results[7075]['evidence'].extend([dpj,dpt])
    results[7075]['source_reported_chronologies']=[dict(source='JS段匹磾附傳',capture_year=321,death_expression='到襄國…經年…事露被害',death_year=None),dict(source='TJ321年條附敘',capture_year=321,death_expression='久之與文鴦邵續皆為後趙所殺',death_year=None,interpretation='久之非明321本人死亡；不以附敘所在年當卒年。')]
    for ident,note in {7073:'楊毅難敵子仇池左賢王下辨公三key身份閉，宋明咸康三337族兄初襲殺，TJ337同年；後楊初355被宋奴殺不能套毅，生未知。',7075:'段匹磾務勿塵子疾陸眷弟、劉琨盟友後害琨、邵續聯軍至石趙俘11key閉；SS段日防為同害劉琨原注字異不新造人。JS入襄國經年後被害、TJ321後久之附敘死亡均無卒年，拒把321被俘年硬當確卒；史生卒不詳，遊說王敦段是他人書中往事不當仍活。'}.items():results[ident]['unresolved'].append(note)
    round57a_reads={7078:[('sl_01','别将王洛生'),('tj_13','其将王洛生'),('sl_02','王洛生仕勒为部将')],7079:[('sl_00','征东将军石邃'),('sl_01','右卫将军石邃'),('tj_15','石邃将兵卫送曜'),('wsh_01','其太子邃省可尚书'),('js_17','季龙太子邃'),('sl_02','石邃字大渊')],7083:[('sl_01','北平相阳裕'),('sl_02','北平阳裕事叚疾陆眷'),('sl_03','唐国内史阳裕'),('tj_13','王浚从事中郎阳裕'),('tj_15','北平阳裕事段疾陆眷'),('tj_16','北平相阳裕')],7086:[('hy_00','仆射任颜'),('sl_09','仆射任颜谋反'),('tj_16','汉仆射任颜谋反')],7096:[('tj_13','访遣子扶帅兵三千')]}
    for ident,passages in round57a_reads.items():
        results[ident].update(source_identities_all_adjudicated=True,identity_status='early_queue_all_source_key_subjects_individually_read_and_bridged',last_manual_review_batch='round57a_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人原文回補 '+ch,'本人原文及官族事件上下已實讀，混同別名與他人死亡不套本人；省名/字形異另記。'))
    results[7078]['historical_birth']=None
    results[7078]['historical_death']=None
    results[7078]['last_attested_activity']=314
    results[7078]['appearance_upper_bound']=314
    results[7078]['evidence'].append(chunk_ev('sl_02','重以刀溃其腹出胃而死','SL重編王洛生附傳無紀年的獄中自殺','完整附傳明押浚五百騎可橋TJ軍將；後囚自殺未明年，314是押浚在世年非自殺年，也非真正初仕。'))
    results[7079]['historical_birth']=None
    sd=chunk_ev('tj_15','其夜，杀邃及其妃张氏','資治通鑑337七月石邃廢後被殺','原文父虎盛怒廢太子，當夜殺邃及妃男女26同棺；無本人卒齡，JS佛圖澄傳三日死小阿彌是邃子非邃。')
    results[7079]['historical_death']=date(337,sd,month=7)
    results[7079]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7079]['evidence'].append(sd)
    yu=chunk_ev('sl_03','阳裕字士伦右北平无终人','SL陽裕完整附傳與相鄰陽騖傳分界','本人王浚治中逃段、五主、麻秋司馬被燕執、郎中左司馬及規龍城，末61卒無年；下一段才陽騖字士秋，不能把士秋作裕別名。')
    results[7083]['evidence'].extend([yu,chunk_ev('sl_03','及死皝甚悼之比葬三临时年六十一','SL重編陽裕卒61無年','裕士倫之本傳末，下一陽騖士秋歷四朝及其子瑤是不同人，不把陽騖官齡親族日期混入裕。'),chunk_ev('tj_16','燕王皝使唐国内史阳裕等筑城','通鑑咸康七341正月陽裕築龍城','獨立早源341年標題原文已讀，具名唐國內史築龍城；341為實際已任官在世年，不是首仕年。')])
    results[7083].update(historical_birth=None,historical_death=None,last_attested_activity=341,identity_status='yangyu_shilun_six_key_subjects_closed_adjacent_yangwu_alias_rejected')
    results[7083]['source_reported_age_at_death']={'age':61,'calendar_year':None,'source':'SL重編陽裕附傳','source_level':'late_reconstruction'}
    results[7083]['rejected_source_aliases']=[dict(key=next(k for k in results[7083]['keys'] if k.startswith('sl:sl_03')),alias='陽士秋',actual_person='陽騖，字士秋',reason='相鄰傳陽騖姓名被誤採為陽裕別名；本key名與bio主體明為陽裕士倫，錯alias裁出而非把兩人合。')]
    ry=chunk_ev('tj_16','九月，汉仆射任颜谋反，诛','資治通鑑咸康四338九月任顏被誅','雄妻任太后弟，壽因誅雄諸子，與華陽及SL同案；任氏憂死或他人的卒年不套任顏。')
    results[7086]['historical_birth']=None
    results[7086]['historical_death']=date(338,ry,month=9)
    results[7086]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7086]['evidence'].append(ry)
    yg=evidence(SOURCE/'_cache/texts/魏书.txt','父野干，尚昭成女辽西公主','魏書外戚賀訥傳父野干及獻明后兄妹橋','全文賀訥是太祖元舅、獻明后兄，父野干即賀后父，祖纥尚平文女；原CBDB136033女136032賀氏拓跋珪母吻合，野不是此人的姓，幹為獨立名字誤切。無生卒記載，不取index310。')
    results[7082].update(review_status='source_checked_dates_unknown',historical_birth=None,historical_death=None,duplicate_of=None,source_identities_all_adjudicated=True,identity_status='cbdb136033_yegan_father_of_tuobagui_mother_he_weishu_hene_genealogy_bridge',last_manual_review_batch='round57a_cbdb_primary_closure')
    results[7082]['evidence'].extend([yg,dict(citation='原CBDB136033野干姓名族系限定',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='c_surname_chn野、c_mingzi_chn干；KIN176女兒136032賀氏(拓跋珪母)；生卒null；index310 type14 source136032；source9601頁3227、Q10538044、贺野干。',interpretation='已實查KINSHIP_CODES176是女兒而不是父；女賀氏拓跋珪母直接閉魏書賀訥元舅/獻明后兄、父野干。310是女兒導出的索引，不是本人活動年。')])
    results[7082]['historical_identity_correction']={'name_full':'賀野干','surname':'賀','given_name':'野干','basis':'魏書賀訥傳父野干、女獻明后與原庫女賀氏及贺野干名目同人；原庫錯拆野姓干名，遊戲原keys保留供修。'}
    results[7096].update(duplicate_of=None,candidate_duplicate_of=7384,identity_status='single_tj_key_zhoufang_son_fu_scope_clear_target_7384_mixed_hold_whole_redirect',last_attested_activity=315,appearance_upper_bound=315)
    results[7096]['historical_birth']=None
    results[7096]['historical_death']=None
    for ident,note in {7078:'王洛生三key押王浚五百騎獨事閉，314已為軍將可給登場上界及在世年；SL獄中自殺無年，不能用314押俘年當本人死亡或真正初仕。',7079:'石邃大淵阿鐵虎鄭氏長子六key閉，328衛送曜到337七月被父虎殺；JS佛圖澄小阿彌三日死是邃子，非太子本人三日卒。',7082:'野干原庫確賀氏拓跋珪母之父，魏書賀訥獻明后兄及父野干橋明，應姓名賀野干而非野姓幹名；沒有生卒，index310不可當生年或活動，未猜365等卒年。',7083:'陽裕士倫陽耽侄六key本人官事閉，341龍城築城獨立TJ明活；SL61卒無年且晚重編，陽士秋是下一陽騖的字，錯別名明確剔出，勿再拿此alias做跨人去重。',7086:'任顏李雄妻任后弟三key同，338九月謀反被壽誅；不把後其他任氏/雄子死套本人。',7096:'原TJ單key明南中郎將周訪子扶率三千同石覽救荀崧，315軍將身份無歧；名字扶原字照保。7384兼318叛彭城319死等別支，撤回整條去重維持，只候周訪子分支；不可因target混就說本單key本人未讀未明。'}.items():results[ident]['unresolved'].append(note)
    round57b_reads={7090:[('sl_00','曜遣刘雅赵染攻疋'),('sl_01','征北将军刘雅'),('tj_12','安西将军刘雅'),('tj_13','征北将军刘雅'),('tj_14','以刘雅为大司徒')],7091:[('sl_00','将军卜抽将兵监守东宫'),('tj_13','冠威将军卜抽')],7095:[('tj_13','莚叱郡传教吴曾格杀之'),('js_07','筵叱郡传教吴曾')],7099:[('sl_09','尚书姚华田褒'),('tj_15','尚书姚华'),('tj_16','景骞、田褒、姚华、许涪')],7100:[('tj_13','以告义兴太守孔侃'),('js_07','乃告乱于义兴太守孔侃'),('js_12','父侃，大司农')],7101:[('sl_00','平东将军宋哲'),('tj_13','诏平东将军宋哲屯华阴')],7102:[('sl_02','杀太守宋烛迎降'),('tj_16','共杀太守宋烛以降于赵')],7114:[('sl_09','征西将军李遐及将军李西'),('tj_16','征西将军李遐、将军李西')],7117:[('sl_09','征西将军李遐及将军李西'),('tj_16','征西将军李遐、将军李西')]}
    for ident,passages in round57b_reads.items():
        results[ident].update(source_identities_all_adjudicated=ident not in [7090,7100],identity_status='early_queue_key_passages_read_identity_scope_adjudicated' if ident not in [7090,7100] else 'source_branches_read_cross_identity_bridge_unresolved',last_manual_review_batch='round57b_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人原文回補 '+ch,'已讀本人及事件上下文；各支是否可橋另記，原句名字命中本身不代表跨支同一人。'))
    for ident in [7090,7091,7095,7100,7101]:
        results[ident]['historical_birth']=None
        results[ident]['historical_death']=None
    for ident in [7091,7095,7101]:results[ident]['review_status']='source_checked_dates_unknown'
    results[7090]['review_status']='needs_further_research'
    results[7100]['review_status']='needs_further_research'
    results[7090]['evidence'].extend([chunk_ev('sl_00','聪使太子粲及征北将军刘雅','晚出SL劉雅與劉雅生同戰名異','已讀後續郭默郭誦救趙固、耿稚張皮夜襲、苦戰20餘日與格增接應全段；SL確作劉雅，不是把字串劉雅生機械截成劉雅。'),chunk_ev('js_08','粲率刘雅生等步骑十万','晉書劉雅生同戰獨立原文','同小平津/孟津、趙固陽城山、郭誦張皮夜襲20餘日格增接應獨事寫劉雅生；可判該戰源別異名，但不足把全部311–324劉雅經歷自動套給6927。'),chunk_ev('sl_00','大司徒中山王雅','晚出SL大司徒中山王雅諫殺游子遠','已讀本人官位與同朱紀呼延晏諫言；此支有官名但不以晚重編單書自動閉雅生與劉雅其餘官鏈。')])
    results[7090]['candidate_duplicate_of']=6927
    results[7090]['source_identity_branches']=[dict(keys=[k for k in results[7090]['keys'] if k!='sl:sl_00:156:劉雅'],identity='劉雅，311安西到318征北迎靳明及320大司徒；TJ13:291不是同卷另一劉雅生key',birth=None,death=None),dict(keys=['sl:sl_00:156:劉雅'],identity='此原key兼劉雅官鏈及318趙固戰；該戰早源作劉雅生，名異來源關係保留待判',candidate_duplicate_of=6927,birth=None,death=None)]
    results[7100]['source_identity_branches']=[dict(keys=[k for k in results[7100]['keys'] if 'js_12' not in k],identity='315義興太守孔侃受周札告亂、周筵府中殺周續',birth=None,death=None),dict(keys=[k for k in results[7100]['keys'] if 'js_12' in k],identity='孔愉從兄、孔坦父、大司農孔侃；未得義興守轉大司農或父子官事明橋',birth=None,death=None)]
    results[7101].update(last_attested_activity=317,appearance_upper_bound=315)
    results[7101]['evidence'].append(chunk_ev('tj_13','太守宋哲奔江东','資治通鑑建武元317正月宋哲','315平東屯華陰、316援長安同軍官；317弘農太守奔江東及辛巳至建康奉詔，非此時死亡。'))
    results[7095].update(last_attested_activity=315,appearance_upper_bound=315)
    for ident in [7099,7114,7117]:
        ev=chunk_ev('tj_16','景骞、田褒、姚华、许涪及征西将军李遐、将军李西等怀奸乱政，皆收杀之','資治通鑑咸康四338夏四月李壽入成都收殺具名諸臣','338夏四月段內具名收殺；不是其後李期幽死或李始兄弟死亡套到這些人。')
        results[ident]['historical_birth']=None
        results[ident]['historical_death']=date(338,ev,month=4)
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
        results[ident]['review_status']='primary_text_corroborated'
    ev=chunk_ev('tj_16','共杀太守宋烛以降于赵','資治通鑑338五月後趙攻燕冀陽宋燭遇害','已覆核在五月李期縊死月界之後、後趙進攻棘城前冀陽流寓者殺宋燭迎趙，宋晃從兄明族，無本人年齡。')
    results[7102].update(historical_birth=None,historical_death=date(338,ev,month=5),review_status='primary_text_corroborated')
    results[7102]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7102]['evidence'].append(ev)
    for ident,note in {7090:'五key本人段均讀，但SL00兼318趙固戰作劉雅、JS/TJ作劉雅生，同戰源別名異保留；TJ13:291實指迎靳明劉雅，不能把同卷另一雅生子串當此key混人。缺跨全部官鏈同人證故whole仍hold，不硬合6927，史生卒未知。',7091:'卜抽同監守東宮抑太弟乂辭位章兩key閉；被殺玮遐或乂不可當卜抽卒，生卒未知。',7095:'吳曾郡傳教同周筵府上刀環擊殺周續兩key閉，315已任事可界登場；不是成漢軍將，死者周續非曾。',7099:'姚華三key成漢尚書與景騫田褒信任到338四月同收殺明橋，無生年。',7100:'孔侃三key皆讀，義興太守兩key閉；孔愉從兄孔坦父大司農支雖同朝同名但缺本人轉仕/族系明橋，不認定必兩人亦不強整條閉。',7101:'宋哲平東將軍華陰屯兵→援長安→弘農守奔江東奉詔同官事時序兩key閉，317至建康為在世，無明卒年。',7102:'宋燭冀陽太守宋晃從兄兩key閉，338五月被流寓之士殺迎趙，非後其他宋姓死年。',7114:'李西成漢將軍與姚華李遐同338四月收殺兩key閉，不把被殺名單的李期25歲套來。',7117:'李遐成漢征西將軍與姚華李西同338四月收殺兩key閉，無生年。'}.items():results[ident]['unresolved'].append(note)
    round57c_reads={7104:[('js_17','张忠，字巨和'),('sl_05','张忠字巨和'),('gs_01','张忠为林下之契'),('gs_01','隐士张忠见而嗟曰'),('sz_02','士张忠见之曰')],7105:[('js_01','保将张春等疾之'),('sl_01','保将张春等疾之'),('sl_02','中军将军张春'),('sl_06','遣中军将军张春乞师'),('sl_08','与别将张春还南安祁山'),('tj_13','保将张春疾之'),('tj_14','晋王保将张春、杨次'),('tj_17','中军将军张春乞师于姚弋仲')],7108:[('wsh_00','次弟屈，刚猛多变'),('sl_01','烈帝次弟屈刚猛多诈'),('tj_16','翳槐次弟屈，刚猛多诈')],7109:[('hy_00','以司隶景骞为尚书令'),('js_15','雄司隶校尉景骞'),('sl_08','雄司隶校尉宣[一作景]骞'),('sl_09','尚书令景骞'),('tj_15','司隶校尉景骞'),('tj_16','景骞、田褒、姚华、许涪')],7111:[('hy_00','会养弟攸从成都病还'),('sl_09','会养弟攸从成都病还'),('sl_09','鸩杀寿养弟安北将军攸'),('tj_16','鸩杀寿养弟安北将军攸')]}
    for ident,passages in round57c_reads.items():
        results[ident].update(source_identities_all_adjudicated=ident in [7108,7109,7111],identity_status='early_queue_all_key_subjects_read_bridge_complete' if ident in [7108,7109,7111] else 'all_key_passages_read_but_cross_branch_identity_bridge_missing',last_manual_review_batch='round57c_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人原文回補 '+ch,'已實讀本人段落；版本姓名異字、死因異說與未橋身份分支另記，不用候選JSON作史證。'))
    for ident in [7104,7105]:results[ident].update(historical_birth=None,historical_death=None,review_status='needs_further_research')
    results[7104]['source_identity_branches']=[dict(keys=[k for k in results[7104]['keys'] if k.startswith(('js:','sl:'))],identity='中山張忠巨和永嘉隱泰山、苻堅徵至長安還途華陰關卒；JS期頤但無明年',birth=None,death=None),dict(keys=[k for k in results[7104]['keys'] if k.startswith(('gs:','sz:'))],identity='GS僧朗林下友張忠徵堅華陰卒可橋JS；另GS/SZ相道恒兒時隱士張忠僅同名身份，與前者仍缺獨特事件互橋',birth=None,death=None)]
    results[7105]['source_identity_branches']=[dict(keys=[k for k in results[7105]['keys'] if not any(ch in k for ch in ['sl_02','sl_06','tj_17'])],identity='315–320司馬保將張春，譖刺陳安、立司馬瞻後奔枹罕；不套司馬保27歲或被殺年',birth=None,death=None),dict(keys=[k for k in results[7105]['keys'] if any(ch in k for ch in ['sl_02','sl_06','tj_17'])],identity='350–351後趙中軍張春奔襄國、為石祗向姚弋仲乞師；未得與司馬保將轉仕橋',birth=None,death=None)]
    ev=chunk_ev('tj_16','乃相与杀屈而立孤','資治通鑑咸康四338代王翳槐卒後殺屈','同年十一月什翼犍立以前明殺屈，不把前一隱士93歲卒當拓跋屈年齡；魏書同梁蓋推孤獨事。')
    results[7108].update(historical_birth=None,historical_death=date(338,ev),review_status='primary_text_corroborated')
    results[7108]['evidence'].append(ev)
    ev=chunk_ev('tj_16','景骞、田褒、姚华、许涪及征西将军李遐、将军李西等怀奸乱政，皆收杀之','資治通鑑338四月景騫被李壽收殺','華陽國志明司隸景騫轉尚書令，將張淳使蜀任司隸與李期尚書令支直接橋；SL宣一作景是原書明異文，不另人。')
    results[7109].update(historical_birth=None,historical_death=date(338,ev,month=4),review_status='primary_text_corroborated')
    results[7109]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7109]['evidence'].append(ev)
    ev=chunk_ev('tj_16','鸩杀寿养弟安北将军攸','資治通鑑338李攸死亡','338李壽入成都前攸已死；死因與華陽國志病還途中死、壽陽言李越藥殺真異說，保338年不判毒殺為定論。')
    results[7111].update(historical_birth=None,historical_death=date(338,ev),review_status='primary_text_corroborated')
    results[7111]['evidence'].append(ev)
    results[7111]['source_reported_cause_of_death']=[dict(source='HY及SL李壽傳',expression='從成都病還死道中；李壽陽言李越藥殺'),dict(source='TJ及SL李期傳',expression='李期鴆殺養弟安北將軍攸')]
    for ident,note in {7104:'張忠中山巨和完整JS與SL本傳已讀，期頤為敘年齡未明卒年，GS僧朗林下友有徵堅華陰卒獨事可橋；但相道恒八九歲隱士支仍缺同人獨特橋，不將後來道恒41772歲反推成張忠生卒。',7105:'張春八key皆讀：司馬保將315–320與後趙中軍350–351未得轉仕或族系明橋，可能一人長期任將但不靠名字同軍職強合；司馬保死病/幽殺是他人源別，張春只是出奔，史生卒未知。',7108:'拓跋屈翳槐次弟、什翼犍及孤兄弟梁蓋殺屈立孤三key同事件閉；338明死亡，無生年，前文宋纖或皇甫方回93歲不是本人。',7109:'景騫六key張淳蜀使時司隸→HY335明改尚書令→338四月收殺身份閉；SL宣[一作景]是原書明異字，保源別不當多一人。',7111:'李攸壽養弟安北三key閉，338死亡年一致，HY病歸途中死/壽佯稱越毒殺與TJ期鴆殺真死因異說並存，不把一支敘述壓成定論。'}.items():results[ident]['unresolved'].append(note)
    round57d_reads={7112:[('sl_01','平阳李洪聚众数千垒于舞阳'),('sl_02','李洪为大理'),('sl_03','李洪平阳人也初帅流民入定陵'),('sl_04','李洪为驸马都尉'),('tj_15','李洪为大理'),('tj_16','燕右司李洪之弟普'),('tj_17','内史李洪从俊出营外'),('tj_18','光禄大夫李洪为司空'),('tj_18','李洪为驸马都尉')],7116:[('sg_06','李越以陇西反'),('hy_00','以越为相国，与寿并录尚书事'),('sl_09','雄子车骑将军越镇江阳'),('tj_15','成主雄之子车骑将军越'),('tj_16','寿奏建宁王越')],7118:[('js_07','弢遣杜弘出海昏'),('js_09','会杜弘据临贺'),('js_18','杜弢将杜弘南走广州'),('js_19','杜弢余党杜弘奔临贺'),('tj_13','遣其将杜弘、张彦'),('tj_13','杜弘诣王敦降'),('tj_14','杜弘言于敦')],7119:[('js_10','会蜀贼杜畴作乱'),('js_19','蜀人杜畴、蹇抚'),('tj_12','蜀人杜畴等复反')],7120:[('js_08','纳尝问梅陶'),('js_09','谘议参军梅陶'),('js_10','而尚书梅陶私奏女妓'),('ssxy_01','敦咨议参军梅陶谏敦乃止'),('tj_13','咨议参军梅陶'),('tj_15','尚书梅陶与亲人曹识书')],7123:[('js_19','参军冯素与汝班不协'),('tj_12','湘州参军冯素与蜀人汝班有隙')],7125:[('sl_02','大姓王清等密谋内应泳收斩之'),('tj_16','大姓王清等密谋应赵，泳收斩之')]}
    for ident,passages in round57d_reads.items():
        results[ident].update(source_identities_all_adjudicated=ident!=7116,identity_status='all_source_key_subjects_individually_read_same_career_event_chain' if ident!=7116 else 'han_wei_rebel_and_chenghan_emperor_son_are_different_people',last_manual_review_batch='round57d_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人原文回補 '+ch,'本人原段與相連轉仕/同事已讀；不把被俘逃亡、其他人的死亡或後人追記當本人死亡紀年。'))
    for ident in [7112,7118,7119,7120,7123]:results[ident].update(historical_birth=None,historical_death=None,review_status='source_checked_dates_unknown')
    results[7112].update(last_attested_activity=370,appearance_upper_bound=337)
    results[7112]['source_reported_office_variants']=[dict(source='SL01',expression='311平陽流民舞陽壁，茍晞假雍州刺史'),dict(source='SL03完整本人附傳',expression='平陽流民舞陽壁，王浚假雍州刺史；後歸皝大理內史右司馬、司空、入秦駙馬卒無年',interpretation='同平陽籍舞陽壁雍州官與其後大理兩端獨特橋可閉身份；授官人茍晞/王浚源異不靜改。')]
    results[7118].update(last_attested_activity=322,appearance_upper_bound=315)
    results[7120].update(last_attested_activity=334,appearance_upper_bound=315)
    results[7120]['evidence'].append(chunk_ev('js_09','尚书梅陶与亲人曹识书','晉書陶侃傳梅陶致曹識書本人官位','完整上下是陶侃臨終後梅陶書；死者陶侃而不是梅陶。SS引鄧粲/王隱書亦明救陶侃者梅陶非梅頤，不將兄弟混。'))
    results[7116].update(historical_birth=None,historical_death=None,review_status='identity_conflict',duplicate_of=None)
    ev=chunk_ev('tj_16','寿奏建宁王越、景骞、田褒','資治通鑑338四月李越成漢支被殺','只歸成漢李雄子建寧王李越分支，不能套SG漢魏金城蘇則在曹操死前平隴西李越支。')
    results[7116]['source_identity_branches']=[dict(keys=[k for k in results[7116]['keys'] if k.startswith('sg:')],identity='曹操在世時反隴西、被金城太守蘇則圍降的李越；漢魏支',birth=None,death=None,evidence=[chunk_ev('sg_06','李越以陇西反','三國志蘇則傳漢魏李越','後接太祖崩，是220以前隴西反者；降不是死亡，不接成漢雄子。')]),dict(keys=[k for k in results[7116]['keys'] if not k.startswith('sg:')],identity='成漢李雄子、李期兄建寧王相國李越',birth=None,death=date(338,ev,month=4),evidence=[ev])]
    results[7116]['evidence'].append(ev)
    ev=chunk_ev('tj_16','大姓王清等密谋应赵，泳收斩之','資治通鑑咸康四338五月朝鮮王清','已覆核前有五月李期縊死月界，338五月棘城攻防朝鮮令孫泳殺內應王清；同謀數百獲赦不是王清也赦，明斬具名。')
    results[7125].update(historical_birth=None,historical_death=date(338,ev,month=5),review_status='primary_text_corroborated')
    results[7125]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[7125]['evidence'].append(ev)
    for ident,note in {7112:'李洪八key含平陽舞陽壁、燕大理右司馬內史到司空入秦駙馬同人物鏈已讀，SL本人傳明轉仕可橋，授雍州者茍晞/王浚源異另列；370到長安駙馬已證在世，SL末卒無年，死亂者弟李普非洪。',7116:'李越真混SG曹操死前隴西叛者與四世紀李雄子建寧王兩人，五key原段均讀，頂層生卒撤空，338四月死亡只留成漢分支，不用原遊戲單人年套兩支。',7118:'杜弘六key原段閉：杜弢海昏廬陵→奔臨賀王機→廣州敗→零陵尹奉送王敦→322石頭前鋒；JS18及TJ13轉仕明橋，不因前後敵友變化判混；死張彥王機溫邵不是杜弘，無明本人生卒。',7119:'杜疇三key蜀流民湘州應詹討伐、汝班應之推杜弢同連續事件閉；杜畴/杜弢明兩人，不能把杜弢315死套杜疇。',7120:'梅陶六key論月旦同鐘雅、王敦諮議救侃後尚書國喪女妓及334曹識書官事鏈閉；SS注明救侃者梅陶非兄頤，史生卒無年；334陶侃死是寫信人梅陶在世下界。',7123:'汝班蜀流民與馮素私隙、被誣反應杜疇兩key同，流民群體傷亡/杜弢起兵不等本人明卒；無生卒。',7125:'王清朝鮮大姓內應趙被孫泳斬兩key同，338五月本人明死；同謀數百後赦不推翻具名王清已斬。'}.items():results[ident]['unresolved'].append(note)
    round57e_reads={7127:[('sl_09','尚书姚华田褒'),('tj_15','尚书姚华、田褒'),('tj_16','景骞、田褒、姚华、许涪')],7128:[('sl_01','越骑校尉石广'),('sl_02','虎镇西将军石广撃斩之'),('tj_15','石广讨斩之'),('tj_16','虎追广至邺，杀之')],7129:[('tj_13','襄城太守石览'),('js_12','崧遣主簿石览将兵入洛'),('js_18','故吏平南将军石览')],7131:[('js_08','为氐窦首所杀'),('tj_12','为氐窦首所杀')],7136:[('sl_00','舍人荀裕告'),('tj_13','东宫舍人荀裕告')],7138:[('sl_09','中常侍许涪'),('tj_15','中常侍许涪'),('tj_16','景骞、田褒、姚华、许涪')],7139:[('js_09','又遣部将许高讨机'),('js_19','遣督护许高讨机'),('tj_13','遣督护许高讨王机')],7140:[('js_14','兰池长赵奭'),('sl_08','兰池长赵婴[一作奭]')],7146:[('js_12','将吏马俊、郑攀'),('js_13','侃故将郑攀、马俊'),('js_09','侃督护郑攀于沌阳'),('js_19','伏波将军郑攀'),('tj_13','荆州将吏郑攀、马俊')],7147:[('js_09','长史陈颁言于敦'),('js_18','从事中郎陈颁代卓'),('tj_13','长史陈颁言于敦')],7148:[('hy_00','氐符成、隗文作乱宜都'),('sl_09','氐符成隗文作乱宜都'),('tj_12','氐苻成、隗文复叛'),('tj_16','将军隗文等皆举兵反'),('js_14','势将邓定、隗文')],7149:[('tj_13','州人以秘子寿领州事'),('js_07','参弟寿求领州'),('js_12','州人立众兄寿为刺史')],7150:[('js_12','将吏马俊、郑攀'),('js_13','侃故将郑攀、马俊'),('js_09','侃将郑攀、苏温、马俊'),('js_19','其将马俊、苏温等执曾'),('tj_13','荆州将吏郑攀、马俊')],7151:[('js_19','侃司马鲁恬言于侃'),('tj_13','司马鲁恬谏曰')],7152:[('sl_02','营丘内史鲜于屈'),('tj_16','营丘内史鲜于屈')]}
    for ident,passages in round57e_reads.items():
        results[ident].update(source_identities_all_adjudicated=ident not in [7129,7148],identity_status='all_key_personal_passages_read_same_event_or_explicit_career_bridge' if ident not in [7129,7148] else 'local_text_branches_read_unretrieved_or_unbridged_key_remains',last_manual_review_batch='round57e_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人原文回補 '+ch,'已實讀本人與同事件上下、原明轉仕鏈；同名仍缺橋的分支另隔，不以姓名索引或抽取候選作原典證據。'))
    for ident in [7129,7131,7136,7139,7140,7146,7147,7148,7149,7150,7151]:
        results[ident].update(historical_birth=None,historical_death=None,review_status='needs_further_research' if ident in [7129,7148] else 'source_checked_dates_unknown')
    results[7129]['evidence'].append(dict(citation='原CBDB416915及BIOG_SOURCE_DATA/TEXT_CODES實查',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='石覽；BD=0/未知；source4736襄陽府志section5190264，Q65823155。',interpretation='地方志具體section未取，不能只同名宣稱該key與荀崧故吏已全閉；TJ及JS三書支有荀灌突圍與周訪子救援獨事可閉。'))
    results[7129]['source_identity_branches']=[dict(keys=[k for k in results[7129]['keys'] if not k.startswith('cbdb:')],identity='荀崧故吏主簿後襄城太守/平南將軍石覽，荀灌求救周訪子同救宛',birth=None,death=None,attested_activity_years=[315]),dict(keys=['cbdb:416915'],identity='襄陽府志石覽待原頁身份橋',birth=None,death=None)]
    results[7148]['source_identity_branches']=[dict(keys=[k for k in results[7148]['keys'] if not any(ch in k for ch in ['tj_16','js_14'])],identity='311宜都巴東與苻成流民反、拘黃龕隗文',birth=None,death=None),dict(keys=[k for k in results[7148]['keys'] if any(ch in k for ch in ['tj_16','js_14'])],identity='347李勢故將隗文與鄧定反、入成都奉范賁；未得與311支轉仕明橋',birth=None,death=None,attested_activity_years=[347])]
    results[7149]['source_reported_chronologies']=[dict(source='TJ315年附敘初交州事、JS陶璜/顧眾傳',death_year=None,death_upper_bound=315,expression='顧秘卒後子壽領交州，梁碩擒令母鴆殺；TJ初是追敘，JS顧眾迎喪歷杜弢亂六年',interpretation='不能將附敘所在315當確切死亡，也不反推某個具體年；壽母令鴆是本人死法而非秘/參之死。')]
    for ident in [7127,7138]:
        ev=chunk_ev('tj_16','景骞、田褒、姚华、许涪及征西将军李遐、将军李西等怀奸乱政，皆收杀之','資治通鑑338夏四月田褒許涪同李壽入成都被殺','已讀335受寵尚書田褒/中常侍許涪到338收殺，月界在其後五月李期縊死之前，明四月。')
        results[ident].update(historical_birth=None,historical_death=date(338,ev,month=4),review_status='primary_text_corroborated')
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    for ident,phrase,note in [(7128,'虎追广至邺，杀之','石廣鎮西被二歸控謀逆，追邺殺；337鎮西討侯子光與333越騎同駐長安西軍官鏈，不把被石廣斬者侯子光死亡誤當廣死。'),(7152,'共收屈，数其罪而杀之','鮮于屈營丘內史請降趙為武寧令孫興殺，非同段朝鮮王清或宋燭。')]:
        ev=chunk_ev('tj_16',phrase,'資治通鑑338五月本人被殺月界覆核',note+' 前有五月李期縊卒，後六月罷司徒；原年度內月界已實讀。')
        results[ident].update(historical_birth=None,historical_death=date(338,ev,month=5),review_status='primary_text_corroborated')
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    for ident,note in {7127:'田褒三key尚書勸立李期到338四月收殺身份閉，生未知。',7128:'石廣四key後趙鎮西/越騎長安關中軍將至338五月二歸譖殺同官事鏈閉；侯子光斬首不是本人死，史生未知。',7129:'石覽三早史荀崧故吏襄城守/平南將軍、荀灌求援周訪子同救315閉；原庫襄陽府志section未取，整條仍hold，不靠同名閉CBDB。',7131:'竇首氐人殺逃雍閻鼎兩key同，死者閻鼎非竇首；生卒不詳。',7136:'荀裕東宮舍人告崔玮許遐劝乂反兩key同，收殺者玮遐不是裕，生卒未知。',7138:'許涪李期中常侍窺伺李壽，三key335受寵到338四月同收殺明閉，生未知。',7139:'許高陶侃督護討王機三key同；JS侃傳斬機、JS機傳/TJ病死掘尸是王機死因源異，不當許高卒年。',7140:'趙奭/婴原SL明一作奭，兩key同蘭池長張冰得玺上報張寔獨事，無生卒；315明年只有晚SL故不推已證活動自動修正。',7146:'鄭攀陶侃督護敗杜曾、伏波破王真、請留侃拒王廙後降朱軌五key有原故將與轉叛明橋，非因敵友換就判混；後王廙大殺舊將未具名不硬當鄭攀死。',7147:'陳頒王敦長史救陶侃到從事中郎擬代甘卓三key同近年敦幕官事鏈閉，無生卒；提名湘州未獲帝允不可說已任湘州刺史。',7148:'隗文五key皆讀；311氐流民宜都巴東與347漢故將奉范賁兩支未得轉仕橋，可能同人長壽但不僅名字推合。347被袁喬破不是死，袁喬36歲死不得套隗文，整條未知hold。',7149:'顧壽父秘兄參弟眾交州梁碩三key明族事橋；TJ315初是前事回溯，JS顧眾迎喪遇杜弢亂六年，無明卒年，不能硬填315或靠六年不明起算逆推。',7150:'馬俊五key陶侃舊將與蘇温鄭攀西迎杜曾、末與蘇温執曾降周訪原同僚與轉仕橋閉；殺被執杜曾非馬俊死，生卒未知。',7151:'魯恬陶侃司馬諫勿輕杜曾兩key同，陶侃兵死數百不是魯恬明卒；生卒不詳。',7152:'鮮于屈營丘內史降趙被武寧令孫興收殺兩key同，338五月月界明；不是同段孫泳斬朝鮮王清。'}.items():results[ident]['unresolved'].append(note)
    round57f_reads={7154:[('tj_12','建平流民傅密'),('js_10','建平流人傅密'),('ssxy_02','建平民傅密')],7156:[('sl_00','右将军刘参攻郭黙'),('tj_12','右将军刘参攻郭默')],7167:[('sl_00','侍中卜干泣谏'),('sl_01','侍中卜干谓之曰'),('tj_13','侍中卜干谓休、崇')],7168:[('sl_01','汲郡向冰聚众数千'),('tj_12','汲郡向冰聚众数千')],7169:[('sl_02','积弩将军呼延晃'),('tj_16','积弩将军呼延晃')],7172:[('js_08','侍中宋敞送笺降于曜'),('sl_00','侍中宋敞[纪事本末作宗敞]'),('tj_13','侍中宗敞送降笺于曜')],7173:[('sl_00','中常侍王沉宣懐俞容'),('tj_13','汉中常侍王沈、宣怀')],7175:[('sl_02','建威将军张支'),('tj_16','建威将军张支')]}
    for ident,passages in round57f_reads.items():
        results[ident].update(source_identities_all_adjudicated=True,identity_status='all_key_personal_passages_read_same_unique_event_identity',last_manual_review_batch='round57f_early_multikey_reaudit')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'早期多鍵本人原文回補 '+ch,'已讀本人及同事件前後，舊字異寫依原文保存；同段他人死亡與年齡不混入本人。'))
    for ident in [7154,7156,7167,7168,7172,7173]:results[ident].update(historical_birth=None,historical_death=None,review_status='source_checked_dates_unknown')
    results[7172].update(last_attested_activity=316,appearance_upper_bound=316)
    for ident in [7169,7175]:
        ev=chunk_ev('tj_16','评等与战，斩晃、支首','資治通鑑咸康五339夏四月燕襲遼西追兵被殺','已讀339夏四月李闳被俘月界後庾亮上疏、再燕評襲趙斬晃支，後五月代王會參合陂；故339四月明死亡，非SL未明月份之推定。')
        results[ident].update(historical_birth=None,historical_death=date(339,ev,month=4),review_status='primary_text_corroborated')
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    for ident,note in {7154:'傅密建平流民迎杜弢致周顗敗績三key同獨事，周顗後被害不等傅密死，生卒未知。',7156:'劉參前趙右將攻郭默於懷兩key同，接會祗病薨死的是傅祗非劉參，未補生卒。',7167:'卜幹侍中同勸陳休卜崇及七臣案泣諫三key閉，休崇自稱過50與被殺皆非卜幹年齡/死亡，幹只免庶人，生卒未知。',7168:'向冰汲郡聚眾壁枋頭兩key同，張賓稱大軍渡後冰必可擒是預計，實文僅大破獲資，不當已斬或已死，無生卒。',7169:'呼延晃後趙積弩將軍追燕評與張支同斬兩key閉，339夏四月明月界，生未知。',7172:'宋敞/宗敞原SL注紀事本末作宗敞，三key同316長安獻降笺侍中，原文被劉曜斬的是索綝子，不是被索留而後送笺的宋敞；生卒未知。',7173:'宣懷/懐漢中常侍與王沈郭猗用事兩key同，原SL懐字非缺本人，後命訊王鑒與養女立后是其事；被斬王鑒等不當宣懷死，生卒未知。',7175:'張支後趙建威將軍同石成呼延晃追燕評，被斬兩key同，339四月明死，生未知。'}.items():results[ident]['unresolved'].append(note)
    round58a_reads={8903:[('sl_03','洪谓博士胡文'),('sl_04','中书监胡文中书令王鱼'),('tj_17','京兆王鱼、安定程肱、胡文')],9283:[('sl_03','荥阳太守刘逺奔鲁阳'),('tj_18','慕容忠攻荥阳太守刘远')]}
    for ident,passages in round58a_reads.items():
        results[ident].update(source_identities_all_adjudicated=True,identity_status='previous_unbatched_original_key_passages_rechecked_and_same_person_closed',last_manual_review_batch='round58a_unbatched_queue_reaudit',historical_birth=None,historical_death=None,review_status='source_checked_dates_unknown')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'無batch標籤舊記錄逐鍵原文回檢 '+ch,'已實讀各key本人段落並核官事時序，未知生卒保持空；下述活動年僅在世與登場約束。'))
    ev=evidence(SOURCE/'_cache/texts/资治通鉴.txt','中书监胡文、中书令王鱼','資治通鑑全文永和十一355胡文諫苻生','已讀公元三五五年年界與後九月梁后諸臣被殺；350苻健軍咨與355中書監同王魚同僚秦朝官事鏈，SL洪博士至生中書監無反向年代。死后及毛梁是他人，無胡文卒年。')
    results[8903]['evidence'].append(ev)
    results[8903].update(last_attested_activity=355,appearance_upper_bound=350)
    results[9283].update(last_attested_activity=363,appearance_upper_bound=363)
    results[8903]['unresolved'].append('胡文三key350苻洪博士/苻健與王魚同軍咨→355仍與王魚同中書監令官事鏈回讀閉；獨立TJ全文355同名同官諫生可證在世，不套梁后毛貴梁楞梁安死亡，生卒未知。')
    results[9283]['unresolved'].append('劉遠/逺兩key明同363興寧元荥陽太守被慕容忠攻奔魯陽，SL續密城退江陵不等死亡；兩書同官同攻者同去處足橋，生卒未知，非據名字一次命中即核。')
    hegs=chunk_ev('gs_01','释慧永姓潘。河内人也','高僧傳慧永全本人傳回讀','已讀竺曇現師、庐山西林及山上茅室、聞香室虎驅山、烏橋醉營主馬驚次日懺悔答戒神、何無忌浔陽相會到義熙十414卒83。')
    hesz=chunk_ev('sz_02','河内人也。出家为竺昙现弟子','名僧傳抄惠永本人全傳回讀','已讀庐山北岭到南岭凌雲精舍、聞香室虎驅山、烏橋醉營主馬驚次日懺悔答戒神到太元五380卒83；同師同籍與獨特事件明同人，寺名及死亡異說另保，不誤稱老師不同。')
    hegdeath=chunk_ev('gs_01','以晋义熙十年遇疾绵笃','高僧傳慧永義熙十414卒83','敘到言終而卒春秋83，年度明414；不是慧遠416。')
    hesdeath=chunk_ev('sz_02','春秋八十三，晋泰元五年卒','名僧傳抄惠永太元五380卒83','原抄實寫泰元五，映380；與GS414真紀年衝突，不能靜選更常見414。')
    results[7396].update(source_identities_all_adjudicated=True,identity_status='same_monk_identified_by_teacher_origin_and_unique_wuqiao_incident_but_death_chronologies_conflict',last_manual_review_batch='round58b_unbatched_identity_conflict_reaudit',historical_birth=None,review_status='primary_text_corroborated')
    results[7396]['historical_death']={'year':None,'precision':'conflicting_source_years','calendar':'historical_CE_year','alternatives':[date(380,hesdeath),date(414,hegdeath)]}
    results[7396]['source_reported_chronologies']=[dict(source='名僧傳抄惠永傳',death_year=380,age=83,conditional_birth_year=298,calculation='380-83+1=298；按虛歲條件推算，不是獨立生年'),dict(source='高僧傳慧永傳',death_year=414,age=83,conditional_birth_year=332,calculation='414-83+1=332；按虛歲條件推算，不是獨立生年')]
    results[7396]['evidence'].extend([hegs,hesz,hegdeath,hesdeath])
    results[7396]['retracted_review_statements']=['舊unresolved稱different teacher/temple故不能判同人：老師不同之說撤回，兩書都竺曇現；西林/凌雲及紀年源異不壓過同師同籍烏橋獨事身份橋。']
    results[7396]['unresolved']=[s for s in results[7396]['unresolved'] if 'Both biographies explicitly age83' not in s]
    results[7396]['unresolved'].append('已逐讀兩本傳，同河內、同師竺曇現、同室虎與烏橋醉營主馬驚次日懺悔答戒神獨事，可閉慧/惠永同人；太元五380與義熙十414兩明年仍衝突，頂層不選單年，生298/332只是各自卒齡83虛歲條件推算。')
    for ident,passages in {7043:[('sl_09','蜀郡太守江夏郑融'),('js_19','邵陵太守郑融')]}.items():
        results[ident].update(last_manual_review_batch='round58b_unbatched_identity_conflict_reaudit',historical_birth=None,historical_death=None,source_identities_all_adjudicated=False,review_status='needs_further_research',identity_status='shu_prefect_captive_and_shaoling_prefect_killed_are_unbridged_scopes_not_proved_distinct_people')
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'未標批次鄭融兩key原文回檢 '+ch,'SL313蜀郡守江夏人北出枳被李恭費黑獲，JS杜弢害邵陵守；捕非死，郡官不同且未有轉仕逃還橋，不能硬同也不僅不同郡便斷必兩人。'))
    results[7043]['source_identity_branches']=[dict(keys=['sl:sl_09:333:鄭融'],identity='蜀郡太守江夏鄭融，313涪陵北出枳被俘，後事未知',birth=None,death=None),dict(keys=['js:js_19:125:鄭融'],identity='邵陵太守鄭融，杜弢所害，原段未明獨立年',birth=None,death=None)]
    results[7043]['unresolved'].append('兩key原段回讀：目前只能說蜀郡守與邵陵守缺橋，313被獲不等死亡，不能由官郡異逕判已證兩人；保分支hold，不套單一死亡年。')
    round58c_reads={7054:[('sg_10','韩嵩刘光之说刘表'),('sl_02','刘光安定人也一云侯子光'),('tj_15','安定侯子光')],7065:[('sl_00','封昶妻张氏为慈乡君'),('sl_01','其夜杀邃并其妃张氏'),('tj_15','杀邃及其妃张氏')],7680:[('hy_00','辄造汉中守将张才急书'),('sl_09','輙造汉中守将张才急书'),('sl_02','殿中将军张才'),('tj_17','殿中将军张才'),('wsh_02','鉴弟苞与胡张才')],7746:[('sl_02','抚军将军石寜'),('sl_03','赵梁州刺史石宁独据上邽不下'),('sl_05','赵将石寜独据上邽不下'),('tj_16','中书监石宁为征西将军'),('tj_17','骠骑将军石宁奔柏人'),('tj_17','赵凉州刺史石宁独据上邽不下')],7764:[('js_12','允之遣朱焘、何准'),('js_00','父准，见《外戚传》'),('js_17','何准，字幼道'),('ssxy_02','何凖字防道庐江灊人'),('ssxy_03','充弟准亦精勤唯读佛经')]}
    for ident,passages in round58c_reads.items():
        results[ident].update(source_identities_all_adjudicated=False,last_manual_review_batch='round58c_unbatched_mixed_or_unbridged_records_all_key_reaudit',historical_birth=None,historical_death=None)
        for ch,phrase in passages:results[ident]['evidence'].append(chunk_ev(ch,phrase,'無批次舊混人/缺橋記錄逐鍵回讀 '+ch,'已實讀本人範圍；本回讀可確認每一key指向的分支，但不代表分支可合成同一人；真正不同人與尚無轉仕橋分開標識。'))
    results[7054].update(identity_status='confirmed_distinct_han_liubiao_adviser_and_337_young_rebel_hou_ziguang',review_status='identity_conflict')
    results[7054]['source_identity_branches']=[dict(keys=['sg:sg_10:260:劉光'],identity='韓嵩同勸漢末劉表的劉光，張璠評文追述',birth=None,death=None),dict(keys=[k for k in results[7054]['keys'] if not k.startswith('sg:')],identity='安定侯子光，SL作劉光一云侯子光，弱冠佛太子337為石廣斬',birth=None,death=None,source_reported_death_year=337)]
    for ident in [7065,7680,7764]:results[ident].update(review_status='needs_further_research',identity_status='source_scopes_individually_read_but_missing_marriage_or_career_bridge_not_proved_distinct_people')
    results[7065]['source_identity_branches']=[dict(keys=['sl:sl_00:440:張氏'],identity='劉曜皇后劉氏叔劉昶妻、慈鄉君張氏，非劉曜本人妻',birth=None,death=None),dict(keys=[k for k in results[7065]['keys'] if k!='sl:sl_00:440:張氏'],identity='石邃妃張氏337七月與邃兒女同殺；與慈鄉君未得改嫁或族系橋',birth=None,death=None,source_reported_death_year=337)]
    results[7680]['source_identity_branches']=[dict(keys=[k for k in results[7680]['keys'] if k.startswith('hy:') or 'sl_09' in k],identity='李壽所借急書之成漢漢中守將張才，336/337破司馬勳為不同原書時序；無明本人卒年',birth=None,death=None),dict(keys=[k for k in results[7680]['keys'] if not (k.startswith('hy:') or 'sl_09' in k)],identity='後趙殿中將軍胡張才，與李松石苞攻冉閔失敗石鑑誅；未得成漢到後趙轉仕橋',birth=None,death=None,source_reported_death_year=349)]
    results[7764]['source_identity_branches']=[dict(keys=['js:js_12:108:何準'],identity='蘇峻亂王允之遣與朱燾擊韓晃軍人何準；未具籍字族系',birth=None,death=None),dict(keys=[k for k in results[7764]['keys'] if k!='js:js_12:108:何準'],identity='何充弟幼道第五、穆章后父何準，47卒而無明卒年；357追封不是卒年',birth=None,death=None,age_at_death=47)]
    results[7746].update(review_status='identity_conflict',identity_status='confirmed_incompatible_350_shanggui_death_and_351_bairen_activity_with_347_predecessor_scope_unassigned')
    results[7746]['source_identity_branches']=[dict(keys=['sl:sl_03:418:石寧','sl:sl_05:25:石寧'],identity='後趙上邽守梁州(SL)/涼州(TJ)刺史石寧350十二月為苻雄斬',birth=None,death=None,source_reported_death_year=350,source_reported_death_month=12),dict(keys=['sl:sl_02:103:石寧','tj:tj_17:94:石寧'],identity='後趙抚軍/驃騎石寧350奔襄國、351劉顯弒祗後奔柏人；明晚於上邽支死亡，不能同人',birth=None,death=None,attested_activity_years=[351]),dict(keys=['tj:tj_16:83:石寧','sl:sl_02:103:石寧'],identity='347中書監轉征西隨麻秋伐涼石寧；尚未判屬上邽支或襄國支，SL02該key兼前後兩組經歷不硬附全部',birth=None,death=None,attested_activity_years=[347])]
    for ident,note in {7054:'漢末劉表議者與337弱冠安定佛太子為不同人；SL明一云侯子光可橋TJ侯子光，不把SL這支整體套SG。',7065:'回讀確認SL00是皇后劉氏叔劉昶妻張氏，SL01/TJ是石邃妃張氏；沒有改嫁/族系橋故hold，但只憑配偶不同不能宣稱已證絕不可能同人，狀態調待考，不將337卒套慈鄉君。',7680:'五key逐讀分成漢漢中與後趙殿中支，無明轉仕橋仍hold；347成亡後轉趙349不是年代不可能，故不宣稱已證兩人；HY336與SL337破司馬勳源別亦保。',7746:'原文350十二月苻雄殺上邽涼州/梁州石寧與351劉顯殺祗後驃騎石寧奔柏人真不相容，確需至少兩人；347征西中書監支缺橋暫不硬派任何一方，SL02兼多經歷分scope處理。',7764:'五key本人段回讀，四key何充第五弟幼道穆章后父47卒同人閉；另王允之遣戰韓晃的何準缺字籍橋，隱士不仕與義軍未授官未必邏輯排斥，不逕斷已證異人，整條待考；357追封不是死亡，后40466歲不是父年齡。'}.items():results[ident]['unresolved'].append(note)
    fj=chunk_ev('tj_13','冯\ue409者帅其众降','資治通鑑316馮缺字者降李回原文回讀','單key本人即與馬嚴同被孔萇攻、後率眾降李回者，角色及事件可定；中間PUA不是可安全刪除的字。馬嚴投水死不套本人。')
    fw={'citation':'維基文庫資治通鑑卷089公開傳本文字實讀','url':'https://zh.wikisource.org/wiki/資治通鑑/卷089','retrieval':'2026-11-09 public original text via r.jina.ai; full_name_character_still_missing','excerpt':'严惧而出走，赴水死。冯□者帅其众降。回徙居易京，流民归之者相继于道。','interpretation':'公開本同樣顯示中間缺字，不能說只有本地PUA錯碼；此段本人唯一可閉單key來源身份，但完整姓名仍未識，不把遊戲馮者確認為完整史名。'}
    results[7429].update(source_identities_all_adjudicated=True,identity_status='single_key_unique_surrender_event_subject_verified_but_middle_name_character_unidentified',last_manual_review_batch='round58d_name_text_defects_reaudit',historical_birth=None,historical_death=None,review_status='source_checked_dates_unknown',historical_full_name_verified=False,last_attested_activity=316,appearance_upper_bound=316)
    results[7429]['evidence'].extend([fj,fw])
    results[7429]['historical_name_reading']={'display':'馮□者','status':'one_middle_character_unidentified','local_raw':'馮\ue409者','public_text':'馮□者','game_name':'馮者','interpretation':'遊戲姓名少一原文字，僅提出缺字顯示供修，未猜填字形。'}
    results[7429]['unresolved'].append('公開通鑑089與本地均顯示馮姓與者之間有缺字；已閉單key同事件本人身份scope，完整姓名仍不詳，兩個問題分開記，絕不假稱補全姓名。316率眾降明在世，不是馬嚴投水死。')
    bg=chunk_ev('sl_00','太史令弁广明言于曜','SL光初二年劉曜途中止兵原文回讀','本人唯一角色前趙太史令劝曜勿行，姓名可能弁廣明或弁廣後接明言；保原連寫，不靠抽取分詞強定。')
    bgj=evidence(SOURCE/'_cache/texts/晋书.txt','太史令弁广明言于曜','晉書劉曜載記同句獨立傳本對讀','完整上下同劉雅劉厚攻陳倉20日、曜次雍城遇妖星犯月止行；JS也原連寫弁廣明言，能核同事件本人而不能單憑此句斷姓名分詞。')
    results[7546].update(source_identities_all_adjudicated=True,identity_status='single_key_unique_court_astronomer_event_subject_verified_name_segmentation_unresolved',last_manual_review_batch='round58d_name_text_defects_reaudit',historical_birth=None,historical_death=None,review_status='source_checked_dates_unknown',historical_full_name_verified=False)
    results[7546]['evidence'].extend([bg,bgj])
    results[7546]['historical_name_reading']={'raw_expression':'太史令弁廣明言於曜','alternatives':['弁廣明，言於曜','弁廣，明言於曜'],'status':'segmentation_unresolved','interpretation':'只有姓名分詞仍待另一出現或可靠異本，不把單key唯一太史令身份也誤報未讀；不逕改遊戲名。'}
    results[7546]['unresolved'].append('JS完整載記實讀仍與SL同連寫太史令弁廣明言於曜，唯一官事主體可定而姓名分詞未能獨立裁決；source全key scope閉不表示姓名已補全，沒有新增日期推定。')
    sunxt={'citation':'新唐書卷073下宰相世系表孫氏公開原表實讀','url':'https://zh.wikisource.org/wiki/新唐書/卷073下','retrieval':'2026-11-09 public original text via r.jina.ai; original CBDB notes XTS73B3b matched to actual genealogy','excerpt':'顗避地河朔，居武邑武遂。生煇，字光休，後趙射聲校尉。生緯，字元文，幽州都督。生周，字季洽，後燕高陽王文學。生敬仁，字士和，北燕司隸功曹。','interpretation':'已讀上代道恭及顗/芳分列、兄烈昌黎支與下代緯周官字；與SL輝字光休射聲、父顗子緯孫周三元組足閉孫輝本體及CBDB31581，名形煇/輝、緯字元文/符文及中書令官屬原書有異，保源別。不是由原庫抽取JSON直接當原表。'}
    sunsl=chunk_ev('sl_02','孙辉字光休晋中书令𫖮之少子','晚重編SL孫輝完整族官段重讀','完整至父居武遂、本人後趙射聲、子緯幽州都督及孫周後燕高陽文學，與新唐書同祖子孫官字可閉；SL緯字符文周字季冶及將顗稱中書令，與XTS不同原字照保。')
    results[7887].update(source_identities_all_adjudicated=True,identity_status='sun_hui_guangxiu_father_yi_son_wei_grandson_zhou_office_genealogy_bridged_to_original_xts_table',last_manual_review_batch='round58e_original_xts_table_retrieved',historical_birth=None,historical_death=None,review_status='source_checked_dates_unknown')
    results[7887]['evidence'].extend([sunxt,sunsl,dict(citation='原CBDB31581及31582族系/來源注實查',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='31581孫輝：c_notes Sun(1) Hui [31581] XTS,73B.3b；父75=31580、子180=31582。31582孫緯：XTS73B.3b；父75=31581、子180=31583。兩人BD0未知，Q45419953/Q45419955僅同原庫導入。',interpretation='原庫身份以實讀原表父子孫官字作橋，0不是公元0，沒有採索引年或猜測生卒。')])
    results[7887]['source_reported_name_genealogy_variants']=[dict(source='SL',name='孫輝',style='光休',father='顗（稱晉中書令）',son='緯字符文幽州都督',grandson='周字季冶後燕高陽王文學'),dict(source='XTS73下',name='孫煇',style='光休',father='顗（同段芳稱中書令）',son='緯字元文幽州都督',grandson='周字季洽後燕高陽王文學')]
    results[7887].setdefault('retracted_review_statements',[]).append('舊原表未取待核現已補原文；保留源別字形，不強改成單一標準名。')
    results[7887]['unresolved']=[s for s in results[7887]['unresolved'] if 'CBDB新唐書73B3b引文未取' not in s]
    results[7887]['unresolved'].append('已取得並逐讀原引XTS73下孫氏表，光休後趙射聲/父顗/子緯幽州都督/孫周高陽文學四鏈與SL及原庫閉合；源字輝/煇、符文/元文、季冶/季洽與中書令屬顗/芳有異並保，無本人生卒明年。')
    results[7061]['evidence'].append(sunxt)
    results[7061].setdefault('retracted_review_statements',[]).append('原庫父孫輝子孫周新唐書原表未取的舊限制已補取；但王浚督護軍事支仍未有父子字明橋，wholefalse繼續。')
    results[7061]['unresolved'].append('補取得XTS73下緯元文、父光休射聲、子周高陽文學原表；與王浚督護孫緯只有同名同幽州官地相容，軍事三key未給父子/字，仍不靠合理履歷自動閉整條。')
    k碑={'citation':'顏真卿康使君神道碑銘，全唐文卷0344公開原文實讀','url':'https://zh.wikisource.org/wiki/全唐文/卷0344','retrieval':'2026-11-09 public original text via r.jina.ai; genealogy paragraph and Kang Xixian identity read','excerpt':'超之裔孫魏強弩將軍權，權生晉虎賁中郎將泰，泰生（闕）太守威，威生蘭陵令、奮節將軍翼，隨晉元帝過江，為吳興郡丞，因居烏程，事見山謙之《吳興記》。','interpretation':'原庫康權32491注See descendant Xixian32492；原庫32492明康希銑與顏真卿文集10.8b–10b，此公開碑本人名希銑南金、家世終唐國安子吻合。祖權明曹魏強弩將軍、曾孫翼已隨元帝過江，是祖譜支；不能把另一前秦357太史令同名人死亡套此裸祖譜key。頁面是自動轉錄並有君訊/闕字，僅就清楚可讀祖官鏈裁定，不補闕字。'}
    kp=chunk_ev('tj_18','六月，太史令康权言于秦主生','資治通鑑永和十三357六月康權被苻生撲殺','已重讀本人完整諫象句與生怒扑殺，前秦太史令康權本人死亡；陰謀繼帝苻堅及後裔碑康希銑為別人。')
    results[8039].update(source_identities_all_adjudicated=False,reviewed_source_keys=results[8039]['keys'][:],identity_status='cbdb_ancestral_wei_crossbow_general_distinct_from_former_qin_court_astronomer_killed_357',last_manual_review_batch='round58f_original_tang_epitaph_genealogy_retrieved',historical_birth=None,historical_death=None,review_status='identity_conflict')
    results[8039]['evidence'].extend([k碑,kp,chunk_ev('sl_04','六月太史令康权言于生','SL康權前秦太史令支本人重讀','同TJ357六月三月並出孛入太微東井，苻生怒以妖言扑殺；不以晚SL另算獨立確證。'),dict(citation='原CBDB康權32491及所指康希銑32492文獻注實查',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='32491康權：See documentation for descendant Xixian32492。32492康希銑：Yan Zhenqing WJ10.8b–10b；source66734頁131488、24309頁3189、3689 section62636；BD646/716為原庫限定，未套本片。',interpretation='本回讀追到實際顏碑的康希銑祖魏權，不再停在未知祖譜；後裔原庫生卒未在本片另作核實或反推祖年。')])
    qdeath=date(357,kp,month=6)
    qdeath.update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
    results[8039]['source_identity_branches']=[dict(keys=['sl:sl_04:107:康權','tj:tj_18:193:康權'],identity='前秦太史令康權，357六月諫苻生被扑殺',birth=None,death=qdeath,evidence=[kp]),dict(keys=['cbdb:32491'],identity='康希銑碑先祖曹魏強弩將軍康權，子晉虎賁中郎將泰、曾孫翼隨晉元帝過江；生卒未載',birth=None,death=None,evidence=[k碑])]
    results[8039].setdefault('retracted_review_statements',[]).append('舊未得後裔祖譜身份橋之限制已補原碑；所得不是同一前秦太史令，而是曹魏強弩將軍支，whole仍false須拆，不將357套祖。')
    results[8039]['unresolved'].append('已實取顏真卿康希銑碑，原庫32491指祖魏強弩將軍權；其子晉泰、曾孫翼元帝過江的世代鏈與前秦357太史令不能同套。所有key範圍已讀但原遊戲條目混支，整人BD留空；前秦死亡357六月只留分支，祖魏支生卒不詳，待拆分。')
    cj=evidence(SOURCE/'_cache/texts/晋书.txt','长子邵，永嘉太守。少子系','晉書蔡謨完整本傳末蔡邵蔡系官系實讀','蔡謨長子邵永嘉、少子系撫軍長史，與原CBDB父12005蔡謨、兄12006蔡邵永嘉同三元組；不把蔡謨77卒套蔡系。')
    ca=evidence(SOURCE/'_cache/texts/晋书.txt','蔡谟，字道明，陈留考城人也','晉書蔡謨傳首祖睦德克族系對讀','曾祖睦魏尚書、祖德樂平太守、父克，與原庫12002睦→12003德→12004克→12005謨父族四代相合；獨立橋明原庫父謨而非另一同名。')
    cs=chunk_ev('ssxy_01','蔡系字子叔济阳人司徒谟第二子','世說雅量蔡子叔謝萬征虜亭原注重讀','中興書引蔡系謨次子撫軍長史；JS13同謝萬被推下床送客獨事，字父兄官完整橋閉三key。')
    results[8080].update(source_identities_all_adjudicated=True,identity_status='cbdb_cai_mo_father_shao_yongjia_brother_and_mu_de_ke_ancestry_match_jinshu_cai_xi_younger_son',last_manual_review_batch='round58g_original_db_father_and_jinshu_family_bridge',historical_birth=None,historical_death=None,review_status='source_checked_dates_unknown')
    results[8080]['evidence'].extend([cj,ca,cs,chunk_ev('js_13','尝与蔡系送客于征虏亭','晉書謝萬傳蔡系同事重讀','同SS蔡子叔舉謝萬褥掷地、壞面不介意獨事，非王系或蔡氏其他人。'),dict(citation='原CBDB12002–12007 BIOG_MAIN/KIN_DATA實查',database_path=str(SOURCE/'_cache/cbdb_20261017.sqlite3'),excerpt='12007蔡系父75=12005蔡謨；12006蔡邵父75=12005，注明永嘉太守。12005父75=12004蔡克、長子182=12006、次子183=12007；12004父75=12003蔡德；12003父75=12002蔡睦。蔡系index330，BD0未知；原注稱prefect Fuzhou。',interpretation='原庫具父兄祖譜識別，不是僅同名；以JS首祖末子獨立讀明閉其identity，index330不當生年或實際活動，原庫抚州守不轉抄為已證履歷。')])
    results[8080]['source_reported_office_discrepancy']=[dict(source='原CBDB12007注',office='prefect of Fuzhou (Jiangxi)',status='unsupported_by_consulted_primary_passages'),dict(source='JS蔡謨傳末及SS引中興書',office='撫軍長史',status='explicitly_attested_role_no_exact_appointment_year')]
    results[8080].setdefault('retracted_review_statements',[]).append('舊尚無蔡謨父系原文橋之hold撤回：原庫KIN實父蔡謨且長兄邵永嘉，晉書全傳有相同父祖兄弟官系。')
    results[8080]['unresolved']=[s for s in results[8080]['unresolved'] if '尚無蔡謨父系原文橋' not in s]
    results[8080]['unresolved'].append('原庫父蔡謨兄邵永嘉及睦德克謨祖鏈已與晉書全傳首末互證，三key全閉；蔡系本人生卒仍不詳，未取父蔡謨77歲卒或index330。原庫撫州守缺古文支持，原書明撫軍長史，單記官名源別不猜補轉任年。')
    # Source hierarchy correction: the cached SL is a late reconstruction.
    independent_death_sources={7166:('tj_16','守将劳杨败死',339),7513:('tj_16','收当、思明斩之，夷其三族',345),7547:('tj_17','丙子，燕献怀太子晔卒',356),7670:('tj_17','斩京兆太守刘秀离',349),7686:('tj_17','苞怒，杀光等百馀人',349),7700:('tj_17','苞怒，杀光等百馀人',349),7805:('tj_17','刘显弑赵王祗及其丞相安乐王炳',351),7837:('tj_17','闵杀显及其公卿己下百馀人',352),7846:('tj_17','洪自以右族，轻琚，琚遂杀洪',352)}
    for ident,(ch,phrase,year) in independent_death_sources.items():
        ev=chunk_ev(ch,phrase,'資治通鑑獨立本人死亡紀年覆核','已讀該年上下文與本人死亡句，不依SL確年；月份保守暫僅年')
        results[ident]['historical_death']=date(year,ev)
        results[ident]['evidence'].append(ev)
    for ident,phrase in [(7651,'刘霸断其手足，斫眼溃肠'),(7652,'宣使杨柸等缘猕猴梯而入，杀韬'),(7657,'及宣杀韬，虎疑其知而不告，亦诛之')]:
        ev=chunk_ev('tj_16',phrase,'資治通鑑永和四348八月石宣弒弟案','整段獨立早源紀年實讀；石韜被兄使者殺、石宣火刑、趙攬被疑知情誅')
        results[ident]['historical_death']=date(348,ev,month=8)
        results[ident]['historical_death'].update(precision='lunar_month',calendar='Chinese_lunisolar_reign_year_mapped_to_CE_year_only')
        results[ident]['evidence'].append(ev)
    yi346=chunk_ev('tj_16','弈单骑突门，门者射而杀之','資治通鑑永和二346冬','獨立支持346冬，十月僅SL，不保精月')
    results[7400]['historical_death']=date(346,yi346)
    results[7400]['historical_death']['season']='winter'
    results[7400]['evidence'].append(yi346)
    dang345=chunk_ev('tj_16','收当、思明斩之，夷其三族','資治通鑑永和元345八月後冬十月前','通鑑具名馬當被殺，獨立年度；八月叙次而非另列死亡日期，保守僅年')
    results[7427]['historical_death']=date(345,dang345)
    results[7427]['evidence'].append(dang345)
    for result in results.values():
        if any(k.startswith('sl:') for k in result['keys']):
            result.setdefault('source_limitations',[]).append('本地SL十六國春秋為明屠喬孫項琳重編本，非崔鴻原著直傳；單獨記載不得視作獨立早期史料確證。')
        for field in ['historical_birth','historical_death']:
            value=result.get(field)
            if value and ('十六國春秋' in value.get('source','') or '十六国春秋' in value.get('source','')):
                result.setdefault('late_reconstruction_dates',{})[field]=value
                result[field]=None
                result['unresolved'].append('源層級回檢：上述SL日期移至late_reconstruction_dates，待獨立早源核明，不供嚴格補丁。')
        if result['keys'] and all(k.startswith('sl:') for k in result['keys']) and not result.get('independent_activity_evidence'):
            for field in ['attested_activity_years','last_attested_activity','appearance_upper_bound','first_attested_career_year']:
                if field in result:result.setdefault('late_reconstruction_activity',{})[field]=result.pop(field)
    (ROOT/'data/persons/reviews/cross_shard_7000.json').write_text(json.dumps({'decisions':cross},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    for result in results.values():
        if result['source_identities_all_adjudicated']:
            result.pop('date_scope',None)

    output=ROOT/'data/persons/reviews/review_7000_9492.jsonl'
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('w',encoding='utf-8',newline='\n') as handle:
        for result in results.values():
            handle.write(json.dumps(result,ensure_ascii=False)+'\n')
    print(json.dumps(dict(rows=len(results),statuses=collections.Counter(x['review_status'] for x in results.values()),
        birth_supported=sum(x['historical_birth'] is not None for x in results.values()),
        death_supported=sum(x['historical_death'] is not None for x in results.values()),
        changed_dates=sum(any(x['historical_'+k] and x['historical_'+k].get('year') is not None and x['historical_'+k]['year']!=x['current_game'][k] for k in ['birth','death']) for x in results.values()),
        duplicates=sum(x['duplicate_of'] is not None for x in results.values()),identity_conflicts=sum(x['review_status']=='identity_conflict' for x in results.values())),ensure_ascii=False))


if __name__=='__main__':
    main()
