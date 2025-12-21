#include "../../Common/protocol.h"
#include "../storage.h"
#include "../../Lib/cJSON.h"
#include <stdio.h>
#include <string.h>

extern void send_response(int socket_fd, int status, const char *message, cJSON *data);
extern void log_action(const char *status, const char *action, const char *input, const char *result);


static Player *get_player_by_fd(int client_fd) {
    for (int i = 0; i < player_count; i++) {
        if (players[i].socket_fd == client_fd && players[i].is_online)
            return &players[i];
    }
    return NULL;
}


void handle_list_teams(int client_fd) {
    cJSON *arr = cJSON_CreateArray();

    for (int i = 0; i < team_count; i++) {
        if (teams[i].current_size > 0) {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "id", teams[i].team_id);
            cJSON_AddStringToObject(obj, "name", teams[i].team_name);
            cJSON_AddNumberToObject(
                obj,
                "slots",
                teams[i].current_size
            );
            cJSON_AddItemToArray(arr, obj);
        }
    }

    if (cJSON_GetArraySize(arr) == 0) {
        cJSON_Delete(arr);
        send_response(client_fd, 201, "List empty", NULL);
        return;
    }

    send_response(client_fd, RES_TEAM_SUCCESS,
                  "Get teams list successfully", arr);
}


void handle_list_members(int client_fd, cJSON *payload) {
    Player *player = get_player_by_fd(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Not logged in", NULL);
        return;
    }

    cJSON *name_node = cJSON_GetObjectItem(payload, "team_name");
    if (!name_node) {
        send_response(client_fd, 611, "Missing team_name", NULL);
        return;
    }

    Team *team = find_team_by_name(name_node->valuestring);
    if (!team) {
        send_response(client_fd, RES_TEAM_NO_EXIST, "Team not found", NULL);
        return;
    }

    cJSON *arr = cJSON_CreateArray();

    for (int i = 0; i < team->current_size; i++) {
        Player *p = find_player_by_id(team->member_ids[i]);
        if (!p) continue;

        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "id", p->id);
        cJSON_AddStringToObject(obj, "name", p->username);
        cJSON_AddBoolToObject(obj, "is_captain", p->id == team->captain_id);

        cJSON_AddItemToArray(arr, obj);
    }

    send_response(client_fd, RES_TEAM_SUCCESS,
                  "Get team members successfully", arr);
}



void handle_create_team(int client_fd, cJSON *payload) {
    Player *player = get_player_by_fd(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Not logged in", NULL);
        return;
    }

    if (player->team_id != 0) {
        send_response(client_fd, RES_ALREADY_IN_TEAM,
                      "Already in a team", NULL);
        return;
    }

    cJSON *name_node = cJSON_GetObjectItem(payload, "team_name");
    if (!name_node) {
        send_response(client_fd, RES_UNKNOWN_ACTION,
                      "Missing team_name", NULL);
        return;
    }

    Team *team = create_team(name_node->valuestring, player->id);
    if (!team) {
        send_response(client_fd, RES_UNKNOWN_ACTION,
                      "Cannot create team", NULL);
        return;
    }

    player->team_id = team->team_id;
    player->status = STATUS_IN_TEAM;

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "team_id", team->team_id);
    cJSON_AddStringToObject(data, "team_name", team->team_name);

    send_response(client_fd, RES_TEAM_SUCCESS,
                  "Create team successfully", data);
}


void handle_req_join(int client_fd, cJSON *payload) {
    Player *player = get_player_by_fd(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Not logged in", NULL);
        return;
    }

    if (player->team_id != 0) {
        send_response(client_fd, RES_ALREADY_IN_TEAM,
                      "Already in a team", NULL);
        return;
    }

    cJSON *name_node = cJSON_GetObjectItem(payload, "team_name");
    if (!name_node || name_node->valuestring == NULL ||
        strlen(name_node->valuestring) == 0) {

        send_response(client_fd, 611, "Missing or empty team_name", NULL);
        return;
    }

    Team *team = find_team_by_name(name_node->valuestring);
    if (!team) {
        send_response(client_fd, RES_TEAM_NO_EXIST,
                      "Team not found", NULL);
        return;
    }

    if (team->captain_id == player->id) {
        send_response(client_fd, 611, "Captain cannot request own team", NULL);
        return;
    }

    if (team->current_size >= MAX_MEMBERS) {
        send_response(client_fd, RES_TEAM_FULL,
                      "Team is full", NULL);
        return;
    }

    add_pending_request(team, player->id);

    send_response(client_fd, RES_TEAM_SUCCESS,
                  "Send request successfully", NULL);
}




