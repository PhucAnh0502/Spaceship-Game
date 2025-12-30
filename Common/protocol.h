#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 1000
#define MAX_TEAMS 10
#define MAX_MEMBERS 3

// Cost constants
#define COST_AMMO_BOX 100
#define COST_LASER 1000
#define COST_LASER_BATTERY 100
#define COST_MISSILE 2000
#define COST_BASIC_ARMOR 1000
#define COST_HEAVY_ARMOR 2000
#define COST_REPAIR_PER_HP 1

// Stats constants
#define DMG_CANNON 10
#define DMG_LASER 100
#define DMG_MISSILE 800
#define AMOR_VAL_BASIC 500
#define AMOR_VAL_HEAVY 1500

typedef enum
{
    ACT_REGISTER = 1,
    ACT_LOGIN,
    ACT_LOGOUT,

    ACT_LIST_TEAMS,
    ACT_LIST_MEMBERS,
    ACT_CREATE_TEAM,
    ACT_REQ_JOIN,
    ACT_APPROVE_REQ,
    ACT_REFUSE_REQ,
    ACT_INVITE,
    ACT_ACCEPT_INVITE,
    ACT_REFUSE_INVITE,
    ACT_LEAVE_TEAM,
    ACT_KICK_MEMBER,

    ACT_BUY_ITEM,
    ACT_FIX_SHIP,

    ACT_SEND_CHALLANGE,
    ACT_ACCEPT_CHALLANGE,
    ACT_REFUSE_CHALLANGE,
    ACT_ATTACK,
    ACT_END_GAME,

    ACT_TREASURE_APPEAR,
    ACT_ANSWER,

    ACT_MOCK_EQUIP = 99
} ActionType;

typedef enum
{
    RES_AUTH_SUCCESS = 100,
    RES_ACCOUNT_EXISTS = 111,
    RES_ACCOUNT_NOT_FOUND = 112,
    RES_INVALID_PASSWORD = 113,
    RES_ACCOUNT_ALREADY_LOGGED_IN = 114,
    RES_NOT_LOGGED_IN = 115,
    RES_MISSING_CREDENTIALS = 116,

    RES_TEAM_SUCCESS = 200,
    RES_ALREADY_IN_TEAM = 211,
    RES_TEAM_FULL = 212,
    RES_NOT_TEAM_CAPTAIN = 213,
    RES_USER_IN_ANOTHER_TEAM = 214,
    RES_TEAM_NO_EXIST = 215,

    RES_SHOP_SUCCESS = 300,
    RES_NOT_ENOUGH_COIN = 311,
    RES_NOT_ENOUGH_SLOTS = 312,
    RES_HP_IS_FULL = 313,

    RES_BATTLE_SUCCESS = 400,
    RES_OPONENT_NOT_FOUND = 411,
    RES_MEMBER_NOT_READY = 412,
    RES_OUT_OF_AMMO = 413,
    RES_INVALID_TARGET = 414,
    RES_TEAM_ALREADY_IN_BATTLE = 415,

    RES_TREASURE_SUCCESS = 500,
    RES_ANSWER_WRONG = 511,
    RES_TREASURE_OPENED = 512,

    RES_UNKNOWN_ACTION = 611,
    RES_INVALID_ID = 612,
    RES_INVALID_ARGUMENT = 613,
    RES_NOT_FOUND = 614,
    RES_END_GAME = 600
} ResponseCode;

typedef enum
{
    ITEM_AMMO_30MM = 1,
    ITEM_WEAPON_LASER_GUN,
    ITEM_LASER_BATTERY,
    ITEM_WEAPON_MISSILE,
    ITEM_ARMOR_BASIC_KIT,
    ITEM_ARMOR_HEAVY_KIT
} ShopItemType;

typedef enum
{
    WEAPON_CANNON_30MM = 1,
    WEAPON_LASER = 2,
    WEAPON_MISSILE = 3
} WeaponType;

typedef enum
{
    ARMOR_NONE = 1,
    ARMOR_BASIC = 2,
    ARMOR_HEAVY = 3
} ArmorType;

typedef enum
{
    STATUS_OFFLINE = 0,
    STATUS_LOBBY = 1,
    STATUS_IN_TEAM = 2,
    STATUS_READY = 3,
    STATUS_IN_BATTLE = 4
} UserStatus;

typedef struct
{
    WeaponType weapon; // current weapons type (CANNON, LASER, MISSILES)
    int current_ammo;  // remaining ammo in this slot
} WeaponSlot;

typedef struct
{
    int hp;
    int coin;

    //---EQUIPMENTS---
    WeaponSlot cannons[4];  // 4 cannons slot (normal or laser)
    WeaponSlot missiles[4]; // 4 missiles slot

    struct
    {
        ArmorType type;
        int current_durability; // current armour durability
    } armor[2];                 // 2 slot for armour
} BattleShip;

typedef struct
{
    int id;
    char username[50];
    char password[50];

    BattleShip ship; // battle ship that player controlled

    int team_id;
    int socket_fd;
    int is_online;
    int status;
} Player;

typedef struct
{
    int team_id;
    char team_name[50];
    int captain_id;
    int member_ids[MAX_MEMBERS];
    int current_size;

    int pending_requests[MAX_CLIENTS];
    int pending_size;
    int opponent_team_id;
} Team;

typedef struct
{
    int id;
    char question[256];
    char options[4][100];
    int correct_option;
    int reward_type;
} Question; // multiple choice questions

#endif