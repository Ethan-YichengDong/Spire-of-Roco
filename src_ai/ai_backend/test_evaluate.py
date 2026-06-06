import unittest

import evaluate


class EvaluateTests(unittest.TestCase):
    def test_simulate_match_returns_metrics(self):
        result = evaluate.simulate_match(
            p1_policy="random",
            p2_policy="heuristic",
            seed=11,
            max_rounds=5,
        )

        self.assertIn(result["winner"], (0, 1, 2))
        self.assertGreaterEqual(result["rounds"], 1)
        self.assertGreaterEqual(result["actions"], 0)
        self.assertEqual(result["illegal_actions"], 0)

    def test_evaluate_aggregates_matches(self):
        summary = evaluate.evaluate(
            p1_policy="random",
            p2_policy="heuristic",
            matches=3,
            seed=21,
            max_rounds=5,
        )

        self.assertEqual(summary["matches"], 3)
        self.assertEqual(sum(summary["wins"].values()), 3)
        self.assertGreaterEqual(summary["avg_rounds"], 1.0)
        self.assertEqual(summary["illegal_actions"], 0)
        self.assertIn("card_usage", summary)


if __name__ == "__main__":
    unittest.main()
