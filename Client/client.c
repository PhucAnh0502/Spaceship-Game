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
    cJSON *response = NULL;
    
    while (1) {
        response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);
        
        if (response != NULL) {
            return response;
        }

        char dummy;
        int check = recv(sock, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
        if (check == 0) {
            printf("\n[ERROR] Server disconnected unexpectedly!\n");
            close(sock);
            exit(1);
        } else if (check < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("\n[ERROR] Socket error");
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
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            printf(">> Server [%d]: %s\n", status->valueint, msg->valuestring);
        }
        cJSON_Delete(res);
    }
}

void do_login() {
    if (current_user_id != 0) {
        printf(">> You are already logged in!\n");
        return;
    }

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
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        
        if (status && msg) {
            printf(">> Server [%d]: %s\n", status->valueint, msg->valuestring);

            if (status->valueint == RES_AUTH_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    current_user_id = cJSON_GetObjectItem(res_data, "id")->valueint;
                    printf(">> Login success! User ID: %d\n", current_user_id);
                }
            }
        }
        cJSON_Delete(res);
    }
}

void do_logout() {
    if (current_user_id == 0) {
        printf(">> You are not logged in.\n");
        return;
    }

    send_json(sock, ACT_LOGOUT, NULL);

    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        if (msg) printf(">> Server: %s\n", msg->valuestring);
        
        current_user_id = 0;
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
    }
    printf("0. Exit\n");
    printf("============================\n");
    printf("Your choice: ");
}

int main() {
    struct sockaddr_in serv_addr;

    memset(client_buffer, 0, BUFFER_SIZE);
    client_buf_len = 0;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    printf("Connected to server %s:%d\n", SERVER_IP, PORT);

    int choice;
    char buffer[10];

    while (1) {
        print_menu();
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
        if (sscanf(buffer, "%d", &choice) != 1) continue;

        switch (choice) {
            case 1: do_register(); break;
            case 2: do_login(); break;
            case 3: do_logout(); break;
            case 0:
                printf("Exiting...\n");
                close(sock);
                return 0;
            default: printf("Invalid choice!\n");
        }
    }

    return 0;
}