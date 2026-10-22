"""import_roster.py - write the extended officers (data/persons/roster.json) into a world scenario (M10_PERSONS.md §5).

usage:
  python tools/import_roster.py <in Scen*.s11> <out .s11> [--roster data/persons/roster.json] [--report r.json]

Input: a world scenario as the game reads it (<data_dir>\\scenario\\Scen*.s11 written by convert_scen.py /
scenario_build.py: header mark 'WIDE', 'WF'+u16 R or 'U'+u8 R+u16 U at file offset 0x2c), or a stock 42-city one.
It holds officers 0..849 (850 x 152 bytes). The roster lists the extended officers 850..PS-1 (consecutive ids);
their records are inserted after record 849, so every later section shifts by (PS-850) x 152 bytes.

Record (tools/roster/common.py): name fields, years, relations, stats, ... from the roster; bytes the roster does not
describe come from its Koei template officer (raw.template) in the input file. Faces: 2000 (male) / 2100 (female),
the generic portraits the game itself falls back to (0x48a450) and gives a reset new officer (0x485ff0).
State relative to the scenario's start year Y (file offset 0x5B):
  death < Y            -> 8 死亡 (as Koei's historical scenarios store officers dead before the start)
  appear > Y           -> 6 未登場 (appears in year `appear` at city_id; 0x59e4b0 handles it like Koei's)
  otherwise            -> 7 未發現 in city_id (Koei's own encoding of most free adult officers at the start)
  all of them: no corps (0xff), affiliation = location = city_id, loyalty 0, merit 0.
Relations (father, mother, spouse, sworn, blood, liked, disliked) are kept only if they name an officer 0..PS-1
that exists (Koei's empty slots: status 0xff with no location), else -1.
Virtual fathers (M10 §4.6): with PT = PS+250 > 2000 the father/mother values 2000..2060 of Koei's records 0..849
become 32000..32060 (roster values are real officer ids and are not touched).
Officer ids 850..1099 (new / special officers) in other places of the stream (emperor, force ruler / strategist,
corps leader, item owner, Koei records' relations) would shift by PS-850; they are counted and shifted.
Header: u8 v + 'P' + u16 PS at file offset 0x20 (v = Koei's small value there, 0 in scenarios); the 0x2c mark is kept.
The output is re-parsed and checked; nothing outside the person section, the VF fields and the mark changes.
"""
import argparse
import collections
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from convert_scen import scen_layout, header, KIND_SCEN, WIDE_MARK  # noqa: E402

REC = 152
S0 = 850
YEAR_OFF = 0x5B
PS_OFF = 0x20
FACE_MALE, FACE_FEMALE = 2000, 2100
ST_DEAD, ST_UNAPPEARED, ST_UNFOUND, ST_FREE = 8, 6, 7, 4
STATUS = {0: '君主', 1: '都督', 2: '太守', 3: '一般', 4: '在野', 5: '俘虜', 6: '未登場', 7: '未發現', 8: '死亡', 255: '空'}
VF_LO, VF_HI, VF_NEW = 2000, 2060, 32000
REL1 = {'blood': 63, 'father': 65, 'mother': 67, 'spouse': 70, 'sworn': 72}
LIKED, DISLIKED = 75, 85


class ImportError_(Exception):
    pass


def i16(b, o):
    return struct.unpack_from('<h', b, o)[0]


def stream_marks(D):
    """-> (wide, R, PS) from the header marks at 0x2c and 0x20"""
    m = bytes(D[0x2c:0x30])
    if m == WIDE_MARK:
        wide, R = True, 42
    elif m[:2] == b'WF':
        wide, R = True, struct.unpack('<H', m[2:])[0]
    elif m[:1] == b'U':
        wide, R = True, m[1]
    elif m == bytes(4):
        wide, R = False, 42
    else:
        raise ImportError_(f'unknown header mark {m!r} at 0x2c')
    p = bytes(D[PS_OFF:PS_OFF + 4])
    if p[1:2] == b'P':
        PS = struct.unpack('<H', p[2:])[0]
    elif p[1:] == bytes(3):
        PS = S0
    else:
        raise ImportError_(f'header dword at 0x20 is {p.hex()}, neither a small value nor a PS mark')
    return wide, R, PS


