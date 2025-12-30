#include "../../../Common/protocol.h"
#include "../../services/storage/storage.h"
#include "../../../Lib/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

extern void send_response(int socket_fd, int status, const char *message, cJSON *data);
extern Player* find_player_by_socket(int socket_fd);
extern void update_player_to_file(Player *player);
extern void log_action(const char *status, const char *action, const char *input, const char *result);
// Loại rương
typedef enum {
    CHEST_BRONZE = 0,
    CHEST_SILVER = 1,
    CHEST_GOLD = 2
} ChestType;

// Câu hỏi
typedef struct {
    char question[256];
    char options[4][100];
    int correct_option;
    ChestType chest_type;
    int reward_coins;
} QuestionBank;

QuestionBank question_bank[] = {
    // BRONZE (100 coins)
    {"2 + 2 = ?", {"3", "4", "5", "6"}, 1, CHEST_BRONZE, 100},
    {"Bao nhieu ngay trong tuan?", {"5", "6", "7", "8"}, 2, CHEST_BRONZE, 100},
    {"5 * 6 = ?", {"25", "30", "35", "40"}, 1, CHEST_BRONZE, 100},
    
    // SILVER (500 coins)
    {"Thu do Viet Nam?", {"Ha Noi", "TP HCM", "Da Nang", "Hue"}, 0, CHEST_SILVER, 500},
    {"Hanh tinh gan Mat Troi nhat?", {"Sao Kim", "Sao Thuy", "Trai Dat", "Sao Hoa"}, 1, CHEST_SILVER, 500},
    {"Dai duong lon nhat?", {"Atlantic", "An Do", "Thai Binh Duong", "Bac Bang Duong"}, 2, CHEST_SILVER, 500},
    
    // GOLD (2000 coins)
    {"Van toc anh sang (km/s)?", {"300000", "150000", "500000", "100000"}, 0, CHEST_GOLD, 2000},
    {"The chien 2 ket thuc nam nao?", {"1943", "1944", "1945", "1946"}, 2, CHEST_GOLD, 2000},
    {"Can bac 2 cua 2704?", {"48", "52", "56", "64"}, 1, CHEST_GOLD, 2000},
};

int question_bank_size = sizeof(question_bank) / sizeof(QuestionBank);

// Rương
typedef struct TreasureChest {
    int id;
    ChestType chest_type;
    int question_id;
    int is_opened;
    char opened_by[50];
    time_t created_at;
    struct TreasureChest *next;
} TreasureChest;

TreasureChest *treasure_list = NULL;
int next_treasure_id = 1;

const char* get_chest_name(ChestType type) {
    switch(type) {
        case CHEST_BRONZE: return "Dong";
        case CHEST_SILVER: return "Bac";
        case CHEST_GOLD: return "Vang";
        default: return "Unknown";
    }
}

TreasureChest* find_treasure(int treasure_id) {
    TreasureChest *current = treasure_list;
    while (current != NULL) {
        if (current->id == treasure_id) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

int can_receive_treasure(Player *player) {
    if (!player || !player->is_online) {
        return 0;
    }
    
    // CHỈ gửi treasure cho người đang TRONG TRẬN ĐẤU
    // Nếu muốn gửi cho cả người có đội, thêm: || player->status == STATUS_IN_TEAM
    if (player->status == STATUS_IN_BATTLE) {
        return 1;
    }
    
    return 0;
}

// Thả rương
void handle_treasure_appear(int client_fd, cJSON *payload) {
    // Random loại rương (50% Bronze, 35% Silver, 15% Gold)
    srand(time(NULL) + next_treasure_id);
    int rand_val = rand() % 100;
    ChestType chest_type;
    
    if (rand_val < 50) {
        chest_type = CHEST_BRONZE;
    } else if (rand_val < 85) {
        chest_type = CHEST_SILVER;
    } else {
        chest_type = CHEST_GOLD;
    }
    
    // Tìm câu hỏi phù hợp
    int available[20], count = 0;
    for (int i = 0; i < question_bank_size; i++) {
        if (question_bank[i].chest_type == chest_type) {
            available[count++] = i;
        }
    }
    
    if (count == 0) {
        printf("[ERROR] No questions available for chest type %d\n", chest_type);
        return;
    }
    
    int question_idx = available[rand() % count];
    QuestionBank *q = &question_bank[question_idx];
    
    // Tạo rương
    TreasureChest *new_treasure = (TreasureChest*)malloc(sizeof(TreasureChest));
    new_treasure->id = next_treasure_id++;
    new_treasure->chest_type = chest_type;
    new_treasure->question_id = question_idx;
    new_treasure->is_opened = 0;
    new_treasure->opened_by[0] = '\0';
    new_treasure->created_at = time(NULL);
    new_treasure->next = treasure_list;
    treasure_list = new_treasure;
    
    // Tạo broadcast data
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "treasure_id", new_treasure->id);
    cJSON_AddStringToObject(data, "chest_type", get_chest_name(chest_type));
    cJSON_AddStringToObject(data, "question", q->question);
    
    cJSON *options = cJSON_CreateArray();
    for (int i = 0; i < 4; i++) {
        cJSON_AddItemToArray(options, cJSON_CreateString(q->options[i]));
    }
    cJSON_AddItemToObject(data, "options", options);
    cJSON_AddNumberToObject(data, "reward", q->reward_coins);
    
    // Broadcast tới players online
    extern Player* players;
    extern int player_count;
    
    int broadcast_count = 0;
    for (int i = 0; i < player_count; i++) {
        if (can_receive_treasure(&players[i])) {
            send_response(players[i].socket_fd, RES_TREASURE_SUCCESS, 
                         "Ruong kho bau xuat hien!", cJSON_Duplicate(data, 1));
            broadcast_count++;
        }
    }
    
    cJSON_Delete(data);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), 
             "Treasure %d (%s) auto spawned, broadcast to %d players", 
             new_treasure->id, get_chest_name(chest_type), broadcast_count);
    log_action("SUCCESS", "AUTO_TREASURE", "SYSTEM", log_msg);
    
    printf("[TREASURE] ID=%d, Type=%s, Question=%s, Broadcast=%d\n", 
           new_treasure->id, get_chest_name(chest_type), q->question, broadcast_count);
}

