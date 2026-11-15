"""Grow the scenario-officer range of a world save, for the virtual officer pool.

    python tools/expand_person_pool.py IN.s11 OUT.s11 [--add 4000] [--report r.json]

The runtime numbers officers 0..PS-1 (scenario), PS..PS+149 (new), PS+150..PT-1
(special: bandit / barbarian leaders and the trade system's temporary 商 leaders).
Raising worldmod.ini persons= by ADD moves the new / special officers up by ADD,
so a save made before must have ADD native blank records inserted at PS and every
reference to an officer id >= PS moved up. This tool does that and nothing else:

  * world: emperor, order list, officer relations and order parameters, item
    owners, force rulers / strategists, corps leaders, city / gate / port
    governors, unit leaders / deputies / officer targets;
  * trade block (TRAD, XPRESS_HUFF): caravan leaders and merchant records;
  * header: the officer count mark.

Regions whose layout is not decoded (the AI block and the rest between the units
and the hex map) are left byte-identical; every value there that could be an old
id of an officer in use is listed in the report. Only reads IN and writes OUT.
"""
import argparse
import collections
import ctypes as ct
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEAD, MWC = 435, b'MWC\x1a'
REC, STATUS, NEW = 203, 100, 150
SPECIALS = {ord('P'): 100, ord('Q'): 300}
P_REL = [63, 65, 67, 70, 72] + [75 + 2 * i for i in range(10)] + [102, 196]
P_TASK, P_PARAM = 169, 170
TASK_OFFICER_PARAMS = {0: (1,), 12: (0,), 23: (1,)}
CARAVAN = 52                                    # LegacyCaravan, leader at +4
XPRESS_HUFF = 4


class ConvError(Exception):
    pass


def sha(data):
    return hashlib.sha256(data).hexdigest()


def unpack(data):
    if data[HEAD:HEAD + 4] != MWC:
        raise ConvError('not an expanded world save (no MWC block)')
    raw_size, size = struct.unpack_from('<II', data, HEAD + 4)
    if len(data) != HEAD + 12 + size:
        raise ConvError('container length mismatch')
    raw = zlib.decompress(data[HEAD + 12:])
    if len(raw) != raw_size:
        raise ConvError('decompressed length mismatch')
    return bytes(data[:HEAD]), raw


def pack(head, raw):
    packed = zlib.compress(raw, 9)
    return head + MWC + struct.pack('<II', len(raw), len(packed)) + packed


def counts(head):
    if head[0x2c:0x2e] != b'WF':
        raise ConvError('header has no force-count mark')
    if head[32] != 1 or head[33] not in SPECIALS:
        raise ConvError(f'unsupported officer mark {head[32:36].hex()}')
    R = struct.unpack_from('<H', head, 0x2e)[0]
    PS = struct.unpack_from('<H', head, 34)[0]
    return R, PS, PS + NEW + SPECIALS[head[33]]


def trade_block(raw):
    at = raw.rfind(b'TRAD')
    while at >= 0:
        if at + 16 <= len(raw) and at + 12 + struct.unpack_from('<I', raw, at + 8)[0] == len(raw):
            return at
        at = raw.rfind(b'TRAD', 0, at)
    raise ConvError('no terminal TRAD block')


