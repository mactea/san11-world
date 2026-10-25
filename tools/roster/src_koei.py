"""光荣原版 0..849 号武将 → data/persons/src/koei.json（去重基准；扩展武将的关系可以指向他们，但他们本身不改）"""
from common import SRC, koei_records, cstr, i16, save_json

# 光荣用自造字码（Big5 解不出）的名字
FIX_NAME = {184: ("孔", "伷"), 229: ("司馬", "伷"), 249: ("朱", "儁"), 616: ("劉", "璝")}
out = []
for i, r in enumerate(koei_records()):
    if r[100] == 0xff and not r[0]:
        continue
    sei, mei = FIX_NAME.get(i, (cstr(r[0:5]), cstr(r[5:10])))
    out.append({
        "koei_id": i, "sei": sei, "mei": mei, "azana": cstr(r[10:15]),
        "face": i16(r, 53), "sex": r[55], "appear": i16(r, 56), "birth": i16(r, 58), "death": i16(r, 60),
        "death_type": r[62], "blood": i16(r, 63), "father": i16(r, 65), "mother": i16(r, 67), "generation": r[69],
        "spouse": i16(r, 70), "sworn": i16(r, 72), "aishou": r[74],
        "liked": [x for x in (i16(r, 75 + 2 * k) for k in range(5)) if x >= 0],
        "disliked": [x for x in (i16(r, 85 + 2 * k) for k in range(5)) if x >= 0],
        "apt": list(r[107:113]), "stats": list(r[113:118]), "growth": list(r[118:123]), "b123": r[123],
        "skill": r[124], "b125": r[125], "giri": r[126], "yabou": r[127], "kiyou": r[128], "seikaku": r[129],
        "voice": r[130], "tone": r[131], "kanshitsu": r[132], "senryaku": r[133], "b134": r[134], "b135": r[135],
        "model": list(r[136:142]), "b142": list(r[142:148]), "b148": r[148], "b101": r[101],
    })
save_json(SRC / "koei.json", out)
print("koei persons", len(out), "named 0..699:", sum(1 for p in out if p["koei_id"] < 700 and p["sei"]))
