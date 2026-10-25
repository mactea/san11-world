"""Generate the data for more cities than Koei's 42 (M7_LAYOUT.md): territories in the world map,
the area/base tables the game kept as static data, and the new cities' records.

Usage: python gen_bases.py [world_dir] --cities C
  world_dir: data/world (world_types.npy, world_shex.bin, world_info.json, cities_world.json from
             tools/select_cities.py). C = total number of cities, 42..1000 (new cities = the first C-42 of
             cities_world.json, which is in importance order).
--flatten-plots (the build's first run, before connect_cities.py and make_world.py --types-from): a city still
short of facility plots with PLOT_SHORT_SHARE gets the cliffs of its territory nearest it made land (FLATTEN_REACH),
written into world_dir's world_types.npy (and world_types.before_roads.npy), so the terrain and the 3D heights
built from it have the farmland the second run places plots on.
Writes into --out (default world_dir; the world_dir inputs from make_world.py are read as they are, except with
--flatten-plots):
  world_shex.bin     area ids of the territories (u16) and the city footprints (terrain 16): 7, 3 or 1 hexes by
                     the city's tier in data/city_tiers.json (M11_MAP.md), with ways to the water opened for ports
  china_overrides.bin  edits of Koei's China block in final numbering ("WCO2"): make_world's, plus the
                     outer band hexes new cities take (the seam with Koei's areas, see territories())
  bases_tables.bin   "WBTB", u32 C, then {u32 size, bytes} for AREAADJ, MAT87A, MAT87B, MATC, AREA2CITY,
                     AREA2BASE, CITYTAB844, DBGBASE (the order worldmod's bases.cpp reads)
  bases_cities.json  the new cities (id, name, lo, hi, province, neighbours, tier, shape, cells) for the scenario
                     converter and connect_cities.py, and "port_open" (hexes opened towards the water)
  bases_objs.bin     their 3D city models (OBJS records added by worldmod)
  city_shapes.bin    "WCSH", u32 1, u32 C, C x {u8 shape, u8 tier, u8 variant, u8 flags} (M11_MAP.md, read by worldmod)
  world_height.npy   the vertex heights with the land around each new city levelled (Koei's cities stand
                     on plateaus at their model's height); only when the world directory has vertex data
  china_patch.bin    copied from the world directory (make_world.py): where worldmod draws China from it
Id layout: cities 0..C-1 (Koei's 0..41 unchanged), gates C..C+9, ports C+10..N-1, special areas N..N+5,
N = C+45. With --cities 42 the tables equal the originals byte for byte (--verify checks that).
"""
import argparse
import json
import shutil
import struct
from collections import defaultdict, deque
from pathlib import Path

import numpy as np
from scipy import ndimage

HERE = Path(__file__).resolve().parent
DATA = HERE.parent / "data"
GAME = HERE.parent.parent                 # the game directory (--game); the default is this project's own layout
EXE = None                                # <game>/san11pk.exe: Koei's tables are read from it (main() sets these)
SCENARIO = None                           # <game>/Media/scenario/Scenario.s11: Koei's city neighbour lists
WATER = (6, 7, 8)
CITY_T = 16


def exe_bytes(va, n):
    with open(EXE, "rb") as f:
        f.seek(va - 0x400000)
        return f.read(n)


def hex_nbrs(y, x):
    d = [(0, -1), (0, 1), (-1, -1), (-1, 0), (1, -1), (1, 0)] if y % 2 == 0 else [(0, -1), (0, 1), (-1, 0), (-1, 1), (1, 0), (1, 1)]
    return [(y + a, x + b) for a, b in d]


# M11 (M11_MAP.md): a new city covers 7, 3 or 1 hexes by its tier in data/city_tiers.json (1 big, 2 middle,
# 3 small; Koei's 42 keep 7). The 3-hex triangle k (0..5) is the centre and its neighbours ROT[k], ROT[k+1].
ROT = (1, 5, 4, 0, 2, 3)            # hex_nbrs indices clockwise: E, lower right, lower left, W, upper left, upper right
SHAPE_7, SHAPE_1 = 0, 7             # city_shapes.bin shape: 0 = 7 hexes, 1..6 = triangle k = shape - 1, 7 = 1 hex
NAV = (7, 8)                        # navigable water: 河, 海 (川 6 is not)
CELL_SCORE = {6: -60, 13: -60, 7: -100, 8: -100, 15: -8, 14: -2}   # choosing a triangle: avoid water, fords, cliffs, banks
TIER_FLAT = {1: (8, 12), 2: (6, 10), 3: (4, 7)}             # land levelled around the model, blended out to (vertices)
LARGEST = ("羅馬", "黎軒", "安都", "斯賓", "迦太")           # Rome, Alexandria, Antioch, Ctesiphon, Carthage: model toL
PORT_REACH = 6                      # a port (city_tiers.json "port") may get a way this many hexes long to the water
OPEN_AS = {14: 10, 15: 11, 6: 12}   # opened like connect_cities.py: bank -> main road, cliff -> plank road, stream -> ferry


def shape_cells(y, x, shape):
    n = hex_nbrs(y, x)
    if shape == SHAPE_7:
        return [(y, x)] + n
    if shape == SHAPE_1:
        return [(y, x)]
    k = shape - 1
    return [(y, x), n[ROT[k]], n[ROT[(k + 1) % 6]]]


def load_tiers(new):
    """{city id: (tier, port)} of the new cities from data/city_tiers.json (tier 1 and no port without it)"""
    f = DATA / "city_tiers.json"
    rows = {r["id"]: r for r in json.loads(f.read_text(encoding="utf-8"))} if f.exists() else {}
    out = {}
    for c in new:
        r = rows.get(c["id"])
        if r and r["name"] != c["name"]:
            raise SystemExit(f"city_tiers.json: id {c['id']} is {r['name']}, cities_world.json says {c['name']}")
        out[c["id"]] = (int(r["tier"]), bool(r["port"])) if r else (1, False)
    return out


def nav_steps(t, cells, limit):
    """hex steps from the cells to the nearest navigable water (1 = touching it), None beyond limit"""
    seen = set(cells)
    front = list(cells)
    for d in range(1, limit + 1):
        nxt = []
        for c in front:
            for n in hex_nbrs(*c):
                if n in seen or not (0 <= n[0] < t.shape[0] and 0 <= n[1] < t.shape[1]):
                    continue
                if t[n] in NAV:
                    return d
                seen.add(n)
                nxt.append(n)
        front = nxt
    return None


def choose_shape(y, x, tier, port, tb, free, salt=0):
    """tier 1: 7 hexes, tier 3: 1 hex; tier 2: the triangle that stands best on land (CELL_SCORE) and, for a
    port, touches navigable water (or comes next to it), using only hexes free(y, x) allows; tb = the terrain
    without roads (the same in both of the build's gen_bases.py runs); equal triangles: turned by salt (the
    city id), so they do not all point the same way. None: no triangle fits."""
    if tier <= 1:
        return SHAPE_7
    if tier >= 3:
        return SHAPE_1
    best = None
    for k in range(6):
        cells = shape_cells(y, x, 1 + k)
        if not all(free(*c) for c in cells):
            continue
        s = sum(CELL_SCORE.get(int(tb[c]), 0) for c in cells[1:]) + 0.01 * ((k + salt) % 6)
        if port:                                          # 40 touching the water, less the farther it is
            d = nav_steps(tb, cells, PORT_REACH + 2)
            s += 40 if d == 1 else max(0, 20 - 2 * d) if d else 0
        if best is None or s > best[0]:
            best = (s, k)
    return None if best is None else 1 + best[1]


