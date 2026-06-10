#include "game_engine.h"
#include "../src_gui/gui_manager.h"
#include "../src_data/battle_calculator.h"
#include "../src_data/data_manager.h"
#include "../src_ai/ai_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <direct.h>

static FILE* g_battle_log = NULL;

static const char* ElementName(ElementType element) {
    switch (element) {
        case ELEMENT_NORMAL: return "Normal";
        case ELEMENT_WATER: return "Water";
        case ELEMENT_FIRE: return "Fire";
        case ELEMENT_GRASS: return "Grass";
        case ELEMENT_ELECTRIC: return "Electric";
        default: return "Unknown";
    }
}

static const char* ActionName(ActionType type) {
    switch (type) {
        case ACTION_NONE: return "None";
        case ACTION_PLAY_CARD: return "PlayCard";
        case ACTION_SWITCH_CHAR: return "SwitchChar";
        case ACTION_END_TURN: return "EndTurn";
        default: return "Unknown";
    }
}

static void EnsureLogDirExists(void) {
    _mkdir("logs");
}

static void OpenBattleLog(void) {
    EnsureLogDirExists();

    time_t now = time(NULL);
    struct tm* local_time = localtime(&now);
    char path[128];

    if (local_time != NULL) {
        strftime(path, sizeof(path), "logs/battle_%Y%m%d_%H%M%S.log", local_time);
    } else {
        snprintf(path, sizeof(path), "logs/battle_unknown_time.log");
    }

    g_battle_log = fopen(path, "w");
    if (g_battle_log == NULL) {
        printf("[LOG] Failed to create battle log file: %s\n", path);
        return;
    }

    fprintf(g_battle_log, "Spire of Roco Battle Log\n");
    fprintf(g_battle_log, "log_file=%s\n\n", path);
    fflush(g_battle_log);
    printf("[LOG] Battle log written to: %s\n", path);
}

static void CloseBattleLog(void) {
    if (g_battle_log == NULL) return;
    fprintf(g_battle_log, "\n[LogClosed]\n");
    fclose(g_battle_log);
    g_battle_log = NULL;
}

static void LogCharacter(int slot, const Character* ch, int is_active) {
    if (g_battle_log == NULL) return;

    fprintf(g_battle_log,
            "    slot=%d%s id=%d name=%s element=%s hp=%d/%d speed=%d alive=%d buffs=[",
            slot, is_active ? " active" : "", ch->char_id, ch->name,
            ElementName(ch->element), ch->hp, ch->max_hp, ch->speed, ch->is_alive);
    for (int i = 0; i < BUFF_COUNT; i++) {
        if (i > 0) fprintf(g_battle_log, ",");
        fprintf(g_battle_log, "%d", ch->buffs[i]);
    }
    fprintf(g_battle_log, "]\n");
}

static void LogPlayerState(const char* label, const Player* player) {
    if (g_battle_log == NULL) return;

    fprintf(g_battle_log,
            "  [%s] player_id=%d active_idx=%d energy=%d/%d hand=%d draw=%d discard=%d\n",
            label, player->player_id, player->active_idx,
            player->energy, player->max_energy,
            player->hand_count, player->draw_count, player->discard_count);

    for (int i = 0; i < TEAM_SIZE; i++) {
        LogCharacter(i, &player->team[i], i == player->active_idx);
    }

    fprintf(g_battle_log, "    hand_cards=");
    for (int i = 0; i < player->hand_count; i++) {
        if (i > 0) fprintf(g_battle_log, " | ");
        fprintf(g_battle_log, "#%d:%s(cost=%d,type=%d,target=%d)",
                i, player->hand[i].name, player->hand[i].energy_cost,
                player->hand[i].type, player->hand[i].target_type);
    }
    fprintf(g_battle_log, "\n");
}

static void LogGameState(const char* title, const GameState* state) {
    if (g_battle_log == NULL) return;

    fprintf(g_battle_log, "\n[%s]\n", title);
    fprintf(g_battle_log, "round=%d current_turn=%d stage=%d scene=%d\n",
            state->round_count, state->current_turn,
            state->game_stage, state->current_scene);
    LogPlayerState("P1", &state->p1);
    LogPlayerState("P2", &state->p2);
    fflush(g_battle_log);
}

