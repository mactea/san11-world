"""组装最终名单：build/merged.jsonl（+ 光荣 850 人）→ data/persons/roster.json 等

    python build.py [--total 15000]

1. 年代筛选：在世时间要与 170..370 年有交集
2. 姓名：繁体、Big5、姓名各 1~2 字（超长的名截成 2 字，全名留在 name_full）
3. 出身城：中国人按籍贯（gazetteer），海外人按经纬度最近城，都没有就按所属政权的默认城
4. 属性：五维（来源给的，没有就按身份估）、适性、特技、性格类、相性等（attrs.py）
5. 编号：光荣 0..849 不变；扩展武将 850..total-1；人数不足用虚构补位（fiction.py，可信度 D）
6. 关系：父母、配偶、义兄弟、血缘、亲近 5、厌恶 5 解析成编号（可以指向光荣武将）"""
import argparse
import collections
import re
from attrs import aptitude, pick_skill, personality, raw_defaults, rnd, clamp, HOME, skills
from cands import BUILD, norm_bio
from common import PERSONS, SRC, big5_ok, load_json, load_jsonl, save_json, save_jsonl, split_name, s2t
from fiction import make as make_fiction, REGION_CULTURE
from gazetteer import origin_city, nearest_city
from merge import nkey, compatible

YEAR_LO, YEAR_HI = 170, 370

# 所属政权 → (默认城, 相性基准)。相性 0..149 是一个环，光荣设定：曹操 25 刘备 75 孙权 125 袁绍 101 董卓 1 吕布 145
POLITY = {
    "东汉": ("洛陽", 80), "東漢": ("洛陽", 80), "汉": ("洛陽", 80), "漢": ("洛陽", 80), "董卓": ("長安", 2),
    "袁绍": ("鄴", 101), "袁紹": ("鄴", 101), "袁术": ("壽春", 140), "袁術": ("壽春", 140), "曹操": ("許昌", 25),
    "曹魏": ("洛陽", 25), "魏": ("洛陽", 25), "三國魏": ("洛陽", 25), "蜀汉": ("成都", 75), "蜀漢": ("成都", 75),
    "蜀": ("成都", 75), "三國蜀": ("成都", 75), "刘备": ("成都", 75), "劉備": ("成都", 75), "孙吴": ("建業", 125),
    "孫吳": ("建業", 125), "吴": ("建業", 125), "吳": ("建業", 125), "三國吳": ("建業", 125), "孙权": ("建業", 125),
    "孫權": ("建業", 125), "西晋": ("洛陽", 31), "西晉": ("洛陽", 31), "晋": ("洛陽", 31), "晉": ("洛陽", 31),
    "东晋": ("建業", 36), "東晉": ("建業", 36), "黄巾": ("鄴", 7), "黃巾": ("鄴", 7), "刘表": ("襄陽", 45),
    "劉表": ("襄陽", 45), "刘璋": ("成都", 55), "劉璋": ("成都", 55), "刘焉": ("成都", 55), "劉焉": ("成都", 55),
    "公孙度": ("襄平", 64), "公孫度": ("襄平", 64), "公孙瓒": ("北平", 65), "公孫瓚": ("北平", 65), "吕布": ("下邳", 145),
    "呂布": ("下邳", 145), "马腾": ("武威", 70), "馬騰": ("武威", 70), "张鲁": ("漢中", 9), "張魯": ("漢中", 9),
    "前赵": ("晉陽", 138), "前趙": ("晉陽", 138), "汉赵": ("晉陽", 138), "漢趙": ("晉陽", 138), "后赵": ("鄴", 146),
    "後趙": ("鄴", 146), "成汉": ("成都", 57), "成漢": ("成都", 57), "前凉": ("武威", 68), "前涼": ("武威", 68),
    "前燕": ("薊", 110), "前秦": ("長安", 90), "代": ("晉陽", 116), "冉魏": ("鄴", 148), "段部": ("北平", 104),
    "宇文部": ("北平", 106), "慕容部": ("襄平", 110), "拓跋部": ("晉陽", 116), "南匈奴": ("晉陽", 135),
    "匈奴": ("晉陽", 135), "鲜卑": ("北平", 108), "鮮卑": ("北平", 108), "乌桓": ("北平", 104), "烏桓": ("北平", 104),
    "南蛮": ("建寧", 60), "南蠻": ("建寧", 60), "山越": ("會稽", 118), "孙策": ("吳", 126), "孫策": ("吳", 126),
    "孙坚": ("長沙", 126), "孫堅": ("長沙", 126), "白波": ("洛陽", 10), "黑山": ("鄴", 8), "张燕": ("鄴", 8),
    "張燕": ("鄴", 8), "刘繇": ("吳", 85), "劉繇": ("吳", 85), "陶谦": ("下邳", 82), "陶謙": ("下邳", 82),
    "张绣": ("宛", 18), "張繡": ("宛", 18), "韩遂": ("武威", 72), "韓遂": ("武威", 72), "士燮": ("交趾", 120),
    "李傕": ("長安", 4), "郭汜": ("長安", 4), "公孙渊": ("襄平", 64), "公孫淵": ("襄平", 64), "孟获": ("建寧", 60),
    "羅馬帝國": (None, 12), "羅馬": (None, 12), "羅馬教會": (None, 18), "帕提亞": (None, 50), "安息": (None, 50),
    "薩珊波斯": (None, 47), "貴霜": (None, 95), "亞美尼亞": (None, 40), "帕爾米拉": (None, 44), "高句麗": ("丸都", 107),
    "百濟": ("馬韓", 112), "新羅": ("斯盧", 114), "倭": ("邪馬台", 117), "夫餘": ("扶餘", 109), "哥特": (None, 132), "法蘭克": (None, 128),
    "阿勒曼尼": (None, 130), "阿克蘇姆": (None, 88),
}
ETHNIC_CITY = {"匈奴": "晉陽", "南匈奴": "晉陽", "鲜卑": "龍城", "乌桓": "北平", "羌": "天水", "氐": "天水", "羯": "晉陽",
               "賨": "江州", "巴": "江州", "蛮": "武陵", "山越": "會稽", "高句丽": "丸都", "夫余": "扶餘", "倭": "邪馬台",
               "西域": "龜茲"}
