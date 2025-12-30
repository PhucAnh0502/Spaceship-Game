#ifndef CLIENT_TREASURE_C
#define CLIENT_TREASURE_C

#include "client_state.h"
#include <unistd.h>

// Hiển thị treasure đã pending (khi UI bị khóa)
void show_pending_treasure() {
    pthread_mutex_lock(&pending_mutex);
    if (pending_treasure.has_pending) {
        pthread_mutex_lock(&treasure_mutex);
        waiting_for_treasure_answer = 1;
        current_treasure_id = pending_treasure.treasure_id;
        pthread_mutex_unlock(&treasure_mutex);
        
        printf("\n");
        printf("RUONG KHO BAU XUAT HIEN!\n");
        printf("Treasure ID: %d\n", pending_treasure.treasure_id);
        printf("Loai: Ruong %s\n", pending_treasure.chest_type);
        printf("Phan thuong: %d coins\n", pending_treasure.reward);
        printf("\n");
        printf("Cau hoi: %s\n", pending_treasure.question);
        
        for (int i = 0; i < pending_treasure.option_count; i++) {
            printf("  %d. %s\n", i, pending_treasure.options[i]);
        }
        printf("\n");
        printf("Nhap dap an (0-%d) hoac 'q' de bo qua: ", pending_treasure.option_count - 1);
        fflush(stdout);
        
        pending_treasure.has_pending = 0;
    }
    pthread_mutex_unlock(&pending_mutex);
}

// Background listener thread - nhận broadcast từ server
// void* background_listener(void* arg) {
//     while (!should_exit) {
//         cJSON *response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);
        
//         if (response != NULL) {
//             cJSON *status = cJSON_GetObjectItem(response, "status");
//             cJSON *data = cJSON_GetObjectItem(response, "data");
            
//             // Xử lý broadcast - treasure xuất hiện
//             if (status && status->valueint == RES_TREASURE_SUCCESS) {
//                 if (data) {
//                     cJSON *treasure_id = cJSON_GetObjectItem(data, "treasure_id");
//                     cJSON *question = cJSON_GetObjectItem(data, "question");
//                     cJSON *options = cJSON_GetObjectItem(data, "options");
//                     cJSON *reward = cJSON_GetObjectItem(data, "reward");
//                     cJSON *chest_type = cJSON_GetObjectItem(data, "chest_type");
                    
//                     if (treasure_id && question && options) {
//                         // Kiểm tra UI có đang bị khóa không
//                         if (is_ui_locked()) {
//                             // Lưu treasure vào pending
//                             pthread_mutex_lock(&pending_mutex);
//                             pending_treasure.has_pending = 1;
//                             pending_treasure.treasure_id = treasure_id->valueint;
//                             strncpy(pending_treasure.question, question->valuestring, 255);
//                             if (chest_type) strncpy(pending_treasure.chest_type, chest_type->valuestring, 49);
//                             if (reward) pending_treasure.reward = reward->valueint;
                            
//                             int idx = 0;
//                             cJSON *option = NULL;
//                             cJSON_ArrayForEach(option, options) {
//                                 if (idx < 4) {
//                                     strncpy(pending_treasure.options[idx], option->valuestring, 99);
//                                     idx++;
//                                 }
//                             }
//                             pending_treasure.option_count = idx;
//                             pthread_mutex_unlock(&pending_mutex);
//                         } else {
//                             // UI không khóa, hiển thị ngay
//                             pthread_mutex_lock(&treasure_mutex);
//                             waiting_for_treasure_answer = 1;
//                             current_treasure_id = treasure_id->valueint;
//                             pthread_mutex_unlock(&treasure_mutex);
                            
//                             printf("\n");
//                             printf("RUONG KHO BAU XUAT HIEN!\n");
//                             printf("Treasure ID: %d\n", treasure_id->valueint);
//                             if (chest_type) printf("Loai: Ruong %s\n", chest_type->valuestring);
//                             if (reward) printf("Phan thuong: %d coins\n", reward->valueint);
//                             printf("\n");
//                             printf("Cau hoi: %s\n", question->valuestring);
                            
