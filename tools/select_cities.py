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

       python select_cities.py [world_dir] --replace
Snaps every city of the existing cities_world.json again to its coordinates, keeping every id, name and order
(after a change of the projection, tools/worldgeo.py, or of the terrain). A city keeps its hex when that is still a
good site near the (new) target: a bonus of KEEP_BONUS for its current hex within 6 hexes of the target, so cities
whose target did not move stay put. New cities inside Koei's China block, and cities not in the candidate lists
(placed by hand), keep their hexes. Cities in the order of
the list; the current footprints of cities not yet placed whose site can stay are reserved. Extra rules:
  * a port (data/city_tiers.json "port") whose coordinates lie within COAST_NEAR hexes of the sea is snapped to the
    sea (as --coast);
  * data/city_sites.json: {"<name>": {"coast": true | false, "bank": [lat, lon], "coords": [lat, lon]}}: "coast"
    forces / forbids the sea; "coords": the coordinates of a city placed by hand (not in the candidate lists); "bank": the city's centre must be on the same side of the rivers (河/海/川 not crossed) as this point
    (the Roman bank of the Rhine and Danube for the limes towns).
Prints every city that moved.
"""
import argparse
import json
from pathlib import Path

import numpy as np

from make_world import COVER, ROAD, SHORE, hex_ring, lonlat_to_hex
from worldgeo import WorldGeo, WorldSpec

HERE = Path(__file__).resolve().parent
SEA_NEAR = 2                    # --move --coast: the city touches the sea this close to its coordinates if it can
KEEP_BONUS = 12.0               # --replace: score bonus of a city's current hex (within 6 hexes of its target)
COAST_NEAR = 2.0                # --replace: a port whose coordinates are this close to the sea is snapped to it
DATA = HERE.parent / "data"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("world", nargs="?", default=str(DATA / "world"))
    ap.add_argument("--spacing", type=float, default=14.0)
    ap.add_argument("--max", type=int, default=0, help="at most this many new cities (0 = all that fit)")
    ap.add_argument("--move", nargs="+", metavar="NAME", help="only move these cities of the existing cities_world.json")
    ap.add_argument("--coast", action="store_true", help="with --move: snap to a site at the sea")
    ap.add_argument("--replace", action="store_true", help="snap every city of cities_world.json again, ids kept")
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
    if a.replace:
        return replace_cities(w, cands, lo, x, passable, land, taken, a.spacing, t, spec)
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


def replace_cities(w, cands, lo, x, passable, land, taken, spacing, t, spec):
    """--replace (module docstring)"""
    from scipy import ndimage
    R, C = taken.shape
    cur = json.loads((w / "cities_world.json").read_text(encoding="utf-8"))
    where = {c["name"]: (l, h) for c, l, h in zip(cands, lo, x)}
    tf = DATA / "city_tiers.json"
    tiers = {r["id"]: r for r in json.loads(tf.read_text(encoding="utf-8"))} if tf.exists() else {}
    sf = DATA / "city_sites.json"
    rules = json.loads(sf.read_text(encoding="utf-8")) if sf.exists() else {}
    sea = t == 8
    dsea = ndimage.distance_transform_edt(~sea)
    shore = ~np.isin(t, (6, 7, 8))
    blk = np.zeros((R, C), bool)
    blk[spec.china_lo:spec.china_lo + 200, spec.china_hi:spec.china_hi + 200] = True

    for nm, r in rules.items():                           # coordinates of cities placed by hand
        if isinstance(r, dict) and "coords" in r:
            from make_world import lonlat_to_hex
            from worldgeo import WorldGeo
            g_ = WorldGeo(spec, DATA / "cities_georef.json")
            l_, h_ = lonlat_to_hex(g_, np.array([r["coords"][1]], float), np.array([r["coords"][0]], float))
            where[nm] = (float(l_[0]), float(h_[0]))

    def target(c):
        if c["name"] not in where:                        # placed by hand (no coordinates): stays
            return c["lo"], c["hi"]
        l, h = where[c["name"]]
        y0 = int(round(l))
        y0, x0 = y0, int(round(h - 0.5 * (y0 & 1)))
        # coordinates that fall on Koei's block or its margin (his Liaodong reaches further east than the real one):
        # the nearest point just outside the margin
        a0, a1, b0, b1 = spec.china_lo - 4, spec.china_lo + 203, spec.china_hi - 4, spec.china_hi + 203
        if a0 <= y0 <= a1 and b0 <= x0 <= b1:
            k = int(np.argmin([y0 - a0, a1 - y0, x0 - b0, b1 - x0]))
            y0, x0 = [(a0 - 1, x0), (a1 + 1, x0), (y0, b0 - 1), (y0, b1 + 1)][k]
        return y0, x0

    def foot(y, xx):
        return [(y, xx)] + hex_ring(y, xx)
    reserved = {}
    for c in cur:
        if blk[c["lo"], c["hi"]] or hex_dist((c["lo"], c["hi"]), target(c)) <= 6:
            reserved[c["id"]] = foot(c["lo"], c["hi"])
    placed = []
    seats = json.loads((DATA / "cities_georef.json").read_text(encoding="utf-8"))
    koei_placed = [(k["lo"] + spec.china_lo, k["hi"] + spec.china_hi + 0.5 * ((k["lo"] + spec.china_lo) & 1)) for k in seats]
    done_taken = taken.copy()
    for cells in reserved.values():
        for h in cells:
            done_taken[h] = True
    moved = []
    for c in cur:
        cid, old = c["id"], (c["lo"], c["hi"])
        for h in reserved.get(cid, []):
            done_taken[h] = False
        others = placed + koei_placed
        if blk[old] or c["name"] not in where:          # (a city of the block keeps its hex)
            y, xx = old
        else:
            y0, x0 = target(c)
            rule = rules.get(c["name"], {})
            tier = tiers.get(cid, {})
            coast = rule.get("coast")
            if coast is None:
                coast = bool(tier.get("port")) and dsea[min(max(y0, 0), R - 1), min(max(x0, 0), C - 1)] <= COAST_NEAR
            side = None
            if "bank" in rule:
                from make_world import lonlat_to_hex
                from worldgeo import WorldGeo
                g = WorldGeo(spec, DATA / "cities_georef.json")
                bl, bx = lonlat_to_hex(g, np.array([rule["bank"][1]]), np.array([rule["bank"][0]]))
                by = int(round(bl[0])); bh = int(round(bx[0] - 0.5 * (by & 1)))
                r0, r1, c0, c1 = max(0, by - 40), min(R, by + 41), max(0, bh - 40), min(C, bh + 41)
                lab = hex_label(shore[r0:r1, c0:c1], r0)
                side = (lab, r0, c0, lab[by - r0, bh - c0])
                if side[3] == 0:
                    raise SystemExit(f"{c['name']}: its bank point is on water")
            best = None
            for reach in ((6, 12) if coast else (6,)):
                if best is not None:
                    break
                for dy in range(-reach, reach + 1):
                    for dx in range(-reach, reach + 1):
                        yy, xq = y0 + dy, x0 + dx
                        if not (0 <= yy < R and 0 <= xq < C):
                            continue
                        if side is not None:
                            lab, r0, c0, want = side
                            if not (0 <= yy - r0 < lab.shape[0] and 0 <= xq - c0 < lab.shape[1]) or lab[yy - r0, xq - c0] != want:
                                continue
                        cand = snap(yy, xq, dy, dx, None, R, C, done_taken, land, passable, others or [(-999, -999)],
                                    spacing, sea if coast else None, tier.get("tier", 1) <= 1, (y0, x0))
                        if cand is None:
                            continue
                        if (yy, xq) == old and hex_dist(old, (y0, x0)) <= 6:
                            cand = (cand[0] + KEEP_BONUS,) + cand[1:]
                        if best is None or (cand[4], cand[0]) > (best[4], best[0]):
                            best = cand
            if best is None:
                raise SystemExit(f"{c['name']}: no free land site near its coordinates" + (" at the sea" if coast else ""))
            y, xx = best[1], best[2]
        if (y, xx) != old:
            moved.append((c["name"], cid, old, (y, xx)))
        c["lo"], c["hi"] = int(y), int(xx)
        placed.append((y, xx + 0.5 * (y & 1)))
        for h in foot(y, xx):
            done_taken[h] = True
    (w / "cities_world.json").write_text(json.dumps(cur, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"{len(cur)} cities snapped again, {len(moved)} moved:")
    for nm, cid, a_, b_ in moved:
        print(f"  {nm} (id {cid}): {a_} -> {b_}, {hex_dist(a_, b_)} hexes")


def hex_label(mask, row0):
    """connected components of mask by hex neighbours (rows offset row0 in the world, for the odd-row shift)"""
    lab = np.zeros(mask.shape, np.int32)
    n = 0
    for y, x in zip(*np.nonzero(mask)):
        if lab[y, x]:
            continue
        n += 1
        lab[y, x] = n
        stack = [(y, x)]
        while stack:
            a, b = stack.pop()
            for p, q in hex_ring(a + row0, b):
                p -= row0
                if 0 <= p < mask.shape[0] and 0 <= q < mask.shape[1] and mask[p, q] and not lab[p, q]:
                    lab[p, q] = n
                    stack.append((p, q))
    return lab


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
