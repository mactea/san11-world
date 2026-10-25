"""中华三国志（ZHSan）史实剧本 → data/persons/src/zhsan.json

各剧本按人物 ID 合并（同一 ID 在各剧本里是同一个人），取第一次出现的数据；列传取 AllBiographies。
ID 0..669 与光荣原版编号一致。"""
import json
from common import ZHSAN_DIR, ZHSAN_HIST, SRC, s2t, save_json

FIELDS = ["SurName", "GivenName", "CalledName", "Sex", "YearBorn", "YearAvailable", "YearDead", "DeadReason",
          "BaseCommand", "BaseStrength", "BaseIntelligence", "BasePolitics", "BaseGlamour", "BaseBraveness",
          "BaseCalmness", "PCharacter", "Ambition", "PersonalLoyalty", "Ideal", "IdealTendencyIDString",
          "StrategyTendency", "Generation", "BornRegion", "AvailableLocation", "PictureIndex", "Qualification",
          "Tags", "RealTitlesString", "UniqueMilitaryKindsString"]


def kv(lst):
    return {e["Key"]: e["Value"] for e in lst}


persons = {}
for sc in ZHSAN_HIST:
    d = json.loads((ZHSAN_DIR / f"{sc}.json").read_text(encoding="utf-8-sig"), strict=False)
    bios = kv(d["AllBiographies"]["Biographys"])
    rel = {k: kv(d[k]) for k in ["FatherIds", "MotherIds", "SpouseIds", "BrotherIds", "CloseIds", "HatedIds"]}
    for p in d["Persons"]["GameObjects"]:
        pid = p["ID"]
        if pid in persons:
            continue
        e = {"zh_id": pid, "scen": sc}
        for f in FIELDS:
            e[f] = p.get(f)
        b = bios.get(pid) or {}
        e["brief"] = (b.get("Brief") or "").strip()
        e["history"] = (b.get("History") or "").strip()
        e["romance"] = (b.get("Romance") or "").strip()
        e["father"] = rel["FatherIds"].get(pid, -1)
        e["mother"] = rel["MotherIds"].get(pid, -1)
        e["spouse"] = rel["SpouseIds"].get(pid, -1)
        e["brothers"] = rel["BrotherIds"].get(pid, [])
        e["close"] = rel["CloseIds"].get(pid, [])
        e["hated"] = rel["HatedIds"].get(pid, [])
        persons[pid] = e
out = []
for pid in sorted(persons):
    e = persons[pid]
    for f in ["SurName", "GivenName", "CalledName", "brief", "history", "romance"]:
        e[f] = s2t(e[f] or "").replace("\r", "").strip()
    out.append(e)
save_json(SRC / "zhsan.json", out)
print("zhsan persons", len(out), "ids", out[0]["zh_id"], "..", out[-1]["zh_id"])
