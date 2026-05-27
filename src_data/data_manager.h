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

// 抽牌机制（抽牌堆空时自动从弃牌堆洗入）
void draw_card(Player* p, int amount);

// 从全局卡池选取指定ID的卡牌加入玩家抽牌堆
void add_to_draw_pile(Player* p, int card_id);

// 对抽牌堆执行Fisher-Yates随机洗牌
void shuffle_draw_pile(Player* p);

// 将手牌中指定位置的卡牌移入弃牌堆
void discard_from_hand(Player* p, int hand_idx);

// 回合结束时从抽牌堆补满手牌至上限
void refill_hand(Player* p);

// 阵亡角色出战自动切换：若出战角色阵亡则移交至下一个存活角色，数据原地保留可复活
void remove_dead_from_team(Player* p, int team_idx);

// 消耗玩家能量并钳制至[0, max_energy]，防止越界
void consume_energy(Player* p, int amount);

// 恢复玩家能量并钳制至[0, max_energy]，防止越界
void restore_energy(Player* p, int amount);

// 每回合结束时将玩家能量恢复至上限
void refill_energy(Player* p);

#endif // DATA_MANAGER_H
