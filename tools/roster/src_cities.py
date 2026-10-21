"""530 城（光荣 42 + 新 488）的编号、名字、地区、经纬度 → data/persons/src/cities.json（主目录的城市表只读）"""
import json
from common import ROOT, MAIN_REPO, SRC, save_json

georef = json.loads((ROOT / "data" / "cities_georef.json").read_text(encoding="utf-8"))
world = json.loads((MAIN_REPO / "data" / "world" / "cities_world.json").read_text(encoding="utf-8"))
ll = {}
for f in ["cities_west.json", "cities_more.json"]:
    for name, full, region, lat, lon, *_ in json.loads((ROOT / "data" / f).read_text(encoding="utf-8")):
        ll.setdefault(full, (lat, lon))
        ll.setdefault(name, (lat, lon))
for e in json.loads((ROOT / "data" / "new_cities.json").read_text(encoding="utf-8")):
    ll.setdefault(e["full"], (e["lat"], e["lon"]))
    ll.setdefault(e["name"], (e["lat"], e["lon"]))
# 新加的 30 座中国郡城，原表没有经纬度：按郡治今址取近似值
CN_NEW = {"蒼梧(廣信)": (23.48, 111.28), "鬱林(布山)": (23.10, 109.60), "合浦": (21.66, 109.20), "珠崖": (19.90, 110.30),
          "臨賀": (24.40, 111.55), "始興(曲江)": (24.80, 113.60), "永昌(不韋)": (25.12, 99.17), "興古": (24.10, 104.20),
          "越巂(邛都)": (27.89, 102.26), "牂柯(且蘭)": (26.60, 107.50), "巴西(閬中)": (31.58, 105.97),
          "建安": (27.04, 118.32), "豫章(南昌)": (28.68, 115.89), "臨川": (27.95, 116.36), "南康(雩都)": (25.95, 115.41),
          "丹陽(宛陵)": (30.95, 118.76), "東陽(長山)": (29.08, 119.65), "新都(始新)": (29.60, 119.04), "衡陽": (27.90, 112.60),
          "昭陵": (27.24, 111.47), "宜都(夷道)": (30.39, 111.45), "揭陽": (23.55, 116.37), "博羅": (23.17, 114.29),
          "高涼": (21.90, 111.40), "象林(林邑)": (15.88, 108.33), "夷洲(臺灣)": (23.70, 120.96), "侯官(東冶)": (26.08, 119.30),
          "東安": (31.70, 120.30), "永寧(東甌)": (28.02, 120.65), "臨海(章安)": (28.85, 121.38)}
# 光荣 42 城所属州（光荣原版）
KOEI_STATE = ["幽州", "幽州", "幽州", "冀州", "青州", "并州", "冀州", "青州", "徐州", "徐州", "揚州", "兗州", "兗州", "豫州",
              "豫州", "司隸", "荊州", "司隸", "荊州", "涼州", "涼州", "涼州", "揚州", "揚州", "揚州", "揚州", "揚州", "荊州",
              "荊州", "荊州", "荊州", "荊州", "荊州", "荊州", "荊州", "益州", "益州", "益州", "益州", "益州", "南中", "南中"]
out = []
for k in georef:
    out.append({"id": k["id"], "name": k["name"], "full": k["name"], "region": KOEI_STATE[k["id"]],
                "lat": k["lat"], "lon": k["lon"], "koei": True})
missing = []
for c in world:
    p = ll.get(c["full"]) or ll.get(c["name"]) or CN_NEW.get(c["full"])
    if not p:
        missing.append(c["full"])
    out.append({"id": c["id"], "name": c["name"], "full": c["full"], "region": c["region"],
                "lat": p[0] if p else None, "lon": p[1] if p else None, "koei": False})
out.sort(key=lambda e: e["id"])
save_json(SRC / "cities.json", out)
print("cities", len(out), "without lat/lon", len(missing), missing[:20])
