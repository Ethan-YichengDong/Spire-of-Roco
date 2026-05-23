#ifndef BATTLE_CALCULATOR_H
#define BATTLE_CALCULATOR_H

#include "../game_core.h"

// 管道结算第一段：获取原始面板伤害
int GetRawDamage(Card* card, Character* attacker);

// 管道结算第二段：防御截获与反应增幅（使用攻击者元素属性判定克制）
int CalculateMitigation(int raw_damage, Card* card, Character* attacker, Character* target);

// 管道结算第三段：真实扣血与阵亡事件触发器
void CommitDamageAndCheck(int final_damage, Character* target);

// 解析并结算本回合双方的行动，更新游戏状态
void ResolveTurn(GameState *state, Action a1, Action a2);

#endif // BATTLE_CALCULATOR_H
