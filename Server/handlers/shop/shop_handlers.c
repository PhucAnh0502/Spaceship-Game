#include "../../../Common/protocol.h"
#include "../../services/storage/storage.h"
#include "../../../Lib/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern void send_response(int socket_fd, int status, const char *message, cJSON *data);
extern Player* find_player_by_socket(int socket_fd);
extern void update_player_to_file(Player *player);
extern void log_action(const char *status, const char *action, const char *input, const char *result);

void handle_buy_ammo(int client_fd, cJSON *payload) {
    Player *player = find_player_by_socket(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Chua dang nhap", NULL);
        return;
    }

    cJSON *qty_node = cJSON_GetObjectItem(payload, "quantity");
    if (!qty_node || qty_node->valueint <= 0) {
        send_response(client_fd, RES_SHOP_SUCCESS, "So luong khong hop le", NULL);
        return;
    }

    int quantity = qty_node->valueint;
    int total_cost = COST_AMMO_BOX * quantity;

    if (player->ship.coin < total_cost) {
        send_response(client_fd, RES_NOT_ENOUGH_COIN, "Khong du tien", NULL);
        log_action("ERROR", "BUY_AMMO", player->username, "Khong du coin");
        return;
    }

    // Tìm slot cannon (30mm hoặc trống)
    int slot_found = -1;
    for (int i = 0; i < 4; i++) {
        if (player->ship.cannons[i].weapon == WEAPON_CANNON_30MM || 
            player->ship.cannons[i].weapon == 0) {
            slot_found = i;
            break;
        }
    }

    if (slot_found == -1) {
        send_response(client_fd, RES_NOT_ENOUGH_SLOTS, "Khong con slot phao", NULL);
        return;
    }

    player->ship.coin -= total_cost;
    player->ship.cannons[slot_found].weapon = WEAPON_CANNON_30MM;
    player->ship.cannons[slot_found].current_ammo += 50 * quantity;

    update_player_to_file(player);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "coin", player->ship.coin);
    cJSON_AddNumberToObject(data, "ammo", player->ship.cannons[slot_found].current_ammo);
    cJSON_AddNumberToObject(data, "slot", slot_found);

    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "Mua %d hop dan, con %d coins", quantity, player->ship.coin);
    log_action("SUCCESS", "BUY_AMMO", player->username, log_msg);

    send_response(client_fd, RES_SHOP_SUCCESS, "Mua dan thanh cong", data);
}

void handle_buy_laser(int client_fd, cJSON *payload) {
    Player *player = find_player_by_socket(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Chua dang nhap", NULL);
        return;
    }

    if (player->ship.coin < COST_LASER) {
        send_response(client_fd, RES_NOT_ENOUGH_COIN, "Khong du tien", NULL);
        return;
    }

    // Tìm slot trống
    int slot_found = -1;
    for (int i = 0; i < 4; i++) {
        if (player->ship.cannons[i].weapon == 0) {
            slot_found = i;
            break;
        }
    }

    if (slot_found == -1) {
        send_response(client_fd, RES_NOT_ENOUGH_SLOTS, "Het slot phao (max 4)", NULL);
        return;
    }

    player->ship.coin -= COST_LASER;
    player->ship.cannons[slot_found].weapon = WEAPON_LASER;
    player->ship.cannons[slot_found].current_ammo = 0;

    update_player_to_file(player);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "coin", player->ship.coin);
    cJSON_AddNumberToObject(data, "slot", slot_found);

    log_action("SUCCESS", "BUY_LASER", player->username, "Mua phao laser");
    send_response(client_fd, RES_SHOP_SUCCESS, "Mua phao laser thanh cong", data);
}