ROLE_STATS = {"君主": [70, 60, 65, 65, 75], "宗室": [50, 45, 55, 55, 60], "后妃": [10, 10, 55, 45, 70],
              "武将": [65, 68, 45, 35, 50], "文臣": [35, 30, 62, 68, 55], "谋士": [45, 30, 72, 60, 55],
              "宦官": [15, 15, 55, 45, 30], "外戚": [45, 40, 45, 45, 50], "名士": [20, 15, 70, 55, 70],
              "隐士": [15, 15, 68, 45, 60], "方士": [20, 15, 70, 35, 60], "僧道": [15, 10, 68, 40, 65],
              "宗教": [15, 10, 68, 45, 68], "学者": [15, 10, 75, 50, 55], "外族首领": [62, 68, 40, 35, 55],
              "叛军首领": [60, 62, 45, 30, 60], "其他": [40, 40, 45, 45, 45]}
REL = {"父": "father", "母": "mother", "妻": "spouse", "夫": "spouse", "配偶": "spouse", "义兄弟": "sworn",
       "義兄弟": "sworn"}
CHILD = {"子", "女", "长子", "長子", "次子", "少子", "嗣子", "养子", "養子"}
SIBLING = {"兄", "弟", "姊", "妹", "姐", "从兄", "從兄", "从弟", "從弟", "兄弟", "族兄", "族弟"}


POLITY_KW = [("晉", "西晉"), ("晋", "西晋"), ("魏", "曹魏"), ("吳", "孫吳"), ("吴", "孙吴"), ("蜀", "蜀漢"),
             ("趙", "後趙"), ("赵", "后赵"), ("燕", "前燕"), ("秦", "前秦"), ("涼", "前涼"), ("凉", "前凉"),
             ("漢", "東漢"), ("汉", "东汉"), ("羅馬", "羅馬帝國"), ("波斯", "薩珊波斯")]


