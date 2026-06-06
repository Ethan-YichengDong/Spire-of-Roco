#include "gui_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src_data/data_manager.h"
#ifdef USE_EASYX
#include <graphics.h>
#include <conio.h>
static int g_draw_y = 10;
static void reset_draw_y() { g_draw_y = 10; cleardevice(); }
static void draw_line(const char* s) { outtextxy(10, g_draw_y, s); g_draw_y += 24; }
#else
// no graphical helpers
#endif

// 简易基于控制台的 GUI 实现，便于在没有图形库时交互和调试。

void InitGUI() {
#ifdef USE_EASYX
    initgraph(800,600);
    setbkcolor(WHITE);
    cleardevice();
    settextstyle(20,0,"宋体");
#else
    // 控制台不需要特别初始化
#endif
}

void CloseGUI() {
#ifdef USE_EASYX
    closegraph();
#else
    // 控制台不需要特别释放
#endif
}

void print_character(const Character* ch) {
    if (!ch) return;
#ifdef USE_EASYX
    char buf[256];
    snprintf(buf, sizeof(buf), "%s (ID:%d) HP:%d/%d Elem:%d Speed:%d %s",
             ch->name, ch->char_id, ch->hp, ch->max_hp, ch->element, ch->speed,
             ch->is_alive ? "" : "[DEAD]");
    draw_line(buf);
#else
    printf("%s (ID:%d) HP:%d/%d Elem:%d Speed:%d %s\n",
           ch->name, ch->char_id, ch->hp, ch->max_hp, ch->element, ch->speed,
           ch->is_alive ? "" : "[DEAD]");
#endif
}

void print_card(const Card* c, int idx) {
    if (!c) return;
#ifdef USE_EASYX
    char buf[256];
    snprintf(buf, sizeof(buf), " [%2d] %s (ID:%d) Cost:%d Dmg:%d Def:%d Heal:%d Type:%d Target:%d",
             idx, c->name, c->card_id, c->energy_cost, c->base_damage, c->base_defense, c->base_heal, c->type, c->target_type);
    draw_line(buf);
#else
    printf(" [%2d] %s (ID:%d) Cost:%d Dmg:%d Def:%d Heal:%d Type:%d Target:%d\n",
           idx, c->name, c->card_id, c->energy_cost, c->base_damage, c->base_defense, c->base_heal, c->type, c->target_type);
#endif
}

void RenderGameBoard(GameState state) {
#ifdef USE_EASYX
    reset_draw_y();
    char buf[256];
    snprintf(buf,sizeof(buf), "=== 游戏面板 (回合:%d) 当前行动: 玩家 %d ===", state.round_count, state.current_turn);
    draw_line(buf);

    snprintf(buf,sizeof(buf), "-- 玩家1: %s --", state.p1.name); draw_line(buf);
    draw_line(" Active: "); print_character(&state.p1.team[state.p1.active_idx]);
    snprintf(buf,sizeof(buf), " Energy: %d/%d  手牌:%d  抽牌堆:%d 弃牌堆:%d", state.p1.energy, state.p1.max_energy, state.p1.hand_count, state.p1.draw_count, state.p1.discard_count); draw_line(buf);
    if (state.p1.hand_count > 0) {
        draw_line(" 手牌:");
        for (int i = 0; i < state.p1.hand_count; i++) {
            print_card(&state.p1.hand[i], i);
        }
    }

    snprintf(buf,sizeof(buf), "\n-- 玩家2: %s --", state.p2.name); draw_line(buf);
    draw_line(" Active: "); print_character(&state.p2.team[state.p2.active_idx]);
    snprintf(buf,sizeof(buf), " Energy: %d/%d  手牌:%d  抽牌堆:%d 弃牌堆:%d", state.p2.energy, state.p2.max_energy, state.p2.hand_count, state.p2.draw_count, state.p2.discard_count); draw_line(buf);
    if (state.p2.hand_count > 0) {
        draw_line(" 手牌:");
        for (int i = 0; i < state.p2.hand_count; i++) {
            print_card(&state.p2.hand[i], i);
        }
    }

    draw_line("========================================");
#else
    printf("\n=== 游戏面板 (回合:%d) 当前行动: 玩家 %d ===\n", state.round_count, state.current_turn);

    printf("-- 玩家1: %s --\n", state.p1.name);
    printf(" Active: "); print_character(&state.p1.team[state.p1.active_idx]);
    printf(" Energy: %d/%d  手牌:%d  抽牌堆:%d 弃牌堆:%d\n", state.p1.energy, state.p1.max_energy, state.p1.hand_count, state.p1.draw_count, state.p1.discard_count);
    if (state.p1.hand_count > 0) {
        printf(" 手牌:\n");
        for (int i = 0; i < state.p1.hand_count; i++) {
            print_card(&state.p1.hand[i], i);
        }
    }

    printf("\n-- 玩家2: %s --\n", state.p2.name);
    printf(" Active: "); print_character(&state.p2.team[state.p2.active_idx]);
    printf(" Energy: %d/%d  手牌:%d  抽牌堆:%d 弃牌堆:%d\n", state.p2.energy, state.p2.max_energy, state.p2.hand_count, state.p2.draw_count, state.p2.discard_count);
    if (state.p2.hand_count > 0) {
        printf(" 手牌:\n");
        for (int i = 0; i < state.p2.hand_count; i++) {
            print_card(&state.p2.hand[i], i);
        }
    }

    printf("========================================\n");
#endif
}

