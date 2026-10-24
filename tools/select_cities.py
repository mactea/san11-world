"""Pick the new cities' sites at Koei's China city density.

Candidates: data/new_cities.json (east, {name, full, region, lat, lon}), data/cities_west.json
(west, [name, full, region, lat, lon, rank]) and data/cities_more.json (same format: northern and eastern
Europe, the steppe, India, Korea, Japan, South-East Asia; added for the 500-city stage). Rank 1 = provincial capital, 2 = major city, 3 = minor
city or tribal centre. Candidates are placed in rank order; a site is skipped when it lies closer than
--spacing hexes (Koei's China: nearest-neighbour median 21, minimum 12.8) to a city already placed,
including Koei's 42. Each site is snapped to nearby passable land whose 7-hex footprint is free.

Usage: python select_cities.py [world_dir] [--spacing 14] [--max N]   (--max: at most N new cities)
Writes <world_dir>/cities_world.json: [{id, name, full, region, lo, hi, rank}], ids from 42.

       python select_cities.py [world_dir] --move NAME [NAME ...] [--coast]
Moves only the named cities of the existing cities_world.json to their (changed) coordinates in the candidate
lists, keeping every id and every other city as it is (a full run would renumber the cities after a change).
The site is snapped as above; instead of the spacing rule it keeps away from the other cities where it can.
--coast: only sites whose centre hex touches the sea (8), so the city is a seaport whatever its footprint
(searched up to 12 hexes from the coordinates when there is none within 6, distance weighted 6 times as much as
without --coast, since the coordinates are the port itself; sites touching the sea within 2 hexes of the
coordinates' hex win over all others); a big city (tier 1 in data/city_tiers.json, 7 hexes) instead needs all
7 hexes on land and one of them touching the sea, so its footprint never covers sea (a strait, say).
"""
import argparse
import json
from pathlib import Path

import numpy as np

from make_world import COVER, ROAD, SHORE, hex_ring, lonlat_to_hex
from worldgeo import WorldGeo, WorldSpec

HERE = Path(__file__).resolve().parent
SEA_NEAR = 2                    # --move --coast: the city touches the sea this close to its coordinates if it can
DATA = HERE.parent / "data"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("world", nargs="?", default=str(DATA / "world"))
    ap.add_argument("--spacing", type=float, default=14.0)
    ap.add_argument("--max", type=int, default=0, help="at most this many new cities (0 = all that fit)")
    ap.add_argument("--move", nargs="+", metavar="NAME", help="only move these cities of the existing cities_world.json")
    ap.add_argument("--coast", action="store_true", help="with --move: snap to a site at the sea")
    a = ap.parse_args()
    w = Path(a.world)
    info = json.loads((w / "world_info.json").read_text())
    spec = WorldSpec(rows=info["rows"], cols=info["cols"], lon0=info["lon0"], lat0=info["lat0"],
                     china_lo=info["china_lo"], china_hi=info["china_hi"])
    geo = WorldGeo(spec, DATA / "cities_georef.json")
    t = np.load(w / "world_types.npy")
    R, C = t.shape
    koei = json.loads((DATA / "cities_georef.json").read_text(encoding="utf-8"))
    koei_names = {c["name"] for c in koei}

    cand = {}
    for c in json.loads((DATA / "cities_west.json").read_text(encoding="utf-8")):
        name, full, region, lat, lon, rank = c
        cand[name] = {"name": name, "full": full, "region": region, "lat": lat, "lon": lon, "rank": rank}
    more = DATA / "cities_more.json"
    for c in (json.loads(more.read_text(encoding="utf-8")) if more.exists() else []):
        name, full, region, lat, lon, rank = c
        assert name not in cand, name
        cand[name] = {"name": name, "full": full, "region": region, "lat": lat, "lon": lon, "rank": rank}
    for c in json.loads((DATA / "new_cities.json").read_text(encoding="utf-8")):
        if c["name"] not in cand:
            cand[c["name"]] = {**c, "rank": 1}
    cands = [c for c in cand.values() if c["name"] not in koei_names]
    for c in cands:
        assert len(c["name"].encode("big5")) <= 4, c["name"]
    cands.sort(key=lambda c: c["rank"])          # stable: list order within a rank

    lo, x = lonlat_to_hex(geo, np.array([c["lon"] for c in cands]), np.array([c["lat"] for c in cands]))
    placed = [(k["lo"] + spec.china_lo, k["hi"] + spec.china_hi + 0.5 * ((k["lo"] + spec.china_lo) & 1)) for k in koei]
    passable = np.isin(t, COVER + (SHORE, ROAD))
    land = ~np.isin(t, (6, 7, 8))                 # the city footprint is stamped over mountains too
    taken = np.zeros_like(passable)
    taken[spec.china_lo - 4:spec.china_lo + 204, spec.china_hi - 4:spec.china_hi + 204] = True
    if a.move:
        return move_cities(w, a.move, cands, lo, x, placed, passable, land, taken, a.spacing,
                           (t == 8) if a.coast else None)
    out, skipped = [], []
    for c, l, h in zip(cands, lo, x):
        if a.max and len(out) >= a.max:
            skipped.append((c["name"], "over --max")); continue
        y0 = int(round(l)); x0 = int(round(h - 0.5 * (y0 & 1)))
        if not (6 <= y0 < R - 6 and 6 <= x0 < C - 6):
            skipped.append((c["name"], "outside world")); continue
        best = None
        for dy in range(-6, 7):
            for dx in range(-6, 7):
                y, xx = y0 + dy, x0 + dx
                cells = [(y, xx)] + hex_ring(y, xx)
                if any(not (0 <= p < R and 0 <= q < C) or taken[p, q] for p, q in cells):
                    continue
                nland = sum(land[p, q] for p, q in cells)
                if nland < 5:
                    continue
                score = nland * 10 + sum(passable[p, q] for p, q in cells) * 4 - (dy * dy + dx * dx) ** 0.5 + (20 if passable[y, xx] else 0)
                if best is None or score > best[0]:
                    best = (score, y, xx)
        if best is None:                              # no land nearby
            skipped.append((c["name"], "no land")); continue
        _, y, xx = best
        gx = xx + 0.5 * (y & 1)
        d = min(np.hypot(y - p, gx - q) for p, q in placed) if placed else 99
        if d < a.spacing:
            skipped.append((c["name"], f"{d:.0f} hexes from a city")); continue
        placed.append((y, gx))
        for p, q in [(y, xx)] + hex_ring(y, xx):
            taken[p, q] = True
        out.append({"name": c["name"], "full": c["full"], "region": c["region"], "lo": int(y), "hi": int(xx), "rank": c["rank"]})
    for i, c in enumerate(out):
        c["id"] = 42 + i
    (w / "cities_world.json").write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"{len(out)} cities placed (+42 Koei = {len(out) + 42}), {len(skipped)} skipped")
    by = {}
    for n, why in skipped:
        by.setdefault(why.split(' ')[-1] if 'from' in why else why, []).append(n)
    for k, v in by.items():
        print(f"  skipped ({k}): {' '.join(v)}")


