#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../Lib/cJSON.h"
#include "../Common/protocol.h"

void send_json(int socket_fd, cJSON *json);

void send_response(int socket_fd, int status, const char *message, cJSON *data);

void log_action(const char *status, const char *action, const char *input, const char *result);

cJSON *receive_json(int socket_fd, char *buffer, int *buf_len, int max_buf_size);

void broadcast_to_team(Team *team, int status, const char *message, cJSON *payload);
#endif