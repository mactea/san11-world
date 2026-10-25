"""Terrain patches (data/terrain_patches.json, M11_MAP.md): local corrections of the generated hex terrain where
the DEM classification of make_world.py goes wrong at the map's scale (about 9 km per hex).

    {"straits": [{"name": ..., "line": [[lat, lon], ...], "max_fill": 400}, ...],
     "plains":  [{"name": ..., "center": [lat, lon], "radius": 11, "max_elev": 2100,
                  "cover": {"0": 0.3, "1": 0.4, ...}}, ...],
     "passes":  [{"name": ..., "line": [[lat, lon], ...]}, ...],
     "cuts":    [{"name": ..., "line": [[lat, lon], ...]}, ...],
     "wetlands": [{"name": ..., "polygon": [[lat, lon], ...], "share": 0.6, "poison": 0.05}, ...],
     "rapids":  [{"name": ..., "line": [[lat, lon], ...]}, ...],
     "streams": {"navigable_max_scalerank": 5, "ford_max_scalerank": 8, "match_hexes": 4,
                 "navigable_core_hexes": 1.5, "navigable_max_elev": 1500, "join_max_gap": 3,
                 "join_lowland_max_gap": 10, "join_lowland_max_elev": 50, "join_max_rise": 30}}

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
* streams: the generated 川 6 (impassable to every unit and to ships) is replaced outside Koei's block. Koei's 川
  are about 2 hexes wide; make_world.py draws Natural Earth's medium rivers up to 6-9 hexes wide near the sea, where
  they look like (and should be) navigable water. Each 川 hex, and each 渡所 12 make_world.py's own roads put on one,
  takes the class of the nearest Natural Earth centreline of scalerank <= "ford_max_scalerank" within "match_hexes":
    - scalerank <= "navigable_max_scalerank" (also when such a line is within "navigable_core_hexes", the confluence
      of a medium river and a thin one): 河 7, sailed by ships and crossed by land units by boat like 長江; its 渡所
      become 河 too (roads end at the banks, as on every 河);
    - else 浅瀬 13 (ford: land units wade, ships cannot); a 渡所 there stays 渡所 (roads cross fords by bridge);
    - no centreline near (Koei's 川 carried a few hexes out of the block by make_world.py): 浅瀬, or 海 8 where the
      DEM is below sea level (his coastal 川 strip continued into the sea).
  Then each 河/海 body holding such new 河 that is not joined to the open sea (a body with at least OCEAN_SEA sea
  hexes) is joined through land that becomes 河 (no cliffs, cities, the new cities' 7 hexes or Koei's block): to the
  open sea through at most "join_max_gap" hexes, else to the open sea through at most "join_lowland_max_gap" hexes
  no higher than "join_lowland_max_elev" metres (deltas and estuaries, where the centrelines stop short of the
  coast), else to another 河/海 body through at most "join_max_gap" hexes (the 1-2 hex banks and rasterisation gaps
  that cut rivers off), never climbing more than "join_max_rise" metres above the water on both sides (a gap in
  a river is level, a watershed is not). Repeated until nothing joins. Every other 河 body not joined to the open
  sea (a river whose rasterized mouth stops a hex short of the coast, the Amur) is joined to the sea itself (海) the
  same way, never to another river (so rivers parted at a watershed stay apart). Above "navigable_max_elev" metres the new 河 is 浅瀬
  instead (mountain rivers in gorges: the upper Mekong and Salween, the Yarlung Tsangpo).
* rapids: falls and cataracts that break a navigable river (the Khone Falls of the Mekong). The 河/浅瀬/川 hexes
  along "line" (drawn across the river, bank to bank) become 崖 15 after the streams rule: ships cannot pass and
  land units cannot wade across there (they still walk round on the banks); both stretches stay navigable water.
* passes: a mountain pass, e.g. through Koei's northern wall (his China block ends in about 20 hexes of 崖 along its
  north edge, so the steppe reached China on foot only through Liaoxi and Hexi). The hexes along "line" (a path of
  hex neighbours, as for straits) that are 崖 15 or 岸 14 become 桟道 11, a 1-hex plank road; with "bridge": true,
  outside Koei's block a river on the line (河 7, 浅瀬 13, 川 6) becomes 渡所 12, a bridge (the Jihong bridge over
  the Lancang on the Yongchang road); other hexes stay.
* cuts: a watershed where two rivers' brushes still touch (outside Koei's block): the 河/浅瀬/川 hexes along "line"
  become land (草 0), and no river is joined back through them.
* wetlands: within "polygon", the share "share" of the land cover hexes (草土砂湿毒森荒) becomes 湿 3, the share
  "poison" of them 毒 4 (by a noise field fixed per name, patches of a few hexes); other terrain stays.
  This is the one patch that edits Koei's own block: only his margin (no base, facility plot or dike, Koei's
  SHEX bytes 3 and 5), and make_world.py --types-from carries the 桟道 into china_overrides.bin and paints it on his
  3D stage as it does for the roads connect_cities.py lays there.
* (no data) 渡所 12 on 河 7: Koei never puts a ferry on 河 (his 長江 and 黃河 are crossed by boat; all 88 of his
  ferries are on 川 6). make_world.py's own road builder used to bridge 河 too; a ferry with at least two 河
  neighbours and no 川 or 浅瀬 one is turned back into 河 so the river stays navigable.

Koei's China block is never changed (except his margin under "passes"). The result depends only on the data, the
DEM and the terrain, and applying it again changes nothing, so it can run on a fresh classification (make_world.py, before the cities and roads) and on
an existing world directory:

    python terrain_patches.py [world_dir]     (world_types.before_roads.npy and world_types.npy, rewritten in place)

The streams step reads data/raw/ne_rivers/*.zip (the Natural Earth rivers make_world.py draws).
"""
import argparse
import heapq
import json
import zlib
from collections import deque
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
DATA = HERE.parent / "data"
STREAM, RIVER, SEA, PLANK, BRIDGE, FORD, SHORE, MOUNTAIN, CITY = 6, 7, 8, 11, 12, 13, 14, 15, 16
OCEAN_SEA = 5000                 # sea hexes a water body needs to count as open sea (as tools/navigable.py)


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


