import argparse
import csv
import json
import random
from copy import deepcopy
from pathlib import Path

import agent

MAX_HAND_SIZE = 10
MAX_DECK_SIZE = 30
DEFAULT_DECK_SIZE = 16
STARTING_ENERGY = 3
MAX_ACTIONS_PER_TURN = 20


REPO_ROOT = Path(__file__).resolve().parents[2]
CARDS_PATH = REPO_ROOT / "src_data" / "cards.txt"
CHARACTERS_PATH = REPO_ROOT / "src_data" / "characters.txt"


def load_cards(path: Path = CARDS_PATH) -> list[dict]:
    cards = []
    with path.open(newline="") as fp:
        for row in csv.DictReader(fp):
            cards.append(
                {
                    "card_id": int(row["id"]),
                    "name": row["name"],
                    "element": int(row["element"]),
                    "type": int(row["type"]),
                    "energy_cost": int(row["energy"]),
                    "base_damage": int(row["damage"]),
                    "base_defense": int(row["defense"]),
                    "base_heal": int(row["heal"]),
                    "buff_effect": int(row["buff_effect"]),
                    "buff_value": int(row["buff_value"]),
                    "buff_duration": int(row["buff_duration"]),
                    "target_type": int(row["target_type"]),
                }
            )
    return cards


def load_characters(path: Path = CHARACTERS_PATH) -> list[dict]:
    characters = []
    with path.open(newline="") as fp:
        for row in csv.DictReader(fp):
            max_hp = int(row["hp"])
            characters.append(
                {
                    "char_id": int(row["id"]),
                    "name": row["name"],
                    "element": int(row["element"]),
                    "hp": max_hp,
                    "max_hp": max_hp,
                    "speed": int(row["speed"]),
                    "is_alive": 1,
                    "buffs": [0 for _ in range(agent.BUFF_POWER + 1)],
                }
            )
    return characters


def clone_card(card: dict) -> dict:
    cloned = dict(card)
    cloned.pop("hand_idx", None)
    return cloned


def clone_character(character: dict, index: int) -> dict:
    cloned = dict(character)
    cloned["index"] = index
    cloned["buffs"] = list(character.get("buffs", []))
    return cloned


def build_balanced_deck(cards: list[dict], deck_size: int = DEFAULT_DECK_SIZE) -> list[dict]:
    deck = []
    while len(deck) < deck_size:
        for card in cards:
            if len(deck) >= deck_size:
                break
            deck.append(clone_card(card))
    return deck[: min(deck_size, MAX_DECK_SIZE)]


def make_player(player_id: int, cards: list[dict], characters: list[dict], rng: random.Random) -> dict:
    team = [clone_character(characters[i % len(characters)], i) for i in range(agent.TEAM_SIZE)]
    draw_pile = build_balanced_deck(cards)
    rng.shuffle(draw_pile)

    player = {
        "player_id": player_id,
        "name": f"Player{player_id}",
        "active_idx": 0,
        "energy": STARTING_ENERGY,
        "max_energy": STARTING_ENERGY,
        "team": team,
        "hand": [],
        "draw_pile": draw_pile,
        "discard_pile": [],
    }
    draw_cards(player, MAX_HAND_SIZE, rng)
    return player


def alive(character: dict) -> bool:
    return bool(character.get("is_alive", 0)) and int(character.get("hp", 0)) > 0


def alive_members(player: dict) -> list[dict]:
    return [member for member in player["team"] if alive(member)]


def total_alive_hp(player: dict) -> int:
    return sum(int(member.get("hp", 0)) for member in player["team"] if alive(member))


def active_character(player: dict) -> dict:
    active_idx = int(player.get("active_idx", 0))
    if 0 <= active_idx < len(player["team"]):
        return player["team"][active_idx]
    return player["team"][0]


def sync_alive(character: dict) -> None:
    if int(character.get("hp", 0)) <= 0:
        character["hp"] = 0
        character["is_alive"] = 0
        character["buffs"] = [0 for _ in range(agent.BUFF_POWER + 1)]
    else:
        character["is_alive"] = 1


def auto_switch_if_dead(player: dict) -> None:
    if alive(active_character(player)):
        return
    for member in player["team"]:
        if alive(member):
            player["active_idx"] = int(member["index"])
            return


def draw_cards(player: dict, amount: int, rng: random.Random) -> None:
    for _ in range(amount):
        if len(player["hand"]) >= MAX_HAND_SIZE:
            return
        if not player["draw_pile"]:
            if not player["discard_pile"]:
                return
            player["draw_pile"] = player["discard_pile"]
            player["discard_pile"] = []
            rng.shuffle(player["draw_pile"])
        player["hand"].append(player["draw_pile"].pop())


