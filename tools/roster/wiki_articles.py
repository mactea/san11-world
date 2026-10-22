"""从英文维基百科转储取整篇条目（列表、王朝、铭文、殉道者名单这类综述条目），切成子代理用的块
→ data/persons/_cache/chunks/wl_NN.txt。这些条目里有很多没有单独条目、只在表里出现的人（各王朝诸王、廷臣、主教、拉比）

    python wiki_articles.py            # 用下面 ARTICLES 里的条目
    python wiki_articles.py --find 关键词   # 在索引里找条目名"""
import bz2
import html
import re
import sys
from common import CACHE
from scan_wiki import DUMP, INDEX, strip_nested

OUT = CACHE / "chunks"
CHUNK = 30000
# 地区 → 条目名（找不到的会提示）
ARTICLES = {
    "印度": ["Western Satraps", "Kushan Empire", "Satavahana dynasty", "Andhra Ikshvaku", "Vakataka dynasty",
             "Gupta Empire", "Nagas of Padmavati", "Abhira dynasty", "Pallava dynasty", "Kadamba dynasty",
             "Western Ganga dynasty", "Kushano-Sasanian Kingdom", "Murunda dynasty", "Kalabhra dynasty",
             "List of monarchs of Sri Lanka", "Anuradhapura Kingdom", "Sangam period", "Kuninda Kingdom",
             "Yaudheya", "Shaka era", "Sasanian Empire", "Kingdom of Khotan", "Funan"],
    "北非": ["List of Roman governors of Egypt", "List of Roman governors of Africa", "Scillitan Martyrs",
             "Martyrs of Abitinae", "Perpetua and Felicity", "Donatism", "Desert Fathers", "Pope of Alexandria",
             "List of kings of Axum", "List of monarchs of Kush", "Kingdom of Aksum", "Council of Carthage",
             "Seventh Council of Carthage", "Nobatia", "Blemmyes", "Meletian schism", "Pachomius the Great",
             "Melitius of Lycopolis", "Antony the Great"],
    "中东": ["Shapur I's inscription at Ka'ba-ye Zartosht", "Paikuli inscription", "Kartir", "Hatra",
             "Osroene", "Characene", "Adiabene", "Palmyrene Empire", "Palmyra", "Amoraim", "Tannaim",
             "Lakhmids", "Kingdom of Himyar", "List of rulers of Saba and Himyar", "Arsacid dynasty of Armenia",
             "Kingdom of Iberia", "List of monarchs of Iberia", "Caucasian Albania", "Bardaisan", "Edessa",
             "Mani (prophet)", "Manichaeism", "Nisibis", "Sasanian Empire", "Mamikonian", "Kamsarakan",
             "Bagratuni dynasty", "Arshakuni", "Persian Martyrs", "Shapur II", "Narseh", "Bahram II",
             "Hormizd II", "Kingdom of Armenia (antiquity)", "Emesa", "Nabataean Kingdom", "Persian martyr acts"],
}


def titles():
    """条目名 → 所在 bz2 流的 (起, 止)"""
    offs, where = [], {}
    with bz2.open(INDEX, "rt", encoding="utf-8") as f:
        for line in f:
            o, _, t = line.rstrip("\n").split(":", 2)
            o = int(o)
            if not offs or offs[-1] != o:
                offs.append(o)
            where[t] = len(offs) - 1
    offs.append(DUMP.stat().st_size)
    return where, offs


def article(t, where, offs):
    """取条目原文（跟随重定向一次）"""
    for _ in range(2):
        if t not in where:
            return None, t
        i = where[t]
        with open(DUMP, "rb") as f:
            f.seek(offs[i])
            data = bz2.decompress(f.read(offs[i + 1] - offs[i])).decode("utf-8", "replace")
        p = data[data.find(f"<title>{html.escape(t, quote=False)}</title>"):]
        p = p[:p.find("</page>")]
        m = re.search(r"<redirect title=\"([^\"]+)\"", p)
        if m:
            t = html.unescape(m.group(1))
            continue
        tm = re.search(r"<text[^>]*>(.*?)</text>", p, re.S)
        text = tm.group(1) if tm else ""
        return text.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", '"').replace("&amp;", "&"), t
    return None, t


def full_clean(text):
    """维基文本 → 纯文本，保留表格（列表条目里的人名多在表里）：表格每行一行、单元格用 | 分开"""
    t = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    t = re.sub(r"<ref[^>/]*/>", "", t)
    t = re.sub(r"<ref[^>]*>.*?</ref>", "", t, flags=re.S)
    t = re.sub(r"\{\{(?:lang|transl|nowrap|small|nihongo)\|(?:[^|{}]*\|)?([^{}]*?)\}\}", r"\1", t, flags=re.I)
    t = strip_nested(t, "{{", "}}")
    t = re.sub(r"\[\[(?:File|Image|Category):[^\[\]]*(\[\[[^\]]*\]\][^\[\]]*)*\]\]", "", t, flags=re.I)
    t = re.sub(r"\[\[([^|\]]*\|)?([^\]]*)\]\]", r"\2", t)
    t = re.sub(r"\[https?://\S+ ([^\]]*)\]", r"\1", t)
    t = re.sub(r"'{2,}", "", t)
    t = re.sub(r"^\{\|.*$|^\|\}.*$", "", t, flags=re.M)
    t = re.sub(r"^\|-.*$", "\n", t, flags=re.M)
    t = re.sub(r"^[|!]\s*", "| ", t, flags=re.M)
    t = re.sub(r"\|\||!!", " | ", t)
    t = re.sub(r"(?:style|class|rowspan|colspan|width|align|scope)=\"[^\"]*\"\s*\|?", "", t)
    t = re.sub(r"<[^>]+>", "", t)
    t = t.split("\n== References")[0].split("\n==References")[0].split("\n== See also")[0].split("\n== Notes")[0]
    t = re.sub(r"[ \t]+", " ", t)
    return re.sub(r"\n\s*\n+", "\n", t).strip()


def main():
    where, offs = titles()
    if len(sys.argv) > 2 and sys.argv[1] == "--find":
        kw = sys.argv[2].lower()
        for t in where:
            if kw in t.lower():
                print(t)
        return
    for f in OUT.glob("wl_*.txt"):
        f.unlink()
    parts, seen = [], set()
    for region, ts in ARTICLES.items():
        for t in ts:
            text, real = article(t, where, offs)
            if not text or real in seen:
                print("找不到或重复：", t, "→", real)
                continue
            seen.add(real)
            parts.append((region, real, full_clean(text)))
    chunks, cur, n = [], [], 0
    for region, t, body in parts:
        for i in range(0, len(body), CHUNK):
            piece = f"\n\n##### [{region}] Wikipedia: {t}" + (f"（续 {i // CHUNK}）" if i else "") + "\n" + body[i:i + CHUNK]
            if n + len(piece) > CHUNK and cur:
                chunks.append("".join(cur))
                cur, n = [], 0
            cur.append(piece)
            n += len(piece)
    if cur:
        chunks.append("".join(cur))
    for i, c in enumerate(chunks):
        heads = re.findall(r"##### \[[^\]]+\] Wikipedia: ([^\n（]+)", c)
        (OUT / f"wl_{i:02d}.txt").write_text(f"# 英文维基百科条目：{'；'.join(dict.fromkeys(heads))}\n" + c, encoding="utf-8")
    print("articles", len(parts), "chars", sum(len(b) for _, _, b in parts), "chunks", len(chunks))


if __name__ == "__main__":
    main()
