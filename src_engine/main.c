#include <stdio.h>
#include <stdlib.h>
#include "../src_data/data_manager.h"
#include "../src_engine/game_engine.h"

int main() {
    printf("========================================\n");
    printf("   Spire of Roco\n");
    printf("========================================\n\n");

    printf("[Startup] Loading game data...\n");
    load_game_data("src_data/cards.txt", "src_data/characters.txt");

    if (g_card_count == 0) {
        printf("[Error] Card data failed to load or card count is 0. Check src_data/cards.txt.\n");
        return EXIT_FAILURE;
    }
    if (g_char_count == 0) {
        printf("[Error] Character data failed to load or character count is 0. Check src_data/characters.txt.\n");
        return EXIT_FAILURE;
    }
    printf("[Startup] Loaded %d cards and %d characters.\n\n", g_card_count, g_char_count);

    printf("[Startup] Entering game...\n\n");
    RunGameLoop();

    printf("\n[Exit] Thanks for playing Spire of Roco.\n");
    return EXIT_SUCCESS;
}