def refill_hand(player: dict, rng: random.Random) -> None:
    draw_cards(player, MAX_HAND_SIZE - len(player["hand"]), rng)


def decrement_buffs(player: dict) -> None:
    for member in player["team"]:
        buffs = member.get("buffs", [])
        for idx, value in enumerate(buffs):
            if idx == agent.BUFF_SHIELD:
                continue
            if value > 0:
                buffs[idx] -= 1


def serialize_card(card: dict, hand_idx: int) -> dict:
    serialized = dict(card)
    serialized["hand_idx"] = hand_idx
    return serialized


def serialize_player(player: dict) -> dict:
    return {
        "player_id": player["player_id"],
        "name": player["name"],
        "active_idx": player["active_idx"],
        "energy": player["energy"],
        "max_energy": player["max_energy"],
        "hand_count": len(player["hand"]),
        "draw_count": len(player["draw_pile"]),
        "discard_count": len(player["discard_pile"]),
        "team": [deepcopy(member) for member in player["team"]],
        "hand": [serialize_card(card, idx) for idx, card in enumerate(player["hand"])],
    }


def make_state(self_player: dict, opponent: dict, round_count: int, current_turn: int) -> dict:
    return {
        "schema_version": 1,
        "turn_model": "sequential",
        "ai_player_id": self_player["player_id"],
        "round_count": round_count,
        "current_turn": current_turn,
        "game_stage": 2,
        "current_scene": 2,
        "target_protocol": {
            "enemy_single": "0-2",
            "self_single": "10-12",
            "enemy_all": -1,
            "self_all": -2,
        },
        "players": {
            "self": serialize_player(self_player),
            "opponent": serialize_player(opponent),
        },
    }


def effective_element(card: dict, attacker: dict) -> int:
    card_element = int(card.get("element", agent.ELEMENT_NORMAL))
    if card_element == agent.ELEMENT_NORMAL:
        return int(attacker.get("element", agent.ELEMENT_NORMAL))
    return card_element


def default_buff_for_element(element: int) -> int:
    if element == agent.ELEMENT_WATER:
        return agent.BUFF_WET
    if element == agent.ELEMENT_FIRE:
        return agent.BUFF_BURN
    if element == agent.ELEMENT_GRASS:
        return agent.BUFF_POISON
    return agent.BUFF_NONE


def apply_mitigation(raw_damage: int, attack_element: int, target: dict) -> int:
    damage = raw_damage
    if attack_element == agent.ELEMENT_WATER and target["element"] == agent.ELEMENT_FIRE:
        damage *= 2
    elif attack_element == agent.ELEMENT_FIRE and target["element"] == agent.ELEMENT_GRASS:
        damage *= 2
    elif attack_element == agent.ELEMENT_GRASS and target["element"] == agent.ELEMENT_WATER:
        damage *= 2

    if attack_element == agent.ELEMENT_ELECTRIC and target["buffs"][agent.BUFF_WET] > 0:
        damage *= 2

    shield = target["buffs"][agent.BUFF_SHIELD]
    if shield > 0:
        absorbed = min(shield, damage)
        target["buffs"][agent.BUFF_SHIELD] -= absorbed
        damage -= absorbed
    return max(0, damage)


def resolve_on_target(card: dict, attacker: dict, target: dict) -> tuple[int, int]:
    damage_done = 0
    healing_done = 0

    if alive(target) and card["base_damage"] > 0:
        raw = card["base_damage"]
        if attacker["buffs"][agent.BUFF_POWER] > 0:
            raw += 2
        damage_done = apply_mitigation(raw, card["element"], target)
        target["hp"] -= damage_done
        sync_alive(target)

    if alive(target) and card["base_defense"] > 0:
        target["buffs"][agent.BUFF_SHIELD] += card["base_defense"]

    if card["base_heal"] > 0:
        before = target["hp"]
        target["hp"] = min(target["max_hp"], target["hp"] + card["base_heal"])
        sync_alive(target)
        healing_done = max(0, target["hp"] - before)

    if alive(target) and card["buff_effect"] != agent.BUFF_NONE:
        target["buffs"][card["buff_effect"]] += card["buff_duration"] + 1

    return damage_done, healing_done


def normalize_self_target(target_idx: int) -> int:
    if 10 <= target_idx < 10 + agent.TEAM_SIZE:
        return target_idx - 10
    return target_idx


