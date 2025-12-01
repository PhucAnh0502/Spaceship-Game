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

#endif