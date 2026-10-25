"""convert_save_m11.py - carry a PK save (Save%03d.s11) over to the M11 map (and, for a pre-M10 save, to M10's officers).

usage:
  python tools/convert_save_m11.py IN.s11 OUT.s11 --old-data DIR --new-data DIR --game GAMEDIR
         [--roster data/persons/roster.json] [--report r.json]

  --old-data  the world data directory the save was made with (world_shex.bin, china_overrides.bin, bases_cities.json,
              world_info.json), e.g. <game>\\backup_before_m11\\world_custom
  --new-data  the M11 world data directory (same files + city_shapes.bin)
  --game      the game directory: only Media\\san11pkres.bin is read (Koei's China terrain, resource 0x12B7)
  --roster    the extended officers (M10_PERSONS.md); only used when the save still has 850 scenario officers
Only reads its inputs and writes OUT (refuses OUT == IN).

File: 90-byte header, 345-byte save info, then "MWC" 0x1a, u32 raw size, u32 compressed size, zlib stream of the world
(the PK save writer 0x6e39f2, reader 0x6e6ff7). The world is the save-format stream (convert_scen.save_layout, wide
ids: header mark 'WIDE' at 0x2c, R = 42, U = 1000) with PT = PS + 250 officer records of 203 bytes and the W x H per-hex
block at the end. PS: header dword 0x20 = u8 v + 'P' + u16 PS (M10), or a small value v for PS = 850.

Officers (only when the save has PS = 850 and the roster gives PS' > 850; a save with PS = PS' is left as it is):
  * records 850..PS'-1 inserted after record 849, built from the roster as tools/import_roster.py builds scenario
    records (first 152 bytes, template = the save's own Koei record) plus the save-only tail every unused / unappeared
    officer has (SAVE_TAIL); state by the save's year: dead (death < year), 未登場 (appear > year), else 未發現.
  * officer ids 850..1099 (new / special officers) -> + (PS'-850) in every officer id field (ID_FIELDS below; found
    by disassembling the serializers: officer 0x48b760 incl. +0xa8, +0x164 and the order parameters, world head
    0x483120, force, corps, city, gate/port, item, unit, AI base records); virtual fathers 2000..2060 -> 32000..32060.
  * header dword 0x20 -> v + 'P' + u16 PS'.
Map (M11_MAP.md): terrain is not in the save (worldmod builds it from world_shex.bin + Koei's China block +
china_overrides.bin at load; the per-hex block at the end of the save only keeps a few counter bits). What depends on it:
  * base positions: cities whose centre moved (罽賓, 佩林; after a new projection, hundreds of them) get the new centre
    in their building record (the game also recomputes base positions from the 3D objects on entering the map,
    0x57E910); a unit or facility on the new city hexes is moved by the rules below;
  * facilities on 内政用地 (SHEX byte 5) whose hex is no longer a plot of the same city (the plot went, or its
    territory went to another city) move to a free plot of that city (nearest first); with too few free plots, to
    the nearest free land hex of the city's territory (reported in "facilities_off_plot");
  * other facilities (陣、砦、関 ...) whose hex became water, cliff or a base move to the nearest free land hex; a
    change between land types under them is kept;
  * units on (or heading to) hexes whose terrain changed into something they cannot stand on (water / land, a base, a
    cliff for a land unit) move to the nearest hex of the same kind (land / water) that is free.
Everything else is kept byte for byte; the output is re-parsed and checked (verify()).
"""
import argparse
import collections
import json
import re
import struct
import sys
import zlib
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from convert_scen import save_layout, WIDE_MARK  # noqa: E402
import import_roster as IR  # noqa: E402

HDR, INFO = 90, 345
W0 = HDR + INFO                      # "MWC" block
MWC = b'MWC\x1a'
S0, P0 = 850, 1100
REC = 203
PS_OFF = 0x20
VF_LO, VF_HI, VF_NEW = 2000, 2060, 32000
# save-only tail (bytes 152..202) of every officer without a history: +0x128 = 100, no orders (+0x13c -1, params -1),
# +0x164 none, +0x160 -1 (all unused / unappeared / undiscovered officers of the user's saves have exactly this)
SAVE_TAIL = bytes.fromhex('64' + '00' * 16 + 'ff' * 25 + '0000ffff00ff000000')
assert len(SAVE_TAIL) == REC - 152
N_EXTRA = 45                         # gates + ports: N = C + 45
CITY_T, GATE_T, PORT_T = 16, 18, 17
WATER = (6, 7, 8)                    # 川 / 河 / 海: units standing there are ships
NAV = (7, 8)


class ConvError(Exception):
    pass


def i16(b, o):
    return struct.unpack_from('<h', b, o)[0]


# ---------------------------------------------------------------- officer id fields of the save-format stream
# officer record (203 B): i16 fields (0x48b760); +0xa8 and +0x164 are officer ids too (setters 0x48a900 / validator
# 0x48b340 check them against 0..1099)
P_REL = [(63, 'blood'), (65, 'father'), (67, 'mother'), (70, 'spouse'), (72, 'sworn')] + \
        [(75 + 2 * i, 'liked') for i in range(5)] + [(85 + 2 * i, 'disliked') for i in range(5)] + \
        [(102, '+0xa8'), (196, '+0x164')]
