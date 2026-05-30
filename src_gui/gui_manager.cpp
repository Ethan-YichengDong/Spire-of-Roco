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

// 角色选择阶段：让玩家从全局角色池中为队伍槽位选择角色
// 返回值是选中的角色在g_all_characters中的索引
int SelectCharacterFromUI(int player_id, int slot_number) {
    // 占位逻辑：按默认顺序分配，后续由GUI同学替换为实际的选择界面
    // P1默认选择前三个角色(0:Squirtle, 1:Charmander, 2:Bulbasaur)
    // P2默认选择(3:Pikachu, 0:Squirtle, 1:Charmander)
    printf("[GUI] SelectCharacterFromUI: 玩家 %d 正在为槽位 %d 选择角色\n", player_id, slot_number);
    if (player_id == 1) {
        int defaults[] = {0, 1, 2};
        return defaults[slot_number];
    } else {
        int defaults[] = {3, 0, 1};
        return defaults[slot_number];
    }
}

// 卡牌选择阶段：让玩家从全局卡牌池中为牌库逐张挑选卡牌
// 返回选中的卡牌在g_all_cards中的索引，返回-1表示结束选择
int SelectCardFromUI(int player_id, int current_deck_size) {
    // 占位逻辑：自动按顺序加入每种卡牌2张（共16张），后续由GUI同学替换为实际选择界面
    printf("[GUI] SelectCardFromUI: 玩家 %d 正在选择第 %d 张卡牌\n", player_id, current_deck_size + 1);
    // 默认构筑：8种卡各2张，选满16张后自动结束
    if (current_deck_size >= 16) return -1;
    // 每种卡牌2张：索引按0,0,1,1,2,2,...排列
    return (current_deck_size / 2) % 8;
}

// 主菜单阶段：获取玩家的游戏模式选择，返回MODE_PVP或MODE_PVE
int GetModeSelectionFromUI() {
    // 占位逻辑：默认返回本地PvP模式，后续由GUI同学替换为实际菜单选择界面
    printf("[GUI] GetModeSelectionFromUI: 当前默认选择 本地PvP模式\n");
    return MODE_PVP;
}
