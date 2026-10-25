"""扫描英文维基百科转储，找在世时间与 170–370 年有交集的中国以外人物 → data/persons/src/wiki.json

依据条目的分类（「230s births」「268 deaths」「3rd-century Roman emperors」……）判断年代；
中国（汉、三国、晋、十六国）和中国北方民族的人物由中文史书覆盖，这里跳过；
已经在 Laouenan 名人库里的（按 Wikidata 编号）也跳过。

    python scan_wiki.py [进程数，默认 20]"""
import bz2
import gzip
import json
import multiprocessing as mp
import re
import sys
from common import CACHE, SRC, load_json, save_json

W = CACHE / "wiki"
DUMP = W / "enwiki-latest-pages-articles-multistream.xml.bz2"
INDEX = W / "enwiki-latest-pages-articles-multistream-index.txt.bz2"
LO, HI = 170, 370

CAT = re.compile(r"\[\[\s*Category\s*:\s*([^\]|\n]+)", re.I)
ORD = {"1st": 1, "2nd": 2, "3rd": 3, "4th": 4, "5th": 5}
CHINA = re.compile(r"Han dynasty|Eastern Han|Cao Wei|Shu Han|Eastern Wu|Jin dynasty|Sixteen Kingdoms|Former |Later Zhao|"
                   r"Cheng-Han|Chinese|China|Xianbei|Xiongnu|Wuhuan|Di people|Qiang|Jie people|Yellow Turban|"
                   r"Three Kingdoms|Han-Zhao|Dai \(Sixteen|Ran Wei|Tuyuhun|Northern Wei|Liu Song|Southern dynasties", re.I)
NOT_PERSON = re.compile(r"Lists? of|disambiguation|^\d+s?$|Battle of|Siege of|Council of|families$|dynasty$", re.I)
LEGEND = re.compile(r"legendary|mytholog|fictional|apocryphal", re.I)


def years_from(cats):
    """分类 → (生年下限, 生年上限, 卒年下限, 卒年上限, 活动世纪集合)"""
    b = d = None
    cent = set()
    for c in cats:
        if re.search(r"\bBC\b|BCE", c):
            continue
        m = re.fullmatch(r"(\d{1,3})(s)? (births|deaths)", c)
        if m:
            y = int(m.group(1))
            rng = (y, y + 9) if m.group(2) else (y, y)
            if m.group(3) == "births":
                b = rng
            else:
                d = rng
            continue
        m = re.fullmatch(r"(1st|2nd|3rd|4th|5th)-century (births|deaths)", c)
        if m:
            n = ORD[m.group(1)]
            rng = (100 * (n - 1) + 1, 100 * n)
            if m.group(2) == "births" and b is None:
                b = rng
            if m.group(2) == "deaths" and d is None:
                d = rng
            continue
        m = re.match(r"(1st|2nd|3rd|4th|5th)-century ", c)
        if m:
            cent.add(ORD[m.group(1)])
        m = re.match(r"(\d{3})s? (?!births|deaths)", c)               # 「230s in the Roman Empire」之类不算
    return b, d, cent


def in_window(b, d, cent, act=None):
    if b and d:
        return d[1] >= LO and b[0] <= HI - 15
    if d:
        return d[1] >= LO and d[0] <= HI + 50
    if b:
        return b[1] >= LO - 60 and b[0] <= HI - 15
    if act:
        return act[1] >= LO and act[0] <= HI
    return bool(cent & {2, 3, 4})        # 只有世纪：2–4 世纪都收（2 世纪的由 batch_wiki 按导言里的年份再筛，
                                         # 4 世纪后期的人由子代理按年份 skip）


INFO_KEYS = r"birth_date|death_date|reign|term_start|term_end|years_active|term|office_term|era"
INFOBOX_PERSON = re.compile(r"\{\{\s*Infobox (royalty|monarch|person|officeholder|saint|Christian leader|"
                            r"military person|philosopher|writer|religious biography|noble|pharaoh|clergy)", re.I)


