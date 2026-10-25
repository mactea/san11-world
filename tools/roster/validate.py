"""校验 data/persons/roster.json → data/persons/validate.json（有问题的条目列出来，退出码非 0）

检查：编号连续、姓名字各 1~2 字（字可空）且 Big5、列传 Big5 且不超过 150 字、生 < 登场 ≤ 卒、年份与 170..370 有交集、
五维 1..100、适性 0..3、特技 0..99 或 255、性格类取值范围、相性 0..149、关系编号有效（0..849 光荣或名单内）、
父母比子女大 14 岁以上、配偶异性（一夫多妻时只有妻子一方指向丈夫，和光荣原版一样）、出身城在 530 城里。"""
import collections
import sys
from common import PERSONS, SRC, big5_ok, load_json, save_json

R = load_json(PERSONS / "roster.json")
koei = {k["koei_id"]: k for k in load_json(SRC / "koei.json")}
cities = {c["full"] for c in load_json(SRC / "cities.json")}
byid = {e["id"]: e for e in R}
issues = collections.defaultdict(list)


def bad(kind, e, msg=""):
    issues[kind].append(f"{e['id']} {e['sei']}{e['mei']} {msg}".strip())


ids = [e["id"] for e in R]
if ids != list(range(850, 850 + len(R))):
    issues["编号不连续"].append(f"{ids[:3]}..{ids[-3:]}")
RANGE = {"giri": 4, "yabou": 4, "kiyou": 4, "seikaku": 3, "voice": 7, "tone": 15, "kanshitsu": 2, "senryaku": 3}
for e in R:
    if not (1 <= len(e["sei"]) <= 2 and 1 <= len(e["mei"]) <= 2 and len(e["azana"]) <= 2):
        bad("姓名长度", e)
    if not big5_ok(e["sei"] + e["mei"] + e["azana"]):
        bad("姓名非 Big5", e)
    if not big5_ok(e["bio"]) or len(e["bio"]) > 150 or not e["bio"]:
        bad("列传", e, f"{len(e['bio'])} 字")
    if not (e["birth"] < e["appear"] <= e["death"]):
        bad("年份顺序", e, f"{e['birth']}/{e['appear']}/{e['death']}")
    if e["death"] < 170 or e["birth"] > 370:
        bad("年代", e, f"{e['birth']}-{e['death']}")
    if not all(1 <= v <= 100 for v in e["stats"]) or len(e["stats"]) != 5:
        bad("五维", e, str(e["stats"]))
    if len(e["apt"]) != 6 or not all(0 <= v <= 3 for v in e["apt"]):
        bad("适性", e, str(e["apt"]))
    if not (0 <= e["skill"] <= 99 or e["skill"] == 255):
        bad("特技", e, str(e["skill"]))
    for f, hi in RANGE.items():
        if not 0 <= e[f] <= hi:
            bad("性格类", e, f"{f}={e[f]}")
    if not 0 <= e["aishou"] <= 149:
        bad("相性", e, str(e["aishou"]))
    if e["city"] not in cities:
        bad("出身城", e, str(e["city"]))
    for f in ("father", "mother", "spouse", "sworn", "blood"):
        t = e[f]
        if t == -1:
            continue
        if not (t in koei or t in byid):
            bad("关系编号", e, f"{f}={t}")
            continue
        if f in ("father", "mother"):
            tb = koei[t]["birth"] if t in koei else byid[t]["birth"]
            if e["birth"] - tb < 14:
                bad("父母年龄", e, f"{f}={t} 生于 {tb}，本人 {e['birth']}")
        if f == "spouse":
            ts = koei[t]["sex"] if t in koei else byid[t]["sex"]
            if ts == e["sex"]:
                bad("配偶同性", e, f"spouse={t}")
    for f in ("liked", "disliked"):
        if len(e[f]) > 5 or any(not (t in koei or t in byid) for t in e[f]):
            bad("亲近厌恶", e, f"{f}={e[f]}")
out = {"total": len(R), "issue_counts": {k: len(v) for k, v in issues.items()},
       "issues": {k: v[:100] for k, v in issues.items()}}
save_json(PERSONS / "validate.json", out, indent=1)
print(out["issue_counts"] or "全部通过")
sys.exit(1 if issues else 0)
