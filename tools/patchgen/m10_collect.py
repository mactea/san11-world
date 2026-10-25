"""M10 phase 0: collect every candidate site for growing the officer (武将) table 1100 -> P.

Usage: python m10_collect.py <pk.asm from disasm.py> <out dir>. Output: <out dir>/sites.jsonl, one object per instruction with the reasons
it was picked, and a summary. Categories:
  abs    absolute address inside the officer array [PBASE-0x10, PEND+0x190) (array base, end, fields of person 0..)
  disp   displacement 0xc0bc (world+0xC0BC = officer array) or a folded world+0xC0BC+k with k < 0x190
  stride imul/lea by 0x190 (officer record size) and the /0x190 magic for pointer -> id
  count  officer-count constants: 1100/1099/1101, 850/849/851, 1000/999 (shared with units), 800/799, 150/149,
         700/699/670, and byte sizes of officer-indexed arrays (1100*k, 850*k, 150*k for k in 2,4,8,12,16,0x14,0x18)
"""
import collections
import json
import re
import sys
from pathlib import Path

ASM = Path(sys.argv[1])
HERE = Path(sys.argv[2])
PBASE, PSIZE, PCOUNT = 0x720DA14, 0x190, 1100
PEND = PBASE + PCOUNT * PSIZE          # 0x72790d4 (treasures start here)
WOFF = 0xC0BC

counts = {1100: "1100", 1099: "1099", 1101: "1101", 850: "850", 849: "849", 851: "851",
          1000: "1000", 999: "999", 800: "800", 799: "799", 150: "150", 149: "149", 700: "700", 699: "699",
          670: "670"}
for n, tag in ((1100, "1100"), (850, "850")):
    for k in (2, 4, 8, 12, 16, 0x14, 0x18, 0x20):
        counts.setdefault(n * k, f"{tag}*{k}")
counts.setdefault(PCOUNT * PSIZE, "1100*0x190")
counts.setdefault(850 * 152, "850*152 (scenario)")
counts.setdefault(1100 * 203, "1100*203 (save)")

HEXRE = re.compile(r"0x[0-9a-f]+")
out = []
summ = collections.Counter()
prev = None
for line in open(ASM, encoding="utf-8"):
    addr = int(line[:8], 16)
    rest = line[30:].strip() if len(line) > 30 else ""
    parts = rest.split(None, 1)
    mn = parts[0] if parts else ""
    ops = parts[1] if len(parts) > 1 else ""
    why = []
    for tok in HEXRE.findall(ops):
        v = int(tok, 16)
        if PBASE - 0x10 <= v < PEND + PSIZE:
            why.append(f"abs {v - PBASE:+#x}")
        if v == WOFF or (WOFF < v < WOFF + PSIZE and "[" in ops):
            why.append(f"disp world+{v:#x}")
        if v == PSIZE and mn in ("imul", "lea", "add", "sub", "cmp", "mov") and ("imul" in mn or "add" in mn or "sub" in mn):
            why.append("stride 0x190")
        if v in counts and mn in ("cmp", "mov", "push", "lea", "sub", "add", "imul", "and", "test") and not ops.startswith("dword ptr [esp") or (v in counts and mn == "cmp"):
            why.append(f"count {counts[v]}")
    if mn == "imul" and ", 0x190" in ops:
        why.append("stride 0x190")
    if "0x51eb851f" in ops:
        why.append("magic /100 or /400")
    if why:
        rec = {"addr": f"{addr:#x}", "insn": f"{mn} {ops}", "why": sorted(set(why))}
        out.append(rec)
        for w in rec["why"]:
            summ[w.split()[0] + " " + (w.split()[1] if w.startswith("count") else "")] += 1
with open(HERE / "sites.jsonl", "w", encoding="utf-8") as f:
    for r in out:
        f.write(json.dumps(r, ensure_ascii=False) + "\n")
print("sites", len(out))
for k, v in sorted(summ.items(), key=lambda kv: -kv[1]):
    print(f"{v:6d}  {k}")
