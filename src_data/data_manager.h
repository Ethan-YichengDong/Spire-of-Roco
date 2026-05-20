#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "../game_core.h"

#define MAX_GLOBAL_CARDS 100    // 全局最大卡牌种类容量
#define MAX_GLOBAL_CHARS 50     // 全局最大角色种类容量

// 全局卡牌数据数组及其规模
extern Card g_all_cards[MAX_GLOBAL_CARDS];
extern int g_card_count;

// 全局角色数据数组及其规模
extern Character g_all_characters[MAX_GLOBAL_CHARS];
extern int g_char_count;

// 从外部文件加载卡牌与角色的初始静态数据
void load_game_data(const char* cards_file, const char* chars_file);

// 根据卡牌的能量消耗对手牌进行分类排序
void sort_hand_by_energy(Player* player);

// 切换角色的存活状态
void toggle_is_alive(Character* character);

// 奖励机制：向玩家的卡组中添加指定ID的新卡片
void add_reward_to_deck(Player* player, int card_id);

// 战斗重置机制：初始化牌库
void init_deck(Player* p);

// 无中生有/抽牌骨架机制
void draw_card(Player* p, int amount);

#endif // DATA_MANAGER_H
