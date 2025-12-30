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
#include "../Server/handlers/shop/client_shop.c"

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

cJSON *treasure_response_data = NULL;
volatile int waiting_for_result = 0;

volatile int end_game_flag = 0;     // 0: Bình thường, 1: Có kết quả trận đấu
volatile int last_match_result = 0; // 0: Thua, 1: Thắng
char last_winner_name[50] = "Unknown";

pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sync_cond = PTHREAD_COND_INITIALIZER;
cJSON *sync_response = NULL;

void do_attack();
void do_challenge();
void do_accept();
void do_treasure_hunt();
void do_mock_equip();
void show_game_result_screen();
void *background_listener(void *arg);

void lock_ui()
{
    pthread_mutex_lock(&ui_mutex);
    ui_locked = 1;
    pthread_mutex_unlock(&ui_mutex);
}

void unlock_ui()
{
    pthread_mutex_lock(&ui_mutex);
    ui_locked = 0;
    pthread_mutex_unlock(&ui_mutex);
}

int is_ui_locked()
{
    pthread_mutex_lock(&ui_mutex);
    int locked = ui_locked;
    pthread_mutex_unlock(&ui_mutex);
    return locked;
}

// cJSON *wait_for_response()
// {
//     while (1)
//     {
//         cJSON *response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);
//         if (response)
//             return response;

//         char dummy;
//         int check = recv(sock, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
//         if (check == 0)
//         {
//             endwin();
//             printf("\n[ERROR] Server disconnected unexpectedly!\n");
//             close(sock);
//             exit(1);
//         }
//     }
// }

cJSON *wait_for_response()
{
    pthread_mutex_lock(&sync_mutex);

    waiting_for_result = 1; // Bật cờ báo "Tôi đang chờ"

    // Nếu có dữ liệu cũ chưa xóa, xóa đi
    if (sync_response)
    {
        cJSON_Delete(sync_response);
        sync_response = NULL;
    }

    // Vòng lặp chờ tín hiệu (Condition Variable)
    // Nó sẽ NHẢ mutex ra và ngủ. Khi Listener gọi signal(), nó tỉnh dậy và LẤY lại mutex.
    while (sync_response == NULL)
    {
        pthread_cond_wait(&sync_cond, &sync_mutex);
    }

    cJSON *res = sync_response;
    sync_response = NULL;   // Reset để lần sau dùng
    waiting_for_result = 0; // Tắt cờ

    pthread_mutex_unlock(&sync_mutex);
    return res;
}

void show_player_status()
{
    printf("\n========== TRANG THAI HIEN TAI ==========\n");
    printf("Coins: %d\n", current_coins);
    printf("HP: %d/1000\n", current_hp);
}

// Hàm xác nhận mua hàng (UI Ncurses)
int confirm_purchase(const char *item_name, int cost)
{
    // 1. Tắt chế độ chờ (timeout) để hàm này hoạt động chặn (blocking)
    // Người dùng bắt buộc phải chọn xong mới làm việc khác
    timeout(-1);

    int height, width;
    getmaxyx(stdscr, height, width); // Lấy kích thước màn hình

    // --- TRƯỜNG HỢP 1: KHÔNG ĐỦ TIỀN ---
    if (current_coins < cost)
    {
        clear();
        box(stdscr, 0, 0); // Vẽ khung

        // Tiêu đề cảnh báo
        attron(A_BOLD | COLOR_PAIR(1)); // Màu đỏ (Giả sử pair 1 là Red)
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
        getch();  // Chờ bấm phím bất kỳ
        return 0; // Trả về False
    }

    // --- TRƯỜNG HỢP 2: ĐỦ TIỀN -> HỎI XÁC NHẬN ---
    int choice = 0; // 0 = YES, 1 = NO
    int key;

    while (1)
    {
        clear();
        box(stdscr, 0, 0); // Vẽ khung

        // Tiêu đề
        attron(A_BOLD | COLOR_PAIR(2)); // Màu xanh (Giả sử pair 2 là Green)
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
            attron(A_REVERSE | A_BOLD); // Highlight nếu đang chọn
        mvprintw(btn_y, width / 2 - 10, "[ YES ]");
        if (choice == 0)
            attroff(A_REVERSE | A_BOLD);

        // Vẽ nút NO
        if (choice == 1)
            attron(A_REVERSE | A_BOLD); // Highlight nếu đang chọn
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
        switch (key)
        {
        case KEY_LEFT:
        case KEY_RIGHT:
            choice = !choice; // Đảo lựa chọn giữa 0 và 1
            break;

        case 10: // Phím Enter
        case 13:
            return (choice == 0); // Nếu chọn YES (0) -> Trả về 1 (True), ngược lại 0

        case 27: // Phím ESC -> Hủy luôn
        case 'n':
        case 'N':
            return 0;

        case 'y':
        case 'Y':
            return 1;
        }
    }
}