def polity_info(pol):
    """政权 → (默认城, 相性基准)；表里没有的按关键字（「赵王伦」→ 晋 …）"""
    if not pol:
        return (None, None)
    if pol in POLITY:
        return POLITY[pol]
    for kw, std in POLITY_KW:
        if kw in pol:
            return POLITY[std]
    return (None, None)


def big5_fix(s):
    """Big5 编不了的字：换成异体字表里的写法，没有就去掉"""
    out = []
    for ch in s:
        if big5_ok(ch):
            out.append(ch)
            continue
        alt = next((a for a in VARIANT.get(ch, "") if big5_ok(a)), None)
        out.append(alt or "")
    return "".join(out)


VARIANT = {"堃": "坤", "喆": "哲", "峯": "峰", "羣": "群", "衆": "眾", "爲": "為", "裏": "裡", "綫": "線", "麪": "麵",
           "嵗": "歲", "啓": "啟", "敍": "敘", "眞": "真", "淸": "清", "靑": "青", "鷄": "雞", "絶": "絕", "巖": "岩",
           "鍊": "煉", "迹": "跡", "粧": "妝", "吿": "告", "髙": "高", "徳": "德", "兊": "兌", "姫": "姬", "鉅": "巨",
           "竪": "豎", "廐": "廄", "鄕": "鄉", "曺": "曹", "槪": "概", "卽": "即", "旣": "既", "鬭": "鬥", "鬪": "鬥",
           "飮": "飲", "珎": "珍", "墻": "牆", "蔿": "蒍", "禕": "褘", "暠": "皓", "頵": "君", "璿": "璇",
           "昞": "昺", "瓌": "瑰", "顗": "凱", "綝": "林", "瑍": "煥", "鯈": "儵", "瑨": "晉", "儁": "俊", "伷": "胄",
           "璝": "瑰", "叚": "段", "寜": "寧", "冲": "沖", "逺": "遠", "熈": "熙", "禀": "稟", "覇": "霸", "鲂": "魴",
           "禇": "褚", "蘓": "蘇", "荆": "荊", "辥": "薛", "嚻": "囂", "曁": "暨", "顔": "顏", "麹": "麴", "鞾": "靴",
           "郄": "郤", "憙": "喜", "斅": "教", "暅": "亘", "卫": "衛", "徳": "德", "灋": "法", "陁": "陀", "婣": "姻",
           "秊": "年", "邨": "村", "蒐": "搜", "匄": "丐", "隂": "陰", "庻": "庶", "旹": "時", "蔵": "藏", "琎": "璡"}


def life(p):
    """补齐生年、登场、卒年（光荣习惯：登场 ≈ 生年 + 16）"""
    y0, y1 = p.get("years") or [None, None]
    b, d = p.get("birth"), p.get("death")
    first = y0 or b or d
    last = y1 or d or b
    if b is None:
        b = (first - (22 if p["role"] in ("武将", "文臣", "谋士", "宦官") else 18)) if first else None
    if b is None:
        b = 190
    b = int(b)
    if d is None:
        d = max(int(last) + 8 if last else b + 55, b + 30)
        d = min(d, b + 75)
    d = int(d)
    if d <= b:
        d = b + 30
    appear = p.get("appear") or b + 16
    appear = int(min(max(appear, b + 12), d))
    return b, appear, d


def in_period(p):
    if len(p["name"]) < 2:
        return False
    b, a, d = life(p)
    return d >= YEAR_LO and b <= YEAR_HI - 15


def fix_role(p, stats):
    """中华三国志、CBDB 没有身份字段：按五维推断"""
    if p.get("role") not in (None, "", "其他"):
        return p["role"]
    if p.get("sex") == 1:
        return "后妃" if max(stats[:2]) < 50 else "武将"
    return "武将" if max(stats[:2]) >= max(stats[2:4]) else "文臣"


def names(p):
    name = s2t(p["name"])
    full = p.get("name_full") or name
    sei, mei = p.get("sei") or "", p.get("mei") or ""
    if not sei or not mei or sei + mei != name or len(sei) > 2 or len(mei) > 2:
        sei, mei = split_name(name)
    if len(mei) > 2:
        mei = mei[:2]
    if len(sei) > 2:
        sei = sei[:2]
    return big5_fix(sei), big5_fix(mei), full


