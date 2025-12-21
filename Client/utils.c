#include "utils.h"
#include "../Common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <ncurses.h>

void get_input(int y, int x, const char *prompt, char *buffer, int size, int is_password) {
    mvprintw(y, x, "%s", prompt);
    if(is_password) noecho();
    else echo();

    getnstr(buffer, size - 1);
    noecho();
}

void display_response_message(int y, int x, int color_pair, int status, const char *msg) {
    attron(COLOR_PAIR(color_pair));
    if(status){
        mvprintw(y, x, ">> Server [%d]: %s", status, msg);
    } else {
        mvprintw(y, x, ">> Server: %s", msg);
    }
    attroff(COLOR_PAIR(color_pair));
}

void send_json(int sock, int action, cJSON *data) {
    if (sock <= 0) return;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "action", action);
    if (data) {
        cJSON_AddItemToObject(root, "data", data);
    } else {
        cJSON_AddNullToObject(root, "data");
    }

    char *str_json = cJSON_PrintUnformatted(root);
    if(str_json == NULL) return;
    
    size_t len = strlen(str_json);

    char *msg = (char *)malloc(len + 3);
    if (!msg) {
        free(str_json);
        cJSON_Delete(root);
        return;
    }

    strcpy(msg, str_json);
    strcat(msg, "\r\n");

    if (send(sock, msg, strlen(msg), 0) < 0) {
        perror("[Client] Send failed");
    }

    free(msg);
    free(str_json);
    cJSON_Delete(root);
}

cJSON *receive_json(int socket_fd, char *buffer, int *buf_len, int max_buf_size) {
    if (socket_fd <= 0 || buffer == NULL || buf_len == NULL) return NULL;

    char *line_end = strstr(buffer, "\r\n");

    if (line_end == NULL) {
        int free_space = max_buf_size - *buf_len - 1;
        if (free_space <= 0) {
            printf("[WARN] Buffer overflow detected, clearing buffer.\n");
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