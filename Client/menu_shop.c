#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>

#include "main_menu.h"
#include "menu_shop.h"
#include "../Common/protocol.h"
#include "../Lib/cJSON.h"
#include "Utils/utils.h"
#include "../Server/handlers/shop/client_state.h"

void do_buy_ammo() {
    lock_ui();
    clear();

    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 4, "--- MUA DAN 30MM ---");
    attroff(A_BOLD | COLOR_PAIR(2));
    mvprintw(4, 4, "Gia: %d coins/hop (50 vien)", COST_AMMO_BOX);
    mvprintw(5, 4, "So luong (0 de huy): ");
    refresh();

    char input[20];
    get_input(5, 28, "", input, sizeof(input), 0);
    int quantity = atoi(input);
    if (quantity <= 0) {
        mvprintw(7, 4, "Huy mua.");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    int total_cost = COST_AMMO_BOX * quantity;
    mvprintw(7, 4, "Tong chi phi: %d coins", total_cost);
    refresh();

    if (!confirm_purchase("dan 30mm", total_cost)) {
        mvprintw(9, 4, "Huy mua.");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", ITEM_AMMO_30MM);
    cJSON_AddNumberToObject(data, "quantity", quantity);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();

    clear();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            display_response_message(4, 4, status->valueint == RES_SHOP_SUCCESS ? 2 : 1,
                                     status->valueint, msg->valuestring);

            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                    }
                }
                fetch_and_update_status(); 
            }
        }
        cJSON_Delete(res);
    } else {
        mvprintw(4, 4, ">> No response from server");
    }

    mvprintw(8, 4, "HP: %d | Coins: %d", current_hp, current_coins);
    mvprintw(10, 4, "Press any key to continue...");
    refresh();
    getch();
    unlock_ui();
}

void do_buy_laser() {
    lock_ui();
    clear();

    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 4, "--- MUA PHAO LASER ---");
    attroff(A_BOLD | COLOR_PAIR(2));
    mvprintw(4, 4, "Gia: %d coins", COST_LASER);
    refresh();

    if (!confirm_purchase("phao laser", COST_LASER)) {
        mvprintw(6, 4, "Huy mua.");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", ITEM_WEAPON_LASER_GUN);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();

    clear();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            display_response_message(4, 4, status->valueint == RES_SHOP_SUCCESS ? 2 : 1,
                                     status->valueint, msg->valuestring);

            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                    }
                }
                fetch_and_update_status(); 
            }
        }
        cJSON_Delete(res);
    } else {
        mvprintw(4, 4, ">> No response from server");
    }

    mvprintw(8, 4, "HP: %d | Coins: %d", current_hp, current_coins);
    mvprintw(10, 4, "Press any key to continue...");
    refresh();
    getch();
    unlock_ui();
}

void do_buy_laser_battery() {
    lock_ui();
    clear();

    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 4, "--- MUA PIN LASER ---");
    attroff(A_BOLD | COLOR_PAIR(2));
    mvprintw(4, 4, "Gia: %d coins/bo (10 lan ban)", COST_LASER_BATTERY);
    mvprintw(5, 4, "So luong (0 de huy): ");
    refresh();

    char input[20];
    get_input(5, 28, "", input, sizeof(input), 0);
    int quantity = atoi(input);
    if (quantity <= 0) {
        mvprintw(7, 4, "Huy mua.");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    int total_cost = COST_LASER_BATTERY * quantity;
    mvprintw(7, 4, "Tong chi phi: %d coins", total_cost);
    refresh();

    if (!confirm_purchase("pin laser", total_cost)) {
        mvprintw(9, 4, "Huy mua.");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", ITEM_LASER_BATTERY);
    cJSON_AddNumberToObject(data, "quantity", quantity);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();

    clear();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            display_response_message(4, 4, status->valueint == RES_SHOP_SUCCESS ? 2 : 1,
                                     status->valueint, msg->valuestring);

            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                    }
                }
                fetch_and_update_status(); 
            }
        }
        cJSON_Delete(res);
    } else {
        mvprintw(4, 4, ">> No response from server");
    }

    mvprintw(8, 4, "HP: %d | Coins: %d", current_hp, current_coins);
    mvprintw(10, 4, "Press any key to continue...");
    refresh();
    getch();
    unlock_ui();
}

