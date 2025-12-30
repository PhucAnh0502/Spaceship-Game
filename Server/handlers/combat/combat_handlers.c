#include "../../../Common/protocol.h"
#include "../../../Lib/cJSON.h"
#include "../../services/storage/storage.h"
#include "../../services/utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include "combat_handlers.h"

extern Player *players; // Player array
extern Team teams[MAX_TEAMS]; // Teams array
extern void send_response(int socket_fd, int status, const char *message, cJSON *data);

extern Team *find_team_by_id(int team_id);

extern Player *find_player_by_id(int id);

//---------------------------------------------------------
// 1. CHALLENGE LOGIC
//---------------------------------------------------------

void handle_send_challenge(int client_fd, cJSON *payload) {
    // 1. Get challenger and challenger team info
    Player *challenger = get_player_by_fd(client_fd);
    if (!challenger) {
        send_response(client_fd, RES_NOT_FOUND, "challenger not found", NULL);
        return;
    }

    Team *challenger_team = find_team_by_id(challenger->team_id);
    if (!challenger_team) {
        send_response(client_fd, RES_TEAM_NO_EXIST, "You are not in a team", NULL);
        return;
    }

    // Check captain privillege
    if (challenger_team->captain_id != challenger->id) {
        send_response(client_fd, RES_NOT_TEAM_CAPTAIN, "Only captain can send challenge", NULL);
        return;
    }

    // 2. Get target team info
    int target_team_id = -1;
    cJSON *target_team_node = cJSON_GetObjectItem(payload, "target_team_name"); // Get target team name from payload
    if (target_team_node && cJSON_IsString(target_team_node)) {
        Team* team = find_team_by_name(target_team_node->valuestring);
        if (!team) {
            send_response(client_fd, RES_NOT_FOUND, "Target team not found", NULL);
            return;
        }

        target_team_id = team->team_id;
    }
    // If team not found
    if (target_team_id == -1) {
        send_response(client_fd, RES_NOT_FOUND, "Target team not found", NULL);
        return;
    }

    // TODO: Find opponent in teams array
    Team *opponent_team = find_team_by_id(target_team_id);

    if (target_team_id == challenger_team->team_id) {
        send_response(client_fd, RES_INVALID_TARGET, "Cannot challenge your own team", NULL);
        return;
    }

    if (opponent_team->opponent_team_id != 0) {
        send_response(client_fd, RES_TEAM_FULL, "Target team is busy/in another match", NULL);
        return;
    }

    if (!opponent_team) {
        send_response(client_fd, RES_OPONENT_NOT_FOUND, "Opponent team not found", NULL);
        return;
    }

    // 3. Team binding logic
    challenger_team->opponent_team_id = opponent_team->team_id;
    opponent_team->opponent_team_id = challenger_team->team_id;

    // 4. Send response
    send_response(client_fd, RES_BATTLE_SUCCESS, "Challenge sent. Waiting for response...", NULL);

    // 5. Send notification to other team captain
    Player *target_captain = find_player_by_id(opponent_team->captain_id);
    if (target_captain && target_captain->is_online) {
        cJSON *invite_data = cJSON_CreateObject();
        cJSON_AddNumberToObject(invite_data, "challenger_team_id", challenger_team->team_id);
        cJSON_AddStringToObject(invite_data, "challenger_team_name", challenger_team->team_name);
        send_response(target_captain->socket_fd, ACT_SEND_CHALLANGE, "Incoming challenge request!", invite_data);
    }
}

