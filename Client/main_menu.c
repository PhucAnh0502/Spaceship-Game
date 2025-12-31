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
    timeout(-1);

    clear();
    box(stdscr, 0, 0); 

    int height, width;
    getmaxyx(stdscr, height, width);

    if (winner_team_id == current_team_id) {
        // --- MÀN HÌNH CHIẾN THẮNG  ---
        attron(A_BOLD | COLOR_PAIR(2));
        mvprintw(height / 2 - 4, (width - 20) / 2, "**********************");
        mvprintw(height / 2 - 3, (width - 20) / 2, "* VICTORY!      *");
        mvprintw(height / 2 - 2, (width - 20) / 2, "**********************");
        attroff(A_BOLD | COLOR_PAIR(2));

        mvprintw(height / 2, (width - 40) / 2, "Congratulations! Your team has won.");
    } else if (winner_team_id != current_team_id) {
        // --- MÀN HÌNH THẤT BẠI  ---
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

    while (1) {
        int ch = getch();
        if (ch == 10 || ch == 13)
            break; 
    }

    end_game_flag = 0;
}

int draw_menu(const char *title, const char *options[], int n_opts) {
    clear();
    int highlight = 0;
    int c;
    int height, width;
    timeout(100);

    while (1) {
        if (end_game_flag) {
            show_game_result_screen(); 

            timeout(-1);
            return -1;
        }
        // --- VẼ UI ---
        erase();
        getmaxyx(stdscr, height, width);
        box(stdscr, 0, 0);

        // 1. Tiêu đề
        attron(A_BOLD | COLOR_PAIR(2));
        mvprintw(2, (width - strlen(title)) / 2, "%s", title);
        attroff(A_BOLD | COLOR_PAIR(2));

        // 2. Trạng thái User 
        int start_y = 7;
        int line_y = start_y - 2;
        int is_combat_panel = 0;
        if (current_user_id != 0) {
            mvprintw(4, 4, "User: %s | ", current_username);

            if (current_hp < 300)
                attron(COLOR_PAIR(1)); 
            else
                attron(COLOR_PAIR(2));
            printw("HP: %d", current_hp);
            if (current_hp < 300)
                attroff(COLOR_PAIR(1));
            else
                attroff(COLOR_PAIR(2));

            printw(" | Coin: %d", current_coins);

            if (strstr(title, "COMBAT ZONE") != NULL) {
                draw_compact_status(5, 4);
                start_y = 9; 
                line_y = start_y - 1; 
                is_combat_panel = 1;
            }
        } else {
            mvprintw(4, 4, "Status: Guest");
        }

        if (is_combat_panel) {
            mvwhline(stdscr, line_y, 1, ACS_HLINE, width - 2);
        } else {
            mvwhline(stdscr, start_y - 2, 1, ACS_HLINE, width - 2);
        }

        // 3. Menu Options
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

        refresh(); 

        // --- XỬ LÝ NHẬP LIỆU ---
        c = getch(); 

        if (c == ERR) {
            continue;
        }

        switch (c) {
            case KEY_UP:
                highlight = (highlight == 0) ? n_opts - 1 : highlight - 1;
                break;
            case KEY_DOWN:
                highlight = (highlight == n_opts - 1) ? 0 : highlight + 1;
                break;
            case 10: 
                timeout(-1); 
                return highlight;
            case KEY_BACKSPACE:
            case 127:
                timeout(-1); 
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
        "1. Team Management",
        "2. Combat Zone",
        "3. Treasure Hunt",
        "4. Logout"
    };
    int n_choices = 4;

    int height, width;
    getmaxyx(stdscr, height, width);
    box(stdscr, 0, 0);

    // Vẽ Header
    attron(A_BOLD | COLOR_PAIR(2)); 
    mvprintw(2, 10, "=== MAIN DASHBOARD ===");
    mvprintw(3, 10, "User: %s | HP: %d | Coins: %d", current_username, current_hp, current_coins);
    attroff(A_BOLD | COLOR_PAIR(2));

    // Vẽ danh sách lựa chọn
    for (int i = 0; i < n_choices; i++) {
        if (highlight == i) {
            attron(A_REVERSE); 
            mvprintw(5 + i, 10, "-> %s", options[i]);
            attroff(A_REVERSE);
        } else {
            mvprintw(5 + i, 10, "   %s", options[i]);
        }
    }
    attron(A_BOLD | COLOR_PAIR(1)); 
    mvprintw(height - 3, 10, "Use UP/DOWN to move, ENTER to select.");
    attron(A_BOLD | COLOR_PAIR(1)); 
    refresh(); 
}

void draw_dead_popup() {
    int h, w;
    getmaxyx(stdscr, h, w); // Lấy kích thước màn hình

    int box_h = 10;
    int box_w = 40;
    int start_y = (h - box_h) / 2;
    int start_x = (w - box_w) / 2;

    // 1. Xóa vùng bên trong popup (để che text của background bên dưới)
    attron(COLOR_PAIR(0)); // Màu mặc định
    for(int i = 0; i < box_h; i++) {
        mvhline(start_y + i, start_x, ' ', box_w);
    }

    // 2. Vẽ viền khung (Màu đỏ đậm)
    attron(COLOR_PAIR(1) | A_BOLD);
    // Vẽ các đường ngang/dọc
    mvhline(start_y, start_x, ACS_HLINE, box_w);
    mvhline(start_y + box_h - 1, start_x, ACS_HLINE, box_w);
    mvvline(start_y, start_x, ACS_VLINE, box_h);
    mvvline(start_y, start_x + box_w - 1, ACS_VLINE, box_h);

    // Vẽ 4 góc
    mvaddch(start_y, start_x, ACS_ULCORNER);
    mvaddch(start_y, start_x + box_w - 1, ACS_URCORNER);
    mvaddch(start_y + box_h - 1, start_x, ACS_LLCORNER);
    mvaddch(start_y + box_h - 1, start_x + box_w - 1, ACS_LRCORNER);

    // 3. Viết nội dung thông báo
    const char* title = "YOU ARE DEAD!";
    mvprintw(start_y + 2, start_x + (box_w - strlen(title)) / 2, "%s", title);
    attroff(COLOR_PAIR(1) | A_BOLD); // Tắt màu đỏ

    mvprintw(start_y + 4, start_x + 3, "HP reached 0.");
    mvprintw(start_y + 5, start_x + 3, "Spectating teammates...");

    // Hướng dẫn thoát
    attron(A_DIM);
    mvprintw(start_y + 8, start_x + (box_w - 20) / 2, "Press [Q] to Logout");
    attroff(A_DIM);
}