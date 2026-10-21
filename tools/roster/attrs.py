"""按三国志11 的字段生成扩展武将属性（确定性：同一人每次生成结果相同）。

输入是合并后的人物（五维、兵种倾向、身份、所属、年份……），输出补齐：适性、成长、特技、义理、野望、起用、性格、
声音、口调、汉室、战略倾向、相性、世代、死法、出身地方，以及光荣记录里语义未明的字节（按同类光荣武将取值）。
规则的依据是光荣原版 0..669 号武将的统计（src/koei.json）。"""
import hashlib
from common import SRC, load_json

LEAN = {"枪": 0, "槍": 0, "戟": 1, "弩": 2, "骑": 3, "騎": 3, "兵器": 4, "水": 5}
# 只用原版特技 0..99（100 号以后是社区补丁加的）；光荣原版里只有 0~1 人持有的算稀有，只给顶级人物
_skill_rows = None


def skills():
    global _skill_rows
    if _skill_rows is None:
        _skill_rows = {e["id"]: e for e in load_json(r"C:\Games\san11\remake\data\out\skills.json")["skills"] if e["id"] < 100}
    return _skill_rows


def rnd(key, salt, n):
    """确定性伪随机 0..n-1"""
    h = hashlib.md5(f"{key}|{salt}".encode("utf-8")).digest()
    return int.from_bytes(h[:4], "little") % n


def clamp(v, lo=1, hi=100):
    return max(lo, min(hi, int(round(v))))


# ---- 适性 0..3 = C B A S
def aptitude(key, stats, lean, region):
    ld, st, it = stats[0], stats[1], stats[2]
    base = 0 if ld < 55 else 1 if ld < 78 else 2
    apt = []
    for t in range(6):
        a = base
        if t == 4:                                     # 兵器：偏智力
            a = 0 if it < 60 else 1 if it < 80 else 2
        if t == 5:                                     # 水军：南方、海边加一档，北方、草原减一档
            a = base + (1 if region in SOUTH else -1 if region in STEPPE else 0)
        if LEAN.get(lean) == t:
            a += 1
        elif rnd(key, f"apt{t}", 5) == 0:
            a += 1 if rnd(key, f"aptd{t}", 2) else -1
        if a >= 3 and not (ld >= 85 and (st >= 80 or t == 4)):
            a = 2                                      # S 只给统率高的名将
        apt.append(max(0, min(3, a)))
    return apt


SOUTH = {"揚州", "荊州", "交州", "南中", "南海", "倭", "東夷", "夷洲", "希臘", "西西里", "撒丁", "科西嘉", "塞浦路斯",
         "呂基亞", "北歐", "不列顛", "意大利", "埃及", "腓尼基", "阿非利加", "天竺"}
STEPPE = {"匈奴", "鮮卑", "康居", "薩爾馬提亞", "伏爾加", "烏拉爾", "西伯利亞", "西羌", "西域", "涼州", "并州", "貴霜",
          "撒哈拉", "阿拉伯"}


# ---- 特技
def pick_skill(key, stats, lean, role, tier):
    """tier: 0 普通 1 名人 2 一流（史书公认）；约一半人有特技，和光荣原版比例相当"""
    ld, st, it, po, ch = stats
    t = LEAN.get(lean)
    # 能力一般的人也能有的常见特技（光荣原版里普通武将、文官常见的那些）
    if max(ld, st) >= max(it, po):
        cands = [(2, 9), (2, 27), (1, 11), (1, 21), (2, 7), (1, 6), (1, 8), (1, 51), (1, 33), (1, 13), (1, 30), (1, 72)]
    else:
        cands = [(2, 72), (2, 73), (1, 84), (2, 85), (2, 86), (2, 80), (1, 81), (2, 77), (2, 78), (1, 97), (1, 98),
                 (1, 95), (1, 96), (1, 75), (1, 76)]
    if st >= 75 and ld >= 60:
        cands += [(3, {0: 34, 1: 35, 2: 36, 3: 37, 5: 38}.get(t, 51)), (1, 11), (1, 12), (1, 27), (1, 30)]
        if t == 3:
            cands += [(2, 3), (1, 49), (1, 25)]
        if t == 2:
            cands += [(2, 50)]
        if t == 5:
            cands += [(2, 4), (2, 5), (1, 14)]
    if ld >= 70:
        cands += [(1, 9), (1, 29), (1, 18), (1, 26), (1, 13), (1, 33), (1, 19), (1, 16)]
        if t == 4:
            cands += [(3, 17), (2, 24), (2, 77)]
    if it >= 75:
        cands += [(2, 53), (2, 54), (2, 55), (2, 56), (1, 59), (2, 64), (2, 65), (1, 68), (2, 72), (2, 73), (1, 74)]
    if po >= 70:
        cands += [(2, 80), (2, 81), (2, 82), (2, 83), (2, 84), (2, 85), (2, 86), (1, 87), (1, 88), (2, 77), (2, 78),
                  (1, 95), (1, 96), (1, 97)]
    if ch >= 75:
        cands += [(3, 79), (1, 96), (1, 10), (1, 86)]
    if role in ("后妃",) or (role == "名士" and ch >= 70):
        cands += [(2, 99), (1, 75), (1, 76), (1, 69)]
    if role in ("方士", "僧道", "宗教"):
        cands += [(3, 98), (2, 97), (1, 8), (1, 70)]
    if role in ("外族首领",):
        cands += [(2, 91), (2, 92), (2, 93), (2, 94), (2, 6), (1, 15)]
    if tier >= 2:                                      # 稀有特技
        if st >= 92:
            cands += [(3, 39), (2, 40), (2, 45 if t == 3 else 42 if t == 0 else 43 if t == 1 else 44 if t == 2 else 47 if t == 5 else 41)]
        if it >= 92:
            cands += [(3, 57), (2, 58), (2, 61), (2, 63), (2, 66), (2, 67), (1, 62)]
        if po >= 92:
            cands += [(2, 89), (2, 90), (2, 87), (2, 88)]
        if ld >= 92:
            cands += [(2, 23), (2, 22), (1, 2), (1, 32)]
    has = 36 + 10 * tier + max(0, max(stats) - 60) // 2   # 有特技的概率（%），总体约四成半，和光荣原版相当
    if not cands or rnd(key, "hasskill", 100) >= has:
        return 0xff
    total = sum(w for w, _ in cands)
    r = rnd(key, "skill", total)
    for w, s in cands:
        if r < w:
            return s
        r -= w
    return cands[-1][1]


