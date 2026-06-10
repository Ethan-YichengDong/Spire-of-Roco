#include "ai_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

#define AI_BUFFER_SIZE 16384
#define AI_PORT 8888
#define AI_SOCKET_TIMEOUT_MS 1000

static char g_ai_policy[32] = "heuristic";

void SetAIBackendPolicy(const char* policy) {
    if (!policy || policy[0] == '\0') {
        strncpy(g_ai_policy, "heuristic", sizeof(g_ai_policy) - 1);
        g_ai_policy[sizeof(g_ai_policy) - 1] = '\0';
        return;
    }
    strncpy(g_ai_policy, policy, sizeof(g_ai_policy) - 1);
    g_ai_policy[sizeof(g_ai_policy) - 1] = '\0';
}

typedef SOCKET RocoSocket;

static void cleanup_winsock(void) {
    WSACleanup();
}

static int ensure_winsock_started(void) {
    static int started = 0;

    if (started) return 0;

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return -1;
    }

    atexit(cleanup_winsock);
    started = 1;
    return 0;
}

static void close_ai_socket(RocoSocket sock) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
}

static int set_ai_socket_timeout(RocoSocket sock, DWORD timeout_ms) {
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
        return -1;
    }
    return 0;
}

// 追加格式化文本到固定大小缓冲区，避免 sprintf 造成越界风险
static void append_fmt(char* buffer, size_t buffer_size, size_t* offset, const char* fmt, ...) {
    if (*offset >= buffer_size) return;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buffer + *offset, buffer_size - *offset, fmt, args);
    va_end(args);

    if (written < 0) return;
    if ((size_t)written >= buffer_size - *offset) {
        *offset = buffer_size - 1;
    } else {
        *offset += (size_t)written;
    }
}

// JSON 字符串转义，当前主要用于名称字段
static void append_json_string(char* buffer, size_t buffer_size, size_t* offset, const char* text) {
    append_fmt(buffer, buffer_size, offset, "\"");
    for (int i = 0; i < MAX_NAME_LEN && text[i] != '\0'; i++) {
        char ch = text[i];
        unsigned char uch = (unsigned char)ch;
        if (ch == '"' || ch == '\\') {
            append_fmt(buffer, buffer_size, offset, "\\%c", ch);
        } else if (uch < 32 || uch > 126) {
            append_fmt(buffer, buffer_size, offset, "\\u%04x", uch);
        } else {
            append_fmt(buffer, buffer_size, offset, "%c", ch);
        }
    }
    append_fmt(buffer, buffer_size, offset, "\"");
}

static int get_safe_active_idx(const Player* player) {
    if (player->active_idx >= 0 && player->active_idx < TEAM_SIZE) {
        return player->active_idx;
    }
    return 0;
}

static int get_safe_hand_count(const Player* player) {
    if (player->hand_count >= 0 && player->hand_count <= MAX_HAND_SIZE) {
        return player->hand_count;
    }
    return 0;
}

static int get_safe_deck_count(int count) {
    if (count >= 0 && count <= MAX_DECK_SIZE) {
        return count;
    }
    return 0;
}

static int is_serializable_character(const Character* ch) {
    if (ch->element < ELEMENT_NORMAL || ch->element > ELEMENT_ELECTRIC) return 0;
    if (ch->max_hp <= 0 || ch->max_hp > 10000) return 0;
    if (ch->hp < 0 || ch->hp > ch->max_hp) return 0;
    if (ch->is_alive != 0 && ch->is_alive != 1) return 0;
    return 1;
}

static void append_buffs(char* buffer, size_t buffer_size, size_t* offset, const Character* ch, int is_valid) {
    append_fmt(buffer, buffer_size, offset, "[");
    for (int i = 0; i < BUFF_COUNT; i++) {
        if (i > 0) append_fmt(buffer, buffer_size, offset, ",");
        append_fmt(buffer, buffer_size, offset, "%d", is_valid ? ch->buffs[i] : 0);
    }
    append_fmt(buffer, buffer_size, offset, "]");
}

