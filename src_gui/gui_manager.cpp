#include "gui_manager.h"
#include <stdio.h>
#include <string.h>

// EasyX headers
// #include <graphics.h>
// #include <conio.h>

void InitGUI() {
    // initgraph(1280, 720, EX_SHOWCONSOLE);
}

void CloseGUI() {
    // closegraph();
}

void RenderGameBoard(GameState state) {
    // cleardevice();
    
    // Draw developer institutional information
    // settextcolor(WHITE);
    // outtextxy(10, 10, _T("Department: CS, Student ID: 123456, Name: John Doe"));

    // Draw Player 1
    // ...
    // Draw Player 2
    // ...

    // FlushBatchDraw();
    printf("[GUI] RenderGameBoard called. Turn: %d, P1 HP: %d, P2 HP: %d\n", state.current_turn, state.p1.team[state.p1.active_idx].hp, state.p2.team[state.p2.active_idx].hp);
}

void ShowTurnTransitionMask(int player_id) {
    // setfillcolor(BLACK);
    // solidrectangle(0, 0, 1280, 720);
    // char msg[256];
    // sprintf(msg, "Player %d's Turn, Please Blindfold Player %d. Click to Confirm.", player_id, (player_id == 1) ? 2 : 1);
    // outtextxy(400, 360, msg);
    
    // MOUSEMSG m;
    // while (true) {
    //     m = GetMouseMsg();
    //     if (m.uMsg == WM_LBUTTONDOWN) break;
    // }
    printf("[GUI] ShowTurnTransitionMask: Player %d's Turn. Click to Confirm.\n", player_id);
}

Action GetHumanInputFromUI(int player_id, GameState state) {
    (void)state; // Unused parameter
    Action act;
    act.type = ACTION_PLAY_CARD;
    act.actor_id = player_id;
    act.card_hand_idx = 0; // default play first card
    act.switch_to_idx = -1;
    
    printf("[GUI] GetHumanInputFromUI resolving default Action for Player %d\n", player_id);
    return act;
}
