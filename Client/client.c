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
#include "main_menu.h"
#include "combat_menu.h"
#include "team_menu.h"
#include "menu_shop.h"

#define SERVER_IP "127.0.0.1"

int sock = 0;
int current_user_id = 0;
int current_coins = 0;
int current_hp = 1000;
char current_username[50] = "";

// Snapshot trang bị
typedef struct {
    int weapon;
    int ammo;
} ClientWeaponSlot;

typedef struct {
    int type;
    int durability;
} ClientArmorSlot;

ClientWeaponSlot client_cannons[4] = {0};
ClientWeaponSlot client_missiles[4] = {0};
ClientArmorSlot client_armor[2] = {0};

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

cJSON *treasure_response_data = NULL;
volatile int waiting_for_result = 0;

volatile int end_game_flag = 0; // 0: Bình thường, 1: Có kết quả trận đấu
char last_winner_name[50] = "Unknown";
volatile int winner_team_id = -1;
volatile int current_team_id = -1;
pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sync_cond = PTHREAD_COND_INITIALIZER;
cJSON *sync_response = NULL;


void print_equipment_status();

void *background_listener(void *arg);

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

cJSON *wait_for_response() {
    pthread_mutex_lock(&sync_mutex);

    waiting_for_result = 1;

    if (sync_response) {
        cJSON_Delete(sync_response);
        sync_response = NULL;
    }

    while (sync_response == NULL) {
        pthread_cond_wait(&sync_cond, &sync_mutex);
    }

    cJSON *res = sync_response;
    sync_response = NULL;
    waiting_for_result = 0;

    pthread_mutex_unlock(&sync_mutex);
    return res;
}

void show_player_status() {
    fetch_and_update_status();

    int y = 2;
    int x = 2;

    attron(A_BOLD);
    mvprintw(y, x, "=== TRANG THAI HIEN TAI ===");
    attroff(A_BOLD);

    mvprintw(y + 1, x, "User: %s", current_username);
    mvprintw(y + 2, x, "HP: %d/1000 | Coins: %d", current_hp, current_coins);

    draw_compact_status(y + 4, x);
    refresh();
}

int fetch_and_update_status() {
    send_json(sock, ACT_GET_STATUS, NULL);
    cJSON *res = wait_for_response();
    if (!res)
        return 0;

    cJSON *status = cJSON_GetObjectItem(res, "status");
    cJSON *data = cJSON_GetObjectItem(res, "data");
    if (status && status->valueint == RES_SHOP_SUCCESS && data) {
        cJSON *hp = cJSON_GetObjectItem(data, "hp");
        cJSON *coin = cJSON_GetObjectItem(data, "coin");
        if (hp)
            current_hp = hp->valueint;
        if (coin)
            current_coins = coin->valueint;

        // Armor
        cJSON *armor = cJSON_GetObjectItem(data, "armor");
        if (armor && cJSON_IsArray(armor)) {
            int idx = 0;
            cJSON *it = NULL;
            cJSON_ArrayForEach(it, armor) {
                if (idx >= 2) break;
                cJSON *t = cJSON_GetObjectItem(it, "type");
                cJSON *d = cJSON_GetObjectItem(it, "durability");
                client_armor[idx].type = t ? t->valueint : 0;
                client_armor[idx].durability = d ? d->valueint : 0;
                idx++;
            }
        }

        // Cannons
        cJSON *cans = cJSON_GetObjectItem(data, "cannons");
        if (cans && cJSON_IsArray(cans)) {
            int idx = 0;
            cJSON *it = NULL;
            cJSON_ArrayForEach(it, cans) {
                if (idx >= 4) break;
                cJSON *w = cJSON_GetObjectItem(it, "weapon");
                cJSON *a = cJSON_GetObjectItem(it, "ammo");
                client_cannons[idx].weapon = w ? w->valueint : 0;
                client_cannons[idx].ammo = a ? a->valueint : 0;
                idx++;
            }
        }

        // Missiles
        cJSON *miss = cJSON_GetObjectItem(data, "missiles");
        if (miss && cJSON_IsArray(miss)) {
            int idx = 0;
            cJSON *it = NULL;
            cJSON_ArrayForEach(it, miss) {
                if (idx >= 4) break;
                cJSON *w = cJSON_GetObjectItem(it, "weapon");
                cJSON *a = cJSON_GetObjectItem(it, "ammo");
                client_missiles[idx].weapon = w ? w->valueint : 0;
                client_missiles[idx].ammo = a ? a->valueint : 0;
                idx++;
            }
        }
    }
    cJSON_Delete(res);
    return 1;
}

static const char *armor_label(int type) {
    switch (type) {
        case ARMOR_BASIC: return "Basic";
        case ARMOR_HEAVY: return "Heavy";
        default: return "None";
    }
}

static const char *weapon_label(int type) {
    switch (type) {
        case WEAPON_CANNON_30MM: return "Cannon";
        case WEAPON_LASER: return "Laser";
        case WEAPON_MISSILE: return "Missile";
        default: return "None";
    }
}

