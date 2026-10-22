"""把 src/wiki.json 里还没收的人分成子代理用的批次 → data/persons/_cache/batches/wk_NN.txt

跳过已在 Laouenan 名人库（按 Wikidata 编号）或已收海外人物表（按英文名）里的人。

    python batch_wiki.py [每批人数，默认 60]"""
import re
import sys
from cands import en_key, known_zh
from common import CACHE, SRC, load_json

N = int(sys.argv[1]) if len(sys.argv) > 1 else 60
known = known_zh()


def modern(e):
    """6 世纪以后的人：分类有「1939 births」「Living people」「15th-century …」；
    只靠信息框年份入选的，信息框里出现四位数年份也算（三位数常是页码、档案号）"""
    for c in e["cats"]:
        if c == "Living people" or re.match(r"\d{4}s? (births|deaths)", c):
            return True
        if any(int(m) >= 6 for m in re.findall(r"(\d+)(?:st|nd|rd|th)-century", c)) and "BC" not in c:
            return True
    if e.get("act") and not (e["b"] or e["d"]):
        return any(500 <= int(y) <= 2100 for v in e["info"].values() for y in re.findall(r"(?<!\d)(\d{4})(?!\d)", v))
    return False


rows = [e for e in load_json(SRC / "wiki.json") if not e["in_laouenan"] and en_key(e["title"]) not in known and not modern(e)]
out = CACHE / "batches"
out.mkdir(parents=True, exist_ok=True)
for f in out.glob("wk_*.txt"):
    f.unlink()


def rng(r):
    return "" if not r else str(r[0]) if r[0] == r[1] else f"{r[0]}-{r[1]}"


for b in range(0, len(rows), N):
    lines = []
    for i, e in enumerate(rows[b:b + N], start=b):
        langs = ",".join(f"{k}:{v}" for k, v in e["langs"].items())
        info = "; ".join(f"{k}={v}" for k, v in e["info"].items())
        cats = "; ".join(c for c in e["cats"] if not re.search(r"births|deaths", c))[:300]
        lines.append(" | ".join([str(i), e["title"], e["qid"] or "-", rng(e["b"]), rng(e["d"]), langs or "-",
                                 "LEGEND" if e["legend"] else "-", cats, info, e["lead"][:700]]))
    (out / f"wk_{b // N:03d}.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
print("persons", len(rows), "batches", (len(rows) + N - 1) // N)
