#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "../Common/protocol.h"
#include "../Lib/cJSON.h"
#include "storage.h"
#include "utils.h"

typedef struct {
    int fd;
    char buffer[BUFFER_SIZE];
    int buf_len;
} ClientContext;

ClientContext clients[MAX_CLIENTS];

extern void handle_register(int client_fd, cJSON *payload);
extern void handle_login(int client_fd, cJSON *payload);
extern void handle_logout(int client_fd);
extern void handle_buy_item(int client_fd, cJSON *payload);
extern void handle_fix_ship(int client_fd, cJSON *payload);
extern void handle_treasure_appear(int client_fd, cJSON *payload);
extern void handle_answer(int client_fd, cJSON *payload);
extern int check_auth(int client_fd);
extern void send_response(int socket_fd, int status, const char *message, cJSON *data);

pthread_t treasure_spawner_thread;
int server_running = 1;

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].fd = -1;
        clients[i].buf_len = 0;
        memset(clients[i].buffer, 0, BUFFER_SIZE);
    }
}

void remove_client(int index) {
    int sd = clients[index].fd;
    handle_logout(sd);
    close(sd);
    clients[index].fd = -1;
    clients[index].buf_len = 0;
    memset(clients[index].buffer, 0, BUFFER_SIZE);
    printf("[LOG] Client disconnected: %d\n", sd);
}

void process_request(int client_fd, cJSON *root){
    if(!root) return;

    cJSON *action_node = cJSON_GetObjectItem(root, "action");
    if(!action_node) {
        send_response(client_fd, RES_UNKNOWN_ACTION, "Unknown action", NULL);
        return;
    }

    int action = action_node->valueint;
    cJSON *payload = cJSON_GetObjectItem(root, "data");

    int user_id = check_auth(client_fd);
    if(action != ACT_LOGIN && action != ACT_REGISTER && user_id == 0) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Login required", NULL);
        return;
    }

    switch(action) {
        case ACT_REGISTER:
            handle_register(client_fd, payload);
            break;
        case ACT_LOGIN:
            handle_login(client_fd, payload);
            break;
        case ACT_LOGOUT:
            handle_logout(client_fd);
            break;
        case ACT_BUY_ITEM:
            handle_buy_item(client_fd, payload);
            break;
        case ACT_FIX_SHIP:
            handle_fix_ship(client_fd, payload);
            break;
        case ACT_ANSWER:
            handle_answer(client_fd, payload);
            break;
        default:
            send_response(client_fd, RES_UNKNOWN_ACTION, "Unknown action", NULL);
            break;
    }
}

void* auto_spawn_treasure(void* arg) {
    srand(time(NULL));
    
    while (server_running) {
        // Chờ 30-60 giây random
        int wait_time = 30 + (rand() % 31);
        sleep(wait_time);
        
        // Kiểm tra có player online không
        extern Player* players;
        extern int player_count;
        
        int online_count = 0;
        for (int i = 0; i < player_count; i++) {
            if (players[i].is_online) {
                online_count++;
            }
        }
        
        // Chỉ thả nếu có player online
        if (online_count > 0) {
            printf("\n[AUTO] Spawning treasure... (%d players online)\n", online_count);
            handle_treasure_appear(-1, NULL);
        }
    }
    
    return NULL;
}

void run_server() {
    int master_socket, new_socket, max_sd, sd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;

    if((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0 ) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if(bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if(listen(master_socket, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server running on port %d... Waiting for connections.\n", PORT);

    while(1){
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;
            if(sd > 0) {
                FD_SET(sd, &readfds);
            }
            if(sd > max_sd) {
                max_sd = sd;
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if((activity < 0) && (errno!=EINTR)) {
            printf("Select error");
        }

        if(FD_ISSET(master_socket, &readfds)) {
            if((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            printf("New connection: socket fd is %d\n" , new_socket);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i].fd == -1) {
                    clients[i].fd = new_socket;
                    clients[i].buf_len = 0;
                    memset(clients[i].buffer, 0, BUFFER_SIZE);
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;
            if(FD_ISSET(sd , &readfds)) {
                cJSON *json = receive_json(sd, clients[i].buffer, &clients[i].buf_len, BUFFER_SIZE);
                if(json) {
                    process_request(sd, json);
                    cJSON_Delete(json);

                    while (strstr(clients[i].buffer, "\r\n") != NULL) {
                        json = receive_json(sd, clients[i].buffer, &clients[i].buf_len, BUFFER_SIZE);
                        if (json) {
                            process_request(sd, json);
                            cJSON_Delete(json);
                        } else {
                            break;
                        }
                    }
                } else {
                    char temp;
                    int check = recv(sd, &temp, 1, MSG_PEEK | MSG_DONTWAIT);
                    
                    if (check == 0) {
                        remove_client(i);
                    } else if (check < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        remove_client(i);
                    }
                }
            }
        }
    }
}

int main() {
    load_accounts("accounts.txt");

    init_clients();
    
    pthread_create(&treasure_spawner_thread, NULL, auto_spawn_treasure, NULL);
    printf("[AUTO] Treasure spawner started (30-60s interval)\n");
    
    run_server();
    
    server_running = 0;
    pthread_join(treasure_spawner_thread, NULL);

    return 0;
}
