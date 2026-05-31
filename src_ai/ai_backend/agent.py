import json
import os
import urllib.error
import urllib.request

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

DEFAULT_AI_POLICY = "heuristic"
DEFAULT_LLM_API = "openai"
DEFAULT_LLM_BASE_URL = "http://114.212.227.193:8000"
DEFAULT_LLM_MODEL = "Qwen3.5-4B"
DEFAULT_LLM_TIMEOUT = 5.0
DEFAULT_LLM_MAX_TOKENS = 256
DEFAULT_LLM_TEMPERATURE = 0.0


def _end_turn(reason: str) -> dict:
    return {
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
        target_idx = int(target.get("index", 0))
        score = 0.0

        heal = int(card.get("base_heal", 0))
        if heal > 0:
            missing = max(0, int(target.get("max_hp", 0)) - int(target.get("hp", 0)))
            effective_heal = min(heal, missing)
            score += effective_heal * 1.7
            if not _alive(target):
                score += 55.0

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

    player = players.get("self", {})
    opponent = players.get("opponent", {})
    energy = int(player.get("energy", 0))

    best_score, switch_idx = _score_switch(player)
    best_action = None
    if switch_idx >= 0:
        best_action = {
            "type": ACTION_SWITCH_CHAR,
            "card_hand_idx": -1,
            "switch_to_idx": switch_idx,
            "target_idx": 0,
            "debug_reason": "switch_to_healthier_character",
        }

    for card in player.get("hand", []):
        cost = int(card.get("energy_cost", 0))
        if cost > energy:
            continue

        score, target_idx = _score_card(card, player, opponent)
        if score <= 0:
            continue

        # Slightly prefer efficient low-cost cards when scores are close.
        score += 1.0 / max(1, cost)

        if best_action is None or score > best_score:
            best_score = score
            best_action = {
                "type": ACTION_PLAY_CARD,
                "card_hand_idx": int(card.get("hand_idx", 0)),
                "switch_to_idx": -1,
                "target_idx": target_idx,
                "debug_reason": f"play_{card.get('name', 'card')}",
            }

    if best_action is None:
        return _end_turn("no_profitable_action")

    return best_action


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


def _build_llm_prompt(state_dict: dict, heuristic_action: dict) -> str:
    compact_state = json.dumps(state_dict, ensure_ascii=False, separators=(",", ":"))
    compact_fallback = json.dumps(heuristic_action, ensure_ascii=False, separators=(",", ":"))
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

只允许输出 JSON，不要解释。JSON 必须包含:
{{"type": int, "card_hand_idx": int, "switch_to_idx": int, "target_idx": int, "debug_reason": str}}

如果无法判断，请返回这个启发式兜底动作:
{compact_fallback}

当前状态:
{compact_state}
""".strip()


def process_game_state_llm(state_dict: dict) -> dict:
    heuristic_action = process_game_state_heuristic(state_dict)
    prompt = _build_llm_prompt(state_dict, heuristic_action)

    try:
        llm_action = query_llm(prompt)
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, ValueError, OSError) as exc:
        heuristic_action["debug_reason"] = f"llm_fallback:{type(exc).__name__}"
        return heuristic_action

    normalized = _normalize_action(llm_action, "llm_action")
    if normalized is None:
        heuristic_action["debug_reason"] = "llm_fallback:invalid_action"
        return heuristic_action

    return normalized


def process_game_state(state_dict: dict, policy: str | None = None) -> dict:
    selected_policy = (policy or os.getenv("ROCO_AI_POLICY", DEFAULT_AI_POLICY)).strip().lower()

    if selected_policy in ("llm", "ollama", "model"):
        return process_game_state_llm(state_dict)

    if selected_policy not in ("heuristic", "rule", "rules"):
        fallback = process_game_state_heuristic(state_dict)
        fallback["debug_reason"] = f"unknown_policy:{selected_policy}"
        return fallback

    return process_game_state_heuristic(state_dict)
