import json
import os
import random
import urllib.error
import urllib.request
from datetime import datetime
from pathlib import Path

ACTION_NONE = 0
ACTION_PLAY_CARD = 1
ACTION_SWITCH_CHAR = 2
ACTION_END_TURN = 3

ELEMENT_NORMAL = 0
ELEMENT_WATER = 1
ELEMENT_FIRE = 2
ELEMENT_GRASS = 3
ELEMENT_ELECTRIC = 4

CARD_TYPE_ATTACK = 0
CARD_TYPE_SKILL = 1
CARD_TYPE_POWER = 2

BUFF_NONE = 0
BUFF_SHIELD = 1
BUFF_WET = 2
BUFF_BURN = 3
BUFF_POISON = 4
BUFF_POWER = 5

TARGET_ENEMY_SINGLE = 0
TARGET_ENEMY_ALL = 1
TARGET_SELF_SINGLE = 2
TARGET_SELF_ALL = 3

TEAM_SIZE = 3

DEFAULT_AI_POLICY = "heuristic"
DEFAULT_LLM_API = "openai"
DEFAULT_LLM_BASE_URL = "http://114.212.227.193:8000"
DEFAULT_LLM_MODEL = "Qwen3.5-4B"
DEFAULT_LLM_TIMEOUT = 5.0
DEFAULT_LLM_MAX_TOKENS = 256
DEFAULT_LLM_TEMPERATURE = 0.0
DEFAULT_LLM_CANDIDATE_LIMIT = 8
DEFAULT_LLM_STRICT_ACTION_ID = True
DEFAULT_DECISION_LOG_ENABLED = True
DEFAULT_DECISION_LOG_DIR = "logs"


def _end_turn(reason: str) -> dict:
    return {
        "action_id": "end",
        "type": ACTION_END_TURN,
        "card_hand_idx": -1,
        "switch_to_idx": -1,
        "target_idx": 0,
        "debug_reason": reason,
    }


def _legacy_decision(state: dict) -> dict:
    energy = state.get("my_energy", 0)
    hand_count = state.get("hand_count", 0)
    if energy > 0 and hand_count > 0:
        return {
            "action_id": "legacy:play:0:0",
            "type": ACTION_PLAY_CARD,
            "card_hand_idx": 0,
            "switch_to_idx": -1,
            "target_idx": 0,
            "debug_reason": "legacy_play_first_card",
        }
    return _end_turn("legacy_no_play")


