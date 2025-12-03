#include "utils.h"
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

void send_json(int socket_fd, cJSON *json) {
    if (socket_fd <= 0 || json == NULL) return;

    char *str_json = cJSON_PrintUnformatted(json);
    if (str_json == NULL) return;

    size_t len = strlen(str_json);

    char *msg = (char *)malloc(len + 3);
    if (msg == NULL) {
        free(str_json);
        return;
    }

    strcpy(msg, str_json);
    strcat(msg, "\r\n");

    if (send(socket_fd, msg, strlen(msg), 0) < 0) {
        perror("[Server] Send failed");
    }

    free(msg);
    free(str_json);
}

cJSON *receive_json(int socket_fd, char *buffer, int *buf_len, int max_buf_size) {
    if (socket_fd <= 0 || buffer == NULL || buf_len == NULL) return NULL;

    char *line_end = strstr(buffer, "\r\n");

    if (line_end == NULL) {
        int free_space = max_buf_size - *buf_len - 1;
        if (free_space <= 0) {
            printf("[WARN] Buffer overflow detected fd %d, clearing.\n", socket_fd);
            *buf_len = 0;
            free_space = max_buf_size - 1;
        }

        int bytes_received = recv(socket_fd, buffer + *buf_len, free_space, 0);

        if (bytes_received > 0) {
            *buf_len += bytes_received;
            buffer[*buf_len] = '\0';
            line_end = strstr(buffer, "\r\n"); 
        } else {
            return NULL; 
        }
    }

    if (line_end != NULL) {
        *line_end = '\0'; 
        
        cJSON *json = cJSON_Parse(buffer);
        if (json == NULL) {
            printf("[ERROR] JSON Parse Error: %s\n", buffer);
        }

        char *next_start = line_end + 2;
        int remaining_len = *buf_len - (next_start - buffer);

        if (remaining_len > 0) {
            memmove(buffer, next_start, remaining_len);
        }
        *buf_len = remaining_len;
        buffer[*buf_len] = '\0';
        
        return json;
    }

    return NULL;
}

void send_response(int socket_fd, int status, const char *msg, cJSON *data) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "status", status);
    cJSON_AddStringToObject(root, "message", msg);
    if (data) {
        cJSON_AddItemToObject(root, "data", data);
    } else {
        cJSON_AddNullToObject(root, "data");
    }
    
    send_json(socket_fd, root);
    cJSON_Delete(root);
}

void log_action(const char *status, const char *action, const char *input, const char *result) {
    FILE *file = fopen("logs.txt", "a");
    if(file == NULL) {
        perror("Failed to open log file");
        return;
    }
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    
    fprintf(file, "[%s] [%s] Action: %s | Input: %s | Result: %s\n", 
            time_str, status, action, (input ? input : "N/A"), result);
    fclose(file);
}