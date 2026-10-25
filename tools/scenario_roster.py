"""剧本势力（名单版）：用扩展武将名单（data/persons/roster.json）里的真实人物，给世界版剧本加上历史势力。

    python tools/scenario_roster.py scenarios/world_198.json --data <世界数据目录> --in <导入名单后的 Scen*.s11> \
        --out <输出 .s11> --roster <roster.json> [--report r.json]

和 scenario_build.py 的区别：scenario_build 在导入名单（import_roster.py）之前运行，新武将只能用光荣留的 30 个空位；
这个工具在导入名单之后运行，武将直接用名单里的人（编号 850 以后，也可以用光荣原版里未登场、未发现、在野的武将），
所以一个剧本可以加几十个势力、几百个武将。--in 是 import_roster.py 的输出（不改它），结果写到 --out，
所以重复运行不会越改越乱。剧本 JSON 的格式见 docs/剧本制作.md「用名单里的人物建势力」一节，例子见 scenarios/world_198.json。

人物用名字找，不用编号（名单重新生成时编号会变）：`"p"` 先对外文原名 name_orig，再对中文全名 name_full，再对姓+名；
同名的多个人用 `"born"`（大约的生年）挑最接近的。找到的人必须在剧本开始那年还活着、没有属于别的势力。
名单里登场年份晚于剧本年份的人，登场年改成剧本年份；名单生年明显不对（剧本那年还没出生或不到 14 岁）而 JSON 写了
`"birth"` 的，生年改成 JSON 的值。每一处改动都写进报告。
"""
import argparse
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import import_roster as ir                                                   # noqa: E402
from scenario_build import big5, BuildError, KOKUGO_FREE, TEMPLATE_CITY, SCEN_FILES  # noqa: E402

REC = 152
ST_RULER, ST_GENERAL, ST_FREE, ST_UNAPPEARED, ST_UNFOUND, ST_DEAD = 0, 3, 4, 6, 7, 8
STATUS = ir.STATUS
CONF = {'A': 3, 'B': 2, 'C': 1, 'D': 0}
DEFAULT_TIERS = {"1": {"troops": 25000, "gold": 5000, "food": 50000},
                 "2": {"troops": 12000, "gold": 3000, "food": 25000},
                 "3": {"troops": 5000, "gold": 1500, "food": 10000},
                 "koei": {"troops": 20000, "gold": 4000, "food": 40000}}


class Scen:
    """a world scenario after import_roster (PS officers)"""

    def __init__(self, data):
        self.D = bytearray(data)
        info = ir.parse(self.D)
        self.L, self.C, self.R, self.PS, self.year = info['L'], info['C'], info['R'], info['PS'], info['year']
        self.F = self.R + 5
        self.W = (self.F + 31) // 32

    def at(self, sect, i):
        o, c, s = self.L[sect]
        if not 0 <= i < c:
            raise BuildError(f'{sect} 编号 {i} 超出范围 0~{c - 1}')
        return o + s * i

    def i16(self, off):
        return struct.unpack_from('<h', self.D, off)[0]

    def pname(self, i):
        o = self.at('person', i)
        return ir.show(self.D[o:o + 5]) + ir.show(self.D[o + 5:o + 10])

    def ruler(self, f):
        return self.i16(self.at('force', f))

    def forces(self):
        return [f for f in range(self.F) if self.ruler(f) != -1]

    def owner(self, c):
        return self.D[self.at('building', c) + 1]

    def corps_force(self, k):
        return self.D[self.at('army', k)]


def city_table(data_dir):
    """name -> id, id -> (name, tier), id -> (lo, hi)"""
    names, info, pos = {}, {}, {}
    wi = json.loads((Path(data_dir) / 'world_info.json').read_text(encoding='utf-8'))   # Koei's cities: China block
    for k in json.loads((HERE.parent / 'data' / 'cities_georef.json').read_text(encoding='utf-8')):
        names[k['name']] = k['id']
        info[k['id']] = (k['name'], 'koei')
        pos[k['id']] = (k['lo'] + wi['china_lo'], k['hi'] + wi['china_hi'])
    for c in json.loads((Path(data_dir) / 'bases_cities.json').read_text(encoding='utf-8'))['new']:
        names[c['name']] = c['id']
        info[c['id']] = (c['name'], str(c.get('tier', 3)))
        pos[c['id']] = (c['lo'], c['hi'])
    return names, info, pos


