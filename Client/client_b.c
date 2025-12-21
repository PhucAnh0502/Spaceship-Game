#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#include "../Common/protocol.h"
#include "../Lib/cJSON.h"
#include "utils.h"

#define SERVER_IP "127.0.0.1"

int sock = 0;
int current_user_id = 0;

char client_buffer[BUFFER_SIZE];
int client_buf_len = 0;

cJSON *wait_for_response()
{
    cJSON *response = NULL;

    while (1)
    {
        response = receive_json(sock, client_buffer, &client_buf_len, BUFFER_SIZE);

        if (response != NULL)
        {
            return response;
        }

        char dummy;
        int check = recv(sock, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
        if (check == 0)
        {
            printf("\n[ERROR] Server disconnected unexpectedly!\n");
            close(sock);
            exit(1);
        }
        else if (check < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("\n[ERROR] Socket error");
            exit(1);
        }
    }
}

void do_register()
{
    char username[50], password[50];
    printf("\n--- REGISTER ---\n");
    get_input("Username: ", username, 50);
    get_input("Password: ", password, 50);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "username", username);
    cJSON_AddStringToObject(data, "password", password);

    send_json(sock, ACT_REGISTER, data);

    cJSON *res = wait_for_response();

    if (res)
    {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");
        if (status && msg)
        {
            printf(">> Server [%d]: %s\n", status->valueint, msg->valuestring);
        }
        cJSON_Delete(res);
    }
}

void do_login()
{
    if (current_user_id != 0)
    {
        printf(">> You are already logged in!\n");
        return;
    }

    char username[50], password[50];
    printf("\n--- LOGIN ---\n");
    get_input("Username: ", username, 50);
    get_input("Password: ", password, 50);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "username", username);
    cJSON_AddStringToObject(data, "password", password);

    send_json(sock, ACT_LOGIN, data);

    cJSON *res = wait_for_response();

    if (res)
    {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *status = cJSON_GetObjectItem(res, "status");

        if (status && msg)
        {
            printf(">> Server [%d]: %s\n", status->valueint, msg->valuestring);

            if (status->valueint == RES_AUTH_SUCCESS)
            {
                cJSON *res_data = cJSON_GetObjectItem(res, "data");
                if (res_data)
                {
                    current_user_id = cJSON_GetObjectItem(res_data, "id")->valueint;
                    printf(">> Login success! User ID: %d\n", current_user_id);
                }
            }
        }
        cJSON_Delete(res);
    }
}

void do_logout()
{
    if (current_user_id == 0)
    {
        printf(">> You are not logged in.\n");
        return;
    }

    send_json(sock, ACT_LOGOUT, NULL);

    cJSON *res = wait_for_response();

    if (res)
    {
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        if (msg)
            printf(">> Server: %s\n", msg->valuestring);

        current_user_id = 0;
        cJSON_Delete(res);
    }
}

