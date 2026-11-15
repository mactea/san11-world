import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from remove_cross_era_persons import patch_people, patch_units, verify_diff


class RemovalTests(unittest.TestCase):
    def test_promotes_existing_deputy_without_changing_unit_resources(self):
        original = bytes([37]) * 98
        original = original[:1] + struct.pack('<3h', 2348, 988, 1178) + original[7:]
        result, edits, units = patch_units(original, 0, 1, 98, {2348})
        self.assertEqual(struct.unpack_from('<3h', result, 1), (988, 1178, -1))
        self.assertEqual(result[7:], original[7:])
        verify_diff(original, result, edits)

    def test_stops_before_orphaning_unit(self):
        original = bytearray(98)
        struct.pack_into('<3h', original, 1, 2348, -1, -1)
        with self.assertRaisesRegex(AssertionError, 'no leader'):
            patch_units(bytes(original), 0, 1, 98, {2348})

    def test_disables_slot_and_clears_incoming_relationships(self):
        original = bytearray(b'\xff' * (203 * 2))
        original[100] = 3
        original[203 + 100] = 6
        struct.pack_into('<h', original, 203 + 65, 0)
        original[203:203 + 10] = b'other-name'
        result, edits = patch_people(bytes(original), 0, 2, 203, [{'id': 0, 'status': 3}])
        self.assertEqual(result[100], 255)
        self.assertEqual(struct.unpack_from('<h', result, 203 + 65)[0], -1)
        self.assertEqual(result[203:203 + 10], b'other-name')
        self.assertEqual(result[203 + 100], 6)
        verify_diff(bytes(original), result, edits)

    def test_refuses_to_disable_force_ruler_without_succession(self):
        with self.assertRaisesRegex(AssertionError, 'leadership'):
            patch_people(bytes(203), 0, 1, 203, [{'id': 0, 'status': 0}])


if __name__ == '__main__':
    unittest.main()
