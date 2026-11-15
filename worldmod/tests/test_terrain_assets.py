import struct
import unittest
from inventory_terrain_assets import decode_linear, ground_contract, ROOT


class TerrainAssets(unittest.TestCase):
    def test_linear_layouts(self):
        for bits in (24, 32):
            payload = bytes(range(2*3*(bits//8)))
            data = struct.pack('<8sIIHHI', b'WFTX0010', 24+len(payload), 1, 2, 3, bits)+payload
            images = decode_linear(data)
            self.assertEqual(images[0][1], payload)
            self.assertEqual(images[0][0]['width'], 2)
            self.assertEqual(images[0][0]['height'], 3)

    def test_malformed_and_unknown_layouts(self):
        valid = struct.pack('<8sIIHHI', b'WFTX0010', 27, 1, 1, 1, 24)+b'abc'
        for data in (valid[:10], valid[:-1], valid+b'x', valid[:20]+struct.pack('<I', 8)+valid[24:],
                     valid[:12]+struct.pack('<I', 0)+valid[16:]):
            with self.subTest(data=data), self.assertRaises(ValueError):
                decode_linear(data)

    def test_ground_loader_and_seasons(self):
        contract = ground_contract(ROOT/'build/render-game/san11pk_render.exe')
        self.assertEqual([r['resource'] for r in contract['seasons']], ['0x12c1', '0x12c2', '0x12c0', '0x12c3'])
        self.assertEqual(len(contract['verified_blocks']), 8)
        self.assertEqual(contract['optional_conversion']['format'], 'DXT1')
        self.assertEqual(contract['texture_selection']['record_stride'], 36)

    def test_local_material_usage(self):
        from analyze_ground_materials import analyze
        result = analyze()
        self.assertEqual(sum(row['china_vertices'] for row in result['materials']), 640000)
        sample = next(row for row in result['materials'] if row['material_index'] == 1)
        self.assertEqual(sample['hex_association'][0]['type'], 4)
        self.assertGreater(sample['hex_association'][0]['percent'], 90)
        if result['world']:
            self.assertEqual(result['world']['outside_ground_table_vertices'], 0)
            self.assertAlmostEqual(sum(row['world_share_percent'] for row in result['materials']), 100)


if __name__ == '__main__':
    unittest.main()
