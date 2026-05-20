#include "game_engine.h"
#include "../src_gui/gui_manager.h"
#include "../src_data/battle_calculator.h"
#include "../src_data/data_manager.h"
#include "../src_ai/ai_bridge.h"
#include <stdio.h>

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
    // 初始化基本状态
    state.round_count = 1;      // 当前回合数初始化
    state.current_turn = 1;     // 设置为玩家1的回合
    state.game_stage = 1;       // 第一阶段
    state.current_scene = SCENE_BATTLE; // 推入战斗节点
    state.p1.player_id = 1;
    state.p2.player_id = 2;
    state.p1.active_idx = 0;    // 当前出战角色设为队伍第一个（下标0）
    state.p2.active_idx = 0;
    
    // 如果有读取到角色数据，为双方分配首发角色
    if (g_char_count > 1) {
        state.p1.team[0] = g_all_characters[0];
        state.p2.team[0] = g_all_characters[1];
    }
    
    // 初始化双方的初始手牌信息和能量池
    state.p1.energy = 3;
    state.p2.energy = 3;
    state.p1.hand_count = 0;
    state.p2.hand_count = 0;
    
    // 赋予双方手牌中的第一张卡（测试逻辑）
    if (g_card_count > 0) {
        state.p1.hand[state.p1.hand_count++] = g_all_cards[0];
        state.p2.hand[state.p2.hand_count++] = g_all_cards[0];
    }

    // 初始化并呼出游戏图形界面
    InitGUI();

    int mode = 0; // 当前设置为本地 PvP（后续可扩展配置项）
    
    // 游戏核心判定循环，只要两边主战角色都仍然存活就继续游戏
    while (state.p1.team[state.p1.active_idx].is_alive && state.p2.team[state.p2.active_idx].is_alive) {
        // 显示提示进入 玩家1 操作回合的UI界面
        ShowTurnTransitionMask(1);
        Action a1 = GetHumanInputFromUI(1, state);
        
        // 显示提示进入 玩家2 操作回合的UI界面
        ShowTurnTransitionMask(2);
        Action a2 = GetPlayer2Action(state, mode);
        
        // 双方动作提交完成后，送入结算器解析本回合效果及伤害
        ResolveTurn(&state, a1, a2);
        
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
    CloseGUI();
}
