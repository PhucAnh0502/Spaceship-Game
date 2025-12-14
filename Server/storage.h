#ifndef STORAGE_H
#define STORAGE_H

#include "../Common/protocol.h"

extern Player *players;
extern int player_count;
extern size_t player_capacity;

extern Team *teams;
extern int team_count;

void load_accounts(const char* filename);
int register_player(const char* username, const char* password);
Player* find_player_by_username(const char* username);

/**
 * @brief Helper function for getting player by id. Note that a player is 
 * linked to a socket, so a player id is that socket fd
 * 
 * @param id player id (this is socket id too)
 * 
 * @return Player informations
 */
Player* find_player_by_id(int id);


/**
 * @brief Function for mapping from clientfd to coresponding player id
 * 
 * @param client_fd socket fd of connection to server
 * 
 * @return Player information linked to client_fd
 */
Player* get_player_by_fd (int client_fd);

/**
 * @brief Finction for find team by team id
 * @param team_id Team id that you want to search for
 * 
 * @return team infomation of given team id
 */
Team* find_team_by_id(int team_id);

void init_teams();
#endif