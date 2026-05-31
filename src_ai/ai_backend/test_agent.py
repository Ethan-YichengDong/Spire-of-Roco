import unittest

import agent


def _character(index, name, element, hp, max_hp, is_alive=1):
    return {
        "index": index,
        "char_id": index + 1,
        "name": name,
        "element": element,
        "hp": hp,
        "max_hp": max_hp,
        "speed": 50,
        "is_alive": is_alive,
        "buffs": [0, 0, 0, 0, 0, 0],
    }


def _card(hand_idx, name, cost, target_type, base_damage=0, base_heal=0, base_defense=0):
    return {
        "hand_idx": hand_idx,
        "card_id": hand_idx + 1,
        "name": name,
        "element": agent.ELEMENT_NORMAL,
        "type": agent.CARD_TYPE_ATTACK if base_damage > 0 else agent.CARD_TYPE_SKILL,
        "energy_cost": cost,
        "base_damage": base_damage,
        "base_defense": base_defense,
        "base_heal": base_heal,
        "buff_effect": agent.BUFF_NONE,
        "buff_value": 0,
        "buff_duration": 0,
        "target_type": target_type,
    }


def make_state():
    self_player = {
        "player_id": 2,
        "name": "AI",
        "active_idx": 0,
        "energy": 3,
        "max_energy": 3,
        "hand_count": 5,
        "draw_count": 0,
        "discard_count": 0,
        "team": [
            _character(0, "Squirtle", agent.ELEMENT_WATER, 60, 100, 1),
            _character(1, "Charmander", agent.ELEMENT_FIRE, 80, 90, 1),
            _character(2, "Bulbasaur", agent.ELEMENT_GRASS, 0, 110, 0),
        ],
        "hand": [
            _card(0, "SingleAtk", 1, agent.TARGET_ENEMY_SINGLE, base_damage=10),
            _card(1, "AllAtk", 3, agent.TARGET_ENEMY_ALL, base_damage=4),
            _card(2, "SingleHeal", 2, agent.TARGET_SELF_SINGLE, base_heal=20),
            _card(3, "AllHeal", 3, agent.TARGET_SELF_ALL, base_heal=15),
            _card(4, "ExpensiveAtk", 4, agent.TARGET_ENEMY_SINGLE, base_damage=100),
        ],
    }
    opponent = {
        "player_id": 1,
        "name": "Human",
        "active_idx": 0,
        "energy": 3,
        "max_energy": 3,
        "hand_count": 0,
        "draw_count": 0,
        "discard_count": 0,
        "team": [
            _character(0, "EnemyFire", agent.ELEMENT_FIRE, 100, 100, 1),
            _character(1, "EnemyDead", agent.ELEMENT_GRASS, 0, 90, 0),
            _character(2, "EnemyLow", agent.ELEMENT_GRASS, 5, 90, 1),
        ],
        "hand": [],
    }
    return {
        "schema_version": 1,
        "ai_player_id": 2,
        "round_count": 1,
        "current_turn": 2,
        "players": {
            "self": self_player,
            "opponent": opponent,
        },
    }


def make_attack_state():
    state = make_state()
    for member in state["players"]["self"]["team"]:
        member["hp"] = member["max_hp"]
        member["is_alive"] = 1
    return state


class AgentLegalActionTests(unittest.TestCase):
    def test_generate_legal_actions_filters_energy_and_dead_targets(self):
        actions = agent.generate_legal_actions(make_state())
        action_ids = {action["action_id"] for action in actions}

        self.assertEqual(
            action_ids,
            {
                "switch:1",
                "play:0:0",
                "play:0:2",
                "play:1:-1",
                "play:2:10",
                "play:2:11",
                "play:3:-2",
                "end",
            },
        )

    def test_select_legal_action_accepts_old_self_single_target_protocol(self):
        selected = agent.select_legal_action(
            {
                "type": agent.ACTION_PLAY_CARD,
                "card_hand_idx": 2,
                "switch_to_idx": -1,
                "target_idx": 1,
                "debug_reason": "old_protocol",
            },
            make_state(),
        )

        self.assertIsNotNone(selected)
        self.assertEqual(selected["action_id"], "play:2:11")
        self.assertEqual(selected["target_idx"], 11)

    def test_heuristic_returns_legal_kill_action(self):
        action = agent.process_game_state_heuristic(make_attack_state())

        self.assertEqual(action["action_id"], "play:0:2")
        self.assertEqual(action["type"], agent.ACTION_PLAY_CARD)
        self.assertEqual(action["target_idx"], 2)

    def test_difficulty_policies_return_legal_actions(self):
        state = make_state()
        for policy in ("easy", "normal", "hard"):
            with self.subTest(policy=policy):
                action = agent.process_game_state(state, policy=policy)
                selected = agent.select_legal_action(action, state)
                self.assertIsNotNone(selected)
                self.assertEqual(action["action_id"], selected["action_id"])

    def test_hard_policy_keeps_kill_priority(self):
        action = agent.process_game_state(make_attack_state(), policy="hard")

        self.assertEqual(action["action_id"], "play:0:2")
        self.assertTrue(action["debug_reason"].startswith("hard_score:"))

    def test_llm_policy_can_select_by_action_id(self):
        original_query_llm = agent.query_llm
        agent.query_llm = lambda prompt: {"action_id": "play:1:-1", "debug_reason": "test_llm_choice"}

        try:
            action = agent.process_game_state_llm(make_state())
        finally:
            agent.query_llm = original_query_llm

        self.assertEqual(action["action_id"], "play:1:-1")
        self.assertEqual(action["target_idx"], -1)
        self.assertEqual(action["debug_reason"], "test_llm_choice")

    def test_llm_policy_falls_back_when_action_id_is_illegal(self):
        original_query_llm = agent.query_llm
        agent.query_llm = lambda prompt: {"action_id": "play:4:0", "debug_reason": "illegal_expensive_card"}

        try:
            action = agent.process_game_state_llm(make_attack_state())
        finally:
            agent.query_llm = original_query_llm

        self.assertEqual(action["action_id"], "play:0:2")
        self.assertEqual(action["debug_reason"], "llm_fallback:invalid_action")


if __name__ == "__main__":
    unittest.main()