static void wait_for_enter(const char* prompt) {
#ifdef USE_EASYX
    draw_line(prompt);
    _getch();
#else
    printf("%s", prompt);
    fflush(stdout);
    int c = getchar();
    while (c != '\n' && c != EOF) { if (c == '\r') break; c = getchar(); }
#endif
}

void ShowTurnTransitionMask(int player_id) {
#ifdef USE_EASYX
    char buf[128]; snprintf(buf,sizeof(buf), "---- 轮到 玩家 %d ----", player_id); draw_line(buf);
    wait_for_enter("按任意键继续...");
#else
    printf("\n---- 轮到 玩家 %d ----\n", player_id);
    wait_for_enter("按回车继续...\n");
#endif
}

Action GetHumanInputFromUI(int player_id, GameState state) {
    Action act;
    act.type = ACTION_NONE;
    act.actor_id = player_id;
    act.card_hand_idx = -1;
    act.switch_to_idx = -1;
    act.target_idx = -1;

    Player* p;
    if (player_id == state.p1.player_id) {
        p = (Player*)&state.p1;
    } else if (player_id == state.p2.player_id) {
        p = (Player*)&state.p2;
    } else {
        p = (player_id == 1) ? (Player*)&state.p1 : (Player*)&state.p2;
    }

    while (1) {
        printf("\n玩家 %d 操作选择：\n", player_id);
        printf(" 0: 结束回合\n 1: 出牌\n 2: 切换角色\n");
        printf("\n请输入选项编号: ");
        int opt = -1;
        if (scanf("%d", &opt) != 1) { while(getchar()!='\n'); opt = -1; }
        // 清理换行
        int ch = getchar(); if (ch != '\n' && ch != EOF) while (getchar()!='\n');

        if (opt == 0) { act.type = ACTION_END_TURN; return act; }
        else if (opt == 1) {
            if (p->hand_count == 0) { printf("手牌为空，无法出牌。\n"); continue; }
            printf("请选择手牌索引 (0..%d): ", p->hand_count - 1);
            int idx = -1;
            if (scanf("%d", &idx) != 1) { while(getchar()!='\n'); printf("输入无效\n"); continue; }
            while(getchar()!='\n');
            if (idx < 0 || idx >= p->hand_count) { printf("索引越界\n"); continue; }
            act.type = ACTION_PLAY_CARD;
            act.card_hand_idx = idx;
            act.actor_id = player_id;
            // 简化：若卡为单体目标，询问目标队伍索引；若为全体则填0
            Card* c = &p->hand[idx];
            if (c->target_type == TARGET_ENEMY_SINGLE || c->target_type == TARGET_SELF_SINGLE) {
                printf("请选择目标索引 (0..%d): ", TEAM_SIZE - 1);
                int tid = -1;
                if (scanf("%d", &tid) != 1) { while(getchar()!='\n'); printf("输入无效\n"); continue; }
                while(getchar()!='\n');
                act.target_idx = tid;
            } else {
                act.target_idx = 0; // 代表全体
            }
            return act;
        }
        else if (opt == 2) {
            printf("可切换的队伍成员:\n");
            for (int i = 0; i < TEAM_SIZE; i++) {
                printf(" %d: ", i); print_character(&p->team[i]);
            }
            printf("请选择切换到的索引 (0..%d): ", TEAM_SIZE - 1);
            int s = -1;
            if (scanf("%d", &s) != 1) { while(getchar()!='\n'); printf("输入无效\n"); continue; }
            while(getchar()!='\n');
            if (s < 0 || s >= TEAM_SIZE || !p->team[s].is_alive) { printf("无效的索引或角色已阵亡\n"); continue; }
            act.type = ACTION_SWITCH_CHAR;
            act.switch_to_idx = s;
            act.actor_id = player_id;
            return act;
        }
        else {
            printf("未知选项，请重试。\n");
        }
    }
}

