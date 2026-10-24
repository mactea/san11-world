"""Navigable rivers for port cities on a river (M11_MAP.md): data/navigable_rivers.json lists the cities, and
for each one a channel of 河 (7, navigable) is laid from the city to the open sea along the river's 川 (6).

    [{"city": "番禺", "river": "珠江", "via": [[lat, lon], ...]}, ...]      ("via": optional waypoints, in order)

The channel is the cheapest hex path from next to the city's footprint (the hexes around a 7-hex city, the city's
own neighbours for a 3- or 1-hex one, which then touches it whichever triangle gen_bases.py chooses) to sea that
is connected to the ocean (a water body with at least OCEAN_SEA sea hexes, not a lake): 川 is cheap, existing 河
cheaper, land expensive (gaps in the rasterized river are filled), and the path keeps off every other city's
7 hexes, Koei's bases, facility plots and dikes. A channel found earlier in the list counts as open sea for the
later ones (so a tributary may end in it). On it 川 and land become 河; roads (主徑 10) become ferries (12)
so they stay walkable; ferries stay. In Koei's block only his patch (make_world.py china_patch) and his 川 are
changed (his own 3D stage is drawn there otherwise).

The course is derived from the terrain without roads (base_terrain(): world_types.before_roads.npy with make_world's
China patch), so gen_bases.py (both runs) and connect_cities.py get the same hexes; apply() writes them only where
the terrain is still as it was then.
"""
import heapq
import json
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
DATA = HERE.parent / "data"
STREAM, RIVER, SEA, ROAD, PLANK, BRIDGE, CITY, MOUNTAIN = 6, 7, 8, 10, 11, 12, 16, 15
OCEAN_SEA = 5000                 # sea hexes a water body needs to count as open sea
STEP = {RIVER: 0.4, SEA: 0.4, STREAM: 1.0, BRIDGE: 1.0, MOUNTAIN: 16.0}   # other land: LAND
LAND = 8.0


def nbrs(y, x):
    d = ((0, -1), (0, 1), (-1, -1), (-1, 0), (1, -1), (1, 0)) if y % 2 == 0 else ((0, -1), (0, 1), (-1, 0), (-1, 1), (1, 0), (1, 1))
    return [(y + a, x + b) for a, b in d]


def ring(y, x, r):
    """hexes at distance exactly r (1 or 2)"""
    inner = {(y, x)} | set(nbrs(y, x))
    if r == 1:
        return sorted(inner - {(y, x)})
    return sorted({n for h in inner for n in nbrs(*h)} - inner)


def base_terrain(w, t, spec):
    """the terrain the courses are derived from: world_types.before_roads.npy (t without it), with Koei's block
    patched as make_world.py last drew it (china_patch_types.npy, which connect_cities.py starts from too)"""
    f = Path(w) / "world_types.before_roads.npy"
    tb = np.load(f) if f.exists() else t.copy()
    tb = tb if tb.shape == t.shape else t.copy()
    f = Path(w) / "china_patch_types.npy"
    if f.exists():
        pt = np.load(f)
        blk = (slice(spec.china_lo, spec.china_lo + 200), slice(spec.china_hi, spec.china_hi + 200))
        tb[blk] = np.where(pt != 255, pt, tb[blk])
    return tb


def ocean_mask(t):
    from connect_cities import components                 # (connect_cities imports this module)
    lab = components(np.isin(t, (RIVER, SEA)))
    n_sea = np.bincount(lab[t == SEA] + 1, minlength=lab.max() + 2)
    big = np.flatnonzero(n_sea >= OCEAN_SEA) - 1
    return np.isin(lab, big[big >= 0])


def load():
    f = DATA / "navigable_rivers.json"
    return json.loads(f.read_text(encoding="utf-8")) if f.exists() else []