void do_register()
{
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
    if (res)
    {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg)
        {
            if (status->valueint == RES_AUTH_SUCCESS)
            {
                display_response_message(8, 10, 2, status->valueint, msg->valuestring);
            }
            else
            {
                display_response_message(8, 10, 1, status->valueint, msg->valuestring);
            }
        }
        cJSON_Delete(res);
    }

    unlock_ui();
    mvprintw(10, 10, "Press any key to continue...");
    getch();
}

void do_login()
{
    clear();
    if (current_user_id != 0)
    {
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
    if (res)
    {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");

        if (status && msg)
        {
            if (status->valueint == RES_AUTH_SUCCESS)
            {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data)
                {
                    current_user_id = cJSON_GetObjectItem(res_data, "id")->valueint;

                    cJSON *hp = cJSON_GetObjectItem(res_data, "hp");
                    cJSON *coin = cJSON_GetObjectItem(res_data, "coin");

                    if (hp)
                        current_hp = hp->valueint;
                    if (coin)
                        current_coins = coin->valueint;

                    printf("\t>> Login success! User ID: %d\n", current_user_id);
                    printf(">> HP: %d | Coins: %d\n", current_hp, current_coins);

                    // Start listener thread
                    should_exit = 0;
                    display_response_message(8, 10, 2, status->valueint, msg->valuestring);
                }
            }
            else
            {
                display_response_message(8, 10, 1, status->valueint, msg->valuestring);
            }
        }
        cJSON_Delete(res);
    }

    unlock_ui();
    mvprintw(10, 10, "Press any key to continue...");
    getch();
}

void do_logout()
{
    send_json(sock, ACT_LOGOUT, NULL);
    cJSON *res = wait_for_response();
    if (res)
    {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        current_user_id = 0;
        cJSON_Delete(res);
    }
}

void do_list_teams()
{
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

    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_TEAM_SUCCESS)
        {
            // --- HIỂN THỊ DẠNG BẢNG ---
            attron(A_BOLD);
            // In Header bảng: ID (rộng 5), Name (rộng 20), Slots (rộng 10)
            mvprintw(4, 5, "%-5s %-30s %-10s", "ID", "TEAM NAME", "MEMBERS");
            attroff(A_BOLD);
            mvhline(5, 5, ACS_HLINE, 50); // Vẽ đường kẻ ngang

            if (cJSON_IsArray(data))
            {
                int row = 6;
                cJSON *team;
                cJSON_ArrayForEach(team, data)
                {
                    int id = cJSON_GetObjectItem(team, "id")->valueint;
                    char *name = cJSON_GetObjectItem(team, "name")->valuestring;
                    int slots = cJSON_GetObjectItem(team, "slots")->valueint;

                    mvprintw(row++, 5, "%-5d %-30s %d/3", id, name, slots);
                }

                if (row == 6)
                {
                    mvprintw(6, 5, "No teams found.");
                }
            }
        }
        else
        {
            // Có lỗi (in màu đỏ)
            display_response_message(4, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Error");
        }
        cJSON_Delete(res);
    }
    else
    {
        mvprintw(4, 5, "Error: No response from server.");
    }

    // Dừng màn hình
    attron(A_DIM);
    mvprintw(20, 5, "Press any key to return...");
    attroff(A_DIM);
    getch();
}

void do_create_team()
{
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

    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");

        if (status && msg)
        {
            // RES_TEAM_SUCCESS là 200 (Định nghĩa trong protocol.h)
            if (status->valueint == RES_TEAM_SUCCESS)
            {
                // THÀNH CÔNG: In màu xanh (Color Pair 2)
                display_response_message(6, 5, 2, status->valueint, msg->valuestring);

                // Cập nhật state nếu cần (ví dụ login thành công thì update ID)
            }
            else
            {
                // THẤT BẠI: In màu đỏ (Color Pair 1)
                // Đây chính là chỗ hiển thị lỗi server gửi về
                display_response_message(6, 5, 1, status->valueint, msg->valuestring);
            }
        }
        cJSON_Delete(res);
    }
    else
    {
        // Trường hợp không nhận được JSON hoặc lỗi mạng
        mvprintw(6, 5, ">> Error: No response from server!");
    }

    // 3. Dừng màn hình để đọc lỗi
    mvprintw(8, 5, "Press any key to return...");
    getch();
}

