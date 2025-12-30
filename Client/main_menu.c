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


void show_game_result_screen() {
    // Tắt chế độ non-blocking để chờ người dùng ấn phím
    timeout(-1);

    clear();
    box(stdscr, 0, 0); // Vẽ khung viền

    int height, width;
    getmaxyx(stdscr, height, width);

    if (winner_team_id == current_team_id) {
        // --- MÀN HÌNH CHIẾN THẮNG (Màu Xanh) ---
        attron(A_BOLD | COLOR_PAIR(2));
        mvprintw(height / 2 - 4, (width - 20) / 2, "**********************");
        mvprintw(height / 2 - 3, (width - 20) / 2, "* VICTORY!      *");
        mvprintw(height / 2 - 2, (width - 20) / 2, "**********************");
        attroff(A_BOLD | COLOR_PAIR(2));

        mvprintw(height / 2, (width - 40) / 2, "Congratulations! Your team has won.");
    } else if (winner_team_id != current_team_id) {
        // --- MÀN HÌNH THẤT BẠI (Màu Đỏ) ---
        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw(height / 2 - 4, (width - 20) / 2, "######################");
        mvprintw(height / 2 - 3, (width - 20) / 2, "#       DEFEAT       #");
        mvprintw(height / 2 - 2, (width - 20) / 2, "######################");
        attroff(A_BOLD | COLOR_PAIR(1));

        mvprintw(height / 2, (width - 40) / 2, "Your team was eliminated.");
        mvprintw(height / 2 + 1, (width - 40) / 2, "Winner: %s", last_winner_name);
        mvprintw(height / 2 + 2, (width - 40) / 2, "Winner id : %d", winner_team_id);
        mvprintw(height / 2 + 3, (width - 40) / 2, "current id : %d", current_team_id);
    }

    attron(A_DIM);
    mvprintw(height - 4, (width - 30) / 2, "Your ship has been repaired.");
    mvprintw(height - 3, (width - 30) / 2, "Press [ENTER] to return to Lobby...");
    attroff(A_DIM);

    refresh();

    // Chờ người dùng ấn Enter để thoát
    while (1) {
        int ch = getch();
        if (ch == 10 || ch == 13)
            break; // Enter key
    }

    // Reset cờ sau khi đã xem xong
    end_game_flag = 0;
}

// Hàm tiện ích để vẽ menu và trả về lựa chọn của người dùng
int draw_menu(const char *title, const char *options[], int n_opts) {
    clear();
    int highlight = 0;
    int c;
    int height, width;

    // Cài đặt timeout: getch() sẽ chờ 100ms.
    // Nếu không có phím nào được ấn, nó trả về ERR nhưng vòng lặp vẫn chạy tiếp -> UI được vẽ lại.
    timeout(100);

    while (1) {
        if (end_game_flag) {
            show_game_result_screen(); // Hiển thị Pop-up kết quả ngay lập tức

            // Sau khi xem kết quả xong, buộc thoát khỏi menu con để về Lobby
            timeout(-1);
            return -1;
        }
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
        if (current_user_id != 0) {
            // Hiển thị HP với màu sắc cảnh báo nếu máu thấp
            mvprintw(4, 4, "User: %s | ", current_username);

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
        } else {
            mvprintw(4, 4, "Status: Guest");
        }

        mvwhline(stdscr, 5, 1, ACS_HLINE, width - 2);

        // 3. Menu Options
        int start_y = 7;
        int start_x = 4;
        for (int i = 0; i < n_opts; i++) {
            if (i == highlight) {
                attron(A_REVERSE);
                mvprintw(start_y + i, start_x, " -> %s ", options[i]);
                attroff(A_REVERSE);
            } else {
                mvprintw(start_y + i, start_x, "    %s ", options[i]);
            }
        }

        attron(COLOR_PAIR(1));
        mvprintw(height - 2, 2, "[UP/DOWN]: Move | [ENTER]: Select | [BACKSPACE]: Back");
        attroff(COLOR_PAIR(1));

        refresh(); // Đẩy thay đổi lên màn hình

        // --- XỬ LÝ NHẬP LIỆU ---
        c = getch(); // Hàm này giờ chỉ chờ 100ms

        if (c == ERR) {
            // Không có phím nào được ấn (Timeout)
            // Tiếp tục vòng lặp để vẽ lại UI với dữ liệu mới (nếu có)
            continue;
        }

        // Nếu có phím ấn, xử lý như bình thường
        switch (c) {
            case KEY_UP:
                highlight = (highlight == 0) ? n_opts - 1 : highlight - 1;
                break;
            case KEY_DOWN:
                highlight = (highlight == n_opts - 1) ? 0 : highlight + 1;
                break;
            case 10: // Enter
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

void print_menu(int highlight) {
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
    if (current_user_id != 0) {
        attron(COLOR_PAIR(2));
        mvprintw(2, 10, "Logged in as User ID: %d", current_user_id);
        mvprintw(3, 10, "Coins: %d | HP: %d", current_coins, current_hp);
        attroff(COLOR_PAIR(2));

        current_choices = (char **) choices_user;
        n_choices = sizeof(choices_user) / sizeof(choices_user[0]);
    } else {
        mvprintw(2, 10, "Status: Guest (Not Logged In)");
        current_choices = (char **) choices_guest;
        n_choices = sizeof(choices_guest) / sizeof(choices_guest[0]);
    }

    for (int i = 0; i < n_choices; i++) {
        refresh();
        if (highlight == i) {
            attron(A_REVERSE);
            mvprintw(5 + i, 10, "-> %s", current_choices[i]);
            attroff(A_REVERSE);
        } else {
            mvprintw(5 + i, 10, "   %s", current_choices[i]);
        }
    }

    mvprintw(5 + n_choices + 2, 10, "Use arrow keys to move, Enter to select.");
    refresh();
}

void print_dashboard_menu(int highlight) {
    erase();
    const char *options[] = {
        "1. Shop System",
        "2. Team Management",
        "3. Combat Zone",
        "4. Treasure Hunt",
        "5. Logout"
    };
    int n_choices = 5;

    int height, width;
    getmaxyx(stdscr, height, width);
    box(stdscr, 0, 0);

    // Vẽ Header
    attron(A_BOLD | COLOR_PAIR(2)); // Màu xanh lá
    mvprintw(2, 10, "=== MAIN DASHBOARD ===");
    mvprintw(3, 10, "User: %s | HP: %d | Coins: %d", current_username, current_hp, current_coins);
    attroff(A_BOLD | COLOR_PAIR(2));

    // Vẽ danh sách lựa chọn
    for (int i = 0; i < n_choices; i++) {
        if (highlight == i) {
            attron(A_REVERSE); // Highlight dòng đang chọn
            mvprintw(5 + i, 10, "-> %s", options[i]);
            attroff(A_REVERSE);
        } else {
            mvprintw(5 + i, 10, "   %s", options[i]);
        }
    }
    attron(A_BOLD | COLOR_PAIR(1)); // Màu
    mvprintw(height - 3, 10, "Use UP/DOWN to move, ENTER to select.");
    attron(A_BOLD | COLOR_PAIR(1)); // Màu
    refresh(); // Quan trọng: Đẩy thay đổi lên màn hình
}