def courses(tb, new, tiers, spec, patch=None):
    """tb: terrain without roads; new: the new cities [{id, name, lo, hi}]; tiers: {id: tier}; spec: WorldSpec;
    patch: make_world's china_patch.npy (200x200, the patched part of Koei's block) or None.
    Returns [{city, river, hexes: [[y, x, type], ...], stream, land, road, length}] (None hexes: no course found)."""
    rows = load()
    if not rows:
        return []
    R, C = tb.shape
    clo, chi = spec.china_lo, spec.china_hi
    koei = np.load(DATA / "china_shex.npy")
    protect = np.isin(tb, (CITY, 17, 18))
    blk = (slice(clo, clo + 200), slice(chi, chi + 200))
    protect[blk] |= (koei[..., 5] != 0) | (koei[..., 3] != 0)
    koei_own = np.zeros((R, C), bool)                     # Koei's own stage (not the patch): only his 川 may change
    koei_own[blk] = True if patch is None else ~patch
    protect |= koei_own & (tb != STREAM)
    by_name = {c["name"]: c for c in new}
    near = {}
    for c in new:
        for h in [(c["lo"], c["hi"])] + ring(c["lo"], c["hi"], 1):
            near[h] = c["name"]
    ocean = ocean_mask(tb)
    geo = None
    out = []
    for row in rows:
        c = by_name.get(row["city"])
        if c is None:
            continue
        name = c["name"]
        blocked = protect.copy()
        for h, nm in near.items():
            if nm != name:
                blocked[h] = True
        y0, x0 = c["lo"], c["hi"]
        blocked[y0, x0] = True
        tier = tiers.get(c["id"], 1)
        if tier <= 1:
            for h in ring(y0, x0, 1):
                blocked[h] = True
        start = [h for h in ring(y0, x0, 2 if tier <= 1 else 1) if not blocked[h]]
        goals = []
        for lat, lon in row.get("via", []):
            if geo is None:
                from make_world import lonlat_to_hex, to_hex
                from worldgeo import WorldGeo
                geo = WorldGeo(spec, DATA / "cities_georef.json")
            lo, x = lonlat_to_hex(geo, np.array([lon]), np.array([lat]))
            yy, xx = to_hex(lo, x)
            gy, gx = int(yy[0]), int(xx[0])
            disk = {(gy, gx)} | {n for h in nbrs(gy, gx) for n in nbrs(*h)}      # within 2 hexes, on the river
            goals.append({h for h in disk if tb[h] in (STREAM, RIVER)} or disk)
        path = []
        for g in goals + [None]:
            seg = search(tb, blocked, ocean, start, g)
            if seg is None:
                path = None
                break
            path += seg
            start = [seg[-1]] if seg else start
        rec = {"city": name, "river": row.get("river", ""), "hexes": None}
        if path is not None:
            hexes, seen = [], set()
            for y, x in path:
                if (y, x) in seen or ocean[y, x]:
                    continue
                seen.add((y, x))
                k = int(tb[y, x])
                if k not in (RIVER, SEA, BRIDGE, ROAD, PLANK):
                    hexes.append([y, x, RIVER])
                elif k in (ROAD, PLANK):
                    hexes.append([y, x, BRIDGE])
            rec["hexes"] = hexes
            rec["stream"] = sum(1 for y, x, k in hexes if tb[y, x] == STREAM)
            rec["road"] = sum(1 for y, x, k in hexes if k == BRIDGE)
            rec["land"] = len(hexes) - rec["stream"] - rec["road"]
            rec["length"] = len(seen)
            for h in seen:                                # a later river may end in this channel (博羅 -> 番禺's)
                ocean[h] = True
        out.append(rec)
    return out


def search(tb, blocked, ocean, start, goal):
    """cheapest path from any start hex to a goal hex (goal None: an ocean hex, which ends the path unvisited)"""
    R, C = tb.shape
    prev, heap, h = {}, [], None
    for h in start:
        if goal is None and ocean[h]:
            return []
        d = STEP.get(int(tb[h]), LAND)
        heapq.heappush(heap, (d, h, None))
    while heap:
        d, h, p = heapq.heappop(heap)
        if h in prev:
            continue
        prev[h] = p
        if (goal is not None and h in goal) or d > 4000:
            break
        for n in nbrs(*h):
            if n in prev or not (0 <= n[0] < R and 0 <= n[1] < C):
                continue
            if goal is None and ocean[n]:
                path = []
                while h is not None:
                    path.append(h)
                    h = prev[h]
                return path[::-1]
            if blocked[n]:
                continue
            heapq.heappush(heap, (d + STEP.get(int(tb[n]), LAND), n, h))
    if goal is not None and h in goal:
        path = []
        while h is not None:
            path.append(h)
            h = prev[h]
        return path[::-1]
    return None


def apply(t, tb, found):
    """write the channels into t where it still has the terrain the course was derived from; returns the hexes
    changed [(y, x, type)]"""
    changed = []
    for r in found:
        for y, x, k in r["hexes"] or []:
            if t[y, x] == tb[y, x] and t[y, x] != k:
                t[y, x] = k
                changed.append((y, x, k))
    return changed
