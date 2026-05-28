#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "../game_core.h"

#define MAX_GLOBAL_CARDS 100    // 全局最大卡牌种类容量
#define MAX_GLOBAL_CHARS 50     // 全局最大角色种类容量

// ===== 全局数据池 =====
extern Card g_all_cards[MAX_GLOBAL_CARDS];
extern int g_card_count;
extern Character g_all_characters[MAX_GLOBAL_CHARS];
extern int g_char_count;

// ===== 第一阶段：数据加载与初始化 =====
// 从外部文件加载卡牌与角色的初始静态数据
void load_game_data(const char* cards_file, const char* chars_file);
// 战斗重置：清空抽牌堆、弃牌堆和手牌
void init_deck(Player* p);
// 从全局角色池按ID选取角色克隆到玩家队伍指定位置
void assign_character_to_team(Player* p, int char_id, int team_slot);

// ===== 第二阶段：牌组构筑 =====
// 从全局卡池按ID列表批量选牌加入抽牌堆，返回成功加入张数
int build_draw_pile(Player* p, int* card_ids, int count);
// 对抽牌堆执行Fisher-Yates随机洗牌
void shuffle_draw_pile(Player* p);
// 抽牌：从抽牌堆顶部逐张抽入手牌（抽牌堆空时自动将弃牌堆洗入）
void draw_card(Player* p, int amount);
// 将手牌中指定位置的卡牌移入弃牌堆
void discard_from_hand(Player* p, int hand_idx);

// ===== 第三阶段：手牌管理 =====
// 根据卡牌能量消耗对手牌升序排列（供GUI展示用）
void sort_hand_by_energy(Player* player);
// 回合结束时从抽牌堆补满手牌至上限
void refill_hand(Player* p);
// 战后奖励：向玩家手牌中添加指定ID的新卡牌
void add_reward_to_deck(Player* player, int card_id);

// ===== 第四阶段：能量管理 =====
// 消耗能量并钳制至[0, max_energy]
void consume_energy(Player* p, int amount);
// 恢复能量并钳制至[0, max_energy]
void restore_energy(Player* p, int amount);
// 每回合结束时将能量恢复至上限
void refill_energy(Player* p);

// ===== 第五阶段：角色状态管理 =====
// 根据HP同步角色的存活状态（hp<=0则标记阵亡，hp>0则标记存活）
void toggle_is_alive(Character* character);
// 出战角色阵亡时自动切换到队伍中下一个存活角色
void remove_dead_from_team(Player* p, int team_idx);

#endif // DATA_MANAGER_H