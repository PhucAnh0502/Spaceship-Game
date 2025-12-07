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
 * @param payload challenge request body
 */
void handle_accept_challenge(int client_fd, cJSON *payload);

/**
 * @brief function for fixing ship
 * 
 * @param client_fd team id that sent challenge request
 * @param payload challenge request body
 */
void handle_fix_ship(int client_fd, cJSON *payload);

/**
 * @brief Function for handling attack command
 * 
 * @param client_fd team id that sent challenge request
 * @param payload challenge request body
 */
void handle_attack(int client_fd, cJSON *payload);