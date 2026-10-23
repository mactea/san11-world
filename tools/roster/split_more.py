"""第五轮：还没读过的中文史料 → 子代理用的块 data/persons/_cache/chunks/<前缀>_NN.txt（规则 raw/SPEC_TEXT.md）

  wsh  魏书 卷一序纪、卷十三皇后传、卷十四/十五/二十三/二十四（拓跋早期宗室、代国臣僚）、
       卷九十五..一百三（十六国诸主、东北诸国、氐羌吐谷浑、西域、蠕蠕、宇文、段部、高车）
  yi   宋书卷九十五..九十八、南齐书卷五十七..五十九、梁书卷五十四（四夷传，只收追记 370 年以前的人）
  jk   建康实录 卷一..卷九（孙吴、东晋到孝武帝）
  yl   太平御览里引用的亡佚史书（王隐晋书、晋中兴书、魏略、江表传、吴录……）的条目
  sz   僧传：比丘尼传（CBETA T2063）、出三藏记集卷十三..十五（T2145）、名僧传抄（X1523）

    python split_more.py [每块字数，默认 30000]"""
import re
import sys
import urllib.request
from common import CACHE

T = CACHE / "texts"
OUT = CACHE / "chunks"
CHUNK = 30000
CB = "https://raw.githubusercontent.com/cbeta-org/xml-p5/master/"
NUM = "一二三四五六七八九十百"


def cut(prefix, title, text):
    """同 split_books.cut（那个模块一导入就会重切别的书，所以这里另写一份）"""
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
        heads = re.findall(r"(?:卷[一二三四五六七八九十百]+|〔[^〕]{1,30}〕)[^\n]{0,20}", c)
        (OUT / f"{prefix}_{i:02d}.txt").write_text(
            f"# {title} {heads[0] if heads else ''} .. {heads[-1] if heads else ''}\n" + c, encoding="utf-8")
    print(prefix, len(text), "chars,", len(chunks), "chunks")


def juan(t, names):
    """纪传体正史：按「卷X 」标题取指定的卷（目录里也有标题，取正文里那次）"""
    heads = [(m.start(), m.group(1)) for m in re.finditer(rf"^(卷[{NUM}]+[上中下]?) ", t, flags=re.M)]
    body = {}
    for (a, name), nxt in zip(heads, heads[1:] + [(len(t), "")]):
        body[name] = t[a:nxt[0]]                     # 后出现的覆盖目录里的
    return "\n".join(body[n] for n in names if n in body)


def weishu(t, names):
    """殆知阁本魏书、宋书、南齐书、梁书正文的卷首是「帝纪第一 序纪」「列传第八十三」（「卷X」只在目录里）"""
    heads = [(m.start(), m.group(1)) for m in
             re.finditer(rf"^\s*((?:帝纪|列传|志)[^第\n]{{0,4}}第[{NUM}]+[上中下]?)", t, flags=re.M)]
    body = {}
    for (a, name), nxt in zip(heads, heads[1:] + [(len(t), "")]):
        body.setdefault(re.sub(r"(帝纪|列传|志)[^第]*第", r"\1第", name), t[a:nxt[0]])
    return "\n".join(body[n] for n in names if n in body)


