import argparse
import json

import agent
import evaluate


TEAM_SIZE = agent.TEAM_SIZE
DEFAULT_DECK_SIZE = 16

STYLE_PROFILES = {
    "balanced": {"attack": 5, "defense": 4, "heal": 4, "power": 3},
    "aggressive": {"attack": 7, "defense": 2, "heal": 2, "power": 5},
    "defensive": {"attack": 3, "defense": 6, "heal": 5, "power": 2},
    "sustain": {"attack": 3, "defense": 3, "heal": 7, "power": 3},
    "power": {"attack": 4, "defense": 2, "heal": 3, "power": 7},
}

ELEMENT_COUNTERS = {
    agent.ELEMENT_FIRE: agent.ELEMENT_WATER,
    agent.ELEMENT_GRASS: agent.ELEMENT_FIRE,
    agent.ELEMENT_WATER: agent.ELEMENT_GRASS,
}


def normalize_style(style: str) -> str:
    normalized = style.strip().lower()
    return normalized if normalized in STYLE_PROFILES else "balanced"


def character_score(character: dict, style: str, wanted_elements: set[int]) -> float:
    hp = int(character.get("max_hp", character.get("hp", 0)))
    speed = int(character.get("speed", 0))
    element = int(character.get("element", agent.ELEMENT_NORMAL))

    if style == "aggressive":
        score = speed * 1.4 + hp * 0.45
    elif style in ("defensive", "sustain"):
        score = hp * 1.15 + speed * 0.35
    else:
        score = hp * 0.85 + speed * 0.85

    if element in wanted_elements:
        score += 35.0
    return score


def wanted_counter_elements(opponent_team: list[dict] | None) -> set[int]:
    wanted = set()
    for character in opponent_team or []:
        counter = ELEMENT_COUNTERS.get(int(character.get("element", agent.ELEMENT_NORMAL)))
        if counter is not None:
            wanted.add(counter)
    return wanted


def choose_team(
    characters: list[dict],
    style: str = "balanced",
    opponent_team: list[dict] | None = None,
    team_size: int = TEAM_SIZE,
) -> list[dict]:
    style = normalize_style(style)
    wanted_elements = wanted_counter_elements(opponent_team)
    selected = []
    used_ids = set()
    used_elements = set()

    candidates = sorted(
        characters,
        key=lambda character: character_score(character, style, wanted_elements),
        reverse=True,
    )

    for character in candidates:
        element = int(character.get("element", agent.ELEMENT_NORMAL))
        if character.get("char_id") in used_ids:
            continue
        if element in used_elements and len(candidates) - len(selected) >= team_size:
            continue
        selected.append(dict(character))
        used_ids.add(character.get("char_id"))
        used_elements.add(element)
        if len(selected) >= team_size:
            break

    for character in candidates:
        if len(selected) >= team_size:
            break
        if character.get("char_id") in used_ids:
            continue
        selected.append(dict(character))
        used_ids.add(character.get("char_id"))

    for idx, character in enumerate(selected):
        character["index"] = idx
    return selected[:team_size]


def card_bucket(card: dict) -> str:
    if int(card.get("base_damage", 0)) > 0:
        return "attack"
    if int(card.get("base_heal", 0)) > 0:
        return "heal"
    if int(card.get("base_defense", 0)) > 0:
        return "defense"
    if int(card.get("buff_effect", agent.BUFF_NONE)) == agent.BUFF_POWER:
        return "power"
    if int(card.get("type", agent.CARD_TYPE_SKILL)) == agent.CARD_TYPE_POWER:
        return "power"
    return "defense"


def bucket_cards(cards: list[dict]) -> dict[str, list[dict]]:
    buckets = {key: [] for key in ("attack", "defense", "heal", "power")}
    for card in cards:
        buckets[card_bucket(card)].append(dict(card))
    for bucket in buckets.values():
        bucket.sort(key=lambda card: (int(card.get("energy_cost", 0)), card.get("card_id", 0)))
    return buckets


def build_deck(
    cards: list[dict],
    team: list[dict],
    style: str = "balanced",
    deck_size: int = DEFAULT_DECK_SIZE,
) -> list[dict]:
    style = normalize_style(style)
    profile = dict(STYLE_PROFILES[style])
    deck_size = max(1, min(deck_size, evaluate.MAX_DECK_SIZE))
    total_profile = sum(profile.values())
    if total_profile != deck_size:
        scale = deck_size / max(1, total_profile)
        profile = {key: max(1, round(value * scale)) for key, value in profile.items()}

    while sum(profile.values()) > deck_size:
        biggest = max(profile, key=profile.get)
        profile[biggest] -= 1
    while sum(profile.values()) < deck_size:
        profile["attack"] += 1

    buckets = bucket_cards(cards)
    team_elements = {int(member.get("element", agent.ELEMENT_NORMAL)) for member in team}
    deck = []
    for bucket_name, amount in profile.items():
        bucket = buckets.get(bucket_name, [])
        if not bucket:
            continue
        preferred = sorted(
            bucket,
            key=lambda card: (
                0 if int(card.get("element", agent.ELEMENT_NORMAL)) in team_elements else 1,
                int(card.get("energy_cost", 0)),
                card.get("card_id", 0),
            ),
        )
        for idx in range(amount):
            deck.append(dict(preferred[idx % len(preferred)]))

    if len(deck) < deck_size:
        fallback_cards = sorted(cards, key=lambda card: (int(card.get("energy_cost", 0)), card.get("card_id", 0)))
        idx = 0
        while len(deck) < deck_size and fallback_cards:
            deck.append(dict(fallback_cards[idx % len(fallback_cards)]))
            idx += 1

    return deck[:deck_size]


def recommend_draft(style: str = "balanced", deck_size: int = DEFAULT_DECK_SIZE) -> dict:
    cards = evaluate.load_cards()
    characters = evaluate.load_characters()
    team = choose_team(characters, style=style)
    deck = build_deck(cards, team, style=style, deck_size=deck_size)
    return {
        "style": normalize_style(style),
        "team": team,
        "deck": deck,
        "team_ids": [member["char_id"] for member in team],
        "deck_ids": [card["card_id"] for card in deck],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Recommend an AI team and deck.")
    parser.add_argument("--style", default="balanced", choices=sorted(STYLE_PROFILES))
    parser.add_argument("--deck-size", type=int, default=DEFAULT_DECK_SIZE)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    draft = recommend_draft(style=args.style, deck_size=args.deck_size)
    if args.json:
        print(json.dumps(draft, ensure_ascii=False, indent=2, sort_keys=True))
        return

    print(f"Draft style: {draft['style']}")
    print(f"Team IDs: {draft['team_ids']}")
    print(f"Deck IDs: {draft['deck_ids']}")


if __name__ == "__main__":
    main()