def apply_action(actor: dict, opponent: dict, action: dict) -> dict:
    result = {
        "damage": 0,
        "healing": 0,
        "card_name": None,
        "action_type": action.get("type"),
    }

    if action["type"] == agent.ACTION_SWITCH_CHAR:
        switch_idx = int(action.get("switch_to_idx", -1))
        if 0 <= switch_idx < len(actor["team"]) and alive(actor["team"][switch_idx]):
            actor["active_idx"] = switch_idx
        return result

    if action["type"] != agent.ACTION_PLAY_CARD:
        return result

    hand_idx = int(action.get("card_hand_idx", -1))
    if hand_idx < 0 or hand_idx >= len(actor["hand"]):
        return result

    card = clone_card(actor["hand"][hand_idx])
    if actor["energy"] < card["energy_cost"]:
        return result

    actor["energy"] = max(0, actor["energy"] - card["energy_cost"])
    attacker = active_character(actor)
    card["element"] = effective_element(card, attacker)
    if card["type"] == agent.CARD_TYPE_ATTACK and card["buff_effect"] == agent.BUFF_NONE:
        card["buff_effect"] = default_buff_for_element(card["element"])

    result["card_name"] = card["name"]
    target_type = int(card["target_type"])
    if target_type in (agent.TARGET_ENEMY_ALL, agent.TARGET_SELF_ALL):
        target_player = opponent if target_type == agent.TARGET_ENEMY_ALL else actor
        targets = target_player["team"]
    else:
        target_player = actor if target_type == agent.TARGET_SELF_SINGLE else opponent
        target_idx = int(action.get("target_idx", 0))
        if target_type == agent.TARGET_SELF_SINGLE:
            target_idx = normalize_self_target(target_idx)
        if target_idx < 0 or target_idx >= len(target_player["team"]):
            target_idx = target_player["active_idx"]
        targets = [target_player["team"][target_idx]]

    for target in targets:
        damage, healing = resolve_on_target(card, attacker, target)
        result["damage"] += damage
        result["healing"] += healing

    actor["discard_pile"].append(actor["hand"].pop(hand_idx))
    auto_switch_if_dead(opponent)
    auto_switch_if_dead(actor)
    return result


def can_play_any_card(player: dict) -> bool:
    return any(card["energy_cost"] <= player["energy"] for card in player["hand"])


def choose_action(policy: str, state: dict, rng: random.Random) -> dict:
    legal_actions = agent.generate_legal_actions(state)
    normalized_policy = policy.strip().lower()

    if normalized_policy == "random":
        non_end = [action for action in legal_actions if action["type"] != agent.ACTION_END_TURN]
        return dict(rng.choice(non_end or legal_actions))

    if normalized_policy == "first":
        for action in legal_actions:
            if action["type"] == agent.ACTION_PLAY_CARD:
                return dict(action)
        return dict(legal_actions[-1])

    if normalized_policy == "end":
        return dict(legal_actions[-1])

    action = agent.process_game_state(state, policy=normalized_policy)
    selected = agent.select_legal_action(action, state, legal_actions)
    return selected if selected is not None else {"type": agent.ACTION_END_TURN, "debug_reason": "illegal_action"}


def play_turn(actor: dict, opponent: dict, policy: str, rng: random.Random, round_count: int, metrics: dict) -> None:
    action_count = 0
    while alive_members(actor) and alive_members(opponent) and actor["hand"] and can_play_any_card(actor):
        if action_count >= MAX_ACTIONS_PER_TURN:
            metrics["forced_turn_stops"] += 1
            break

        state = make_state(actor, opponent, round_count, actor["player_id"])
        legal_actions = agent.generate_legal_actions(state)
        action = choose_action(policy, state, rng)
        selected = agent.select_legal_action(action, state, legal_actions)
        if selected is None:
            metrics["illegal_actions"] += 1
            break

        if selected["type"] == agent.ACTION_END_TURN:
            break

        result = apply_action(actor, opponent, selected)
        metrics["actions"] += 1
        metrics["damage"] += result["damage"]
        metrics["healing"] += result["healing"]
        metrics["action_types"][str(selected["type"])] = metrics["action_types"].get(str(selected["type"]), 0) + 1
        if result["card_name"]:
            metrics["card_usage"][result["card_name"]] = metrics["card_usage"].get(result["card_name"], 0) + 1
        if str(selected.get("debug_reason", "")).startswith("llm_fallback"):
            metrics["fallback_actions"] += 1

        action_count += 1


def end_round(players: list[dict], rng: random.Random) -> None:
    for player in players:
        decrement_buffs(player)
        player["energy"] = player["max_energy"]
        refill_hand(player, rng)
        auto_switch_if_dead(player)


def winner_by_hp(p1: dict, p2: dict) -> int:
    p1_alive = bool(alive_members(p1))
    p2_alive = bool(alive_members(p2))
    if p1_alive and not p2_alive:
        return 1
    if p2_alive and not p1_alive:
        return 2
    if total_alive_hp(p1) > total_alive_hp(p2):
        return 1
    if total_alive_hp(p2) > total_alive_hp(p1):
        return 2
    return 0