class People:
    """roster lookup by name (ids change when the roster is rebuilt)"""

    def __init__(self, roster):
        self.R = roster
        self.by = {}
        for o in roster:
            for k in {o.get('name_orig') or '', o.get('name_full') or '', o['sei'] + o['mei']}:
                if k:
                    self.by.setdefault(k, []).append(o)

    def find(self, key, born=None, who=''):
        hits = self.by.get(key, [])
        if not hits:
            raise BuildError(f'{who}名单里找不到「{key}」（按外文原名、中文全名、姓+名找）')
        if len(hits) > 1:
            if born is None:
                raise BuildError(f'{who}「{key}」在名单里有 {len(hits)} 人（' +
                                 '、'.join(f"{o['id']} 生{o['birth']}" for o in hits) + '）：加 "born" 区分')
            hits = sorted(hits, key=lambda o: abs(o['birth'] - born))
        return hits[0]


def resources(cs, tier, tiers):
    t = dict(tiers.get(tier, tiers['3']))
    for k in ('troops', 'gold', 'food'):
        if k in cs:
            t[k] = cs[k]
    return t


def build(spec, scn, roster, data_dir):
    D, at = scn.D, scn.at
    F, R, year = scn.F, scn.R, scn.year
    names, cinfo, cpos = city_table(data_dir)
    people = People(roster)
    tiers = {**DEFAULT_TIERS, **spec.get('tiers', {})}
    rep = {'year': year, 'forces': [], 'fixes': [], 'auto': [], 'warnings': []}
    tc = bytes(D[at('city', TEMPLATE_CITY):at('city', TEMPLATE_CITY) + 81])
    live = scn.forces()
    used_kok = {D[at('force', f) + 4 + F + 1] for f in live}
    used_col = {D[at('force', f) + 4 + F + 2] for f in live}
    taken = set()                                   # persons placed by this spec
    taken_city = {}

    def cid(x, who):
        if isinstance(x, int):
            v = x
        elif x in names:
            v = names[x]
        else:
            raise BuildError(f'{who}：找不到城市「{x}」')
        if not 0 <= v < scn.C:
            raise BuildError(f'{who}：城市编号 {v} 超出范围')
        return v

    def person(o, fname):
        """-> person id for an officer entry"""
        if 'koei' in o:                             # a Koei officer 0..849 by name
            hits = [i for i in range(ir.S0) if scn.pname(i) == o['koei']]
            if not hits:
                raise BuildError(f'势力「{fname}」：光荣武将里没有「{o["koei"]}」')
            return hits[0], None
        r = people.find(o['p'], o.get('born'), f'势力「{fname}」：')
        return r['id'], r

    created = []
    for fs in spec['forces']:
        fname = fs['name']
        fid = next((f for f in range(R) if scn.ruler(f) == -1), None)
        kid = next((k for k in range(R) if D[at('army', k)] == 0xff), None)
        if fid is None or kid is None:
            raise BuildError(f'势力已满：这个剧本最多 {R} 个常规势力')
        # ---- 国号: 42 free slots; afterwards the ruler's name is shown (as Koei's own forces do)
        kok = 0xff
        if fs.get('kokugo', True):
            kok = next((k for k in KOKUGO_FREE if k not in used_kok), 0xff)
            if kok == 0xff:
                rep['warnings'].append(f'国号空位用完：「{fname}」不设国号，游戏里用君主名称呼')
            else:
                used_kok.add(kok)
        color = fs.get('color')
        if color is None:
            free = [c for c in range(55) if c not in used_col]
            if free:
                color = free[0]
            else:                                   # reuse the colour whose forces are farthest away
                first = fs['cities'][0]
                here = cpos[cid(first['city'] if isinstance(first, dict) else first, fname)]
                far = {}
                for f in scn.forces():
                    if f >= R:
                        continue
                    col = D[at('force', f) + 4 + F + 2]
                    for c in range(scn.C):
                        if scn.owner(c) == f and c in cpos:
                            d = abs(cpos[c][0] - here[0]) + abs(cpos[c][1] - here[1])
                            far[col] = min(far.get(col, 10 ** 9), d)
                color = max((c for c in far if c < 50), key=lambda c: far[c])
                rep['warnings'].append(f'55 种颜色用完：「{fname}」和远处的势力同色（{color}）')
        if not 0 <= color <= 54:
            raise BuildError(f'势力「{fname}」的颜色 {color} 不在 0~54')
        used_col.add(color)
        # ---- cities
        cities = []
        for cs in fs['cities']:
            cs = cs if isinstance(cs, dict) else {'city': cs}
            c = cid(cs['city'], f'势力「{fname}」')
            if scn.owner(c) != 0xff or D[at('city', c)] != 0xff:
                raise BuildError(f'城市「{cs["city"]}」在这个剧本里已经有主人了')
            if c in taken_city:
                raise BuildError(f'城市「{cs["city"]}」同时写在「{taken_city[c]}」和「{fname}」里')
            taken_city[c] = fname
            cities.append((c, cs))
        if not cities:
            raise BuildError(f'势力「{fname}」至少要有一座城')
        if fs.get('max_cities') is not None and len(cities) > fs['max_cities']:
            raise BuildError(f'势力「{fname}」有 {len(cities)} 座城，超过 max_cities={fs["max_cities"]}')
        own = {c for c, _ in cities}
        # ---- officers
        members = []                                 # (pid, city, entry)

        def place(pid, r, o, c, why):
            if pid in taken:
                raise BuildError(f'「{scn.pname(pid)}」（{pid}）被安排了两次')
            po = at('person', pid)
            st = D[po + 100]
            death = struct.unpack_from('<h', D, po + 60)[0]
            if st == ST_DEAD or death < year:
                raise BuildError(f'势力「{fname}」：「{scn.pname(pid)}」（{pid}）卒于 {death}，{year} 年已不在世')
            if st not in (ST_FREE, ST_UNAPPEARED, ST_UNFOUND):
                raise BuildError(f'势力「{fname}」：「{scn.pname(pid)}」（{pid}）在剧本里是「{STATUS.get(st, st)}」，不能再安排')
            taken.add(pid)
            members.append((pid, c, o))
            fix = {}
            appear, birth = struct.unpack_from('<hh', D, po + 56)
            if 'birth' in o and birth > year - 14:
                fix['birth'] = (birth, o['birth'])
                struct.pack_into('<h', D, po + 58, o['birth'])
            if appear > year:
                fix['appear'] = (appear, year)
                struct.pack_into('<h', D, po + 56, year)
            if fix:
                rep['fixes'].append({'force': fname, 'id': pid, 'name': scn.pname(pid),
                                     'orig': (r or {}).get('name_orig', ''), **{k: list(v) for k, v in fix.items()}})
            b = struct.unpack_from('<h', D, po + 58)[0]
            if b > year - 14:
                rep['warnings'].append(f'「{scn.pname(pid)}」（{pid}）生于 {b}，{year} 年才 {year - b} 岁')
            D[po + 95] = kid
            struct.pack_into('<hh', D, po + 96, c, c)
            D[po + 100] = ST_GENERAL
            D[po + 104] = o.get('loyalty', fs.get('loyalty', 95))
            struct.pack_into('<H', D, po + 105, o.get('merit', 3000))

        for o in fs.get('officers', []):
            pid, r = person(o, fname)
            c = cid(o['city'], f'势力「{fname}」') if 'city' in o else cities[0][0]
            if c not in own:
                raise BuildError(f'势力「{fname}」：「{o.get("p", o.get("koei"))}」所在的「{o.get("city")}」不是这个势力的城')
            place(pid, r, o, c, 'spec')
        # ---- auto: roster officers of the given polities whose city is one of this force's cities
        au = fs.get('auto')
        if au:
            pols, roles = set(au.get('polity', [])), set(au.get('roles', []))
            minc = CONF[au.get('min_conf', 'B')]
            exclude = set(au.get('exclude', []))
            n = 0
            for r in roster:
                if r['polity'] not in pols or (roles and r['role'] not in roles) or CONF.get(r['conf'], 0) < minc:
                    continue
                if r['city_id'] not in own or r['id'] in taken or (r.get('name_orig') or r['name_full']) in exclude:
                    continue
                if not (r['appear'] <= year <= r['death'] and r['birth'] <= year - au.get('min_age', 16)):
                    continue
                if r['birth'] + au.get('max_age', 75) < year:
                    continue
                po = at('person', r['id'])
                if D[po + 100] not in (ST_UNAPPEARED, ST_UNFOUND, ST_FREE):
                    continue
                place(r['id'], r, {}, r['city_id'], 'auto')
                rep['auto'].append({'force': fname, 'id': r['id'], 'name': scn.pname(r['id']),
                                    'orig': r.get('name_orig', ''), 'city': cinfo[r['city_id']][0], 'conf': r['conf']})
                n += 1
        # ---- ruler
        rk = fs['ruler']
        rpid = next((pid for pid, c, o in members if o.get('p', o.get('koei')) == rk), None)
        if rpid is None:
            raise BuildError(f'势力「{fname}」的君主「{rk}」必须是它 officers 里的一个')
        po = at('person', rpid)
        D[po + 100] = ST_RULER
        D[po + 104] = 100
        struct.pack_into('<H', D, po + 105, 10000)
        D[po + 148] = 200
        strat = -1
        if fs.get('strategist'):
            strat = next((pid for pid, c, o in members if o.get('p', o.get('koei')) == fs['strategist']), None)
            if strat is None:
                raise BuildError(f'势力「{fname}」的军师「{fs["strategist"]}」必须是它 officers 里的一个')
        # ---- corps, force record, 国号, intro
        D[at('army', kid):at('army', kid) + 10] = (bytes([fid, 1]) + struct.pack('<h', rpid) + bytes([0]) +
                                                   struct.pack('<h', 0x1c) + bytes([0]) + struct.pack('<h', -1))
        rel = [50] * F
        rel[fid] = 100
        for k in range(R, F):
            rel[k] = 0
        fo = at('force', fid)
        D[fo:fo + 4 + F + 14 + 4 * scn.W] = (struct.pack('<hh', rpid, strat) + bytes(rel) + bytes([9, kok, color, 0]) +
                                             struct.pack('<h', -1) + bytes(4 * scn.W + 8))
        for k in scn.forces():
            if k != fid:
                D[at('force', k) + 4 + fid] = 0 if k >= R else 50
        if kok != 0xff:
            to = at('type14', kok)
            D[to:to + 5] = big5(fs.get('kokugo_name', fname), 5, '国号')
        intro = None
        if fs.get('intro'):
            if fid < 42:
                do = scn.L['force_desc'][0] + 369 * fid
                D[do] = fs.get('stars', 3)
                D[do + 1:do + 369] = big5(fs['intro'], 368, '势力介绍')
                intro = True
            else:
                rep['warnings'].append(f'「{fname}」是 {fid} 号势力：剧本只有 0~41 号的势力介绍，intro 没写进去')
                intro = False
        # ---- cities
        for c, cs in cities:
            D[at('building', c) + 1] = fid
            co = at('city', c)
            r = bytearray(D[co:co + 81])
            res = resources(cs, cinfo.get(c, ('', '3'))[1], tiers)
            r[0] = kid
            struct.pack_into('<III', r, 5, res['troops'], res['gold'], res['food'])
            r[17:65] = tc[17:65]
            r[73:75] = tc[73:75]
            D[co:co + 81] = r
            if c < 42:
                D[0x1DB + c] = color
        rc = struct.unpack_from('<h', D, at('person', rpid) + 96)[0]
        created.append({'fid': fid, 'corps': kid, 'name': fname, 'kokugo': kok if kok != 0xff else None,
                        'color': color, 'intro': intro, 'ruler': rpid, 'ruler_name': scn.pname(rpid),
                        'ruler_city': cinfo[rc][0], 'cities': [cinfo[c][0] for c, _ in cities],
                        'officers': len(members), 'auto': sum(1 for a in rep['auto'] if a['force'] == fname)})
    # ---- relations (by force name, or by an existing force's ruler name)
    fid_of = {x['name']: x['fid'] for x in created}
    for f in live:
        fid_of.setdefault(scn.pname(scn.ruler(f)), f)
    for fs in spec['forces']:
        me = fid_of[fs['name']]
        for other, v in fs.get('relations', {}).items():
            if other not in fid_of:
                raise BuildError(f'relations 里的「{other}」不是任何势力（写势力名或原有势力的君主名）')
            o = fid_of[other]
            v = max(0, min(100, int(v)))
            D[at('force', me) + 4 + o] = v
            D[at('force', o) + 4 + me] = v
    rep['forces'] = created
    return rep