static void append_character_json(char* buffer, size_t buffer_size, size_t* offset, const Character* ch, int index) {
    int is_valid = is_serializable_character(ch);

    append_fmt(buffer, buffer_size, offset,
               "{\"index\":%d,\"char_id\":%d,\"name\":",
               index, is_valid ? ch->char_id : 0);
    append_json_string(buffer, buffer_size, offset, is_valid ? ch->name : "");
    append_fmt(buffer, buffer_size, offset,
               ",\"element\":%d,\"hp\":%d,\"max_hp\":%d,\"speed\":%d,\"is_alive\":%d,\"buffs\":",
               is_valid ? ch->element : ELEMENT_NORMAL,
               is_valid ? ch->hp : 0,
               is_valid ? ch->max_hp : 0,
               is_valid ? ch->speed : 0,
               is_valid ? ch->is_alive : 0);
    append_buffs(buffer, buffer_size, offset, ch, is_valid);
    append_fmt(buffer, buffer_size, offset, "}");
}

static void append_card_json(char* buffer, size_t buffer_size, size_t* offset, const Card* card, int hand_idx) {
    append_fmt(buffer, buffer_size, offset,
               "{\"hand_idx\":%d,\"card_id\":%d,\"name\":",
               hand_idx, card->card_id);
    append_json_string(buffer, buffer_size, offset, card->name);
    append_fmt(buffer, buffer_size, offset,
               ",\"element\":%d,\"type\":%d,\"energy_cost\":%d,"
               "\"base_damage\":%d,\"base_defense\":%d,\"base_heal\":%d,"
               "\"buff_effect\":%d,\"buff_value\":%d,\"buff_duration\":%d,\"target_type\":%d}",
               card->element, card->type, card->energy_cost,
               card->base_damage, card->base_defense, card->base_heal,
               card->buff_effect, card->buff_value, card->buff_duration, card->target_type);
}

static void append_player_json(char* buffer, size_t buffer_size, size_t* offset, const Player* player) {
    int hand_count = get_safe_hand_count(player);
    int energy = player->energy > 0 ? player->energy : 0;
    int max_energy = player->max_energy > 0 ? player->max_energy : energy;

    append_fmt(buffer, buffer_size, offset,
               "{\"player_id\":%d,\"name\":",
               player->player_id);
    append_json_string(buffer, buffer_size, offset, player->name);
    append_fmt(buffer, buffer_size, offset,
               ",\"active_idx\":%d,\"energy\":%d,\"max_energy\":%d,"
               "\"hand_count\":%d,\"draw_count\":%d,\"discard_count\":%d,\"team\":[",
               get_safe_active_idx(player), energy, max_energy,
               hand_count, get_safe_deck_count(player->draw_count), get_safe_deck_count(player->discard_count));

    for (int i = 0; i < TEAM_SIZE; i++) {
        if (i > 0) append_fmt(buffer, buffer_size, offset, ",");
        append_character_json(buffer, buffer_size, offset, &player->team[i], i);
    }

    append_fmt(buffer, buffer_size, offset, "],\"hand\":[");
    for (int i = 0; i < hand_count; i++) {
        if (i > 0) append_fmt(buffer, buffer_size, offset, ",");
        append_card_json(buffer, buffer_size, offset, &player->hand[i], i);
    }
    append_fmt(buffer, buffer_size, offset, "]}");
}

static int get_default_target_idx_for_card(Card* card, Player* acting_player, Player* target_player, int protocol_target) {
    if (card->target_type == TARGET_ENEMY_ALL) return -1;
    if (card->target_type == TARGET_SELF_ALL) return -2;

    if (card->target_type == TARGET_SELF_SINGLE) {
        if (protocol_target >= 10 && protocol_target < 10 + TEAM_SIZE) {
            return protocol_target - 10; // AI 协议遵守 10~12，转换为当前结算器的本队 0~2
        }
        if (protocol_target >= 0 && protocol_target < TEAM_SIZE) {
            return protocol_target; // 兼容旧协议
        }
        return get_safe_active_idx(acting_player);
    }

    if (protocol_target >= 0 && protocol_target < TEAM_SIZE && target_player->team[protocol_target].is_alive) {
        return protocol_target;
    }
    return get_safe_active_idx(target_player);
}