def _safe_int(value, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _normalize_action(action: dict, fallback_reason: str) -> dict | None:
    if not isinstance(action, dict):
        return None

    action_type = _safe_int(action.get("type"), ACTION_NONE)
    if action_type not in (ACTION_NONE, ACTION_PLAY_CARD, ACTION_SWITCH_CHAR, ACTION_END_TURN):
        return None

    normalized = {
        "type": action_type,
        "card_hand_idx": _safe_int(action.get("card_hand_idx"), -1),
        "switch_to_idx": _safe_int(action.get("switch_to_idx"), -1),
        "target_idx": _safe_int(action.get("target_idx"), 0),
        "debug_reason": str(action.get("debug_reason", fallback_reason)),
    }

    if action_type == ACTION_END_TURN:
        normalized["card_hand_idx"] = -1
        normalized["switch_to_idx"] = -1
        normalized["target_idx"] = 0
    elif action_type == ACTION_SWITCH_CHAR:
        normalized["card_hand_idx"] = -1
        normalized["target_idx"] = 0
    elif action_type == ACTION_PLAY_CARD:
        normalized["switch_to_idx"] = -1

    return normalized


def _buff(character: dict, buff_idx: int) -> int:
    buffs = character.get("buffs", [])
    if 0 <= buff_idx < len(buffs):
        return int(buffs[buff_idx])
    return 0


def _team(player: dict) -> list:
    return player.get("team", [])


def _active_character(player: dict) -> dict:
    team = _team(player)
    if not team:
        return {}
    active_idx = int(player.get("active_idx", 0))
    if 0 <= active_idx < len(team):
        return team[active_idx]
    return team[0]


def _alive(character: dict) -> bool:
    return bool(character.get("is_alive", 0)) and int(character.get("hp", 0)) > 0


def _alive_members(player: dict) -> list:
    return [ch for ch in _team(player) if _alive(ch)]


def _hand(player: dict) -> list:
    return player.get("hand", [])


def _indexed_team(player: dict) -> list:
    return [(i, ch) for i, ch in enumerate(_team(player))]


def _team_index(character: dict, fallback: int = 0) -> int:
    return _safe_int(character.get("index"), fallback)


def _card_hand_idx(card: dict, fallback: int = 0) -> int:
    return _safe_int(card.get("hand_idx"), fallback)


def _make_action_id(action: dict) -> str:
    action_type = _safe_int(action.get("type"), ACTION_NONE)
    if action_type == ACTION_PLAY_CARD:
        return f"play:{_safe_int(action.get('card_hand_idx'), -1)}:{_safe_int(action.get('target_idx'), 0)}"
    if action_type == ACTION_SWITCH_CHAR:
        return f"switch:{_safe_int(action.get('switch_to_idx'), -1)}"
    if action_type == ACTION_END_TURN:
        return "end"
    return "none"


def _make_action(action_type: int, card_hand_idx: int, switch_to_idx: int, target_idx: int, reason: str) -> dict:
    action = {
        "type": action_type,
        "card_hand_idx": card_hand_idx,
        "switch_to_idx": switch_to_idx,
        "target_idx": target_idx,
        "debug_reason": reason,
    }
    action["action_id"] = _make_action_id(action)
    return action


def _find_card_by_hand_idx(player: dict, hand_idx: int) -> dict | None:
    hand = _hand(player)
    for fallback_idx, card in enumerate(hand):
        if _card_hand_idx(card, fallback_idx) == hand_idx:
            return card
    if 0 <= hand_idx < len(hand):
        return hand[hand_idx]
    return None


def _canonical_target_idx_for_card(card: dict, target_idx: int) -> int:
    target_type = _safe_int(card.get("target_type"), TARGET_ENEMY_SINGLE)
    if target_type == TARGET_ENEMY_ALL:
        return -1
    if target_type == TARGET_SELF_ALL:
        return -2
    if target_type == TARGET_SELF_SINGLE and 0 <= target_idx < TEAM_SIZE:
        return 10 + target_idx
    return target_idx


def _action_signature(action: dict) -> tuple:
    return (
        _safe_int(action.get("type"), ACTION_NONE),
        _safe_int(action.get("card_hand_idx"), -1),
        _safe_int(action.get("switch_to_idx"), -1),
        _safe_int(action.get("target_idx"), 0),
    )


def _play_targets_for_card(card: dict, player: dict, opponent: dict) -> list:
    target_type = _safe_int(card.get("target_type"), TARGET_ENEMY_SINGLE)

    if target_type == TARGET_ENEMY_SINGLE:
        return [
            _team_index(ch, fallback_idx)
            for fallback_idx, ch in _indexed_team(opponent)
            if _alive(ch)
        ]

    if target_type == TARGET_ENEMY_ALL:
        return [-1] if _alive_members(opponent) else []

    if target_type == TARGET_SELF_SINGLE:
        # The engine currently normalizes dead self-single targets back to active,
        # so the AI-side legal space keeps self-single targets alive-only.
        return [
            10 + _team_index(ch, fallback_idx)
            for fallback_idx, ch in _indexed_team(player)
            if _alive(ch)
        ]

    if target_type == TARGET_SELF_ALL:
        return [-2]

    return []


def generate_legal_actions(state_dict: dict) -> list:
    players = state_dict.get("players")
    if not isinstance(players, dict):
        return [_end_turn("missing_players")]

    player = players.get("self", {})
    opponent = players.get("opponent", {})
    energy = max(0, _safe_int(player.get("energy"), 0))
    hand_count = max(0, _safe_int(player.get("hand_count"), len(_hand(player))))
    active_idx = _safe_int(player.get("active_idx"), 0)

    legal_actions = []

    for fallback_idx, member in _indexed_team(player):
        switch_idx = _team_index(member, fallback_idx)
        if switch_idx != active_idx and _alive(member):
            legal_actions.append(
                _make_action(
                    ACTION_SWITCH_CHAR,
                    -1,
                    switch_idx,
                    0,
                    f"legal_switch_to_{switch_idx}",
                )
            )

    for fallback_idx, card in enumerate(_hand(player)):
        hand_idx = _card_hand_idx(card, fallback_idx)
        if hand_idx < 0 or hand_idx >= hand_count:
            continue
        if _safe_int(card.get("energy_cost"), 0) > energy:
            continue

        for target_idx in _play_targets_for_card(card, player, opponent):
            legal_actions.append(
                _make_action(
                    ACTION_PLAY_CARD,
                    hand_idx,
                    -1,
                    target_idx,
                    f"legal_play_{card.get('name', 'card')}",
                )
            )

    legal_actions.append(_end_turn("legal_end_turn"))
    return legal_actions


def _canonicalize_action_for_state(action: dict, state_dict: dict) -> dict | None:
    normalized = _normalize_action(action, "candidate_action")
    if normalized is None:
        return None

    players = state_dict.get("players")
    if not isinstance(players, dict):
        normalized["action_id"] = _make_action_id(normalized)
        return normalized

    player = players.get("self", {})
    if normalized["type"] == ACTION_PLAY_CARD:
        card = _find_card_by_hand_idx(player, normalized["card_hand_idx"])
        if card is None:
            return None
        normalized["target_idx"] = _canonical_target_idx_for_card(card, normalized["target_idx"])

    normalized["action_id"] = _make_action_id(normalized)
    return normalized


def select_legal_action(candidate: dict, state_dict: dict, legal_actions: list | None = None) -> dict | None:
    if not isinstance(candidate, dict):
        return None

    legal_actions = legal_actions if legal_actions is not None else generate_legal_actions(state_dict)
    if not legal_actions:
        return None

    requested_id = candidate.get("action_id")
    if requested_id is not None:
        for legal_action in legal_actions:
            if str(legal_action.get("action_id")) == str(requested_id):
                selected = dict(legal_action)
                selected["debug_reason"] = str(candidate.get("debug_reason", selected.get("debug_reason", "legal_action_id")))
                return selected

    canonical = _canonicalize_action_for_state(candidate, state_dict)
    if canonical is None:
        return None

    signature = _action_signature(canonical)
    for legal_action in legal_actions:
        if _action_signature(legal_action) == signature:
            selected = dict(legal_action)
            selected["debug_reason"] = str(candidate.get("debug_reason", selected.get("debug_reason", "legal_action")))
            return selected

    return None


def _hp_ratio(character: dict) -> float:
    max_hp = max(1, int(character.get("max_hp", 1)))
    return int(character.get("hp", 0)) / max_hp


def _effective_element(card: dict, attacker: dict) -> int:
    card_element = int(card.get("element", ELEMENT_NORMAL))
    if card_element == ELEMENT_NORMAL:
        return int(attacker.get("element", ELEMENT_NORMAL))
    return card_element


def _element_multiplier(attack_element: int, target_element: int) -> int:
    if attack_element == ELEMENT_WATER and target_element == ELEMENT_FIRE:
        return 2
    if attack_element == ELEMENT_FIRE and target_element == ELEMENT_GRASS:
        return 2
    if attack_element == ELEMENT_GRASS and target_element == ELEMENT_WATER:
        return 2
    return 1


def _estimate_damage(card: dict, attacker: dict, target: dict) -> int:
    base_damage = int(card.get("base_damage", 0))
    if base_damage <= 0:
        return 0

    attack_element = _effective_element(card, attacker)
    damage = base_damage
    if _buff(attacker, BUFF_POWER) > 0:
        damage += 2

    damage *= _element_multiplier(attack_element, int(target.get("element", ELEMENT_NORMAL)))

    if attack_element == ELEMENT_ELECTRIC and _buff(target, BUFF_WET) > 0:
        damage *= 2

    shield = _buff(target, BUFF_SHIELD)
    return max(0, damage - shield)


def _enemy_single_score(card: dict, attacker: dict, opponent: dict) -> tuple:
    best = None
    for target in _alive_members(opponent):
        damage = _estimate_damage(card, attacker, target)
        score = float(damage)

        if int(card.get("base_damage", 0)) > 0 and damage >= int(target.get("hp", 0)):
            score += 80.0

        if int(target.get("index", -1)) == int(opponent.get("active_idx", 0)):
            score += 6.0

        if int(card.get("buff_effect", BUFF_NONE)) != BUFF_NONE and int(card.get("base_damage", 0)) == 0:
            score += 8.0

        if best is None or score > best[0]:
            best = (score, int(target.get("index", 0)))

    if best is None:
        return 0.0, 0
    return best


def _enemy_all_score(card: dict, attacker: dict, opponent: dict) -> tuple:
    score = 0.0
    for target in _alive_members(opponent):
        damage = _estimate_damage(card, attacker, target)
        score += damage
        if int(card.get("base_damage", 0)) > 0 and damage >= int(target.get("hp", 0)):
            score += 60.0

    if int(card.get("buff_effect", BUFF_NONE)) != BUFF_NONE and int(card.get("base_damage", 0)) == 0:
        score += 5.0 * len(_alive_members(opponent))

    return score, -1


def _self_single_score(card: dict, player: dict) -> tuple:
    active_idx = int(player.get("active_idx", 0))
    candidates = _team(player)
    if not candidates:
        return 0.0, 10

    best = None
    for target in candidates:
        if not _alive(target):
            continue

        target_idx = int(target.get("index", 0))
        score = 0.0

        heal = int(card.get("base_heal", 0))
        if heal > 0:
            missing = max(0, int(target.get("max_hp", 0)) - int(target.get("hp", 0)))
            effective_heal = min(heal, missing)
            score += effective_heal * 1.7

        defense = int(card.get("base_defense", 0))
        if defense > 0 and _alive(target):
            low_hp_bonus = 1.0 + max(0.0, 0.65 - _hp_ratio(target))
            active_bonus = 1.25 if target_idx == active_idx else 0.8
            score += defense * low_hp_bonus * active_bonus

        if int(card.get("buff_effect", BUFF_NONE)) == BUFF_POWER and _alive(target):
            if target_idx == active_idx:
                score += 18.0 if _buff(target, BUFF_POWER) == 0 else 6.0
            else:
                score += 5.0

        if best is None or score > best[0]:
            best = (score, 10 + target_idx)

    if best is None:
        return 0.0, 10 + active_idx
    return best


def _self_all_score(card: dict, player: dict) -> tuple:
    score = 0.0
    for target in _team(player):
        if not _alive(target) and int(card.get("base_heal", 0)) <= 0:
            continue

        heal = int(card.get("base_heal", 0))
        if heal > 0:
            missing = max(0, int(target.get("max_hp", 0)) - int(target.get("hp", 0)))
            score += min(heal, missing) * 1.3
            if not _alive(target):
                score += 45.0

        defense = int(card.get("base_defense", 0))
        if defense > 0 and _alive(target):
            score += defense * (1.0 + max(0.0, 0.5 - _hp_ratio(target)))

        if int(card.get("buff_effect", BUFF_NONE)) == BUFF_POWER and _alive(target):
            score += 8.0 if _buff(target, BUFF_POWER) == 0 else 3.0

    return score, -2


def _score_card(card: dict, player: dict, opponent: dict) -> tuple:
    attacker = _active_character(player)
    target_type = int(card.get("target_type", TARGET_ENEMY_SINGLE))

    if target_type == TARGET_ENEMY_SINGLE:
        return _enemy_single_score(card, attacker, opponent)
    if target_type == TARGET_ENEMY_ALL:
        return _enemy_all_score(card, attacker, opponent)
    if target_type == TARGET_SELF_SINGLE:
        return _self_single_score(card, player)
    if target_type == TARGET_SELF_ALL:
        return _self_all_score(card, player)

    return 0.0, 0


def _enemy_single_action_score(card: dict, attacker: dict, opponent: dict, target_idx: int) -> float:
    target = None
    for member in _alive_members(opponent):
        if _team_index(member) == target_idx:
            target = member
            break
    if target is None:
        return 0.0

    damage = _estimate_damage(card, attacker, target)
    score = float(damage)
    if int(card.get("base_damage", 0)) > 0 and damage >= int(target.get("hp", 0)):
        score += 80.0
        score -= max(0, damage - int(target.get("hp", 0))) * 0.15
    if target_idx == _safe_int(opponent.get("active_idx"), 0):
        score += 6.0
    if _safe_int(card.get("buff_effect"), BUFF_NONE) != BUFF_NONE and _safe_int(card.get("base_damage"), 0) == 0:
        score += 8.0
    return score


def _self_single_action_score(card: dict, player: dict, protocol_target_idx: int) -> float:
    target_idx = protocol_target_idx - 10 if protocol_target_idx >= 10 else protocol_target_idx
    target = None
    for fallback_idx, member in _indexed_team(player):
        if _team_index(member, fallback_idx) == target_idx:
            target = member
            break
    if target is None or not _alive(target):
        return 0.0

    active_idx = _safe_int(player.get("active_idx"), 0)
    score = 0.0
    heal = _safe_int(card.get("base_heal"), 0)
    if heal > 0:
        missing = max(0, _safe_int(target.get("max_hp"), 0) - _safe_int(target.get("hp"), 0))
        score += min(heal, missing) * 1.7

    defense = _safe_int(card.get("base_defense"), 0)
    if defense > 0:
        low_hp_bonus = 1.0 + max(0.0, 0.65 - _hp_ratio(target))
        active_bonus = 1.25 if target_idx == active_idx else 0.8
        score += defense * low_hp_bonus * active_bonus

    if _safe_int(card.get("buff_effect"), BUFF_NONE) == BUFF_POWER:
        if target_idx == active_idx:
            score += 18.0 if _buff(target, BUFF_POWER) == 0 else 6.0
        else:
            score += 5.0

    return score


def _score_play_action(action: dict, state_dict: dict, difficulty: str) -> float:
    players = state_dict.get("players", {})
    player = players.get("self", {}) if isinstance(players, dict) else {}
    opponent = players.get("opponent", {}) if isinstance(players, dict) else {}
    card = _find_card_by_hand_idx(player, _safe_int(action.get("card_hand_idx"), -1))
    if card is None:
        return 0.0

    target_idx = _safe_int(action.get("target_idx"), 0)
    target_type = _safe_int(card.get("target_type"), TARGET_ENEMY_SINGLE)
    attacker = _active_character(player)

    if target_type == TARGET_ENEMY_SINGLE:
        score = _enemy_single_action_score(card, attacker, opponent, target_idx)
    elif target_type == TARGET_ENEMY_ALL:
        score, _ = _enemy_all_score(card, attacker, opponent)
    elif target_type == TARGET_SELF_SINGLE:
        score = _self_single_action_score(card, player, target_idx)
    elif target_type == TARGET_SELF_ALL:
        score, _ = _self_all_score(card, player)
    else:
        score = 0.0

    cost = max(1, _safe_int(card.get("energy_cost"), 1))
    score += 1.0 / cost

    if difficulty == "hard":
        score += _hard_policy_bonus(action, state_dict, base_score=score)

    return score


def _score_switch_action(action: dict, player: dict, difficulty: str) -> float:
    switch_score, switch_idx = _score_switch(player)
    if switch_idx != _safe_int(action.get("switch_to_idx"), -1):
        return 0.0
    if difficulty == "hard":
        switch_score += 2.0
    return switch_score


def _best_followup_card_score(action: dict, state_dict: dict) -> float:
    players = state_dict.get("players", {})
    player = players.get("self", {}) if isinstance(players, dict) else {}
    opponent = players.get("opponent", {}) if isinstance(players, dict) else {}
    played_idx = _safe_int(action.get("card_hand_idx"), -1)
    played_card = _find_card_by_hand_idx(player, played_idx)
    if played_card is None:
        return 0.0

    remaining_energy = _safe_int(player.get("energy"), 0) - _safe_int(played_card.get("energy_cost"), 0)
    if remaining_energy <= 0:
        return 0.0

    best = 0.0
    attacker = _active_character(player)
    for fallback_idx, card in enumerate(_hand(player)):
        hand_idx = _card_hand_idx(card, fallback_idx)
        if hand_idx == played_idx or _safe_int(card.get("energy_cost"), 0) > remaining_energy:
            continue
        score, _ = _score_card(card, player, opponent)
        if _safe_int(card.get("target_type"), TARGET_ENEMY_SINGLE) == TARGET_ENEMY_SINGLE:
            score, _ = _enemy_single_score(card, attacker, opponent)
        best = max(best, float(score))
    return best


def _hard_policy_bonus(action: dict, state_dict: dict, base_score: float) -> float:
    if _safe_int(action.get("type"), ACTION_NONE) != ACTION_PLAY_CARD:
        return 0.0

    players = state_dict.get("players", {})
    player = players.get("self", {}) if isinstance(players, dict) else {}
    card = _find_card_by_hand_idx(player, _safe_int(action.get("card_hand_idx"), -1))
    if card is None:
        return 0.0

    bonus = 0.0
    cost = max(1, _safe_int(card.get("energy_cost"), 1))
    bonus += min(4.0, base_score / cost * 0.05)
    bonus += _best_followup_card_score(action, state_dict) * 0.18

    if _safe_int(card.get("buff_effect"), BUFF_NONE) == BUFF_POWER:
        affordable_attacks = 0
        remaining_energy = _safe_int(player.get("energy"), 0) - _safe_int(card.get("energy_cost"), 0)
        for hand_card in _hand(player):
            if (
                _safe_int(hand_card.get("type"), CARD_TYPE_SKILL) == CARD_TYPE_ATTACK
                and _safe_int(hand_card.get("energy_cost"), 0) <= max(0, remaining_energy)
            ):
                affordable_attacks += 1
        bonus += min(6.0, affordable_attacks * 3.0)

    bonus += _opponent_model_bonus(card, state_dict)
    return bonus


def _opponent_model_bonus(card: dict, state_dict: dict) -> float:
    model = state_dict.get("opponent_model", {})
    if not isinstance(model, dict):
        return 0.0

    style = str(model.get("estimated_style", "balanced"))
    target_type = _safe_int(card.get("target_type"), TARGET_ENEMY_SINGLE)
    is_self_target = target_type in (TARGET_SELF_SINGLE, TARGET_SELF_ALL)
    is_attack = _safe_int(card.get("base_damage"), 0) > 0
    is_defense = _safe_int(card.get("base_defense"), 0) > 0
    is_heal = _safe_int(card.get("base_heal"), 0) > 0
    is_power = _safe_int(card.get("buff_effect"), BUFF_NONE) == BUFF_POWER

    if style == "aggressive" and is_self_target and (is_defense or is_heal):
        return 3.0
    if style in ("defensive", "sustain") and is_attack:
        return 2.0
    if style == "setup" and is_attack:
        return 1.5
    if style == "balanced" and is_power:
        return 0.8
    return 0.0


def _choose_scored_action(state_dict: dict, difficulty: str) -> dict:
    ranked_actions = rank_legal_actions(state_dict, difficulty)
    legal_actions = [item["action"] for item in ranked_actions]
    if not legal_actions:
        return _end_turn(f"{difficulty}_no_legal_action")

    best = ranked_actions[0]
    best_action = best["action"]
    best_score = best["score"]
    if best_score <= 0:
        return select_legal_action(
            {"action_id": "end", "debug_reason": f"{difficulty}_no_profitable_action"},
            state_dict,
            legal_actions,
        ) or _end_turn(f"{difficulty}_no_profitable_action")

    selected = dict(best_action)
    selected["debug_reason"] = f"{difficulty}_score:{best_score:.2f}"
    return selected


def rank_legal_actions(state_dict: dict, difficulty: str = "normal") -> list[dict]:
    legal_actions = generate_legal_actions(state_dict)
    players = state_dict.get("players", {})
    player = players.get("self", {}) if isinstance(players, dict) else {}

    ranked = []
    for action in legal_actions:
        action_type = _safe_int(action.get("type"), ACTION_NONE)
        if action_type == ACTION_PLAY_CARD:
            score = _score_play_action(action, state_dict, difficulty)
        elif action_type == ACTION_SWITCH_CHAR:
            score = _score_switch_action(action, player, difficulty)
        else:
            score = 0.0

        ranked.append({"score": float(score), "action": dict(action)})

    ranked.sort(key=lambda item: item["score"], reverse=True)
    return ranked


def _score_switch(player: dict) -> tuple:
    active = _active_character(player)
    if not active:
        return 0.0, -1

    active_idx = int(player.get("active_idx", 0))
    active_ratio = _hp_ratio(active)
    best_score = 0.0
    best_idx = -1

    for member in _alive_members(player):
        idx = int(member.get("index", 0))
        if idx == active_idx:
            continue

        ratio = _hp_ratio(member)
        score = 0.0
        if not _alive(active):
            score = 100.0 + ratio * 20.0
        elif active_ratio < 0.25 and ratio > active_ratio + 0.25:
            score = 24.0 + (ratio - active_ratio) * 20.0
        elif active_ratio < 0.4 and ratio > 0.75:
            score = 12.0

        if score > best_score:
            best_score = score
            best_idx = idx

    return best_score, best_idx


def process_game_state_heuristic(state_dict: dict) -> dict:
    players = state_dict.get("players")
    if not isinstance(players, dict):
        return _legacy_decision(state_dict)

    return _choose_scored_action(state_dict, "normal")


def process_game_state_hard(state_dict: dict) -> dict:
    players = state_dict.get("players")
    if not isinstance(players, dict):
        return _legacy_decision(state_dict)
    return _choose_scored_action(state_dict, "hard")


def process_game_state_easy(state_dict: dict) -> dict:
    legal_actions = generate_legal_actions(state_dict)
    candidates = [action for action in legal_actions if action.get("type") != ACTION_END_TURN]
    if not candidates:
        return legal_actions[-1] if legal_actions else _end_turn("easy_no_action")

    seed = os.getenv("ROCO_AI_RANDOM_SEED")
    rng = random.Random(int(seed)) if seed and seed.isdigit() else random
    action = dict(rng.choice(candidates))
    action["debug_reason"] = "random_legal_action"
    return action


def _extract_json_object(text: str) -> dict:
    text = text.strip()
    if text.startswith("```"):
        lines = text.splitlines()
        if lines and lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].startswith("```"):
            lines = lines[:-1]
        text = "\n".join(lines).strip()

    try:
        return json.loads(text)
    except json.JSONDecodeError:
        start = text.find("{")
        end = text.rfind("}")
        if start >= 0 and end > start:
            return json.loads(text[start:end + 1])
        raise