def cn(n):
    """阿拉伯数字 → 卷名用的中文数字（1..120）"""
    d = "零一二三四五六七八九"
    if n < 10:
        return d[n]
    if n < 20:
        return "十" + (d[n - 10] if n > 10 else "")
    if n < 100:
        return d[n // 10] + "十" + (d[n % 10] if n % 10 else "")
    r = n - 100
    return "一百" + ("" if r == 0 else d[r] if r < 10 else cn(r))


# 太平御览里引的、今已亡佚或只剩辑本的汉晋史书（正史、现存书不收，那些已经读过）
LOST = ["王隐晋书", "臧荣绪晋书", "虞预晋书", "朱凤晋书", "何法盛晋中兴书", "晋中兴书", "晋阳秋", "续晋阳秋", "汉晋春秋",
        "晋纪", "干宝晋纪", "邓粲晋纪", "徐广晋纪", "晋诸公赞", "晋诸公别传", "晋八王故事", "晋起居注", "晋百官名", "晋东宫旧事",
        "晋惠帝起居注", "晋太康起居注", "晋咸康起居注", "晋令", "魏略", "魏武故事", "魏晋世语", "世语", "魏末传", "魏氏春秋",
        "吴书", "吴录", "吴历", "江表传", "英雄记", "九州春秋", "典略", "献帝春秋", "献帝传", "山阳公载记", "汉末英雄记",
        "续汉书", "谢承后汉书", "华峤后汉书", "袁山松后汉书", "司马彪续汉书", "张璠汉纪", "张璠后汉纪", "袁宏后汉纪", "汉魏春秋",
        "益部耆旧传", "益部耆旧杂记", "会稽典录", "汝南先贤传", "陈留耆旧传", "零陵先贤传", "楚国先贤传", "襄阳记", "襄阳耆旧传",
        "豫章旧志", "长沙耆旧传", "武昌记", "荆州记", "广州记", "交州记", "蜀记", "季汉辅臣赞",
        "三十国春秋", "赵书", "前燕录", "后燕录", "燕书", "秦书", "前秦录", "凉州记", "西河记",
        "段龟龙凉州记", "车频秦书", "裴景仁秦书", "田融赵书", "范亨燕书", "王度二石传",
        "孙盛晋阳秋", "孙盛魏氏春秋", "环济吴纪", "张勃吴录", "韦昭吴书", "虞溥江表传", "鱼豢魏略", "王沈魏书", "郭颁世语",
        "傅畅晋诸公赞", "荀绰冀州记", "冀州记", "兖州记", "晋后略", "晋后书", "中兴书", "逸士传",
        "文士传", "名士传", "竹林七贤论", "七贤传", "孝子传", "妒记", "语林", "郭子", "俗说"]
LOST_SET = set(LOST)


def yulan(t):
    """每条引文以「《书名》曰」开头，「又曰」接上一部书。只留亡佚史书的条目，前面加门类标题方便定位"""
    out, book, cat = [], None, ""
    for line in t.split("\n"):
        s = line.strip()
        m = re.match(r"^([^《\s“”：，。]{0,4})《([^》]{1,12})》曰", s)       # 「王隐《晋书》曰」「《魏略》曰」
        if m:
            book = m.group(1) + m.group(2)
        elif not s.startswith("又曰"):
            if re.match(r"^(卷第?[" + NUM + r"]+|[^《]{1,20}部[" + NUM + r"]*)", s) and len(s) < 40:
                cat = s
            book = None if not s.startswith("又") else book
            continue
        if book in LOST_SET and len(s) > 12:
            out.append(f"〔{cat}〕《{book}》" + s[s.find("曰"):] if s.startswith("又曰") else f"〔{cat}〕{s}")
    seen, uniq = set(), []
    for x in out:                                    # 同一条在不同门类里重复引
        k = re.sub(r"^〔[^〕]*〕", "", x)
        if k not in seen:
            seen.add(k)
            uniq.append(x)
    return "\n".join(uniq)


def cbeta(path, juans=None):
    """CBETA P5 XML → 纯文本；juans：只要这些卷（整数）"""
    f = T / path.split("/")[-1]
    if not f.exists():
        f.write_bytes(urllib.request.urlopen(urllib.request.Request(CB + path, headers={"User-Agent": "san11world-roster/1.0"}),
                                             timeout=300).read())
    x = f.read_text(encoding="utf-8")
    x = x[x.find("<body"):]
    x = re.sub(r"<(note|rdg|cb:t)\b[^>]*>.*?</\1>", "", x, flags=re.S)
    x = re.sub(r'<milestone(?=[^>]*unit="juan")[^>]*\bn="(\d+)"[^>]*/>', lambda m: f"\n〔卷{int(m.group(1))}〕\n", x)
    x = re.sub(r"<(p|lb|head|lg|l)\b[^>]*>", "\n", x)
    x = re.sub(r"<[^>]+>", "", x)
    x = re.sub(r"[ \t]+", "", x)
    x = re.sub(r"\n\s*\n+", "\n", x)
    if juans:
        parts = re.split(r"\n(?=〔卷\d+〕)", x)
        x = "\n".join(p for p in parts if (m := re.match(r"〔卷(\d+)〕", p)) and int(m.group(1)) in juans)
    return x


def main():
    global CHUNK
    CHUNK = int(sys.argv[1]) if len(sys.argv) > 1 else 30000
    ws = (T / "魏书.txt").read_text(encoding="utf-8")
    cut("wsh", "魏书", weishu(ws, ["帝纪第一", "列传第一", "列传第二", "列传第三", "列传第十一", "列传第十二"] +
                         [f"列传第{cn(n)}" for n in range(83, 92)]))
    yi = [weishu((T / "宋书.txt").read_text(encoding="utf-8"), [f"列传第{cn(n)}" for n in range(55, 59)]),
          weishu((T / "南齐书.txt").read_text(encoding="utf-8"), ["列传第三十八", "列传第三十九", "列传第四十"]),
          weishu((T / "梁书.txt").read_text(encoding="utf-8"), ["列传第四十八"])]
    cut("yi", "宋书、南齐书、梁书 四夷传", "\n".join(yi))
    jk = (T / "建康实录.txt").read_text(encoding="utf-8")
    a, b = re.search(r"建康实?[録录]卷一\n", jk), re.search(r"建康实?[録录]卷十\n", jk)   # 四库本卷首「建康実録卷N」
    cut("jk", "建康实录", jk[a.start():b.start() if b else len(jk)])
    cut("yl", "太平御览所引亡佚史书", yulan((T / "太平御览.txt").read_text(encoding="utf-8")))
    cut("sz", "比丘尼传、出三藏记集（卷十三至十五）、名僧传抄",
        "\n".join([cbeta("T/T50/T50n2063.xml"), cbeta("T/T55/T55n2145.xml", {13, 14, 15}),
                   cbeta("X/X77/X77n1523.xml")]))


if __name__ == "__main__":
    main()