void do_buy_missile() {
    lock_ui();
    clear();

    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 4, "--- MUA TEN LUA ---");
    attroff(A_BOLD | COLOR_PAIR(2));
    mvprintw(4, 4, "Gia: %d coins/qua", COST_MISSILE);
    refresh();

    if (!confirm_purchase("ten lua", COST_MISSILE)) {
        mvprintw(6, 4, "Huy mua.");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", ITEM_WEAPON_MISSILE);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();

    clear();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            display_response_message(4, 4, status->valueint == RES_SHOP_SUCCESS ? 2 : 1,
                                     status->valueint, msg->valuestring);

            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                    }
                }
                fetch_and_update_status(); 
            }
        }
        cJSON_Delete(res);
    } else {
        mvprintw(4, 4, ">> No response from server");
    }

    mvprintw(8, 4, "HP: %d | Coins: %d", current_hp, current_coins);
    mvprintw(10, 4, "Press any key to continue...");
    refresh();
    getch();
    unlock_ui();
}

void do_buy_armor() {
    lock_ui();
    clear();

    int armor_type;

    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 4, "--- MUA GIAP ---");
    attroff(A_BOLD | COLOR_PAIR(2));
    mvprintw(4, 4, "1. Giap co ban (%d coins, Amor = %d)", COST_BASIC_ARMOR, AMOR_VAL_BASIC);
    mvprintw(5, 4, "2. Giap tang cuong (%d coins, Amor = %d)", COST_HEAVY_ARMOR, AMOR_VAL_HEAVY);
    mvprintw(6, 4, "Chon (0 de huy): ");
    refresh();

    char input[20];
    get_input(6, 23, "", input, sizeof(input), 0);
    armor_type = atoi(input);

    if (armor_type == 0) {
        mvprintw(8, 4, "Huy mua.");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    if (armor_type != 1 && armor_type != 2) {
        mvprintw(8, 4, "Lua chon khong hop le!");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    int item_type = (armor_type == 1) ? ITEM_ARMOR_BASIC_KIT : ITEM_ARMOR_HEAVY_KIT;
    int cost = (armor_type == 1) ? COST_BASIC_ARMOR : COST_HEAVY_ARMOR;
    const char* armor_name = (armor_type == 1) ? "giap co ban" : "giap tang cuong";

    if (!confirm_purchase(armor_name, cost)) {
        mvprintw(8, 4, "Huy mua.");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", item_type);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();

    clear();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            display_response_message(4, 4, status->valueint == RES_SHOP_SUCCESS ? 2 : 1,
                                     status->valueint, msg->valuestring);

            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                    }
                }
                fetch_and_update_status(); 
            }
        }
        cJSON_Delete(res);
    } else {
        mvprintw(4, 4, ">> No response from server");
    }

    mvprintw(8, 4, "HP: %d | Coins: %d", current_hp, current_coins);
    mvprintw(10, 4, "Press any key to continue...");
    refresh();
    getch();
    unlock_ui();
}

