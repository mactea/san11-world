"""审计 roster.json：关系、属性、相性、特技、出处 → data/persons/audit.json（每类问题的数量和样例）

和 validate.py 的区别：validate 只查格式和取值范围（不过就不能导入）；这里查「合不合理」，
结果要人看、再决定改规则还是改数据。

    python audit.py"""
import collections
import math
import re
from attrs import skills
from build import polity_info
from common import PERSONS, SRC, load_json, save_json

CHINA_REG = {"幽州", "冀州", "青州", "并州", "徐州", "揚州", "兗州", "豫州", "司隸", "荊州", "涼州", "益州", "南中", "交州",
             "鮮卑", "西域", "東夷", "倭", "匈奴", "西羌", "夷洲", "南海"}
from build import VIOLENT_SELF as VIOLENT, CALM_DEATH as CALM          # 和建表用同一套判断


def sphere(e):
    return "中" if e["region"] in CHINA_REG else "外"


def main():
    roster = load_json(PERSONS / "roster.json")
    koei = {k["koei_id"]: k for k in load_json(SRC / "koei.json")}
    by = {e["id"]: e for e in roster}
    sk = skills()
    out = collections.OrderedDict()

    def add(cat, e, msg):
        d = out.setdefault(cat, {"n": 0, "samples": []})
        d["n"] += 1
        if len(d["samples"]) < 25:
            d["samples"].append(f"{e['id']} {e['sei']}{e['mei']}（{e.get('polity') or e['region']}）：{msg}")

    def life(i):
        if i in by:
            return by[i]["birth"], by[i]["death"], by[i]["sex"]
        k = koei.get(i)
        return (k["birth"], k["death"], k["sex"]) if k else None

    def name(i):
        if i in by:
            return by[i]["sei"] + by[i]["mei"]
        k = koei.get(i)
        return (k["sei"] + k["mei"] + "（光荣）") if k else f"#{i}"

    # ================= 关系
    for e in roster:
        b, d = e["birth"], e["death"]
        for f, sex in (("father", 0), ("mother", 1)):
            t = e[f]
            if t < 0:
                continue
            L = life(t)
            if not L:
                add("关系·父母编号不存在", e, f"{f}={t}")
                continue
            gap = b - L[0]
            if not 14 <= gap <= 60:
                add("关系·父母年龄差不合理", e, f"{f} {name(t)} 生于 {L[0]}，本人 {b}（差 {gap}）")
            if L[2] != sex:
                add("关系·父母性别不对", e, f"{f} {name(t)}")
            if L[1] < b - (1 if sex == 0 else 0):
                add("关系·父母在本人出生前已死", e, f"{f} {name(t)} 卒 {L[1]}，本人生 {b}")
            if t in by and sphere(by[t]) != sphere(e):
                add("关系·父母与本人不在同一文化圈（多半是同名误连）", e, f"{f} {name(t)}（{by[t]['region']}）")
        s = e["spouse"]
        if s >= 0:
            L = life(s)
            if not L:
                add("关系·配偶编号不存在", e, f"{s}")
            else:
                if L[2] == e["sex"]:
                    add("关系·配偶同性", e, name(s))
                if abs(L[0] - b) > 35:
                    add("关系·配偶年龄差超过 35", e, f"{name(s)} 生 {L[0]}，本人 {b}")
                if L[1] < b + 12 or L[0] > d - 12:
                    add("关系·配偶与本人在世时间几乎不重叠", e, f"{name(s)} {L[0]}–{L[1]}，本人 {b}–{d}")
                back = by[s]["spouse"] if s in by else koei[s]["spouse"]
                if back != e["id"] and s in by:
                    add("关系·配偶不对称（对方的配偶不是本人）", e, f"{name(s)} 的配偶是 {name(back) if back >= 0 else '无'}")
                if s in by and sphere(by[s]) != sphere(e):
                    add("关系·配偶与本人不在同一文化圈", e, name(s))
        for f in ("liked", "disliked"):
            seen = set()
            for t in e[f]:
                L = life(t)
                if t == e["id"]:
                    add(f"关系·{f}含本人", e, "")
                elif not L:
                    add(f"关系·{f}编号不存在", e, str(t))
                elif L[1] < b + 5 or L[0] > d - 5:
                    add(f"关系·{f}与本人在世时间不重叠", e, f"{name(t)} {L[0]}–{L[1]}，本人 {b}–{d}")
                elif t in by and sphere(by[t]) != sphere(e):
                    add(f"关系·{f}跨文化圈（多半是同名误连）", e, f"{name(t)}（{by[t]['region']}）")
                if t in seen:
                    add(f"关系·{f}重复", e, name(t))
                seen.add(t)
        both = set(e["liked"]) & set(e["disliked"])
        if both:
            add("关系·同一人既亲近又厌恶", e, "、".join(name(t) for t in both))
        if e["father"] >= 850 and e["father"] in by:
            fa = by[e["father"]]
            if e["generation"] != min(4, fa["generation"] + 1):
                add("关系·世代与父亲不衔接", e, f"本人 {e['generation']}，父 {fa['generation']}")
            if e["blood"] != fa["blood"]:
                add("关系·血缘与父亲不同", e, f"{e['blood']} vs {fa['blood']}")
            if e["sei"] != fa["sei"] and sphere(e) == "中" and not fa["sei"].endswith("氏"):
                add("关系·父子不同姓（中国人物）", e, f"父 {name(e['father'])}")
    # 关系覆盖率：来源里写了父亲/配偶、却没解析出编号
    merged = {k: m for m in __import__("common").load_jsonl(PERSONS / "build" / "merged.jsonl") for k in m["keys"][:1]}
    for e in roster:
        m = merged.get(e["keys"][0])
        if not m:
            continue
        kin = {k for k, _ in m.get("kin") or []}
        if kin & {"父", "父親"} and e["father"] < 0:
            add("关系·来源写了父亲但没对上编号", e, "、".join(n for k, n in m["kin"] if k in ("父", "父親"))[:40])
        if kin & {"妻", "夫", "配偶", "丈夫", "妻子"} and e["spouse"] < 0:
            add("关系·来源写了配偶但没对上编号", e, "、".join(n for k, n in m["kin"] if k in ("妻", "夫", "配偶", "丈夫", "妻子"))[:40])

    # ================= 属性
    koei_main = [k for k in koei.values() if k["koei_id"] < 670 and k["sei"]]
    for e in roster:
        st = e["stats"]
        ld, wu, it, po, ch = st
        if max(st) >= 90 and e["conf"] == "C":
            add("属性·可信度 C（资料单薄）却有 90 以上的能力", e, f"{st}")
        if max(st) >= 95 and e["conf"] != "A":
            add("属性·95 以上只该给公认一流人物", e, f"{st} conf {e['conf']}")
        if e["role"] in ("后妃", "宗教", "僧道", "学者", "名士", "宦官") and max(ld, wu) >= 70:
            add("属性·不领兵的身份却有高统率/武力", e, f"{e['role']} {st}")
        if e["role"] == "武将" and max(ld, wu) < 45:
            add("属性·武将统率武力都很低", e, f"{st}")
        if len(set(st)) == 1:
            add("属性·五维全部相同（像默认值）", e, f"{st}")
        if e["death"] - e["birth"] > 95 or e["death"] - e["birth"] < 16:
            add("属性·寿命不合理", e, f"{e['birth']}–{e['death']}")
        if not (e["birth"] + 12 <= e["appear"] <= e["death"]):
            add("属性·登场年不在 生年+12 … 卒年", e, f"{e['birth']}/{e['appear']}/{e['death']}")
        bio = e["bio"]
        if VIOLENT.search(bio) and e["death_type"] == 0 and not CALM.search(bio):
            add("属性·列传写了横死，死法却是天寿", e, VIOLENT.search(bio).group(0))
        if e["death_type"] == 1 and CALM.search(bio) and not VIOLENT.search(bio):
            add("属性·列传写了病卒，死法却是横死", e, CALM.search(bio).group(0))
        if sphere(e) == "外" and e["kanshitsu"] != 0:
            add("属性·外国人物汉室重视度不是 0", e, f"{e['kanshitsu']}")
        if 3 in e["apt"] and ld < 85:
            add("属性·统率不到 85 却有 S 适性", e, f"统率 {ld} 适性 {e['apt']}")
        if e["apt"][5] >= 2 and e["region"] in ("匈奴", "鮮卑", "康居", "薩爾馬提亞", "西羌", "西域", "撒哈拉"):
            add("属性·草原、沙漠地区人物水军适性 A 以上", e, f"{e['apt']}")
        if e["role"] == "君主" and e["yabou"] <= 1:
            add("属性·君主野望很低", e, f"野望 {e['yabou']}")
    def mean(xs):
        return sum(xs) / max(1, len(xs))
    out["属性·五维平均（扩展 vs 光荣 0..669）"] = {
        "扩展": [round(mean([e["stats"][i] for e in roster]), 1) for i in range(5)],
        "光荣": [round(mean([k["stats"][i] for k in koei_main]), 1) for i in range(5)],
        "扩展 90 以上人数": [sum(e["stats"][i] >= 90 for e in roster) for i in range(5)],
        "光荣 90 以上人数": [sum(k["stats"][i] >= 90 for k in koei_main) for i in range(5)]}

    # ================= 相性
    def circ(xs):
        a = [x / 150 * 2 * math.pi for x in xs]
        c, s = mean([math.cos(x) for x in a]), mean([math.sin(x) for x in a])
        return (math.atan2(s, c) % (2 * math.pi)) / (2 * math.pi) * 150, math.hypot(c, s)
    pol = collections.defaultdict(list)
    for e in roster:
        pol[e["polity"] or "（无）"].append(e["aishou"])
    rows = []
    for p, xs in sorted(pol.items(), key=lambda x: -len(x[1])):
        if len(xs) < 15:
            continue
        mu, R = circ(xs)
        base = polity_info(p)[1]
        rows.append(f"{p}：{len(xs)} 人，中心 {mu:.0f}（规则基准 {base}），集中度 {R:.2f}")
        if R < 0.6 and p != "（无）":
            out.setdefault("相性·同一政权的人相性分散（集中度 < 0.6）", {"n": 0, "samples": []})
            out["相性·同一政权的人相性分散（集中度 < 0.6）"]["n"] += 1
            out["相性·同一政权的人相性分散（集中度 < 0.6）"]["samples"].append(rows[-1])
    out["相性·各政权分布（≥15 人）"] = rows[:60]
    for e in roster:
        base = polity_info(e["polity"])[1]
        if base is not None:
            dist = min(abs(e["aishou"] - base), 150 - abs(e["aishou"] - base))
            if dist > 20:
                add("相性·离所属政权基准超过 20", e, f"{e['aishou']}（{e['polity']} 基准 {base}）")
    for e in roster:
        for t in e["disliked"]:
            if t in by:
                dist = min(abs(e["aishou"] - by[t]["aishou"]), 150 - abs(e["aishou"] - by[t]["aishou"]))
                if dist <= 3:
                    add("相性·与厌恶的人相性几乎相同", e, f"{name(t)} {e['aishou']}/{by[t]['aishou']}")

    # ================= 特技
    koei_cnt = collections.Counter(k["skill"] for k in koei_main if k["skill"] != 0xff)
    ext_cnt = collections.Counter(e["skill"] for e in roster if e["skill"] != 0xff)
    out["特技·持有率"] = {"扩展": f"{sum(ext_cnt.values())}/{len(roster)} = {sum(ext_cnt.values()) / len(roster):.0%}",
                     "光荣": f"{sum(koei_cnt.values())}/{len(koei_main)} = {sum(koei_cnt.values()) / len(koei_main):.0%}"}
    out["特技·最多的 15 种（扩展 / 光荣）"] = [f"{sk[s]['name']}({s}) {n} / {koei_cnt.get(s, 0)}" for s, n in ext_cnt.most_common(15)]
    ratio = len(roster) / len(koei_main)
    out["特技·光荣里 ≤2 人有、扩展里却很多的稀有特技"] = [
        f"{sk[s]['name']}({s}) 扩展 {n}、光荣 {koei_cnt.get(s, 0)}" for s, n in ext_cnt.items()
        if koei_cnt.get(s, 0) <= 2 and n > max(3, koei_cnt.get(s, 0) * ratio * 2)]
    WAR = {"戰鬥", "战斗", "行军", "攻擊", "攻击"}
    for e in roster:
        s = e["skill"]
        if s == 0xff:
            continue
        info = sk.get(s, {})
        cat = info.get("category", "")
        ld, wu, it, po, ch = e["stats"]
        if cat in WAR and max(ld, wu) < 55:
            add("特技·统率武力都低却有战斗/行军特技", e, f"{info.get('name')} {e['stats']}")
        if cat in ("内政", "內政") and po < 50:
            add("特技·政治低却有内政特技", e, f"{info.get('name')} {e['stats']}")
        if cat in ("计略", "計略") and it < 55:
            add("特技·智力低却有计略特技", e, f"{info.get('name')} {e['stats']}")
        compat = info.get("bing_zhong_compat") or {}
        lean = {"槍": "枪", "騎": "骑"}.get(e["lean"], e["lean"])
        if lean in compat and not compat[lean] and cat in WAR:
            add("特技·和兵种倾向不合", e, f"{info.get('name')}，倾向 {e['lean']}")
    out["特技·各类别数量"] = dict(collections.Counter(sk[e["skill"]]["category"] for e in roster if e["skill"] in sk))

    # ================= 出处
    for e in roster:
        if not e["src"]:
            add("出处·没有出处", e, "")
        if not e["bio"] or e["bio"] in ("事蹟不詳。",) or len(e["bio"]) < 15:
            add("出处·列传过短或空", e, e["bio"])
        if re.search(r"[A-Za-z]{4,}", e["bio"]):
            add("出处·列传里有英文", e, re.search(r"[A-Za-z][A-Za-z ]{3,}", e["bio"]).group(0)[:30])
        if re.search(r"原文|本段|本块|未见于|子代理|known|skip|不确定|拿不准", e["bio"]):
            add("出处·列传里混入了整理说明", e, e["bio"][:50])
        if e["conf"] == "A" and all(s.startswith(("三國群雄志", "CBDB")) for s in e["src"]):
            add("出处·可信度 A 但只有群雄志/CBDB", e, "；".join(e["src"]))
        if sphere(e) == "外" and not any(re.search(r"Q\d+|Wikipedia|Herodian|Augusta|Eusebius|Zosimus|Ammianus|三國史記|三国史记", s)
                                        for s in e["src"]):
            add("出处·海外人物出处不可追溯", e, "；".join(e["src"])[:60])
        years = [int(y) for y in re.findall(r"(?<!\d)(\d{3})年", e["bio"])]
        if years and (max(years) > e["death"] + 2 or min(years) < e["birth"] - 1):
            add("出处·列传里的年份超出生卒", e, f"列传 {min(years)}–{max(years)}，生卒 {e['birth']}–{e['death']}")
    srcs = collections.Counter(s.split("·")[0].split(" ")[0].split("（")[0] for e in roster for s in e["src"][:1])
    out["出处·首要出处分布"] = dict(srcs.most_common(25))
    save_json(PERSONS / "audit.json", out, indent=1)
    for k, v in out.items():
        if isinstance(v, dict) and "n" in v:
            print(f"{v['n']:6d}  {k}")
        else:
            print(f"   ---  {k}: {v if not isinstance(v, list) else len(v)}")


if __name__ == "__main__":
    main()