static void LogAction(const GameState* state, Action action) {
    if (g_battle_log == NULL) return;

    const Player* actor = (action.actor_id == 1) ? &state->p1 : &state->p2;
    fprintf(g_battle_log,
            "  P%d action=%s card_hand_idx=%d switch_to_idx=%d target_idx=%d",
            action.actor_id, ActionName(action.type),
            action.card_hand_idx, action.switch_to_idx, action.target_idx);

    if (action.type == ACTION_PLAY_CARD &&
        action.card_hand_idx >= 0 &&
        action.card_hand_idx < actor->hand_count) {
        const Card* card = &actor->hand[action.card_hand_idx];
        fprintf(g_battle_log,
                " card=%s cost=%d damage=%d defense=%d heal=%d target_type=%d",
                card->name, card->energy_cost, card->base_damage,
                card->base_defense, card->base_heal, card->target_type);
    }

    fprintf(g_battle_log, "\n");
    fflush(g_battle_log);
}

static int ReadEnvInt(const char* name, int default_value, int min_value, int max_value) {
    const char* raw = getenv(name);
    if (raw == NULL || raw[0] == '\0') return default_value;

    int value = atoi(raw);
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int PlayerHasAliveCharacter(const Player* player) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (player->team[i].is_alive) return 1;
    }
    return 0;
}

static Action NormalizeActionForCurrentState(GameState* state, Action action, int actor_id) {
    Player* acting = (actor_id == 1) ? &state->p1 : &state->p2;
    Player* target = (actor_id == 1) ? &state->p2 : &state->p1;

    action.actor_id = actor_id;

    if (action.type == ACTION_SWITCH_CHAR) {
        if (action.switch_to_idx < 0 || action.switch_to_idx >= TEAM_SIZE ||
            !acting->team[action.switch_to_idx].is_alive) {
            action.type = ACTION_END_TURN;
        }
        return action;
    }

    if (action.type != ACTION_PLAY_CARD) {
        return action;
    }

    if (action.card_hand_idx < 0 || action.card_hand_idx >= acting->hand_count) {
        action.type = ACTION_END_TURN;
        return action;
    }

    Card* card = &acting->hand[action.card_hand_idx];
    if (card->target_type == TARGET_ENEMY_ALL) {
        action.target_idx = -1;
    } else if (card->target_type == TARGET_SELF_ALL) {
        action.target_idx = -2;
    } else if (card->target_type == TARGET_SELF_SINGLE) {
        if (action.target_idx >= 10 && action.target_idx < 10 + TEAM_SIZE) {
            action.target_idx -= 10;
        }
        if (action.target_idx < 0 || action.target_idx >= TEAM_SIZE ||
            !acting->team[action.target_idx].is_alive) {
            action.target_idx = acting->active_idx;
        }
    } else {
        if (action.target_idx < 0 || action.target_idx >= TEAM_SIZE ||
            !target->team[action.target_idx].is_alive) {
            action.target_idx = target->active_idx;
        }
    }

    return action;
}

// 鑾峰彇鐜╁2鐨勮鍔紝鍩轰簬閫夋嫨鐨勬父鎴忔ā寮?
Action GetPlayer2Action(GameState state, int mode) {
    if (mode == MODE_PVP) {
        // 鏈湴 PvP 妯″紡锛氭帴鏀剁帺瀹?鐨勪汉宸ヨ緭鍏?
        return GetHumanInputFromUI(2, state);
    } else {
        // MODE_PVE锛氶€氳繃 Socket 妗ヨ繛璋冪敤 AI 鍚庡彴鏉ョ敓鎴愯鍔?
        return GetAIActionFromBackend(state, 2);
    }
}

