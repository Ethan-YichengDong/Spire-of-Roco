#ifndef GAME_CORE_H
#define GAME_CORE_H

#define MAX_NAME_LEN    32
#define TEAM_SIZE       3
#define MAX_HAND_SIZE   10
#define MAX_DECK_SIZE   30

typedef enum { ELEMENT_NORMAL = 0, ELEMENT_WATER, ELEMENT_FIRE, ELEMENT_GRASS, ELEMENT_ELECTRIC } ElementType;
typedef enum { CARD_TYPE_ATTACK = 0, CARD_TYPE_SKILL, CARD_TYPE_POWER } CardType;
typedef enum { BUFF_NONE = 0, BUFF_SHIELD, BUFF_WET, BUFF_BURN, BUFF_POISON, BUFF_POWER, BUFF_COUNT } BuffType;
typedef enum { ACTION_NONE = 0, ACTION_PLAY_CARD, ACTION_SWITCH_CHAR, ACTION_END_TURN } ActionType;

typedef struct {
    int card_id;
    char name[MAX_NAME_LEN];
    ElementType element;
    CardType type;
    int energy_cost;
    int base_damage;
    int base_defense;
    BuffType buff_effect;
    int buff_value;
    int buff_duration;
} Card;

typedef struct {
    int char_id;
    char name[MAX_NAME_LEN];
    ElementType element;
    int hp;
    int max_hp;
    int speed;
    int is_alive;
    int buffs[BUFF_COUNT];
} Character;

typedef struct {
    int player_id;
    char name[MAX_NAME_LEN];
    Character team[TEAM_SIZE];
    int active_idx;
    int energy;
    int max_energy;
    Card hand[MAX_HAND_SIZE];
    int hand_count;
} Player;

typedef struct {
    Player p1;
    Player p2;
    int round_count;
    int current_turn;
    int game_stage;
} GameState;

typedef struct {
    ActionType type;
    int actor_id;
    int card_hand_idx;
    int switch_to_idx;
} Action;

#endif // GAME_CORE_H