def layout(C, wide, R, PS):
    """scen_layout with PS officer records (sections after the officers shift)"""
    L, end = scen_layout(C, C + 45, wide=wide, R=R)
    po = L['person'][0]
    d = (PS - S0) * REC
    out = {k: ((o + d if o > po else o), (PS if k == 'person' else c), s) for k, (o, c, s) in L.items()}
    return out, end + d


def parse(D):
    kind, v1, v2, xor = header(D)
    if kind not in KIND_SCEN or xor:
        raise ImportError_(f'not a plain scenario stream (kind {kind:#x}, xor {xor})')
    wide, R, PS = stream_marks(D)
    hits = [C for C in range(42, 1001) if layout(C, wide, R, PS)[1] == len(D)]
    if len(hits) != 1:
        raise ImportError_(f'no unique scenario layout for {len(D)} bytes (wide={wide}, R={R}, PS={PS}): {hits}')
    C = hits[0]
    L, _ = layout(C, wide, R, PS)
    year = i16(D, YEAR_OFF)
    wh = L['world_head'][0]
    if i16(D, wh + 1) != year:
        raise ImportError_(f'start year {year} at 0x5B differs from the world head year {i16(D, wh + 1)}')
    return {'C': C, 'wide': wide, 'R': R, 'PS': PS, 'L': L, 'year': year}


def recs(D, L, name):
    o, c, s = L[name]
    return [bytearray(D[o + s * i:o + s * (i + 1)]) for i in range(c)]


def name_field(text, what, oid):
    try:
        b = text.encode('cp950')
    except UnicodeEncodeError:
        raise ImportError_(f'officer {oid}: {what} {text!r} is not Big5 (cp950)')
    if len(b) > 4:
        raise ImportError_(f'officer {oid}: {what} {text!r} is {len(b)} bytes, the field holds 4 + NUL')
    return b + bytes(5 - len(b))


def _cp950(b):
    try:
        bytes(b).split(b'\0')[0].decode('cp950')
        return True
    except UnicodeDecodeError:
        return False


def is_empty(r):
    """Koei's unused slots (史實01.., 古代..): status 0xff and no location"""
    return r[100] == 0xff and i16(r, 96) == -1 and i16(r, 98) == -1


def state(o, year):
    if o['death'] < year:
        return ST_DEAD
    if o['appear'] > year:
        return ST_UNAPPEARED
    return ST_UNFOUND


def build_record(o, tmpl, st, exists, PS, drops):
    oid = o['id']
    r = bytearray(tmpl)
    r[0:5] = name_field(o['sei'], 'sei', oid)
    r[5:10] = name_field(o['mei'], 'mei', oid)
    r[10:15] = name_field(o.get('azana', ''), 'azana', oid)
    r[15:53] = bytes(38)
    struct.pack_into('<h', r, 53, FACE_FEMALE if o['sex'] == 1 else FACE_MALE)
    r[55] = o['sex']
    struct.pack_into('<hhh', r, 56, o['appear'], o['birth'], o['death'])
    r[62] = o['death_type']

    def rel(v, what):
        if v == -1:
            return -1
        if 0 <= v < PS and exists(v):
            return v
        drops[what] += 1
        return -1
    for k, off in REL1.items():
        struct.pack_into('<h', r, off, rel(o[k], k))
    r[69] = o['generation']
    r[74] = o['aishou']
    for off, key in ((LIKED, 'liked'), (DISLIKED, 'disliked')):
        vs = [v for v in (rel(v, key) for v in o[key]) if v != -1][:5]
        struct.pack_into('<5h', r, off, *(vs + [-1] * (5 - len(vs))))
    c = o['city_id']
    r[95] = 0xff                                    # corps: none
    struct.pack_into('<hh', r, 96, c, c)            # affiliation = location = city
    r[100] = st
    raw = o['raw']
    r[101] = raw['b101']
    r[102:104] = b'\xff\xff'
    r[104] = 0                                      # loyalty
    struct.pack_into('<H', r, 105, 0)               # merit
    r[107:113] = bytes(o['apt'])
    r[113:118] = bytes(o['stats'])
    r[118:123] = bytes(o['growth'])
    r[123] = o['home']
    r[124] = o['skill']
    r[125] = raw['b125']
    r[126:134] = bytes([o['giri'], o['yabou'], o['kiyou'], o['seikaku'], o['voice'], o['tone'], o['kanshitsu'], o['senryaku']])
    r[134] = raw['b134']
    r[135] = raw['b135']
    r[136:142] = bytes(raw['model'])
    r[142:148] = bytes(raw['b142'])
    r[148] = raw['b148']
    r[149:152] = bytes(3)
    assert len(r) == REC
    return r