void do_list_members()
{
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

    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_TEAM_SUCCESS)
        {
            // Hiển thị tên team đang xem
            mvprintw(6, 5, "Members of team: [%s]", team_name);

            // --- HIỂN THỊ DẠNG BẢNG ---
            attron(A_BOLD);
            mvprintw(8, 5, "%-5s %-20s %-15s", "ID", "USERNAME", "ROLE");
            attroff(A_BOLD);
            mvhline(9, 5, ACS_HLINE, 45);

            if (cJSON_IsArray(data))
            {
                int row = 10;
                cJSON *member;
                cJSON_ArrayForEach(member, data)
                {
                    int id = cJSON_GetObjectItem(member, "id")->valueint;
                    char *name = cJSON_GetObjectItem(member, "name")->valuestring;
                    int is_cap = cJSON_GetObjectItem(member, "is_captain")->valueint;

                    // In thông tin, nếu là captain thì in màu xanh hoặc đánh dấu sao
                    if (is_cap)
                    {
                        attron(COLOR_PAIR(2)); // Màu xanh cho captain
                        mvprintw(row, 5, "%-5d %-20s %-15s", id, name, "CAPTAIN");
                        attroff(COLOR_PAIR(2));
                    }
                    else
                    {
                        mvprintw(row, 5, "%-5d %-20s %-15s", id, name, "MEMBER");
                    }
                    row++;
                }
            }
        }
        else
        {
            // Lỗi (VD: Team không tồn tại) - In màu đỏ
            display_response_message(6, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Error");
        }
        cJSON_Delete(res);
    }
    else
    {
        mvprintw(6, 5, "Error: No response from server.");
    }

    attron(A_DIM);
    mvprintw(20, 5, "Press any key to return...");
    attroff(A_DIM);
    getch();
}

void do_req_join()
{
    clear();
    char name[50];
    get_input(4, 5, "Team name to join: ", name, 50, 0);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "team_name", name);

    send_json(sock, ACT_REQ_JOIN, data);

    cJSON *res = wait_for_response();
    if (res)
    {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}

void do_approve_req(int approve)
{
    clear();
    char username[50];
    get_input(4, 5, "Target username: ", username, 50, 0);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "target_username", username);

    send_json(sock,
              approve ? ACT_APPROVE_REQ : ACT_REFUSE_REQ,
              data);

    cJSON *res = wait_for_response();
    if (res)
    {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}

