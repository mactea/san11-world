"""公版/开放网站上的史料 → 子代理用的块 data/persons/_cache/chunks/<前缀>_NN.txt
（文件先用浏览器或下载脚本存到 data/persons/_cache/texts/<书>/）

  fau  拜占庭的浮士德《亚美尼亚史》（Langlois 法译 1867，remacle.org）            330–387 年
  moi  霍伦的摩西《亚美尼亚史》卷 2–3（Langlois 法译 1869，remacle.org）           帕提亚末至 5 世纪
  mhv  《大史》第 33–37 章（Geiger 英译 1912，lakdiva.org）+《岛史》第 21–22 章
       （Oldenberg 英译 1879，ancient-buddhist-texts.net）                          斯里兰卡诸王
  niya 尼雅佉卢文书（Burrow 英译 1940，华盛顿大学丝路网站）                        3 世纪鄯善
  cth  《狄奥多西法典》各条的受文人、日期（拉丁文，thelatinlibrary.com）            313–438 年
  je   犹太百科「Amora」「Tannaim and Amoraim」（1906，公版）                       3–4 世纪拉比
  soc / soz / thd / phs  苏格拉底卷 1–4、索佐门卷 1–6、狄奥多勒卷 1–4、腓罗斯托尔吉乌斯《教会史》（希腊文，First1KGreek）
  vc   优西比乌《君士坦丁传》（希腊文，First1KGreek）

    python split_open.py [每块字数，默认 30000] [church：只切教会史家]"""
import html as html_mod
import re
import sys
from common import CACHE
from split_classic import GH, cut, get, tei_text
import split_classic

T = CACHE / "texts"


def clean(h):
    """一般网页 → 纯文本"""
    h = re.sub(r"<(script|style|head)\b.*?</\1>", " ", h, flags=re.S | re.I)
    h = re.sub(r"<(p|br|div|h\d|li|tr)\b[^>]*>", "\n", h, flags=re.I)
    t = html_mod.unescape(re.sub(r"<[^>]+>", "", h))
    t = re.sub(r"[ \t\xa0]+", " ", t)
    return re.sub(r"\n\s*\n+", "\n", t).strip()


def read(book, name):
    b = (T / book / name).read_bytes()
    for enc in ("utf-8", "cp1252"):
        try:
            return b.decode(enc)
        except UnicodeDecodeError:
            pass


def remacle(h):
    """remacle.org：脚注区（第二次出现的 _ftn1 锚点之后）去掉，正文里的 [N] 脚注号去掉"""
    marks = [m.start() for m in re.finditer(r'name="?_ftn1"?', h, flags=re.I)]
    if marks:
        h = h[:marks[-1]]
    h = re.sub(r'<a[^>]+href="#_ftn\d+"[^>]*>.*?</a>', "", h, flags=re.S | re.I)
    return clean(h)


def cth():
    """每条法令只留：编号、开头（皇帝 + 受文人 + 官职）、结尾的日期（dat./data/pp./acc. … coss.）"""
    rows = []
    for i in range(1, 17):
        t = clean(read("theod", f"theod{i:02d}.shtml"))
        for m in re.finditer(r"(CTh\.\d+\.\d+\.\d+[a-z]?)[^\n]*\n(.*?)(?=\nCTh\.|\Z)", t, flags=re.S):
            body = m.group(2).split("\ninterpretatio")[0].replace("\n", " ").strip()
            if not body or not re.search(r"\b(imp|idem|iidem|eadem)", body[:120], flags=re.I):    # 目录、标题行
                continue
            head = body[:130]
            d = re.search(r"\b(dat|data|p\. ?p|pp|acc|lecta|proposita|recitata)\b\.?[^.]{0,20}\..{0,160}$", body)
            tail = d.group(0) if d else body[-120:]
            rows.append(f"{m.group(1)} | {head} || {tail}")
    return "\n".join(rows)


def main():
    split_classic.CHUNK = int(sys.argv[1]) if len(sys.argv) > 1 else 30000
    if "church" in sys.argv:
        return church()
    cut("fau", "Faustus of Byzantium, History of Armenia (tr. Langlois, French)",
        remacle(read("faustus", "histoire.htm")))
    cut("moi", "Moses of Khoren, History of Armenia, books II-III (tr. Langlois, French)",
        "\n".join(remacle(read("moise", f)) for f in ("histoire2.htm", "histoire3.htm")))
    cut("mhv", "Mahavamsa ch. 33-37 (tr. Geiger) + Dipavamsa ch. 21-23 (tr. Oldenberg)",
        "\n".join([f"\n[Mahavamsa {i}]\n" + clean(read("mahavamsa", f"chap{i:03d}.html")) for i in range(33, 38)]
                  + [f"\n[Dipavamsa {f[:2]}]\n" + clean(read("dipavamsa", f))
                     for f in ("21-Abhaya.htm", "22-Vasabha.htm", "23-Table-of-Kings.htm")]))
    cut("niya", "Kharosthi documents from Niya / Chinese Turkestan (tr. Burrow 1940)",
        clean(read("niya", "niyadocts.html")))
    cut("cth", "Codex Theodosianus: law number | opening (emperors, addressee, office) || dating clause", cth())
    cut("je", "Jewish Encyclopedia (1906): Amora; Tannaim and Amoraim",
        "\n".join(clean(read("je", f)) for f in ("1421-amora", "14240-tannaim-and-amoraim")))
    church()


def church():
    for prefix, (path, title, last) in CHURCH.items():
        text = tei_text(get(GH + "OpenGreekAndLatin/First1KGreek/master/data/" + path, T / "classic" / path.split("/")[-1]))
        m = re.search(rf"^\[Book {last + 1}\]", text, flags=re.M) if last else None
        cut(prefix, title, text[:m.start()] if m else text)


# 教会史家（希腊文，First1KGreek）：只取写到 370 年代为止的卷
CHURCH = {"soc": ("tlg2057/tlg002/tlg2057.tlg002.1st1K-grc1.xml", "Socrates Scholasticus, Ecclesiastical History, books 1-4", 4),
          "soz": ("tlg2048/tlg001/tlg2048.tlg001.1st1K-grc1.xml", "Sozomen, Ecclesiastical History, books 1-6", 6),
          "thd": ("tlg4089/tlg003/tlg4089.tlg003.1st1K-grc1.xml", "Theodoret, Ecclesiastical History, books 1-4", 4),
          "phs": ("tlg2058/tlg001/tlg2058.tlg001.1st1K-grc1.xml", "Philostorgius, Ecclesiastical History (epitome)", 0),
          "vc": ("tlg2018/tlg020/tlg2018.tlg020.1st1K-grc1.xml", "Eusebius, Life of Constantine", 0)}


if __name__ == "__main__":
    main()