def _join_url(base_url: str, path: str) -> str:
    return f"{base_url.rstrip('/')}/{path.lstrip('/')}"


def _read_float_env(name: str, default: float) -> float:
    try:
        return float(os.getenv(name, str(default)))
    except ValueError:
        return default


def _read_int_env(name: str, default: int) -> int:
    try:
        return int(os.getenv(name, str(default)))
    except ValueError:
        return default


def _read_bool_env(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in ("1", "true", "yes", "on")


def _decision_log_enabled() -> bool:
    return _read_bool_env("ROCO_AI_DECISION_LOG", DEFAULT_DECISION_LOG_ENABLED)


def _decision_log_path() -> Path:
    configured_path = os.getenv("ROCO_AI_DECISION_LOG_PATH")
    if configured_path:
        return Path(configured_path)

    log_dir = Path(os.getenv("ROCO_AI_DECISION_LOG_DIR", DEFAULT_DECISION_LOG_DIR))
    date_tag = datetime.now().strftime("%Y%m%d")
    return log_dir / f"ai_decisions_{date_tag}.jsonl"


def _post_json(url: str, payload: dict, timeout: float) -> dict:
    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def _query_openai_compatible(prompt: str) -> dict:
    base_url = os.getenv("ROCO_LLM_BASE_URL", DEFAULT_LLM_BASE_URL)
    url = os.getenv("ROCO_LLM_URL", _join_url(base_url, "/v1/chat/completions"))
    model = os.getenv("ROCO_LLM_MODEL", DEFAULT_LLM_MODEL)
    timeout = _read_float_env("ROCO_LLM_TIMEOUT", DEFAULT_LLM_TIMEOUT)
    max_tokens = _read_int_env("ROCO_LLM_MAX_TOKENS", DEFAULT_LLM_MAX_TOKENS)
    temperature = _read_float_env("ROCO_LLM_TEMPERATURE", DEFAULT_LLM_TEMPERATURE)

    payload = {
        "model": model,
        "messages": [
            {
                "role": "system",
                "content": "你是一个只输出合法 JSON 动作的卡牌游戏 AI。不要输出解释或 Markdown。",
            },
            {"role": "user", "content": prompt},
        ],
        "temperature": temperature,
        "max_tokens": max_tokens,
    }

    result = _post_json(url, payload, timeout)
    choices = result.get("choices", [])
    if not choices:
        raise ValueError("openai_compatible_response_missing_choices")

    choice = choices[0]
    message = choice.get("message", {})
    content = message.get("content", choice.get("text", ""))
    if isinstance(content, list):
        content = "".join(str(part.get("text", part)) if isinstance(part, dict) else str(part) for part in content)

    return _extract_json_object(str(content))


def _query_ollama_compatible(prompt: str) -> dict:
    base_url = os.getenv("ROCO_LLM_BASE_URL", "http://127.0.0.1:11434")
    url = os.getenv("ROCO_LLM_URL", _join_url(base_url, "/api/generate"))
    model = os.getenv("ROCO_LLM_MODEL", "qwen")
    timeout = _read_float_env("ROCO_LLM_TIMEOUT", DEFAULT_LLM_TIMEOUT)
    data = {
        "model": model,
        "prompt": prompt,
        "stream": False,
        "format": "json",
    }

    result = _post_json(url, data, timeout)
    llm_response = result.get("response", result)
    if isinstance(llm_response, dict):
        return llm_response
    return _extract_json_object(str(llm_response))


def query_llm(prompt: str) -> dict:
    api_type = os.getenv("ROCO_LLM_API", DEFAULT_LLM_API).strip().lower()
    if api_type in ("openai", "vllm", "qwen", "chat"):
        return _query_openai_compatible(prompt)
    if api_type in ("ollama", "generate"):
        return _query_ollama_compatible(prompt)
    raise ValueError(f"unsupported_llm_api:{api_type}")


def _describe_action_for_prompt(action: dict, state_dict: dict) -> str:
    players = state_dict.get("players", {})
    player = players.get("self", {}) if isinstance(players, dict) else {}

    action_type = _safe_int(action.get("type"), ACTION_NONE)
    if action_type == ACTION_END_TURN:
        return "End current turn"
    if action_type == ACTION_SWITCH_CHAR:
        return f"Switch active character to team index {action.get('switch_to_idx')}"
    if action_type == ACTION_PLAY_CARD:
        card = _find_card_by_hand_idx(player, _safe_int(action.get("card_hand_idx"), -1)) or {}
        return (
            f"Play hand[{action.get('card_hand_idx')}] "
            f"{card.get('name', 'card')} to target {action.get('target_idx')}"
        )
    return "Unknown action"


def _legal_actions_for_prompt(legal_actions: list, state_dict: dict) -> list:
    return [
        {
            "action_id": action.get("action_id"),
            "type": action.get("type"),
            "card_hand_idx": action.get("card_hand_idx"),
            "switch_to_idx": action.get("switch_to_idx"),
            "target_idx": action.get("target_idx"),
            "summary": _describe_action_for_prompt(action, state_dict),
        }
        for action in legal_actions
    ]


def _character_summary(character: dict) -> dict:
    return {
        "idx": character.get("index"),
        "name": character.get("name"),
        "hp": character.get("hp"),
        "max_hp": character.get("max_hp"),
        "element": character.get("element"),
        "alive": character.get("is_alive"),
        "buffs": character.get("buffs", []),
    }


def _player_summary(player: dict) -> dict:
    active = _active_character(player)
    return {
        "player_id": player.get("player_id"),
        "active_idx": player.get("active_idx"),
        "energy": player.get("energy"),
        "hand_count": player.get("hand_count"),
        "active": _character_summary(active),
        "team": [_character_summary(member) for member in _team(player)],
        "hand": [
            {
                "hand_idx": card.get("hand_idx"),
                "name": card.get("name"),
                "cost": card.get("energy_cost"),
                "damage": card.get("base_damage"),
                "defense": card.get("base_defense"),
                "heal": card.get("base_heal"),
                "buff": card.get("buff_effect"),
                "target_type": card.get("target_type"),
            }
            for card in _hand(player)
        ],
    }


def _state_summary_for_prompt(state_dict: dict) -> dict:
    players = state_dict.get("players", {})
    player = players.get("self", {}) if isinstance(players, dict) else {}
    opponent = players.get("opponent", {}) if isinstance(players, dict) else {}
    return {
        "round_count": state_dict.get("round_count"),
        "current_turn": state_dict.get("current_turn"),
        "self": _player_summary(player),
        "opponent": _player_summary(opponent),
    }


def _limited_legal_actions_for_llm(state_dict: dict, difficulty: str = "hard") -> list:
    limit = max(1, _read_int_env("ROCO_LLM_CANDIDATE_LIMIT", DEFAULT_LLM_CANDIDATE_LIMIT))
    ranked = rank_legal_actions(state_dict, difficulty)
    top_actions = [item["action"] for item in ranked[:limit]]
    if not any(action.get("action_id") == "end" for action in top_actions):
        end_action = select_legal_action({"action_id": "end"}, state_dict, [item["action"] for item in ranked])
        if end_action is not None:
            top_actions.append(end_action)
    return top_actions


def _build_llm_prompt(state_dict: dict, heuristic_action: dict, legal_actions: list) -> str:
    compact_summary = json.dumps(_state_summary_for_prompt(state_dict), ensure_ascii=False, separators=(",", ":"))
    compact_fallback = json.dumps(heuristic_action, ensure_ascii=False, separators=(",", ":"))
    compact_legal_actions = json.dumps(
        _legal_actions_for_prompt(legal_actions, state_dict),
        ensure_ascii=False,
        separators=(",", ":"),
    )
    return f"""
你是《Spire of Roco》的 AI 玩家，请根据当前 GameState 选择一个合法动作。

行动枚举:
- ACTION_NONE = 0
- ACTION_PLAY_CARD = 1
- ACTION_SWITCH_CHAR = 2
- ACTION_END_TURN = 3

目标索引协议:
- 敌方单体: 0~2
- 己方单体: 10~12
- 敌方全体: -1
- 己方全体: -2

只允许从合法动作列表中选择动作。你只能输出:
{{"action_id": str, "debug_reason": str}}

如果无法判断，请返回这个启发式兜底动作:
{compact_fallback}

合法动作列表:
{compact_legal_actions}

当前局势摘要:
{compact_summary}
""".strip()


def process_game_state_llm(state_dict: dict) -> dict:
    legal_actions = _limited_legal_actions_for_llm(state_dict, "hard")
    heuristic_action = process_game_state_hard(state_dict)
    prompt = _build_llm_prompt(state_dict, heuristic_action, legal_actions)

    try:
        llm_action = query_llm(prompt)
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, ValueError, OSError) as exc:
        heuristic_action["debug_reason"] = f"llm_fallback:{type(exc).__name__}"
        return heuristic_action

    if _read_bool_env("ROCO_LLM_STRICT_ACTION_ID", DEFAULT_LLM_STRICT_ACTION_ID) and "action_id" not in llm_action:
        heuristic_action["debug_reason"] = "llm_fallback:missing_action_id"
        return heuristic_action

    selected = select_legal_action(llm_action, state_dict, legal_actions)
    if selected is None:
        heuristic_action["debug_reason"] = "llm_fallback:invalid_action"
        return heuristic_action

    selected["debug_reason"] = str(llm_action.get("debug_reason", selected.get("debug_reason", "llm_action")))
    return selected


