#include "storage.h"
#include "../Common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Player *players = NULL;
int player_count = 0;
size_t player_capacity = 0;

Player* find_player_by_username(const char* username) {
    for(int i = 0; i < player_count; i++){
        if(strcmp(players[i].username, username) == 0){
            return &players[i];
        }
    }
    return NULL;
}

Player* find_player_by_id(int id) {
    for(int i = 0; i < player_count; i++){
        if(players[i].id == id){
            return &players[i];
        }
    }
    return NULL;
}

// Tìm player theo socket_fd
Player* find_player_by_socket(int socket_fd) {
    for(int i = 0; i < player_count; i++){
        if(players[i].socket_fd == socket_fd && players[i].is_online){
            return &players[i];
        }
    }
    return NULL;
}

// Cập nhật player vào file
void update_player_to_file(Player *player) {
    if (!player) return;
    
    // Chỉ lưu id, username, password. Ship data (hp, coin, weapons) CHỈ tồn tại trong sẽ reset khi server restart!
    
    FILE *file = fopen("accounts.txt", "w");
    if (!file) {
        printf("[ERROR] Cannot open accounts.txt for writing\n");
        return;
    }
    
    for (int i = 0; i < player_count; i++) {
        fprintf(file, "%d %s %s\n", 
                players[i].id, 
                players[i].username, 
                players[i].password);
    }
    
    fclose(file);
    
    printf("[STORAGE] Updated player %s (coin=%d, hp=%d)\n", 
           player->username, player->ship.coin, player->ship.hp);
}

int check_account_data(const char *username)
{
    if (!username || strlen(username) == 0)
        return 0;

    int allSpaces = 1;
    for (size_t i = 0; i < strlen(username); ++i)
    {
        if (username[i] != ' ')
        {
            allSpaces = 0;
            break;
        }
    }
    if (allSpaces)
        return 0;

    return 1;
}

void init_default_ship(Player *player) {
    player->ship.hp = 1000;
    player->ship.coin = 0;
    player->is_online = 0;
    player->status = STATUS_OFFLINE;
    player->socket_fd = 0;
}

void load_accounts(const char* filename) {
    FILE* file = fopen(filename, "r");
    if(!file) {
        player_capacity = 8;
        players = (Player*)calloc(player_capacity, sizeof(Player));
        printf("[STORAGE] Account file not found.\n");
        return;
    }

    size_t capacity = 8;
    size_t index = 0;
    players = (Player*)calloc(capacity, sizeof(Player));

    if(!players) {
        fprintf(stderr, "[ERROR] Memory allocation failed\n");
        fclose(file);
        return;
    }

    char line[512];
    int lineNumber = 0;

    while(fgets(line, sizeof(line), file)) {
        lineNumber++;

        size_t length = strlen(line);
        while(length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }

        if(length == 0) continue;

        int id_value;
        char username_buffer[51];
        char password_buffer[51];

        if(sscanf(line, "%d %50s %50s", &id_value, username_buffer, password_buffer) != 3) {
            fprintf(stderr, "[ERROR] Line %d has invalid format: '%s'\n", lineNumber, line);
            continue;
        }

        if(index >= capacity) {
            size_t new_capacity = capacity * 2;
            Player* tmp = (Player*)realloc(players, new_capacity * sizeof(Player));
            if(!tmp) {
                fprintf(stderr, "[ERROR] Memory allocation failed while expanding players\n");
                break;
            }
            memset(tmp + capacity, 0, (new_capacity - capacity) * sizeof(Player));
            players = tmp;
            capacity = new_capacity;
        }

        players[index].id = id_value;
        strncpy(players[index].username, username_buffer, sizeof(players[index].username) - 1);
        players[index].username[sizeof(players[index].username) - 1] = '\0';
        strncpy(players[index].password, password_buffer, sizeof(players[index].password) - 1);
        players[index].password[sizeof(players[index].password) - 1] = '\0';

        init_default_ship(&players[index]);
        
        index++;
    }

    player_count = index;
    player_capacity = capacity;

    fclose(file);
    printf("[STORAGE] Loaded %d accounts from %s\n", player_count, filename);
}

void append_account_to_file(const char* filename, Player* player) {
    FILE* file = fopen(filename, "a");
    if(!file) return;
    fprintf(file, "%d %s %s\n", player->id, player->username, player->password);
    fclose(file);
}

int register_player(const char* username, const char* password) {
    if(players == NULL) {
        player_capacity = 8;
        players = (Player*)calloc(player_capacity, sizeof(Player));
        player_count = 0;
    }

    if(find_player_by_username(username) != NULL) {
        return 0;
    }

    if(player_count >= player_capacity) {
        size_t new_capacity = player_capacity * 2;
        if(new_capacity == 0) new_capacity = 8;

        Player *temp = (Player*)realloc(players, new_capacity * sizeof(Player));
        if(!temp) {
            fprintf(stderr, "[ERROR] Memory allocation failed while expanding players\n");
            return -1;
        }
        memset(temp + player_capacity, 0, (new_capacity - player_capacity) * sizeof(Player));
        players = temp;
        player_capacity = new_capacity;
        printf("[STORAGE] Expanded player capacity to %zu for new user.\n", player_capacity);
    }

    Player *new_player = &players[player_count];
    int new_id = (player_count > 0) ? players[player_count - 1].id + 1 : 1;
    new_player->id = new_id;
    strncpy(new_player->username, username, sizeof(new_player->username) - 1);
    strncpy(new_player->password, password, sizeof(new_player->password) - 1);
    init_default_ship(new_player);

    append_account_to_file("accounts.txt", new_player);
    player_count++;
    return 1;
}
