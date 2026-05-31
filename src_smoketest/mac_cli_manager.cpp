#include "../src_gui/gui_manager.h"
#include "../src_data/data_manager.h"
#include <stdio.h>
#include <stdlib.h>

// 在不包含 EasyX 的系统上（如 Mac/Linux），启动 CLI 的模拟测试环境入口
void InitGUI() {
    printf("\n======================================================\n");
    printf("   洛克王国爬塔版 - Mac 系统冒烟测试端 (CLI 模式)\n");
    printf("======================================================\n");
}

// 测试结束时的资源兜底释放或日志输出提示
void CloseGUI() {
    printf("======================================================\n");
    printf("   自动冒烟测试结束。\n");
    printf("======================================================\n");
}

// 终端文字版界面渲染引擎，输出核心状态数据到 console 下以供调试排错使用
void RenderGameBoard(GameState state) {
    printf("\n------ [回合结算完毕 - 当前盘面] ------\n");
    printf("轮次: %d\n", state.round_count);
    Character p1_char = state.p1.team[state.p1.active_idx];
    Character p2_char = state.p2.team[state.p2.active_idx];
    
    printf("[玩家 1] 手牌数: %d | 能量: %d\n", state.p1.hand_count, state.p1.energy);
    printf("出战宠物 >> %s (当前血量: %d/%d, 速度: %d)\n", p1_char.name, p1_char.hp, p1_char.max_hp, p1_char.speed);
    
    printf("[玩家 2] 手牌数: %d | 能量: %d\n", state.p2.hand_count, state.p2.energy);
    printf("出战宠物 >> %s (当前血量: %d/%d, 速度: %d)\n", p2_char.name, p2_char.hp, p2_char.max_hp, p2_char.speed);
    printf("---------------------------------------\n");
}

// 测试终端中假装存在一个切换提示的占位符函数
void ShowTurnTransitionMask(int player_id) {
    printf("\n>>> [系统流程] 进入玩家 %d 的回合掩码阶段 (自动跳过点击确认以执行自动化冒烟)...\n", player_id);
}

// 构建由自动化测试脚本进行控制的伪造的人类玩家动作回放响应
Action GetHumanInputFromUI(int player_id, GameState state) {
    Action act;
    act.actor_id = player_id;
    act.switch_to_idx = -1;
    act.target_idx = 0;
    
    Player* p = (player_id == 1) ? &state.p1 : &state.p2;

    printf("\n>>> [系统自动输入] 玩家 %d 正在选择操作...\n", player_id);
    
    // 如果处于测试态的人类方有能量并且有卡牌，直接无脑打出第 1 张手牌进行自动化测试演练
    if (p->hand_count > 0 && p->energy >= p->hand[0].energy_cost) {
        act.type = ACTION_PLAY_CARD;
        act.card_hand_idx = 0;
        printf(" -> 尝试打出第 0 张卡牌: %s (耗能:%d)\n", p->hand[0].name, p->hand[0].energy_cost);
    } else {
        // 如果能量不够则结束回合
        act.type = ACTION_END_TURN;
        act.card_hand_idx = 0;
        printf(" -> 能量不足或没有手牌，被动结束本回合。\n");
    }
    
    return act;
}

int SelectCharacterFromUI(int player_id, int slot_number) {
    int p1_defaults[] = {0, 1, 2};
    int p2_defaults[] = {3, 0, 1};
    int selected = (player_id == 1) ? p1_defaults[slot_number] : p2_defaults[slot_number];

    if (g_char_count <= 0) return 0;
    selected %= g_char_count;
    printf("[CLI] 玩家 %d 自动选择槽位 %d 的角色索引: %d\n", player_id, slot_number, selected);
    return selected;
}

int SelectCardFromUI(int player_id, int current_deck_size) {
    if (current_deck_size >= 16) return -1;
    if (g_card_count <= 0) return -1;

    int selected = (current_deck_size / 2) % g_card_count;
    printf("[CLI] 玩家 %d 自动选择第 %d 张卡牌索引: %d\n",
           player_id, current_deck_size + 1, selected);
    return selected;
}

int GetModeSelectionFromUI() {
    const char* raw = getenv("ROCO_GAME_MODE");
    if (raw != NULL && raw[0] == '1') {
        printf("[CLI] 环境变量选择 PvE 模式。\n");
        return MODE_PVE;
    }

    printf("[CLI] 默认选择 PvP 模式。\n");
    return MODE_PVP;
}
