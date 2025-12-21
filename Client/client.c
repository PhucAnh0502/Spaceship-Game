#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ncurses.h>
#include <pthread.h>

#include "../Common/protocol.h"
#include "../Lib/cJSON.h"
#include "Utils/utils.h"
#include "../Server/handlers/shop/client_state.h"
#include "../Server/handlers/shop/client_treasure.c"


#define SERVER_IP "127.0.0.1"

int sock = 0;
int current_user_id = 0;
int current_coins = 0;
int current_hp = 1000;

char client_buffer[BUFFER_SIZE];
int client_buf_len = 0;

pthread_t listener_thread;
volatile int should_exit = 0;

// UI Lock
volatile int ui_locked = 0;
pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;

// Treasure state
volatile int waiting_for_treasure_answer = 0;
volatile int current_treasure_id = 0;
pthread_mutex_t treasure_mutex = PTHREAD_MUTEX_INITIALIZER;

// Pending treasure
PendingTreasure pending_treasure = {0};
pthread_mutex_t pending_mutex = PTHREAD_MUTEX_INITIALIZER;

void lock_ui() {
    pthread_mutex_lock(&ui_mutex);
    ui_locked = 1;
    pthread_mutex_unlock(&ui_mutex);
}

void unlock_ui() {
    pthread_mutex_lock(&ui_mutex);
    ui_locked = 0;
    pthread_mutex_unlock(&ui_mutex);
}

int is_ui_locked() {
    pthread_mutex_lock(&ui_mutex);
    int locked = ui_locked;
    pthread_mutex_unlock(&ui_mutex);
    return locked;
}

cJSON* wait_for_response() {
    while (1) {
        cJSON *response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);
        if (response) return response;

        char dummy;
        int check = recv(sock, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
        if (check == 0) {
            endwin();
            printf("\n[ERROR] Server disconnected unexpectedly!\n");
            close(sock);
            exit(1);
        }
    }
}

void show_player_status() {
    printf("\n========== TRANG THAI HIEN TAI ==========\n");
    printf("Coins: %d\n", current_coins);
    printf("HP: %d/1000\n", current_hp);
}

int confirm_purchase(const char* item_name, int cost) {
    if (current_coins < cost) {
        printf("\n>> Khong du coins! Can %d, hien co %d\n", cost, current_coins);
        return 0;
    }
    
    printf("\nXac nhan mua %s voi gia %d coins? (y/n): ", item_name, cost);
    fflush(stdout);
    
    char confirm[10];
    if (fgets(confirm, sizeof(confirm), stdin) == NULL) return 0;
    
    return (confirm[0] == 'y' || confirm[0] == 'Y');
}

void do_register() {
    lock_ui();
    
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
    
    unlock_ui();
    mvprintw(10, 10, "Press any key to continue...");
    getch();
}

void do_login() {
    if (current_user_id != 0) {
        display_response_message(10, 10, 1, 0, "You are already logged in!");
        getch();
        return;
    }

    lock_ui();
    
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
                    
                    cJSON *hp = cJSON_GetObjectItem(res_data, "hp");
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    
                    if (hp) current_hp = hp->valueint;
                    if (coin) current_coins = coin->valueint;
                    
                    printf(">> Login success! User ID: %d\n", current_user_id);
                    printf(">> HP: %d | Coins: %d\n", current_hp, current_coins);
                    
                    // Start listener thread
                    should_exit = 0;
                    pthread_create(&listener_thread, NULL, background_listener, NULL);
                    display_response_message(8, 10, 2, status->valueint, msg->valuestring);
                }
            } else {
                display_response_message(8, 10, 1, status->valueint, msg->valuestring);
            }
        }
        cJSON_Delete(res);
    }
    
    unlock_ui();
    mvprintw(10, 10, "Press any key to continue...");
    getch();
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
        if (current_user_id == 0) {
            display_response_message(10, 10, 1, 0, "You are not logged in.");
            getch();
            return;
        }

        //     should_exit = 1;
        //     pthread_join(listener_thread, NULL);

        //     send_json(sock, ACT_LOGOUT, NULL);
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
}

void do_create_team() {
    char name[50];
    get_input(4,5,"Team name: ", name, 50,0);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "team_name", name);

    send_json(sock, ACT_CREATE_TEAM, data);
    cJSON *res = wait_for_response();
    if (res) {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        if (msg) {
            display_response_message(8, 10, 2, 0, msg->valuestring);
        }
        current_user_id = 0;
        current_coins = 0;
        current_hp = 1000;
        cJSON_Delete(res);
    }
    mvprintw(10, 10, "Press any key to continue...");
    getch();
}

