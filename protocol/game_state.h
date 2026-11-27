#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum abfp_terrain_type {
    ABFP_TERRAIN_UNKNOWN = 0,
    ABFP_TERRAIN_FLOOR,
    ABFP_TERRAIN_WALL,
    ABFP_TERRAIN_DOOR,
    ABFP_TERRAIN_STAIRS_UP,
    ABFP_TERRAIN_STAIRS_DOWN,
    ABFP_TERRAIN_RUBBLE,
    ABFP_TERRAIN_SHOP,
    ABFP_TERRAIN_DOOR_OPEN,
    ABFP_TERRAIN_DOOR_BROKEN,
    ABFP_TERRAIN_TRAP,
    ABFP_TERRAIN_VEIN
} abfp_terrain_type;

#define ABFP_MAX_MESSAGE_LENGTH 160
#define ABFP_MAX_INVENTORY_ITEMS 64
#define ABFP_MAX_SHOP_ITEMS 24
#define ABFP_MAX_SPELLS 64
#define ABFP_MAX_RACES 16
#define ABFP_MAX_CLASSES 16
#define ABFP_MAX_SAVES 24
#define ABFP_NUM_STATS 5

/* Item capability bitflags exported on abfp_inventory_item.flags */
#define ABFP_ITEM_FLAG_USABLE     0x001u
#define ABFP_ITEM_FLAG_WEARABLE   0x002u
#define ABFP_ITEM_FLAG_POTION     0x004u
#define ABFP_ITEM_FLAG_SCROLL     0x008u
#define ABFP_ITEM_FLAG_FOOD       0x010u
#define ABFP_ITEM_FLAG_FUEL       0x020u
#define ABFP_ITEM_FLAG_SELLABLE   0x040u
#define ABFP_ITEM_FLAG_AMMO       0x080u
#define ABFP_ITEM_FLAG_QUIVER     0x100u
#define ABFP_ITEM_FLAG_THROWABLE  0x200u

/* Spell capability bitflags exported on abfp_spell_entry.flags */
#define ABFP_SPELL_FLAG_LEARNED   0x01u
#define ABFP_SPELL_FLAG_CAN_CAST  0x02u
#define ABFP_SPELL_FLAG_CAN_STUDY 0x04u
#define ABFP_SPELL_FLAG_NEEDS_AIM 0x08u

typedef struct abfp_message {
    int32_t type;
    char text[ABFP_MAX_MESSAGE_LENGTH];
} abfp_message;

typedef enum abfp_entity_type {
    ABFP_ENTITY_MONSTER = 1,
    ABFP_ENTITY_OBJECT = 2
} abfp_entity_type;

typedef struct abfp_cell {
    int32_t x, y;
    abfp_terrain_type terrain;
    uint8_t visible, lit;
    uint8_t occupied_by_monster, occupied_by_object;
} abfp_cell;

/* kind_id is an Angband race index or known object-kind index.
 * Object category/variant are tval/sval and are zero for unidentified items.
 * The renderer maps these game IDs to its own models and animations. */
typedef struct abfp_entity_state {
    char name[64];
    uint64_t id;
    int32_t x, y;
    abfp_entity_type type;
    int32_t kind_id;
    int32_t category_id;
    int32_t variant_id;
    int32_t hitpoints, max_hitpoints;
    int32_t quantity;
} abfp_entity_state;

typedef struct abfp_inventory_item {
    int32_t index;
    char label;
    uint8_t equipped;
    uint32_t flags;
    int32_t quantity;
    int32_t category_id;
    int32_t variant_id;
    int32_t sell_price;
    char slot_name[24];
    char name[96];
} abfp_inventory_item;

typedef struct abfp_spell_entry {
    int32_t index;
    int32_t book_index;
    int32_t level;
    int32_t mana;
    int32_t fail;
    uint32_t flags;
    char name[48];
    char info[48];
} abfp_spell_entry;

typedef struct abfp_birth_option {
    int32_t index;
    char name[32];
    int32_t stat_adj[ABFP_NUM_STATS];
    int32_t hit_die;
    int32_t exp_factor;
    uint8_t has_magic;
} abfp_birth_option;

typedef struct abfp_birth_state {
    uint8_t active;
    uint8_t rolled;
    int32_t race_index;
    int32_t class_index;
    int32_t points_left;
    int32_t stats_base[ABFP_NUM_STATS];
    int32_t stats_total[ABFP_NUM_STATS];
    int32_t points_spent[ABFP_NUM_STATS];
    int32_t points_inc[ABFP_NUM_STATS];
    int32_t hitpoints;
    int32_t gold;
    char name[48];
    abfp_birth_option races[ABFP_MAX_RACES];
    uint32_t race_count;
    abfp_birth_option classes[ABFP_MAX_CLASSES];
    uint32_t class_count;
} abfp_birth_state;

typedef struct abfp_shop_item {
    int32_t index;
    int32_t quantity;
    int32_t price;
    int32_t category_id;
    int32_t variant_id;
    char name[96];
} abfp_shop_item;

typedef struct abfp_shop_state {
    uint8_t active;
    uint8_t is_home;
    char name[64];
    char owner[64];
    abfp_shop_item items[ABFP_MAX_SHOP_ITEMS];
    uint32_t item_count;
} abfp_shop_state;

typedef struct abfp_player_state {
    int32_t hitpoints, max_hitpoints;
    int32_t level;
    int32_t gold;
    int32_t mana, max_mana;
    uint8_t is_dead;
    int32_t exp, max_exp;
    int32_t armor_class;
    int32_t speed;
    int32_t food_pct;
    int32_t new_spells;
    uint8_t can_choose_spells;
    uint8_t has_launcher;
    char name[48];
    char race_name[32];
    char class_name[32];
} abfp_player_state;

typedef struct abfp_save_entry {
    char name[48];
    char description[128];
    uint8_t is_current;
    uint8_t is_dead;
} abfp_save_entry;

typedef struct abfp_snapshot {
    uint64_t turn;
    int32_t dungeon_depth;
    int32_t player_x, player_y;
    abfp_player_state player;
    const abfp_cell* cells;
    uint32_t cell_count;
    const abfp_entity_state* entities;
    uint32_t entity_count;
    const abfp_message* messages;
    uint32_t message_count;
    const abfp_inventory_item* inventory;
    uint32_t inventory_count;
    const abfp_shop_state* shop;
    const abfp_spell_entry* spells;
    uint32_t spell_count;
    const abfp_birth_state* birth;
    const abfp_save_entry* saves;
    uint32_t save_count;
} abfp_snapshot;

#ifdef __cplusplus
}
#endif