def home_city(p, region_of):
    if p.get("city"):
        return p["city"]
    if p.get("world") and p.get("lat") is not None and p.get("lon") is not None:
        return nearest_city(p["lat"], p["lon"])[0]
    c = origin_city(p.get("origin") or "")
    if c:
        return c
    for o in reversed(p.get("offices") or []):          # 没有籍贯：按任职的郡（「北地太守」「荊州刺史」→ 治所）
        m = re.match(r"(.{1,3}?)(太守|刺史|內史|内史|相|令|長|都尉|牧|校尉|護軍)$", s2t(o))
        if m and (c := origin_city(m.group(1))):
            return c
    eth = p.get("ethnic")
    if eth in ETHNIC_CITY and ETHNIC_CITY[eth]:
        return ETHNIC_CITY[eth]
    for pol in [p.get("polity")] + list(reversed(p.get("polity_path") or [])):
        if polity_info(pol)[0]:
            return polity_info(pol)[0]
    return "洛陽" if not p.get("world") else None


def tier_of(p, stats):
    m = max(stats)
    return 2 if m >= 90 and p.get("conf") in ("A", "B") else 1 if m >= 78 else 0


def lift(x):
    """史书抽取的五维整体比光荣偏低（光荣只收有名的人）：70 以下往上拉一点，不改变高低顺序"""
    return x + (70 - x) * 0.2 if x < 70 else x


def est_stats(p):
    s = p.get("stats")
    if s and len(s) == 5 and all(isinstance(x, (int, float)) for x in s):
        src = p["keys"][0].split(":")[0]
        return [clamp(lift(x) if src not in ("zhsan", "qxz") else x) for x in s]
    base = ROLE_STATS.get(p.get("role") or "其他", ROLE_STATS["其他"])
    bump = min(15, len(p.get("offices") or []) * 2)
    return [clamp(b + bump * (i in (0, 3)) + rnd(p["keys"][0], f"st{i}", 15) - 7) for i, b in enumerate(base)]


