#include "ai_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

// Fallback Rule-Based AI
static Action FallbackAI(GameState state, int ai_player_id) {
    printf("[AI Bridge] Falling back to local Rule-Based AI.\n");
    Action act = {0};
    act.actor_id = ai_player_id;
    
    Player* ai_p = (ai_player_id == 1) ? &state.p1 : &state.p2;
    Character* active_char = &ai_p->team[ai_p->active_idx];

    // Simple deterministic logic: Play defense if HP < 30, else attack with first affordable card
    if (active_char->hp > 0 && ai_p->hand_count > 0) {
        for (int i = 0; i < ai_p->hand_count; i++) {
            if (ai_p->energy >= ai_p->hand[i].energy_cost) {
                // If low HP, prefer playing a buff/shield or power card if available
                if (active_char->hp < 30 && ai_p->hand[i].type != CARD_TYPE_ATTACK) {
                    act.type = ACTION_PLAY_CARD;
                    act.card_hand_idx = i;
                    act.switch_to_idx = -1;
                    return act;
                }
            }
        }
        
        // Default to playing the first affordable card
        for (int i = 0; i < ai_p->hand_count; i++) {
            if (ai_p->energy >= ai_p->hand[i].energy_cost) {
                act.type = ACTION_PLAY_CARD;
                act.card_hand_idx = i;
                act.switch_to_idx = -1;
                return act;
            }
        }
    }
    
    // Default to ending turn if no action possible
    act.type = ACTION_END_TURN;
    act.card_hand_idx = 0;
    act.switch_to_idx = -1;
    return act;
}

// Serialize the state to a minified JSON (Subset for brevity)
static void SerializeGameState(GameState state, int ai_player_id, char* buffer) {
    Player* ai_p = (ai_player_id == 1) ? &state.p1 : &state.p2;
    Player* opp_p = (ai_player_id == 1) ? &state.p2 : &state.p1;
    
    sprintf(buffer, "{\"turn\":%d,\"stage\":%d,\"my_hp\":%d,\"my_energy\":%d,\"opp_hp\":%d,\"hand_count\":%d}", 
            state.current_turn, state.game_stage,
            ai_p->team[ai_p->active_idx].hp, ai_p->energy,
            opp_p->team[opp_p->active_idx].hp, ai_p->hand_count);
}

Action GetAIActionFromBackend(GameState state, int ai_player_id) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char json_payload[1024] = {0};

    SerializeGameState(state, ai_player_id, json_payload);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[AI Bridge] Socket creation error.\n");
        return FallbackAI(state, ai_player_id);
    }

    // Set timeout to 1 second to prevent blocking forever if Python server is down
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8888);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        close(sock);
        return FallbackAI(state, ai_player_id);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[AI Bridge] Connection Failed.\n");
        close(sock);
        return FallbackAI(state, ai_player_id);
    }

    send(sock, json_payload, strlen(json_payload), 0);
    int valread = recv(sock, buffer, 1024, 0);
    close(sock);

    if (valread <= 0) {
        printf("[AI Bridge] Socket timeout or disconnect during recv.\n");
        return FallbackAI(state, ai_player_id);
    }

    // Parse incoming JSON loosely
    // e.g., {"type": 1, "card_hand_idx": 0, "switch_to_idx": -1}
    Action act = {0};
    act.actor_id = ai_player_id;
    int type = 0, card_idx = 0, switch_idx = -1;

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
        printf("[AI Bridge] Malformed JSON received from backend.\n");
        return FallbackAI(state, ai_player_id);
    }

    return act;
}
