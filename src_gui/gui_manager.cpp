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
#define UI_CARD_H 128
#define UI_SKILL_CARD_H 148
#define UI_CHARACTER_CARD_H 148
#define UI_BUTTON_H 50
#define UI_FONT_FACE_UTF8 "Microsoft YaHei"

static int g_draw_y = 10;
void print_character(const Character* ch);

static IMAGE g_art_background;
static IMAGE g_art_main_panel;
static IMAGE g_art_status_panel;
static IMAGE g_art_side_panel;
static IMAGE g_art_records_panel;
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
static int g_left_side_x = 24;
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
static int g_return_to_menu_requested = 0;
static int g_menu_button_visible = 0;
static int g_menu_button_x = 0;
static int g_menu_button_y = 0;
static int g_menu_button_w = 0;
static int g_menu_button_h = 0;
static int g_last_canvas_w = 0;
static int g_last_canvas_h = 0;
static int g_ui_layout_version = 0;
static int g_defer_present = 0;
static HFONT g_ui_font = NULL;

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
    g_ui_margin = clamp_int(w / 82, 14, 28);
    g_status_h = clamp_int(h / 22, 40, 52);
    g_left_side_x = g_ui_margin;
    g_main_y = g_status_h + g_ui_margin;
    g_side_w = clamp_int(w / 5, 220, 330);
    int center_gap = clamp_int(w / 96, 10, 22);
    g_main_x = g_left_side_x + g_side_w + center_gap;
    g_side_x = w - g_side_w - g_ui_margin;
    g_main_w = g_side_x - g_main_x - center_gap;
    if (g_main_w < 360) {
        g_side_w = clamp_int((w - (g_ui_margin * 2) - 360 - (center_gap * 2)) / 2, 180, 220);
        g_main_x = g_left_side_x + g_side_w + center_gap;
        g_side_x = w - g_side_w - g_ui_margin;
        g_main_w = g_side_x - g_main_x - center_gap;
    }
    if (g_main_w < 300) g_main_w = 300;
    int content_bottom = h - g_ui_margin;
    g_main_h = content_bottom - g_main_y;
    if (g_main_h < 360) g_main_h = h - g_main_y - 58;
    g_team_panel_y = g_main_y;
    g_team_panel_h = clamp_int(content_bottom - g_team_panel_y, 248, 430);
    g_records_panel_y = g_team_panel_y + g_team_panel_h + g_ui_margin;
    g_records_panel_h = clamp_int(content_bottom - g_records_panel_y, 120, 220);
}

static int bottom_button_y() {
    return g_ui_h - g_ui_margin - UI_BUTTON_H;
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

static void draw_background_shell() {
    load_ui_assets();
    update_layout();
    draw_art_or_fill(&g_art_background, 0, 0, g_ui_w, g_ui_h, RGB(37, 46, 54));
    draw_art_or_fill(&g_art_main_panel, g_main_x, g_main_y, g_main_w, g_main_h, RGB(238, 231, 201));
    setlinecolor(RGB(88, 105, 119));
    rectangle(g_main_x, g_main_y, g_main_x + g_main_w, g_main_y + g_main_h);
}

static void reset_draw_y() {
    update_layout();
    g_draw_y = g_main_y + 14;
    setbkcolor(RGB(238, 231, 201));
    settextcolor(RGB(31, 37, 41));
    cleardevice();
    draw_background_shell();
}

static std::wstring utf8_to_wide_str(const char* utf8) {
    if (!utf8) return std::wstring();
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    UINT codepage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (wlen == 0) {
        codepage = CP_ACP;
        flags = 0;
        wlen = MultiByteToWideChar(codepage, flags, utf8, -1, NULL, 0);
    }
    if (wlen == 0) return std::wstring();
    wchar_t* wbuf = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    if (!wbuf) return std::wstring();
    MultiByteToWideChar(codepage, flags, utf8, -1, wbuf, wlen);
    std::wstring s(wbuf);
    free(wbuf);
    if (!s.empty() && s[0] == 0xFEFF) s.erase(0, 1);
    return s;
}

static int textwidth_wide(const std::wstring& text) {
    if (text.empty()) return 0;
    HDC hdc = GetImageHDC(NULL);
    SIZE size = { 0, 0 };
    SetBkMode(hdc, TRANSPARENT);
    GetTextExtentPoint32W(hdc, text.c_str(), (int)text.size(), &size);
    return size.cx;
}

static int scaled_font_height(int height) {
    int canvas_h = g_ui_h > 0 ? g_ui_h : 720;
    int scale_percent = 108;
    if (canvas_h > 720) {
        scale_percent += clamp_int(((canvas_h - 720) * 24) / 360, 0, 24);
    }
    int adjusted = (height * scale_percent + 50) / 100;
    return clamp_int(adjusted, height + 1, height + 8);
}

// Helper wrappers that accept UTF-8 literals or UTF-8 file text.
static void outtextxy_utf8(int x, int y, const char* utf8) {
    std::wstring s = utf8_to_wide_str(utf8);
    HDC hdc = GetImageHDC(NULL);
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x, y, s.c_str(), (int)s.size());
}
static void outtextxy_clipped_utf8(int x, int y, int max_w, const char* utf8) {
    if (max_w <= 0) return;
    std::wstring s = utf8_to_wide_str(utf8);
    if (textwidth_wide(s) > max_w) {
        const std::wstring suffix = L"...";
        while (!s.empty() && textwidth_wide(s + suffix) > max_w) {
            s.erase(s.size() - 1);
        }
        s += suffix;
    }
    HDC hdc = GetImageHDC(NULL);
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x, y, s.c_str(), (int)s.size());
}
static void outtextxy_centered_utf8(int x, int y, int w, int h, const char* utf8) {
    if (w <= 0 || h <= 0) return;
    std::wstring s = utf8_to_wide_str(utf8);
    int text_w = textwidth_wide(s);
    int draw_x = x + (w - text_w) / 2;
    if (draw_x < x + 2) draw_x = x + 2;
    HDC hdc = GetImageHDC(NULL);
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, draw_x, y + 2, s.c_str(), (int)s.size());
}
static void settextstyle_utf8(int height, int width, const char* utf8Name) {
    std::wstring face = utf8_to_wide_str((utf8Name && utf8Name[0]) ? utf8Name : UI_FONT_FACE_UTF8);
    if (face.empty()) face = L"Microsoft YaHei";
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = scaled_font_height(height);
    lf.lfWidth = width;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcsncpy(lf.lfFaceName, face.c_str(), LF_FACESIZE - 1);
    HFONT next_font = CreateFontIndirectW(&lf);
    if (!next_font) return;
    HDC hdc = GetImageHDC(NULL);
    SelectObject(hdc, next_font);
    if (g_ui_font) DeleteObject(g_ui_font);
    g_ui_font = next_font;
}
static void draw_line(const char* s) {
    outtextxy_clipped_utf8(g_main_x + 16, g_draw_y, g_main_w - 32, s);
    g_draw_y += clamp_int(scaled_font_height(18) + 8, 28, 36);
}
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
    settextstyle_utf8(22, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(28, 36, 42));
    outtextxy_clipped_utf8(x, y, g_ui_w - x - g_ui_margin, title);
    settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
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

static void draw_global_menu_button(int visible) {
    g_menu_button_visible = visible;
    if (!visible) {
        g_menu_button_x = g_menu_button_y = g_menu_button_w = g_menu_button_h = 0;
        return;
    }

    g_menu_button_w = clamp_int(g_ui_w / 10, 118, 150);
    g_menu_button_h = clamp_int(g_status_h - 12, 30, 42);
    g_menu_button_x = g_ui_w - g_ui_margin - g_menu_button_w;
    g_menu_button_y = (g_status_h - g_menu_button_h) / 2;

    setfillcolor(RGB(74, 94, 124));
    fillrectangle(g_menu_button_x, g_menu_button_y,
                  g_menu_button_x + g_menu_button_w,
                  g_menu_button_y + g_menu_button_h);
    setlinecolor(RGB(36, 54, 78));
    rectangle(g_menu_button_x, g_menu_button_y,
              g_menu_button_x + g_menu_button_w,
              g_menu_button_y + g_menu_button_h);
    settextstyle_utf8(18, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(255, 250, 230));
    outtextxy_centered_utf8(g_menu_button_x, g_menu_button_y + 3,
                            g_menu_button_w, g_menu_button_h - 6,
                            "Main Menu");
    settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(31, 37, 41));
}

