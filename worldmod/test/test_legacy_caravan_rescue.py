import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
from rescue_legacy_caravans import transfers


def row(unit=1, **values):
    result = dict(unit=unit, origin=0, destination=1, flags=3, type=1, force=1, city_owner=1,
                  food=2000, troops=500, city_food=11000)
    result.update(values)
    return result


def paths(*units, steps=1):
    return {str(u): dict(out=dict(steps=steps), home=dict(steps=steps)) for u in units}


class RescueTests(unittest.TestCase):
    def test_conservation_and_shared_city_stock(self):
        changes, skipped = transfers([row(1), row(2), row(3)], paths(1, 2, 3))
        self.assertEqual([r['unit'] for r in changes], [1, 2])
        self.assertEqual(changes[-1]['city_after'], 9000)
        self.assertEqual(skipped[0]['unit'], 3)
        self.assertEqual(sum(r['after']-r['before'] for r in changes), 2000)

    def test_not_enough_never_partially_deducts(self):
        changes, skipped = transfers([row(city_food=9500)], paths(1))
        self.assertEqual(changes, [])
        self.assertEqual(skipped[0]['required'], 1000)

    def test_no_enemy_or_ai_food(self):
        changes, skipped = transfers([row(city_owner=2), row(2, flags=1)], paths(1, 2))
        self.assertEqual(changes, [])
        self.assertEqual(len(skipped), 1)
        self.assertIn('owner', skipped[0]['reason'])

    def test_no_unverified_route(self):
        self.assertEqual(transfers([row()], {})[0], [])

    def test_native_capacity(self):
        changes, skipped = transfers([row(troops=60000)], paths(1, steps=1000))
        self.assertEqual(changes, [])
        self.assertIn('capacity', skipped[0]['reason'])

    def test_idempotence(self):
        first, _ = transfers([row()], paths(1))
        repaired = row(food=first[0]['after'], city_food=first[0]['city_after'])
        self.assertEqual(transfers([repaired], paths(1))[0], [])

    def test_return_does_not_budget_an_extra_round_trip(self):
        changes, _ = transfers([row(destination=0, city_food=50000)], paths(1, steps=500))
        self.assertEqual(changes[0]['after'], 6450)

    def test_low_food_first(self):
        changes, _ = transfers([row(1, food=2000, city_food=12000), row(2, food=0, city_food=12000)], paths(1, 2))
        self.assertEqual([r['unit'] for r in changes], [2])


if __name__ == '__main__':
    unittest.main()
