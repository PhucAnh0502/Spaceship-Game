#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ncurses.h>
#include <pthread.h>

#include "main_menu.h"
#include "../Common/protocol.h"
#include "../Lib/cJSON.h"
#include "Utils/utils.h"
#include "../Server/handlers/shop/client_state.h"


// Hàm chấp nhận thách đấu
void do_accept() {
    clear();
    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 5, "=== ACCEPT CHALLENGE ===");
    attroff(A_BOLD | COLOR_PAIR(2));

    mvprintw(4, 5, "Sending accept request...");
    refresh();

    // 1. Gửi lệnh
    send_json(sock, ACT_ACCEPT_CHALLANGE, NULL);

    // 2. Chờ phản hồi
    cJSON *res = wait_for_response();

    move(4, 0);
    clrtoeol(); // Xóa dòng "Sending..."

    if (res) {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_BATTLE_SUCCESS) {
            // Hiển thị thông tin trận đấu bắt đầu
            attron(A_BOLD);
            display_response_message(4, 5, 2, 400, "BATTLE STARTED!");
            attroff(A_BOLD);

            if (data) {
                cJSON *opp_name = cJSON_GetObjectItem(data, "opponent_name");
                cJSON *match_id = cJSON_GetObjectItem(data, "match_id");

                if (opp_name)
                    mvprintw(6, 5, "OPPONENT: %s", opp_name->valuestring);
                if (match_id)
                    mvprintw(7, 5, "MATCH ID: %d", match_id->valueint);
            }
        } else {
            display_response_message(4, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Error");
        }
        cJSON_Delete(res);
    } else {
        mvprintw(4, 5, "Error: No response from server.");
    }

    attron(A_DIM);
    mvprintw(10, 5, "Press any key to return...");
    attroff(A_DIM);
    getch();
}

void do_attack() {
    clear();
    attron(A_BOLD | COLOR_PAIR(1)); // Màu đỏ cho Combat
    mvprintw(2, 5, "=== ATTACK CONTROL ===");
    attroff(A_BOLD | COLOR_PAIR(1));

    char buffer[50];
    int target_uid, wp_type, wp_slot;

    // --- FORM NHẬP LIỆU ---
    // 1. Nhập ID mục tiêu
    get_input(4, 5, "Target User ID: ", buffer, 50, 0);
    target_uid = atoi(buffer);

    // 2. Chọn loại vũ khí
    mvprintw(6, 5, "Weapon Type (1:Cannon, 2:Laser, 3:Missile): ");
    echo();
    getnstr(buffer, 49);
    noecho();
    wp_type = atoi(buffer);

    // 3. Chọn Slot (0-3)
    mvprintw(7, 5, "Slot (0-3): ");
    echo();
    getnstr(buffer, 49);
    noecho();
    wp_slot = atoi(buffer);

    // Validate cơ bản
    if (wp_slot < 0 || wp_slot > 3) {
        mvprintw(9, 5, ">> Invalid Slot! Must be 0-3.");
        getch();
        return;
    }

    // Gửi lệnh
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "target_user_id", target_uid);
    cJSON_AddNumberToObject(data, "weapon_id", wp_type);
    cJSON_AddNumberToObject(data, "weapon_slot", wp_slot);
    send_json(sock, ACT_ATTACK, data);

    // Chờ kết quả
    cJSON *res = wait_for_response();
    if (res) {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *d = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_BATTLE_SUCCESS) {
            // TẤN CÔNG THÀNH CÔNG -> HIỆN BẢNG KẾT QUẢ
            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(9, 5, ">> ATTACK SUCCESSFUL!");
            attroff(COLOR_PAIR(2) | A_BOLD);

            if (d) {
                int dmg = cJSON_GetObjectItem(d, "damage")->valueint;
                int hp = cJSON_GetObjectItem(d, "target_hp")->valueint;
                int armor = cJSON_GetObjectItem(d, "target_armor")->valueint;
                int ammo = cJSON_GetObjectItem(d, "remaining_ammo")->valueint;

                // Vẽ bảng kết quả mini
                mvhline(11, 5, ACS_HLINE, 40);
                mvprintw(12, 5, "Damage Dealt   : %d", dmg);
                mvprintw(13, 5, "Target HP      : %d", hp);
                mvprintw(14, 5, "Target Armor   : %d", armor);
                mvprintw(15, 5, "Ammo Remaining : %d", ammo);
                mvhline(16, 5, ACS_HLINE, 40);
            }
        } else {
            // LỖI (Hết đạn, sai mục tiêu...)
            display_response_message(9, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Failed");
        }
        cJSON_Delete(res);
    } else {
        mvprintw(9, 5, "Error: No response.");
    }

    attron(A_DIM);
    mvprintw(18, 5, "Press any key to continue...");
    attroff(A_DIM);
    getch();
}

void do_challenge() {
    clear();
    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 5, "=== SEND CHALLENGE ===");
    attroff(A_BOLD | COLOR_PAIR(2));

    char buffer[50];
    int target_id;

    // Nhập ID đội muốn thách đấu
    get_input(4, 5, "Enter Opponent Team ID: ", buffer, 50, 0);
    target_id = atoi(buffer);

    if (target_id <= 0) {
        mvprintw(6, 5, ">> Invalid Team ID!");
        mvprintw(8, 5, "Press any key to return...");
        getch();
        return;
    }

    // 1. Gửi lệnh
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "target_team_id", target_id);
    send_json(sock, ACT_SEND_CHALLANGE, data);

    // 2. Chờ phản hồi
    cJSON *res = wait_for_response();
    if (res) {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");

        if (status) {
            // RES_BATTLE_SUCCESS = 400
            if (status->valueint == RES_BATTLE_SUCCESS) {
                display_response_message(6, 5, 2, status->valueint, msg->valuestring);
            } else {
                display_response_message(6, 5, 1, status->valueint, msg->valuestring);
            }
        }
        cJSON_Delete(res);
    } else {
        mvprintw(6, 5, "Error: No response from server.");
    }

    attron(A_DIM);
    mvprintw(10, 5, "Press any key to return...");
    attroff(A_DIM);
    getch();
}

void menu_combat() {
    clear();
    const char *options[] = {
        "1. Send Challenge",
        "2. Accept Challenge",
        "3. Attack Opponent",
        "4. Back"
    };
    int n_opts = 4;

    while (1) {
        int choice = draw_menu("COMBAT ZONE", options, n_opts);
        if (choice == -1 || choice == 3)
            break;

        switch (choice) {
            case 0:
                do_challenge();
                break; // Từ client.c
            case 1:
                do_accept();
                break; // Từ client.c
            case 2:
                do_attack();
                break; // Từ client.c
        }
    }
}