static void draw_status_bar(const GameState* st, int acting_player_id, const char* phase) {
    if (!st) return;
    update_layout();
    char buf[256];
    int show_menu_button = 1;
    int menu_w = clamp_int(g_ui_w / 10, 118, 150);
    int menu_x = g_ui_w - g_ui_margin - menu_w;
    int hotkey_w = clamp_int(g_ui_w / 5, 170, 240);
    int hotkey_x = show_menu_button ? (menu_x - hotkey_w - 10) : (g_ui_w - g_ui_margin - hotkey_w);
    int round_w = 130;
    int current_id = acting_player_id > 0 ? acting_player_id : st->current_turn;
    int banner_w = clamp_int(g_ui_w / 4, 250, 380);
    int banner_x = (g_ui_w - banner_w) / 2;
    int phase_x = banner_x + banner_w + 12;
    int phase_w = hotkey_x - phase_x - 8;
    draw_art_or_fill(&g_art_status_panel, 0, 0, g_ui_w, g_status_h, RGB(238, 244, 252));
    setlinecolor(RGB(70, 95, 130));
    rectangle(0, 0, g_ui_w - 1, g_status_h);
    settextcolor(RGB(26, 36, 45));
    snprintf(buf, sizeof(buf), "Round %d", st->round_count);
    outtextxy_clipped_utf8(g_ui_margin, 12, round_w - 8, buf);
    setfillcolor(current_id == 2 ? RGB(224, 103, 82) : RGB(71, 135, 200));
    fillrectangle(banner_x, 6, banner_x + banner_w, g_status_h - 6);
    setlinecolor(current_id == 2 ? RGB(146, 57, 45) : RGB(44, 89, 142));
    rectangle(banner_x, 6, banner_x + banner_w, g_status_h - 6);
    settextcolor(RGB(255, 250, 230));
    snprintf(buf, sizeof(buf), "Current Player: Player %d", current_id);
    outtextxy_clipped_utf8(banner_x + 14, 12, banner_w - 28, buf);
    settextcolor(RGB(26, 36, 45));
    if (phase && phase[0] != '\0') {
        snprintf(buf, sizeof(buf), "Phase: %s", phase);
        outtextxy_clipped_utf8(phase_x, 12, phase_w, buf);
    }
    outtextxy_clipped_utf8(hotkey_x, 12, hotkey_w, "Esc: Quit  F11: Toggle");
    draw_global_menu_button(show_menu_button);
    g_draw_y = g_main_y + 14;
}

static void draw_simple_status_bar(const char* title) {
    update_layout();
    int show_menu_button = !(title && strcmp(title, "Main Menu") == 0);
    int menu_w = clamp_int(g_ui_w / 10, 118, 150);
    int menu_x = g_ui_w - g_ui_margin - menu_w;
    int hotkey_w = clamp_int(g_ui_w / 5, 170, 240);
    int hotkey_x = show_menu_button ? (menu_x - hotkey_w - 10) : (g_ui_w - g_ui_margin - hotkey_w);
    draw_art_or_fill(&g_art_status_panel, 0, 0, g_ui_w, g_status_h, RGB(238, 244, 252));
    setlinecolor(RGB(70, 95, 130));
    rectangle(0, 0, g_ui_w - 1, g_status_h);
    settextcolor(RGB(26, 36, 45));
    outtextxy_clipped_utf8(g_ui_margin, 12, hotkey_x - g_ui_margin - 8, title ? title : "Spire of Roco");
    outtextxy_clipped_utf8(hotkey_x, 12, hotkey_w, "Esc: Quit  F11: Toggle");
    draw_global_menu_button(show_menu_button);
    g_draw_y = g_main_y + 14;
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
    if (g_menu_button_visible &&
        out_msg->uMsg == WM_LBUTTONDOWN &&
        point_in_rect(out_msg->x, out_msg->y,
                      g_menu_button_x, g_menu_button_y,
                      g_menu_button_w, g_menu_button_h)) {
        g_return_to_menu_requested = 1;
        return 0;
    }
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
    int label_font = clamp_int((h * 42) / 100, 18, 24);
    int label_h = scaled_font_height(label_font);
    settextstyle_utf8(label_font, 0, UI_FONT_FACE_UTF8);
    outtextxy_clipped_utf8(x + (selected ? 30 : 12), y + (h - label_h) / 2, w - (selected ? 42 : 24), label);
    settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(31, 37, 41));
    present_frame();
}

static void draw_button(int x, int y, int w, int h, const char* label) {
    draw_button_state(x, y, w, h, label, 0, 0, 0);
}

static void draw_quantity_control(int x, int y, int w, int h, int quantity) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", quantity);
    setfillcolor(quantity > 0 ? RGB(38, 68, 91) : RGB(246, 242, 222));
    fillrectangle(x, y, x + w, y + h);
    setlinecolor(quantity > 0 ? RGB(18, 43, 61) : RGB(96, 83, 55));
    rectangle(x, y, x + w, y + h);
    settextstyle_utf8(22, 0, UI_FONT_FACE_UTF8);
    settextcolor(quantity > 0 ? RGB(255, 250, 228) : RGB(31, 37, 41));
    int text_h = scaled_font_height(22);
    outtextxy_centered_utf8(x, y + (h - text_h) / 2, w, h, buf);
    settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(31, 37, 41));
    present_frame();
}

static void draw_overlay_message_kind(const char* utf8msg, UiMessageKind kind) {
    (void)utf8msg;
    (void)kind;
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

static void DrawCardFrame(int x, int y, int w, int h, COLORREF fill, COLORREF border, int selected, int hover, int disabled) {
    COLORREF outer = disabled ? RGB(118, 118, 113) : (selected ? RGB(46, 132, 87) : (hover ? RGB(71, 105, 151) : border));
    setfillcolor(fill);
    fillrectangle(x, y, x + w, y + h);
    setlinecolor(outer);
    rectangle(x, y, x + w, y + h);
    if (selected || hover) {
        setlinecolor(selected ? RGB(53, 172, 111) : RGB(105, 142, 186));
        rectangle(x + 2, y + 2, x + w - 2, y + h - 2);
    }
    if (disabled) {
        setlinecolor(RGB(142, 142, 136));
        line(x + 7, y + 7, x + w - 7, y + h - 7);
        line(x + w - 7, y + 7, x + 7, y + h - 7);
    }
}

static void DrawElementIcon(ElementType element, int cx, int cy, int size, int disabled) {
    COLORREF color = disabled ? RGB(135, 135, 130) : element_color(element);
    setfillcolor(color);
    setlinecolor(RGB(64, 58, 48));
    switch (element) {
        case ELEMENT_WATER:
            fillellipse(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2);
            line(cx, cy - size / 2, cx + size / 3, cy + size / 2);
            line(cx, cy - size / 2, cx - size / 3, cy + size / 2);
            break;
        case ELEMENT_FIRE: {
            POINT fire_pts[3] = { { cx, cy - size / 2 }, { cx + size / 2, cy + size / 2 }, { cx - size / 2, cy + size / 2 } };
            fillpolygon(fire_pts, 3);
            break;
        }
        case ELEMENT_GRASS:
            fillellipse(cx - size / 2, cy - size / 3, cx + size / 4, cy + size / 3);
            fillellipse(cx - size / 4, cy - size / 3, cx + size / 2, cy + size / 3);
            break;
        case ELEMENT_ELECTRIC: {
            POINT bolt[6] = {
                { cx - size / 5, cy - size / 2 }, { cx + size / 3, cy - size / 2 },
                { cx + size / 8, cy - size / 10 }, { cx + size / 2, cy - size / 10 },
                { cx - size / 4, cy + size / 2 }, { cx, cy + size / 10 }
            };
            fillpolygon(bolt, 6);
            break;
        }
        case ELEMENT_NORMAL:
        default:
            fillrectangle(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2);
            break;
    }
}

static void DrawPlaceholderArt(int x, int y, int w, int h, ElementType element, int disabled) {
    COLORREF base = disabled ? RGB(176, 176, 170) : element_color(element);
    COLORREF bg = disabled ? RGB(198, 198, 192) : RGB(220, 229, 220);
    setfillcolor(bg);
    fillrectangle(x, y, x + w, y + h);
    setlinecolor(RGB(101, 94, 79));
    rectangle(x, y, x + w, y + h);
    setfillcolor(base);
    fillellipse(x + w / 5, y + h / 5, x + (w * 3) / 5, y + (h * 3) / 5);
    setfillcolor(disabled ? RGB(150, 150, 145) : RGB(246, 226, 138));
    fillrectangle(x + (w * 2) / 5, y + h / 2, x + (w * 4) / 5, y + (h * 4) / 5);
    DrawElementIcon(element, x + w / 2, y + h / 2, clamp_int((w < h ? w : h) / 3, 18, 34), disabled);
}

static int team_slot_h() {
    int header_h = 82;
    int available = g_team_panel_h - header_h - (UI_PAD * 2) - ((TEAM_SIZE - 1) * UI_GAP);
    return clamp_int(available / TEAM_SIZE, 78, 108);
}

static int team_panel_x_for_player(int player_id) {
    return (player_id == 1) ? g_left_side_x : g_side_x;
}

static void get_team_slot_rect(int player_id, int slot, UiRect* rect) {
    update_layout();
    int slot_h = team_slot_h();
    int panel_x = team_panel_x_for_player(player_id);
    int x = panel_x + UI_PAD;
    int y = g_team_panel_y + UI_PAD + 76 + slot * (slot_h + UI_GAP);
    set_ui_rect(rect, x, y, g_side_w - (UI_PAD * 2), slot_h);
}

static int hit_team_slot(int mouse_x, int mouse_y, int player_id, UiRect* out_rect) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        UiRect rect;
        get_team_slot_rect(player_id, i, &rect);
        if (point_in_ui_rect(mouse_x, mouse_y, &rect)) {
            if (out_rect) *out_rect = rect;
            return i;
        }
    }
    return -1;
}

