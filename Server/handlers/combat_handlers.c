#include "../../Common/protocol.h"
#include "../../Lib/cJSON.h"
#include "../storage.h"
#include <stdio.h>
#include <stdlib.h>
#include "combat_handlers.h"

extern Player *players; // Player array
extern Team *teams;     // Teams array
extern void send_response(int socket_fd, int status, const char *message, cJSON *data);
extern Team *find_team_by_id(int team_id);
extern Player *find_player_by_id(int id);

//---------------------------------------------------------
// 1. CHALLENGE LOGIC
//---------------------------------------------------------

void handle_send_challenge(int client_fd, cJSON *payload)
{
    Player *challenger = get_player_by_fd(client_fd);
    Team *challenger_team = find_team_by_id(challenger->team_id);
    cJSON *target_team_node = cJSON_GetObjectItem(payload, "target_team_id"); // Get target team Id from payload

    // If team not found
    if (!target_team_node)
    {
        send_response(client_fd, RES_NOT_FOUND, "Team not found", NULL);
        return;
    }

    int target_team_id = target_team_node->valueint;
    // TODO: Find opponent in teams array
    Team *opponent_team = find_team_by_id(target_team_id);

    if (!opponent_team)
    {
        send_response(client_fd, RES_OPONENT_NOT_FOUND, "Opponent team not found", NULL);
        return;
    }

    // If found
    Player *opponent_captain = find_player_by_id(opponent_team->captain_id);

    // Send response to target team
    send_response(client_fd, RES_BATTLE_SUCCESS, "Challenge sent", NULL);

    // Send challenge to opponent captain
    send_response(opponent_captain->socket_fd, ACT_REQ_JOIN, "You received a challenge", NULL);
}

void handle_accept_challenge(int client_fd, cJSON *payload)
{
    // 1. Get challenger information
    Player *challenger = get_player_by_fd(client_fd);
    if (!challenger || challenger->team_id == 0)
    {
        send_response(client_fd, RES_UNKNOWN_ACTION, "You are not in a team", NULL);
        return;
    }

    // 2. Get challenger's team id
    Team *my_team = find_team_by_id(challenger->team_id);

    // 3. Get challenger team Infor
    Team *opponent_team = find_team_by_id(my_team->opponent_team_id);
    if (!opponent_team)
    {
        send_response(client_fd, RES_OPONENT_NOT_FOUND, "Opponent team not found", NULL);
        return;
    }

    // 4. Get challenger's team captain ID
    Player *opponent_captain = find_player_by_id(opponent_team->captain_id);

    // --- GAME LOGIC HANDLER ---
    if (challenger->status == STATUS_IN_BATTLE)
    {
        send_response(client_fd, RES_TEAM_ALREADY_IN_BATTLE, "challenger team is already in battle", NULL);
        return;
    }

    // Update status
    challenger->status = STATUS_IN_BATTLE;
    if (opponent_captain)
        opponent_captain->status = STATUS_IN_BATTLE;

    // Update status for my_team members
    for (int i = 0; i < my_team->current_size; i++)
    {
        int mem_id = my_team->member_ids[i];
        if (mem_id <= 0)
            continue; // INVALID member id

        Player *team_member = find_player_by_id(mem_id);

        if (team_member != NULL)
        {
            team_member->status = STATUS_IN_BATTLE;
        }
    }

    // Update status for opponent_team members
    for (int i = 0; i < opponent_team->current_size; i++)
    {
        int mem_id = opponent_team->member_ids[i];
        if (mem_id <= 0)
            continue; // INVALID member id

        Player *team_member = find_player_by_id(mem_id);

        if (team_member != NULL)
        {
            team_member->status = STATUS_IN_BATTLE;
        }
    }

    // Binding opponent team to both of the team
    my_team->opponent_team_id = opponent_team->team_id;
    opponent_team->opponent_team_id = my_team->team_id;

    // --- SEND RESPONSE

    // Notification about game start
    send_response(client_fd, RES_BATTLE_SUCCESS, "Game Started! You are in team B", NULL);

    // Send response for opponent_captain's team captain
    if (opponent_captain && opponent_captain->is_online)
    {
        send_response(opponent_captain->socket_fd, RES_BATTLE_SUCCESS, "Your challenge was accepted! Game Started!", NULL);
        // TODO: Implement game start ACT
    }
}

//---------------------------------------------------------
// 2. SHIP FIXING LOGIC (ACT_FIX_SHIP)
//---------------------------------------------------------