void handle_approve_req(int client_fd, cJSON *payload, int approve) {
    Player *captain = get_player_by_fd(client_fd);
    if (!captain) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Not logged in", NULL);
        return;
    }

    Team *team = find_team_by_id(captain->team_id);
    if (!team || team->captain_id != captain->id) {
        send_response(client_fd, RES_NOT_TEAM_CAPTAIN,
                      "Not team captain", NULL);
        return;
    }

    cJSON *name_node = cJSON_GetObjectItem(payload, "target_username");
    if (!name_node) {
        send_response(client_fd, RES_UNKNOWN_ACTION,
                      "Missing target_username", NULL);
        return;
    }

    Player *target = find_player_by_username(name_node->valuestring);
    if (!target) {
        send_response(client_fd, RES_ACCOUNT_NOT_FOUND,
                      "User not found", NULL);
        return;
    }

    int target_id = target->id;

    if (!approve) {
        remove_pending_request(team, target_id);
        send_response(client_fd, RES_TEAM_SUCCESS,
                      "Request refused", NULL);
        return;
    }

    if (team->current_size >= MAX_MEMBERS) {
        send_response(client_fd, RES_TEAM_FULL,
                      "Team is full", NULL);
        return;
    }

    if (!remove_pending_request(team, target_id)) {
        send_response(client_fd, RES_TEAM_NO_EXIST,
                      "Request not found", NULL);
        return;
    }

    add_member(team, target_id);

    target->team_id = team->team_id;
    target->status = STATUS_IN_TEAM;

    send_response(client_fd, RES_TEAM_SUCCESS,
                  "Request approved", NULL);
}



void handle_leave_team(int client_fd) {
    Player *player = get_player_by_fd(client_fd);
    if (!player || player->team_id == 0) {
        send_response(client_fd, RES_ALREADY_IN_TEAM,
                      "User not in team", NULL);
        return;
    }

    Team *team = find_team_by_id(player->team_id);
    if (!team) {
        send_response(client_fd, RES_TEAM_NO_EXIST,
                      "Team not found", NULL);
        return;
    }

    if (team->captain_id == player->id && team->current_size > 1) {
        send_response(client_fd, RES_UNKNOWN_ACTION,
                      "Captain cannot leave", NULL);
        return;
    }

    remove_member(team, player->id);

    player->team_id = 0;
    player->status = STATUS_LOBBY;

    send_response(client_fd, RES_TEAM_SUCCESS,
                  "Leave team successfully", NULL);
}


void handle_kick_member(int client_fd, cJSON *payload) {
    Player *captain = get_player_by_fd(client_fd);
    if (!captain) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Not logged in", NULL);
        return;
    }

    Team *team = find_team_by_id(captain->team_id);
    if (!team || team->captain_id != captain->id) {
        send_response(client_fd, RES_NOT_TEAM_CAPTAIN,
                      "Not team captain", NULL);
        return;
    }

    cJSON *name_node = cJSON_GetObjectItem(payload, "target_username");
    if (!name_node || name_node->valuestring == NULL) {
        send_response(client_fd, RES_UNKNOWN_ACTION,
                      "Missing target_username", NULL);
        return;
    }

    Player *target = find_player_by_username(name_node->valuestring);
    if (!target) {
        send_response(client_fd, RES_TEAM_NO_EXIST,
                      "User not found", NULL);
        return;
    }

    if (target->id == captain->id) {
        send_response(client_fd, RES_UNKNOWN_ACTION,
                      "Cannot kick yourself", NULL);
        return;
    }

    if (team->current_size <= 1) {
        send_response(client_fd, RES_UNKNOWN_ACTION,
                      "Team has only one member", NULL);
        return;
    }

    if (!is_member(team, target->id)) {
        send_response(client_fd, RES_TEAM_NO_EXIST,
                      "Target not in team", NULL);
        return;
    }

    remove_member(team, target->id);

    target->team_id = 0;
    target->status = STATUS_LOBBY;

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "kicked_username", target->username);

    send_response(client_fd, RES_TEAM_SUCCESS,
                  "Member kicked", data);
}



void handle_team_action(int client_fd, int action, cJSON *payload) {
    switch (action) {
        case ACT_LIST_TEAMS:
            handle_list_teams(client_fd);
            break;
        case ACT_LIST_MEMBERS:
            handle_list_members(client_fd, payload);
            break;
        case ACT_CREATE_TEAM:
            handle_create_team(client_fd, payload);
            break;
        case ACT_REQ_JOIN:
            handle_req_join(client_fd, payload);
            break;
        case ACT_APPROVE_REQ:
            handle_approve_req(client_fd, payload, 1);
            break;
        case ACT_REFUSE_REQ:
            handle_approve_req(client_fd, payload, 0);
            break;
        case ACT_LEAVE_TEAM:
            handle_leave_team(client_fd);
            break;
        case ACT_KICK_MEMBER:
            handle_kick_member(client_fd, payload);
            break;
        default:
            send_response(client_fd, RES_UNKNOWN_ACTION,
                          "Unknown team action", NULL);
    }
}
