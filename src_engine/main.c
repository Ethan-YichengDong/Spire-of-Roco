#include <stdio.h>
#include "../src_data/data_manager.h"
#include "../src_engine/game_engine.h"

int main() {
    printf("Loading data...\n");
    load_game_data("src_data/cards.txt", "src_data/characters.txt");
    printf("Loaded %d cards and %d characters.\n", g_card_count, g_char_count);
    
    printf("Starting Game Loop...\n");
    RunGameLoop();
    
    printf("Game Over.\n");
    return 0;
}