void handle_accept_challenge(int client_fd, cJSON *payload) {
    printf("Begin accept challenge\n");
    // 1. Get Accepter infomation
    Player *accepter = get_player_by_fd(client_fd);
    if (!accepter) {
        send_response(client_fd, RES_NOT_FOUND, "Accepter not found", NULL);
        return;
    }

    Team *my_team = find_team_by_id(accepter->team_id);
    if (!my_team) {
        send_response(client_fd, RES_NOT_FOUND, "Accepter's team NOT found", NULL);
        return;
    }

    // 2. Check captain privilledge
    if (my_team->captain_id != accepter->id) {
        send_response(client_fd, RES_NOT_TEAM_CAPTAIN, "Only captain can accept challenge", NULL);
        return;
    }

    // 3. Check team binding
    if (my_team->opponent_team_id == 0) {
        // this mean that there is no challenger
        send_response(client_fd, RES_OPONENT_NOT_FOUND, "No pending challenge found", NULL);
        return;
    }

    // Get opponent team from binded ID
    Team *opponent_team = find_team_by_id(my_team->opponent_team_id);
    if (!opponent_team) {
        // Case that enemy team quit or disband while pending for accept
        my_team->opponent_team_id = 0; // Reset
        send_response(client_fd, RES_OPONENT_NOT_FOUND, "Opponent team no longer exists", NULL);
        return;
    }

    // 4. Game start
    handle_game_start(my_team, opponent_team);
}

void handle_game_start(Team *my_team, Team *opponent_team) {
    // Loop through my team to update in battel status
    for (int i = 0; i < my_team->current_size; i++) {
        int mem_id = my_team->member_ids[i];
        if (mem_id > 0) {
            Player *p = find_player_by_id(mem_id);
            if (p)
                p->status = STATUS_IN_BATTLE;
        }
    }

    // Loop through enemy team to update in battle status
    for (int i = 0; i < opponent_team->current_size; i++) {
        int mem_id = opponent_team->member_ids[i];
        if (mem_id > 0) {
            Player *p = find_player_by_id(mem_id);
            if (p)
                p->status = STATUS_IN_BATTLE;
        }
    }

    // 2. Send response
    cJSON *game_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(game_data, "match_id", my_team->team_id * 100 + opponent_team->team_id);
    cJSON_AddStringToObject(game_data, "opponent_name", opponent_team->team_name);

    broadcast_to_team(my_team, RES_BATTLE_SUCCESS, "Battle Started!", game_data);
    printf("Game Started\n");
}

//---------------------------------------------------------
// 2. SHIP FIXING LOGIC (ACT_FIX_SHIP)
//---------------------------------------------------------

void handle_fix_ship(int client_fd, cJSON *payload) {
    // Get requested player infomation
    Player *p = get_player_by_fd(client_fd);
    // If not found any player
    if (!p) {
        send_response(client_fd, RES_NOT_FOUND, "Player not found", NULL);
        return;
    }

    // If player hp is full, abort this command
    if (p->ship.hp >= 1000) {
        send_response(client_fd, RES_HP_IS_FULL, "HP is full", NULL);
        return;
    }

    // Cap max heal hp at 1000
    int hp_missing = 1000 - p->ship.hp;
    int cost = hp_missing * COST_REPAIR_PER_HP;

    // If player do not have enough money, heal to the maximum ammount available
    if (p->ship.coin < cost) {
        cost = p->ship.coin;
        hp_missing = cost / COST_REPAIR_PER_HP;
    }

    // Case that player do not have enough coin for heal for just 1 HP
    if (p->ship.coin < COST_REPAIR_PER_HP) {
        send_response(client_fd, RES_NOT_ENOUGH_COIN, "Not enough coin", NULL);
        return;
    }

    // Update
    p->ship.coin -= cost;
    p->ship.hp += hp_missing;

    // Create response data
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "healed_amount", hp_missing);
    cJSON_AddNumberToObject(data, "current_hp", p->ship.hp);
    cJSON_AddNumberToObject(data, "remaining_coin", p->ship.coin);

    send_response(client_fd, RES_SHOP_SUCCESS, "Fix spaceship successfully", data);
}

//---------------------------------------------------------
// 3. ATTACK LOGIC (ACT_ATTACK)
//---------------------------------------------------------