static void draw_character_slot(const Character* ch, int slot_idx, int x, int y, int w, int h,
                                int active, int hover, int disabled, int mirror) {
    if (!ch) return;
    char buf[160];
    int alive = ch->is_alive && !disabled;
    COLORREF fill = alive ? (active ? RGB(237, 248, 233) : RGB(238, 233, 205)) : RGB(204, 204, 198);
    COLORREF border = active ? RGB(42, 145, 92) : RGB(113, 101, 78);
    DrawCardFrame(x, y, w, h, fill, border, active, hover, !alive);
    setfillcolor(alive ? element_color(ch->element) : RGB(135, 135, 130));
    fillrectangle(x + 1, y + 1, x + w - 1, y + 22);
    settextstyle_utf8(16, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(255, 250, 230));
    snprintf(buf, sizeof(buf), "%d  %s", slot_idx + 1, ch->name);
    outtextxy_clipped_utf8(x + 8, y + 4, w - 16, buf);

    int art_size = clamp_int(h - 42, 34, 48);
    int art_x = mirror ? x + w - art_size - 10 : x + 10;
    int art_y = y + 30;
    DrawPlaceholderArt(art_x, art_y, art_size, art_size, ch->element, !alive);
    int text_x = mirror ? x + 10 : art_x + art_size + 10;
    int text_w = w - art_size - 30;
    int meter_y = y + h - 18;
    int compact = h < 96;
    if (active) {
        setfillcolor(RGB(46, 132, 87));
        fillrectangle(mirror ? x + 8 : x + w - 70, y + 27, mirror ? x + 70 : x + w - 8, y + 45);
        settextcolor(RGB(255, 250, 230));
        outtextxy_clipped_utf8(mirror ? x + 14 : x + w - 64, y + 28, 54, "ACTIVE");
    }
    if (!alive) {
        settextcolor(RGB(128, 45, 42));
        outtextxy_clipped_utf8(text_x, y + 28, text_w, "DEFEATED");
    }
    settextstyle_utf8(16, 0, UI_FONT_FACE_UTF8);
    settextcolor(alive ? RGB(74, 67, 55) : RGB(104, 104, 100));
    snprintf(buf, sizeof(buf), "%s  SPD %d", element_label(ch->element), ch->speed);
    outtextxy_clipped_utf8(text_x, y + (compact ? 34 : 50), text_w, buf);
    snprintf(buf, sizeof(buf), "HP %d/%d", ch->hp, ch->max_hp);
    outtextxy_clipped_utf8(text_x, y + (compact ? 52 : 70), text_w, buf);
    draw_meter_colored(x + 10, meter_y, w - 20, ch->hp, ch->max_hp, hp_state_color(ch->hp, ch->max_hp));
    settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
}

static void draw_player_team_panel(const Player* player, int player_id, int panel_x, const char* fallback_title,
                                   COLORREF accent, int hover_slot, int mirror) {
    update_layout();
    char buf[128];
    int x = panel_x + UI_PAD;
    int y = g_team_panel_y + UI_PAD;
    draw_art_or_fill(&g_art_side_panel, panel_x, g_team_panel_y, g_side_w, g_team_panel_h, RGB(242, 239, 220));
    setlinecolor(RGB(62, 76, 92));
    rectangle(panel_x, g_team_panel_y, panel_x + g_side_w, g_team_panel_y + g_team_panel_h);
    setfillcolor(accent);
    fillrectangle(mirror ? panel_x + g_side_w - 8 : panel_x, g_team_panel_y, mirror ? panel_x + g_side_w : panel_x + 8, g_team_panel_y + g_team_panel_h);
    if (!player) {
        draw_section_title(x, y, fallback_title);
        settextcolor(RGB(93, 89, 79));
        outtextxy_clipped_utf8(x, y + 36, g_side_w - (UI_PAD * 2), "Team status appears here.");
        return;
    }
    settextcolor(RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "Player %d  %s", player_id, player->name);
    draw_section_title(x, y, buf);
    y += 30;
    snprintf(buf, sizeof(buf), "Energy %d/%d  Hand %d", player->energy, player->max_energy, player->hand_count);
    outtextxy_clipped_utf8(x, y, g_side_w - (UI_PAD * 2), buf);
    draw_meter_colored(x, y + 24, g_side_w - (UI_PAD * 2), player->energy, player->max_energy, RGB(68, 145, 206));
    for (int i = 0; i < TEAM_SIZE; i++) {
        UiRect rect;
        get_team_slot_rect(player_id, i, &rect);
        draw_character_slot(&player->team[i], i, rect.x, rect.y, rect.w, rect.h,
                            i == player->active_idx, hover_slot == i, !player->team[i].is_alive, mirror);
    }
}

// Render each player's team on its own side of the battlefield.
static void draw_team_hp_panel(const GameState* st, int hover_player_id = 0, int hover_slot = -1) {
    if (!st) {
        draw_player_team_panel(NULL, 1, g_left_side_x, "Player 1 Team", RGB(73, 126, 180), -1, 0);
        draw_player_team_panel(NULL, 2, g_side_x, "Player 2 Team", RGB(176, 93, 77), -1, 1);
        return;
    }
    draw_player_team_panel(&st->p1, 1, g_left_side_x, "Player 1 Team", RGB(73, 126, 180),
                           hover_player_id == 1 ? hover_slot : -1, 0);
    draw_player_team_panel(&st->p2, 2, g_side_x, "Player 2 Team", RGB(176, 93, 77),
                           hover_player_id == 2 ? hover_slot : -1, 1);
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
    int compact = h < 112 || w < 210;
    COLORREF elem = disabled ? RGB(128, 128, 128) : element_color(c->element);
    COLORREF fill = disabled ? RGB(210, 211, 205) : (selected ? RGB(240, 248, 231) : (hover ? RGB(248, 240, 218) : RGB(234, 228, 200)));
    COLORREF border = disabled ? RGB(126, 126, 120) : (selected ? RGB(52, 150, 99) : (hover ? RGB(80, 113, 154) : RGB(85, 74, 56)));
    DrawCardFrame(x, y, w, h, fill, border, selected, hover, disabled);
    setfillcolor(elem);
    fillrectangle(x + 1, y + 1, x + w - 1, y + 28);
    settextstyle_utf8(compact ? 16 : 18, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(255, 250, 230));
    snprintf(buf, sizeof(buf), "[%d] %s", idx, c->name);
    outtextxy_clipped_utf8(x + 10, y + 6, w - 56, buf);

    setfillcolor(disabled ? RGB(150, 150, 145) : RGB(244, 226, 134));
    fillrectangle(x + w - 42, y + 5, x + w - 10, y + 25);
    setlinecolor(RGB(93, 78, 38));
    rectangle(x + w - 42, y + 5, x + w - 10, y + 25);
    snprintf(buf, sizeof(buf), "%d", c->energy_cost);
    settextcolor(RGB(47, 39, 18));
    outtextxy_utf8(x + w - 31, y + 7, buf);

    int art_w = compact ? clamp_int(w / 4, 46, 62) : clamp_int(w / 3, 64, 92);
    int art_h = h - 48;
    if (art_h < 40) art_h = 40;
    DrawPlaceholderArt(x + 10, y + 38, art_w, art_h, c->element, disabled);
    int text_x = x + art_w + 20;
    int text_w = w - art_w - 30;
    settextstyle_utf8(compact ? 15 : 16, 0, UI_FONT_FACE_UTF8);
    settextcolor(disabled ? RGB(104, 104, 100) : RGB(74, 67, 55));
    snprintf(buf, sizeof(buf), "%s / %s", element_label(c->element), card_type_label(c->type));
    outtextxy_clipped_utf8(text_x, y + 38, text_w, buf);
    build_card_stats_text(c, buf, sizeof(buf));
    outtextxy_clipped_utf8(text_x, y + 58, text_w, buf);
    snprintf(buf, sizeof(buf), "Cost %d", c->energy_cost);
    outtextxy_clipped_utf8(text_x, y + 78, text_w, buf);
    if (quantity > 0) {
        snprintf(buf, sizeof(buf), "x%d", quantity);
        setfillcolor(RGB(50, 68, 86));
        fillrectangle(x + w - 47, y + h - 27, x + w - 12, y + h - 8);
        settextcolor(RGB(246, 242, 222));
        outtextxy_utf8(x + w - 40, y + h - 25, buf);
    }
    settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(31, 37, 41));
    present_frame();
}

static void draw_character_option_panel(const Character* ch, int idx, int x, int y, int w, int h, int selected, int hover, int disabled) {
    if (!ch) return;
    char buf[160];
    int compact = h < 112;
    COLORREF fill = disabled ? RGB(207, 207, 201) : (selected ? RGB(237, 247, 230) : (hover ? RGB(248, 240, 218) : RGB(234, 228, 200)));
    COLORREF border = disabled ? RGB(124, 124, 119) : (selected ? RGB(52, 150, 99) : (hover ? RGB(80, 113, 154) : RGB(85, 74, 56)));
    DrawCardFrame(x, y, w, h, fill, border, selected, hover, disabled);
    setfillcolor(disabled ? RGB(128, 128, 128) : element_color(ch->element));
    fillrectangle(x + 1, y + 1, x + w - 1, y + 28);
    settextstyle_utf8(compact ? 16 : 18, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(255, 250, 230));
    snprintf(buf, sizeof(buf), "[%d] %s", idx, ch->name);
    outtextxy_clipped_utf8(x + 10, y + 6, w - 20, buf);
    int art_w = compact ? clamp_int(w / 4, 42, 58) : clamp_int(w / 3, 64, 96);
    int art_h = compact ? clamp_int(h - 50, 28, 44) : h - 52;
    DrawPlaceholderArt(x + 10, y + 38, art_w, art_h, ch->element, disabled);
    int text_x = x + art_w + 20;
    int text_w = w - art_w - 30;
    settextstyle_utf8(compact ? 15 : 16, 0, UI_FONT_FACE_UTF8);
    settextcolor(disabled ? RGB(92, 92, 88) : RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "%s  Speed %d", element_label(ch->element), ch->speed);
    outtextxy_clipped_utf8(text_x, y + (compact ? 36 : 40), text_w, buf);
    snprintf(buf, sizeof(buf), "HP %d/%d%s", ch->hp, ch->max_hp, ch->is_alive ? "" : "  Defeated");
    settextcolor(disabled ? RGB(104, 104, 100) : RGB(74, 67, 55));
    outtextxy_clipped_utf8(text_x, y + (compact ? h - 38 : 62), text_w, buf);
    draw_meter_colored(text_x, y + h - 20, text_w, ch->hp, ch->max_hp, hp_state_color(ch->hp, ch->max_hp));
    present_frame();
}