P_STATUS = 100
P_TASK, P_PARAM = 169, 170           # +0x13c i8 order, +0x140 6 x i32 parameters (setter 0x489bd0, getter 0x4897b0)
# order -> parameter indices that are officer ids (order executor 0x5b9e10 and the daily move update 0x4a18d2):
# 0 move (p0 destination location, p1 the officer leading the group), 12 (p0 officer, 0x5b9fac / 0x5c4ebf),
# 23 (p0 city, p1 officer, 0x5ba12a); the other orders' parameters are cities, buildings, forces, type-29 ids
TASK_OFFICER_PARAMS = {0: (1,), 12: (0,), 23: (1,)}
FATHER_MOTHER = (65, 67)
# other sections: (section, offset, what); all i16 (-1 = none)
OTHER = [('world_head', 5, 'emperor'), ('force', 0, 'ruler'), ('force', 2, 'strategist'), ('army', 2, 'corps leader'),
         ('city', 0, 'city governor'), ('gate', 64, 'gate governor'), ('port', 64, 'port governor'),
         ('item', 53, 'item owner'), ('unit', 1, 'unit leader'), ('unit', 3, 'unit deputy 1'), ('unit', 5, 'unit deputy 2')]
ORDERS = 212                         # world head +0x184 list: 1000 x i16 officers with orders (0x483311)
U_TTYPE, U_TID, U_POS, U_DEST = 21, 22, 24, 28   # unit target type (3 = officer), u16 target id, i16 lo/hi pos, dest
AI_BASE, AI_REC = 0x2ce8, 0x34       # AI block base records, +2 = i16 officer (fields.md)
B_POS = 13                           # building record i16 lo, i16 hi


# ---------------------------------------------------------------- container
def unpack(D):
    if D[8:18] != b'KOEI%SAN11' or struct.unpack_from('<I', D, 4)[0] != 0x1a:
        raise ConvError('not a PK save (kind 0x1a)')
    if struct.unpack_from('<I', D, 0x24)[0]:
        raise ConvError('XOR-obfuscated stream: not supported')
    if D[W0:W0 + 4] != MWC:
        raise ConvError(f'no MWC block at {W0:#x}')
    raw_n, comp_n = struct.unpack_from('<II', D, W0 + 4)
    if W0 + 12 + comp_n != len(D):
        raise ConvError('MWC compressed size does not match the file size')
    d = zlib.decompressobj()
    world = d.decompress(D[W0 + 12:])
    if not d.eof or d.unused_data or len(world) != raw_n:
        raise ConvError('MWC stream does not inflate to its stated size')
    return bytes(D[:W0]), world


def pack(head, world):
    comp = zlib.compress(world, 9)
    return head + MWC + struct.pack('<II', len(world), len(comp)) + comp


def stream_ps(head):
    p = head[PS_OFF:PS_OFF + 4]
    if p[1:2] == b'P':
        return struct.unpack('<H', p[2:])[0]
    if p[1:] == bytes(3):
        return S0
    raise ConvError(f'header dword 0x20 = {p.hex()}: neither a small value nor a PS mark')


# ---------------------------------------------------------------- layout
def layout(C, PT, cells):
    L, _ = save_layout(1, 2, 0, C, C + N_EXTRA, wide=True)
    po = L['person'][0]
    d = (PT - P0) * REC
    out = {}
    for k, (o, c, s) in L.items():
        if k == 'person':
            out[k] = (o, PT, s)
        elif k == 'hexmap':
            out[k] = (o + d, cells, 4)
        else:
            out[k] = ((o + d) if o > po else o, c, s)
    end = max(o + c * s for o, c, s in out.values())
    return out, end


def parse(head, world, C, cells):
    if head[0x2c:0x30] != WIDE_MARK:
        raise ConvError(f'header mark {head[0x2c:0x30]!r} at 0x2c: only R = 42 / U = 1000 saves ("WIDE") are supported')
    PS = stream_ps(head)
    PT = PS + 250
    L, end = layout(C, PT, cells)
    if end != len(world):
        raise ConvError(f'world stream is {len(world)} bytes, the layout for C={C}, PS={PS} says {end}')
    return PS, PT, L


def rec(B, L, name, i):
    o, c, s = L[name]
    return o + s * i


# ---------------------------------------------------------------- terrain
def read_link(res, index):
    with open(res, 'rb') as f:
        head = f.read(16)
        if head[:4] != b'LINK' or index >= struct.unpack_from('<I', head, 4)[0]:
            raise ConvError(f'{res}: not a LINK resource pack')
        f.seek(16 + 8 * index)
        off, size = struct.unpack('<II', f.read(8))
        f.seek(off)
        return f.read(size)


