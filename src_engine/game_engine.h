#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "../game_core.h"

// 启动并运行主游戏循环
void RunGameLoop();

// 根据指定的模式（AI或人类控制）获取玩家2的行动决策
Action GetPlayer2Action(GameState state, int mode);

#endif // GAME_ENGINE_H