_LINES = {}


def river_lines(geo, R, C, max_rank):
    """hexes on Natural Earth river centrelines: (R, C) uint8, the lowest scalerank through each hex (255: none)"""
    key = (id(geo), R, C, max_rank)
    if key not in _LINES:
        _LINES[key] = _river_lines(geo, R, C, max_rank)
    return _LINES[key]


def _river_lines(geo, R, C, max_rank):
    from make_world import lonlat_to_hex, read_shapes, to_hex
    out = np.full((R, C), 255, np.uint8)
    zips = sorted((DATA / "raw/ne_rivers").glob("*.zip"))
    if not zips:
        raise SystemExit("terrain_patches.json streams: data/raw/ne_rivers/*.zip is missing (python san11kit.py download)")
    from make_world import hydro_rules
    rules = hydro_rules()
    excluded = set(rules.get("exclude_rivers", ()))
    for sr in read_shapes(zips[0]).iterShapeRecords():
        rank = int(rules.get("scalerank", {}).get(sr.record["name"], sr.record["scalerank"]))
        if rank > max_rank or sr.record["name"] in excluded:
            continue
        pts = np.asarray(sr.shape.points, dtype=float)
        parts = list(sr.shape.parts) + [len(pts)]
        for a, b in zip(parts[:-1], parts[1:]):
            p = pts[a:b]
            if len(p) < 2:
                continue
            seg = np.hypot(*np.diff(p, axis=0).T)
            n = np.maximum(1, np.ceil(seg / 0.02).astype(int))
            dense = np.concatenate([np.linspace(p[i], p[i + 1], n[i], endpoint=False) for i in range(len(seg))] + [p[-1:]])
            lo, x = lonlat_to_hex(geo, dense[:, 0], dense[:, 1])
            if lo.max() < -3 or lo.min() > R + 3 or x.max() < -3 or x.min() > C + 3:
                continue
            yl, yh = to_hex(lo, x)
            ok = (yl >= 0) & (yl < R) & (yh >= 0) & (yh < C)
            np.minimum.at(out, (yl[ok], yh[ok]), rank)
    return out