void check_notifications() {
    char buffer[4096];
    // MSG_DONTWAIT: Đọc ngay lập tức, không chặn chương trình nếu không có tin
    int received = recv(sock, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
    
    if (received > 0) {
        buffer[received] = '\0';
        
        // 1. Parse JSON từ buffer
        cJSON *root = cJSON_Parse(buffer);
        if (root == NULL) {
            // Nếu không phải JSON chuẩn thì in raw
            printf("\n[NOTIFICATION] Raw: %s\n", buffer);
            return;
        }

        // 2. Lấy các trường cơ bản
        cJSON *status_node = cJSON_GetObjectItem(root, "status");
        cJSON *msg_node = cJSON_GetObjectItem(root, "message");
        cJSON *data_node = cJSON_GetObjectItem(root, "data");

        // 3. Xử lý hiển thị dựa trên loại thông báo
        if (status_node && status_node->valueint == ACT_SEND_CHALLANGE) {
            // --- TRƯỜNG HỢP: CÓ LỜI MỜI THÁCH ĐẤU (Status 17) ---
            printf("\n========================================\n");
            printf("       ⚔️  INCOMING CHALLENGE! ⚔️       \n");
            printf("========================================\n");
            
            if (data_node) {
                cJSON *t_name = cJSON_GetObjectItem(data_node, "challenger_team_name");
                cJSON *t_id = cJSON_GetObjectItem(data_node, "challenger_team_id");
                
                if (t_name && t_id) {
                    printf("Enemy Team: %s (ID: %d)\n", t_name->valuestring, t_id->valueint);
                    printf("Message:    %s\n", msg_node ? msg_node->valuestring : "");
                    printf("----------------------------------------\n");
                    printf("👉 ACTION: Select 'Accept Challenge' (Option 7) to fight!\n");
                }
            }
        } 
        else if (status_node && status_node->valueint == RES_BATTLE_SUCCESS) {
            // --- TRƯỜNG HỢP: GAME START HOẶC THÔNG BÁO CHIẾN ĐẤU ---
            printf("\n>>> 🔔 BATTLE UPDATE: %s\n", msg_node ? msg_node->valuestring : "");
            
            // Nếu có data chi tiết (ví dụ thông báo sát thương)
            if (data_node) {
                char *data_str = cJSON_PrintUnformatted(data_node);
                if (data_str) {
                    printf("    Details: %s\n", data_str);
                    free(data_str);
                }
            }
        }
        else {
            // --- CÁC THÔNG BÁO KHÁC ---
            printf("\n[NOTIFICATION] Server: %s\n", msg_node ? msg_node->valuestring : buffer);
        }

        // 4. Dọn dẹp bộ nhớ JSON
        cJSON_Delete(root);

    } else {
        printf("\n[INFO] No new notifications.\n");
    }
}

void print_menu()
{
    printf("Choice: ");
    printf("\n============================\n");
    if (current_user_id == 0)
    {
        printf("1. Register\n");
        printf("2. Login\n");
    }
    else
    {
        printf("User ID: %d\n", current_user_id);
        printf("3. Logout\n");
        printf("4. Create Team (Mock)\n");
        printf("5. Send Challenge\n");
        printf("6. Accept Challenge\n");
        printf("7. Attack\n");
        printf("8. Check Notifications (Quan trọng!)\n");
    }
    printf("0. Exit\n");
    printf("============================\n");
    printf("Your choice: ");
}

// Hàm gửi lệnh tạo team
void do_create_team()
{
    char name[50];
    printf("Enter team name: ");
    scanf("%s", name);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "name", name);
    send_json(sock, ACT_CREATE_TEAM, data);

    cJSON *res = wait_for_response(); // Chờ server xác nhận
    if (res)
        cJSON_Delete(res);
}

// Hàm gửi lệnh thách đấu
void do_challenge()
{
    int target_id;
    printf("\n--- SEND CHALLENGE ---\n");
    printf("Enter Opponent Team ID: ");
    
    // Kiểm tra nhập liệu để tránh trôi lệnh
    if (scanf("%d", &target_id) != 1) {
        printf("[ERROR] Invalid input!\n");
        while(getchar() != '\n'); // Xóa buffer nếu nhập sai
        return;
    }
    getchar(); // Quan trọng: Xóa ký tự \n thừa trong buffer

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "target_team_id", target_id);
    
    // Gửi lệnh đi
    send_json(sock, ACT_SEND_CHALLANGE, data);

    // Chờ phản hồi
    cJSON *res = wait_for_response();
    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");

        if (status) {
            if (status->valueint == RES_BATTLE_SUCCESS) {
                // Thành công: Server báo "Challenge sent..."
                printf("✅ [SUCCESS] %s\n", msg ? msg->valuestring : "Request sent.");
            } else {
                // Thất bại: Server báo lỗi (VD: Team not found, Team busy...)
                printf("❌ [ERROR] Failed to send challenge: %s (Code: %d)\n", 
                       msg ? msg->valuestring : "Unknown error", 
                       status->valueint);
            }
        }
        cJSON_Delete(res);
    }
}