# officer id fields outside the extended records: (section, offset in record) -> i16
OTHER_FIELDS = [('world_head', 5, 'emperor'), ('force', 0, 'ruler'), ('force', 2, 'strategist'),
                ('army', 2, 'corps leader'), ('item', 53, 'item owner')]
KOEI_REL = [(off, k) for k, off in REL1.items()] + [(LIKED + 2 * i, 'liked') for i in range(5)] + \
           [(DISLIKED + 2 * i, 'disliked') for i in range(5)]


def fix_ids(D, L, PS):
    """virtual fathers and new / special officer ids in a stream with 850 officers -> (bytes, VF count, shifted)"""
    PT = PS + 250
    out = bytearray(D)
    shift = PS - S0
    found = collections.Counter()
    for sec, off, what in OTHER_FIELDS:
        o, c, s = L[sec]
        for i in range(c):
            v = i16(out, o + s * i + off)
            if S0 <= v < 1100:
                found[what] += 1
                struct.pack_into('<h', out, o + s * i + off, v + shift)
    vf = 0
    po = L['person'][0]
    for i in range(S0):
        b = po + REC * i
        for off, what in KOEI_REL:
            v = i16(out, b + off)
            if off in (65, 67) and VF_LO <= v <= VF_HI:
                if PT > VF_LO:                       # persons.cpp: VF = 32000 when PT > 2000
                    struct.pack_into('<h', out, b + off, v - VF_LO + VF_NEW)
                    vf += 1
            elif S0 <= v < 1100:
                found[f'officer {what}'] += 1
                struct.pack_into('<h', out, b + off, v + shift)
            elif v >= 1100 or v < -1:
                raise ImportError_(f'Koei officer {i}: {what} = {v} is no officer id')
    return bytes(out), vf, dict(found)


def import_roster(D, roster):
    info = parse(D)
    if info['PS'] != S0:
        raise ImportError_(f'the input already holds {info["PS"]} officers ("PS" mark): import into a file with 850')
    ids = [o['id'] for o in roster]
    if ids != list(range(S0, S0 + len(roster))):
        raise ImportError_('roster ids must be consecutive from 850')
    PS = S0 + len(roster)
    PT = PS + 250
    if PT > 32000:
        raise ImportError_(f'PT = {PT} > 32000 (M10 §5a)')
    C, L, year = info['C'], info['L'], info['year']
    if D[PS_OFF + 1:PS_OFF + 4] != bytes(3):
        raise ImportError_('header dword at 0x20 does not leave room for the PS mark')
    koei = recs(D, L, 'person')
    rep = {'start_year': year, 'C': C, 'R': info['R'], 'PS': PS, 'PT': PT}
    out, rep['vf_converted'], rep['new_special_ids_shifted'] = fix_ids(D, L, PS)
    rep['koei_names_with_user_defined_big5'] = sum(1 for r in koei if not all(_cp950(r[k:k + 5]) for k in (0, 5, 10)))

    def exists(v):
        return v >= S0 or not is_empty(koei[v])
    drops = collections.Counter()
    new = []
    st_count = collections.Counter()
    for o in roster:
        if not 0 <= o['city_id'] < C:
            raise ImportError_(f'officer {o["id"]}: city_id {o["city_id"]} not a city (0..{C - 1})')
        t = o['raw'].get('template', 156)
        if not 0 <= t < 670 or is_empty(koei[t]):
            t = 156
        st = state(o, year)
        st_count[STATUS[st]] += 1
        new.append(build_record(o, koei[t], st, exists, PS, drops))
    rep['extended_status'] = dict(st_count)
    rep['relations_dropped'] = dict(drops)
    pe = L['person'][0] + REC * S0
    E = out[:pe] + b''.join(new) + out[pe:]
    E = E[:PS_OFF] + E[PS_OFF:PS_OFF + 1] + b'P' + struct.pack('<H', PS) + E[PS_OFF + 4:]
    return E, rep


