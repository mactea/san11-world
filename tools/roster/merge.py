"""合并去重：build/cands_*.jsonl → build/merged.jsonl

* 中国人物按「简体化姓名」分组，组内按活动年份相容（相差不超过 TOL 年）聚成同一人；
  没有年份的记录并入同名的第一个人。
* 与光荣 0..849 号同名且年份相容的人算「三国志11 已包含」，不进扩展名单（关系仍可指向光荣武将）。
* 字段按来源优先级合并：五维 中华三国志 > 史书抽取 > 群雄志 > CBDB；列传 中华三国志正史 > 史书 > 其他。"""
import glob
import re
from pathlib import Path
from common import SRC, load_json, load_jsonl, save_jsonl, save_json
from cands import BUILD

TOL = 35
PRI = {"zhsan": 0, "tj": 1, "js": 1, "hhs": 1, "sl": 1, "hy": 1, "sg": 1, "ss": 1, "sk": 1, "wsh": 1, "yi": 1, "jk": 1, "sz": 1, "yl": 2, "gl": 2, "wk": 2,
       "rg": 2, "wl": 2, "dio": 2, "hdn": 2, "ha": 2, "eus": 2, "zos": 2, "amm": 2,
       "fau": 2, "moi": 2, "mhv": 2, "niya": 2, "cth": 2, "je": 2,
       "soc": 2, "soz": 2, "thd": 2, "phs": 2, "vc": 2,
       "c354": 2, "eun": 2, "jor": 2, "lat": 2, "lib": 2, "syr": 2, "nld": 2, "qxz": 3, "cbdb": 4}
_t2s = None


def t2s(s):
    global _t2s
    if _t2s is None:
        import opencc
        _t2s = opencc.OpenCC("t2s")
    return _t2s.convert(s)


# 史书底本和各来源用字不一的异体字：比对前统一
# （王濬/王浚、张寔/张实这类是同时代的不同人，不能并）
VARIANT = str.maketrans({"儁": "俊", "雋": "俊", "隽": "俊", "伷": "胄", "硃": "朱", "瓚": "瓒", "璝": "瑰", "淩": "凌",
                         "諮": "咨", "谘": "咨", "卻": "郤", "却": "郤"})


def nkey(name):
    return re.sub(r"[·・\s]", "", t2s(name)).translate(VARIANT)


def span(c):
    ys = [y for y in [c.get("birth"), c.get("death"), *(c.get("years") or [])] if isinstance(y, (int, float))]
    return (min(ys), max(ys)) if ys else None


def compatible(a, b):
    if a is None or b is None:
        return True
    return a[0] - TOL <= b[1] and b[0] - TOL <= a[1]


def chunk_of(c):
    """史书抽取的记录所在的块（同一块里同名的记录是子代理有意分开的不同人）"""
    parts = c["key"].split(":")
    return ":".join(parts[:2]) if len(parts) >= 4 else None


def same_person(cl, c):
    """c 能否并入聚类 cl（cl = [span, recs]）"""
    if not compatible(cl[0], span(c)):
        return False
    ch = chunk_of(c)
    for r in cl[1]:
        if ch and chunk_of(r) == ch:
            return False
        d1, d2 = r.get("death"), c.get("death")
        if d1 and d2 and abs(d1 - d2) > 8:                # 卒年都知道却对不上：同名异人（吴桓彝 / 东晋桓彝）
            return False
        f1, f2 = (r.get("years") or [None])[0], (c.get("years") or [None])[0]
        if (d1 and f2 and f2 > d1 + 1) or (d2 and f1 and f1 > d2 + 1):
            return False
        o1, o2 = (r.get("origin") or "")[:2], (c.get("origin") or "")[:2]
        p1, p2 = r.get("polity") or "", c.get("polity") or ""
        if o1 and o2 and o1 != o2 and p1 and p2 and p1 != p2 and p2 not in (r.get("polity_path") or []) \
                and p1 not in (c.get("polity_path") or []):
            return False
    return True


# 光荣按称号命名的人物
KOEI_ALIAS = {"劉宏": 700, "劉辯": 701, "劉協": 702}


def koei_index():
    idx = {}
    for k in load_json(SRC / "koei.json"):
        if 670 <= k["koei_id"] < 700 or not k["sei"]:
            continue
        s = None if k["koei_id"] >= 800 else (k["birth"], k["death"])
        idx.setdefault(nkey(k["sei"] + k["mei"]), []).append((k["koei_id"], s))
        for alias, kid in KOEI_ALIAS.items():
            if kid == k["koei_id"]:
                idx.setdefault(nkey(alias), []).append((kid, s))
    return idx


def src_of(c):
    return c["key"].split(":")[0]