// Hàm chấp nhận thách đấu
void do_accept()
{
    printf("\n--- ACCEPT CHALLENGE ---\n");
    
    // Gửi lệnh chấp nhận (không cần payload)
    send_json(sock, ACT_ACCEPT_CHALLANGE, NULL);

    // Chờ phản hồi (Server sẽ trả về thông báo Start Game)
    cJSON *res = wait_for_response();
    if (res)
    {
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");
        cJSON *data = cJSON_GetObjectItem(res, "data");

        if (status) {
            if (status->valueint == RES_BATTLE_SUCCESS) {
                // Thành công: Bắt đầu trận đấu
                printf("🚀 [GAME START] %s\n", msg ? msg->valuestring : "Battle started!");
                
                // Nếu có dữ liệu kèm theo (Ví dụ: Tên đối thủ)
                if (data && !cJSON_IsNull(data)) {
                    cJSON *opp_name = cJSON_GetObjectItem(data, "opponent_name");
                    cJSON *match_id = cJSON_GetObjectItem(data, "match_id");
                    
                    if (opp_name) printf("⚔️  Your Opponent: %s\n", opp_name->valuestring);
                    if (match_id) printf("🆔 Match ID: %d\n", match_id->valueint);
                }
            } else {
                // Thất bại: Có thể do hết thời gian, lỗi hệ thống...
                printf("❌ [ERROR] Cannot start game: %s\n", msg ? msg->valuestring : "Unknown error");
            }
        }
        cJSON_Delete(res);
    }
}

void do_attack()
{
    int target_uid, wp_type;
    printf("Enter Target User ID: ");
    if (scanf("%d", &target_uid) != 1) return; // Kiểm tra nhập liệu
    
    printf("Weapon (1:Cannon, 2:Laser, 3:Missile): ");
    if (scanf("%d", &wp_type) != 1) return;
    
    // Xóa bộ nhớ đệm bàn phím để tránh lỗi trôi lệnh menu sau này
    getchar(); 

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "target_user_id", target_uid);
    cJSON_AddNumberToObject(data, "weapon_id", wp_type);
    send_json(sock, ACT_ATTACK, data);

    cJSON *res = wait_for_response();
    if (res)
    {
        // 1. Kiểm tra trạng thái phản hồi từ Server
        cJSON *status = cJSON_GetObjectItem(res, "status");
        cJSON *msg = cJSON_GetObjectItem(res, "message");

        if (status && status->valueint == RES_BATTLE_SUCCESS) // RES_BATTLE_SUCCESS = 400
        {
            cJSON *d = cJSON_GetObjectItem(res, "data");
            
            // 2. Kiểm tra data có hợp lệ và không phải NULL không
            if (d && !cJSON_IsNull(d))
            {
                cJSON *dmg_node = cJSON_GetObjectItem(d, "damage");
                cJSON *hp_node = cJSON_GetObjectItem(d, "target_hp");

                // 3. Chỉ in ra nếu các trường tồn tại
                if (dmg_node && hp_node)
                {
                    printf(">> HIT! Damage: %d | Target HP: %d\n", 
                           dmg_node->valueint, 
                           hp_node->valueint);
                }
                else
                {
                    printf(">> Attack success but no damage data returned.\n");
                }
            }
        }
        else 
        {
            // Trường hợp tấn công thất bại (Sai mục tiêu, hết đạn...)
            printf(">> Attack Failed: %s\n", msg ? msg->valuestring : "Unknown error");
        }
        
        cJSON_Delete(res);
    }
}

int main()
{
    struct sockaddr_in serv_addr;

    memset(client_buffer, 0, BUFFER_SIZE);
    client_buf_len = 0;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    printf("Connected to server %s:%d\n", SERVER_IP, PORT);

    int choice;
    char buffer[10];

    while (1)
    {
        print_menu();

        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
            break;
        if (sscanf(buffer, "%d", &choice) != 1)
            continue;

        switch (choice)
        {
        case 1:
            do_register();
            break;
        case 2:
            do_login();
            break;
        case 3:
            do_logout();
            break;
        case 4:
            do_create_team();
            break;
        case 5:
            do_challenge();
            break;
        case 6:
            do_accept();
            break;
        case 7:
            do_attack();
            break;
        case 8:
            check_notifications();
            break; // Bấm cái này để xem có ai mời hay bị bắn không
        case 0:
            close(sock);
            return 0;
        }
    }

    return 0;
}