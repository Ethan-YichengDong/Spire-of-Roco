#include <stdio.h>
#include "../src_data/data_manager.h"
#include "../src_engine/game_engine.h"

int main() {
    printf("正在加载游戏初始数据（卡牌/角色）...\n");
    // 加载资源配置文件中的静态卡牌和角色数据
    load_game_data("src_data/cards.txt", "src_data/characters.txt");
    printf("成功加载了 %d 张卡牌以及 %d 个角色。\n", g_card_count, g_char_count);
    
    printf("正在启动主游戏循环...\n");
    // 运行游戏内核总控循环
    RunGameLoop();
    
    printf("游戏结束。\n");
    return 0;
}