def layout(raw, R, PT, C, U, cells):
    """section -> (offset, count, record size); the tail sections by position only"""
    F = R + 5
    force = 214 + 2 * (F - 47) + 4 * ((R + 36) // 32) - 8
    out, at = {}, 0
    for name, count, size in [('head', 1, 2212 + R - 42), ('building', 16384, 18), ('person', PT, REC),
                              ('item', 100, 58), ('force', F, force), ('army', F, 17), ('city', C, 108),
                              ('gate_port', 45, 70), ('type14', 84, 89), ('unit', U, 98)]:
        out[name] = (at, count, size)
        at += count * size
    trad = trade_block(raw)
    hexmap = trad - cells * 4
    if hexmap < at:
        raise ConvError('world stream is shorter than its layout')
    out['tail'] = (at, 1, hexmap - at)
    out['hexmap'] = (hexmap, cells, 4)
    out['trade'] = (trad, 1, len(raw) - trad)
    return out


def check_layout(raw, L, C, PT):
    b = L['building'][0]
    if any(raw[b + 18 * i] for i in range(C)) or not raw[b + 18 * C]:
        raise ConvError('building table does not match the city count')
    p = L['person'][0]
    bad = [i for i in range(PT) if raw[p + REC * i + STATUS] not in (0, 1, 2, 3, 4, 5, 6, 7, 8, 255)]
    if bad:
        raise ConvError(f'officer records do not line up (first bad status at {bad[0]})')
    if raw[L['head'][0] + L['head'][2] - 64:L['head'][0] + L['head'][2]] != b'\xff' * 64:
        raise ConvError('order list does not end the world head')


def id_fields(raw, L):
    """every decoded officer id field of the world: (offset, struct format, label)"""
    head_end = L['head'][2]
    yield 5, '<h', 'emperor'
    for k in range(1000):
        yield head_end - 2000 + 2 * k, '<h', 'order list'
    p, count, _ = L['person']
    for i in range(count):
        b = p + REC * i
        for off in P_REL:
            yield b + off, '<h', f'officer +{off}'
        task = struct.unpack_from('<b', raw, b + P_TASK)[0]
        for k in TASK_OFFICER_PARAMS.get(task, ()):
            yield b + P_PARAM + 4 * k, '<i', f'officer order {task}'
    for name, off, label in [('item', 53, 'item owner'), ('force', 0, 'ruler'), ('force', 2, 'strategist'),
                             ('army', 2, 'corps leader'), ('city', 0, 'city governor'),
                             ('gate_port', 64, 'gate/port governor'), ('unit', 1, 'unit leader'),
                             ('unit', 3, 'unit deputy'), ('unit', 5, 'unit deputy')]:
        o, count, size = L[name]
        for i in range(count):
            yield o + size * i + off, '<h', label
    o, count, size = L['unit']
    for i in range(count):
        if raw[o + size * i + 21] == 3:
            yield o + size * i + 22, '<H', 'unit target officer'


def remap_world(raw, L, PS, PT, add):
    out, changed = bytearray(raw), collections.Counter()
    for at, fmt, label in id_fields(raw, L):
        v = struct.unpack_from(fmt, raw, at)[0]
        if PS <= v < PT:
            struct.pack_into(fmt, out, at, v + add)
            changed[label] += 1
    return out, changed


# ---------------------------------------------------------------- trade block
def _cabinet():
    api = ct.WinDLL('cabinet', use_last_error=True)
    for name in ('CreateCompressor', 'CreateDecompressor'):
        getattr(api, name).argtypes = [ct.c_uint32, ct.c_void_p, ct.POINTER(ct.c_void_p)]
    for name in ('Compress', 'Decompress'):
        getattr(api, name).argtypes = [ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.c_void_p, ct.c_size_t,
                                       ct.POINTER(ct.c_size_t)]
    for name in ('CloseCompressor', 'CloseDecompressor'):
        getattr(api, name).argtypes = [ct.c_void_p]
    return api


def trade_payload(block):
    magic, version, size, raw_size = struct.unpack_from('<4sIII', block)
    if magic != b'TRAD' or size != len(block) - 12:
        raise ConvError('malformed TRAD block')
    api, handle, actual = _cabinet(), ct.c_void_p(), ct.c_size_t()
    if not api.CreateDecompressor(XPRESS_HUFF, None, ct.byref(handle)):
        raise ct.WinError(ct.get_last_error())
    try:
        out = ct.create_string_buffer(raw_size)
        if not api.Decompress(handle, block[16:], len(block) - 16, out, raw_size, ct.byref(actual)) \
                or actual.value != raw_size:
            raise ConvError('TRAD decompression failed')
        return version, out.raw
    finally:
        api.CloseDecompressor(handle)


def trade_pack(version, payload):
    api, handle, size = _cabinet(), ct.c_void_p(), ct.c_size_t()
    if not api.CreateCompressor(XPRESS_HUFF, None, ct.byref(handle)):
        raise ct.WinError(ct.get_last_error())
    try:
        out = ct.create_string_buffer(len(payload) + 4096)
        if not api.Compress(handle, payload, len(payload), out, len(out), ct.byref(size)):
            raise ct.WinError(ct.get_last_error())
        block = struct.pack('<4sIII', b'TRAD', version, size.value + 4, len(payload)) + out.raw[:size.value]
    finally:
        api.CloseCompressor(handle)
    if trade_payload(block) != (version, payload):
        raise ConvError('TRAD block does not survive recompression')
    return block


def trade_fields(payload):
    """offsets of the officer ids in the payload prefix every version shares: caravan leaders, merchant records"""
    C, G = struct.unpack_from('<II', payload)
    at = 8 + 4 * C * G + 12                     # price index, last month, rng, next AI force
    active = struct.unpack_from('<I', payload, at)[0]
    at += 4
    if active > 100000 or at + active * CARAVAN > len(payload):
        raise ConvError('TRAD caravan table does not fit')
    fields = [(at + CARAVAN * i + 4, 'caravan leader') for i in range(active)]
    at += active * CARAVAN
    events = struct.unpack_from('<I', payload, at)[0]
    at += 4 + 12 * events
    done = struct.unpack_from('<I', payload, at)[0]
    at += 4 + 36 * done
    iR, iC, iG, iH = struct.unpack_from('<IIII', payload, at)
    at += 20
    m = at + iR * iC + 2 * iH * iC * iG         # information age table (u8), price history (i16)
    if m + 4 > len(payload):
        raise ConvError('TRAD information tables do not fit')
    n = struct.unpack_from('<I', payload, m)[0]
    if n > 100000 or m + 4 + 12 * n > len(payload):
        raise ConvError('TRAD merchant table does not fit')
    merchants = (m, n)
    fields += [(merchants[0] + 4 + 12 * i, 'merchant record') for i in range(merchants[1])]
    return fields


def remap_trade(block, PS, PT, add):
    version, payload = trade_payload(block)
    out, changed = bytearray(payload), collections.Counter()
    for at, label in trade_fields(payload):
        v = struct.unpack_from('<i', payload, at)[0]
        if PS <= v < PT:
            struct.pack_into('<i', out, at, v + add)
            changed[label] += 1
    return (trade_pack(version, bytes(out)) if changed else block), changed, payload, bytes(out)


# ---------------------------------------------------------------- conversion
def stray(data, ids, skip=()):
    """offsets where a 16-bit value equals an id of an officer in use, outside the remapped fields"""
    skip, hits = set(skip), []
    for v in ids:
        needle, at = struct.pack('<H', v), -1
        while (at := data.find(needle, at + 1)) >= 0:
            if at not in skip:
                hits.append((at, v))
    return sorted(hits)


def convert(data, C, U, cells, add):
    head, raw = unpack(data)
    R, PS, PT = counts(head)
    if PT + add > 32000:
        raise ConvError('officer count would exceed 32000')
    L = layout(raw, R, PT, C, U, cells)
    check_layout(raw, L, C, PT)
    p = L['person'][0]
    record = lambda i: raw[p + REC * i:p + REC * (i + 1)]
    blanks = collections.Counter(record(i) for i in range(PS + NEW, PT) if record(i)[STATUS] == 255)
    if not blanks or blanks.most_common(1)[0][1] < 20:
        raise ConvError('no native blank record in the special pool')
    blank = blanks.most_common(1)[0][0]
    used = [i for i in range(PS, PT) if record(i)[STATUS] != 255]

    trad = L['trade'][0]
    world, changed = remap_world(raw[:trad], L, PS, PT, add)
    block, trade_changed, payload, new_payload = remap_trade(raw[trad:], PS, PT, add)
    insert = p + REC * PS
    new_raw = bytes(world[:insert]) + blank * add + bytes(world[insert:]) + block
    new_head = bytearray(head)
    struct.pack_into('<H', new_head, 34, PS + add)
    result = pack(bytes(new_head), new_raw)

    # the inverse must give the input back
    back_head, back = unpack(result)
    if counts(back_head) != (R, PS + add, PT + add):
        raise ConvError('header mark not as written')
    L2 = layout(back, R, PT + add, C, U, cells)
    check_layout(back, L2, C, PT + add)
    if back[insert:insert + REC * add] != blank * add:
        raise ConvError('inserted records differ')
    trad2 = L2['trade'][0]
    undone, _ = remap_world(back[:trad2], L2, PS + add, PT + add, -add)
    if bytes(undone[:insert]) + bytes(undone[insert + REC * add:]) != raw[:trad]:
        raise ConvError('world is not restored by the inverse')
    if trade_payload(back[trad2:])[1] != new_payload:
        raise ConvError('trade payload not as written')
    restored = bytearray(new_payload)
    for at, _ in trade_fields(new_payload):
        v = struct.unpack_from('<i', new_payload, at)[0]
        if PS + add <= v < PT + add:
            struct.pack_into('<i', restored, at, v - add)
    if bytes(restored) != payload:
        raise ConvError('trade payload is not restored by the inverse')

    # every remapped reference must still name the same officer
    p2 = L2['person'][0]
    for at, fmt, label in id_fields(back, L2):
        v = struct.unpack_from(fmt, back, at)[0]
        if PS <= v < PS + add and fmt != '<H' and label not in ('order list',):
            raise ConvError(f'{label} at {at} names an inserted blank record ({v})')
    for i in used:
        a, b = record(i), back[p2 + REC * (i + add):p2 + REC * (i + add + 1)]
        if a[:10] != b[:10] or a[STATUS] != b[STATUS]:
            raise ConvError(f'officer {i} did not move to {i + add}')

    # values that merely look like an old id of an officer in use, outside every decoded field (for review)
    tail_o, _, tail_n = L['tail']
    tail = stray(raw[tail_o:tail_o + tail_n], used)
    trade = stray(payload, used, [at for at, _ in trade_fields(payload)])
    report = dict(R=R, PS=PS, PT=PT, add=add, used_special=len(used), world_changed=dict(changed),
                  trade_changed=dict(trade_changed), input_sha256=sha(data), output_sha256=sha(result),
                  lookalikes=dict(tail=len(tail), tail_first=tail[:8], trade=len(trade),
                                  trade_values=dict(collections.Counter(v for _, v in trade))))
    return result, report


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('input', type=Path)
    ap.add_argument('output', type=Path)
    ap.add_argument('--add', type=int, default=4000)
    ap.add_argument('--report', type=Path)
    a = ap.parse_args()
    if a.output.resolve() == a.input.resolve():
        ap.error('OUT must differ from IN')
    cfg = json.loads((ROOT / 'project.json').read_text(encoding='utf-8'))
    info = json.loads((Path(cfg['game_dir']) / cfg['data_name'] / 'world_info.json').read_text(encoding='utf-8'))
    try:
        result, report = convert(a.input.read_bytes(), cfg['cities'], cfg['units'],
                                 info['rows'] * info['cols'], a.add)
    except ConvError as e:
        print(f'错误：{e}', file=sys.stderr)
        return 1
    a.output.write_bytes(result)
    text = json.dumps(report, ensure_ascii=False, indent=2)
    if a.report:
        a.report.write_text(text + '\n', encoding='utf-8')
    print(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