// 鍥炲悎鍐呭惊鐜細鍏佽鍚屼竴鐜╁杩炵画鍑虹墝锛岀洿鍒拌兘閲忚€楀敖鎴栦富鍔ㄧ粨鏉熷洖鍚?
#if 0
static void PlayerTurnLoop(GameState* state, int player_id, int mode) {
    Player* player = (player_id == 1) ? &state->p1 : &state->p2;

    while (player->hand_count > 0) {
        // 鑻ュ繁鏂瑰嚭鎴樿鑹查樀浜★紝鑷姩鍒囨崲鍒伴槦浼嶄腑涓嬩竴涓瓨娲昏鑹?
        if (!player->team[player->active_idx].is_alive) {
            remove_dead_from_team(player, player->active_idx);
            if (!player->team[player->active_idx].is_alive) break;
        }

        // 妫€鏌ユ槸鍚﹁繕鏈夎冻澶熺殑鑳介噺鎵撳嚭浠绘剰涓€寮犳墜鐗?
        int can_act = 0;
        for (int i = 0; i < player->hand_count; i++) {
            if (player->energy >= player->hand[i].energy_cost) {
                can_act = 1;
                break;
            }
        }
        if (!can_act) break;

        RenderGameBoard(*state);

        Action action;
        if (player_id == 1) {
            action = GetHumanInputFromUI(player_id, *state);
        } else {
            action = GetPlayer2Action(*state, mode);
        }

        action = NormalizeActionForCurrentState(state, action, player_id);
        if (action.type == ACTION_END_TURN) break;

        if (g_battle_log != NULL) {
            fprintf(g_battle_log, "\n[TurnAction]\n");
            LogAction(state, action);
        }

        ExecuteAction(state, &action, player_id);
        LogGameState("AfterAction", state);
    }
}
#endif

static int HasExplicitSingleTarget(GameState* state, Action action, int player_id) {
    Player* acting = (player_id == 1) ? &state->p1 : &state->p2;

    if (action.type != ACTION_PLAY_CARD) return 1;
    if (action.card_hand_idx < 0 || action.card_hand_idx >= acting->hand_count) return 0;

    Card* card = &acting->hand[action.card_hand_idx];
    if (card->target_type != TARGET_ENEMY_SINGLE && card->target_type != TARGET_SELF_SINGLE) {
        return 1;
    }

    if (card->target_type == TARGET_SELF_SINGLE &&
        action.target_idx >= 10 &&
        action.target_idx < 10 + TEAM_SIZE) {
        return 1;
    }

    return action.target_idx >= 0 && action.target_idx < TEAM_SIZE;
}

static int IsCompletePlannedAction(GameState* state, Action action, int player_id) {
    Player* acting = (player_id == 1) ? &state->p1 : &state->p2;

    if (action.type == ACTION_SWITCH_CHAR) {
        return action.switch_to_idx >= 0 &&
               action.switch_to_idx < TEAM_SIZE &&
               acting->team[action.switch_to_idx].is_alive;
    }

    if (action.type != ACTION_PLAY_CARD) return 1;
    if (action.card_hand_idx < 0 || action.card_hand_idx >= acting->hand_count) return 0;
    if (acting->energy < acting->hand[action.card_hand_idx].energy_cost) return 0;
    return HasExplicitSingleTarget(state, action, player_id);
}

static void BuildActionSummary(GameState* state, Action action, int player_id, char* out, size_t out_size) {
    Player* acting = (player_id == 1) ? &state->p1 : &state->p2;
    Player* enemy = (player_id == 1) ? &state->p2 : &state->p1;

    if (action.type == ACTION_PLAY_CARD &&
        action.card_hand_idx >= 0 &&
        action.card_hand_idx < acting->hand_count) {
        Card* card = &acting->hand[action.card_hand_idx];
        char target_name[64];
        if (card->target_type == TARGET_ENEMY_ALL) {
            snprintf(target_name, sizeof(target_name), "all enemies");
        } else if (card->target_type == TARGET_SELF_ALL) {
            snprintf(target_name, sizeof(target_name), "all allies");
        } else if (card->target_type == TARGET_SELF_SINGLE) {
            int idx = action.target_idx;
            if (idx >= 10 && idx < 10 + TEAM_SIZE) idx -= 10;
            if (idx < 0 || idx >= TEAM_SIZE) idx = acting->active_idx;
            snprintf(target_name, sizeof(target_name), "%s", acting->team[idx].name);
        } else {
            int idx = action.target_idx;
            if (idx < 0 || idx >= TEAM_SIZE) idx = enemy->active_idx;
            snprintf(target_name, sizeof(target_name), "%s", enemy->team[idx].name);
        }
        snprintf(out, out_size, "P%d Play %s -> %s (cost %d)",
                 player_id, card->name, target_name, card->energy_cost);
        return;
    }

    if (action.type == ACTION_SWITCH_CHAR &&
        action.switch_to_idx >= 0 &&
        action.switch_to_idx < TEAM_SIZE) {
        snprintf(out, out_size, "P%d Switch to %s", player_id, acting->team[action.switch_to_idx].name);
        return;
    }

    snprintf(out, out_size, "P%d %s", player_id, ActionName(action.type));
}

