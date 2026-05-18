#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "../game_core.h"

#define MAX_GLOBAL_CARDS 100
#define MAX_GLOBAL_CHARS 50

extern Card g_all_cards[MAX_GLOBAL_CARDS];
extern int g_card_count;

extern Character g_all_characters[MAX_GLOBAL_CHARS];
extern int g_char_count;

void load_game_data(const char* cards_file, const char* chars_file);
void sort_hand_by_energy(Player* player);
void toggle_is_alive(Character* character);
void add_reward_to_deck(Player* player, int card_id);

#endif // DATA_MANAGER_H
