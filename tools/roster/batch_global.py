"""把 src/laouenan.json 分成子代理用的输入批次 → data/persons/_cache/batches/gl_NN.txt"""
import sys
from common import CACHE, SRC, load_json

N = int(sys.argv[1]) if len(sys.argv) > 1 else 120
rows = load_json(SRC / "laouenan.json")
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
    (out / f"gl_{b // N:02d}.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
print("batches", (len(rows) + N - 1) // N)