def port_access(t, cells, reach, ok):
    """how units leave the city by water: ("direct", []) when a city hex touches navigable water; else the
    cheapest way of at most `reach` hexes over land ok(y, x) allows to a hex next to navigable water:
    ("open", the banks/cliffs/streams on it, to open with OPEN_AS) or ("land", []) when it is already passable;
    (None, []) when there is none"""
    import heapq
    if any(t[n] in NAV for c in cells for n in hex_nbrs(*c)):
        return "direct", []
    fp = set(cells)
    step = {14: 1.5, 15: 3.0, 6: 3.0}
    heap, prev = [], {}
    for c in cells:
        for n in hex_nbrs(*c):
            if n not in fp and t[n] not in NAV and ok(*n):
                heapq.heappush(heap, (step.get(int(t[n]), 1.0), 1, n, None))
    while heap:
        d, k, v, p = heapq.heappop(heap)
        if v in prev:
            continue
        prev[v] = p
        if any(t[m] in NAV for m in hex_nbrs(*v)):
            path = []
            while v is not None:
                path.append(v)
                v = prev[v]
            opened = [h for h in path if int(t[h]) in OPEN_AS]
            return ("open" if opened else "land"), opened
        if k >= reach:
            continue
        for n in hex_nbrs(*v):
            if n not in prev and n not in fp and t[n] not in NAV and ok(*n):
                heapq.heappush(heap, (d + step.get(int(t[n]), 1.0), k + 1, n, v))
    return None, []


def wall_ou_cells(pack, clo, chi):
    """城墙（OBJS type 46/47/48）的 footprint 占用的世界 ou 格子。pack 是 Media/san11pkres.bin 的路径，
    clo/chi 是 China 块的世界 hex 偏移（ou 偏移 = 2*clo, 2*chi）。内政用地（type 45）的 footprint 和这些
    格子重叠时，游戏会拒绝创建它的 3D 物体，所以铺地时要用它避开城墙。"""
    if not Path(pack).exists():
        return set()
    with open(pack, "rb") as f:
        head = f.read(16)
        if head[:4] != b"LINK":
            return set()
        count = struct.unpack_from("<I", head, 4)[0]
        if 0x12C5 >= count:
            return set()
        f.seek(16 + 8 * 0x12C5)
        off, size = struct.unpack("<II", f.read(8))
        f.seek(off)
        objs = f.read(size)
    dy, dx = 2 * clo, 2 * chi
    fp = ((0, 0), (-1, 0), (0, -1), (-1, -1))
    out = set()
    for i in range(65535):
        rec = objs[8 + i * 14:8 + i * 14 + 14]
        if not rec[0] or struct.unpack_from("<H", rec, 1)[0] not in (46, 47, 48):
            continue
        our = struct.unpack_from("<H", rec, 3)[0] + dy
        ouc = struct.unpack_from("<H", rec, 5)[0] + dx
        for a, b in fp:
            out.add((our + a, ouc + b))
    return out


# Facility plots (内政用地): SHEX byte 5 = HEX20 bit18 (0x484090); on entering the map 0x57E910 lists each
# city's bit18 hexes (by area) in its +0xF0 table of 30, without a bound check. Koei gives every city 10, 12,
# 15, 18, 20 or 22 plots of soil (type 1), in 1-4 compact patches of 4-10, 3.5-14 hexes from the city, in its
# own area, at most 26% of the flat land there (every Koei city has 76 or more such hexes). New cities get the
# tier of their town's population about AD 200 (data/city_population.json, by name, "pop"); without one, of
# their rank; places that did not exist then get PLOT_GONE. They go 3-14 hexes out on flat land of the city's
# own territory, at most PLOT_LAND_SHARE of it, so cities with little land get fewer. A city short of its count
# that way may use up to PLOT_SHORT_SHARE of it; with --flatten-plots the cliffs nearest the city in its territory
# (FLATTEN_REACH hexes) become land until that share holds its count (FLATTEN_MARGIN more for the roads).
PLOT_TIERS = ((250000, 22), (100000, 20), (40000, 18), (15000, 15), (5000, 12), (0, 10))
RANK_PLOTS = {1: 15, 2: 12, 3: 10}
PLOT_GONE = 6
PLOT_LAND_SHARE = 0.25
PLOT_SHORT_SHARE = 0.5
FLATTEN_REACH = (3, 10)
FLATTEN_MARGIN = 1.5          # the roads connect_cities.py lays next take some of the flat land
PLOT_MIN = 2                                    # ... but this many wherever the land allows
PLOT_TERRAIN = (0, 1, 2, 3, 5, 9, 14)           # flat cover and banks (no roads, water, mountains)


def plot_count(c, pop):
    p = pop.get(c["name"], {})
    if p.get("existed_200") is False:
        return PLOT_GONE
    if p.get("pop"):
        return next(n for lim, n in PLOT_TIERS if p["pop"] >= lim)
    return RANK_PLOTS.get(c.get("rank", 3), PLOT_TIERS[-1][1])


def plot_dist(y0, x0, y, x):
    return np.hypot((y - y0) * 0.866, x + 0.5 * (y & 1) - x0 - 0.5 * (y0 & 1))


def plot_cands(y0, x0, ok):
    return {(y, x) for y in range(y0 - 17, y0 + 18) for x in range(x0 - 15, x0 + 16)
            if 3 <= plot_dist(y0, x0, y, x) <= 14 and ok(y, x)}


def place_plots(y0, x0, n, ok, share=PLOT_LAND_SHARE):
    """up to n plot hexes (and no more than the share of the land that may hold one) for the city at
    (y0, x0), in patches of about 7 grown around seeds spread round the city 4-9 hexes out; ok(y, x): the hex
    may be a plot of this city"""
    def dist(y, x):
        return plot_dist(y0, x0, y, x)
    cand = plot_cands(y0, x0, ok)
    n = min(n, max(PLOT_MIN, int(share * len(cand))), len(cand))
    if n <= 0:
        return []
    k = max(1, round(n / 7))
    seeds = []
    for _ in range(k):
        pool = [h for h in sorted(cand) if 4 <= dist(*h) <= 9] or sorted(cand)
        if not pool:
            break
        if seeds:
            h = max(pool, key=lambda h: min(np.hypot((h[0] - s[0]) * 0.866, h[1] - s[1]) for s in seeds))
        else:
            h = min(pool, key=lambda h: abs(dist(*h) - 6))
        seeds.append(h)
    def grow(s, size):
        patch = [s]
        cand.discard(s)
        while len(patch) < size:
            nxt = {h for p in patch for h in hex_nbrs(*p) if h in cand}
            if not nxt:
                break
            h = min(sorted(nxt), key=lambda h: np.hypot((h[0] - s[0]) * 0.866, h[1] - s[1]))
            patch.append(h); cand.discard(h)
        return patch
    out = []
    for i, s in enumerate(seeds):
        size = (n - len(out)) // (len(seeds) - i)      # what is left, spread over the patches still to grow
        if size > 0 and s in cand:
            out += grow(s, size)
    while len(out) < n and cand:                       # patches that could not grow: start more
        out += grow(min(sorted(cand), key=lambda h: abs(dist(*h) - 6)), n - len(out))
    return out