def merge_cluster(recs):
    recs = sorted(recs, key=lambda c: PRI.get(src_of(c), 9))
    m = {"keys": [c["key"] for c in recs], "srcs": sorted({src_of(c) for c in recs})}
    for f in ["name", "sei", "mei", "azana", "sex", "birth", "death", "appear", "origin", "ethnic", "polity", "role",
              "lean", "lat", "lon", "place", "name_full", "name_orig", "qid", "seikaku", "giri", "yabou", "aishou",
              "generation", "region_un", "zh_id", "group"]:
        for c in recs:
            v = c.get(f)
            if v not in (None, "", "无", "無", "其他") or (f in ("sex",) and v is not None):
                m[f] = v
                break
    m.setdefault("role", "其他")
    m["stats"] = next((c["stats"] for c in recs if c.get("stats") and len(c["stats"]) == 5), None)
    bios = [c.get("bio") for c in recs if c.get("bio")]
    m["bio"] = max(bios[:2], key=len) if bios else ""
    m["death_violent"] = any(c.get("death_violent") for c in recs)
    yrs = [y for c in recs for y in (c.get("years") or []) if isinstance(y, (int, float))]
    m["years"] = [min(yrs), max(yrs)] if yrs else [None, None]
    m["offices"] = list(dict.fromkeys(o for c in recs for o in c.get("offices") or []))[:12]
    m["kin"] = [list(x) for x in dict.fromkeys(tuple(x) for c in recs for x in c.get("kin") or [])]
    m["friends"] = list(dict.fromkeys(x for c in recs for x in c.get("friends") or []))
    m["foes"] = list(dict.fromkeys(x for c in recs for x in c.get("foes") or []))
    m["alias"] = list(dict.fromkeys(x for c in recs for x in c.get("alias") or []))
    m["src"] = list(dict.fromkeys(c["src"] for c in recs if c.get("src")))
    confs = [c.get("conf", "C") for c in recs]
    m["conf"] = min(confs)                                   # A < B < C < D
    m["romance"] = all(c.get("romance") for c in recs)
    m["fame"] = max(c.get("fame", 0) for c in recs) + 8 * (len({src_of(c) for c in recs}) - 1)
    m["world"] = any(c.get("world") for c in recs)
    return m


def main():
    cands = []
    for f in sorted(glob.glob(str(BUILD / "cands_*.jsonl"))):
        cands += load_jsonl(f)
    koei = koei_index()
    groups, pre_koei = {}, []
    for c in cands:
        k = nkey(c["name"])
        if span(c) is None and k in koei:                   # 没年份又和光荣武将同名：就是光荣那位（群雄志名单多是这种）
            m = merge_cluster([c])
            m["koei_id"] = koei[k][0][0]
            pre_koei.append(m)
            continue
        groups.setdefault(k, []).append(c)
    allcl = []                                              # [nkey, span, recs]
    for k, recs in groups.items():
        clusters = []                                       # [(span, [recs])]
        for c in sorted(recs, key=lambda c: span(c) is None):
            s = span(c)
            for cl in clusters:
                if same_person(cl, c):
                    if s:
                        cl[0] = (min(cl[0][0], s[0]), max(cl[0][1], s[1])) if cl[0] else s
                    cl[1].append(c)
                    break
            else:
                clusters.append([s, [c]])
        allcl += [[k, s, rs] for s, rs in clusters]
    # ---- 别名合并：「刘元海」的别名有「刘渊」、另一组就叫「刘渊」，年份相容 → 同一人
    parent = list(range(len(allcl)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    by_k = {}
    for i, (k, s, rs) in enumerate(allcl):
        by_k.setdefault(k, []).append(i)
    alias_koei = {}
    for i, (k, s, rs) in enumerate(allcl):
        for a in {a for r in rs for a in r.get("alias") or [] if 2 <= len(a) <= 4 and all("一" <= ch <= "鿿" for ch in a)}:
            ak = nkey(a)
            if ak == k:
                continue
            hit = [kid for kid, ks in koei.get(ak, []) if compatible(ks, s)]
            if hit:
                alias_koei[i] = hit[0]
            for j in by_k.get(ak, []):
                if j != i and compatible(allcl[j][1], s) and not any(chunk_of(r1) and chunk_of(r1) == chunk_of(r2)
                                                                     for r1 in rs for r2 in allcl[j][2]):
                    parent[find(i)] = find(j)
    groups2 = {}
    for i in range(len(allcl)):
        groups2.setdefault(find(i), []).append(i)
    merged, in_koei = [], pre_koei
    for root, members in groups2.items():
        rs = [r for i in members for r in allcl[i][2]]
        spans = [allcl[i][1] for i in members if allcl[i][1]]
        s = (min(x[0] for x in spans), max(x[1] for x in spans)) if spans else None
        m = merge_cluster(rs)
        hit = [kid for i in members for kid, ks in koei.get(allcl[i][0], []) if compatible(ks, s)] + \
              [alias_koei[i] for i in members if i in alias_koei]
        if hit:
            m["koei_id"] = hit[0]
            in_koei.append(m)
        else:
            merged.append(m)
    save_jsonl(BUILD / "merged.jsonl", merged)
    save_jsonl(BUILD / "in_koei.jsonl", in_koei)
    by = {}
    for m in merged:
        for s in m["srcs"]:
            by[s] = by.get(s, 0) + 1
    stat = {"candidates": len(cands), "merged_new": len(merged), "matched_koei": len(in_koei), "by_source": by,
            "conf": {c: sum(m["conf"] == c for m in merged) for c in "ABCD"}}
    save_json(BUILD / "merge_stats.json", stat, indent=1)
    print(stat)


if __name__ == "__main__":
    main()
