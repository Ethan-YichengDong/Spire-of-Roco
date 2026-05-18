#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#include "../game_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the GUI window
void InitGUI();
// Close the GUI window
void CloseGUI();

void RenderGameBoard(GameState state);
void ShowTurnTransitionMask(int player_id);
Action GetHumanInputFromUI(int player_id, GameState state);

#ifdef __cplusplus
}
#endif

#endif // GUI_MANAGER_H
