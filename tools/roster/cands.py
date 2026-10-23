"""各来源 → 统一的候选人物格式（data/persons/build/cands_<来源>.jsonl）

候选格式：
  key       来源内唯一键（zhsan:123 / qxz:caopi / cbdb:30257 / tj:皇甫嵩@184 / gl:Q1001 / …）
  name      繁体全名；sei/mei 姓、名；azana 字；sex 0 男 1 女
  years     [最早, 最晚] 活动年；birth/death 生卒（可空）；death_violent
  origin    籍贯原文；lat/lon 坐标（海外人物）；polity 所属；role 身份；offices 官职
  kin       [[关系, 对方名字或键]]；friends/foes 名字
  stats     五维（可空）；lean 兵种倾向；bio 列传；src 出处；conf 可信度 A/B/C/D；fame 知名度（越大越有名）
  ethnic    民族（中国史书人物）；world 是否海外
"""
import glob
import json
import re
from pathlib import Path
from common import SRC, RAW, PERSONS, load_json, load_jsonl, save_jsonl, s2t

BUILD = PERSONS / "build"
H = re.compile(r"[一-鿿]")

# 中华三国志五维 → 光荣尺度（用两边都有的 670 人校出来的众数差）
ZH_OFF = [1, 2, -1, 4, -1]
ZH_SEIKAKU = {1: 3, 2: 0, 4: 2, 6: 1}
# 中华三国志剧本里给剧情用的泛称角色，不是人物
ZH_NOT_PERSON = {"傳令官", "旁白", "童子", "農民", "盜賊", "士兵", "百姓", "使者", "商人", "刺客", "山賊", "侍女"}


def norm_bio(s, limit=150):
    """列传整理：去掉换行和多余空白，按句截到 limit 字以内"""
    s = re.sub(r"\s+", "", s or "")
    s = s.replace("没有记载。", "").replace("沒有記載。", "")
    # 子代理写给我们看的说明（「原文记载仅此」「原文仅记姓与字」「事迹存疑」）不进列传
    s = "".join(x for x in re.split(r"(?<=[。；])", s) if not re.search(r"原文|本段|存疑|史料不详|知者甚少", x))
    if len(s) <= limit:
        return s
    cut = s[:limit]
    k = max(cut.rfind("。"), cut.rfind("；"))
    return cut[:k + 1] if k >= limit // 2 else cut[:limit - 1].rstrip("，、") + "。"


def from_zhsan():
    out = []
    for e in load_json(SRC / "zhsan.json"):
        name = (e["SurName"] or "") + (e["GivenName"] or "")
        if not name or not H.search(name) or name in ZH_NOT_PERSON:
            continue
        stats = [max(1, min(100, e[f] + o)) for f, o in zip(
            ["BaseCommand", "BaseStrength", "BaseIntelligence", "BasePolitics", "BaseGlamour"], ZH_OFF)]
        hist = e["history"]
        m = re.search(r"([一-鿿]{2,8}?)人(?:也|。|，)", hist[:120])
        bio = hist if len(hist) >= 20 else (e["brief"] + (e["romance"] if len(e["brief"]) < 30 else ""))
        bio = re.sub(r"^《[^》]*》[：:]", "", bio)
        rom = ("演義" in e["brief"] or "演义" in e["brief"]) or (len(hist) < 20 and len(e["romance"]) > 20)
        out.append({
            "key": f"zhsan:{e['zh_id']}", "zh_id": e["zh_id"], "name": name, "sei": e["SurName"], "mei": e["GivenName"],
            "azana": e["CalledName"] or "", "sex": 1 if e["Sex"] else 0,
            "birth": e["YearBorn"], "death": e["YearDead"], "appear": e["YearAvailable"],
            "years": [e["YearAvailable"], e["YearDead"]], "death_violent": e["DeadReason"] in (1, 2, 3),
            "origin": m.group(1) if m else "", "stats": stats,
            "seikaku": ZH_SEIKAKU.get(e["PCharacter"]), "giri": e["PersonalLoyalty"], "yabou": e["Ambition"],
            "aishou": (e["Ideal"] - 75) % 150, "generation": e["Generation"],
            "kin": [["父", f"zhsan:{e['father']}"]] * (e["father"] >= 0) + [["母", f"zhsan:{e['mother']}"]] * (e["mother"] >= 0)
                   + [["配偶", f"zhsan:{e['spouse']}"]] * (e["spouse"] >= 0) + [["义兄弟", f"zhsan:{b}"] for b in e["brothers"]],
            "friends": [f"zhsan:{x}" for x in e["close"]], "foes": [f"zhsan:{x}" for x in e["hated"]],
            "bio": norm_bio(bio), "src": "中華三國志 " + e["scen"], "conf": "C" if rom else "A",
            "romance": rom, "fame": 60,
        })
    return out


