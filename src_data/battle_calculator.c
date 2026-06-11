#include "battle_calculator.h"
#include "data_manager.h"
#include <stdio.h>
#include <string.h>

// ============================================================
//  内部辅助函数（static，仅本文件可见）
// ============================================================

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

// 根据卡牌目标类型与索引解析实际指向的角色指针
// TARGET_ENEMY_SINGLE → target_idx 0~2 映射敌方队伍
// TARGET_SELF_SINGLE  → target_idx 0~2 映射己方队伍
static Character* resolve_target(Player* acting, Player* enemy, int target_idx, TargetType target_type) {
    if (target_type == TARGET_SELF_SINGLE) {
        if (target_idx >= 0 && target_idx < TEAM_SIZE)
            return &acting->team[target_idx];
        return &acting->team[acting->active_idx];
    } else {
        if (target_idx >= 0 && target_idx < TEAM_SIZE)
            return &enemy->team[target_idx];
        return &enemy->team[enemy->active_idx];
    }
} 

// 每回合结束时减少角色Buff持续回合数（护盾除外，其值为吸收量不受回合影响）
static void decrement_buffs(Character* c) {
    for (int i = 0; i < BUFF_COUNT; i++) {
        if (i == BUFF_SHIELD) continue;
        if (c->buffs[i] > 0) {
            c->buffs[i]--;
        }
    }
}

static int get_element_multiplier(int raw_damage, ElementType attack_element, Character* target) {
    int multiplier = 1;

    if (attack_element == ELEMENT_WATER && target->element == ELEMENT_FIRE) multiplier *= 2;
    else if (attack_element == ELEMENT_FIRE && target->element == ELEMENT_GRASS) multiplier *= 2;
    else if (attack_element == ELEMENT_GRASS && target->element == ELEMENT_WATER) multiplier *= 2;

    if (attack_element == ELEMENT_ELECTRIC && target->buffs[BUFF_WET] > 0) {
        multiplier *= 2;
    }

    (void)raw_damage;
    return multiplier;
}

static void append_damage_report(ResolutionReport* report,
                                 int target_player_id,
                                 int target_slot_idx,
                                 Character* target,
                                 int raw_damage,
                                 int element_bonus_damage,
                                 int shield_absorbed,
                                 int final_damage,
                                 int hp_before,
                                 int hp_after) {
    if (!report || report->event_count >= MAX_RESOLUTION_EVENTS) return;

    DamageResolutionEvent* event = &report->events[report->event_count++];
    event->target_player_id = target_player_id;
    event->target_slot_idx = target_slot_idx;
    snprintf(event->target_name, sizeof(event->target_name), "%s", target->name);
    event->raw_damage = raw_damage;
    event->element_bonus_damage = element_bonus_damage;
    event->shield_absorbed = shield_absorbed;
    event->final_damage = final_damage;
    event->hp_before = hp_before;
    event->hp_after = hp_after;
    event->has_damage = 1;
}

// ============================================================
//  三段式伤害结算流水线（公有接口）
// ============================================================

// 第一段：获取原始面板伤害（含力量Buff加成，激活时固定+2）
int GetRawDamage(Card* card, Character* attacker) {
    int raw = card->base_damage;
    if (attacker->buffs[BUFF_POWER] > 0) {
        raw += 2;
    }
    return raw;
}

// 第二段：元素克制倍率 → 异常Combo反应 → 护盾抵扣
int CalculateMitigation(int raw_damage, ElementType attack_element, Character* target) {
    int damage = raw_damage;

    // 一、元素属性克制：水克火 / 火克草 / 草克水，克制方伤害×2
    if (attack_element == ELEMENT_WATER && target->element == ELEMENT_FIRE)      damage *= 2;
    else if (attack_element == ELEMENT_FIRE && target->element == ELEMENT_GRASS) damage *= 2;
    else if (attack_element == ELEMENT_GRASS && target->element == ELEMENT_WATER) damage *= 2;

    // 二、异常状态Combo：潮湿+电系 → 感电，伤害翻倍
    if (attack_element == ELEMENT_ELECTRIC && target->buffs[BUFF_WET] > 0) {
        damage *= 2;
    }

    // 三、护盾抵扣：护盾优先吸收伤害
    if (target->buffs[BUFF_SHIELD] > 0) {
        int shield = target->buffs[BUFF_SHIELD];
        if (damage <= shield) {
            target->buffs[BUFF_SHIELD] -= damage;
            damage = 0;
        } else {
            damage -= shield;
            target->buffs[BUFF_SHIELD] = 0;
        }
    }

    return damage;
}

// 第三段：执行真实扣血，通过toggle_is_alive同步存活状态
void CommitDamageAndCheck(int final_damage, Character* target) {
    target->hp -= final_damage;
    toggle_is_alive(target);
}

// ============================================================
//  行动结算（static，供ExecuteAction调用）
// ============================================================