static void DrawCharacterCard(const Character* ch, int idx, int x, int y, int w, int h, int selected, int hover, int disabled) {
    draw_character_option_panel(ch, idx, x, y, w, h, selected, hover, disabled);
}

static void DrawSkillCard(const Card* c, int idx, int x, int y, int w, int h, int selected, int hover, int disabled, int quantity) {
    draw_card_panel(c, idx, x, y, w, h, selected, hover, disabled, quantity);
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
        card_h = clamp_int(card_h, 104, UI_CARD_H);
    }
    int card_w = (columns == 2) ? ((available_w - UI_GAP) / 2) : clamp_int(available_w, 280, 680);
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

static int compute_card_grid_rects(int count, int start_y, int footer_y, int preferred_h, int* xs, int* ys, int* ws, int* hs) {
    int available_w = g_main_w - 48;
    int available_h = footer_y - start_y - UI_GAP;
    int columns = available_w >= 620 ? 2 : 1;
    int rows = (count + columns - 1) / columns;
    int card_w = (available_w - ((columns - 1) * UI_GAP)) / columns;
    int card_h = preferred_h;
    if (rows > 0) {
        card_h = (available_h - ((rows - 1) * UI_GAP)) / rows;
        card_h = clamp_int(card_h, 104, preferred_h);
    }
    int origin_x = g_main_x + 24;
    for (int i = 0; i < count; i++) {
        int col = i % columns;
        int row = i / columns;
        xs[i] = origin_x + col * (card_w + UI_GAP);
        ys[i] = start_y + row * (card_h + UI_GAP);
        ws[i] = card_w;
        hs[i] = card_h;
    }
    return columns;
}

static int visible_card_rows_for_current_layout() {
    int start_y = g_main_y + 112;
    int available_h = bottom_button_y() - start_y - UI_GAP;
    return clamp_int((available_h / (UI_CARD_H + UI_GAP)) * 2, 3, 8);
}

static void draw_center_battlefield_frame(const char* title, UiRect* out_rect) {
    int x = g_main_x + UI_PAD;
    int y = g_main_y + UI_PAD;
    int w = g_main_w - (UI_PAD * 2);
    int h = clamp_int(g_main_h / 2, 190, 300);
    set_ui_rect(out_rect, x, y, w, h);
    draw_soft_panel(x, y, w, h, RGB(231, 237, 224), RGB(93, 112, 92));
    setlinecolor(RGB(141, 156, 126));
    line(x + w / 2, y + 18, x + w / 2, y + h - 18);
    setlinecolor(RGB(176, 187, 160));
    rectangle(x + 12, y + 34, x + (w / 2) - 12, y + h - 18);
    rectangle(x + (w / 2) + 12, y + 34, x + w - 12, y + h - 18);
    settextcolor(RGB(74, 86, 70));
    outtextxy_clipped_utf8(x + 14, y + 10, w - 28, title ? title : "Battlefield");
}

static Player* mutable_player(GameState* st, int player_id) {
    return (player_id == st->p1.player_id) ? &st->p1 : &st->p2;
}

static void draw_action_records_panel(const ActionRecord* records, int record_count, int player_id) {
    update_layout();
    int w = clamp_int(g_main_w / 3, 220, 360);
    int h = clamp_int(g_main_h / 4, 112, 170);
    int x = g_main_x + g_main_w - w - UI_PAD;
    int y = g_main_y + UI_PAD;
    char buf[256];
    int shown = 0;
    draw_art_or_fill(&g_art_records_panel, x, y, w, h, RGB(230, 234, 230));
    setlinecolor(RGB(62, 76, 92));
    rectangle(x, y, x + w, y + h);
    settextcolor(RGB(27, 33, 35));
    snprintf(buf, sizeof(buf), "P%d Card Records", player_id);
    outtextxy_clipped_utf8(x + 8, y + 8, w - 16, buf);
    int line_y = y + 36;
    int max_lines = (h - 44) / 22;
    for (int i = 0; i < record_count && shown < max_lines; i++) {
        if (records[i].action.type != ACTION_PLAY_CARD) continue;
        snprintf(buf, sizeof(buf), "%02d. %s", shown + 1, records[i].summary);
        outtextxy_clipped_utf8(x + 8, line_y, w - 16, buf);
        line_y += 22;
        shown++;
    }
    if (shown == 0) {
        outtextxy_clipped_utf8(x + 8, y + 36, w - 16, "No card records");
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
    int record_h = clamp_int(g_main_h / 4, 112, 170);
    g_draw_y = g_main_y + UI_PAD + record_h + UI_GAP;
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
                                    int hover_action, int hover_switch_slot, int can_edit) {
    begin_deferred_present();
    draw_planning_shell(st, player_id, records, record_count, title);
    draw_team_hp_panel(st, player_id, hover_switch_slot);
    int bx = g_main_x + 24, by = g_draw_y + 10, bw = 240, bh = UI_BUTTON_H;
    draw_button_state(bx, by, bw, bh, "End Turn", 0, hover_action == 0, 0);
    draw_button_state(bx, by + 55, bw, bh, "Play Card", 0, hover_action == 1, 0);
    draw_button_state(bx, by + 110, bw, bh, can_edit ? "Edit Card" : "Edit Action", 0, hover_action == 3, !can_edit);
    end_deferred_present();
}

static void draw_target_select_screen(GameState* st, int player_id, const ActionRecord* records, int record_count,
                                      const Card* card, int target_player_id, UiRect* target_rects, int hover_target) {
    char buf[256];
    begin_deferred_present();
    draw_planning_shell(st, player_id, records, record_count, "Choose a target");
    draw_team_hp_panel(st, target_player_id, hover_target);
    snprintf(buf, sizeof(buf), "Card: %s", card ? card->name : "");
    draw_soft_panel(g_main_x + UI_PAD, g_draw_y + 4, g_main_w - (UI_PAD * 2), 46, RGB(238, 232, 204), RGB(85, 74, 56));
    settextcolor(RGB(27, 33, 35));
    outtextxy_clipped_utf8(g_main_x + UI_PAD + 12, g_draw_y + 17, g_main_w - (UI_PAD * 2) - 24, buf);
    for (int t = 0; t < TEAM_SIZE; t++) {
        get_team_slot_rect(target_player_id, t, target_rects ? &target_rects[t] : NULL);
    }
    draw_button_state(g_main_x + 16, bottom_button_y(), 120, 38, "Back", 0, hover_target == 10, 0);
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
    g_draw_y += 40;
    snprintf(buf, sizeof(buf), "Selected %d/%d", selected_count, max_select);
    int count_x = g_main_x + UI_PAD + 4;
    int count_y = g_draw_y;
    int count_w = clamp_int(g_main_w / 4, 180, 260);
    draw_soft_panel(count_x, count_y, count_w, 36, RGB(245, 238, 209), RGB(88, 76, 52));
    settextstyle_utf8(22, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(31, 37, 41));
    outtextxy_centered_utf8(count_x, count_y + 4, count_w, 28, buf);
    settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
    g_draw_y += 48;
    int cx = g_main_x + 24;
    int cy = g_draw_y + 8;
    int card_x[MAX_GLOBAL_CARDS] = {0};
    int card_y[MAX_GLOBAL_CARDS] = {0};
    int card_w_arr[MAX_GLOBAL_CARDS] = {0};
    int card_h_arr[MAX_GLOBAL_CARDS] = {0};
    int footer_y = bottom_button_y();
    compute_card_grid_rects(show, cy, footer_y - UI_GAP, UI_SKILL_CARD_H, card_x, card_y, card_w_arr, card_h_arr);
    for (int i = 0; i < show; i++) {
        int x = card_x[i];
        int y = card_y[i];
        int card_w = card_w_arr[i];
        int ch = card_h_arr[i];
        set_ui_rect(card_rects ? &card_rects[i] : NULL, x, y, card_w, ch);
        DrawSkillCard(&g_all_cards[i], i, x, y, card_w, ch, i == selected_idx || (qty && qty[i] > 0), hover_card == i, 0, qty ? qty[i] : 0);
        if (qty) {
            int x_plus = x + card_w - 48;
            int x_qty = x_plus - 52;
            int x_minus = x_qty - 44;
            int control_y = y + ch - 42;
            set_ui_rect(minus_rects ? &minus_rects[i] : NULL, x_minus, control_y, 36, 32);
            set_ui_rect(plus_rects ? &plus_rects[i] : NULL, x_plus, control_y, 36, 32);
            draw_button_state(x_minus, control_y, 36, 32, "-", 0, hover_minus == i, qty[i] <= 0);
            draw_quantity_control(x_qty, control_y, 44, 32, qty[i]);
            draw_button_state(x_plus, control_y, 36, 32, "+", 0, hover_plus == i, selected_count >= max_select);
        }
    }
    set_ui_rect(finish_rect, cx, footer_y, 150, 40);
    set_ui_rect(confirm_rect, cx + 162, footer_y, 130, 40);
    draw_button_state(cx, footer_y, 150, 40, qty ? "End Building" : "Finish Build", 0, hover_finish, 0);
    if (!qty) draw_button_state(cx + 162, footer_y, 130, 40, "Confirm", selected_idx >= 0, hover_confirm, 0);
    end_deferred_present();
}

static void draw_menu_button(int x, int y, int w, int h, const char* label, int hover) {
    draw_button_state(x, y, w, h, label, 0, hover, 0);
}

static void draw_main_menu_screen(MenuSelection hover_choice) {
    begin_deferred_present();
    reset_draw_y();
    draw_simple_status_bar("Main Menu");
    int menu_w = clamp_int(g_main_w / 2, 340, 460);
    int bx = g_main_x + (g_main_w - menu_w) / 2;
    int by = g_main_y + clamp_int(g_main_h / 5, 70, 130);
    int bh = 58;
    settextstyle_utf8(34, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(26, 36, 45));
    outtextxy_clipped_utf8(g_main_x + UI_PAD, by - 82, g_main_w - (UI_PAD * 2), "Spire of Roco");
    settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
    draw_menu_button(bx, by, menu_w, bh, "PVP Mode", hover_choice == MENU_PVP);
    draw_menu_button(bx, by + 76, menu_w, bh, "PVE Mode", hover_choice == MENU_PVE);
    draw_menu_button(bx, by + 152, menu_w, bh, "Credits", hover_choice == MENU_CREDITS);
    end_deferred_present();
}

static const char* ai_policy_label(AiPolicy policy) {
    switch (policy) {
        case AI_POLICY_RANDOM: return "Random";
        case AI_POLICY_HARD: return "Hard";
        case AI_POLICY_LLM: return "LLM";
        case AI_POLICY_HEURISTIC:
        default: return "Heuristic";
    }
}

static void draw_ai_policy_menu_screen(AiPolicy hover_policy, int hover_back) {
    begin_deferred_present();
    reset_draw_y();
    draw_simple_status_bar("AI Difficulty");
    int menu_w = clamp_int(g_main_w / 2, 340, 500);
    int bx = g_main_x + (g_main_w - menu_w) / 2;
    int by = g_main_y + clamp_int(g_main_h / 7, 48, 96);
    int bh = 54;
    int gap = 68;
    settextstyle_utf8(32, 0, UI_FONT_FACE_UTF8);
    settextcolor(RGB(26, 36, 45));
    outtextxy_clipped_utf8(g_main_x + UI_PAD, by - 62, g_main_w - (UI_PAD * 2), "Choose AI Difficulty");
    settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
    draw_menu_button(bx, by, menu_w, bh, ai_policy_label(AI_POLICY_HEURISTIC), hover_policy == AI_POLICY_HEURISTIC);
    draw_menu_button(bx, by + gap, menu_w, bh, ai_policy_label(AI_POLICY_RANDOM), hover_policy == AI_POLICY_RANDOM);
    draw_menu_button(bx, by + gap * 2, menu_w, bh, ai_policy_label(AI_POLICY_HARD), hover_policy == AI_POLICY_HARD);
    draw_menu_button(bx, by + gap * 3, menu_w, bh, ai_policy_label(AI_POLICY_LLM), hover_policy == AI_POLICY_LLM);
    draw_button_state(g_main_x + 24, bottom_button_y(), 140, 40, "Back", 0, hover_back, 0);
    end_deferred_present();
}

static std::string read_text_file_utf8(const char* path) {
    if (!path) return std::string();
    FILE* fp = fopen(path, "rb");
    if (!fp) return std::string();
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0 || size > 65536) {
        fclose(fp);
        return std::string();
    }
    std::string text;
    text.resize((size_t)size);
    if (size > 0) fread(&text[0], 1, (size_t)size, fp);
    fclose(fp);
    return text;
}