def move_cities(w, names, cands, lo, x, placed, passable, land, taken, spacing, sea=None):
    """re-snap the named cities of cities_world.json to their candidate coordinates (ids unchanged); sea: only
    sites whose centre touches it"""
    R, C = taken.shape
    cur = json.loads((w / "cities_world.json").read_text(encoding="utf-8"))
    where = {c["name"]: (l, h) for c, l, h in zip(cands, lo, x)}
    tf = DATA / "city_tiers.json"
    tier = {r["id"]: r["tier"] for r in json.loads(tf.read_text(encoding="utf-8"))} if tf.exists() else {}
    for c in cur:
        if c["name"] not in names:
            placed.append((c["lo"], c["hi"] + 0.5 * (c["lo"] & 1)))
            for p, q in [(c["lo"], c["hi"])] + hex_ring(c["lo"], c["hi"]):
                taken[p, q] = True
    for nm in names:
        c = next((c for c in cur if c["name"] == nm), None)
        if c is None or nm not in where:
            raise SystemExit(f"{nm}: not in cities_world.json or in the candidate lists")
        l, h = where[nm]
        y0 = int(round(l)); x0 = int(round(h - 0.5 * (y0 & 1)))
        best = None
        for reach in ((6, 12) if sea is not None else (6,)):
            if best is not None:
                break
            for dy in range(-reach, reach + 1):
                for dx in range(-reach, reach + 1):
                    best = snap(y0 + dy, x0 + dx, dy, dx, best, R, C, taken, land, passable, placed, spacing, sea,
                                tier.get(c["id"], 1) <= 1, (y0, x0))
        if best is None:
            raise SystemExit(f"{nm}: no free land site near its coordinates" + (" at the sea" if sea is not None else ""))
        _, y, xx, d, _ = best
        print(f"{nm} (id {c['id']}): ({c['lo']}, {c['hi']}) -> ({y}, {xx}), {d:.1f} hexes from the nearest city"
              + (f" (less than --spacing {spacing:g})" if d < spacing else ""))
        c["lo"], c["hi"] = int(y), int(xx)
        placed.append((y, xx + 0.5 * (y & 1)))
        for p, q in [(y, xx)] + hex_ring(y, xx):
            taken[p, q] = True
    (w / "cities_world.json").write_text(json.dumps(cur, ensure_ascii=False, indent=1), encoding="utf-8")


def hex_dist(a, b):
    (q0, r0), (q1, r1) = [(x - (y - (y & 1)) // 2, y) for y, x in (a, b)]
    return max(abs(q0 - q1), abs(r0 - r1), abs(q0 + r0 - q1 - r1))


def snap(y, xx, dy, dx, best, R, C, taken, land, passable, placed, spacing, sea, big=False, target=None):
    """score the site (y, xx) for move_cities; returns the better of it and best (big: a 7-hex city; target: the
    coordinates' hex, sea must be touched within SEA_NEAR hexes of it while one such site exists)"""
    cells = [(y, xx)] + hex_ring(y, xx)
    if any(not (0 <= p < R and 0 <= q < C) or taken[p, q] for p, q in cells):
        return best
    if sea is not None:
        if big:
            if not all(land[p, q] for p, q in cells):
                return best
            touch = {n for p, q in cells for n in hex_ring(p, q) if 0 <= n[0] < R and 0 <= n[1] < C and sea[n]}
        else:
            touch = {h for h in cells[1:] if sea[h]}
        if not touch:
            return best
        near_port = min(hex_dist(h, target) for h in touch) <= SEA_NEAR
        if best is not None and best[4] and not near_port:
            return best
        if best is not None and near_port and not best[4]:
            best = None
    nland = sum(land[p, q] for p, q in cells)
    if nland < (4 if sea is not None else 5):
        return best
    d = min(np.hypot(y - p, xx + 0.5 * (y & 1) - q) for p, q in placed)
    # at the sea the coordinates are the port itself: staying close to them counts more than dry land around
    near = 6.0 if sea is not None else 1.0
    score = (nland * 10 + sum(passable[p, q] for p, q in cells) * 4 - near * (dy * dy + dx * dx) ** 0.5
             + (20 if passable[y, xx] else 0) + 0.5 * min(d, spacing))
    return (score, y, xx, d, sea is not None and near_port) if best is None or score > best[0] else best


if __name__ == "__main__":
    main()
