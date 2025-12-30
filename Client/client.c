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
#include "main_menu.c"
#include "menu_shop.c"
#include "combat_menu.c"
#include "team_menu.c"

#define SERVER_IP "127.0.0.1"

int sock = 0;
int current_user_id = 0;
int current_coins = 0;
int current_hp = 1000;
char current_username[50] = "";

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

    waiting_for_result = 1; // Bật cờ báo "Tôi đang chờ"

    // Nếu có dữ liệu cũ chưa xóa, xóa đi
    if (sync_response) {
        cJSON_Delete(sync_response);
        sync_response = NULL;
    }

    // Vòng lặp chờ tín hiệu (Condition Variable)
    // Nó sẽ NHẢ mutex ra và ngủ. Khi Listener gọi signal(), nó tỉnh dậy và LẤY lại mutex.
    while (sync_response == NULL) {
        pthread_cond_wait(&sync_cond, &sync_mutex);
    }

    cJSON *res = sync_response;
    sync_response = NULL; // Reset để lần sau dùng
    waiting_for_result = 0; // Tắt cờ

    pthread_mutex_unlock(&sync_mutex);
    return res;
}

void show_player_status() {
    printf("\n========== TRANG THAI HIEN TAI ==========\n");
    printf("Coins: %d\n", current_coins);
    printf("HP: %d/1000\n", current_hp);
}

int main() {
    struct sockaddr_in serv_addr;

    memset(client_buffer, 0, BUFFER_SIZE);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
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
    timeout(100);
    int dashboard_highlight = 0;
    int need_redraw = 1; // Biến cờ để kiểm soát việc vẽ lại màn hình
    // MENU CHÍNH
    while (1) {
        if (current_user_id == 0) {
            // --- GUEST MENU ---
            const char *options[] = {"1. Register", "2. Login", "3. Exit"};
            // draw_menu đang set timeout(-1) khi return
            int choice = draw_menu("WELCOME GUEST", options, 3);

            if (choice == 2 || choice == -1)
                break;
            switch (choice) {
                case 0:
                    do_register();
                    break;
                case 1:
                    do_login();
                    // [FIX 2] Reset highlight để menu không bị lệch dòng khi mới vào
                    dashboard_highlight = 0;
                    need_redraw = 1; // Đánh dấu cần vẽ lại sau login
                    break;
            }
        } else {
            timeout(100);
            if (end_game_flag) {
                show_game_result_screen();
                // Sau khi xem xong kết quả, continue để vẽ lại dashboard mới
                need_redraw = 1; // Đánh dấu cần vẽ lại
                continue;
            }
            if (current_hp <= 0) {
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
                if (ch == 'q' || ch == 'Q') {
                    do_logout();
                }

                // Nếu server gửi lệnh reset game (hồi máu), vòng lặp sau sẽ tự thoát if này
                continue;
            }

            // 1. Kiểm tra trạng thái
            int is_treasure_mode = 0;
            pthread_mutex_lock(&pending_mutex);
            is_treasure_mode = pending_treasure.has_pending;
            pthread_mutex_unlock(&pending_mutex);

            // 2. Đọc phím bấm (Input) - Chỉ gọi 1 lần duy nhất ở đây
            int c = getch();

            // 3. Điều phối xử lý (Update & Draw)
            if (is_treasure_mode) {
                // Chuyển phím bấm và quyền kiểm soát cho màn hình Treasure
                run_treasure_mode(c);
            } else {
                // --- LOGIC MENU CHÍNH ---
                // Vẽ menu chỉ khi cần
                if (need_redraw) {
                    print_dashboard_menu(dashboard_highlight);
                    need_redraw = 0; // Reset cờ
                }

                // Xử lý phím cho menu
                if (c != ERR) {
                    switch (c) {
                        case KEY_UP:
                            dashboard_highlight = (dashboard_highlight == 0) ? 4 : dashboard_highlight - 1;
                            need_redraw = 1; // Cần vẽ lại do thay đổi highlight
                            break;
                        case KEY_DOWN:
                            dashboard_highlight = (dashboard_highlight == 4) ? 0 : dashboard_highlight + 1;
                            need_redraw = 1; // Cần vẽ lại do thay đổi highlight
                            break;
                        case 10: // Phím ENTER
                            timeout(-1);

                            switch (dashboard_highlight) {
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
                                    break;
                                case 4:
                                    do_logout();
                                    break;
                            }

                            timeout(100);
                            clear(); // Xóa màn hình menu con
                            need_redraw = 1; // Cần vẽ lại sau khi thoát menu con
                            break;
                    }
                }
            }
        }
    }

    // Dọn dẹp
    endwin();
    close(sock);
    return 0;
}




