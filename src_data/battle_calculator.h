#ifndef BATTLE_CALCULATOR_H
#define BATTLE_CALCULATOR_H

#include "../game_core.h"

// 解析并结算本回合双方的行动，更新游戏状态
void ResolveTurn(GameState *state, Action a1, Action a2);

#endif // BATTLE_CALCULATOR_H
