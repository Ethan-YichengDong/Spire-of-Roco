#include "data_manager.h"
#include <stdio.h>
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
