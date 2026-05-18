#include "battle_calculator.h"
#include <stdio.h>

// 执行应用具体行动：切换角色或对目标释放卡牌技能的结算
static void apply_action(Player* acting_player, Player* target_player, Action action) {
    // 切换出战角色的行动
    if (action.type == ACTION_SWITCH_CHAR) {
        if (action.switch_to_idx >= 0 && action.switch_to_idx < TEAM_SIZE && acting_player->team[action.switch_to_idx].is_alive) {
            acting_player->active_idx = action.switch_to_idx;
        }
    } 
    // 出牌的行动
    else if (action.type == ACTION_PLAY_CARD) {
        if (action.card_hand_idx >= 0 && action.card_hand_idx < acting_player->hand_count) {
            Card played_card = acting_player->hand[action.card_hand_idx];
            Character* target_char = &target_player->team[target_player->active_idx];

            // 检查能量是否足够释放此牌
            if (acting_player->energy >= played_card.energy_cost) {
                acting_player->energy -= played_card.energy_cost;

                // 计算基础伤害
                int damage = played_card.base_damage;

                // 元素克制系统：水克火，火克草，草克水
                if (played_card.element == ELEMENT_WATER && target_char->element == ELEMENT_FIRE) damage *= 2;
                else if (played_card.element == ELEMENT_FIRE && target_char->element == ELEMENT_GRASS) damage *= 2;
                else if (played_card.element == ELEMENT_GRASS && target_char->element == ELEMENT_WATER) damage *= 2;
                // 雷元素对附带水Buff（潮湿）的目标可以造成翻倍伤害
                if (played_card.element == ELEMENT_ELECTRIC && target_char->buffs[BUFF_WET] > 0) damage *= 2;

                // 应用卡牌附加的增益/减益效果（Buff/Debuff）
                if (played_card.buff_effect != BUFF_NONE) {
                    target_char->buffs[played_card.buff_effect] = played_card.buff_duration;
                }

                // 计算最终扣血并检查存活状态
                target_char->hp -= damage;
                if (target_char->hp <= 0) {
                    target_char->hp = 0;
                    target_char->is_alive = 0;
                }
            }

            // 出手后从手牌中移除该卡牌
            for (int i = action.card_hand_idx; i < acting_player->hand_count - 1; i++) {
                acting_player->hand[i] = acting_player->hand[i + 1];
            }
            acting_player->hand_count--;
        }
    }
}

// 每回合结束时减少所有角色身上Buff效果的持续时间
static void decrement_buffs(Character* c) {
    for (int i = 0; i < BUFF_COUNT; i++) {
        if (c->buffs[i] > 0) {
            c->buffs[i]--;
        }
    }
}

// 整个回合的逻辑判定主入口：根据速度来排列双方玩家的行动次序并分别结算代码逻辑
void ResolveTurn(GameState *state, Action a1, Action a2) {
    Character* c1 = &state->p1.team[state->p1.active_idx];
    Character* c2 = &state->p2.team[state->p2.active_idx];

    // 基于速度属性优先进行先手判断，速度快的先执行
    if (c1->speed >= c2->speed) {
        apply_action(&state->p1, &state->p2, a1);
        if (c2->is_alive) { // 万一前面一击直接被击败，则必须检查后手存活状态再应用后手行动
            apply_action(&state->p2, &state->p1, a2);
        }
    } else {
        apply_action(&state->p2, &state->p1, a2);
        if (c1->is_alive) {
            apply_action(&state->p1, &state->p2, a1);
        }
    }

    // 结算完成后，减少所有登场与未登场角色的Buff持续时间一回合
    for (int i = 0; i < TEAM_SIZE; i++) {
        decrement_buffs(&state->p1.team[i]);
        decrement_buffs(&state->p2.team[i]);
    }
}
