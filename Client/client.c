#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>

#include "../Common/protocol.h"
#include "../Lib/cJSON.h"
#include "utils.h"
#include "client_state.h"
#include "client_treasure.c"
#include "client_shop.c"

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
    
    unlock_ui();
}

void do_login() {
    if (current_user_id != 0) {
        printf(">> You are already logged in!\n");
        return;
    }

    lock_ui();
    
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
                    
                    cJSON *hp = cJSON_GetObjectItem(res_data, "hp");
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    
                    if (hp) current_hp = hp->valueint;
                    if (coin) current_coins = coin->valueint;
                    
                    printf(">> Login success! User ID: %d\n", current_user_id);
                    printf(">> HP: %d | Coins: %d\n", current_hp, current_coins);
                    
                    // Start listener thread
                    should_exit = 0;
                    pthread_create(&listener_thread, NULL, background_listener, NULL);
                }
            }
        }
        cJSON_Delete(res);
    }
    
    unlock_ui();
}

void do_logout() {
    if (current_user_id == 0) {
        printf(">> You are not logged in.\n");
        return;
    }

    should_exit = 1;
    pthread_join(listener_thread, NULL);

    send_json(sock, ACT_LOGOUT, NULL);

    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        if (msg) printf(">> Server: %s\n", msg->valuestring);
        
        current_user_id = 0;
        current_coins = 0;
        current_hp = 1000;
        cJSON_Delete(res);
    }
}

void print_menu() {
    printf("\n============================\n");
    if (current_user_id == 0) {
        printf("1. Register\n");
        printf("2. Login\n");
    } else {
        printf("User ID: %d | %d coins | %d HP\n", current_user_id, current_coins, current_hp);
        printf("--- Account ---\n");
        printf("3. Logout\n");
        
        printf("\n--- Shop ---\n");
        printf("4. Mua dan 30mm\n");
        printf("5. Mua phao laser\n");
        printf("6. Mua pin laser\n");
        printf("7. Mua ten lua\n");
        printf("8. Mua giap\n");
        printf("9. Sua tau\n");
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
        // Kiểm tra và hiển thị treasure pending (nếu có)
        show_pending_treasure();
        
        // Kiểm tra trạng thái treasure trước khi hiển thị menu
        pthread_mutex_lock(&treasure_mutex);
        int is_waiting = waiting_for_treasure_answer;
        pthread_mutex_unlock(&treasure_mutex);
        
        if (!is_waiting) {
            print_menu();
        }
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
        
        // Nếu đang chờ trả lời treasure
        pthread_mutex_lock(&treasure_mutex);
        is_waiting = waiting_for_treasure_answer;
        pthread_mutex_unlock(&treasure_mutex);
        
        if (is_waiting) {
            // Kiểm tra nếu người dùng muốn bỏ qua
            if (buffer[0] == 'q' || buffer[0] == 'Q') {
                pthread_mutex_lock(&treasure_mutex);
                waiting_for_treasure_answer = 0;
                current_treasure_id = 0;
                pthread_mutex_unlock(&treasure_mutex);
                printf("Bo qua ruong kho bau.\n");
                continue;
            }
            
            int answer;
            if (sscanf(buffer, "%d", &answer) == 1) {
                if (answer >= 0 && answer <= 3) {
                    handle_treasure_answer(answer);
                } else {
                    printf("Dap an khong hop le! Nhap 0-3 hoac 'q' de bo qua: ");
                }
            } else {
                printf("Dap an khong hop le! Nhap 0-3 hoac 'q' de bo qua: ");
            }
            continue;
        }
        
        // Xử lý menu bình thường
        if (sscanf(buffer, "%d", &choice) != 1) continue;

        switch (choice) {
            case 1: do_register(); break;
            case 2: do_login(); break;
            case 3: do_logout(); break;
            case 4: do_buy_ammo(); break;
            case 5: do_buy_laser(); break;
            case 6: do_buy_laser_battery(); break;
            case 7: do_buy_missile(); break;
            case 8: do_buy_armor(); break;
            case 9: do_fix_ship(); break;
            case 0:
                printf("Exiting...\n");
                if (current_user_id != 0) {
                    should_exit = 1;
                    pthread_join(listener_thread, NULL);
                }
                close(sock);
                return 0;
            default: printf("Invalid choice!\n");
        }
    }

    return 0;
}