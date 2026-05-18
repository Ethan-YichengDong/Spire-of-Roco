#include "ai_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

// 降级策略（Fallback）：基于硬编码规则的本地 AI，防止 Python 服务端断开时闪退或卡死
static Action FallbackAI(GameState state, int ai_player_id) {
    printf("[AI Bridge] 后台连接失败或超时，降级至基于本地规则的本地 AI。\n");
    Action act = {0};
    act.actor_id = ai_player_id;
    
    Player* ai_p = (ai_player_id == 1) ? &state.p1 : &state.p2;
    Character* active_char = &ai_p->team[ai_p->active_idx];

    // 简单决定逻辑：如果角色血量 < 30，优先找防御/Buff卡打出，否则打出第一张能买得起的卡
    if (active_char->hp > 0 && ai_p->hand_count > 0) {
        for (int i = 0; i < ai_p->hand_count; i++) {
            if (ai_p->energy >= ai_p->hand[i].energy_cost) {
                // 血量较低时，倾向于打出技能卡或者能力卡来防御
                if (active_char->hp < 30 && ai_p->hand[i].type != CARD_TYPE_ATTACK) {
                    act.type = ACTION_PLAY_CARD;
                    act.card_hand_idx = i;
                    act.switch_to_idx = -1;
                    return act;
                }
            }
        }
        
        // 默认行为：从左向右找到第一张买得起的卡直接打出
        for (int i = 0; i < ai_p->hand_count; i++) {
            if (ai_p->energy >= ai_p->hand[i].energy_cost) {
                act.type = ACTION_PLAY_CARD;
                act.card_hand_idx = i;
                act.switch_to_idx = -1;
                return act;
            }
        }
    }
    
    // 如果没有卡能打或者没能量了，直接选择结束回合
    act.type = ACTION_END_TURN;
    act.card_hand_idx = 0;
    act.switch_to_idx = -1;
    return act;
}

// 序列化游戏状态：将 C 语言中的 GameState 压缩转换为传递给 Python 端 AI 的 JSON 格式字符串
static void SerializeGameState(GameState state, int ai_player_id, char* buffer) {
    Player* ai_p = (ai_player_id == 1) ? &state.p1 : &state.p2;
    Player* opp_p = (ai_player_id == 1) ? &state.p2 : &state.p1;
    
    sprintf(buffer, "{\"turn\":%d,\"stage\":%d,\"my_hp\":%d,\"my_energy\":%d,\"opp_hp\":%d,\"hand_count\":%d}", 
            state.current_turn, state.game_stage,
            ai_p->team[ai_p->active_idx].hp, ai_p->energy,
            opp_p->team[opp_p->active_idx].hp, ai_p->hand_count);
}

// 主入口函数：通过本机的 Socket 通信端口请求外部的 Python AI 的决策
Action GetAIActionFromBackend(GameState state, int ai_player_id) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char json_payload[1024] = {0};

    // 序列化当前的桌面状态作为发送体 payload
    SerializeGameState(state, ai_player_id, json_payload);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[AI Bridge] Socket 建立失败。\n");
        return FallbackAI(state, ai_player_id);
    }

    // 设置 1 秒的通信超时机制，防止 Python 服务端未启动导致整个游戏 C 进程被锁死
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    serv_addr.sin_family = AF_INET;
    // 规定 Python 后端挂载的端口
    serv_addr.sin_port = htons(8888);

    // 连接本机地址 127.0.0.1
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        close(sock);
        return FallbackAI(state, ai_player_id);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[AI Bridge] 连接至 Python AI 后端失败。\n");
        close(sock);
        return FallbackAI(state, ai_player_id);
    }

    // 发送游戏当前状态
    send(sock, json_payload, strlen(json_payload), 0);
    // 阻塞并在 1 秒内等待返回决策结果
    int valread = recv(sock, buffer, 1024, 0);
    close(sock);

    // 处理通信超时或接收失败的场景
    if (valread <= 0) {
        printf("[AI Bridge] 接收 Socket 消息超时或断开。\n");
        return FallbackAI(state, ai_player_id);
    }

    // 将收到的模糊 JSON 字符串结果解析为 Action 结构体字段
    // 典型格式: {"type": 1, "card_hand_idx": 0, "switch_to_idx": -1}
    Action act = {0};
    act.actor_id = ai_player_id;
    int type = 0, card_idx = 0, switch_idx = -1;

    // 宽松的 JSON 手动解析
    if (strstr(buffer, "\"type\"") != NULL) {
        sscanf(strstr(buffer, "\"type\""), "\"type\":%d", &type);
        act.type = (ActionType)type;
        
        if (strstr(buffer, "\"card_hand_idx\"") != NULL) {
            sscanf(strstr(buffer, "\"card_hand_idx\""), "\"card_hand_idx\":%d", &card_idx);
            act.card_hand_idx = card_idx;
        }
        if (strstr(buffer, "\"switch_to_idx\"") != NULL) {
            sscanf(strstr(buffer, "\"switch_to_idx\""), "\"switch_to_idx\":%d", &switch_idx);
            act.switch_to_idx = switch_idx;
        }
    } else {
        printf("[AI Bridge] 收到来自 AI 后端的非法 JSON 格式反馈数据。\n");
        return FallbackAI(state, ai_player_id);
    }

    return act;
}
