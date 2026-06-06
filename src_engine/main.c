#include <stdio.h>
#include <stdlib.h>
#include "../src_data/data_manager.h"
#include "../src_engine/game_engine.h"

int main() {
    printf("========================================\n");
    printf("   Spire of Roco — 洛克王国爬塔版\n");
    printf("========================================\n\n");

    // 加载卡牌与角色的静态数据文件
    printf("[启动] 正在加载游戏数据...\n");
    load_game_data("src_data/cards.txt", "src_data/characters.txt");

    // 校验数据是否成功加载
    if (g_card_count == 0) {
        printf("[错误] 卡牌数据加载失败或卡牌数量为0，请检查 src_data/cards.txt 文件。\n");
        return EXIT_FAILURE;
    }
    if (g_char_count == 0) {
        printf("[错误] 角色数据加载失败或角色数量为0，请检查 src_data/characters.txt 文件。\n");
        return EXIT_FAILURE;
    }
    printf("[启动] 成功加载 %d 张卡牌、%d 个角色。\n\n", g_card_count, g_char_count);

    // 启动游戏引擎（内部管理 SCENE_MENU → DRAFT → BATTLE → RESULT 全流程）
    printf("[启动] 正在进入游戏...\n\n");
    RunGameLoop();

    printf("\n[退出] 感谢游玩 Spire of Roco！\n");
    return EXIT_SUCCESS;
}