def info_years(text):
    """信息框里生卒、在位字段的公元年（100..450，带 BC 的不算）→ (最早, 最晚) 或 None"""
    ys = []
    for m in re.finditer(rf"\|\s*(?:{INFO_KEYS})\s*=\s*([^\n]*)", text[:8000]):
        v = m.group(1)
        if re.search(r"\bBC|BCE", v):
            continue
        ys += [int(y) for y in re.findall(r"(?<![\d.])(\d{3})(?![\d])", v) if 100 <= int(y) <= 450]
    return (min(ys), max(ys)) if ys else None


def strip_nested(t, op, cl):
    """去掉成对的 op…cl（可嵌套，如模板 {{ }}、表格 {| |}）"""
    out, depth, i = [], 0, 0
    while i < len(t):
        if t.startswith(op, i):
            depth += 1
            i += len(op)
        elif depth and t.startswith(cl, i):
            depth -= 1
            i += len(cl)
        else:
            if not depth:
                out.append(t[i])
            i += 1
    return "".join(out)


def clean(text):
    """维基文本 → 导言的纯文本（去模板、引用、表格、文件链接）"""
    t = text[:40000]
    t = re.sub(r"<!--.*?-->", "", t, flags=re.S)
    t = re.sub(r"<ref[^>/]*/>", "", t)
    t = re.sub(r"<ref[^>]*>.*?</ref>", "", t, flags=re.S)
    t = strip_nested(strip_nested(t, "{{", "}}"), "{|", "|}")
    t = re.sub(r"\[\[(?:File|Image|Category):[^\[\]]*(\[\[[^\]]*\]\][^\[\]]*)*\]\]", "", t, flags=re.I)
    t = re.sub(r"\[\[([^|\]]*\|)?([^\]]*)\]\]", r"\2", t)
    t = re.sub(r"\[https?://\S+ ([^\]]*)\]", r"\1", t)
    t = re.sub(r"'{2,}", "", t)
    t = re.sub(r"<[^>]+>", "", t)
    lead = t.split("\n==")[0]
    lead = re.sub(r"\s+", " ", lead).strip()
    return lead


def infobox(text):
    out = {}
    for k in ("birth_date", "death_date", "birth_place", "death_place", "reign", "predecessor", "successor",
              "spouse", "father", "mother", "issue", "dynasty", "religion", "allegiance", "rank", "office"):
        m = re.search(rf"\|\s*{k}\s*=\s*([^\n|]*(?:\[\[[^\]]*\]\][^\n|]*)*)", text)
        if m:
            v = re.sub(r"\[\[([^|\]]*\|)?([^\]]*)\]\]", r"\2", m.group(1))
            v = re.sub(r"\{\{|\}\}|<[^>]+>", " ", v)
            v = re.sub(r"\s+", " ", v).strip()[:80]
            if v:
                out[k] = v
    return out


PAGE = re.compile(r"<page>(.*?)</page>", re.S)


