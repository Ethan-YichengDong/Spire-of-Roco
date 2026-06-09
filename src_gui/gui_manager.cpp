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
void print_character(const Character* ch);

static IMAGE g_art_background;
static IMAGE g_art_main_panel;
static IMAGE g_art_status_panel;
static IMAGE g_art_side_panel;
static IMAGE g_art_records_panel;
static IMAGE g_art_message_panel;
static IMAGE g_art_button_idle;
static IMAGE g_art_button_selected;
static IMAGE g_art_button_disabled;
static IMAGE g_art_hp_fill;
static IMAGE g_art_energy_fill;
static IMAGE g_art_bar_frame;
static IMAGE g_art_card_plate;
static IMAGE g_art_portrait_normal;
static IMAGE g_art_portrait_water;
static IMAGE g_art_portrait_fire;
static IMAGE g_art_portrait_grass;
static IMAGE g_art_portrait_electric;
static int g_ui_assets_ready = 0;
static int g_ui_w = 1280;
static int g_ui_h = 720;
static int g_ui_margin = 24;
static int g_status_h = 44;
static int g_main_x = 24;
static int g_main_y = 60;
static int g_main_w = 820;
static int g_main_h = 580;
static int g_side_x = 900;
static int g_side_w = 356;
static int g_team_panel_y = 60;
static int g_team_panel_h = 300;
static int g_records_panel_y = 376;
static int g_records_panel_h = 260;
static int g_is_fullscreen = 1;
static int g_is_quitting = 0;
static int g_last_canvas_w = 0;
static int g_last_canvas_h = 0;
static int g_ui_layout_version = 0;

static void draw_background_shell();
static void present_frame();
static int layout_changed_since(int* seen_version);

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void update_layout() {
    int w = getwidth();
    int h = getheight();
    if (w <= 0) w = 1280;
    if (h <= 0) h = 720;

    g_ui_w = w;
    g_ui_h = h;
    g_ui_margin = clamp_int(w / 70, 18, 32);
    g_status_h = clamp_int(h / 22, 40, 52);
    g_main_x = g_ui_margin;
    g_main_y = g_status_h + g_ui_margin;
    g_side_w = clamp_int(w / 4, 340, 460);
    g_side_x = w - g_side_w - g_ui_margin;
    g_main_w = g_side_x - g_main_x - g_ui_margin;
    if (g_main_w < 520) g_main_w = w - (g_ui_margin * 2);
    g_main_h = h - g_main_y - 86;
    if (g_main_h < 420) g_main_h = h - g_main_y - 58;
    g_team_panel_y = g_main_y;
    g_team_panel_h = 304;
    g_records_panel_y = g_team_panel_y + g_team_panel_h + g_ui_margin;
    g_records_panel_h = h - g_records_panel_y - 86;
    if (g_records_panel_h < 220) g_records_panel_h = 220;
}

static int bottom_button_y() {
    return g_ui_h - g_ui_margin - 96;
}

static void invalidate_ui_assets() {
    g_ui_assets_ready = 0;
}

static void resize_canvas_for_mode(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (g_last_canvas_w == width && g_last_canvas_h == height) {
        update_layout();
        return;
    }
    Resize(NULL, width, height);
    g_last_canvas_w = width;
    g_last_canvas_h = height;
    invalidate_ui_assets();
    update_layout();
    g_ui_layout_version++;
}

static void apply_window_mode(int fullscreen) {
    HWND hwnd = GetHWnd();
    if (!hwnd) return;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (screen_w < 1024) screen_w = 1024;
    if (screen_h < 720) screen_h = 720;

    if (fullscreen) {
        SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, screen_w, screen_h, SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        MoveWindow(hwnd, 0, 0, screen_w, screen_h, TRUE);
        resize_canvas_for_mode(screen_w, screen_h);
        g_is_fullscreen = 1;
        return;
    }

    int client_w = clamp_int(screen_w / 2, 1024, screen_w);
    int client_h = clamp_int(screen_h / 2, 720, screen_h);
    if (client_w >= screen_w) client_w = clamp_int((screen_w * 4) / 5, 1024, screen_w);
    if (client_h >= screen_h) client_h = clamp_int((screen_h * 4) / 5, 720, screen_h);
    RECT wr = { 0, 0, client_w, client_h };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    int window_w = wr.right - wr.left;
    int window_h = wr.bottom - wr.top;
    int window_x = (screen_w - window_w) / 2;
    int window_y = (screen_h - window_h) / 2;
    SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_NOTOPMOST, window_x, window_y, window_w, window_h, SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    resize_canvas_for_mode(client_w, client_h);
    g_is_fullscreen = 0;
}

static void quit_game_from_gui() {
    if (g_is_quitting) return;
    g_is_quitting = 1;
    EndBatchDraw();
    closegraph();
    exit(0);
}

static void handle_global_hotkeys() {
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        quit_game_from_gui();
    }
    int alt_enter = ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(VK_RETURN) & 0x0001));
    if ((GetAsyncKeyState(VK_F11) & 0x0001) || alt_enter) {
        apply_window_mode(!g_is_fullscreen);
    }
}

static void load_ui_assets() {
    if (g_ui_assets_ready) return;
    update_layout();
    loadimage(&g_art_background, "assets\\ui\\battle_background.bmp", g_ui_w, g_ui_h, true);
    loadimage(&g_art_main_panel, "assets\\ui\\main_panel.bmp", g_main_w, g_main_h, true);
    loadimage(&g_art_status_panel, "assets\\ui\\status_panel.bmp", g_ui_w, g_status_h, true);
    loadimage(&g_art_side_panel, "assets\\ui\\side_panel.bmp", g_side_w, g_team_panel_h, true);
    loadimage(&g_art_records_panel, "assets\\ui\\records_panel.bmp", g_side_w, g_records_panel_h, true);
    loadimage(&g_art_message_panel, "assets\\ui\\message_panel.bmp", g_ui_w - (g_ui_margin * 2), 46, true);
    loadimage(&g_art_button_idle, "assets\\ui\\button_idle.bmp", 420, 60, true);
    loadimage(&g_art_button_selected, "assets\\ui\\button_selected.bmp", 420, 60, true);
    loadimage(&g_art_button_disabled, "assets\\ui\\button_disabled.bmp", 420, 60, true);
    loadimage(&g_art_hp_fill, "assets\\ui\\hp_bar_fill.bmp", 360, 10, true);
    loadimage(&g_art_energy_fill, "assets\\ui\\energy_bar_fill.bmp", 360, 10, true);
    loadimage(&g_art_bar_frame, "assets\\ui\\bar_frame.bmp", 364, 14, true);
    loadimage(&g_art_card_plate, "assets\\ui\\card_plate.bmp", 900, 72, true);
    loadimage(&g_art_portrait_normal, "assets\\ui\\portrait_normal.bmp", 32, 32, true);
    loadimage(&g_art_portrait_water, "assets\\ui\\portrait_water.bmp", 32, 32, true);
    loadimage(&g_art_portrait_fire, "assets\\ui\\portrait_fire.bmp", 32, 32, true);
    loadimage(&g_art_portrait_grass, "assets\\ui\\portrait_grass.bmp", 32, 32, true);
    loadimage(&g_art_portrait_electric, "assets\\ui\\portrait_electric.bmp", 32, 32, true);
    g_ui_assets_ready = 1;
}