def water_bodies(t):
    """(labels of the 河/海 bodies, -1 elsewhere; mask of the bodies joined to the open sea)"""
    from connect_cities import components
    lab = components(np.isin(t, (RIVER, SEA)))
    n_sea = np.bincount(lab[t == SEA] + 1, minlength=lab.max() + 2)
    big = np.flatnonzero(n_sea >= OCEAN_SEA) - 1
    return lab, np.isin(lab, big[big >= 0])


def join_gap(lab, ocean, comp, allowed, max_gap, accept=None):
    """fewest allowed hexes (at most max_gap) from water body comp to other water: {0: hexes to the open sea,
    1: hexes to another 河/海 body} (the keys found). accept(source water hex, path, target water hex): whether
    such a join may be made (None: every one)"""
    R, C = lab.shape
    ys, xs = np.nonzero(lab == comp)
    heap, seen, found = [], set(), {}
    for y, x in zip(ys.tolist(), xs.tolist()):
        for n in nbrs(y, x):
            if 0 <= n[0] < R and 0 <= n[1] < C and lab[n] < 0 and allowed[n]:
                heapq.heappush(heap, (1, n, (n,), (y, x)))
    while heap and 0 not in found:
        k, h, path, src = heapq.heappop(heap)
        if h in seen:
            continue
        seen.add(h)
        for n in nbrs(*h):
            if not (0 <= n[0] < R and 0 <= n[1] < C) or n in seen:
                continue
            if lab[n] >= 0 and lab[n] != comp:
                if accept is None or accept(src, path, n):
                    found.setdefault(0 if ocean[n] else 1, path)
            elif lab[n] < 0 and allowed[n] and k < max_gap:
                heapq.heappush(heap, (k + 1, n, path + (n,), src))
    return found


def streams(t, e0, geo, cfg, china, avoid=None):
    """the streams rule (module docstring) on t in place; returns report lines"""
    from scipy import ndimage
    R, C = t.shape
    cand = np.isin(t, (STREAM, BRIDGE)) & ~china
    if not (cand & (t == STREAM)).any():
        return []                                          # applied before: nothing to do
    nav_max, ford_max = int(cfg.get("navigable_max_scalerank", 5)), int(cfg.get("ford_max_scalerank", 8))
    match, core = float(cfg.get("match_hexes", 4)), float(cfg.get("navigable_core_hexes", 1.5))
    rank = river_lines(geo, R, C, ford_max)
    d_nav = ndimage.distance_transform_edt(rank > nav_max)
    d_any = ndimage.distance_transform_edt(rank > ford_max)
    sourced = d_any <= match
    nav = cand & sourced & ((d_nav <= d_any) | (d_nav <= core))
    if cfg.get("navigable_max_elev") is not None:
        nav &= e0 <= float(cfg["navigable_max_elev"])
    n = {"川": int((cand & (t == STREAM)).sum()), "渡所": int((cand & (t == BRIDGE)).sum()),
         "渡所→河": int((nav & (t == BRIDGE)).sum())}
    for key, mask, k in (("河", nav, RIVER), ("浅瀬", cand & sourced & ~nav & (t == STREAM), FORD),
                         ("海 (no centreline, below sea level)", cand & ~sourced & (t == STREAM) & (e0 < 0), SEA),
                         ("浅瀬 (no centreline)", cand & ~sourced & (t == STREAM), FORD)):
        n[key] = int(mask.sum())
        t[mask] = k
    out = ["streams: " + ", ".join(f"{k} {v}" for k, v in n.items())]
    # join the new 河 to the open sea through short gaps
    allowed = ~china & ~np.isin(t, (RIVER, SEA, MOUNTAIN, CITY, 17, 18))
    if avoid is not None:
        allowed &= ~avoid
    max_gap, low_gap = int(cfg.get("join_max_gap", 3)), int(cfg.get("join_lowland_max_gap", 10))
    lowland = e0 <= float(cfg.get("join_lowland_max_elev", 50))     # deltas and estuaries
    joined, gap, failed = [], {}, set()
    outside = np.zeros((R, C), bool)
    outside[~china] = True
    rise = float(cfg.get("join_max_rise", 30))

    def level(src, path, n):
        """a join into a river must not climb: no higher than both banks' water by more than join_max_rise metres
        (a watershed between two rivers is, a gap in a rasterized river is not)"""
        return t[n] == SEA or max(e0[h] for h in path) <= max(e0[src], e0[n]) + rise

    def to_sea(src, path, n):
        return t[n] == SEA
    while True:
        lab, ocean = water_bodies(t)
        seaward = set(np.unique(lab[ocean]).tolist())
        todo = sorted({int(c) for c in np.unique(lab[nav]) if c >= 0} - seaward)
        # every other 河 body outside the block: to the open sea only
        rest = sorted({int(c) for c in np.unique(lab[(t == RIVER) & outside]) if c >= 0} - seaward - set(todo))
        path = None
        labs, first = np.unique(lab.ravel(), return_index=True)
        first = dict(zip(labs.tolist(), first.tolist()))
        for comp in todo + rest:
            key = first[comp]          # a body that could not join, by its first hex (labels change as bodies join)
            if key in failed:
                continue
            ok = level if comp in todo else to_sea
            short = join_gap(lab, ocean, comp, allowed, max_gap, ok)
            path = short.get(0) or join_gap(lab, ocean, comp, allowed & lowland, low_gap, ok).get(0)
            if not path and comp in todo:
                path = short.get(1)
            if path:
                break
            failed.add(key)
        if not path:
            break
        for h in path:
            gap[int(t[h])] = gap.get(int(t[h]), 0) + 1
            t[h] = RIVER
            allowed[h] = False
        joined.append(len(path))
    left = sorted({int(c) for c in np.unique(lab[nav]) if c >= 0} - set(np.unique(lab[ocean]).tolist()))
    out.append(f"streams: {len(joined)} joins through {sum(joined)} land hexes made 河 (at most {max_gap} each, "
               f"{low_gap} through lowland to the open sea; "
               f"from {', '.join(f'{k}: {v}' for k, v in sorted(gap.items()))}); {len(left)} water bodies with new 河 "
               f"still apart from the open sea, holding {int(sum((nav & (lab == c)).sum() for c in left))} of the new 河")
    return out


