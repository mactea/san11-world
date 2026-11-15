"""Validate the local executable contract without executing it."""
from pathlib import Path
import tempfile
import unittest
import struct

import pefile

from audit_render_paths import audit, CONTRACT, ROOT
from extract_water_fixture import decode


class AuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = ROOT / 'build/render-game/san11pk_render.exe'
        cls.data = cls.source.read_bytes()

    def test_local_contract(self):
        with tempfile.TemporaryDirectory() as folder:
            result = audit(self.source, Path(folder))
            self.assertFalse(result['game_executed'])
            self.assertEqual(len(result['shaders']), 14)
            self.assertEqual(len(result['indexed_wrapper_calls']), 19)
            self.assertIn({'site': '0x422d24', 'role': 'water'}, result['indexed_wrapper_calls'])
            self.assertEqual(result['world_object_contract']['declaration_elements'][3], (1, 0, 4, 0, 10, 0))

    def test_changed_contract_rejected_before_output(self):
        pe = pefile.PE(data=self.data, fast_load=True)
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for name, (begin, _, _) in CONTRACT.items():
                with self.subTest(block=name):
                    changed = bytearray(self.data)
                    changed[pe.get_offset_from_rva(begin - 0x400000)] ^= 1
                    copy = root / 'changed.exe'
                    copy.write_bytes(changed)
                    with self.assertRaisesRegex(ValueError, 'unsupported ' + name):
                        audit(copy, root / 'output')
                    self.assertFalse((root / 'output').exists())

    def test_water_fixture_revision_guard(self):
        atlases = [(ROOT / f'build/water-atlas-{i}.bgra').read_bytes() for i in range(2)]
        resource = struct.pack('<8sII', b'WFTX0010', 524320, 2)
        resource += b''.join(struct.pack('<HHI', 256, 256, 32) + pixels for pixels in atlases)
        self.assertEqual(decode(resource), atlases)
        with self.assertRaisesRegex(ValueError, 'unsupported water resource hash'):
            decode(resource[:-1])
        changed = bytearray(resource)
        changed[24] ^= 1
        with self.assertRaisesRegex(ValueError, 'unsupported water resource hash'):
            decode(changed)


if __name__ == '__main__':
    unittest.main()
