#include "gui_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src_data/data_manager.h"
#ifdef USE_EASYX
#include <graphics.h>
#include <conio.h>
#include <windows.h>
#include <string>
static int g_draw_y = 10;
static void reset_draw_y() {
    g_draw_y = 10;
    setbkcolor(WHITE);
    settextcolor(BLACK);
    cleardevice();
}

// Convert UTF-8 C string to current ANSI code page string
static std::string utf8_to_acp_str(const char* utf8) {
    if (!utf8) return std::string();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wlen == 0) return std::string();
    wchar_t* wbuf = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wbuf, wlen);
    int alen = WideCharToMultiByte(CP_ACP, 0, wbuf, -1, NULL, 0, NULL, NULL);
    char* abuf = (char*)malloc(alen);
    WideCharToMultiByte(CP_ACP, 0, wbuf, -1, abuf, alen, NULL, NULL);
    std::string s(abuf);
    free(wbuf);
    free(abuf);
    return s;
}

// Helper wrappers that accept UTF-8 literals
static void outtextxy_utf8(int x, int y, const char* utf8) {
    std::string s = utf8_to_acp_str(utf8);
    outtextxy(x, y, s.c_str());
}
static void settextstyle_utf8(int height, int width, const char* utf8Name) {
    std::string s = utf8_to_acp_str(utf8Name);
    settextstyle(height, width, s.c_str());
}
static void draw_line(const char* s) { outtextxy_utf8(10, g_draw_y, s); g_draw_y += 24; }
#ifdef USE_EASYX
static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

static void draw_button(int x, int y, int w, int h, const char* label) {
    setlinecolor(BLACK);
    settextcolor(BLACK);
    rectangle(x, y, x + w, y + h);
    outtextxy_utf8(x + 6, y + 6, label);
}

static int wait_click_in_rect(int rx, int ry, int rw, int rh) {
    MOUSEMSG m;
    while (1) {
        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (point_in_rect(m.x, m.y, rx, ry, rw, rh)) return 1;
        }
    }
    return 0;
}
#endif
#else
// no graphical helpers
#endif

// 简易基于控制台的 GUI 实现，便于在没有图形库时交互和调试。

