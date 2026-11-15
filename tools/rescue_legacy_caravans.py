"""Inspect expanded v13 saves and stage bounded, one-time grain transfers."""
import ctypes as ct
import hashlib
import json
import struct
import zlib
from pathlib import Path


def digest(data):
    return hashlib.sha256(data).hexdigest()


def trade_payload(block):
    magic, version, size, raw_size = struct.unpack_from('<4sIII', block)
    if magic != b'TRAD' or version != 13 or size != len(block)-12 or raw_size > 128*1024*1024:
        raise ValueError('Expected bounded terminal v13 TRAD block')
    api = ct.WinDLL('cabinet', use_last_error=True)
    api.CreateDecompressor.argtypes = [ct.c_uint32, ct.c_void_p, ct.POINTER(ct.c_void_p)]
    api.Decompress.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t)]
    api.CloseDecompressor.argtypes = [ct.c_void_p]
    handle = ct.c_void_p()
    if not api.CreateDecompressor(4, None, ct.byref(handle)):
        raise ct.WinError(ct.get_last_error())
    try:
        source = ct.create_string_buffer(block[16:])
        result = ct.create_string_buffer(raw_size)
        actual = ct.c_size_t()
        if not api.Decompress(handle, source, len(block)-16, result, raw_size, ct.byref(actual)) or actual.value != raw_size:
            raise ValueError('Invalid trade compression')
        return result.raw
    finally:
        api.CloseDecompressor(handle)


def inspect_save(path):
    data = path.read_bytes()
    if data[435:439] != b'MWC\x1a' or data[44:48] != b'WF\xf9\x00':
        raise ValueError('Only the verified 249-force world layout is supported')
    raw_size, packed_size = struct.unpack_from('<II', data, 439)
    if len(data) != 447+packed_size or raw_size > 128*1024*1024:
        raise ValueError('Invalid save size')
    raw = zlib.decompress(data[447:])
    if len(raw) != raw_size:
        raise ValueError('Invalid world compression')
    trad_at = raw.rfind(b'TRAD')
    pl = trade_payload(raw[trad_at:])
    cities, goods = struct.unpack_from('<II', pl)
    if (cities, goods) != (530, 72) or data[32:36] != bytes.fromhex('01515337'):
        raise ValueError('Unexpected city, commodity or officer layout')
    pos = 8+cities*goods*4
    month, rng, next_ai, count = struct.unpack_from('<iIII', pl, pos)
    if count > 1000:
        raise ValueError('Invalid caravan count')
    cars = [struct.unpack_from('<12iI', pl, pos+16+i*52) for i in range(count)]
    # Native serializers 0x483120, 0x481d00, 0x47c900 and 0x497270;
    # expanded relation rows / alliance bitsets and force counts are included.
    city_at = 2212+207+16384*18+14613*203+100*58+254*(214+414+24)+254*17
    unit_at = city_at+530*108+45*70+84*89
    army_at = city_at-254*17
    rows = []
    for car in cars:
        uid, leader, force, corps, origin, dest = car[:6]
        if not (0 <= uid < 1000 and 0 <= origin < cities and 0 <= dest < cities):
            raise ValueError('Invalid caravan identity')
        u = unit_at+uid*98
        if struct.unpack_from('<h', raw, u+1)[0] != leader:
            raise ValueError('Native unit directory does not match TRAD identities')
        rows.append(dict(unit=uid, leader=leader, force=force, origin=origin, destination=dest,
                         flags=car[12], type=raw[u], troops=struct.unpack_from('<H', raw, u+7)[0],
                         food=struct.unpack_from('<I', raw, u+14)[0],
                         position=struct.unpack_from('<hh', raw, u+24),
                         city_food=struct.unpack_from('<I', raw, city_at+origin*108+15)[0],
                         city_owner=(raw[army_at+raw[city_at+origin*108+2]*17] if raw[city_at+origin*108+2]<254 else 255)))
    return data, raw, dict(source=str(path), sha256=digest(data), month=month,
                           city_offset=city_at, unit_offset=unit_at, trad_offset=trad_at, caravans=rows)


