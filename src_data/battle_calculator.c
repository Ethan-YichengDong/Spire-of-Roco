#include "battle_calculator.h"
#include <stdio.h>

static void apply_action(Player* acting_player, Player* target_player, Action action) {
    if (action.type == ACTION_SWITCH_CHAR) {
        if (action.switch_to_idx >= 0 && action.switch_to_idx < TEAM_SIZE && acting_player->team[action.switch_to_idx].is_alive) {
            acting_player->active_idx = action.switch_to_idx;
        }
    } else if (action.type == ACTION_PLAY_CARD) {
        if (action.card_hand_idx >= 0 && action.card_hand_idx < acting_player->hand_count) {
            Card played_card = acting_player->hand[action.card_hand_idx];
            Character* target_char = &target_player->team[target_player->active_idx];

            if (acting_player->energy >= played_card.energy_cost) {
                acting_player->energy -= played_card.energy_cost;

                // Base damage
                int damage = played_card.base_damage;

                // Elemental multipliers
                if (played_card.element == ELEMENT_WATER && target_char->element == ELEMENT_FIRE) damage *= 2;
                else if (played_card.element == ELEMENT_FIRE && target_char->element == ELEMENT_GRASS) damage *= 2;
                else if (played_card.element == ELEMENT_GRASS && target_char->element == ELEMENT_WATER) damage *= 2;
                // Electric vs Wet
                if (played_card.element == ELEMENT_ELECTRIC && target_char->buffs[BUFF_WET] > 0) damage *= 2;

                // Apply buff
                if (played_card.buff_effect != BUFF_NONE) {
                    target_char->buffs[played_card.buff_effect] = played_card.buff_duration;
                }

                target_char->hp -= damage;
                if (target_char->hp <= 0) {
                    target_char->hp = 0;
                    target_char->is_alive = 0;
                }
            }

            // Remove card from hand
            for (int i = action.card_hand_idx; i < acting_player->hand_count - 1; i++) {
                acting_player->hand[i] = acting_player->hand[i + 1];
            }
            acting_player->hand_count--;
        }
    }
}

static void decrement_buffs(Character* c) {
    for (int i = 0; i < BUFF_COUNT; i++) {
        if (c->buffs[i] > 0) {
            c->buffs[i]--;
        }
    }
}

void ResolveTurn(GameState *state, Action a1, Action a2) {
    Character* c1 = &state->p1.team[state->p1.active_idx];
    Character* c2 = &state->p2.team[state->p2.active_idx];

    // Priority based on speed
    if (c1->speed >= c2->speed) {
        apply_action(&state->p1, &state->p2, a1);
        if (c2->is_alive) { // Might have died
            apply_action(&state->p2, &state->p1, a2);
        }
    } else {
        apply_action(&state->p2, &state->p1, a2);
        if (c1->is_alive) {
            apply_action(&state->p1, &state->p2, a1);
        }
    }

    // Decrement buffs
    for (int i = 0; i < TEAM_SIZE; i++) {
        decrement_buffs(&state->p1.team[i]);
        decrement_buffs(&state->p2.team[i]);
    }
}
