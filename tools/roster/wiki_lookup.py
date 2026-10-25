"""本地英文维基百科查询（给子代理查条目用）：条目名索引存进 sqlite，查找和取全文都是即时的

    python wiki_lookup.py --build              # 建索引（第一次，约几分钟）
    python wiki_lookup.py --find "Ermanaric"   # 找条目名（包含关键词，不分大小写，最多 50 个）
    python wiki_lookup.py --get "Ermanaric"    # 取条目纯文本（跟随重定向，保留表格）"""
import bz2
import sqlite3
import sys
from common import CACHE
from scan_wiki import DUMP, INDEX

DB = CACHE / "wiki" / "titles.sqlite"


def build():
    if DB.exists():
        DB.unlink()
    db = sqlite3.connect(DB)
    db.execute("create table t (title text, stream integer)")
    db.execute("create table s (i integer primary key, lo integer, hi integer)")
    offs, rows = [], []
    with bz2.open(INDEX, "rt", encoding="utf-8") as f:
        for line in f:
            o, _, title = line.rstrip("\n").split(":", 2)
            o = int(o)
            if not offs or offs[-1] != o:
                offs.append(o)
            rows.append((title, len(offs) - 1))
            if len(rows) >= 500000:
                db.executemany("insert into t values (?, ?)", rows)
                rows = []
    db.executemany("insert into t values (?, ?)", rows)
    offs.append(DUMP.stat().st_size)
    db.executemany("insert into s values (?, ?, ?)", [(i, offs[i], offs[i + 1]) for i in range(len(offs) - 1)])
    db.execute("create index ti on t(title)")
    db.execute("create index tl on t(lower(title))")
    db.commit()
    print("titles indexed")


def get(title):
    """条目纯文本（跟随重定向）；找不到返回 None"""
    import html
    import re
    from wiki_articles import full_clean
    db = sqlite3.connect(DB)
    for _ in range(3):
        row = db.execute("select stream from t where title = ?", (title,)).fetchone() or \
            db.execute("select stream from t where lower(title) = lower(?)", (title,)).fetchone()
        if not row:
            return None, title
        lo, hi = db.execute("select lo, hi from s where i = ?", (row[0],)).fetchone()
        with open(DUMP, "rb") as f:
            f.seek(lo)
            data = bz2.decompress(f.read(hi - lo)).decode("utf-8", "replace")
        k = data.find(f"<title>{html.escape(title, quote=False)}</title>")
        if k < 0:
            k = data.lower().find(f"<title>{html.escape(title, quote=False).lower()}</title>")
        p = data[k:]
        p = p[:p.find("</page>")]
        m = re.search(r"<redirect title=\"([^\"]+)\"", p)
        if m:
            title = html.unescape(m.group(1))
            continue
        tm = re.search(r"<text[^>]*>(.*?)</text>", p, re.S)
        return full_clean(html.unescape(tm.group(1)) if tm else ""), title
    return None, title


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    if sys.argv[1] == "--build":
        build()
    elif sys.argv[1] == "--find":
        db = sqlite3.connect(DB)
        kw = sys.argv[2].lower()
        for (t,) in db.execute("select title from t where lower(title) like ? and title not like '%:%' limit 50",
                               (f"%{kw}%",)):
            print(t)
    elif sys.argv[1] == "--get":
        text, real = get(sys.argv[2])
        print(f"# {real}\n{text}" if text is not None else f"找不到：{sys.argv[2]}")


if __name__ == "__main__":
    main()
