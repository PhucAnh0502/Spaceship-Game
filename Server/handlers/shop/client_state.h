#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

#include <pthread.h>
#include "../../../Common/protocol.h"
#include "../../../Lib/cJSON.h"

// Connection
extern int sock;
extern char client_buffer[];
extern int client_buf_len;

// Player state
extern int current_user_id;
extern int current_coins;
extern int current_hp;

// Thread control
extern pthread_t listener_thread;
extern volatile int should_exit;

// UI Lock (để tránh treasure chen vào khi đang thao tác)
extern volatile int ui_locked;
extern pthread_mutex_t ui_mutex;

// Treasure state
extern volatile int waiting_for_treasure_answer;
extern volatile int current_treasure_id;
extern pthread_mutex_t treasure_mutex;

// Pending treasure (khi UI bị khóa)
typedef struct {
    int has_pending;
    int treasure_id;
    char question[256];
    char options[4][100];
    int option_count;
    char chest_type[50];
    int reward;
} PendingTreasure;

extern PendingTreasure pending_treasure;
extern pthread_mutex_t pending_mutex;

// Từ utils.c (đã có sẵn)
extern void send_json(int sock, int action, cJSON *data);
extern cJSON* receive_json(int sock, char *buffer, int *buf_len, int buf_size);
// extern void get_input(const char *prompt, char *buffer, int size);

// Wait for server response
cJSON* wait_for_response(void);

// UI Lock functions
void lock_ui(void);
void unlock_ui(void);
int is_ui_locked(void);

// Helper để hiển thị trạng thái
void show_player_status(void);

// Helper để xác nhận mua hàng
int confirm_purchase(const char* item_name, int cost);

#endif // CLIENT_STATE_H