def work(args):
    lo_hi = args
    out = []
    with open(DUMP, "rb") as f:
        for lo, hi in lo_hi:
            f.seek(lo)
            data = bz2.decompress(f.read(hi - lo)).decode("utf-8", "replace")
            for pm in PAGE.finditer(data):
                p = pm.group(1)
                if "<ns>0</ns>" not in p or "<redirect" in p:
                    continue
                tm = re.search(r"<text[^>]*>(.*?)</text>", p, re.S)
                if not tm:
                    continue
                text = (tm.group(1).replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", '"')
                        .replace("&amp;", "&"))
                cats = [c.strip() for c in CAT.findall(text)]
                person_box = bool(INFOBOX_PERSON.search(text[:8000]))
                if not person_box and not any(re.search(r"births|deaths|century", c) for c in cats):
                    continue
                b, d, cent = years_from(cats)
                act = info_years(text) if person_box and not (b or d) else None
                if not (b or d or cent or act) or not in_window(b, d, cent, act):
                    continue
                if not (b or d or act or any(re.search(r"births|deaths|people|year of", c, re.I) for c in cats)):
                    # 只有「3rd-century X」：X 是人的类别才收（emperors、bishops、writers……）
                    if not any(re.search(r"century (\w+ )*(emperors|kings|queens|monarchs|rulers|bishops|popes|saints|"
                                         r"martyrs|writers|philosophers|historians|poets|Romans|Greeks|Persians|"
                                         r"consuls|senators|generals|usurpers|princes|princesses|women|men|theologians|"
                                         r"physicians|jurists|rhetoricians|scholars|clergy|priests|nobility)", c, re.I)
                               for c in cats):
                        continue
                title = re.search(r"<title>(.*?)</title>", p).group(1).replace("&amp;", "&").replace("&quot;", '"')
                if NOT_PERSON.search(title) or any(CHINA.search(c) for c in cats):
                    continue
                pid = int(re.search(r"<id>(\d+)</id>", p).group(1))
                out.append({"title": title, "page": pid, "b": b, "d": d, "cent": sorted(cent), "act": act,
                            "cats": [c for c in cats if not re.search(r"Articles|Wikipedia|CS1|Pages|Webarchive|"
                                                                      r"Use dmy|Use mdy|stub|Short description", c)][:14],
                            "legend": any(LEGEND.search(c) for c in cats), "info": infobox(text[:6000]),
                            "lead": clean(text)[:900]})
    return out


def streams():
    """多流索引：每行「偏移:页号:标题」，同一偏移是一个 bz2 流"""
    offs = []
    last = None
    with bz2.open(INDEX, "rt", encoding="utf-8") as f:
        for line in f:
            o = int(line.split(":", 1)[0])
            if o != last:
                offs.append(o)
                last = o
    offs.append(DUMP.stat().st_size)
    return list(zip(offs, offs[1:]))


def sql_rows(path, table):
    """mysqldump 的 INSERT 语句 → 每行一个元组（只解析数字和单引号字符串）"""
    tok = re.compile(r"\((\d+),'((?:[^'\\]|\\.)*)','((?:[^'\\]|\\.)*)'")
    inside = False
    with gzip.open(path, "rt", encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith(f"INSERT INTO `{table}`"):
                inside = True                     # 新版转储：INSERT 一行，数据在后面各行
            elif not line.startswith("("):
                inside = False
            if inside:
                for m in tok.finditer(line):
                    yield int(m.group(1)), m.group(2), m.group(3).replace("\\'", "'").replace('\\"', '"')


def main():
    nproc = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    ss = streams()
    print("streams", len(ss), flush=True)
    groups = [ss[i::nproc * 8] for i in range(nproc * 8)]
    found = []
    with mp.Pool(nproc) as pool:
        for i, r in enumerate(pool.imap_unordered(work, groups)):
            found += r
            if i % 20 == 0:
                print(f"{i + 1}/{len(groups)}", len(found), flush=True)
    pages = {e["page"] for e in found}
    qid, zh = {}, {}
    for pg, name, val in sql_rows(W / "enwiki-latest-page_props.sql.gz", "page_props"):
        if pg in pages and name == "wikibase_item":
            qid[pg] = val
    for pg, lang, val in sql_rows(W / "enwiki-latest-langlinks.sql.gz", "langlinks"):
        if pg in pages and lang in ("zh", "ja", "ko"):
            zh.setdefault(pg, {})[lang] = val
    laou = {e["wikidata_code"] for e in load_json(SRC / "laouenan.json")}
    out = []
    for e in found:
        e["qid"] = qid.get(e["page"], "")
        e["langs"] = zh.get(e["page"], {})
        e["in_laouenan"] = e["qid"] in laou
        out.append(e)
    out.sort(key=lambda e: e["title"])
    save_json(SRC / "wiki.json", out)
    print("wiki persons", len(out), "new (not in Laouenan)", sum(not e["in_laouenan"] for e in out))


if __name__ == "__main__":
    main()
