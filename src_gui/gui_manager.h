#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#include "../game_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化 GUI 窗口组件
void InitGUI();
// 关闭并释放 GUI 窗口资源
void CloseGUI();

// 渲染当前游戏板状态（血量，蓝量，手牌数等重要信息）
void RenderGameBoard(GameState state);
// 显示回合切换时的遮罩层，提示当前出手玩家
void ShowTurnTransitionMask(int player_id);
// 从 UI 界面捕获玩家的具体输入（键盘或鼠标指令），并转换为 Action 返回
Action GetHumanInputFromUI(int player_id, GameState state);

#ifdef __cplusplus
}
#endif

#endif // GUI_MANAGER_H