def check(scn, rep, before):
    """consistency of the whole file after the build"""
    D, at, F, R = scn.D, scn.at, scn.F, scn.R
    errs = []
    E = ir.parse(D)
    if E['L'] != scn.L or E['PS'] != scn.PS:
        errs.append('re-parse: layout differs')
    if len(D) != len(before):
        errs.append('file size changed')
    seen = {}
    for x in rep['forces']:
        f, k = x['fid'], x['corps']
        if scn.ruler(f) != x['ruler'] or scn.corps_force(k) != f:
            errs.append(f'{x["name"]}: force / corps record')
        rp = at('person', x['ruler'])
        rc = struct.unpack_from('<h', D, rp + 96)[0]
        if D[rp + 100] != ST_RULER or D[rp + 95] != k or scn.owner(rc) != f:
            errs.append(f'{x["name"]}: ruler not in an own city')
    corps_of = {}
    for k in range(F):
        if D[at('army', k)] != 0xff:
            corps_of[k] = D[at('army', k)]
    new_f = {x['fid'] for x in rep['forces']}
    count = {f: 0 for f in new_f}
    for i in range(scn.PS):
        po = at('person', i)
        st, k = D[po + 100], D[po + 95]
        if st in (0, 1, 2, 3):
            f = corps_of.get(k)
            loc = struct.unpack_from('<hh', D, po + 96)
            if f in new_f:
                count[f] += 1
                if loc[0] != loc[1] or not 0 <= loc[0] < scn.C or scn.owner(loc[0]) != f:
                    errs.append(f'officer {i} {scn.pname(i)}: city {loc} not owned by force {f}')
                if i in seen:
                    errs.append(f'officer {i} twice')
                seen[i] = f
            if f is None:
                errs.append(f'officer {i} {scn.pname(i)}: status {st} with corps {k} that has no force')
            nb = bytes(D[po:po + 15])
            if not all(ir.big5_ok(nb[5 * j:5 * j + 5]) for j in range(3)):
                errs.append(f'officer {i}: name not Big5')
    for x in rep['forces']:
        if count[x['fid']] != x['officers']:
            errs.append(f'{x["name"]}: {count[x["fid"]]} officers in the file, {x["officers"]} placed')
    B = Scen(before)
    for c in range(scn.C):
        f = scn.owner(c)
        k = D[at('city', c)]
        if f == B.owner(c) and k == B.D[B.at('city', c)]:
            continue                                 # untouched (Koei's 194 has 宛 in 張濟's corps without an owner)
        if (f == 0xff) != (k == 0xff) or (f != 0xff and corps_of.get(k) != f):
            errs.append(f'city {c}: owner {f} / corps {k} disagree')
    # nothing outside the touched sections changes
    touched = ['person', 'force', 'army', 'city', 'building', 'type14', 'force_desc', 'scen_info']
    for name, (o, c, s) in scn.L.items():
        if name not in touched and before[o:o + c * s] != bytes(D[o:o + c * s]):
            errs.append(f'section {name} changed')
    return errs


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('spec')
    ap.add_argument('--data', required=True, help='世界数据目录（有 bases_cities.json）')
    ap.add_argument('--in', dest='inp', required=True, help='import_roster.py 输出的剧本')
    ap.add_argument('--out', required=True)
    ap.add_argument('--roster', default=str(HERE.parent / 'data' / 'persons' / 'roster.json'))
    ap.add_argument('--report')
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    try:
        spec = json.loads(Path(a.spec).read_text(encoding='utf-8'))
        roster = json.loads(Path(a.roster).read_text(encoding='utf-8'))
        before = Path(a.inp).read_bytes()
        scn = Scen(before)
        if scn.PS != ir.S0 + len(roster):
            raise BuildError(f'剧本里有 {scn.PS} 个武将，名单是 {ir.S0 + len(roster)}：--in 要用同一份名单导入的文件')
        if 'base' in spec and Path(a.inp).name.lower() != SCEN_FILES[spec['base']].lower():
            raise BuildError(f'JSON 的 base={spec["base"]} 是 {SCEN_FILES[spec["base"]]}，--in 却是 {Path(a.inp).name}')
        rep = build(spec, scn, roster, a.data)
        errs = check(scn, rep, before)
        if errs:
            raise BuildError('检查不通过：\n  ' + '\n  '.join(errs[:30]))
        Path(a.out).write_bytes(bytes(scn.D))
        if a.report:
            Path(a.report).write_text(json.dumps(rep, ensure_ascii=False, indent=1), encoding='utf-8')
        n = sum(x['officers'] for x in rep['forces'])
        print(f'{Path(a.out).name}: {len(rep["forces"])} 个势力，{n} 名武将（自动 {len(rep["auto"])}），'
              f'改了 {len(rep["fixes"])} 人的登场/生年，提示 {len(rep["warnings"])} 条')
        for w in rep['warnings']:
            print('  提示：', w)
        return 0
    except BuildError as e:
        print('错误：', e, file=sys.stderr)
        return 1
    except (KeyError, ValueError, json.JSONDecodeError) as e:
        print(f'错误：剧本 JSON 写得不对：{e!r}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
