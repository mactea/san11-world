"""生成名单查看网页的数据 → data/persons/viewer/roster_data.js（index.html 同目录，双击打开即可，不需要服务器）

数据里有：扩展武将的全部字段（光荣记录里语义未明的字节不放）、光荣 0..849 号武将的姓名和生卒（显示关系用）、
各来源的原始抽取记录（build/cands_*.jsonl，按 keys 对应）、审计问题（build/audit_by_id.json，先跑 audit.py）。

    python audit.py; python viewer.py"""
import glob
import json
from cands import BUILD
from common import PERSONS, SRC, load_json, load_jsonl

OUT = PERSONS / "viewer"
SRC_FIELDS = ("name", "azana", "alias", "birth", "death", "years", "origin", "polity", "role", "offices", "kin",
              "friends", "foes", "bio", "stats", "src", "conf", "name_orig", "place")


def trim(v, n=400):
    return v[:n] if isinstance(v, str) else v


def main():
    roster = load_json(PERSONS / "roster.json")
    want = {k for e in roster for k in e["keys"]}
    cands = {}
    for f in glob.glob(str(BUILD / "cands_*.jsonl")):
        for c in load_jsonl(f):
            if c["key"] in want:
                cands[c["key"]] = {k: trim(c[k]) for k in SRC_FIELDS if c.get(k) not in (None, "", [], [None, None])}
    audit = load_json(BUILD / "audit_by_id.json") if (BUILD / "audit_by_id.json").exists() else {}
    people = []
    for e in roster:
        d = {k: v for k, v in e.items() if k not in ("raw", "growth", "face")}
        d["srcrec"] = [cands[k] | {"key": k} for k in e["keys"] if k in cands]
        d["issues"] = audit.get(str(e["id"]), [])
        people.append(d)
    koei = {k["koei_id"]: [k["sei"] + k["mei"], k["birth"], k["death"], k["sex"]]
            for k in load_json(SRC / "koei.json") if k["sei"]}
    stats = load_json(PERSONS / "roster_stats.json")
    OUT.mkdir(parents=True, exist_ok=True)
    with open(OUT / "roster_data.js", "w", encoding="utf-8") as f:
        f.write("window.ROSTER=")
        json.dump({"people": people, "koei": koei, "stats": {k: stats[k] for k in ("total", "koei", "new", "conf")}},
                  f, ensure_ascii=False, separators=(",", ":"))
        f.write(";\n")
    print("viewer data", len(people), "people,", (OUT / "roster_data.js").stat().st_size // 2 ** 20, "MB")


if __name__ == "__main__":
    main()
