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
#include "../Server/handlers/shop/client_shop.c"


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
            // Hiển thị màu xanh lá cây
            display_response_message(4, 5, 2, status->valueint, msg ? msg->valuestring : "Success");

            // Cập nhật lại hiển thị tiền/hp ngay lập tức
            if (data) {
                cJSON *coin = cJSON_GetObjectItem(data, "coin");
                cJSON *hp = cJSON_GetObjectItem(data, "hp");
                if (coin)
                    current_coins = coin->valueint;
                if (hp)
                    current_hp = hp->valueint;
            }
        } else {
            display_response_message(4, 5, 1, status ? status->valueint : 0, "Failed");
        }
        cJSON_Delete(res);
    }
    // Dừng màn hình để xem kết quả
    mvprintw(12, 5, "Press any key...");
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
        "7. [DEBUG] Get Full Gear (Cheat)", // [THÊM LỰA CHỌN NÀY]
        "8. Back"
    };
    int n_opts = 8; // Tăng số lượng option lên 8

    while (1) {
        int choice = draw_menu("SHOP SYSTEM", options, n_opts);
        if (choice == -1 || choice == 7)
            break; // Choice 7 là Back

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
                break; // [GỌI HÀM CHEAT]
        }
    }
}


// Hàm xác nhận mua hàng (UI Ncurses)
int confirm_purchase(const char *item_name, int cost) {
    // 1. Tắt chế độ chờ (timeout) để hàm này hoạt động chặn (blocking)
    // Người dùng bắt buộc phải chọn xong mới làm việc khác
    timeout(-1);

    int height, width;
    getmaxyx(stdscr, height, width); // Lấy kích thước màn hình

    // --- TRƯỜNG HỢP 1: KHÔNG ĐỦ TIỀN ---
    if (current_coins < cost) {
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
        getch(); // Chờ bấm phím bất kỳ
        return 0; // Trả về False
    }

    // --- TRƯỜNG HỢP 2: ĐỦ TIỀN -> HỎI XÁC NHẬN ---
    int choice = 0; // 0 = YES, 1 = NO
    int key;

    while (1) {
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
        switch (key) {
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

void do_treasure_hunt() {
    // Cài đặt timeout cho getch để nó không chặn mãi mãi
    // Điều này giúp vòng lặp chạy liên tục để kiểm tra kho báu mới
    timeout(100);

    while (1) {
        // 1. Kiểm tra xem có dữ liệu kho báu mới từ thread nền không
        pthread_mutex_lock(&pending_mutex);
        int has_treasure = pending_treasure.has_pending;
        pthread_mutex_unlock(&pending_mutex);

        // NẾU CÓ KHO BÁU -> HIỂN THỊ CÂU HỎI NGAY
        if (has_treasure) {
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
            int choice = draw_menu(title, (const char **) options, 5);

            // Dọn dẹp
            for (int i = 0; i < 4; i++)
                free(options[i]);

            // Xử lý lựa chọn
            if (choice == -1 || choice == 4) {
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
            if (treasure_response_data) {
                // Xóa dữ liệu cũ nếu có
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
            while (waiting_for_result && timeout_counter < 50) {
                // Chờ tối đa 5 giây (50 * 100ms)
                usleep(100000); // Ngủ 100ms
                timeout_counter++;
            }

            // Hết vòng lặp, kiểm tra xem có dữ liệu không
            cJSON *res = treasure_response_data; // Lấy dữ liệu từ listener
            treasure_response_data = NULL; // Reset pointer
            waiting_for_result = 0; // Reset cờ

            if (res) {
                // ... (Phần hiển thị kết quả GIỮ NGUYÊN như cũ) ...
                cJSON *status = cJSON_GetObjectItem(res, "status");
                cJSON *msg = cJSON_GetObjectItem(res, "message");
                cJSON *d = cJSON_GetObjectItem(res, "data");

                clear();
                if (status && status->valueint == RES_TREASURE_SUCCESS) {
                    // (Code in chúc mừng...)
                    attron(A_BOLD | COLOR_PAIR(3));
                    mvprintw(5, 5, " $$$ CONGRATULATIONS! $$$ ");
                    attroff(A_BOLD | COLOR_PAIR(3));
                    mvprintw(7, 5, ">> %s", msg ? msg->valuestring : "");
                    if (d) {
                        // Update coin
                        cJSON *total = cJSON_GetObjectItem(d, "total_coins");
                        if (total)
                            current_coins = total->valueint;
                        // ...
                    }
                } else {
                    // (Code in lỗi...)
                    display_response_message(5, 5, 1, status ? status->valueint : 0, msg ? msg->valuestring : "Failed");
                }
                cJSON_Delete(res);
            } else {
                mvprintw(5, 5, "Error: Timeout waiting for server.");
            }

            mvprintw(12, 5, "Press any key to return...");
            timeout(-1); // Chặn chờ phím
            getch();
            return; // Xong một lượt thì thoát ra menu chính
        }

        // NẾU KHÔNG CÓ KHO BÁU -> HIỆN MÀN HÌNH CHỜ
        else {
            erase(); // Dùng erase thay vì clear để đỡ nháy màn hình
            box(stdscr, 0, 0); // Vẽ khung

            attron(A_BOLD | COLOR_PAIR(3));
            mvprintw(2, 5, "=== TREASURE HUNT ZONE ===");
            attroff(A_BOLD | COLOR_PAIR(3));

            mvprintw(5, 5, ">> Scanning for treasure signals...");

            // Tạo hiệu ứng loading đơn giản
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

            int ch = getch(); // Hàm này giờ chỉ chờ 100ms
            if (ch == KEY_BACKSPACE || ch == 127) {
                timeout(-1); // Reset lại chế độ chuẩn trước khi thoát
                return;
            }
        }
    }
}

// Hàm này chịu trách nhiệm cho TOÀN BỘ logic của màn hình Treasure (Vẽ + Xử lý)
void run_treasure_mode(int key) {
    // --- PHẦN 1: VẼ GIAO DIỆN (Draw) ---
    erase();
    refresh(); // Làm sạch nền

    int height = 15;
    int width = 60;

    // Tạo window popup
    WINDOW *win = newwin(height, width, (LINES - height) / 2, (COLS - width) / 2);
    if (win == NULL)
        return;

    box(win, 0, 0); // Vẽ khung

    // Vẽ nội dung
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

    wrefresh(win); // Đẩy nội dung lên màn hình
    delwin(win); // Xóa cấu trúc window (chỉ xóa trong bộ nhớ)

    // --- PHẦN 2: XỬ LÝ LOGIC (Update) ---
    // Nếu không có phím bấm (ERR) thì chỉ vẽ thôi, không làm gì cả
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

            // Xử lý xong -> Tắt popup & Xóa cờ pending
            pthread_mutex_lock(&pending_mutex);
            pending_treasure.has_pending = 0;
            pthread_mutex_unlock(&pending_mutex);

            clear(); // Xóa màn hình
        }
    } else if (key == 'q' || key == 'Q') {
        // Bỏ qua rương
        pthread_mutex_lock(&pending_mutex);
        pending_treasure.has_pending = 0;
        pthread_mutex_unlock(&pending_mutex);
        clear();
    }
}