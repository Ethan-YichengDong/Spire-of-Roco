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

#define UI_PAD 12
#define UI_GAP 8
#define UI_CARD_H 74
#define UI_BUTTON_H 44
#define UI_MESSAGE_HISTORY 3

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
static int g_defer_present = 0;
static char g_message_history[UI_MESSAGE_HISTORY][160] = { {0}, {0}, {0} };
static int g_message_count = 0;

static void draw_background_shell();
static void present_frame();
static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh);
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
static void outtextxy_clipped_utf8(int x, int y, int max_w, const char* utf8) {
    std::string s = utf8_to_acp_str(utf8);
    if (max_w <= 0) return;
    if (textwidth(s.c_str()) > max_w) {
        while (s.size() > 3 && textwidth((s + "...").c_str()) > max_w) {
            s.erase(s.size() - 1);
        }
        s += "...";
    }
    outtextxy(x, y, s.c_str());
}
static void settextstyle_utf8(int height, int width, const char* utf8Name) {
    std::string s = utf8_to_acp_str(utf8Name);
    settextstyle(height, width, s.c_str());
}
static void draw_line(const char* s) { outtextxy_utf8(g_main_x + 16, g_draw_y, s); g_draw_y += 26; }
#ifdef USE_EASYX
static void draw_overlay_message(const char* utf8msg);

typedef enum {
    UI_MSG_HINT = 0,
    UI_MSG_CONFIRM,
    UI_MSG_ERROR
} UiMessageKind;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} UiRect;

static void set_ui_rect(UiRect* rect, int x, int y, int w, int h) {
    if (!rect) return;
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
}

static int point_in_ui_rect(int x, int y, const UiRect* rect) {
    return rect && point_in_rect(x, y, rect->x, rect->y, rect->w, rect->h);
}

static COLORREF element_color(ElementType element) {
    switch (element) {
        case ELEMENT_WATER: return RGB(64, 140, 210);
        case ELEMENT_FIRE: return RGB(205, 82, 55);
        case ELEMENT_GRASS: return RGB(84, 151, 86);
        case ELEMENT_ELECTRIC: return RGB(213, 164, 47);
        case ELEMENT_NORMAL:
        default: return RGB(126, 124, 115);
    }
}

static const char* element_label(ElementType element) {
    switch (element) {
        case ELEMENT_WATER: return "Water";
        case ELEMENT_FIRE: return "Fire";
        case ELEMENT_GRASS: return "Grass";
        case ELEMENT_ELECTRIC: return "Electric";
        case ELEMENT_NORMAL:
        default: return "Normal";
    }
}

static const char* card_type_label(CardType type) {
    switch (type) {
        case CARD_TYPE_ATTACK: return "Attack";
        case CARD_TYPE_SKILL: return "Skill";
        case CARD_TYPE_POWER: return "Power";
        default: return "Card";
    }
}

static const char* target_type_label(TargetType target) {
    switch (target) {
        case TARGET_ENEMY_SINGLE: return "Enemy";
        case TARGET_ENEMY_ALL: return "All Enemies";
        case TARGET_SELF_SINGLE: return "Ally";
        case TARGET_SELF_ALL: return "All Allies";
        default: return "Target";
    }
}

static const char* buff_label(BuffType buff) {
    switch (buff) {
        case BUFF_SHIELD: return "Shield";
        case BUFF_WET: return "Wet";
        case BUFF_BURN: return "Burn";
        case BUFF_POISON: return "Poison";
        case BUFF_POWER: return "Power";
        case BUFF_NONE:
        default: return "";
    }
}

static COLORREF hp_state_color(int hp, int max_hp) {
    if (max_hp <= 0) return RGB(120, 120, 120);
    int pct = (hp * 100) / max_hp;
    if (pct <= 25) return RGB(201, 62, 56);
    if (pct <= 55) return RGB(213, 151, 55);
    return RGB(71, 157, 91);
}

static void draw_section_title(int x, int y, const char* title) {
    settextstyle_utf8(22, 0, "SimSun");
    settextcolor(RGB(28, 36, 42));
    outtextxy_utf8(x, y, title);
    settextstyle_utf8(20, 0, "SimSun");
}

static void draw_soft_panel(int x, int y, int w, int h, COLORREF fill, COLORREF border) {
    setfillcolor(fill);
    fillrectangle(x, y, x + w, y + h);
    setlinecolor(border);
    rectangle(x, y, x + w, y + h);
}

static void present_frame() {
    if (g_defer_present > 0) return;
    FlushBatchDraw();
}

static void begin_deferred_present() {
    g_defer_present++;
}

