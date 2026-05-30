#include "game_engine.h"
#include "../src_gui/gui_manager.h"
#include "../src_data/battle_calculator.h"
#include "../src_data/data_manager.h"
#include "../src_ai/ai_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
            // 切换后仍无存活角色，说明全军覆没，终止本回合
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
        // 能量不足以打出任何手牌，自动结束本回合
        if (!can_act) break;

        // 刷新界面，展示当前盘面
        RenderGameBoard(*state);

        // 获取该玩家的行动决策
        Action action;
        if (player_id == 1) {
            action = GetHumanInputFromUI(player_id, *state);
        } else {
            action = GetPlayer2Action(*state, mode);
        }

        // 玩家主动选择结束回合
        if (action.type == ACTION_END_TURN) break;

        // 执行该行动（出牌/换人），结算伤害与效果
        ExecuteAction(state, &action, player_id);
    }
}

// ============================================================
//  选角阶段：为指定玩家填充3个角色（不重复）
// ============================================================
static void SelectTeamPhase(Player* player, int player_id, int mode, GameState* state) {
    int retries = 0;  // 防死循环计数

    for (int slot = 0; slot < TEAM_SIZE; slot++) {
        int char_idx;
        if (player_id == 2 && mode == MODE_PVE) {
            // PvE模式下P2由AI自动选角（占位逻辑，后续由D同学替换）
            char_idx = (slot < g_char_count) ? slot : 0;
            printf("[引擎] PvE模式: AI为玩家2自动选择角色 %s\n", g_all_characters[char_idx].name);
        } else {
            char_idx = SelectCharacterFromUI(player_id, slot);
        }

        // 防止同一玩家重复选择同一角色
        int duplicate = 0;
        for (int prev = 0; prev < slot; prev++) {
            if (player->team[prev].char_id == g_all_characters[char_idx].char_id) {
                duplicate = 1;
                break;
            }
        }

        if (duplicate) {
            if (++retries >= 10) {
                // 兜底保护：自动分配第一个未被使用的角色，防止死循环
                printf("[引擎] 重复次数过多，自动分配未使用角色\n");
                for (int k = 0; k < g_char_count; k++) {
                    int used = 0;
                    for (int prev = 0; prev < slot; prev++) {
                        if (player->team[prev].char_id == g_all_characters[k].char_id) {
                            used = 1; break;
                        }
                    }
                    if (!used) {
                        assign_character_to_team(player, g_all_characters[k].char_id, slot);
                        retries = 0;
                        break;
                    }
                }
            } else {
                printf("[引擎] 角色重复，请重新选择\n");
                slot--;
                continue;
            }
        } else {
            assign_character_to_team(player, g_all_characters[char_idx].char_id, slot);
            retries = 0;
        }

        // 每次选角后刷新界面
        RenderGameBoard(*state);
    }
    player->active_idx = 0;  // 槽位0为首发
}

// ============================================================
//  选牌阶段：为指定玩家逐张构筑牌库
// ============================================================
static void BuildDeckPhase(Player* player, int player_id, int mode) {
    while (player->draw_count < MAX_DECK_SIZE) {
        int card_idx;
        if (player_id == 2 && mode == MODE_PVE) {
            // PvE模式下P2由AI自动选牌（占位逻辑，后续由D同学替换）
            if (player->draw_count >= 16) break;
            card_idx = (player->draw_count / 2) % g_card_count;
            printf("[引擎] PvE模式: AI为玩家2自动选择卡牌 %s\n", g_all_cards[card_idx].name);
        } else {
            card_idx = SelectCardFromUI(player_id, player->draw_count);
        }
        if (card_idx < 0) break;  // 玩家结束选择
        player->draw_pile[player->draw_count++] = g_all_cards[card_idx];
    }
    printf("玩家%d 牌库共 %d 张卡牌\n", player_id, player->draw_count);
    shuffle_draw_pile(player);  // Fisher-Yates随机洗牌
}

