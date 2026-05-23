#include "battle_calculator.h"
#include <stdio.h>

// 获取原始面板伤害（三段式流水线第一段）
// 基础伤害 + 攻击者力量Buff加成（力量Buff激活时固定+2伤害）
int GetRawDamage(Card* card, Character* attacker) {
    int raw = card->base_damage;
    // buffs[]中>0表示该Buff处于激活状态，力量Buff提供固定伤害加成
    if (attacker->buffs[BUFF_POWER] > 0) {
        raw += 2;
    }
    return raw;
}

// 根据角色元素属性返回其默认施加的异常状态类型
// 水→潮湿 / 火→燃烧 / 草→中毒 / 电、普通→无默认异常
static BuffType get_element_default_buff(ElementType element) {
    switch (element) {
        case ELEMENT_WATER:   return BUFF_WET;
        case ELEMENT_FIRE:    return BUFF_BURN;
        case ELEMENT_GRASS:   return BUFF_POISON;
        default:              return BUFF_NONE;
    }
}

// 防御截获与元素反应增幅（三段式流水线第二段）
// 依次处理：元素属性克制倍率 → 异常状态Combo反应 → 护盾抵扣减伤
int CalculateMitigation(int raw_damage, Card* card, Character* attacker, Character* target) {
    (void)card; // card参数保留供后续特殊卡牌逻辑扩展使用
    int damage = raw_damage;

    // 一、元素属性克制判定：克制方伤害×2
    // 水克火 / 火克草 / 草克水
    if (attacker->element == ELEMENT_WATER && target->element == ELEMENT_FIRE)      damage *= 2;
    else if (attacker->element == ELEMENT_FIRE && target->element == ELEMENT_GRASS) damage *= 2;
    else if (attacker->element == ELEMENT_GRASS && target->element == ELEMENT_WATER) damage *= 2;

    // 二、异常状态Combo反应判定
    // 潮湿状态 + 电系攻击 → 触发"感电"，伤害翻倍
    if (attacker->element == ELEMENT_ELECTRIC && target->buffs[BUFF_WET] > 0) {
        damage *= 2;
    }

    // 三、护盾抵扣：目标持有护盾时，护盾值优先吸收伤害
    if (target->buffs[BUFF_SHIELD] > 0) {
        int shield = target->buffs[BUFF_SHIELD];
        if (damage <= shield) {
            // 护盾足以完全抵挡本次伤害
            target->buffs[BUFF_SHIELD] -= damage;
            damage = 0;
        } else {
            // 护盾不足，扣除全部护盾后剩余伤害穿透
            damage -= shield;
            target->buffs[BUFF_SHIELD] = 0;
        }
    }

    return damage;
}

// 真实扣血与阵亡事件触发器（三段式流水线第三段）
// 执行最终生命值扣除，血量归零则标记角色阵亡
void CommitDamageAndCheck(int final_damage, Character* target) {
    target->hp -= final_damage;
    if (target->hp <= 0) {
        target->hp = 0;
        target->is_alive = 0;
    }
}

// 根据行动目标索引解析实际指向的角色指针
// target_idx: 0~2→敌方队伍对应位置 / 10~12→己方队伍(下标-10) / 其他→默认敌方出战角色
static Character* resolve_target(Player* acting, Player* enemy, int target_idx) {
    if (target_idx >= 0 && target_idx < TEAM_SIZE) {
        return &enemy->team[target_idx];
    } else if (target_idx >= 10 && target_idx < 10 + TEAM_SIZE) {
        return &acting->team[target_idx - 10];
    }
    // 索引异常时回退至敌方出战角色，保证游戏不崩溃
    return &enemy->team[enemy->active_idx];
}

// 对单个目标执行完整的三段式伤害结算与Buff/护盾施加
static void resolve_on_target(Card* played_card, Character* attacker, Character* target_char) {
    if (!target_char->is_alive) return;

    // === 三段式伤害结算流水线 ===
    if (played_card->base_damage > 0) {
        int raw = GetRawDamage(played_card, attacker);
        int final_dmg = CalculateMitigation(raw, played_card, attacker, target_char);
        CommitDamageAndCheck(final_dmg, target_char);
    }

    // 施加护盾/防御值（用于为己方角色提供防护）
    if (played_card->base_defense > 0) {
        target_char->buffs[BUFF_SHIELD] += played_card->base_defense;
    }

    // 施加异常状态（Buff/Debuff），持续回合数来自卡牌数据
    if (played_card->buff_effect != BUFF_NONE) {
        target_char->buffs[played_card->buff_effect] = played_card->buff_duration;
    }
}