void InitGUI() {
#ifdef USE_EASYX
    initgraph(800,600);
    setbkcolor(WHITE);
    settextcolor(BLACK);
    cleardevice();
    settextstyle_utf8(20,0,"SimSun");
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
    snprintf(buf,sizeof(buf), "=== Game Board (Round:%d) Current Turn: Player %d ===", state.round_count, state.current_turn);
    draw_line(buf);

    snprintf(buf,sizeof(buf), "-- Player1: %s --", state.p1.name); draw_line(buf);
    draw_line(" Active: "); print_character(&state.p1.team[state.p1.active_idx]);
    snprintf(buf,sizeof(buf), " Energy: %d/%d  Hand:%d  Draw:%d Discard:%d", state.p1.energy, state.p1.max_energy, state.p1.hand_count, state.p1.draw_count, state.p1.discard_count); draw_line(buf);
    if (state.p1.hand_count > 0) {
        draw_line(" Hand:");
        for (int i = 0; i < state.p1.hand_count; i++) {
            print_card(&state.p1.hand[i], i);
        }
    }

    snprintf(buf,sizeof(buf), "\n-- Player2: %s --", state.p2.name); draw_line(buf);
    draw_line(" Active: "); print_character(&state.p2.team[state.p2.active_idx]);
    snprintf(buf,sizeof(buf), " Energy: %d/%d  Hand:%d  Draw:%d Discard:%d", state.p2.energy, state.p2.max_energy, state.p2.hand_count, state.p2.draw_count, state.p2.discard_count); draw_line(buf);
    if (state.p2.hand_count > 0) {
        draw_line(" Hand:");
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
    Sleep(150);
    while (1) {
        if ((GetAsyncKeyState(VK_RETURN) & 0x8000) ||
            (GetAsyncKeyState(VK_SPACE) & 0x8000) ||
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
            break;
        }
        Sleep(16);
    }
    while ((GetAsyncKeyState(VK_RETURN) & 0x8000) ||
           (GetAsyncKeyState(VK_SPACE) & 0x8000) ||
           (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        Sleep(16);
    }
#else
    printf("%s", prompt);
    fflush(stdout);
    int c = getchar();
    while (c != '\n' && c != EOF) { if (c == '\r') break; c = getchar(); }
#endif
}

void ShowTurnTransitionMask(int player_id) {
#ifdef USE_EASYX
    char buf[128]; snprintf(buf,sizeof(buf), "---- Turn: Player %d ----", player_id); draw_line(buf);
    wait_for_enter("Click, Space, or Enter to continue...");
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

#ifdef USE_EASYX
    reset_draw_y();
    draw_line("Please choose action:");
    int bx = 30, by = g_draw_y + 10, bw = 200, bh = 40;
    draw_button(bx, by, bw, bh, "End Turn");
    draw_button(bx, by + 60, bw, bh, "Play Card");
    draw_button(bx, by + 120, bw, bh, "Switch Character");

    MOUSEMSG m;
    while (1) {
        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) { act.type = ACTION_END_TURN; return act; }
            else if (point_in_rect(m.x, m.y, bx, by + 60, bw, bh)) {
                if (p->hand_count == 0) { draw_line("Hand empty, cannot play."); continue; }
                // 显示手牌为可点按钮
                int hx = 260, hy = by, hw = 220, hh = 40;
                for (int i = 0; i < p->hand_count; i++) {
                    char buf[128]; snprintf(buf, sizeof(buf), "[%d] %s", i, p->hand[i].name);
                    draw_button(hx, hy + i * (hh + 8), hw, hh, buf);
                }
                // 等待手牌点击
                while (1) {
                    m = GetMouseMsg();
                    if (m.uMsg == WM_LBUTTONDOWN) {
                        for (int i = 0; i < p->hand_count; i++) {
                            int rx = hx, ry = hy + i * (hh + 8);
                            if (point_in_rect(m.x, m.y, rx, ry, hw, hh)) {
                                act.type = ACTION_PLAY_CARD;
                                act.card_hand_idx = i;
                                act.actor_id = player_id;
                                Card* c = &p->hand[i];
                                if (c->target_type == TARGET_ENEMY_SINGLE || c->target_type == TARGET_SELF_SINGLE) {
                                    // 显示目标选择（简化为显示 TEAM_SIZE 个按钮）
                                    int tx = 30, ty = hy + p->hand_count * (hh + 8) + 20, tw = 140, th = 40;
                                    for (int t = 0; t < TEAM_SIZE; t++) {
                                        char tb[64]; snprintf(tb, sizeof(tb), "Target %d", t);
                                        draw_button(tx + t * (tw + 8), ty, tw, th, tb);
                                    }
                                    while (1) {
                                        m = GetMouseMsg();
                                        if (m.uMsg == WM_LBUTTONDOWN) {
                                            for (int t = 0; t < TEAM_SIZE; t++) {
                                                int rx2 = tx + t * (tw + 8);
                                                if (point_in_rect(m.x, m.y, rx2, ty, tw, th)) {
                                                    act.target_idx = t;
                                                    return act;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    act.target_idx = 0; // 全体
                                    return act;
                                }
                            }
                        }
                    }
                }
            }
            else if (point_in_rect(m.x, m.y, bx, by + 120, bw, bh)) {
                // 显示可切换角色
                int sx = 260, sy = by, sw = 300, sh = 48;
                for (int i = 0; i < TEAM_SIZE; i++) {
                    char tmp[128]; snprintf(tmp, sizeof(tmp), "[%d] %s", i, p->team[i].name);
                    draw_button(sx, sy + i * (sh + 8), sw, sh, tmp);
                }
                while (1) {
                    m = GetMouseMsg();
                    if (m.uMsg == WM_LBUTTONDOWN) {
                        for (int i = 0; i < TEAM_SIZE; i++) {
                            int rx = sx, ry = sy + i * (sh + 8);
                            if (point_in_rect(m.x, m.y, rx, ry, sw, sh)) {
                                if (!p->team[i].is_alive) { draw_line("Character is dead"); break; }
                                act.type = ACTION_SWITCH_CHAR;
                                act.switch_to_idx = i;
                                act.actor_id = player_id;
                                return act;
                            }
                        }
                    }
                }
            }
        }
    }
#else
    // 退回控制台实现
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
            Card* c = &p->hand[idx];
            if (c->target_type == TARGET_ENEMY_SINGLE || c->target_type == TARGET_SELF_SINGLE) {
                printf("请选择目标索引 (0..%d): ", TEAM_SIZE - 1);
                int tid = -1;
                if (scanf("%d", &tid) != 1) { while(getchar()!='\n'); printf("输入无效\n"); continue; }
                while(getchar()!='\n');
                act.target_idx = tid;
            } else { act.target_idx = 0; }
            return act;
        }
        else if (opt == 2) {
            printf("可切换的队伍成员:\n");
            for (int i = 0; i < TEAM_SIZE; i++) { printf(" %d: ", i); print_character(&p->team[i]); }
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
        else { printf("未知选项，请重试。\n"); }
    }
#endif
}


int SelectCharacterFromUI(int player_id, int slot_number) {
#ifdef USE_EASYX
    reset_draw_y();
    char buf[128]; snprintf(buf, sizeof(buf), "[Character Select] Player %d choose for slot %d", player_id, slot_number); draw_line(buf);
    if (g_char_count == 0) { draw_line("Global character pool empty, returning 0"); return 0; }
    // 绘制为可点击列表
    int sx = 30, sy = g_draw_y + 10, sw = 640, sh = 36;
    for (int i = 0; i < g_char_count; i++) {
        char tmp[256]; snprintf(tmp, sizeof(tmp), "[%2d] %s (ID:%d) HP:%d Speed:%d", i, g_all_characters[i].name, g_all_characters[i].char_id, g_all_characters[i].max_hp, g_all_characters[i].speed);
        draw_button(sx, sy + i * (sh + 6), sw, sh, tmp);
    }
    MOUSEMSG m;
    while (1) {
        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            for (int i = 0; i < g_char_count; i++) {
                int rx = sx, ry = sy + i * (sh + 6);
                if (point_in_rect(m.x, m.y, rx, ry, sw, sh)) {
                    char chosen[128]; snprintf(chosen, sizeof(chosen), "Selected: %s", g_all_characters[i].name); draw_line(chosen);
                    return i;
                }
            }
        }
    }
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
    reset_draw_y();
    char buf[128]; snprintf(buf, sizeof(buf), "[Deck Build] Player %d selecting card %d", player_id, current_deck_size + 1); draw_line(buf);
    if (g_card_count == 0) { draw_line("Global card pool empty, returning -1"); return -1; }
    int show = g_card_count < 20 ? g_card_count : 20;
    int cx = 30, cy = g_draw_y + 10, cw = 340, ch = 36;
    for (int i = 0; i < show; i++) {
        char tmp[256]; snprintf(tmp, sizeof(tmp), "[%2d] %s", i, g_all_cards[i].name);
        draw_button(cx, cy + i * (ch + 6), cw, ch, tmp);
    }
    // add finish button
    draw_button(cx + cw + 20, cy + show * (ch + 6), 120, 40, "Finish Build");
    MOUSEMSG m;
    while (1) {
        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            for (int i = 0; i < show; i++) {
                int rx = cx, ry = cy + i * (ch + 6);
                if (point_in_rect(m.x, m.y, rx, ry, cw, ch)) {
                    char chosen[128]; snprintf(chosen, sizeof(chosen), "Selected: %s", g_all_cards[i].name); draw_line(chosen);
                    return i;
                }
            }
            // 结束按钮
            if (point_in_rect(m.x, m.y, cx + cw + 20, cy + show * (ch + 6), 120, 40)) {
                return -1;
            }
        }
    }
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
#ifdef USE_EASYX
    reset_draw_y(); draw_line("Main Menu: Select mode:");
    int bx = 60, by = g_draw_y + 10, bw = 240, bh = 60;
    draw_button(bx, by, bw, bh, "Local PvP (default)");
    draw_button(bx, by + 90, bw, bh, "AI PvE");
    MOUSEMSG m;
    while (1) {
        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) { draw_line("Mode: PvP"); return MODE_PVP; }
            if (point_in_rect(m.x, m.y, bx, by + 90, bw, bh)) { draw_line("Mode: PvE"); return MODE_PVE; }
        }
    }
#else
    printf("\n主菜单：选择模式 0=本地PvP 1=人机PvE (默认0): ");
    int m = MODE_PVP;
    if (scanf("%d", &m) != 1) { while(getchar()!='\n'); m = MODE_PVP; }
    while(getchar()!='\n');
    if (m != MODE_PVE) m = MODE_PVP;
    printf("选择模式: %s\n", (m == MODE_PVE) ? "PvE" : "PvP");
    return m;
#endif
}
