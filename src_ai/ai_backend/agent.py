import json
import urllib.request
import urllib.error

# 用于连接大语言模型（LLM）的请求桥接函数（目前以本地 Ollama 的 mock/stub 为例）
def query_llm(prompt: str) -> dict:
    url = "http://127.0.0.1:11434/api/generate"
    data = {
        "model": "qwen",
        "prompt": prompt,
        "stream": False,
        "format": "json"
    }
    
    try:
        # 发送网络请求，设置超时时间为 5 秒
        req = urllib.request.Request(url, data=json.dumps(data).encode('utf-8'), headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=5) as response:
            result = json.loads(response.read().decode('utf-8'))
            llm_response = json.loads(result.get("response", "{}"))
            return llm_response
    except Exception as e:
        print(f"大语言模型 (LLM) API 请求报错: {e}")
        # 如果模型服务无法响应或者挂断，则返回兜底（Fallback）的标准出牌动作结构
        return {"type": 1, "card_hand_idx": 0, "switch_to_idx": -1}

# 核心决策逻辑：包装并发送当前的具体游戏状态给大模型做判定
def process_game_state(state_dict: dict) -> dict:
    # 构建用来引导大型语言模型推理指令的 Prompt 上下文
    prompt = f"""
    你是一个正在游玩卡牌对战游戏的 AI。当前游戏面板状态如下：
    {json.dumps(state_dict, indent=2)}

    你需要选择执行一项操作。 
    行动类型的数字枚举映射如下: ACTION_NONE=0（无）, ACTION_PLAY_CARD=1（出牌）, ACTION_SWITCH_CHAR=2（换人）, ACTION_END_TURN=3（结束回合）.
    请严格遵循以下 JSON 数据模式并只输出 JSON:
    {{"type": (int), "card_hand_idx": (int), "switch_to_idx": (int)}}
    如果你想出牌，请确保对应的 'type' 写为 1，并且给出 'card_hand_idx' 中打出的手牌索引. 
    如果你当前的能量不够，或者此时并没有手牌，请选择 type: 3 直接跳过。
    """

    # 如果有本地部署好的 Ollama 服务，可以解开注释向模型进行提问请求
    # action = query_llm(prompt)
    
    # 作为占位符的默认执行策略：伪造 LLM 已经解析完成，且无脑选择手牌第一张出牌
    action = {
        "type": 1,
        "card_hand_idx": 0,
        "switch_to_idx": -1
    }

    # 进行二次安全校验：确保必须包含 C 语言解析器强制依赖的基础字段
    if "type" not in action:
        action["type"] = 1
    if "card_hand_idx" not in action:
        action["card_hand_idx"] = 0
    if "switch_to_idx" not in action:
        action["switch_to_idx"] = -1

    return action