def flatten_city_sites(w, out, new, clo, chi, patch, sites=None):
    """Level the land around each new city to one height (Koei's cities stand on plateaus) and return
    those heights. sites: per city (vertex row, vertex col, r_flat, r_blend), the centre of its footprint (a
    triangle's model stands on the triangle's centre) and the radii by tier (TIER_FLAT); default: the centre
    hex, 8 and 12. Reads w/world_height.npy and w/world_mat.npy (material 0 = water, left alone) and
    writes out/world_height.npy; without vertex data (a --hex-only world) every city gets Koei's usual 20.
    A city inside Koei's block stands on his stage (the game draws his own there, which is not levelled):
    its height is his terrain height at the centre, so pick a flat site for it. The patch (make_world's
    china_patch(), 200x200 mask) is drawn from world_height.npy like the world outside."""
    src = w / "world_height.npy"
    if not src.exists() or not new:
        return [20] * len(new)
    dst = np.array(np.load(src))                     # in memory (~30 MB), so --out may equal the world directory
    mat = np.load(w / "world_mat.npy", mmap_mode="r")
    koei_v = np.load(DATA / "china_k3st_v.npy", mmap_mode="r")
    heights = []
    for i, c in enumerate(new):
        vr = c["lo"] * 4 + 114
        vc = c["hi"] * 4 + 114 + 2 * (c["lo"] & 1)
        if clo <= c["lo"] < clo + 200 and chi <= c["hi"] < chi + 200 and not patch[c["lo"] - clo, c["hi"] - chi]:
            heights.append(int(koei_v[vr - 4 * clo, vc - 4 * chi, 0]))
            continue
        fr, fc, r_flat, r_blend = sites[i] if sites else (vr, vc, 8, 12)
        r0, c0 = int(round(fr)), int(round(fc))
        yy, xx = np.mgrid[r0 - r_blend:r0 + r_blend + 1, c0 - r_blend:c0 + r_blend + 1]
        d = np.hypot(yy - fr, xx - fc)
        wgt = np.clip((r_blend - d) / (r_blend - r_flat), 0, 1)      # 1 inside r_flat, 0 beyond r_blend
        sl = (slice(r0 - r_blend, r0 + r_blend + 1), slice(c0 - r_blend, c0 + r_blend + 1))
        hw = dst[sl].astype(np.float32)
        land = mat[sl] != 0
        near = land & (d <= min(6, r_flat))
        h = int(np.median(hw[near])) if near.any() else 20
        h = max(8, min(250, h))
        m = land & (wgt > 0)
        hw[m] = wgt[m] * h + (1 - wgt[m]) * hw[m]
        dst[sl] = np.clip(np.round(hw), 0, 255).astype(np.uint8)
        heights.append(h)
    np.save(out / "world_height.npy", dst)
    return heights


# Territories tile the land like the pieces of a jigsaw puzzle, as Koei's areas tile China: every land
# hex belongs to the city that is cheapest to walk to (one Dijkstra from all cities at once over the hex
# grid). Mountains and above all rivers are expensive to cross, so borders follow ridges and rivers.
# Coastal waters (up to 6 hexes from land) belong to the city of the nearest coast; open sea, and land
# farther than max_cost from every city (Siberia, the Sahara), stay with the special area of the margins.
TERR_COST = {0: 1.0, 1: 1.0, 2: 1.2, 3: 1.6, 4: 1.6, 5: 1.4, 6: 3.0, 7: 7.0, 8: 4.0, 9: 1.2, 10: 1.0, 11: 2.0,
             12: 2.0, 13: 1.5, 14: 1.0, 15: 3.0, 16: 1.0, 17: 1.0, 18: 1.0, 19: 1.0}


def hex_graph(cost):
    """hex grid graph (lo = row, odd rows half a hex further along hi); edge weight = cost of entering"""
    from scipy.sparse import csr_matrix
    R, C = cost.shape
    idx = np.arange(R * C).reshape(R, C)
    a_list = [idx[:, :-1].ravel(), idx[:-1].ravel()]
    b_list = [idx[:, 1:].ravel(), idx[1:].ravel()]
    rows = np.arange(R - 1)
    ev, od = rows[rows % 2 == 0], rows[rows % 2 == 1]
    a_list += [idx[ev, 1:].ravel(), idx[od, :-1].ravel()]
    b_list += [idx[ev + 1, :-1].ravel(), idx[od + 1, 1:].ravel()]
    a = np.concatenate(a_list); b = np.concatenate(b_list)
    w = cost.ravel()
    src = np.concatenate([a, b]); dst = np.concatenate([b, a])
    ok = np.isfinite(w[dst]) & np.isfinite(w[src])
    return csr_matrix((w[dst][ok].astype(np.float32), (src[ok], dst[ok])), shape=(R * C, R * C))


# The seam with Koei's China: Koei's areas fill his 200x200 block right up to its edge (the outer ones are
# decorative mountain margins), so territories that simply stopped at the block would meet his along a
# straight line. Instead the outer BAND hexes of the block are contested too: the seeds are the new cities
# and every block hex deeper than BAND (with its Koei area), and the band and the world outside go to
# the cheapest seed. A Koei seed starts with the walking cost from the nearest Koei city (so his areas
# grow outward around his cities, not as a strip parallel to the block edge); the decorative margins
# (special areas) start at 0. A band hex changes only when a new city wins it (Koei's own areas never move
# inside the block, and nothing changes next to a Koei base or on his facility plots); outside the block
# Koei's areas continue.
BAND = 12