static void ReplayPlannedPrefix(GameState* planning, const GameState* round_start, ActionRecord* records, int record_count) {
    *planning = *round_start;
    for (int i = 0; i < record_count; i++) {
        Action action = records[i].action;
        action = NormalizeActionForCurrentState(planning, action, records[i].player_id);
        if (action.type != ACTION_END_TURN && action.type != ACTION_NONE) {
            ExecuteAction(planning, &action, records[i].player_id);
        }
    }
}

static void CollectPlayerPlan(GameState* round_start, int player_id, int mode, ActionRecord* out_records, int* out_count) {
    GameState planning;
    int count = 0;
    ReplayPlannedPrefix(&planning, round_start, out_records, 0);

    while (count < MAX_TURN_ACTIONS) {
        if (IsReturnToMenuRequested()) break;
        Player* player = (player_id == 1) ? &planning.p1 : &planning.p2;
        if (!PlayerHasAliveCharacter(player)) break;
        if (!player->team[player->active_idx].is_alive) {
            remove_dead_from_team(player, player->active_idx);
        }

        RenderGameBoard(planning);

        int edit_index = -1;
        Action action;
        if (player_id == 2 && mode == MODE_PVE) {
            action = GetAIActionFromBackend(planning, 2);
        } else {
            action = GetPlannedInputFromUI(player_id, planning, out_records, count, &edit_index);
        }
        if (IsReturnToMenuRequested()) break;

        if (action.type == ACTION_EDIT_STEP) {
            if (edit_index >= 0 && edit_index < count) {
                count = edit_index;
                ReplayPlannedPrefix(&planning, round_start, out_records, count);
            }
            continue;
        }

        if (!IsCompletePlannedAction(&planning, action, player_id)) {
            printf("[Engine] Ignored incomplete planned action from P%d.\n", player_id);
            continue;
        }

        action = NormalizeActionForCurrentState(&planning, action, player_id);
        if (action.type == ACTION_END_TURN || action.type == ACTION_NONE) break;

        out_records[count].action = action;
        out_records[count].player_id = player_id;
        memset(&out_records[count].played_card, 0, sizeof(out_records[count].played_card));
        out_records[count].has_played_card = 0;
        if (action.type == ACTION_PLAY_CARD &&
            action.card_hand_idx >= 0 &&
            action.card_hand_idx < player->hand_count) {
            out_records[count].played_card = player->hand[action.card_hand_idx];
            out_records[count].has_played_card = 1;
        }
        BuildActionSummary(&planning, action, player_id,
                           out_records[count].summary,
                           sizeof(out_records[count].summary));

        ExecuteAction(&planning, &action, player_id);
        count++;
    }

    *out_count = count;
}

static int ResolveOneRecord(GameState* state, ActionRecord* record, int step_number, int step_total) {
    Player* acting = (record->player_id == 1) ? &state->p1 : &state->p2;
    if (!PlayerHasAliveCharacter(acting)) return 0;
    if (!acting->team[acting->active_idx].is_alive) {
        remove_dead_from_team(acting, acting->active_idx);
    }
    if (!PlayerHasAliveCharacter(acting)) return 0;

    if (!HasExplicitSingleTarget(state, record->action, record->player_id)) {
        printf("[Engine] Skipped incomplete recorded card action from P%d.\n", record->player_id);
        return 0;
    }

    Action action = NormalizeActionForCurrentState(state, record->action, record->player_id);
    if (action.type == ACTION_END_TURN || action.type == ACTION_NONE) return 0;

    if (g_battle_log != NULL) {
        fprintf(g_battle_log, "\n[ResolvedAction]\n");
        LogAction(state, action);
    }

    ResolutionReport report;
    ExecuteActionWithReport(state, &action, record->player_id, &report);
    LogGameState("AfterResolvedAction", state);
    if (action.type == ACTION_PLAY_CARD) {
        ShowResolutionStep(*state, record, &report, step_number, step_total);
        return 1;
    }
    return 0;
}