class World:
    """terrain type, area id and plot flag per hex as worldmod builds them at load (worldmod.cpp loadWorldShex/shexHook)"""

    def __init__(self, d, china, C):
        d = Path(d)
        info = json.loads((d / 'world_info.json').read_text(encoding='utf-8'))
        self.H, self.W = info['rows'], info['cols']
        self.Y0, self.X0 = info['china_lo'], info['china_hi']
        raw = (d / 'world_shex.bin').read_bytes()
        if raw[:4] != b'WSHX' or struct.unpack_from('<II', raw, 4) != (self.W, self.H):
            raise ConvError(f'{d}: world_shex.bin header')
        a = np.frombuffer(raw, np.uint8, self.W * self.H * 11, 12).reshape(self.H, self.W, 11).copy()
        cb = china.copy()
        ov = (d / 'china_overrides.bin').read_bytes()
        tag, n = ov[:4], struct.unpack_from('<I', ov, 4)[0]
        if tag not in (b'WCOV', b'WCO2'):
            raise ConvError(f'{d}: china_overrides.bin tag {tag!r}')

        def apply():
            for i in range(n):
                lo, hi = struct.unpack_from('<HH', ov, 8 + 15 * i)
                if lo < 200 and hi < 200:
                    cb[lo, hi] = np.frombuffer(ov, np.uint8, 11, 8 + 15 * i + 4)
        if tag == b'WCOV':
            apply()
        area = cb[:, :, 1].astype(np.int32) | (cb[:, :, 2].astype(np.int32) << 8)
        area = np.where(area >= 42, area + C - 42, area)
        cb[:, :, 1] = area & 0xff
        cb[:, :, 2] = area >> 8
        if tag == b'WCO2':
            apply()
        a[self.Y0:self.Y0 + 200, self.X0:self.X0 + 200] = cb
        self.t = a[:, :, 0]
        self.area = a[:, :, 1].astype(np.int32) | (a[:, :, 2].astype(np.int32) << 8)
        self.plot = a[:, :, 5] != 0
        bc = json.loads((d / 'bases_cities.json').read_text(encoding='utf-8'))
        if bc['C'] != C:
            raise ConvError(f'{d}: bases_cities.json has C={bc["C"]}, expected {C}')
        self.cities = {x['id']: x for x in bc['new']}

    def inside(self, lo, hi):
        return 0 <= lo < self.H and 0 <= hi < self.W


def hex_nbrs(y, x):
    d = [(0, -1), (0, 1), (-1, -1), (-1, 0), (1, -1), (1, 0)] if y % 2 == 0 else \
        [(0, -1), (0, 1), (-1, 0), (-1, 1), (1, 0), (1, 1)]
    return [(y + a, x + b) for a, b in d]


def hex_dist(a, b):
    def cube(y, x):
        q = x - (y - (y & 1)) // 2
        return q, y, -q - y
    (q1, r1, s1), (q2, r2, s2) = cube(*a), cube(*b)
    return max(abs(q1 - q2), abs(r1 - r2), abs(s1 - s2))


def nearest(start, ok, world, limit=60):
    """breadth-first nearest hex (by steps) satisfying ok"""
    seen = {start}
    q = collections.deque([(start, 0)])
    while q:
        p, k = q.popleft()
        if p != start and ok(p):
            return p
        if k >= limit:
            continue
        for n in hex_nbrs(*p):
            if n not in seen and world.inside(*n):
                seen.add(n)
                q.append((n, k + 1))
    return None


# ---------------------------------------------------------------- officers
class IdMap:
    def __init__(self, ps_from, ps_to):
        self.shift = ps_to - ps_from
        self.PT = ps_to + 250
        self.vf = ps_from == S0 and self.PT > VF_LO     # an old save keeps Koei's 2000..2060

    def __call__(self, v, parent=False):
        if parent and self.vf and VF_LO <= v <= VF_HI:
            return v - VF_LO + VF_NEW
        if self.shift and S0 <= v < P0:
            return v + self.shift
        return v


def officer_fields(B, L, PT):
    """every officer id field: yields (byte offset, width, kind, label); kind 'p' = father/mother"""
    po = L['person'][0]
    for i in range(PT):
        b = po + REC * i
        for off, what in P_REL:
            yield b + off, 2, 'p' if off in FATHER_MOTHER else 'o', f'officer {i} {what}'
        t = struct.unpack_from('<b', B, b + P_TASK)[0]
        for k in TASK_OFFICER_PARAMS.get(t, ()):
            yield b + P_PARAM + 4 * k, 4, 'o', f'officer {i} order {t} param {k}'
    for sec, off, what in OTHER:
        o, c, s = L[sec]
        for i in range(c):
            yield o + s * i + off, 2, 'o', f'{what} [{i}]'
    wh = L['world_head'][0]
    for k in range(1000):
        yield wh + ORDERS + 2 * k, 2, 'o', f'orders list [{k}]'
    o, c, s = L['unit']
    for i in range(c):
        if B[o + s * i + U_TTYPE] == 3:
            yield o + s * i + U_TID, 2, 'u', f'unit {i} target officer'
    ao = L['ai'][0]
    N = (L['ai'][2] - 20020) // AI_REC + 87
    for i in range(N):
        yield ao + AI_BASE + AI_REC * i + 2, 2, 'o', f'AI base record {i}'


def getv(B, o, w, kind):
    if w == 4:
        return struct.unpack_from('<i', B, o)[0]
    return struct.unpack_from('<H' if kind == 'u' else '<h', B, o)[0]


def putv(B, o, w, kind, v):
    struct.pack_into('<i' if w == 4 else ('<H' if kind == 'u' else '<h'), B, o, v)