static void draw_credits_screen(const char* credits_text, int hover_back) {
    begin_deferred_present();
    reset_draw_y();
    draw_simple_status_bar("Credits");
    draw_section_title(g_main_x + UI_PAD + 4, g_draw_y, "Credits");
    int text_x = g_main_x + 28;
    int text_y = g_draw_y + 44;
    int text_w = g_main_w - 56;
    int line_h = 24;
    int max_lines = (bottom_button_y() - text_y - UI_GAP) / line_h;
    const char* fallback = "Credits file not found. Please edit docs/credits.txt.";
    const char* src = (credits_text && credits_text[0] != '\0') ? credits_text : fallback;
    char line[256];
    int line_len = 0;
    int shown = 0;
    settextcolor(RGB(38, 44, 48));
    for (const char* p = src; ; p++) {
        char ch = *p;
        if (ch == '\r') continue;
        if (ch == '\n' || ch == '\0') {
            line[line_len] = '\0';
            if (shown < max_lines) {
                outtextxy_clipped_utf8(text_x, text_y + shown * line_h, text_w, line);
                shown++;
            }
            line_len = 0;
            if (ch == '\0') break;
            continue;
        }
        if (line_len < (int)sizeof(line) - 1) line[line_len++] = ch;
    }
    draw_button_state(g_main_x + 24, bottom_button_y(), 140, 40, "Back", 0, hover_back, 0);
    end_deferred_present();
}

#endif
#else
// no graphical helpers
#endif

// 绠€鏄撳熀浜庢帶鍒跺彴鐨?GUI 瀹炵幇锛屼究浜庡湪娌℃湁鍥惧舰搴撴椂浜や簰鍜岃皟璇曘€?

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
    settextstyle_utf8(20,0,UI_FONT_FACE_UTF8);
    draw_background_shell();
    outtextxy_utf8(20, 20, "Spire of Roco loading...");
    present_frame();
#else
    // 鎺у埗鍙颁笉闇€瑕佺壒鍒垵濮嬪寲
#endif
}

void CloseGUI() {
#ifdef USE_EASYX
    EndBatchDraw();
    if (g_ui_font) {
        DeleteObject(g_ui_font);
        g_ui_font = NULL;
    }
    closegraph();
#else
    // 鎺у埗鍙颁笉闇€瑕佺壒鍒噴鏀?
#endif
}

void ClearReturnToMenuRequest(void) {
#ifdef USE_EASYX
    g_return_to_menu_requested = 0;
    g_menu_button_visible = 0;
#endif
}

int IsReturnToMenuRequested(void) {
#ifdef USE_EASYX
    return g_return_to_menu_requested;
#else
    return 0;
#endif
}