def process_game_state_hybrid(state_dict: dict) -> dict:
    candidate_actions = _limited_legal_actions_for_llm(state_dict, "hard")
    fallback_action = process_game_state_hard(state_dict)
    prompt = _build_llm_prompt(state_dict, fallback_action, candidate_actions)

    try:
        llm_action = query_llm(prompt)
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, ValueError, OSError) as exc:
        fallback_action["debug_reason"] = f"hybrid_fallback:{type(exc).__name__}"
        return fallback_action

    if "action_id" not in llm_action:
        fallback_action["debug_reason"] = "hybrid_fallback:missing_action_id"
        return fallback_action

    selected = select_legal_action(llm_action, state_dict, candidate_actions)
    if selected is None:
        fallback_action["debug_reason"] = "hybrid_fallback:invalid_action"
        return fallback_action

    selected["debug_reason"] = str(llm_action.get("debug_reason", selected.get("debug_reason", "hybrid_action")))
    return selected


def explain_action(state_dict: dict, action: dict, policy: str | None = None, candidate_limit: int = 5) -> dict:
    ranked = rank_legal_actions(state_dict, "hard")[: max(1, candidate_limit)]
    return {
        "ts": datetime.now().isoformat(timespec="seconds"),
        "policy": policy or os.getenv("ROCO_AI_POLICY", DEFAULT_AI_POLICY),
        "round_count": state_dict.get("round_count"),
        "current_turn": state_dict.get("current_turn"),
        "ai_player_id": state_dict.get("ai_player_id"),
        "chosen_action": action,
        "state_summary": _state_summary_for_prompt(state_dict),
        "top_candidates": [
            {
                "score": round(item["score"], 3),
                "action": item["action"],
                "summary": _describe_action_for_prompt(item["action"], state_dict),
            }
            for item in ranked
        ],
    }


