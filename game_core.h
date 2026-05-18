#ifndef GAME_CORE_H
#define GAME_CORE_H

// 基础限制常量定义
#define MAX_NAME_LEN    32      // 名字最大长度
#define TEAM_SIZE       3       // 每个队伍的角色数量
#define MAX_HAND_SIZE   10      // 最大手牌数量
#define MAX_DECK_SIZE   30      // 卡组最大容量

// 元素属性定义：普通、水、火、草、电
typedef enum { ELEMENT_NORMAL = 0, ELEMENT_WATER, ELEMENT_FIRE, ELEMENT_GRASS, ELEMENT_ELECTRIC } ElementType;
// 卡牌类型定义：攻击卡、技能卡、能力卡
typedef enum { CARD_TYPE_ATTACK = 0, CARD_TYPE_SKILL, CARD_TYPE_POWER } CardType;
// 状态/增益效果定义：无、护盾、潮湿、燃烧、中毒、力量Buff
typedef enum { BUFF_NONE = 0, BUFF_SHIELD, BUFF_WET, BUFF_BURN, BUFF_POISON, BUFF_POWER, BUFF_COUNT } BuffType;
// 行动类型定义：无、出牌、切换角色、结束回合
typedef enum { ACTION_NONE = 0, ACTION_PLAY_CARD, ACTION_SWITCH_CHAR, ACTION_END_TURN } ActionType;

// 卡牌结构体定义
typedef struct {
    int card_id;                // 卡牌ID
    char name[MAX_NAME_LEN];    // 卡牌名称
    ElementType element;        // 元素属性
    CardType type;              // 卡牌类型
    int energy_cost;            // 能量消耗
    int base_damage;            // 基础伤害
    int base_defense;           // 基础防御/护盾值
    BuffType buff_effect;       // 施加的Buff类型
    int buff_value;             // Buff数值
    int buff_duration;          // Buff持续回合数
} Card;

// 角色结构体定义
typedef struct {
    int char_id;                // 角色ID
    char name[MAX_NAME_LEN];    // 角色名称
    ElementType element;        // 角色元素属性
    int hp;                     // 当前生命值
    int max_hp;                 // 最大生命值
    int speed;                  // 速度（决定行动顺序）
    int is_alive;               // 是否存活状态标志
    int buffs[BUFF_COUNT];      // 当前持有的每种Buff的层数/持续时间
} Character;

// 玩家/队伍结构体定义
typedef struct {
    int player_id;              // 玩家ID
    char name[MAX_NAME_LEN];    // 玩家名
    Character team[TEAM_SIZE];  // 玩家的角色队伍
    int active_idx;             // 当前出战角色的索引
    int energy;                 // 当前能量值
    int max_energy;             // 最大能量值上限
    Card hand[MAX_HAND_SIZE];   // 手牌列表
    int hand_count;             // 当前手牌数量
} Player;

// 游戏全局状态结构体定义
typedef struct {
    Player p1;                  // 玩家1
    Player p2;                  // 玩家2
    int round_count;            // 回合数计数器
    int current_turn;           // 当前轮到哪位玩家行动的出牌权（1或2）
    int game_stage;             // 游戏阶段标志
} GameState;

// 行动指令结构体定义
typedef struct {
    ActionType type;            // 执行的行动类型
    int actor_id;               // 行动发起者ID
    int card_hand_idx;          // 当打出卡牌时使用，记录出手牌列表索引
    int switch_to_idx;          // 当切换角色时使用，记录目标角色索引
} Action;

#endif // GAME_CORE_H
