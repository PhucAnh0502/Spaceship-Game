#include "../../../Common/protocol.h"
#include "../../services/storage/storage.h"
#include "../../../Lib/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern void send_response(int socket_fd, int status, const char *message, cJSON *data);
extern void log_action(const char *status, const char *action, const char *input, const char *result);

void handle_register(int client_fd, cJSON *payload) {
    cJSON *user_node = cJSON_GetObjectItem(payload, "username");
    cJSON *pass_node = cJSON_GetObjectItem(payload, "password");

    if(!user_node || !pass_node) {
        send_response(client_fd, RES_MISSING_CREDENTIALS, "Missing username/password", NULL);
        log_action("ERROR", "REGISTER", "NULL", "Missing input");
        return;
    }

    int result = register_player(user_node->valuestring, pass_node->valuestring);

    if(result == 1){
        printf("[LOG] Account created: %s\n", user_node->valuestring);
        log_action("SUCCESS", "REGISTER", user_node->valuestring, "Account created successfully");
        send_response(client_fd, RES_AUTH_SUCCESS, "Registered successfully", NULL);
    } else if(result == 0){
        log_action("ERROR", "REGISTER", user_node->valuestring, "Account already exists");
        send_response(client_fd, RES_ACCOUNT_EXISTS, "Username already exists", NULL);
    } else {
        log_action("ERROR", "REGISTER", user_node->valuestring, "Server error");
        send_response(client_fd, RES_UNKNOWN_ACTION, "Server error during registration", NULL);
    }
}

void handle_login(int client_fd, cJSON *payload) {
    cJSON *user_node = cJSON_GetObjectItem(payload, "username");
    cJSON *pass_node = cJSON_GetObjectItem(payload, "password");

    if(!user_node || !pass_node) {
        send_response(client_fd, RES_MISSING_CREDENTIALS, "Missing username/password", NULL);
        log_action("ERROR", "LOGIN", "NULL", "Missing input");
        return;
    }

    char *username = user_node->valuestring;
    char *password = pass_node->valuestring;
    Player *player = find_player_by_username(username);

    if(!player) {
        log_action("ERROR", "LOGIN", username, "Account does not exist");
        send_response(client_fd, RES_ACCOUNT_NOT_FOUND, "Account does not exist", NULL);
        return;
    }

    if(strcmp(player->password, password) != 0) {
        log_action("ERROR", "LOGIN", username, "Incorrect password");
        send_response(client_fd, RES_INVALID_PASSWORD, "Incorrect password", NULL);
        return;
    }

    if(player->is_online) {
        log_action("ERROR", "LOGIN", username, "Account already logged in");
        send_response(client_fd, RES_ACCOUNT_ALREADY_LOGGED_IN, "Account already logged in", NULL);
        return;
    }

    player->is_online = 1;
    player->socket_fd = client_fd;
    player->status = STATUS_LOBBY;

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "id", player->id);
    cJSON_AddNumberToObject(data, "hp", player->ship.hp);
    cJSON_AddNumberToObject(data, "coin", player->ship.coin);

    printf("[LOG] Player logged in: %s\n", username);
    log_action("SUCCESS", "LOGIN", username, "Logged in successfully");
    send_response(client_fd, RES_AUTH_SUCCESS, "Logged in successfully", data);
}

void handle_logout(int client_fd) {
    Player *target = NULL;

    extern Player* players;
    extern int player_count;

    if(players) {
        for(int i = 0; i < player_count; i++) {
            if(players[i].socket_fd == client_fd && players[i].is_online) {
                target = &players[i];
                break;
            }
        }
    }

    if(target){
        char username[50];
        strcpy(username, target->username);
        target->is_online = 0;
        target->socket_fd = 0;
        target->status = STATUS_OFFLINE;
        Team* team = find_team_by_id(target->team_id);
        if(team) {
            for(int i = 0; i < team->current_size; i++) {
                if(team->member_ids[i] == target->id) {
                    for(int j = i; j < team->current_size - 1; j++) {
                        team->member_ids[j] = team->member_ids[j + 1];
                    }
                    team->current_size--;
                    break;
                }
                if(team->captain_id == target->id) {
                    team->current_size = 0;
                    team->captain_id = -1;
                    break;
                }
            }
        }
        target->team_id = 0;
        printf("[LOG] Player logged out: %s\n", username);
        log_action("SUCCESS", "LOGOUT", username, "Logged out successfully");
        send_response(client_fd, RES_AUTH_SUCCESS, "Logged out successfully", NULL);
    } else {
        log_action("ERROR", "LOGOUT", "UNKNOWN", "No active session found");
        send_response(client_fd, RES_NOT_LOGGED_IN, "Not logged in", NULL);
    }
}

int check_auth(int client_fd) {
    extern Player* players;
    extern int player_count;

    if(players) {
        for(int i = 0; i < player_count; i++) {
            if(players[i].socket_fd == client_fd && players[i].is_online) {
                return 1; 
            }
        }
    }
    return 0; 
}