void do_fix_ship() {
    lock_ui();
    clear();

    int hp_needed = 1000 - current_hp;

    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 4, "--- SUA TAU ---");
    attroff(A_BOLD | COLOR_PAIR(2));

    if (hp_needed <= 0) {
        mvprintw(4, 4, ">> Tau da day HP, khong can sua!");
        mvprintw(6, 4, "HP: %d | Coins: %d", current_hp, current_coins);
        mvprintw(8, 4, "Press any key to continue...");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    int repair_cost = hp_needed * COST_REPAIR_PER_HP;

    mvprintw(4, 4, "Gia: %d coin/HP", COST_REPAIR_PER_HP);
    mvprintw(5, 4, "HP can sua: %d", hp_needed);
    mvprintw(6, 4, "Tong chi phi: %d coins", repair_cost);
    refresh();

    if (!confirm_purchase("sua tau", repair_cost)) {
        mvprintw(8, 4, "Huy sua.");
        refresh();
        getch();
        unlock_ui();
        return;
    }

    send_json(sock, ACT_FIX_SHIP, NULL);
    cJSON *res = wait_for_response();

    clear();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            display_response_message(3, 4, status->valueint == RES_SHOP_SUCCESS ? 2 : 1,
                                     status->valueint, msg->valuestring);

            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *hp = cJSON_GetObjectItem(res_data, "hp");
                    cJSON *hp2 = cJSON_GetObjectItem(res_data, "current_hp");
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    cJSON *coin2 = cJSON_GetObjectItem(res_data, "remaining_coin");

                    if (hp2)
                        current_hp = hp2->valueint;
                    else if (hp)
                        current_hp = hp->valueint;

                    if (coin2)
                        current_coins = coin2->valueint;
                    else if (coin)
                        current_coins = coin->valueint;
                }
                fetch_and_update_status(); 
            }
        }
        cJSON_Delete(res);
    } else {
        mvprintw(3, 4, ">> No response from server");
    }

    mvprintw(7, 4, "HP: %d | Coins: %d", current_hp, current_coins);
    mvprintw(9, 4, "Press any key to continue...");
    refresh();
    getch();
    unlock_ui();
}


void do_mock_equip() {
    if (!confirm_purchase("Goi CHEAT (Free)", 0))
        return;

    send_json(sock, ACT_MOCK_EQUIP, NULL);
    cJSON *res = wait_for_response();

    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_SHOP_SUCCESS) {
            display_response_message(15, 5, 2, status->valueint, msg ? msg->valuestring : "Success");

            if (data) {
                cJSON *coin = cJSON_GetObjectItem(data, "coin");
                cJSON *hp = cJSON_GetObjectItem(data, "hp");
                if (coin)
                    current_coins = coin->valueint;
                if (hp)
                    current_hp = hp->valueint;
            }
            fetch_and_update_status();
        } else {
            display_response_message(15, 5, 1, status ? status->valueint : 0, "Failed");
        }
        cJSON_Delete(res);
    }

    mvprintw(17, 5, "Press any key...");
    getch();
}

void menu_shop() {
    clear();
    const char *options[] = {
        "1. Buy Ammo (30mm)",
        "2. Buy Laser Gun",
        "3. Buy Laser Battery",
        "4. Buy Missiles",
        "5. Buy Armor",
        "6. Fix Ship",
        "7. [DEBUG] Get Full Gear (Cheat)", 
        "8. Back"
    };
    int n_opts = 8; 

    while (1) {
        int choice = draw_menu("SHOP SYSTEM", options, n_opts);
        if (choice == -1 || choice == 7)
            break; 

        switch (choice) {
            case 0:
                do_buy_ammo();
                break;
            case 1:
                do_buy_laser();
                break;
            case 2:
                do_buy_laser_battery();
                break;
            case 3:
                do_buy_missile();
                break;
            case 4:
                do_buy_armor();
                break;
            case 5:
                do_fix_ship();
                break;
            case 6:
                do_mock_equip();
                break; 
        }
    }
}