void do_leave_team()
{
    clear();
    send_json(sock, ACT_LEAVE_TEAM, NULL);
    cJSON *res = wait_for_response();
    if (res)
    {
        printf(">> %s\n", cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}

void do_kick_member()
{
    char name[50];
    get_input(4, 5, "Username to kick: ", name, 50, 0);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "target_username", name);

    send_json(sock, ACT_KICK_MEMBER, data);

    cJSON *res = wait_for_response();
    if (res)
    {
        printf(">> %s\n",
               cJSON_GetObjectItem(res, "message")->valuestring);
        cJSON_Delete(res);
    }
}

void print_menu(int highlight)
{
    const char *choices_guest[] = {
        "1. Register",
        "2. Login",
        "3. Exit"};

    const char *choices_user[] = {
        "1. Logout",
        "2. Create Team",
        "3. List Teams",
        "4. Join Team",
        "5. Exit"

    };
    int n_choices;
    char **current_choices;

    erase();
    mvprintw(1, 10, "=== SPACE BATTLE ONLINE ===");
    if (current_user_id != 0)
    {
        attron(COLOR_PAIR(2));
        mvprintw(2, 10, "Logged in as User ID: %d", current_user_id);
        mvprintw(3, 10, "Coins: %d | HP: %d", current_coins, current_hp);
        attroff(COLOR_PAIR(2));

        current_choices = (char **)choices_user;
        n_choices = sizeof(choices_user) / sizeof(choices_user[0]);
    }
    else
    {
        mvprintw(2, 10, "Status: Guest (Not Logged In)");
        current_choices = (char **)choices_guest;
        n_choices = sizeof(choices_guest) / sizeof(choices_guest[0]);
    }

    for (int i = 0; i < n_choices; i++)
    {
        refresh();
        if (highlight == i)
        {
            attron(A_REVERSE);
            mvprintw(5 + i, 10, "-> %s", current_choices[i]);
            attroff(A_REVERSE);
        }
        else
        {
            mvprintw(5 + i, 10, "   %s", current_choices[i]);
        }
    }

    mvprintw(5 + n_choices + 2, 10, "Use arrow keys to move, Enter to select.");
    refresh();
}

// Hàm tiện ích để vẽ menu và trả về lựa chọn của người dùng
int draw_menu(const char *title, const char *options[], int n_opts)
{
    clear();
    int highlight = 0;
    int c;
    int height, width;

    // Cài đặt timeout: getch() sẽ chờ 100ms.
    // Nếu không có phím nào được ấn, nó trả về ERR nhưng vòng lặp vẫn chạy tiếp -> UI được vẽ lại.
    timeout(100);

    while (1)
    {
        // --- VẼ UI ---
        // (Xóa màn hình và vẽ lại toàn bộ khung, bao gồm cả HP/Coin mới nhất)
        erase();
        getmaxyx(stdscr, height, width);
        box(stdscr, 0, 0);

        // 1. Tiêu đề
        attron(A_BOLD | COLOR_PAIR(2));
        mvprintw(2, (width - strlen(title)) / 2, "%s", title);
        attroff(A_BOLD | COLOR_PAIR(2));

        // 2. Trạng thái User (Sẽ tự cập nhật giá trị mới nhất từ biến toàn cục)
        if (current_user_id != 0)
        {
            // Hiển thị HP với màu sắc cảnh báo nếu máu thấp
            mvprintw(4, 4, "User: %d | ", current_user_id);

            if (current_hp < 300)
                attron(COLOR_PAIR(1)); // Màu đỏ nếu máu < 300
            else
                attron(COLOR_PAIR(2));
            printw("HP: %d", current_hp);
            if (current_hp < 300)
                attroff(COLOR_PAIR(1));
            else
                attroff(COLOR_PAIR(2));

            printw(" | Coin: %d", current_coins);
        }
        else
        {
            mvprintw(4, 4, "Status: Guest");
        }

        mvwhline(stdscr, 5, 1, ACS_HLINE, width - 2);

        // 3. Menu Options
        int start_y = 7;
        int start_x = 4;
        for (int i = 0; i < n_opts; i++)
        {
            if (i == highlight)
            {
                attron(A_REVERSE);
                mvprintw(start_y + i, start_x, " -> %s ", options[i]);
                attroff(A_REVERSE);
            }
            else
            {
                mvprintw(start_y + i, start_x, "    %s ", options[i]);
            }
        }

        attron(COLOR_PAIR(1));
        mvprintw(height - 2, 2, "[UP/DOWN]: Move | [ENTER]: Select | [BACKSPACE]: Back");
        attroff(COLOR_PAIR(1));

        refresh(); // Đẩy thay đổi lên màn hình

        // --- XỬ LÝ NHẬP LIỆU ---
        c = getch(); // Hàm này giờ chỉ chờ 100ms

        if (c == ERR)
        {
            // Không có phím nào được ấn (Timeout)
            // Tiếp tục vòng lặp để vẽ lại UI với dữ liệu mới (nếu có)
            continue;
        }

        // Nếu có phím ấn, xử lý như bình thường
        switch (c)
        {
        case KEY_UP:
            highlight = (highlight == 0) ? n_opts - 1 : highlight - 1;
            break;
        case KEY_DOWN:
            highlight = (highlight == n_opts - 1) ? 0 : highlight + 1;
            break;
        case 10:         // Enter
            timeout(-1); // Tắt timeout trước khi return để các hàm nhập liệu khác hoạt động bình thường
            return highlight;
        case KEY_BACKSPACE:
        case 127:
            timeout(-1); // Tắt timeout
            return -1;
        default:
            break;
        }
    }
}

void menu_shop()
{
    clear();
    const char *options[] = {
        "1. Buy Ammo (30mm)",
        "2. Buy Laser Gun",
        "3. Buy Laser Battery",
        "4. Buy Missiles",
        "5. Buy Armor",
        "6. Fix Ship",
        "7. [DEBUG] Get Full Gear (Cheat)", // [THÊM LỰA CHỌN NÀY]
        "8. Back"};
    int n_opts = 8; // Tăng số lượng option lên 8

    while (1)
    {
        int choice = draw_menu("SHOP SYSTEM", options, n_opts);
        if (choice == -1 || choice == 7)
            break; // Choice 7 là Back

        switch (choice)
        {
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
            break; // [GỌI HÀM CHEAT]
        }
    }
}

void menu_team()
{
    clear();
    // Rút gọn text để giao diện gọn hơn
    const char *options[] = {
        "1. List All Teams",
        "2. Create New Team",
        "3. View Team Members",
        "4. Request to Join Team",
        "5. Approve Request (Captain)", // Rút gọn
        "6. Refuse Request (Captain)",  // Rút gọn
        "7. Kick Member (Captain)",     // Rút gọn
        "8. Leave Team",
        "9. Back"};
    int n_opts = 9;

    while (1)
    {
        int choice = draw_menu("TEAM MANAGEMENT", options, n_opts);
        if (choice == -1 || choice == 8)
            break;

        switch (choice)
        {
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

void menu_combat()
{
    clear();
    const char *options[] = {
        "1. Send Challenge",
        "2. Accept Challenge",
        "3. Attack Opponent",
        "4. Back"};
    int n_opts = 4;

    while (1)
    {
        int choice = draw_menu("COMBAT ZONE", options, n_opts);
        if (choice == -1 || choice == 3)
            break;

        switch (choice)
        {
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

int main()
{
    struct sockaddr_in serv_addr;

    memset(client_buffer, 0, BUFFER_SIZE);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    printf("Connected to server %s:%d\n", SERVER_IP, PORT);

    if (pthread_create(&listener_thread, NULL, background_listener, NULL) != 0) {
        perror("Failed to create listener thread");
        return 1;
    }

    // Khởi tạo ncurses
    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK); // [THÊM] Màu vàng cho Kho báu
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // MENU CHÍNH
    while (1)
    {
        if (current_user_id == 0)
        {
            // --- GUEST MENU ---
            const char *options[] = {"1. Register", "2. Login", "3. Exit"};
            int choice = draw_menu("WELCOME GUEST", options, 3);

            if (choice == 2 || choice == -1)
                break; // Exit
            switch (choice)
            {
            case 0:
                do_register();
                break;
            case 1:
                do_login();
                break;
            }
        }
        else
        {
            if (end_game_flag)
            {
                show_game_result_screen();
                // Sau khi xem xong kết quả, continue để vẽ lại dashboard mới
                continue;
            }
            if (current_hp <= 0)
            {
                // Hiển thị màn hình "Đã chết / Đang xem"
                timeout(100); // Non-blocking để cập nhật nếu được hồi sinh
                erase();

                attron(A_BOLD | COLOR_PAIR(1));
                mvprintw(5, 10, "=================================");
                mvprintw(6, 10, "          YOU ARE DEAD!          ");
                mvprintw(7, 10, "=================================");
                attroff(A_BOLD | COLOR_PAIR(1));

                mvprintw(9, 10, "Your HP reached 0.");
                mvprintw(10, 10, "Spectating teammates...");

                // Nút thoát khẩn cấp
                mvprintw(12, 10, "[Q] Quit / Logout");

                refresh();

                int ch = getch();
                if (ch == 'q' || ch == 'Q')
                {
                    do_logout();
                }

                // Nếu server gửi lệnh reset game (hồi máu), vòng lặp sau sẽ tự thoát if này
                continue;
            }

            // --- USER DASHBOARD ---
            const char *options[] = {
                "1. Shop System",
                "2. Team Management",
                "3. Combat Zone",
                "4. Treasure Hunt", // [THÊM]
                "5. Logout"};
            int choice = draw_menu("MAIN DASHBOARD", options, 5);

            switch (choice)
            {
            case 0:
                menu_shop();
                break;
            case 1:
                menu_team();
                break;
            case 2:
                menu_combat();
                break;
            case 3:
                do_treasure_hunt();
                break; // [GỌI HÀM]
            case 4:
                do_logout();
                break;
            case -1:
                do_logout();
                break;
            }
        }
    }

    // Dọn dẹp
    endwin();
    close(sock);
    return 0;
}

void do_challenge()
{
    clear();
    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(2, 5, "=== SEND CHALLENGE ===");
    attroff(A_BOLD | COLOR_PAIR(2));

    char buffer[50];
    int target_id;

    // Nhập ID đội muốn thách đấu
    get_input(4, 5, "Enter Opponent Team ID: ", buffer, 50, 0);
    target_id = atoi(buffer);

    if (target_id <= 0)
    {
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
    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");

        if (status)
        {
            // RES_BATTLE_SUCCESS = 400
            if (status->valueint == RES_BATTLE_SUCCESS)
            {
                display_response_message(6, 5, 2, status->valueint, msg->valuestring);
            }
            else
            {
                display_response_message(6, 5, 1, status->valueint, msg->valuestring);
            }
        }
        cJSON_Delete(res);
    }
    else
    {
        mvprintw(6, 5, "Error: No response from server.");
    }

    attron(A_DIM);
    mvprintw(10, 5, "Press any key to return...");
    attroff(A_DIM);
    getch();
}

// Hàm chấp nhận thách đấu
void do_accept()
{
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

    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_BATTLE_SUCCESS)
        {
            // Hiển thị thông tin trận đấu bắt đầu
            attron(A_BOLD);
            display_response_message(4, 5, 2, 400, "BATTLE STARTED!");
            attroff(A_BOLD);

            if (data)
            {
                cJSON *opp_name = cJSON_GetObjectItem(data, "opponent_name");
                cJSON *match_id = cJSON_GetObjectItem(data, "match_id");

                if (opp_name)
                    mvprintw(6, 5, "OPPONENT: %s", opp_name->valuestring);
                if (match_id)
                    mvprintw(7, 5, "MATCH ID: %d", match_id->valueint);
            }
        }
        else
        {
            display_response_message(4, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Error");
        }
        cJSON_Delete(res);
    }
    else
    {
        mvprintw(4, 5, "Error: No response from server.");
    }

    attron(A_DIM);
    mvprintw(10, 5, "Press any key to return...");
    attroff(A_DIM);
    getch();
}

void do_attack()
{
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
    if (wp_slot < 0 || wp_slot > 3)
    {
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
    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *d = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_BATTLE_SUCCESS)
        {
            // TẤN CÔNG THÀNH CÔNG -> HIỆN BẢNG KẾT QUẢ
            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(9, 5, ">> ATTACK SUCCESSFUL!");
            attroff(COLOR_PAIR(2) | A_BOLD);

            if (d)
            {
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
        }
        else
        {
            // LỖI (Hết đạn, sai mục tiêu...)
            display_response_message(9, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Failed");
        }
        cJSON_Delete(res);
    }
    else
    {
        mvprintw(9, 5, "Error: No response.");
    }

    attron(A_DIM);
    mvprintw(18, 5, "Press any key to continue...");
    attroff(A_DIM);
    getch();
}

void do_treasure_hunt()
{
    // Cài đặt timeout cho getch để nó không chặn mãi mãi
    // Điều này giúp vòng lặp chạy liên tục để kiểm tra kho báu mới
    timeout(100);

    while (1)
    {
        // 1. Kiểm tra xem có dữ liệu kho báu mới từ thread nền không
        pthread_mutex_lock(&pending_mutex);
        int has_treasure = pending_treasure.has_pending;
        pthread_mutex_unlock(&pending_mutex);

        // NẾU CÓ KHO BÁU -> HIỂN THỊ CÂU HỎI NGAY
        if (has_treasure)
        {
            timeout(-1); // Tắt timeout để người dùng suy nghĩ trả lời (chặn chờ phím)

            // --- Lấy dữ liệu an toàn ---
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

            // --- Vẽ Menu câu hỏi ---
            char title[512];
            snprintf(title, sizeof(title), "CHEST: %s ($%d) | %s", type_str, reward, question);

            // Gọi hàm draw_menu để hiện câu hỏi
            int choice = draw_menu(title, (const char **)options, 5);

            // Dọn dẹp
            for (int i = 0; i < 4; i++)
                free(options[i]);

            // Xử lý lựa chọn
            if (choice == -1 || choice == 4)
            {
                // Người dùng chọn Skip, thoát ra
                timeout(100); // Bật lại timeout cho lần lặp sau
                return;
            }

            // Gửi đáp án
            clear();
            mvprintw(5, 5, "Sending answer...");
            refresh();

            cJSON *data = cJSON_CreateObject();
            cJSON_AddNumberToObject(data, "treasure_id", t_id);
            cJSON_AddNumberToObject(data, "answer", choice);
            send_json(sock, ACT_ANSWER, data);

            // [THÊM] Bật cờ báo hiệu tôi đang chờ kết quả
            waiting_for_result = 1;
            if (treasure_response_data)
            { // Xóa dữ liệu cũ nếu có
                cJSON_Delete(treasure_response_data);
                treasure_response_data = NULL;
            }

            send_json(sock, ACT_ANSWER, data);

            // Tắt cờ pending để tránh hiện lại câu hỏi cũ
            pthread_mutex_lock(&pending_mutex);
            pending_treasure.has_pending = 0;
            pthread_mutex_unlock(&pending_mutex);

            // Chờ kết quả
            int timeout_counter = 0;
            while (waiting_for_result && timeout_counter < 50)
            {                   // Chờ tối đa 5 giây (50 * 100ms)
                usleep(100000); // Ngủ 100ms
                timeout_counter++;
            }

            // Hết vòng lặp, kiểm tra xem có dữ liệu không
            cJSON *res = treasure_response_data; // Lấy dữ liệu từ listener
            treasure_response_data = NULL;       // Reset pointer
            waiting_for_result = 0;              // Reset cờ

            if (res)
            {
                // ... (Phần hiển thị kết quả GIỮ NGUYÊN như cũ) ...
                cJSON *status = cJSON_GetObjectItem(res, "status");
                cJSON *msg = cJSON_GetObjectItem(res, "message");
                cJSON *d = cJSON_GetObjectItem(res, "data");

                clear();
                if (status && status->valueint == RES_TREASURE_SUCCESS)
                {
                    // (Code in chúc mừng...)
                    attron(A_BOLD | COLOR_PAIR(3));
                    mvprintw(5, 5, " $$$ CONGRATULATIONS! $$$ ");
                    attroff(A_BOLD | COLOR_PAIR(3));
                    mvprintw(7, 5, ">> %s", msg ? msg->valuestring : "");
                    if (d)
                    {
                        // Update coin
                        cJSON *total = cJSON_GetObjectItem(d, "total_coins");
                        if (total)
                            current_coins = total->valueint;
                        // ...
                    }
                }
                else
                {
                    // (Code in lỗi...)
                    display_response_message(5, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Failed");
                }
                cJSON_Delete(res);
            }
            else
            {
                mvprintw(5, 5, "Error: Timeout waiting for server.");
            }

            mvprintw(12, 5, "Press any key to return...");
            timeout(-1); // Chặn chờ phím
            getch();
            return; // Xong một lượt thì thoát ra menu chính
        }

        // NẾU KHÔNG CÓ KHO BÁU -> HIỆN MÀN HÌNH CHỜ
        else
        {
            erase();           // Dùng erase thay vì clear để đỡ nháy màn hình
            box(stdscr, 0, 0); // Vẽ khung

            attron(A_BOLD | COLOR_PAIR(3));
            mvprintw(2, 5, "=== TREASURE HUNT ZONE ===");
            attroff(A_BOLD | COLOR_PAIR(3));

            mvprintw(5, 5, ">> Scanning for treasure signals...");

            // Tạo hiệu ứng loading đơn giản
            static int loading = 0;
            loading = (loading + 1) % 4;
            mvprintw(6, 5, "   Waiting%s   ", (loading == 0) ? ".  " : (loading == 1) ? ".. "
                                                                   : (loading == 2)   ? "..."
                                                                                      : "   ");

            mvprintw(8, 5, "[BACKSPACE] Return to Dashboard");
            refresh();

            int ch = getch(); // Hàm này giờ chỉ chờ 100ms
            if (ch == KEY_BACKSPACE || ch == 127)
            {
                timeout(-1); // Reset lại chế độ chuẩn trước khi thoát
                return;
            }
        }
    }
}

void do_mock_equip()
{
    if (!confirm_purchase("Goi CHEAT (Free)", 0))
        return;

    send_json(sock, ACT_MOCK_EQUIP, NULL);
    cJSON *res = wait_for_response();

    if (res)
    {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status && status->valueint == RES_SHOP_SUCCESS)
        {
            // Hiển thị màu xanh lá cây
            display_response_message(4, 5, 2, status->valueint, msg ? msg->valuestring : "Success");

            // Cập nhật lại hiển thị tiền/hp ngay lập tức
            if (data)
            {
                cJSON *coin = cJSON_GetObjectItem(data, "coin");
                cJSON *hp = cJSON_GetObjectItem(data, "hp");
                if (coin)
                    current_coins = coin->valueint;
                if (hp)
                    current_hp = hp->valueint;
            }
        }
        else
        {
            display_response_message(4, 5, 1, status ? status->valueint : 0, "Failed");
        }
        cJSON_Delete(res);
    }
    // Dừng màn hình để xem kết quả
    mvprintw(12, 5, "Press any key...");
    getch();
}

void show_game_result_screen()
{
    // Tắt chế độ non-blocking để chờ người dùng ấn phím
    timeout(-1);

    clear();
    box(stdscr, 0, 0); // Vẽ khung viền

    int height, width;
    getmaxyx(stdscr, height, width);

    if (last_match_result == 1)
    {
        // --- MÀN HÌNH CHIẾN THẮNG (Màu Xanh) ---
        attron(A_BOLD | COLOR_PAIR(2));
        mvprintw(height / 2 - 4, (width - 20) / 2, "**********************");
        mvprintw(height / 2 - 3, (width - 20) / 2, "* VICTORY!      *");
        mvprintw(height / 2 - 2, (width - 20) / 2, "**********************");
        attroff(A_BOLD | COLOR_PAIR(2));

        mvprintw(height / 2, (width - 40) / 2, "Congratulations! Your team has won.");
    }
    else
    {
        // --- MÀN HÌNH THẤT BẠI (Màu Đỏ) ---
        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw(height / 2 - 4, (width - 20) / 2, "######################");
        mvprintw(height / 2 - 3, (width - 20) / 2, "#       DEFEAT       #");
        mvprintw(height / 2 - 2, (width - 20) / 2, "######################");
        attroff(A_BOLD | COLOR_PAIR(1));

        mvprintw(height / 2, (width - 40) / 2, "Your team was eliminated.");
        mvprintw(height / 2 + 1, (width - 40) / 2, "Winner: %s", last_winner_name);
    }

    attron(A_DIM);
    mvprintw(height - 4, (width - 30) / 2, "Your ship has been repaired.");
    mvprintw(height - 3, (width - 30) / 2, "Press [ENTER] to return to Lobby...");
    attroff(A_DIM);

    refresh();

    // Chờ người dùng ấn Enter để thoát
    while (1)
    {
        int ch = getch();
        if (ch == 10 || ch == 13)
            break; // Enter key
    }

    // Reset cờ sau khi đã xem xong
    end_game_flag = 0;
}

// ============================================================
// HÀM BACKGROUND LISTENER HOÀN CHỈNH
// ============================================================
void *background_listener(void *arg)
{
    while (!should_exit)
    {
        // 1. Dùng Blocking Mode (Không dùng usleep).
        // Hàm này sẽ TREO ở đây cho đến khi Server gửi gì đó.
        cJSON *response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);
        if (response == NULL)
        {
            // Nếu NULL thường là do mất kết nối hoặc lỗi mạng nghiêm trọng
            printf("\n[ERROR] Server disconnected!\n");
            should_exit = 1;
            break;
        }

        cJSON *status = cJSON_GetObjectItem(response, "status");
        cJSON *data = cJSON_GetObjectItem(response, "data");
        cJSON *action = cJSON_GetObjectItem(response, "action"); // Nên check thêm action

        int code = status ? status->valueint : 0;
        int act_code = action ? action->valueint : 0;

        // --- PHÂN LOẠI GÓI TIN ---

        // A. GÓI TIN MÀ MAIN THREAD ĐANG CHỜ (LOGIN, MUA ĐỒ, TẤN CÔNG...)
        // Kiểm tra xem Main Thread có đang đợi không (biến waiting_for_result)
        // VÀ gói tin này KHÔNG PHẢI là thông báo bất đồng bộ (như kho báu/end game)
        int is_sync_msg = 0;

        pthread_mutex_lock(&sync_mutex); // Dùng mutex riêng cho việc đồng bộ
        if (waiting_for_result &&
            act_code != ACT_TREASURE_APPEAR &&
            code != RES_END_GAME &&
            code != RES_TREASURE_SUCCESS)
        {
            // Đây là câu trả lời Main Thread đang cần
            if (sync_response)
                cJSON_Delete(sync_response);
            sync_response = cJSON_Duplicate(response, 1); // Copy dữ liệu

            waiting_for_result = 0;          // Tắt cờ chờ
            pthread_cond_signal(&sync_cond); // <--- ĐÁNH THỨC MAIN THREAD NGAY LẬP TỨC
            is_sync_msg = 1;
        }
        pthread_mutex_unlock(&sync_mutex);

        if (is_sync_msg)
        {
            cJSON_Delete(response);
            continue; // Đã xử lý xong, quay lại vòng lặp
        }

        // B. XỬ LÝ CÁC GÓI TIN BẤT ĐỒNG BỘ (ASYNC EVENTS)

        // 1. End Game
        if (code == RES_END_GAME)
        {
            pthread_mutex_lock(&pending_mutex);
            end_game_flag = 1;
            if (data)
            {
                cJSON *winner_flag = cJSON_GetObjectItem(data, "is_winner");
                cJSON *w_name = cJSON_GetObjectItem(data, "winner_team_name");
                if (winner_flag)
                    last_match_result = winner_flag->valueint;
                if (w_name)
                    strncpy(last_winner_name, w_name->valuestring, 49);
            }
            // Update UI State an toàn
            pthread_mutex_lock(&ui_mutex); // Giả sử bạn có ui_mutex
            current_hp = 1000;
            pthread_mutex_unlock(&ui_mutex);

            pthread_mutex_unlock(&pending_mutex);
        }

        // 2. Kho báu xuất hiện
        else if (code == RES_TREASURE_SUCCESS && cJSON_GetObjectItem(data, "question"))
        {
            pthread_mutex_lock(&pending_mutex);
            pending_treasure.has_pending = 1;

            cJSON *t_id = cJSON_GetObjectItem(data, "treasure_id");
            cJSON *ques = cJSON_GetObjectItem(data, "question");
            // ... (Code parse kho báu của bạn giữ nguyên) ...
            if (t_id)
                pending_treasure.treasure_id = t_id->valueint;
            if (ques)
                strncpy(pending_treasure.question, ques->valuestring, 255);
            // ...
            pthread_mutex_unlock(&pending_mutex);
        }

        // 3. Kho báu bị mở mất
        else if (code == RES_TREASURE_OPENED)
        {
            pthread_mutex_lock(&pending_mutex);
            pending_treasure.has_pending = 0;
            pthread_mutex_unlock(&pending_mutex);
        }

        // 4. Cập nhật HP/Coin thụ động (Real-time update)
        // (Xảy ra khi bắn trúng, bị bắn, hoặc mở rương thành công)
        if (data)
        {
            cJSON *hp_node = cJSON_GetObjectItem(data, "current_hp");
            cJSON *coin_node = cJSON_GetObjectItem(data, "current_coin"); // Server thường trả về "coin" hoặc "current_coin"
            cJSON *total_coin = cJSON_GetObjectItem(data, "total_coins");

            // Dùng UI Mutex để bảo vệ biến toàn cục
            if (hp_node || coin_node || total_coin)
            {
                pthread_mutex_lock(&ui_mutex);
                if (hp_node)
                    current_hp = hp_node->valueint;
                if (coin_node)
                    current_coins = coin_node->valueint;
                if (total_coin)
                    current_coins = total_coin->valueint;
                pthread_mutex_unlock(&ui_mutex);
            }
        }

        cJSON_Delete(response);
        // KHÔNG CÓ usleep() Ở ĐÂY
    }
    return NULL;
}