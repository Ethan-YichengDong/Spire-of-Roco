#ifndef AI_BRIDGE_H
#define AI_BRIDGE_H

#include "../game_core.h"

Action GetAIActionFromBackend(GameState state, int ai_player_id);

#endif // AI_BRIDGE_H