void handle_attack(int client_fd, cJSON *payload) {
    // Get attacker information
    Player *attacker = get_player_by_fd(client_fd);
    if (!attacker) {
        send_response(client_fd, RES_NOT_FOUND, "Player not found", NULL);
        return;
    }

    // Get target and weapong from payload
    cJSON *target_node = cJSON_GetObjectItem(payload, "target_username");
    cJSON *weapon_node = cJSON_GetObjectItem(payload, "weapon_id");
    cJSON *weapon_slot_node = cJSON_GetObjectItem(payload, "weapon_slot");

    // Validate input
    if (!target_node || !weapon_node || !weapon_slot_node) {
        send_response(client_fd, RES_UNKNOWN_ACTION, "Invalid parameters", NULL);
        return;
    }
    Player *p = find_player_by_username(target_node->valuestring);
    if (!p) {
        send_response(client_fd, RES_NOT_FOUND, "Player not found", NULL);
        return;
    }
    int target_id = p->id;
    int weapon_type = weapon_node->valueint; // 1: Cannon, 2: Laser, 3: Missile
    int weapon_slot = weapon_slot_node->valueint;

    if (weapon_slot < 0 || weapon_slot >= 8) {
        send_response(client_fd, RES_UNKNOWN_ACTION, "Invalid weapon slot", NULL);
        return;
    }

    Player *target = find_player_by_id(target_id); // Get target information
    if (!target) {
        send_response(client_fd, RES_INVALID_TARGET, "Invalid Target", NULL);
        return;
    }

    // 1. Check attacker status
    if (attacker->status != STATUS_IN_BATTLE) {
        send_response(client_fd, RES_UNKNOWN_ACTION, "You are not in combat", NULL);
        return;
    }

    if (attacker->ship.hp <= 0) {
        send_response(client_fd, RES_UNKNOWN_ACTION, "You are dead", NULL);
        return;
    }
    // 2. Check if target is teamate
    if (attacker->team_id == target->team_id) {
        send_response(client_fd, RES_INVALID_TARGET, "Can NOT attack teammate", NULL);
        return;
    }

    //---------------------------------------------------------
    // 1. CHECK AMMO & DEDUCT
    //---------------------------------------------------------

    // 1. Damage indication and minus ammo
    WeaponSlot *found_slot = NULL;
    int damage = 0;

    // Check weapon type
    if (weapon_type == WEAPON_MISSILE) {
        // check dmg and ammo
        if (attacker->ship.missiles[weapon_slot].weapon == WEAPON_MISSILE &&
            attacker->ship.missiles[weapon_slot].current_ammo > 0) {
            found_slot = &attacker->ship.missiles[weapon_slot];
            damage = DMG_MISSILE; // 800
        }
    } else if (weapon_type == WEAPON_CANNON_30MM || weapon_type == WEAPON_LASER) {
        if (attacker->ship.cannons[weapon_slot].weapon == weapon_type &&
            attacker->ship.cannons[weapon_slot].current_ammo > 0) {
            found_slot = &attacker->ship.cannons[weapon_slot];

            // Indicate damage by type of weapons
            if (weapon_type == WEAPON_CANNON_30MM)
                damage = DMG_CANNON; // 10
            else if (weapon_type == WEAPON_LASER)
                damage = DMG_LASER; // 100
        }
    }

    // TODO: add return RES_OUT_OF_AMMO logic
    // If can NOT found any weapon or out of ammo
    if (found_slot == NULL) {
        send_response(client_fd, RES_OUT_OF_AMMO, "Out of ammo or weapon not equipped", NULL);
        return;
    }

    found_slot->current_ammo--;

    // ---------------------------------------------------------
    // 2. Damage target's armour/HP
    // ---------------------------------------------------------

    // 1. Getting armour slot we are using
    int armour_idx = -1; // No armour

    if (target->ship.armor[0].current_durability > 0) {
        armour_idx = 0;
    } else if (target->ship.armor[1].current_durability > 0) {
        armour_idx = 1;
    }

    int current_armor_val = 0;
    int real_hp_loss = 0;

    // 2. Calculate health/damage reduction
    if (armour_idx != -1) // If target has armour
    {
        current_armor_val = target->ship.armor[armour_idx].current_durability;

        if (current_armor_val >= damage) // Armour take all the damage
        {
            target->ship.armor[armour_idx].current_durability -= damage; // armour takes all the damage

            current_armor_val = target->ship.armor[armour_idx].current_durability; // update
        } else // Damage overflow, broken armour
        {
            real_hp_loss = damage - current_armor_val;
            target->ship.armor[armour_idx].current_durability = 0; // Armour index into 0
            target->ship.hp -= real_hp_loss;
            current_armor_val = 0;
        }
    } else // target do not have armour -> damage into health directly
    {
        target->ship.hp -= damage;
    }

    // prevent damage overflow
    if (target->ship.hp < 0)
        target->ship.hp = 0;
    // ---------------------------------------------------------
    // 3. Send response
    // ---------------------------------------------------------
    cJSON *res_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(res_data, "damage", damage);
    cJSON_AddNumberToObject(res_data, "target_hp", target->ship.hp);
    cJSON_AddNumberToObject(res_data, "target_armor", current_armor_val); // Update target armour for better UX
    cJSON_AddNumberToObject(res_data, "armor_slot_hit", armour_idx); // Update armour slot that take dmg
    cJSON_AddNumberToObject(res_data, "remaining_ammo", found_slot->current_ammo);
    // Update reamining ammo for better UX
    send_response(client_fd, RES_BATTLE_SUCCESS, "Attack successfully", res_data);

    // 3.1 Notification target that they have been attacked
    if (target->is_online && target->socket_fd > 0) {
        cJSON *victim_data = cJSON_CreateObject();
        cJSON_AddNumberToObject(victim_data, "current_hp", target->ship.hp); // Update HP
        cJSON_AddNumberToObject(victim_data, "damage_taken", damage);
        cJSON_AddStringToObject(victim_data, "attacker_name", attacker->username);
        send_response(target->socket_fd, RES_BATTLE_SUCCESS, "Warning: You are under attack!", victim_data);
    }
    // 4. Check end game condition
    // TODO: Implement end game condition
    if (target->ship.hp == 0) {
        check_end_game(client_fd, target->team_id);
    }
}