static void end_deferred_present() {
    if (g_defer_present > 0) g_defer_present--;
    if (g_defer_present == 0) FlushBatchDraw();
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

static void draw_simple_status_bar(const char* title) {
    update_layout();
    draw_art_or_fill(&g_art_status_panel, 0, 0, g_ui_w, g_status_h, RGB(238, 244, 252));
    setlinecolor(RGB(70, 95, 130));
    rectangle(0, 0, g_ui_w - 1, g_status_h);
    settextcolor(RGB(26, 36, 45));
    outtextxy_utf8(g_ui_margin, 12, title ? title : "Spire of Roco");
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

static void draw_button_state(int x, int y, int w, int h, const char* label, int selected, int hover, int disabled) {
    IMAGE* img = disabled ? &g_art_button_disabled : (selected ? &g_art_button_selected : &g_art_button_idle);
    COLORREF fill = disabled ? RGB(204, 207, 203) : (selected ? RGB(199, 229, 208) : RGB(236, 230, 198));
    draw_art_or_fill(img, x, y, w, h, fill);
    if (hover && !disabled) {
        setfillcolor(selected ? RGB(210, 240, 220) : RGB(246, 238, 207));
        fillrectangle(x + 2, y + 2, x + w - 2, y + h - 2);
    }
    setlinecolor(disabled ? RGB(116, 124, 124) : (selected ? RGB(45, 112, 80) : (hover ? RGB(80, 113, 154) : RGB(65, 56, 43))));
    rectangle(x, y, x + w, y + h);
    if (selected) {
        setfillcolor(RGB(52, 150, 99));
        fillrectangle(x + 7, y + 7, x + 22, y + h - 7);
    }
    settextcolor(disabled ? RGB(95, 98, 98) : (selected ? RGB(20, 42, 34) : RGB(29, 31, 30)));
    outtextxy_clipped_utf8(x + (selected ? 30 : 12), y + (h - 20) / 2, w - (selected ? 42 : 24), label);
    settextcolor(RGB(31, 37, 41));
    present_frame();
}

static void draw_button(int x, int y, int w, int h, const char* label) {
    draw_button_state(x, y, w, h, label, 0, 0, 0);
}

// Draw a button and mark it as checked (used to indicate a confirmed selection)
static void draw_button_with_check(int x, int y, int w, int h, const char* label) {
    draw_button_state(x, y, w, h, label, 1, 0, 0);
}

// Draw a non-intrusive overlay message at the bottom of the screen to avoid
// overlapping interactive UI elements (buttons, lists).
static void draw_overlay_message_kind(const char* utf8msg, UiMessageKind kind) {
    update_layout();
    if (utf8msg && utf8msg[0] != '\0' && strncmp(g_message_history[0], utf8msg, sizeof(g_message_history[0])) != 0) {
        for (int i = UI_MESSAGE_HISTORY - 1; i > 0; i--) {
            strncpy(g_message_history[i], g_message_history[i - 1], sizeof(g_message_history[i]) - 1);
            g_message_history[i][sizeof(g_message_history[i]) - 1] = '\0';
        }
        strncpy(g_message_history[0], utf8msg, sizeof(g_message_history[0]) - 1);
        g_message_history[0][sizeof(g_message_history[0]) - 1] = '\0';
        if (g_message_count < UI_MESSAGE_HISTORY) g_message_count++;
    }
    int x = g_ui_margin;
    int w = g_ui_w - (g_ui_margin * 2);
    int h = 70;
    int y = g_ui_h - g_ui_margin - h;
    COLORREF border = RGB(65, 56, 43);
    COLORREF accent = RGB(80, 113, 154);
    if (kind == UI_MSG_CONFIRM) { border = RGB(45, 112, 80); accent = RGB(52, 150, 99); }
    if (kind == UI_MSG_ERROR) { border = RGB(150, 62, 54); accent = RGB(201, 62, 56); }
    draw_art_or_fill(&g_art_message_panel, x, y, w, h, RGB(246, 242, 222));
    setlinecolor(border);
    rectangle(x, y, x + w, y + h);
    setfillcolor(accent);
    fillrectangle(x + 8, y + 10, x + 14, y + h - 10);
    settextcolor(RGB(29, 31, 30));
    int line_y = y + 9;
    for (int i = 0; i < g_message_count; i++) {
        settextcolor(i == 0 ? RGB(29, 31, 30) : RGB(93, 89, 79));
        outtextxy_clipped_utf8(x + 24, line_y, w - 36, g_message_history[i]);
        line_y += 20;
    }
    present_frame();
}

static void draw_overlay_message(const char* utf8msg) {
    draw_overlay_message_kind(utf8msg, UI_MSG_HINT);
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

static void draw_meter_colored(int x, int y, int w, int value, int max_value, COLORREF fill) {
    int inner_w = w - 4;
    int fill_w = clamp_meter_width(value, max_value, inner_w);
    setfillcolor(RGB(36, 40, 43));
    fillrectangle(x, y, x + w, y + 14);
    if (fill_w > 0) {
        setfillcolor(fill);
        fillrectangle(x + 2, y + 2, x + 2 + fill_w, y + 12);
    }
    setlinecolor(RGB(68, 55, 35));
    rectangle(x, y, x + w, y + 14);
}

static void draw_character_hud_row(const Character* ch, int x, int y, int w, const char* prefix, int active, int hover) {
    if (!ch) return;
    char buf[128];
    COLORREF row_fill = ch->is_alive ? (active ? RGB(236, 244, 229) : RGB(235, 232, 213)) : RGB(205, 205, 198);
    COLORREF row_border = active ? RGB(52, 150, 99) : (hover ? RGB(80, 113, 154) : RGB(135, 122, 96));
    draw_soft_panel(x, y, w, 42, row_fill, row_border);
    if (active) {
        setfillcolor(RGB(52, 150, 99));
        fillrectangle(x, y, x + 5, y + 42);
    }
    IMAGE* portrait = portrait_for_element(ch->element);
    draw_art_or_fill(portrait, x + 8, y + 5, 32, 32, RGB(167, 158, 139));
    setlinecolor(ch->is_alive ? RGB(82, 70, 45) : RGB(100, 100, 100));
    rectangle(x + 8, y + 5, x + 40, y + 37);
    if (!ch->is_alive) {
        setlinecolor(RGB(130, 30, 30));
        line(x + 11, y + 8, x + 37, y + 34);
        line(x + 37, y + 8, x + 11, y + 34);
    }
    settextcolor(RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "%s %s", prefix, ch->name);
    outtextxy_clipped_utf8(x + 48, y + 4, w - 58, buf);
    snprintf(buf, sizeof(buf), "HP %d/%d", ch->hp, ch->max_hp);
    outtextxy_utf8(x + 48, y + 22, buf);
    draw_meter_colored(x + 118, y + 24, w - 128, ch->hp, ch->max_hp, hp_state_color(ch->hp, ch->max_hp));
}

// Render both teams' characters and HP on a side panel (right side) so that
// their HP is always visible during the turn.
static void draw_team_hp_panel(const GameState* st) {
    update_layout();
    int x = g_side_x + 10, y = g_team_panel_y + 10;
    char buf[128];
    draw_art_or_fill(&g_art_side_panel, g_side_x, g_team_panel_y, g_side_w, g_team_panel_h, RGB(242, 239, 220));
    setlinecolor(RGB(62, 76, 92));
    rectangle(g_side_x, g_team_panel_y, g_side_x + g_side_w, g_team_panel_y + g_team_panel_h);
    if (!st) {
        draw_section_title(x + 6, y, "Team HUD");
        settextcolor(RGB(93, 89, 79));
        outtextxy_utf8(x + 6, y + 36, "Team status appears here.");
        return;
    }
    settextcolor(RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "Player 1  %s", st->p1.name);
    draw_section_title(x + 6, y, buf); y += 30;
    for (int i = 0; i < TEAM_SIZE; i++) {
        snprintf(buf, sizeof(buf), "%d", i + 1);
        draw_character_hud_row(&st->p1.team[i], x + 6, y, g_side_w - 32, buf, i == st->p1.active_idx, 0);
        y += 46;
    }
    y += 8;
    snprintf(buf, sizeof(buf), "Player 2  %s", st->p2.name);
    draw_section_title(x + 6, y, buf); y += 30;
    for (int i = 0; i < TEAM_SIZE; i++) {
        snprintf(buf, sizeof(buf), "%d", i + 1);
        draw_character_hud_row(&st->p2.team[i], x + 6, y, g_side_w - 32, buf, i == st->p2.active_idx, 0);
        y += 46;
    }
}

static void build_card_stats_text(const Card* c, char* out, size_t out_size) {
    char stats[128] = "";
    int wrote = 0;
    if (!c || !out || out_size == 0) return;
    if (c->base_damage > 0) wrote += snprintf(stats + wrote, sizeof(stats) - wrote, "DMG %d  ", c->base_damage);
    if (c->base_defense > 0) wrote += snprintf(stats + wrote, sizeof(stats) - wrote, "SHD %d  ", c->base_defense);
    if (c->base_heal > 0) wrote += snprintf(stats + wrote, sizeof(stats) - wrote, "HEAL %d  ", c->base_heal);
    if (c->buff_effect != BUFF_NONE) {
        const char* buff = buff_label(c->buff_effect);
        if (buff && buff[0] != '\0') {
            wrote += snprintf(stats + wrote, sizeof(stats) - wrote, "%s", buff);
            if (c->buff_value > 0) wrote += snprintf(stats + wrote, sizeof(stats) - wrote, " +%d", c->buff_value);
            if (c->buff_duration > 0) wrote += snprintf(stats + wrote, sizeof(stats) - wrote, " %dt", c->buff_duration);
            wrote += snprintf(stats + wrote, sizeof(stats) - wrote, "  ");
        }
    }
    if (wrote == 0) snprintf(stats, sizeof(stats), "Utility  ");
    snprintf(out, out_size, "%sTarget: %s", stats, target_type_label(c->target_type));
}

static void draw_card_panel(const Card* c, int idx, int x, int y, int w, int h, int selected, int hover, int disabled, int quantity) {
    if (!c) return;
    char buf[256];
    int compact = h < 72 || w < 360;
    COLORREF elem = disabled ? RGB(128, 128, 128) : element_color(c->element);
    COLORREF fill = disabled ? RGB(210, 211, 205) : (selected ? RGB(239, 246, 228) : (hover ? RGB(246, 239, 216) : RGB(232, 225, 197)));
    COLORREF border = disabled ? RGB(126, 126, 120) : (selected ? RGB(52, 150, 99) : (hover ? RGB(80, 113, 154) : RGB(85, 74, 56)));
    draw_art_or_fill(&g_art_card_plate, x, y, w, h, fill);
    setfillcolor(fill);
    fillrectangle(x + 2, y + 2, x + w - 2, y + h - 2);
    setlinecolor(border);
    rectangle(x, y, x + w, y + h);
    setfillcolor(elem);
    fillrectangle(x, y, x + 9, y + h);
    if (selected) {
        setlinecolor(RGB(52, 150, 99));
        rectangle(x + 2, y + 2, x + w - 2, y + h - 2);
    }

    setfillcolor(disabled ? RGB(150, 150, 145) : RGB(244, 226, 134));
    fillrectangle(x + w - 46, y + 8, x + w - 12, y + 34);
    setlinecolor(RGB(93, 78, 38));
    rectangle(x + w - 46, y + 8, x + w - 12, y + 34);
    snprintf(buf, sizeof(buf), "%d", c->energy_cost);
    settextcolor(RGB(47, 39, 18));
    outtextxy_utf8(x + w - 34, y + 12, buf);

    settextstyle_utf8(compact ? 19 : 22, 0, "SimSun");
    settextcolor(disabled ? RGB(91, 91, 88) : RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "[%d] %s", idx, c->name);
    outtextxy_clipped_utf8(x + 20, y + (compact ? 6 : 8), w - 78, buf);
    settextstyle_utf8(compact ? 16 : 18, 0, "SimSun");
    settextcolor(disabled ? RGB(104, 104, 100) : RGB(74, 67, 55));
    snprintf(buf, sizeof(buf), "%s / %s", element_label(c->element), card_type_label(c->type));
    outtextxy_clipped_utf8(x + 20, y + (compact ? 28 : 34), w - 44, buf);
    build_card_stats_text(c, buf, sizeof(buf));
    outtextxy_clipped_utf8(x + 20, y + (compact ? h - 20 : 53), w - 34, buf);
    if (quantity > 0) {
        snprintf(buf, sizeof(buf), "x%d", quantity);
        setfillcolor(RGB(50, 68, 86));
        fillrectangle(x + w - 47, y + h - 27, x + w - 12, y + h - 8);
        settextcolor(RGB(246, 242, 222));
        outtextxy_utf8(x + w - 40, y + h - 25, buf);
    }
    settextstyle_utf8(20, 0, "SimSun");
    settextcolor(RGB(31, 37, 41));
    present_frame();
}

static void draw_character_option_panel(const Character* ch, int idx, int x, int y, int w, int h, int selected, int hover, int disabled) {
    if (!ch) return;
    char buf[160];
    COLORREF fill = disabled ? RGB(207, 207, 201) : (selected ? RGB(237, 246, 230) : (hover ? RGB(246, 239, 216) : RGB(232, 225, 197)));
    COLORREF border = disabled ? RGB(124, 124, 119) : (selected ? RGB(52, 150, 99) : (hover ? RGB(80, 113, 154) : RGB(85, 74, 56)));
    draw_soft_panel(x, y, w, h, fill, border);
    setfillcolor(disabled ? RGB(128, 128, 128) : element_color(ch->element));
    fillrectangle(x, y, x + 8, y + h);
    IMAGE* portrait = portrait_for_element(ch->element);
    draw_art_or_fill(portrait, x + 16, y + 8, 34, 34, RGB(167, 158, 139));
    setlinecolor(RGB(82, 70, 45));
    rectangle(x + 16, y + 8, x + 50, y + 42);
    settextcolor(disabled ? RGB(92, 92, 88) : RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "[%d] %s", idx, ch->name);
    outtextxy_clipped_utf8(x + 60, y + 8, w - 72, buf);
    snprintf(buf, sizeof(buf), "HP %d/%d   %s   Speed %d%s", ch->hp, ch->max_hp, element_label(ch->element), ch->speed, ch->is_alive ? "" : "   Defeated");
    settextcolor(disabled ? RGB(104, 104, 100) : RGB(74, 67, 55));
    outtextxy_clipped_utf8(x + 60, y + 28, w - 72, buf);
    draw_meter_colored(x + 60, y + h - 17, w - 78, ch->hp, ch->max_hp, hp_state_color(ch->hp, ch->max_hp));
    present_frame();
}

static int compute_planned_card_rects(int card_count, int start_y, int footer_y, int* xs, int* ys, int* ws, int* hs) {
    int available_w = g_main_w - 48;
    int available_h = footer_y - start_y - UI_GAP;
    int single_rows = available_h / (UI_CARD_H + UI_GAP);
    int columns = (card_count > single_rows && available_w >= 560) ? 2 : 1;
    int rows = (card_count + columns - 1) / columns;
    int card_h = UI_CARD_H;
    if (rows > 0) {
        card_h = (available_h - ((rows - 1) * UI_GAP)) / rows;
        card_h = clamp_int(card_h, 58, UI_CARD_H);
    }
    int card_w = (columns == 2) ? ((available_w - UI_GAP) / 2) : clamp_int(available_w, 380, 680);
    int origin_x = g_main_x + 24;
    for (int i = 0; i < card_count; i++) {
        int col = i % columns;
        int row = i / columns;
        xs[i] = origin_x + col * (card_w + UI_GAP);
        ys[i] = start_y + row * (card_h + UI_GAP);
        ws[i] = card_w;
        hs[i] = card_h;
    }
    return columns;
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
}

static void draw_battle_plan_screen(GameState* st, int player_id, const ActionRecord* records, int record_count, const char* title,
                                    int hover_action, int can_edit) {
    begin_deferred_present();
    draw_planning_shell(st, player_id, records, record_count, title);
    int bx = g_main_x + 24, by = g_draw_y + 10, bw = 240, bh = UI_BUTTON_H;
    draw_button_state(bx, by, bw, bh, "End Turn", 0, hover_action == 0, 0);
    draw_button_state(bx, by + 55, bw, bh, "Play Card", 0, hover_action == 1, 0);
    draw_button_state(bx, by + 110, bw, bh, "Switch Character", 0, hover_action == 2, 0);
    draw_button_state(bx, by + 165, bw, bh, can_edit ? "Edit Card" : "Edit Action", 0, hover_action == 3, !can_edit);
    draw_overlay_message_kind("Choose an action for this turn.", UI_MSG_HINT);
    end_deferred_present();
}

static void draw_target_select_screen(GameState* st, int player_id, const ActionRecord* records, int record_count,
                                      const Card* card, const Character* target_team, int hover_target) {
    char buf[256];
    begin_deferred_present();
    draw_planning_shell(st, player_id, records, record_count, "Choose a target");
    snprintf(buf, sizeof(buf), "Card: %s", card ? card->name : "");
    draw_line(buf);
    int tx = g_main_x + 48, ty = g_draw_y + 20, tw = clamp_int(g_main_w - 96, 360, 620), th = 58;
    for (int t = 0; t < TEAM_SIZE; t++) {
        int disabled = target_team && !target_team[t].is_alive && card && card->target_type != TARGET_SELF_SINGLE;
        draw_character_option_panel(&target_team[t], t, tx, ty + t * (th + UI_GAP), tw, th, hover_target == t, hover_target == t, disabled);
    }
    draw_button_state(g_main_x + 16, bottom_button_y(), 120, 38, "Back", 0, hover_target == 10, 0);
    draw_overlay_message_kind("Select a highlighted target or go back.", UI_MSG_HINT);
    end_deferred_present();
}

static void draw_deck_build_screen(int player_id, int selected_count, int max_select, const int* qty, int show,
                                   int selected_idx, int hover_card, int hover_minus, int hover_plus,
                                   int hover_finish, int hover_confirm,
                                   UiRect* card_rects, UiRect* minus_rects, UiRect* plus_rects,
                                   UiRect* finish_rect, UiRect* confirm_rect) {
    char buf[256];
    begin_deferred_present();
    reset_draw_y();
    draw_simple_status_bar("Deck Build");
    draw_team_hp_panel(NULL);
    snprintf(buf, sizeof(buf), "Deck Build - Player %d", player_id);
    draw_section_title(g_main_x + UI_PAD + 4, g_draw_y, buf);
    g_draw_y += 32;
    snprintf(buf, sizeof(buf), "Selected %d/%d", selected_count, max_select);
    draw_line(buf);
    int cx = g_main_x + 24;
    int cy = g_draw_y + 8;
    int card_w = qty ? clamp_int(g_main_w - 260, 360, 620) : clamp_int(g_main_w - 64, 360, 680);
    int ch = UI_CARD_H;
    for (int i = 0; i < show; i++) {
        int y = cy + i * (ch + UI_GAP);
        set_ui_rect(card_rects ? &card_rects[i] : NULL, cx, y, card_w, ch);
        draw_card_panel(&g_all_cards[i], i, cx, y, card_w, ch, i == selected_idx || (qty && qty[i] > 0), hover_card == i, 0, qty ? qty[i] : 0);
        if (qty) {
            int x_minus = cx + card_w + UI_GAP;
            int x_qty = x_minus + 42 + UI_GAP;
            int x_plus = x_qty + 58 + UI_GAP;
            set_ui_rect(minus_rects ? &minus_rects[i] : NULL, x_minus, y + 15, 42, 38);
            set_ui_rect(plus_rects ? &plus_rects[i] : NULL, x_plus, y + 15, 42, 38);
            draw_button_state(x_minus, y + 15, 42, 38, "-", 0, hover_minus == i, qty[i] <= 0);
            snprintf(buf, sizeof(buf), "%d", qty[i]);
            draw_button_state(x_qty, y + 15, 58, 38, buf, qty[i] > 0, 0, 0);
            draw_button_state(x_plus, y + 15, 42, 38, "+", 0, hover_plus == i, selected_count >= max_select);
        }
    }
    int footer_y = bottom_button_y();
    set_ui_rect(finish_rect, cx, footer_y, 150, 40);
    set_ui_rect(confirm_rect, cx + 162, footer_y, 130, 40);
    draw_button_state(cx, footer_y, 150, 40, qty ? "End Building" : "Finish Build", 0, hover_finish, 0);
    if (!qty) draw_button_state(cx + 162, footer_y, 130, 40, "Confirm", selected_idx >= 0, hover_confirm, 0);
    snprintf(buf, sizeof(buf), "Total cards: %d/%d", selected_count, max_select);
    draw_overlay_message_kind(buf, UI_MSG_HINT);
    end_deferred_present();
}

static void draw_main_menu_screen(int selected_mode, int hover_choice, int hover_confirm) {
    begin_deferred_present();
    reset_draw_y();
    draw_simple_status_bar("Main Menu");
    draw_section_title(g_main_x + UI_PAD + 4, g_draw_y, "Spire of Roco");
    g_draw_y += 38;
    draw_line("Select battle mode");
    int bx = g_main_x + 52, by = g_draw_y + 12, bw = 320, bh = 64;
    draw_button_state(bx, by, bw, bh, "Local PvP", selected_mode == MODE_PVP, hover_choice == MODE_PVP, 0);
    draw_button_state(bx, by + 88, bw, bh, "AI PvE", selected_mode == MODE_PVE, hover_choice == MODE_PVE, 0);
    draw_button_state(bx, by + 190, 140, 42, "Confirm", selected_mode >= 0, hover_confirm, 0);
    draw_overlay_message_kind("F11 toggles fullscreen. Esc quits.", UI_MSG_HINT);
    end_deferred_present();
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
    int y = g_draw_y;
    int x = g_main_x + 16;
    int w = clamp_int(g_main_w - 64, 360, 620);
    draw_character_option_panel(ch, ch->char_id, x, y, w, 58, 0, 0, !ch->is_alive);
    g_draw_y += 66;
#else
    printf("%s (ID:%d) HP:%d/%d Elem:%d Speed:%d %s\n",
           ch->name, ch->char_id, ch->hp, ch->max_hp, ch->element, ch->speed,
           ch->is_alive ? "" : "[DEAD]");
#endif
}

void print_card(const Card* c, int idx) {
    if (!c) return;
#ifdef USE_EASYX
    int y = g_draw_y;
    int x = g_main_x + 16;
    int w = g_main_w - 32;
    if (w > 700) w = 700;
    draw_card_panel(c, idx, x, y, w, UI_CARD_H, 0, 0, 0, 0);
    g_draw_y += UI_CARD_H + UI_GAP;
#else
    printf(" [%2d] %s (ID:%d) Cost:%d Dmg:%d Def:%d Heal:%d Type:%d Target:%d\n",
           idx, c->name, c->card_id, c->energy_cost, c->base_damage, c->base_defense, c->base_heal, c->type, c->target_type);
#endif
}

static void draw_battle_screen(GameState state) {
#ifdef USE_EASYX
    begin_deferred_present();
    reset_draw_y();
    draw_status_bar(&state, state.current_turn, "Board");
    draw_team_hp_panel(&state);
    char buf[256];
    snprintf(buf,sizeof(buf), "Game Board - Round %d", state.round_count);
    draw_section_title(g_main_x + 16, g_draw_y, buf);
    g_draw_y += 32;

    snprintf(buf,sizeof(buf), "Player 1: %s", state.p1.name); draw_line(buf);
    draw_line("Active");
    print_character(&state.p1.team[state.p1.active_idx]);
    snprintf(buf,sizeof(buf), "Energy: %d/%d  Hand:%d  Draw:%d  Discard:%d", state.p1.energy, state.p1.max_energy, state.p1.hand_count, state.p1.draw_count, state.p1.discard_count); draw_line(buf);
    draw_meter(g_main_x + 124, g_draw_y - 24, 220, state.p1.energy, state.p1.max_energy, &g_art_energy_fill);
    if (state.p1.hand_count > 0) {
        draw_line("Hand");
        for (int i = 0; i < state.p1.hand_count && g_draw_y + UI_CARD_H < bottom_button_y(); i++) {
            print_card(&state.p1.hand[i], i);
        }
    }

    if (g_draw_y + 172 < bottom_button_y()) {
        g_draw_y += UI_GAP;
        snprintf(buf,sizeof(buf), "Player 2: %s", state.p2.name); draw_line(buf);
        draw_line("Active");
        print_character(&state.p2.team[state.p2.active_idx]);
        snprintf(buf,sizeof(buf), "Energy: %d/%d  Hand:%d  Draw:%d  Discard:%d", state.p2.energy, state.p2.max_energy, state.p2.hand_count, state.p2.draw_count, state.p2.discard_count); draw_line(buf);
        draw_meter(g_main_x + 124, g_draw_y - 24, 220, state.p2.energy, state.p2.max_energy, &g_art_energy_fill);
    }
    draw_overlay_message_kind("Battle state refreshed.", UI_MSG_HINT);
    end_deferred_present();
#endif
}

void RenderGameBoard(GameState state) {
#ifdef USE_EASYX
    draw_battle_screen(state);
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
    layout_version = g_ui_layout_version;
    {
        int can_edit = has_card_record(records, record_count);
        int bx = g_main_x + 24, by = g_draw_y + 10, bw = 240, bh = UI_BUTTON_H;
        int hover_action = -1;
        int need_redraw = 1;
        while (1) {
            if (need_redraw) {
                draw_battle_plan_screen(&state, player_id, records, record_count, "Plan your turn", hover_action, can_edit);
                bx = g_main_x + 24; by = g_draw_y + 10; bw = 240; bh = UI_BUTTON_H;
                need_redraw = 0;
            }
            if (!poll_mouse_message(&m)) {
                if (layout_changed_since(&layout_version)) goto main_menu;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto main_menu;
            int new_hover = -1;
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) new_hover = 0;
            else if (point_in_rect(m.x, m.y, bx, by + 55, bw, bh)) new_hover = 1;
            else if (point_in_rect(m.x, m.y, bx, by + 110, bw, bh)) new_hover = 2;
            else if (point_in_rect(m.x, m.y, bx, by + 165, bw, bh)) new_hover = 3;
            if (new_hover != hover_action) {
                hover_action = new_hover;
                need_redraw = 1;
            }
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) {
                act.type = ACTION_END_TURN;
                return act;
            }
            if (point_in_rect(m.x, m.y, bx, by + 55, bw, bh)) {
                if (p->hand_count <= 0) {
                    draw_overlay_message_kind("Hand empty", UI_MSG_ERROR);
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
    layout_version = g_ui_layout_version;
    {
        int back_x = g_main_x + 16, back_y = bottom_button_y(), back_w = 120, back_h = 38;
        int card_x[MAX_HAND_SIZE] = {0};
        int card_y[MAX_HAND_SIZE] = {0};
        int card_w[MAX_HAND_SIZE] = {0};
        int card_h[MAX_HAND_SIZE] = {0};
        int hover_card = -1;
        int hover_back = 0;
        int need_redraw = 1;
        while (1) {
            if (need_redraw) {
                begin_deferred_present();
                draw_planning_shell(&state, player_id, records, record_count, "Choose a card");
                back_x = g_main_x + 16; back_y = bottom_button_y();
                int card_start_y = g_draw_y + 10;
                compute_planned_card_rects(p->hand_count, card_start_y, back_y, card_x, card_y, card_w, card_h);
                for (int i = 0; i < p->hand_count; i++) {
                    draw_card_panel(&p->hand[i], i, card_x[i], card_y[i], card_w[i], card_h[i], 0, hover_card == i, p->energy < p->hand[i].energy_cost, 0);
                }
                draw_button_state(back_x, back_y, back_w, back_h, "Back", 0, hover_back, 0);
                draw_overlay_message_kind("Choose a playable card.", UI_MSG_HINT);
                end_deferred_present();
                need_redraw = 0;
            }
            if (!poll_mouse_message(&m)) {
                if (layout_changed_since(&layout_version)) goto card_select;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto card_select;
            int new_hover_card = -1;
            int new_hover_back = 0;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) new_hover_back = 1;
            for (int i = 0; i < p->hand_count; i++) {
                if (point_in_rect(m.x, m.y, card_x[i], card_y[i], card_w[i], card_h[i])) {
                    new_hover_card = i;
                    break;
                }
            }
            if (new_hover_card != hover_card || new_hover_back != hover_back) {
                hover_card = new_hover_card;
                hover_back = new_hover_back;
                need_redraw = 1;
            }
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) {
                reset_pending_action(&act, player_id);
                goto main_menu;
            }
            for (int i = 0; i < p->hand_count; i++) {
                if (!point_in_rect(m.x, m.y, card_x[i], card_y[i], card_w[i], card_h[i])) continue;
                if (p->energy < p->hand[i].energy_cost) {
                    draw_overlay_message_kind("Not enough energy", UI_MSG_ERROR);
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
    layout_version = g_ui_layout_version;
    {
        Card* c = &p->hand[act.card_hand_idx];
        Character* target_team = NULL;
        if (c->target_type == TARGET_SELF_SINGLE) {
            target_team = p->team;
        } else {
            target_team = (player_id == state.p1.player_id) ? state.p2.team : state.p1.team;
        }
        int tx = g_main_x + 48, ty = g_draw_y + 20, tw = clamp_int(g_main_w - 96, 360, 620), th = 58;
        int back_x = g_main_x + 16, back_y = bottom_button_y(), back_w = 120, back_h = 38;
        int hover_target = -1;
        int need_redraw = 1;
        while (1) {
            if (need_redraw) {
                draw_target_select_screen(&state, player_id, records, record_count, c, target_team, hover_target);
                tx = g_main_x + 48; ty = g_draw_y + 20; tw = clamp_int(g_main_w - 96, 360, 620); th = 58;
                back_x = g_main_x + 16; back_y = bottom_button_y();
                need_redraw = 0;
            }
            if (!poll_mouse_message(&m)) {
                if (layout_changed_since(&layout_version)) goto target_select;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto target_select;
            int new_hover = -1;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) new_hover = 10;
            for (int t = 0; t < TEAM_SIZE; t++) {
                int ry = ty + t * (th + UI_GAP);
                if (point_in_rect(m.x, m.y, tx, ry, tw, th)) {
                    new_hover = t;
                    break;
                }
            }
            if (new_hover != hover_target) {
                hover_target = new_hover;
                need_redraw = 1;
            }
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) {
                reset_pending_action(&act, player_id);
                goto main_menu;
            }
            for (int t = 0; t < TEAM_SIZE; t++) {
                int ry = ty + t * (th + UI_GAP);
                if (!point_in_rect(m.x, m.y, tx, ry, tw, th)) continue;
                if (!target_team[t].is_alive && c->target_type != TARGET_SELF_SINGLE) {
                    draw_overlay_message_kind("Target is defeated", UI_MSG_ERROR);
                    break;
                }
                act.target_idx = t;
                return act;
            }
        }
    }

switch_select:
    p = mutable_player(&state, player_id);
    layout_version = g_ui_layout_version;
    {
        int sx = g_main_x + 48, sy = g_draw_y + 20, sw = clamp_int(g_main_w - 96, 360, 620), sh = 58;
        int back_x = g_main_x + 16, back_y = bottom_button_y(), back_w = 120, back_h = 38;
        int hover_char = -1;
        int hover_back = 0;
        int need_redraw = 1;
        while (1) {
            if (need_redraw) {
                begin_deferred_present();
                draw_planning_shell(&state, player_id, records, record_count, "Choose active character");
                sx = g_main_x + 48; sy = g_draw_y + 20; sw = clamp_int(g_main_w - 96, 360, 620); sh = 58;
                back_x = g_main_x + 16; back_y = bottom_button_y();
                for (int i = 0; i < TEAM_SIZE; i++) {
                    draw_character_option_panel(&p->team[i], i, sx, sy + i * (sh + UI_GAP), sw, sh, i == p->active_idx, hover_char == i, !p->team[i].is_alive);
                }
                draw_button_state(back_x, back_y, back_w, back_h, "Back", 0, hover_back, 0);
                draw_overlay_message_kind("Choose a living team member.", UI_MSG_HINT);
                end_deferred_present();
                need_redraw = 0;
            }
            if (!poll_mouse_message(&m)) {
                if (layout_changed_since(&layout_version)) goto switch_select;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto switch_select;
            int new_hover_char = -1;
            int new_hover_back = 0;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) new_hover_back = 1;
            for (int i = 0; i < TEAM_SIZE; i++) {
                int ry = sy + i * (sh + UI_GAP);
                if (point_in_rect(m.x, m.y, sx, ry, sw, sh)) {
                    new_hover_char = i;
                    break;
                }
            }
            if (new_hover_char != hover_char || new_hover_back != hover_back) {
                hover_char = new_hover_char;
                hover_back = new_hover_back;
                need_redraw = 1;
            }
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) {
                reset_pending_action(&act, player_id);
                goto main_menu;
            }
            for (int i = 0; i < TEAM_SIZE; i++) {
                int ry = sy + i * (sh + UI_GAP);
                if (!point_in_rect(m.x, m.y, sx, ry, sw, sh)) continue;
                if (!p->team[i].is_alive) {
                    draw_overlay_message_kind("Character is defeated", UI_MSG_ERROR);
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
    begin_deferred_present();
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
        end_deferred_present();
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
    begin_deferred_present();
    reset_draw_y();
    draw_status_bar(&state, record ? record->player_id : state.current_turn, "Resolution");
    draw_team_hp_panel(&state);
    char buf[256];
    snprintf(buf, sizeof(buf), "Round Resolution  %d/%d", step_number, step_total);
    draw_section_title(g_main_x + 16, g_draw_y, buf);
    g_draw_y += 34;
    if (record) {
        snprintf(buf, sizeof(buf), "Resolving: %s", record->summary);
        draw_soft_panel(g_main_x + 16, g_draw_y, clamp_int(g_main_w - 48, 420, 760), 46, RGB(238, 232, 204), RGB(85, 74, 56));
        settextcolor(RGB(27, 33, 35));
        outtextxy_clipped_utf8(g_main_x + 30, g_draw_y + 14, clamp_int(g_main_w - 76, 360, 700), buf);
        g_draw_y += 58;
    } else {
        draw_line("Resolving: No actions to resolve");
    }
    draw_line("Active characters");
    int active_w = clamp_int((g_main_w - 56) / 2, 260, 380);
    int active_y = g_draw_y;
    draw_character_option_panel(&state.p1.team[state.p1.active_idx], state.p1.active_idx, g_main_x + 16, active_y, active_w, 58, 1, 0, !state.p1.team[state.p1.active_idx].is_alive);
    draw_character_option_panel(&state.p2.team[state.p2.active_idx], state.p2.active_idx, g_main_x + 28 + active_w, active_y, active_w, 58, 1, 0, !state.p2.team[state.p2.active_idx].is_alive);
    g_draw_y += 72;
    if (report && report->event_count > 0) {
        draw_line("Resolution events");
        for (int i = 0; i < report->event_count; i++) {
            const DamageResolutionEvent* event = &report->events[i];
            if (!event->has_damage) continue;
            int panel_w = clamp_int(g_main_w - 48, 420, 760);
            int panel_h = 48;
            COLORREF border = event->final_damage > 0 ? RGB(170, 68, 58) : RGB(52, 150, 99);
            draw_soft_panel(g_main_x + 16, g_draw_y, panel_w, panel_h, RGB(239, 232, 210), border);
            setfillcolor(border);
            fillrectangle(g_main_x + 16, g_draw_y, g_main_x + 24, g_draw_y + panel_h);
            snprintf(buf, sizeof(buf), "%s  HP %d -> %d",
                     event->target_name,
                     event->hp_before,
                     event->hp_after);
            settextcolor(RGB(27, 33, 35));
            outtextxy_clipped_utf8(g_main_x + 34, g_draw_y + 7, panel_w - 48, buf);
            snprintf(buf, sizeof(buf), "Damage %d   Element +%d   Shield %d",
                     event->final_damage,
                     event->element_bonus_damage,
                     event->shield_absorbed);
            settextcolor(RGB(84, 76, 62));
            outtextxy_clipped_utf8(g_main_x + 34, g_draw_y + 27, panel_w - 48, buf);
            g_draw_y += panel_h + UI_GAP;
        }
    } else {
        draw_line("Damage details: no damage");
    }
    draw_overlay_message_kind("Click or press any key to continue.", UI_MSG_CONFIRM);
    end_deferred_present();
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
    int layout_version = g_ui_layout_version;
    if (g_char_count == 0) { draw_line("Global character pool empty, returning 0"); if (out_count) *out_count = 0; return 0; }
    int sx = g_main_x + 24, sy = g_main_y + 128, sw = clamp_int(g_main_w - 64, 360, 680), sh = 58;
    int confirm_x = sx, confirm_y = bottom_button_y();
    int* selected = (int*)malloc(sizeof(int) * g_char_count);
    memset(selected, 0, sizeof(int) * g_char_count);
    int hover_idx = -1;
    int hover_confirm = 0;
    MOUSEMSG m;
    int need_redraw = 1;
    while (1) {
        if (need_redraw) {
            char header[160];
            reset_draw_y();
            draw_simple_status_bar("Character Draft");
            draw_team_hp_panel(st);
            snprintf(header, sizeof(header), "Character Draft - Player %d", player_id);
            draw_section_title(g_main_x + UI_PAD + 4, g_draw_y, header);
            g_draw_y += 34;
            snprintf(header, sizeof(header), "Choose up to %d", max_select);
            draw_line(header);
            sx = g_main_x + 24; sy = g_draw_y + 8; sw = clamp_int(g_main_w - 64, 360, 680); sh = 58;
            confirm_x = sx; confirm_y = bottom_button_y();
            begin_deferred_present();
            for (int i = 0; i < g_char_count; i++) {
                draw_character_option_panel(&g_all_characters[i], i, sx, sy + i * (sh + UI_GAP), sw, sh, selected[i], hover_idx == i, 0);
            }
            draw_button_state(confirm_x, confirm_y, 140, 40, "Confirm", 0, hover_confirm, 0);
            draw_overlay_message_kind("Select characters for your team.", UI_MSG_HINT);
            end_deferred_present();
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
        int new_hover_idx = -1;
        int new_hover_confirm = 0;
        for (int i = 0; i < g_char_count; i++) {
            int ry = sy + i * (sh + UI_GAP);
            if (point_in_rect(m.x, m.y, sx, ry, sw, sh)) {
                new_hover_idx = i;
                break;
            }
        }
        if (point_in_rect(m.x, m.y, confirm_x, confirm_y, 140, 40)) new_hover_confirm = 1;
        if (new_hover_idx != hover_idx || new_hover_confirm != hover_confirm) {
            hover_idx = new_hover_idx;
            hover_confirm = new_hover_confirm;
            need_redraw = 1;
        }
        if (m.uMsg == WM_LBUTTONDOWN) {
            int handled = 0;
            for (int i = 0; i < g_char_count; i++) {
                int rx = sx, ry = sy + i * (sh + UI_GAP);
                if (point_in_rect(m.x, m.y, rx, ry, sw, sh)) { selected[i] = !selected[i]; need_redraw = 1; handled = 1; break; }
            }
            if (!handled && point_in_rect(m.x, m.y, confirm_x, confirm_y, 140, 40)) {
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
    int layout_version = g_ui_layout_version;
    if (g_card_count == 0) { draw_line("Global card pool empty, returning -1"); return -1; }
    int max_rows = 8;
    int show = g_card_count < max_rows ? g_card_count : max_rows;
    UiRect card_rects[MAX_GLOBAL_CARDS] = {};
    UiRect finish_rect = {};
    UiRect confirm_rect = {};
    int selected_idx = -1;
    int hover_card = -1;
    int hover_finish = 0;
    int hover_confirm = 0;
    MOUSEMSG m;
    int need_redraw = 1;
    while (1) {
        if (need_redraw) {
            draw_deck_build_screen(player_id, current_deck_size, MAX_DECK_SIZE, NULL, show, selected_idx,
                                   hover_card, -1, -1, hover_finish, hover_confirm,
                                   card_rects, NULL, NULL, &finish_rect, &confirm_rect);
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (layout_changed_since(&layout_version)) goto single_card_select;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto single_card_select;
        int new_hover_card = -1;
        int new_hover_finish = 0;
        int new_hover_confirm = 0;
        for (int i = 0; i < show; i++) {
            if (point_in_ui_rect(m.x, m.y, &card_rects[i])) {
                new_hover_card = i;
                break;
            }
        }
        if (point_in_ui_rect(m.x, m.y, &finish_rect)) new_hover_finish = 1;
        if (point_in_ui_rect(m.x, m.y, &confirm_rect)) new_hover_confirm = 1;
        if (new_hover_card != hover_card || new_hover_finish != hover_finish || new_hover_confirm != hover_confirm) {
            hover_card = new_hover_card;
            hover_finish = new_hover_finish;
            hover_confirm = new_hover_confirm;
            need_redraw = 1;
        }
        if (m.uMsg == WM_LBUTTONDOWN) {
            int handled = 0;
            for (int i = 0; i < show; i++) {
                if (point_in_ui_rect(m.x, m.y, &card_rects[i])) {
                    selected_idx = i;
                    need_redraw = 1;
                    handled = 1;
                    break;
                }
            }
            // finish button
            if (!handled && point_in_ui_rect(m.x, m.y, &finish_rect)) {
                return -1;
            }
            // confirm button
            if (!handled && point_in_ui_rect(m.x, m.y, &confirm_rect)) {
                if (selected_idx >= 0) {
                    char chosen[128]; snprintf(chosen, sizeof(chosen), "Confirmed: %s", g_all_cards[selected_idx].name); draw_overlay_message_kind(chosen, UI_MSG_CONFIRM);
                    return selected_idx;
                } else {
                    draw_overlay_message_kind("No card selected", UI_MSG_ERROR);
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
    int layout_version = g_ui_layout_version;
    if (g_card_count == 0) { draw_line("Global card pool empty, returning 0"); if (out_count) *out_count = 0; return 0; }
    int max_rows = 8;
    int show = g_card_count < max_rows ? g_card_count : max_rows;
    UiRect card_rects[MAX_GLOBAL_CARDS] = {};
    UiRect minus_rects[MAX_GLOBAL_CARDS] = {};
    UiRect plus_rects[MAX_GLOBAL_CARDS] = {};
    UiRect finish_rect = {};
    int* qty = (int*)calloc(show, sizeof(int));
    if (!qty) { if (out_count) *out_count = 0; return 0; }
    int total = 0;
    int hover_card = -1;
    int hover_minus = -1;
    int hover_plus = -1;
    int hover_finish = 0;
    MOUSEMSG m;
    int need_redraw = 1;
    while (1) {
        if (need_redraw) {
            draw_deck_build_screen(player_id, total, max_select, qty, show, -1,
                                   hover_card, hover_minus, hover_plus, hover_finish, 0,
                                   card_rects, minus_rects, plus_rects, &finish_rect, NULL);
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
        int new_hover_card = -1;
        int new_hover_minus = -1;
        int new_hover_plus = -1;
        int new_hover_finish = 0;
        for (int i = 0; i < show; i++) {
            if (point_in_ui_rect(m.x, m.y, &card_rects[i])) new_hover_card = i;
            if (point_in_ui_rect(m.x, m.y, &minus_rects[i])) new_hover_minus = i;
            if (point_in_ui_rect(m.x, m.y, &plus_rects[i])) new_hover_plus = i;
        }
        if (point_in_ui_rect(m.x, m.y, &finish_rect)) new_hover_finish = 1;
        if (new_hover_card != hover_card || new_hover_minus != hover_minus || new_hover_plus != hover_plus || new_hover_finish != hover_finish) {
            hover_card = new_hover_card;
            hover_minus = new_hover_minus;
            hover_plus = new_hover_plus;
            hover_finish = new_hover_finish;
            need_redraw = 1;
        }
        if (m.uMsg == WM_LBUTTONDOWN) {
            int handled = 0;
            for (int i = 0; i < show; i++) {
                if (point_in_ui_rect(m.x, m.y, &minus_rects[i])) {
                    if (qty[i] > 0) { qty[i]--; total--; }
                    need_redraw = 1; handled = 1; break;
                }
                if (point_in_ui_rect(m.x, m.y, &plus_rects[i])) {
                    if (total < max_select) { qty[i]++; total++; }
                    else draw_overlay_message_kind("Cannot exceed deck limit", UI_MSG_ERROR);
                    need_redraw = 1; handled = 1; break;
                }
            }
            if (!handled && point_in_ui_rect(m.x, m.y, &finish_rect)) {
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
    int layout_version = g_ui_layout_version;
    int bx = g_main_x + 52, by = g_draw_y + 10, bw = 280, bh = 64;
    int confirm_x = bx, confirm_y = by + 200;
    int selected_mode = -1;
    int hover_choice = -1;
    int hover_confirm = 0;
    int need_redraw = 1;
    MOUSEMSG m;
    while (1) {
        if (need_redraw) {
            draw_main_menu_screen(selected_mode, hover_choice, hover_confirm);
            bx = g_main_x + 52; by = g_draw_y + 12; bw = 320; bh = 64;
            confirm_x = bx; confirm_y = by + 190;
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (layout_changed_since(&layout_version)) goto mode_menu;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto mode_menu;
        int new_hover_choice = -1;
        int new_hover_confirm = 0;
        if (point_in_rect(m.x, m.y, bx, by, bw, bh)) new_hover_choice = MODE_PVP;
        else if (point_in_rect(m.x, m.y, bx, by + 88, bw, bh)) new_hover_choice = MODE_PVE;
        else if (point_in_rect(m.x, m.y, confirm_x, confirm_y, 140, 42)) new_hover_confirm = 1;
        if (new_hover_choice != hover_choice || new_hover_confirm != hover_confirm) {
            hover_choice = new_hover_choice;
            hover_confirm = new_hover_confirm;
            need_redraw = 1;
        }
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) { selected_mode = MODE_PVP; need_redraw = 1; }
            else if (point_in_rect(m.x, m.y, bx, by + 88, bw, bh)) { selected_mode = MODE_PVE; need_redraw = 1; }
            else if (point_in_rect(m.x, m.y, confirm_x, confirm_y, 140, 42)) {
                if (selected_mode == MODE_PVP || selected_mode == MODE_PVE) {
                    char msg[64]; snprintf(msg, sizeof(msg), "Mode confirmed: %s", (selected_mode == MODE_PVE) ? "PvE" : "PvP");
                    draw_overlay_message_kind(msg, UI_MSG_CONFIRM);
                    return selected_mode;
                } else {
                    draw_overlay_message_kind("No mode selected", UI_MSG_ERROR);
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
