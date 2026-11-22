import unittest
from gestures import Stabilizer, angle, count_fingers


class GestureTests(unittest.TestCase):
    def test_geometry_is_rotation_independent(self):
        self.assertAlmostEqual(angle((0, 0, 0), (1, 0, 0), (2, 0, 0)), 180)
        self.assertAlmostEqual(angle((0, 0, 0), (0, 1, 0), (0, 2, 0)), 180)
        self.assertEqual(angle((0, 0, 0), (0, 0, 0), (0, 0, 0)), 0)

    def test_reject_invalid_hand(self):
        with self.assertRaises(ValueError):
            count_fingers([])
        self.assertEqual(count_fingers([(0, 0, 0)] * 21), 0)

    def test_jitter_release_and_retrigger(self):
        stable = Stabilizer(0.4)
        self.assertEqual(stable.update(1, 0), (None, None))
        self.assertEqual(stable.update(2, 0.2), (None, None))
        self.assertEqual(stable.update(1, 0.3), (None, None))
        self.assertEqual(stable.update(1, 0.8), (1, 1))
        self.assertEqual(stable.update(1, 1), (1, None))
        self.assertEqual(stable.update(None, 1.1), (1, None))
        self.assertEqual(stable.update(None, 1.6), (None, None))
        self.assertEqual(stable.update(1, 1.7), (None, None))
        self.assertEqual(stable.update(1, 2.2), (1, 1))


if __name__ == "__main__":
    unittest.main()
