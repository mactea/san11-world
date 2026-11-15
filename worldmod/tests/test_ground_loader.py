import struct
import unittest
import json
from pathlib import Path

from ground_loader_contract import decode_wft, ground_identity, audit_loader
from prepare_ground_identities import read_resource, ROOT


def wft(records):
    body = b''.join(records)
    return struct.pack('<8sII', b'WFTX0010',16+len(body),len(records))+body


class GroundLoader(unittest.TestCase):
    def test_explicit_mips_and_row_padding(self):
        base = b'abcdef'+b'XY'+b'ghijkl'+b'ZW'
        record = struct.pack('<HH4B',2,2,24,0,1,0)+base+b'mnoP'
        meta, levels, palettes = decode_wft(wft([record]))[0]
        self.assertEqual(levels,[b'abcdefghijkl',b'mno'])
        self.assertEqual([m['pitch'] for m in meta['mips']],[8,4])
        self.assertEqual(palettes,b'')

    def test_palettes_empty_and_bounds(self):
        palette = bytes(range(256))*8
        record = struct.pack('<HH4B',3,1,8,2,0,0)+b'abcX'+palette
        self.assertEqual(decode_wft(wft([record]))[0][2],palette)
        self.assertEqual(decode_wft(wft([])),[])
        for data in (wft([record[:-1]]),wft([record+b'x']),wft([struct.pack('<HH4B',1,1,32,0,1,0)+b'1234']),
                     wft([struct.pack('<HH4B',1,1,8,0,0,0)+b'1234'])):
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                decode_wft(data)

    def test_half_size_floor_and_opaque_x(self):
        # Integer sum/4 deliberately differs from rounded image-library BOX.
        pixels = bytearray(64*64*3)
        for y in range(64):
            for x in range(64):
                pixels[(y*64+x)*3:(y*64+x)*3+3] = bytes((x%2+2*(y%2),17,253))
        meta = {'width':64,'height':64,'bits':24,'mip_count':1}
        shape, original = ground_identity(meta,bytes(pixels))
        self.assertEqual(shape,(64,64,22,1))
        self.assertEqual(original[:8],bytes((0,17,253,255,1,17,253,255)))
        shape, reduced = ground_identity(meta,bytes(pixels),True)
        self.assertEqual(shape,(32,32,22,1))
        self.assertEqual(reduced,bytes((1,17,253,255))*1024)
        with self.assertRaises(ValueError):
            ground_identity({**meta,'mip_count':7},bytes(pixels))

    def test_local_loader_and_previously_unverified_resources(self):
        self.assertEqual(len(audit_loader(ROOT/'build/render-game/san11pk_render.exe')),13)
        project = json.loads((ROOT.parent/'project.json').read_text(encoding='utf-8'))
        pack = Path(project['game_dir'])/'Media/san11pkres.bin'
        expected = {0x893:5,0x12c4:3,0x12f3:2,0x12f4:2,0x12f8:0,0x12f9:3,0x1307:2}
        for resource, count in expected.items():
            self.assertEqual(len(decode_wft(read_resource(pack,resource))),count)
        for resource in (0x12c0,0x12c1,0x12c2,0x12c3):
            rows = decode_wft(read_resource(pack,resource))
            self.assertEqual(len(rows),36)
            meta, levels, _ = rows[1]
            self.assertEqual((meta['bits'],meta['mip_count']),(24,1))
            self.assertEqual(ground_identity(meta,levels[0])[0],(64,64,22,1))


if __name__ == '__main__':
    unittest.main()
