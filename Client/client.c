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

void do_attack();
void do_challenge();
void do_accept();

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



cJSON *wait_for_response()
{
    while (1)
    {
        cJSON *response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);
        if (response)
            return response;

        char dummy;
        int check = recv(sock, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
        if (check == 0)
        {
            endwin();
            printf("\n[ERROR] Server disconnected unexpectedly!\n");
            close(sock);
            exit(1);
        }
    }
}

void show_player_status()
{
    printf("\n========== TRANG THAI HIEN TAI ==========\n");
    printf("Coins: %d\n", current_coins);
    printf("HP: %d/1000\n", current_hp);
}

int confirm_purchase(const char *item_name, int cost)
{
    if (current_coins < cost)
    {
        printf("\n>> Khong du coins! Can %d, hien co %d\n", cost, current_coins);
        return 0;
    }

    printf("\nXac nhan mua %s voi gia %d coins? (y/n): ", item_name, cost);
    fflush(stdout);

    char confirm[10];
    if (fgets(confirm, sizeof(confirm), stdin) == NULL)
        return 0;

    return (confirm[0] == 'y' || confirm[0] == 'Y');
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

                    printf(">> Login success! User ID: %d\n", current_user_id);
                    printf(">> HP: %d | Coins: %d\n", current_hp, current_coins);

                    // Start listener thread
                    should_exit = 0;
                    pthread_create(&listener_thread, NULL, background_listener, NULL);
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
    move(3, 0); clrtoeol(); 

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
                
                if (row == 6) {
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
        "3. Exit"
    };

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
int draw_menu(const char *title, const char *options[], int n_opts) {
    int highlight = 0;
    int c;
    int height, width;

    while (1) {
        clear(); // Xóa sạch màn hình để tránh lỗi hiển thị rác
        getmaxyx(stdscr, height, width); // Lấy kích thước terminal
        box(stdscr, 0, 0); // Vẽ khung viền bao quanh

        // 1. Hiển thị Tiêu đề (Căn giữa)
        attron(A_BOLD | COLOR_PAIR(2));
        mvprintw(2, (width - strlen(title)) / 2, "%s", title);
        attroff(A_BOLD | COLOR_PAIR(2));

        // 2. Hiển thị Trạng thái User (Góc trên bên trái, trong khung)
        if (current_user_id != 0) {
            mvprintw(4, 4, "User: %d | HP: %d | Coin: %d", current_user_id, current_hp, current_coins);
        } else {
            mvprintw(4, 4, "Status: Guest");
        }
        
        // Vẽ đường kẻ ngang phân cách
        mvwhline(stdscr, 5, 1, ACS_HLINE, width - 2); 

        // 3. Hiển thị danh sách lựa chọn
        int start_y = 7; // Bắt đầu in menu từ dòng 7
        int start_x = 4; // Cách lề trái 4 ký tự

        for (int i = 0; i < n_opts; i++) {
            if (i == highlight) {
                attron(A_REVERSE); // Đảo màu cho dòng đang chọn
                mvprintw(start_y + i, start_x, " -> %s ", options[i]);
                attroff(A_REVERSE);
            } else {
                mvprintw(start_y + i, start_x, "    %s ", options[i]);
            }
        }
        
        // 4. Hướng dẫn sử dụng ở đáy
        attron(COLOR_PAIR(1)); // Màu đỏ nhạt (hoặc màu khác tùy bạn chỉnh)
        mvprintw(height - 2, 2, "[UP/DOWN]: Move | [ENTER]: Select | [BACKSPACE]: Back");
        attroff(COLOR_PAIR(1));

        refresh();

        c = getch();
        switch (c) {
            case KEY_UP:
                highlight = (highlight == 0) ? n_opts - 1 : highlight - 1;
                break;
            case KEY_DOWN:
                highlight = (highlight == n_opts - 1) ? 0 : highlight + 1;
                break;
            case 10: // Enter
                return highlight;
            case KEY_BACKSPACE: 
            case 127: 
                return -1;
            default:
                break;
        }
    }
}

void menu_shop() {
    const char *options[] = {
        "1. Buy Ammo (30mm)",
        "2. Buy Laser Gun",
        "3. Buy Laser Battery",
        "4. Buy Missiles",
        "5. Buy Armor",
        "6. Fix Ship",
        "7. Back"
    };
    int n_opts = 7;

    while(1) {
        int choice = draw_menu("SHOP SYSTEM", options, n_opts);
        if (choice == -1 || choice == 6) break;

        switch(choice) {
            case 0: do_buy_ammo(); break;           // Từ client_shop.c
            case 1: do_buy_laser(); break;          // Từ client_shop.c
            case 2: do_buy_laser_battery(); break;  // Từ client_shop.c
            case 3: do_buy_missile(); break;        // Từ client_shop.c
            case 4: do_buy_armor(); break;          // Từ client_shop.c
            case 5: do_fix_ship(); break;           // Từ client_shop.c
        }
    }
}

void menu_team() {
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
        "9. Back"
    };
    int n_opts = 9;

    while(1) {
        int choice = draw_menu("TEAM MANAGEMENT", options, n_opts);
        if (choice == -1 || choice == 8) break;

        switch(choice) {
            case 0: do_list_teams(); break;
            case 1: do_create_team(); break;
            case 2: do_list_members(); break;
            case 3: do_req_join(); break;
            case 4: do_approve_req(1); break;
            case 5: do_approve_req(0); break;
            case 6: do_kick_member(); break;
            case 7: do_leave_team(); break;
        }
    }
}