// ============================================================
// HÀM BACKGROUND LISTENER HOÀN CHỈNH
// ============================================================
void *background_listener(void *arg) {
    while (!should_exit) {
        // 1. Dùng Blocking Mode (Không dùng usleep).
        // Hàm này sẽ TREO ở đây cho đến khi Server gửi gì đó.
        cJSON *response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);
        if (response == NULL) {
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
        cJSON *ques_node = (data) ? cJSON_GetObjectItem(data, "question") : NULL;
        int is_treasure_broadcast = (code == RES_TREASURE_SUCCESS && ques_node != NULL);
        // A. GÓI TIN MÀ MAIN THREAD ĐANG CHỜ (LOGIN, MUA ĐỒ, TẤN CÔNG...)
        // Kiểm tra xem Main Thread có đang đợi không (biến waiting_for_result)
        // VÀ gói tin này KHÔNG PHẢI là thông báo bất đồng bộ (như kho báu/end game)
        int is_sync_msg = 0;

        pthread_mutex_lock(&sync_mutex); // Dùng mutex riêng cho việc đồng bộ

        if (waiting_for_result &&
            !is_treasure_broadcast &&
            act_code != ACT_TREASURE_APPEAR &&
            code != RES_END_GAME) {
            // Đây là câu trả lời Main Thread đang cần
            if (sync_response)
                cJSON_Delete(sync_response);
            sync_response = cJSON_Duplicate(response, 1); // Copy dữ liệu

            waiting_for_result = 0; // Tắt cờ chờ
            pthread_cond_signal(&sync_cond); // <--- ĐÁNH THỨC MAIN THREAD NGAY LẬP TỨC
            pthread_mutex_unlock(&sync_mutex);
            is_sync_msg = 1;
            continue;
        }
        pthread_mutex_unlock(&sync_mutex);

        if (is_sync_msg) {
            cJSON_Delete(response);
            continue; // Đã xử lý xong, quay lại vòng lặp
        }

        // B. XỬ LÝ CÁC GÓI TIN BẤT ĐỒNG BỘ (ASYNC EVENTS)

        // 1. End Game
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
            // Update UI State an toàn
            pthread_mutex_lock(&ui_mutex); // Giả sử bạn có ui_mutex
            current_hp = 1000;
            pthread_mutex_unlock(&ui_mutex);

            pthread_mutex_unlock(&pending_mutex);
            pthread_mutex_lock(&sync_mutex);
            if (waiting_for_result) {
                waiting_for_result = 0; // Hủy cờ chờ
                pthread_cond_broadcast(&sync_cond);
            }
            pthread_mutex_unlock(&sync_mutex);
        }

        if (status && status->valueint == RES_TREASURE_SUCCESS) {
            if (data) {
                cJSON *treasure_id = cJSON_GetObjectItem(data, "treasure_id");
                cJSON *question = cJSON_GetObjectItem(data, "question");
                cJSON *options = cJSON_GetObjectItem(data, "options");
                cJSON *reward = cJSON_GetObjectItem(data, "reward");
                cJSON *chest_type = cJSON_GetObjectItem(data, "chest_type");

                if (treasure_id && question && options) {
                    // LOGIC MỚI: Chỉ lưu dữ liệu vào pending_treasure và bật cờ
                    pthread_mutex_lock(&pending_mutex);

                    pending_treasure.has_pending = 1; // Bật cờ để Main Loop biết
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
        }
        // 2. Kho báu xuất hiện
        else if (code == RES_TREASURE_SUCCESS && cJSON_GetObjectItem(data, "question")) {
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
        else if (code == RES_TREASURE_OPENED) {
            pthread_mutex_lock(&pending_mutex);
            pending_treasure.has_pending = 0;
            pthread_mutex_unlock(&pending_mutex);
        }

        // 4. Cập nhật HP/Coin thụ động (Real-time update)
        // (Xảy ra khi bắn trúng, bị bắn, hoặc mở rương thành công)
        if (data) {
            cJSON *hp_node = cJSON_GetObjectItem(data, "current_hp");
            cJSON *coin_node = cJSON_GetObjectItem(data, "current_coin");
            // Server thường trả về "coin" hoặc "current_coin"
            cJSON *total_coin = cJSON_GetObjectItem(data, "total_coins");

            // Dùng UI Mutex để bảo vệ biến toàn cục
            if (hp_node || coin_node || total_coin) {
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
    }
    return NULL;
}