//                             int idx = 0;
//                             cJSON *option = NULL;
//                             cJSON_ArrayForEach(option, options) {
//                                 printf("  %d. %s\n", idx, option->valuestring);
//                                 idx++;
//                             }
//                             printf("\n");
//                             printf("Nhap dap an (0-%d) hoac 'q' de bo qua: ", idx-1);
//                             fflush(stdout);
//                         }
//                         cJSON_Delete(response);
//                         continue;
//                     }
//                 }
//             }
            
//             // Xử lý thông báo rương đã mở
//             if (status && status->valueint == RES_TREASURE_OPENED) {
//                 pthread_mutex_lock(&treasure_mutex);
//                 waiting_for_treasure_answer = 0;
//                 current_treasure_id = 0;
//                 pthread_mutex_unlock(&treasure_mutex);
                
//                 // Xóa pending treasure nếu có
//                 pthread_mutex_lock(&pending_mutex);
//                 pending_treasure.has_pending = 0;
//                 pthread_mutex_unlock(&pending_mutex);
                
//                 if (data && !is_ui_locked()) {
//                     cJSON *opened_by = cJSON_GetObjectItem(data, "opened_by");
//                     cJSON *reward = cJSON_GetObjectItem(data, "reward");
//                     if (opened_by) {
//                         printf("\n");
//                         printf("RUONG DA DUOC MO boi player %s\n", opened_by->valuestring);
//                         if (reward) printf("Phan thuong: %d coins\n", reward->valueint);
//                         printf("\nNhan Enter de tiep tuc...");
//                         fflush(stdout);
//                     }
//                 }
//             }
            
//             // Xử lý kết quả trả lời treasure sai (từ broadcast)
//             if (status && status->valueint == RES_ANSWER_WRONG && !is_ui_locked()) {
//                 cJSON *msg = cJSON_GetObjectItem(response, "message");
//                 printf("\n");
//                 printf("SAI ROI! %s\n", msg ? msg->valuestring : "");
//                 printf("\nNhan Enter de tiep tuc...");
//                 fflush(stdout);
                
//                 pthread_mutex_lock(&treasure_mutex);
//                 waiting_for_treasure_answer = 0;
//                 current_treasure_id = 0;
//                 pthread_mutex_unlock(&treasure_mutex);
//             }
            
//             cJSON_Delete(response);
//         }
//         usleep(100000); // 100ms
//     }
//     return NULL;
// }

// Xử lý trả lời treasure
void handle_treasure_answer(int answer) {
    pthread_mutex_lock(&treasure_mutex);
    int treasure_id = current_treasure_id;
    pthread_mutex_unlock(&treasure_mutex);
    
    if (treasure_id <= 0) {
        printf("Khong co ruong nao dang cho tra loi!\n");
        return;
    }

    lock_ui();

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "treasure_id", treasure_id);
    cJSON_AddNumberToObject(data, "answer", answer);

    send_json(sock, ACT_ANSWER, data);
    
    // Reset trạng thái sau khi gửi
    pthread_mutex_lock(&treasure_mutex);
    waiting_for_treasure_answer = 0;
    current_treasure_id = 0;
    pthread_mutex_unlock(&treasure_mutex);
    
    cJSON *res = wait_for_response();
    
    if (res) {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg) {
            printf("\n");
            
            if (status->valueint == RES_TREASURE_SUCCESS) {
                printf("CHINH XAC! %s\n", msg->valuestring);
                
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data) {
                    cJSON *reward = cJSON_GetObjectItem(res_data, "reward");
                    cJSON *total = cJSON_GetObjectItem(res_data, "total_coins");
                    if (reward && total) {
                        printf("Phan thuong: +%d coins\n", reward->valueint);
                        current_coins = total->valueint;
                        printf("Tong coins: %d\n", current_coins);
                    }
                }
            } else {
                printf(">> %s\n", msg->valuestring);
            }
        }
        cJSON_Delete(res);
    }
    
    unlock_ui();
}

#endif