def remap_ids(B, L, PT, m):
    """apply the id map to every officer id field of a stream with PT records (in place) -> Counter of changes"""
    n = collections.Counter()
    for o, w, kind, what in officer_fields(B, L, PT):
        v = getv(B, o, w, kind)
        nv = m(v, parent=kind == 'p')
        if nv != v:
            putv(B, o, w, kind, nv)
            n['virtual father' if nv >= VF_NEW else re.sub(r' ?\[?\d+\]?', '', what)] += 1
    return n


def id_errors(B, L, PT, PS):
    bad = []
    for o, w, kind, what in officer_fields(B, L, PT):
        v = getv(B, o, w, kind)
        if kind == 'u':
            ok = v == 0xffff or v < PT
        else:
            ok = v == -1 or 0 <= v < PT or (kind == 'p' and PT > VF_LO and VF_NEW <= v <= VF_NEW + VF_HI - VF_LO) \
                or (kind == 'p' and PT <= VF_LO and VF_LO <= v <= VF_HI)
        if not ok:
            bad.append((what, v))
    return bad


def referenced_above(B, L, PT, lo):
    """officer ids >= lo referenced anywhere -> {id: [fields]}"""
    out = collections.defaultdict(list)
    for o, w, kind, what in officer_fields(B, L, PT):
        v = getv(B, o, w, kind)
        if kind == 'u' and v == 0xffff:
            continue
        if lo <= v < PT:
            out[v].append(what)
    return out


def extended_records(B, L, roster, year, PS):
    """records 850..PS-1 (203 B) from the roster; B is the 850-officer stream"""
    po = L['person'][0]
    koei = [bytearray(B[po + REC * i: po + REC * i + 152]) for i in range(S0)]

    def exists(v):
        return v >= S0 or not IR.is_empty(koei[v])
    drops = collections.Counter()
    st = collections.Counter()
    out = []
    for o in roster:
        t = o['raw'].get('template', 156)
        if not 0 <= t < 670 or IR.is_empty(koei[t]):
            t = 156
        s = IR.state(o, year)
        st[IR.STATUS[s]] += 1
        r = IR.build_record(o, koei[t], s, exists, PS, drops)
        out.append(bytes(r) + SAVE_TAIL)
    return out, dict(st), dict(drops)


def convert_officers(world, L, PS_from, roster, year, C):
    """850-officer save stream -> PS' = 850 + len(roster) officers; returns (new stream, new layout, report)"""
    PS = S0 + len(roster)
    PT = PS + 250
    if [o['id'] for o in roster] != list(range(S0, PS)):
        raise ConvError('roster ids must be consecutive from 850')
    if PT > VF_NEW:
        raise ConvError(f'PT = {PT} > 32000')
    for o in roster:
        if not 0 <= o['city_id'] < C:
            raise ConvError(f'officer {o["id"]}: city {o["city_id"]} is not a city 0..{C - 1}')
    B = bytearray(world)
    po = L['person'][0]
    # ids that will move: refuse orders whose parameter semantics are unknown if they hold a value in 850..1099
    for i in range(P0):
        b = po + REC * i
        t = struct.unpack_from('<b', B, b + P_TASK)[0]
        if t in (-1,) or t in TASK_OFFICER_PARAMS:
            continue
        ps_ = struct.unpack_from('<6i', B, b + P_PARAM)
        if t not in (2, 5, 9, 10, 15, 16, 17, 18, 19, 20, 21, 22, 24, 37, 41, 42, 43) and any(S0 <= v < P0 for v in ps_):
            raise ConvError(f'officer {i}: order {t} has parameters {ps_} in 850..1099 and its parameter types are '
                            'not known (cannot tell whether they are officer ids)')
    m = IdMap(S0, PS)
    changed = remap_ids(B, L, P0, m)
    ext, st, drops = extended_records(B, L, roster, year, PS)
    E = bytes(B[:po + REC * S0]) + b''.join(ext) + bytes(B[po + REC * S0:])
    L2, end = layout(C, PT, L['hexmap'][1])
    if end != len(E):
        raise ConvError('officer insertion: size mismatch')
    rep = {'PS_from': PS_from, 'PS_to': PS, 'PT_to': PT, 'id_fields_changed': dict(changed),
           'extended_status': st, 'relations_dropped': drops}
    return E, L2, rep


# ---------------------------------------------------------------- map
CLIFF = 15
LAND_OK = (0, 1, 2, 3, 4, 5, 9, 10, 11, 12, 13, 14, 19)     # land a facility or a land unit can stand on


def unit_ok(t_old, t_new):
    """can a unit that stood on terrain t_old stand on t_new? (bases never; land stays land, water stays water, a
    land unit not on a cliff)"""
    if t_new in (CITY_T, GATE_T, PORT_T):
        return False
    if t_old not in WATER and t_new == CLIFF:
        return False
    return (t_old in WATER) == (t_new in WATER)


