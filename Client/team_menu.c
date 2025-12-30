#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ncurses.h>
#include <pthread.h>

#include "main_menu.c"
#include "../Common/protocol.h"
#include "../Lib/cJSON.h"
#include "Utils/utils.h"
#include "../Server/handlers/shop/client_state.h"
#include "../Server/handlers/shop/client_treasure.c"
#include "../Server/handlers/shop/client_shop.c"
#include "services/storage/storage.h"


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
            if (status->valueint == RES_AUTH_SUCCESS) {
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
    clear();
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

                    strcpy(current_username, username);

                    cJSON *hp = cJSON_GetObjectItem(res_data, "hp");
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");

                    if (hp)
                        current_hp = hp->valueint;
                    if (coin)
                        current_coins = coin->valueint;

                    // Start listener thread
                    should_exit = 0;
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

    flushinp();

    timeout(-1);
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
    clear(); // Xóa màn hình cũ

    // Tiêu đề
    attron(A_BOLD | COLOR_PAIR(2)); // Chữ đậm, màu xanh
    mvprintw(2, 5, "=== LIST OF TEAMS ===");
    attroff(A_BOLD | COLOR_PAIR(2));

    mvprintw(3, 5, "Fetching data...");
    refresh();

    // 1. Gửi lệnh
    send_json(sock, ACT_LIST_TEAMS, NULL);

    // 2. Chờ phản hồi
    cJSON *res = wait_for_response();

    // Xóa dòng "Fetching data..." để in kết quả
    move(3, 0);
    clrtoeol();

    if (res) {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_TEAM_SUCCESS) {
            // --- HIỂN THỊ DẠNG BẢNG ---
            attron(A_BOLD);
            // In Header bảng: ID (rộng 5), Name (rộng 20), Slots (rộng 10)
            mvprintw(4, 5, "%-5s %-30s %-10s", "ID", "TEAM NAME", "MEMBERS");
            attroff(A_BOLD);
            mvhline(5, 5, ACS_HLINE, 50); // Vẽ đường kẻ ngang

            if (cJSON_IsArray(data)) {
                int row = 6;
                cJSON *team;
                cJSON_ArrayForEach(team, data) {
                    int id = cJSON_GetObjectItem(team, "id")->valueint;
                    char *name = cJSON_GetObjectItem(team, "name")->valuestring;
                    int slots = cJSON_GetObjectItem(team, "slots")->valueint;

                    mvprintw(row++, 5, "%-5d %-30s %d/3", id, name, slots);
                }

                if (row == 6) {
                    mvprintw(6, 5, "No teams found.");
                }
            }
        } else {
            // Có lỗi (in màu đỏ)
            display_response_message(4, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Error");
        }
        cJSON_Delete(res);
    } else {
        mvprintw(4, 5, "Error: No response from server.");
    }

    // Dừng màn hình
    attron(A_DIM);
    mvprintw(20, 5, "Press any key to return...");
    attroff(A_DIM);
    getch();
}

void do_create_team() {
    clear();
    attron(A_BOLD);
    mvprintw(2, 5, "=== CREATE NEW TEAM ===");
    attroff(A_BOLD);
    refresh();

    char name[50];
    get_input(4, 5, "Enter Team name: ", name, 50, 0);

    // 1. Gửi dữ liệu đi
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "team_name", name);
    send_json(sock, ACT_CREATE_TEAM, data);

    // 2. Chờ phản hồi
    cJSON *res = wait_for_response();

    if (res) {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");

        cJSON *data_obj = cJSON_GetObjectItem(res, "data");

        if (status && msg) {
            // RES_TEAM_SUCCESS là 200 (Định nghĩa trong protocol.h)
            if (status->valueint == RES_TEAM_SUCCESS) {
                // THÀNH CÔNG: In màu xanh (Color Pair 2)
                display_response_message(6, 5, 2, status->valueint, msg->valuestring);
                cJSON *team_id_node = cJSON_GetObjectItem(res, "team_id");

                if (data_obj) {
                    cJSON *tid = cJSON_GetObjectItem(data_obj, "team_id");
                    if (tid) {
                        current_team_id = tid->valueint;
                    }
                }

                // Cập nhật state nếu cần (ví dụ login thành công thì update ID)
            } else {
                // THẤT BẠI: In màu đỏ (Color Pair 1)
                // Đây chính là chỗ hiển thị lỗi server gửi về
                display_response_message(6, 5, 1, status->valueint, msg->valuestring);
            }
        }
        cJSON_Delete(res);
    } else {
        // Trường hợp không nhận được JSON hoặc lỗi mạng
        mvprintw(6, 5, ">> Error: No response from server!");
    }

    // 3. Dừng màn hình để đọc lỗi
    mvprintw(8, 5, "Press any key to return...");
    getch();
}

