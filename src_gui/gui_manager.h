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
// 角色选择阶段：让玩家从全局角色池中为队伍槽位(slot:0/1/2)选择1个角色，返回在g_all_characters中的索引
int SelectCharacterFromUI(int player_id, int slot_number);
// 卡牌选择阶段：让玩家从全局卡牌池中为牌库挑选卡牌，返回在g_all_cards中的索引，返回-1表示结束选择
int SelectCardFromUI(int player_id, int current_deck_size);
// 主菜单阶段：获取玩家的游戏模式选择，返回MODE_PVP或MODE_PVE（定义于game_core.h）
int GetModeSelectionFromUI();
#include "../src_data/data_manager.h"
#ifdef __cplusplus
}
#endif

#endif // GUI_MANAGER_H
