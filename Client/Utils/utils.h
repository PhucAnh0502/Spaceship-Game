#ifndef UTILS_H
#define UTILS_H

#include "../../Lib/cJSON.h"

void send_json(int sock, int action, cJSON *data);

cJSON *receive_json(int socket_fd, char *buffer, int *buf_len, int max_buf_size);

void get_input(int y, int x, const char *prompt, char *buffer, int size, int is_password);

void display_response_message(int y, int x, int color_pair, int status, const char *msg);

#endif