void do_list_members() {
    char team_name[50];
    get_input(4,5,"Enter team name: ", team_name, 50,0);

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
    get_input(4,5,"Team name to join: ", name, 50,0);

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
    get_input(4,5,"Target username: ", username, 50,0);

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
    get_input(4,5,"Username to kick: ", name, 50,0);

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
      //**//
    }else {
        printf("User ID: %d | %d coins | %d HP\n", current_user_id, current_coins, current_hp);
        printf("--- Account ---\n");
        printf("3. Logout\n");
        // printf("\n--- Shop ---\n");
        // printf("4. Mua dan 30mm\n");
        // printf("5. Mua phao laser\n");
        // printf("6. Mua pin laser\n");
        // printf("7. Mua ten lua\n");
        // printf("8. Mua giap\n");
        // printf("9. Sua tau\n");
        printf("4. List teams\n");
        printf("5. Create team\n");
        printf("6. List team members\n");
        printf("7. Request join team\n");
        printf("8. Approve join request\n");
        printf("9. Refuse join request\n");
        printf("10. Leave team\n");
        printf("11. Kick member\n");
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

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    printf("Connected to server %s:%d\n", SERVER_IP, PORT);

    //char buf[16];

    while (1) {
        //     // Kiểm tra và hiển thị treasure pending (nếu có)
        //     show_pending_treasure();
        //
        //     // Kiểm tra trạng thái treasure trước khi hiển thị menu
        //     pthread_mutex_lock(&treasure_mutex);
        //     int is_waiting = waiting_for_treasure_answer;
        //     pthread_mutex_unlock(&treasure_mutex);
        //
        //     if (!is_waiting) {
        //         print_menu();
        //     }
        //
        //     if (fgets(buf, sizeof(buf), stdin) == NULL) break;
        //
        //     // Nếu đang chờ trả lời treasure
        //     pthread_mutex_lock(&treasure_mutex);
        //     is_waiting = waiting_for_treasure_answer;
        //     pthread_mutex_unlock(&treasure_mutex);
        //
        //     if (is_waiting) {
        //         // Kiểm tra nếu người dùng muốn bỏ qua
        //         if (buf[0] == 'q' || buf[0] == 'Q') {
        //             pthread_mutex_lock(&treasure_mutex);
        //             waiting_for_treasure_answer = 0;
        //             current_treasure_id = 0;
        //             pthread_mutex_unlock(&treasure_mutex);
        //             printf("Bo qua ruong kho bau.\n");
        //             continue;
        //         }
        //
        //         int answer;
        //         if (sscanf(buf, "%d", &answer) == 1) {
        //             if (answer >= 0 && answer <= 3) {
        //                 handle_treasure_answer(answer);
        //             } else {
        //                 printf("Dap an khong hop le! Nhap 0-3 hoac 'q' de bo qua: ");
        //             }
        //         } else {
        //             printf("Dap an khong hop le! Nhap 0-3 hoac 'q' de bo qua: ");
        //         }
        //         continue;
        //     }
        //
        //     // Xử lý menu bình thường
        //     if (sscanf(buf, "%d", &choice) != 1) continue;
        //     print_menu();
        //     if (!fgets(buf, sizeof(buf), stdin)) break;
        //     choice = atoi(buf);
        //
        //     switch (choice) {
        //         case 1: do_register(); break;
        //         case 2: do_login(); break;
        //         case 3: do_logout(); break;
        //         case 4: do_buy_ammo(); break;
        //         case 5: do_buy_laser(); break;
        //         case 6: do_buy_laser_battery(); break;
        //         case 7: do_buy_missile(); break;
        //         case 8: do_buy_armor(); break;
        //         case 9: do_fix_ship(); break;
        //         case 0:
        //             printf("Exiting...\n");
        //             if (current_user_id != 0) {
        //                 should_exit = 1;
        //                 pthread_join(listener_thread, NULL);
        //             }
        //             close(sock);
        //             return 0;
        //
        //         case 12: do_list_teams(); break;
        //         case 13: do_create_team(); break;
        //         case 14: do_list_members(); break;
        //         case 15: do_req_join(); break;
        //         case 16: do_approve_req(1); break;
        //         case 17: do_approve_req(0); break;
        //         case 10: do_leave_team(); break;
        //         case 11: do_kick_member(); break;
        //          default:
        //              printf("Invalid choice\n");
        //     }
        // }
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
}
