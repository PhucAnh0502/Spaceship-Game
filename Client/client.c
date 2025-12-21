#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ncurses.h>
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
            endwin();
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
    clear();
    mvprintw(2, 10, "--- REGISTER ---");
    get_input(4, 10, "Username: ", username, 50, 0);
    get_input(5, 10, "Password: ", password, 50, 1);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "username", username);
    cJSON_AddStringToObject(data, "password", password);

    send_json(sock, ACT_REGISTER, data);

    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            if(status->valueint == RES_AUTH_SUCCESS) {
                display_response_message(8, 10, 2, status->valueint, msg->valuestring);
            } else {
                display_response_message(8, 10, 1, status->valueint, msg->valuestring);
            }
        }
        cJSON_Delete(res);
    }
    mvprintw(10, 10, "Press any key to continue...");
    getch();
}

void do_login() {
    if (current_user_id != 0) {
        display_response_message(10, 10, 1, 0, "You are already logged in!");
        getch();
        return;
    }

    char username[50], password[50];
    erase();
    mvprintw(2, 10, "=== LOGIN ===");
    get_input(4, 10, "Username: ", username, 50, 0);
    get_input(5, 10, "Password: ", password, 50, 1);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "username", username);
    cJSON_AddStringToObject(data, "password", password);

    send_json(sock, ACT_LOGIN, data);

    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        
        if (status && msg) {
            if (status->valueint == RES_AUTH_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    current_user_id = cJSON_GetObjectItem(res_data, "id")->valueint;
                    display_response_message(8, 10, 2, status->valueint, msg->valuestring);
                }
            } else {
                display_response_message(8, 10, 1, status->valueint, msg->valuestring);
            }
        }
        cJSON_Delete(res);
    }
    mvprintw(10, 10, "Press any key to continue...");
    getch();
}

void do_logout() {
    if (current_user_id == 0) {
        display_response_message(10, 10, 1, 0, "You are not logged in.");
        getch();
        return;
    }

    send_json(sock, ACT_LOGOUT, NULL);

    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        if (msg) {
            display_response_message(8, 10, 2, 0, msg->valuestring);
        }
        current_user_id = 0;
        cJSON_Delete(res);
    }
    mvprintw(10, 10, "Press any key to continue...");
    getch();
}

void print_menu(int highlight) {
    const char *choices[] = {
        "1. Register",
        "2. Login",
        "3. Logout",
        "0. Exit"
    };
    int n_choices = sizeof(choices) / sizeof(choices[0]);

    erase();
    mvprintw(1, 10, "=== SPACE BATTLE ONLINE ===");
    if(current_user_id != 0){
        attron(COLOR_PAIR(2));
        mvprintw(2, 10, "Logged in as User ID: %d", current_user_id);
        attroff(COLOR_PAIR(2));
    }

    for(int i = 0; i < n_choices; i++){
        if(highlight == i){
            attron(A_REVERSE);
            mvprintw(5 + i, 10, "-> %s", choices[i]);
            attroff(A_REVERSE);
        } else {
            mvprintw(5 + i, 10, "%s", choices[i]);
        }
    }
    mvprintw(12, 10, "Use arrow keys to move, Enter to select.");
    refresh();
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

    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    int choice = -1;
    int highlight = 0;

    while (1) {
        print_menu(highlight);
        int c = getch();
        
        switch (c)
        {
        case KEY_UP:
                highlight = (highlight == 0) ? 3 : highlight - 1;
                break;
            case KEY_DOWN:
                highlight = (highlight == 3) ? 0 : highlight + 1;
                break;
            case 10:
                choice = highlight;
                break;
            default:
                break;
        }

        if(choice != -1){
            if(choice == 0){
                do_register();
            } else if(choice == 1){
                do_login();
            } else if(choice == 2){
                do_logout();
            } else if(choice == 3){
                break;
            }
            choice = -1;
        }
    }

    endwin();
    close(sock);
    return 0;
}