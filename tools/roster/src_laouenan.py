"""Laouenan 等「全球知名人物交叉核实数据库」(Wikidata/Wikipedia 派生, 2022) → data/persons/src/laouenan.json

取在世时间与 185..355 年重叠的人（生卒年缺一项时按活 60 年估）。"""
import csv
import gzip
from common import CACHE, SRC, save_json

csv.field_size_limit(10 ** 8)
KEEP = ["wikidata_code", "name", "birth", "death", "birth_min", "birth_max", "death_min", "death_max",
        "birth_estimation", "death_estimation", "gender", "level1_main_occ", "level2_main_occ", "level3_main_occ",
        "level3_all_occ", "un_subregion", "un_region", "citizenship_1_b", "citizenship_2_b", "string_citizenship_raw_d",
        "area1_of_rattachment", "area2_of_rattachment", "list_areas_of_rattach", "number_wiki_editions",
        "wiki_readers_2015_2018", "ranking_visib_5criteria", "sum_visib_ln_5criteria", "list_wikipedia_editions",
        "bpla1", "bplo1", "dpla1", "dplo1", "curid"]


def num(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


out = []
with gzip.open(CACHE / "laouenan.csv.gz", "rt", encoding="utf-8", errors="replace") as f:
    for r in csv.DictReader(f):
        b = num(r["birth"]) or num(r["birth_estimation"])
        d = num(r["death"]) or num(r["death_estimation"])
        if b is None and d is None:
            continue
        b2 = b if b is not None else d - 60
        d2 = d if d is not None else b + 60
        if b2 <= 355 and d2 >= 185:
            e = {k: r.get(k, "") for k in KEEP}
            e["b"], e["d"] = b, d
            out.append(e)
out.sort(key=lambda e: num(e["ranking_visib_5criteria"]) or 9e9)
save_json(SRC / "laouenan.json", out)
print("laouenan period", len(out))
