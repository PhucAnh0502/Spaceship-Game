#include "../../../Lib/cJSON.h"

/**
 * @brief function for sending combat challenge
 *
 * @param client_fd team id that sent challenge request
 * @param payload challenge request body
 */
void handle_send_challenge(int client_fd, cJSON *payload);

/**
 * @brief function for accepting combat challenge
 *
 * @param client_fd team id that sent challenge request
 * @param payload request body
 */
void handle_accept_challenge(int client_fd, cJSON *payload);

/**
 * @brief function for fixing ship
 * 
 * @param client_fd team id that sent challenge request
 * @param payload request body
 */
void handle_fix_ship(int client_fd, cJSON *payload);

/**
 * @brief Function for handling attack command
 * 
 * @param client_fd team id that sent challenge request
 * @param payload request body
 */
void handle_attack(int client_fd, cJSON *payload);
void handle_get_status(int client_fd, cJSON *payload);

/**
 * @brief Function for checking end game condidtion
 * 
 * @param team_id team id that sent challenge request
 */
void check_end_game(int client_fd,int team_id);

/**
 * @brief Function for setting end game condition to one team
 * 
 * @param client_fd socket id
 * @param team team for checking end game condition
 * @param payload request body
 */
void end_game_for_team(int client_fd, Team *team, cJSON *payload);

void handle_game_start(Team* my_team, Team* opponent_team);