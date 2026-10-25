"""Syriaca.org 人物库（TEI，CC BY 4.0，GitHub srophe/syriaca-data）→ 170–370 年在世的人 → 子代理用的块 chunks/syr_NN.txt

先取数据（只要 data/persons，约 3000 个文件）：
    git clone --depth 1 --filter=blob:none --no-checkout https://github.com/srophe/syriaca-data.git data/persons/_cache/syriaca
    cd data/persons/_cache/syriaca && git sparse-checkout set data/persons && git checkout

    python split_syriaca.py [每块条数，默认 60]"""
import glob
import re
import sys
import xml.etree.ElementTree as ET
from common import CACHE

N = int(sys.argv[1]) if len(sys.argv) > 1 else 60
OUT = CACHE / "chunks"
TEI = "{http://www.tei-c.org/ns/1.0}"
SRO = "{https://srophe.app}"
XML = "{http://www.w3.org/XML/1998/namespace}"


def years(el):
    """一个 floruit/birth/death 里的日期 → (起, 止)"""
    out = []
    for d in el.iter(TEI + "date"):
        a = d.get(SRO + "computed-start") or d.get("when") or d.get("notBefore") or d.get("from")
        b = d.get(SRO + "computed-end") or d.get("when") or d.get("notAfter") or d.get("to")
        ya = int(re.match(r"-?\d+", a).group()) if a and re.match(r"-?\d+", a) else None
        yb = int(re.match(r"-?\d+", b).group()) if b and re.match(r"-?\d+", b) else None
        if ya is not None or yb is not None:
            out.append((ya if ya is not None else yb, yb if yb is not None else ya))
    return out


def text(el):
    return re.sub(r"\s+", " ", "".join(el.itertext())).strip()


def person(f):
    p = ET.parse(f).getroot().find(f".//{TEI}person")
    if p is None:
        return None
    spans = {k: [y for e in p.findall(TEI + k) for y in years(e)] for k in ("floruit", "birth", "death")}
    allspan = [y for v in spans.values() for y in v]
    if not allspan:
        return None
    lo, hi = min(a for a, b in allspan), max(b for a, b in allspan)
    if spans["death"]:
        hi = max(b for a, b in spans["death"])
    if spans["birth"]:
        lo = min(a for a, b in spans["birth"])
    if hi < 170 or lo > 370 or (spans["birth"] and not spans["floruit"] and lo > 355):
        return None
    head = [text(n) for n in p.findall(TEI + "persName") if n.get(XML + "lang") == "en"]
    syr = [text(n) for n in p.findall(TEI + "persName") if (n.get(XML + "lang") or "").startswith("syr")][:1]
    notes = [text(n) for n in p.findall(TEI + "note") if n.get("type") in ("abstract", "description")]
    places = sorted({text(n) for k in ("birth", "death", "residence") for e in p.findall(TEI + k)
                     for n in e.iter(TEI + "placeName")})
    sex = [text(g) for g in p.findall(TEI + "gender")][:1]
    fmt = lambda k: "; ".join(f"{a}–{b}" if a != b else f"{a}" for a, b in spans[k])
    pid = re.search(r"(\d+)\.xml$", f).group(1)
    return (f"[syriaca person {pid}] {head[0] if head else '?'}"
            + (f" | also: {', '.join(dict.fromkeys(head[1:6]))}" if len(head) > 1 else "")
            + (f" | syr: {syr[0]}" if syr else "") + f" | {p.get('ana', '')}"
            + (f" | floruit {fmt('floruit')}" if spans["floruit"] else "") + (f" | born {fmt('birth')}" if spans["birth"] else "")
            + (f" | died {fmt('death')}" if spans["death"] else "") + (f" | {sex[0]}" if sex else "")
            + (f" | places: {', '.join(places)}" if places else "") + "\n  " + " / ".join(notes))


def main():
    rows = [r for f in sorted(glob.glob(str(CACHE / "syriaca" / "data" / "persons" / "tei" / "*.xml")),
                              key=lambda s: int(re.search(r"(\d+)\.xml$", s).group(1)))
            if (r := person(f))]
    for f in OUT.glob("syr_*.txt"):
        f.unlink()
    for i in range(0, len(rows), N):
        (OUT / f"syr_{i // N:02d}.txt").write_text(
            "# Syriaca.org persons (CC BY 4.0) alive 170-370, one record per block\n" + "\n".join(rows[i:i + N]),
            encoding="utf-8")
    print("syriaca", len(rows), "persons,", (len(rows) + N - 1) // N, "chunks")


if __name__ == "__main__":
    main()