static int CountCardRecords(const ActionRecord* records, int record_count) {
    int count = 0;
    for (int i = 0; i < record_count; i++) {
        if (records[i].action.type == ACTION_PLAY_CARD) count++;
    }
    return count;
}

static void ResolvePlannedRound(GameState* state,
                                int first_player_id,
                                ActionRecord* p1_records,
                                int p1_count,
                                ActionRecord* p2_records,
                                int p2_count) {
    ActionRecord* first_records = (first_player_id == 1) ? p1_records : p2_records;
    ActionRecord* second_records = (first_player_id == 1) ? p2_records : p1_records;
    int first_count = (first_player_id == 1) ? p1_count : p2_count;
    int second_count = (first_player_id == 1) ? p2_count : p1_count;
    int second_player_id = (first_player_id == 1) ? 2 : 1;
    int total_steps = CountCardRecords(first_records, first_count) + CountCardRecords(second_records, second_count);
    int step = 1;

    for (int i = 0; i < first_count; i++) {
        if (ResolveOneRecord(state, &first_records[i], step, total_steps)) step++;
    }

    Player* second_player = (second_player_id == 1) ? &state->p1 : &state->p2;
    if (!PlayerHasAliveCharacter(second_player)) {
        return;
    }

    for (int i = 0; i < second_count; i++) {
        if (ResolveOneRecord(state, &second_records[i], step, total_steps)) step++;
        if (!PlayerHasAliveCharacter(second_player)) break;
    }
}

// ============================================================
//  閫夎闃舵锛氫负鎸囧畾鐜╁濉厖3涓鑹诧紙涓嶉噸澶嶏級
// ============================================================
static int SelectTeamPhase(Player* player, int player_id, int mode, GameState* state) {
    int slot = 0;
    while (slot < TEAM_SIZE) {
        if (IsReturnToMenuRequested()) return 1;
        int picks[TEAM_SIZE]; int pick_count = 0;
        if (player_id == 2 && mode == MODE_PVE) {
            // PvE mode: AI fills remaining slots
            for (; slot < TEAM_SIZE; slot++) {
                int char_idx = (slot < g_char_count) ? slot : 0;
                printf("[Engine] PvE: AI selects character %s\n", g_all_characters[char_idx].name);
                assign_character_to_team(player, g_all_characters[char_idx].char_id, slot);
            }
            break;
        } else {
            // Allow player to select multiple characters at once
            pick_count = SelectMultipleCharactersFromUI(player_id, state, TEAM_SIZE - slot, picks, &pick_count);
            if (IsReturnToMenuRequested()) return 1;
        }

        if (pick_count <= 0) {
            // no picks, let user try again
            continue;
        }

        for (int pi = 0; pi < pick_count && slot < TEAM_SIZE; pi++) {
            int char_idx = picks[pi];
            if (char_idx < 0 || char_idx >= g_char_count) {
                printf("[Engine] invalid character index, selecting 0\n"); char_idx = 0;
            }
            int duplicate = 0;
            for (int prev = 0; prev < slot; prev++) {
                if (player->team[prev].char_id == g_all_characters[char_idx].char_id) { duplicate = 1; break; }
            }
            if (duplicate) {
                printf("[Engine] duplicate selection %s, skipped\n", g_all_characters[char_idx].name);
                continue;
            }
            assign_character_to_team(player, g_all_characters[char_idx].char_id, slot);
            slot++;
        }

        RenderGameBoard(*state);
        if (IsReturnToMenuRequested()) return 1;
    }

    player->active_idx = 0;
    return 0;
}