// 执行应用具体行动：切换角色上场，或对指定目标释放卡牌并完成数值结算
static void apply_action(Player* acting_player, Player* target_player, Action action) {
    // === 行动类型一：切换出战角色 ===
    if (action.type == ACTION_SWITCH_CHAR) {
        if (action.switch_to_idx >= 0 && action.switch_to_idx < TEAM_SIZE
            && acting_player->team[action.switch_to_idx].is_alive) {
            acting_player->active_idx = action.switch_to_idx;
        }
        return;
    }

    // === 行动类型二：打出卡牌 ===
    if (action.type != ACTION_PLAY_CARD) return;
    if (action.card_hand_idx < 0 || action.card_hand_idx >= acting_player->hand_count) return;

    Card played_card = acting_player->hand[action.card_hand_idx];
    Character* active_char = &acting_player->team[acting_player->active_idx];

    // 能量不足则放弃本次行动
    if (acting_player->energy < played_card.energy_cost) return;

    // 扣除能量并钳制，防止越界导致UI崩溃
    acting_player->energy -= played_card.energy_cost;
    if (acting_player->energy < 0) acting_player->energy = 0;
    if (acting_player->energy > acting_player->max_energy) acting_player->energy = acting_player->max_energy;

    // 从出战角色实时赋予卡牌元素属性（卡牌本身不绑定固定元素，由使用者决定）
    played_card.element = active_char->element;
    // 若卡牌数据中未指定Buff效果，则根据角色元素属性赋予默认异常状态
    if (played_card.buff_effect == BUFF_NONE) {
        played_card.buff_effect = get_element_default_buff(active_char->element);
    }

    // === 根据目标索引分发结算 ===
    if (action.target_idx == -1 || action.target_idx == -2) {
        // AOE：范围伤害/群体效果，遍历对应队伍中所有存活角色
        Player* aoe_team = (action.target_idx == -1) ? target_player : acting_player;
        for (int i = 0; i < TEAM_SIZE; i++) {
            resolve_on_target(&played_card, active_char, &aoe_team->team[i]);
        }
    } else {
        // 单体目标：根据索引解析目标角色（敌方或己方）
        Character* target_char = resolve_target(acting_player, target_player, action.target_idx);
        resolve_on_target(&played_card, active_char, target_char);
    }

    // 出手后从手牌中移除此卡（后续手牌前移一位填补空位）
    for (int i = action.card_hand_idx; i < acting_player->hand_count - 1; i++) {
        acting_player->hand[i] = acting_player->hand[i + 1];
    }
    acting_player->hand_count--;
}

// 每回合结束时，减少角色身上所有Buff的剩余持续回合数
static void decrement_buffs(Character* c) {
    for (int i = 0; i < BUFF_COUNT; i++) {
        if (c->buffs[i] > 0) {
            c->buffs[i]--;
        }
    }
}

// 回合结算主入口：根据速度决定双方行动次序，执行并推进Buff回合
void ResolveTurn(GameState *state, Action a1, Action a2) {
    Character* c1 = &state->p1.team[state->p1.active_idx];
    Character* c2 = &state->p2.team[state->p2.active_idx];

    // 速度高者优先执行行动（速度相同时P1先手）
    if (c1->speed >= c2->speed) {
        apply_action(&state->p1, &state->p2, a1);
        // 先手攻击若直接击倒对方出战角色，后手行动取消
        if (c2->is_alive) {
            apply_action(&state->p2, &state->p1, a2);
        }
    } else {
        apply_action(&state->p2, &state->p1, a2);
        if (c1->is_alive) {
            apply_action(&state->p1, &state->p2, a1);
        }
    }

    // 回合结束：双方所有角色（含替补）的Buff持续回合统一递减1
    for (int i = 0; i < TEAM_SIZE; i++) {
        decrement_buffs(&state->p1.team[i]);
        decrement_buffs(&state->p2.team[i]);
    }
}