static int parse_json_int(const char* json, const char* key, int* value) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* pos = strstr(json, pattern);
    if (pos == NULL) return 0;

    pos = strchr(pos, ':');
    if (pos == NULL) return 0;
    pos++;

    while (*pos != '\0' && isspace((unsigned char)*pos)) pos++;

    char* end = NULL;
    long parsed = strtol(pos, &end, 10);
    if (end == pos) return 0;

    *value = (int)parsed;
    return 1;
}

static int send_all(RocoSocket sock, const char* data, size_t len) {
    size_t sent_total = 0;
    while (sent_total < len) {
        size_t remaining = len - sent_total;
        int chunk_size = (remaining > (size_t)INT_MAX) ? INT_MAX : (int)remaining;
        int sent = send(sock, data + sent_total, chunk_size, 0);
        if (sent == SOCKET_ERROR || sent == 0) return -1;
        sent_total += (size_t)sent;
    }
    return 0;
}

// 降级策略（Fallback）：基于硬编码规则的本地 AI，防止 Python 服务端断开时闪退或卡死
static Action FallbackAI(GameState state, int ai_player_id) {
    printf("[AI Bridge] Backend unavailable or timed out. Falling back to local %s AI.\n", g_ai_policy);
    Action act = {0};
    act.actor_id = ai_player_id;
    
    Player* ai_p = (ai_player_id == 1) ? &state.p1 : &state.p2;
    Player* opp_p = (ai_player_id == 1) ? &state.p2 : &state.p1;
    int active_idx = get_safe_active_idx(ai_p);
    int hand_count = get_safe_hand_count(ai_p);
    Character* active_char = &ai_p->team[active_idx];

    // 简单决定逻辑：如果角色血量 < 30，优先找防御/Buff卡打出，否则打出第一张能买得起的卡
    if (strcmp(g_ai_policy, "random") == 0 && hand_count > 0) {
        int playable[MAX_HAND_SIZE];
        int playable_count = 0;
        for (int i = 0; i < hand_count && playable_count < MAX_HAND_SIZE; i++) {
            if (ai_p->energy >= ai_p->hand[i].energy_cost) {
                playable[playable_count++] = i;
            }
        }
        if (playable_count > 0) {
            int idx = playable[rand() % playable_count];
            act.type = ACTION_PLAY_CARD;
            act.card_hand_idx = idx;
            act.switch_to_idx = -1;
            act.target_idx = get_default_target_idx_for_card(&ai_p->hand[idx], ai_p, opp_p, INT_MIN);
            return act;
        }
    }

    if (strcmp(g_ai_policy, "hard") == 0 && active_char->hp > 0 && hand_count > 0) {
        int best_idx = -1;
        int best_score = -999999;
        for (int i = 0; i < hand_count; i++) {
            Card* card = &ai_p->hand[i];
            if (ai_p->energy < card->energy_cost) continue;
            int score = card->base_damage * 3 + card->base_defense * 2 + card->base_heal * 2;
            if (card->buff_effect != BUFF_NONE) score += 10 + card->buff_value + card->buff_duration;
            if (active_char->hp < active_char->max_hp / 3 && card->type != CARD_TYPE_ATTACK) score += 20;
            score -= card->energy_cost;
            if (score > best_score) {
                best_score = score;
                best_idx = i;
            }
        }
        if (best_idx >= 0) {
            act.type = ACTION_PLAY_CARD;
            act.card_hand_idx = best_idx;
            act.switch_to_idx = -1;
            act.target_idx = get_default_target_idx_for_card(&ai_p->hand[best_idx], ai_p, opp_p, INT_MIN);
            return act;
        }
    }

    if (active_char->hp > 0 && hand_count > 0) {
        for (int i = 0; i < hand_count; i++) {
            if (ai_p->energy >= ai_p->hand[i].energy_cost) {
                // 血量较低时，倾向于打出技能卡或者能力卡来防御
                if (active_char->hp < 30 && ai_p->hand[i].type != CARD_TYPE_ATTACK) {
                    act.type = ACTION_PLAY_CARD;
                    act.card_hand_idx = i;
                    act.switch_to_idx = -1;
                    act.target_idx = get_default_target_idx_for_card(&ai_p->hand[i], ai_p, opp_p, INT_MIN);
                    return act;
                }
            }
        }
        
        // 默认行为：从左向右找到第一张买得起的卡直接打出
        for (int i = 0; i < hand_count; i++) {
            if (ai_p->energy >= ai_p->hand[i].energy_cost) {
                act.type = ACTION_PLAY_CARD;
                act.card_hand_idx = i;
                act.switch_to_idx = -1;
                act.target_idx = get_default_target_idx_for_card(&ai_p->hand[i], ai_p, opp_p, INT_MIN);
                return act;
            }
        }
    }
    
    // 如果没有卡能打或者没能量了，直接选择结束回合
    act.type = ACTION_END_TURN;
    act.card_hand_idx = 0;
    act.switch_to_idx = -1;
    act.target_idx = 0;
    return act;
}