// 对单个目标执行伤害/护盾/治疗/Buff结算
// 伤害、护盾和治疗仅对存活目标生效；阵亡角色不会被普通治疗复活
static void resolve_on_target(Card* played_card,
                              Character* attacker,
                              Character* target_char,
                              int target_player_id,
                              int target_slot_idx,
                              ResolutionReport* report) {
    // 伤害（仅存活目标）
    if (target_char->is_alive && played_card->base_damage > 0) {
        int hp_before = target_char->hp;
        int shield_before = target_char->buffs[BUFF_SHIELD];
        int raw = GetRawDamage(played_card, attacker);
        int multiplier = get_element_multiplier(raw, played_card->element, target_char);
        int boosted = raw * multiplier;
        int base_without_element_after_shield = raw - shield_before;
        int final_dmg;
        int shield_absorbed;
        int actual_damage;
        int base_actual_damage;
        if (base_without_element_after_shield < 0) base_without_element_after_shield = 0;
        final_dmg = CalculateMitigation(raw, played_card->element, target_char);
        CommitDamageAndCheck(final_dmg, target_char);
        shield_absorbed = boosted - final_dmg;
        if (shield_absorbed < 0) shield_absorbed = 0;
        actual_damage = hp_before - target_char->hp;
        if (actual_damage < 0) actual_damage = 0;
        base_actual_damage = base_without_element_after_shield;
        if (base_actual_damage > hp_before) base_actual_damage = hp_before;
        append_damage_report(report,
                             target_player_id,
                             target_slot_idx,
                             target_char,
                             raw,
                             actual_damage - base_actual_damage,
                             shield_absorbed,
                             actual_damage,
                             hp_before,
                             target_char->hp);
    }

    // 护盾（仅存活目标）
    if (target_char->is_alive && played_card->base_defense > 0) {
        target_char->buffs[BUFF_SHIELD] += played_card->base_defense;
    }

    // 治疗（仅对存活目标生效）
    if (target_char->is_alive && played_card->base_heal > 0) {
        target_char->hp += played_card->base_heal;
        if (target_char->hp > target_char->max_hp) target_char->hp = target_char->max_hp;
        toggle_is_alive(target_char);
    }

    // Buff/Debuff（仅存活目标，叠加回合数）
    if (target_char->is_alive && played_card->buff_effect != BUFF_NONE) {
        target_char->buffs[played_card->buff_effect] += played_card->buff_duration + 1;
    }
}

// 执行单个Action：切换角色 或 打出卡牌并完成数值结算
static void apply_action(Player* acting_player, Player* target_player, Action action, ResolutionReport* report) {
    // 切换出战角色
    if (action.type == ACTION_SWITCH_CHAR) {
        if (action.switch_to_idx >= 0 && action.switch_to_idx < TEAM_SIZE 
            && acting_player->team[action.switch_to_idx].is_alive) {
            acting_player->active_idx = action.switch_to_idx;
        }
        return;
    }

    // 打出卡牌
    if (action.type != ACTION_PLAY_CARD) return;
    if (action.card_hand_idx < 0 || action.card_hand_idx >= acting_player->hand_count) return;

    Card played_card = acting_player->hand[action.card_hand_idx];
    Character* active_char = &acting_player->team[acting_player->active_idx];

    if (acting_player->energy < played_card.energy_cost) return;

    // 扣除能量
    consume_energy(acting_player, played_card.energy_cost);

    // 卡牌元素：若为普通(0)则从出战角色继承，非零则为固定属性卡牌
    if (played_card.element == ELEMENT_NORMAL) {
        played_card.element = active_char->element;
    }
    // 攻击卡：若未指定Buff，则按卡牌最终属性赋予默认异常状态
    if (played_card.type == CARD_TYPE_ATTACK && played_card.buff_effect == BUFF_NONE) {
        played_card.buff_effect = get_element_default_buff(played_card.element);
    }

    // 根据卡牌目标类型分发结算
    if (played_card.target_type == TARGET_ENEMY_ALL || played_card.target_type == TARGET_SELF_ALL) {
        Player* aoe_team = (played_card.target_type == TARGET_ENEMY_ALL) ? target_player : acting_player;
        for (int i = 0; i < TEAM_SIZE; i++) {
            int target_player_id = (played_card.target_type == TARGET_ENEMY_ALL)
                                   ? target_player->player_id
                                   : acting_player->player_id;
            resolve_on_target(&played_card, active_char, &aoe_team->team[i],
                              target_player_id, i, report);
        }
    } else {
        Character* target_char = resolve_target(acting_player, target_player, action.target_idx, played_card.target_type);
        int target_slot_idx = action.target_idx;
        int target_player_id = (played_card.target_type == TARGET_SELF_SINGLE)
                               ? acting_player->player_id
                               : target_player->player_id;
        if (target_slot_idx < 0 || target_slot_idx >= TEAM_SIZE) {
            target_slot_idx = (played_card.target_type == TARGET_SELF_SINGLE)
                              ? acting_player->active_idx
                              : target_player->active_idx;
        }
        resolve_on_target(&played_card, active_char, target_char,
                          target_player_id, target_slot_idx, report);
    }

    // 出手后卡牌移入弃牌堆
    discard_from_hand(acting_player, action.card_hand_idx);
}

// ============================================================
//  回合结算接口（公有）
// ============================================================

// 执行单个行动：acting_player_id为1或2，引擎在循环中多次调用直到能量耗尽或主动结束
void ExecuteAction(GameState* state, Action* action, int acting_player_id) {
    ExecuteActionWithReport(state, action, acting_player_id, NULL);
}

void ExecuteActionWithReport(GameState* state, Action* action, int acting_player_id, ResolutionReport* report) {
    Player* acting = (acting_player_id == 1) ? &state->p1 : &state->p2;
    Player* target = (acting_player_id == 1) ? &state->p2 : &state->p1;
    if (report) {
        memset(report, 0, sizeof(*report));
    }
    apply_action(acting, target, *action, report);
}

// 回合结束清理：Buff-1、能量回满、手牌补满、阵亡出战自动切换
void EndTurn(GameState* state) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        decrement_buffs(&state->p1.team[i]);
        decrement_buffs(&state->p2.team[i]);
    }
    refill_energy(&state->p1);
    refill_energy(&state->p2);
    refill_hand(&state->p1);
    refill_hand(&state->p2);
    remove_dead_from_team(&state->p1, state->p1.active_idx);
    remove_dead_from_team(&state->p2, state->p2.active_idx);
}
