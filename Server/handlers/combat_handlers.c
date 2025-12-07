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
    Player *challenger = find_player_by_id(client_fd);
    Team *challenger_team = find_team_by_id(challenger->team_id);
    cJSON *target_team_node = cJSON_GetObjectItem(payload, "target_team_id");
    // If team not found
    if (!target_team_node)
    {
        send_response(client_fd, RES_NOT_FOUND, "Team not found", NULL);
        return;
    }

    int target_team_id = target_team_node->valueint;
    // TODO: Find opponent in teams array
    Team *opponent_team = find_team_by_id(target_team_id);

    // If found
    Player *opponent_captain = find_player_by_id(opponent_team->captain_id);

    challenger_team->opponent_team_id = opponent_team->team_id;
    opponent_team->opponent_team_id = challenger_team->team_id;
    // Send response to target team
    send_response(client_fd, RES_BATTLE_SUCCESS, "Challenge sent", NULL);

    // Send challenge to opponent captain
    send_response(opponent_captain->socket_fd, ACT_REQ_JOIN, "You received a challenge", NULL);
}

void handle_accept_challenge(int client_fd, cJSON *payload)
{
    // 1. Get accepter ID
    Player *accepter = get_player_by_fd(client_fd);
    if (!accepter || accepter->team_id == 0)
    {
        send_response(client_fd, RES_UNKNOWN_ACTION, "You are not in a team", NULL);
        return;
    }

    // 2. Get accepter's team id
    Team *my_team = find_team_by_id(accepter->team_id);

    // 3. Check if is there is any pending challenger
    if (my_team->opponent_team_id == 0)
    {
        send_response(client_fd, RES_OPONENT_NOT_FOUND, "No pending challenge found", NULL);
        return;
    }

    // 4. Get challenger team Infor
    Team *opponent_team = find_team_by_id(my_team->opponent_team_id);
    if (!opponent_team)
    {
        send_response(client_fd, RES_OPONENT_NOT_FOUND, "Opponent team not found", NULL);
        return;
    }

    // 5. Get challenger's team captain ID
    Player *challenger = find_player_by_id(opponent_team->captain_id);

    // --- GAME LOGIC HANDLER ---

    // Update status
    accepter->status = STATUS_IN_BATTLE;
    if (challenger)
        challenger->status = STATUS_IN_BATTLE;

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

    // Update status for challenger team members
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
    // --- SEND RESPONSE

    // Notification about game start
    send_response(client_fd, RES_BATTLE_SUCCESS, "Game Started! You are in team B", NULL);

    // Send response for challenger's team captain
    if (challenger && challenger->is_online)
    {
        send_response(challenger->socket_fd, RES_BATTLE_SUCCESS, "Your challenge was accepted! Game Started!", NULL);
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
    if (hp_missing <= 0)
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

    int target_id = cJSON_GetObjectItem(payload, "target_user_id")->valueint;
    int weapon_type = cJSON_GetObjectItem(payload, "weapon_id")->valueint; // 1: Cannon, 2: Laser, 3: Missile

    Player *target = find_player_by_id(target_id); // Get target information
    if (!target)
    {
        send_response(client_fd, RES_INVALID_TARGET, "Invalid Target", NULL);
        return;
    }

    // 1. Damage indication and minus ammo
    int damage = 0;

    // check weapon type
    if (weapon_type == WEAPON_MISSILE)
    {
        // check dmg and ammo
        damage = DMG_MISSILE; // 800
    }
    else if (weapon_type == WEAPON_CANNON_30MM)
    {
        damage = DMG_CANNON; // 10
    }

    // TODO: add return RES_OUT_OF_AMMO logic

    // 2. Damage target's armour/HP
    int current_armour = target->ship.armor[0].current_durability; // Get armour values
    int real_hp_loss = 0;

    if (current_armour > 0) // case that target got armour
    {
        if (current_armour >= damage) // case that target armour is bigger than attacker's dmg
        {
            target->ship.armor[0].current_durability -= damage;
        }
        else
        {
            real_hp_loss = damage - current_armour;
            target->ship.armor[0].current_durability = 0;
            target->ship.hp -= real_hp_loss;
        }
    }
    else // cast that target do not have armor
    {
        target->ship.hp -= damage;
    }

    if (target->ship.hp < 0)
        target->ship.hp = 0; // case that damage overflow
       
    // 3. Send response
    cJSON *res_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(res_data, "damage", damage);
    cJSON_AddNumberToObject(res_data, "target_hp", target->ship.hp);
    send_response(client_fd, RES_BATTLE_SUCCESS, "Attack successfully", res_data);
    
    // 4. Check end game condition
    // TODO: Implement end game condition
    
}
