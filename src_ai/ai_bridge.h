#ifndef AI_BRIDGE_H
#define AI_BRIDGE_H

#include "../game_core.h"

// 通过桥接调用后台逻辑（如 Python 进程）来获取 AI 玩家的行动判定
Action GetAIActionFromBackend(GameState state, int ai_player_id);

#endif // AI_BRIDGE_