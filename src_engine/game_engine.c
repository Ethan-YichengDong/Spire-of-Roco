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
        printf("[LOG] 无法创建战斗日志文件: %s\n", path);
        return;
    }

    fprintf(g_battle_log, "Spire of Roco Battle Log\n");
    fprintf(g_battle_log, "log_file=%s\n\n", path);
    fflush(g_battle_log);
    printf("[LOG] 战斗日志已写入: %s\n", path);
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

// 获取玩家2的行动，基于选择的游戏模式
Action GetPlayer2Action(GameState state, int mode) {
    if (mode == MODE_PVP) {
        // 本地 PvP 模式：接收玩家2的人工输入
        return GetHumanInputFromUI(2, state);
    } else {
        // MODE_PVE：通过 Socket 桥连调用 AI 后台来生成行动
        return GetAIActionFromBackend(state, 2);
    }
}

// 回合内循环：允许同一玩家连续出牌，直到能量耗尽或主动结束回合
#if 0
static void PlayerTurnLoop(GameState* state, int player_id, int mode) {
    Player* player = (player_id == 1) ? &state->p1 : &state->p2;

    while (player->hand_count > 0) {
        // 若己方出战角色阵亡，自动切换到队伍中下一个存活角色
        if (!player->team[player->active_idx].is_alive) {
            remove_dead_from_team(player, player->active_idx);
            if (!player->team[player->active_idx].is_alive) break;
        }

        // 检查是否还有足够的能量打出任意一张手牌
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

static void ResolveOneRecord(GameState* state, ActionRecord* record, int step_number, int step_total) {
    Player* acting = (record->player_id == 1) ? &state->p1 : &state->p2;
    if (!PlayerHasAliveCharacter(acting)) return;
    if (!acting->team[acting->active_idx].is_alive) {
        remove_dead_from_team(acting, acting->active_idx);
    }
    if (!PlayerHasAliveCharacter(acting)) return;

    if (!HasExplicitSingleTarget(state, record->action, record->player_id)) {
        printf("[Engine] Skipped incomplete recorded card action from P%d.\n", record->player_id);
        return;
    }

    Action action = NormalizeActionForCurrentState(state, record->action, record->player_id);
    if (action.type == ACTION_END_TURN || action.type == ACTION_NONE) return;

    if (g_battle_log != NULL) {
        fprintf(g_battle_log, "\n[ResolvedAction]\n");
        LogAction(state, action);
    }

    ResolutionReport report;
    ExecuteActionWithReport(state, &action, record->player_id, &report);
    LogGameState("AfterResolvedAction", state);
    ShowResolutionStep(*state, record, &report, step_number, step_total);
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
    int total_steps = first_count + second_count;
    int step = 1;

    if (total_steps <= 0) {
        ShowResolutionStep(*state, NULL, NULL, 1, 1);
        return;
    }

    for (int i = 0; i < first_count; i++) {
        ResolveOneRecord(state, &first_records[i], step++, total_steps);
    }

    Player* second_player = (second_player_id == 1) ? &state->p1 : &state->p2;
    if (!PlayerHasAliveCharacter(second_player)) {
        ActionRecord skipped;
        memset(&skipped, 0, sizeof(skipped));
        skipped.player_id = second_player_id;
        skipped.action.type = ACTION_NONE;
        snprintf(skipped.summary, sizeof(skipped.summary),
                 "P%d is defeated; remaining actions skipped", second_player_id);
        ShowResolutionStep(*state, &skipped, NULL, step, total_steps);
        return;
    }

    for (int i = 0; i < second_count; i++) {
        ResolveOneRecord(state, &second_records[i], step++, total_steps);
        if (!PlayerHasAliveCharacter(second_player)) break;
    }
}

// ============================================================
//  选角阶段：为指定玩家填充3个角色（不重复）
// ============================================================
static void SelectTeamPhase(Player* player, int player_id, int mode, GameState* state) {
    int slot = 0;
    while (slot < TEAM_SIZE) {
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
    }

    player->active_idx = 0;
}

// ============================================================
//  选牌阶段：为指定玩家逐张构筑牌库
// ============================================================
static void BuildDeckPhase(Player* player, int player_id, int mode) {
    while (player->draw_count < MAX_DECK_SIZE) {
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

    printf("玩家%d 牌库共 %d 张卡牌\n", player_id, player->draw_count);
    shuffle_draw_pile(player);
}

// 执行并管理游戏核心循环
void RunGameLoop() {
    GameState state;
    int mode;

    memset(&state, 0, sizeof(GameState));
    srand((unsigned int)time(NULL));

    state.round_count = 1;
    state.current_turn = 1;
    state.p1.player_id = 1;
    state.p2.player_id = 2;
    strcpy(state.p1.name, "玩家1");
    strcpy(state.p2.name, "玩家2");
    state.p1.max_energy = 3;
    state.p2.max_energy = 3;
    init_deck(&state.p1);
    init_deck(&state.p2);

    // ================================================================
    //  场景一：SCENE_MENU — 主菜单
    // ================================================================
    state.current_scene = SCENE_MENU;
    state.game_stage = 0;
    InitGUI();
    OpenBattleLog();
    LogGameState("InitialState", &state);
    RenderGameBoard(state);

    int env_mode = ReadEnvInt("ROCO_GAME_MODE", -1, -1, 1);
    if (env_mode >= 0) {
        mode = env_mode;
        printf("[Engine] 使用环境变量选择模式: %s\n",
               mode == MODE_PVP ? "本地PvP" : "人机对战(PvE)");
    } else {
        mode = GetModeSelectionFromUI();
    }

    int max_rounds = ReadEnvInt("ROCO_SMOKE_MAX_ROUNDS", 0, 0, 10000);
    if (max_rounds > 0) {
        printf("[Engine] 冒烟最大回合数: %d\n", max_rounds);
    }
    printf("游戏模式: %s\n", (mode == MODE_PVP) ? "本地PvP" : "人机对战(PvE)");

    // ================================================================
    //  场景二：SCENE_DRAFT — 队伍选择与牌库构筑
    // ================================================================
    state.current_scene = SCENE_DRAFT;
    state.game_stage = 1;
    RenderGameBoard(state);

    printf("\n===== 玩家1 队伍选择 =====\n");
    SelectTeamPhase(&state.p1, 1, mode, &state);

    ShowTurnTransitionMask(2);
    printf("\n===== 玩家2 队伍选择 =====\n");
    SelectTeamPhase(&state.p2, 2, mode, &state);

    printf("\n===== 玩家1 牌库构筑 =====\n");
    BuildDeckPhase(&state.p1, 1, mode);

    ShowTurnTransitionMask(2);
    printf("\n===== 玩家2 牌库构筑 =====\n");
    BuildDeckPhase(&state.p2, 2, mode);

    if (state.p1.draw_count == 0 || state.p2.draw_count == 0) {
        printf("[引擎] 错误: 牌库不能为空，游戏异常退出。\n");
        CloseBattleLog();
        CloseGUI();
        return;
    }

    draw_card(&state.p1, MAX_HAND_SIZE);
    draw_card(&state.p2, MAX_HAND_SIZE);
    state.p1.energy = state.p1.max_energy;
    state.p2.energy = state.p2.max_energy;
    LogGameState("AfterDraft", &state);

    // ================================================================
    //  场景三：SCENE_BATTLE — 战斗
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
            ShowTurnTransitionMask(1);
            CollectPlayerPlan(&state, 1, mode, p1_records, &p1_count);

            state.current_turn = 2;
            ShowTurnTransitionMask(2);
            CollectPlayerPlan(&state, 2, mode, p2_records, &p2_count);

            ResolvePlannedRound(&state, 1, p1_records, p1_count, p2_records, p2_count);
        } else {
            state.current_turn = 2;
            ShowTurnTransitionMask(2);
            CollectPlayerPlan(&state, 2, mode, p2_records, &p2_count);

            state.current_turn = 1;
            ShowTurnTransitionMask(1);
            CollectPlayerPlan(&state, 1, mode, p1_records, &p1_count);

            ResolvePlannedRound(&state, 2, p1_records, p1_count, p2_records, p2_count);
        }

        EndTurn(&state);
        LogGameState("AfterRound", &state);
        RenderGameBoard(state);
        state.round_count++;
    }

    // ================================================================
    //  场景四：SCENE_RESULT — 结算
    // ================================================================
    state.current_scene = SCENE_RESULT;
    state.game_stage = 3;
    RenderGameBoard(state);

    int p1_alive = PlayerHasAliveCharacter(&state.p1);
    printf("\n========== 游戏结束 ==========\n");
    if (p1_alive) {
        printf("玩家1获胜！\n");
    } else {
        printf("玩家2获胜！\n");
    }
    printf("总回合数: %d\n", state.round_count);
    printf("===============================\n");

    CloseBattleLog();
    CloseGUI();
}