//---------------------------------------------------------
// 4. END GAME LOGIC (ACT_END_GAME)
//---------------------------------------------------------

void check_end_game(int client_fd, int team_id) {
    Team *losing_team = find_team_by_id(team_id);
    printf("Losing team: %s\n", losing_team->team_name);

    if (!losing_team) {
        send_response(client_fd, RES_NOT_FOUND, "Team not found", NULL);
        return;
    }

    //---------------------------------------------------------
    // 1. Check if all member of one team is already eliminated
    //---------------------------------------------------------

    int alive_count = 0;
    for (int i = 0; i < losing_team->current_size; i++) {
        int member_id = losing_team->member_ids[i];
        Player *p = find_player_by_id(member_id);

        // If player found and HP > 0 -> alive
        if (p) {
            if (p->ship.hp > 0 && p->is_online) {
                printf("Player %s is alive\n", p->username);
                alive_count++;
            } else if (p->ship.hp <= 0 && p->is_online) {
                printf("Player %s is dead\n", p->username);
                cJSON *death_note = cJSON_CreateObject();
                cJSON_AddStringToObject(death_note, "status", "DEAD");
                cJSON_AddStringToObject(death_note, "info", "Waiting for teammates...");

                send_response(p->socket_fd, RES_END_GAME, "YOU DIED! Spectating...", death_note);
                cJSON_Delete(death_note);
            }
        }
    }

    if (alive_count > 0) // team is not completely eliminated
        return;

    //---------------------------------------------------------
    // 2. Handle end game logic
    //---------------------------------------------------------

    // Determinate winning team
    int winning_team_id = losing_team->opponent_team_id;
    Team *winning_team = find_team_by_id(winning_team_id);

    // TODO: Logging informations

    // ---------------------------------------------------------
    // 3. Send response
    // ---------------------------------------------------------
    cJSON *end_game_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(end_game_data, "winner_team_id", winning_team_id);
    if (winning_team) {
        cJSON_AddStringToObject(end_game_data, "winner_team_name", winning_team->team_name);
    }

    // Send notification for both team and reset status
    end_game_for_team(client_fd, losing_team, end_game_data);
    end_game_for_team(client_fd, winning_team, end_game_data);

    cJSON_Delete(end_game_data);

    if (losing_team) losing_team->opponent_team_id = 0;
    if (winning_team) winning_team->opponent_team_id = 0;
}