def convert_map(world, L, old, new, C):
    B = bytearray(world)
    N = C + N_EXTRA
    rep = {'cities_moved': [], 'facilities_moved': [], 'facilities_off_plot': [], 'units_moved': [],
           'unit_dest_moved': [], 'problems': []}
    ob = L['building'][0]
    bld, unit_at = {}, {}                            # hex -> building id / unit index
    for i in range(L['building'][1]):
        r = B[ob + 18 * i: ob + 18 * i + 18]
        if r[0] != 0xff:
            bld[struct.unpack_from('<hh', r, B_POS)] = i
    ou, nu, su = L['unit']
    for k in range(nu):
        lo, hi = struct.unpack_from('<hh', B, ou + su * k + U_POS)
        if lo >= 0:
            unit_at[(lo, hi)] = k

    def free(p):
        return p not in bld and p not in unit_at
    moved_pos = {}                                   # old hex -> new hex of moved buildings
    # 1. bases: new cities' centres (all moves first: a city may take a hex another city leaves)
    moves = []
    for cid in range(42, C):
        a, b = old.cities[cid], new.cities[cid]
        if a['name'] != b['name']:
            raise ConvError(f'city {cid} is {a["name"]} in the old data and {b["name"]} in the new')
        o = ob + 18 * cid
        pos = struct.unpack_from('<hh', B, o + B_POS)
        if B[o] != 0:
            raise ConvError(f'building {cid} is not a city in the save')
        if pos != (a['lo'], a['hi']):
            rep['problems'].append(f'city {cid} {a["name"]} at {pos} in the save, old data says {(a["lo"], a["hi"])}')
        want = (b['lo'], b['hi'])
        if pos != want:
            moves.append((cid, pos, want))
    for cid, pos, want in moves:
        bld.pop(pos, None)
    for cid, pos, want in moves:
        j = bld.get(want)
        if j is not None and j < N:
            raise ConvError(f'new centre {want} of city {cid} is base {j}')
        struct.pack_into('<hh', B, ob + 18 * cid + B_POS, *want)
        bld[want] = cid                              # a facility on the new centre is moved below
        moved_pos[pos] = want
        rep['cities_moved'].append({'id': cid, 'name': new.cities[cid]['name'], 'from': list(pos), 'to': list(want)})
    for i in range(N):                               # every base on its terrain in the new map
        o = ob + 18 * i
        lo, hi = struct.unpack_from('<hh', B, o + B_POS)
        want_t = {0: CITY_T, 1: GATE_T, 2: PORT_T}.get(B[o])
        if want_t is None or not new.inside(lo, hi) or new.t[lo, hi] != want_t:
            raise ConvError(f'base {i} (type {B[o]}) at ({lo},{hi}) is not on terrain {want_t} in the new map')
    # 2. facilities
    orphans, strays = collections.defaultdict(list), []
    for i in range(N, L['building'][1]):
        o = ob + 18 * i
        if B[o] == 0xff:
            continue
        p = struct.unpack_from('<hh', B, o + B_POS)
        if not new.inside(*p):
            raise ConvError(f'facility {i} at {p} outside the map')
        on_base = new.t[p] in (CITY_T, GATE_T, PORT_T)
        if old.plot[p]:
            if on_base or not new.plot[p] or new.area[p] != old.area[p]:
                orphans[int(old.area[p])].append(i)
        elif on_base or new.t[p] not in LAND_OK:
            strays.append(i)
        elif old.t[p] != new.t[p] and old.t[p] not in LAND_OK:
            raise ConvError(f'facility {i} (type {B[o]}) at {p}: terrain {old.t[p]} -> {new.t[p]}; no rule to fix it')
    for i in [i for ids in orphans.values() for i in ids] + strays:
        p = struct.unpack_from('<hh', B, ob + 18 * i + B_POS)
        if bld.get(p) == i:
            bld.pop(p)

    def place(i, f, how, city=None, dist=None):
        o = ob + 18 * i
        p = struct.unpack_from('<hh', B, o + B_POS)
        struct.pack_into('<hh', B, o + B_POS, *f)
        bld[f] = i
        moved_pos[p] = f
        nm = new.cities.get(city, {}).get('name', str(city)) if city is not None else None
        rep[how].append({'id': i, 'type': B[o], 'city': city, 'city_name': nm, 'from': list(p), 'to': list(f),
                         'steps': hex_dist(p, f)})
    for city, ids in sorted(orphans.items()):
        plots = [tuple(map(int, q)) for q in zip(*np.nonzero(new.plot & (new.area == city)))]
        plots = [q for q in plots if free(q) and new.t[q] not in (CITY_T, GATE_T, PORT_T)]
        pairs = sorted((hex_dist(struct.unpack_from('<hh', B, ob + 18 * i + B_POS), f), i, f) for i in ids for f in plots)
        done, used = set(), set()
        for dist, i, f in pairs:
            if i in done or f in used:
                continue
            place(i, f, 'facilities_moved', city)
            done.add(i)
            used.add(f)
        for i in ids:
            if i in done:
                continue
            p = struct.unpack_from('<hh', B, ob + 18 * i + B_POS)
            f = nearest(p, lambda q: free(q) and new.t[q] in LAND_OK and new.area[q] == city and not new.plot[q], new, 80)
            if f is None:
                raise ConvError(f'city {city}: facility {i} lost its plot and there is no free plot or land in the city')
            place(i, f, 'facilities_off_plot', city)
    for i in strays:
        p = struct.unpack_from('<hh', B, ob + 18 * i + B_POS)
        f = nearest(p, lambda q: free(q) and new.t[q] in LAND_OK and not new.plot[q], new, 80)
        if f is None:
            raise ConvError(f'facility {i} at {p}: no free land hex nearby')
        place(i, f, 'facilities_moved')
    # 3. units on / heading to hexes they can no longer use
    for k in range(nu):
        o = ou + su * k
        lo, hi = struct.unpack_from('<hh', B, o + U_POS)
        if lo < 0:
            continue
        if not unit_ok(old.t[lo, hi], new.t[lo, hi]) or (lo, hi) in bld:
            to = old.t[lo, hi]
            p = nearest((lo, hi), lambda q: free(q) and unit_ok(to, new.t[q]), new)
            if p is None:
                raise ConvError(f'unit {k} at ({lo},{hi}): no free hex nearby')
            struct.pack_into('<hh', B, o + U_POS, *p)
            unit_at.pop((lo, hi), None)
            unit_at[p] = k
            rep['units_moved'].append({'unit': k, 'from': [lo, hi], 'to': list(p)})
        dlo, dhi = struct.unpack_from('<hh', B, o + U_DEST)
        if dlo >= 0:
            d = (dlo, dhi)
            if d in moved_pos:
                nd = moved_pos[d]
            elif new.inside(*d) and old.t[d] != new.t[d] and not unit_ok(old.t[d], new.t[d]):
                nd = nearest(d, lambda q: unit_ok(old.t[d], new.t[q]), new)
            else:
                continue
            struct.pack_into('<hh', B, o + U_DEST, *nd)
            rep['unit_dest_moved'].append({'unit': k, 'from': list(d), 'to': list(nd)})
    # 4. per-hex block: report counters on hexes that changed
    ho = L['hexmap'][0]
    hx = np.frombuffer(bytes(B[ho:ho + 4 * new.W * new.H]), np.uint32).reshape(new.H, new.W)
    changed = (old.t != new.t) | (old.plot != new.plot)
    rep['hexmap_nonzero'] = int((hx != 0).sum())
    rep['hexmap_nonzero_on_changed_hexes'] = [[int(y), int(x), int(hx[y, x])] for y, x in zip(*np.nonzero(changed & (hx != 0)))]
    rep['terrain_changed_hexes'] = int(changed.sum())
    return bytes(B), rep


