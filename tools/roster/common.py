"""名单工具的公共部分：路径、简繁转换、Big5 检查、光荣剧本武将记录的字段。

光荣剧本武将记录 152 字节（Scen*.s11，0..849 号），字段对照（用刘备、曹操、吕布等已知设定核对过）：
  0 姓[5]  5 名[5]  10 字[5]  15 [25] 空  40 [13] 空  53 头像 i16  55 性别
  56 登场 i16  58 生年 i16  60 卒年 i16  62 死法(0 天寿/病死 1 横死)  63 血缘 i16  65 父 i16  67 母 i16
  69 世代  70 配偶 i16  72 义兄弟 i16  74 相性(0..149)  75 亲近 i16[5]  85 厌恶 i16[5]
  95 军团  96 所属 i16  98 所在 i16  100 身份  101 (?)  104 忠诚  105 功绩 u16
  107 适性[6] 枪戟弩骑兵器水(0..3=C..S)  113 能力[5] 统武智政魅  118 成长类型[5]  123 (0..11 ?)  124 特技
  125 (0..2 ?)  126 义理(0..4)  127 野望(0..4)  128 起用(0..4)  129 性格(0..3)  130 声音(0..7)  131 口调(0..15)
  132 汉室(0..2)  133 战略倾向(0..3)  134 (0..2 ?)  135 (0..3 ?)  136 [6] 单挑模型  142 [6]  148 (8 的倍数)
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PERSONS = ROOT / "data" / "persons"
CACHE = PERSONS / "_cache"
SRC = PERSONS / "src"
RAW = PERSONS / "raw"
MAIN_REPO = ROOT.parent / "san11world"
KOEI_SCEN = Path(r"C:\Games\san11\Romance Of Three Kingdom 11 PK\Media\scenario\Scen000.s11")
ZHSAN_DIR = Path(r"C:\Games\san11\remake\data\out\ZHSan\Content\Data\Scenario")
ZHSAN_HIST = ["184DHZS", "190FDLM", "192XDZX", "194QXGJ", "197YSCD", "200GDZZ", "207SGML", "208CBZZ", "211LBRS",
              "217HZZZ", "219SFTX", "227WHBF", "251MSZZ", "263SHWJ", "280LWSG"]
QXZ_ROSTER = Path(r"C:\Users\garib\Documents\Work\CMCode\San11V1New\scripts\gendata\roster.mjs")
TPRSN = Path(r"C:\Games\san11\remake\data\out\tprsn_parsed.json")
SKILLS = Path(r"C:\Games\san11\remake\data\out\skills.json")

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

_cc = None


def s2t(text):
    """简体 → 繁体（OpenCC s2tw：台湾用字，和 Big5 字库最吻合）"""
    global _cc
    if _cc is None:
        import opencc
        _cc = opencc.OpenCC("s2tw")
    return _cc.convert(text)


def big5_ok(text):
    try:
        text.encode("cp950")
        return True
    except UnicodeEncodeError:
        return False


def big5_bad_chars(text):
    return "".join(ch for ch in text if not big5_ok(ch))


COMPOUND = ("諸葛", "司馬", "夏侯", "公孫", "太史", "皇甫", "歐陽", "上官", "令狐", "東方", "司徒", "鍾離", "宇文", "慕容",
            "拓跋", "禿髮", "乞伏", "沮渠", "赫連", "呼延", "獨孤", "長孫", "宇文", "賀蘭", "万俟", "尉遲", "呼衍", "淳于",
            "公羊", "司空", "毌丘", "母丘", "胡母", "諸葛", "第五", "鮮于", "閭丘", "段干", "百里", "東郭", "南宮", "西門",
            "羊舌", "微生", "梁丘", "左丘", "東門", "濮陽", "单于", "單于", "公沙", "士孫", "馬適", "壺丘", "苻", "姚")


def split_name(name, sei=None):
    """全名 → (姓, 名)；姓、名各最多 2 字"""
    if sei and name.startswith(sei):
        return sei, name[len(sei):]
    for c in COMPOUND:
        if len(c) == 2 and name.startswith(c) and len(name) > 2:
            return c, name[2:]
    if len(name) == 4:
        return name[:2], name[2:]
    return name[:1], name[1:]


def load_json(p):
    return json.loads(Path(p).read_text(encoding="utf-8"))


def save_json(p, obj, indent=None):
    p = Path(p)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(obj, ensure_ascii=False, indent=indent), encoding="utf-8")


def load_jsonl(p):
    out = []
    for line in Path(p).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line:
            out.append(json.loads(line))
    return out


def save_jsonl(p, rows):
    p = Path(p)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text("".join(json.dumps(r, ensure_ascii=False) + "\n" for r in rows), encoding="utf-8")


def koei_records():
    """光荣原版剧本 0..849 号武将的原始 152 字节记录"""
    sys.path.insert(0, str(ROOT / "tools"))
    from convert_scen import scen_layout
    D = KOEI_SCEN.read_bytes()
    L, end = scen_layout(42, 87)
    assert end == len(D)
    o, c, s = L["person"]
    return [D[o + s * i:o + s * (i + 1)] for i in range(c)]


def cstr(b):
    return b.split(b"\0")[0].decode("cp950", "replace")


def i16(r, off):
    return struct.unpack_from("<h", r, off)[0]