// Trả lời
void handle_answer(int client_fd, cJSON *payload) {
    Player *player = find_player_by_socket(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Chua dang nhap", NULL);
        return;
    }
    
    if (player->status != STATUS_IN_BATTLE) {
        send_response(client_fd, RES_TREASURE_SUCCESS, "Ban phai dang trong tran dau!", NULL);
        return;
    }
    
    cJSON *treasure_id_node = cJSON_GetObjectItem(payload, "treasure_id");
    cJSON *answer_node = cJSON_GetObjectItem(payload, "answer");
    
    if (!treasure_id_node || !answer_node) {
        send_response(client_fd, RES_TREASURE_SUCCESS, "Thieu du lieu", NULL);
        return;
    }
    
    int treasure_id = treasure_id_node->valueint;
    int player_answer = answer_node->valueint;
    
    if (player_answer < 0 || player_answer > 3) {
        send_response(client_fd, RES_TREASURE_SUCCESS, "Dap an khong hop le (0-3)", NULL);
        return;
    }
    
    TreasureChest *treasure = find_treasure(treasure_id);
    if (!treasure) {
        send_response(client_fd, RES_TREASURE_SUCCESS, "Ruong khong ton tai", NULL);
        return;
    }
    
    // Race condition
    if (treasure->is_opened) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Da duoc mo boi %s", treasure->opened_by);
        send_response(client_fd, RES_TREASURE_OPENED, msg, NULL);
        
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg), "Da bi mo boi nguoi khac");
        log_action("ERROR", "ANSWER", player->username, log_msg);
        return;
    }
    
    QuestionBank *q = &question_bank[treasure->question_id];
    
    // Kiểm tra đáp án
    if (player_answer != q->correct_option) {
        send_response(client_fd, RES_ANSWER_WRONG, "Sai roi!", NULL);
        
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg), "Tra loi sai treasure %d", treasure_id);
        log_action("ERROR", "ANSWER", player->username, log_msg);
        return;
    }
    
    // ĐÚNG - Claim ngay
    treasure->is_opened = 1;
    strncpy(treasure->opened_by, player->username, sizeof(treasure->opened_by) - 1);
    
    player->ship.coin += q->reward_coins;
    update_player_to_file(player);
    
    // Response cho winner
    cJSON *winner_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(winner_data, "reward", q->reward_coins);
    cJSON_AddNumberToObject(winner_data, "total_coins", player->ship.coin);
    cJSON_AddStringToObject(winner_data, "chest_type", get_chest_name(treasure->chest_type));
    
    send_response(client_fd, RES_TREASURE_SUCCESS, 
                 "DUNG ROI! Ban nhan duoc ruong!", winner_data);
    
    // Broadcast tới tất cả
    extern Player* players;
    extern int player_count;
    
    for (int i = 0; i < player_count; i++) {
        if (can_receive_treasure(&players[i]) && players[i].socket_fd != client_fd) {
            cJSON *notif = cJSON_CreateObject();
            cJSON_AddNumberToObject(notif, "treasure_id", treasure_id);
            cJSON_AddStringToObject(notif, "opened_by", player->username);
            cJSON_AddStringToObject(notif, "chest_type", get_chest_name(treasure->chest_type));
            cJSON_AddNumberToObject(notif, "reward", q->reward_coins);
            
            send_response(players[i].socket_fd, RES_TREASURE_OPENED, 
                         "Ruong da duoc mo!", notif);
        }
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), 
             "Mo ruong %s, nhan %d coins", 
             get_chest_name(treasure->chest_type), q->reward_coins);
    log_action("SUCCESS", "ANSWER", player->username, log_msg);
    
    printf("[TREASURE] %s mo ruong %d (%s) nhan %d coins\n",
           player->username, treasure_id, get_chest_name(treasure->chest_type), q->reward_coins);
}