def fallback_bio(p, city):
    parts = []
    if p.get("origin"):
        parts.append(f"{s2t(p['origin'])}人。")
    elif city:
        parts.append(f"{city.split('(')[0]}出身。")
    if p.get("offices"):
        parts.append("歷任" + "、".join(p["offices"][:4]) + "。")
    if p.get("polity"):
        parts.append(f"仕於{p['polity']}。")
    return "".join(parts) or "事蹟不詳。"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--total", type=int, default=15000)
    args = ap.parse_args()
    NEW = args.total - 850
    koei = load_json(SRC / "koei.json")
    cities = load_json(SRC / "cities.json")
    city_by = {c["full"]: c for c in cities}
    region_of = {c["full"]: c["region"] for c in cities}
    merged = [p for p in load_jsonl(BUILD / "merged.jsonl") if in_period(p) and all(names(p)[:2])]
    # 最后再查一遍「其实是光荣武将」：繁简、异体统一后同名，且生卒相差不到 10 年
    kidx = collections.defaultdict(list)
    for k in koei:
        if k["sei"] and (k["koei_id"] < 670 or 700 <= k["koei_id"] < 800):
            kidx[nkey(k["sei"] + k["mei"])].append((k["birth"], k["death"]))
    def is_koei(p):
        b, _, d = life(p)
        n = "".join(names(p)[:2])
        strict = n.endswith("氏")                         # 「劉氏」这类称呼重名很多：生卒都要对上
        return any((abs(kb - b) <= 10 and abs(kd - d) <= 10) if strict else (abs(kb - b) <= 10 or abs(kd - d) <= 10)
                   for kb, kd in kidx.get(nkey(n), []))
    dup = [p for p in merged if is_koei(p)]
    merged = [p for p in merged if not is_koei(p)]
    print("与光荣武将重复、剔除", len(dup), [p["name"] for p in dup[:20]])
    in_koei = load_jsonl(BUILD / "in_koei.jsonl")
    # ---- 排序：可信度 A/B 在前，同级按知名度；超过名额时从后面截掉
    order = {"A": 0, "B": 1, "C": 2, "D": 3}
    merged.sort(key=lambda p: (order[p["conf"]], -p.get("fame", 0)))
    real = merged[:NEW]
    dropped = merged[NEW:]
    # ---- 出身城
    for p in real:
        p["city"] = home_city(p, region_of)
        if p["city"] and p["city"] not in city_by:
            p["city"] = "洛陽" if not p.get("world") else None
    nocity = [p for p in real if not p["city"]]
    for p in nocity:                                   # 海外又没坐标：按联合国分区给一个城
        p["city"] = {"Southern Europe": "羅馬", "Western Europe": "盧格杜努姆(里昂)", "North Africa": "亞歷山大",
                     "Western Asia (Middle East Caucasus)": "安條克", "South Asia incl. Indian Peninsula": "華氏城",
                     "Eastern Europe": "錫爾米烏姆"}.get(p.get("region_un"), "羅馬")
        if p["city"] not in city_by:
            p["city"] = "羅馬"
    # ---- 虚构补位：按城平均，人数最少的城先补
    per_city = collections.Counter(p["city"] for p in real)
    fic, used = [], {p["name"] for p in real} | {k["sei"] + k["mei"] for k in koei}
    need = NEW - len(real)
    overseas = [c for c in cities if c["id"] >= 42 and c["id"] < 500]
    serial = collections.Counter()
    while len(fic) < need:
        c = min(overseas, key=lambda c: (per_city[c["full"]], c["id"]))
        f = make_fiction(c, serial[c["id"]], used)
        serial[c["id"]] += 1
        per_city[c["full"]] += 1
        if f:
            f["keys"] = [f["key"]]
            fic.append(f)
    allp = real + fic
    # ---- 编号：中国人物按生年，海外按地区再按生年，虚构最后
    for p in allp:
        p["_life"] = life(p)
    allp.sort(key=lambda p: (p.get("conf") == "D", bool(p.get("world")), region_of.get(p["city"], "") if p.get("world") else "",
                             p["_life"][0], p["name"]))
    for i, p in enumerate(allp):
        p["id"] = 850 + i
    # ---- 关系解析用的索引
    by_key, by_name = {}, collections.defaultdict(list)
    for k in koei:
        if k["sei"] and not (670 <= k["koei_id"] < 700):
            by_name[nkey(k["sei"] + k["mei"])].append((k["koei_id"], (k["birth"], k["death"])))
    for m in in_koei:
        for key in m["keys"]:
            by_key[key] = m["koei_id"]
    for p in allp:
        for key in p.get("keys", []):
            by_key[key] = p["id"]
        by_name[nkey(p["name"])].append((p["id"], (p["_life"][0], p["_life"][2])))
        for a in p.get("alias") or []:
            if 2 <= len(a) <= 4 and a[0] == p["name"][0]:
                by_name[nkey(a)].append((p["id"], (p["_life"][0], p["_life"][2])))

    by_en = collections.defaultdict(list)                     # 海外人物的亲属、敌友按英文名写
    for p in allp:
        for n in {p.get("name_orig") or "", *(p.get("alias_en") or [])}:
            n = re.sub(r"\s*\(.*?\)", "", n).strip().lower()
            if n:
                by_en[n].append(p["id"])

    kinfo = {k["koei_id"]: (k["birth"], k["death"], k["sex"]) for k in koei}
    pinfo = {p["id"]: (p["_life"][0], p["_life"][2], p["sex"]) for p in allp}

    def info(i):
        return pinfo.get(i) or kinfo.get(i)

    def resolve(ref, near, pred=None):
        """名字/键 → 编号；pred 过滤不合年龄、性别的同名者"""
        ok = (lambda t: t is not None and info(t) and (pred is None or pred(info(t))))
        if ref in by_key:
            t = by_key[ref]
            return t if ok(t) else None
        if ":" in ref:
            return None
        if re.fullmatch(r"[A-Za-z .'\-]+", ref):
            hits = [t for t in by_en.get(re.sub(r"\s*\(.*?\)", "", ref).strip().lower(), []) if ok(t)]
            return hits[0] if len(hits) == 1 else None
        hits = [i for i, s in by_name.get(nkey(s2t(ref)), []) if compatible(s, near) and ok(i)]
        return hits[0] if hits else None

    rel = {p["id"]: {"father": -1, "mother": -1, "spouse": -1, "sworn": -1, "liked": [], "disliked": []} for p in allp}
    for p in allp:
        b, _, d = p["_life"]
        near = (b - 10, d + 10)
        r = rel[p["id"]]
        alive = lambda t: t[0] <= d and t[1] >= b                                   # 生存年代有重叠
        for kind, ref in p.get("kin") or []:
            f = REL.get(kind)
            if f in ("father", "mother"):
                pred = lambda t, f=f: b - 60 <= t[0] <= b - 14 and t[1] >= b - 1 and t[2] == (0 if f == "father" else 1)
            elif f == "spouse":
                pred = lambda t: t[2] != p["sex"] and abs(t[0] - b) <= 35
            elif kind in CHILD:
                pred = lambda t: b + 14 <= t[0] <= min(b + 60, d + 1)
            elif kind in SIBLING:
                pred = lambda t: abs(t[0] - b) <= 25
            else:
                pred = alive
            t = resolve(ref, near, pred)
            if t is None or t == p["id"]:
                continue
            if f and r[f] == -1:
                r[f] = t
                if f == "spouse" and t in rel and rel[t]["spouse"] == -1:
                    rel[t]["spouse"] = p["id"]
            elif kind in CHILD and t in rel:
                parent = "mother" if p["sex"] == 1 else "father"
                if rel[t][parent] == -1:
                    rel[t][parent] = p["id"]
            elif kind in SIBLING and len(r["liked"]) < 5 and t not in r["liked"]:
                r["liked"].append(t)
        for ref in p.get("friends") or []:
            t = resolve(ref, near, alive)
            if t is not None and t != p["id"] and t not in r["liked"] and len(r["liked"]) < 5:
                r["liked"].append(t)
        for ref in p.get("foes") or []:
            t = resolve(ref, near, alive)
            if t is not None and t != p["id"] and t not in r["disliked"] and t not in r["liked"] and len(r["disliked"]) < 5:
                r["disliked"].append(t)
    gen, blood = {}, {}
    byid = {p["id"]: p for p in allp}
    kblood = {k["koei_id"]: k["blood"] for k in koei}
    kgen = {k["koei_id"]: k["generation"] for k in koei}

    def gen_of(i, depth=0):
        if i < 850:
            return kgen.get(i, 1)
        if i in gen:
            return gen[i]
        f = rel[i]["father"] if rel[i]["father"] >= 0 else rel[i]["mother"]
        gen[i] = 1 if f < 0 or depth > 6 else min(4, gen_of(f, depth + 1) + 1)
        return gen[i]

    def blood_of(i, depth=0):
        if i < 850:
            return kblood.get(i, i)
        if i in blood:
            return blood[i]
        f = rel[i]["father"]
        blood[i] = i if f < 0 or depth > 6 else blood_of(f, depth + 1)
        return blood[i]

    # ---- 属性和输出记录
    sk = skills()
    out = []
    for p in allp:
        i = p["id"]
        b, appear, d = p["_life"]
        sei, mei, full = names(p)
        stats = est_stats(p)
        p["role"] = fix_role(p, stats)
        region = region_of.get(p["city"], "")
        tier = tier_of(p, stats)
        lean = p.get("lean") or "无"
        pol = p.get("polity") or ""
        pk = p["keys"][0]
        base_aishou = polity_info(pol)[1]
        if base_aishou is None:
            base_aishou = rnd(region or pol or pk, "aishou", 150)
        aishou = p.get("aishou")
        if aishou is None:
            aishou = (base_aishou + rnd(pk, "ai", 13) - 6) % 150
        kan = 2 if pol in ("东汉", "東漢", "蜀汉", "蜀漢", "汉", "漢") and p["role"] not in ("宦官", "叛军首领") else \
            0 if (p.get("world") or p.get("ethnic") not in (None, "", "汉", "漢") or p["role"] == "叛军首领") else 1
        pers = personality(pk, {"stats": stats, "role": p["role"], "sex": p["sex"], "kanshitsu": kan,
                                "rebel": p["role"] == "叛军首领"})
        for f in ("giri", "yabou", "seikaku"):
            if p.get(f) is not None:
                pers[f] = p[f]
        military = max(stats[:2]) >= max(stats[2:4])
        raw = raw_defaults(pk, p["sex"], tier, military)
        skill = pick_skill(pk, stats, lean, p["role"], tier)
        bio = p.get("bio") or fallback_bio(p, p["city"])
        r = rel[i]
        out.append({
            "id": i, "sei": sei, "mei": mei, "azana": big5_fix((p.get("azana") or "")[:2]), "name_full": full,
            "name_orig": p.get("name_orig", ""), "sex": p["sex"], "birth": b, "appear": appear, "death": d,
            "death_type": 1 if p.get("death_violent") else 0,
            "city": p["city"], "city_id": city_by[p["city"]]["id"] if p["city"] in city_by else None, "region": region,
            "home": HOME.get(region, 11 if p.get("world") else 4), "polity": pol, "role": p["role"],
            "offices": p.get("offices") or [], "stats": stats, "apt": aptitude(pk, stats, lean, region),
            "growth": raw["growth"], "lean": lean, "skill": skill, "skill_name": sk[skill]["name"] if skill in sk else "",
            **pers, "aishou": int(aishou), "generation": 0, "father": r["father"], "mother": r["mother"],
            "spouse": r["spouse"], "sworn": r["sworn"], "blood": 0, "liked": r["liked"], "disliked": r["disliked"],
            "bio": norm_bio(big5_fix(bio)), "raw": {k: v for k, v in raw.items() if k != "growth"},
            "face": -1, "face_class": f"{'女' if p['sex'] else '男'}·{'武' if military else '文'}·"
                                     f"{REGION_CULTURE.get(region, '胡' if p.get('ethnic') not in (None, '', '汉', '漢') else '漢')}",
            "conf": p.get("conf", "C"), "romance": bool(p.get("romance")), "src": p.get("src") or [],
            "keys": p.get("keys", []), "qid": p.get("qid", ""),
        })
    for e in out:
        e["generation"] = gen_of(e["id"])
        e["blood"] = blood_of(e["id"])
    save_json(PERSONS / "roster.json", out)
    save_jsonl(BUILD / "dropped.jsonl", dropped) if dropped else None
    report(out, koei, len(merged), len(dropped))


