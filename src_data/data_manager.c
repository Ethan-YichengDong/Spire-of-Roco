#include "data_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 全局卡牌数据数组及其计数
Card g_all_cards[MAX_GLOBAL_CARDS];
int g_card_count = 0;

// 全局角色数据数组及其计数
Character g_all_characters[MAX_GLOBAL_CHARS];
int g_char_count = 0;

// 从CSV等配置文件中读取和解析卡牌和角色数据
void load_game_data(const char* cards_file, const char* chars_file) {
    FILE* fp = fopen(cards_file, "r");
    if (fp) {
        char line[256];
        g_card_count = 0;
        // 跳过文件第一行的表头
        if (fgets(line, sizeof(line), fp)) {}
        
        // 逐行解析直到文件结束或达到最大卡牌数量限制
        while (fgets(line, sizeof(line), fp) && g_card_count < MAX_GLOBAL_CARDS) {
            Card* c = &g_all_cards[g_card_count];
            // 格式要求：ID,名称,元素属性,类型,消耗,基础伤害,基础防御护盾,增益效果类型,增益数值,增益持续时间
            sscanf(line, "%d,%31[^,],%d,%d,%d,%d,%d,%d,%d,%d",
                   &c->card_id, c->name, (int*)&c->element, (int*)&c->type,
                   &c->energy_cost, &c->base_damage, &c->base_defense,
                   (int*)&c->buff_effect, &c->buff_value, &c->buff_duration);
            g_card_count++;
        }
        fclose(fp);
    }

    fp = fopen(chars_file, "r");
    if (fp) {
        char line[256];
        g_char_count = 0;
        // 跳过文件第一行的表头
        if (fgets(line, sizeof(line), fp)) {}
        
        // 逐行解析直到文件结束或达到最大角色数量限制
        while (fgets(line, sizeof(line), fp) && g_char_count < MAX_GLOBAL_CHARS) {
            Character* ch = &g_all_characters[g_char_count];
            // 格式要求：ID,名称,元素属性,最大生命值,速度
            sscanf(line, "%d,%31[^,],%d,%d,%d",
                   &ch->char_id, ch->name, (int*)&ch->element,
                   &ch->max_hp, &ch->speed);
            
            // 初始化角色初始状态
            ch->hp = ch->max_hp;        // 当前血量设为最大值
            ch->is_alive = 1;           // 默认存活
            // 清理所有的Buff状态
            for (int i = 0; i < BUFF_COUNT; i++) ch->buffs[i] = 0;
            g_char_count++; //角色总数增加（初始为0）
        }
        fclose(fp);
    }
}

// 通过比较卡牌耗能，使用冒泡排序将玩家的手牌按照能量消耗升序排列
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

// 检查并更新角色的存活状态
void toggle_is_alive(Character* character) {
    if (character->hp <= 0) {
        character->is_alive = 0; // 标记阵亡
        character->hp = 0;       // 防止出现负数血量
    } else {
        character->is_alive = 1;
    }
}

// 向玩家添加卡牌作为战利品（当前临时加入至手牌）
void add_reward_to_deck(Player* player, int card_id) {
    // GameState Player 没有专门的额外卡牌数组，如果是手牌未满，直接塞给当前手牌
    if (player->hand_count < MAX_HAND_SIZE) {
        // 在全局卡池中匹配所需添加的卡牌ID
        for (int i = 0; i < g_card_count; i++) {
            if (g_all_cards[i].card_id == card_id) {
                player->hand[player->hand_count++] = g_all_cards[i];
                break;
            }
        }
    }
}

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

// 战斗重置机制：初始化抽牌堆和弃牌堆为空
void init_deck(Player* p) {
    p->draw_count = 0;
    p->discard_count = 0;
    p->hand_count = 0;
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

// 将手牌中指定位置的卡牌移入弃牌堆，手牌前移填补空位
void discard_from_hand(Player* p, int hand_idx) {
    if (hand_idx < 0 || hand_idx >= p->hand_count) return;
    if (p->discard_count >= MAX_DECK_SIZE) return;

    p->discard_pile[p->discard_count++] = p->hand[hand_idx];

    for (int i = hand_idx; i < p->hand_count - 1; i++) {
        p->hand[i] = p->hand[i + 1];
    }
    p->hand_count--;
}

// 执行抽牌操作：从抽牌堆顶部逐张抽入手牌
// 抽牌堆耗尽时自动将弃牌堆全部移入抽牌堆并重新洗牌，实现循环牌库
void draw_card(Player* p, int amount) {
    for (int i = 0; i < amount; i++) {
        // 抽牌堆空时：将弃牌堆所有卡牌移回抽牌堆并洗牌
        if (p->draw_count == 0) {
            if (p->discard_count == 0) break; // 无牌可抽

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

// 回合结束时补满手牌：从抽牌堆抽牌直到手牌达到上限或无牌可抽
void refill_hand(Player* p) {
    int to_draw = MAX_HAND_SIZE - p->hand_count;
    if (to_draw > 0) {
        draw_card(p, to_draw);
    }
}

// 阵亡角色出战自动切换：若出战角色阵亡，自动切换到队伍中下一个存活角色
// 角色数据原地保留不销毁，后续可通过治疗+复活逻辑恢复战场
void remove_dead_from_team(Player* p, int team_idx) {
    if (team_idx < 0 || team_idx >= TEAM_SIZE) return;
    if (p->team[team_idx].is_alive) return; // 角色仍存活，无需处理

    // 只有阵亡的是当前出战角色时才需要切换
    if (p->active_idx != team_idx) return;

    // 遍历队伍寻找下一个存活角色接替出战
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (p->team[i].is_alive) {
            p->active_idx = i;
            return;
        }
    }
    // 全队覆灭则保持原出战索引不变，由上层引擎判定游戏结束
}

// 消耗玩家能量：扣除指定数值后钳制到[0, max_energy]防止UI越界崩溃
void consume_energy(Player* p, int amount) {
    p->energy -= amount;
    if (p->energy < 0) p->energy = 0;
    if (p->energy > p->max_energy) p->energy = p->max_energy;
}

// 恢复玩家能量：增加指定数值后钳制到[0, max_energy]防止上限溢出
void restore_energy(Player* p, int amount) {
    p->energy += amount;
    if (p->energy < 0) p->energy = 0;
    if (p->energy > p->max_energy) p->energy = p->max_energy;
}

// 每回合结束时将玩家能量恢复至上限
void refill_energy(Player* p) {
    p->energy = p->max_energy;
}
