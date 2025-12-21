#ifndef CLIENT_SHOP_C
#define CLIENT_SHOP_C

#include "client_state.h"

void do_buy_ammo() {
    lock_ui();
    
    int quantity;
    show_player_status();
    
    printf("\n--- MUA DAN 30MM ---\n");
    printf("Gia: %d coins/hop (50 vien)\n", COST_AMMO_BOX);
    printf("So luong (0 de huy): ");
    
    char input[20];
    if (fgets(input, sizeof(input), stdin) == NULL || sscanf(input, "%d", &quantity) != 1 || quantity <= 0) {
        printf("Huy mua.\n");
        unlock_ui();
        return;
    }
    
    int total_cost = COST_AMMO_BOX * quantity;
    printf("Tong chi phi: %d coins\n", total_cost);
    
    if (!confirm_purchase("dan 30mm", total_cost)) {
        printf("Huy mua.\n");
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", ITEM_AMMO_30MM);
    cJSON_AddNumberToObject(data, "quantity", quantity);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            printf("\n>> Server [%d]: %s\n", status->valueint, msg->valuestring);
            
            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                        printf(">> Coins con lai: %d\n", current_coins);
                    }
                }
            }
        }
        cJSON_Delete(res);
    }
    
    unlock_ui();
}

void do_buy_laser() {
    lock_ui();
    show_player_status();
    
    printf("\n--- MUA PHAO LASER ---\n");
    printf("Gia: %d coins\n", COST_LASER);
    
    if (!confirm_purchase("phao laser", COST_LASER)) {
        printf("Huy mua.\n");
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", ITEM_WEAPON_LASER_GUN);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            printf("\n>> Server [%d]: %s\n", status->valueint, msg->valuestring);
            
            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                        printf(">> Coins con lai: %d\n", current_coins);
                    }
                }
            }
        }
        cJSON_Delete(res);
    }
    
    unlock_ui();
}

void do_buy_laser_battery() {
    lock_ui();
    
    int quantity;
    show_player_status();
    
    printf("\n--- MUA PIN LASER ---\n");
    printf("Gia: %d coins/bo (10 lan ban)\n", COST_LASER_BATTERY);
    printf("So luong (0 de huy): ");
    
    char input[20];
    if (fgets(input, sizeof(input), stdin) == NULL || sscanf(input, "%d", &quantity) != 1 || quantity <= 0) {
        printf("Huy mua.\n");
        unlock_ui();
        return;
    }
    
    int total_cost = COST_LASER_BATTERY * quantity;
    printf("Tong chi phi: %d coins\n", total_cost);
    
    if (!confirm_purchase("pin laser", total_cost)) {
        printf("Huy mua.\n");
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", ITEM_LASER_BATTERY);
    cJSON_AddNumberToObject(data, "quantity", quantity);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            printf("\n>> Server [%d]: %s\n", status->valueint, msg->valuestring);
            
            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                        printf(">> Coins con lai: %d\n", current_coins);
                    }
                }
            }
        }
        cJSON_Delete(res);
    }
    
    unlock_ui();
}

void do_buy_missile() {
    lock_ui();
    show_player_status();
    
    printf("\n--- MUA TEN LUA ---\n");
    printf("Gia: %d coins/qua\n", COST_MISSILE);
    
    if (!confirm_purchase("ten lua", COST_MISSILE)) {
        printf("Huy mua.\n");
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", ITEM_WEAPON_MISSILE);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            printf("\n>> Server [%d]: %s\n", status->valueint, msg->valuestring);
            
            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                        printf(">> Coins con lai: %d\n", current_coins);
                    }
                }
            }
        }
        cJSON_Delete(res);
    }
    
    unlock_ui();
}

void do_buy_armor() {
    lock_ui();
    
    int armor_type;
    show_player_status();
    
    printf("\n--- MUA GIAP ---\n");
    printf("1. Giap co ban (%d coins, Amor = %d)\n", COST_BASIC_ARMOR, AMOR_VAL_BASIC);
    printf("2. Giap tang cuong (%d coins, Amor = %d)\n", COST_HEAVY_ARMOR, AMOR_VAL_HEAVY);
    printf("Chon (0 de huy): ");
    
    char input[20];
    if (fgets(input, sizeof(input), stdin) == NULL || sscanf(input, "%d", &armor_type) != 1) {
        printf("Huy mua.\n");
        unlock_ui();
        return;
    }
    
    if (armor_type == 0) {
        printf("Huy mua.\n");
        unlock_ui();
        return;
    }
    
    if (armor_type != 1 && armor_type != 2) {
        printf("Lua chon khong hop le!\n");
        unlock_ui();
        return;
    }

    int item_type = (armor_type == 1) ? ITEM_ARMOR_BASIC_KIT : ITEM_ARMOR_HEAVY_KIT;
    int cost = (armor_type == 1) ? COST_BASIC_ARMOR : COST_HEAVY_ARMOR;
    const char* armor_name = (armor_type == 1) ? "giap co ban" : "giap tang cuong";
    
    if (!confirm_purchase(armor_name, cost)) {
        printf("Huy mua.\n");
        unlock_ui();
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "item_type", item_type);

    send_json(sock, ACT_BUY_ITEM, data);
    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            printf("\n>> Server [%d]: %s\n", status->valueint, msg->valuestring);
            
            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    if (coin) {
                        current_coins = coin->valueint;
                        printf(">> Coins con lai: %d\n", current_coins);
                    }
                }
            }
        }
        cJSON_Delete(res);
    }
    
    unlock_ui();
}

void do_fix_ship() {
    lock_ui();
    show_player_status();
    
    int hp_needed = 1000 - current_hp;
    
    if (hp_needed <= 0) {
        printf("\n>> Tau da day HP, khong can sua!\n");
        unlock_ui();
        return;
    }
    
    int repair_cost = hp_needed * COST_REPAIR_PER_HP;
    
    printf("\n--- SUA TAU ---\n");
    printf("Gia: %d coin/HP\n", COST_REPAIR_PER_HP);
    printf("HP can sua: %d\n", hp_needed);
    printf("Tong chi phi: %d coins\n", repair_cost);
    
    if (!confirm_purchase("sua tau", repair_cost)) {
        printf("Huy sua.\n");
        unlock_ui();
        return;
    }

    send_json(sock, ACT_FIX_SHIP, NULL);
    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            printf("\n>> Server [%d]: %s\n", status->valueint, msg->valuestring);
            
            if (status->valueint == RES_SHOP_SUCCESS) {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *hp = cJSON_GetObjectItem(res_data, "hp");
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");
                    
                    if (hp) {
                        current_hp = hp->valueint;
                        printf(">> HP: %d\n", current_hp);
                    }
                    if (coin) {
                        current_coins = coin->valueint;
                        printf(">> Coins: %d\n", current_coins);
                    }
                }
            }
        }
        cJSON_Delete(res);
    }
    
    unlock_ui();
}

#endif