void print_character(const Character* ch) {
    if (!ch) return;
#ifdef USE_EASYX
    int y = g_draw_y;
    int x = g_main_x + 16;
    int w = clamp_int(g_main_w - 64, 360, 620);
    DrawCharacterCard(ch, ch->char_id, x, y, w, UI_CHARACTER_CARD_H, 0, 0, !ch->is_alive);
    g_draw_y += UI_CHARACTER_CARD_H + UI_GAP;
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
    DrawSkillCard(c, idx, x, y, w, UI_CARD_H, 0, 0, 0, 0);
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
    UiRect battlefield = {};
    snprintf(buf, sizeof(buf), "Battlefield - Round %d", state.round_count);
    draw_center_battlefield_frame(buf, &battlefield);
    int active_gap = UI_GAP + 8;
    int active_w = (battlefield.w - (UI_PAD * 4) - active_gap) / 2;
    int active_h = clamp_int(battlefield.h - 58, 116, UI_CHARACTER_CARD_H);
    int active_y = battlefield.y + 42;
    DrawCharacterCard(&state.p1.team[state.p1.active_idx], state.p1.active_idx,
                      battlefield.x + UI_PAD, active_y, active_w, active_h, 1, 0,
                      !state.p1.team[state.p1.active_idx].is_alive);
    DrawCharacterCard(&state.p2.team[state.p2.active_idx], state.p2.active_idx,
                      battlefield.x + UI_PAD + active_w + active_gap, active_y, active_w, active_h, 1, 0,
                      !state.p2.team[state.p2.active_idx].is_alive);
    settextcolor(RGB(74, 86, 70));
    outtextxy_clipped_utf8(battlefield.x + battlefield.w / 2 - 14, active_y + active_h / 2 - 8, 28, "VS");

    Player* active_player = (state.current_turn == state.p2.player_id) ? &state.p2 : &state.p1;
    int panel_x = g_main_x + UI_PAD;
    int panel_y = battlefield.y + battlefield.h + UI_GAP;
    int panel_w = g_main_w - (UI_PAD * 2);
    int panel_h = 58;
    draw_soft_panel(panel_x, panel_y, panel_w, panel_h, RGB(238, 232, 204), RGB(85, 74, 56));
    snprintf(buf, sizeof(buf), "Current: Player %d  Energy %d/%d  Hand %d  Draw %d  Discard %d",
             active_player->player_id, active_player->energy, active_player->max_energy,
             active_player->hand_count, active_player->draw_count, active_player->discard_count);
    settextcolor(RGB(27, 33, 35));
    outtextxy_clipped_utf8(panel_x + 12, panel_y + 9, panel_w - 24, buf);
    draw_meter(panel_x + 12, panel_y + 34, clamp_int(panel_w - 24, 160, 360),
               active_player->energy, active_player->max_energy, &g_art_energy_fill);

    g_draw_y = panel_y + panel_h + UI_GAP;
    if (active_player->hand_count > 0) {
        snprintf(buf, sizeof(buf), "Player %d Hand", active_player->player_id);
        draw_section_title(g_main_x + UI_PAD, g_draw_y, buf);
        g_draw_y += 28;
        int footer_y = bottom_button_y() - 8;
        int card_x[MAX_HAND_SIZE] = {0};
        int card_y[MAX_HAND_SIZE] = {0};
        int card_w[MAX_HAND_SIZE] = {0};
        int card_h[MAX_HAND_SIZE] = {0};
        compute_planned_card_rects(active_player->hand_count, g_draw_y, footer_y, card_x, card_y, card_w, card_h);
        for (int i = 0; i < active_player->hand_count; i++) {
            if (card_y[i] + card_h[i] > footer_y) break;
            DrawSkillCard(&active_player->hand[i], i, card_x[i], card_y[i], card_w[i], card_h[i], 0, 0,
                          active_player->energy < active_player->hand[i].energy_cost, 0);
        }
    } else {
        draw_line("Hand empty");
    }
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

void ShowTurnTransitionMask(int player_id) {
    (void)player_id;
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
            if (IsReturnToMenuRequested()) return act;
            if (layout_changed_since(&layout_version)) goto action_menu;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto action_menu;
        if (m.uMsg == WM_LBUTTONDOWN) {
            int switch_slot = hit_team_slot(m.x, m.y, player_id, NULL);
            if (switch_slot >= 0) {
                if (!p->team[switch_slot].is_alive) {
                    draw_overlay_message_kind("Character is defeated", UI_MSG_ERROR);
                    continue;
                }
                act.type = ACTION_SWITCH_CHAR;
                act.switch_to_idx = switch_slot;
                act.actor_id = player_id;
                return act;
            }
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) { act.type = ACTION_END_TURN; return act; }
            else if (point_in_rect(m.x, m.y, bx, by + 60, bw, bh)) {
                if (p->hand_count == 0) { draw_line("Hand empty, cannot play."); continue; }
                // 娓呯悊骞舵樉绀烘墜鐗屼负鍙偣鎸夐挳锛堥伩鍏嶆枃鏈噸鍙狅級
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
                // 绛夊緟鎵嬬墝鐐瑰嚮
                while (1) {
                    if (!poll_mouse_message(&m)) {
                        if (IsReturnToMenuRequested()) return act;
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
                                // 鏋勯€犲姩浣滃苟鏄剧ず淇℃伅
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
                                    // 鏄剧ず鐩爣閫夋嫨锛堢畝鍖栦负鏄剧ず TEAM_SIZE 涓寜閽紝闄勫甫瑙掕壊鍚嶏級
                                    UiRect target_rects[TEAM_SIZE] = {};
                                    Character* target_team = NULL;
                                    int target_player_id = player_id;
                                    if (c->target_type == TARGET_SELF_SINGLE) {
                                        target_team = p->team;
                                        target_player_id = player_id;
                                    }
                                    else {
                                        // 鏁屾柟闃熶紞
                                        if (player_id == state.p1.player_id) {
                                            target_team = (Character*)state.p2.team;
                                            target_player_id = state.p2.player_id;
                                        } else {
                                            target_team = (Character*)state.p1.team;
                                            target_player_id = state.p1.player_id;
                                        }
                                    }
                                    draw_team_hp_panel(&state, target_player_id, -1);
                                    for (int t = 0; t < TEAM_SIZE; t++) {
                                        get_team_slot_rect(target_player_id, t, &target_rects[t]);
                                    }
                                    while (1) {
                                        if (!poll_mouse_message(&m)) {
                                            if (IsReturnToMenuRequested()) return act;
                                            if (layout_changed_since(&layout_version)) goto action_menu;
                                            continue;
                                        }
                                        if (layout_changed_since(&layout_version)) goto action_menu;
                                        if (m.uMsg == WM_LBUTTONDOWN) {
                                            for (int t = 0; t < TEAM_SIZE; t++) {
                                                if (point_in_ui_rect(m.x, m.y, &target_rects[t])) {
                                                    if (target_team && !target_team[t].is_alive && c->target_type != TARGET_SELF_SINGLE) {
                                                        draw_overlay_message_kind("Target is defeated", UI_MSG_ERROR);
                                                        break;
                                                    }
                                                    act.target_idx = t;
                                                    return act;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    act.target_idx = 0; // 鍏ㄤ綋
                                    return act;
                                }
                            }
                        }
                    }
                }
            }
            else if (point_in_rect(m.x, m.y, bx, by + 120, bw, bh)) {
                // 娓呯悊骞舵樉绀哄彲鍒囨崲瑙掕壊锛堥伩鍏嶆枃鏈噸鍙狅級
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
                        if (IsReturnToMenuRequested()) return act;
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
        int hover_switch_slot = -1;
        int need_redraw = 1;
        while (1) {
            if (need_redraw) {
                draw_battle_plan_screen(&state, player_id, records, record_count, "Plan your turn", hover_action, hover_switch_slot, can_edit);
                bx = g_main_x + 24; by = g_draw_y + 10; bw = 240; bh = UI_BUTTON_H;
                need_redraw = 0;
            }
            if (!poll_mouse_message(&m)) {
                if (IsReturnToMenuRequested()) return act;
                if (layout_changed_since(&layout_version)) goto main_menu;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto main_menu;
            int new_hover = -1;
            int new_hover_switch_slot = hit_team_slot(m.x, m.y, player_id, NULL);
            if (point_in_rect(m.x, m.y, bx, by, bw, bh)) new_hover = 0;
            else if (point_in_rect(m.x, m.y, bx, by + 55, bw, bh)) new_hover = 1;
            else if (point_in_rect(m.x, m.y, bx, by + 110, bw, bh)) new_hover = 3;
            if (new_hover != hover_action || new_hover_switch_slot != hover_switch_slot) {
                hover_action = new_hover;
                hover_switch_slot = new_hover_switch_slot;
                need_redraw = 1;
            }
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (hover_switch_slot >= 0) {
                if (!p->team[hover_switch_slot].is_alive) {
                    draw_overlay_message_kind("Character is defeated", UI_MSG_ERROR);
                    continue;
                }
                act.type = ACTION_SWITCH_CHAR;
                act.switch_to_idx = hover_switch_slot;
                act.actor_id = player_id;
                return act;
            }
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
            if (can_edit && point_in_rect(m.x, m.y, bx, by + 110, bw, bh)) {
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
                    DrawSkillCard(&p->hand[i], i, card_x[i], card_y[i], card_w[i], card_h[i], 0, hover_card == i, p->energy < p->hand[i].energy_cost, 0);
                }
                draw_button_state(back_x, back_y, back_w, back_h, "Back", 0, hover_back, 0);
                end_deferred_present();
                need_redraw = 0;
            }
            if (!poll_mouse_message(&m)) {
                if (IsReturnToMenuRequested()) return act;
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
        int target_player_id = player_id;
        if (c->target_type == TARGET_SELF_SINGLE) {
            target_team = p->team;
            target_player_id = player_id;
        } else {
            target_team = (player_id == state.p1.player_id) ? state.p2.team : state.p1.team;
            target_player_id = (player_id == state.p1.player_id) ? state.p2.player_id : state.p1.player_id;
        }
        UiRect target_rects[TEAM_SIZE] = {};
        int back_x = g_main_x + 16, back_y = bottom_button_y(), back_w = 120, back_h = 38;
        int hover_target = -1;
        int need_redraw = 1;
        while (1) {
            if (need_redraw) {
                draw_target_select_screen(&state, player_id, records, record_count, c, target_player_id, target_rects, hover_target);
                back_x = g_main_x + 16; back_y = bottom_button_y();
                need_redraw = 0;
            }
            if (!poll_mouse_message(&m)) {
                if (IsReturnToMenuRequested()) return act;
                if (layout_changed_since(&layout_version)) goto target_select;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto target_select;
            int new_hover = -1;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) new_hover = 10;
            else new_hover = hit_team_slot(m.x, m.y, target_player_id, NULL);
            if (new_hover != hover_target) {
                hover_target = new_hover;
                need_redraw = 1;
            }
            if (m.uMsg != WM_LBUTTONDOWN) continue;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) {
                reset_pending_action(&act, player_id);
                goto main_menu;
            }
            {
                int target_slot = hit_team_slot(m.x, m.y, target_player_id, NULL);
                if (target_slot < 0) continue;
                if (!target_team[target_slot].is_alive && c->target_type != TARGET_SELF_SINGLE) {
                    draw_overlay_message_kind("Target is defeated", UI_MSG_ERROR);
                    continue;
                }
                act.target_idx = target_slot;
                return act;
            }
        }
    }

switch_select:
    p = mutable_player(&state, player_id);
    layout_version = g_ui_layout_version;
    {
        UiRect switch_rects[TEAM_SIZE] = {};
        int back_x = g_main_x + 16, back_y = bottom_button_y(), back_w = 120, back_h = 38;
        int hover_char = -1;
        int hover_back = 0;
        int need_redraw = 1;
        while (1) {
            if (need_redraw) {
                begin_deferred_present();
                draw_planning_shell(&state, player_id, records, record_count, "Choose active character");
                draw_team_hp_panel(&state, player_id, hover_char);
                back_x = g_main_x + 16; back_y = bottom_button_y();
                for (int i = 0; i < TEAM_SIZE; i++) {
                    get_team_slot_rect(player_id, i, &switch_rects[i]);
                }
                draw_button_state(back_x, back_y, back_w, back_h, "Back", 0, hover_back, 0);
                end_deferred_present();
                need_redraw = 0;
            }
            if (!poll_mouse_message(&m)) {
                if (IsReturnToMenuRequested()) return act;
                if (layout_changed_since(&layout_version)) goto switch_select;
                continue;
            }
            if (layout_changed_since(&layout_version)) goto switch_select;
            int new_hover_char = -1;
            int new_hover_back = 0;
            if (point_in_rect(m.x, m.y, back_x, back_y, back_w, back_h)) new_hover_back = 1;
            else new_hover_char = hit_team_slot(m.x, m.y, player_id, NULL);
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
            {
                int switch_slot = hit_team_slot(m.x, m.y, player_id, NULL);
                if (switch_slot < 0) continue;
                if (!p->team[switch_slot].is_alive) {
                    draw_overlay_message_kind("Character is defeated", UI_MSG_ERROR);
                    continue;
                }
                act.type = ACTION_SWITCH_CHAR;
                act.switch_to_idx = switch_slot;
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
                if (IsReturnToMenuRequested()) return act;
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

#ifdef USE_EASYX
static const Character* find_character_by_name(const GameState* state, const char* name) {
    if (!state || !name || name[0] == '\0') return NULL;
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (strcmp(state->p1.team[i].name, name) == 0) return &state->p1.team[i];
        if (strcmp(state->p2.team[i].name, name) == 0) return &state->p2.team[i];
    }
    return NULL;
}

static int collect_resolution_targets(const GameState* state, const ActionRecord* record,
                                      const ResolutionReport* report, const Character** targets, int max_targets) {
    int count = 0;
    if (!state || !record || !targets || max_targets <= 0) return 0;
    if (report && report->event_count > 0) {
        for (int i = 0; i < report->event_count && count < max_targets; i++) {
            const DamageResolutionEvent* event = &report->events[i];
            const Player* target_player = NULL;
            const Character* ch = NULL;
            if (event->target_player_id == state->p1.player_id) target_player = &state->p1;
            else if (event->target_player_id == state->p2.player_id) target_player = &state->p2;

            if (target_player &&
                event->target_slot_idx >= 0 &&
                event->target_slot_idx < TEAM_SIZE) {
                ch = &target_player->team[event->target_slot_idx];
            } else {
                ch = find_character_by_name(state, event->target_name);
            }
            if (!ch) continue;
            int duplicate = 0;
            for (int k = 0; k < count; k++) {
                if (targets[k] == ch) duplicate = 1;
            }
            if (!duplicate) targets[count++] = ch;
        }
    }
    if (count > 0 || record->action.type != ACTION_PLAY_CARD || !record->has_played_card) return count;

    const Player* acting = (record->player_id == state->p1.player_id) ? &state->p1 : &state->p2;
    const Player* enemy = (record->player_id == state->p1.player_id) ? &state->p2 : &state->p1;
    const Card* card = &record->played_card;
    if (card->target_type == TARGET_ENEMY_ALL || card->target_type == TARGET_SELF_ALL) {
        const Player* target_player = (card->target_type == TARGET_ENEMY_ALL) ? enemy : acting;
        for (int i = 0; i < TEAM_SIZE && count < max_targets; i++) targets[count++] = &target_player->team[i];
    } else if (card->target_type == TARGET_SELF_SINGLE) {
        int idx = record->action.target_idx;
        if (idx >= 10 && idx < 10 + TEAM_SIZE) idx -= 10;
        if (idx < 0 || idx >= TEAM_SIZE) idx = acting->active_idx;
        targets[count++] = &acting->team[idx];
    } else {
        int idx = record->action.target_idx;
        if (idx < 0 || idx >= TEAM_SIZE) idx = enemy->active_idx;
        targets[count++] = &enemy->team[idx];
    }
    return count;
}

static int draw_resolution_card_flow(const GameState* state, const ActionRecord* record, const ResolutionReport* report, int x, int y, int w) {
    if (!state || !record || record->action.type != ACTION_PLAY_CARD || !record->has_played_card) return 0;
    const Player* acting = (record->player_id == state->p1.player_id) ? &state->p1 : &state->p2;
    const Character* caster = &acting->team[acting->active_idx];
    const Character* targets[TEAM_SIZE] = { NULL, NULL, NULL };
    int target_count = collect_resolution_targets(state, record, report, targets, TEAM_SIZE);
    int arrow_w = 30;
    int caster_w = clamp_int((w - 2 * arrow_w - 2 * UI_GAP) / 3, 150, 230);
    int skill_w = clamp_int(caster_w, 150, 220);
    int target_w = w - caster_w - skill_w - 2 * arrow_w - 2 * UI_GAP;
    if (target_w < 150) target_w = 150;
    int card_h = 124;
    int target_h = target_count > 1 ? 88 : card_h;
    int flow_h = target_count > 1 ? target_count * target_h + (target_count - 1) * UI_GAP : card_h;
    DrawCharacterCard(caster, acting->active_idx, x, y, caster_w, card_h, 1, 0, !caster->is_alive);
    settextcolor(RGB(74, 86, 70));
    outtextxy_clipped_utf8(x + caster_w + 8, y + card_h / 2 - 8, arrow_w, "->");
    DrawSkillCard(&record->played_card, record->action.card_hand_idx,
                  x + caster_w + arrow_w + UI_GAP, y, skill_w, card_h, 1, 0, 0, 0);
    outtextxy_clipped_utf8(x + caster_w + arrow_w + UI_GAP + skill_w + 8, y + card_h / 2 - 8, arrow_w, "->");
    int tx = x + caster_w + skill_w + 2 * arrow_w + 2 * UI_GAP;
    for (int i = 0; i < target_count; i++) {
        int ty = y + i * (target_h + UI_GAP);
        DrawCharacterCard(targets[i], i, tx, ty, target_w, target_h, 0, 0, !targets[i]->is_alive);
    }
    if (target_count <= 0) {
        draw_soft_panel(tx, y, target_w, card_h, RGB(231, 229, 211), RGB(113, 101, 78));
        settextcolor(RGB(84, 76, 62));
        outtextxy_clipped_utf8(tx + 12, y + card_h / 2 - 8, target_w - 24, "No affected target");
    }
    return flow_h;
}
#endif

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
    if (record && record->action.type == ACTION_PLAY_CARD && record->has_played_card) {
        int flow_h = draw_resolution_card_flow(&state, record, report, g_main_x + 16, g_draw_y, g_main_w - 32);
        g_draw_y += flow_h + 14;
    } else {
        int active_w = clamp_int((g_main_w - 56) / 2, 220, 320);
        int active_y = g_draw_y;
        DrawCharacterCard(&state.p1.team[state.p1.active_idx], state.p1.active_idx, g_main_x + 16, active_y, active_w, 116, 1, 0, !state.p1.team[state.p1.active_idx].is_alive);
        DrawCharacterCard(&state.p2.team[state.p2.active_idx], state.p2.active_idx, g_main_x + 28 + active_w, active_y, active_w, 116, 1, 0, !state.p2.team[state.p2.active_idx].is_alive);
        g_draw_y += 130;
    }
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
    end_deferred_present();
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

int ShowVictoryScreen(GameState state, int winner_id) {
#ifdef USE_EASYX
victory_screen:
    int layout_version = g_ui_layout_version;
    int hover_menu = 0;
    int hover_again = 0;
    int need_redraw = 1;
    MOUSEMSG m;

    while (1) {
        update_layout();
        int panel_w = clamp_int(g_ui_w / 2, 420, 620);
        int panel_h = clamp_int(g_ui_h / 3, 240, 320);
        int panel_x = (g_ui_w - panel_w) / 2;
        int panel_y = (g_ui_h - panel_h) / 2;
        int button_w = clamp_int((panel_w - 72) / 2, 160, 230);
        int button_h = 48;
        int menu_x = panel_x + 24;
        int again_x = panel_x + panel_w - 24 - button_w;
        int button_y = panel_y + panel_h - 72;

        if (need_redraw) {
            draw_battle_screen(state);
            setfillcolor(RGB(23, 29, 34));
            solidrectangle(0, 0, g_ui_w, g_ui_h);
            draw_soft_panel(panel_x, panel_y, panel_w, panel_h, RGB(244, 238, 212), RGB(78, 63, 42));
            setfillcolor(RGB(56, 100, 139));
            fillrectangle(panel_x, panel_y, panel_x + panel_w, panel_y + 8);

            char title[128];
            snprintf(title, sizeof(title), "Congratulations! Player %d Wins!", winner_id);
            settextstyle_utf8(34, 0, UI_FONT_FACE_UTF8);
            settextcolor(RGB(31, 37, 41));
            outtextxy_centered_utf8(panel_x + 24, panel_y + 54, panel_w - 48, 48, title);

            settextstyle_utf8(20, 0, UI_FONT_FACE_UTF8);
            settextcolor(RGB(84, 76, 62));
            outtextxy_centered_utf8(panel_x + 48, panel_y + 116, panel_w - 96, 34, "Choose your next step");

            draw_button_state(menu_x, button_y, button_w, button_h, "Main Menu", 0, hover_menu, 0);
            draw_button_state(again_x, button_y, button_w, button_h, "Play Again", 0, hover_again, 0);
            present_frame();
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (IsReturnToMenuRequested()) return 0;
            if (layout_changed_since(&layout_version)) goto victory_screen;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto victory_screen;

        int new_hover_menu = point_in_rect(m.x, m.y, menu_x, button_y, button_w, button_h);
        int new_hover_again = point_in_rect(m.x, m.y, again_x, button_y, button_w, button_h);
        if (new_hover_menu != hover_menu || new_hover_again != hover_again) {
            hover_menu = new_hover_menu;
            hover_again = new_hover_again;
            need_redraw = 1;
        }
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (hover_menu) return 0;
            if (hover_again) return 1;
        }
    }
#else
    printf("Congratulations! Player %d Wins!\n", winner_id);
    return 0;
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
    int card_x[MAX_GLOBAL_CHARS] = {0};
    int card_y[MAX_GLOBAL_CHARS] = {0};
    int card_w[MAX_GLOBAL_CHARS] = {0};
    int card_h[MAX_GLOBAL_CHARS] = {0};
    int confirm_x = g_main_x + 24, confirm_y = bottom_button_y();
    int selected_idx = -1;
    int hover_idx = -1;
    int hover_confirm = 0;
    MOUSEMSG m;
    int need_redraw = 1;
    while (1) {
        if (need_redraw) {
            reset_draw_y();
            draw_simple_status_bar("Character Draft");
            draw_team_hp_panel(NULL); // keep HP panel intact; callers render full state before calling select
            snprintf(buf, sizeof(buf), "Player %d - Character slot %d", player_id, slot_number);
            draw_section_title(g_main_x + UI_PAD + 4, g_draw_y, buf);
            g_draw_y += 36;
            int start_y = g_draw_y + 8;
            confirm_x = g_main_x + 24;
            confirm_y = bottom_button_y();
            compute_card_grid_rects(g_char_count, start_y, confirm_y - UI_GAP, UI_CHARACTER_CARD_H, card_x, card_y, card_w, card_h);
            for (int i = 0; i < g_char_count; i++) {
                draw_character_option_panel(&g_all_characters[i], i, card_x[i], card_y[i], card_w[i], card_h[i], i == selected_idx, hover_idx == i, 0);
            }
            draw_button_state(confirm_x, confirm_y, 140, 40, "Confirm", selected_idx >= 0, hover_confirm, 0);
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (IsReturnToMenuRequested()) return -1;
            if (layout_changed_since(&layout_version)) goto single_character_select;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto single_character_select;
        int new_hover_idx = -1;
        int new_hover_confirm = 0;
        for (int i = 0; i < g_char_count; i++) {
            if (point_in_rect(m.x, m.y, card_x[i], card_y[i], card_w[i], card_h[i])) {
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
                if (point_in_rect(m.x, m.y, card_x[i], card_y[i], card_w[i], card_h[i])) {
                    selected_idx = i;
                    need_redraw = 1;
                    handled = 1;
                    break;
                }
            }
            // confirm button
            if (!handled && point_in_rect(m.x, m.y, confirm_x, confirm_y, 140, 40)) {
                if (selected_idx >= 0) {
                    return selected_idx;
                } else {
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
    int card_x[MAX_GLOBAL_CHARS] = {0};
    int card_y[MAX_GLOBAL_CHARS] = {0};
    int card_w[MAX_GLOBAL_CHARS] = {0};
    int card_h[MAX_GLOBAL_CHARS] = {0};
    int confirm_x = g_main_x + 24, confirm_y = bottom_button_y();
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
            confirm_x = g_main_x + 24; confirm_y = bottom_button_y();
            compute_card_grid_rects(g_char_count, g_draw_y + 8, confirm_y - UI_GAP, UI_CHARACTER_CARD_H, card_x, card_y, card_w, card_h);
            begin_deferred_present();
            for (int i = 0; i < g_char_count; i++) {
                draw_character_option_panel(&g_all_characters[i], i, card_x[i], card_y[i], card_w[i], card_h[i], selected[i], hover_idx == i, 0);
            }
            draw_button_state(confirm_x, confirm_y, 140, 40, "Confirm", 0, hover_confirm, 0);
            end_deferred_present();
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (IsReturnToMenuRequested()) {
                free(selected);
                if (out_count) *out_count = 0;
                return 0;
            }
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
            if (point_in_rect(m.x, m.y, card_x[i], card_y[i], card_w[i], card_h[i])) {
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
                if (point_in_rect(m.x, m.y, card_x[i], card_y[i], card_w[i], card_h[i])) { selected[i] = !selected[i]; need_redraw = 1; handled = 1; break; }
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
    int max_rows = visible_card_rows_for_current_layout();
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
            if (IsReturnToMenuRequested()) return -1;
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
    int max_rows = visible_card_rows_for_current_layout();
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
            if (IsReturnToMenuRequested()) {
                free(qty);
                if (out_count) *out_count = 0;
                return 0;
            }
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

MenuSelection ShowMainMenu(void) {
#ifdef USE_EASYX
main_menu:
    int layout_version = g_ui_layout_version;
    MenuSelection hover_choice = MENU_NONE;
    int need_redraw = 1;
    MOUSEMSG m;
    while (1) {
        if (need_redraw) {
            draw_main_menu_screen(hover_choice);
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (IsReturnToMenuRequested()) return MENU_NONE;
            if (layout_changed_since(&layout_version)) goto main_menu;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto main_menu;
        int menu_w = clamp_int(g_main_w / 2, 340, 460);
        int bx = g_main_x + (g_main_w - menu_w) / 2;
        int by = g_main_y + clamp_int(g_main_h / 5, 70, 130);
        int bh = 58;
        MenuSelection new_hover_choice = MENU_NONE;
        if (point_in_rect(m.x, m.y, bx, by, menu_w, bh)) new_hover_choice = MENU_PVP;
        else if (point_in_rect(m.x, m.y, bx, by + 76, menu_w, bh)) new_hover_choice = MENU_PVE;
        else if (point_in_rect(m.x, m.y, bx, by + 152, menu_w, bh)) new_hover_choice = MENU_CREDITS;
        if (new_hover_choice != hover_choice) {
            hover_choice = new_hover_choice;
            need_redraw = 1;
        }
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (hover_choice != MENU_NONE) return hover_choice;
        }
    }
#else
    const char* raw = getenv("ROCO_GAME_MODE");
    if (raw != NULL && raw[0] == '1') return MENU_PVE;
    return MENU_PVP;
#endif
}

AiPolicy ShowAIPolicyMenu(void) {
#ifdef USE_EASYX
ai_policy_menu:
    int layout_version = g_ui_layout_version;
    AiPolicy hover_policy = AI_POLICY_NONE;
    int hover_back = 0;
    int need_redraw = 1;
    MOUSEMSG m;
    while (1) {
        if (need_redraw) {
            draw_ai_policy_menu_screen(hover_policy, hover_back);
            need_redraw = 0;
        }

        if (!poll_mouse_message(&m)) {
            if (IsReturnToMenuRequested()) return AI_POLICY_NONE;
            if (layout_changed_since(&layout_version)) goto ai_policy_menu;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto ai_policy_menu;

        int menu_w = clamp_int(g_main_w / 2, 340, 500);
        int bx = g_main_x + (g_main_w - menu_w) / 2;
        int by = g_main_y + clamp_int(g_main_h / 7, 48, 96);
        int bh = 54;
        int gap = 68;
        int back_x = g_main_x + 24;
        int back_y = bottom_button_y();
        AiPolicy new_hover_policy = AI_POLICY_NONE;
        int new_hover_back = 0;

        if (point_in_rect(m.x, m.y, bx, by, menu_w, bh)) new_hover_policy = AI_POLICY_HEURISTIC;
        else if (point_in_rect(m.x, m.y, bx, by + gap, menu_w, bh)) new_hover_policy = AI_POLICY_RANDOM;
        else if (point_in_rect(m.x, m.y, bx, by + gap * 2, menu_w, bh)) new_hover_policy = AI_POLICY_HARD;
        else if (point_in_rect(m.x, m.y, bx, by + gap * 3, menu_w, bh)) new_hover_policy = AI_POLICY_LLM;
        else if (point_in_rect(m.x, m.y, back_x, back_y, 140, 40)) new_hover_back = 1;

        if (new_hover_policy != hover_policy || new_hover_back != hover_back) {
            hover_policy = new_hover_policy;
            hover_back = new_hover_back;
            need_redraw = 1;
        }

        if (m.uMsg == WM_LBUTTONDOWN) {
            if (hover_back) return AI_POLICY_NONE;
            if (hover_policy != AI_POLICY_NONE) return hover_policy;
        }
    }
#else
    const char* raw = getenv("ROCO_AI_POLICY");
    if (raw && strcmp(raw, "random") == 0) return AI_POLICY_RANDOM;
    if (raw && strcmp(raw, "hard") == 0) return AI_POLICY_HARD;
    if (raw && strcmp(raw, "llm") == 0) return AI_POLICY_LLM;
    return AI_POLICY_HEURISTIC;
#endif
}

int ShowCreditsScreenFromFile(const char* path) {
#ifdef USE_EASYX
credits_screen:
    int layout_version = g_ui_layout_version;
    std::string credits = read_text_file_utf8(path);
    int hover_back = 0;
    int need_redraw = 1;
    MOUSEMSG m;
    while (1) {
        if (need_redraw) {
            draw_credits_screen(credits.c_str(), hover_back);
            need_redraw = 0;
        }
        if (!poll_mouse_message(&m)) {
            if (IsReturnToMenuRequested()) return 1;
            if (layout_changed_since(&layout_version)) goto credits_screen;
            continue;
        }
        if (layout_changed_since(&layout_version)) goto credits_screen;
        int back_x = g_main_x + 24;
        int back_y = bottom_button_y();
        int new_hover_back = point_in_rect(m.x, m.y, back_x, back_y, 140, 40);
        if (new_hover_back != hover_back) {
            hover_back = new_hover_back;
            need_redraw = 1;
        }
        if (m.uMsg == WM_LBUTTONDOWN && new_hover_back) return 1;
    }
#else
    printf("[Credits] %s\n", path ? path : "docs/credits.txt");
    return 1;
#endif
}

int GetModeSelectionFromUI() {
    MenuSelection selection = ShowMainMenu();
    if (selection == MENU_PVE) return MODE_PVE;
    return MODE_PVP;
}
