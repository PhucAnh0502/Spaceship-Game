#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#include "../Common/protocol.h"
#include "../Lib/cJSON.h"
#include "utils.h"

#define SERVER_IP "127.0.0.1"

int sock = 0;
int current_user_id = 0;

char client_buffer[BUFFER_SIZE];
int client_buf_len = 0;


cJSON* wait_for_response() {
    while (1) {
        cJSON *response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);
        if (response) return response;

        char dummy;
        int check = recv(sock, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
        if (check == 0) {
            printf("\n[ERROR] Server disconnected!\n");
            close(sock);
            exit(1);
        }
    }
}


void do_register() {
    char username[50], password[50];

    printf("\n--- REGISTER ---\n");
    get_input("Username: ", username, 50);
    get_input("Password: ", password, 50);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "username", username);
    cJSON_AddStringToObject(data, "password", password);

    send_json(sock, ACT_REGISTER, data);

    cJSON *res = wait_for_response();
    if (res) {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}

void do_login() {
    char username[50], password[50];

    printf("\n--- LOGIN ---\n");
    get_input("Username: ", username, 50);
    get_input("Password: ", password, 50);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "username", username);
    cJSON_AddStringToObject(data, "password", password);

    send_json(sock, ACT_LOGIN, data);

    cJSON *res = wait_for_response();
    if (res) {
        int status = cJSON_GetObjectItem(res, "status")->valueint;
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);

        if (status == RES_AUTH_SUCCESS) {
            current_user_id = cJSON_GetObjectItem(
                cJSON_GetObjectItem(res, "data"), "id")->valueint;
            printf(">> Logged in as user %d\n", current_user_id);
        }
        cJSON_Delete(res);
    }
}

void do_logout() {
    send_json(sock, ACT_LOGOUT, NULL);
    cJSON *res = wait_for_response();
    if (res) {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        current_user_id = 0;
        cJSON_Delete(res);
    }
}


void do_list_teams() {
    send_json(sock, ACT_LIST_TEAMS, NULL);
    cJSON *res = wait_for_response();
    if (!res) return;

    int status = cJSON_GetObjectItem(res, "status")->valueint;
    if (status != 200) {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
        return;
    }

    cJSON *arr = cJSON_GetObjectItem(res, "data");
    printf("\n--- TEAM LIST ---\n");
    cJSON *team;
    cJSON_ArrayForEach(team, arr) {
        printf("ID: %d | Name: %s | Slots: %d\n",
            cJSON_GetObjectItem(team, "id")->valueint,
            cJSON_GetObjectItem(team, "name")->valuestring,
            cJSON_GetObjectItem(team, "slots")->valueint);
    }
    cJSON_Delete(res);
}

void do_create_team() {
    char name[50];
    get_input("Team name: ", name, 50);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "team_name", name);

    send_json(sock, ACT_CREATE_TEAM, data);
    cJSON *res = wait_for_response();
    if (res) {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}

void do_list_members() {
    char team_name[50];
    get_input("Enter team name: ", team_name, 50);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "team_name", team_name);

    send_json(sock, ACT_LIST_MEMBERS, data);

    cJSON *res = wait_for_response();
    if (!res) return;

    if (cJSON_GetObjectItem(res, "status")->valueint != 200) {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
        return;
    }

    cJSON *members = cJSON_GetObjectItem(res, "data");
    cJSON *mem;

    printf("\n--- MEMBERS OF TEAM '%s' ---\n", team_name);
    cJSON_ArrayForEach(mem, members) {
        printf("ID: %d | Name: %s | Captain: %s\n",
            cJSON_GetObjectItem(mem, "id")->valueint,
            cJSON_GetObjectItem(mem, "name")->valuestring,
            cJSON_GetObjectItem(mem, "is_captain")->valueint ? "YES" : "NO"
        );
    }

    cJSON_Delete(res);
}

void do_req_join() {
    char name[50];
    get_input("Team name to join: ", name, 50);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "team_name", name);

    send_json(sock, ACT_REQ_JOIN, data);

    cJSON *res = wait_for_response();
    if (res) {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}


void do_approve_req(int approve) {
    char username[50];
    get_input("Target username: ", username, 50);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "target_username", username);

    send_json(sock,
              approve ? ACT_APPROVE_REQ : ACT_REFUSE_REQ,
              data);

    cJSON *res = wait_for_response();
    if (res) {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}

void do_leave_team() {
    send_json(sock, ACT_LEAVE_TEAM, NULL);
    cJSON *res = wait_for_response();
    if (res) {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}

void do_kick_member() {
    char name[50];
    get_input("Username to kick: ", name, 50);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "target_username", name);

    send_json(sock, ACT_KICK_MEMBER, data);

    cJSON *res = wait_for_response();
    if (res) {
        printf(">> %s\n",
            cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}



void print_menu() {
    printf("\n============================\n");
    if (current_user_id == 0) {
        printf("1. Register\n");
        printf("2. Login\n");
    } else {
        printf("User ID: %d\n", current_user_id);
        printf("3. Logout\n");
        printf("4. List teams\n");
        printf("5. Create team\n");
        printf("6. List team members\n");
        printf("7. Request join team\n");
        printf("8. Approve join request\n");
        printf("9. Refuse join request\n");
        printf("10. Leave team\n");
        printf("11. Kick member\n");
    }
    printf("0. Exit\n");
    printf("============================\n");
    printf("Your choice: ");
}


int main() {
    struct sockaddr_in serv_addr;

    memset(client_buffer, 0, BUFFER_SIZE);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    printf("Connected to server %s:%d\n", SERVER_IP, PORT);

    int choice;
    char buf[16];

    while (1) {
        print_menu();
        if (!fgets(buf, sizeof(buf), stdin)) break;
        choice = atoi(buf);

        switch (choice) {
            case 1: do_register(); break;
            case 2: do_login(); break;
            case 3: do_logout(); break;
            case 4: do_list_teams(); break;
            case 5: do_create_team(); break;
            case 6: do_list_members(); break;
            case 7: do_req_join(); break;
            case 8: do_approve_req(1); break;
            case 9: do_approve_req(0); break;
            case 10: do_leave_team(); break;
            case 11: do_kick_member(); break;
            case 0:
                close(sock);
                return 0;
            default:
                printf("Invalid choice\n");
        }
    }
    return 0;
}
