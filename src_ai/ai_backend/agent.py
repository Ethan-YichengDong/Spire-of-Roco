import json
import urllib.request
import urllib.error

# Mock/Stub for Ollama API connector
def query_llm(prompt: str) -> dict:
    url = "http://127.0.0.1:11434/api/generate"
    data = {
        "model": "qwen",
        "prompt": prompt,
        "stream": False,
        "format": "json"
    }
    
    try:
        req = urllib.request.Request(url, data=json.dumps(data).encode('utf-8'), headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=5) as response:
            result = json.loads(response.read().decode('utf-8'))
            llm_response = json.loads(result.get("response", "{}"))
            return llm_response
    except Exception as e:
        print(f"LLM API Error: {e}")
        # Fallback dictionary if LLM fails
        return {"type": 1, "card_hand_idx": 0, "switch_to_idx": -1}

def process_game_state(state_dict: dict) -> dict:
    prompt = f"""
    You are an AI playing a card game. Here is the current game state:
    {json.dumps(state_dict, indent=2)}

    You must choose an action. 
    Action Enum values: ACTION_NONE=0, ACTION_PLAY_CARD=1, ACTION_SWITCH_CHAR=2, ACTION_END_TURN=3.
    Reply strictly in JSON matching this schema:
    {{"type": (int), "card_hand_idx": (int), "switch_to_idx": (int)}}
    Make sure to pick type: 1 and a valid card_hand_idx if you want to play a card. 
    If you don't have enough energy or cards, pick type: 3.
    """

    # Query LLM (can be commented out to just return a deterministic action)
    # action = query_llm(prompt)
    
    # Placeholder fast-fallback if we don't literally run Ollama right now
    # We simulate an LLM parsing it and returning a valid play
    action = {
        "type": 1,
        "card_hand_idx": 0,
        "switch_to_idx": -1
    }

    # Ensure strictly required fields exist
    if "type" not in action:
        action["type"] = 1
    if "card_hand_idx" not in action:
        action["card_hand_idx"] = 0
    if "switch_to_idx" not in action:
        action["switch_to_idx"] = -1

    return action
