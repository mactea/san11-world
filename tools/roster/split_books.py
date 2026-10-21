"""其他史书切块（按段落边界，每块约 CHUNK 字）→ data/persons/_cache/chunks/<前缀>_NN.txt

  js   晋书 列传（卷三十一..卷一百，西晋、东晋人物；载记由十六国春秋覆盖）
  hhs  后汉书 卷五十四..卷九十（东汉末人物、四夷传）
  sl   十六国春秋（全书）
  hy   华阳国志 卷五..卷十二（刘焉父子、蜀汉、西晋、成汉、蜀地士人）
"""
import re
import sys
from common import CACHE

CHUNK = int(sys.argv[1]) if len(sys.argv) > 1 else 30000
T = CACHE / "texts"
OUT = CACHE / "chunks"


def body_index(t, marker):
    """目录里也有卷名：取最后一次出现（正文）"""
    hits = [m.start() for m in re.finditer(re.escape(marker), t)]
    if not hits:
        raise SystemExit(f"找不到 {marker}")
    return hits[-1]


def cut(prefix, title, text):
    for f in OUT.glob(f"{prefix}_*.txt"):
        f.unlink()
    paras = text.split("\n")
    chunks, cur = [], []
    n = 0
    for p in paras:
        cur.append(p)
        n += len(p) + 1
        if n >= CHUNK:
            chunks.append("\n".join(cur))
            cur, n = [], 0
    if cur:
        chunks.append("\n".join(cur))
    for i, c in enumerate(chunks):
        heads = re.findall(r"卷[一二三四五六七八九十百]+[^\n]{0,24}", c)
        (OUT / f"{prefix}_{i:02d}.txt").write_text(
            f"# {title} {heads[0] if heads else ''} .. {heads[-1] if heads else ''}\n" + c, encoding="utf-8")
    print(prefix, len(text), "chars,", len(chunks), "chunks")


js = (T / "晋书.txt").read_text(encoding="utf-8")
a, b = body_index(js, "\n列传第一 "), body_index(js, "\n载记第一 ")
cut("js", "晋书", js[a:b])
hhs = (T / "后汉书.txt").read_text(encoding="utf-8")
a, b = body_index(hhs, "卷五十四 "), body_index(hhs, "卷九十 ")
b = hhs.find("\n卷", b + 10) if hhs.find("\n卷", b + 10) > 0 else len(hhs)
cut("hhs", "后汉书", hhs[a:b])
sl = (T / "十六国春秋.txt").read_text(encoding="utf-8")
cut("sl", "十六国春秋", sl)
sg = (T / "三国志.txt").read_text(encoding="utf-8")                      # 四库本，无标点，含裴注
cut("sg", "三国志", sg)
ss = (T / "世说新语.txt").read_text(encoding="utf-8")
cut("ssxy", "世说新语", ss)
gs = (T / "高僧传.txt").read_text(encoding="utf-8")
cut("gs", "高僧传", gs[:gs.find("卷第七") if gs.find("卷第七") > 0 else len(gs)])   # 前六卷：译经、义解，多为晋以前
hj = (T / "后汉纪.txt").read_text(encoding="utf-8")
a = hj.find("孝灵皇帝纪")
cut("hj", "后汉纪", hj[a if a > 0 else 0:])
hy = (T / "华阳国志.txt").read_text(encoding="utf-8")
a = hy.find("●卷五")
b = hy.find("●卷十二")
cut("hy", "华阳国志", hy[a if a > 0 else 0:b if b > 0 else len(hy)])