QXZ_TIER = {  # 档位 → 五维基准（统 武 智 政 魅）
    "ru": [80, 70, 75, 78, 85], "ru2": [60, 50, 65, 65, 70], "quan": [85, 75, 78, 65, 75], "wu1": [80, 88, 55, 40, 65],
    "wu2": [72, 78, 52, 40, 58], "wu3": [62, 68, 42, 35, 48], "zhi1": [70, 35, 92, 80, 72], "zhi2": [55, 30, 80, 70, 60],
    "zheng1": [45, 30, 75, 88, 72], "zheng2": [35, 25, 65, 75, 60], "fan": [40, 40, 40, 40, 40]}
QXZ_LEAN = {"spear": "枪", "halberd": "戟", "bow": "弩", "cavalry": "骑", "cav": "骑", "navy": "水", "siege": "兵器"}
QXZ_ROLE = {"ru": "君主", "ru2": "君主", "quan": "武将", "wu1": "武将", "wu2": "武将", "wu3": "武将", "zhi1": "谋士",
            "zhi2": "谋士", "zheng1": "文臣", "zheng2": "文臣", "fan": "其他"}


def from_qxz():
    import hashlib
    out = []
    for e in load_json(SRC / "qxz.json"):
        base = QXZ_TIER.get(e["tier"], QXZ_TIER["fan"])
        h = hashlib.md5(e["slug"].encode()).digest()
        stats = [max(1, min(99, b + (h[i] % 11) - 5)) for i, b in enumerate(base)]
        g = e["group"]
        polity = "曹魏" if g.startswith("魏") else "蜀漢" if g.startswith("蜀") else "孫吳" if g.startswith("吳") else ""
        out.append({"key": f"qxz:{e['slug']}", "name": e["name"], "azana": e["azana"], "sex": 0,
                    "birth": e["birth"], "death": e["death"], "years": [e["birth"], e["death"]],
                    "stats": stats, "lean": QXZ_LEAN.get(e["lean"], "无"), "role": QXZ_ROLE.get(e["tier"], "其他"),
                    "polity": polity, "group": g, "src": "三國群雄志扩容名单", "conf": "C" if e["romance"] else "B",
                    "romance": e["romance"], "fame": 40})
    return out


def from_cbdb():
    out = []
    for e in load_json(SRC / "cbdb.json"):
        offices = [o for o, _ in e.get("offices", []) if o]
        yrs = [y for y in [e["birth"], e["death"], e["index_year"], *e["fl"]] if y]
        out.append({"key": f"cbdb:{e['cbdb_id']}", "name": e["name"], "sei": e["sei"], "mei": e["mei"],
                    "azana": e.get("azana", ""), "sex": 1 if e["female"] else 0, "birth": e["birth"], "death": e["death"],
                    "years": [min(yrs), max(yrs)] if yrs else [None, None], "origin": (e.get("addr") or [""])[0],
                    "offices": offices, "polity": e["dynasty"],
                    "kin": [[r, f"cbdb:{k}"] for k, r, _ in e.get("kin", [])],
                    "src": "CBDB " + "；".join(e.get("sources", [])[:2]), "conf": "B", "fame": 20 + 5 * len(offices)})
    return out