def territories(t, new, clo, chi, china_area, koei_pos, coast=6, max_cost=60.0):
    """area of every hex outside the core of the block (new cities by their id, Koei areas in Koei numbering
    as negative -1 - a, -2**30 = none) and the band hexes a new city wins (mask)"""
    from scipy.sparse.csgraph import dijkstra
    R, C = t.shape
    cost = np.full(t.shape, np.inf, np.float32)
    for k, c in TERR_COST.items():
        cost[t == k] = c
    g = hex_graph(cost)
    ii, jj = np.mgrid[0:200, 0:200]
    depth = np.minimum(np.minimum(ii, 199 - ii), np.minimum(jj, 199 - jj))
    core = depth >= BAND
    core_lo, core_hi = np.nonzero(core)
    seeds = np.concatenate([np.array([c["lo"] * C + c["hi"] for c in new], np.int64),
                            ((core_lo + clo) * C + core_hi + chi).astype(np.int64)])
    label = np.concatenate([np.array([c["id"] for c in new], np.int64), -1 - china_area[core_lo, core_hi]])
    # start cost of the Koei seeds: walking cost from the nearest Koei city (0 for the special areas)
    kcity = np.array([lo_ * C + hi_ for lo_, hi_ in koei_pos], np.int64)
    dk = dijkstra(g, indices=kcity, min_only=True)
    special = china_area[core_lo, core_hi] >= 87
    start = np.concatenate([np.zeros(len(new)), np.where(special, 0.0, dk[seeds[len(new):]])])
    start = np.where(np.isfinite(start), start, 0.0)
    # one Dijkstra from a virtual node V joined to every seed by an edge of its start cost
    from scipy.sparse import csr_matrix, vstack, hstack
    V = R * C
    row = csr_matrix((start + 1e-3, (np.zeros(len(seeds), np.int64), seeds)), shape=(1, V + 1))
    G = vstack([hstack([g, csr_matrix((V, 1))]), row]).tocsr()
    dist, pred = dijkstra(G, indices=V, return_predecessors=True)
    dist = dist[:V].reshape(R, C)
    pred = pred[:V].astype(np.int64)
    # the seed every hex was reached from: pointer jumping up the shortest-path tree
    seed_of = np.where(pred == V, np.arange(V), pred)
    seed_of[pred < 0] = -1
    for _ in range(40):
        ok = seed_of >= 0
        nxt = seed_of.copy()
        nxt[ok] = seed_of[seed_of[ok]]
        if np.array_equal(nxt, seed_of):
            break
        seed_of = nxt
    lut = np.full(V, 0, np.int64)
    lut[seeds] = np.arange(len(seeds))
    lab = np.full(V, -2 ** 30, np.int64)
    ok = seed_of >= 0
    lab[ok] = label[lut[seed_of[ok]]]
    lab = lab.reshape(R, C)
    lab[dist > max_cost] = -2 ** 30
    sea = t == 8
    lab[sea & (ndimage.distance_transform_edt(sea) > coast)] = -2 ** 30   # open sea
    # band: only where a new city wins, not on or next to Koei's bases (terrain 16 city, 17 port, 18 gate)
    blk_t = t[clo:clo + 200, chi:chi + 200]
    near_base = ndimage.binary_dilation(np.isin(blk_t, (16, 17, 18)), iterations=2)
    blab = lab[clo:clo + 200, chi:chi + 200]
    won = (~core) & (blab >= 42) & ~near_base
    return lab, won


# New cities inside Koei's block take their territory out of his areas (his gates' and ports' too, which
# reach far inland): a block hex goes to the cheapest of them when walking there from it is cheaper than
# from every other base. The hexes on or next to Koei's bases, his facility plots and open sea never change
# hands; everything else in the block stays as Koei drew it.
def block_territories(t, cities, others, clo, chi, plots, max_cost=60.0, m=80):
    """block-local labels (city id, -1 = Koei's area stays) for the new cities inside the block
    (cities: [{id, lo, hi}]); others: [(lo, hi)] of every other base (Koei's base hexes, the new cities outside)"""
    from scipy.sparse.csgraph import dijkstra
    R, C = t.shape
    y0, y1 = max(0, clo - m), min(R, clo + 200 + m)
    y0 -= y0 % 2                                          # keep row parity
    x0, x1 = max(0, chi - m), min(C, chi + 200 + m)
    tw = t[y0:y1, x0:x1]
    cost = np.full(tw.shape, np.inf, np.float32)
    for k, c in TERR_COST.items():
        cost[tw == k] = c
    g = hex_graph(cost)
    W = x1 - x0
    src = [(c["lo"] - y0) * W + c["hi"] - x0 for c in cities]
    d_new, _, src_of = dijkstra(g, indices=src, min_only=True, return_predecessors=True)
    d_other = dijkstra(g, indices=[(y - y0) * W + x - x0 for y, x in others if y0 <= y < y1 and x0 <= x < x1],
                       min_only=True)
    sl = (slice(clo - y0, clo - y0 + 200), slice(chi - x0, chi - x0 + 200))
    d_new, d_other, src_of = (a_.reshape(y1 - y0, W)[sl] for a_ in (d_new, d_other, src_of))
    tb = t[clo:clo + 200, chi:chi + 200]
    near_base = ndimage.binary_dilation(np.isin(tb, (16, 17, 18)), iterations=2)
    take = (d_new < d_other) & (d_new <= max_cost) & ~near_base & (plots == 0) & (tb != 8)
    cid = {s: c["id"] for s, c in zip(src, cities)}
    out = np.full((200, 200), -1, np.int64)
    out[take] = [cid[s] for s in src_of[take]]
    return out


