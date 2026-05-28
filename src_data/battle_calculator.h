#ifndef BATTLE_CALCULATOR_H
#define BATTLE_CALCULATOR_H

#include "../game_core.h"

// ===== 三段式伤害结算流水线 =====
// 第一段：获取原始面板伤害（含攻击力Buff加成）
int GetRawDamage(Card* card, Character* attacker);
// 第二段：元素克制、异常Combo反应与护盾抵扣
int CalculateMitigation(int raw_damage, ElementType attack_element, Character* target);
// 第三段：真实扣血与阵亡事件触发
void CommitDamageAndCheck(int final_damage, Character* target);

// ===== 回合结算接口 =====
// 执行单个行动：acting_player_id为1或2，引擎在循环中多次调用直到能量耗尽或主动结束
void ExecuteAction(GameState* state, Action* action, int acting_player_id);
// 回合结束清理：Buff递减、能量回满、手牌补满、阵亡出战切换
void EndTurn(GameState* state);

#endif // BATTLE_CALCULATOR_H