// 执行并管理游戏核心循环
void RunGameLoop() {
    GameState state;
    int mode;

    // ===== 零初始化，防止早期RenderGameBoard读到未初始化数据 =====
    memset(&state, 0, sizeof(GameState));

    // ===== 随机数种子 =====
    srand((unsigned int)time(NULL));

    // ===== 基本状态初始化（在场景渲染前完成，保证RenderGameBoard安全读取）=====
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
    RenderGameBoard(state);
    mode = GetModeSelectionFromUI();
    printf("游戏模式: %s\n", (mode == MODE_PVP) ? "本地PvP" : "人机对战(PvE)");

    // ================================================================
    //  场景二：SCENE_DRAFT — 队伍选择与牌库构筑
    // ================================================================
    state.current_scene = SCENE_DRAFT;
    state.game_stage = 1;
    RenderGameBoard(state);

    // ---- 玩家1 选角 ----
    printf("\n===== 玩家1 队伍选择 =====\n");
    SelectTeamPhase(&state.p1, 1, mode, &state);

    // ---- 玩家2 选角（先遮罩再选，防偷看）----
    ShowTurnTransitionMask(2);
    printf("\n===== 玩家2 队伍选择 =====\n");
    SelectTeamPhase(&state.p2, 2, mode, &state);

    // ---- 玩家1 选牌 ----
    printf("\n===== 玩家1 牌库构筑 =====\n");
    BuildDeckPhase(&state.p1, 1, mode);

    // ---- 玩家2 选牌（先遮罩再选，防偷看）----
    ShowTurnTransitionMask(2);
    printf("\n===== 玩家2 牌库构筑 =====\n");
    BuildDeckPhase(&state.p2, 2, mode);

    // ---- 牌库最少卡牌数校验 ----
    if (state.p1.draw_count == 0 || state.p2.draw_count == 0) {
        printf("[引擎] 错误: 牌库不能为空，游戏异常退出。\n");
        CloseGUI();
        return;
    }

    // ---- 初始抽牌至满手、能量充满 ----
    draw_card(&state.p1, MAX_HAND_SIZE);
    draw_card(&state.p2, MAX_HAND_SIZE);
    state.p1.energy = state.p1.max_energy;
    state.p2.energy = state.p2.max_energy;

    // ================================================================
    //  场景三：SCENE_BATTLE — 战斗
    // ================================================================
    state.current_scene = SCENE_BATTLE;
    state.game_stage = 2;
    RenderGameBoard(state);

    // 首回合随机决定先后手，全游戏沿用该顺序
    int p1_first = (rand() % 2 == 0);

    // 游戏核心循环：当任一方三只角色全军覆没时结束
    while (1) {
        // 判定双方是否全军覆没（三只角色全部阵亡才结束）
        int p1_has_alive = 0, p2_has_alive = 0;
        for (int i = 0; i < TEAM_SIZE; i++) {
            if (state.p1.team[i].is_alive) p1_has_alive = 1;
            if (state.p2.team[i].is_alive) p2_has_alive = 1;
        }
        if (!p1_has_alive || !p2_has_alive) break;

        if (p1_first) {
            state.current_turn = 1;
            ShowTurnTransitionMask(1);
            PlayerTurnLoop(&state, 1, mode);

            state.current_turn = 2;
            ShowTurnTransitionMask(2);
            PlayerTurnLoop(&state, 2, mode);
        } else {
            state.current_turn = 2;
            ShowTurnTransitionMask(2);
            PlayerTurnLoop(&state, 2, mode);

            state.current_turn = 1;
            ShowTurnTransitionMask(1);
            PlayerTurnLoop(&state, 1, mode);
        }

        // 回合结束清理
        EndTurn(&state);
        RenderGameBoard(state);
        state.round_count++;
    }

    // ================================================================
    //  场景四：SCENE_RESULT — 结算
    // ================================================================
    state.current_scene = SCENE_RESULT;
    state.game_stage = 3;
    RenderGameBoard(state);

    int p1_alive = 0;
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (state.p1.team[i].is_alive) p1_alive = 1;
    }
    printf("\n========== 游戏结束 ==========\n");
    if (p1_alive) {
        printf("玩家1获胜！\n");
    } else {
        printf("玩家2获胜！\n");
    }
    printf("总回合数: %d\n", state.round_count);
    printf("===============================\n");

    // 摧毁/关闭 GUI 窗口
    CloseGUI();
}