void do_list_members() {
    clear();

    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 5, "=== VIEW TEAM MEMBERS ===");
    attroff(A_BOLD | COLOR_PAIR(2));

    char team_name[50];
    // Nhập tên team muốn xem
    get_input(4, 5, "Enter Team Name to view: ", team_name, 50, 0);

    // 1. Tạo payload và gửi
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "team_name", team_name);
    send_json(sock, ACT_LIST_MEMBERS, payload);

    // 2. Chờ phản hồi
    cJSON *res = wait_for_response();

    if (res) {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_TEAM_SUCCESS) {
            // Hiển thị tên team đang xem
            mvprintw(6, 5, "Members of team: [%s]", team_name);

            // --- HIỂN THỊ DẠNG BẢNG ---
            attron(A_BOLD);
            mvprintw(8, 5, "%-5s %-20s %-15s", "ID", "USERNAME", "ROLE");
            attroff(A_BOLD);
            mvhline(9, 5, ACS_HLINE, 45);

            if (cJSON_IsArray(data)) {
                int row = 10;
                cJSON *member;
                cJSON_ArrayForEach(member, data) {
                    int id = cJSON_GetObjectItem(member, "id")->valueint;
                    char *name = cJSON_GetObjectItem(member, "name")->valuestring;
                    int is_cap = cJSON_GetObjectItem(member, "is_captain")->valueint;

                    // In thông tin, nếu là captain thì in màu xanh hoặc đánh dấu sao
                    if (is_cap) {
                        attron(COLOR_PAIR(2)); // Màu xanh cho captain
                        mvprintw(row, 5, "%-5d %-20s %-15s", id, name, "CAPTAIN");
                        attroff(COLOR_PAIR(2));
                    } else {
                        mvprintw(row, 5, "%-5d %-20s %-15s", id, name, "MEMBER");
                    }
                    row++;
                }
            }
        } else {
            // Lỗi (VD: Team không tồn tại) - In màu đỏ
            display_response_message(6, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Error");
        }
        cJSON_Delete(res);
    } else {
        mvprintw(6, 5, "Error: No response from server.");
    }

    attron(A_DIM);
    mvprintw(20, 5, "Press any key to return...");
    attroff(A_DIM);
    getch();
}

void do_req_join() {
    clear();
    char name[50];
    get_input(4, 5, "Team name to join: ", name, 50, 0);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "team_name", name);

    send_json(sock, ACT_REQ_JOIN, data);

    cJSON *res = wait_for_response();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");

        display_response_message(8, 5,
                                 (status && status->valueint == RES_TEAM_SUCCESS) ? 2 : 1,
                                 status ? status->valueint : 0,
                                 msg ? msg->valuestring : "Unknown response");

        cJSON_Delete(res);
    }

    mvprintw(12, 5, "Press any key to continue...");
    getch();
}


void do_approve_req(int approve) {
    clear();
    char username[50];
    get_input(4, 5, "Target username: ", username, 50, 0);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "target_username", username);

    send_json(sock,
              approve ? ACT_APPROVE_REQ : ACT_REFUSE_REQ,
              data);

    cJSON *res = wait_for_response();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");

        display_response_message(8, 5,
                                 (status && status->valueint == RES_TEAM_SUCCESS) ? 2 : 1,
                                 status ? status->valueint : 0,
                                 msg ? msg->valuestring : "Unknown");

        cJSON_Delete(res);
    }

    mvprintw(12, 5, "Press any key...");
    Player *p = find_player_by_username(username);
    current_team_id = p->team_id;
    getch();
}


void do_leave_team() {
    clear(); // Xóa màn hình ncurses

    cJSON *data = cJSON_CreateObject();
    send_json(sock, ACT_LEAVE_TEAM, data);

    cJSON *res = wait_for_response();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");

        // Hiển thị message với màu sắc
        display_response_message(
            6, 5,
            (status && status->valueint == RES_TEAM_SUCCESS) ? 2 : 1,
            status ? status->valueint : 0,
            msg ? msg->valuestring : "Unknown"
        );

        cJSON_Delete(res);
    }

    mvprintw(10, 5, "Press any key to continue...");
    getch(); // Đợi người dùng đọc message
}


void do_kick_member() {
    clear();
    char name[50];
    get_input(4, 5, "Username to kick: ", name, 50, 0);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "target_username", name);

    send_json(sock, ACT_KICK_MEMBER, data);

    cJSON *res = wait_for_response();
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");

        display_response_message(6, 5,
                                 (status && status->valueint == RES_TEAM_SUCCESS) ? 2 : 1,
                                 status ? status->valueint : 0,
                                 msg ? msg->valuestring : "Unknown");

        cJSON_Delete(res);
    }

    mvprintw(8, 5, "Press any key to continue...");
    getch();
}








void menu_team() {
    clear();
    // Rút gọn text để giao diện gọn hơn
    const char *options[] = {
        "1. List All Teams",
        "2. Create New Team",
        "3. View Team Members",
        "4. Request to Join Team",
        "5. Approve Request (Captain)", // Rút gọn
        "6. Refuse Request (Captain)", // Rút gọn
        "7. Kick Member (Captain)", // Rút gọn
        "8. Leave Team",
        "9. Back"
    };
    int n_opts = 9;

    while (1) {
        int choice = draw_menu("TEAM MANAGEMENT", options, n_opts);
        if (choice == -1 || choice == 8)
            break;

        switch (choice) {
            case 0:
                do_list_teams();
                break;
            case 1:
                do_create_team();
                break;
            case 2:
                do_list_members();
                break;
            case 3:
                do_req_join();
                break;
            case 4:
                do_approve_req(1);
                break;
            case 5:
                do_approve_req(0);
                break;
            case 6:
                do_kick_member();
                break;
            case 7:
                do_leave_team();
                break;
        }
    }
}