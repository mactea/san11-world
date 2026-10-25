"""三国群雄志扩容名单 roster.mjs 和三国志11 新武将文件 TPrsn → data/persons/src/qxz.json、tprsn.json"""
import json
import re
from common import QXZ_ROSTER, TPRSN, SRC, s2t, save_json

text = QXZ_ROSTER.read_text(encoding="utf-8")
body = text[text.index("`") + 1:text.rindex("`")]
out, group = [], ""
for line in body.splitlines():
    line = line.strip()
    if not line:
        continue
    if line.startswith("#"):
        group = s2t(line.strip("# =").strip())
        continue
    f = line.split("|")
    f += [""] * (8 - len(f))
    slug, name, azana, birth, death, tier, lean, skill = f[:8]
    romance = "（演义）" in name or "（演義）" in name
    name = re.sub(r"（.*?）", "", name)
    out.append({"slug": slug, "name": s2t(name), "azana": s2t(azana), "birth": int(birth) if birth else None,
                "death": int(death) if death else None, "tier": tier, "lean": lean, "skill": skill,
                "romance": romance, "group": group})
save_json(SRC / "qxz.json", out)
print("qxz", len(out), "romance", sum(e["romance"] for e in out))

CJK = re.compile(r"[一-鿿]")

tp = []
for e in json.loads(TPRSN.read_text(encoding="utf-8")):
    # 名字字段里夹着 \0 和半个字节：只保留汉字
    sei = "".join(ch for ch in e["surname"] if CJK.match(ch))
    mei = "".join(ch for ch in e["given_name"] if CJK.match(ch))
    az = "".join(ch for ch in e["courtesy_name"] if CJK.match(ch))
    tp.append({"sei": sei, "mei": mei, "azana": az, "file": e.get("_source_filename_stem")})
save_json(SRC / "tprsn.json", tp)
print("tprsn", len(tp), [t["sei"] + t["mei"] for t in tp[:30]])