# ---------------------------------------------------------------- summary / verification
STATUS = {0: '君主', 1: '都督', 2: '太守', 3: '一般', 4: '在野', 5: '俘虜', 6: '未登場', 7: '未發現', 8: '死亡', 255: '空'}


def summary(head, B, L, PS, PT, C):
    wh = L['world_head'][0]
    po = L['person'][0]
    st = collections.Counter(STATUS.get(B[po + REC * i + P_STATUS], B[po + REC * i + P_STATUS]) for i in range(PT))
    players = [(k, v) for k, v in enumerate(B[wh + 163:wh + 210]) if v != 0xff]
    o, c, s = L['building']
    owned = collections.Counter(B[o + s * i + 1] for i in range(C))
    ou, nu, su = L['unit']
    alive = sum(1 for k in range(nu) if i16(B, ou + su * k + U_POS) >= 0)
    ref = referenced_above(B, L, PT, S0)
    new_lo, new_hi, sp_lo = PS, PS + 149, PS + 150
    ref_new = {v: f for v, f in ref.items() if new_lo <= v <= new_hi}
    ref_sp = {v: f for v, f in ref.items() if v >= sp_lo}
    ext_ref = {v: f for v, f in ref.items() if S0 <= v < PS}
    ruler_name = bytes(head[HDR:HDR + 9]).split(b'\0')[0].decode('cp950', 'replace')   # save info: player's ruler
    fo, fc, fs = L['force']
    player = [f for f in range(fc) if 0 <= i16(B, fo + fs * f) < PT and
              IR.show(B[po + REC * i16(B, fo + fs * f):][:5]) + IR.show(B[po + REC * i16(B, fo + fs * f) + 5:][:5]) == ruler_name]
    orders = sum(1 for k in range(1000) if i16(B, wh + ORDERS + 2 * k) != -1)
    with_task = sum(1 for i in range(PT) if struct.unpack_from('<b', B, po + REC * i + P_TASK)[0] != -1)
    return {
        'date': f'{i16(B, wh + 1)}-{B[wh + 3]}', 'PS': PS, 'PT': PT,
        'player_ruler': ruler_name, 'player_force': player,
        'player_cities': [i for i in range(C) if player and B[o + s * i + 1] in player],
        'player_slots_to_force': players,
        'cities_by_owner_force': {('none' if k == 0xff else k): v for k, v in sorted(owned.items())},
        'officer_status': dict(st), 'units_alive': alive,
        'orders_list_entries': orders, 'officers_with_an_order': with_task,
        'new_officers_referenced': {v: f[:6] for v, f in sorted(ref_new.items())},
        'special_officers_referenced': {v: f[:6] for v, f in sorted(ref_sp.items())},
        'extended_officers_referenced_count': len(ext_ref),
        'header_0x20': head[PS_OFF:PS_OFF + 4].hex(), 'header_0x2c': head[0x2c:0x30].decode('latin1'),
    }