def bfs_matrix(edges, n, sources, nodes_ok):
    """hop counts between sources; paths may only pass through nodes for which nodes_ok(v) is true"""
    M = np.full((len(sources), len(sources)), 255, np.uint8)
    idx = {s: i for i, s in enumerate(sources)}
    for s in sources:
        dist = {s: 0}
        q = deque([s])
        while q:
            u = q.popleft()
            if u != s and not nodes_ok(u):
                continue
            for v in edges[u]:
                if v not in dist:
                    dist[v] = dist[u] + 1
                    q.append(v)
        for v, d in dist.items():
            if v in idx:
                M[idx[s], idx[v]] = min(d, 254)
    return M


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("world", nargs="?", default=str(DATA / "world"))
    ap.add_argument("--cities", type=int, required=True)
    ap.add_argument("--verify", action="store_true", help="with --cities 42: compare with the exe's tables")
    ap.add_argument("--out", help="output directory (default: the world directory); the inputs are never modified")
    ap.add_argument("--game", default=str(GAME), help="game directory (san11pk.exe and Media/scenario/Scenario.s11 are read)")
    ap.add_argument("--flatten-plots", action="store_true",
                    help="make cliffs land where a city is short of facility plots (into world_dir's world_types)")
    a = ap.parse_args()
    global EXE, SCENARIO
    EXE = Path(a.game) / "san11pk.exe"
    SCENARIO = Path(a.game) / "Media" / "scenario" / "Scenario.s11"
    for f in (EXE, SCENARIO):
        if not f.exists():
            raise SystemExit(f"{f} not found: --game must be the game directory")
    C = a.cities
    if not 42 <= C <= 1000:
        raise SystemExit("--cities must be 42..1000")
    N = C + 45
    dC = C - 42
    w = Path(a.world)
    info = json.loads((w / "world_info.json").read_text())
    R, Cc = info["rows"], info["cols"]
    clo, chi = info["china_lo"], info["china_hi"]
    t = np.load(w / "world_types.npy")
    shex = np.fromfile(w / "world_shex.bin", np.uint8)
    hdr, rec = shex[:12], shex[12:].reshape(R, Cc, 11).copy()

    # ---- original tables, renumbered: gates/ports 42..86 -> C..N-1, special 87..92 -> N..N+5
    def renum(a_):
        a_ = int(a_)
        return a_ if a_ < 42 else a_ + dC
    adj0 = np.frombuffer(exe_bytes(0x796768, 93 * 56), np.int16).reshape(93, 7, 4)
    a2c0 = np.frombuffer(exe_bytes(0x79c2b0, 93), np.uint8)
    tab0 = np.frombuffer(exe_bytes(0x844130, 42 * 4), np.int32)
    dbg0 = np.frombuffer(exe_bytes(0x7e7bd8, 87 * 16), np.uint8).reshape(87, 16)
    master = SCENARIO.read_bytes()
    koei_nb = []
    koei_prov = []
    for i in range(42):
        r = master[0x5a + 25 * i:0x5a + 25 * i + 25]
        koei_prov.append(r[18])
        koei_nb.append([x for x in r[19:25] if x != 255])

    adj = {}                       # area -> list of (neighbour, route type)
    for u in range(93):
        adj[renum(u)] = [(renum(v), int(rt), int(p1), int(p2)) for v, rt, p1, p2 in adj0[u] if v >= 0]   # p1/p2: a hex for types 1/4
    city_nb = {i: list(koei_nb[i]) for i in range(42)}

    # ---- new cities and their territories
    new = json.loads((w / "cities_world.json").read_text(encoding="utf-8"))[:dC]
    koei = json.loads((DATA / "cities_georef.json").read_text(encoding="utf-8"))
    area = np.full((R, Cc), N + 3, np.int32)              # unclaimed world: the special area Koei used for its margins (90)
    china_area = rec[clo:clo + 200, chi:chi + 200, 1].astype(np.int32) | (rec[clo:clo + 200, chi:chi + 200, 2].astype(np.int32) << 8)
    seeds = [{**c, "id": 42 + k} for k, c in enumerate(new)]
    for k, c in enumerate(new):
        if c.get("id", 42 + k) != 42 + k:
            raise SystemExit(f"cities_world.json: {c['name']} has id {c['id']}, expected {42 + k}")
        c["id"] = 42 + k
    # footprints by tier (M11_MAP.md); the triangles are chosen on the terrain without roads, so both of the
    # build's gen_bases.py runs (before and after connect_cities.py) choose the same ones
    tiers = load_tiers(new)
    tb = t.copy()
    if (w / "world_types.before_roads.npy").exists():
        tb = np.load(w / "world_types.before_roads.npy")
        tb = tb if tb.shape == t.shape else t.copy()
    # river ports (data/navigable_rivers.json, tools/navigable.py): a 河 channel from the city to the open sea,
    # derived from the terrain without roads (so connect_cities.py lays the same one), written into it and into
    # t where t still has that terrain (after connect_cities.py and make_world.py it is there already)
    import navigable
    from worldgeo import WorldSpec
    spec = WorldSpec(rows=R, cols=Cc, lon0=info["lon0"], lat0=info["lat0"], china_lo=clo, china_hi=chi)
    tbase = navigable.base_terrain(w, t, spec)
    rivers = navigable.courses(tbase, new, {k: v[0] for k, v in tiers.items()}, spec,
                               np.load(w / "china_patch.npy") if (w / "china_patch.npy").exists() else None)
    for r in rivers:
        for y, x, k_ in r["hexes"] or []:
            tb[y, x] = k_
    river_blk = {}                                        # block hexes changed (block coordinates -> terrain)
    for y, x, k_ in navigable.apply(t, tbase, rivers):
        if clo <= y < clo + 200 and chi <= x < chi + 200:
            river_blk[(y - clo, x - chi)] = k_
        else:
            rec[y, x, 0], rec[y, x, 4] = k_, 2 if k_ == 7 else 6

    def in_block(y, x):
        return clo <= y < clo + 200 and chi <= x < chi + 200
    blocked = np.zeros((R, Cc), bool)                     # Koei's bases, facility plots, dikes; other footprints
    kb = rec[clo:clo + 200, chi:chi + 200]
    blocked[clo:clo + 200, chi:chi + 200] = (np.isin(t[clo:clo + 200, chi:chi + 200], (16, 17, 18))
                                             | (kb[..., 5] != 0) | (kb[..., 3] != 0))
    shape = {}
    for c in new:
        tier, _ = tiers[c["id"]]
        if tier != 2:
            shape[c["id"]] = choose_shape(c["lo"], c["hi"], tier, False, tb, None)
            for y, x in shape_cells(c["lo"], c["hi"], shape[c["id"]]):
                blocked[y, x] = True
    no_triangle = []
    for c in new:
        tier, port = tiers[c["id"]]
        if tier != 2:
            continue
        in0 = in_block(c["lo"], c["hi"])
        blocked[c["lo"], c["hi"]] = False
        s_ = choose_shape(c["lo"], c["hi"], tier, port, tb,
                          lambda y, x: 0 <= y < R and 0 <= x < Cc and not blocked[y, x] and in_block(y, x) == in0, c["id"])
        if s_ is None:
            no_triangle.append(c["name"]); s_ = SHAPE_1
        shape[c["id"]] = s_
        for y, x in shape_cells(c["lo"], c["hi"], s_):
            blocked[y, x] = True
    cells_of = {c["id"]: shape_cells(c["lo"], c["hi"], shape[c["id"]]) for c in new}
    inside = []                                           # new cities inside Koei's block (on his own terrain)
    for c in new:
        ins = [in_block(y, x) for y, x in cells_of[c["id"]]]
        if any(ins) and not all(ins):
            raise SystemExit(f"{c['name']}: its {len(ins)} hexes cross the edge of Koei's China block; move it fully inside or outside")
        inside.append(ins[0])
    lab, won = territories(t, [s for s, i in zip(seeds, inside) if not i], clo, chi, china_area,
                           [(k["lo"] + clo, k["hi"] + chi) for k in koei])
    unclaimed = lab == -2 ** 30
    koei_lab = (lab < 0) & ~unclaimed
    area[lab >= 42] = lab[lab >= 42]
    area[koei_lab] = np.vectorize(renum)(-1 - lab[koei_lab])              # Koei's areas continued outward
    area[clo:clo + 200, chi:chi + 200] = -1               # China keeps Koei's own areas (remapped by worldmod)
    band_new = np.where(won & (rec[clo:clo + 200, chi:chi + 200, 5] == 0),     # band hexes taken by new cities
                        lab[clo:clo + 200, chi:chi + 200], -1)
    if any(inside):
        bases = [(y + clo, x + chi) for y, x in zip(*np.nonzero(np.isin(t[clo:clo + 200, chi:chi + 200], (16, 17, 18))))]
        blk_lab = block_territories(t, [s for s, i in zip(seeds, inside) if i],
                                    bases + [(s["lo"], s["hi"]) for s, i in zip(seeds, inside) if not i],
                                    clo, chi, rec[clo:clo + 200, chi:chi + 200, 5])
        band_new = np.where(blk_lab >= 0, blk_lab, band_new)
    foot_blk = np.zeros((200, 200), bool)                 # city hexes of the new cities inside the block
    for k, c in enumerate(new):
        cid = 42 + k
        for y, x in cells_of[cid]:
            t[y, x] = CITY_T
            area[y, x] = cid
            rec[y, x, 0] = CITY_T
            rec[y, x, 4] = 6
            if inside[k]:
                band_new[y - clo, x - chi] = cid
                foot_blk[y - clo, x - chi] = True
    # ports: a way from the city to navigable water (河/海) so units can sail out. Cities flagged "port" in
    # city_tiers.json get one up to PORT_REACH hexes long, every other city only when one bank or cliff hex
    # parts it from the water; banks, cliffs and streams on the way are opened (OPEN_AS), as connect_cities.py
    # opens the banks where a sea route lands. Koei's bases, plots, dikes and the city hexes stay as they are.
    foot_all = {h for cs in cells_of.values() for h in cs}
    blk_edits = dict(river_blk)                           # block hexes opened (block coordinates -> terrain)

    def open_ok(y, x):
        return (0 <= y < R and 0 <= x < Cc and (y, x) not in foot_all and t[y, x] not in (16, 17, 18)
                and not (in_block(y, x) and (rec[y, x, 5] or rec[y, x, 3])))
    port_how, port_open, port_fail = {}, [], []
    for c in new:
        cid = c["id"]
        tier, port = tiers[cid]
        how, opened = port_access(t, cells_of[cid], PORT_REACH if port else 1, open_ok)
        if not port and how == "open" and any(int(t[h]) == 6 for h in opened):
            how, opened = None, []                         # not a port: no ferries over streams
        port_how[cid] = how
        if port and how is None:
            port_fail.append(c["name"])
        for y, x in opened:
            k_ = OPEN_AS[int(t[y, x])]
            t[y, x] = k_
            port_open.append([int(y), int(x), int(k_)])
            if in_block(y, x):
                blk_edits[(y - clo, x - chi)] = k_
            else:
                rec[y, x, 0], rec[y, x, 4] = k_, 6
    koei_port = []
    for k in koei:
        y, x = k["lo"] + clo, k["hi"] + chi
        how, _ = port_access(t, [(y, x)] + hex_nbrs(y, x), 1, lambda y, x: t[y, x] not in (6, 14, 15, 16, 17, 18))
        koei_port.append(how is not None)

    # facility plots: soil hexes flagged in SHEX byte 5, in the city's own territory
    pf = DATA / "city_population.json"
    pop = json.loads(pf.read_text(encoding="utf-8")) if pf.exists() else {}
    plot_blk = np.zeros((200, 200), bool)
    plots_made, plots_short = 0, []
    wall_ou = wall_ou_cells(Path(a.game) / "Media" / "san11pkres.bin", clo, chi)

    def plot_free(y, x):
        our, ouc = 2 * y + 57, 2 * x + 57 + (y & 1)
        return all((our + a, ouc + b) not in wall_ou for a, b in ((0, 0), (-1, 0), (0, -1), (-1, -1)))

    def owner(y, x):
        if clo <= y < clo + 200 and chi <= x < chi + 200:
            return band_new[y - clo, x - chi]
        return area[y, x]
    flattened = []
    if a.flatten_plots:
        import heapq
        made = {}
        for k, c in enumerate(new):
            cid, want = 42 + k, plot_count(c, pop)
            y0, x0 = c["lo"], c["hi"]
            have = len(plot_cands(y0, x0, lambda y, x: 0 <= y < R and 0 <= x < Cc and owner(y, x) == cid
                                  and t[y, x] in PLOT_TERRAIN and not rec[y, x, 5] and plot_free(y, x)))
            need = int(np.ceil(FLATTEN_MARGIN * want / PLOT_SHORT_SHARE)) - have
            if need <= 0:
                continue

            def cliff(y, x):
                return (0 <= y < R and 0 <= x < Cc and not in_block(y, x) and t[y, x] == 15 and owner(y, x) == cid
                        and FLATTEN_REACH[0] <= plot_dist(y0, x0, y, x) <= FLATTEN_REACH[1])
            # the cliffs nearest the city first
            heap = [(plot_dist(y0, x0, y, x), y, x) for y in range(y0 - 12, y0 + 13) for x in range(x0 - 12, x0 + 13)
                    if cliff(y, x)]
            heapq.heapify(heap)
            n = 0
            while heap and n < need:
                _, y, x = heapq.heappop(heap)
                if t[y, x] != 15:
                    continue
                ks = [int(t[h]) for h in hex_nbrs(y, x) if 0 <= h[0] < R and 0 <= h[1] < Cc and t[h] in (0, 1, 2, 3, 5, 9)]
                t[y, x] = max(set(ks), key=ks.count) if ks else 0
                rec[y, x, 0] = t[y, x]
                made[(y, x)] = int(t[y, x])
                n += 1
            if n:
                flattened.append(f"{c['name']} {n}")
        for name in ("world_types.npy", "world_types.before_roads.npy"):
            f = w / name
            if f.exists() and made:
                tw = np.load(f)
                for (y, x), k_ in made.items():
                    if tw[y, x] == 15:
                        tw[y, x] = k_
                np.save(f, tw)
        print(f"flatten-plots: {len(made)} cliffs made land for {len(flattened)} cities short of facility plots"
              + (f": {', '.join(flattened)}" if flattened else ""))
    for k, c in enumerate(new):
        cid, want = 42 + k, plot_count(c, pop)

        def ok(y, x):
            return (0 <= y < R and 0 <= x < Cc and owner(y, x) == cid
                    and t[y, x] in PLOT_TERRAIN and not rec[y, x, 5] and plot_free(y, x))
        got = place_plots(c["lo"], c["hi"], want, ok)
        if len(got) < want:
            got = place_plots(c["lo"], c["hi"], want, ok, PLOT_SHORT_SHARE)
        for y, x in got:
            t[y, x] = 1
            if clo <= y < clo + 200 and chi <= x < chi + 200:
                plot_blk[y - clo, x - chi] = True
            else:
                rec[y, x, 0], rec[y, x, 4], rec[y, x, 5] = 1, 6, 1
        plots_made += len(got)
        if len(got) < want:
            plots_short.append(f"{c['name']} {len(got)}/{want}")

    # ---- adjacency between areas (whole world, China included with Koei's ids renumbered)
    full = area.copy()
    full[clo:clo + 200, chi:chi + 200] = np.where(band_new >= 0, band_new, np.vectorize(renum)(china_area))
    passable = ~np.isin(t, WATER) & (t != 15)
    border = defaultdict(lambda: [0, 0])                  # (a, b) -> [land contacts, water contacts]
    for dy, dx_even, dx_odd in ((0, 1, 1), (1, -1, 0), (1, 0, 1)):
        for parity in (0, 1):
            dx = dx_even if parity == 0 else dx_odd
            ys = np.arange(parity, R - dy, 2)
            if dx >= 0:
                A_ = full[ys][:, :Cc - dx]; B_ = full[ys + dy][:, dx:]
                pa = passable[ys][:, :Cc - dx]; pb = passable[ys + dy][:, dx:]
            else:
                A_ = full[ys][:, -dx:]; B_ = full[ys + dy][:, :Cc + dx]
                pa = passable[ys][:, -dx:]; pb = passable[ys + dy][:, :Cc + dx]
            m = A_ != B_
            for u, v, land in zip(A_[m].ravel(), B_[m].ravel(), (pa & pb)[m].ravel()):
                key = (int(min(u, v)), int(max(u, v)))
                border[key][0 if land else 1] += 1
    newareas = set(range(42, C))
    cand = defaultdict(list)
    for (u, v), (land, wat) in border.items():
        if u >= N or v >= N:                               # special areas get no new links
            continue
        if u not in newareas and v not in newareas:        # Koei's own links stay as they are
            continue
        rt = 0 if land >= 2 else 3
        score = land * 10 + wat
        cand[u].append((score, v, rt))
        cand[v].append((score, u, rt))
    for u in sorted(cand):
        cand[u].sort(reverse=True)
    for u in range(42, C):
        adj[u] = []
    added = 0
    for u in sorted(cand):
        for score, v, rt in cand[u]:
            if len(adj.setdefault(u, [])) >= 7 or len(adj.setdefault(v, [])) >= 7:
                continue
            if any(e[0] == v for e in adj[u]):
                continue
            adj[u].append((v, rt, -1, -1))
            adj[v].append((u, rt, -1, -1))
            added += 1
    # city adjacency (<= 6): direct city-to-city area links
    for u in range(C):
        city_nb.setdefault(u, [])
    for u in range(42, C):
        for v, rt, _, _ in sorted(adj[u], key=lambda e: -border[(min(u, e[0]), max(u, e[0]))][0]):
            if v < C and v not in city_nb[u] and len(city_nb[u]) < 6 and len(city_nb[v]) < 6:
                city_nb[u].append(v)
                city_nb[v].append(u)

    # make the city graph connected: nearest city pairs first, across components first (Kruskal), then
    # give every new city at least 2 neighbours; links keep the 6-city / 7-area limits
    pos = {i: (k["lo"] + clo, k["hi"] + chi + 0.5 * ((k["lo"] + clo) & 1)) for i, k in enumerate(koei)}
    for c in new:
        pos[c["id"]] = (c["lo"], c["hi"] + 0.5 * (c["lo"] & 1))
    ids = sorted(pos)
    P = np.array([pos[i] for i in ids])
    D = np.hypot(P[:, None, 0] - P[None, :, 0], P[:, None, 1] - P[None, :, 1])
    pairs = sorted((D[i, j], ids[i], ids[j]) for i in range(len(ids)) for j in range(i + 1, len(ids))
                   if D[i, j] <= 160 and (ids[i] >= 42 or ids[j] >= 42))
    parent = list(range(C))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for u in range(C):
        for v in city_nb[u]:
            parent[find(u)] = find(v)

    def link(u, v, d):
        if len(city_nb[u]) >= 6 or len(city_nb[v]) >= 6 or len(adj[u]) >= 7 or len(adj[v]) >= 7:
            return False
        if v in city_nb[u]:
            return False
        # route type: land if the straight line is mostly land, else water
        ys = np.linspace(pos[u][0], pos[v][0], 40).round().astype(int)
        xs = np.linspace(pos[u][1], pos[v][1], 40).round().astype(int).clip(0, Cc - 1)
        rt = 0 if (~np.isin(t[ys.clip(0, R - 1), xs], WATER)).mean() > 0.6 else 3
        city_nb[u].append(v); city_nb[v].append(u)
        if not any(e[0] == v for e in adj[u]):
            adj[u].append((v, rt, -1, -1)); adj[v].append((u, rt, -1, -1))
        return True
    extra = 0
    for d, u, v in pairs:
        if find(u) != find(v) and link(u, v, d):
            parent[find(u)] = find(v); extra += 1
    for d, u, v in pairs:
        if d <= 45 and (len(city_nb[u]) < 2 and u >= 42 or len(city_nb[v]) < 2 and v >= 42) and link(u, v, d):
            extra += 1

    # ---- tables
    AREAADJ = np.full((N + 6, 7, 4), -1, np.int16)
    for u, lst in adj.items():
        for k, e in enumerate(lst[:7]):
            AREAADJ[u, k] = e
    edges_a = {u: [e[0] for e in adj.get(u, []) if e[1] in (0, 3, 5)] for u in range(N + 6)}
    edges_b = {u: [e[0] for e in adj.get(u, [])] for u in range(N + 6)}
    MAT87A = bfs_matrix(edges_a, N + 6, list(range(N)), lambda v: v < N)
    MAT87B = bfs_matrix(edges_b, N + 6, list(range(N)), lambda v: v < N)
    MATC = bfs_matrix({u: city_nb[u] for u in range(C)}, C, list(range(C)), lambda v: True)
    AREA2CITY = np.zeros(N + 6, np.uint16)          # u16 since phase C (the game's table is u8)
    AREA2CITY[:C] = np.arange(C)
    AREA2CITY[C:N] = a2c0[42:87]
    AREA2CITY[N:N + 6] = a2c0[87:93]
    AREA2BASE = np.concatenate([np.arange(N), [-1, -8, -21, -24, -39, -41]]).astype(np.int32)
    CITYTAB844 = np.concatenate([tab0, np.full(dC, 12)]).astype(np.int32)
    DBGBASE = np.zeros((N, 16), np.uint8)
    DBGBASE[:42] = dbg0[:42]
    DBGBASE[C:N] = dbg0[42:87]

    if a.verify:
        ok = {
            "AREAADJ": AREAADJ.tobytes() == exe_bytes(0x796768, 93 * 56),
            "MAT87A": MAT87A.tobytes() == exe_bytes(0x797bc0, 87 * 87),
            "MAT87B": MAT87B.tobytes() == exe_bytes(0x799958, 87 * 87),
            "MATC": MATC.tobytes() == exe_bytes(0x79b830, 42 * 42),
            "AREA2CITY": AREA2CITY.astype(np.uint8).tobytes() == exe_bytes(0x79c2b0, 93),
            "AREA2BASE": AREA2BASE.tobytes() == exe_bytes(0x79c358, 93 * 4),
            "CITYTAB844": CITYTAB844.tobytes() == exe_bytes(0x844130, 42 * 4),
            "DBGBASE": DBGBASE.tobytes() == exe_bytes(0x7e7bd8, 87 * 16),
        }
        print("verify against the exe:", ok)

    out = Path(a.out) if a.out else w
    out.mkdir(parents=True, exist_ok=True)
    with open(out / "bases_tables.bin", "wb") as f:
        f.write(b"WBTB" + struct.pack("<I", C))
        for arr in (AREAADJ, MAT87A, MAT87B, MATC, AREA2CITY, AREA2BASE, CITYTAB844, DBGBASE):
            b = arr.tobytes()
            f.write(struct.pack("<I", len(b)) + b)
    # world SHEX with the territories (China's block is replaced by worldmod with Koei's, remapped)
    outside = area >= 0
    rec[..., 1][outside] = (area[outside] & 0xff).astype(np.uint8)
    rec[..., 2][outside] = (area[outside] >> 8).astype(np.uint8)
    (out / "world_shex.bin").write_bytes(hdr.tobytes() + rec.tobytes())
    # china_overrides.bin, final numbering ("WCO2": worldmod applies it after renumbering Koei's ids): the
    # make_world overrides (Koei numbering) renumbered, plus the band hexes new cities took
    ov = {}
    src_ov = w / "china_overrides.bin"
    if src_ov.exists():
        b = src_ov.read_bytes()
        if b[:4] == b"WCOV":
            n = struct.unpack_from("<I", b, 4)[0]
            for i in range(n):
                lo_, hi_ = struct.unpack_from("<HH", b, 8 + 15 * i)
                r = bytearray(b[12 + 15 * i:8 + 15 * (i + 1)])
                a_ = renum(r[1] | (r[2] << 8))
                r[1], r[2] = a_ & 0xff, a_ >> 8
                ov[(lo_, hi_)] = r
    for (lo_, hi_), k_ in blk_edits.items():             # ways to the water opened, river channels in the block
        r = ov.get((lo_, hi_), bytearray(rec[clo + lo_, chi + hi_]))
        r[0], r[4] = k_, 2 if k_ == 7 else 6
        ov[(lo_, hi_)] = r
    for lo_, hi_ in zip(*np.nonzero(band_new >= 0)):
        r = ov.get((int(lo_), int(hi_)), bytearray(rec[clo + lo_, chi + hi_]))
        a_ = int(band_new[lo_, hi_])
        r[1], r[2] = a_ & 0xff, a_ >> 8
        if foot_blk[lo_, hi_]:                            # city hex: as Koei's city hexes (flags clear)
            r[0], r[4] = CITY_T, 6
            r[3] = r[5] = r[6] = r[7] = r[8] = r[10] = 0
        if plot_blk[lo_, hi_]:                            # facility plot: soil, byte 5 set
            r[0], r[4], r[5] = 1, 6, 1
        ov[(int(lo_), int(hi_))] = r
    with open(out / "china_overrides.bin", "wb") as f:
        f.write(b"WCO2" + struct.pack("<I", len(ov)))
        for (lo_, hi_), r in sorted(ov.items()):
            f.write(struct.pack("<HH", lo_, hi_) + bytes(r))
    np.save(out / "world_types.npy", t)
    # 3D city models for the new cities: OBJS records (type 0 = city model) in free object slots; worldmod
    # adds them when it loads Koei's OBJS. bases_objs.bin: "WBOB", u32 n, n x {u16 slot, 14-byte record}.
    # The record's height byte is the ground height the model stands on: in Koei's OBJS it always equals the
    # terrain height at the city centre, and the terrain around the city is a plateau at that height. So each
    # new city gets the median land height near its centre, and the land within 8 vertices is levelled to it
    # (blended out to 12), in a copy of world_height.npy written to the output directory.
    patch = np.load(w / "china_patch.npy") if (w / "china_patch.npy").exists() else np.zeros((200, 200), bool)
    sites = []
    for c in new:
        cs = cells_of[c["id"]] if shape[c["id"]] not in (SHAPE_7, SHAPE_1) else [(c["lo"], c["hi"])]
        vr = np.mean([y * 4 + 114 for y, x in cs])
        vc = np.mean([x * 4 + 114 + 2 * (y & 1) for y, x in cs])
        sites.append((vr, vc) + TIER_FLAT[max(1, min(3, tiers[c["id"]][0]))])
    heights = flatten_city_sites(w, out, new, clo, chi, patch, sites)
    if out.resolve() != w.resolve():                      # worldmod draws the patch from world_height.npy
        if (w / "china_patch.bin").exists():
            shutil.copy2(w / "china_patch.bin", out / "china_patch.bin")
        else:
            (out / "china_patch.bin").unlink(missing_ok=True)
    # the record's variant byte picks the model: 0 toM (tier 1), 2 toL (the LARGEST metropolises), 1 toS
    # (tiers 2 and 3; worldmod scales it further by tier, M11_MAP.md)
    variant = {c["id"]: (2 if c["name"] in LARGEST else 0) if tiers[c["id"]][0] <= 1 else 1 for c in new}
    with open(out / "bases_objs.bin", "wb") as f:
        f.write(b"WBOB" + struct.pack("<I", len(new)))
        for k, c in enumerate(new):
            our = 2 * c["lo"] + 57
            ouc = 2 * c["hi"] + 57 + (c["lo"] & 1)
            f.write(struct.pack("<H", 60000 + k) + struct.pack("<BHHHBfBB", 1, 0, our, ouc, heights[k], 0.0,
                                                               variant[c["id"]], 0))
    # city_shapes.bin (M11_MAP.md): "WCSH", u32 version 1, u32 C, C x {u8 shape, u8 tier, u8 variant, u8 flags};
    # flags bit0 = port (a way to navigable water from the city)
    with open(out / "city_shapes.bin", "wb") as f:
        f.write(b"WCSH" + struct.pack("<II", 1, C))
        for i in range(C):
            if i < 42:
                f.write(struct.pack("<4B", SHAPE_7, 0, 0, int(koei_port[i])))
            else:
                f.write(struct.pack("<4B", shape[i], tiers[i][0], variant[i], int(port_how[i] is not None)))
    # new city records for the scenario converter
    kpos = np.array([[k["lo"] + clo, k["hi"] + chi] for k in koei], float)
    cities = []
    for k, c in enumerate(new):
        j = int(np.argmin(np.hypot(kpos[:, 0] - c["lo"], kpos[:, 1] - c["hi"])))
        cities.append({"id": 42 + k, "name": c["name"], "full": c.get("full", c["name"]), "lo": c["lo"], "hi": c["hi"],
                       "province": int(koei_prov[koei[j]["id"]]), "neighbours": city_nb[42 + k],
                       "tier": tiers[42 + k][0], "shape": shape[42 + k], "cells": [list(map(int, h)) for h in cells_of[42 + k]]})
    koei_changed = {i: city_nb[i] for i in range(42) if city_nb[i] != koei_nb[i]}
    (out / "bases_cities.json").write_text(json.dumps({"C": C, "N": N, "new": cities, "koei_neighbours": koei_changed,
                                                       "port_open": port_open},
                                                      ensure_ascii=False, indent=1), encoding="utf-8")
    area[clo:clo + 200, chi:chi + 200] = -1               # the block's hexes are counted from band_new
    owned = np.concatenate([area[(area >= 42) & (area < C)].ravel(), band_new[band_new >= 0].ravel()])
    sizes = np.bincount(owned - 42, minlength=dC) if dC else []
    print(f"C={C} N={N}: {dC} new cities, {added} border links + {extra} distance links, territory sizes "
          f"{(int(np.min(sizes)), int(np.median(sizes)), int(np.max(sizes))) if dC else '-'}; "
          f"unreachable city pairs {int((MATC == 255).sum())}, base pairs {int((MAT87B == 255).sum())}")
    from collections import Counter
    nt = Counter(tiers[c["id"]][0] for c in new)
    print(f"footprints: {nt[1]} x 7 hexes, {nt[2] - len(no_triangle)} triangles (orientations k0..k5: "
          f"{[sum(1 for c in new if shape[c['id']] == 1 + k) for k in range(6)]}), {nt[3] + len(no_triangle)} x 1 hex"
          + (f"; no free triangle (1 hex instead): {', '.join(no_triangle)}" if no_triangle else ""))
    flagged = [c for c in new if tiers[c["id"]][1]]
    hw = Counter(port_how[c["id"]] for c in flagged)
    other = Counter(port_how[c["id"]] for c in new if not tiers[c["id"]][1])
    print(f"ports (city_tiers.json): {len(flagged)}: {hw['direct']} touch the water, {hw['land']} reach it over land, "
          f"{hw['open']} through an opened bank/cliff/stream, {len(port_fail)} without navigable water within "
          f"{PORT_REACH} hexes" + (f": {' '.join(port_fail)}" if port_fail else ""))
    print(f"other cities at the water: {other['direct']} touch it, {other['land']} one hex away, {other['open']} behind "
          f"a bank or cliff (opened); {len(port_open)} hexes opened; Koei's cities at the water: {sum(koei_port)}")
    if rivers:
        print(f"navigable rivers (navigable_rivers.json): " + "; ".join(
            f"{r['city']} {r['river']} {len(r['hexes'])} hexes (川 {r['stream']}, land {r['land']}, road {r['road']})"
            if r["hexes"] is not None else f"{r['city']} {r['river']}: NO COURSE TO THE SEA" for r in rivers))
    print(f"facility plots: {plots_made} for {dC} new cities"
          + (f"; fewer than planned (little land in the territory) for {len(plots_short)}: {', '.join(plots_short)}" if plots_short else ""))


if __name__ == "__main__":
    main()