// ============================================================
//  閫夌墝闃舵锛氫负鎸囧畾鐜╁閫愬紶鏋勭瓚鐗屽簱
// ============================================================
static int BuildDeckPhase(Player* player, int player_id, int mode) {
    while (player->draw_count < MAX_DECK_SIZE) {
        if (IsReturnToMenuRequested()) return 1;
        int picks[MAX_DECK_SIZE]; int pick_count = 0;
        int finalize = 0;
        if (player_id == 2 && mode == MODE_PVE) {
            // PvE: AI selects remaining cards (placeholder logic)
            if (player->draw_count >= 16) break;
            int card_idx = (player->draw_count / 2) % g_card_count;
            printf("[Engine] PvE: AI selects card %s\n", g_all_cards[card_idx].name);
            player->draw_pile[player->draw_count++] = g_all_cards[card_idx];
            continue;
        } else {
            // Let player pick multiple cards at once
            int raw_ret = SelectMultipleCardsFromUI(player_id, MAX_DECK_SIZE - player->draw_count, picks, &pick_count);
            if (IsReturnToMenuRequested()) return 1;
            if (raw_ret < 0) { finalize = 1; pick_count = -raw_ret; }
            else pick_count = raw_ret;
        }

        if (pick_count <= 0) {
            if (player->draw_count == 0) {
                // Prevent finishing with an empty deck; prompt user to pick at least one card
                printf("[Engine] Deck cannot be empty. Please select at least one card.\n");
                continue;
            } else {
                break;
            }
        }
        for (int pi = 0; pi < pick_count && player->draw_count < MAX_DECK_SIZE; pi++) {
            int card_idx = picks[pi];
            if (card_idx < 0 || card_idx >= g_card_count) { printf("[Engine] invalid card index, skipped\n"); continue; }
            player->draw_pile[player->draw_count++] = g_all_cards[card_idx];
        }
        if (finalize) break;
    }

    printf("Player %d deck contains %d cards.\n", player_id, player->draw_count);
    shuffle_draw_pile(player);
    return 0;
}

