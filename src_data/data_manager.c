#include "data_manager.h"
#include <stdio.h>
#include <string.h>

Card g_all_cards[MAX_GLOBAL_CARDS];
int g_card_count = 0;

Character g_all_characters[MAX_GLOBAL_CHARS];
int g_char_count = 0;

void load_game_data(const char* cards_file, const char* chars_file) {
    FILE* fp = fopen(cards_file, "r");
    if (fp) {
        char line[256];
        g_card_count = 0;
        // Skip header
        if (fgets(line, sizeof(line), fp)) {}
        while (fgets(line, sizeof(line), fp) && g_card_count < MAX_GLOBAL_CARDS) {
            Card* c = &g_all_cards[g_card_count];
            // Format: id,name,element,type,energy,damage,defense,buff_effect,buff_value,buff_duration
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
        // Skip header
        if (fgets(line, sizeof(line), fp)) {}
        while (fgets(line, sizeof(line), fp) && g_char_count < MAX_GLOBAL_CHARS) {
            Character* ch = &g_all_characters[g_char_count];
            // Format: id,name,element,max_hp,speed
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

void toggle_is_alive(Character* character) {
    if (character->hp <= 0) {
        character->is_alive = 0;
        character->hp = 0;
    } else {
        character->is_alive = 1;
    }
}

void add_reward_to_deck(Player* player, int card_id) {
    // deck array not inherently tracked in game_core.hPlayer, adding to hand instead if there is space.
    if (player->hand_count < MAX_HAND_SIZE) {
        for (int i = 0; i < g_card_count; i++) {
            if (g_all_cards[i].card_id == card_id) {
                player->hand[player->hand_count++] = g_all_cards[i];
                break;
            }
        }
    }
}