void handle_buy_laser_battery(int client_fd, cJSON *payload) {
    Player *player = find_player_by_socket(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Chua dang nhap", NULL);
        return;
    }

    cJSON *qty_node = cJSON_GetObjectItem(payload, "quantity");
    if (!qty_node || qty_node->valueint <= 0) {
        send_response(client_fd, RES_SHOP_SUCCESS, "So luong khong hop le", NULL);
        return;
    }

    int quantity = qty_node->valueint;
    int total_cost = COST_LASER_BATTERY * quantity;

    if (player->ship.coin < total_cost) {
        send_response(client_fd, RES_NOT_ENOUGH_COIN, "Khong du tien", NULL);
        return;
    }

    // Tìm slot có laser
    int laser_slot = -1;
    for (int i = 0; i < 4; i++) {
        if (player->ship.cannons[i].weapon == WEAPON_LASER) {
            laser_slot = i;
            break;
        }
    }

    if (laser_slot == -1) {
        send_response(client_fd, RES_SHOP_SUCCESS, "Chua co phao laser. Mua phao laser truoc.", NULL);
        return;
    }

    player->ship.coin -= total_cost;
    player->ship.cannons[laser_slot].current_ammo += 10 * quantity;

    update_player_to_file(player);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "coin", player->ship.coin);
    cJSON_AddNumberToObject(data, "laser_ammo", player->ship.cannons[laser_slot].current_ammo);

    log_action("SUCCESS", "BUY_BATTERY", player->username, "Mua pin laser");
    send_response(client_fd, RES_SHOP_SUCCESS, "Mua pin laser thanh cong", data);
}

void handle_buy_missile(int client_fd, cJSON *payload) {
    Player *player = find_player_by_socket(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Chua dang nhap", NULL);
        return;
    }

    cJSON *qty_node = cJSON_GetObjectItem(payload, "quantity");
    if (!qty_node || qty_node->valueint <= 0) {
        send_response(client_fd, RES_SHOP_SUCCESS, "So luong khong hop le", NULL);
        return;
    }

    int quantity = qty_node->valueint;
    int total_cost = COST_MISSILE * quantity;

    if (player->ship.coin < total_cost) {
        send_response(client_fd, RES_NOT_ENOUGH_COIN, "Khong du tien", NULL);
        return;
    }

    // Đếm missile hiện có
    int missile_count = 0;
    for (int i = 0; i < 4; i++) {
        if (player->ship.missiles[i].weapon == WEAPON_MISSILE) {
            missile_count++;
        }
    }

    if (missile_count + quantity > 4) {
        send_response(client_fd, RES_NOT_ENOUGH_SLOTS, "Toi da 4 ten lua", NULL);
        return;
    }

    player->ship.coin -= total_cost;

    // Thêm missile
    int added = 0;
    for (int i = 0; i < 4 && added < quantity; i++) {
        if (player->ship.missiles[i].weapon == 0) {
            player->ship.missiles[i].weapon = WEAPON_MISSILE;
            player->ship.missiles[i].current_ammo = 1;
            added++;
        }
    }

    update_player_to_file(player);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "coin", player->ship.coin);
    cJSON_AddNumberToObject(data, "missiles_added", added);

    log_action("SUCCESS", "BUY_MISSILE", player->username, "Mua ten lua");
    send_response(client_fd, RES_SHOP_SUCCESS, "Mua ten lua thanh cong", data);
}

