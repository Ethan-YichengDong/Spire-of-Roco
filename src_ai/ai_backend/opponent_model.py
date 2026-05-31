import agent


def hp_ratio(character: dict) -> float:
    max_hp = max(1, int(character.get("max_hp", 1)))
    return int(character.get("hp", 0)) / max_hp


def card_bucket(card: dict) -> str:
    if int(card.get("base_damage", 0)) > 0:
        return "attack"
    if int(card.get("base_heal", 0)) > 0:
        return "heal"
    if int(card.get("base_defense", 0)) > 0:
        return "defense"
    if int(card.get("buff_effect", agent.BUFF_NONE)) == agent.BUFF_POWER:
        return "power"
    return "utility"


def hand_profile(player: dict) -> dict:
    profile = {"attack": 0, "defense": 0, "heal": 0, "power": 0, "utility": 0}
    for card in player.get("hand", []):
        bucket = card_bucket(card)
        profile[bucket] = profile.get(bucket, 0) + 1
    return profile


def estimate_style(profile: dict) -> str:
    attack = profile.get("attack", 0)
    defense = profile.get("defense", 0)
    heal = profile.get("heal", 0)
    power = profile.get("power", 0)

    if attack >= defense + heal + 1:
        return "aggressive"
    if heal >= attack + 2:
        return "sustain"
    if defense + heal >= attack + power + 2:
        return "defensive"
    if power >= attack and power >= 2:
        return "setup"
    return "balanced"


def lowest_hp_target(player: dict) -> dict | None:
    alive_team = [
        member
        for member in player.get("team", [])
        if bool(member.get("is_alive", 0)) and int(member.get("hp", 0)) > 0
    ]
    if not alive_team:
        return None
    target = min(alive_team, key=hp_ratio)
    return {
        "index": target.get("index"),
        "name": target.get("name"),
        "hp_ratio": round(hp_ratio(target), 3),
    }


class OpponentModel:
    def __init__(self) -> None:
        self.observations = 0
        self.switches = 0
        self.last_active_idx = None
        self.seen_cards = {}

    def update(self, state_dict: dict) -> dict:
        players = state_dict.get("players", {})
        opponent = players.get("opponent", {}) if isinstance(players, dict) else {}
        active_idx = opponent.get("active_idx")

        if self.last_active_idx is not None and active_idx != self.last_active_idx:
            self.switches += 1
        self.last_active_idx = active_idx
        self.observations += 1

        for card in opponent.get("hand", []):
            name = str(card.get("name", "unknown"))
            self.seen_cards[name] = self.seen_cards.get(name, 0) + 1

        profile = hand_profile(opponent)
        style = estimate_style(profile)
        common_cards = sorted(self.seen_cards.items(), key=lambda item: item[1], reverse=True)[:5]

        return {
            "observations": self.observations,
            "estimated_style": style,
            "switches": self.switches,
            "active_idx": active_idx,
            "hand_profile": profile,
            "lowest_hp_target": lowest_hp_target(opponent),
            "common_seen_cards": [{"name": name, "count": count} for name, count in common_cards],
        }
