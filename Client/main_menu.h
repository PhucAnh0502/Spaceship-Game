

#ifndef SPACESHIP_GAME_MAIN_MENU_H
void show_game_result_screen();
int draw_menu(const char *title, const char *options[], int n_opts);
void print_menu(int highlight);
void print_dashboard_menu(int highlight);
#define SPACESHIP_GAME_MAIN_MENU_H

#endif //SPACESHIP_GAME_MAIN_MENU_H