# ---- 性格类
ROLE_GIRI = {"君主": 1, "宗室": 2, "后妃": 2, "武将": 2, "文臣": 2, "谋士": 2, "宦官": 1, "外戚": 1, "名士": 3,
             "隐士": 3, "方士": 2, "僧道": 3, "宗教": 3, "学者": 3, "外族首领": 1, "叛军首领": 1, "其他": 2}
ROLE_YABOU = {"君主": 4, "宗室": 2, "后妃": 1, "武将": 2, "文臣": 1, "谋士": 2, "宦官": 2, "外戚": 3, "名士": 0,
              "隐士": 0, "方士": 1, "僧道": 0, "宗教": 0, "学者": 0, "外族首领": 3, "叛军首领": 4, "其他": 1}


def personality(key, p):
    stats, role = p["stats"], p.get("role") or "其他"
    ld, st, it, po, ch = stats
    g = ROLE_GIRI.get(role, 2) + (rnd(key, "giri", 3) - 1)
    if p.get("rebel"):
        g -= 1
    y = ROLE_YABOU.get(role, 1) + (rnd(key, "yabou", 3) - 1)
    # 性格 0 小心 1 冷静 2 刚胆 3 莽撞：武力高、智力低偏莽撞；智力高偏冷静
    if st - it >= 25:
        sk = 3 if rnd(key, "sk", 3) else 2
    elif it - st >= 25:
        sk = 1 if rnd(key, "sk", 3) else 0
    else:
        sk = rnd(key, "sk", 4)
    voice = sk if p.get("sex", 0) == 0 else rnd(key, "voice", 4)
    if p.get("sex", 0) == 0:
        tone = 14 if it - st >= 20 else 8 if st - it >= 25 else 15 if g >= 3 else 10
        tone = tone if rnd(key, "tone", 4) else [9, 10, 13, 14, 15, 11][rnd(key, "tone2", 6)]
    else:
        tone = [0, 1, 2, 3, 4, 6][rnd(key, "tonef", 6)]
    kiyou = 3 if g >= 3 else 0 if it >= 80 else rnd(key, "kiyou", 5)
    kanshitsu = p.get("kanshitsu", 1)
    senryaku = 0 if role in ("君主", "叛军首领") and y >= 3 else rnd(key, "senryaku", 4)
    return {"giri": max(0, min(4, g)), "yabou": max(0, min(4, y)), "kiyou": kiyou, "seikaku": sk, "voice": voice,
            "tone": tone, "kanshitsu": kanshitsu, "senryaku": senryaku}


# 光荣「出身地方」（记录 +123）：0 幽州 1 冀州 2 青徐 3 兗豫 4 司隸 5 關中 6 涼州 7 揚州 8 荊北 9 荊南 10 益州 11 南中
HOME = {"幽州": 0, "冀州": 1, "并州": 1, "青州": 2, "徐州": 2, "兗州": 3, "豫州": 3, "司隸": 4, "涼州": 6, "揚州": 7,
        "荊州": 8, "益州": 10, "南中": 11, "交州": 11}

_koei = None


def koei_template(sex, tier, military):
    """语义未明的字节按同性别、同类光荣武将取值（原样照抄一个模板武将）"""
    global _koei
    if _koei is None:
        _koei = [k for k in load_json(SRC / "koei.json") if k["koei_id"] < 670 and k["sei"]]
    pool = [k for k in _koei if k["sex"] == sex and (max(k["stats"][:2]) >= max(k["stats"][2:4])) == military
            and (max(k["stats"]) >= 80) == (tier >= 1) and k["skill"] == 0xff] or [k for k in _koei if k["sex"] == sex]
    return pool


def raw_defaults(key, sex, tier, military):
    pool = koei_template(sex, tier, military)
    t = pool[rnd(key, "tmpl", len(pool))]
    return {"b101": t["b101"], "b125": t["b125"], "b134": t["b134"], "b135": t["b135"], "model": t["model"],
            "b142": t["b142"], "b148": t["b148"], "growth": t["growth"], "template": t["koei_id"]}
