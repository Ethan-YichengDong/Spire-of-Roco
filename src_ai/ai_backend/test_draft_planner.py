import unittest

import draft_planner
import evaluate


class DraftPlannerTests(unittest.TestCase):
    def test_choose_team_returns_unique_team(self):
        characters = evaluate.load_characters()
        team = draft_planner.choose_team(characters, style="balanced")

        self.assertEqual(len(team), 3)
        self.assertEqual(len({member["char_id"] for member in team}), 3)
        self.assertEqual([member["index"] for member in team], [0, 1, 2])

    def test_build_deck_respects_size_and_core_buckets(self):
        cards = evaluate.load_cards()
        characters = evaluate.load_characters()
        team = draft_planner.choose_team(characters, style="balanced")
        deck = draft_planner.build_deck(cards, team, style="balanced", deck_size=16)
        buckets = {draft_planner.card_bucket(card) for card in deck}

        self.assertEqual(len(deck), 16)
        self.assertIn("attack", buckets)
        self.assertIn("defense", buckets)
        self.assertIn("heal", buckets)
        self.assertIn("power", buckets)

    def test_recommend_draft_returns_ids_for_engine_integration(self):
        draft = draft_planner.recommend_draft(style="aggressive", deck_size=16)

        self.assertEqual(draft["style"], "aggressive")
        self.assertEqual(len(draft["team_ids"]), 3)
        self.assertEqual(len(draft["deck_ids"]), 16)


if __name__ == "__main__":
    unittest.main()