def report(out, koei, n_real_all, n_dropped):
    from common import save_json as sj
    by_conf = collections.Counter(e["conf"] for e in out)
    by_region = collections.Counter(e["region"] for e in out)
    by_src = collections.Counter(s.split(" ")[0].split("·")[0] for e in out for s in e["src"][:1])
    bad = [e["id"] for e in out if not (big5_ok(e["sei"] + e["mei"] + e["azana"] + e["bio"]) and e["sei"] and e["mei"])]
    stat = {"total": 850 + len(out), "koei": 850, "new": len(out), "real_candidates": n_real_all, "dropped_over_quota": n_dropped,
            "conf": dict(by_conf), "with_skill": sum(e["skill"] != 0xff for e in out),
            "with_father": sum(e["father"] >= 0 for e in out), "with_spouse": sum(e["spouse"] >= 0 for e in out),
            "with_liked": sum(bool(e["liked"]) for e in out), "with_disliked": sum(bool(e["disliked"]) for e in out),
            "female": sum(e["sex"] == 1 for e in out), "big5_or_name_problems": len(bad), "bad_ids": bad[:50],
            "by_region": dict(by_region.most_common()), "first_source": dict(by_src.most_common(30))}
    sj(PERSONS / "roster_stats.json", stat, indent=1)
    print({k: v for k, v in stat.items() if k not in ("by_region", "first_source", "bad_ids")})


if __name__ == "__main__":
    main()