def append_decision_log(state_dict: dict, action: dict, policy: str | None = None) -> Path | None:
    if not _decision_log_enabled():
        return None

    path = _decision_log_path()
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        record = explain_action(state_dict, action, policy=policy)
        with path.open("a", encoding="utf-8") as fp:
            fp.write(json.dumps(record, ensure_ascii=False, separators=(",", ":")))
            fp.write("\n")
        return path
    except OSError:
        return None


def process_game_state(state_dict: dict, policy: str | None = None) -> dict:
    request_policy = state_dict.get("ai_policy") if isinstance(state_dict, dict) else None
    selected_policy = (policy or request_policy or os.getenv("ROCO_AI_POLICY", DEFAULT_AI_POLICY)).strip().lower()

    if selected_policy in ("random", "easy", "casual"):
        return process_game_state_easy(state_dict)

    if selected_policy in ("hard", "expert"):
        return process_game_state_hard(state_dict)

    if selected_policy in ("hybrid", "llm_hybrid"):
        return process_game_state_hybrid(state_dict)

    if selected_policy in ("llm", "ollama", "model"):
        return process_game_state_llm(state_dict)

    if selected_policy not in ("heuristic", "normal", "rule", "rules"):
        fallback = process_game_state_heuristic(state_dict)
        fallback["debug_reason"] = f"unknown_policy:{selected_policy}"
        return fallback

    return process_game_state_heuristic(state_dict)