// 鎵ц骞剁鐞嗘父鎴忔牳蹇冨惊鐜?
static int RunBattleLoop(int mode) {
    GameState state;

    ClearReturnToMenuRequest();
    memset(&state, 0, sizeof(GameState));
    srand((unsigned int)time(NULL));

    state.round_count = 1;
    state.current_turn = 1;
    state.p1.player_id = 1;
    state.p2.player_id = 2;
    strcpy(state.p1.name, "Player 1");
    strcpy(state.p2.name, "Player 2");
    state.p1.max_energy = 3;
    state.p2.max_energy = 3;
    init_deck(&state.p1);
    init_deck(&state.p2);

    // ================================================================
    //  鍦烘櫙涓€锛歋CENE_MENU 鈥?涓昏彍鍗?
    // ================================================================
    OpenBattleLog();
    LogGameState("InitialState", &state);

    int max_rounds = ReadEnvInt("ROCO_SMOKE_MAX_ROUNDS", 0, 0, 10000);
    if (max_rounds > 0) {
        printf("[Engine] Smoke-test max rounds: %d\n", max_rounds);
    }
    printf("Game mode: %s\n", (mode == MODE_PVP) ? "Local PvP" : "PvE");

    // ================================================================
    //  鍦烘櫙浜岋細SCENE_DRAFT 鈥?闃熶紞閫夋嫨涓庣墝搴撴瀯绛?
    // ================================================================
    state.current_scene = SCENE_DRAFT;
    state.game_stage = 1;
    RenderGameBoard(state);

    printf("\n===== Player 1 Team Selection =====\n");
    state.current_turn = 1;
    if (SelectTeamPhase(&state.p1, 1, mode, &state)) {
        CloseBattleLog();
        return 1;
    }

    printf("\n===== Player 2 Team Selection =====\n");
    state.current_turn = 2;
    if (SelectTeamPhase(&state.p2, 2, mode, &state)) {
        CloseBattleLog();
        return 1;
    }

    printf("\n===== Player 1 Deck Build =====\n");
    state.current_turn = 1;
    if (BuildDeckPhase(&state.p1, 1, mode)) {
        CloseBattleLog();
        return 1;
    }

    printf("\n===== Player 2 Deck Build =====\n");
    state.current_turn = 2;
    if (BuildDeckPhase(&state.p2, 2, mode)) {
        CloseBattleLog();
        return 1;
    }

    if (state.p1.draw_count == 0 || state.p2.draw_count == 0) {
        printf("[Engine] Error: decks cannot be empty. Aborting game.\n");
        CloseBattleLog();
        return 0;
    }

    draw_card(&state.p1, MAX_HAND_SIZE);
    draw_card(&state.p2, MAX_HAND_SIZE);
    state.p1.energy = state.p1.max_energy;
    state.p2.energy = state.p2.max_energy;
    LogGameState("AfterDraft", &state);

    // ================================================================
    //  鍦烘櫙涓夛細SCENE_BATTLE 鈥?鎴樻枟
    // ================================================================
    state.current_scene = SCENE_BATTLE;
    state.game_stage = 2;
    RenderGameBoard(state);

    int p1_first = (rand() % 2 == 0);

    while ((max_rounds == 0 || state.round_count <= max_rounds) &&
           PlayerHasAliveCharacter(&state.p1) &&
           PlayerHasAliveCharacter(&state.p2)) {
        ActionRecord p1_records[MAX_TURN_ACTIONS];
        ActionRecord p2_records[MAX_TURN_ACTIONS];
        int p1_count = 0;
        int p2_count = 0;
        memset(p1_records, 0, sizeof(p1_records));
        memset(p2_records, 0, sizeof(p2_records));
        LogGameState("BeforeRound", &state);

        if (p1_first) {
            state.current_turn = 1;
            CollectPlayerPlan(&state, 1, mode, p1_records, &p1_count);
            if (IsReturnToMenuRequested()) {
                CloseBattleLog();
                return 1;
            }

            state.current_turn = 2;
            CollectPlayerPlan(&state, 2, mode, p2_records, &p2_count);
            if (IsReturnToMenuRequested()) {
                CloseBattleLog();
                return 1;
            }

            ResolvePlannedRound(&state, 1, p1_records, p1_count, p2_records, p2_count);
        } else {
            state.current_turn = 2;
            CollectPlayerPlan(&state, 2, mode, p2_records, &p2_count);
            if (IsReturnToMenuRequested()) {
                CloseBattleLog();
                return 1;
            }

            state.current_turn = 1;
            CollectPlayerPlan(&state, 1, mode, p1_records, &p1_count);
            if (IsReturnToMenuRequested()) {
                CloseBattleLog();
                return 1;
            }

            ResolvePlannedRound(&state, 2, p1_records, p1_count, p2_records, p2_count);
        }
        if (IsReturnToMenuRequested()) {
            CloseBattleLog();
            return 1;
        }

        EndTurn(&state);
        LogGameState("AfterRound", &state);
        RenderGameBoard(state);
        state.round_count++;
    }

    // ================================================================
    //  鍦烘櫙鍥涳細SCENE_RESULT 鈥?缁撶畻
    // ================================================================
    state.current_scene = SCENE_RESULT;
    state.game_stage = 3;
    RenderGameBoard(state);

    int p1_alive = PlayerHasAliveCharacter(&state.p1);
    printf("\n========== Game Over ==========\n");
    if (p1_alive) {
        printf("Player 1 wins.\n");
    } else {
        printf("Player 2 wins.\n");
    }
    printf("Total rounds: %d\n", state.round_count);
    printf("===============================\n");

    CloseBattleLog();
    return 0;
}

void RunGameLoop() {
    InitGUI();

#ifndef USE_EASYX
    int env_mode = ReadEnvInt("ROCO_GAME_MODE", -1, -1, 1);
    if (env_mode >= 0) {
        printf("[Engine] Using mode selected by environment: %s\n",
               env_mode == MODE_PVP ? "Local PvP" : "PvE");
        RunBattleLoop(env_mode);
        CloseGUI();
        return;
    }
#endif

    while (1) {
        ClearReturnToMenuRequest();
        MenuSelection selection = ShowMainMenu();
        if (selection == MENU_PVP) {
            RunBattleLoop(MODE_PVP);
#ifdef USE_EASYX
            continue;
#else
            break;
#endif
        }
        if (selection == MENU_PVE) {
            RunBattleLoop(MODE_PVE);
#ifdef USE_EASYX
            continue;
#else
            break;
#endif
        }
        if (selection == MENU_CREDITS) {
            ShowCreditsScreenFromFile("docs/credits.txt");
            continue;
        }
        if (selection == MENU_EXIT) {
            break;
        }
    }

    CloseGUI();
}
