#include "utils.h"
#include <sys/socket.h>
#include <unistd.h>

void send_json(int socket_fd, cJSON *json) {
    if (socket_fd <= 0 || json == NULL) return;

    char *str_json = cJSON_PrintUnformatted(json);
    if (str_json == NULL) return;

    if (send(socket_fd, str_json, strlen(str_json), 0) < 0) {
        perror("Send failed");
    }

    free(str_json);
}

void send_response(int socket_fd, int status, const char *msg, cJSON *data) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "status", status);
    cJSON_AddStringToObject(root, "message", msg);
    cJSON_AddItemToObject(root, "data", data);
    send_json(socket_fd, root);
    cJSON_Delete(root);
}