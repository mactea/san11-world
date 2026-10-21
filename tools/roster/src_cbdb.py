"""CBDB（中国历代人物传记资料库, Harvard/中研院/北大, 2026-10-17 版）→ data/persons/src/cbdb.json

取东汉末到东晋十六国：年份可知的按指数年/生卒年与 140..380 重叠；年份不详的只取三国、西晋和十六国诸政权的人。"""
import sqlite3
from collections import defaultdict
from common import CACHE, SRC, save_json

db = sqlite3.connect(CACHE / "cbdb_20261017.sqlite3")
c = db.cursor()
DY_LATE = {3, 23, 26, 39, 42, 51, 53, 64, 65, 69, 70, 82, 87}        # 年份不详也收
DY_ANY = DY_LATE | {2, 25, 27, 45, 63, 72, 73, 76, 83, 0}             # 有年份时按年份筛
dyn = dict(c.execute("select c_dy, c_dynasty_chn from DYNASTIES"))
rows = c.execute("select c_personid, c_name_chn, c_female, c_birthyear, c_deathyear, c_index_year, c_dy, "
                 "c_fl_earliest_year, c_fl_latest_year, c_index_addr_id, c_notes, c_surname_chn, c_mingzi_chn "
                 "from BIOG_MAIN").fetchall()
keep = {}
for pid, name, fem, by, dy_, iy, d, fe, fl, addr, notes, sur, ming in rows:
    if d not in DY_ANY or not name:
        continue
    yrs = [y for y in (by, dy_, iy, fe, fl) if y]
    if yrs:
        if not (min(yrs) <= 380 and max(yrs) >= 140):
            continue
    elif d not in DY_LATE:
        continue
    keep[pid] = {"cbdb_id": pid, "name": name, "sei": sur or "", "mei": ming or "", "female": bool(fem),
                 "birth": by or None, "death": dy_ or None, "index_year": iy or None, "fl": [fe or None, fl or None],
                 "dynasty": dyn.get(d, ""), "addr_id": addr, "notes": notes or ""}
ids = list(keep)
q = lambda sql: c.execute(sql.replace("IDS", ",".join(map(str, ids)))).fetchall()
for pid, alt, t in q("select c_personid, c_alt_name_chn, c_alt_name_type_code from ALTNAME_DATA where c_personid in (IDS)"):
    if t == 4 and alt:
        keep[pid].setdefault("azana", alt)
addr = dict(c.execute("select c_addr_id, c_name_chn from ADDR_CODES"))
for pid, a, t in q("select c_personid, c_addr_id, c_addr_type from BIOG_ADDR_DATA where c_personid in (IDS)"):
    keep[pid].setdefault("addr", []).append(addr.get(a, ""))
for e in keep.values():
    if e["addr_id"] and not e.get("addr"):
        e["addr"] = [addr.get(e["addr_id"], "")]
kin = {k: (r, alt) for k, r, alt in c.execute("select c_kincode, c_kinrel_chn, c_kinrel from KINSHIP_CODES")}
for pid, kid, kc in q("select c_personid, c_kin_id, c_kin_code from KIN_DATA where c_personid in (IDS)"):
    keep[pid].setdefault("kin", []).append([kid, kin.get(kc, ("", ""))[0], kin.get(kc, ("", ""))[1]])
off = {o: n for o, n in c.execute("select c_office_id, c_office_chn from OFFICE_CODES")}
for pid, o, fy in q("select c_personid, c_office_id, c_firstyear from POSTED_TO_OFFICE_DATA where c_personid in (IDS)"):
    keep[pid].setdefault("offices", []).append([off.get(o, ""), fy or None])
status = dict(c.execute("select c_status_code, c_status_desc_chn from STATUS_CODES"))
for pid, s in q("select c_personid, c_status_code from STATUS_DATA where c_personid in (IDS)"):
    keep[pid].setdefault("status", []).append(status.get(s, ""))
texts = dict(c.execute("select c_textid, c_title_chn from TEXT_CODES"))
for pid, t, pg in q("select c_personid, c_textid, c_pages from BIOG_SOURCE_DATA where c_personid in (IDS)"):
    keep[pid].setdefault("sources", []).append(f"{texts.get(t, '')}{' ' + pg if pg else ''}")
out = sorted(keep.values(), key=lambda e: e["cbdb_id"])
save_json(SRC / "cbdb.json", out)
by = defaultdict(int)
for e in out:
    by[e["dynasty"]] += 1
print("cbdb", len(out), dict(sorted(by.items(), key=lambda x: -x[1])))