void handle_buy_armor(int client_fd, cJSON *payload) {
    Player *player = find_player_by_socket(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Chua dang nhap", NULL);
        return;
    }

    cJSON *type_node = cJSON_GetObjectItem(payload, "armor_type");
    if (!type_node || !cJSON_IsNumber(type_node)) {
        send_response(client_fd, RES_SHOP_SUCCESS, "Loai giap khong hop le", NULL);
        return;
    }

    int armor_type = type_node->valueint;
    int cost, durability;
    const char *type_name;

    if (armor_type == ITEM_ARMOR_BASIC_KIT) {
        cost = COST_BASIC_ARMOR;
        durability = AMOR_VAL_BASIC;
        type_name = "Co ban";
    } else if (armor_type == ITEM_ARMOR_HEAVY_KIT) {
        cost = COST_HEAVY_ARMOR;
        durability = AMOR_VAL_HEAVY;
        type_name = "Nang";
    } else {
        send_response(client_fd, RES_SHOP_SUCCESS, "Loai giap khong ton tai", NULL);
        return;
    }

    if (player->ship.coin < cost) {
        send_response(client_fd, RES_NOT_ENOUGH_COIN, "Khong du tien", NULL);
        return;
    }

    // Đếm layer giáp
    int armor_count = 0;
    for (int i = 0; i < 2; i++) {
        if (player->ship.armor[i].type != ARMOR_NONE) {
            armor_count++;
        }
    }

    if (armor_count >= 2) {
        send_response(client_fd, RES_NOT_ENOUGH_SLOTS, "Toi da 2 lop giap", NULL);
        return;
    }

    player->ship.coin -= cost;

    // Thêm giáp
    for (int i = 0; i < 2; i++) {
        if (player->ship.armor[i].type == ARMOR_NONE) {
            player->ship.armor[i].type = (armor_type == ITEM_ARMOR_BASIC_KIT) ? ARMOR_BASIC : ARMOR_HEAVY;
            player->ship.armor[i].current_durability = durability;
            break;
        }
    }

    update_player_to_file(player);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "coin", player->ship.coin);
    cJSON_AddStringToObject(data, "armor_type", type_name);

    log_action("SUCCESS", "BUY_ARMOR", player->username, "Mua giap");
    send_response(client_fd, RES_SHOP_SUCCESS, "Mua giap thanh cong", data);
}

// void handle_fix_ship(int client_fd, cJSON *payload) {
//     Player *player = find_player_by_socket(client_fd);
//     if (!player) {
//         send_response(client_fd, RES_NOT_LOGGED_IN, "Chua dang nhap", NULL);
//         return;
//     }
//
//     int max_hp = 1000;
//     int hp_needed = max_hp - player->ship.hp;
//
//     if (hp_needed <= 0) {
//         send_response(client_fd, RES_HP_IS_FULL, "HP da day", NULL);
//         return;
//     }
//
//     int repair_cost = hp_needed * COST_REPAIR_PER_HP;
//
//     if (player->ship.coin < repair_cost) {
//         send_response(client_fd, RES_NOT_ENOUGH_COIN, "Khong du tien sua", NULL);
//         return;
//     }
//
//     player->ship.coin -= repair_cost;
//     player->ship.hp = max_hp;
//
//     update_player_to_file(player);
//
//     cJSON *data = cJSON_CreateObject();
//     cJSON_AddNumberToObject(data, "hp", player->ship.hp);
//     cJSON_AddNumberToObject(data, "coin", player->ship.coin);
//     cJSON_AddNumberToObject(data, "repair_cost", repair_cost);
//
//     log_action("SUCCESS", "FIX_SHIP", player->username, "Sua tau");
//     send_response(client_fd, RES_SHOP_SUCCESS, "Sua tau thanh cong", data);
// }

void handle_buy_item(int client_fd, cJSON *payload) {
    cJSON *item_type_node = cJSON_GetObjectItem(payload, "item_type");
    if (!item_type_node) {
        send_response(client_fd, RES_SHOP_SUCCESS, "Thieu item_type", NULL);
        return;
    }

    int item_type = item_type_node->valueint;

    switch (item_type) {
        case ITEM_AMMO_30MM:
            handle_buy_ammo(client_fd, payload);
            break;
        case ITEM_WEAPON_LASER_GUN:
            handle_buy_laser(client_fd, payload);
            break;
        case ITEM_LASER_BATTERY:
            handle_buy_laser_battery(client_fd, payload);
            break;
        case ITEM_WEAPON_MISSILE:
            handle_buy_missile(client_fd, payload);
            break;
        case ITEM_ARMOR_BASIC_KIT:
        case ITEM_ARMOR_HEAVY_KIT:
            handle_buy_armor(client_fd, payload);
            break;
        default:
            send_response(client_fd, RES_SHOP_SUCCESS, "Loai item khong ton tai", NULL);
            break;
    }
}