int confirm_purchase(const char *item_name, int cost) {
    timeout(-1);

    int height, width;
    getmaxyx(stdscr, height, width); 

    // --- TRƯỜNG HỢP 1: KHÔNG ĐỦ TIỀN ---
    if (current_coins < cost) {
        clear();
        box(stdscr, 0, 0); 

        // Tiêu đề cảnh báo
        attron(A_BOLD | COLOR_PAIR(1)); 
        mvprintw(height / 2 - 2, (width - 20) / 2, "!!! INSUFFICIENT FUNDS !!!");
        attroff(A_BOLD | COLOR_PAIR(1));

        // Thông tin chi tiết
        char msg[100];
        snprintf(msg, sizeof(msg), "Cost: %d | You have: %d", cost, current_coins);
        mvprintw(height / 2, (width - strlen(msg)) / 2, "%s", msg);

        // Hướng dẫn thoát
        attron(A_DIM);
        mvprintw(height / 2 + 2, (width - 26) / 2, "Press any key to return...");
        attroff(A_DIM);

        refresh();
        getch(); 
        return 0; 
    }

    // --- TRƯỜNG HỢP 2: ĐỦ TIỀN -> HỎI XÁC NHẬN ---
    int choice = 0; 
    int key;

    while (1) {
        clear();
        box(stdscr, 0, 0); 

        // Tiêu đề
        attron(A_BOLD | COLOR_PAIR(2)); 
        mvprintw(height / 2 - 4, (width - 20) / 2, "=== CONFIRM PURCHASE ===");
        attroff(A_BOLD | COLOR_PAIR(2));

        // Nội dung câu hỏi
        char prompt[100];
        snprintf(prompt, sizeof(prompt), "Buy item: %s", item_name);
        mvprintw(height / 2 - 2, (width - strlen(prompt)) / 2, "%s", prompt);

        char price_info[50];
        snprintf(price_info, sizeof(price_info), "Price: %d coins", cost);
        mvprintw(height / 2 - 1, (width - strlen(price_info)) / 2, "%s", price_info);

        // Vẽ 2 nút chọn: [ YES ] và [ NO ]
        int btn_y = height / 2 + 2;

        // Vẽ nút YES
        if (choice == 0)
            attron(A_REVERSE | A_BOLD); 
        mvprintw(btn_y, width / 2 - 10, "[ YES ]");
        if (choice == 0)
            attroff(A_REVERSE | A_BOLD);

        // Vẽ nút NO
        if (choice == 1)
            attron(A_REVERSE | A_BOLD); 
        mvprintw(btn_y, width / 2 + 4, "[ NO ]");
        if (choice == 1)
            attroff(A_REVERSE | A_BOLD);

        // Hướng dẫn
        attron(A_DIM);
        mvprintw(height - 3, 2, "Use [LEFT/RIGHT] to choose, [ENTER] to confirm.");
        attroff(A_DIM);

        refresh();

        // Xử lý phím bấm
        key = getch();
        switch (key) {
            case KEY_LEFT:
            case KEY_RIGHT:
                choice = !choice; 
                break;

            case 10: 
            case 13:
                return (choice == 0); 

            case 27: 
            case 'n':
            case 'N':
                return 0;

            case 'y':
            case 'Y':
                return 1;
        }
    }
}