void print_equipment_status() {
    printf("Armor: [%s:%d] [%s:%d]\n",
           armor_label(client_armor[0].type), client_armor[0].durability,
           armor_label(client_armor[1].type), client_armor[1].durability);

    printf("Cannons:\n");
    for (int i = 0; i < 4; i++) {
        printf("  Slot %d: %s | Ammo: %d\n", i,
               weapon_label(client_cannons[i].weapon), client_cannons[i].ammo);
    }

    printf("Missiles:\n");
    for (int i = 0; i < 4; i++) {
        printf("  Slot %d: %s | Ammo: %d\n", i,
               weapon_label(client_missiles[i].weapon), client_missiles[i].ammo);
    }
}

void draw_compact_status(int y, int x) {
    mvprintw(y, x, "Armor: [%s:%d] [%s:%d]",
             armor_label(client_armor[0].type), client_armor[0].durability,
             armor_label(client_armor[1].type), client_armor[1].durability);

    mvprintw(y + 1, x, "Cannons:");
    move(y + 1, x + 10);
    for (int i = 0; i < 4; i++) {
        printw("[%d:%s/%d] ", i + 1,
               weapon_label(client_cannons[i].weapon), client_cannons[i].ammo);
    }

    mvprintw(y + 2, x, "Missiles:");
    move(y + 2, x + 10);
    for (int i = 0; i < 4; i++) {
        printw("[%d:%s/%d] ", i + 1,
               weapon_label(client_missiles[i].weapon), client_missiles[i].ammo);
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;
    if (argc != 2) {
        printf("Invalid Argument: \n Usage: %s <server_IP>", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    memset(client_buffer, 0, BUFFER_SIZE);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    printf("Connected to server %s:%d\n", server_ip, PORT);

    if (pthread_create(&listener_thread, NULL, background_listener, NULL) != 0) {
        perror("Failed to create listener thread");
        return 1;
    }

    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(100);
    int dashboard_highlight = 0;
    int need_redraw = 1;
    // MENU CHÍNH
    while (1) {
        if (current_user_id == 0) {
            // --- GUEST MENU ---
            const char *options[] = {"1. Register", "2. Login", "3. Exit"};

            int choice = draw_menu("WELCOME GUEST", options, 3);

            if (choice == 2 || choice == -1)
                break;
            switch (choice) {
                case 0:
                    do_register();
                    break;
                case 1:
                    do_login();
                    dashboard_highlight = 0;
                    need_redraw = 1;
                    break;
            }
        } else {
            timeout(100);
            if (end_game_flag) {
                show_game_result_screen();
                need_redraw = 1;
                continue;
            }
            if (current_hp <= 0) {
                timeout(100);
                draw_dead_popup();

                refresh();

                int ch = getch();
                if (ch == 'q' || ch == 'Q') {
                    do_logout();
                }

                continue;
            }

            int is_treasure_mode = 0;
            pthread_mutex_lock(&pending_mutex);
            is_treasure_mode = pending_treasure.has_pending;
            pthread_mutex_unlock(&pending_mutex);

            int c = getch();

            if (is_treasure_mode) {
                run_treasure_mode(c);
            } else {
                // --- LOGIC MENU CHÍNH ---
                if (need_redraw) {
                    print_dashboard_menu(dashboard_highlight);
                    need_redraw = 0;
                }

                if (c != ERR) {
                    switch (c) {
                        case KEY_UP:
                            dashboard_highlight = (dashboard_highlight == 0) ? 3 : dashboard_highlight - 1;
                            need_redraw = 1;
                            break;
                        case KEY_DOWN:
                            dashboard_highlight = (dashboard_highlight == 3) ? 0 : dashboard_highlight + 1;
                            need_redraw = 1;
                            break;
                        case 10:
                            timeout(-1);

                            switch (dashboard_highlight) {
                                case 0:
                                    menu_team();
                                    break;
                                case 1:
                                    menu_combat();
                                    break;
                                case 2:
                                    do_treasure_hunt();
                                    break;
                                case 3:
                                    do_logout();
                                    break;
                            }

                            timeout(100);
                            clear();
                            need_redraw = 1;
                            break;
                    }
                }
            }
        }
    }

    endwin();
    close(sock);
    return 0;
}


void *background_listener(void *arg) {
    while (!should_exit) {
        cJSON *response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);
        if (response == NULL) {
            printf("\n[ERROR] Server disconnected!\n");
            should_exit = 1;
            break;
        }

        cJSON *status = cJSON_GetObjectItem(response, "status");
        cJSON *data = cJSON_GetObjectItem(response, "data");

        cJSON *action = cJSON_GetObjectItem(response, "action");

        int code = status ? status->valueint : 0;
        int act_code = action ? action->valueint : 0;

        cJSON *ques_node = (data) ? cJSON_GetObjectItem(data, "question") : NULL;
        int is_treasure_broadcast = (code == RES_TREASURE_SUCCESS && ques_node != NULL);
        // GÓI TIN MÀ MAIN THREAD ĐANG CHỜ (LOGIN, MUA ĐỒ, TẤN CÔNG...)
        int is_sync_msg = 0;

        pthread_mutex_lock(&sync_mutex);

        if (waiting_for_result &&
            !is_treasure_broadcast &&
            act_code != ACT_TREASURE_APPEAR &&
            code != RES_END_GAME) {
            if (sync_response)
                cJSON_Delete(sync_response);
            sync_response = cJSON_Duplicate(response, 1);

            waiting_for_result = 0;
            pthread_cond_signal(&sync_cond);
            pthread_mutex_unlock(&sync_mutex);
            is_sync_msg = 1;
            continue;
        }
        pthread_mutex_unlock(&sync_mutex);

        if (is_sync_msg) {
            cJSON_Delete(response);
            continue;
        }

        // XỬ LÝ CÁC GÓI TIN BẤT ĐỒNG BỘ (ASYNC EVENTS)

        if (code == RES_END_GAME) {
            pthread_mutex_lock(&pending_mutex);
            end_game_flag = 1;
            if (data) {
                cJSON *winner_flag = cJSON_GetObjectItem(data, "winner_team_id");
                cJSON *w_name = cJSON_GetObjectItem(data, "winner_team_name");
                if (winner_flag) {
                    winner_team_id = winner_flag->valueint;
                }
                if (w_name)
                    strncpy(last_winner_name, w_name->valuestring, 49);
            }
            pthread_mutex_lock(&ui_mutex);
            current_hp = 1000;
            pthread_mutex_unlock(&ui_mutex);

            pthread_mutex_unlock(&pending_mutex);
            // pthread_mutex_lock(&sync_mutex);
            // if (waiting_for_result) {
            //     waiting_for_result = 0;
            //     pthread_cond_broadcast(&sync_cond);
            // }
            // pthread_mutex_unlock(&sync_mutex);
        }

        if (status && status->valueint == RES_TREASURE_SUCCESS) {
            if (data) {
                cJSON *treasure_id = cJSON_GetObjectItem(data, "treasure_id");
                cJSON *question = cJSON_GetObjectItem(data, "question");
                cJSON *options = cJSON_GetObjectItem(data, "options");
                cJSON *reward = cJSON_GetObjectItem(data, "reward");
                cJSON *chest_type = cJSON_GetObjectItem(data, "chest_type");

                if (treasure_id && question && options) {
                    pthread_mutex_lock(&pending_mutex);

                    pending_treasure.has_pending = 1;
                    pending_treasure.treasure_id = treasure_id->valueint;
                    strncpy(pending_treasure.question, question->valuestring, 255);

                    if (chest_type)
                        strncpy(pending_treasure.chest_type, chest_type->valuestring, 49);
                    if (reward)
                        pending_treasure.reward = reward->valueint;

                    int idx = 0;
                    cJSON *option = NULL;
                    cJSON_ArrayForEach(option, options) {
                        if (idx < 4) {
                            strncpy(pending_treasure.options[idx], option->valuestring, 99);
                            idx++;
                        }
                    }
                    pending_treasure.option_count = idx;

                    pthread_mutex_unlock(&pending_mutex);
                }
            }
        } else if (code == RES_TREASURE_SUCCESS && cJSON_GetObjectItem(data, "question")) {
            pthread_mutex_lock(&pending_mutex);
            pending_treasure.has_pending = 1;

            cJSON *t_id = cJSON_GetObjectItem(data, "treasure_id");
            cJSON *ques = cJSON_GetObjectItem(data, "question");

            if (t_id)
                pending_treasure.treasure_id = t_id->valueint;
            if (ques)
                strncpy(pending_treasure.question, ques->valuestring, 255);
            pthread_mutex_unlock(&pending_mutex);
        } else if (code == RES_TREASURE_OPENED) {
            pthread_mutex_lock(&pending_mutex);
            pending_treasure.has_pending = 0;
            pthread_mutex_unlock(&pending_mutex);
        }

        if (data) {
            cJSON *hp_node = cJSON_GetObjectItem(data, "current_hp");
            cJSON *coin_node = cJSON_GetObjectItem(data, "current_coin");
            cJSON *total_coin = cJSON_GetObjectItem(data, "total_coins");
            cJSON *armor_slot_0 = cJSON_GetObjectItem(data, "armor_slot_0");
            cJSON *armor_slot_1 = cJSON_GetObjectItem(data, "armor_slot_1");

            if (hp_node || coin_node || total_coin || armor_slot_0 || armor_slot_1) {
                pthread_mutex_lock(&ui_mutex);
                if (hp_node)
                    current_hp = hp_node->valueint;
                if (coin_node)
                    current_coins = coin_node->valueint;
                if (total_coin)
                    current_coins = total_coin->valueint;
                if (armor_slot_0)
                    client_armor[0].durability = armor_slot_0->valueint;
                if (armor_slot_1)
                    client_armor[1].durability = armor_slot_1->valueint;
                pthread_mutex_unlock(&ui_mutex);
            }
        }

        cJSON_Delete(response);
    }
    return NULL;
}
