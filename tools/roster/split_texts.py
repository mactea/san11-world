"""把资治通鉴 168..370 年（建宁元年..太和五年）按年份标题切成约 CHUNK 字的块，供子代理逐块抽取人物。
输出 data/persons/_cache/chunks/tj_NN.txt，每块第一行写起止年份标题。"""
import re
import sys
from common import CACHE

CHUNK = int(sys.argv[1]) if len(sys.argv) > 1 else 40000
t = (CACHE / "texts" / "资治通鉴.txt").read_text(encoding="utf-8")
start = t.rindex("\n", 0, t.index("孝桓皇帝下建宁元年"))
end = t.index("太宗简文皇帝咸安元年")
seg = t[start:end]
# 年份标题行：「　　孝灵皇帝中光和五年」或「　　烈祖明皇帝中之上太和五年（辛亥，公元二三一年）」
YEAR = r"[　 ]*[^\n，。：“]{2,20}?[元一二三四五六七八九十]+年(?:（[^\n]*）)?"
heads = [0] + [m.start() for m in re.finditer(r"\n" + YEAR + r"\n", seg)]
out_dir = CACHE / "chunks"
out_dir.mkdir(exist_ok=True)
for f in out_dir.glob("tj_*.txt"):
    f.unlink()
cuts, last = [0], 0
for h in heads:
    if h - last >= CHUNK:
        cuts.append(h)
        last = h
cuts.append(len(seg))
for n in range(len(cuts) - 1):
    part = seg[cuts[n]:cuts[n + 1]].strip("\n")
    years = [l.strip() for l in part.split("\n") if re.fullmatch(YEAR, l)]
    first, lastyr = (years[0], years[-1]) if years else ("", "")
    (out_dir / f"tj_{n:02d}.txt").write_text(f"# 资治通鉴 {first} .. {lastyr}\n" + part, encoding="utf-8")
    print(n, len(part), first[:30], "..", lastyr[:30])
