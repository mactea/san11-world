"""已经收进名单的海外人物（全球名人库批次 raw/gl_*.jsonl、维基批次 raw/wk_*.jsonl、希腊拉丁史书抽取）
→ data/persons/_cache/known_world.tsv，给读希腊/拉丁史书的子代理查重、统一译名用

每行：英文名 | 中文名 | 完整中文名 | 生年 | 卒年 | 所属"""
import glob
from common import CACHE, RAW, SRC, load_json, load_jsonl

names = {e["wikidata_code"]: e["name"].replace("_", " ") for e in load_json(SRC / "laouenan.json")}
rows = set()
for f in sorted(glob.glob(str(RAW / "gl_*.jsonl"))) + sorted(glob.glob(str(RAW / "wk_*.jsonl"))):
    for e in load_jsonl(f):
        if e.get("skip") or not e.get("zh"):
            continue
        en = e.get("title") or names.get(e.get("qid"), "")
        rows.add(" | ".join(str(x if x is not None else "") for x in
                            [en, e["zh"], e.get("zh_full", ""), e.get("birth"), e.get("death"), e.get("polity", "")]))
for p in ("dio", "hdn", "ha", "eus", "zos", "amm"):                      # 希腊、拉丁史书抽出的人（SPEC_CLASSIC）
    for f in sorted(glob.glob(str(RAW / f"{p}_*.jsonl"))):
        for e in load_jsonl(f):
            if e.get("zh") and e.get("name_en"):
                y = e.get("years") or [None, None]
                rows.add(" | ".join(str(x if x is not None else "") for x in
                                    [e["name_en"], e["zh"], e.get("zh_full", ""), e.get("birth") or y[0],
                                     e.get("death") or y[1], e.get("polity", "")]))
(CACHE / "known_world.tsv").write_text("\n".join(sorted(rows)) + "\n", encoding="utf-8")
print("known_world", len(rows))