def expected_officers(Bin, Lin, PTin, m):
    """the input's officer records 0..849 and 850..1099 with the id map applied (what the output must hold)"""
    X = bytearray(Bin)
    remap_ids(X, Lin, PTin, m)
    po = Lin['person'][0]
    return [bytes(X[po + REC * i: po + REC * (i + 1)]) for i in range(PTin)], X


def verify(D, E, C, cells, roster, conv_rep, old, new):
    hin, Bin = unpack(D)
    hout, Bout = unpack(E)
    PSi, PTi, Li = parse(hin, Bin, C, cells)
    PSo, PTo, Lo = parse(hout, Bout, C, cells)
    v = {}
    # header / save info
    if hout[:PS_OFF] != hin[:PS_OFF] or hout[PS_OFF + 4:] != hin[PS_OFF + 4:] or hout[PS_OFF] != hin[PS_OFF]:
        raise ConvError('header / save info changed outside the PS mark')
    if PSo != S0 and hout[PS_OFF + 1:PS_OFF + 4] != b'P' + struct.pack('<H', PSo):
        raise ConvError('PS mark')
    v['header'] = {'0x20': hout[PS_OFF:PS_OFF + 4].hex(), 'PS': PSo, 'PT': PTo, '0x2c': hout[0x2c:0x30].decode('latin1')}
    # officer ids valid
    bad = id_errors(Bout, Lo, PTo, PSo)
    if bad:
        raise ConvError(f'{len(bad)} invalid officer ids, e.g. {bad[:10]}')
    v['officer_id_fields_checked'] = sum(1 for _ in officer_fields(Bout, Lo, PTo))
    # officer records: input (id-mapped) == output, inserted ones == roster
    m = IdMap(PSi, PSo)
    exp, X = expected_officers(Bin, Li, PTi, m)
    poo = Lo['person'][0]
    out_rec = [Bout[poo + REC * i: poo + REC * (i + 1)] for i in range(PTo)]
    if PSi == PSo:
        if out_rec != exp:
            raise ConvError('officer records changed although PS is unchanged')
        v['officers'] = f'all {PTo} records byte-identical to the input'
    else:
        if out_rec[:S0] != exp[:S0]:
            raise ConvError('Koei officers 0..849 differ from input + id map')
        if out_rec[PSo:] != exp[S0:]:
            raise ConvError('new / special officers differ from input records 850..1099 + id map')
        raw_eq = sum(1 for i in range(S0) if out_rec[i] == Bin[Li['person'][0] + REC * i: Li['person'][0] + REC * (i + 1)])
        v['officers'] = (f'0..849 equal to the input with the id map ({raw_eq} byte-identical, {S0 - raw_eq} with '
                         f'shifted ids / virtual fathers); {PTo - PSo} new/special = input 850..1099 + id map')
    if roster is not None and PSo == S0 + len(roster):
        mism = []
        for k, o in enumerate(roster):
            r = out_rec[S0 + k]
            name = IR.show(r[0:5]) + IR.show(r[5:10])
            if name != o['sei'] + o['mei']:
                mism.append((S0 + k, name, o['sei'] + o['mei']))
        v['extended_vs_roster_name_mismatches'] = len(mism)
        if mism:
            v['extended_vs_roster_examples'] = mism[:10]
            if PSi != PSo:
                raise ConvError(f'inserted officers do not match the roster: {mism[:5]}')
    # every other section: identical to input + id map, except the listed building / unit position changes
    allowed = set()
    ob = Lo['building'][0]
    for x in conv_rep['map']['cities_moved']:
        allowed |= {('building', x['id'])}
    for x in conv_rep['map']['facilities_moved'] + conv_rep['map']['facilities_off_plot']:
        allowed |= {('building', x['id'])}
    for x in conv_rep['map']['units_moved'] + conv_rep['map']['unit_dest_moved']:
        allowed |= {('unit', x['unit'])}
    diffs = collections.Counter()
    for name, (o, c, s) in Lo.items():
        if name == 'person':
            continue
        oi = Li[name][0]
        a, b = X[oi:oi + c * s], Bout[o:o + c * s]
        if a == b:
            continue
        if name in ('building', 'unit'):
            for i in range(c):
                ra, rb = a[s * i:s * (i + 1)], b[s * i:s * (i + 1)]
                if ra != rb:
                    if (name, i) not in allowed:
                        raise ConvError(f'{name} {i} changed unexpectedly')
                    pos = (B_POS, B_POS + 4) if name == 'building' else (U_POS, U_DEST + 4)
                    if ra[:pos[0]] != rb[:pos[0]] or ra[pos[1]:] != rb[pos[1]:]:
                        raise ConvError(f'{name} {i}: bytes other than the position changed')
                    diffs[name] += 1
        else:
            raise ConvError(f'section {name} differs from input + id map')
    v['sections_other_than_officers'] = f'identical to input + id map except positions of {dict(diffs)}'
    # map checks on the output
    N = C + N_EXTRA
    pos_seen = {}
    for i in range(Lo['building'][1]):
        o = ob + 18 * i
        if Bout[o] == 0xff:
            continue
        p = struct.unpack_from('<hh', Bout, o + B_POS)
        if p in pos_seen:
            raise ConvError(f'buildings {pos_seen[p]} and {i} on the same hex {p}')
        pos_seen[p] = i
        if i < N:
            if new.t[p] != {0: CITY_T, 1: GATE_T, 2: PORT_T}[Bout[o]]:
                raise ConvError(f'base {i} not on its terrain')
            if i >= 42 and i < C and p != (new.cities[i]['lo'], new.cities[i]['hi']):
                raise ConvError(f'city {i} not at its new centre')
        else:
            q = struct.unpack_from('<hh', X, Li['building'][0] + 18 * i + B_POS)
            if new.t[p] in (CITY_T, GATE_T, PORT_T) or (not new.plot[p] and new.t[p] not in LAND_OK):
                raise ConvError(f'facility {i} at {p} on a base, water or cliff')
            if old.plot[q] and not (new.plot[p] and new.area[p] == old.area[q]) and \
                    i not in {x['id'] for x in conv_rep['map']['facilities_off_plot']}:
                raise ConvError(f'facility {i} not on a plot of its city {old.area[q]}')
    ou, nu, su = Lo['unit']
    for k in range(nu):
        lo, hi = struct.unpack_from('<hh', Bout, ou + su * k + U_POS)
        if lo >= 0 and new.t[lo, hi] in (CITY_T, GATE_T, PORT_T):
            raise ConvError(f'unit {k} on a base hex')
        if lo >= 0 and (lo, hi) in pos_seen:
            raise ConvError(f'unit {k} on building {pos_seen[(lo, hi)]}')
    v['map'] = 'bases on city/gate/port terrain of the new map, new-city centres match bases_cities.json, ' \
               'no two buildings on one hex, no unit on a base or building, facilities on plots of their own city ' \
               '(or listed in facilities_off_plot) and never on a base, water or cliff'
    v['summary_out'] = summary(hout, Bout, Lo, PSo, PTo, C)
    v['size'] = {'file_in': len(D), 'file_out': len(E), 'world_in': len(Bin), 'world_out': len(Bout)}
    return v