static Action NormalizeBackendAction(GameState state, int ai_player_id, Action act, int has_target_idx) {
    Player* ai_p = (ai_player_id == 1) ? &state.p1 : &state.p2;
    Player* opp_p = (ai_player_id == 1) ? &state.p2 : &state.p1;

    act.actor_id = ai_player_id;

    if (act.type == ACTION_SWITCH_CHAR) {
        act.card_hand_idx = -1;
        act.target_idx = 0;
        if (act.switch_to_idx < 0 || act.switch_to_idx >= TEAM_SIZE ||
            !ai_p->team[act.switch_to_idx].is_alive ||
            act.switch_to_idx == get_safe_active_idx(ai_p)) {
            return FallbackAI(state, ai_player_id);
        }
        return act;
    }

    if (act.type == ACTION_PLAY_CARD) {
        act.switch_to_idx = -1;
        if (act.card_hand_idx < 0 || act.card_hand_idx >= get_safe_hand_count(ai_p)) {
            return FallbackAI(state, ai_player_id);
        }

        Card* card = &ai_p->hand[act.card_hand_idx];
        if (ai_p->energy < card->energy_cost) {
            return FallbackAI(state, ai_player_id);
        }

        int protocol_target = has_target_idx ? act.target_idx : INT_MIN;
        act.target_idx = get_default_target_idx_for_card(card, ai_p, opp_p, protocol_target);
        return act;
    }

    if (act.type == ACTION_END_TURN) {
        act.card_hand_idx = -1;
        act.switch_to_idx = -1;
        act.target_idx = 0;
        return act;
    }

    return FallbackAI(state, ai_player_id);
}

// 序列化游戏状态：将 C 语言中的 GameState 转换为 AI 后端使用的 JSON v1 状态快照
static void SerializeGameState(GameState state, int ai_player_id, char* buffer, size_t buffer_size) {
    Player* ai_p = (ai_player_id == 1) ? &state.p1 : &state.p2;
    Player* opp_p = (ai_player_id == 1) ? &state.p2 : &state.p1;
    size_t offset = 0;

    buffer[0] = '\0';
    append_fmt(buffer, buffer_size, &offset,
               "{\"schema_version\":1,\"turn_model\":\"sequential\","
               "\"ai_player_id\":%d,\"ai_policy\":\"%s\","
               "\"round_count\":%d,\"current_turn\":%d,"
               "\"game_stage\":%d,\"current_scene\":%d,"
               "\"target_protocol\":{\"enemy_single\":\"0-2\",\"self_single\":\"10-12\","
               "\"enemy_all\":-1,\"self_all\":-2},\"players\":{\"self\":",
               ai_player_id, g_ai_policy, state.round_count, state.current_turn,
               state.game_stage, state.current_scene);
    append_player_json(buffer, buffer_size, &offset, ai_p);
    append_fmt(buffer, buffer_size, &offset, ",\"opponent\":");
    append_player_json(buffer, buffer_size, &offset, opp_p);
    append_fmt(buffer, buffer_size, &offset, "}}");
}

