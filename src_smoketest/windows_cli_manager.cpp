#include "../src_gui/gui_manager.h"
#include "../src_data/data_manager.h"
#include <stdio.h>
#include <stdlib.h>

void InitGUI() {
    printf("\n======================================================\n");
    printf("   Spire of Roco - Windows smoke test CLI mode\n");
    printf("======================================================\n");
}

void CloseGUI() {
    printf("======================================================\n");
    printf("   Automated smoke test finished.\n");
    printf("======================================================\n");
}

void RenderGameBoard(GameState state) {
    printf("\n------ [Current Board] ------\n");
    printf("Round: %d\n", state.round_count);

    Character p1_char = state.p1.team[state.p1.active_idx];
    Character p2_char = state.p2.team[state.p2.active_idx];

    printf("[Player 1] hand: %d | energy: %d\n", state.p1.hand_count, state.p1.energy);
    printf("Active character: %s (hp: %d/%d, speed: %d)\n",
           p1_char.name, p1_char.hp, p1_char.max_hp, p1_char.speed);

    printf("[Player 2] hand: %d | energy: %d\n", state.p2.hand_count, state.p2.energy);
    printf("Active character: %s (hp: %d/%d, speed: %d)\n",
           p2_char.name, p2_char.hp, p2_char.max_hp, p2_char.speed);
    printf("-----------------------------\n");
}

void ShowTurnTransitionMask(int player_id) {
    printf("\n>>> [Flow] Entering player %d turn. Smoke test auto-confirms transition.\n", player_id);
}

Action GetHumanInputFromUI(int player_id, GameState state) {
    Action act;
    act.actor_id = player_id;
    act.switch_to_idx = -1;
    act.target_idx = 0;

    Player* p = (player_id == 1) ? &state.p1 : &state.p2;

    printf("\n>>> [Auto Input] Player %d is choosing an action...\n", player_id);

    if (p->hand_count > 0 && p->energy >= p->hand[0].energy_cost) {
        act.type = ACTION_PLAY_CARD;
        act.card_hand_idx = 0;
        printf(" -> Playing hand card 0: %s (cost: %d)\n", p->hand[0].name, p->hand[0].energy_cost);
    } else {
        act.type = ACTION_END_TURN;
        act.card_hand_idx = -1;
        printf(" -> Not enough energy or no hand cards. Ending turn.\n");
    }

    return act;
}

int SelectCharacterFromUI(int player_id, int slot_number) {
    int p1_defaults[] = {0, 1, 2};
    int p2_defaults[] = {3, 0, 1};
    int safe_slot = slot_number;

    if (g_char_count <= 0) return 0;
    if (safe_slot < 0 || safe_slot >= TEAM_SIZE) safe_slot = 0;

    int selected = (player_id == 1) ? p1_defaults[safe_slot] : p2_defaults[safe_slot];
    selected %= g_char_count;
    printf("[CLI] Player %d auto-selected character slot %d index %d\n",
           player_id, slot_number, selected);
    return selected;
}

int SelectCardFromUI(int player_id, int current_deck_size) {
    if (current_deck_size >= 16) return -1;
    if (g_card_count <= 0) return -1;

    int selected = (current_deck_size / 2) % g_card_count;
    printf("[CLI] Player %d auto-selected deck card %d index %d\n",
           player_id, current_deck_size + 1, selected);
    return selected;
}

int GetModeSelectionFromUI() {
    const char* raw = getenv("ROCO_GAME_MODE");
    if (raw != NULL && raw[0] == '1') {
        printf("[CLI] Environment selected PvE mode.\n");
        return MODE_PVE;
    }

    printf("[CLI] Defaulting to PvP mode.\n");
    return MODE_PVP;
}