def apply(t, e0, geo, spec, avoid=None):
    """apply data/terrain_patches.json (and the 河 ferry rule) to t in place; e0: hex elevation (metres, the
    make_world.py sample); avoid: optional mask of hexes no river is joined through (the new cities' 7 hexes).
    Returns report lines."""
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
                               & ~np.isin(t[y0:y1, x0:x1], (STREAM, FORD, RIVER, SEA)))
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

    cut = np.zeros((R, C), bool)
    for c in data.get("cuts", []):
        line = [h for h in hex_line(geo, c["line"]) if inside(h)]
        made = 0
        for h in line:
            cut[h] = True
            if t[h] in (STREAM, RIVER, FORD):
                t[h] = 0
                made += 1
        out.append(f"cut {c['name']}: line {len(line)} hexes, {made} river hexes made land")

    for wl in data.get("wetlands", []):
        from make_world import lonlat_to_hex, points_in_ring
        ring = np.asarray(wl["polygon"], float)
        lo, hi = lonlat_to_hex(geo, ring[:, 1], ring[:, 0])
        y0, y1 = max(0, int(lo.min())), min(R - 1, int(lo.max()) + 1)
        x0, x1 = max(0, int(hi.min())), min(C - 1, int(hi.max()) + 1)
        Y, X = np.mgrid[y0:y1 + 1, x0:x1 + 1]
        inpoly = points_in_ring(Y.ravel().astype(float), X.ravel() + 0.5 * (Y.ravel() & 1), np.stack([lo, hi], 1)).reshape(Y.shape)
        sub = t[y0:y1 + 1, x0:x1 + 1]
        cand = inpoly & np.isin(sub, (0, 1, 2, 3, 4, 5, 9)) & ~china[y0:y1 + 1, x0:x1 + 1]
        rng = np.random.default_rng(zlib.crc32(wl["name"].encode("utf-8")))
        u = noise(Y.shape, rng, 1.5)
        rank = np.zeros(Y.shape)
        if cand.any():
            order = np.argsort(np.argsort(u[cand], kind="stable"), kind="stable")
            rank[cand] = (order + 0.5) / cand.sum()
        before = int(np.isin(sub[cand], (3, 4)).sum())
        sub[cand & (rank < float(wl.get("share", 0.5)))] = 3
        sub[cand & (rank < float(wl.get("poison", 0)))] = 4
        out.append(f"wetland {wl['name']}: {int(cand.sum())} land cover hexes, 湿/毒 {before} -> {int(np.isin(sub[cand], (3, 4)).sum())}")

    if "streams" in data:
        out += streams(t, e0, geo, data["streams"], china, cut if avoid is None else (avoid | cut))

    for rp in data.get("rapids", []):
        line = [h for h in hex_line(geo, rp["line"]) if inside(h)]
        made = 0
        for h in line:
            if t[h] in (STREAM, RIVER, FORD):
                t[h] = MOUNTAIN
                made += 1
        out.append(f"rapids {rp['name']}: line {len(line)} hexes, {made} river hexes made 崖")

    bridges = set()
    if data.get("passes"):
        koei = np.load(DATA / "china_shex.npy")
        for p in data["passes"]:
            line = hex_line(geo, p["line"])
            made, kept, bridged = 0, [], 0
            for h in line:
                if not (0 <= h[0] < R and 0 <= h[1] < C):
                    continue
                if china[h]:
                    k = koei[h[0] - spec.china_lo, h[1] - spec.china_hi]
                    if k[0] in (CITY, 17, 18) or k[3] or k[5]:
                        raise SystemExit(f"terrain_patches.json: pass {p['name']} crosses Koei's base, plot or dike at {h}")
                if t[h] in (MOUNTAIN, SHORE):
                    t[h] = PLANK
                    made += 1
                elif t[h] in (RIVER, FORD, STREAM) and not china[h] and p.get("bridge"):
                    t[h] = BRIDGE
                    bridged += 1
                    bridges.add(h)
                elif t[h] == BRIDGE and not china[h]:
                    bridges.add(h)
                elif t[h] != PLANK:
                    kept.append(int(t[h]))
            n_in = sum(1 for h in line if china[h])
            out.append(f"pass {p['name']}: line {len(line)} hexes ({n_in} in Koei's block), {made} cliffs/banks made 桟道"
                       + (f", {bridged} river hexes made 渡所 (bridge)" if bridged else "")
                       + (f"; other terrain on it kept: {kept}" if kept else ""))

    # 渡所 on 河 (make_world.py's road builder): back to 河
    n = 0
    for y, x in zip(*np.nonzero((t == BRIDGE) & ~china)):
        if (int(y), int(x)) in bridges:
            continue
        ks = [int(t[h]) for h in nbrs(y, x) if 0 <= h[0] < R and 0 <= h[1] < C]
        if ks.count(RIVER) >= 2 and STREAM not in ks and FORD not in ks:
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
    from make_world import Dem, cache_key, sample_hexes
    from worldgeo import WorldGeo, WorldSpec
    w = Path(a.world)
    info = json.loads((w / "world_info.json").read_text())
    spec = WorldSpec(rows=info["rows"], cols=info["cols"], lon0=info["lon0"], lat0=info["lat0"],
                     china_lo=info["china_lo"], china_hi=info["china_hi"])
    geo = WorldGeo(spec, DATA / "cities_georef.json")
    key = cache_key(spec, geo)
    if Path(a.cache).exists() and str(np.load(a.cache)["key"]) == key:
        e0 = np.load(a.cache)["e0"]
    else:
        e0 = sample_hexes(geo, Dem(DATA / "etopo60s_i16.npy"), spec.rows, spec.cols)[0]
    avoid = None
    if (w / "cities_world.json").exists():                 # the new cities' 7 hexes: no river joined through them
        avoid = np.zeros((spec.rows, spec.cols), bool)
        for c in json.loads((w / "cities_world.json").read_text(encoding="utf-8")):
            for y, x in [(c["lo"], c["hi"])] + nbrs(c["lo"], c["hi"]):
                if 0 <= y < spec.rows and 0 <= x < spec.cols:
                    avoid[y, x] = True
    for name in ("world_types.before_roads.npy", "world_types.npy"):
        f = w / name
        if not f.exists():
            continue
        t = np.load(f)
        old = t.copy()
        rep = apply(t, e0, geo, spec, avoid)
        np.save(f, t)
        print(f"{name}: {int((t != old).sum())} hexes changed")
        for line in rep:
            print("  " + line)


if __name__ == "__main__":
    main()
