"""把 src/laouenan.json 里还没整理过的人分成子代理用的输入批次 → data/persons/_cache/batches/gl_NN.txt

已经有 raw/gl_*.jsonl 结果的人（按 Wikidata 编号）跳过；新批次的编号接着已有的往下排。"""
import sys
from common import CACHE, RAW, SRC, load_json, load_jsonl

N = int(sys.argv[1]) if len(sys.argv) > 1 else 120
done = {e.get("qid") for f in RAW.glob("gl_*.jsonl") for e in load_jsonl(f)}
rows = [e for e in load_json(SRC / "laouenan.json") if e["wikidata_code"] not in done]
start = max([int(f.stem.split("_")[1]) + 1 for f in RAW.glob("gl_*.jsonl")] or [0])
out = CACHE / "batches"
out.mkdir(parents=True, exist_ok=True)
for f in out.glob("gl_*.txt"):
    f.unlink()


def yr(x):
    return "" if x is None else str(round(x))


for b in range(0, len(rows), N):
    lines = []
    for i, e in enumerate(rows[b:b + N], start=b):
        occ = e["level3_all_occ"].replace("D:_", "").split("_P:_")[0].replace("_", " ")[:80]
        lines.append(" | ".join([str(i), e["wikidata_code"], e["name"].replace("_", " "), yr(e["b"]), yr(e["d"]),
                                 e["un_subregion"] or "-", e["citizenship_1_b"].replace("_", " "), occ,
                                 e["number_wiki_editions"]]))
    (out / f"gl_{start + b // N:02d}.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
print("persons", len(rows), "batches", (len(rows) + N - 1) // N, "from", start)
