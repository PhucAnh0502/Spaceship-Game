#ifndef STORAGE_H
#define STORAGE_H

#include "../Common/protocol.h"

extern Player *players;
extern int player_count;
extern size_t player_capacity;

void load_accounts(const char* filename);
void update_player_to_file(Player *player);
int register_player(const char* username, const char* password);
Player* find_player_by_username(const char* username);
Player* find_player_by_id(int id);
Player* find_player_by_socket(int socket_fd);

#endif
