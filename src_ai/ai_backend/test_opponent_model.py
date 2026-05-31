import unittest

import opponent_model


def make_state(active_idx=0):
    return {
        "players": {
            "opponent": {
                "active_idx": active_idx,
                "team": [
                    {"index": 0, "name": "A", "hp": 30, "max_hp": 100, "is_alive": 1},
                    {"index": 1, "name": "B", "hp": 80, "max_hp": 100, "is_alive": 1},
                    {"index": 2, "name": "C", "hp": 0, "max_hp": 100, "is_alive": 0},
                ],
                "hand": [
                    {"name": "Atk1", "base_damage": 10, "base_heal": 0, "base_defense": 0, "buff_effect": 0},
                    {"name": "Atk2", "base_damage": 4, "base_heal": 0, "base_defense": 0, "buff_effect": 0},
                    {"name": "Power", "base_damage": 0, "base_heal": 0, "base_defense": 0, "buff_effect": 5},
                ],
            }
        }
    }


class OpponentModelTests(unittest.TestCase):
    def test_update_estimates_style_and_lowest_target(self):
        model = opponent_model.OpponentModel()
        profile = model.update(make_state())

        self.assertEqual(profile["observations"], 1)
        self.assertEqual(profile["estimated_style"], "aggressive")
        self.assertEqual(profile["lowest_hp_target"]["index"], 0)
        self.assertEqual(profile["hand_profile"]["attack"], 2)

    def test_update_counts_switches(self):
        model = opponent_model.OpponentModel()
        model.update(make_state(active_idx=0))
        profile = model.update(make_state(active_idx=1))

        self.assertEqual(profile["observations"], 2)
        self.assertEqual(profile["switches"], 1)


if __name__ == "__main__":
    unittest.main()