int SelectCharacterFromUI(int player_id, int slot_number) {
#ifdef USE_EASYX
    char buf[128]; snprintf(buf,sizeof(buf), "[角色选择] 玩家 %d 为槽位 %d 选择角色", player_id, slot_number); draw_line(buf);
    if (g_char_count == 0) { draw_line("全局角色池为空，返回0"); return 0; }
    for (int i = 0; i < g_char_count; i++) {
        char tmp[128]; snprintf(tmp,sizeof(tmp), " %2d: %s (ID:%d) HP:%d Speed:%d", i, g_all_characters[i].name, g_all_characters[i].char_id, g_all_characters[i].max_hp, g_all_characters[i].speed); draw_line(tmp);
    }
    draw_line("输入选择的索引 (在控制台输入):");
    int sel = -1;
    if (scanf("%d", &sel) != 1) { while(getchar()!='\n'); sel = 0; }
    while(getchar()!='\n');
    if (sel < 0 || sel >= g_char_count) sel = 0;
    char chosen[128]; snprintf(chosen,sizeof(chosen), "选择: %s", g_all_characters[sel].name); draw_line(chosen);
    return sel;
#else
    printf("\n[角色选择] 玩家 %d 为槽位 %d 选择角色\n", player_id, slot_number);
    if (g_char_count == 0) { printf("全局角色池为空，返回0\n"); return 0; }
    for (int i = 0; i < g_char_count; i++) {
        printf(" %2d: %s (ID:%d) HP:%d Speed:%d\n", i, g_all_characters[i].name, g_all_characters[i].char_id, g_all_characters[i].max_hp, g_all_characters[i].speed);
    }
    printf("输入选择的索引 (0..%d): ", g_char_count - 1);
    int sel = -1;
    if (scanf("%d", &sel) != 1) { while(getchar()!='\n'); sel = 0; }
    while(getchar()!='\n');
    if (sel < 0 || sel >= g_char_count) sel = 0;
    printf("选择: %s\n", g_all_characters[sel].name);
    return sel;
#endif
}

int SelectCardFromUI(int player_id, int current_deck_size) {
#ifdef USE_EASYX
    char buf[128]; snprintf(buf,sizeof(buf), "[卡牌构筑] 玩家 %d 选择第 %d 张卡牌 (输入 -1 结束)", player_id, current_deck_size + 1); draw_line(buf);
    if (g_card_count == 0) { draw_line("全局卡池为空，返回 -1"); return -1; }
    int show = g_card_count < 20 ? g_card_count : 20;
    for (int i = 0; i < show; i++) {
        print_card(&g_all_cards[i], i);
    }
    draw_line("输入要加入的卡牌索引 (在控制台输入) :");
    int sel = -2;
    if (scanf("%d", &sel) != 1) { while(getchar()!='\n'); sel = -1; }
    while(getchar()!='\n');
    if (sel == -1) return -1;
    if (sel < 0 || sel >= g_card_count) sel = 0;
    char chosen[128]; snprintf(chosen,sizeof(chosen), "已选择: %s", g_all_cards[sel].name); draw_line(chosen);
    return sel;
#else
    printf("\n[卡牌构筑] 玩家 %d 选择第 %d 张卡牌 (输入 -1 结束)\n", player_id, current_deck_size + 1);
    if (g_card_count == 0) { printf("全局卡池为空，返回 -1\n"); return -1; }
    int show = g_card_count < 20 ? g_card_count : 20;
    for (int i = 0; i < show; i++) {
        print_card(&g_all_cards[i], i);
    }
    printf("输入要加入的卡牌索引 (0..%d) 或 -1 结束: ", g_card_count - 1);
    int sel = -2;
    if (scanf("%d", &sel) != 1) { while(getchar()!='\n'); sel = -1; }
    while(getchar()!='\n');
    if (sel == -1) return -1;
    if (sel < 0 || sel >= g_card_count) sel = 0;
    printf("已选择: %s\n", g_all_cards[sel].name);
    return sel;
#endif
}

int GetModeSelectionFromUI() {
    printf("\n主菜单：选择模式 0=本地PvP 1=人机PvE (默认0): ");
    int m = MODE_PVP;
    if (scanf("%d", &m) != 1) { while(getchar()!='\n'); m = MODE_PVP; }
    while(getchar()!='\n');
    if (m != MODE_PVE) m = MODE_PVP;
    printf("选择模式: %s\n", (m == MODE_PVE) ? "PvE" : "PvP");
    return m;
}
