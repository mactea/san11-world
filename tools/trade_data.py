"""M13 trade data: data/trade_goods.json + the city list -> trade.bin (worldmod trade.cpp, M13_TRADE.md).

Usage:
  python tools/trade_data.py [--out <dir>/trade.bin] [--cities data/world/cities_world.json]
                             [--china-x 436 --china-y 1234] [--report]

Inputs
  data/trade_goods.json        goods, producers, price curves, market groups, hubs (data, documented in the file)
  data/cities_georef.json      Koei's 42 cities (China-local lo / hi, shifted by --china-y / --china-x)
  data/world/cities_world.json the other cities (id, region, world lo / hi)
  data/city_tiers.json         tier and port flag of the other cities

trade.bin (little endian)
  "TRDB" u32 version=1, u32 C, u32 G, u32 0
  G x { char name[16] utf-8, u32 base, u32 0 }
  C x { i16 lo, i16 hi, u8 flags (1 hub, 2 port, 4 Koei China), u8 group (0 = none), u16 market size, char name[16] utf-8 }
  C x G u32 price      gold per load with the price index at 1000
  C x G u8  supply     producer level 0..3 (0: the city does not sell the good unless it is a hub)
"""
import argparse
import json
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def cube(lo, hi):
    """odd-r layout (row = lo, odd rows shifted to +hi) -> cube q, r"""
    return hi - (lo - (lo & 1)) // 2, lo


def hexdist(a, b):
    q1, r1 = cube(*a)
    q2, r2 = cube(*b)
    dq, dr = q1 - q2, r1 - r2
    return (abs(dq) + abs(dr) + abs(dq + dr)) // 2


def name16(s):
    b = s.encode("utf-8")
    while len(b) > 15:                      # cut at a character boundary
        s = s[:-1]
        b = s.encode("utf-8")
    return b + b"\0" * (16 - len(b))


def load_cities(args):
    koei = json.load(open(ROOT / "data/cities_georef.json", encoding="utf-8"))
    world = json.load(open(args.cities, encoding="utf-8"))
    tiers = {t["id"]: t for t in json.load(open(ROOT / "data/city_tiers.json", encoding="utf-8"))}
    gd = json.load(open(args.goods, encoding="utf-8"))
    zhou = gd["china_zhou"]
    cities = {}
    for k in koei:
        cid = k["id"]
        cities[cid] = dict(id=cid, name=k["name"], lo=k["lo"] + args.china_y, hi=k["hi"] + args.china_x,
                           region=zhou[str(cid)], tier=None, port=cid in gd["koei_ports"], koei=True)
    for w in world:
        t = tiers.get(w["id"], {})
        cities[w["id"]] = dict(id=w["id"], name=w["name"], lo=w["lo"], hi=w["hi"], region=w["region"],
                               tier=t.get("tier", w.get("rank", 3)), port=bool(t.get("port")), koei=False)
    C = max(cities) + 1
    missing = [i for i in range(C) if i not in cities]
    if missing:
        sys.exit(f"cities without data: {missing[:20]}")
    return [cities[i] for i in range(C)], gd


def build(args):
    cities, gd = load_cities(args)
    goods = gd["goods"]
    C, G = len(cities), len(goods)
    pm, mk = gd["price_model"], gd["market"]
    per = pm["per_hexes"]
    lvf = {int(k): v for k, v in pm["level_factor"].items()}
    groups = [g for g in gd["groups"] if g != "about"]
    region_group = {}
    for gi, g in enumerate(groups):
        for r in gd["groups"][g]:
            region_group[r] = gi + 1
    hubs = {int(k) for k in gd["hubs"]["cities"]}

    supply = [[0] * G for _ in range(C)]
    for g, good in enumerate(goods):
        for p in good["producers"]:
            lvl = p["level"]
            for c in cities:
                hit = ("city" in p and c["id"] == p["city"]) or ("zhou" in p and c["koei"] and c["region"] == p["zhou"]) \
                      or ("region" in p and c["region"] == p["region"]) \
                      or ("zhou" in p and not c["koei"] and c["region"] == p["zhou"])
                if hit:
                    supply[c["id"]][g] = max(supply[c["id"]][g], lvl)

    price = [[0] * G for _ in range(C)]
    for g, good in enumerate(goods):
        prod = [(c, supply[c["id"]][g]) for c in cities if supply[c["id"]][g]]
        if not prod:
            sys.exit(f"good {good['key']} has no producer")
        group_demand = {groups.index(k) + 1: v for k, v in good.get("demand", {}).items()}
        for c in cities:
            best = None
            for p, lvl in prod:
                d = hexdist((c["lo"], c["hi"]), (p["lo"], p["hi"]))
                m = min(good["cap"], (1 + d / per) ** good["exp"])
                v = good["base"] * lvf[lvl] * m
                best = v if best is None or v < best else best
            dem = group_demand.get(region_group.get(c["region"], 0), 1.0)
            if supply[c["id"]][g]:          # at a producer: its own price, no demand markup
                dem = 1.0
            price[c["id"]][g] = max(1, int(round(best * dem)))

    out = bytearray()
    out += b"TRDB" + struct.pack("<IIII", 1, C, G, 0)
    for good in goods:
        out += name16(good["name"]) + struct.pack("<II", good["base"], 0)
    for c in cities:
        flags = (1 if c["id"] in hubs else 0) | (2 if c["port"] else 0) | (4 if c["koei"] else 0)
        size = mk["size_koei"] if c["koei"] else mk["size_tier"][str(c["tier"])]
        if c["id"] in hubs:
            size = int(size * mk["hub_size_factor"])
        out += struct.pack("<hhBBH", c["lo"], c["hi"], flags, region_group.get(c["region"], 0), size) + name16(c["name"])
    for c in range(C):
        out += struct.pack(f"<{G}I", *price[c])
    for c in range(C):
        out += bytes(supply[c])
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_bytes(out)
    print(f"trade_data: {C} cities, {G} goods, {len(hubs)} hubs -> {args.out} ({len(out)} bytes)")
    if args.report:
        report(cities, goods, price, supply)


def report(cities, goods, price, supply):
    byname = {c["name"]: c["id"] for c in cities}
    show = ["成都", "洛陽", "長安", "敦煌", "疏勒", "木鹿", "斯賓", "帕爾", "安都", "黎軒", "羅馬", "婆盧", "穆濟", "番禺", "合浦", "沙布"]
    print("price per load (gold) at selected cities; * = producer")
    print("        " + "".join(f"{n:>6}" for n in show))
    for g, good in enumerate(goods):
        row = ""
        for n in show:
            c = byname[n]
            row += f"{price[c][g]:>5}{'*' if supply[c][g] else ' '}"
        print(f"{good['name']:<4}{good['key']:<4}" + row)
    s = goods.index(next(g for g in goods if g["key"] == "silk"))
    p = goods.index(next(g for g in goods if g["key"] == "pepper"))
    print(f"silk Rome / Luoyang {price[byname['羅馬']][s] / price[byname['洛陽']][s]:.1f}x; "
          f"pepper Rome / Muziris {price[byname['羅馬']][p] / price[byname['穆濟']][p]:.1f}x")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--goods", default=str(ROOT / "data/trade_goods.json"))
    ap.add_argument("--cities", default=str(ROOT / "data/world/cities_world.json"))
    ap.add_argument("--out", default=str(ROOT / "data/world/trade.bin"))
    ap.add_argument("--china-x", type=int, default=436)
    ap.add_argument("--china-y", type=int, default=1234)
    ap.add_argument("--report", action="store_true")
    build(ap.parse_args())


if __name__ == "__main__":
    main()