# ---------------------------------------------------------------- driver
def convert(D, old, new, roster):
    C = len(new.cities) + 42
    cells = new.W * new.H
    if (old.W, old.H) != (new.W, new.H):
        raise ConvError('old and new maps differ in size')
    head, world = unpack(D)
    PS, PT, L = parse(head, world, C, cells)
    rep = {'summary_in': summary(head, world, L, PS, PT, C)}
    year = i16(world, L['world_head'][0] + 1)
    PS_to = S0 + len(roster) if roster is not None else PS
    if PS == PS_to:
        rep['officers'] = f'save already has PS = {PS}: officer records and ids left as they are'
        B, L2 = world, L
    elif PS == S0:
        B, L2, rep['officers'] = convert_officers(world, L, PS, roster, year, C)
        head = head[:PS_OFF + 1] + b'P' + struct.pack('<H', PS_to) + head[PS_OFF + 4:]
    else:
        raise ConvError(f'the save has PS = {PS}, the roster gives {PS_to}: only 850 -> PS\' or PS\' -> PS\' are supported')
    B, rep['map'] = convert_map(B, L2, old, new, C)
    return pack(head, B), rep, C, cells


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('inp')
    ap.add_argument('out')
    ap.add_argument('--old-data', required=True)
    ap.add_argument('--new-data', required=True)
    ap.add_argument('--game', required=True, help='game directory (reads Media/san11pkres.bin only)')
    ap.add_argument('--roster', default=str(HERE.parent / 'data' / 'persons' / 'roster.json'))
    ap.add_argument('--report')
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    try:
        if Path(a.inp).resolve() == Path(a.out).resolve():
            raise ConvError('OUT must not be IN')
        china_raw = read_link(Path(a.game) / 'Media' / 'san11pkres.bin', 0x12B7)
        if china_raw[:8] != b'SHEX0008':
            raise ConvError('resource 0x12B7 is not SHEX0008')
        china = np.frombuffer(china_raw, np.uint8, 200 * 200 * 11, 8).reshape(200, 200, 11)
        C = json.loads((Path(a.new_data) / 'bases_cities.json').read_text(encoding='utf-8'))['C']
        old, new = World(a.old_data, china, C), World(a.new_data, china, C)
        roster = json.loads(Path(a.roster).read_text(encoding='utf-8')) if a.roster else None
        D = Path(a.inp).read_bytes()
        E, rep, C, cells = convert(D, old, new, roster)
        rep['verify'] = verify(D, E, C, cells, roster, rep, old, new)
        Path(a.out).write_bytes(E)
        rep['in'], rep['out'] = str(a.inp), str(a.out)
        if a.report:
            Path(a.report).write_text(json.dumps(rep, ensure_ascii=False, indent=1, default=str), encoding='utf-8')
        m = rep['map']
        print(f'{Path(a.inp).name} -> {a.out}: {len(D)} -> {len(E)} bytes; officers: '
              f'{rep["officers"] if isinstance(rep["officers"], str) else rep["officers"]["PS_to"]}; '
              f'cities moved {len(m["cities_moved"])}, facilities moved {len(m["facilities_moved"])} '
              f'(+{len(m["facilities_off_plot"])} off the plots), '
              f'units moved {len(m["units_moved"])}, unit destinations {len(m["unit_dest_moved"])}; verified')
        return 0
    except ConvError as e:
        print('error:', e, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
