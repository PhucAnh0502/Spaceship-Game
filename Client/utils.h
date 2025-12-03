#ifndef UTILS_H
#define UTILS_H

#include "../Lib/cJSON.h"

void send_json(int sock, int action, cJSON *data);

cJSON *receive_json(int socket_fd, char *buffer, int *buf_len, int max_buf_size);

void get_input(const char *prompt, char *buffer, int size);

#endif