void menu_combat() {
    const char *options[] = {
        "1. Send Challenge",
        "2. Accept Challenge",
        "3. Attack Opponent",
        "4. Back"
    };
    int n_opts = 4;

    while(1) {
        int choice = draw_menu("COMBAT ZONE", options, n_opts);
        if (choice == -1 || choice == 3) break;

        switch(choice) {
            case 0: do_challenge(); break; // Từ client.c
            case 1: do_accept(); break;    // Từ client.c
            case 2: do_attack(); break;    // Từ client.c
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

   // Khởi tạo ncurses
    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // MENU CHÍNH
    while (1) {
        if (current_user_id == 0) {
            // --- GUEST MENU ---
            const char *options[] = {"1. Register", "2. Login", "3. Exit"};
            int choice = draw_menu("WELCOME GUEST", options, 3);
            
            if (choice == 2 || choice == -1) break; // Exit
            switch(choice) {
                case 0: do_register(); break;
                case 1: do_login(); break;
            }
        } else {
            // --- USER DASHBOARD ---
            const char *options[] = {
                "1. Shop System", 
                "2. Team Management", 
                "3. Combat Zone", 
                "4. Refresh Status",
                "5. Logout"
            };
            int choice = draw_menu("MAIN DASHBOARD", options, 5);

            switch(choice) {
                case 0: menu_shop(); break;
                case 1: menu_team(); break;
                case 2: menu_combat(); break;
                case 3: 
                    // Refresh status (gửi heartbeat hoặc logic khác nếu cần)
                    // Hiện tại chỉ cần redraw menu là sẽ update UI
                    break; 
                case 4: 
                    do_logout(); 
                    break;
                case -1: // Nút backspace ở main menu cũng hỏi logout
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
    int target_id;
    printf("\n--- SEND CHALLENGE ---\n");
    printf("Enter Opponent Team ID: ");

    // Kiểm tra nhập liệu để tránh trôi lệnh
    if (scanf("%d", &target_id) != 1)
    {
        printf("[ERROR] Invalid input!\n");
        while (getchar() != '\n')
            ; // Xóa buffer nếu nhập sai
        return;
    }
    getchar(); // Quan trọng: Xóa ký tự \n thừa trong buffer

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "target_team_id", target_id);

    // Gửi lệnh đi
    send_json(sock, ACT_SEND_CHALLANGE, data);

    // Chờ phản hồi
    cJSON *res = wait_for_response();
    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");

        if (status)
        {
            if (status->valueint == RES_BATTLE_SUCCESS)
            {
                // Thành công: Server báo "Challenge sent..."
                printf("[SUCCESS] %s\n", msg ? msg->valuestring : "Request sent.");
            }
            else
            {
                // Thất bại: Server báo lỗi (VD: Team not found, Team busy...)
                printf("[ERROR] Failed to send challenge: %s (Code: %d)\n",
                       msg ? msg->valuestring : "Unknown error",
                       status->valueint);
            }
        }
        cJSON_Delete(res);
    }
}

// Hàm chấp nhận thách đấu
void do_accept()
{
    clear();
    printf("\n--- ACCEPT CHALLENGE ---\n");

    // Gửi lệnh chấp nhận (không cần payload)
    send_json(sock, ACT_ACCEPT_CHALLANGE, NULL);

    // Chờ phản hồi (Server sẽ trả về thông báo Start Game)
    cJSON *res = wait_for_response();
    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status)
        {
            if (status->valueint == RES_BATTLE_SUCCESS)
            {
                // Thành công: Bắt đầu trận đấu
                printf("[GAME START] %s\n", msg ? msg->valuestring : "Battle started!");

                // Nếu có dữ liệu kèm theo (Ví dụ: Tên đối thủ)
                if (data && !cJSON_IsNull(data))
                {
                    cJSON *opp_name = cJSON_GetObjectItem(data, "opponent_name");
                    cJSON *match_id = cJSON_GetObjectItem(data, "match_id");

                    if (opp_name)
                        printf("Your Opponent: %s\n", opp_name->valuestring);
                    if (match_id)
                        printf("Match ID: %d\n", match_id->valueint);
                }
            }
            else
            {
                // Thất bại: Có thể do hết thời gian, lỗi hệ thống...
                printf("❌ [ERROR] Cannot start game: %s\n", msg ? msg->valuestring : "Unknown error");
            }
        }
        cJSON_Delete(res);
    }
}

void do_attack()
{
    int target_uid, wp_type, wp_slot;
    printf("Enter Target User ID: ");
    if (scanf("%d", &target_uid) != 1)
        return; // Kiểm tra nhập liệu

    printf("Enter weapon slots: ");
    if (scanf("%d", &wp_slot) != 1)
        return; // Kiểm tra nhập liệu

    if (0 < wp_slot && wp_slot < 4)
    {
        printf("Weapon (1:Cannon, 2:Laser): ");
    }
    else if (4 <= wp_slot && wp_slot < 8)
    {
        printf("Weapon (3:Missile): ");
    }
    else
    {
        printf("Invalid weapon slot\n");
        return;
    }

    if (scanf("%d", &wp_type) != 1)
        return;

    // Xóa bộ nhớ đệm bàn phím để tránh lỗi trôi lệnh menu sau này
    getchar();

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "target_user_id", target_uid);
    cJSON_AddNumberToObject(data, "weapon_id", wp_type);
    cJSON_AddNumberToObject(data, "weapon_slot", wp_slot);
    send_json(sock, ACT_ATTACK, data);

    cJSON *res = wait_for_response();
    if (res)
    {
        // 1. Kiểm tra trạng thái phản hồi từ Server
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");

        if (status && status->valueint == RES_BATTLE_SUCCESS) // RES_BATTLE_SUCCESS = 400
        {
            cJSON *d = cJSON_GetObjectItem(res, "data");

            // 2. Kiểm tra data có hợp lệ và không phải NULL không
            if (d && !cJSON_IsNull(d))
            {
                cJSON *dmg_node = cJSON_GetObjectItem(d, "damage");
                cJSON *hp_node = cJSON_GetObjectItem(d, "target_hp");

                // 3. Chỉ in ra nếu các trường tồn tại
                if (dmg_node && hp_node)
                {
                    printf(">> HIT! Damage: %d | Target HP: %d\n",
                           dmg_node->valueint,
                           hp_node->valueint);
                }
                else
                {
                    printf(">> Attack success but no damage data returned.\n");
                }
            }
        }
        else
        {
            // Trường hợp tấn công thất bại (Sai mục tiêu, hết đạn...)
            printf(">> Attack Failed: %s\n", msg ? msg->valuestring : "Unknown error");
        }

        cJSON_Delete(res);
    }
}