def simulate_match(
    p1_policy: str,
    p2_policy: str,
    seed: int,
    max_rounds: int,
    cards: list[dict] | None = None,
    characters: list[dict] | None = None,
) -> dict:
    rng = random.Random(seed)
    cards = cards or load_cards()
    characters = characters or load_characters()
    p1 = make_player(1, cards, characters, rng)
    p2 = make_player(2, cards, characters, rng)
    players = [p1, p2]
    metrics = {
        "actions": 0,
        "illegal_actions": 0,
        "fallback_actions": 0,
        "forced_turn_stops": 0,
        "damage": 0,
        "healing": 0,
        "action_types": {},
        "card_usage": {},
    }

    first_player = rng.choice([1, 2])
    round_count = 1
    while round_count <= max_rounds and alive_members(p1) and alive_members(p2):
        order = (players if first_player == 1 else [p2, p1])
        for actor in order:
            opponent = p2 if actor is p1 else p1
            policy = p1_policy if actor is p1 else p2_policy
            play_turn(actor, opponent, policy, rng, round_count, metrics)
            if not alive_members(opponent):
                break
        if not alive_members(p1) or not alive_members(p2):
            break
        end_round(players, rng)
        round_count += 1

    return {
        "winner": winner_by_hp(p1, p2),
        "rounds": min(round_count, max_rounds),
        "p1_hp": total_alive_hp(p1),
        "p2_hp": total_alive_hp(p2),
        **metrics,
    }


def merge_counter(target: dict, source: dict) -> None:
    for key, value in source.items():
        target[key] = target.get(key, 0) + value


def evaluate(
    p1_policy: str,
    p2_policy: str,
    matches: int,
    seed: int,
    max_rounds: int,
) -> dict:
    cards = load_cards()
    characters = load_characters()
    summary = {
        "p1_policy": p1_policy,
        "p2_policy": p2_policy,
        "matches": matches,
        "seed": seed,
        "max_rounds": max_rounds,
        "wins": {"0": 0, "1": 0, "2": 0},
        "avg_rounds": 0.0,
        "avg_actions": 0.0,
        "illegal_actions": 0,
        "fallback_actions": 0,
        "forced_turn_stops": 0,
        "damage": 0,
        "healing": 0,
        "action_types": {},
        "card_usage": {},
    }

    total_rounds = 0
    total_actions = 0
    for match_idx in range(matches):
        result = simulate_match(
            p1_policy,
            p2_policy,
            seed + match_idx,
            max_rounds,
            cards=cards,
            characters=characters,
        )
        summary["wins"][str(result["winner"])] += 1
        total_rounds += result["rounds"]
        total_actions += result["actions"]
        summary["illegal_actions"] += result["illegal_actions"]
        summary["fallback_actions"] += result["fallback_actions"]
        summary["forced_turn_stops"] += result["forced_turn_stops"]
        summary["damage"] += result["damage"]
        summary["healing"] += result["healing"]
        merge_counter(summary["action_types"], result["action_types"])
        merge_counter(summary["card_usage"], result["card_usage"])

    if matches > 0:
        summary["avg_rounds"] = round(total_rounds / matches, 2)
        summary["avg_actions"] = round(total_actions / matches, 2)

    return summary


def print_summary(summary: dict) -> None:
    print("AI Evaluation Summary")
    print(f"p1_policy={summary['p1_policy']} p2_policy={summary['p2_policy']}")
    print(f"matches={summary['matches']} seed={summary['seed']} max_rounds={summary['max_rounds']}")
    print(f"wins={summary['wins']} avg_rounds={summary['avg_rounds']} avg_actions={summary['avg_actions']}")
    print(
        "illegal_actions={illegal} fallback_actions={fallback} forced_turn_stops={forced}".format(
            illegal=summary["illegal_actions"],
            fallback=summary["fallback_actions"],
            forced=summary["forced_turn_stops"],
        )
    )
    print(f"damage={summary['damage']} healing={summary['healing']}")
    print(f"action_types={summary['action_types']}")
    print(f"card_usage={summary['card_usage']}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run pure-Python AI policy evaluations.")
    parser.add_argument("--matches", type=int, default=20)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--max-rounds", type=int, default=40)
    parser.add_argument("--p1-policy", default="random")
    parser.add_argument("--p2-policy", default="heuristic")
    parser.add_argument("--json", action="store_true", help="Print machine-readable JSON.")
    args = parser.parse_args()

    summary = evaluate(
        p1_policy=args.p1_policy,
        p2_policy=args.p2_policy,
        matches=max(1, args.matches),
        seed=args.seed,
        max_rounds=max(1, args.max_rounds),
    )
    if args.json:
        print(json.dumps(summary, ensure_ascii=False, indent=2, sort_keys=True))
    else:
        print_summary(summary)


if __name__ == "__main__":
    main()