void handle_fix_ship(int client_fd, cJSON *payload)
{
    // Get requested player infomation
    Player *p = get_player_by_fd(client_fd);
    // If not found any player
    if (!p)
    {
        send_response(client_fd, RES_NOT_FOUND, "Player not found", NULL);
        return;
    }

    // If player hp is full, abort this command
    if (p->ship.hp >= 1000)
    {
        send_response(client_fd, RES_HP_IS_FULL, "HP is full", NULL);
        return;
    }

    // Cap max heal hp at 1000
    int hp_missing = 1000 - p->ship.hp;
    int cost = hp_missing * COST_REPAIR_PER_HP;

    // If player do not have enough money, heal to the maximum ammount available
    if (p->ship.coin < cost)
    {
        cost = p->ship.coin;
        hp_missing = cost / COST_REPAIR_PER_HP;
    }

    // Case that player do not have enough coin for heal for just 1 HP
    if (p->ship.coin < COST_REPAIR_PER_HP)
    {
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

void handle_attack(int client_fd, cJSON *payload)
{
    // Get attacker information
    Player *attacker = get_player_by_fd(client_fd);
    if (!attacker)
    {
        send_response(client_fd, RES_NOT_FOUND, "Player not found", NULL);
        return;
    }

    // Get target and weapong from payload
    cJSON *target_node = cJSON_GetObjectItem(payload, "target_user_id");
    cJSON *weapon_node = cJSON_GetObjectItem(payload, "weapon_id");

    // Validate input
    if (!target_node || !weapon_node)
    {
        send_response(client_fd, RES_UNKNOWN_ACTION, "Invalid parameters", NULL);
        return;
    }

    int target_id = target_node->valueint;
    int weapon_type = weapon_node->valueint; // 1: Cannon, 2: Laser, 3: Missile

    Player *target = find_player_by_id(target_id); // Get target information
    if (!target)
    {
        send_response(client_fd, RES_INVALID_TARGET, "Invalid Target", NULL);
        return;
    }

    //---------------------------------------------------------
    // 1. CHECK AMMO & DEDUCT
    //---------------------------------------------------------

    // 1. Damage indication and minus ammo
    WeaponSlot *found_slot = NULL;
    int damage = 0;

    // Check weapon type
    if (weapon_type == WEAPON_MISSILE)
    {
        // check dmg and ammo
        for (int i = 0; i < 4; i++)
        {
            if (attacker->ship.missiles[i].weapon == WEAPON_MISSILE &&
                attacker->ship.missiles[i].current_ammo > 0)
            {
                found_slot = &attacker->ship.missiles[i];
                damage = DMG_MISSILE; // 800
                break;
            }
        }
    }
    else if (weapon_type == WEAPON_CANNON_30MM)
    {
        for (int i = 0; i < 4; i++)
        {
            if (attacker->ship.cannons[i].weapon == weapon_type &&
                attacker->ship.cannons[i].current_ammo > 0)
            {
                found_slot = &attacker->ship.cannons[i];

                // Indicate damage by type of weapons
                if (weapon_type == WEAPON_CANNON_30MM)
                    damage = DMG_CANNON; // 10
                else if (weapon_type == WEAPON_LASER)
                    damage = DMG_LASER; // 100

                break;
            }
        }
    }

    // TODO: add return RES_OUT_OF_AMMO logic
    // If can NOT found any weapon or out of ammo
    if (found_slot == NULL)
    {
        send_response(client_fd, RES_OUT_OF_AMMO, "Out of ammo or weapon not equipped", NULL);
        return;
    }

    found_slot->current_ammo--;

    // ---------------------------------------------------------
    // 2. Damage target's armour/HP
    // ---------------------------------------------------------

    // 1. Getting armour slot we are using
    int armour_idx = -1; // No armour

    if (target->ship.armor[0].current_durability > 0)
    {
        armour_idx = 0;
    }
    else if (target->ship.armor[1].current_durability > 0)
    {
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
        }
        else // Damage overflow, broken armour
        {
            real_hp_loss = damage - current_armor_val;
            target->ship.armor[armour_idx].current_durability = 0; // Armour index into 0
            target->ship.hp -= real_hp_loss;
            current_armor_val = 0;
        }
    }
    else // target do not have armour -> damage into health directly
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
    cJSON_AddNumberToObject(res_data, "target_armor", current_armor_val);          // Update target armour for better UX
    cJSON_AddNumberToObject(res_data, "armor_slot_hit", armour_idx);               // Update armour slot that take dmg
    cJSON_AddNumberToObject(res_data, "remaining_ammo", found_slot->current_ammo); // Update reamining ammo for better UX
    send_response(client_fd, RES_BATTLE_SUCCESS, "Attack successfully", res_data);

    // 4. Check end game condition
    // TODO: Implement end game condition
    if (target->ship.hp == 0)
    {
        check_end_game(client_fd, target->team_id);
    }
}

void check_end_game(int client_fd, int team_id)
{
    Team *losing_team = find_team_by_id(team_id);
    if (!losing_team)
    {
        send_response(client_fd, RES_NOT_FOUND, "Team not found", NULL);
        return;
    }

    //---------------------------------------------------------
    // 1. Check if all member of one team is already eliminated
    //---------------------------------------------------------

    int alive_count = 0;
    for (int i = 0; i < losing_team->current_size; i++)
    {
        int member_id = losing_team->member_ids[i];
        Player *p = find_player_by_id(member_id);

        // If player found and HP > 0 -> alive
        if (p && p->ship.hp > 0)
        {
            alive_count++;
        }
        else if (!p)
        {
            send_response(client_fd, RES_NOT_FOUND, "Player not found", NULL);
            continue;
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

    // Send notification for both team and reset status
    end_game_for_team(client_fd,losing_team,end_game_data);
    end_game_for_team(client_fd,winning_team,end_game_data);
}

void end_game_for_team(int client_fd, Team *team, cJSON *payload)
{
    if (!team)
    {
        send_response(client_fd, RES_NOT_FOUND, "Team not found", NULL);
        return;
    }

    // Loop through every member of teams
    for (int i = 0; i < team->current_size; i++)
    {
        int member_id = team->member_ids[i];
        if (member_id <= 0)
        {
            send_response(client_fd, RES_INVALID_ID, "Invalid member id", NULL);
            continue;
        }

        Player *member = find_player_by_id(member_id);
        if (member && member->is_online)
        {
            // 1. Reset status to IN_TEAM (Ready for new game)
            member->status = STATUS_IN_TEAM;

            // 2. Send result notification
            send_response(member->socket_fd, RES_BATTLE_SUCCESS, "Game Over", payload);
        }
    }

    // Delete opponent team link
    team->opponent_team_id = 0;
}