static void draw_art_or_fill(IMAGE* img, int x, int y, int w, int h, COLORREF fill) {
    if (img && img->getwidth() > 0 && img->getheight() > 0) {
        putimage(x, y, w, h, img, 0, 0);
        return;
    }
    setfillcolor(fill);
    fillrectangle(x, y, x + w, y + h);
}

static IMAGE* portrait_for_element(ElementType element) {
    switch (element) {
        case ELEMENT_WATER: return &g_art_portrait_water;
        case ELEMENT_FIRE: return &g_art_portrait_fire;
        case ELEMENT_GRASS: return &g_art_portrait_grass;
        case ELEMENT_ELECTRIC: return &g_art_portrait_electric;
        case ELEMENT_NORMAL:
        default: return &g_art_portrait_normal;
    }
}

static void draw_background_shell() {
    load_ui_assets();
    update_layout();
    draw_art_or_fill(&g_art_background, 0, 0, g_ui_w, g_ui_h, RGB(37, 46, 54));
    draw_art_or_fill(&g_art_main_panel, g_main_x, g_main_y, g_main_w, g_main_h, RGB(238, 231, 201));
}

static void reset_draw_y() {
    update_layout();
    g_draw_y = g_main_y + 14;
    setbkcolor(RGB(238, 231, 201));
    settextcolor(RGB(31, 37, 41));
    cleardevice();
    draw_background_shell();
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
static void draw_line(const char* s) { outtextxy_utf8(g_main_x + 16, g_draw_y, s); g_draw_y += 26; }
#ifdef USE_EASYX
static void draw_overlay_message(const char* utf8msg);

static void present_frame() {
    FlushBatchDraw();
}

static void draw_status_bar(const GameState* st, int acting_player_id, const char* phase) {
    if (!st) return;
    update_layout();
    char buf[256];
    draw_art_or_fill(&g_art_status_panel, 0, 0, g_ui_w, g_status_h, RGB(238, 244, 252));
    setlinecolor(RGB(70, 95, 130));
    rectangle(0, 0, g_ui_w - 1, g_status_h);
    settextcolor(RGB(26, 36, 45));
    snprintf(buf, sizeof(buf), "Round %d", st->round_count);
    outtextxy_utf8(g_ui_margin, 12, buf);
    snprintf(buf, sizeof(buf), "Acting Player: P%d", acting_player_id > 0 ? acting_player_id : st->current_turn);
    outtextxy_utf8(g_ui_margin + 150, 12, buf);
    if (phase && phase[0] != '\0') {
        snprintf(buf, sizeof(buf), "Phase: %s", phase);
        outtextxy_utf8(g_ui_margin + 350, 12, buf);
    }
    outtextxy_utf8(g_ui_w - 250, 12, "Esc: Quit  F11: Toggle");
    g_draw_y = g_main_y + 14;
}

static void drain_easyx_input() {
    while (MouseHit()) {
        handle_global_hotkeys();
        GetMouseMsg();
    }
    Sleep(80);
    while (MouseHit()) {
        handle_global_hotkeys();
        GetMouseMsg();
    }
    while ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||
           (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ||
           (GetAsyncKeyState(VK_MBUTTON) & 0x8000)) {
        handle_global_hotkeys();
        Sleep(10);
    }
    int any_down = 1;
    while (any_down) {
        handle_global_hotkeys();
        any_down = 0;
        for (int vk = 8; vk <= 255; vk++) {
            if (GetAsyncKeyState(vk) & 0x8000) {
                any_down = 1;
                break;
            }
        }
        if (any_down) Sleep(10);
    }
}

static void wait_for_fresh_ack(const char* prompt) {
    draw_overlay_message(prompt);
    int layout_version = g_ui_layout_version;
    drain_easyx_input();
    while (1) {
        handle_global_hotkeys();
        if (layout_changed_since(&layout_version)) {
            draw_background_shell();
            draw_overlay_message(prompt);
        }
        if (MouseHit()) {
            MOUSEMSG mm = GetMouseMsg();
            if (mm.uMsg == WM_LBUTTONDOWN || mm.uMsg == WM_RBUTTONDOWN) return;
        }
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||
            (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ||
            (GetAsyncKeyState(VK_MBUTTON) & 0x8000)) {
            return;
        }
        for (int vk = 8; vk <= 255; vk++) {
            if (GetAsyncKeyState(vk) & 0x8000) return;
        }
        Sleep(10);
    }
}

static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

static int poll_mouse_message(MOUSEMSG* out_msg) {
    handle_global_hotkeys();
    if (!MouseHit()) {
        Sleep(10);
        return 0;
    }
    *out_msg = GetMouseMsg();
    return 1;
}

static int layout_changed_since(int* seen_version) {
    if (!seen_version) return 0;
    if (*seen_version == g_ui_layout_version) return 0;
    *seen_version = g_ui_layout_version;
    return 1;
}

static void draw_button(int x, int y, int w, int h, const char* label) {
    draw_art_or_fill(&g_art_button_idle, x, y, w, h, RGB(236, 230, 198));
    setlinecolor(RGB(65, 56, 43));
    rectangle(x, y, x + w, y + h);
    settextcolor(RGB(29, 31, 30));
    outtextxy_utf8(x + 6, y + 6, label);
    present_frame();
}

// Draw a button and mark it as checked (used to indicate a confirmed selection)
static void draw_button_with_check(int x, int y, int w, int h, const char* label) {
    draw_art_or_fill(&g_art_button_selected, x, y, w, h, RGB(200, 234, 214));
    setlinecolor(RGB(45, 90, 73));
    rectangle(x, y, x + w, y + h);
    setfillcolor(RGB(52, 150, 99));
    fillrectangle(x + 6, y + 6, x + 22, y + h - 6);
    settextcolor(RGB(20, 42, 34));
    outtextxy_utf8(x + 30, y + 6, label);
    present_frame();
}

// Draw a non-intrusive overlay message at the bottom of the screen to avoid
// overlapping interactive UI elements (buttons, lists).
static void draw_overlay_message(const char* utf8msg) {
    std::string s = utf8_to_acp_str(utf8msg);
    update_layout();
    int x = g_ui_margin, w = g_ui_w - (g_ui_margin * 2), h = 46, y = g_ui_h - g_ui_margin - h; // bottom area for messages
    draw_art_or_fill(&g_art_message_panel, x, y, w, h, RGB(246, 242, 222));
    setlinecolor(RGB(65, 56, 43));
    rectangle(x, y, x + w, y + h);
    settextcolor(RGB(29, 31, 30));
    outtextxy(x + 6, y + 10, s.c_str());
    present_frame();
}

static int clamp_meter_width(int value, int max_value, int width) {
    if (max_value <= 0 || value <= 0) return 0;
    if (value >= max_value) return width;
    return (value * width) / max_value;
}

static void draw_meter(int x, int y, int w, int value, int max_value, IMAGE* fill_img) {
    int inner_w = w - 4;
    int fill_w = clamp_meter_width(value, max_value, inner_w);
    draw_art_or_fill(&g_art_bar_frame, x, y, w, 14, RGB(35, 42, 48));
    if (fill_w > 0 && fill_img && fill_img->getwidth() > 0) {
        putimage(x + 2, y + 2, fill_w, 10, fill_img, 0, 0);
    } else if (fill_w > 0) {
        setfillcolor(RGB(203, 54, 49));
        fillrectangle(x + 2, y + 2, x + 2 + fill_w, y + 12);
    }
    setlinecolor(RGB(68, 55, 35));
    rectangle(x, y, x + w, y + 14);
}

static void draw_character_hud_row(const Character* ch, int x, int y, const char* prefix) {
    if (!ch) return;
    char buf[128];
    IMAGE* portrait = portrait_for_element(ch->element);
    draw_art_or_fill(portrait, x, y, 32, 32, RGB(167, 158, 139));
    setlinecolor(ch->is_alive ? RGB(82, 70, 45) : RGB(100, 100, 100));
    rectangle(x, y, x + 32, y + 32);
    if (!ch->is_alive) {
        setlinecolor(RGB(130, 30, 30));
        line(x + 3, y + 3, x + 29, y + 29);
        line(x + 29, y + 3, x + 3, y + 29);
    }
    settextcolor(RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "%s %s", prefix, ch->name);
    outtextxy_utf8(x + 40, y, buf);
    snprintf(buf, sizeof(buf), "HP %d/%d", ch->hp, ch->max_hp);
    outtextxy_utf8(x + 40, y + 17, buf);
    draw_meter(x + 134, y + 17, g_side_w - 162, ch->hp, ch->max_hp, &g_art_hp_fill);
}

// Render both teams' characters and HP on a side panel (right side) so that
// their HP is always visible during the turn.
static void draw_team_hp_panel(const GameState* st) {
    if (!st) return;
    update_layout();
    int x = g_side_x + 10, y = g_team_panel_y + 10;
    char buf[128];
    draw_art_or_fill(&g_art_side_panel, g_side_x, g_team_panel_y, g_side_w, g_team_panel_h, RGB(242, 239, 220));
    setlinecolor(RGB(62, 76, 92));
    rectangle(g_side_x, g_team_panel_y, g_side_x + g_side_w, g_team_panel_y + g_team_panel_h);
    settextcolor(RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "Player1: %s", st->p1.name);
    outtextxy_utf8(x + 6, y, buf); y += 22;
    for (int i = 0; i < TEAM_SIZE; i++) {
        snprintf(buf, sizeof(buf), "P1[%d]", i);
        draw_character_hud_row(&st->p1.team[i], x + 6, y, buf);
        y += 38;
    }
    y += 6;
    snprintf(buf, sizeof(buf), "Player2: %s", st->p2.name);
    outtextxy_utf8(x + 6, y, buf); y += 22;
    for (int i = 0; i < TEAM_SIZE; i++) {
        snprintf(buf, sizeof(buf), "P2[%d]", i);
        draw_character_hud_row(&st->p2.team[i], x + 6, y, buf);
        y += 38;
    }
}

static void draw_button_disabled(int x, int y, int w, int h, const char* label) {
    draw_art_or_fill(&g_art_button_disabled, x, y, w, h, RGB(204, 207, 203));
    setlinecolor(RGB(116, 124, 124));
    rectangle(x, y, x + w, y + h);
    settextcolor(RGB(95, 98, 98));
    outtextxy_utf8(x + 6, y + 6, label);
    settextcolor(RGB(31, 37, 41));
    present_frame();
}

static Player* mutable_player(GameState* st, int player_id) {
    return (player_id == st->p1.player_id) ? &st->p1 : &st->p2;
}

static void draw_action_records_panel(const ActionRecord* records, int record_count, int player_id) {
    update_layout();
    int x = g_side_x, y = g_records_panel_y, w = g_side_w, h = g_records_panel_h;
    char buf[256];
    int shown = 0;
    draw_art_or_fill(&g_art_records_panel, x, y, w, h, RGB(230, 234, 230));
    setlinecolor(RGB(62, 76, 92));
    rectangle(x, y, x + w, y + h);
    settextcolor(RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "P%d Card Records", player_id);
    outtextxy_utf8(x + 8, y + 8, buf);
    int line_y = y + 36;
    for (int i = 0; i < record_count && shown < 10; i++) {
        if (records[i].action.type != ACTION_PLAY_CARD) continue;
        snprintf(buf, sizeof(buf), "%02d. %s", shown + 1, records[i].summary);
        outtextxy_utf8(x + 8, line_y, buf);
        line_y += 22;
        shown++;
    }
    if (shown == 0) {
        outtextxy_utf8(x + 8, y + 36, "No card records");
    }
}

static int has_card_record(const ActionRecord* records, int record_count) {
    for (int i = 0; i < record_count; i++) {
        if (records[i].action.type == ACTION_PLAY_CARD) return 1;
    }
    return 0;
}

static void reset_pending_action(Action* act, int player_id) {
    act->type = ACTION_NONE;
    act->actor_id = player_id;
    act->card_hand_idx = -1;
    act->switch_to_idx = -1;
    act->target_idx = -1;
}

static void draw_planning_shell(GameState* st, int player_id, const ActionRecord* records, int record_count, const char* title) {
    reset_draw_y();
    draw_status_bar(st, player_id, "Planning");
    draw_team_hp_panel(st);
    draw_action_records_panel(records, record_count, player_id);
    Player* p = mutable_player(st, player_id);
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", title);
    draw_line(buf);
    snprintf(buf, sizeof(buf), "Energy: %d/%d", p->energy, p->max_energy);
    draw_line(buf);
    draw_meter(g_main_x + 124, g_draw_y - 24, 220, p->energy, p->max_energy, &g_art_energy_fill);
    draw_line("Active:");
    print_character(&p->team[p->active_idx]);
    present_frame();
}

#endif
#else
// no graphical helpers
#endif

// 简易基于控制台的 GUI 实现，便于在没有图形库时交互和调试。

void InitGUI() {
#ifdef USE_EASYX
    SetProcessDPIAware();
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (screen_w < 1024) screen_w = 1024;
    if (screen_h < 720) screen_h = 720;
    initgraph(screen_w, screen_h);
    apply_window_mode(1);
    BeginBatchDraw();
    load_ui_assets();
    setbkcolor(RGB(238, 231, 201));
    settextcolor(RGB(31, 37, 41));
    setbkmode(TRANSPARENT);
    cleardevice();
    settextstyle_utf8(20,0,"SimSun");
    draw_background_shell();
    outtextxy_utf8(20, 20, "Spire of Roco loading...");
    present_frame();
#else
    // 控制台不需要特别初始化
#endif
}

void CloseGUI() {
#ifdef USE_EASYX
    EndBatchDraw();
    closegraph();
#else
    // 控制台不需要特别释放
#endif
}

void print_character(const Character* ch) {
    if (!ch) return;
#ifdef USE_EASYX
    char buf[256];
    int y = g_draw_y;
    int x = g_main_x + 16;
    IMAGE* portrait = portrait_for_element(ch->element);
    draw_art_or_fill(portrait, x, y, 38, 38, RGB(167, 158, 139));
    setlinecolor(ch->is_alive ? RGB(82, 70, 45) : RGB(100, 100, 100));
    rectangle(x, y, x + 38, y + 38);
    snprintf(buf, sizeof(buf), "%s (ID:%d) HP:%d/%d Elem:%d Speed:%d %s",
             ch->name, ch->char_id, ch->hp, ch->max_hp, ch->element, ch->speed,
             ch->is_alive ? "" : "[DEAD]");
    settextcolor(RGB(27, 33, 35));
    outtextxy_utf8(x + 48, y + 2, buf);
    draw_meter(x + 48, y + 24, 240, ch->hp, ch->max_hp, &g_art_hp_fill);
    g_draw_y += 46;
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
    int y = g_draw_y;
    int x = g_main_x + 16;
    int w = g_main_w - 32;
    if (w > 700) w = 700;
    draw_art_or_fill(&g_art_card_plate, x, y, w, 38, RGB(222, 215, 184));
    setlinecolor(RGB(83, 72, 55));
    rectangle(x, y, x + w, y + 38);
    snprintf(buf, sizeof(buf), " [%2d] %s (ID:%d) Cost:%d Dmg:%d Def:%d Heal:%d Type:%d Target:%d",
             idx, c->name, c->card_id, c->energy_cost, c->base_damage, c->base_defense, c->base_heal, c->type, c->target_type);
    settextcolor(RGB(29, 31, 30));
    outtextxy_utf8(x + 8, y + 9, buf);
    g_draw_y += 42;
#else
    printf(" [%2d] %s (ID:%d) Cost:%d Dmg:%d Def:%d Heal:%d Type:%d Target:%d\n",
           idx, c->name, c->card_id, c->energy_cost, c->base_damage, c->base_defense, c->base_heal, c->type, c->target_type);
#endif
}

void RenderGameBoard(GameState state) {
#ifdef USE_EASYX
    reset_draw_y();
    draw_status_bar(&state, state.current_turn, "Board");
    draw_team_hp_panel(&state);
    char buf[256];
    snprintf(buf,sizeof(buf), "=== Game Board (Round:%d) Current Turn: Player %d ===", state.round_count, state.current_turn);
    draw_line(buf);

    snprintf(buf,sizeof(buf), "-- Player1: %s --", state.p1.name); draw_line(buf);
    draw_line(" Active: "); print_character(&state.p1.team[state.p1.active_idx]);
    snprintf(buf,sizeof(buf), " Energy: %d/%d  Hand:%d  Draw:%d Discard:%d", state.p1.energy, state.p1.max_energy, state.p1.hand_count, state.p1.draw_count, state.p1.discard_count); draw_line(buf);
    draw_meter(g_main_x + 124, g_draw_y - 24, 220, state.p1.energy, state.p1.max_energy, &g_art_energy_fill);
    if (state.p1.hand_count > 0) {
        draw_line(" Hand:");
        for (int i = 0; i < state.p1.hand_count; i++) {
            print_card(&state.p1.hand[i], i);
        }
    }

    snprintf(buf,sizeof(buf), "\n-- Player2: %s --", state.p2.name); draw_line(buf);
    draw_line(" Active: "); print_character(&state.p2.team[state.p2.active_idx]);
    snprintf(buf,sizeof(buf), " Energy: %d/%d  Hand:%d  Draw:%d Discard:%d", state.p2.energy, state.p2.max_energy, state.p2.hand_count, state.p2.draw_count, state.p2.discard_count); draw_line(buf);
    draw_meter(g_main_x + 124, g_draw_y - 24, 220, state.p2.energy, state.p2.max_energy, &g_art_energy_fill);
    if (state.p2.hand_count > 0) {
        draw_line(" Hand:");
        for (int i = 0; i < state.p2.hand_count; i++) {
            print_card(&state.p2.hand[i], i);
        }
    }

    draw_line("========================================");
    present_frame();
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
    int layout_version = g_ui_layout_version;
    // Wait for any key or mouse click using non-blocking checks so the EasyX window doesn't need console focus
    while (1) {
        handle_global_hotkeys();
        if (layout_changed_since(&layout_version)) {
            draw_background_shell();
            draw_overlay_message(prompt);
        }
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
    present_frame();
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
    char buf[256];

#ifdef USE_EASYX
action_menu:
    reset_draw_y();
    int layout_version = g_ui_layout_version;
    // render side panel with HP so it's always visible
    draw_team_hp_panel(&state);
    snprintf(buf, sizeof(buf), "Player %d's turn - Please choose action:", player_id); settextcolor(BLACK); draw_line(buf); draw_line("Current:"); print_character(&p->team[p->active_idx]);
    int bx = g_main_x + 24, by = g_draw_y + 10, bw = 220, bh = 44;
    draw_button(bx, by, bw, bh, "End Turn");
    draw_button(bx, by + 60, bw, bh, "Play Card");
    draw_button(bx, by + 120, bw, bh, "Switch Character");

    MOUSEMSG m;
    while (1) {
        if (!poll_mouse_message(&m)) {
            if (layout_changed_since(&layout_version)) goto action_menu;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto action_menu;
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) { act.type = ACTION_END_TURN; return act; }
            else if (point_in_rect(m.x, m.y, bx, by + 60, bw, bh)) {
                if (p->hand_count == 0) { draw_line("Hand empty, cannot play."); continue; }
                // 清理并显示手牌为可点按钮（避免文本重叠）
                reset_draw_y();
                draw_team_hp_panel(&state);
                snprintf(buf, sizeof(buf), "Player %d's turn - Select a hand card:", player_id); draw_line(buf);
                // show current active character info
                draw_line(" Active: "); print_character(&p->team[p->active_idx]);
                int hx = g_main_x + 180, hy = g_draw_y + 10, hw = 280, hh = 44;
                for (int i = 0; i < p->hand_count; i++) {
                    char buf[128]; snprintf(buf, sizeof(buf), "[%d] %s", i, p->hand[i].name);
                    draw_button(hx, hy + i * (hh + 8), hw, hh, buf);
                }
                // 等待手牌点击
                while (1) {
                    if (!poll_mouse_message(&m)) {
                        if (layout_changed_since(&layout_version)) goto action_menu;
                        continue;
                    }
                    if (layout_changed_since(&layout_version)) goto action_menu;
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
                                    int tx = g_side_x + 24, ty = g_team_panel_y + g_team_panel_h + 24, tw = g_side_w - 48, th = 44;
                                    Character* target_team = NULL;
                                    if (c->target_type == TARGET_SELF_SINGLE) target_team = p->team;
                                    else {
                                        // 敌方队伍
                                        if (player_id == state.p1.player_id) target_team = (Character*)state.p2.team;
                                        else target_team = (Character*)state.p1.team;
                                    }
                                    for (int t = 0; t < TEAM_SIZE; t++) {
                                        char tb[128]; snprintf(tb, sizeof(tb), "Target %d: %s", t, target_team[t].name);
                                        draw_button(tx, ty + t * (th + 8), tw, th, tb);
                                    }
                                    while (1) {
                                        if (!poll_mouse_message(&m)) {
                                            if (layout_changed_since(&layout_version)) goto action_menu;
                                            continue;
                                        }
                                        if (layout_changed_since(&layout_version)) goto action_menu;
                                        if (m.uMsg == WM_LBUTTONDOWN) {
                                            for (int t = 0; t < TEAM_SIZE; t++) {
                                                int rx2 = tx;
                                                int ry2 = ty + t * (th + 8);
                                                if (point_in_rect(m.x, m.y, rx2, ry2, tw, th)) {
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
                snprintf(buf, sizeof(buf), "Player %d's turn - Select a character to switch:", player_id); settextcolor(BLACK); draw_line(buf); draw_line("Current:"); print_character(&p->team[p->active_idx]);
                int sx = g_main_x + 120, sy = g_draw_y + 10, sw = 380, sh = 50;
                for (int i = 0; i < TEAM_SIZE; i++) {
                    char tmp[128]; snprintf(tmp, sizeof(tmp), "[%d] %s", i, p->team[i].name);
                    draw_button(sx, sy + i * (sh + 8), sw, sh, tmp);
                }
                while (1) {
                    if (!poll_mouse_message(&m)) {
                        if (layout_changed_since(&layout_version)) goto action_menu;
                        continue;
                    }
                    if (layout_changed_since(&layout_version)) goto action_menu;
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
        printf("Current active:\n");
        print_character(&p->team[p->active_idx]);
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
            printf("Current active:\n");
            print_character(&p->team[p->active_idx]);
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

Action GetPlannedInputFromUI(int player_id, GameState state, const ActionRecord* records, int record_count, int* edit_index) {
    Action act;
    reset_pending_action(&act, player_id);
    if (edit_index) *edit_index = -1;

#ifdef USE_EASYX
    Player* p = mutable_player(&state, player_id);
    MOUSEMSG m;
    int layout_version = g_ui_layout_version;
    char buf[256];

main_menu:
    p = mutable_player(&state, player_id);
    draw_planning_shell(&state, player_id, records, record_count, "Plan your turn");
    layout_version = g_ui_layout_version;
    {
        int bx = g_main_x + 24, by = g_draw_y + 10, bw = 220, bh = 44;
        draw_button(bx, by, bw, bh, "End Turn");
        draw_button(bx, by + 55, bw, bh, "Play Card");
        draw_button(bx, by + 110, bw, bh, "Switch Character");
        int can_edit = has_card_record(records, record_count);
        if (can_edit) draw_button(bx, by + 165, bw, bh, "Edit Card");
        else draw_button_disabled(bx, by + 165, bw, bh, "Edit Action");
        while (1) {
            if (!poll_mouse_message(&m)) {
                if (layout_changed_since(&layout_version)) goto main_menu;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto main_menu;
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) {
                act.type = ACTION_END_TURN;
                return act;
            }
            if (point_in_rect(m.x, m.y, bx, by + 55, bw, bh)) {
                if (p->hand_count <= 0) {
                    draw_overlay_message("Hand empty");
                    continue;
                }
                goto card_select;
            }
            if (point_in_rect(m.x, m.y, bx, by + 110, bw, bh)) {
                goto switch_select;
            }
            if (can_edit && point_in_rect(m.x, m.y, bx, by + 165, bw, bh)) {
                goto edit_select;
            }
        }
    }

card_select:
    p = mutable_player(&state, player_id);
    draw_planning_shell(&state, player_id, records, record_count, "Choose a card");
    layout_version = g_ui_layout_version;
    {
        int cx = g_main_x + 24, cy = g_draw_y + 10, cw = clamp_int(g_main_w - 64, 380, 620), ch = 40;
        int back_x = g_main_x + 16, back_y = bottom_button_y(), back_w = 120, back_h = 38;
        for (int i = 0; i < p->hand_count; i++) {
            snprintf(buf, sizeof(buf), "[%d] %s  Cost:%d", i, p->hand[i].name, p->hand[i].energy_cost);
            if (p->energy >= p->hand[i].energy_cost) {
                draw_button(cx, cy + i * (ch + 6), cw, ch, buf);
            } else {
                draw_button_disabled(cx, cy + i * (ch + 6), cw, ch, buf);
            }
        }
        draw_button(back_x, back_y, back_w, back_h, "Back");
        while (1) {
            if (!poll_mouse_message(&m)) {
                if (layout_changed_since(&layout_version)) goto card_select;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto card_select;
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) {
                reset_pending_action(&act, player_id);
                goto main_menu;
            }
            for (int i = 0; i < p->hand_count; i++) {
                int ry = cy + i * (ch + 6);
                if (!point_in_rect(m.x, m.y, cx, ry, cw, ch)) continue;
                if (p->energy < p->hand[i].energy_cost) {
                    draw_overlay_message("Not enough energy");
                    break;
                }
                act.type = ACTION_PLAY_CARD;
                act.card_hand_idx = i;
                act.actor_id = player_id;
                if (p->hand[i].target_type == TARGET_ENEMY_SINGLE || p->hand[i].target_type == TARGET_SELF_SINGLE) {
                    goto target_select;
                }
                act.target_idx = (p->hand[i].target_type == TARGET_ENEMY_ALL) ? -1 : -2;
                return act;
            }
        }
    }

target_select:
    p = mutable_player(&state, player_id);
    draw_planning_shell(&state, player_id, records, record_count, "Choose a target");
    layout_version = g_ui_layout_version;
    {
        Card* c = &p->hand[act.card_hand_idx];
        Character* target_team = NULL;
        if (c->target_type == TARGET_SELF_SINGLE) {
            target_team = p->team;
        } else {
            target_team = (player_id == state.p1.player_id) ? state.p2.team : state.p1.team;
        }
        snprintf(buf, sizeof(buf), "Card: %s", c->name);
        draw_line(buf);
        int tx = g_main_x + 48, ty = g_draw_y + 20, tw = clamp_int(g_main_w - 96, 360, 620), th = 46;
        int back_x = g_main_x + 16, back_y = bottom_button_y(), back_w = 120, back_h = 38;
        for (int t = 0; t < TEAM_SIZE; t++) {
            snprintf(buf, sizeof(buf), "[%d] %s HP:%d/%d", t, target_team[t].name, target_team[t].hp, target_team[t].max_hp);
            if (target_team[t].is_alive || c->target_type == TARGET_SELF_SINGLE) draw_button(tx, ty + t * (th + 8), tw, th, buf);
            else draw_button_disabled(tx, ty + t * (th + 8), tw, th, buf);
        }
        draw_button(back_x, back_y, back_w, back_h, "Back");
        while (1) {
            if (!poll_mouse_message(&m)) {
                if (layout_changed_since(&layout_version)) goto target_select;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto target_select;
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) {
                reset_pending_action(&act, player_id);
                goto main_menu;
            }
            for (int t = 0; t < TEAM_SIZE; t++) {
                int ry = ty + t * (th + 8);
                if (!point_in_rect(m.x, m.y, tx, ry, tw, th)) continue;
                if (!target_team[t].is_alive && c->target_type != TARGET_SELF_SINGLE) {
                    draw_overlay_message("Target is defeated");
                    break;
                }
                act.target_idx = t;
                return act;
            }
        }
    }

switch_select:
    p = mutable_player(&state, player_id);
    draw_planning_shell(&state, player_id, records, record_count, "Choose active character");
    layout_version = g_ui_layout_version;
    {
        int sx = g_main_x + 48, sy = g_draw_y + 20, sw = clamp_int(g_main_w - 96, 360, 620), sh = 50;
        int back_x = g_main_x + 16, back_y = bottom_button_y(), back_w = 120, back_h = 38;
        for (int i = 0; i < TEAM_SIZE; i++) {
            snprintf(buf, sizeof(buf), "[%d] %s HP:%d/%d", i, p->team[i].name, p->team[i].hp, p->team[i].max_hp);
            if (p->team[i].is_alive) draw_button(sx, sy + i * (sh + 8), sw, sh, buf);
            else draw_button_disabled(sx, sy + i * (sh + 8), sw, sh, buf);
        }
        draw_button(back_x, back_y, back_w, back_h, "Back");
        while (1) {
            if (!poll_mouse_message(&m)) {
                if (layout_changed_since(&layout_version)) goto switch_select;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto switch_select;
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) {
                reset_pending_action(&act, player_id);
                goto main_menu;
            }
            for (int i = 0; i < TEAM_SIZE; i++) {
                int ry = sy + i * (sh + 8);
                if (!point_in_rect(m.x, m.y, sx, ry, sw, sh)) continue;
                if (!p->team[i].is_alive) {
                    draw_overlay_message("Character is defeated");
                    break;
                }
                act.type = ACTION_SWITCH_CHAR;
                act.switch_to_idx = i;
                act.actor_id = player_id;
                return act;
            }
        }
    }

edit_select:
    draw_planning_shell(&state, player_id, records, record_count, "Edit an action");
    layout_version = g_ui_layout_version;
    {
        int ex = g_main_x + 24, ey = g_draw_y + 10, ew = clamp_int(g_main_w - 64, 420, 700), eh = 40;
        int back_x = g_main_x + 16, back_y = bottom_button_y(), back_w = 120, back_h = 38;
        int display_to_record[MAX_TURN_ACTIONS];
        int display_count = 0;
        for (int i = 0; i < record_count && i < MAX_TURN_ACTIONS; i++) {
            if (records[i].action.type != ACTION_PLAY_CARD) continue;
            display_to_record[display_count] = i;
            snprintf(buf, sizeof(buf), "%02d. %s", display_count + 1, records[i].summary);
            draw_button(ex, ey + display_count * (eh + 6), ew, eh, buf);
            display_count++;
        }
        draw_button(back_x, back_y, back_w, back_h, "Back");
        while (1) {
            if (!poll_mouse_message(&m)) {
                if (layout_changed_since(&layout_version)) goto edit_select;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto edit_select;
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) {
                reset_pending_action(&act, player_id);
                goto main_menu;
            }
            for (int i = 0; i < display_count; i++) {
                int ry = ey + i * (eh + 6);
                if (point_in_rect(m.x, m.y, ex, ry, ew, eh)) {
                    if (edit_index) *edit_index = display_to_record[i];
                    act.type = ACTION_EDIT_STEP;
                    act.actor_id = player_id;
                    return act;
                }
            }
        }
    }
#else
    return GetHumanInputFromUI(player_id, state);
#endif
}

void ShowResolutionStep(GameState state, const ActionRecord* record, const ResolutionReport* report, int step_number, int step_total) {
#ifdef USE_EASYX
    reset_draw_y();
    draw_status_bar(&state, record ? record->player_id : state.current_turn, "Resolution");
    draw_team_hp_panel(&state);
    char buf[256];
    snprintf(buf, sizeof(buf), "Round Resolution  %d/%d", step_number, step_total);
    draw_line(buf);
    if (record) {
        snprintf(buf, sizeof(buf), "Resolving: %s", record->summary);
        draw_line(buf);
    } else {
        draw_line("Resolving: No actions to resolve");
    }
    draw_line("Current battle state:");
    snprintf(buf, sizeof(buf), "P1 Active: %s HP:%d/%d  Energy:%d/%d",
             state.p1.team[state.p1.active_idx].name,
             state.p1.team[state.p1.active_idx].hp,
             state.p1.team[state.p1.active_idx].max_hp,
             state.p1.energy, state.p1.max_energy);
    draw_line(buf);
    snprintf(buf, sizeof(buf), "P2 Active: %s HP:%d/%d  Energy:%d/%d",
             state.p2.team[state.p2.active_idx].name,
             state.p2.team[state.p2.active_idx].hp,
             state.p2.team[state.p2.active_idx].max_hp,
             state.p2.energy, state.p2.max_energy);
    draw_line(buf);
    if (report && report->event_count > 0) {
        draw_line("Damage details:");
        for (int i = 0; i < report->event_count; i++) {
            const DamageResolutionEvent* event = &report->events[i];
            if (!event->has_damage) continue;
            snprintf(buf, sizeof(buf), "%s: %d -> %d  Damage:%d  Element:+%d  Shield:%d",
                     event->target_name,
                     event->hp_before,
                     event->hp_after,
                     event->final_damage,
                     event->element_bonus_damage,
                     event->shield_absorbed);
            draw_line(buf);
        }
    } else {
        draw_line("Damage details: no damage");
    }
    present_frame();
    wait_for_fresh_ack("Click or press any key to continue...");
#else
    printf("[Resolution %d/%d] %s\n", step_number, step_total, record ? record->summary : "");
    if (report && report->event_count > 0) {
        for (int i = 0; i < report->event_count; i++) {
            const DamageResolutionEvent* event = &report->events[i];
            if (!event->has_damage) continue;
            printf("  %s damage=%d element_bonus=%d shield=%d hp=%d->%d\n",
                   event->target_name,
                   event->final_damage,
                   event->element_bonus_damage,
                   event->shield_absorbed,
                   event->hp_before,
                   event->hp_after);
        }
    }
#endif
}


int SelectCharacterFromUI(int player_id, int slot_number) {
#ifdef USE_EASYX
    // legacy single-select preserved for compatibility
single_character_select:
    reset_draw_y();
    int layout_version = g_ui_layout_version;
    char buf[128]; snprintf(buf, sizeof(buf), "=== Character Select === Player %d selecting for slot %d", player_id, slot_number); settextcolor(BLACK); draw_line(buf);
    if (g_char_count == 0) { draw_line("Global character pool empty, returning 0"); return 0; }
    // Draw clickable list but keep selection until confirmed
    int sx = g_main_x + 24, sy = g_draw_y + 10, sw = clamp_int(g_main_w - 64, 360, 620), sh = 40;
    int confirm_x = sx, confirm_y = sy + g_char_count * (sh + 6) + 10; // place confirm below list
    int selected_idx = -1;
    MOUSEMSG m;
    int need_redraw = 1;
    while (1) {
        if (need_redraw) {
            reset_draw_y();
            draw_team_hp_panel(NULL); // keep HP panel intact; callers render full state before calling select
            for (int i = 0; i < g_char_count; i++) {
                char tmp[256]; snprintf(tmp, sizeof(tmp), "[%2d] %s (ID:%d) HP:%d Speed:%d", i, g_all_characters[i].name, g_all_characters[i].char_id, g_all_characters[i].max_hp, g_all_characters[i].speed);
                if (i == selected_idx) draw_button_with_check(sx, sy + i * (sh + 6), sw, sh, tmp);
                else draw_button(sx, sy + i * (sh + 6), sw, sh, tmp);
            }
            draw_button(confirm_x, confirm_y, 120, 40, "Confirm");
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (layout_changed_since(&layout_version)) goto single_character_select;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto single_character_select;
        if (m.uMsg == WM_LBUTTONDOWN) {
            int handled = 0;
            for (int i = 0; i < g_char_count; i++) {
                int rx = sx, ry = sy + i * (sh + 6);
                if (point_in_rect(m.x, m.y, rx, ry, sw, sh)) {
                    selected_idx = i;
                    need_redraw = 1;
                    handled = 1;
                    break;
                }
            }
            // confirm button
            if (!handled && point_in_rect(m.x, m.y, confirm_x, confirm_y, 120, 40)) {
                if (selected_idx >= 0) {
                    char chosen[128]; snprintf(chosen, sizeof(chosen), "Confirmed: %s", g_all_characters[selected_idx].name); draw_overlay_message(chosen);
                    return selected_idx;
                } else {
                    draw_overlay_message("No character selected");
                    need_redraw = 1;
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

// New: multi-select characters at once. Returns number selected (0 = none). Fills out_indices and out_count.
int SelectMultipleCharactersFromUI(int player_id, const GameState* st, int max_select, int* out_indices, int* out_count) {
#ifdef USE_EASYX
multi_character_select:
    reset_draw_y();
    int layout_version = g_ui_layout_version;
    char buf[128]; snprintf(buf, sizeof(buf), "=== Character Select === Player %d selecting - choose up to %d", player_id, max_select); settextcolor(BLACK); draw_line(buf);
    if (g_char_count == 0) { draw_line("Global character pool empty, returning 0"); if (out_count) *out_count = 0; return 0; }
    int sx = g_main_x + 24, sy = g_draw_y + 10, sw = clamp_int(g_main_w - 64, 360, 620), sh = 40;
    int confirm_x = sx, confirm_y = sy + g_char_count * (sh + 6) + 10;
    int* selected = (int*)malloc(sizeof(int) * g_char_count);
    memset(selected, 0, sizeof(int) * g_char_count);
    MOUSEMSG m;
    int need_redraw = 1;
    while (1) {
        if (need_redraw) {
            reset_draw_y();
            draw_team_hp_panel(st);
            for (int i = 0; i < g_char_count; i++) {
                char tmp[256]; snprintf(tmp, sizeof(tmp), "[%2d] %s (ID:%d) HP:%d Speed:%d", i, g_all_characters[i].name, g_all_characters[i].char_id, g_all_characters[i].max_hp, g_all_characters[i].speed);
                if (selected[i]) draw_button_with_check(sx, sy + i * (sh + 6), sw, sh, tmp);
                else draw_button(sx, sy + i * (sh + 6), sw, sh, tmp);
            }
            draw_button(confirm_x, confirm_y, 120, 40, "Confirm");
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (layout_changed_since(&layout_version)) {
                free(selected);
                goto multi_character_select;
            }
            continue;
        }
        if (layout_changed_since(&layout_version)) {
            free(selected);
            goto multi_character_select;
        }
        if (m.uMsg == WM_LBUTTONDOWN) {
            int handled = 0;
            for (int i = 0; i < g_char_count; i++) {
                int rx = sx, ry = sy + i * (sh + 6);
                if (point_in_rect(m.x, m.y, rx, ry, sw, sh)) { selected[i] = !selected[i]; need_redraw = 1; handled = 1; break; }
            }
            if (!handled && point_in_rect(m.x, m.y, confirm_x, confirm_y, 120, 40)) {
                int count = 0;
                for (int i = 0; i < g_char_count && count < max_select; i++) { if (selected[i]) out_indices[count++] = i; }
                free(selected);
                if (out_count) *out_count = count;
                return count;
            }
        }
    }
#else
    printf("\n[Character Select - Multi] Player %d - select up to %d (enter indices separated by space, -1 to finish)\n", player_id, max_select);
    if (g_char_count == 0) { printf("Global character pool empty, returning 0\n"); if (out_count) *out_count = 0; return 0; }
    for (int i = 0; i < g_char_count; i++) printf(" %2d: %s (ID:%d) HP:%d Speed:%d\n", i, g_all_characters[i].name, g_all_characters[i].char_id, g_all_characters[i].max_hp, g_all_characters[i].speed);
    int count = 0; int idx = -2;
    while (count < max_select) {
        if (scanf("%d", &idx) != 1) { while(getchar()!='\n'); break; }
        if (idx == -1) break;
        if (idx < 0 || idx >= g_char_count) continue;
        out_indices[count++] = idx;
    }
    while(getchar()!='\n');
    if (out_count) *out_count = count;
    return count;
#endif
}

int SelectCardFromUI(int player_id, int current_deck_size) {
#ifdef USE_EASYX
    // legacy single-select behavior preserved for compatibility
single_card_select:
    reset_draw_y();
    int layout_version = g_ui_layout_version;
    char buf[128]; snprintf(buf, sizeof(buf), "[Deck Build] Player %d - Selected %d so far", player_id, current_deck_size); draw_line(buf);
    if (g_card_count == 0) { draw_line("Global card pool empty, returning -1"); return -1; }
    int cx = g_main_x + 24, cy = g_draw_y + 10, cw = clamp_int(g_main_w - 64, 340, 620), ch = 40;
    int max_rows = (bottom_button_y() - cy - 80) / (ch + 6);
    if (max_rows < 4) max_rows = 4;
    if (max_rows > 20) max_rows = 20;
    int show = g_card_count < max_rows ? g_card_count : max_rows;
    int confirm_x = cx, confirm_y = cy + show * (ch + 6) + 10; // place confirm below list
    int selected_idx = -1;
    MOUSEMSG m;
    int need_redraw = 1;
    while (1) {
        if (need_redraw) {
            reset_draw_y();
            draw_team_hp_panel(NULL);
            for (int i = 0; i < show; i++) {
                char tmp[256]; snprintf(tmp, sizeof(tmp), "[%2d] %s", i, g_all_cards[i].name);
                if (i == selected_idx) draw_button_with_check(cx, cy + i * (ch + 6), cw, ch, tmp);
                else draw_button(cx, cy + i * (ch + 6), cw, ch, tmp);
            }
            // add finish and confirm buttons
            draw_button(confirm_x, confirm_y, 120, 40, "Finish Build");
            draw_button(confirm_x, confirm_y + 60, 120, 40, "Confirm");
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (layout_changed_since(&layout_version)) goto single_card_select;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto single_card_select;
        if (m.uMsg == WM_LBUTTONDOWN) {
            int handled = 0;
            for (int i = 0; i < show; i++) {
                int rx = cx, ry = cy + i * (ch + 6);
                if (point_in_rect(m.x, m.y, rx, ry, cw, ch)) {
                    selected_idx = i;
                    need_redraw = 1;
                    handled = 1;
                    break;
                }
            }
            // finish button
            if (!handled && point_in_rect(m.x, m.y, confirm_x, confirm_y, 120, 40)) {
                return -1;
            }
            // confirm button
            if (!handled && point_in_rect(m.x, m.y, confirm_x, confirm_y + 60, 120, 40)) {
                if (selected_idx >= 0) {
                    char chosen[128]; snprintf(chosen, sizeof(chosen), "Confirmed: %s", g_all_cards[selected_idx].name); draw_overlay_message(chosen);
                    return selected_idx;
                } else {
                    draw_overlay_message("No card selected");
                    need_redraw = 1;
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

// New: allow selecting multiple cards at once; returns number of selected indices (0 = none), fills out_indices and sets out_count
int SelectMultipleCardsFromUI(int player_id, int max_select, int* out_indices, int* out_count) {
#ifdef USE_EASYX
multi_card_select:
    reset_draw_y();
    int layout_version = g_ui_layout_version;
    char buf[256]; snprintf(buf, sizeof(buf), "[Deck Build] Player %d - Deck Building (max %d cards)", player_id, max_select); draw_line(buf);
    if (g_card_count == 0) { draw_line("Global card pool empty, returning 0"); if (out_count) *out_count = 0; return 0; }
    int show = g_card_count < 8 ? g_card_count : 8; // limit UI to 8 unique card types per spec
    int cx = g_main_x + 24, cy = g_draw_y + 10, card_w = clamp_int(g_main_w - 250, 300, 560), card_h = 40;
    int minus_w = 40, qty_w = 60, plus_w = 40;
    int line_h = card_h + 6;
    int confirm_x = cx, confirm_y = cy + show * line_h + 60;
    int* qty = (int*)calloc(show, sizeof(int));
    if (!qty) { if (out_count) *out_count = 0; return 0; }
    int total = 0;
    MOUSEMSG m;
    int need_redraw = 1;
    while (1) {
        if (need_redraw) {
            reset_draw_y();
            draw_team_hp_panel(NULL);
            char header[128]; snprintf(header, sizeof(header), "Player %d Deck Building - Selected %d/%d", player_id, total, max_select); settextcolor(BLACK); draw_line(header);
            for (int i = 0; i < show; i++) {
                char tmp[256]; snprintf(tmp, sizeof(tmp), "[%2d] %s", i, g_all_cards[i].name);
                int x_label = cx;
                int x_minus = cx + card_w + 8;
                int x_qty = x_minus + minus_w + 8;
                int x_plus = x_qty + qty_w + 8;
                // draw card label
                draw_button(x_label, cy + i * line_h, card_w, card_h, tmp);
                // draw minus, qty, plus controls
                char mbuf[16]; snprintf(mbuf, sizeof(mbuf), "-"); draw_button(x_minus, cy + i * line_h, minus_w, card_h, mbuf);
                char qbuf[32]; snprintf(qbuf, sizeof(qbuf), "%d", qty[i]); draw_button(x_qty, cy + i * line_h, qty_w, card_h, qbuf);
                char pbuf[16]; snprintf(pbuf, sizeof(pbuf), "+"); draw_button(x_plus, cy + i * line_h, plus_w, card_h, pbuf);
            }
            // footer buttons
            draw_button(confirm_x, confirm_y, 140, 40, "End Building");
            // show total at bottom overlay as real-time statistic
            char tbuf[64]; snprintf(tbuf, sizeof(tbuf), "Total cards: %d/%d", total, max_select); draw_overlay_message(tbuf);
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (layout_changed_since(&layout_version)) {
                free(qty);
                goto multi_card_select;
            }
            continue;
        }
        if (layout_changed_since(&layout_version)) {
            free(qty);
            goto multi_card_select;
        }
        if (m.uMsg == WM_LBUTTONDOWN) {
            int handled = 0;
            for (int i = 0; i < show; i++) {
                int x_minus = cx + card_w + 8;
                int x_qty = x_minus + minus_w + 8;
                int x_plus = x_qty + qty_w + 8;
                int ry = cy + i * line_h;
                if (point_in_rect(m.x, m.y, x_minus, ry, minus_w, card_h)) {
                    if (qty[i] > 0) { qty[i]--; total--; }
                    need_redraw = 1; handled = 1; break;
                }
                if (point_in_rect(m.x, m.y, x_plus, ry, plus_w, card_h)) {
                    if (total < max_select) { qty[i]++; total++; }
                    else draw_overlay_message("Cannot exceed deck limit");
                    need_redraw = 1; handled = 1; break;
                }
            }
            if (!handled && point_in_rect(m.x, m.y, confirm_x, confirm_y, 140, 40)) {
                // End Building: finalize selection and return negative count to signal engine to stop further rounds
                int count = 0;
                for (int i = 0; i < show && count < max_select; i++) {
                    for (int k = 0; k < qty[i] && count < max_select; k++) {
                        out_indices[count++] = i;
                    }
                }
                free(qty);
                if (out_count) *out_count = count;
                return -count; // negative indicates finalize
            }
        }
    }
#else
    // Console fallback: simple text controls
    printf("\n[Deck Build] Player %d - Deck Building (max %d cards)\n", player_id, max_select);
    if (g_card_count == 0) { printf("Global card pool empty, returning 0\n"); if (out_count) *out_count = 0; return 0; }
    int show = g_card_count < 8 ? g_card_count : 8;
    int* qty = (int*)calloc(show, sizeof(int));
    if (!qty) { if (out_count) *out_count = 0; return 0; }
    int total = 0;
    while (1) {
        printf("Current selection (%d/%d):\n", total, max_select);
        for (int i = 0; i < show; i++) {
            printf(" %2d: %s  qty=%d\n", i, g_all_cards[i].name, qty[i]);
        }
        printf("Commands: +<index> to add, -<index> to remove, f to finish, e to end building\n");
        char cmd[64];
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        if (cmd[0] == 'f') break;
        if (cmd[0] == 'e') { free(qty); if (out_count) *out_count = 0; return 0; }
        if ((cmd[0] == '+' || cmd[0] == '-') ) {
            int idx = atoi(cmd + 1);
            if (idx < 0 || idx >= show) { printf("Invalid index\n"); continue; }
            if (cmd[0] == '+') {
                if (total < max_select) { qty[idx]++; total++; }
                else printf("Cannot exceed deck limit\n");
            } else {
                if (qty[idx] > 0) { qty[idx]--; total--; }
                else printf("Quantity already 0\n");
            }
            continue;
        }
        printf("Unknown command\n");
    }
    // flatten quantities into out_indices
    int count = 0;
    for (int i = 0; i < show && count < max_select; i++) {
        for (int k = 0; k < qty[i] && count < max_select; k++) out_indices[count++] = i;
    }
    free(qty);
    if (out_count) *out_count = count;
    return count;
#endif
}

int GetModeSelectionFromUI() {
#ifdef USE_EASYX
mode_menu:
    reset_draw_y(); draw_line("Main Menu: Select mode:");
    int layout_version = g_ui_layout_version;
    int bx = g_main_x + 52, by = g_draw_y + 10, bw = 280, bh = 64;
    int confirm_x = bx, confirm_y = by + 200; // place confirm below options
    int selected_mode = -1;
    MOUSEMSG m;
    while (1) {
        // redraw options
        if (selected_mode == MODE_PVP) draw_button_with_check(bx, by, bw, bh, "Local PvP (default)");
        else draw_button(bx, by, bw, bh, "Local PvP (default)");
        if (selected_mode == MODE_PVE) draw_button_with_check(bx, by + 90, bw, bh, "AI PvE");
        else draw_button(bx, by + 90, bw, bh, "AI PvE");
        draw_button(confirm_x, confirm_y, 120, 40, "Confirm");

        if (!poll_mouse_message(&m)) {
            if (layout_changed_since(&layout_version)) goto mode_menu;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto mode_menu;
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) { selected_mode = MODE_PVP; }
            else if (point_in_rect(m.x, m.y, bx, by + 90, bw, bh)) { selected_mode = MODE_PVE; }
            else if (point_in_rect(m.x, m.y, confirm_x, confirm_y, 120, 40)) {
                if (selected_mode == MODE_PVP || selected_mode == MODE_PVE) {
                    char msg[64]; snprintf(msg, sizeof(msg), "Mode confirmed: %s", (selected_mode == MODE_PVE) ? "PvE" : "PvP");
                    draw_overlay_message(msg);
                    return selected_mode;
                } else {
                    draw_overlay_message("No mode selected");
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
