"""希腊、拉丁史书原文和《三国史记》→ 子代理用的块 data/persons/_cache/chunks/<前缀>_NN.txt

  hdn  希罗狄安《马可之后的罗马史》（希腊文，First1KGreek）              180–238 年
  ha   《罗马君王传》马可·奥勒留传起（拉丁文，Perseus）                  161–285 年
  eus  优西比乌《教会史》卷 5–10（英译，First1KGreek）                    170–324 年
  zos  佐西默斯《新史》卷 1–4（希腊文，First1KGreek）                     238–395 年
  amm  阿米阿努斯·马塞利努斯《历史》（英译，Perseus）                     353–378 年
  sk   三国史记（殆知阁本）新罗、高句丽、百济本纪 170–370 年段和列传

    python split_classic.py [每块字数，默认 30000]"""
import re
import sys
import urllib.request
from common import CACHE

CHUNK = int(sys.argv[1]) if len(sys.argv) > 1 else 30000
T = CACHE / "texts" / "classic"
OUT = CACHE / "chunks"
GH = "https://raw.githubusercontent.com/"
SRC = {
    "hdn": ["OpenGreekAndLatin/First1KGreek/master/data/tlg0015/tlg001/tlg0015.tlg001.1st1K-grc1.xml"],
    "ha": [f"PerseusDL/canonical-latinLit/master/data/phi2331/phi{i:03d}/phi2331.phi{i:03d}.perseus-lat2.xml"
           for i in range(4, 31)],
    "eus": ["OpenGreekAndLatin/First1KGreek/master/data/tlg2018/tlg002/tlg2018.tlg002.1st1K-eng1.xml"],
    "zos": ["OpenGreekAndLatin/First1KGreek/master/data/tlg4084/tlg001/tlg4084.tlg001.1st1K-grc1.xml"],
    "amm": ["PerseusDL/canonical-latinLit/master/data/stoa0023/stoa001/stoa0023.stoa001.perseus-eng2.xml"],
}
TITLE = {"hdn": "Herodian, History of the Empire after Marcus",
         "ha": "Historia Augusta", "eus": "Eusebius, Ecclesiastical History (tr.)",
         "zos": "Zosimus, New History", "amm": "Ammianus Marcellinus, Res Gestae (tr.)"}
DZG = "https://raw.githubusercontent.com/garychowcmu/daizhigev20/master/史藏/载记/三国史记.txt"


def get(url, path):
    if not path.exists():
        req = urllib.request.Request(urllib.parse.quote(url, safe=":/"), headers={"User-Agent": "san11world-roster/1.0"})
        path.write_bytes(urllib.request.urlopen(req, timeout=300).read())
    return path.read_text(encoding="utf-8")


def tei_text(xml):
    """TEI → 纯文本：书/章编号写成「[卷.章]」，注释、校勘去掉"""
    xml = xml[xml.find("<body"):] if "<body" in xml else xml
    xml = re.sub(r"<(note|bibl|app|rdg|head type=\"sub\")[^>]*>.*?</\1>", " ", xml, flags=re.S)

    def mark(m):
        n = re.search(r' n="([^"]+)"', m.group(0))
        sub = re.search(r'subtype="([^"]+)"', m.group(0))
        if not n or not sub or "urn:" in n.group(1):
            return "\n"
        if sub.group(1) == "book":
            return f"\n[Book {n.group(1)}]\n"
        return f"\n[{n.group(1)}] " if sub.group(1) == "chapter" else " "
    xml = re.sub(r"<(div|milestone|pb|lb|p|head)\b[^>]*>", lambda m: mark(m) if m.group(1) == "div" else "\n"
                 if m.group(1) in ("p", "head") else " ", xml)
    t = re.sub(r"<[^>]+>", "", xml)
    t = re.sub(r"&[a-z]+;", " ", t)
    t = re.sub(r"[ \t]+", " ", t)
    return re.sub(r"\n\s*\n+", "\n", t).strip()


def cut(prefix, title, text):
    for f in OUT.glob(f"{prefix}_*.txt"):
        f.unlink()
    chunks, cur, n = [], [], 0
    for p in text.split("\n"):
        cur.append(p)
        n += len(p) + 1
        if n >= CHUNK:
            chunks.append("\n".join(cur))
            cur, n = [], 0
    if cur:
        chunks.append("\n".join(cur))
    for i, c in enumerate(chunks):
        marks = re.findall(r"^\[((?:Book|Vita)[^\]]+)\]", c, flags=re.M) or re.findall(r"^\[([^\]]+)\]", c, flags=re.M)
        (OUT / f"{prefix}_{i:02d}.txt").write_text(
            f"# {title} [{marks[0] if marks else ''} .. {marks[-1] if marks else ''}]\n" + c, encoding="utf-8")
    print(prefix, len(text), "chars,", len(chunks), "chunks")


def from_book(text, first):
    """从第 first 卷的标记开始（标记形如「[Book 71]」）"""
    m = re.search(rf"^\[Book {first}\]", text, flags=re.M)
    if not m:
        raise SystemExit(f"找不到第 {first} 卷")
    return text[m.start():]


def main():
    T.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    for prefix, paths in SRC.items():
        parts = []
        for p in paths:
            xml = get(GH + p, T / p.split("/")[-1])
            head = re.search(r"<title(?:\s[^>]*)?>(.*?)</title>", xml, flags=re.S)       # 罗马君王传：每篇传记的标题
            parts.append((f"\n[Vita: {re.sub(r'\s+', ' ', head.group(1)).strip()}]\n" if prefix == "ha" and head else "")
                         + tei_text(xml))
        text = "\n".join(parts)
        if prefix == "eus":
            text = from_book(text, 5)
        if prefix == "zos":
            m = re.search(r"^\[Book 5\]", text, flags=re.M)
            text = text[:m.start()] if m else text
        cut(prefix, TITLE[prefix], text)
    sk = get(DZG, T / "三国史记.txt")
    cut("sk", "三国史记", sk_period(sk))


def sk_period(t):
    """三国史记：各本纪只取 170–370 年对应诸王的部分；列传取第五（乙巴素、明临答夫、昔于老……）、
    第八（勿稽子……）、第九（仓助利），其余列传是 5 世纪以后的人"""
    keep = []
    for start, end in [("阿达罗尼师今", "实圣尼师今"), ("新大王", "广开土王"), ("肖古王", "辰斯王")]:
        a = t.find(start)
        b = t.find(end, a + 1)
        if a < 0:
            raise SystemExit(f"三国史记 找不到 {start}")
        keep.append(t[a:b if b > a else a + 60000])
    for n, nxt in [("五", "六"), ("八", "九"), ("九", "一{十}")]:
        a, b = t.find(f"列传第{n}。"), t.find(f"列传第{nxt}。")
        keep.append(t[a:b])
    return "\n".join(keep)


if __name__ == "__main__":
    main()
