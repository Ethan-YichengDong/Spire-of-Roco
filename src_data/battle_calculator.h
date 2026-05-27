#ifndef BATTLE_CALCULATOR_H
#define BATTLE_CALCULATOR_H

#include "../game_core.h"

// ===== 三段式伤害结算流水线 =====
// 第一段：获取原始面板伤害（含攻击力Buff加成）
int GetRawDamage(Card* card, Character* attacker);
// 第二段：元素克制、异常Combo反应与护盾抵扣
int CalculateMitigation(int raw_damage, Card* card, Character* attacker, Character* target);
// 第三段：真实扣血与阵亡事件触发
void CommitDamageAndCheck(int final_damage, Character* target);

// ===== 回合结算主入口 =====
// 根据速度判定行动次序，结算双方Action，执行回合结束清理
void ResolveTurn(GameState *state, Action a1, Action a2);

#endif // BATTLE_CALCULATOR_H