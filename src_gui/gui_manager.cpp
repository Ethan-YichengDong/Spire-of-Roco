#include "gui_manager.h"
#include <stdio.h>
#include <string.h>

// EasyX 图形库头文件，主要用于 Windows 系统下的图形渲染
// #include <graphics.h>
// #include <conio.h>

// 初始化游戏图形化渲染窗口
void InitGUI() {
    // initgraph(1280, 720, EX_SHOWCONSOLE);
}

// 释放图形页面，关闭程序渲染组件
void CloseGUI() {
    // closegraph();
}

// 根据当前传入的 GameState 游戏状态参数，对界面中的UI元素（人物状态，卡牌模型等）进行重绘
void RenderGameBoard(GameState state) {
    // cleardevice();
    
    // 渲染开发者及系统相关的信息水印
    // settextcolor(WHITE);
    // outtextxy(10, 10, _T("Department: CS, Student ID: 123456, Name: John Doe"));

    // 绘制玩家1和玩家2的角色数值及元素模型
    // ...
    // Draw Player 2
    // ...

    // FlushBatchDraw();
    
    // 控制台日志降级输出，便于在没有 EasyX 的平台调试查阅
    printf("[GUI] 调用了中心渲染 RenderGameBoard 函数。 回合: %d, P1 血量: %d, P2 血量: %d\n", state.current_turn, state.p1.team[state.p1.active_idx].hp, state.p2.team[state.p2.active_idx].hp);
}

// 显示回合交接时的防作弊遮罩幕（在一台机器上本地双人对战时使用）
void ShowTurnTransitionMask(int player_id) {
    // setfillcolor(BLACK);
    // solidrectangle(0, 0, 1280, 720);
    // char msg[256];
    // sprintf(msg, "Player %d's Turn, Please Blindfold Player %d. Click to Confirm.", player_id, (player_id == 1) ? 2 : 1);
    // outtextxy(400, 360, msg);
    
    // 等待鼠标点击事件，确认后才消去遮罩并展示相应面板
    // MOUSEMSG m;
    // while (true) {
    //     m = GetMouseMsg();
    //     if (m.uMsg == WM_LBUTTONDOWN) break;
    // }
    printf("[GUI] ShowTurnTransitionMask: 轮到玩家 %d 行动了。点击以确认交接。\n", player_id);
}

// 从图形界面捕获真人玩家对于打牌或者换宠的交互输入
Action GetHumanInputFromUI(int player_id, GameState state) {
    (void)state; // 压制由于不使用参数造成的编译器警告
    
    // 目前占位用的伪输入数据，默认打出第一张卡牌
    Action act;
    act.type = ACTION_PLAY_CARD;
    act.actor_id = player_id;
    act.card_hand_idx = 0; // 默认出第一张手牌
    act.switch_to_idx = -1;
    
    printf("[GUI] GetHumanInputFromUI: 正在为玩家 %d 解析默认的卡牌行动交互动作\n", player_id);
    return act;
}
