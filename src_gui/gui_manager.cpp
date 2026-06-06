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
static void reset_draw_y() { g_draw_y = 10; cleardevice(); }

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
    // Draw filled button with visible text to avoid overlap artifacts
    setfillcolor(WHITE);
    setlinecolor(BLACK);
    fillrectangle(x, y, x + w, y + h);
    rectangle(x, y, x + w, y + h);
    settextcolor(BLACK);
    outtextxy_utf8(x + 6, y + 6, label);
}

// Draw a button and mark it as checked (used to indicate a confirmed selection)
static void draw_button_with_check(int x, int y, int w, int h, const char* label) {
    draw_button(x, y, w, h, label);
    // draw a small '(selected)' label at the right edge of the button (ASCII safe)
    settextcolor(BLACK);
    outtextxy_utf8(x + w - 90, y + 6, "(selected)");
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

// Draw a non-intrusive overlay message at the bottom of the screen to avoid
// overlapping interactive UI elements (buttons, lists).
static void draw_overlay_message(const char* utf8msg) {
    std::string s = utf8_to_acp_str(utf8msg);
    int x = 20, w = 760, h = 40, y = 540; // bottom area for messages
    setfillcolor(WHITE);
    setlinecolor(BLACK);
    fillrectangle(x, y, x + w, y + h);
    rectangle(x, y, x + w, y + h);
    settextcolor(BLACK);
    outtextxy(x + 6, y + 10, s.c_str());
}

// Render both teams' characters and HP on a side panel (right side) so that
// their HP is always visible during the turn.
static void draw_team_hp_panel(const GameState* st) {
    if (!st) return;
    int x = 520, y = 10;
    char buf[128];
    setfillcolor(WHITE);
    setlinecolor(BLACK);
    // background for panel
    fillrectangle(x - 8, y - 4, 780, 220);
    rectangle(x - 8, y - 4, 780, 220);
    settextcolor(BLACK);
    snprintf(buf, sizeof(buf), "Player1: %s", st->p1.name);
    outtextxy_utf8(x, y, buf); y += 22;
    for (int i = 0; i < TEAM_SIZE; i++) {
        snprintf(buf, sizeof(buf), " P1[%d] %s  HP:%d/%d", i, st->p1.team[i].name, st->p1.team[i].hp, st->p1.team[i].max_hp);
        outtextxy_utf8(x, y, buf); y += 20;
    }
    y += 6;
    snprintf(buf, sizeof(buf), "Player2: %s", st->p2.name);
    outtextxy_utf8(x, y, buf); y += 22;
    for (int i = 0; i < TEAM_SIZE; i++) {
        snprintf(buf, sizeof(buf), " P2[%d] %s  HP:%d/%d", i, st->p2.team[i].name, st->p2.team[i].hp, st->p2.team[i].max_hp);
        outtextxy_utf8(x, y, buf); y += 20;
    }
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
    printf("\n=== Game Board (Round:%d) Current Turn: Player %d ===\n", state.round_count, state.current_turn);

    printf("-- Player1: %s --\n", state.p1.name);
    printf(" Active: "); print_character(&state.p1.team[state.p1.active_idx]);
    printf(" Energy: %d/%d  Hand:%d  Draw:%d Discard:%d\n", state.p1.energy, state.p1.max_energy, state.p1.hand_count, state.p1.draw_count, state.p1.discard_count);
    if (state.p1.hand_count > 0) {
        printf(" Hand:\n");
        for (int i = 0; i < state.p1.hand_count; i++) {
            print_card(&state.p1.hand[i], i);
        }
    }

    printf("\n-- Player2: %s --\n", state.p2.name);
    printf(" Active: "); print_character(&state.p2.team[state.p2.active_idx]);
    printf(" Energy: %d/%d  Hand:%d  Draw:%d Discard:%d\n", state.p2.energy, state.p2.max_energy, state.p2.hand_count, state.p2.draw_count, state.p2.discard_count);
    if (state.p2.hand_count > 0) {
        printf(" Hand:\n");
        for (int i = 0; i < state.p2.hand_count; i++) {
            print_card(&state.p2.hand[i], i);
        }
    }

    printf("========================================\n");
#endif
}

static void wait_for_enter(const char* prompt) {
#ifdef USE_EASYX
    // Draw prompt in a bottom overlay so it doesn't overlap buttons
    draw_overlay_message(prompt);
    // Wait for any key or mouse click using non-blocking checks so the EasyX window doesn't need console focus
    while (1) {
        // check for mouse clicks queued by EasyX
        if (MouseHit()) {
            MOUSEMSG mm = GetMouseMsg();
            if (mm.uMsg == WM_LBUTTONDOWN || mm.uMsg == WM_RBUTTONDOWN) return;
        }
        // check keyboard state (any key)
        for (int vk = 8; vk <= 255; vk++) {
            if (GetAsyncKeyState(vk) & 0x8000) return;
        }
        Sleep(10);
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
    char buf[128]; snprintf(buf,sizeof(buf), "---- Turn: Player %d ----", player_id); draw_overlay_message(buf);
    wait_for_enter("Press any key to continue...");
#else
    printf("\n---- Turn: Player %d ----\n", player_id);
    wait_for_enter("Press Enter to continue...\n");
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
    // render side panel with HP so it's always visible
    draw_team_hp_panel(&state);
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
                // 清理并显示手牌为可点按钮（避免文本重叠）
                reset_draw_y();
                draw_team_hp_panel(&state);
                draw_line("Select a hand card:");
                int hx = 260, hy = g_draw_y + 10, hw = 220, hh = 40;
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
                                Card* c = &p->hand[i];
                                // If not enough energy, show message and ignore this click
                                if (p->energy < c->energy_cost) {
                                    char msg[128]; snprintf(msg, sizeof(msg), "Not enough energy: %s requires %d, current %d/%d", c->name, c->energy_cost, p->energy, p->max_energy);
                                    draw_overlay_message(msg);
                                    break; // end for loop and wait for next click
                                }
                                // 构造动作并显示信息
                                act.type = ACTION_PLAY_CARD;
                                act.card_hand_idx = i;
                                act.actor_id = player_id;
                                {
                                    char ebuf[128];
                                    snprintf(ebuf, sizeof(ebuf), "Play: %s  Cost:%d  Remaining Energy:%d/%d",
                                             c->name, c->energy_cost, p->energy - c->energy_cost, p->max_energy);
                                    draw_overlay_message(ebuf);
                                }
                                if (c->target_type == TARGET_ENEMY_SINGLE || c->target_type == TARGET_SELF_SINGLE) {
                                    // 显示目标选择（简化为显示 TEAM_SIZE 个按钮，附带角色名）
                                    int tx = 30, ty = hy + p->hand_count * (hh + 8) + 20, tw = 200, th = 40;
                                    Character* target_team = NULL;
                                    if (c->target_type == TARGET_SELF_SINGLE) target_team = p->team;
                                    else {
                                        // 敌方队伍
                                        if (player_id == state.p1.player_id) target_team = (Character*)state.p2.team;
                                        else target_team = (Character*)state.p1.team;
                                    }
                                    for (int t = 0; t < TEAM_SIZE; t++) {
                                        char tb[128]; snprintf(tb, sizeof(tb), "Target %d: %s", t, target_team[t].name);
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
                // 清理并显示可切换角色（避免文本重叠）
                reset_draw_y();
                draw_team_hp_panel(&state);
                draw_line("Select a character to switch:");
                int sx = 260, sy = g_draw_y + 10, sw = 300, sh = 48;
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
    // fallback console implementation
    while (1) {
        printf("\nPlayer %d - choose action:\n", player_id);
        printf(" 0: End Turn\n 1: Play Card\n 2: Switch Character\n");
        printf("\nEnter option number: ");
        int opt = -1;
        if (scanf("%d", &opt) != 1) { while(getchar()!='\n'); opt = -1; }
        // consume remaining newline
        int ch = getchar(); if (ch != '\n' && ch != EOF) while (getchar()!='\n');

        if (opt == 0) { act.type = ACTION_END_TURN; return act; }
        else if (opt == 1) {
            if (p->hand_count == 0) { printf("Hand empty, cannot play.\n"); continue; }
            printf("Choose hand index (0..%d): ", p->hand_count - 1);
            int idx = -1;
            if (scanf("%d", &idx) != 1) { while(getchar()!='\n'); printf("Invalid input\n"); continue; }
            while(getchar()!='\n');
            if (idx < 0 || idx >= p->hand_count) { printf("Index out of range\n"); continue; }
            Card* c = &p->hand[idx];
            if (p->energy < c->energy_cost) {
                printf("Not enough energy: %s requires %d, current %d/%d\n", c->name, c->energy_cost, p->energy, p->max_energy);
                continue;
            }
            act.type = ACTION_PLAY_CARD;
            act.card_hand_idx = idx;
            act.actor_id = player_id;
            printf("Play: %s  Cost:%d  Remaining Energy:%d/%d\n", c->name, c->energy_cost, p->energy - c->energy_cost, p->max_energy);
            if (c->target_type == TARGET_ENEMY_SINGLE || c->target_type == TARGET_SELF_SINGLE) {
                // list available targets with names
                Player* enemy = (player_id == state.p1.player_id) ? (Player*)&state.p2 : (Player*)&state.p1;
                if (c->target_type == TARGET_ENEMY_SINGLE) {
                    printf("Available targets (enemy):\n");
                    for (int t = 0; t < TEAM_SIZE; t++) printf(" %d: %s\n", t, enemy->team[t].name);
                } else {
                    printf("Available targets (self):\n");
                    for (int t = 0; t < TEAM_SIZE; t++) printf(" %d: %s\n", t, p->team[t].name);
                }
                printf("Please choose target index (0..%d): ", TEAM_SIZE - 1);
                int tid = -1;
                if (scanf("%d", &tid) != 1) { while(getchar()!='\n'); printf("Invalid input\n"); continue; }
                while(getchar()!='\n');
                act.target_idx = tid;
            } else { act.target_idx = 0; }
            return act;
        }
        else if (opt == 2) {
            printf("Switchable team members:\n");
            for (int i = 0; i < TEAM_SIZE; i++) { printf(" %d: ", i); print_character(&p->team[i]); }
            printf("Choose index to switch to (0..%d): ", TEAM_SIZE - 1);
            int s = -1;
            if (scanf("%d", &s) != 1) { while(getchar()!='\n'); printf("Invalid input\n"); continue; }
            while(getchar()!='\n');
            if (s < 0 || s >= TEAM_SIZE || !p->team[s].is_alive) { printf("Invalid index or character is dead\n"); continue; }
            act.type = ACTION_SWITCH_CHAR;
            act.switch_to_idx = s;
            act.actor_id = player_id;
            return act;
        }
        else { printf("Unknown option, please try again.\n"); }
    }
#endif
}


int SelectCharacterFromUI(int player_id, int slot_number) {
#ifdef USE_EASYX
    reset_draw_y();
    char buf[128]; snprintf(buf, sizeof(buf), "[Character Select] Player %d choose for slot %d", player_id, slot_number); draw_line(buf);
    if (g_char_count == 0) { draw_line("Global character pool empty, returning 0"); return 0; }
    // Draw clickable list but keep selection until confirmed
    int sx = 30, sy = g_draw_y + 10, sw = 640, sh = 36;
    int confirm_x = sx + sw + 20, confirm_y = sy + g_char_count * (sh + 6);
    int selected_idx = -1;
    MOUSEMSG m;
    while (1) {
        // redraw list with selection marker
        for (int i = 0; i < g_char_count; i++) {
            char tmp[256]; snprintf(tmp, sizeof(tmp), "[%2d] %s (ID:%d) HP:%d Speed:%d", i, g_all_characters[i].name, g_all_characters[i].char_id, g_all_characters[i].max_hp, g_all_characters[i].speed);
            if (i == selected_idx) draw_button_with_check(sx, sy + i * (sh + 6), sw, sh, tmp);
            else draw_button(sx, sy + i * (sh + 6), sw, sh, tmp);
        }
        draw_button(confirm_x, confirm_y, 120, 40, "Confirm");

        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            for (int i = 0; i < g_char_count; i++) {
                int rx = sx, ry = sy + i * (sh + 6);
                if (point_in_rect(m.x, m.y, rx, ry, sw, sh)) {
                    selected_idx = i;
                    char overlay[128]; snprintf(overlay, sizeof(overlay), "Selected: %s", g_all_characters[i].name); draw_overlay_message(overlay);
                    break;
                }
            }
            // confirm button
            if (point_in_rect(m.x, m.y, confirm_x, confirm_y, 120, 40)) {
                if (selected_idx >= 0) {
                    char chosen[128]; snprintf(chosen, sizeof(chosen), "Confirmed: %s", g_all_characters[selected_idx].name); draw_overlay_message(chosen);
                    Sleep(200);
                    return selected_idx;
                } else {
                    draw_overlay_message("No character selected");
                    Sleep(200);
                }
            }
        }
    }
#else
    printf("\n[Character Select] Player %d choose for slot %d\n", player_id, slot_number);
    if (g_char_count == 0) { printf("Global character pool empty, returning 0\n"); return 0; }
    for (int i = 0; i < g_char_count; i++) {
        printf(" %2d: %s (ID:%d) HP:%d Speed:%d\n", i, g_all_characters[i].name, g_all_characters[i].char_id, g_all_characters[i].max_hp, g_all_characters[i].speed);
    }
    printf("Enter selection index (0..%d): ", g_char_count - 1);
    int sel = -1;
    if (scanf("%d", &sel) != 1) { while(getchar()!='\n'); sel = 0; }
    while(getchar()!='\n');
    if (sel < 0 || sel >= g_char_count) sel = 0;
    printf("Selected: %s\n", g_all_characters[sel].name);
    return sel;
#endif
}

int SelectCardFromUI(int player_id, int current_deck_size) {
#ifdef USE_EASYX
    reset_draw_y();
    char buf[128]; snprintf(buf, sizeof(buf), "[Deck Build] Player %d - Selected %d so far", player_id, current_deck_size); draw_line(buf);
    if (g_card_count == 0) { draw_line("Global card pool empty, returning -1"); return -1; }
    int show = g_card_count < 20 ? g_card_count : 20;
    int cx = 30, cy = g_draw_y + 10, cw = 340, ch = 36;
    int confirm_x = cx + cw + 20, confirm_y = cy + show * (ch + 6);
    int selected_idx = -1;
    MOUSEMSG m;
    while (1) {
        // redraw list with selection marker
        for (int i = 0; i < show; i++) {
            char tmp[256]; snprintf(tmp, sizeof(tmp), "[%2d] %s", i, g_all_cards[i].name);
            if (i == selected_idx) draw_button_with_check(cx, cy + i * (ch + 6), cw, ch, tmp);
            else draw_button(cx, cy + i * (ch + 6), cw, ch, tmp);
        }
        // add finish and confirm buttons
        draw_button(confirm_x, confirm_y, 120, 40, "Finish Build");
        draw_button(confirm_x, confirm_y + 60, 120, 40, "Confirm");

        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            for (int i = 0; i < show; i++) {
                int rx = cx, ry = cy + i * (ch + 6);
                if (point_in_rect(m.x, m.y, rx, ry, cw, ch)) {
                    selected_idx = i;
                    char chosen[128]; snprintf(chosen, sizeof(chosen), "Selected: %s", g_all_cards[i].name); draw_overlay_message(chosen);
                    break;
                }
            }
            // finish button
            if (point_in_rect(m.x, m.y, confirm_x, confirm_y, 120, 40)) {
                return -1;
            }
            // confirm button
            if (point_in_rect(m.x, m.y, confirm_x, confirm_y + 60, 120, 40)) {
                if (selected_idx >= 0) {
                    char chosen[128]; snprintf(chosen, sizeof(chosen), "Confirmed: %s", g_all_cards[selected_idx].name); draw_overlay_message(chosen);
                    Sleep(200);
                    return selected_idx;
                } else {
                    draw_overlay_message("No card selected");
                    Sleep(200);
                }
            }
        }
    }
#else
    printf("\n[Deck Build] Player %d - Selected %d so far (input -1 to finish)\n", player_id, current_deck_size);
    if (g_card_count == 0) { printf("Global card pool empty, returning -1\n"); return -1; }
    int show = g_card_count < 20 ? g_card_count : 20;
    for (int i = 0; i < show; i++) {
        print_card(&g_all_cards[i], i);
    }
    printf("Enter index to add (0..%d) or -1 to finish: ", g_card_count - 1);
    int sel = -2;
    if (scanf("%d", &sel) != 1) { while(getchar()!='\n'); sel = -1; }
    while(getchar()!='\n');
    if (sel == -1) return -1;
    if (sel < 0 || sel >= g_card_count) sel = 0;
    printf("Selected: %s\n", g_all_cards[sel].name);
    return sel;
#endif
}

int GetModeSelectionFromUI() {
#ifdef USE_EASYX
    reset_draw_y(); draw_line("Main Menu: Select mode:");
    int bx = 60, by = g_draw_y + 10, bw = 240, bh = 60;
    int confirm_x = bx + bw + 20, confirm_y = by + 120;
    int selected_mode = -1;
    MOUSEMSG m;
    while (1) {
        // redraw options
        if (selected_mode == MODE_PVP) draw_button_with_check(bx, by, bw, bh, "Local PvP (default)");
        else draw_button(bx, by, bw, bh, "Local PvP (default)");
        if (selected_mode == MODE_PVE) draw_button_with_check(bx, by + 90, bw, bh, "AI PvE");
        else draw_button(bx, by + 90, bw, bh, "AI PvE");
        draw_button(confirm_x, confirm_y, 120, 40, "Confirm");

        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) { selected_mode = MODE_PVP; draw_overlay_message("Selected: PvP"); }
            else if (point_in_rect(m.x, m.y, bx, by + 90, bw, bh)) { selected_mode = MODE_PVE; draw_overlay_message("Selected: PvE"); }
            else if (point_in_rect(m.x, m.y, confirm_x, confirm_y, 120, 40)) {
                if (selected_mode == MODE_PVP || selected_mode == MODE_PVE) {
                    char msg[64]; snprintf(msg, sizeof(msg), "Mode confirmed: %s", (selected_mode == MODE_PVE) ? "PvE" : "PvP");
                    draw_overlay_message(msg); Sleep(200);
                    return selected_mode;
                } else {
                    draw_overlay_message("No mode selected"); Sleep(200);
                }
            }
        }
    }
#else
    printf("\nMain Menu: Select mode 0=Local PvP 1=AI PvE (default 0): ");
    int m = MODE_PVP;
    if (scanf("%d", &m) != 1) { while(getchar()!='\n'); m = MODE_PVP; }
    while(getchar()!='\n');
    if (m != MODE_PVE) m = MODE_PVP;
    printf("Mode selected: %s\n", (m == MODE_PVE) ? "PvE" : "PvP");
    return m;
#endif
}
