#include "game_engine.h"
#include "../src_gui/gui_manager.h"
#include "../src_data/battle_calculator.h"
#include "../src_data/data_manager.h"
#include "../src_ai/ai_bridge.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

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
#ifdef _WIN32
    _mkdir("logs");
#else
    mkdir("logs", 0755);
#endif
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

    fprintf(g_battle_log, "Spire of Roco Battle Smoke Log\n");
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

static void LogCharacter(int slot, Character* ch, int is_active) {
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

static void LogPlayerState(const char* label, Player* player) {
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

static void LogGameState(const char* title, GameState* state) {
    if (g_battle_log == NULL) return;

    fprintf(g_battle_log, "\n[%s]\n", title);
    fprintf(g_battle_log, "round=%d current_turn=%d stage=%d scene=%d\n",
            state->round_count, state->current_turn,
            state->game_stage, state->current_scene);
    LogPlayerState("P1", &state->p1);
    LogPlayerState("P2", &state->p2);
    fflush(g_battle_log);
}

static void LogAction(GameState* state, Action action) {
    if (g_battle_log == NULL) return;

    Player* actor = (action.actor_id == 1) ? &state->p1 : &state->p2;
    fprintf(g_battle_log,
            "  P%d action=%s card_hand_idx=%d switch_to_idx=%d target_idx=%d",
            action.actor_id, ActionName(action.type),
            action.card_hand_idx, action.switch_to_idx, action.target_idx);

    if (action.type == ACTION_PLAY_CARD &&
        action.card_hand_idx >= 0 &&
        action.card_hand_idx < actor->hand_count) {
        Card* card = &actor->hand[action.card_hand_idx];
        fprintf(g_battle_log,
                " card=%s cost=%d damage=%d defense=%d heal=%d target_type=%d",
                card->name, card->energy_cost, card->base_damage,
                card->base_defense, card->base_heal, card->target_type);
    }

    fprintf(g_battle_log, "\n");
    fflush(g_battle_log);
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

static void ResolveTurn(GameState* state, Action a1, Action a2) {
    a1 = NormalizeActionForCurrentState(state, a1, 1);
    a2 = NormalizeActionForCurrentState(state, a2, 2);

    if (g_battle_log != NULL) {
        fprintf(g_battle_log, "\n[TurnActions]\n");
        LogAction(state, a1);
        LogAction(state, a2);
    }

    ExecuteAction(state, &a1, 1);
    ExecuteAction(state, &a2, 2);
    EndTurn(state);
}

// 获取玩家2的行动，基于选择的游戏模式
Action GetPlayer2Action(GameState state, int mode) {
    if (mode == 0) {
        // 本地 PvP 模式：接收玩家2的人工输入
        return GetHumanInputFromUI(2, state);
    } else {
        // 模式1（PVE 模式）：通过 Socket 桥连调用 AI 后台（例如 Python 端）来生成行动
        return GetAIActionFromBackend(state, 2);
    }
}

// 执行并管理游戏核心循环
void RunGameLoop() {
    GameState state;
    memset(&state, 0, sizeof(state));

    // 初始化基本状态
    state.round_count = 1;      // 当前回合数初始化
    state.current_turn = 1;     // 设置为玩家1的回合
    state.game_stage = 1;       // 第一阶段
    state.current_scene = SCENE_BATTLE; // 推入战斗节点
    state.p1.player_id = 1;
    state.p2.player_id = 2;
    state.p1.active_idx = 0;    // 当前出战角色设为队伍第一个（下标0）
    state.p2.active_idx = 0;
    
    // 如果有读取到角色数据，为双方分配测试队伍
    if (g_char_count > 0) {
        for (int i = 0; i < TEAM_SIZE; i++) {
            state.p1.team[i] = g_all_characters[i % g_char_count];
            state.p2.team[i] = g_all_characters[(i + 1) % g_char_count];
        }
    }
    
    // 初始化双方的初始手牌信息和能量池
    state.p1.max_energy = 3;
    state.p2.max_energy = 3;
    state.p1.energy = 3;
    state.p2.energy = 3;
    init_deck(&state.p1);
    init_deck(&state.p2);
    
    // 赋予双方手牌中的第一张卡（测试逻辑）
    if (g_card_count > 0) {
        state.p1.hand[state.p1.hand_count++] = g_all_cards[0];
        state.p2.hand[state.p2.hand_count++] = g_all_cards[0];
    }

    // 初始化并呼出游戏图形界面
    InitGUI();
    OpenBattleLog();
    LogGameState("InitialState", &state);

    int mode = 0; // 当前设置为本地 PvP（后续可扩展配置项）
    
    // 游戏核心判定循环，只要两边主战角色都仍然存活就继续游戏
    while (state.p1.team[state.p1.active_idx].is_alive && state.p2.team[state.p2.active_idx].is_alive) {
        LogGameState("BeforeTurn", &state);

        // 显示提示进入 玩家1 操作回合的UI界面
        ShowTurnTransitionMask(1);
        Action a1 = GetHumanInputFromUI(1, state);
        
        // 显示提示进入 玩家2 操作回合的UI界面
        ShowTurnTransitionMask(2);
        Action a2 = GetPlayer2Action(state, mode);
        
        // 双方动作提交完成后，送入结算器解析本回合效果及伤害
        ResolveTurn(&state, a1, a2);
        LogGameState("AfterTurn", &state);
        
        // 刷新重绘界面，显示最新的血量/状态等信息
        RenderGameBoard(state);

        state.round_count++;
        // 每回合开始时统一重置或发放能量（暂定回复至3点）
        state.p1.energy = 3;
        state.p2.energy = 3;
        
        // FIXME: 目前为了防止死循环在没有抽牌逻辑的情况下跑满内存，这里暂时使用 break 强行跳出
        // 真实情况如果手牌打空或者判定胜利状态会自然 break，届时可以拓展这里的抽卡逻辑
        break; 
    }

    // 摧毁/关闭GUI窗口
    CloseBattleLog();
    CloseGUI();
}
