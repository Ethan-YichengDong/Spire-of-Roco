#include "game_engine.h"
#include "../src_gui/gui_manager.h"
#include "../src_data/battle_calculator.h"
#include "../src_data/data_manager.h"
#include "../src_ai/ai_bridge.h"
#include <stdio.h>

Action GetPlayer2Action(GameState state, int mode) {
    if (mode == 0) {
        return GetHumanInputFromUI(2, state);
    } else {
        // Mode 1: AI Mode via Socket bridge (Module 4)
        return GetAIActionFromBackend(state, 2);
    }
}

void RunGameLoop() {
    GameState state;
    // Basic init
    state.round_count = 1;
    state.current_turn = 1;
    state.game_stage = 1;
    state.p1.player_id = 1;
    state.p2.player_id = 2;
    state.p1.active_idx = 0;
    state.p2.active_idx = 0;
    
    // Assign character from glob
    if (g_char_count > 1) {
        state.p1.team[0] = g_all_characters[0];
        state.p2.team[0] = g_all_characters[1];
    }
    
    // Assign hands
    state.p1.energy = 3;
    state.p2.energy = 3;
    state.p1.hand_count = 0;
    state.p2.hand_count = 0;
    
    if (g_card_count > 0) {
        state.p1.hand[state.p1.hand_count++] = g_all_cards[0];
        state.p2.hand[state.p2.hand_count++] = g_all_cards[0];
    }

    InitGUI();

    int mode = 0; // Local PvP
    
    while (state.p1.team[state.p1.active_idx].is_alive && state.p2.team[state.p2.active_idx].is_alive) {
        ShowTurnTransitionMask(1);
        Action a1 = GetHumanInputFromUI(1, state);
        
        ShowTurnTransitionMask(2);
        Action a2 = GetPlayer2Action(state, mode);
        
        ResolveTurn(&state, a1, a2);
        
        RenderGameBoard(state);

        state.round_count++;
        // Re-draw cards or give energy here for next round
        state.p1.energy = 3;
        state.p2.energy = 3;
        // In reality, if out of cards, should break or load more
        break; // break for now to avoid infinite loop without real logic
    }

    CloseGUI();
}