def transfers(rows, routes, reserve=9000):
    """Pure planning. Never partially fund a caravan or share another force's food."""
    balances, changes, skipped = {}, [], []
    eligible = [r for r in rows if r['flags'] & 2 and not r['flags'] & 4 and r['type'] == 1]
    for row in sorted(eligible, key=lambda r: r['food']/max(1, (r['troops']+9)//10)):
        uid, origin = row['unit'], row['origin']
        paths = routes.get(str(uid), {})
        if any('steps' not in paths.get(role, {}) for role in ('out', 'home')):
            skipped.append(dict(unit=uid, reason='no verified safe remaining/return route'))
            continue
        # A conservative route bound: current->destination->current->home.
        # It does not teleport the caravan or alter its orders.
        steps = paths['home']['steps'] if row['destination']==origin else 2*paths['out']['steps']+paths['home']['steps']
        turns = (steps+4)//5
        required = max(3000, ((row['troops']+9)//10)*(turns+(turns+3)//4+4))
        if required <= row['food']:
            continue
        if required > 100000:
            skipped.append(dict(unit=uid, reason='native food capacity exceeded'))
            continue
        if row['city_owner'] != row['force']:
            skipped.append(dict(unit=uid, reason='funding city changed owner'))
            continue
        available = balances.setdefault(origin, row['city_food'])
        delta = required-row['food']
        if available-delta < reserve:
            skipped.append(dict(unit=uid, reason='city food insufficient after military reserve', required=delta))
            continue
        balances[origin] -= delta
        changes.append(dict(unit=uid, origin=origin, force=row['force'], before=row['food'], after=required,
                            transferred=delta, city_before=available, city_after=available-delta,
                            safe_route_steps=steps))
    return changes, skipped


def stage(path, route_file, validation_file, target):
    if path.resolve() == target.resolve():
        raise ValueError('Stage must be a different file')
    data, raw, report = inspect_save(path)
    evidence = json.loads(route_file.read_text(encoding='utf-8'))
    validation = json.loads(validation_file.read_text(encoding='utf-8'))
    if evidence['save_sha256'] != report['sha256'] or validation['save_sha256'] != report['sha256']:
        raise ValueError('Native evidence belongs to a different save')
    if validation['native_cities_verified'] != 530 or validation['native_units_verified'] != len(report['caravans']):
        raise ValueError('Incomplete native layout validation')
    changes, skipped = transfers(report['caravans'], evidence['routes'])
    result, allowed = bytearray(raw), set()
    for row in changes:
        u = report['unit_offset']+row['unit']*98+14
        city = report['city_offset']+row['origin']*108+15
        if struct.unpack_from('<I', result, u)[0] != row['before'] or struct.unpack_from('<I', result, city)[0] != row['city_before']:
            raise ValueError('Transaction inputs changed')
        struct.pack_into('<I', result, u, row['after'])
        struct.pack_into('<I', result, city, row['city_after'])
        allowed.update(range(u, u+4))
        allowed.update(range(city, city+4))
    if any(a != b and i not in allowed for i, (a, b) in enumerate(zip(raw, result))):
        raise AssertionError('Non-grain world state changed')
    if result[report['trad_offset']:] != raw[report['trad_offset']:]:
        raise AssertionError('Trade cargo, history or plans changed')
    packed = zlib.compress(result)
    candidate = data[:439]+struct.pack('<II', len(result), len(packed))+packed
    if zlib.decompress(candidate[447:]) != result or candidate[:439] != data[:439]:
        raise AssertionError('Container round trip failed')
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(candidate)
    _, _, reread = inspect_save(target)
    if path.read_bytes() != data:
        raise RuntimeError('Original save changed during staging')
    names = validation['city_names']
    for row in changes:
        row['origin_name'] = names[str(row['origin'])]
    report.update(staged=str(target), after_sha256=digest(candidate), changes=changes, skipped=skipped,
                  total_grain=sum(r['transferred'] for r in changes), trade_block_unchanged=True,
                  only_native_grain_fields_changed=True, source_unchanged=True)
    return report


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('save', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--routes', type=Path)
    parser.add_argument('--native-validation', type=Path)
    parser.add_argument('--stage', type=Path)
    args = parser.parse_args()
    if args.stage:
        if not args.routes or not args.native_validation:
            parser.error('Staging requires route evidence and native validation')
        report = stage(args.save, args.routes, args.native_validation, args.stage)
    else:
        _, _, report = inspect_save(args.save)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('changes', 'skipped', 'total_grain')}, ensure_ascii=False, indent=2)
          if args.stage else json.dumps(sorted(report['caravans'], key=lambda r: r['food'])[:12], ensure_ascii=False, indent=2))