void do_treasure_hunt() {
    timeout(100);

    while (1) {
        pthread_mutex_lock(&pending_mutex);
        int has_treasure = pending_treasure.has_pending;
        pthread_mutex_unlock(&pending_mutex);

        if (has_treasure) {
            timeout(-1); 

            int t_id;
            char question[256];
            char type_str[50];
            int reward;
            char *options[5];

            pthread_mutex_lock(&pending_mutex);
            t_id = pending_treasure.treasure_id;
            strcpy(question, pending_treasure.question);
            strcpy(type_str, pending_treasure.chest_type);
            reward = pending_treasure.reward;
            for (int i = 0; i < 4; i++)
                options[i] = strdup(pending_treasure.options[i]);
            pthread_mutex_unlock(&pending_mutex);

            options[4] = "Skip / Back";

            char title[512];
            snprintf(title, sizeof(title), "CHEST: %s ($%d) | %s", type_str, reward, question);

            int choice = draw_menu(title, (const char **) options, 5);

            for (int i = 0; i < 4; i++)
                free(options[i]);

            if (choice == -1 || choice == 4) {
                timeout(100); 
                return;
            }

            clear();
            mvprintw(5, 5, "Sending answer...");
            refresh();

            cJSON *data = cJSON_CreateObject();
            cJSON_AddNumberToObject(data, "treasure_id", t_id);
            cJSON_AddNumberToObject(data, "answer", choice);
            send_json(sock, ACT_ANSWER, data);

            waiting_for_result = 1;
            if (treasure_response_data) {
                cJSON_Delete(treasure_response_data);
                treasure_response_data = NULL;
            }

            send_json(sock, ACT_ANSWER, data);

            pthread_mutex_lock(&pending_mutex);
            pending_treasure.has_pending = 0;
            pthread_mutex_unlock(&pending_mutex);

            int timeout_counter = 0;
            while (waiting_for_result && timeout_counter < 50) {
                usleep(100000); 
                timeout_counter++;
            }

            cJSON *res = treasure_response_data; 
            treasure_response_data = NULL; 
            waiting_for_result = 0; 

            if (res) {
                cJSON *status = cJSON_GetObjectItem(res, "status");
                cJSON *msg = cJSON_GetObjectItem(res, "message");
                cJSON *d = cJSON_GetObjectItem(res, "data");

                clear();
                if (status && status->valueint == RES_TREASURE_SUCCESS) {
                    attron(A_BOLD | COLOR_PAIR(3));
                    mvprintw(5, 5, " $$$ CONGRATULATIONS! $$$ ");
                    attroff(A_BOLD | COLOR_PAIR(3));
                    mvprintw(7, 5, ">> %s", msg ? msg->valuestring : "");
                    if (d) {
                        cJSON *total = cJSON_GetObjectItem(d, "total_coins");
                        if (total)
                            current_coins = total->valueint;
                    }
                } else {
                    display_response_message(5, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Failed");
                }
                cJSON_Delete(res);
            } else {
                mvprintw(5, 5, "Error: Timeout waiting for server.");
            }

            mvprintw(12, 5, "Press any key to return...");
            timeout(-1); 
            getch();
            return; 
        }

        // NẾU KHÔNG CÓ KHO BÁU -> HIỆN MÀN HÌNH CHỜ
        else {
            erase(); 
            box(stdscr, 0, 0); 

            attron(A_BOLD | COLOR_PAIR(3));
            mvprintw(2, 5, "=== TREASURE HUNT ZONE ===");
            attroff(A_BOLD | COLOR_PAIR(3));

            mvprintw(5, 5, ">> Scanning for treasure signals...");

            static int loading = 0;
            loading = (loading + 1) % 4;
            mvprintw(6, 5, "   Waiting%s   ", (loading == 0)
                                                  ? ".  "
                                                  : (loading == 1)
                                                        ? ".. "
                                                        : (loading == 2)
                                                              ? "..."
                                                              : "   ");

            mvprintw(8, 5, "[BACKSPACE] Return to Dashboard");
            refresh();

            int ch = getch(); 
            if (ch == KEY_BACKSPACE || ch == 127) {
                timeout(-1); 
                return;
            }
        }
    }
}

void run_treasure_mode(int key) {
    // --- VẼ GIAO DIỆN  ---
    erase();
    refresh(); 

    int height = 15;
    int width = 60;

    WINDOW *win = newwin(height, width, (LINES - height) / 2, (COLS - width) / 2);
    if (win == NULL)
        return;

    box(win, 0, 0); 

    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 1, (width - 23) / 2, "!!! KHO BAU XUAT HIEN !!!");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    mvwprintw(win, 3, 2, "Loai: %s", pending_treasure.chest_type);
    mvwprintw(win, 3, 30, "Thuong: %d coins", pending_treasure.reward);
    mvwprintw(win, 5, 2, "Cau hoi: %.45s...", pending_treasure.question);

    for (int i = 0; i < pending_treasure.option_count; i++) {
        mvwprintw(win, 7 + i, 4, "[%d] %s", i, pending_treasure.options[i]);
    }

    mvwprintw(win, height - 2, 2, "Nhan 0-3 de chon, Q de bo qua.");

    wrefresh(win); 
    delwin(win); 

    // --- XỬ LÝ LOGIC ---
    if (key == ERR) {
        return;
    }
    printf("Key: %d\n", key);
    if (key >= '0' && key <= '3') {
        int ans = key - '0';
        if (ans < pending_treasure.option_count) {
            pthread_mutex_lock(&treasure_mutex);
            current_treasure_id = pending_treasure.treasure_id;
            pthread_mutex_unlock(&treasure_mutex);

            handle_treasure_answer(ans);

            pthread_mutex_lock(&pending_mutex);
            pending_treasure.has_pending = 0;
            pthread_mutex_unlock(&pending_mutex);

            clear(); 
        }
    } else if (key == 'q' || key == 'Q') {
        pthread_mutex_lock(&pending_mutex);
        pending_treasure.has_pending = 0;
        pthread_mutex_unlock(&pending_mutex);
        clear();
    }
}