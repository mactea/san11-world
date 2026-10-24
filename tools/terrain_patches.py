"""Terrain patches (data/terrain_patches.json, M11_MAP.md): local corrections of the generated hex terrain where
the DEM classification of make_world.py goes wrong at the map's scale (about 9 km per hex).

    {"straits": [{"name": ..., "line": [[lat, lon], ...], "max_fill": 400}, ...],
     "plains":  [{"name": ..., "center": [lat, lon], "radius": 11, "max_elev": 2100,
                  "cover": {"0": 0.3, "1": 0.4, ...}}, ...]}

* straits: a sea strait narrower than a hex (Bosporus, Dardanelles, Messina, ...). The hexes along "line" become
  sea (8), a path of hex neighbours, so at least 1 hex of open water joins the two seas. Below-sea-level land
  (DEM < 0) connected to the line becomes sea too: make_world.py keeps only sea joined to more than 2000 sea
  hexes, so a basin behind such a strait (the Sea of Marmara) was drawn as land; it is filled again from the
  line (at most "max_fill" hexes, else the patch is an error). 河 (lakes, rivers) is left as it is.
* plains: a valley floor the mountain mask covered with cliffs (Koei's corridors along rivers are only kept where
  the regional relief is moderate; deep in high ranges, and on low coasts next to a deep sea whose slope counts as
  relief, everything became 崖). Every hex within "radius" hexes of "center" whose DEM elevation is at most
  "max_elev" metres (and is not water) and that is connected to the centre through such hexes is the floor; its
  cliffs (15) become land cover drawn from "cover" (terrain type -> share) in patches of a few hexes,
  deterministic per patch name. Other terrain on the floor stays.
* (no data) 渡所 12 on 河 7: Koei never puts a ferry on 河 (his 長江 and 黃河 are crossed by boat; all 88 of his
  ferries are on 川 6). make_world.py's own road builder used to bridge 河 too; a ferry with at least two 河
  neighbours and no 川 one is turned back into 河 so the river stays navigable.

Koei's China block is never changed. The result depends only on the data, the DEM and the terrain, and applying it
again changes nothing, so it can run on a fresh classification (make_world.py, before the cities and roads) and on
an existing world directory:

    python terrain_patches.py [world_dir]     (world_types.before_roads.npy and world_types.npy, rewritten in place)
"""
import argparse
import json
import zlib
from collections import deque
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
DATA = HERE.parent / "data"
STREAM, RIVER, SEA, BRIDGE, MOUNTAIN = 6, 7, 8, 12, 15


def nbrs(y, x):
    d = ((0, -1), (0, 1), (-1, -1), (-1, 0), (1, -1), (1, 0)) if y % 2 == 0 else ((0, -1), (0, 1), (-1, 0), (-1, 1), (1, 0), (1, 1))
    return [(y + a, x + b) for a, b in d]


def load():
    f = DATA / "terrain_patches.json"
    return json.loads(f.read_text(encoding="utf-8")) if f.exists() else {}


def _cube(y, x):
    return x - (y - (y & 1)) // 2, y            # axial (q, r) of an odd-row-shifted hex (row = lo)