void end_game_for_team(int client_fd, Team *team, cJSON *payload) {
    if (!team) {
        send_response(client_fd, RES_NOT_FOUND, "Team not found", NULL);
        return;
    }

    // Loop through every member of teams
    for (int i = 0; i < team->current_size; i++) {
        int member_id = team->member_ids[i];
        if (member_id <= 0) {
            send_response(client_fd, RES_INVALID_ID, "Invalid member id", NULL);
            continue;
        }

        Player *member = find_player_by_id(member_id);
        if (member && member->is_online) {
            // 1. Reset status to IN_TEAM (Ready for new game)
            member->status = STATUS_IN_TEAM;
            member->ship.hp = 1000;
            // 2. Send result notification
            send_response(member->socket_fd, RES_END_GAME, "Game Over", payload);
        }
    }

    // Delete opponent team link
    team->opponent_team_id = 0;
}

void handle_mock_equip(int client_fd) {
    Player *player = find_player_by_socket(client_fd);
    if (!player) {
        send_response(client_fd, RES_NOT_LOGGED_IN, "Player not found", NULL);
        return;
    }

    // 1. Bơm tiền và Máu
    player->ship.coin += 100000; // Cho 100k coin
    player->ship.hp = 1000; // Full máu

    // 2. Trang bị Vũ khí (Slot 0: Laser, Slot 1: Cannon)
    // Slot 0: Laser (999 đạn)
    player->ship.cannons[0].weapon = WEAPON_LASER;
    player->ship.cannons[0].current_ammo = 999;

    // Slot 1: Cannon (999 đạn)
    player->ship.cannons[1].weapon = WEAPON_CANNON_30MM;
    player->ship.cannons[1].current_ammo = 999;

    // 3. Trang bị Tên lửa (Full 4 slot)
    for (int i = 0; i < 4; i++) {
        player->ship.missiles[i].weapon = WEAPON_MISSILE;
        player->ship.missiles[i].current_ammo = 5; // Max đạn mỗi slot
    }

    // 4. Trang bị Giáp (Heavy Armor)
    player->ship.armor[0].type = ARMOR_HEAVY;
    player->ship.armor[0].current_durability = AMOR_VAL_HEAVY;

    // 4.1. Trang bị Giáp (Normal Armor)
    player->ship.armor[1].type = ARMOR_BASIC;
    player->ship.armor[1].current_durability = AMOR_VAL_BASIC;

    // Lưu lại vào file
    update_player_to_file(player);

    // Gửi phản hồi kèm data mới nhất để client cập nhật UI
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "coin", player->ship.coin);
    cJSON_AddNumberToObject(data, "hp", player->ship.hp);

    log_action("SUCCESS", "MOCK_EQUIP", player->username, "Used cheat command");
    send_response(client_fd, RES_SHOP_SUCCESS, "CHEAT ACTIVATED: Full Gear & Money!", data);
}
