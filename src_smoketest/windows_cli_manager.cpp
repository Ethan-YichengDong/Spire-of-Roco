#include "../src_gui/gui_manager.h"
#include "../src_data/data_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void ClearReturnToMenuRequest(void) {
}

int IsReturnToMenuRequested(void) {
    return 0;
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
    (void)player_id;
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

Action GetPlannedInputFromUI(int player_id, GameState state, const ActionRecord* records, int record_count, int* edit_index) {
    if (edit_index) *edit_index = -1;
    if (record_count > 0) {
        printf("[CLI] Player %d planned card records:\n", player_id);
        for (int i = 0; i < record_count; i++) {
            if (records[i].action.type != ACTION_PLAY_CARD) continue;
            printf("  %d. %s\n", i + 1, records[i].summary);
        }
    }
    return GetHumanInputFromUI(player_id, state);
}

void ShowResolutionStep(GameState state, const ActionRecord* record, const ResolutionReport* report, int step_number, int step_total) {
    printf("\n>>> [Resolution %d/%d] %s\n",
           step_number,
           step_total,
           record ? record->summary : "");
    if (report && report->event_count > 0) {
        for (int i = 0; i < report->event_count; i++) {
            const DamageResolutionEvent* event = &report->events[i];
            if (!event->has_damage) continue;
            printf("    %s damage=%d element_bonus=%d shield=%d hp=%d->%d\n",
                   event->target_name,
                   event->final_damage,
                   event->element_bonus_damage,
                   event->shield_absorbed,
                   event->hp_before,
                   event->hp_after);
        }
    }
    RenderGameBoard(state);
}

int ShowVictoryScreen(GameState state, int winner_id) {
    (void)state;
    printf("Congratulations! Player %d Wins!\n", winner_id);
    return 0;
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

int SelectMultipleCharactersFromUI(int player_id, const GameState* state, int max_select, int* out_indices, int* out_count) {
    (void)state;
    int p1_defaults[] = {0, 1, 2};
    int p2_defaults[] = {3, 0, 1};
    int count = 0;

    if (g_char_count <= 0) {
        if (out_count) *out_count = 0;
        return 0;
    }

    for (int i = 0; i < max_select && i < TEAM_SIZE; i++) {
        int selected = (player_id == 1) ? p1_defaults[i] : p2_defaults[i];
        out_indices[count++] = selected % g_char_count;
    }
    if (out_count) *out_count = count;
    printf("[CLI] Player %d auto-selected %d characters\n", player_id, count);
    return count;
}

int SelectCardFromUI(int player_id, int current_deck_size) {
    if (current_deck_size >= 16) return -1;
    if (g_card_count <= 0) return -1;

    int selected = (current_deck_size / 2) % g_card_count;
    printf("[CLI] Player %d auto-selected deck card %d index %d\n",
           player_id, current_deck_size + 1, selected);
    return selected;
}

int SelectMultipleCardsFromUI(int player_id, int max_select, int* out_indices, int* out_count) {
    int desired = max_select < 16 ? max_select : 16;
    int count = 0;

    if (g_card_count <= 0) {
        if (out_count) *out_count = 0;
        return 0;
    }

    for (int i = 0; i < desired; i++) {
        out_indices[count++] = (i / 2) % g_card_count;
    }
    if (out_count) *out_count = count;
    printf("[CLI] Player %d auto-selected %d deck cards\n", player_id, count);
    return -count;
}

int GetModeSelectionFromUI() {
    MenuSelection selection = ShowMainMenu();
    return selection == MENU_PVE ? MODE_PVE : MODE_PVP;
}

MenuSelection ShowMainMenu(void) {
    const char* raw = getenv("ROCO_GAME_MODE");
    if (raw != NULL && raw[0] == '1') {
        printf("[CLI] Environment selected PvE mode.\n");
        return MENU_PVE;
    }

    printf("[CLI] Defaulting to PvP mode.\n");
    return MENU_PVP;
}

AiPolicy ShowAIPolicyMenu(void) {
    const char* raw = getenv("ROCO_AI_POLICY");
    if (raw != NULL && strcmp(raw, "random") == 0) return AI_POLICY_RANDOM;
    if (raw != NULL && strcmp(raw, "hard") == 0) return AI_POLICY_HARD;
    if (raw != NULL && strcmp(raw, "llm") == 0) return AI_POLICY_LLM;
    return AI_POLICY_HEURISTIC;
}

int ShowCreditsScreenFromFile(const char* path) {
    printf("[CLI] Credits screen skipped: %s\n", path ? path : "docs/credits.txt");
    return 1;
}