def hex_segment(a, b):
    """hexes on the straight line a -> b (both (lo, hi)), each a neighbour of the one before"""
    (q0, r0), (q1, r1) = _cube(*a), _cube(*b)
    s0, s1 = -q0 - r0, -q1 - r1
    n = max(abs(q0 - q1), abs(r0 - r1), abs(s0 - s1))
    out = []
    for i in range(n + 1):
        f = i / max(n, 1)
        q, r, s = q0 + (q1 - q0) * f + 1e-6, r0 + (r1 - r0) * f + 2e-6, s0 + (s1 - s0) * f - 3e-6
        rq, rr, rs = round(q), round(r), round(s)
        if abs(rq - q) > abs(rr - r) and abs(rq - q) > abs(rs - s):
            rq = -rr - rs
        elif abs(rr - r) > abs(rs - s):
            rr = -rq - rs
        out.append((rr, rq + (rr - (rr & 1)) // 2))
    return out


def hex_line(geo, pts):
    """[[lat, lon], ...] -> the hexes along the polyline, consecutive hexes neighbours"""
    from make_world import lonlat_to_hex, to_hex
    lo, x = lonlat_to_hex(geo, np.array([p[1] for p in pts], float), np.array([p[0] for p in pts], float))
    ys, xs = to_hex(lo, x)
    out = []
    for i in range(len(pts) - 1):
        for h in hex_segment((int(ys[i]), int(xs[i])), (int(ys[i + 1]), int(xs[i + 1]))):
            if not out or out[-1] != h:
                out.append(h)
    return out


def flood(mask, seeds, limit):
    """hexes of mask connected to the seeds (seeds included); None when more than limit"""
    R, C = mask.shape
    seen = set(seeds)
    q = deque(seeds)
    while q:
        h = q.popleft()
        for n in nbrs(*h):
            if n not in seen and 0 <= n[0] < R and 0 <= n[1] < C and mask[n]:
                seen.add(n)
                q.append(n)
                if len(seen) > limit:
                    return None
    return seen


def apply(t, e0, geo, spec):
    """apply data/terrain_patches.json (and the 河 ferry rule) to t in place; e0: hex elevation (metres, the
    make_world.py sample). Returns report lines."""
    from make_world import COVER, noise
    data = load()
    R, C = t.shape
    china = np.zeros((R, C), bool)
    china[spec.china_lo:spec.china_lo + 200, spec.china_hi:spec.china_hi + 200] = True
    out = []

    def inside(h):
        return 0 <= h[0] < R and 0 <= h[1] < C and not china[h]

    for s in data.get("straits", []):
        line = [h for h in hex_line(geo, s["line"]) if inside(h)]
        basin = (e0 < 0) & ~np.isin(t, (SEA, RIVER)) & ~china
        hexes = flood(basin, line, s.get("max_fill", 400))
        if hexes is None:
            raise SystemExit(f"terrain_patches.json: strait {s['name']}: the land below sea level joined to it is more "
                             f"than max_fill = {s.get('max_fill', 400)} hexes")
        before = {}
        for h in hexes:
            if t[h] != SEA:
                before[int(t[h])] = before.get(int(t[h]), 0) + 1
                t[h] = SEA
        n = sum(before.values())
        out.append(f"strait {s['name']}: line {len(line)} hexes, {n} hexes made sea"
                   + (f" (from {', '.join(f'{k}: {v}' for k, v in sorted(before.items()))})" if n else ""))

    for p in data.get("plains", []):
        from make_world import lonlat_to_hex, to_hex
        lo, x = lonlat_to_hex(geo, np.array([p["center"][1]], float), np.array([p["center"][0]], float))
        ys, xs = to_hex(lo, x)
        c0 = (int(ys[0]), int(xs[0]))
        r = int(np.ceil(p["radius"])) + 1
        y0, y1, x0, x1 = max(0, c0[0] - r), min(R, c0[0] + r + 1), max(0, c0[1] - r), min(C, c0[1] + r + 1)
        Y, X = np.mgrid[y0:y1, x0:x1]
        d = np.hypot((Y - c0[0]) * 0.866, X + 0.5 * (Y & 1) - c0[1] - 0.5 * (c0[0] & 1))
        floor = np.zeros((R, C), bool)
        floor[y0:y1, x0:x1] = ((d <= p["radius"]) & (e0[y0:y1, x0:x1] <= p["max_elev"]) & ~china[y0:y1, x0:x1]
                               & ~np.isin(t[y0:y1, x0:x1], (STREAM, RIVER, SEA)))
        if not floor[c0]:
            raise SystemExit(f"terrain_patches.json: plain {p['name']}: the centre {c0} is above max_elev")
        hexes = flood(floor, [c0], 10 ** 6)
        mix = {int(k): float(v) for k, v in p["cover"].items()}
        kinds = [k for k in COVER if mix.get(k, 0) > 0]
        rng = np.random.default_rng(zlib.crc32(p["name"].encode("utf-8")))
        shape = (y1 - y0, x1 - x0)
        score = np.stack([np.log(mix[k]) + 1.15 * noise(shape, rng, 1.7) for k in kinds])
        pick = np.asarray(kinds, np.uint8)[np.argmax(score, 0)]
        n = 0
        for h in sorted(hexes):
            if t[h] == MOUNTAIN:
                t[h] = pick[h[0] - y0, h[1] - x0]
                n += 1
        out.append(f"plain {p['name']}: centre {c0}, floor {len(hexes)} hexes, {n} cliffs made land")

    # 渡所 on 河 (make_world.py's road builder): back to 河
    n = 0
    for y, x in zip(*np.nonzero((t == BRIDGE) & ~china)):
        ks = [int(t[h]) for h in nbrs(y, x) if 0 <= h[0] < R and 0 <= h[1] < C]
        if ks.count(RIVER) >= 2 and STREAM not in ks:
            t[y, x] = RIVER
            n += 1
    if n:
        out.append(f"{n} ferries on 河 turned back into 河")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("world", nargs="?", default=str(DATA / "world"))
    ap.add_argument("--cache", default=str(DATA / "world_hexcache.npz"), help="make_world.py's hex elevation cache")
    a = ap.parse_args()
    from make_world import Dem, sample_hexes
    from worldgeo import WorldGeo, WorldSpec
    w = Path(a.world)
    info = json.loads((w / "world_info.json").read_text())
    spec = WorldSpec(rows=info["rows"], cols=info["cols"], lon0=info["lon0"], lat0=info["lat0"],
                     china_lo=info["china_lo"], china_hi=info["china_hi"])
    geo = WorldGeo(spec, DATA / "cities_georef.json")
    key = f"{spec.rows},{spec.cols},{spec.lon0},{spec.lat0},{spec.china_lo},{spec.china_hi}"
    if Path(a.cache).exists() and str(np.load(a.cache)["key"]) == key:
        e0 = np.load(a.cache)["e0"]
    else:
        e0 = sample_hexes(geo, Dem(DATA / "etopo60s_i16.npy"), spec.rows, spec.cols)[0]
    for name in ("world_types.before_roads.npy", "world_types.npy"):
        f = w / name
        if not f.exists():
            continue
        t = np.load(f)
        old = t.copy()
        rep = apply(t, e0, geo, spec)
        np.save(f, t)
        print(f"{name}: {int((t != old).sum())} hexes changed")
        for line in rep:
            print("  " + line)


if __name__ == "__main__":
    main()
