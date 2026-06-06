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

// ============================================================
//  选角阶段：为指定玩家填充3个角色（不重复）
// ============================================================
static void SelectTeamPhase(Player* player, int player_id, int mode, GameState* state) {
    int retries = 0;

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
            pick_count = SelectMultipleCharactersFromUI(player_id, TEAM_SIZE - slot, picks, &pick_count);
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
            slot++; retries = 0;
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
        if (player_id == 2 && mode == MODE_PVE) {
            // PvE: AI selects remaining cards (placeholder logic)
            if (player->draw_count >= 16) break;
            int card_idx = (player->draw_count / 2) % g_card_count;
            printf("[Engine] PvE: AI selects card %s\n", g_all_cards[card_idx].name);
            player->draw_pile[player->draw_count++] = g_all_cards[card_idx];
            continue;
        } else {
            // Let player pick multiple cards at once
            pick_count = SelectMultipleCardsFromUI(player_id, MAX_DECK_SIZE - player->draw_count, picks, &pick_count);
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
        LogGameState("BeforeRound", &state);

        if (p1_first) {
            state.current_turn = 1;
            ShowTurnTransitionMask(1);
            PlayerTurnLoop(&state, 1, mode);

            if (!PlayerHasAliveCharacter(&state.p2)) break;

            state.current_turn = 2;
            ShowTurnTransitionMask(2);
            PlayerTurnLoop(&state, 2, mode);
        } else {
            state.current_turn = 2;
            ShowTurnTransitionMask(2);
            PlayerTurnLoop(&state, 2, mode);

            if (!PlayerHasAliveCharacter(&state.p1)) break;

            state.current_turn = 1;
            ShowTurnTransitionMask(1);
            PlayerTurnLoop(&state, 1, mode);
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