def from_text(prefix):
    """子代理从史书原文抽取的结果 raw/<prefix>_NN.jsonl"""
    out = []
    for f in sorted(glob.glob(str(RAW / f"{prefix}_*.jsonl"))):
        for n, e in enumerate(load_jsonl(f)):
            name = s2t(e.get("name", "")).strip()
            note = re.findall(r"[（(](.*?)[)）]", name)            # 「张氏(蜀后)」：括号里是区分同名的注
            name = re.sub(r"[（(].*?[)）]", "", name).strip()
            if not (2 <= len(name) <= 5) or not all(H.match(c) for c in name):
                continue
            yrs = e.get("years") or [None, None]
            k = f"{prefix}:{Path(f).stem}:{n}:{name}"
            e.setdefault("alias", []).extend(note)
            out.append({
                "key": k, "name": name, "azana": s2t(e.get("azana") or ""), "alias": [s2t(a) for a in e.get("alias") or []],
                "sex": 1 if e.get("sex") == "f" else 0, "birth": e.get("birth"), "death": e.get("death"),
                "years": yrs, "death_violent": bool(e.get("death_violent")), "origin": e.get("origin") or "",
                "ethnic": e.get("ethnic") or "汉", "polity": s2t(e.get("polity") or ""),
                "polity_path": [s2t(x) for x in e.get("polity_path") or []], "role": e.get("role") or "其他",
                "offices": [s2t(o) for o in e.get("offices") or []],
                "kin": [[s2t(x.get("rel", "")), s2t(x.get("name", ""))] for x in e.get("kin") or [] if x.get("name")],
                "friends": [s2t(x) for x in e.get("friends") or []], "foes": [s2t(x) for x in e.get("foes") or []],
                "bio": norm_bio(s2t(e.get("deeds") or "")), "stats": e.get("stats"), "lean": e.get("lean") or "无",
                "src": s2t(e.get("src") or ""), "conf": "B",
                "fame": 10 + 3 * len(e.get("offices") or []) + len(e.get("deeds") or "") // 15})
    return out


def from_global():
    out = []
    src = {e["wikidata_code"]: e for e in load_json(SRC / "laouenan.json")}
    for f in sorted(glob.glob(str(RAW / "gl_*.jsonl"))):
        for e in load_jsonl(f):
            if e.get("skip") or not e.get("zh"):
                continue
            s = src.get(e.get("qid"), {})
            try:
                fame = float(s.get("sum_visib_ln_5criteria") or 0)
            except ValueError:
                fame = 0
            yrs = [e.get("birth"), e.get("death")]
            if yrs == [None, None]:                    # 子代理没给年份：用名人库里的生卒（常是估计值）
                yrs = [round(s["b"]) if s.get("b") else None, round(s["d"]) if s.get("d") else None]
            out.append({
                "key": f"gl:{e['qid']}", "qid": e["qid"], "name": e["zh"], "sei": e.get("sei"), "mei": e.get("mei"),
                "name_full": e.get("zh_full", ""), "name_orig": s.get("name", "").replace("_", " "),
                "sex": 1 if e.get("sex") == "f" else 0, "birth": e.get("birth"), "death": e.get("death"),
                "years": yrs, "death_violent": bool(e.get("death_violent")),
                "polity": e.get("polity", ""), "role": e.get("role", "其他"), "place": e.get("place", ""),
                "lat": e.get("lat"), "lon": e.get("lon"), "offices": e.get("offices") or [],
                "kin": [[x.get("rel", ""), ("gl:" + x["qid"]) if x.get("qid") else x.get("name", "")] for x in e.get("kin") or []],
                "friends": e.get("friends") or [], "foes": e.get("foes") or [], "bio": norm_bio(e.get("bio", "")),
                "stats": e.get("stats"), "lean": e.get("lean", "無"), "src": f"Wikidata {e['qid']}（Laouenan 等 2022）",
                "conf": "A" if e.get("known", True) else "C", "fame": 30 + 3 * fame, "world": True,
                "region_un": s.get("un_subregion", "")})
    return out


# 希腊、拉丁史书（raw/SPEC_CLASSIC.md）：希罗狄安、罗马君王传、优西比乌、佐西默斯、阿米阿努斯
CLASSIC = {"rg": "按地区查找", "wl": "Wikipedia（综述、列表条目）", "dio": "Cassius Dio", "hdn": "Herodian", "ha": "Historia Augusta", "eus": "Eusebius, HE", "zos": "Zosimus", "amm": "Ammianus",
           # 公版/开放网站上的史料（raw/SPEC_OPEN.md，split_open.py）
           "fau": "Faustus of Byzantium", "moi": "Moses of Khoren", "mhv": "Mahavamsa/Dipavamsa",
           "niya": "Niya Kharosthi documents (tr. Burrow)", "cth": "Codex Theodosianus", "je": "Jewish Encyclopedia",
           "soc": "Socrates, HE", "soz": "Sozomen, HE", "thd": "Theodoret, HE", "phs": "Philostorgius, HE",
           "vc": "Eusebius, Vita Constantini", "c354": "Chronography of 354", "eun": "Eunapius, Lives",
           "jor": "Jordanes, Getica", "lat": "Aurelius Victor / Eutropius / Lactantius"}
LOOSE = {"wl", "fau", "moi", "mhv", "niya", "cth", "je", "soc", "soz", "thd", "phs", "vc", "c354", "eun", "jor", "lat"}       # 各块分给不同子代理、同一人会在几块里重复出现的书


def en_key(s):
    return re.sub(r"\s*\(.*?\)", "", s or "").strip().lower()


def known_zh():
    """已收海外人物 英文名 → 中文名（统一子代理给的译名，好让合并认出同一人）"""
    f = PERSONS / "_cache" / "known_world.tsv"
    out = {}
    if f.exists():
        for line in f.read_text(encoding="utf-8").splitlines():
            parts = [x.strip() for x in line.split("|")]
            if len(parts) >= 2 and parts[0]:
                out.setdefault(parts[0].lower(), parts[1])
                out.setdefault(en_key(parts[0]), parts[1])
    return out


def from_classic(prefix, known):
    """known：英文名 → 中文名，会随读随记，不同块、不同子代理给同一个英文名的人用同一个中文名"""
    out = []
    loose = {}                                   # 综述条目（wl）各块之间：去掉括注、「Mar」后同名且年代重叠的算一人
    for f in sorted(glob.glob(str(RAW / f"{prefix}_*.jsonl"))):
        for n, e in enumerate(load_jsonl(f)):
            en = e.get("name_en") or ""
            # 带括注的名字（「Basil of Ancyra (martyr)」）是子代理有意和同名者分开的，只按全名对
            zh = known.get(en.lower()) or ("(" not in en and known.get(en_key(en))) or e.get("zh") or ""
            if not zh:
                continue
            if prefix in LOOSE:
                lk = re.sub(r"^(mar|saint|st\.?)\s+", "", en_key(en))
                ys = [y for y in (e.get("years") or []) + [e.get("birth"), e.get("death")] if isinstance(y, int)]
                for (k2, lo, hi), z2 in loose.items():
                    if k2 == lk and (not ys or lo is None or (min(ys) <= hi + 20 and max(ys) >= lo - 20)):
                        zh = z2
                        break
                else:
                    loose[(lk, min(ys) if ys else None, max(ys) if ys else None)] = zh
            known.setdefault(en.lower(), zh)
            yrs = e.get("years") or [None, None]
            out.append({
                "key": f"{prefix}:{Path(f).stem}:{n}:{en}", "name": zh, "sei": e.get("sei") if zh == e.get("zh") else None,
                "mei": e.get("mei") if zh == e.get("zh") else None, "name_full": e.get("zh_full", ""), "name_orig": en,
                "sex": 1 if e.get("sex") == "f" else 0, "birth": e.get("birth"), "death": e.get("death"),
                "years": yrs, "death_violent": bool(e.get("death_violent")), "polity": e.get("polity", ""),
                "role": e.get("role", "其他"), "place": e.get("place", ""), "lat": e.get("lat"), "lon": e.get("lon"),
                "offices": e.get("offices") or [],
                "kin": [[x.get("rel", ""), x.get("name", "")] for x in e.get("kin") or [] if x.get("name")],
                "friends": e.get("friends") or [], "foes": e.get("foes") or [], "bio": norm_bio(e.get("bio", "")),
                "stats": e.get("stats"), "lean": e.get("lean", "無"), "src": f"{CLASSIC[prefix]} {e.get('src', '')}".strip(),
                "conf": e.get("conf") if e.get("conf") in ("B", "C") else "B",
                "fame": 15 + 3 * len(e.get("offices") or []) + len(e.get("bio") or "") // 15, "world": True})
    return out


def from_wiki():
    """英文维基百科条目（scan_wiki.py → batch_wiki.py → 子代理 raw/wk_*.jsonl，规格同 SPEC_GLOBAL）"""
    out = []
    src = {e["title"]: e for e in (load_json(SRC / "wiki.json") if (SRC / "wiki.json").exists() else [])}
    for f in sorted(glob.glob(str(RAW / "wk_*.jsonl"))):
        for e in load_jsonl(f):
            if e.get("skip") or not e.get("zh"):
                continue
            s = src.get(e.get("title"), {})
            yrs = [e.get("birth"), e.get("death")]
            if yrs == [None, None]:                    # 子代理没给年份：用分类、信息框里的年代
                rb, rd, act = s.get("b"), s.get("d"), s.get("act")
                if act:
                    yrs = list(act)
                elif rb or rd:
                    yrs = [(rb[0] + rb[1]) // 2 + 20 if rb else None, (rd[0] + rd[1]) // 2 if rd else None]
            out.append({
                "key": f"wk:{e.get('qid') or e.get('title')}", "qid": e.get("qid") or s.get("qid", ""), "name": e["zh"],
                "sei": e.get("sei"), "mei": e.get("mei"), "name_full": e.get("zh_full", ""), "name_orig": e.get("title", ""),
                "sex": 1 if e.get("sex") == "f" else 0, "birth": e.get("birth"), "death": e.get("death"),
                "years": yrs, "death_violent": bool(e.get("death_violent")),
                "polity": e.get("polity", ""), "role": e.get("role", "其他"), "place": e.get("place", ""),
                "lat": e.get("lat"), "lon": e.get("lon"), "offices": e.get("offices") or [],
                "kin": [[x.get("rel", ""), x.get("name", "")] for x in e.get("kin") or [] if x.get("name")],
                "friends": e.get("friends") or [], "foes": e.get("foes") or [], "bio": norm_bio(e.get("bio", "")),
                "stats": e.get("stats"), "lean": e.get("lean", "無"),
                "src": f"English Wikipedia「{e.get('title')}」" + (f"（{s['qid']}）" if s.get("qid") else ""),
                "conf": "B" if e.get("known", True) and not s.get("legend") else "C",
                "fame": 20 + 2 * len(s.get("langs") or {}) + len(s.get("lead") or "") // 60, "world": True})
    return out


if __name__ == "__main__":
    BUILD.mkdir(parents=True, exist_ok=True)
    for name, fn in [("zhsan", from_zhsan), ("qxz", from_qxz), ("cbdb", from_cbdb), ("global", from_global),
                     ("wiki", from_wiki)]:
        rows = fn()
        save_jsonl(BUILD / f"cands_{name}.jsonl", rows)
        print(name, len(rows))
    known = known_zh()
    for prefix in CLASSIC:
        rows = from_classic(prefix, known)
        save_jsonl(BUILD / f"cands_{prefix}.jsonl", rows)
        print(prefix, len(rows))
    for prefix in sorted({Path(f).stem.split("_")[0] for f in glob.glob(str(RAW / "*_*.jsonl"))} - {"gl", "wk"} - set(CLASSIC)):
        rows = from_text(prefix)
        save_jsonl(BUILD / f"cands_{prefix}.jsonl", rows)
        print(prefix, len(rows))
