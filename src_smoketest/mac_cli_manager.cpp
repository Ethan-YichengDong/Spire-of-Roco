#include "../src_gui/gui_manager.h"
#include <stdio.h>
#include <stdlib.h>

void InitGUI() {
    printf("\n======================================================\n");
    printf("   Spire of Roco - Mac OS Smoke Test (CLI Mode)\n");
    printf("======================================================\n");
}

void CloseGUI() {
    printf("======================================================\n");
    printf("   Smoke Test Completed.\n");
    printf("======================================================\n");
}

void RenderGameBoard(GameState state) {
    printf("\n------ [回合结算完毕 - 当前盘面] ------\n");
    printf("轮次: %d\n", state.round_count);
    Character p1_char = state.p1.team[state.p1.active_idx];
    Character p2_char = state.p2.team[state.p2.active_idx];
    
    printf("[Player 1] 手牌数: %d | 能量: %d\n", state.p1.hand_count, state.p1.energy);
    printf("出战宠物 >> %s (HP: %d/%d, 速度: %d)\n", p1_char.name, p1_char.hp, p1_char.max_hp, p1_char.speed);
    
    printf("[Player 2] 手牌数: %d | 能量: %d\n", state.p2.hand_count, state.p2.energy);
    printf("出战宠物 >> %s (HP: %d/%d, 速度: %d)\n", p2_char.name, p2_char.hp, p2_char.max_hp, p2_char.speed);
    printf("---------------------------------------\n");
}

void ShowTurnTransitionMask(int player_id) {
    printf("\n>>> [系统流程] 进入 Player %d 的回合掩码阶段 (自动跳过确认以执行自动化冒烟)...\n", player_id);
}

Action GetHumanInputFromUI(int player_id, GameState state) {
    Action act;
    act.actor_id = player_id;
    act.switch_to_idx = -1;
    
    Player* p = (player_id == 1) ? &state.p1 : &state.p2;

    printf("\n>>> [系统自动输入] Player %d 正在选择操作...\n", player_id);
    
    if (p->hand_count > 0 && p->energy >= p->hand[0].energy_cost) {
        act.type = ACTION_PLAY_CARD;
        act.card_hand_idx = 0;
        printf(" -> 打出第 0 张卡牌: %s (耗能:%d)\n", p->hand[0].name, p->hand[0].energy_cost);
    } else {
        act.type = ACTION_END_TURN;
        act.card_hand_idx = 0;
        printf(" -> 能量不足或没有手牌，结束回合\n");
    }
    
    return act;
}