def big5_ok(b):
    """Big5 structure: ASCII, or lead 0x81..0xFE + trail 0x40..0x7E / 0xA1..0xFE. Koei's names use the user-defined
    area (lead 0xFA.., e.g. 孔伷, 張郃's 儁乂) for rare characters, which Python's cp950 does not map."""
    b = bytes(b).split(b'\0')[0]
    i = 0
    while i < len(b):
        if b[i] < 0x80:
            i += 1
        elif 0x81 <= b[i] <= 0xFE and i + 1 < len(b) and (0x40 <= b[i + 1] <= 0x7E or 0xA1 <= b[i + 1] <= 0xFE):
            i += 2
        else:
            return False
    return True


def show(b):
    return bytes(b).split(b'\0')[0].decode('cp950', 'replace')


def verify(D, E, roster, rep):
    """re-parse the output and compare it with the input (plus the id fixes) byte for byte"""
    a, b = parse(D), parse(E)
    PS, PT = rep['PS'], rep['PT']
    if b['PS'] != PS or b['C'] != a['C'] or b['R'] != a['R'] or b['year'] != a['year'] or E[0x2c:0x30] != D[0x2c:0x30]:
        raise ImportError_('re-parse: layout / marks differ')
    if len(E) != len(D) + (PS - S0) * REC:
        raise ImportError_('file size')
    La, Lb = a['L'], b['L']
    X, _, _ = fix_ids(D, La, PS)
    po, d = La['person'][0], (PS - S0) * REC
    for k, (o, c, s) in La.items():
        ob, cb, sb = Lb[k]
        want = (o, PS, s) if k == 'person' else (o if o < po else o + d, c, s)
        if (ob, cb, sb) != want:
            raise ImportError_(f'section {k} misplaced')
    pe = po + REC * S0
    if E[:PS_OFF] + E[PS_OFF + 4:pe] != X[:PS_OFF] + X[PS_OFF + 4:pe] or E[pe + d:] != X[pe:]:
        raise ImportError_('bytes outside the extended officers differ from input + id fixes')
    if E[PS_OFF:PS_OFF + 4] != D[PS_OFF:PS_OFF + 1] + b'P' + struct.pack('<H', PS):
        raise ImportError_('PS mark')
    P = recs(E, Lb, 'person')
    allst = collections.Counter()
    bad = []
    for i, r in enumerate(P):
        if not all(big5_ok(r[5 * k:5 * k + 5]) for k in range(3)) or r[4] or r[9] or r[14]:
            bad.append(i)
        allst[STATUS.get(r[100], r[100])] += 1
        for off, what in KOEI_REL:
            v = i16(r, off)
            okv = v == -1 or 0 <= v < (PS if i >= S0 else PT) or \
                (off in (65, 67) and PT > VF_LO and VF_NEW <= v <= VF_NEW + 60)
            if not okv:
                raise ImportError_(f'officer {i}: {what} = {v} out of range')
        if i >= S0:
            o = roster[i - S0]
            if bytes(r[0:5]).split(b'\0')[0].decode('cp950') != o['sei'] or \
                    bytes(r[5:10]).split(b'\0')[0].decode('cp950') != o['mei'] or i16(r, 96) != o['city_id']:
                raise ImportError_(f'officer {i}: name / city mismatch')
    if bad:
        raise ImportError_(f'names that are not Big5: {bad[:20]}')
    rep['all_status'] = dict(sorted(allst.items()))
    rep['size_in'], rep['size_out'] = len(D), len(E)
    rep['name_sample'] = {i: show(P[i][0:5]) + show(P[i][5:10]) for i in (0, 91, 184, 225, 700, 849, 850, PS - 1)}
    return rep


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('inp')
    ap.add_argument('out')
    ap.add_argument('--roster', default=str(HERE.parent / 'data' / 'persons' / 'roster.json'))
    ap.add_argument('--report', help='write the check report (JSON) here')
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    try:
        roster = json.loads(Path(a.roster).read_text(encoding='utf-8'))
        D = Path(a.inp).read_bytes()
        E, rep = import_roster(D, roster)
        rep = verify(D, E, roster, rep)
        Path(a.out).write_bytes(E)
        rep['in'], rep['out'] = str(a.inp), str(a.out)
        if a.report:
            Path(a.report).write_text(json.dumps(rep, ensure_ascii=False, indent=1), encoding='utf-8')
        print(f'{Path(a.inp).name}: year {rep["start_year"]}, PS={rep["PS"]}, {len(D)} -> {len(E)} bytes, '
              f'extended {rep["extended_status"]}, VF {rep["vf_converted"]}, shifted {rep["new_special_ids_shifted"]}')
        return 0
    except ImportError_ as e:
        print('error:', e, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
