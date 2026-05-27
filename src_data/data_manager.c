#include "data_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===== 全局数据池 =====
Card g_all_cards[MAX_GLOBAL_CARDS];
int g_card_count = 0;
Character g_all_characters[MAX_GLOBAL_CHARS];
int g_char_count = 0;

// ============================================================
//  第一阶段：数据加载与初始化
// ============================================================

// 从CSV等配置文件中读取和解析卡牌和角色数据
void load_game_data(const char* cards_file, const char* chars_file) {
    FILE* fp = fopen(cards_file, "r");
    if (fp) {
        char line[256];
        g_card_count = 0;
        if (fgets(line, sizeof(line), fp)) {} // 跳过表头

        while (fgets(line, sizeof(line), fp) && g_card_count < MAX_GLOBAL_CARDS) {
            Card* c = &g_all_cards[g_card_count];
            sscanf(line, "%d,%31[^,],%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                   &c->card_id, c->name, (int*)&c->element, (int*)&c->type,
                   &c->energy_cost, &c->base_damage, &c->base_defense, &c->base_heal,
                   (int*)&c->buff_effect, &c->buff_value, &c->buff_duration,
                   (int*)&c->target_type);
            g_card_count++;
        }
        fclose(fp);
    }

    fp = fopen(chars_file, "r");
    if (fp) {
        char line[256];
        g_char_count = 0;
        if (fgets(line, sizeof(line), fp)) {} // 跳过表头

        while (fgets(line, sizeof(line), fp) && g_char_count < MAX_GLOBAL_CHARS) {
            Character* ch = &g_all_characters[g_char_count];
            sscanf(line, "%d,%31[^,],%d,%d,%d",
                   &ch->char_id, ch->name, (int*)&ch->element,
                   &ch->max_hp, &ch->speed);

            ch->hp = ch->max_hp;
            ch->is_alive = 1;
            for (int i = 0; i < BUFF_COUNT; i++) ch->buffs[i] = 0;
            g_char_count++;
        }
        fclose(fp);
    }
}

// 战斗重置：初始化抽牌堆、弃牌堆和手牌为空
void init_deck(Player* p) {
    p->draw_count = 0;
    p->discard_count = 0;
    p->hand_count = 0;
}

// ============================================================
//  第二阶段：牌组构筑
// ============================================================

// 从全局卡池选取指定ID的卡牌，复制一份加入玩家抽牌堆
void add_to_draw_pile(Player* p, int card_id) {
    if (p->draw_count >= MAX_DECK_SIZE) return;
    for (int i = 0; i < g_card_count; i++) {
        if (g_all_cards[i].card_id == card_id) {
            p->draw_pile[p->draw_count++] = g_all_cards[i];
            break;
        }
    }
}

// 对抽牌堆执行Fisher-Yates洗牌算法，打乱卡牌顺序
void shuffle_draw_pile(Player* p) {
    for (int i = p->draw_count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Card temp = p->draw_pile[i];
        p->draw_pile[i] = p->draw_pile[j];
        p->draw_pile[j] = temp;
    }
}

// 抽牌：从抽牌堆顶部逐张抽入手牌
// 抽牌堆耗尽时自动将弃牌堆全部移入抽牌堆并重新洗牌，实现循环牌库
void draw_card(Player* p, int amount) {
    for (int i = 0; i < amount; i++) {
        if (p->draw_count == 0) {
            if (p->discard_count == 0) break;
            for (int j = 0; j < p->discard_count; j++) {
                p->draw_pile[j] = p->discard_pile[j];
            }
            p->draw_count = p->discard_count;
            p->discard_count = 0;
            shuffle_draw_pile(p);
        }

        if (p->hand_count >= MAX_HAND_SIZE) break;
        p->hand[p->hand_count++] = p->draw_pile[--p->draw_count];
    }
}

// 将手牌中指定位置的卡牌移入弃牌堆，后续手牌前移填补空位
void discard_from_hand(Player* p, int hand_idx) {
    if (hand_idx < 0 || hand_idx >= p->hand_count) return;
    if (p->discard_count >= MAX_DECK_SIZE) return;

    p->discard_pile[p->discard_count++] = p->hand[hand_idx];

    for (int i = hand_idx; i < p->hand_count - 1; i++) {
        p->hand[i] = p->hand[i + 1];
    }
    p->hand_count--;
}

// ============================================================
//  第三阶段：手牌管理
// ============================================================

// 冒泡排序：将手牌按能量消耗升序排列（供GUI展示用）
void sort_hand_by_energy(Player* player) {
    for (int i = 0; i < player->hand_count - 1; i++) {
        for (int j = 0; j < player->hand_count - i - 1; j++) {
            if (player->hand[j].energy_cost > player->hand[j + 1].energy_cost) {
                Card temp = player->hand[j];
                player->hand[j] = player->hand[j + 1];
                player->hand[j + 1] = temp;
            }
        }
    }
}

// 回合结束时补满手牌：从抽牌堆抽牌直到手牌达到上限或无牌可抽
void refill_hand(Player* p) {
    int to_draw = MAX_HAND_SIZE - p->hand_count;
    if (to_draw > 0) {
        draw_card(p, to_draw);
    }
}

// 战后奖励：按ID从全局卡池查找卡牌并加入玩家手牌
void add_reward_to_deck(Player* player, int card_id) {
    if (player->hand_count < MAX_HAND_SIZE) {
        for (int i = 0; i < g_card_count; i++) {
            if (g_all_cards[i].card_id == card_id) {
                player->hand[player->hand_count++] = g_all_cards[i];
                break;
            }
        }
    }
}

// ============================================================
//  第四阶段：能量管理
// ============================================================

// 消耗能量：扣除指定数值后钳制到[0, max_energy]防止越界
void consume_energy(Player* p, int amount) {
    p->energy -= amount;
    if (p->energy < 0) p->energy = 0;
    if (p->energy > p->max_energy) p->energy = p->max_energy;
}

// 恢复能量：增加指定数值后钳制到[0, max_energy]防止溢出
void restore_energy(Player* p, int amount) {
    p->energy += amount;
    if (p->energy < 0) p->energy = 0;
    if (p->energy > p->max_energy) p->energy = p->max_energy;
}

// 每回合结束时将玩家能量恢复至上限
void refill_energy(Player* p) {
    p->energy = p->max_energy;
}

// ============================================================
//  第五阶段：角色状态管理
// ============================================================

// 根据HP同步角色的存活状态：hp<=0标记阵亡，hp>0标记存活
void toggle_is_alive(Character* character) {
    if (character->hp <= 0) {
        character->is_alive = 0;
        character->hp = 0;
    } else {
        character->is_alive = 1;
    }
}

// 出战角色阵亡时自动切换到队伍中下一个存活角色，数据原地保留可复活
void remove_dead_from_team(Player* p, int team_idx) {
    if (team_idx < 0 || team_idx >= TEAM_SIZE) return;
    if (p->team[team_idx].is_alive) return;

    if (p->active_idx != team_idx) return;

    for (int i = 0; i < TEAM_SIZE; i++) {
        if (p->team[i].is_alive) {
            p->active_idx = i;
            return;
        }
    }
    // 全队覆灭则保持原出战索引不变，由上层引擎判定游戏结束
}