// 主入口函数：通过本机的 Socket 通信端口请求外部的 Python AI 的决策
Action GetAIActionFromBackend(GameState state, int ai_player_id) {
    RocoSocket sock = INVALID_SOCKET;
    struct sockaddr_in serv_addr;
    char buffer[AI_BUFFER_SIZE] = {0};
    char json_payload[AI_BUFFER_SIZE] = {0};

    // 序列化当前的桌面状态作为发送体 payload
    SerializeGameState(state, ai_player_id, json_payload, sizeof(json_payload));

    if (ensure_winsock_started() < 0) {
        printf("[AI Bridge] Winsock initialization failed.\n");
        return FallbackAI(state, ai_player_id);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("[AI Bridge] Socket creation failed.\n");
        return FallbackAI(state, ai_player_id);
    }

    // 设置 1 秒的通信超时机制，防止 Python 服务端未启动导致整个游戏 C 进程被锁死
    if (set_ai_socket_timeout(sock, AI_SOCKET_TIMEOUT_MS) < 0) {
        printf("[AI Bridge] Failed to configure socket timeout.\n");
        close_ai_socket(sock);
        return FallbackAI(state, ai_player_id);
    }

    serv_addr.sin_family = AF_INET;
    // 规定 Python 后端挂载的端口
    serv_addr.sin_port = htons(AI_PORT);

    // 连接本机地址 127.0.0.1
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        close_ai_socket(sock);
        return FallbackAI(state, ai_player_id);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, (int)sizeof(serv_addr)) == SOCKET_ERROR) {
        printf("[AI Bridge] Failed to connect to Python AI backend.\n");
        close_ai_socket(sock);
        return FallbackAI(state, ai_player_id);
    }

    // 发送游戏当前状态
    if (send_all(sock, json_payload, strlen(json_payload)) < 0) {
        printf("[AI Bridge] Failed to send socket message.\n");
        close_ai_socket(sock);
        return FallbackAI(state, ai_player_id);
    }
    shutdown(sock, SD_SEND);

    // 阻塞并在 1 秒内等待返回决策结果
    int valread = recv(sock, buffer, (int)sizeof(buffer) - 1, 0);
    close_ai_socket(sock);

    // 处理通信超时或接收失败的场景
    if (valread <= 0) {
        printf("[AI Bridge] Socket receive timed out or disconnected.\n");
        return FallbackAI(state, ai_player_id);
    }
    buffer[valread] = '\0';

    // 将收到的模糊 JSON 字符串结果解析为 Action 结构体字段
    // 典型格式: {"type": 1, "card_hand_idx": 0, "switch_to_idx": -1, "target_idx": 0}
    Action act = {0};
    act.actor_id = ai_player_id;
    int type = 0, card_idx = -1, switch_idx = -1, target_idx = INT_MIN;

    if (!parse_json_int(buffer, "type", &type)) {
        printf("[AI Bridge] Received invalid JSON from AI backend.\n");
        return FallbackAI(state, ai_player_id);
    }

    act.type = (ActionType)type;
    parse_json_int(buffer, "card_hand_idx", &card_idx);
    parse_json_int(buffer, "switch_to_idx", &switch_idx);
    int has_target_idx = parse_json_int(buffer, "target_idx", &target_idx);

    act.card_hand_idx = card_idx;
    act.switch_to_idx = switch_idx;
    act.target_idx = target_idx;

    return NormalizeBackendAction(state, ai_player_id, act, has_target_idx);
}
