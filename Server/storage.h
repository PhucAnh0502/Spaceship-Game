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


extern Team teams[MAX_TEAMS];
extern int team_count;

Team* find_team_by_id(int team_id);
Team* find_team_by_name(const char *team_name);
int is_valid_team_name(const char *name);
int is_team_name_exists(const char *name);
Team* create_team(const char *name, int captain_id);
int delete_team(int team_id);
int is_member(Team *team, int user_id);
int add_member(Team *team, int user_id);
int remove_member(Team *team, int user_id);
int is_pending_request_exists(Team *team, int user_id);
int add_pending_request(Team *team, int user_id);
int remove_pending_request(Team *team, int user_id);
int is_captain(Team *team, int user_id);

#endif
